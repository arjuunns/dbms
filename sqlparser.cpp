#include "sqlparser.h"
#include "table.h"
#include <sstream>
#include <iostream>
#include <vector>
#include <algorithm>

using namespace std;

// Helper function to convert a string to uppercase
string toUpperCase(string str)
{
    transform(str.begin(), str.end(), str.begin(), ::toupper);
    return str;
}

// Helper function to convert a string to lowercase (for table/column names)
string toLowerCase(string str)
{
    transform(str.begin(), str.end(), str.begin(), ::tolower);
    return str;
}

void SQLParser::executeQuery(Database &db, const string &query)
{
    stringstream ss(query);
    string command;
    ss >> command;
    command = toUpperCase(command); // Convert command to uppercase

    if (command == "CREATE")
    {
        string temp, tableName;
        ss >> temp;
        temp = toUpperCase(temp);
        if (temp != "TABLE")
        {
            cerr << "Syntax error: Expected 'TABLE' after CREATE\n";
            return;
        }

        ss >> tableName;
        tableName = toLowerCase(tableName);

        string columnsDefinition;
        getline(ss, columnsDefinition, ')');
        columnsDefinition.erase(0, columnsDefinition.find('(') + 1);

        stringstream colStream(columnsDefinition);
        vector<string> columnNames, columnTypes;
        string column, type;

        while (getline(colStream, column, ','))
        {
            stringstream columnStream(column);
            columnStream >> column >> type;
            column = toLowerCase(column);
            type = toUpperCase(type);

            if (column.empty() || type.empty())
            {
                cerr << "Invalid column definition: " << column << " " << type << "\n";
                return;
            }
            columnNames.push_back(column);
            columnTypes.push_back(type);
        }

        create(db, tableName, columnNames, columnTypes);
    }
    else if (command == "INSERT")
    {
        string temp, tableName;
        ss >> temp;
        temp = toUpperCase(temp);

        if (temp != "INTO")
        {
            cerr << "Syntax error: Expected 'INTO' after INSERT\n";
            return;
        }

        ss >> tableName;
        tableName = toLowerCase(tableName);

        string rowDataStr;
        getline(ss, rowDataStr);

        size_t openParen = rowDataStr.find('(');
        size_t closeParen = rowDataStr.find(')');

        if (openParen == string::npos || closeParen == string::npos)
        {
            cerr << "Syntax error: Missing parentheses in INSERT statement\n";
            return;
        }

        vector<string> columnNames;
        vector<string> rowData;

        bool hasColumns = (rowDataStr.find("VALUES") != string::npos);
        size_t valuesPos = rowDataStr.find("VALUES");

        if (hasColumns)
        {
            string colNamesStr = rowDataStr.substr(0, valuesPos);
            colNamesStr = colNamesStr.substr(colNamesStr.find('(') + 1, colNamesStr.find(')') - colNamesStr.find('(') - 1);

            stringstream colStream(colNamesStr);
            string colName;
            while (getline(colStream, colName, ','))
            {
                colName.erase(0, colName.find_first_not_of(" \t"));
                colName.erase(colName.find_last_not_of(" \t") + 1);
                columnNames.push_back(colName);
            }

            rowDataStr = rowDataStr.substr(valuesPos + 6);
            openParen = rowDataStr.find('(');
            closeParen = rowDataStr.find(')');
            rowDataStr = rowDataStr.substr(openParen + 1, closeParen - openParen - 1);
        }
        else
        {
            rowDataStr = rowDataStr.substr(openParen + 1, closeParen - openParen - 1);
        }

        stringstream rowStream(rowDataStr);
        string value;
        while (getline(rowStream, value, ','))
        {
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            if (!value.empty() && value.front() == '\'' && value.back() == '\'')
            {
                value = value.substr(1, value.size() - 2);
            }
            rowData.push_back(value);
        }

        Table table = selectTable(db, tableName);

        if (hasColumns)
        {
            table.insertWithColumns(columnNames, rowData);
        }
        else
        {
            table.insert(rowData);
        }
    }
    else if (command == "SELECT")
    {
        vector<string> columnNames;
        string temp, tableName;

        // Parse column names or '*' until 'FROM'
        ss >> temp;
        temp = toUpperCase(temp);
        if (temp == "FROM")
        {
            cerr << "Syntax error: Missing column names after SELECT\n";
            return;
        }

        if (temp == "*")
        {
            columnNames.clear(); // Empty vector means all columns
            ss >> temp; // Expect 'FROM' next
            temp = toUpperCase(temp);
        }
        else
        {
            columnNames.push_back(toLowerCase(temp)); // First column name
            while (ss >> temp)
            {
                temp = toUpperCase(temp);
                if (temp == "FROM")
                {
                    break; // Stop collecting column names
                }
                if (temp.back() == ',') // Handle trailing comma
                {
                    temp.pop_back();
                }
                columnNames.push_back(toLowerCase(temp));
            }
        }

        // Check if 'FROM' was found
        if (temp != "FROM")
        {
            cerr << "Syntax error: Expected 'FROM' after column names\n";
            return;
        }

        // Get table name
        ss >> tableName;
        tableName = toLowerCase(tableName);

        if (!tableName.empty() && tableName.back() == ';')
        {
            tableName.pop_back();
        }

        if (tableName.empty())
        {
            cerr << "Syntax error: Missing table name after 'FROM'\n";
            return;
        }

        // Removed debug output: "Searching for table: [tableName]"
        Table table = selectTable(db, tableName);

        if (table.getName().empty())
        {
            cerr << "Table '" << tableName << "' does not exist!\n";
            return;
        }

        table.displayTable(columnNames); // Pass column names to displayTable
    }
    else if (command == "RENAME")
    {
        string temp, oldTableName, newTableName;
        ss >> temp;
        temp = toUpperCase(temp);

        if (temp != "TABLE")
        {
            cerr << "Syntax error: Expected 'TABLE' after RENAME\n";
            return;
        }

        ss >> oldTableName;
        oldTableName = toLowerCase(oldTableName);

        ss >> temp;
        temp = toUpperCase(temp);
        if (temp != "TO")
        {
            cerr << "Syntax error: Expected 'TO' after table name in RENAME\n";
            return;
        }

        ss >> newTableName;
        newTableName = toLowerCase(newTableName);

        if (!newTableName.empty() && newTableName.back() == ';')
        {
            newTableName.pop_back();
        }

        if (oldTableName.empty() || newTableName.empty())
        {
            cerr << "Syntax error: Missing table names in RENAME statement\n";
            return;
        }

        rename(db, oldTableName, newTableName);
    }
    else if (command == "DROP")
    {
        string temp, tableName;
        ss >> temp;
        temp = toUpperCase(temp);

        if (temp != "TABLE")
        {
            cerr << "Syntax error: Expected 'TABLE' after DROP\n";
            return;
        }

        ss >> tableName;
        tableName = toLowerCase(tableName);

        if (!tableName.empty() && tableName.back() == ';')
        {
            tableName.pop_back();
        }

        if (tableName.empty())
        {
            cerr << "Syntax error: Missing table name after 'TABLE' in DROP\n";
            return;
        }

        drop(db, tableName);
    }
    else if (command == "TRUNCATE")
    {
        string temp, tableName;
        ss >> temp;
        temp = toUpperCase(temp);

        if (temp != "TABLE")
        {
            cerr << "Syntax error: Expected 'TABLE' after TRUNCATE\n";
            return;
        }

        ss >> tableName;
        tableName = toLowerCase(tableName);

        if (!tableName.empty() && tableName.back() == ';')
        {
            tableName.pop_back();
        }

        if (tableName.empty())
        {
            cerr << "Syntax error: Missing table name after 'TABLE' in TRUNCATE\n";
            return;
        }

        truncate(db, tableName);
    }
    else
    {
        cerr << "Invalid SQL Query!\n";
    }
}