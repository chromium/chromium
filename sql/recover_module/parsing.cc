// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/recover_module/parsing.h"

#include <cstddef>
#include <tuple>
#include <utility>

#include "base/logging.h"
#include "base/optional.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "sql/recover_module/record.h"

namespace sql {
namespace recover {

namespace {

// This module defines whitespace as space (0x20).
constexpr bool IsWhiteSpace(char character) {
  return (character == ' ');
}

// Splits a token out of a SQL string representing a column type.
//
// Tokens are separated by space (0x20) characters.
//
// The SQL must not start with a space character.
//
// Returns the token and the rest of the SQL string. Consumes the space after
// the returned token -- the rest of the SQL string will not start with space.
std::pair<base::StringPiece, base::StringPiece> SplitToken(
    base::StringPiece sql) {
  DCHECK(sql.empty() || !IsWhiteSpace(sql[0]));

  size_t token_end = 0;
  while (token_end < sql.length() && !IsWhiteSpace(sql[token_end]))
    ++token_end;

  size_t split = token_end;
  while (split < sql.length() && IsWhiteSpace(sql[split]))
    ++split;

  return {sql.substr(0, token_end), sql.substr(split)};
}

// Column types.
constexpr base::StringPiece kIntegerSql("INTEGER");
constexpr base::StringPiece kFloatSql("FLOAT");
constexpr base::StringPiece kTextSql("TEXT");
constexpr base::StringPiece kBlobSql("BLOB");
constexpr base::StringPiece kNumericSql("NUMERIC");
constexpr base::StringPiece kRowidSql("ROWID");
constexpr base::StringPiece kAnySql("ANY");

// SQL keywords recognized by the recovery module.
constexpr base::StringPiece kStrictSql("STRICT");
constexpr base::StringPiece kNonNullSql1("NOT");
constexpr base::StringPiece kNonNullSql2("NULL");

base::Optional<ModuleColumnType> ParseColumnType(
    base::StringPiece column_type_sql) {
  if (column_type_sql == kIntegerSql)
    return ModuleColumnType::kInteger;
  if (column_type_sql == kFloatSql)
    return ModuleColumnType::kFloat;
  if (column_type_sql == kTextSql)
    return ModuleColumnType::kText;
  if (column_type_sql == kBlobSql)
    return ModuleColumnType::kBlob;
  if (column_type_sql == kNumericSql)
    return ModuleColumnType::kNumeric;
  if (column_type_sql == kRowidSql)
    return ModuleColumnType::kRowId;
  if (column_type_sql == kAnySql)
    return ModuleColumnType::kAny;

  return base::nullopt;
}

// Returns a view into a SQL string representing the column type.
//
// The backing string is guaranteed to live for as long as the process runs.
base::StringPiece ColumnTypeToSql(ModuleColumnType column_type) {
  switch (column_type) {
    case ModuleColumnType::kInteger:
      return kIntegerSql;
    case ModuleColumnType::kFloat:
      return kFloatSql;
    case ModuleColumnType::kText:
      return kTextSql;
    case ModuleColumnType::kBlob:
      return kBlobSql;
    case ModuleColumnType::kNumeric:
      return kNumericSql;
    case ModuleColumnType::kRowId:
      return kIntegerSql;  // rowids are ints.
    case ModuleColumnType::kAny:
      return base::StringPiece();
  }
  NOTREACHED();
}

}  // namespace

std::string RecoveredColumnSpec::ToCreateTableSql() const {
  base::StringPiece not_null_sql = (is_non_null) ? " NOT NULL" : "";
  return base::StrCat(
      {this->name, " ", ColumnTypeToSql(this->type), not_null_sql});
}

bool RecoveredColumnSpec::IsAcceptableValue(ValueType value_type) const {
  if (value_type == ValueType::kNull)
    return !is_non_null || type == ModuleColumnType::kRowId;
  if (type == ModuleColumnType::kAny)
    return true;

  if (value_type == ValueType::kInteger) {
    return type == ModuleColumnType::kInteger ||
           (!is_strict && type == ModuleColumnType::kFloat);
  }
  if (value_type == ValueType::kFloat) {
    return type == ModuleColumnType::kFloat ||
           (!is_strict && type == ModuleColumnType::kFloat);
  }
  if (value_type == ValueType::kText)
    return type == ModuleColumnType::kText;
  if (value_type == ValueType::kBlob) {
    return type == ModuleColumnType::kBlob ||
           (!is_strict && type == ModuleColumnType::kText);
  }
  NOTREACHED() << "Unimplemented value type";
  return false;
}

RecoveredColumnSpec ParseColumnSpec(const char* sqlite_arg) {
  // The result is invalid until its |name| member is set.
  RecoveredColumnSpec result;
  base::StringPiece sql(sqlite_arg);

  base::StringPiece column_name;
  std::tie(column_name, sql) = SplitToken(sql);
  if (column_name.empty()) {
    // Empty column names are invalid.
    DCHECK(!result.IsValid());
    return result;
  }

  base::StringPiece column_type_sql;
  std::tie(column_type_sql, sql) = SplitToken(sql);
  base::Optional<ModuleColumnType> column_type =
      ParseColumnType(column_type_sql);
  if (!column_type.has_value()) {
    // Invalid column type.
    DCHECK(!result.IsValid());
    return result;
  }
  result.type = column_type.value();

  // Consume keywords.
  result.is_non_null = result.type == ModuleColumnType::kRowId;
  while (!sql.empty()) {
    base::StringPiece token;
    std::tie(token, sql) = SplitToken(sql);

    if (token == kStrictSql) {
      if (result.type == ModuleColumnType::kAny) {
        // STRICT is incompatible with ANY.
        DCHECK(!result.IsValid());
        return result;
      }

      result.is_strict = true;
      continue;
    }

    if (token != kNonNullSql1) {
      // Invalid SQL keyword.
      DCHECK(!result.IsValid());
      return result;
    }
    std::tie(token, sql) = SplitToken(sql);
    if (token != kNonNullSql2) {
      // Invalid SQL keyword.
      DCHECK(!result.IsValid());
      return result;
    }
    result.is_non_null = true;
  }

  result.name = column_name.as_string();
  return result;
}

TargetTableSpec ParseTableSpec(const char* sqlite_arg) {
  base::StringPiece sql(sqlite_arg);

  size_t separator_pos = sql.find('.');
  if (separator_pos == base::StringPiece::npos) {
    // The default attachment point name is "main".
    return TargetTableSpec{"main", sqlite_arg};
  }

  if (separator_pos == 0) {
    // Empty attachment point names are invalid.
    return TargetTableSpec();
  }

  base::StringPiece db_name = sql.substr(0, separator_pos);
  base::StringPiece table_name = sql.substr(separator_pos + 1);
  return TargetTableSpec{db_name.as_string(), table_name.as_string()};
}

}  // namespace recover
}  // namespace sql
