// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/recover_module/module.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "sql/recover_module/cursor.h"
#include "sql/recover_module/parsing.h"
#include "sql/recover_module/table.h"
#include "third_party/sqlite/sqlite3.h"

// https://sqlite.org/vtab.html documents SQLite's virtual table module API.

namespace sql {
namespace recover {

namespace {

// SQLite module argument constants.
static constexpr int kModuleNameArgument = 0;
static constexpr int kVirtualTableDbNameArgument = 1;
static constexpr int kVirtualTableNameArgument = 2;
static constexpr int kBackingTableSpecArgument = 3;
static constexpr int kFirstColumnArgument = 4;

// Returns an empty vector on parse errors.
std::vector<RecoveredColumnSpec> ParseColumnSpecs(int argc,
                                                  const char* const* argv) {
  std::vector<RecoveredColumnSpec> result;
  DCHECK_GE(argc, kFirstColumnArgument);
  result.reserve(argc - kFirstColumnArgument + 1);

  for (int i = kFirstColumnArgument; i < argc; ++i) {
    result.emplace_back(ParseColumnSpec(argv[i]));
    if (!result.back().IsValid()) {
      result.clear();
      break;
    }
  }
  return result;
}

int ModuleCreate(sqlite3* sqlite_db,
                 void* /* client_data */,
                 int argc,
                 const char* const* argv,
                 sqlite3_vtab** result_sqlite_table,
                 char** /* error_string */) {
  DCHECK(sqlite_db != nullptr);
  if (argc <= kFirstColumnArgument) {
    // The recovery module needs at least one column specification.
    return SQLITE_MISUSE;
  }
  DCHECK(argv != nullptr);
  DCHECK(result_sqlite_table != nullptr);

  // This module is always expected to be registered as "recover".
  DCHECK_EQ("recover", base::StringPiece(argv[kModuleNameArgument]));

  base::StringPiece db_name(argv[kVirtualTableDbNameArgument]);
  if (db_name != "temp") {
    // Refuse to create tables outside the "temp" database.
    //
    // This check is overly strict. The virtual table can be safely used on any
    // temporary database (ATTACH '' AS other_temp). However, there is no easy
    // way to determine if an attachment point corresponds to a temporary
    // database, and "temp" is sufficient for Chrome's purposes.
    return SQLITE_MISUSE;
  }

  base::StringPiece table_name(argv[kVirtualTableNameArgument]);
  if (!table_name.starts_with("recover_")) {
    // In the future, we may deploy UMA metrics that use the virtual table name
    // to attribute recovery events to Chrome features. In preparation for that
    // future, require all recovery table names to start with "recover_".
    return SQLITE_MISUSE;
  }

  TargetTableSpec backing_table_spec =
      ParseTableSpec(argv[kBackingTableSpecArgument]);
  if (!backing_table_spec.IsValid()) {
    // The parser concluded that the string specifying the backing table is
    // invalid. This is definitely an error in the SQL using the virtual table.
    return SQLITE_MISUSE;
  }

  std::vector<RecoveredColumnSpec> column_specs = ParseColumnSpecs(argc, argv);
  if (column_specs.empty()) {
    // The column specifications were invalid.
    return SQLITE_MISUSE;
  }

  int sqlite_status;
  std::unique_ptr<VirtualTable> table;
  std::tie(sqlite_status, table) = VirtualTable::Create(
      sqlite_db, std::move(backing_table_spec), std::move(column_specs));
  if (sqlite_status != SQLITE_OK)
    return sqlite_status;

  {
    std::string create_table_sql = table->ToCreateTableSql();
    sqlite3_declare_vtab(sqlite_db, create_table_sql.c_str());
  }
  *result_sqlite_table = table->SqliteTable();
  table.release();  // SQLite manages the lifetime of the table.
  return SQLITE_OK;
}

int ModuleConnect(sqlite3* sqlite_db,
                  void* client_data,
                  int argc,
                  const char* const* argv,
                  sqlite3_vtab** result_sqlite_table,
                  char** error_string) {
  // TODO(pwnall): Figure out if it's acceptable to have "recover" be an
  //               eponymous table. If so, use ModuleCreate instead of
  //               ModuleConnect in the entry point table.
  return ModuleCreate(sqlite_db, client_data, argc, argv, result_sqlite_table,
                      error_string);
}

int ModuleBestIndex(sqlite3_vtab* sqlite_table,
                    sqlite3_index_info* index_info) {
  DCHECK(sqlite_table != nullptr);
  DCHECK(index_info != nullptr);

  // The sqlite3_index_info structure is also documented at
  //   https://www.sqlite.org/draft/c3ref/index_info.html
  for (int i = 0; i < index_info->nConstraint; ++i) {
    if (index_info->aConstraint[i].usable == static_cast<char>(false))
      continue;
    // True asks SQLite to evaluate the constraint and pass the result to any
    // follow-up xFilter() calls, via argc/argv.
    index_info->aConstraintUsage[i].argvIndex = 0;
    // True indicates that the virtual table will check the constraint.
    index_info->aConstraintUsage[i].omit = false;
  }
  index_info->orderByConsumed = static_cast<int>(false);

  // SQLite saves the sqlite_idx_info fields set here and passes the values to
  // xFilter().
  index_info->idxStr = nullptr;
  index_info->idxNum = 0;
  index_info->needToFreeIdxStr = static_cast<int>(false);

  return SQLITE_OK;
}

int ModuleDisconnect(sqlite3_vtab* sqlite_table) {
  DCHECK(sqlite_table != nullptr);

  // SQLite takes ownership of the VirtualTable (which is passed around as a
  // sqlite_table) in ModuleCreate() / ModuleConnect(). SQLite then calls
  // ModuleDestroy() / ModuleDisconnect() to relinquish ownership of the
  // VirtualTable. At this point, the table will not be used again, and can be
  // destroyed.
  VirtualTable* const table = VirtualTable::FromSqliteTable(sqlite_table);
  delete table;
  return SQLITE_OK;
}

int ModuleDestroy(sqlite3_vtab* sqlite_table) {
  return ModuleDisconnect(sqlite_table);
}

int ModuleOpen(sqlite3_vtab* sqlite_table,
               sqlite3_vtab_cursor** result_sqlite_cursor) {
  DCHECK(sqlite_table != nullptr);
  DCHECK(result_sqlite_cursor != nullptr);

  VirtualTable* const table = VirtualTable::FromSqliteTable(sqlite_table);
  VirtualCursor* const cursor = table->CreateCursor();
  *result_sqlite_cursor = cursor->SqliteCursor();
  return SQLITE_OK;
}

int ModuleClose(sqlite3_vtab_cursor* sqlite_cursor) {
  DCHECK(sqlite_cursor != nullptr);

  // SQLite takes ownership of the VirtualCursor (which is passed around as a
  // sqlite_cursor) in ModuleOpen(). SQLite then calls ModuleClose() to
  // relinquish ownership of the VirtualCursor. At this point, the cursor will
  // not be used again, and can be destroyed.
  VirtualCursor* const cursor = VirtualCursor::FromSqliteCursor(sqlite_cursor);
  delete cursor;
  return SQLITE_OK;
}

int ModuleFilter(sqlite3_vtab_cursor* sqlite_cursor,
                 int /* best_index_num */,
                 const char* /* best_index_str */,
                 int /* argc */,
                 sqlite3_value** /* argv */) {
  DCHECK(sqlite_cursor != nullptr);

  VirtualCursor* const cursor = VirtualCursor::FromSqliteCursor(sqlite_cursor);
  return cursor->First();
}

int ModuleNext(sqlite3_vtab_cursor* sqlite_cursor) {
  DCHECK(sqlite_cursor != nullptr);

  VirtualCursor* const cursor = VirtualCursor::FromSqliteCursor(sqlite_cursor);
  return cursor->Next();
}

int ModuleEof(sqlite3_vtab_cursor* sqlite_cursor) {
  DCHECK(sqlite_cursor != nullptr);

  VirtualCursor* const cursor = VirtualCursor::FromSqliteCursor(sqlite_cursor);
  return cursor->IsValid() ? 0 : 1;
}

int ModuleColumn(sqlite3_vtab_cursor* sqlite_cursor,
                 sqlite3_context* result_context,
                 int column_index) {
  DCHECK(sqlite_cursor != nullptr);
  DCHECK(result_context != nullptr);

  VirtualCursor* const cursor = VirtualCursor::FromSqliteCursor(sqlite_cursor);
  DCHECK(cursor->IsValid()) << "SQLite called xRowid() without a valid cursor";
  return cursor->ReadColumn(column_index, result_context);
}

int ModuleRowid(sqlite3_vtab_cursor* sqlite_cursor,
                sqlite3_int64* result_rowid) {
  DCHECK(sqlite_cursor != nullptr);
  DCHECK(result_rowid != nullptr);

  VirtualCursor* const cursor = VirtualCursor::FromSqliteCursor(sqlite_cursor);
  DCHECK(cursor->IsValid()) << "SQLite called xRowid() without a valid cursor";
  *result_rowid = cursor->RowId();
  return SQLITE_OK;
}

// SQLite module API version supported by this implementation.
constexpr int kSqliteModuleApiVersion = 1;

// Entry points to the SQLite module.
constexpr sqlite3_module kSqliteModule = {
    kSqliteModuleApiVersion,
    &ModuleCreate,
    &ModuleConnect,
    &ModuleBestIndex,
    &ModuleDisconnect,
    &ModuleDestroy,
    &ModuleOpen,
    &ModuleClose,
    &ModuleFilter,
    &ModuleNext,
    &ModuleEof,
    &ModuleColumn,
    &ModuleRowid,
    /* xUpdate= */ nullptr,
    /* xBegin= */ nullptr,
    /* xSync= */ nullptr,
    /* xCommit= */ nullptr,
    /* xRollback= */ nullptr,
    /* xFindFunction= */ nullptr,
    /* xRename= */ nullptr,
};

}  // namespace

int RegisterRecoverExtension(sqlite3* db) {
  return sqlite3_create_module_v2(db, "recover", &kSqliteModule,
                                  /* pClientData= */ nullptr,
                                  /* xDestroy= */ nullptr);
}

}  // namespace recover
}  // namespace sql
