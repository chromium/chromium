// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_STORAGE_SQL_MIGRATIONS_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_STORAGE_SQL_MIGRATIONS_H_

namespace sql {
class Database;
class MetaTable;
}  // namespace sql

namespace content {

class AttributionStorageSql;

// Changes to the SQL database schema or data format must be accompanied by
// a database migration. This includes new columns, new tables, or changes to
// existing stored data. Data loss must be avoided during migrations, because
// the impact could extend to weeks of conversion data.
//
// To do a migration, add a new function `MigrateToVersionN()` which performs
// the modifications to the old database, and increment `kCurrentVersionNumber`
// in attribution_storage_sql.cc.
//
// Generate a new sql file which will hold the new database schema:
//  * Build and open the Chromium executable
//  * Go to a site which registers an impression to init the database.
//  * Build the sqlite_shell executable:
//      > autoninja -C out/Default sqlite_shell
//  * Using the sqlite_shell executable do the following:
//    > out/Default/sqlite_shell
//    >> .open $USER_DATA_DIR/Default/Conversions
//    >> .output version_nn.sql
//    >> .dump
//  * Replace any rows with relevant test data needed for below.
//  * Add newlines between statements and place in
//  `content/test/data/attribution_reporting/databases/`.
//
// Add a new test to `attribution_storage_sql_migration_unittest.cc` named
// "MigrateVersionNToCurrent" where N is the previous database version. Update
// `AttributionStorageSqlMigrationsTest::GetCurrentSchema()` to use the sql file
// generated above.

// Upgrades |db| to the latest schema, and updates the version stored in
// |meta_table| accordingly. Must be called with an open |db|. Returns false on
// failure.
[[nodiscard]] bool UpgradeAttributionStorageSqlSchema(
    AttributionStorageSql& storage,
    sql::Database& db,
    sql::MetaTable& meta_table);

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_STORAGE_SQL_MIGRATIONS_H_
