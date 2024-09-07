// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBDATA_COMMON_WEB_DATABASE_TABLE_H_
#define COMPONENTS_WEBDATA_COMMON_WEB_DATABASE_TABLE_H_

#include "base/memory/raw_ptr.h"
#include "components/webdata/common/webdata_export.h"

namespace os_crypt_async {
class Encryptor;
}

namespace sql {
class Database;
class MetaTable;
}

// An abstract base class representing a table within a WebDatabase.
// Each table should subclass this, adding type-specific methods as needed.
class WEBDATA_EXPORT WebDatabaseTable {
 public:
  // To look up a WebDatabaseTable of a certain type from WebDatabase,
  // we use a void* key, so that we can simply use the address of one
  // of the type's statics.
  typedef void* TypeKey;

  // The object is not ready for use until Init() has been called.
  WebDatabaseTable();

  WebDatabaseTable(const WebDatabaseTable&) = delete;
  WebDatabaseTable& operator=(const WebDatabaseTable&) = delete;

  virtual ~WebDatabaseTable();

  // Retrieves the TypeKey for the concrete subtype.
  virtual TypeKey GetTypeKey() const = 0;

  // Stores the passed members as instance variables.
  void Init(sql::Database* db,
            sql::MetaTable* meta_table,
            const os_crypt_async::Encryptor* encryptor);

  // Resets the members stored during `Init()`.
  void Shutdown();

  // Create all of the expected SQL tables if they do not already exist.
  // Returns true on success, false on failure.
  virtual bool CreateTablesIfNecessary() = 0;

  // Migrates this table to |version|. Returns false if there was
  // migration work to do and it failed, true otherwise.
  //
  // Implementations may set |*update_compatible_version| to true if the
  // compatible version should be changed to |version|, i.e., if the change will
  // break previous versions when they try to use the updated database.
  // Implementations should otherwise not modify this parameter.
  virtual bool MigrateToVersion(int version,
                                bool* update_compatible_version) = 0;

 protected:
  sql::Database* db() const { return db_; }

  sql::MetaTable* meta_table() const { return meta_table_; }

  const os_crypt_async::Encryptor* encryptor() const { return encryptor_; }

 private:
  // Non-null, except before `Init()` and after `Shutdown()`. Effectively, this
  // means that they are non-null except during the constructor and destructor.
  // They point to objects owned by `WebDatabase` whose lifetime is slightly
  // shorter than that of the `WebDatabaseBackend` owning `this`.
  raw_ptr<sql::Database> db_;
  raw_ptr<sql::MetaTable> meta_table_;
  // Non-null, except before `Init()` and after `Shutdown()`. This object is
  // owned by the `WebdatabaseBackend` owning `this`.
  raw_ptr<const os_crypt_async::Encryptor> encryptor_;
};

#endif  // COMPONENTS_WEBDATA_COMMON_WEB_DATABASE_TABLE_H_
