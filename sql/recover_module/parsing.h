// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_RECOVER_MODULE_PARSING_H_
#define SQL_RECOVER_MODULE_PARSING_H_

#include <string>

namespace sql {
namespace recover {

enum class ValueType;

// The declared data type of a virtual table column.
enum class ModuleColumnType {
  kInteger,
  kFloat,
  kText,
  kBlob,
  kNumeric,
  kRowId,
  kAny,
};

// User-supplied specification for recovering a column in a corrupted table.
struct RecoveredColumnSpec {
  // False if this represents a parsing error.
  bool IsValid() const { return !name.empty(); }
  // Column description suitable for use in a CREATE TABLE statement.
  std::string ToCreateTableSql() const;
  // True if the given value type is admitted by this column specification.
  bool IsAcceptableValue(ValueType value_type) const;

  // Column name reported to the SQLite engine.
  //
  // The empty string is (ab)used for representing invalid column information,
  // which can be used to communicate parsing errors.
  std::string name;
  // The column's canonical type.
  ModuleColumnType type;
  // If true, recovery will skip over null values in this column.
  bool is_non_null = false;
  // If true, recovery will accept values in this column with compatible types.
  bool is_strict = false;
};

// Parses a SQLite module argument that holds a table column specification.
//
// Returns an invalid specification (IsValid() returns false) on parsing errors.
RecoveredColumnSpec ParseColumnSpec(const char* sqlite_arg);

// User-supplied SQL table identifier.
//
// This points to the table whose data is being recovered.
struct TargetTableSpec {
  // False if this represents a parsing error.
  bool IsValid() const { return !table_name.empty(); }

  // The name of the attachment point of the database containing the table.
  std::string db_name;
  // The name of the table. Uniquely identifies a table in a database.
  std::string table_name;
};

// Parses a SQLite module argument that points to a table.
TargetTableSpec ParseTableSpec(const char* sqlite_arg);

}  // namespace recover
}  // namespace sql

#endif  // SQL_RECOVER_MODULE_PARSING_H_
