// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_RECOVER_MODULE_TABLE_H_
#define SQL_RECOVER_MODULE_TABLE_H_

#include <atomic>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/optional.h"
#include "sql/recover_module/parsing.h"
#include "third_party/sqlite/sqlite3.h"

namespace sql {
namespace recover {

class VirtualCursor;

// Represents a virtual table created by CREATE VIRTUAL TABLE recover(...).
//
// Instances are allocated on the heap using the C++ new operator, and passed to
// SQLite via pointers to the sqlite_vtab members. SQLite is responsible for
// managing the instances' lifetimes. SQLite will call xDisconnect() for every
// successful xConnect(), and xDestroy() for every successful xCreate().
//
// Instances are thread-safe.
class VirtualTable {
 public:
  // Returns a SQLite status and a VirtualTable instance.
  //
  // The VirtualTable is non-null iff the status is SQLITE_OK.
  //
  // SQLite is trusted to keep |sqlite_db| alive for as long as this instance
  // lives.
  static std::pair<int, std::unique_ptr<VirtualTable>> Create(
      sqlite3* sqlite_db,
      TargetTableSpec backing_table_spec,
      std::vector<RecoveredColumnSpec> column_specs);

  // Use Create() instead of calling the constructor directly.
  explicit VirtualTable(sqlite3* sqlite_db,
                        sqlite3_file* sqlite_file,
                        int root_page,
                        int page_size,
                        std::vector<RecoveredColumnSpec> column_specs);
  ~VirtualTable();

  VirtualTable(const VirtualTable&) = delete;
  VirtualTable& operator=(const VirtualTable&) = delete;

  // Returns the embedded SQLite virtual table.
  //
  // This getter is not const because SQLite wants a non-const pointer to the
  // structure.
  sqlite3_vtab* SqliteTable() { return &sqlite_table_; }

  // Returns SQLite VFS file used to access the backing table's database.
  //
  // This getter is not const because it must return a non-const pointer.
  sqlite3_file* SqliteFile() { return sqlite_file_; }

  // Returns the page number of the root page for the table's B-tree.
  int root_page_id() const { return root_page_id_; }

  // Returns the page size used by the backing table's database.
  int page_size() const { return page_size_; }

  // Returns the schema of the corrupted table being recovered.
  const std::vector<RecoveredColumnSpec> column_specs() const {
    return column_specs_;
  }

  // Returns a SQL statement describing the virtual table's schema.
  //
  // The return value is suitable to be passed to sqlite3_declare_vtab().
  std::string ToCreateTableSql() const;

  // The VirtualTable instance that embeds a given SQLite virtual table.
  //
  // |sqlite_table| must have been returned by VirtualTable::SqliteTable().
  static inline VirtualTable* FromSqliteTable(sqlite3_vtab* sqlite_table) {
    static_assert(std::is_standard_layout<VirtualTable>::value,
                  "needed for the reinterpret_cast below");
    static_assert(offsetof(VirtualTable, sqlite_table_) == 0,
                  "sqlite_table_ must be the first member of the class");
    VirtualTable* const result = reinterpret_cast<VirtualTable*>(sqlite_table);
    DCHECK_EQ(sqlite_table, &result->sqlite_table_);
    return result;
  }

  // Creates a new cursor for iterating over this table.
  VirtualCursor* CreateCursor();

  // Called right before a cursor belonging to this table will be destroyed.
  void WillDeleteCursor(VirtualCursor* cursor);

 private:
  // SQLite handle for this table. The struct is populated and used by SQLite.
  sqlite3_vtab sqlite_table_;

  // See the corresponding getters for these members' purpose.
  sqlite3_file* const sqlite_file_;
  const int root_page_id_;
  const int page_size_;
  const std::vector<RecoveredColumnSpec> column_specs_;

#if DCHECK_IS_ON()
  // Number of cursors that are still opened.
  std::atomic<int> open_cursor_count_{0};
#endif  // DCHECK_IS_ON()
};

}  // namespace recover
}  // namespace sql

#endif  // SQL_RECOVER_MODULE_TABLE_H_
