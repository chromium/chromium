// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/meta_table.h"

#include <cstdint>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/check_op.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/statement_id.h"
#include "sql/transaction.h"

namespace sql {

namespace {

// Keys understood directly by sql:MetaTable.
constexpr char kVersionKey[] = "version";
constexpr char kCompatibleVersionKey[] = "last_compatible_version";
constexpr char kMmapStatusKey[] = "mmap_status";

bool PrepareSetStatement(std::string_view key,
                         Database& db,
                         Statement& insert_statement) {
  insert_statement.Assign(db.GetCachedStatement(
      SQL_FROM_HERE, "INSERT OR REPLACE INTO meta(key,value) VALUES(?,?)"));
  if (!insert_statement.is_valid()) {
    return false;
  }
  insert_statement.BindString(0, key);
  return true;
}

bool PrepareGetStatement(std::string_view key,
                         Database& db,
                         Statement& select_statement) {
  select_statement.Assign(db.GetCachedStatement(
      SQL_FROM_HERE, "SELECT value FROM meta WHERE key=?"));
  if (!select_statement.is_valid())
    return false;

  select_statement.BindString(0, key);
  return select_statement.Step();
}

}  // namespace

MetaTable::MetaTable() = default;

MetaTable::~MetaTable() = default;

// static
constexpr int64_t MetaTable::kMmapFailure;
constexpr int64_t MetaTable::kMmapSuccess;

// static
bool MetaTable::DoesTableExist(sql::Database* db) {
  DCHECK(db);
  return db->DoesTableExist("meta");
}

// static
bool MetaTable::DeleteTableForTesting(sql::Database* db) {
  DCHECK(db);
  return db->Execute("DROP TABLE IF EXISTS meta");
}

// static
bool MetaTable::GetMmapStatus(Database* db, int64_t* status) {
  DCHECK(db);
  DCHECK(status);

  // It is fine for the status to be missing entirely, but any error prevents
  // memory-mapping.
  Statement select;
  if (!PrepareGetStatement(kMmapStatusKey, *db, select)) {
    *status = 0;
    return true;
  }

  *status = select.ColumnInt64(0);
  return select.Succeeded();
}

// static
bool MetaTable::SetMmapStatus(Database* db, int64_t status) {
  DCHECK(db);
  DCHECK(status == kMmapFailure || status == kMmapSuccess || status >= 0);

  Statement insert;
  if (!PrepareSetStatement(kMmapStatusKey, *db, insert)) {
    return false;
  }

  insert.BindInt64(1, status);
  return insert.Run();
}

// static
RazeIfIncompatibleResult MetaTable::RazeIfIncompatible(
    Database* db,
    int lowest_supported_version,
    int current_version) {
  DCHECK(db);

  if (!DoesTableExist(db)) {
    return RazeIfIncompatibleResult::kCompatible;
  }

  sql::Statement select;
  if (!PrepareGetStatement(kVersionKey, *db, select)) {
    return RazeIfIncompatibleResult::kFailed;
  }
  int64_t on_disk_schema_version = select.ColumnInt64(0);

  if (!PrepareGetStatement(kCompatibleVersionKey, *db, select)) {
    return RazeIfIncompatibleResult::kFailed;
  }
  int64_t on_disk_compatible_version = select.ColumnInt(0);

  select.Clear();  // Clear potential automatic transaction for Raze().

  if ((lowest_supported_version != kNoLowestSupportedVersion &&
       lowest_supported_version > on_disk_schema_version) ||
      (current_version < on_disk_compatible_version)) {
    return db->Raze() ? RazeIfIncompatibleResult::kRazedSuccessfully
                      : RazeIfIncompatibleResult::kFailed;
  }
  return RazeIfIncompatibleResult::kCompatible;
}

bool MetaTable::Init(Database* db, int version, int compatible_version) {
  DCHECK(!db_ && db);
  db_ = db;

  // If values stored are nullptr or missing entirely, 0 will be reported.
  // Require new clients to start with a greater initial version.
  DCHECK_GT(version, 0);
  DCHECK_GT(compatible_version, 0);

  // Make sure the table is created and populated atomically.
  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return false;

  if (!DoesTableExist(db)) {
    if (!db_->Execute("CREATE TABLE meta"
                      "(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY, value "
                      "LONGVARCHAR)")) {
      return false;
    }

    // Newly-created databases start out with mmap'ed I/O, but have no place to
    // store the setting.  Set here so that later opens don't need to validate.
    if (!SetMmapStatus(db_, kMmapSuccess)) {
      return false;
    }

    // Note: there is no index over the meta table. We currently only have a
    // couple of keys, so it doesn't matter. If we start storing more stuff in
    // there, we should create an index.

    // If setting either version number fails, return early to avoid likely
    // crashes or incorrect behavior with respect to migrations.
    if (!SetVersionNumber(version) ||
        !SetCompatibleVersionNumber(compatible_version)) {
      return false;
    }
  }
  return transaction.Commit();
}

void MetaTable::Reset() {
  db_ = nullptr;
}

bool MetaTable::SetVersionNumber(int version) {
  DCHECK_GT(version, 0);
  return SetValue(kVersionKey, version);
}

int MetaTable::GetVersionNumber() {
  int64_t version = 0;
  return GetValue(kVersionKey, &version) ? version : 0;
}

bool MetaTable::SetCompatibleVersionNumber(int version) {
  DCHECK_GT(version, 0);
  return SetValue(kCompatibleVersionKey, version);
}

int MetaTable::GetCompatibleVersionNumber() {
  int version = 0;
  return GetValue(kCompatibleVersionKey, &version) ? version : 0;
}

bool MetaTable::SetValue(std::string_view key, const std::string& value) {
  DCHECK(db_);

  Statement insert;
  PrepareSetStatement(key, *db_, insert);
  insert.BindString(1, value);
  return insert.Run();
}

bool MetaTable::SetValue(std::string_view key, int64_t value) {
  DCHECK(db_);

  Statement insert;
  PrepareSetStatement(key, *db_, insert);
  insert.BindInt64(1, value);
  return insert.Run();
}

bool MetaTable::GetValue(std::string_view key, std::string* value) {
  DCHECK(value);
  DCHECK(db_);

  Statement select;
  if (!PrepareGetStatement(key, *db_, select))
    return false;

  *value = select.ColumnString(0);
  return true;
}

bool MetaTable::GetValue(std::string_view key, int* value) {
  DCHECK(value);
  DCHECK(db_);

  Statement select;
  if (!PrepareGetStatement(key, *db_, select))
    return false;

  *value = select.ColumnInt64(0);
  return true;
}

bool MetaTable::GetValue(std::string_view key, int64_t* value) {
  DCHECK(value);
  DCHECK(db_);

  Statement select;
  if (!PrepareGetStatement(key, *db_, select))
    return false;

  *value = select.ColumnInt64(0);
  return true;
}

bool MetaTable::DeleteKey(std::string_view key) {
  DCHECK(db_);

  Statement delete_statement(
      db_->GetUniqueStatement("DELETE FROM meta WHERE key=?"));
  delete_statement.BindString(0, key);
  return delete_statement.Run();
}

}  // namespace sql
