// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_VALUE_STORE_LEVELDB_SCOPED_DATABASE_H_
#define EXTENSIONS_BROWSER_VALUE_STORE_LEVELDB_SCOPED_DATABASE_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "extensions/browser/value_store/lazy_leveldb.h"
#include "extensions/browser/value_store/value_store.h"

// This database is used to persist values with their keys scoped within a
// specified namespace - AKA |scope|. Values will be written as follows:
//
// <scope><delimiter><scoped-key> -> <value>
//
// Note: |scope| must not contain the delimiter, but the |key| may.
//
class LeveldbScopedDatabase
    : public LazyLevelDb,
      public base::RefCountedThreadSafe<LeveldbScopedDatabase> {
 public:
  // Splits the full key into the scope and inner (scoped) key.
  // Returns true if successfully split, and false if not and leaves |scope| and
  // |key| unchanged.
  static bool SplitKey(const std::string& full_key,
                       std::string* scope,
                       std::string* key);

  // Creates a fully scoped key. |scope| cannot be an empty key and cannot
  // contain the delimiter. |scoped_key| will be set to:
  //
  //   <scope><delimiter><key>
  //
  // Will return true when successful, false if not.
  static bool CreateKey(const std::string& scope,
                        const std::string& key,
                        std::string* scoped_key);

  LeveldbScopedDatabase(const std::string& uma_client_name,
                        const base::FilePath& path);

  // Reads a single |value| from the database for the specified |key|.
  ValueStore::Status Read(const std::string& scope,
                          const std::string& key,
                          base::Optional<base::Value>* value);

  // Reads all |values| from the database stored within the specified |scope|.
  ValueStore::Status Read(const std::string& scope,
                          base::DictionaryValue* values);

  // Writes a single |key| => |value| to the database.
  ValueStore::Status Write(const std::string& scope,
                           const std::string& key,
                           const base::Value& value);

  // Writes all |values| to the database with the keys scoped with |scope|.
  ValueStore::Status Write(const std::string& scope,
                           const base::DictionaryValue& values);

  // Deletes all |keys| from the databases withing the specified |scope|.
  ValueStore::Status DeleteValues(const std::string& scope,
                                  const std::vector<std::string>& keys);

 protected:
  friend class base::RefCountedThreadSafe<LeveldbScopedDatabase>;
  virtual ~LeveldbScopedDatabase();

  static ValueStore::Status AddToWriteBatch(leveldb::WriteBatch* batch,
                                            const std::string& scope,
                                            const std::string& key,
                                            const base::Value& value);

 private:
  DISALLOW_COPY_AND_ASSIGN(LeveldbScopedDatabase);
};

#endif  // EXTENSIONS_BROWSER_VALUE_STORE_LEVELDB_SCOPED_DATABASE_H_
