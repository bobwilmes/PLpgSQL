#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <set>
#include <string>
#include <unordered_map>
#include <regex>
#include <algorithm>
#include <cctype>

// Token types
enum TokenType {
    KEYWORD,
    IDENTIFIER,
    LITERAL,
    OPERATOR,
    SYMBOL,
    COMMENT,
    STRING_LITERAL,
    END_OF_FILE
};

// Token structure
struct Token {
    TokenType type;
    std::string value;
    int line;
};

// Function signature structure
struct FunctionSignature {
    std::string name;
    std::vector<std::string> argumentTypes;
    int line; // Line where the function is defined or first called
};

// Set of keywords
const std::set<std::string> keywords = {
    "select", "insert", "update", "delete", "create", "table", "begin", "end", "declare", "do", "values"
};

// Preprocessor map to store substitutions
std::unordered_map<std::string, std::string> preprocessorMap;

// Lexer class
class Lexer {
private:
    std::string input;
    size_t position = 0;
    int line = 1;

    char peek() {
        return position < input.length() ? input[position] : '\0';
    }

    char advance() {
        char current = peek();
        position++;
        if (current == '\n') line++;
        return current;
    }

    void skipWhitespace() {
        while (isspace(peek())) advance();
    }

    Token handleIdentifierOrKeyword() {
        std::string value;
        while (isalnum(peek()) || peek() == '_') {
            value += advance();
        }
        std::string lowercaseValue = value;
        std::transform(lowercaseValue.begin(), lowercaseValue.end(), lowercaseValue.begin(), ::tolower);
        if (keywords.count(lowercaseValue)) {
            return {KEYWORD, value, line};
        }
        return {IDENTIFIER, value, line};
    }

    Token handleLiteral() {
        std::string value;
        while (isdigit(peek())) {
            value += advance();
        }
        return {LITERAL, value, line};
    }

    Token handleSymbol() {
        return {SYMBOL, std::string(1, advance()), line};
    }

    Token handleStringLiteral() {
        std::string value;
        advance(); // Skip the opening quote
        while (peek() != '"' && peek() != '\0') {
            value += advance();
        }
        if (peek() == '"') advance(); // Skip the closing quote
        return {STRING_LITERAL, value, line};
    }

public:
    Lexer(const std::string &input) : input(input) {}

    std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        while (position < input.length()) {
            skipWhitespace();
            char current = peek();
            if (isalpha(current) || current == '_') {
                tokens.push_back(handleIdentifierOrKeyword());
            } else if (isdigit(current)) {
                tokens.push_back(handleLiteral());
            } else if (current == '"') {
                tokens.push_back(handleStringLiteral());
            } else if (ispunct(current)) {
                tokens.push_back(handleSymbol());
            } else {
                advance();
            }
        }
        tokens.push_back({END_OF_FILE, "", line});
        return tokens;
    }
};

// Preprocessor class
class Preprocessor {
public:
    static std::string process(const std::string &input) {
        std::istringstream stream(input);
        std::ostringstream processedCode;
        std::string line;

        while (std::getline(stream, line)) {
            // Check for #define directive
            if (line.find("#define") == 0) {
                std::istringstream defineStream(line);
                std::string directive, key, value;
                defineStream >> directive >> key;
                std::getline(defineStream, value);
                value = std::regex_replace(value, std::regex("^\\s+|\\s+$"), ""); // Trim whitespace
                preprocessorMap[key] = value;
            } else {
                // Replace macros in the line
                for (const auto &entry : preprocessorMap) {
                    size_t pos = 0;
                    while ((pos = line.find(entry.first, pos)) != std::string::npos) {
                        line.replace(pos, entry.first.length(), entry.second);
                        pos += entry.second.length();
                    }
                }
                processedCode << line << "\n";
            }
        }

        return processedCode.str();
    }
};

// Parser with two-pass analysis
class Parser {
private:
    std::vector<Token> tokens;
    size_t position = 0;
    int indentLevel = 0; // Tracks indentation level
    std::ostringstream formattedCode;
    std::unordered_map<std::string, FunctionSignature> functionTable; // Stores function definitions and calls

    Token peek() {
        return position < tokens.size() ? tokens[position] : Token{END_OF_FILE, "", -1};
    }

    Token advance() {
        return position < tokens.size() ? tokens[position++] : Token{END_OF_FILE, "", -1};
    }

    void writeIndentedLine(const std::string &line) {
        for (int i = 0; i < indentLevel; ++i) {
            formattedCode << "    "; // 4 spaces for each indentation level
        }
        formattedCode << line << "\n";
    }

    // First pass: Detect functions
    void detectFunctionCall() {
        Token functionName = advance();
        if (peek().value == "(") {
            advance(); // Skip '('
            std::vector<std::string> arguments;

            while (peek().value != ")" && peek().type != END_OF_FILE) {
                if (peek().type == LITERAL || peek().type == IDENTIFIER || peek().type == STRING_LITERAL) {
                    arguments.push_back(peek().value);
                }
                advance();
                if (peek().value == ",") advance(); // Skip commas
            }

            if (peek().value == ")") {
                advance(); // Skip ')'
            }

            // Record the function in the function table
            if (functionTable.find(functionName.value) == functionTable.end()) {
                functionTable[functionName.value] = {functionName.value, arguments, functionName.line};
            }
        }
    }

    // Second pass: Validate functions
    void validateFunctionCall() {
        Token functionName = advance();
        writeIndentedLine(functionName.value + " (");

        if (peek().value == "(") {
            advance(); // Skip '('
            std::vector<std::string> arguments;

            while (peek().value != ")" && peek().type != END_OF_FILE) {
                if (peek().type == LITERAL || peek().type == IDENTIFIER || peek().type == STRING_LITERAL) {
                    arguments.push_back(peek().value);
                }
                advance();
                if (peek().value == ",") advance(); // Skip commas
            }

            if (peek().value == ")") {
                advance(); // Skip ')'
            } else {
                writeIndentedLine("-- Error: Missing closing parenthesis for function call.");
            }

            // Check against the function table
            if (functionTable.find(functionName.value) != functionTable.end()) {
                auto &signature = functionTable[functionName.value];
                if (signature.argumentTypes.size() != arguments.size()) {
                    writeIndentedLine("-- Error: Function '" + functionName.value + "' at line " +
                                      std::to_string(signature.line) + " expects " +
                                      std::to_string(signature.argumentTypes.size()) + " arguments, but " +
                                      std::to_string(arguments.size()) + " were provided.");
                }
            } else {
                writeIndentedLine("-- Error: Unknown function '" + functionName.value + "' at line " +
                                  std::to_string(functionName.line) + ".");
            }
        }

        writeIndentedLine(");");
    }

    void parseStatement(bool isFirstPass) {
        Token token = peek();
        if (token.type == IDENTIFIER && peek().value != "(") {
            if (isFirstPass) {
                detectFunctionCall();
            } else {
                validateFunctionCall();
            }
        } else if (token.type == KEYWORD) {
            advance();
            writeIndentedLine(token.value);
        } else {
            advance();
            writeIndentedLine(token.value + ";");
        }
    }

public:
    Parser(const std::vector<Token> &tokens) : tokens(tokens) {}

    void firstPass() {
        while (peek().type != END_OF_FILE) {
            parseStatement(true);
        }
    }

    std::string secondPass() {
        position = 0; // Reset position for second pass
        while (peek().type != END_OF_FILE) {
            parseStatement(false);
        }
        return formattedCode.str();
    }
};

// File I/O functions
std::string readFile(const std::string &filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file " << filename << "\n";
        exit(EXIT_FAILURE);
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void writeFile(const std::string &filename, const std::string &content) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot write to file " << filename << "\n";
        exit(EXIT_FAILURE);
    }
    file << content;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <filename>\n";
        return EXIT_FAILURE;
    }

    std::string filename = argv[1];
    std::string sourceCode = readFile(filename);

    // Preprocess the input
    std::string preprocessedCode = Preprocessor::process(sourceCode);

    Lexer lexer(preprocessedCode);
    auto tokens = lexer.tokenize();

    Parser parser(tokens);

    // First pass: Build the function table
    parser.firstPass();

    // Second pass: Validate functions and generate formatted output
    std::string formattedCode = parser.secondPass();

    std::string outputFilename = filename + ".formatted";
    writeFile(outputFilename, formattedCode);

    std::cout << "Formatted and validated code written to " << outputFilename << "\n";
    return EXIT_SUCCESS;
}
