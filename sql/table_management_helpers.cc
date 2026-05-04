// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/table_management_helpers.h"

#include <stdint.h>

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

namespace sql {

bool CreateTable(
    sql::Database& db,
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

bool CreateIndex(sql::Database& db,
                 std::string_view table_name,
                 base::span<const std::string_view> columns) {
  std::string index_name =
      base::StrCat({table_name, "_", base::JoinString(columns, "_")});
  return db.Execute(
      base::StrCat({"CREATE INDEX ", index_name, " ON ", table_name, "(",
                    base::JoinString(columns, ", "), ")"}));
}

void InsertBuilder(sql::Database& db,
                   sql::Statement& statement,
                   std::string_view table_name,
                   base::span<const std::string_view> column_names,
                   bool or_replace) {
  static constexpr std::string_view kPlaceholder = "?";
  DCHECK(!column_names.empty());

  std::string insert_or_replace =
      base::StrCat({"INSERT ", or_replace ? "OR REPLACE " : ""});
  std::string placeholders = base::JoinString(
      std::vector<std::string_view>(column_names.size(), kPlaceholder), ", ");
  statement.Assign(db.GetUniqueStatement(
      base::StrCat({insert_or_replace, "INTO ", table_name, " (",
                    base::JoinString(column_names, ", "), ") VALUES (",
                    placeholders, ")"})));
}

bool RenameTable(sql::Database& db,
                 std::string_view from,
                 std::string_view to) {
  return db.Execute(base::StrCat({"ALTER TABLE ", from, " RENAME TO ", to}));
}

bool AddColumn(sql::Database& db,
               std::string_view table_name,
               std::string_view column_name,
               std::string_view type) {
  return db.Execute(base::StrCat(
      {"ALTER TABLE ", table_name, " ADD COLUMN ", column_name, " ", type}));
}

bool DropColumn(sql::Database& db,
                std::string_view table_name,
                std::string_view column_name) {
  return db.Execute(
      base::StrCat({"ALTER TABLE ", table_name, " DROP COLUMN ", column_name}));
}

void DeleteBuilder(sql::Database& db,
                   sql::Statement& statement,
                   std::string_view table_name,
                   std::string_view where_clause) {
  std::string where =
      where_clause.empty() ? "" : base::StrCat({" WHERE ", where_clause});
  statement.Assign(
      db.GetUniqueStatement(base::StrCat({"DELETE FROM ", table_name, where})));
}

bool DeleteAllRows(sql::Database& db, std::string_view table_name) {
  return DeleteFromTable(db, table_name, /*where_clause=*/"");
}

bool DeleteFromTable(sql::Database& db,
                     std::string_view table_name,
                     std::string_view where_clause) {
  sql::Statement statement;
  DeleteBuilder(db, statement, table_name, where_clause);
  if (!statement.is_valid()) {
    return false;
  }
  return statement.Run();
}

bool DeleteWhereColumnEq(sql::Database& db,
                         std::string_view table_name,
                         std::string_view column,
                         std::string_view value) {
  sql::Statement statement;
  DeleteBuilder(db, statement, table_name, base::StrCat({column, " = ?"}));
  statement.BindString(0, value);
  if (!statement.is_valid()) {
    return false;
  }
  return statement.Run();
}

bool DeleteWhereColumnEq(sql::Database& db,
                         std::string_view table_name,
                         std::string_view column,
                         int64_t value) {
  sql::Statement statement;
  DeleteBuilder(db, statement, table_name, base::StrCat({column, " = ?"}));
  statement.BindInt64(0, value);
  if (!statement.is_valid()) {
    return false;
  }
  return statement.Run();
}

void UpdateBuilder(sql::Database& db,
                   sql::Statement& statement,
                   std::string_view table_name,
                   base::span<const std::string_view> column_names,
                   std::string_view where_clause) {
  DCHECK(!column_names.empty());

  std::string columns_with_placeholders =
      base::StrCat({base::JoinString(column_names, " = ?, "), " = ?"});
  std::string where =
      where_clause.empty() ? "" : base::StrCat({" WHERE ", where_clause});
  statement.Assign(db.GetUniqueStatement(base::StrCat(
      {"UPDATE ", table_name, " SET ", columns_with_placeholders, where})));
}

void SelectBuilder(sql::Database& db,
                   sql::Statement& statement,
                   std::string_view table_name,
                   base::span<const std::string_view> columns,
                   std::string_view modifiers) {
  statement.Assign(db.GetUniqueStatement(
      base::StrCat({"SELECT ", base::JoinString(columns, ", "), " FROM ",
                    table_name, " ", modifiers})));
}

}  // namespace sql
