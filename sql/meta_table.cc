// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/meta_table.h"

#include <stdint.h>

#include "base/check_op.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace {

// Keys understood directly by sql:MetaTable.
const char kVersionKey[] = "version";
const char kCompatibleVersionKey[] = "last_compatible_version";
const char kMmapStatusKey[] = "mmap_status";

}  // namespace

namespace sql {

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
  const char* kMmapStatusSql = "SELECT value FROM meta WHERE key = ?";
  Statement s(db->GetUniqueStatement(kMmapStatusSql));
  if (!s.is_valid())
    return false;

  // It is fine for the status to be missing entirely, but any error prevents
  // memory-mapping.
  s.BindString(0, kMmapStatusKey);
  *status = s.Step() ? s.ColumnInt64(0) : 0;
  return s.Succeeded();
}

// static
bool MetaTable::SetMmapStatus(Database* db, int64_t status) {
  DCHECK(status == kMmapFailure || status == kMmapSuccess || status >= 0);

  const char* kMmapUpdateStatusSql = "REPLACE INTO meta VALUES (?, ?)";
  Statement s(db->GetUniqueStatement(kMmapUpdateStatusSql));
  s.BindString(0, kMmapStatusKey);
  s.BindInt64(1, status);
  return s.Run();
}

// static
void MetaTable::RazeIfIncompatible(Database* db,
                                   int lowest_supported_version,
                                   int current_version) {
  if (!sql::MetaTable::DoesTableExist(db))
    return;

  // TODO(crbug.com/1228463): Share sql with PrepareGetStatement().
  sql::Statement s(
      db->GetUniqueStatement("SELECT value FROM meta WHERE key=?"));
  s.BindCString(0, kVersionKey);
  if (!s.Step())
    return;
  int on_disk_schema_version = s.ColumnInt(0);

  s.Assign(db->GetUniqueStatement("SELECT value FROM meta WHERE key=?"));
  s.BindCString(0, kCompatibleVersionKey);
  if (!s.Step())
    return;
  int on_disk_compatible_version = s.ColumnInt(0);

  s.Clear();  // Clear potential automatic transaction for Raze().

  if ((lowest_supported_version != kNoLowestSupportedVersion &&
       lowest_supported_version > on_disk_schema_version) ||
      (current_version < on_disk_compatible_version)) {
    db->Raze();
    return;
  }
}

bool MetaTable::Init(Database* db, int version, int compatible_version) {
  DCHECK(!db_ && db);
  db_ = db;

  // If values stored are nullptr or missing entirely, 0 will be reported.
  // Require new clients to start with a greater initial version.
  DCHECK_GT(version, 0);
  DCHECK_GT(compatible_version, 0);

  // Make sure the table is created an populated atomically.
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
    SetMmapStatus(db_, kMmapSuccess);

    // Note: there is no index over the meta table. We currently only have a
    // couple of keys, so it doesn't matter. If we start storing more stuff in
    // there, we should create an index.
    SetVersionNumber(version);
    SetCompatibleVersionNumber(compatible_version);
  }
  return transaction.Commit();
}

void MetaTable::Reset() {
  db_ = nullptr;
}

void MetaTable::SetVersionNumber(int version) {
  DCHECK_GT(version, 0);
  SetValue(kVersionKey, version);
}

int MetaTable::GetVersionNumber() {
  int version = 0;
  return GetValue(kVersionKey, &version) ? version : 0;
}

void MetaTable::SetCompatibleVersionNumber(int version) {
  DCHECK_GT(version, 0);
  SetValue(kCompatibleVersionKey, version);
}

int MetaTable::GetCompatibleVersionNumber() {
  int version = 0;
  return GetValue(kCompatibleVersionKey, &version) ? version : 0;
}

bool MetaTable::SetValue(const char* key, const std::string& value) {
  Statement s;
  PrepareSetStatement(&s, key);
  s.BindString(1, value);
  return s.Run();
}

bool MetaTable::SetValue(const char* key, int value) {
  Statement s;
  PrepareSetStatement(&s, key);
  s.BindInt(1, value);
  return s.Run();
}

bool MetaTable::SetValue(const char* key, int64_t value) {
  Statement s;
  PrepareSetStatement(&s, key);
  s.BindInt64(1, value);
  return s.Run();
}

bool MetaTable::GetValue(const char* key, std::string* value) {
  Statement s;
  if (!PrepareGetStatement(&s, key))
    return false;

  *value = s.ColumnString(0);
  return true;
}

bool MetaTable::GetValue(const char* key, int* value) {
  Statement s;
  if (!PrepareGetStatement(&s, key))
    return false;

  *value = s.ColumnInt(0);
  return true;
}

bool MetaTable::GetValue(const char* key, int64_t* value) {
  Statement s;
  if (!PrepareGetStatement(&s, key))
    return false;

  *value = s.ColumnInt64(0);
  return true;
}

bool MetaTable::DeleteKey(const char* key) {
  DCHECK(db_);
  Statement s(db_->GetUniqueStatement("DELETE FROM meta WHERE key=?"));
  s.BindCString(0, key);
  return s.Run();
}

void MetaTable::PrepareSetStatement(Statement* statement, const char* key) {
  DCHECK(db_ && statement);
  statement->Assign(db_->GetCachedStatement(
      SQL_FROM_HERE, "INSERT OR REPLACE INTO meta (key,value) VALUES (?,?)"));
  statement->BindCString(0, key);
}

bool MetaTable::PrepareGetStatement(Statement* statement, const char* key) {
  DCHECK(db_ && statement);
  statement->Assign(db_->GetCachedStatement(
      SQL_FROM_HERE, "SELECT value FROM meta WHERE key=?"));
  statement->BindCString(0, key);
  return statement->Step();
}

}  // namespace sql
