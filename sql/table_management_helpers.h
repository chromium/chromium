// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_TABLE_MANAGEMENT_HELPERS_H_
#define SQL_TABLE_MANAGEMENT_HELPERS_H_

#include <stdint.h>

#include <string_view>
#include <utility>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "sql/statement_id.h"

namespace sql {

class Database;
class Statement;

// Placeholder for SQL statements, used in builder functions.
inline constexpr std::string_view kPlaceholder = "?";

// Helper functions to construct SQL statements from string constants.
// - Functions with names corresponding to SQL keywords execute the statement
//   directly and return if it was successful.
// - Builder functions only assign the statement, which enables binding
//   values to placeholders before running it.

// Executes a CREATE TABLE statement on `db` which the provided
// `table_name`. The columns are described in `column_names_and_types` as
// pairs of (name, type), where type can include modifiers such as NOT NULL.
// By specifying `compositive_primary_key`, a PRIMARY KEY (col1, col2, ..)
// clause is generated.
// Returns true if successful.
COMPONENT_EXPORT(SQL)
bool CreateTable(
    Database& db,
    std::string_view table_name,
    base::span<const std::pair<const std::string_view, const std::string_view>>
        column_names_and_types,
    base::span<const std::string_view> composite_primary_key = {});

// Creates an index on `table_name` for the provided `columns`.
// The index is named after the table and columns, separated by '_'.
// Returns true if successful.
COMPONENT_EXPORT(SQL)
bool CreateIndex(Database& db,
                 std::string_view table_name,
                 base::span<const std::string_view> columns);

// Initializes `statement` with INSERT INTO `table_name`, with placeholders for
// all `column_names`. By setting `or_replace`, INSERT OR REPLACE INTO is used
// instead.
COMPONENT_EXPORT(SQL)
void InsertBuilder(Database& db,
                   Statement& statement,
                   std::string_view table_name,
                   base::span<const std::string_view> column_names,
                   bool or_replace = false);

// Initializes `statement` with INSERT INTO `table_name`, with placeholders for
// all `column_names`. By setting `or_replace`, INSERT OR REPLACE INTO is used
// instead.
//
// This statement builder uses the statement cache, where `id` should be
// constructed using `SQL_FROM_HERE`, which is based on the source file name and
// line number. For statements which will only be executed once or rarely, use
// `InsertBuilder()` instead.
COMPONENT_EXPORT(SQL)
void CachedInsertBuilder(StatementID id,
                         Database& db,
                         Statement& statement,
                         std::string_view table_name,
                         base::span<const std::string_view> column_names,
                         bool or_replace = false);

// Renames the table `from` into `to` and returns true if successful.
COMPONENT_EXPORT(SQL)
bool RenameTable(Database& db, std::string_view from, std::string_view to);

// Adds a column named `column_name` of `type` to `table_name` and returns true
// if successful.
COMPONENT_EXPORT(SQL)
bool AddColumn(Database& db,
               std::string_view table_name,
               std::string_view column_name,
               std::string_view type);

// Drops the column named `column_name` from `table_name` and returns true if
// successful.
COMPONENT_EXPORT(SQL)
bool DropColumn(Database& db,
                std::string_view table_name,
                std::string_view column_name);

// Initializes `statement` with DELETE FROM `table_name`. A WHERE clause
// can optionally be specified in `where_clause`.
COMPONENT_EXPORT(SQL)
void DeleteBuilder(Database& db,
                   Statement& statement,
                   std::string_view table_name,
                   std::string_view where_clause = "");

// Initializes `statement` with DELETE FROM `table_name`. A WHERE clause
// can optionally be specified in `where_clause`.
//
// This statement builder uses the statement cache, where `id` should be
// constructed using `SQL_FROM_HERE`, which is based on the source file name and
// line number. For statements which will only be executed once or rarely, use
// `DeleteBuilder()` instead.
COMPONENT_EXPORT(SQL)
void CachedDeleteBuilder(StatementID id,
                         Database& db,
                         Statement& statement,
                         std::string_view table_name,
                         std::string_view where_clause = "");

// Deletes all rows from `table_name` and returns true if it was successful.
COMPONENT_EXPORT(SQL)
bool DeleteAllRows(Database& db, std::string_view table_name);

// Like `DeleteBuilder()`, but runs the statement and returns true if it was
// successful. The `where_clause` must be a static string. If you need to
// insert unsafe values, use `DeleteBuilder()` with `Bind()` calls or
// `DeleteWhereColumnEq()`.
COMPONENT_EXPORT(SQL)
bool DeleteFromTable(Database& db,
                     std::string_view table_name,
                     std::string_view where_clause);

// Wrapper around `DeleteFromTable()`, which initializes the where clause as
// `column` = `value`.
// Runs the statement and returns true if it was successful.
COMPONENT_EXPORT(SQL)
bool DeleteWhereColumnEq(Database& db,
                         std::string_view table_name,
                         std::string_view column,
                         std::string_view value);

// Wrapper around `DeleteFromTable()`, which initializes the where clause as
// `column` = `value` for int64_t type.
// Runs the statement and returns true if it was successful.
COMPONENT_EXPORT(SQL)
bool DeleteWhereColumnEq(Database& db,
                         std::string_view table_name,
                         std::string_view column,
                         int64_t value);

// Initializes `statement` with UPDATE `table_name` SET `column_names` = ?, with
// a placeholder for every `column_names`. A WHERE clause can optionally be
// specified in `where_clause`.
COMPONENT_EXPORT(SQL)
void UpdateBuilder(Database& db,
                   Statement& statement,
                   std::string_view table_name,
                   base::span<const std::string_view> column_names,
                   std::string_view where_clause = "");

// Initializes `statement` with UPDATE `table_name` SET `column_names` = ?, with
// a placeholder for every `column_names`. A WHERE clause can optionally be
// specified in `where_clause`.
//
// This statement builder uses the statement cache, where `id` should be
// constructed using `SQL_FROM_HERE`, which is based on the source file name and
// line number. For statements which will only be executed once or rarely, use
// `UpdateBuilder()` instead.
COMPONENT_EXPORT(SQL)
void CachedUpdateBuilder(StatementID id,
                         Database& db,
                         Statement& statement,
                         std::string_view table_name,
                         base::span<const std::string_view> column_names,
                         std::string_view where_clause = "");

// Initializes `statement` with SELECT `columns` FROM `table_name` and
// optionally further `modifiers`, such as WHERE, ORDER BY, etc.
COMPONENT_EXPORT(SQL)
void SelectBuilder(Database& db,
                   Statement& statement,
                   std::string_view table_name,
                   base::span<const std::string_view> columns,
                   std::string_view modifiers = "");

// Initializes `statement` with SELECT `columns` FROM `table_name` and
// optionally further `modifiers`, such as WHERE, ORDER BY, etc.
//
// This statement builder uses the statement cache, where `id` should be
// constructed using `SQL_FROM_HERE`, which is based on the source file name and
// line number. For statements which will only be executed once or rarely, use
// `SelectBuilder()` instead.
COMPONENT_EXPORT(SQL)
void CachedSelectBuilder(StatementID id,
                         Database& db,
                         Statement& statement,
                         std::string_view table_name,
                         base::span<const std::string_view> columns,
                         std::string_view modifiers = "");

}  // namespace sql

#endif  // SQL_TABLE_MANAGEMENT_HELPERS_H_
