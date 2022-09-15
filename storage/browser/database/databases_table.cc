// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/database/databases_table.h"

#include <stdint.h>

#include "base/check.h"
#include "sql/statement.h"

namespace storage {

DatabaseDetails::DatabaseDetails() = default;

DatabaseDetails::DatabaseDetails(const DatabaseDetails& other) = default;

DatabaseDetails::~DatabaseDetails() = default;

bool DatabasesTable::Init() {
  // 'Databases' schema:
  //   id              A unique ID assigned to each database
  //   origin          The originto which the database belongs. This is a
  //                   string that can be used as part of a file name
  //                   (http_webkit.org_0, for example).
  //   name            The database name.
  //   description     A short description of the database.
  //   estimated_size  The estimated size of the database.
  return db_->DoesTableExist("Databases") ||
      (db_->Execute(
           "CREATE TABLE Databases ("
           "id INTEGER PRIMARY KEY AUTOINCREMENT, "
           "origin TEXT NOT NULL, "
           "name TEXT NOT NULL, "
           "description TEXT NOT NULL, "
           "estimated_size INTEGER NOT NULL)") &&
       db_->Execute(
           "CREATE INDEX origin_index ON Databases (origin)") &&
       db_->Execute(
           "CREATE UNIQUE INDEX unique_index ON Databases (origin, name)"));
}

int64_t DatabasesTable::GetDatabaseID(const std::string& origin_identifier,
                                      const std::u16string& database_name) {
  sql::Statement select_statement(db_->GetCachedStatement(
      SQL_FROM_HERE, "SELECT id FROM Databases WHERE origin = ? AND name = ?"));
  select_statement.BindString(0, origin_identifier);
  select_statement.BindString16(1, database_name);

  if (select_statement.Step()) {
    return select_statement.ColumnInt64(0);
  }

  return -1;
}

bool DatabasesTable::GetDatabaseDetails(const std::string& origin_identifier,
                                        const std::u16string& database_name,
                                        DatabaseDetails* details) {
  DCHECK(details);
  sql::Statement select_statement(db_->GetCachedStatement(
      SQL_FROM_HERE, "SELECT description FROM Databases "
                     "WHERE origin = ? AND name = ?"));
  select_statement.BindString(0, origin_identifier);
  select_statement.BindString16(1, database_name);

  if (select_statement.Step()) {
    details->origin_identifier = origin_identifier;
    details->database_name = database_name;
    details->description = select_statement.ColumnString16(0);
    return true;
  }

  return false;
}

bool DatabasesTable::InsertDatabaseDetails(const DatabaseDetails& details) {
  sql::Statement insert_statement(db_->GetCachedStatement(
      SQL_FROM_HERE, "INSERT INTO Databases (origin, name, description, "
                     "estimated_size) VALUES (?, ?, ?, 0)"));
  insert_statement.BindString(0, details.origin_identifier);
  insert_statement.BindString16(1, details.database_name);
  insert_statement.BindString16(2, details.description);

  return insert_statement.Run();
}

bool DatabasesTable::UpdateDatabaseDetails(const DatabaseDetails& details) {
  sql::Statement update_statement(db_->GetCachedStatement(
      SQL_FROM_HERE, "UPDATE Databases SET description = ? "
                     "WHERE origin = ? AND name = ?"));
  update_statement.BindString16(0, details.description);
  update_statement.BindString(1, details.origin_identifier);
  update_statement.BindString16(2, details.database_name);

  return (update_statement.Run() && db_->GetLastChangeCount());
}

bool DatabasesTable::DeleteDatabaseDetails(
    const std::string& origin_identifier,
    const std::u16string& database_name) {
  sql::Statement delete_statement(db_->GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM Databases WHERE origin = ? AND name = ?"));
  delete_statement.BindString(0, origin_identifier);
  delete_statement.BindString16(1, database_name);

  return (delete_statement.Run() && db_->GetLastChangeCount());
}

bool DatabasesTable::GetAllOriginIdentifiers(
    std::vector<std::string>* origin_identifiers) {
  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE, "SELECT DISTINCT origin FROM Databases ORDER BY origin"));

  while (statement.Step())
    origin_identifiers->push_back(statement.ColumnString(0));

  return statement.Succeeded();
}

bool DatabasesTable::GetAllDatabaseDetailsForOriginIdentifier(
    const std::string& origin_identifier,
    std::vector<DatabaseDetails>* details_vector) {
  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE, "SELECT name, description "
                     "FROM Databases WHERE origin = ? ORDER BY name"));
  statement.BindString(0, origin_identifier);

  while (statement.Step()) {
    DatabaseDetails details;
    details.origin_identifier = origin_identifier;
    details.database_name = statement.ColumnString16(0);
    details.description = statement.ColumnString16(1);
    details_vector->push_back(details);
  }

  return statement.Succeeded();
}

bool DatabasesTable::DeleteOriginIdentifier(
    const std::string& origin_identifier) {
  sql::Statement delete_statement(db_->GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM Databases WHERE origin = ?"));
  delete_statement.BindString(0, origin_identifier);

  return (delete_statement.Run() && db_->GetLastChangeCount());
}

}  // namespace storage
