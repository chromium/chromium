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
#include "content/browser/indexed_db/instance/sqlite/backing_store_database_impl.h"
#include "content/browser/indexed_db/instance/sqlite/database_connection.h"
#include "content/browser/indexed_db/status.h"

namespace content::indexed_db::sqlite {

BackingStoreImpl::BackingStoreImpl(
    base::FilePath directory,
    storage::mojom::BlobStorageContext& blob_storage_context)
    : directory_(std::move(directory)),
      blob_storage_context_(blob_storage_context) {}

BackingStoreImpl::~BackingStoreImpl() = default;

bool BackingStoreImpl::CanOpportunisticallyClose() const {
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
  // Though the IDB spec does not mandate sorting, the LevelDB backing store has
  // set a precedent of sorting databases by name. To avoid breaking clients
  // that may depend on this, use a map to return the results in sorted order.
  std::map<std::u16string, int64_t> names_and_versions;

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
    names_and_versions.emplace(name, version);
  }

  if (!in_memory()) {
    EnumerateDatabasesInDirectory(directory_, [&](const base::FilePath& path) {
      if (already_open_file_names.contains(path.BaseName())) {
        return;
      }
      std::ignore =
          DatabaseConnection::Open(/*name=*/{}, path, *this)
              .transform([&](std::unique_ptr<DatabaseConnection> connection) {
                names_and_versions.emplace(connection->metadata().name,
                                           connection->metadata().version);
              });
    });
  }

  std::vector<blink::mojom::IDBNameAndVersionPtr> result;
  for (const auto& [name, version] : names_and_versions) {
    result.emplace_back(blink::mojom::IDBNameAndVersion::New(name, version));
  }
  return result;
}

StatusOr<std::unique_ptr<BackingStore::Database>>
BackingStoreImpl::CreateOrOpenDatabase(const std::u16string& name) {
  if (auto it = open_connections_.find(name); it != open_connections_.end()) {
    return it->second->CreateDatabaseWrapper();
  }
  base::FilePath db_path =
      in_memory() ? base::FilePath()
                  : directory_.Append(DatabaseNameToFileName(name));
  return DatabaseConnection::Open(name, std::move(db_path), *this)
      .transform([&](std::unique_ptr<DatabaseConnection> connection) {
        std::unique_ptr<BackingStoreDatabaseImpl> database =
            connection->CreateDatabaseWrapper();
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
