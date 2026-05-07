// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/table_management_helpers.h"

#include <stdint.h>

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/statement_id.h"

namespace sql {

namespace {

void InsertBuilderInternal(Database& db,
                           Statement& statement,
                           std::string_view table_name,
                           base::span<const std::string_view> column_names,
                           bool or_replace,
                           std::optional<StatementID> optional_id) {
  DCHECK(!column_names.empty());

  std::string insert_or_replace =
      base::StrCat({"INSERT ", or_replace ? "OR REPLACE " : ""});
  std::string placeholders = base::JoinString(
      std::vector<std::string_view>(column_names.size(), kPlaceholder), ", ");
  std::string statement_string = base::StrCat(
      {insert_or_replace, "INTO ", table_name, " (",
       base::JoinString(column_names, ", "), ") VALUES (", placeholders, ")"});
  if (optional_id.has_value()) {
    statement.Assign(db.GetCachedStatement(*optional_id, statement_string));
  } else {
    statement.Assign(db.GetUniqueStatement(statement_string));
  }
}

void DeleteBuilderInternal(Database& db,
                           Statement& statement,
                           std::string_view table_name,
                           std::string_view where_clause,
                           std::optional<StatementID> optional_id) {
  std::string where =
      where_clause.empty() ? "" : base::StrCat({" WHERE ", where_clause});
  std::string statement_string =
      base::StrCat({"DELETE FROM ", table_name, where});
  if (optional_id.has_value()) {
    statement.Assign(db.GetCachedStatement(*optional_id, statement_string));
  } else {
    statement.Assign(db.GetUniqueStatement(statement_string));
  }
}

void UpdateBuilderInternal(Database& db,
                           Statement& statement,
                           std::string_view table_name,
                           base::span<const std::string_view> column_names,
                           std::string_view where_clause,
                           std::optional<StatementID> optional_id) {
  DCHECK(!column_names.empty());

  std::string columns_with_placeholders =
      base::StrCat({base::JoinString(column_names, " = ?, "), " = ?"});
  std::string where =
      where_clause.empty() ? "" : base::StrCat({" WHERE ", where_clause});
  std::string statement_string = base::StrCat(
      {"UPDATE ", table_name, " SET ", columns_with_placeholders, where});
  if (optional_id.has_value()) {
    statement.Assign(db.GetCachedStatement(*optional_id, statement_string));
  } else {
    statement.Assign(db.GetUniqueStatement(statement_string));
  }
}

void SelectBuilderInternal(Database& db,
                           Statement& statement,
                           std::string_view table_name,
                           base::span<const std::string_view> columns,
                           std::string_view modifiers,
                           std::optional<StatementID> optional_id) {
  DCHECK(!columns.empty());

  std::string statement_string =
      base::StrCat({"SELECT ", base::JoinString(columns, ", "), " FROM ",
                    table_name, " ", modifiers});
  if (optional_id.has_value()) {
    statement.Assign(db.GetCachedStatement(*optional_id, statement_string));
  } else {
    statement.Assign(db.GetUniqueStatement(statement_string));
  }
}

}  // namespace

bool CreateTable(
    Database& db,
    std::string_view table_name,
    base::span<const std::pair<const std::string_view, const std::string_view>>
        column_names_and_types,
    base::span<const std::string_view> composite_primary_key) {
  DCHECK(composite_primary_key.empty() || composite_primary_key.size() >= 2);

  std::vector<std::string> combined_names_and_types;
  combined_names_and_types.reserve(column_names_and_types.size());
  for (const auto& [name, type] : column_names_and_types) {
    combined_names_and_types.push_back(base::StrCat({name, " ", type}));
  }

  std::string primary_key_clause =
      composite_primary_key.empty()
          ? ""
          : base::StrCat({", PRIMARY KEY (",
                          base::JoinString(composite_primary_key, ", "), ")"});

  return db.Execute(
      base::StrCat({"CREATE TABLE ", table_name, " (",
                    base::JoinString(combined_names_and_types, ", "),
                    primary_key_clause, ")"}));
}

bool CreateIndex(Database& db,
                 std::string_view table_name,
                 base::span<const std::string_view> columns) {
  std::string index_name =
      base::StrCat({table_name, "_", base::JoinString(columns, "_")});
  return db.Execute(
      base::StrCat({"CREATE INDEX ", index_name, " ON ", table_name, "(",
                    base::JoinString(columns, ", "), ")"}));
}

void InsertBuilder(Database& db,
                   Statement& statement,
                   std::string_view table_name,
                   base::span<const std::string_view> column_names,
                   bool or_replace) {
  InsertBuilderInternal(db, statement, table_name, column_names, or_replace,
                        /*optional_id=*/std::nullopt);
}

void CachedInsertBuilder(StatementID id,
                         Database& db,
                         Statement& statement,
                         std::string_view table_name,
                         base::span<const std::string_view> column_names,
                         bool or_replace) {
  InsertBuilderInternal(db, statement, table_name, column_names, or_replace,
                        id);
}

bool RenameTable(Database& db, std::string_view from, std::string_view to) {
  return db.Execute(base::StrCat({"ALTER TABLE ", from, " RENAME TO ", to}));
}

bool AddColumn(Database& db,
               std::string_view table_name,
               std::string_view column_name,
               std::string_view type) {
  return db.Execute(base::StrCat(
      {"ALTER TABLE ", table_name, " ADD COLUMN ", column_name, " ", type}));
}

bool DropColumn(Database& db,
                std::string_view table_name,
                std::string_view column_name) {
  return db.Execute(
      base::StrCat({"ALTER TABLE ", table_name, " DROP COLUMN ", column_name}));
}

void DeleteBuilder(Database& db,
                   Statement& statement,
                   std::string_view table_name,
                   std::string_view where_clause) {
  DeleteBuilderInternal(db, statement, table_name, where_clause,
                        /*optional_id=*/std::nullopt);
}

void CachedDeleteBuilder(StatementID id,
                         Database& db,
                         Statement& statement,
                         std::string_view table_name,
                         std::string_view where_clause) {
  DeleteBuilderInternal(db, statement, table_name, where_clause, id);
}

bool DeleteAllRows(Database& db, std::string_view table_name) {
  return DeleteFromTable(db, table_name, /*where_clause=*/"");
}

bool DeleteFromTable(Database& db,
                     std::string_view table_name,
                     std::string_view where_clause) {
  Statement statement;
  DeleteBuilder(db, statement, table_name, where_clause);
  if (!statement.is_valid()) {
    return false;
  }
  return statement.Run();
}

bool DeleteWhereColumnEq(Database& db,
                         std::string_view table_name,
                         std::string_view column,
                         std::string_view value) {
  Statement statement;
  DeleteBuilder(db, statement, table_name, base::StrCat({column, " = ?"}));
  statement.BindString(0, value);
  if (!statement.is_valid()) {
    return false;
  }
  return statement.Run();
}

bool DeleteWhereColumnEq(Database& db,
                         std::string_view table_name,
                         std::string_view column,
                         int64_t value) {
  Statement statement;
  DeleteBuilder(db, statement, table_name, base::StrCat({column, " = ?"}));
  statement.BindInt64(0, value);
  if (!statement.is_valid()) {
    return false;
  }
  return statement.Run();
}

void UpdateBuilder(Database& db,
                   Statement& statement,
                   std::string_view table_name,
                   base::span<const std::string_view> column_names,
                   std::string_view where_clause) {
  UpdateBuilderInternal(db, statement, table_name, column_names, where_clause,
                        /*optional_id=*/std::nullopt);
}

void CachedUpdateBuilder(StatementID id,
                         Database& db,
                         Statement& statement,
                         std::string_view table_name,
                         base::span<const std::string_view> column_names,
                         std::string_view where_clause) {
  UpdateBuilderInternal(db, statement, table_name, column_names, where_clause,
                        id);
}

void SelectBuilder(Database& db,
                   Statement& statement,
                   std::string_view table_name,
                   base::span<const std::string_view> columns,
                   std::string_view modifiers) {
  SelectBuilderInternal(db, statement, table_name, columns, modifiers,
                        /*optional_id=*/std::nullopt);
}

void CachedSelectBuilder(StatementID id,
                         Database& db,
                         Statement& statement,
                         std::string_view table_name,
                         base::span<const std::string_view> columns,
                         std::string_view modifiers) {
  SelectBuilderInternal(db, statement, table_name, columns, modifiers, id);
}

}  // namespace sql
