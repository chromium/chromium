// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBDATA_COMMON_WEB_DATABASE_H_
#define COMPONENTS_WEBDATA_COMMON_WEB_DATABASE_H_

#include <map>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "components/webdata/common/web_database_table.h"
#include "components/webdata/common/webdata_export.h"
#include "sql/database.h"
#include "sql/init_status.h"
#include "sql/meta_table.h"

namespace os_crypt_async {
class Encryptor;
}

// This class manages a SQLite database that stores various web page meta data.
class WEBDATA_EXPORT WebDatabase {
 public:
  enum State {
    COMMIT_NOT_NEEDED,
    COMMIT_NEEDED
  };

  // Current database version number.
  //
  // Note: when changing the current version number, corresponding changes must
  // happen in the unit tests, and new migration test added to
  // `WebDatabaseMigrationTest`.
  static constexpr int kCurrentVersionNumber = 134;

  // To support users who are upgrading from older versions of Chrome, we enable
  // migrating from any database version newer than `kDeprecatedVersionNumber`.
  // If an upgrading user has a database version of `kDeprecatedVersionNumber`
  // or lower, their database will be fully deleted and recreated instead
  // (losing all data previously in it).
  //
  // To determine this migration window, we support the same Chrome versions
  // that Chrome Sync does. Any database version that was added before the
  // oldest Chrome version that sync supports can be dropped from the Chromium
  // codebase (i.e., increment `kDeprecatedVersionNumber` and remove related
  // tests + support files).
  //
  // Note the difference between database version and Chrome version! To
  // determine the database version for a given Chrome version, check out the
  // git branch for the Chrome version, and look at `kCurrentVersionNumber` in
  // that branch.
  //
  // To determine the versions of Chrome that Chrome Sync supports, see
  // `max_client_version_to_reject` in server_chrome_sync_config.proto (internal
  // only).
  static constexpr int kDeprecatedVersionNumber = 82;

  // Use this as a path to create an in-memory database.
  static const base::FilePath::CharType kInMemoryPath[];

  WebDatabase();

  WebDatabase(const WebDatabase&) = delete;
  WebDatabase& operator=(const WebDatabase&) = delete;

  virtual ~WebDatabase();

  // Adds a database table. Ownership remains with the caller, which
  // must ensure that the lifetime of |table| exceeds this object's
  // lifetime. Must only be called before Init.
  void AddTable(WebDatabaseTable* table);

  // Retrieves a table based on its |key|.
  WebDatabaseTable* GetTable(WebDatabaseTable::TypeKey key);

  // Call before Init() to set the error callback to be used for the
  // underlying database connection.
  void set_error_callback(const sql::Database::ErrorCallback& error_callback) {
    db_.set_error_callback(error_callback);
  }

  // Initialize the database given a name. The name defines where the SQLite
  // file is. If this returns an error code, no other method should be called.
  //
  // Before calling this method, you must call AddTable for any
  // WebDatabaseTable objects that are supposed to participate in
  // managing the database.
  //
  // `encryptor` must not be null except in test code.
  sql::InitStatus Init(const base::FilePath& db_name,
                       const os_crypt_async::Encryptor* encryptor = nullptr);

  // Transactions management
  void BeginTransaction();
  void CommitTransaction();

  std::string GetDiagnosticInfo(int extended_error, sql::Statement* statement);

  // Exposed for testing only.
  sql::Database* GetSQLConnection();

 private:
  // Used by |Init()| to migration database schema from older versions to
  // current version.
  sql::InitStatus MigrateOldVersionsAsNeeded();

  // Migrates this database to |version|. Returns false if there was
  // migration work to do and it failed, true otherwise.
  //
  // Implementations may set |*update_compatible_version| to true if
  // the compatible version should be changed to |version|.
  // Implementations should otherwise not modify this parameter.
  bool MigrateToVersion(int version,
                        bool* update_compatible_version);

  bool MigrateToVersion58DropWebAppsAndIntents();
  bool MigrateToVersion79DropLoginsTable();
  bool MigrateToVersion105DropIbansTable();

  sql::Database db_;
  sql::MetaTable meta_table_;

  // Map of all the different tables that have been added to this
  // object. Non-owning.
  std::map<WebDatabaseTable::TypeKey, raw_ptr<WebDatabaseTable>> tables_;
};

#endif  // COMPONENTS_WEBDATA_COMMON_WEB_DATABASE_H_
