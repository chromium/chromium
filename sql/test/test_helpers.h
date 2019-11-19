// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_TEST_TEST_HELPERS_H_
#define SQL_TEST_TEST_HELPERS_H_

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"

// Collection of test-only convenience functions.

namespace base {
class FilePath;
}

namespace sql {
class Database;
}

namespace sql {
namespace test {

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
bool CorruptSizeInHeader(const base::FilePath& db_path) WARN_UNUSED_RESULT;

// Common implementation of CorruptSizeInHeader() which operates on loaded
// memory. Shared between CorruptSizeInHeader() and the the mojo proxy testing
// code.
void CorruptSizeInHeaderMemory(unsigned char* header, int64_t db_size);

// Call CorruptSizeInHeader() while holding a SQLite-compatible lock
// on the database.  This can be used to corrupt a database which is
// already open elsewhere.  Blocks until a write lock can be acquired.
bool CorruptSizeInHeaderWithLock(
    const base::FilePath& db_path) WARN_UNUSED_RESULT;

// Frequently corruption is a result of failure to atomically update
// pages in different structures.  For instance, if an index update
// takes effect but the corresponding table update does not.  This
// helper restores the prior version of a b-tree root after running an
// update which changed that b-tree.  The named b-tree must exist and
// must be a leaf node (either index or table).  Returns true if the
// on-disk file is successfully modified, and the restored page
// differs from the updated page.
//
// The resulting database should be possible to open, and many
// statements should work.  SQLITE_CORRUPT will be thrown if a query
// through the index finds the row missing in the table.
//
// TODO(shess): It would be very helpful to allow a parameter to the
// sql statement.  Perhaps a version with a string parameter would be
// sufficient, given affinity rules?
bool CorruptTableOrIndex(const base::FilePath& db_path,
                         const char* tree_name,
                         const char* update_sql) WARN_UNUSED_RESULT;

// Return the number of tables in sqlite_master.
size_t CountSQLTables(sql::Database* db) WARN_UNUSED_RESULT;

// Return the number of indices in sqlite_master.
size_t CountSQLIndices(sql::Database* db) WARN_UNUSED_RESULT;

// Returns the number of columns in the named table.  0 indicates an
// error (probably no such table).
size_t CountTableColumns(sql::Database* db,
                         const char* table) WARN_UNUSED_RESULT;

// Sets |*count| to the number of rows in |table|.  Returns false in
// case of error, such as the table not existing.
bool CountTableRows(sql::Database* db, const char* table, size_t* count);

// Creates a SQLite database at |db_path| from the sqlite .dump output
// at |sql_path|.  Returns false if |db_path| already exists, or if
// sql_path does not exist or cannot be read, or if there is an error
// executing the statements.
bool CreateDatabaseFromSQL(const base::FilePath& db_path,
                           const base::FilePath& sql_path) WARN_UNUSED_RESULT;

// Return the results of running "PRAGMA integrity_check" on |db|.
// TODO(shess): sql::Database::IntegrityCheck() is basically the
// same, but not as convenient for testing.  Maybe combine.
std::string IntegrityCheck(sql::Database* db) WARN_UNUSED_RESULT;

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
std::string ExecuteWithResult(sql::Database* db, const char* sql);
std::string ExecuteWithResults(sql::Database* db,
                               const char* sql,
                               const char* column_sep,
                               const char* row_sep);

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
  static ColumnInfo Create(sql::Database* db,
                           const std::string& db_name,
                           const std::string& table_name,
                           const std::string& column_name) WARN_UNUSED_RESULT;

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

}  // namespace test
}  // namespace sql

#endif  // SQL_TEST_TEST_HELPERS_H_
