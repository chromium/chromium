// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_QUOTA_QUOTA_DATABASE_MIGRATIONS_H_
#define STORAGE_BROWSER_QUOTA_QUOTA_DATABASE_MIGRATIONS_H_

namespace storage {

class QuotaDatabase;

// Helper class of QuotaDatabase which handles the QuotaManager SQL database
// schema migrations. Any change that requires a change in the schema version
// and adds new tables, columns, or modifies existing data should have a
// migration to avoid data loss.
//
// QuotaDatabaseMigrations is a friended class of QuotaDatabase and updates the
// existing SQL QuotaManager database owned by the QuotaDatabase class.
class QuotaDatabaseMigrations {
 public:
  // Upgrades the SQL database owned by `quota_database` to the latest schema,
  // and updates the sql::MetaTable version accordingly.
  static bool UpgradeSchema(QuotaDatabase& quota_database);

 private:
  static void RecordMigrationHistogram(int old_version,
                                       int new_version,
                                       bool success);

  static bool MigrateFromVersion5ToVersion7(QuotaDatabase& quota_database);
  static bool MigrateFromVersion6ToVersion7(QuotaDatabase& quota_database);
  static bool MigrateFromVersion7ToVersion8(QuotaDatabase& quota_database);
  static bool MigrateFromVersion8ToVersion9(QuotaDatabase& quota_database);
  static bool MigrateFromVersion9ToVersion10(QuotaDatabase& quota_database);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_QUOTA_DATABASE_MIGRATIONS_H_
