// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBDATA_COMMON_WEB_DATABASE_H_
#define COMPONENTS_WEBDATA_COMMON_WEB_DATABASE_H_

#include <map>
#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "components/webdata/common/web_database_table.h"
#include "components/webdata/common/webdata_export.h"
#include "sql/database.h"
#include "sql/init_status.h"
#include "sql/meta_table.h"

// This class manages a SQLite database that stores various web page meta data.
class WEBDATA_EXPORT WebDatabase {
 public:
  enum State {
    COMMIT_NOT_NEEDED,
    COMMIT_NEEDED
  };
  // Exposed publicly so the keyword table can access it.
  static const int kCurrentVersionNumber;
  // The newest version of the database Chrome will NOT try to migrate.
  static const int kDeprecatedVersionNumber;
  // Use this as a path to create an in-memory database.
  static const base::FilePath::CharType kInMemoryPath[];

  WebDatabase();
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
  sql::InitStatus Init(const base::FilePath& db_name);

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

  sql::Database db_;
  sql::MetaTable meta_table_;

  // Map of all the different tables that have been added to this
  // object. Non-owning.
  typedef std::map<WebDatabaseTable::TypeKey, WebDatabaseTable*> TableMap;
  TableMap tables_;

  DISALLOW_COPY_AND_ASSIGN(WebDatabase);
};

#endif  // COMPONENTS_WEBDATA_COMMON_WEB_DATABASE_H_
