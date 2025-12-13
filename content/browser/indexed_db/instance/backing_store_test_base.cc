// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/backing_store_test_base.h"

#include "base/test/bind.h"
#include "base/uuid.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "storage/browser/test/fake_blob.h"

using blink::IndexedDBIndexMetadata;
using blink::IndexedDBKey;
using blink::IndexedDBKeyPath;
using blink::IndexedDBObjectStoreMetadata;
using blink::StorageKey;

namespace {

class FakeFileSystemAccessTransferToken
    : public ::blink::mojom::FileSystemAccessTransferToken {
 public:
  explicit FakeFileSystemAccessTransferToken(const base::UnguessableToken& id)
      : id_(id) {}

  void GetInternalID(GetInternalIDCallback callback) override {
    std::move(callback).Run(id_);
  }

  void Clone(mojo::PendingReceiver<blink::mojom::FileSystemAccessTransferToken>
                 clone_receiver) override {
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<FakeFileSystemAccessTransferToken>(id_),
        std::move(clone_receiver));
  }

 private:
  base::UnguessableToken id_;
};

}  // namespace

namespace content::indexed_db {

BackingStoreTestBase::BackingStoreTestBase(bool use_sqlite)
    : sqlite_override_(
          BucketContext::OverrideShouldUseSqliteForTesting(use_sqlite)) {}

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
      quota_manager_proxy_, std::move(blob_storage_context),
      std::move(fsa_context));
  std::tie(std::ignore, std::ignore, data_loss_info_) =
      bucket_context_->InitBackingStore(/*create_if_missing=*/true);

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
  CommitTransactionAndVerify(*transaction);
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

void BackingStoreTestBase::CommitTransactionAndVerify(
    BackingStore::Transaction& transaction) {
  ASSERT_TRUE(CommitTransactionPhaseOneAndVerify(transaction));
  EXPECT_TRUE(transaction.CommitPhaseTwo().ok());
}

bool BackingStoreTestBase::CommitTransactionPhaseOneAndVerify(
    BackingStore::Transaction& transaction) {
  bool blob_write_succeeded = false;
  base::RunLoop phase_one_blob_wait;
  EXPECT_TRUE(
      transaction
          .CommitPhaseOne(
              MockBlobStorageContext::CreateBlobWriteCallback(
                  &blob_write_succeeded, phase_one_blob_wait.QuitClosure()),
              base::BindLambdaForTesting(
                  [&](blink::mojom::FileSystemAccessTransferToken& token_remote,
                      base::OnceCallback<void(const std::vector<uint8_t>&)>
                          deliver_serialized_token) {
                    mojo::PendingRemote<
                        blink::mojom::FileSystemAccessTransferToken>
                        token_clone;
                    token_remote.Clone(
                        token_clone.InitWithNewPipeAndPassReceiver());
                    file_system_access_context_->SerializeHandle(
                        std::move(token_clone),
                        std::move(deliver_serialized_token));
                  }))
          .ok());
  phase_one_blob_wait.Run();
  return blob_write_succeeded;
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

BackingStoreWithExternalObjectsTestBase::
    BackingStoreWithExternalObjectsTestBase(bool use_sqlite)
    : BackingStoreTestBase(use_sqlite) {}

BackingStoreWithExternalObjectsTestBase::
    ~BackingStoreWithExternalObjectsTestBase() = default;

void BackingStoreWithExternalObjectsTestBase::SetUp() {
  BackingStoreTestBase::SetUp();

  const int64_t kTime1 = 13255919133000000ll;
  const int64_t kTime2 = 13287455133000000ll;
  // useful keys and values during tests
  if (IncludesBlobs()) {
    external_objects_.push_back(CreateBlobInfo(u"blob type", "blob payload"));
    external_objects_.push_back(CreateFileInfo(
        u"file name", u"file type",
        base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(kTime1)),
        kBlobFileData1));
    external_objects_.push_back(CreateFileInfo(
        u"file name", u"file type",
        base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(kTime2)),
        kBlobFileData2));
  }
  if (IncludesFileSystemAccessHandles()) {
    external_objects_.push_back(CreateFileSystemAccessHandle());
    external_objects_.push_back(CreateFileSystemAccessHandle());
  }
  value3_ = IndexedDBValue("value3", external_objects_);
  key3_ = IndexedDBKey(u"key3");
}

IndexedDBExternalObject BackingStoreTestBase::CreateFileInfo(
    const std::u16string& file_name,
    const std::u16string& type,
    base::Time last_modified,
    std::string_view file_contents) {
  mojo::PendingRemote<blink::mojom::Blob> remote;
  auto fake_blob = std::make_unique<storage::FakeBlob>(
      base::Uuid::GenerateRandomV4().AsLowercaseString());
  fake_blob->set_body(file_contents);
  mojo::MakeSelfOwnedReceiver(std::move(fake_blob),
                              remote.InitWithNewPipeAndPassReceiver());
  IndexedDBExternalObject info(std::move(remote), file_name, type,
                               last_modified, file_contents.size());
  return info;
}

IndexedDBExternalObject BackingStoreTestBase::CreateBlobInfo(
    const std::u16string& type,
    std::optional<std::string_view> blob_data) {
  mojo::PendingRemote<blink::mojom::Blob> remote;
  auto fake_blob = std::make_unique<storage::FakeBlob>(
      base::Uuid::GenerateRandomV4().AsLowercaseString());
  if (blob_data) {
    fake_blob->set_body(*blob_data);
  }
  mojo::MakeSelfOwnedReceiver(std::move(fake_blob),
                              remote.InitWithNewPipeAndPassReceiver());
  IndexedDBExternalObject info(std::move(remote), type,
                               blob_data ? blob_data->size() : 64);
  return info;
}

IndexedDBExternalObject BackingStoreTestBase::CreateFileSystemAccessHandle() {
  mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> remote;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<FakeFileSystemAccessTransferToken>(
          base::UnguessableToken::Create()),
      remote.InitWithNewPipeAndPassReceiver());
  IndexedDBExternalObject info(std::move(remote));
  return info;
}

// This just checks the data that survive getting stored and recalled, e.g.
// the file path and UUID will change and thus aren't verified.
bool BackingStoreWithExternalObjectsTestBase::CheckBlobInfoMatches(
    const std::vector<IndexedDBExternalObject>& reads) const {
  if (external_objects_.size() != reads.size()) {
    EXPECT_EQ(external_objects_.size(), reads.size());
    return false;
  }
  for (size_t i = 0; i < external_objects_.size(); ++i) {
    const IndexedDBExternalObject& a = external_objects_[i];
    const IndexedDBExternalObject& b = reads[i];
    if (a.object_type() != b.object_type()) {
      EXPECT_EQ(a.object_type(), b.object_type());
      return false;
    }
    switch (a.object_type()) {
      case IndexedDBExternalObject::ObjectType::kFile:
        if (a.file_name() != b.file_name()) {
          EXPECT_EQ(a.file_name(), b.file_name());
          return false;
        }
        if (a.last_modified() != b.last_modified()) {
          EXPECT_EQ(a.last_modified(), b.last_modified());
          return false;
        }
        [[fallthrough]];
      case IndexedDBExternalObject::ObjectType::kBlob:
        if (a.type() != b.type()) {
          EXPECT_EQ(a.type(), b.type());
          return false;
        }
        if (a.size() != b.size()) {
          EXPECT_EQ(a.size(), b.size());
          return false;
        }
        break;
      case IndexedDBExternalObject::ObjectType::kFileSystemAccessHandle:
        if (b.serialized_file_system_access_handle().empty()) {
          EXPECT_FALSE(b.serialized_file_system_access_handle().empty());
          return false;
        }
        break;
    }
  }
  return true;
}

}  // namespace content::indexed_db
