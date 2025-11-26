// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_BACKING_STORE_IMPL_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_BACKING_STORE_IMPL_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/files/file_path.h"
#include "components/services/storage/public/mojom/blob_storage_context.mojom-forward.h"
#include "content/browser/indexed_db/instance/backing_store.h"
#include "content/common/content_export.h"

namespace content::indexed_db {

namespace sqlite {

class DatabaseConnection;

class CONTENT_EXPORT BackingStoreImpl : public BackingStore {
 public:
  // The store itself does not have any footprint on disk except a directory
  // where SQLite DBs will be located, so creation cannot fail. `directory` is
  // assumed to already exist.
  BackingStoreImpl(base::FilePath directory,
                   storage::mojom::BlobStorageContext& blob_storage_context);
  BackingStoreImpl(const BackingStoreImpl&) = delete;
  BackingStoreImpl& operator=(const BackingStoreImpl&) = delete;
  ~BackingStoreImpl() override;

  // BackingStore:
  bool CanOpportunisticallyClose() const override;
  void TearDown(base::WaitableEvent* signal_on_destruction) override;
  void InvalidateBlobReferences() override;
  void StartPreCloseTasks(base::OnceClosure on_done) override;
  void StopPreCloseTasks() override;
  int64_t GetInMemorySize() const override;
  StatusOr<bool> DatabaseExists(std::u16string_view name) override;
  StatusOr<std::vector<blink::mojom::IDBNameAndVersionPtr>>
  GetDatabaseNamesAndVersions() override;
  StatusOr<std::unique_ptr<BackingStore::Database>> CreateOrOpenDatabase(
      const std::u16string& name) override;
  uintptr_t GetIdentifierForMemoryDump() override;
  void FlushForTesting() override;

  void DestroyConnection(const std::u16string& name);

  storage::mojom::BlobStorageContext& blob_storage_context() {
    return *blob_storage_context_;
  }

 private:
  friend class DatabaseConnectionTest;

  bool in_memory() const { return directory_.empty(); }

  // The directory where all databases for this backing store will live. When
  // this is empty, the backing store lives in memory.
  const base::FilePath directory_;

  // This BlobStorageContext is owned by the BucketContext that owns `this`.
  // The BlobStorageContext manages handles to web blobs (both coming from and
  // being vended to the renderer). Despite this object's name, it does not
  // store blobs. Those that are written into IndexedDB are stored in the SQLite
  // DB.
  raw_ref<storage::mojom::BlobStorageContext> blob_storage_context_;

  std::unordered_map<std::u16string, std::unique_ptr<DatabaseConnection>>
      open_connections_;
};

}  // namespace sqlite
}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_BACKING_STORE_IMPL_H_
