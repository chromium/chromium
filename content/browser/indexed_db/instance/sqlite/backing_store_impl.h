// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_BACKING_STORE_IMPL_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_BACKING_STORE_IMPL_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/indexed_db/instance/backing_store.h"

namespace sql {
class Database;
class MetaTable;
}  // namespace sql

namespace content::indexed_db {

struct IndexedDBDataLossInfo;

namespace sqlite {

// Owns an open connection to a SQLite database and supports weak pointer
// semantics.
class DatabaseConnection {
 public:
  explicit DatabaseConnection(std::unique_ptr<sql::Database> db);
  DatabaseConnection(const DatabaseConnection&) = delete;
  DatabaseConnection& operator=(const DatabaseConnection&) = delete;
  ~DatabaseConnection();

  sql::Database* db() const { return db_.get(); }
  sql::MetaTable* meta_table() const { return meta_table_.get(); }

  base::WeakPtr<DatabaseConnection> GetWeakPtr();

 private:
  std::unique_ptr<sql::Database> db_;
  std::unique_ptr<sql::MetaTable> meta_table_;
  base::WeakPtrFactory<DatabaseConnection> weak_factory_{this};
};

class BackingStoreImpl : public BackingStore {
 public:
  static std::tuple<std::unique_ptr<BackingStore>,
                    Status,
                    IndexedDBDataLossInfo,
                    bool /* is_disk_full */>
  OpenAndVerify(base::FilePath data_path);

  BackingStoreImpl(base::FilePath data_path);
  BackingStoreImpl(const BackingStoreImpl&) = delete;
  BackingStoreImpl& operator=(const BackingStoreImpl&) = delete;
  ~BackingStoreImpl() override;

  // BackingStore:
  void TearDown(base::WaitableEvent* signal_on_destruction) override;
  void InvalidateBlobReferences() override;
  void StartPreCloseTasks(base::OnceClosure on_done) override;
  void StopPreCloseTasks() override;
  int64_t GetInMemorySize() const override;
  Status GetDatabaseNames(std::vector<std::u16string>* names) override;
  Status GetDatabaseNamesAndVersions(
      std::vector<blink::mojom::IDBNameAndVersionPtr>* names_and_versions)
      override;
  base::expected<std::unique_ptr<BackingStore::Database>, Status>
  CreateOrOpenDatabase(const std::u16string& name) override;
  uintptr_t GetIdentifierForMemoryDump() override;
  void FlushForTesting() override;

 private:
  const base::FilePath data_path_;
  std::unordered_map<std::u16string, DatabaseConnection> open_connections_;
};

}  // namespace sqlite
}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_BACKING_STORE_IMPL_H_
