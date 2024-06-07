// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_TEST_TEST_HELPERS_H_
#define SQL_TEST_TEST_HELPERS_H_

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <string>
#include <string_view>

#include "base/strings/cstring_view.h"

// Collection of test-only convenience functions.

namespace base {
class FilePath;
}

namespace sql {
class Database;
}

namespace sql::test {

// Read a database's page size. Returns nullopt in case of error.
std::optional<int> ReadDatabasePageSize(const base::FilePath& db_path);

// SQLite stores the database size in the header, and if the actual
// OS-derived size is smaller, the database is considered corrupt.
// [This case is actually a common form of corruption in the wild.]
// This helper sets the in-header size to one page larger than the
// actual file size.  The resulting file will return SQLITE_CORRUPT
// for most operations unless PRAGMA writable_schema is turned ON.
//
// This function operates on the raw database file, outstanding database
// connections may not see the change because of the database cache.  See
// CorruptSizeInHeaderWithLock().
//
// Returns false if any error occurs accessing the file.
[[nodiscard]] bool CorruptSizeInHeader(const base::FilePath& db_path);

// Call CorruptSizeInHeader() while holding a SQLite-compatible lock
// on the database.  This can be used to corrupt a database which is
// already open elsewhere.  Blocks until a write lock can be acquired.
[[nodiscard]] bool CorruptSizeInHeaderWithLock(const base::FilePath& db_path);

// Simulates total index corruption by zeroing the root page of an index B-tree.
//
// The corrupted database will still open successfully. SELECTs on the table
// associated with the index will work, as long as they don't access the index.
// However, any query that accesses the index will fail with SQLITE_CORRUPT.
// DROPping the table or the index will fail.
[[nodiscard]] bool CorruptIndexRootPage(const base::FilePath& db_path,
                                        std::string_view index_name);

// Return the number of tables in sqlite_schema.
[[nodiscard]] size_t CountSQLTables(sql::Database* db);

// Return the number of indices in sqlite_schema.
[[nodiscard]] size_t CountSQLIndices(sql::Database* db);

// Returns the number of columns in the named table.  0 indicates an
// error (probably no such table).
[[nodiscard]] size_t CountTableColumns(sql::Database* db, const char* table);

// Sets |*count| to the number of rows in |table|.  Returns false in
// case of error, such as the table not existing.
bool CountTableRows(sql::Database* db, const char* table, size_t* count);

// Creates a SQLite database at |db_path| from the sqlite .dump output
// at |sql_path|.  Returns false if |db_path| already exists, or if
// sql_path does not exist or cannot be read, or if there is an error
// executing the statements.
[[nodiscard]] bool CreateDatabaseFromSQL(const base::FilePath& db_path,
                                         const base::FilePath& sql_path);

// Test-friendly wrapper around sql::Database::IntegrityCheck().
[[nodiscard]] std::string IntegrityCheck(sql::Database& db);

// ExecuteWithResult() executes |sql| and returns the first column of the first
// row as a string.  The empty string is returned for no rows.  This makes it
// easier to test simple query results using EXPECT_EQ().  For instance:
//   EXPECT_EQ("1024", ExecuteWithResult(db, "PRAGMA page_size"));
//
// ExecuteWithResults() stringifies a larger result set by putting |column_sep|
// between columns and |row_sep| between rows.  For instance:
//   EXPECT_EQ("1,3,5", ExecuteWithResults(
//       db, "SELECT id FROM t ORDER BY id", "|", ","));
// Note that EXPECT_EQ() can nicely diff when using \n as |row_sep|.
//
// To test NULL, use the COALESCE() function:
//   EXPECT_EQ("<NULL>", ExecuteWithResult(
//       db, "SELECT c || '<NULL>' FROM t WHERE id = 1"));
// To test blobs use the HEX() function.
std::string ExecuteWithResult(sql::Database* db, const base::cstring_view sql);
std::string ExecuteWithResults(sql::Database* db,
                               const base::cstring_view sql,
                               const base::cstring_view column_sep,
                               const base::cstring_view row_sep);

// Returns the database size, in pages. Crashes on SQLite errors.
int GetPageCount(sql::Database* db);

// Column information returned by GetColumnInfo.
//
// C++ wrapper around the out-params of sqlite3_table_column_metadata().
struct ColumnInfo {
  // Retrieves schema information for a column in a table.
  //
  // Crashes on SQLite errors.
  //
  // |db_name| should be "main" for the connection's main (opened) database, and
  // "temp" for the connection's temporary (in-memory) database.
  //
  // This is a static method rather than a function so it can be listed in the
  // InternalApiToken access control list.
  [[nodiscard]] static ColumnInfo Create(sql::Database* db,
                                         const std::string& db_name,
                                         const std::string& table_name,
                                         const std::string& column_name);

  // The native data type. Example: "INTEGER".
  std::string data_type;
  // Default collation sequence for sorting. Example: "BINARY".
  std::string collation_sequence;
  // True if the column has a "NOT NULL" constraint.
  bool has_non_null_constraint;
  // True if the column is included in the table's PRIMARY KEY.
  bool is_in_primary_key;
  // True if the column is AUTOINCREMENT.
  bool is_auto_incremented;
};

}  // namespace sql::test

#endif  // SQL_TEST_TEST_HELPERS_H_
