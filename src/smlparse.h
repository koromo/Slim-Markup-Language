#ifndef SML_SMLPARSE_H
#define SML_SMLPARSE_H

#include "smldef.h"
#include "smlobj.h"
#include <memory>
#include <string>
#include <fstream>
#include <queue>

namespace sml
{
    // Parser
    struct Parser
    {
        // Consume front characters while the pred is true.
        template <class It, class Pred>
        void forward(It& b, It e, Pred p)
        {
            while (b != e && p(*b))
            {
                ++b;
            }
        }

        // Consume back characters while the pred is true.
        template <class It, class Pred>
        void backward(It b, It& e, Pred p)
        {
            while (b != e && p(*(e - 1)))
            {
                --e;
            }
        }

        // Consume front whitespaces
        template <class It>
        void consumeWhitespace(It& b, It e)
        {
            forward(b, e, [](char c) { return c == ' ' || c == '\t'; });
        }

        // <key> = <value>
        template <class It>
        void parse_key_eq_value(It& it, It end, table_t* table)
        {
            consumeWhitespace(it, end);
            const auto key = parse_key(it, end, table);

            ++it; // Skip '='.
            consumeWhitespace(it, end);
            const auto val = parse_value(it, end);

            table->addValue(key, val);
        }

        template <class It>
        std::string parse_key(It& it, It end, table_t* table)
        {
            const It keyB = it;
            forward(it, end, [](char c) { return c != ' ' && c != '\t' && c != '='; });
            const It keyE = it;
            if (it == end)
            {
                throw ParseException("Unexpected EOL.");
            }

            const std::string key(keyB, keyE);
            if (table->contains(key))
            {
                throw ParseException("Key duplicated (" + key + ")");
            }

            forward(it, end, [](char c) { return c != '='; });
            if (it == end)
            {
                throw ParseException("Unexpected EOL.");
            }

            return key;
        }

        template <class It>
        std::shared_ptr<Value> parse_value(It& it, It end)
        {
            if (it == end)
            {
                throw ParseException("Unexpected EOL.");
            }

            if (isInteger(it, end))
            {
                return parse_integer(it, end);
            }
            else if (isReal(it, end))
            {
                return parse_real(it, end);
            }
            else if (isString(it, end))
            {
                return parse_string(it, end);
            }
            else if (isArray(it, end))
            {
                return parse_array(it, end);
            }

            throw ParseException("Unexpected right value.");
        }

        template <class It>
        std::shared_ptr<Integer> parse_integer(It& it, It end)
        {
            int sign = 1;
            if (*it == '+' || *it == '-')
            {
                sign = *it == '+' ? 1 : -1;
                ++it; // Skip '+' or '-'
            }

            const It b = it;
            forward(it, end, [](char c) { return '0' <= c && c <= '9'; });
            const It e = it;

            integer_t i = std::stoi(std::string(b, e));
            i *= sign;

            return std::make_shared<Integer>(i);
        }

        template <class It>
        std::shared_ptr<Real> parse_real(It& it, It end)
        {
            int sign = 1;
            if (*it == '+' || *it == '-')
            {
                sign = *it == '+' ? 1 : -1;
                ++it; // Skip '+' or '-'
            }

            const It b = it;
            forward(it, end, [](char c) { return '0' <= c && c <= '9'; });
            ++it; // Skip '.'
            forward(it, end, [](char c) { return '0' <= c && c <= '9'; });
            const It e = it;

#ifdef SML_DOUBLE
            auto r = std::stod(std::string(b, e));
#else
            auto r = std::stof(std::string(b, e));
#endif

            r *= sign;

            return std::make_shared<Real>(r);
        }

        template <class It>
        std::shared_ptr<String> parse_string(It& it, It end)
        {
            ++it; // Skip '\"'

            const It b = it;
            forward(it, end, [](char c) { return c != '\"'; });
            const It e = it;
            ++it; // Skip '\"'

            std::string s(b, e);
            return std::make_shared<String>(s);
        }

        template <class It>
        std::shared_ptr<array_t> parse_array(It& it, It end)
        {
            It tmp = it;
            ++tmp;
            consumeWhitespace(tmp, end);

            if (isInteger(tmp, end))
            {
                return parse_array(it, end, [&](It& i, It e) { return parse_integer(i, e); });
            }
            else if (isReal(tmp, end))
            {
                return parse_array(it, end, [&](It& i, It e) { return parse_real(i, e); });
            }
            else if (isString(tmp, end))
            {
                return parse_array(it, end, [&](It& i, It e) { return parse_string(i, e); });
            }
            else if (isArray(tmp, end))
            {
                return parse_array(it, end, [&](It& i, It e) { return parse_array(i, e); });
            }

            throw ParseException("Invalid array format.");
        }

        template <class It, class ElemParser>
        std::shared_ptr<array_t> parse_array(It& it, It end, ElemParser efun)
        {
            const auto arr = std::make_shared<array_t>();

            while (*it != ']')
            {
                ++it; // Skip '[' or ','
                consumeWhitespace(it, end);

                const auto elem = efun(it, end);
                arr->insertBack(elem);

                consumeWhitespace(it, end);
            }
            ++it; // Skip ']'

            return arr;
        }

        template <class It>
        bool isInteger(It it, It end)
        {
            if (*it == '+' || *it == '-')
            {
                ++it; // Skip '+' or '-'
            }
            if (it == end)
            {
                return false;
            }
            if (*it == '0')
            {
                return false;
            }

            const It b = it;
            forward(it, end, [](char c) { return '0' <= c && c <= '9'; });
            if (it != end && *it == '.') // This is a real.
            {
                return false;
            }
            return !std::string(b, it).empty();
        }

        template <class It>
        bool isReal(It it, It end)
        {
            if (*it == '+' || *it == '-')
            {
                ++it; // Skip '+' or '-'
            }
            if (it == end)
            {
                return false;
            }

            const It b = it;
            forward(it, end, [](char c) { return '0' <= c && c <= '9'; });
            if (it == end || *it != '.')
            {
                return false;
            }
            ++it; // Skip '.'
            forward(it, end, [](char c) { return '0' <= c && c <= '9'; });

            return std::string(b, it) != ".";
        }

        template <class It>
        bool isString(It it, It end)
        {
            if (*it != '\"')
            {
                return false;
            }
            ++it; // Skip '\"'
            forward(it, end, [](char c) { return c != '\"'; });
            return it != end;
        }

        template <class It>
        bool isArray(It it, It end)
        {
            if (*it != '[')
            {
                return false;
            }

            ++it; // Skip '['
            size_t level = 1;
            forward(it, end, [&](char c) {
                if (c == '[')
                {
                    ++level;
                }
                else if (c == ']')
                {
                    --level;
                }
                return level > 0;
            });

            return it != end && *it == ']';
        }

        // [<table key>]
        template <class It>
        table_t* parse_table(It& it, It end, table_t* root)
        {
            consumeWhitespace(it, end);
            if (it == end)
            {
                throw ParseException("Unexpected EOL.");
            }

            const auto isTableArray = *it == '+';
            if (isTableArray)
            {
                ++it;
            }

            if (*it != '[')
            {
                throw ParseException("Unexpected character \'" + *it + std::string("\'."));
            }

            // Parse table path
            std::queue<std::string> path;

            while (*it != ']')
            {
                ++it; // Skip '[' or '.'.
                consumeWhitespace(it, end);

                const It keyB = it;
                forward(it, end, [](char c) { return c != ' ' && c != '\t' && c != '.' && c != ']'; });
                const It keyE = it;
                if (it == end)
                {
                    throw ParseException("Unexpected EOL.");
                }

                const std::string key(keyB, keyE);
                if (key.empty())
                {
                    throw ParseException("Unexpected character \'" + *it + std::string("\'."));
                }

                path.push(key);

                if (*it == ' ' || *it == '\t')
                {
                    forward(it, end, [](char c) { return c != '.' && c != ']'; });
                    if (it == end)
                    {
                        throw ParseException("Unexpected EOL.");
                    }
                }
            }

            ++it; // Skip ']'.

            // Create new table from path
            std::string fullpath;
            table_t* cur = root;
            while (path.size() > 1)
            {
                const auto key = path.front();
                path.pop();

                if (!fullpath.empty())
                {
                    fullpath += ".";
                }
                fullpath += key;

                if (valueIs<array_t>(key, *cur))
                {
                    auto& arr = cur->template valueAs<array_t>(key);
                    if (!arrayIs<table_t>(arr))
                    {
                        throw ParseException("Key is not defined (" + fullpath + ").");
                    }
                    cur = &(arr.template valueAs<table_t>(arr.length() - 1));
                }
                else if (valueIs<table_t>(key, *cur))
                {
                    cur = &(cur->template valueAs<table_t>(key));
                }
                else
                {
                    throw ParseException("Key is not defined (" + fullpath + ").");
                }
            }

            const auto key = path.front();
            const auto newTable = std::make_shared<table_t>();

            if (!fullpath.empty())
            {
                fullpath += ".";
            }
            fullpath += key;

            if (isTableArray)
            {
                if (!cur->contains(key))
                {
                    const auto arr = std::make_shared<array_t>();
                    cur->addValue(key, arr);
                    arr->insertBack(newTable);
                }
                else
                {
                    if (!valueIs<array_t>(key, *cur))
                    {
                        throw ParseException("Key is not defined (" + fullpath + ").");
                    }

                    auto& arr = cur->template valueAs<array_t>(key);
                    if (!arrayIs<table_t>(arr))
                    {
                        throw ParseException("Key is not defined (" + fullpath + ").");
                    }

                    arr.insertBack(newTable);
                }
            }
            else
            {
                if (cur->contains(key))
                {
                    throw ParseException("Key duplicated (" + fullpath + ")");
                }
                cur->addValue(key, newTable);
            }

            return newTable.get();
        }

        std::shared_ptr<const table_t> parse(const std::string& path)
        {
            std::ifstream in;

            in.open(path);
            if (!in.is_open())
            {
                throw ParseException("Failed to open file (" + path + ")");
            }

            struct Closer
            {
                std::ifstream* in;
                ~Closer()
                {
                    in->close();
                }
            };
            Closer closer{ &in };

            const auto rootTable = std::make_shared<table_t>();
            table_t* currentTable = rootTable.get();

            std::string line;
            while (!in.eof())
            {
                std::getline(in, line);

                auto it = std::cbegin(line);
                auto end = std::cend(line);

                // Erase front whitespaces.
                consumeWhitespace(it, end);

                if (it == end || *it == '#')
                {
                    continue;
                }

                if (*it == '[' || *it == '+')
                {
                    // (+)[<table key>]
                    // Create new table
                    currentTable = parse_table(it, end, rootTable.get());
                }
                else
                {
                    // <key> = <value>
                    // Parse pair of key and value
                    parse_key_eq_value(it, end, currentTable);
                }

                // After that the parse, the line need to be empty
                consumeWhitespace(it, end);
                if (it != end && *it != '#')
                {
                    throw ParseException(std::string("Unexpected character \'" + *it + std::string("\'.")));
                }
            }

            return rootTable;
        }
    };

    using ParseResult = table_t;

    /// Parse a .sml file
    inline std::shared_ptr<const ParseResult> parse(const std::string& path)
    {
        return Parser().parse(path);
    }
}

#endif
