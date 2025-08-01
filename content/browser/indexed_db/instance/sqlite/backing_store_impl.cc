// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/sqlite/backing_store_impl.h"

#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/notimplemented.h"
#include "content/browser/indexed_db/file_path_util.h"
#include "content/browser/indexed_db/indexed_db_data_loss_info.h"
#include "content/browser/indexed_db/instance/sqlite/backing_store_database_impl.h"
#include "content/browser/indexed_db/instance/sqlite/database_connection.h"
#include "content/browser/indexed_db/status.h"

namespace content::indexed_db::sqlite {

std::tuple<std::unique_ptr<BackingStore>, Status, IndexedDBDataLossInfo, bool>
BackingStoreImpl::OpenAndVerify(
    base::FilePath directory,
    storage::mojom::BlobStorageContext& blob_storage_context) {
  return {
      std::make_unique<BackingStoreImpl>(std::move(directory),
                                         blob_storage_context),
      Status::OK(),
      IndexedDBDataLossInfo(),
      false,
  };
}

BackingStoreImpl::BackingStoreImpl(
    base::FilePath directory,
    storage::mojom::BlobStorageContext& blob_storage_context)
    : directory_(std::move(directory)),
      blob_storage_context_(blob_storage_context) {}

BackingStoreImpl::~BackingStoreImpl() = default;

bool BackingStoreImpl::CanOpportunisticallyClose() const {
  // In-memory stores have to stay alive.
  if (in_memory()) {
    return false;
  }

  // There's not much of a point in deleting `this` since it doesn't use many
  // resources (just a tiny amount of memory). But for now, match the logic of
  // the LevelDB store, where `this` is cleaned up if there are no active
  // databases and no blobs. This is as simple as checking if there are any
  // `DatabaseConnection` objects.
  return open_connections_.empty();
}

void BackingStoreImpl::TearDown(base::WaitableEvent* signal_on_destruction) {
  NOTIMPLEMENTED();
  signal_on_destruction->Signal();
}

void BackingStoreImpl::InvalidateBlobReferences() {
  NOTIMPLEMENTED();
}

void BackingStoreImpl::StartPreCloseTasks(base::OnceClosure on_done) {
  NOTIMPLEMENTED();
  std::move(on_done).Run();
}

void BackingStoreImpl::StopPreCloseTasks() {}

int64_t BackingStoreImpl::GetInMemorySize() const {
  uint64_t total_size = 0;
  for (const auto& [_, connection] : open_connections_) {
    total_size += connection->GetInMemorySize();
  }
  return total_size;
}

StatusOr<bool> BackingStoreImpl::DatabaseExists(std::u16string_view name) {
  if (auto it = open_connections_.find(std::u16string(name));
      it != open_connections_.end()) {
    return !it->second->IsZygotic();
  }

  if (in_memory()) {
    return false;
  }

  return base::PathExists(directory_.Append(DatabaseNameToFileName(name)));
}

StatusOr<std::vector<blink::mojom::IDBNameAndVersionPtr>>
BackingStoreImpl::GetDatabaseNamesAndVersions() {
  std::vector<blink::mojom::IDBNameAndVersionPtr> names_and_versions;
  std::set<base::FilePath> already_open_file_names;
  for (const auto& [name, db] : open_connections_) {
    already_open_file_names.insert(DatabaseNameToFileName(name));
    // indexedDB.databases() is meant to return *committed* database state, i.e.
    // should not include in-progress VersionChange updates. This is verified by
    // external/wpt/IndexedDB/get-databases.any.html
    int64_t version = db->GetCommittedVersion();
    if (version == blink::IndexedDBDatabaseMetadata::NO_VERSION) {
      continue;
    }
    names_and_versions.emplace_back(
        blink::mojom::IDBNameAndVersion::New(name, version));
  }

  if (!in_memory()) {
    EnumerateDatabasesInDirectory(directory_, [&](const base::FilePath& path) {
      if (already_open_file_names.contains(path.BaseName())) {
        return;
      }
      std::ignore =
          DatabaseConnection::Open(/*name=*/{}, path, *this)
              .transform([&](std::unique_ptr<DatabaseConnection> connection) {
                names_and_versions.emplace_back(
                    blink::mojom::IDBNameAndVersion::New(
                        connection->metadata().name,
                        connection->metadata().version));
              });
    });
  }

  return names_and_versions;
}

StatusOr<std::unique_ptr<BackingStore::Database>>
BackingStoreImpl::CreateOrOpenDatabase(const std::u16string& name) {
  if (auto it = open_connections_.find(name); it != open_connections_.end()) {
    return std::make_unique<BackingStoreDatabaseImpl>(it->second->GetWeakPtr());
  }
  base::FilePath db_path =
      in_memory() ? base::FilePath()
                  : directory_.Append(DatabaseNameToFileName(name));
  return DatabaseConnection::Open(name, std::move(db_path), *this)
      .transform([&](std::unique_ptr<DatabaseConnection> connection) {
        auto database = std::make_unique<BackingStoreDatabaseImpl>(
            connection->GetWeakPtr());
        open_connections_[name] = std::move(connection);
        return database;
      });
}

uintptr_t BackingStoreImpl::GetIdentifierForMemoryDump() {
  NOTIMPLEMENTED();
  return 0;
}

void BackingStoreImpl::FlushForTesting() {
  NOTIMPLEMENTED();
}

void BackingStoreImpl::DestroyConnection(const std::u16string& name) {
  CHECK(open_connections_.erase(name) == 1);
}

}  // namespace content::indexed_db::sqlite
