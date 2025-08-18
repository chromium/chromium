// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/backing_store_test_base.h"

#include "base/task/updateable_sequenced_task_runner.h"
#include "base/test/bind.h"

using blink::IndexedDBIndexMetadata;
using blink::IndexedDBKey;
using blink::IndexedDBKeyPath;
using blink::IndexedDBObjectStoreMetadata;
using blink::StorageKey;

namespace content::indexed_db {

BackingStoreTestBase::BackingStoreTestBase() = default;

BackingStoreTestBase::~BackingStoreTestBase() = default;

void BackingStoreTestBase::SetUp() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

  blob_context_ = std::make_unique<MockBlobStorageContext>();
  file_system_access_context_ =
      std::make_unique<test::MockFileSystemAccessContext>();

  quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
      /*is_incognito=*/false, temp_dir_.GetPath(),
      base::SingleThreadTaskRunner::GetCurrentDefault(), nullptr);
  quota_manager_proxy_ = base::MakeRefCounted<storage::MockQuotaManagerProxy>(
      quota_manager_.get(), base::SingleThreadTaskRunner::GetCurrentDefault());

  CreateFactoryAndBackingStore();

  // useful keys and values during tests
  value1_ = IndexedDBValue("value1", {});
  value2_ = IndexedDBValue("value2", {});

  key1_ = IndexedDBKey(99, blink::mojom::IDBKeyType::Number);
  key2_ = IndexedDBKey(u"key2");
}

void BackingStoreTestBase::CreateFactoryAndBackingStore() {
  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
  auto bucket_info = storage::BucketInfo();
  bucket_info.id = storage::BucketId::FromUnsafeValue(1);
  bucket_info.storage_key = storage_key;
  bucket_info.name = storage::kDefaultBucketName;
  auto bucket_locator = bucket_info.ToBucketLocator();

  mojo::PendingRemote<storage::mojom::BlobStorageContext> blob_storage_context;
  blob_context_->Clone(blob_storage_context.InitWithNewPipeAndPassReceiver());

  mojo::PendingRemote<storage::mojom::FileSystemAccessContext> fsa_context;
  file_system_access_context_->Clone(
      fsa_context.InitWithNewPipeAndPassReceiver());

  bucket_context_ = std::make_unique<BucketContext>(
      bucket_info, temp_dir_.GetPath(), BucketContext::Delegate(),
      scoped_refptr<base::UpdateableSequencedTaskRunner>(),
      quota_manager_proxy_, std::move(blob_storage_context),
      std::move(fsa_context));
  std::tie(std::ignore, std::ignore, data_loss_info_) =
      bucket_context_->InitBackingStoreIfNeeded(/*create_if_missing=*/true);

  backing_store_ = bucket_context_->backing_store();
}

void BackingStoreTestBase::UpdateDatabaseVersion(
    indexed_db::BackingStore::Database& db,
    int64_t version) {
  std::unique_ptr<BackingStore::Transaction> transaction =
      db.CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                           blink::mojom::IDBTransactionMode::VersionChange);
  transaction->Begin(CreateDummyLock());
  EXPECT_TRUE(transaction->SetDatabaseVersion(version).ok());
  CommitTransaction(*transaction);
}

std::unique_ptr<indexed_db::BackingStore::Transaction>
BackingStoreTestBase::CreateAndBeginTransaction(
    indexed_db::BackingStore::Database& db,
    blink::mojom::IDBTransactionMode mode) {
  std::unique_ptr<indexed_db::BackingStore::Transaction> transaction =
      db.CreateTransaction(blink::mojom::IDBTransactionDurability::Relaxed,
                           mode);
  transaction->Begin(CreateDummyLock());
  return transaction;
}

void BackingStoreTestBase::CommitTransaction(
    indexed_db::BackingStore::Transaction& transaction) {
  bool succeeded = false;
  EXPECT_TRUE(
      transaction
          .CommitPhaseOne(
              MockBlobStorageContext::CreateBlobWriteCallback(&succeeded),
              base::DoNothing())
          .ok());
  EXPECT_TRUE(succeeded);
  EXPECT_TRUE(transaction.CommitPhaseTwo().ok());
}

std::vector<PartitionedLock> BackingStoreTestBase::CreateDummyLock() {
  base::RunLoop loop;
  PartitionedLockHolder locks_receiver;
  bucket_context_->lock_manager().AcquireLocks(
      {{{0, "01"}, PartitionedLockManager::LockType::kShared}}, locks_receiver,
      base::BindLambdaForTesting([&loop]() { loop.Quit(); }));
  loop.Run();
  return std::move(locks_receiver.locks);
}

void BackingStoreTestBase::DestroyFactoryAndBackingStore() {
  backing_store_ = nullptr;
  bucket_context_.reset();
}

BackingStore* BackingStoreTestBase::backing_store() {
  return backing_store_;
}

void BackingStoreTestBase::TearDown() {
  DestroyFactoryAndBackingStore();
  if (temp_dir_.IsValid()) {
    ASSERT_TRUE(temp_dir_.Delete());
  }
}

}  // namespace content::indexed_db
