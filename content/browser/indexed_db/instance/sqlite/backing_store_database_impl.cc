// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/sqlite/backing_store_database_impl.h"

#include "base/check.h"
#include "base/notimplemented.h"
#include "content/browser/indexed_db/instance/sqlite/backing_store_transaction_impl.h"
#include "content/browser/indexed_db/status.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_metadata.h"

namespace content::indexed_db::sqlite {

struct MetadataConstants {
 public:
  // These are schema versions of our implementation of `sql::Database`; not the
  // version supplied by the application for the IndexedDB database.
  static constexpr int kCurrentSchemaVersion = 1;
  static constexpr int kCompatibleSchemaVersion = 1;

  // Keys of properties stored in the meta table.
  struct Keys {
    // Metadata relevant to the IndexedDB database. Though these are strictly
    // "data" and not "metadata", they are stored in the meta table for
    // convenience.
    struct Idb {
      // The application-supplied version of the IndexedDB database.
      static constexpr char kVersion[] = "idb.version";
      static constexpr char kMaxObjectStoreId[] = "idb.max_object_store_id";
    };
  };
};

BackingStoreDatabaseImpl::BackingStoreDatabaseImpl(
    const std::u16string& name,
    base::WeakPtr<DatabaseConnection> open_db)
    : connection_(open_db), metadata_(name) {
  CHECK(connection_->db()->is_open());
  bool new_db = !sql::MetaTable::DoesTableExist(connection_->db());
  sql::MetaTable* meta_table = connection_->meta_table();
  CHECK(meta_table->Init(connection_->db(),
                         MetadataConstants::kCurrentSchemaVersion,
                         MetadataConstants::kCompatibleSchemaVersion));
  if (new_db) {
    metadata_.version = blink::IndexedDBDatabaseMetadata::NO_VERSION;
    metadata_.max_object_store_id = 0;
    meta_table->SetValue(MetadataConstants::Keys::Idb::kVersion,
                         metadata_.version);
    meta_table->SetValue(MetadataConstants::Keys::Idb::kMaxObjectStoreId,
                         metadata_.max_object_store_id);
  } else {
    meta_table->GetValue(MetadataConstants::Keys::Idb::kVersion,
                         &metadata_.version);
    meta_table->GetValue(MetadataConstants::Keys::Idb::kMaxObjectStoreId,
                         &metadata_.max_object_store_id);
  }
}

BackingStoreDatabaseImpl::~BackingStoreDatabaseImpl() {
  if (connection_) {
    // The underlying `DatabaseConnection` can be reused by a new instance.
    connection_->meta_table()->Reset();
  }
}

void BackingStoreDatabaseImpl::RollbackUpgrade(UpgradePassKey& pass_key) {
  metadata_ = std::move(pass_key.metadata_snapshot_);
}

Status BackingStoreDatabaseImpl::SetDatabaseVersion(UpgradePassKey&,
                                                    int64_t version) {
  connection_->meta_table()->SetValue(MetadataConstants::Keys::Idb::kVersion,
                                      version);
  metadata_.version = version;
  return Status::OK();
}

const blink::IndexedDBDatabaseMetadata&
BackingStoreDatabaseImpl::GetMetadata() {
  return metadata_;
}

PartitionedLockId BackingStoreDatabaseImpl::GetLockId(
    int64_t object_store_id) const {
  NOTIMPLEMENTED();
  return PartitionedLockId();
}

std::unique_ptr<BackingStore::Transaction>
BackingStoreDatabaseImpl::CreateTransaction(
    blink::mojom::IDBTransactionDurability durability,
    blink::mojom::IDBTransactionMode mode) {
  // TODO(crbug.com/40253999): Handle `durability` and assert preconditions for
  // `mode`.
  if (mode == blink::mojom::IDBTransactionMode::VersionChange) {
    return std::make_unique<BackingStoreVersionChangeTransaction>(
        GetWeakPtr(), UpgradePassKey(metadata_));
  }
  return std::make_unique<BackingStoreTransactionImpl>(GetWeakPtr());
}

Status BackingStoreDatabaseImpl::DeleteDatabase(
    std::vector<PartitionedLock> locks,
    base::OnceClosure on_complete) {
  NOTIMPLEMENTED();
  return Status::OK();
}

base::WeakPtr<BackingStoreDatabaseImpl> BackingStoreDatabaseImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace content::indexed_db::sqlite
