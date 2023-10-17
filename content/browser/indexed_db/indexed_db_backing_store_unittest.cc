// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_backing_store.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <tuple>
#include <utility>

#include "base/barrier_closure.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/synchronization/waitable_event_watcher.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/default_clock.h"
#include "base/uuid.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"
#include "components/services/storage/indexed_db/scopes/varint_coding.h"
#include "components/services/storage/indexed_db/transactional_leveldb/leveldb_write_batch.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_database.h"
#include "components/services/storage/privileged/mojom/indexed_db_control.mojom-test-utils.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/browser/indexed_db/indexed_db_bucket_context.h"
#include "content/browser/indexed_db/indexed_db_class_factory.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_factory.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "content/browser/indexed_db/indexed_db_leveldb_operations.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/features.h"
#include "net/base/schemeful_site.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "storage/browser/test/fake_blob.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

using blink::IndexedDBDatabaseMetadata;
using blink::IndexedDBIndexMetadata;
using blink::IndexedDBKey;
using blink::IndexedDBKeyPath;
using blink::IndexedDBKeyRange;
using blink::IndexedDBObjectStoreMetadata;
using blink::StorageKey;
using url::Origin;

namespace content {
namespace indexed_db_backing_store_unittest {

class TestableIndexedDBBackingStore : public IndexedDBBackingStore {
 public:
  TestableIndexedDBBackingStore(
      IndexedDBBackingStore::Mode backing_store_mode,
      const storage::BucketLocator& bucket_locator,
      const base::FilePath& blob_path,
      std::unique_ptr<TransactionalLevelDBDatabase> db,
      std::unique_ptr<storage::FilesystemProxy> filesystem_proxy,
      BlobFilesCleanedCallback blob_files_cleaned,
      ReportOutstandingBlobsCallback report_outstanding_blobs,
      scoped_refptr<base::SequencedTaskRunner> idb_task_runner)
      : IndexedDBBackingStore(backing_store_mode,
                              bucket_locator,
                              blob_path,
                              std::move(db),
                              std::move(filesystem_proxy),
                              std::move(blob_files_cleaned),
                              std::move(report_outstanding_blobs),
                              std::move(idb_task_runner)) {}

  TestableIndexedDBBackingStore(const TestableIndexedDBBackingStore&) = delete;
  TestableIndexedDBBackingStore& operator=(
      const TestableIndexedDBBackingStore&) = delete;

  ~TestableIndexedDBBackingStore() override = default;

  const std::vector<base::FilePath>& removals() const { return removals_; }
  void ClearRemovals() { removals_.clear(); }

  void StartJournalCleaningTimer() override {
    IndexedDBBackingStore::StartJournalCleaningTimer();
  }

 protected:
  bool RemoveBlobFile(int64_t database_id, int64_t blob_number) const override {
    removals_.push_back(GetBlobFileName(database_id, blob_number));
    return IndexedDBBackingStore::RemoveBlobFile(database_id, blob_number);
  }

 private:
  // This is modified in an overridden virtual function that is properly const
  // in the real implementation, therefore must be mutable here.
  mutable std::vector<base::FilePath> removals_;
};

// Factory subclass to allow the test to use the
// TestableIndexedDBBackingStore subclass.
class TestIDBFactory : public IndexedDBFactory {
 public:
  explicit TestIDBFactory(IndexedDBContextImpl* idb_context)
      : IndexedDBFactory(idb_context,
                         base::DefaultClock::GetInstance()) {}

  TestIDBFactory(const TestIDBFactory&) = delete;
  TestIDBFactory& operator=(const TestIDBFactory&) = delete;

  ~TestIDBFactory() override = default;

 protected:
  std::unique_ptr<IndexedDBBackingStore> CreateBackingStore(
      IndexedDBBackingStore::Mode backing_store_mode,
      const storage::BucketLocator& bucket_locator,
      const base::FilePath& blob_path,
      std::unique_ptr<TransactionalLevelDBDatabase> db,
      std::unique_ptr<storage::FilesystemProxy> filesystem_proxy,
      IndexedDBBackingStore::BlobFilesCleanedCallback blob_files_cleaned,
      IndexedDBBackingStore::ReportOutstandingBlobsCallback
          report_outstanding_blobs,
      scoped_refptr<base::SequencedTaskRunner> idb_task_runner) override {
    // Use the overridden blob storage and File System Access contexts rather
    // than the versions that were passed in to this method. This way tests can
    // use a different context from what is stored in the IndexedDBContext.
    return std::make_unique<TestableIndexedDBBackingStore>(
        backing_store_mode, bucket_locator, blob_path, std::move(db),
        std::move(filesystem_proxy), std::move(blob_files_cleaned),
        std::move(report_outstanding_blobs), std::move(idb_task_runner));
  }
};

struct BlobWrite {
  BlobWrite() = default;
  BlobWrite(BlobWrite&& other) {
    blob = std::move(other.blob);
    path = std::move(other.path);
  }
  BlobWrite(mojo::PendingRemote<::blink::mojom::Blob> blob, base::FilePath path)
      : blob(std::move(blob)), path(path) {}
  ~BlobWrite() = default;

  int64_t GetBlobNumber() const {
    int64_t result;
    EXPECT_TRUE(base::StringToInt64(path.BaseName().AsUTF8Unsafe(), &result));
    return result;
  }

  mojo::Remote<::blink::mojom::Blob> blob;
  base::FilePath path;
};

class MockBlobStorageContext : public ::storage::mojom::BlobStorageContext {
 public:
  ~MockBlobStorageContext() override = default;

  void RegisterFromDataItem(mojo::PendingReceiver<::blink::mojom::Blob> blob,
                            const std::string& uuid,
                            storage::mojom::BlobDataItemPtr item) override {
    NOTREACHED();
  }
  void RegisterFromMemory(mojo::PendingReceiver<::blink::mojom::Blob> blob,
                          const std::string& uuid,
                          ::mojo_base::BigBuffer data) override {
    NOTREACHED();
  }
  void WriteBlobToFile(mojo::PendingRemote<::blink::mojom::Blob> blob,
                       const base::FilePath& path,
                       bool flush_on_write,
                       absl::optional<base::Time> last_modified,
                       WriteBlobToFileCallback callback) override {
    writes_.emplace_back(std::move(blob), path);

    if (write_files_to_disk_) {
      auto filesystem_proxy = std::make_unique<storage::FilesystemProxy>(
          storage::FilesystemProxy::UNRESTRICTED, base::FilePath());
      filesystem_proxy->WriteFileAtomically(path, "fake contents");
    }

    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       storage::mojom::WriteBlobToFileResult::kSuccess));
  }
  void Clone(mojo::PendingReceiver<::storage::mojom::BlobStorageContext>
                 receiver) override {
    receivers_.Add(this, std::move(receiver));
  }

  const std::vector<BlobWrite>& writes() { return writes_; }
  void ClearWrites() { writes_.clear(); }

  // If true, writes a fake file for each blob file to disk.
  // The contents are bogus, but the files will exist.
  void SetWriteFilesToDisk(bool write) { write_files_to_disk_ = write; }

 private:
  std::vector<BlobWrite> writes_;
  bool write_files_to_disk_ = false;
  mojo::ReceiverSet<::storage::mojom::BlobStorageContext> receivers_;
};

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

class MockFileSystemAccessContext
    : public ::storage::mojom::FileSystemAccessContext {
 public:
  ~MockFileSystemAccessContext() override = default;

  void SerializeHandle(
      mojo::PendingRemote<::blink::mojom::FileSystemAccessTransferToken>
          pending_token,
      SerializeHandleCallback callback) override {
    writes_.emplace_back(std::move(pending_token));
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            std::vector<uint8_t>{static_cast<uint8_t>(writes_.size() - 1)}));
  }

  void DeserializeHandle(
      const blink::StorageKey& storage_key,
      const std::vector<uint8_t>& bits,
      mojo::PendingReceiver<::blink::mojom::FileSystemAccessTransferToken>
          token) override {
    NOTREACHED();
  }

  void Clone(mojo::PendingReceiver<::storage::mojom::FileSystemAccessContext>
                 receiver) override {
    receivers_.Add(this, std::move(receiver));
  }

  const std::vector<
      mojo::Remote<::blink::mojom::FileSystemAccessTransferToken>>&
  writes() {
    return writes_;
  }
  void ClearWrites() { writes_.clear(); }

 private:
  std::vector<mojo::Remote<::blink::mojom::FileSystemAccessTransferToken>>
      writes_;
  mojo::ReceiverSet<::storage::mojom::FileSystemAccessContext> receivers_;
};

class IndexedDBBackingStoreTest : public testing::Test {
 public:
  IndexedDBBackingStoreTest() = default;
  IndexedDBBackingStoreTest(const IndexedDBBackingStoreTest&) = delete;
  IndexedDBBackingStoreTest& operator=(const IndexedDBBackingStoreTest&) =
      delete;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    blob_context_ = std::make_unique<MockBlobStorageContext>();
    file_system_access_context_ =
        std::make_unique<MockFileSystemAccessContext>();

    quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
        /*is_incognito=*/false, temp_dir_.GetPath(),
        base::SingleThreadTaskRunner::GetCurrentDefault(), nullptr);
    quota_manager_proxy_ = base::MakeRefCounted<storage::MockQuotaManagerProxy>(
        quota_manager_.get(),
        base::SingleThreadTaskRunner::GetCurrentDefault());

    mojo::PendingRemote<storage::mojom::BlobStorageContext>
        blob_storage_context;
    blob_context_->Clone(blob_storage_context.InitWithNewPipeAndPassReceiver());

    mojo::PendingRemote<storage::mojom::FileSystemAccessContext> fsa_context;
    file_system_access_context_->Clone(
        fsa_context.InitWithNewPipeAndPassReceiver());

    idb_context_ = base::MakeRefCounted<IndexedDBContextImpl>(
        temp_dir_.GetPath(), quota_manager_proxy_,
        base::DefaultClock::GetInstance(), std::move(blob_storage_context),
        std::move(fsa_context), base::SequencedTaskRunner::GetCurrentDefault(),
        base::SequencedTaskRunner::GetCurrentDefault());

    // Needed to get the QuotaClient bound.
    {
      base::RunLoop run_loop;
      idb_context_->IDBTaskRunner()->PostTask(FROM_HERE,
                                              run_loop.QuitClosure());
      run_loop.Run();
    }

    CreateFactoryAndBackingStore();

    // useful keys and values during tests
    value1_ = IndexedDBValue("value1", {});
    value2_ = IndexedDBValue("value2", {});

    key1_ = IndexedDBKey(99, blink::mojom::IDBKeyType::Number);
    key2_ = IndexedDBKey(u"key2");
  }

  void CreateFactoryAndBackingStore() {
    const blink::StorageKey storage_key =
        blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
    auto bucket_info = storage::BucketInfo();
    bucket_info.id = storage::BucketId::FromUnsafeValue(1);
    bucket_info.storage_key = storage_key;
    bucket_info.name = storage::kDefaultBucketName;
    auto bucket_locator = bucket_info.ToBucketLocator();

    idb_factory_ = std::make_unique<TestIDBFactory>(idb_context_.get());

    leveldb::Status s;
    std::tie(bucket_context_handle_, s, std::ignore, data_loss_info_,
             std::ignore) =
        idb_factory_->GetOrCreateBucketContext(
            bucket_info, idb_context_->GetDataPath(bucket_locator),
            /*create_if_missing=*/true);
    if (!bucket_context_handle_.IsHeld()) {
      backing_store_ = nullptr;
      return;
    }
    backing_store_ = static_cast<TestableIndexedDBBackingStore*>(
        bucket_context_handle_->backing_store());
    lock_manager_ = bucket_context_handle_->lock_manager();
  }

  std::vector<PartitionedLock> CreateDummyLock() {
    base::RunLoop loop;
    PartitionedLockHolder locks_receiver;
    lock_manager_->AcquireLocks(
        {{{0, "01"}, PartitionedLockManager::LockType::kShared}},
        locks_receiver.AsWeakPtr(),
        base::BindLambdaForTesting([&loop]() { loop.Quit(); }));
    loop.Run();
    return std::move(locks_receiver.locks);
  }

  void DestroyFactoryAndBackingStore() {
    lock_manager_ = nullptr;
    backing_store_ = nullptr;
    bucket_context_handle_.Release();
    idb_factory_.reset();
  }

  void TearDown() override {
    DestroyFactoryAndBackingStore();
    if (idb_context_ && !idb_context_->IsInMemoryContext()) {
      IndexedDBFactory* factory = idb_context_->GetIDBFactory();

      // Loop through all open buckets, and force close them, and request the
      // deletion of the leveldb state. Once the states are no longer around,
      // delete all of the databases on disk.

      for (const auto& bucket_id : factory->GetOpenBuckets()) {
        base::RunLoop loop;
        IndexedDBBucketContext* per_bucket_factory =
            factory->GetBucketContext(bucket_id);

        auto* leveldb_state =
            per_bucket_factory->backing_store()->db()->leveldb_state();

        base::WaitableEvent leveldb_close_event;
        base::WaitableEventWatcher event_watcher;
        leveldb_state->RequestDestruction(&leveldb_close_event);
        event_watcher.StartWatching(
            &leveldb_close_event,
            base::BindLambdaForTesting(
                [&](base::WaitableEvent*) { loop.Quit(); }),
            base::SequencedTaskRunner::GetCurrentDefault());

        idb_context_->ForceClose(
            bucket_id,
            storage::mojom::ForceCloseReason::FORCE_CLOSE_DELETE_ORIGIN,
            base::DoNothing());
        loop.Run();
        // There is a possible race in `leveldb_close_event` where the signaling
        // thread is still in the WaitableEvent::Signal() method. To ensure that
        // the other thread exits their Signal method, any method on the
        // WaitableEvent can be called to acquire the internal lock (which will
        // subsequently wait for the other thread to exit the Signal method).
        EXPECT_TRUE(leveldb_close_event.IsSignaled());
      }
      // All leveldb databases are closed, and they can be deleted.
      for (auto bucket_locator : idb_context_->GetAllBuckets()) {
        base::test::TestFuture<bool> success;
        idb_context_->DeleteForStorageKey(bucket_locator.storage_key,
                                          success.GetCallback());
        EXPECT_TRUE(success.Get());
      }
    }
    if (temp_dir_.IsValid())
      ASSERT_TRUE(temp_dir_.Delete());

    // Wait until the context has fully destroyed.
    scoped_refptr<base::SequencedTaskRunner> task_runner =
        idb_context_->IDBTaskRunner();
    idb_context_.reset();
    {
      base::RunLoop loop;
      task_runner->PostTask(FROM_HERE, loop.QuitClosure());
      loop.Run();
    }
  }

  TestableIndexedDBBackingStore* backing_store() { return backing_store_; }

  // Cycle the idb runner to help clean up tasks, which allows for a clean
  // shutdown of the leveldb database. This ensures that all file handles are
  // released and the folder can be deleted on windows (which doesn't allow
  // folders to be deleted when inside files are in use/exist).
  void CycleIDBTaskRunner() {
    base::RunLoop cycle_loop;
    idb_context_->IDBTaskRunner()->PostTask(FROM_HERE,
                                            cycle_loop.QuitClosure());
    cycle_loop.Run();
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<MockBlobStorageContext> blob_context_;
  std::unique_ptr<MockFileSystemAccessContext> file_system_access_context_;
  scoped_refptr<storage::MockQuotaManager> quota_manager_;
  scoped_refptr<storage::MockQuotaManagerProxy> quota_manager_proxy_;
  scoped_refptr<IndexedDBContextImpl> idb_context_;
  std::unique_ptr<TestIDBFactory> idb_factory_;
  raw_ptr<PartitionedLockManager> lock_manager_ = nullptr;

  IndexedDBBucketContextHandle bucket_context_handle_;
  raw_ptr<TestableIndexedDBBackingStore> backing_store_ = nullptr;
  IndexedDBDataLossInfo data_loss_info_;

  // Sample keys and values that are consistent.
  IndexedDBKey key1_;
  IndexedDBKey key2_;
  IndexedDBValue value1_;
  IndexedDBValue value2_;
};

class IndexedDBBackingStoreTestForThirdPartyStoragePartitioning
    : public testing::WithParamInterface<bool>,
      public IndexedDBBackingStoreTest {
 public:
  IndexedDBBackingStoreTestForThirdPartyStoragePartitioning() {
    scoped_feature_list_.InitWithFeatureState(
        net::features::kThirdPartyStoragePartitioning,
        IsThirdPartyStoragePartitioningEnabled());
  }

  bool IsThirdPartyStoragePartitioningEnabled() { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    IndexedDBBackingStoreTestForThirdPartyStoragePartitioning,
    testing::Bool());

enum class ExternalObjectTestType {
  kOnlyBlobs,
  kOnlyFileSystemAccessHandles,
  kBlobsAndFileSystemAccessHandles
};

class IndexedDBBackingStoreTestWithExternalObjects
    : public testing::WithParamInterface<ExternalObjectTestType>,
      public IndexedDBBackingStoreTest {
 public:
  IndexedDBBackingStoreTestWithExternalObjects() = default;

  IndexedDBBackingStoreTestWithExternalObjects(
      const IndexedDBBackingStoreTestWithExternalObjects&) = delete;
  IndexedDBBackingStoreTestWithExternalObjects& operator=(
      const IndexedDBBackingStoreTestWithExternalObjects&) = delete;

  virtual ExternalObjectTestType TestType() { return GetParam(); }

  bool IncludesBlobs() {
    return TestType() != ExternalObjectTestType::kOnlyFileSystemAccessHandles;
  }

  bool IncludesFileSystemAccessHandles() {
    return TestType() != ExternalObjectTestType::kOnlyBlobs;
  }

  void SetUp() override {
    IndexedDBBackingStoreTest::SetUp();

    const int64_t kTime1 = 13255919133000000ll;
    const int64_t kTime2 = 13287455133000000ll;
    // useful keys and values during tests
    if (IncludesBlobs()) {
      external_objects_.push_back(CreateBlobInfo(u"blob type", 1));
      external_objects_.push_back(CreateBlobInfo(
          u"file name", u"file type",
          base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(kTime1)),
          kBlobFileData1.size()));
      external_objects_.push_back(CreateBlobInfo(
          u"file name", u"file type",
          base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(kTime2)),
          kBlobFileData2.size()));
    }
    if (IncludesFileSystemAccessHandles()) {
      external_objects_.push_back(CreateFileSystemAccessHandle());
      external_objects_.push_back(CreateFileSystemAccessHandle());
    }
    value3_ = IndexedDBValue("value3", external_objects_);
    key3_ = IndexedDBKey(u"key3");
  }

  IndexedDBExternalObject CreateBlobInfo(const std::u16string& file_name,
                                         const std::u16string& type,
                                         base::Time last_modified,
                                         int64_t size) {
    auto uuid = base::Uuid::GenerateRandomV4().AsLowercaseString();
    mojo::PendingRemote<blink::mojom::Blob> remote;
    base::ThreadPool::CreateSequencedTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](std::string uuid,
               mojo::PendingReceiver<blink::mojom::Blob> pending_receiver) {
              mojo::MakeSelfOwnedReceiver(
                  std::make_unique<storage::FakeBlob>(uuid),
                  std::move(pending_receiver));
            },
            uuid, remote.InitWithNewPipeAndPassReceiver()));
    IndexedDBExternalObject info(std::move(remote), uuid, file_name, type,
                                 last_modified, size);
    return info;
  }

  IndexedDBExternalObject CreateBlobInfo(const std::u16string& type,
                                         int64_t size) {
    auto uuid = base::Uuid::GenerateRandomV4().AsLowercaseString();
    mojo::PendingRemote<blink::mojom::Blob> remote;
    base::ThreadPool::CreateSequencedTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](std::string uuid,
               mojo::PendingReceiver<blink::mojom::Blob> pending_receiver) {
              mojo::MakeSelfOwnedReceiver(
                  std::make_unique<storage::FakeBlob>(uuid),
                  std::move(pending_receiver));
            },
            uuid, remote.InitWithNewPipeAndPassReceiver()));
    IndexedDBExternalObject info(std::move(remote), uuid, type, size);
    return info;
  }

  IndexedDBExternalObject CreateFileSystemAccessHandle() {
    auto id = base::UnguessableToken::Create();
    mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> remote;
    base::ThreadPool::CreateSequencedTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](base::UnguessableToken id,
               mojo::PendingReceiver<
                   blink::mojom::FileSystemAccessTransferToken>
                   pending_receiver) {
              mojo::MakeSelfOwnedReceiver(
                  std::make_unique<FakeFileSystemAccessTransferToken>(id),
                  std::move(pending_receiver));
            },
            id, remote.InitWithNewPipeAndPassReceiver()));
    IndexedDBExternalObject info(std::move(remote));
    return info;
  }

  // This just checks the data that survive getting stored and recalled, e.g.
  // the file path and UUID will change and thus aren't verified.
  bool CheckBlobInfoMatches(
      const std::vector<IndexedDBExternalObject>& reads) const {
    DCHECK(idb_context_->IDBTaskRunner()->RunsTasksInCurrentSequence());

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

  bool CheckBlobReadsMatchWrites(
      const std::vector<IndexedDBExternalObject>& reads) const {
    DCHECK(idb_context_->IDBTaskRunner()->RunsTasksInCurrentSequence());

    if (blob_context_->writes().size() +
            file_system_access_context_->writes().size() !=
        reads.size()) {
      return false;
    }
    std::set<base::FilePath> ids;
    for (const auto& write : blob_context_->writes())
      ids.insert(write.path);
    if (ids.size() != blob_context_->writes().size())
      return false;
    for (const auto& read : reads) {
      switch (read.object_type()) {
        case IndexedDBExternalObject::ObjectType::kBlob:
        case IndexedDBExternalObject::ObjectType::kFile:
          if (ids.count(read.indexed_db_file_path()) != 1)
            return false;
          break;
        case IndexedDBExternalObject::ObjectType::kFileSystemAccessHandle:
          if (read.serialized_file_system_access_handle().size() != 1 ||
              read.serialized_file_system_access_handle()[0] >
                  file_system_access_context_->writes().size()) {
            return false;
          }
          break;
      }
    }
    return true;
  }

  bool CheckBlobWrites() {
    DCHECK(idb_context_->IDBTaskRunner()->RunsTasksInCurrentSequence());

    size_t num_empty_blobs = 0;
    for (const auto& info : external_objects_) {
      if (info.object_type() == IndexedDBExternalObject::ObjectType::kFile &&
          !info.size()) {
        num_empty_blobs++;
      }
    }

    size_t num_written = blob_context_->writes().size() +
                         file_system_access_context_->writes().size();
    if (num_written != external_objects_.size() - num_empty_blobs) {
      return false;
    }
    for (size_t i = 0; i < blob_context_->writes().size(); ++i) {
      const BlobWrite& desc = blob_context_->writes()[i];
      const IndexedDBExternalObject& info = external_objects_[i];
      if (!info.size())
        continue;

      base::RunLoop uuid_loop;
      std::string uuid_out;
      DCHECK(desc.blob.is_bound());
      DCHECK(desc.blob.is_connected());
      desc.blob->GetInternalUUID(
          base::BindLambdaForTesting([&](const std::string& uuid) {
            uuid_out = uuid;
            uuid_loop.Quit();
          }));
      uuid_loop.Run();
      if (uuid_out != info.uuid())
        return false;
    }
    for (size_t i = 0; i < file_system_access_context_->writes().size(); ++i) {
      const IndexedDBExternalObject& info =
          external_objects_[blob_context_->writes().size() + i];
      base::UnguessableToken info_token;
      {
        base::RunLoop loop;
        info.file_system_access_token_remote()->GetInternalID(
            base::BindLambdaForTesting(
                [&](const base::UnguessableToken& token) {
                  info_token = token;
                  loop.Quit();
                }));
        loop.Run();
      }
      base::UnguessableToken written_token;
      {
        base::RunLoop loop;
        file_system_access_context_->writes()[i]->GetInternalID(
            base::BindLambdaForTesting(
                [&](const base::UnguessableToken& token) {
                  written_token = token;
                  loop.Quit();
                }));
        loop.Run();
      }
      if (info_token != written_token) {
        EXPECT_EQ(info_token, written_token);
        return false;
      }
    }
    return true;
  }

  bool CheckBlobRemovals() const {
    DCHECK(idb_context_->IDBTaskRunner()->RunsTasksInCurrentSequence());

    if (backing_store_->removals().size() != blob_context_->writes().size())
      return false;
    for (size_t i = 0; i < blob_context_->writes().size(); ++i) {
      if (blob_context_->writes()[i].path != backing_store_->removals()[i]) {
        return false;
      }
    }
    return true;
  }

  std::vector<IndexedDBExternalObject>& external_objects() {
    return external_objects_;
  }

  // Sample keys and values that are consistent. Public so that posted
  // lambdas passed `this` can access them.
  IndexedDBKey key3_;
  IndexedDBValue value3_;

 protected:
  const std::string kBlobFileData1 = "asdfgasdf";
  const std::string kBlobFileData2 = "aaaaaa";

 private:
  // Blob details referenced by `value3_`. The various CheckBlob*() methods
  // can be used to verify the state as a test progresses.
  std::vector<IndexedDBExternalObject> external_objects_;

  std::vector<std::string> blob_remote_uuids_;
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    IndexedDBBackingStoreTestWithExternalObjects,
    ::testing::Values(
        ExternalObjectTestType::kOnlyBlobs,
        ExternalObjectTestType::kOnlyFileSystemAccessHandles,
        ExternalObjectTestType::kBlobsAndFileSystemAccessHandles));

class IndexedDBBackingStoreTestWithBlobs
    : public IndexedDBBackingStoreTestWithExternalObjects {
 public:
  ExternalObjectTestType TestType() override {
    return ExternalObjectTestType::kOnlyBlobs;
  }
};

BlobWriteCallback CreateBlobWriteCallback(
    bool* succeeded,
    base::OnceClosure on_done = base::OnceClosure()) {
  *succeeded = false;
  return base::BindOnce(
      [](bool* succeeded, base::OnceClosure on_done, BlobWriteResult result,
         storage::mojom::WriteBlobToFileResult error) {
        switch (result) {
          case BlobWriteResult::kFailure:
            NOTREACHED();
            break;
          case BlobWriteResult::kRunPhaseTwoAsync:
          case BlobWriteResult::kRunPhaseTwoAndReturnResult:
            DCHECK_EQ(error, storage::mojom::WriteBlobToFileResult::kSuccess);
            *succeeded = true;
            break;
        }
        if (!on_done.is_null())
          std::move(on_done).Run();
        return leveldb::Status::OK();
      },
      succeeded, std::move(on_done));
}

TEST_F(IndexedDBBackingStoreTest, PutGetConsistency) {
  base::RunLoop loop;
  idb_context_->IDBTaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        const IndexedDBKey key = key1_;
        IndexedDBValue value = value1_;
        {
          IndexedDBBackingStore::Transaction transaction1(
              backing_store()->AsWeakPtr(),
              blink::mojom::IDBTransactionDurability::Relaxed,
              blink::mojom::IDBTransactionMode::ReadWrite);
          transaction1.Begin(CreateDummyLock());
          IndexedDBBackingStore::RecordIdentifier record;
          leveldb::Status s = backing_store()->PutRecord(&transaction1, 1, 1,
                                                         key, &value, &record);
          EXPECT_TRUE(s.ok());
          bool succeeded = false;
          EXPECT_TRUE(
              transaction1.CommitPhaseOne(CreateBlobWriteCallback(&succeeded))
                  .ok());
          EXPECT_TRUE(succeeded);
          EXPECT_TRUE(transaction1.CommitPhaseTwo().ok());
        }

        {
          IndexedDBBackingStore::Transaction transaction2(
              backing_store()->AsWeakPtr(),
              blink::mojom::IDBTransactionDurability::Relaxed,
              blink::mojom::IDBTransactionMode::ReadWrite);
          transaction2.Begin(CreateDummyLock());
          IndexedDBValue result_value;
          EXPECT_TRUE(backing_store()
                          ->GetRecord(&transaction2, 1, 1, key, &result_value)
                          .ok());
          bool succeeded = false;
          EXPECT_TRUE(
              transaction2.CommitPhaseOne(CreateBlobWriteCallback(&succeeded))
                  .ok());
          EXPECT_TRUE(succeeded);
          EXPECT_TRUE(transaction2.CommitPhaseTwo().ok());
          EXPECT_EQ(value.bits, result_value.bits);
        }
        loop.Quit();
      }));
  loop.Run();

  CycleIDBTaskRunner();
}

TEST_P(IndexedDBBackingStoreTestWithExternalObjects, PutGetConsistency) {
  // Initiate transaction1 - writing blobs.
  std::unique_ptr<IndexedDBBackingStore::Transaction> transaction1 =
      std::make_unique<IndexedDBBackingStore::Transaction>(
          backing_store()->AsWeakPtr(),
          blink::mojom::IDBTransactionDurability::Relaxed,
          blink::mojom::IDBTransactionMode::ReadWrite);
  transaction1->Begin(CreateDummyLock());
  IndexedDBBackingStore::RecordIdentifier record;
  EXPECT_TRUE(
      backing_store()
          ->PutRecord(transaction1.get(), 1, 1, key3_, &value3_, &record)
          .ok());
  bool succeeded = false;
  base::RunLoop phase_one_wait;
  EXPECT_TRUE(transaction1
                  ->CommitPhaseOne(CreateBlobWriteCallback(
                      &succeeded, phase_one_wait.QuitClosure()))
                  .ok());
  EXPECT_FALSE(succeeded);
  task_environment_.RunUntilIdle();
  phase_one_wait.Run();

  // Finish up transaction1, verifying blob writes.

  EXPECT_TRUE(succeeded);
  EXPECT_TRUE(CheckBlobWrites());
  EXPECT_TRUE(transaction1->CommitPhaseTwo().ok());

  // Initiate transaction2, reading blobs.
  IndexedDBBackingStore::Transaction transaction2(
      backing_store()->AsWeakPtr(),
      blink::mojom::IDBTransactionDurability::Relaxed,
      blink::mojom::IDBTransactionMode::ReadWrite);
  transaction2.Begin(CreateDummyLock());
  IndexedDBValue result_value;
  EXPECT_TRUE(backing_store()
                  ->GetRecord(&transaction2, 1, 1, key3_, &result_value)
                  .ok());

  // Finish up transaction2, verifying blob reads.
  succeeded = false;
  EXPECT_TRUE(
      transaction2.CommitPhaseOne(CreateBlobWriteCallback(&succeeded)).ok());
  EXPECT_TRUE(succeeded);
  EXPECT_TRUE(transaction2.CommitPhaseTwo().ok());
  EXPECT_EQ(value3_.bits, result_value.bits);

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(CheckBlobInfoMatches(result_value.external_objects));
  EXPECT_TRUE(CheckBlobReadsMatchWrites(result_value.external_objects));

  // Initiate transaction3, deleting blobs.
  std::unique_ptr<IndexedDBBackingStore::Transaction> transaction3 =
      std::make_unique<IndexedDBBackingStore::Transaction>(
          backing_store()->AsWeakPtr(),
          blink::mojom::IDBTransactionDurability::Relaxed,
          blink::mojom::IDBTransactionMode::ReadWrite);
  transaction3->Begin(CreateDummyLock());
  EXPECT_TRUE(
      backing_store()
          ->DeleteRange(transaction3.get(), 1, 1, IndexedDBKeyRange(key3_))
          .ok());
  succeeded = false;
  EXPECT_TRUE(
      transaction3->CommitPhaseOne(CreateBlobWriteCallback(&succeeded)).ok());
  EXPECT_TRUE(succeeded);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(succeeded);

  // Finish up transaction 3, verifying blob deletes.
  EXPECT_TRUE(transaction3->CommitPhaseTwo().ok());
  EXPECT_TRUE(CheckBlobRemovals());

  // Clean up on the IDB sequence.
  transaction1.reset();
  transaction3.reset();
  task_environment_.RunUntilIdle();
}

// http://crbug.com/1131151
// Validate that recovery journal cleanup during a transaction does
// not delete blobs that were just written.
TEST_P(IndexedDBBackingStoreTestWithExternalObjects, BlobWriteCleanup) {
  const std::vector<IndexedDBKey> keys = {
      IndexedDBKey(u"key0"), IndexedDBKey(u"key1"), IndexedDBKey(u"key2"),
      IndexedDBKey(u"key3")};

  const int64_t database_id = 1;
  const int64_t object_store_id = 1;

  external_objects().clear();
  for (size_t j = 0; j < 4; ++j) {
    std::string type = "type " + base::NumberToString(j);
    external_objects().push_back(CreateBlobInfo(base::UTF8ToUTF16(type), 1));
  }

  std::vector<IndexedDBValue> values = {
      IndexedDBValue("value0", {external_objects()[0]}),
      IndexedDBValue("value1", {external_objects()[1]}),
      IndexedDBValue("value2", {external_objects()[2]}),
      IndexedDBValue("value3", {external_objects()[3]}),
  };
  ASSERT_GE(keys.size(), values.size());

  // Validate that cleaning up after writing blobs does not delete those
  // blobs.
  backing_store()->SetExecuteJournalCleaningOnNoTransactionsForTesting();

  std::unique_ptr<IndexedDBBackingStore::Transaction> transaction1 =
      std::make_unique<IndexedDBBackingStore::Transaction>(
          backing_store()->AsWeakPtr(),
          blink::mojom::IDBTransactionDurability::Relaxed,
          blink::mojom::IDBTransactionMode::ReadWrite);
  transaction1->Begin(CreateDummyLock());
  IndexedDBBackingStore::RecordIdentifier record;
  for (size_t i = 0; i < values.size(); ++i) {
    EXPECT_TRUE(backing_store()
                    ->PutRecord(transaction1.get(), database_id,
                                object_store_id, keys[i], &values[i], &record)
                    .ok());
  }

  // Start committing transaction1.
  bool succeeded = false;
  EXPECT_TRUE(
      transaction1->CommitPhaseOne(CreateBlobWriteCallback(&succeeded)).ok());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(CheckBlobWrites());

  // Finish committing transaction1.
  EXPECT_TRUE(succeeded);
  EXPECT_TRUE(transaction1->CommitPhaseTwo().ok());

  // Verify lack of blob removals.
  ASSERT_EQ(0UL, backing_store()->removals().size());

  // Clean up on the IDB sequence.
  transaction1.reset();
  task_environment_.RunUntilIdle();
}

TEST_P(IndexedDBBackingStoreTestWithExternalObjects, DeleteRange) {
  const std::vector<IndexedDBKey> keys = {
      IndexedDBKey(u"key0"), IndexedDBKey(u"key1"), IndexedDBKey(u"key2"),
      IndexedDBKey(u"key3")};
  const IndexedDBKeyRange ranges[] = {
      IndexedDBKeyRange(keys[1], keys[2], false, false),
      IndexedDBKeyRange(keys[1], keys[2], false, false),
      IndexedDBKeyRange(keys[0], keys[2], true, false),
      IndexedDBKeyRange(keys[1], keys[3], false, true),
      IndexedDBKeyRange(keys[0], keys[3], true, true)};

  for (size_t i = 0; i < std::size(ranges); ++i) {
    const int64_t database_id = 1;
    const int64_t object_store_id = i + 1;
    const IndexedDBKeyRange& range = ranges[i];

    std::vector<IndexedDBExternalObject> external_objects;
    for (size_t j = 0; j < 4; ++j) {
      std::string type = "type " + base::NumberToString(j);
      external_objects.push_back(CreateBlobInfo(base::UTF8ToUTF16(type), 1));
    }

    // Reset from previous iteration.
    blob_context_->ClearWrites();
    file_system_access_context_->ClearWrites();
    backing_store()->ClearRemovals();

    std::vector<IndexedDBValue> values = {
        IndexedDBValue("value0", {external_objects[0]}),
        IndexedDBValue("value1", {external_objects[1]}),
        IndexedDBValue("value2", {external_objects[2]}),
        IndexedDBValue("value3", {external_objects[3]}),
    };
    ASSERT_GE(keys.size(), values.size());

    // Initiate transaction1 - write records.
    std::unique_ptr<IndexedDBBackingStore::Transaction> transaction1 =
        std::make_unique<IndexedDBBackingStore::Transaction>(
            backing_store()->AsWeakPtr(),
            blink::mojom::IDBTransactionDurability::Relaxed,
            blink::mojom::IDBTransactionMode::ReadWrite);
    transaction1->Begin(CreateDummyLock());
    IndexedDBBackingStore::RecordIdentifier record;
    for (size_t j = 0; j < values.size(); ++j) {
      EXPECT_TRUE(backing_store()
                      ->PutRecord(transaction1.get(), database_id,
                                  object_store_id, keys[j], &values[j], &record)
                      .ok());
    }

    // Start committing transaction1.
    bool succeeded = false;
    EXPECT_TRUE(
        transaction1->CommitPhaseOne(CreateBlobWriteCallback(&succeeded)).ok());
    task_environment_.RunUntilIdle();

    // Finish committing transaction1.

    EXPECT_TRUE(succeeded);
    EXPECT_TRUE(transaction1->CommitPhaseTwo().ok());

    // Initiate transaction 2 - delete range.
    std::unique_ptr<IndexedDBBackingStore::Transaction> transaction2 =
        std::make_unique<IndexedDBBackingStore::Transaction>(
            backing_store()->AsWeakPtr(),
            blink::mojom::IDBTransactionDurability::Relaxed,
            blink::mojom::IDBTransactionMode::ReadWrite);
    transaction2->Begin(CreateDummyLock());
    IndexedDBValue result_value;
    EXPECT_TRUE(backing_store()
                    ->DeleteRange(transaction2.get(), database_id,
                                  object_store_id, range)
                    .ok());

    // Start committing transaction2.
    succeeded = false;
    EXPECT_TRUE(
        transaction2->CommitPhaseOne(CreateBlobWriteCallback(&succeeded)).ok());
    task_environment_.RunUntilIdle();

    // Finish committing transaction2.

    EXPECT_TRUE(succeeded);
    EXPECT_TRUE(transaction2->CommitPhaseTwo().ok());

    // Verify blob removals.
    ASSERT_EQ(2UL, backing_store()->removals().size());
    EXPECT_EQ(blob_context_->writes()[1].path, backing_store()->removals()[0]);
    EXPECT_EQ(blob_context_->writes()[2].path, backing_store()->removals()[1]);

    // Clean up on the IDB sequence.
    transaction1.reset();
    transaction2.reset();
    task_environment_.RunUntilIdle();
  }
}

TEST_P(IndexedDBBackingStoreTestWithExternalObjects, DeleteRangeEmptyRange) {
  const std::vector<IndexedDBKey> keys = {
      IndexedDBKey(u"key0"), IndexedDBKey(u"key1"), IndexedDBKey(u"key2"),
      IndexedDBKey(u"key3"), IndexedDBKey(u"key4")};
  const IndexedDBKeyRange ranges[] = {
      IndexedDBKeyRange(keys[3], keys[4], true, false),
      IndexedDBKeyRange(keys[2], keys[1], false, false),
      IndexedDBKeyRange(keys[2], keys[1], true, true)};

  for (size_t i = 0; i < std::size(ranges); ++i) {
    const int64_t database_id = 1;
    const int64_t object_store_id = i + 1;
    const IndexedDBKeyRange& range = ranges[i];

    std::vector<IndexedDBExternalObject> external_objects;
    for (size_t j = 0; j < 4; ++j) {
      std::string type = "type " + base::NumberToString(j);
      external_objects.push_back(CreateBlobInfo(base::UTF8ToUTF16(type), 1));
    }

    // Reset from previous iteration.
    blob_context_->ClearWrites();
    file_system_access_context_->ClearWrites();
    backing_store()->ClearRemovals();

    std::vector<IndexedDBValue> values = {
        IndexedDBValue("value0", {external_objects[0]}),
        IndexedDBValue("value1", {external_objects[1]}),
        IndexedDBValue("value2", {external_objects[2]}),
        IndexedDBValue("value3", {external_objects[3]}),
    };
    ASSERT_GE(keys.size(), values.size());

    // Initiate transaction1 - write records.
    std::unique_ptr<IndexedDBBackingStore::Transaction> transaction1 =
        std::make_unique<IndexedDBBackingStore::Transaction>(
            backing_store()->AsWeakPtr(),
            blink::mojom::IDBTransactionDurability::Relaxed,
            blink::mojom::IDBTransactionMode::ReadWrite);
    transaction1->Begin(CreateDummyLock());

    IndexedDBBackingStore::RecordIdentifier record;
    for (size_t j = 0; j < values.size(); ++j) {
      EXPECT_TRUE(backing_store()
                      ->PutRecord(transaction1.get(), database_id,
                                  object_store_id, keys[j], &values[j], &record)
                      .ok());
    }
    // Start committing transaction1.
    bool succeeded = false;
    EXPECT_TRUE(
        transaction1->CommitPhaseOne(CreateBlobWriteCallback(&succeeded)).ok());
    task_environment_.RunUntilIdle();

    // Finish committing transaction1.
    EXPECT_TRUE(succeeded);
    EXPECT_TRUE(transaction1->CommitPhaseTwo().ok());

    // Initiate transaction 2 - delete range.
    std::unique_ptr<IndexedDBBackingStore::Transaction> transaction2 =
        std::make_unique<IndexedDBBackingStore::Transaction>(
            backing_store()->AsWeakPtr(),
            blink::mojom::IDBTransactionDurability::Relaxed,
            blink::mojom::IDBTransactionMode::ReadWrite);
    transaction2->Begin(CreateDummyLock());
    IndexedDBValue result_value;
    EXPECT_TRUE(backing_store()
                    ->DeleteRange(transaction2.get(), database_id,
                                  object_store_id, range)
                    .ok());

    // Start committing transaction2.
    succeeded = false;
    EXPECT_TRUE(
        transaction2->CommitPhaseOne(CreateBlobWriteCallback(&succeeded)).ok());
    task_environment_.RunUntilIdle();

    // Finish committing transaction2.
    EXPECT_TRUE(succeeded);
    EXPECT_TRUE(transaction2->CommitPhaseTwo().ok());

    // Verify blob removals.
    EXPECT_EQ(0UL, backing_store()->removals().size());

    // Clean on the IDB sequence.
    transaction1.reset();
    transaction2.reset();
    task_environment_.RunUntilIdle();
  }
}

TEST_P(IndexedDBBackingStoreTestWithExternalObjects,
       BlobJournalInterleavedTransactions) {
  // Initiate transaction1.
  std::unique_ptr<IndexedDBBackingStore::Transaction> transaction1 =
      std::make_unique<IndexedDBBackingStore::Transaction>(
          backing_store()->AsWeakPtr(),
          blink::mojom::IDBTransactionDurability::Relaxed,
          blink::mojom::IDBTransactionMode::ReadWrite);
  transaction1->Begin(CreateDummyLock());
  IndexedDBBackingStore::RecordIdentifier record1;
  EXPECT_TRUE(
      backing_store()
          ->PutRecord(transaction1.get(), 1, 1, key3_, &value3_, &record1)
          .ok());
  bool succeeded = false;
  EXPECT_TRUE(
      transaction1->CommitPhaseOne(CreateBlobWriteCallback(&succeeded)).ok());
  task_environment_.RunUntilIdle();

  // Verify transaction1 phase one completed.

  EXPECT_TRUE(succeeded);
  EXPECT_TRUE(CheckBlobWrites());
  EXPECT_EQ(0U, backing_store()->removals().size());

  // Initiate transaction2.
  std::unique_ptr<IndexedDBBackingStore::Transaction> transaction2 =
      std::make_unique<IndexedDBBackingStore::Transaction>(
          backing_store()->AsWeakPtr(),
          blink::mojom::IDBTransactionDurability::Relaxed,
          blink::mojom::IDBTransactionMode::ReadWrite);
  transaction2->Begin(CreateDummyLock());
  IndexedDBBackingStore::RecordIdentifier record2;
  EXPECT_TRUE(
      backing_store()
          ->PutRecord(transaction2.get(), 1, 1, key1_, &value1_, &record2)
          .ok());
  succeeded = false;
  EXPECT_TRUE(
      transaction2->CommitPhaseOne(CreateBlobWriteCallback(&succeeded)).ok());
  task_environment_.RunUntilIdle();

  // Verify transaction2 phase one completed.
  EXPECT_TRUE(succeeded);
  EXPECT_TRUE(CheckBlobWrites());
  EXPECT_EQ(0U, backing_store()->removals().size());

  // Finalize both transactions.
  EXPECT_TRUE(transaction1->CommitPhaseTwo().ok());
  EXPECT_EQ(0U, backing_store()->removals().size());

  EXPECT_TRUE(transaction2->CommitPhaseTwo().ok());
  EXPECT_EQ(0U, backing_store()->removals().size());

  // Clean up on the IDB sequence.
  transaction1.reset();
  transaction2.reset();
  task_environment_.RunUntilIdle();
}

TEST_P(IndexedDBBackingStoreTestWithExternalObjects, ActiveBlobJournal) {
  std::unique_ptr<IndexedDBBackingStore::Transaction> transaction1 =
      std::make_unique<IndexedDBBackingStore::Transaction>(
          backing_store()->AsWeakPtr(),
          blink::mojom::IDBTransactionDurability::Relaxed,
          blink::mojom::IDBTransactionMode::ReadWrite);
  transaction1->Begin(CreateDummyLock());
  IndexedDBBackingStore::RecordIdentifier record;
  EXPECT_TRUE(
      backing_store()
          ->PutRecord(transaction1.get(), 1, 1, key3_, &value3_, &record)
          .ok());
  bool succeeded = false;
  EXPECT_TRUE(
      transaction1->CommitPhaseOne(CreateBlobWriteCallback(&succeeded)).ok());

  task_environment_.RunUntilIdle();

  EXPECT_TRUE(succeeded);
  EXPECT_TRUE(CheckBlobWrites());
  EXPECT_TRUE(transaction1->CommitPhaseTwo().ok());

  IndexedDBBackingStore::Transaction transaction2(
      backing_store()->AsWeakPtr(),
      blink::mojom::IDBTransactionDurability::Relaxed,
      blink::mojom::IDBTransactionMode::ReadWrite);
  transaction2.Begin(CreateDummyLock());
  IndexedDBValue read_result_value;
  EXPECT_TRUE(backing_store()
                  ->GetRecord(&transaction2, 1, 1, key3_, &read_result_value)
                  .ok());
  succeeded = false;

  EXPECT_TRUE(
      transaction2.CommitPhaseOne(CreateBlobWriteCallback(&succeeded)).ok());

  EXPECT_TRUE(succeeded);
  EXPECT_TRUE(transaction2.CommitPhaseTwo().ok());
  EXPECT_EQ(value3_.bits, read_result_value.bits);
  EXPECT_TRUE(CheckBlobInfoMatches(read_result_value.external_objects));
  EXPECT_TRUE(CheckBlobReadsMatchWrites(read_result_value.external_objects));
  for (const IndexedDBExternalObject& external_object :
       read_result_value.external_objects) {
    if (external_object.mark_used_callback())
      external_object.mark_used_callback().Run();
  }

  std::unique_ptr<IndexedDBBackingStore::Transaction> transaction3 =
      std::make_unique<IndexedDBBackingStore::Transaction>(
          backing_store()->AsWeakPtr(),
          blink::mojom::IDBTransactionDurability::Relaxed,
          blink::mojom::IDBTransactionMode::ReadWrite);
  transaction3->Begin(CreateDummyLock());
  EXPECT_TRUE(
      backing_store()
          ->DeleteRange(transaction3.get(), 1, 1, IndexedDBKeyRange(key3_))
          .ok());
  succeeded = false;
  EXPECT_TRUE(
      transaction3->CommitPhaseOne(CreateBlobWriteCallback(&succeeded)).ok());
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(succeeded);
  EXPECT_TRUE(transaction3->CommitPhaseTwo().ok());
  EXPECT_EQ(0U, backing_store()->removals().size());
  for (const IndexedDBExternalObject& external_object :
       read_result_value.external_objects) {
    if (external_object.release_callback())
      external_object.release_callback().Run();
  }
  task_environment_.RunUntilIdle();

  if (TestType() != ExternalObjectTestType::kOnlyFileSystemAccessHandles) {
    EXPECT_TRUE(backing_store()->IsBlobCleanupPending());
#if DCHECK_IS_ON()
    EXPECT_EQ(
        3, backing_store()->NumAggregatedJournalCleaningRequestsForTesting());
#endif
    for (int i = 3; i < IndexedDBBackingStore::kMaxJournalCleanRequests; ++i) {
      backing_store()->StartJournalCleaningTimer();
    }
    EXPECT_NE(0U, backing_store()->removals().size());
    EXPECT_TRUE(CheckBlobRemovals());
#if DCHECK_IS_ON()
    EXPECT_EQ(3, backing_store()->NumBlobFilesDeletedForTesting());
#endif
  }

  EXPECT_FALSE(backing_store()->IsBlobCleanupPending());

  // Clean on the IDB sequence.
  transaction1.reset();
  transaction3.reset();
  task_environment_.RunUntilIdle();
}

// Make sure that using very high ( more than 32 bit ) values for
// database_id and object_store_id still work.
TEST_F(IndexedDBBackingStoreTest, HighIds) {
  IndexedDBKey key1 = key1_;
  IndexedDBKey key2 = key2_;
  IndexedDBValue value1 = value1_;

  const int64_t high_database_id = 1ULL << 35;
  const int64_t high_object_store_id = 1ULL << 39;
  // index_ids are capped at 32 bits for storage purposes.
  const int64_t high_index_id = 1ULL << 29;

  const int64_t invalid_high_index_id = 1ULL << 37;

  const IndexedDBKey& index_key = key2;
  std::string index_key_raw;
  EncodeIDBKey(index_key, &index_key_raw);
  {
    IndexedDBBackingStore::Transaction transaction1(
        backing_store()->AsWeakPtr(),
        blink::mojom::IDBTransactionDurability::Relaxed,
        blink::mojom::IDBTransactionMode::ReadWrite);
    transaction1.Begin(CreateDummyLock());
    IndexedDBBackingStore::RecordIdentifier record;
    leveldb::Status s = backing_store()->PutRecord(
        &transaction1, high_database_id, high_object_store_id, key1, &value1,
        &record);
    EXPECT_TRUE(s.ok());

    s = backing_store()->PutIndexDataForRecord(
        &transaction1, high_database_id, high_object_store_id,
        invalid_high_index_id, index_key, record);
    EXPECT_FALSE(s.ok());

    s = backing_store()->PutIndexDataForRecord(
        &transaction1, high_database_id, high_object_store_id, high_index_id,
        index_key, record);
    EXPECT_TRUE(s.ok());

    bool succeeded = false;
    EXPECT_TRUE(
        transaction1.CommitPhaseOne(CreateBlobWriteCallback(&succeeded)).ok());
    EXPECT_TRUE(succeeded);
    EXPECT_TRUE(transaction1.CommitPhaseTwo().ok());
  }

  {
    IndexedDBBackingStore::Transaction transaction2(
        backing_store()->AsWeakPtr(),
        blink::mojom::IDBTransactionDurability::Relaxed,
        blink::mojom::IDBTransactionMode::ReadWrite);
    transaction2.Begin(CreateDummyLock());
    IndexedDBValue result_value;
    leveldb::Status s =
        backing_store()->GetRecord(&transaction2, high_database_id,
                                   high_object_store_id, key1, &result_value);
    EXPECT_TRUE(s.ok());
    EXPECT_EQ(value1.bits, result_value.bits);

    std::unique_ptr<IndexedDBKey> new_primary_key;
    s = backing_store()->GetPrimaryKeyViaIndex(
        &transaction2, high_database_id, high_object_store_id,
        invalid_high_index_id, index_key, &new_primary_key);
    EXPECT_FALSE(s.ok());

    s = backing_store()->GetPrimaryKeyViaIndex(
        &transaction2, high_database_id, high_object_store_id, high_index_id,
        index_key, &new_primary_key);
    EXPECT_TRUE(s.ok());
    EXPECT_TRUE(new_primary_key->Equals(key1));

    bool succeeded = false;
    EXPECT_TRUE(
        transaction2.CommitPhaseOne(CreateBlobWriteCallback(&succeeded)).ok());
    EXPECT_TRUE(succeeded);
    EXPECT_TRUE(transaction2.CommitPhaseTwo().ok());
  }

  CycleIDBTaskRunner();
}

// Make sure that other invalid ids do not crash.
TEST_F(IndexedDBBackingStoreTest, InvalidIds) {
  base::RunLoop loop;
  idb_context_->IDBTaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        const IndexedDBKey key = key1_;
        IndexedDBValue value = value1_;

        // valid ids for use when testing invalid ids
        const int64_t database_id = 1;
        const int64_t object_store_id = 1;
        const int64_t index_id = kMinimumIndexId;
        // index_ids must be > kMinimumIndexId
        const int64_t invalid_low_index_id = 19;
        IndexedDBValue result_value;

        IndexedDBBackingStore::Transaction transaction1(
            backing_store()->AsWeakPtr(),
            blink::mojom::IDBTransactionDurability::Relaxed,
            blink::mojom::IDBTransactionMode::ReadWrite);
        transaction1.Begin(CreateDummyLock());

        IndexedDBBackingStore::RecordIdentifier record;
        leveldb::Status s = backing_store()->PutRecord(
            &transaction1, database_id, KeyPrefix::kInvalidId, key, &value,
            &record);
        EXPECT_FALSE(s.ok());
        s = backing_store()->PutRecord(&transaction1, database_id, 0, key,
                                       &value, &record);
        EXPECT_FALSE(s.ok());
        s = backing_store()->PutRecord(&transaction1, KeyPrefix::kInvalidId,
                                       object_store_id, key, &value, &record);
        EXPECT_FALSE(s.ok());
        s = backing_store()->PutRecord(&transaction1, 0, object_store_id, key,
                                       &value, &record);
        EXPECT_FALSE(s.ok());

        s = backing_store()->GetRecord(&transaction1, database_id,
                                       KeyPrefix::kInvalidId, key,
                                       &result_value);
        EXPECT_FALSE(s.ok());
        s = backing_store()->GetRecord(&transaction1, database_id, 0, key,
                                       &result_value);
        EXPECT_FALSE(s.ok());
        s = backing_store()->GetRecord(&transaction1, KeyPrefix::kInvalidId,
                                       object_store_id, key, &result_value);
        EXPECT_FALSE(s.ok());
        s = backing_store()->GetRecord(&transaction1, 0, object_store_id, key,
                                       &result_value);
        EXPECT_FALSE(s.ok());

        std::unique_ptr<IndexedDBKey> new_primary_key;
        s = backing_store()->GetPrimaryKeyViaIndex(
            &transaction1, database_id, object_store_id, KeyPrefix::kInvalidId,
            key, &new_primary_key);
        EXPECT_FALSE(s.ok());
        s = backing_store()->GetPrimaryKeyViaIndex(
            &transaction1, database_id, object_store_id, invalid_low_index_id,
            key, &new_primary_key);
        EXPECT_FALSE(s.ok());
        s = backing_store()->GetPrimaryKeyViaIndex(&transaction1, database_id,
                                                   object_store_id, 0, key,
                                                   &new_primary_key);
        EXPECT_FALSE(s.ok());

        s = backing_store()->GetPrimaryKeyViaIndex(
            &transaction1, KeyPrefix::kInvalidId, object_store_id, index_id,
            key, &new_primary_key);
        EXPECT_FALSE(s.ok());
        s = backing_store()->GetPrimaryKeyViaIndex(
            &transaction1, database_id, KeyPrefix::kInvalidId, index_id, key,
            &new_primary_key);
        EXPECT_FALSE(s.ok());
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(IndexedDBBackingStoreTest, CreateDatabase) {
  base::RunLoop loop;
  idb_context_->IDBTaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        const std::u16string database_name(u"db1");
        int64_t database_id;
        const int64_t version = 9;

        const int64_t object_store_id = 99;
        const std::u16string object_store_name(u"object_store1");
        const bool auto_increment = true;
        const IndexedDBKeyPath object_store_key_path(u"object_store_key");

        const int64_t index_id = 999;
        const std::u16string index_name(u"index1");
        const bool unique = true;
        const bool multi_entry = true;
        const IndexedDBKeyPath index_key_path(u"index_key");

        {
          IndexedDBDatabaseMetadata database;
          database.name = database_name;
          database.version = version;
          leveldb::Status s = backing_store()->CreateDatabase(database);
          EXPECT_TRUE(s.ok());
          EXPECT_GT(database.id, 0);
          database_id = database.id;

          IndexedDBBackingStore::Transaction transaction(
              backing_store()->AsWeakPtr(),
              blink::mojom::IDBTransactionDurability::Relaxed,
              blink::mojom::IDBTransactionMode::ReadWrite);
          transaction.Begin(CreateDummyLock());

          IndexedDBObjectStoreMetadata object_store;
          s = backing_store()->CreateObjectStore(
              &transaction, database.id, object_store_id, object_store_name,
              object_store_key_path, auto_increment, &object_store);
          EXPECT_TRUE(s.ok());

          IndexedDBIndexMetadata index;
          s = backing_store()->CreateIndex(
              &transaction, database.id, object_store.id, index_id, index_name,
              index_key_path, unique, multi_entry, &index);
          EXPECT_TRUE(s.ok());

          bool succeeded = false;
          EXPECT_TRUE(
              transaction.CommitPhaseOne(CreateBlobWriteCallback(&succeeded))
                  .ok());
          EXPECT_TRUE(succeeded);
          EXPECT_TRUE(transaction.CommitPhaseTwo().ok());
        }

        {
          IndexedDBDatabaseMetadata database;
          bool found;
          leveldb::Status s = backing_store()->ReadMetadataForDatabaseName(
              database_name, &database, &found);
          EXPECT_TRUE(s.ok());
          EXPECT_TRUE(found);

          // database.name is not filled in by the implementation.
          EXPECT_EQ(version, database.version);
          EXPECT_EQ(database_id, database.id);

          EXPECT_EQ(1UL, database.object_stores.size());
          IndexedDBObjectStoreMetadata object_store =
              database.object_stores[object_store_id];
          EXPECT_EQ(object_store_name, object_store.name);
          EXPECT_EQ(object_store_key_path, object_store.key_path);
          EXPECT_EQ(auto_increment, object_store.auto_increment);

          EXPECT_EQ(1UL, object_store.indexes.size());
          IndexedDBIndexMetadata index = object_store.indexes[index_id];
          EXPECT_EQ(index_name, index.name);
          EXPECT_EQ(index_key_path, index.key_path);
          EXPECT_EQ(unique, index.unique);
          EXPECT_EQ(multi_entry, index.multi_entry);
        }
        loop.Quit();
      }));
  loop.Run();

  {
    // Cycle the idb runner to help clean up tasks for the Windows tests.
    base::RunLoop cycle_loop;
    idb_context_->IDBTaskRunner()->PostTask(FROM_HERE,
                                            cycle_loop.QuitClosure());
    cycle_loop.Run();
  }
}

TEST_F(IndexedDBBackingStoreTest, GetDatabaseNames) {
  const std::u16string db1_name(u"db1");
  const int64_t db1_version = 1LL;

  // Database records with DEFAULT_VERSION represent
  // stale data, and should not be enumerated.
  const std::u16string db2_name(u"db2");
  const int64_t db2_version = IndexedDBDatabaseMetadata::DEFAULT_VERSION;

  IndexedDBDatabaseMetadata db1;
  db1.name = db1_name;
  db1.version = db1_version;
  leveldb::Status s = backing_store()->CreateDatabase(db1);

  EXPECT_TRUE(s.ok());
  EXPECT_GT(db1.id, 0LL);

  IndexedDBDatabaseMetadata db2;
  db2.name = db2_name;
  db2.version = db2_version;
  s = backing_store()->CreateDatabase(db2);

  EXPECT_TRUE(s.ok());
  EXPECT_GT(db2.id, db1.id);

  std::vector<std::u16string> names;
  s = backing_store()->GetDatabaseNames(&names);
  EXPECT_TRUE(s.ok());
  ASSERT_EQ(1U, names.size());
  EXPECT_EQ(db1_name, names[0]);
}

TEST_P(IndexedDBBackingStoreTestForThirdPartyStoragePartitioning,
       ReadCorruptionInfoForOpaqueStorageKey) {
  auto filesystem_proxy = std::make_unique<storage::FilesystemProxy>(
      storage::FilesystemProxy::UNRESTRICTED, base::FilePath());
  storage::BucketLocator bucket_locator;
  bucket_locator.storage_key =
      blink::StorageKey::CreateFirstParty(url::Origin());
  bucket_locator.is_default = true;

  // No `path_base`.
  EXPECT_TRUE(indexed_db::ReadCorruptionInfo(filesystem_proxy.get(),
                                             base::FilePath(), bucket_locator)
                  .empty());
}

TEST_P(IndexedDBBackingStoreTestForThirdPartyStoragePartitioning,
       ReadCorruptionInfoForFirstPartyStorageKey) {
  auto filesystem_proxy = std::make_unique<storage::FilesystemProxy>(
      storage::FilesystemProxy::UNRESTRICTED, base::FilePath());
  storage::BucketLocator bucket_locator;
  const base::FilePath path_base = temp_dir_.GetPath();
  bucket_locator.storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://www.google.com/");
  bucket_locator.id = storage::BucketId::FromUnsafeValue(1);
  bucket_locator.is_default = true;
  ASSERT_FALSE(path_base.empty());

  // File not found.
  EXPECT_TRUE(indexed_db::ReadCorruptionInfo(filesystem_proxy.get(), path_base,
                                             bucket_locator)
                  .empty());

  const base::FilePath info_path =
      path_base.AppendASCII("http_www.google.com_0.indexeddb.leveldb")
          .AppendASCII("corruption_info.json");
  ASSERT_TRUE(CreateDirectory(info_path.DirName()));

  // Empty file.
  std::string dummy_data;
  ASSERT_TRUE(base::WriteFile(info_path, dummy_data));
  EXPECT_TRUE(indexed_db::ReadCorruptionInfo(filesystem_proxy.get(), path_base,
                                             bucket_locator)
                  .empty());
  EXPECT_FALSE(PathExists(info_path));

  // File size > 4 KB.
  dummy_data.resize(5000, 'c');
  ASSERT_TRUE(base::WriteFile(info_path, dummy_data));
  EXPECT_TRUE(indexed_db::ReadCorruptionInfo(filesystem_proxy.get(), path_base,
                                             bucket_locator)
                  .empty());
  EXPECT_FALSE(PathExists(info_path));

  // Random string.
  ASSERT_TRUE(base::WriteFile(info_path, "foo bar"));
  EXPECT_TRUE(indexed_db::ReadCorruptionInfo(filesystem_proxy.get(), path_base,
                                             bucket_locator)
                  .empty());
  EXPECT_FALSE(PathExists(info_path));

  // Not a dictionary.
  ASSERT_TRUE(base::WriteFile(info_path, "[]"));
  EXPECT_TRUE(indexed_db::ReadCorruptionInfo(filesystem_proxy.get(), path_base,
                                             bucket_locator)
                  .empty());
  EXPECT_FALSE(PathExists(info_path));

  // Empty dictionary.
  ASSERT_TRUE(base::WriteFile(info_path, "{}"));
  EXPECT_TRUE(indexed_db::ReadCorruptionInfo(filesystem_proxy.get(), path_base,
                                             bucket_locator)
                  .empty());
  EXPECT_FALSE(PathExists(info_path));

  // Dictionary, no message key.
  ASSERT_TRUE(base::WriteFile(info_path, "{\"foo\":\"bar\"}"));
  EXPECT_TRUE(indexed_db::ReadCorruptionInfo(filesystem_proxy.get(), path_base,
                                             bucket_locator)
                  .empty());
  EXPECT_FALSE(PathExists(info_path));

  // Dictionary, message key.
  ASSERT_TRUE(base::WriteFile(info_path, "{\"message\":\"bar\"}"));
  std::string message = indexed_db::ReadCorruptionInfo(
      filesystem_proxy.get(), path_base, bucket_locator);
  EXPECT_FALSE(message.empty());
  EXPECT_FALSE(PathExists(info_path));
  EXPECT_EQ("bar", message);

  // Dictionary, message key and more.
  ASSERT_TRUE(base::WriteFile(info_path, "{\"message\":\"foo\",\"bar\":5}"));
  message = indexed_db::ReadCorruptionInfo(filesystem_proxy.get(), path_base,
                                           bucket_locator);
  EXPECT_FALSE(message.empty());
  EXPECT_FALSE(PathExists(info_path));
  EXPECT_EQ("foo", message);
}

TEST_P(IndexedDBBackingStoreTestForThirdPartyStoragePartitioning,
       ReadCorruptionInfoForThirdPartyStorageKey) {
  auto filesystem_proxy = std::make_unique<storage::FilesystemProxy>(
      storage::FilesystemProxy::UNRESTRICTED, base::FilePath());
  storage::BucketLocator bucket_locator;
  bucket_locator.storage_key = blink::StorageKey::Create(
      url::Origin::Create(GURL("http://www.google.com/")),
      net::SchemefulSite(GURL("http://www.youtube.com/")),
      blink::mojom::AncestorChainBit::kCrossSite);
  bucket_locator.id = storage::BucketId::FromUnsafeValue(1);
  bucket_locator.is_default = true;
  const base::FilePath path_base = idb_context_->GetDataPath(bucket_locator);
  ASSERT_FALSE(path_base.empty());

  // File not found.
  EXPECT_TRUE(indexed_db::ReadCorruptionInfo(filesystem_proxy.get(), path_base,
                                             bucket_locator)
                  .empty());

  base::FilePath info_path =
      path_base.AppendASCII("http_www.google.com_0.indexeddb.leveldb")
          .AppendASCII("corruption_info.json");
  if (IsThirdPartyStoragePartitioningEnabled()) {
    info_path = path_base.AppendASCII("indexeddb.leveldb")
                    .AppendASCII("corruption_info.json");
  }
  ASSERT_TRUE(CreateDirectory(info_path.DirName()));

  // Empty file.
  std::string dummy_data;
  ASSERT_TRUE(base::WriteFile(info_path, dummy_data));
  EXPECT_TRUE(indexed_db::ReadCorruptionInfo(filesystem_proxy.get(), path_base,
                                             bucket_locator)
                  .empty());
  EXPECT_FALSE(PathExists(info_path));

  // File size > 4 KB.
  dummy_data.resize(5000, 'c');
  ASSERT_TRUE(base::WriteFile(info_path, dummy_data));
  EXPECT_TRUE(indexed_db::ReadCorruptionInfo(filesystem_proxy.get(), path_base,
                                             bucket_locator)
                  .empty());
  EXPECT_FALSE(PathExists(info_path));

  // Random string.
  ASSERT_TRUE(base::WriteFile(info_path, "foo bar"));
  EXPECT_TRUE(indexed_db::ReadCorruptionInfo(filesystem_proxy.get(), path_base,
                                             bucket_locator)
                  .empty());
  EXPECT_FALSE(PathExists(info_path));

  // Not a dictionary.
  ASSERT_TRUE(base::WriteFile(info_path, "[]"));
  EXPECT_TRUE(indexed_db::ReadCorruptionInfo(filesystem_proxy.get(), path_base,
                                             bucket_locator)
                  .empty());
  EXPECT_FALSE(PathExists(info_path));

  // Empty dictionary.
  ASSERT_TRUE(base::WriteFile(info_path, "{}"));
  EXPECT_TRUE(indexed_db::ReadCorruptionInfo(filesystem_proxy.get(), path_base,
                                             bucket_locator)
                  .empty());
  EXPECT_FALSE(PathExists(info_path));

  // Dictionary, no message key.
  ASSERT_TRUE(base::WriteFile(info_path, "{\"foo\":\"bar\"}"));
  EXPECT_TRUE(indexed_db::ReadCorruptionInfo(filesystem_proxy.get(), path_base,
                                             bucket_locator)
                  .empty());
  EXPECT_FALSE(PathExists(info_path));

  // Dictionary, message key.
  ASSERT_TRUE(base::WriteFile(info_path, "{\"message\":\"bar\"}"));
  std::string message = indexed_db::ReadCorruptionInfo(
      filesystem_proxy.get(), path_base, bucket_locator);
  EXPECT_FALSE(message.empty());
  EXPECT_FALSE(PathExists(info_path));
  EXPECT_EQ("bar", message);

  // Dictionary, message key and more.
  ASSERT_TRUE(base::WriteFile(info_path, "{\"message\":\"foo\",\"bar\":5}"));
  message = indexed_db::ReadCorruptionInfo(filesystem_proxy.get(), path_base,
                                           bucket_locator);
  EXPECT_FALSE(message.empty());
  EXPECT_FALSE(PathExists(info_path));
  EXPECT_EQ("foo", message);
}

// There was a wrong migration from schema 2 to 3, which always delete IDB
// blobs and doesn't actually write the new schema version. This tests the
// upgrade path where the database doesn't have blob entries, so it' safe to
// keep the database.
// https://crbug.com/756447, https://crbug.com/829125, https://crbug.com/829141
TEST_F(IndexedDBBackingStoreTest, SchemaUpgradeWithoutBlobsSurvives) {
  int64_t database_id;
  const int64_t object_store_id = 99;

  // The database metadata needs to be written so we can verify the blob entry
  // keys are not detected.
  const std::u16string database_name(u"db1");
  const int64_t version = 9;

  const std::u16string object_store_name(u"object_store1");
  const bool auto_increment = true;
  const IndexedDBKeyPath object_store_key_path(u"object_store_key");

  {
    IndexedDBDatabaseMetadata database;
    database.name = database_name;
    database.version = version;
    leveldb::Status s = backing_store()->CreateDatabase(database);
    EXPECT_TRUE(s.ok());
    EXPECT_GT(database.id, 0);
    database_id = database.id;

    IndexedDBBackingStore::Transaction transaction(
        backing_store()->AsWeakPtr(),
        blink::mojom::IDBTransactionDurability::Relaxed,
        blink::mojom::IDBTransactionMode::ReadWrite);
    transaction.Begin(CreateDummyLock());

    IndexedDBObjectStoreMetadata object_store;
    s = backing_store()->CreateObjectStore(
        &transaction, database.id, object_store_id, object_store_name,
        object_store_key_path, auto_increment, &object_store);
    EXPECT_TRUE(s.ok());

    bool succeeded = false;
    EXPECT_TRUE(
        transaction.CommitPhaseOne(CreateBlobWriteCallback(&succeeded)).ok());
    EXPECT_TRUE(succeeded);
    EXPECT_TRUE(transaction.CommitPhaseTwo().ok());
  }
  task_environment_.RunUntilIdle();

  // Save a value.
  IndexedDBBackingStore::Transaction transaction1(
      backing_store()->AsWeakPtr(),
      blink::mojom::IDBTransactionDurability::Relaxed,
      blink::mojom::IDBTransactionMode::ReadWrite);
  transaction1.Begin(CreateDummyLock());
  IndexedDBBackingStore::RecordIdentifier record;
  leveldb::Status s = backing_store()->PutRecord(
      &transaction1, database_id, object_store_id, key1_, &value1_, &record);
  EXPECT_TRUE(s.ok());
  bool succeeded = false;
  EXPECT_TRUE(
      transaction1.CommitPhaseOne(CreateBlobWriteCallback(&succeeded)).ok());
  EXPECT_TRUE(succeeded);
  EXPECT_TRUE(transaction1.CommitPhaseTwo().ok());

  // Set the schema to 2, which was before blob support.
  std::unique_ptr<LevelDBWriteBatch> write_batch = LevelDBWriteBatch::Create();
  const std::string schema_version_key = SchemaVersionKey::Encode();
  std::ignore = indexed_db::PutInt(write_batch.get(), schema_version_key, 2);
  ASSERT_TRUE(backing_store()->db()->Write(write_batch.get()).ok());
  task_environment_.RunUntilIdle();

  DestroyFactoryAndBackingStore();
  CreateFactoryAndBackingStore();

  IndexedDBBackingStore::Transaction transaction2(
      backing_store()->AsWeakPtr(),
      blink::mojom::IDBTransactionDurability::Relaxed,
      blink::mojom::IDBTransactionMode::ReadWrite);
  transaction2.Begin(CreateDummyLock());
  IndexedDBValue result_value;
  EXPECT_TRUE(backing_store()
                  ->GetRecord(&transaction2, database_id, object_store_id,
                              key1_, &result_value)
                  .ok());
  succeeded = false;
  EXPECT_TRUE(
      transaction2.CommitPhaseOne(CreateBlobWriteCallback(&succeeded)).ok());
  EXPECT_TRUE(succeeded);
  EXPECT_TRUE(transaction2.CommitPhaseTwo().ok());
  EXPECT_EQ(value1_.bits, result_value.bits);

  // Test that we upgraded.
  int64_t found_int = 0;
  bool found = false;
  bool success = indexed_db::GetInt(backing_store()->db(), schema_version_key,
                                    &found_int, &found)
                     .ok();
  ASSERT_TRUE(success);

  EXPECT_TRUE(found);
  EXPECT_EQ(indexed_db::kLatestKnownSchemaVersion, found_int);
  task_environment_.RunUntilIdle();
}

// Our v2->v3 schema migration code forgot to bump the on-disk version number.
// This test covers migrating a v3 database mislabeled as v2 to a properly
// labeled v3 database. When the mislabeled database has blob entries, we must
// treat it as corrupt and delete it.
// https://crbug.com/756447, https://crbug.com/829125, https://crbug.com/829141
TEST_F(IndexedDBBackingStoreTestWithBlobs, SchemaUpgradeWithBlobsCorrupt) {
  int64_t database_id;
  const int64_t object_store_id = 99;

  // The database metadata needs to be written so the blob entry keys can
  // be detected.
  const std::u16string database_name(u"db1");
  const int64_t version = 9;

  const std::u16string object_store_name(u"object_store1");
  const bool auto_increment = true;
  const IndexedDBKeyPath object_store_key_path(u"object_store_key");

  {
    IndexedDBDatabaseMetadata database;
    database.name = database_name;
    database.version = version;
    leveldb::Status s = backing_store()->CreateDatabase(database);
    EXPECT_TRUE(s.ok());
    EXPECT_GT(database.id, 0);
    database_id = database.id;

    IndexedDBBackingStore::Transaction transaction(
        backing_store()->AsWeakPtr(),
        blink::mojom::IDBTransactionDurability::Relaxed,
        blink::mojom::IDBTransactionMode::ReadWrite);
    transaction.Begin(CreateDummyLock());

    IndexedDBObjectStoreMetadata object_store;
    s = backing_store()->CreateObjectStore(
        &transaction, database.id, object_store_id, object_store_name,
        object_store_key_path, auto_increment, &object_store);
    EXPECT_TRUE(s.ok());

    bool succeeded = false;
    EXPECT_TRUE(
        transaction.CommitPhaseOne(CreateBlobWriteCallback(&succeeded)).ok());
    EXPECT_TRUE(succeeded);
    EXPECT_TRUE(transaction.CommitPhaseTwo().ok());
  }
  task_environment_.RunUntilIdle();

  base::RunLoop write_blobs_loop;
  // Initiate transaction1 - writing blobs.
  std::unique_ptr<IndexedDBBackingStore::Transaction> transaction1 =
      std::make_unique<IndexedDBBackingStore::Transaction>(
          backing_store()->AsWeakPtr(),
          blink::mojom::IDBTransactionDurability::Relaxed,
          blink::mojom::IDBTransactionMode::ReadWrite);
  transaction1->Begin(CreateDummyLock());
  IndexedDBBackingStore::RecordIdentifier record;
  EXPECT_TRUE(backing_store()
                  ->PutRecord(transaction1.get(), database_id, object_store_id,
                              key3_, &value3_, &record)
                  .ok());
  bool succeeded = false;
  EXPECT_TRUE(transaction1
                  ->CommitPhaseOne(CreateBlobWriteCallback(
                      &succeeded, write_blobs_loop.QuitClosure()))
                  .ok());
  task_environment_.RunUntilIdle();
  write_blobs_loop.Run();

  // Finish up transaction1, verifying blob writes.
  EXPECT_TRUE(succeeded);
  EXPECT_TRUE(CheckBlobWrites());
  EXPECT_TRUE(transaction1->CommitPhaseTwo().ok());

  // Set the schema to 2, which was before blob support.
  std::unique_ptr<LevelDBWriteBatch> write_batch = LevelDBWriteBatch::Create();
  const std::string schema_version_key = SchemaVersionKey::Encode();
  std::ignore = indexed_db::PutInt(write_batch.get(), schema_version_key, 2);
  ASSERT_TRUE(backing_store()->db()->Write(write_batch.get()).ok());

  // Clean up on the IDB sequence.
  transaction1.reset();
  task_environment_.RunUntilIdle();

  DestroyFactoryAndBackingStore();
  CreateFactoryAndBackingStore();

  // The factory returns a null backing store pointer when there is a corrupt
  // database.
  EXPECT_TRUE(data_loss_info_.status == blink::mojom::IDBDataLoss::Total);
}

namespace {

// v3 Blob Data is encoded as a series of:
//   { is_file [bool], blob_number [int64_t as varInt],
//     type [string-with-length, may be empty],
//     (for Blobs only) size [int64_t as varInt]
//     (for Files only) fileName [string-with-length]
//   }
// There is no length field; just read until you run out of data.
std::string EncodeV3BlobInfos(
    const std::vector<IndexedDBExternalObject>& blob_info) {
  std::string ret;
  for (const auto& info : blob_info) {
    DCHECK(info.object_type() == IndexedDBExternalObject::ObjectType::kFile ||
           info.object_type() == IndexedDBExternalObject::ObjectType::kBlob);
    bool is_file =
        info.object_type() == IndexedDBExternalObject::ObjectType::kFile;
    EncodeBool(is_file, &ret);
    EncodeVarInt(info.blob_number(), &ret);
    EncodeStringWithLength(info.type(), &ret);
    if (is_file)
      EncodeStringWithLength(info.file_name(), &ret);
    else
      EncodeVarInt(info.size(), &ret);
  }
  return ret;
}

}  // namespace

TEST_F(IndexedDBBackingStoreTestWithBlobs, SchemaUpgradeV3ToV4) {
  int64_t database_id;
  const int64_t object_store_id = 99;

  const std::u16string database_name(u"db1");
  const int64_t version = 9;

  const std::u16string object_store_name(u"object_store1");
  const bool auto_increment = true;
  const IndexedDBKeyPath object_store_key_path(u"object_store_key");

  {
    IndexedDBDatabaseMetadata database;
    database.name = database_name;
    database.version = version;
    leveldb::Status s = backing_store()->CreateDatabase(database);
    EXPECT_TRUE(s.ok());
    EXPECT_GT(database.id, 0);
    database_id = database.id;

    IndexedDBBackingStore::Transaction transaction(
        backing_store()->AsWeakPtr(),
        blink::mojom::IDBTransactionDurability::Relaxed,
        blink::mojom::IDBTransactionMode::ReadWrite);
    transaction.Begin(CreateDummyLock());

    IndexedDBObjectStoreMetadata object_store;
    s = backing_store()->CreateObjectStore(
        &transaction, database.id, object_store_id, object_store_name,
        object_store_key_path, auto_increment, &object_store);
    EXPECT_TRUE(s.ok());

    bool succeeded = false;
    EXPECT_TRUE(
        transaction.CommitPhaseOne(CreateBlobWriteCallback(&succeeded)).ok());
    EXPECT_TRUE(succeeded);
    EXPECT_TRUE(transaction.CommitPhaseTwo().ok());
  }
  task_environment_.RunUntilIdle();

  // Initiate transaction1 - writing blobs.
  std::unique_ptr<IndexedDBBackingStore::Transaction> transaction1 =
      std::make_unique<IndexedDBBackingStore::Transaction>(
          backing_store()->AsWeakPtr(),
          blink::mojom::IDBTransactionDurability::Relaxed,
          blink::mojom::IDBTransactionMode::ReadWrite);
  transaction1->Begin(CreateDummyLock());
  IndexedDBBackingStore::RecordIdentifier record;
  EXPECT_TRUE(backing_store()
                  ->PutRecord(transaction1.get(), database_id, object_store_id,
                              key3_, &value3_, &record)
                  .ok());
  bool succeeded = false;
  base::RunLoop write_blobs_loop;
  EXPECT_TRUE(transaction1
                  ->CommitPhaseOne(CreateBlobWriteCallback(
                      &succeeded, write_blobs_loop.QuitClosure()))
                  .ok());
  write_blobs_loop.Run();
  task_environment_.RunUntilIdle();

  // Finish up transaction1, verifying blob writes.
  EXPECT_TRUE(succeeded);
  EXPECT_TRUE(CheckBlobWrites());
  ASSERT_TRUE(transaction1->CommitPhaseTwo().ok());
  transaction1.reset();

  task_environment_.RunUntilIdle();

  // Change entries to be v3, and change the schema to be v3.
  std::unique_ptr<LevelDBWriteBatch> write_batch = LevelDBWriteBatch::Create();
  const std::string schema_version_key = SchemaVersionKey::Encode();
  ASSERT_TRUE(
      indexed_db::PutInt(write_batch.get(), schema_version_key, 3).ok());
  const std::string object_store_data_key =
      ObjectStoreDataKey::Encode(database_id, object_store_id, key3_);
  base::StringPiece leveldb_key_piece(object_store_data_key);
  BlobEntryKey blob_entry_key;
  ASSERT_TRUE(BlobEntryKey::FromObjectStoreDataKey(&leveldb_key_piece,
                                                   &blob_entry_key));
  ASSERT_EQ(blob_context_->writes().size(), 3u);
  auto& writes = blob_context_->writes();
  external_objects()[0].set_blob_number(writes[0].GetBlobNumber());
  external_objects()[1].set_blob_number(writes[1].GetBlobNumber());
  external_objects()[2].set_blob_number(writes[2].GetBlobNumber());
  std::string v3_blob_data = EncodeV3BlobInfos(external_objects());
  write_batch->Put(base::StringPiece(blob_entry_key.Encode()),
                   base::StringPiece(v3_blob_data));
  ASSERT_TRUE(backing_store()->db()->Write(write_batch.get()).ok());

  // The migration code uses the physical files on disk, so those need to be
  // written with the correct size & timestamp.
  base::FilePath file1_path = writes[1].path;
  base::FilePath file2_path = writes[2].path;
  ASSERT_TRUE(CreateDirectory(file1_path.DirName()));
  ASSERT_TRUE(CreateDirectory(file2_path.DirName()));
  base::File file1(file1_path,
                   base::File::FLAG_WRITE | base::File::FLAG_CREATE_ALWAYS);
  ASSERT_TRUE(file1.IsValid());
  ASSERT_TRUE(file1.WriteAtCurrentPosAndCheck(
      base::as_bytes(base::make_span(kBlobFileData1))));
  ASSERT_TRUE(file1.SetTimes(external_objects()[1].last_modified(),
                             external_objects()[1].last_modified()));
  file1.Close();

  base::File file2(file2_path,
                   base::File::FLAG_WRITE | base::File::FLAG_CREATE_ALWAYS);
  ASSERT_TRUE(file2.IsValid());
  ASSERT_TRUE(file2.WriteAtCurrentPosAndCheck(
      base::as_bytes(base::make_span(kBlobFileData2))));
  ASSERT_TRUE(file2.SetTimes(external_objects()[2].last_modified(),
                             external_objects()[2].last_modified()));
  file2.Close();

  DestroyFactoryAndBackingStore();
  CreateFactoryAndBackingStore();

  // There should be no corruption.
  ASSERT_TRUE(data_loss_info_.status == blink::mojom::IDBDataLoss::None);

  // Initiate transaction2, reading blobs.
  IndexedDBBackingStore::Transaction transaction2(
      backing_store()->AsWeakPtr(),
      blink::mojom::IDBTransactionDurability::Relaxed,
      blink::mojom::IDBTransactionMode::ReadWrite);
  transaction2.Begin(CreateDummyLock());
  IndexedDBValue result_value;
  EXPECT_TRUE(backing_store()
                  ->GetRecord(&transaction2, database_id, object_store_id,
                              key3_, &result_value)
                  .ok());

  // Finish up transaction2, verifying blob reads.
  succeeded = false;
  EXPECT_TRUE(
      transaction2.CommitPhaseOne(CreateBlobWriteCallback(&succeeded)).ok());
  EXPECT_TRUE(succeeded);
  EXPECT_TRUE(transaction2.CommitPhaseTwo().ok());
  EXPECT_EQ(value3_.bits, result_value.bits);
  EXPECT_TRUE(CheckBlobInfoMatches(result_value.external_objects));
}

TEST_F(IndexedDBBackingStoreTestWithBlobs, SchemaUpgradeV4ToV5) {
  int64_t database_id;
  const int64_t object_store_id = 99;

  const std::u16string database_name(u"db1");
  const int64_t version = 9;

  const std::u16string object_store_name(u"object_store1");
  const bool auto_increment = true;
  const IndexedDBKeyPath object_store_key_path(u"object_store_key");

  // Add an empty blob here to test with.  Empty blobs are not written
  // to disk, so it's important to verify that a database with empty blobs
  // should be considered still valid.
  external_objects().push_back(
      CreateBlobInfo(u"empty blob", u"file type", base::Time::Now(), 0u));
  // The V5 migration checks files on disk, so make sure our fake blob
  // context writes something there to check.
  blob_context_->SetWriteFilesToDisk(true);

  {
    IndexedDBDatabaseMetadata database;
    database.name = database_name;
    database.version = version;
    leveldb::Status s = backing_store()->CreateDatabase(database);
    EXPECT_TRUE(s.ok());
    EXPECT_GT(database.id, 0);
    database_id = database.id;

    IndexedDBBackingStore::Transaction transaction(
        backing_store()->AsWeakPtr(),
        blink::mojom::IDBTransactionDurability::Relaxed,
        blink::mojom::IDBTransactionMode::ReadWrite);
    transaction.Begin(CreateDummyLock());

    IndexedDBObjectStoreMetadata object_store;
    s = backing_store()->CreateObjectStore(
        &transaction, database.id, object_store_id, object_store_name,
        object_store_key_path, auto_increment, &object_store);
    EXPECT_TRUE(s.ok());

    bool succeeded = false;
    EXPECT_TRUE(
        transaction.CommitPhaseOne(CreateBlobWriteCallback(&succeeded)).ok());
    EXPECT_TRUE(succeeded);
    EXPECT_TRUE(transaction.CommitPhaseTwo().ok());
  }
  task_environment_.RunUntilIdle();

  // Initiate transaction - writing blobs.
  std::unique_ptr<IndexedDBBackingStore::Transaction> transaction =
      std::make_unique<IndexedDBBackingStore::Transaction>(
          backing_store()->AsWeakPtr(),
          blink::mojom::IDBTransactionDurability::Relaxed,
          blink::mojom::IDBTransactionMode::ReadWrite);
  transaction->Begin(CreateDummyLock());
  IndexedDBBackingStore::RecordIdentifier record;

  IndexedDBKey key = IndexedDBKey(u"key");
  IndexedDBValue value = IndexedDBValue("value3", external_objects());

  EXPECT_TRUE(backing_store()
                  ->PutRecord(transaction.get(), database_id, object_store_id,
                              key, &value, &record)
                  .ok());
  bool succeeded = false;
  base::RunLoop write_blobs_loop;
  EXPECT_TRUE(transaction
                  ->CommitPhaseOne(CreateBlobWriteCallback(
                      &succeeded, write_blobs_loop.QuitClosure()))
                  .ok());
  write_blobs_loop.Run();
  task_environment_.RunUntilIdle();

  // Finish up transaction, verifying blob writes.
  EXPECT_TRUE(succeeded);
  EXPECT_TRUE(CheckBlobWrites());
  ASSERT_TRUE(transaction->CommitPhaseTwo().ok());
  transaction.reset();

  task_environment_.RunUntilIdle();
  ASSERT_EQ(blob_context_->writes().size(), 3u);

  // Verify V4 to V5 conversion with all blobs intact has no data loss.
  {
    // Change the schema to be v4.
    const int64_t old_version = 4;
    std::unique_ptr<LevelDBWriteBatch> write_batch =
        LevelDBWriteBatch::Create();
    const std::string schema_version_key = SchemaVersionKey::Encode();
    ASSERT_TRUE(
        indexed_db::PutInt(write_batch.get(), schema_version_key, old_version)
            .ok());
    ASSERT_TRUE(backing_store()->db()->Write(write_batch.get()).ok());

    DestroyFactoryAndBackingStore();
    CreateFactoryAndBackingStore();

    // There should be no corruption here.
    ASSERT_EQ(data_loss_info_.status, blink::mojom::IDBDataLoss::None);
  }

  // Verify V4 to V5 conversion with missing blobs has data loss.
  {
    // Change the schema to be v4.
    const int64_t old_version = 4;
    std::unique_ptr<LevelDBWriteBatch> write_batch =
        LevelDBWriteBatch::Create();
    const std::string schema_version_key = SchemaVersionKey::Encode();
    ASSERT_TRUE(
        indexed_db::PutInt(write_batch.get(), schema_version_key, old_version)
            .ok());
    ASSERT_TRUE(backing_store()->db()->Write(write_batch.get()).ok());

    // Pick a blob we wrote arbitrarily and delete it.
    auto path = blob_context_->writes()[1].path;
    auto filesystem_proxy = std::make_unique<storage::FilesystemProxy>(
        storage::FilesystemProxy::UNRESTRICTED, base::FilePath());
    filesystem_proxy->DeleteFile(path);

    DestroyFactoryAndBackingStore();
    CreateFactoryAndBackingStore();

    // This should be corrupted.
    ASSERT_NE(data_loss_info_.status, blink::mojom::IDBDataLoss::None);
    DestroyFactoryAndBackingStore();
  }
}

// This tests that external objects are deleted when ClearObjectStore is called.
// See: http://crbug.com/488851
// TODO(enne): we could use more comprehensive testing for ClearObjectStore.
TEST_P(IndexedDBBackingStoreTestWithExternalObjects, ClearObjectStoreObjects) {
  const std::vector<IndexedDBKey> keys = {
      IndexedDBKey(u"key0"), IndexedDBKey(u"key1"), IndexedDBKey(u"key2"),
      IndexedDBKey(u"key3")};

  const int64_t database_id = 777;
  const int64_t object_store_id = 999;

  // Create two object stores, to verify that only one gets deleted.
  for (size_t i = 0; i < 2; ++i) {
    const int64_t write_object_store_id = object_store_id + i;

    std::vector<IndexedDBExternalObject> external_objects;
    for (size_t j = 0; j < 4; ++j) {
      std::string type = "type " + base::NumberToString(j);
      external_objects.push_back(CreateBlobInfo(base::UTF8ToUTF16(type), 1));
    }

    std::vector<IndexedDBValue> values = {
        IndexedDBValue("value0", {external_objects[0]}),
        IndexedDBValue("value1", {external_objects[1]}),
        IndexedDBValue("value2", {external_objects[2]}),
        IndexedDBValue("value3", {external_objects[3]}),
    };
    ASSERT_GE(keys.size(), values.size());

    // Initiate transaction1 - write records.
    std::unique_ptr<IndexedDBBackingStore::Transaction> transaction1 =
        std::make_unique<IndexedDBBackingStore::Transaction>(
            backing_store()->AsWeakPtr(),
            blink::mojom::IDBTransactionDurability::Relaxed,
            blink::mojom::IDBTransactionMode::ReadWrite);
    transaction1->Begin(CreateDummyLock());
    IndexedDBBackingStore::RecordIdentifier record;
    for (size_t j = 0; j < values.size(); ++j) {
      EXPECT_TRUE(backing_store()
                      ->PutRecord(transaction1.get(), database_id,
                                  write_object_store_id, keys[j], &values[j],
                                  &record)
                      .ok());
    }

    // Start committing transaction1.
    bool succeeded = false;
    EXPECT_TRUE(
        transaction1->CommitPhaseOne(CreateBlobWriteCallback(&succeeded)).ok());
    task_environment_.RunUntilIdle();

    // Finish committing transaction1.

    EXPECT_TRUE(succeeded);
    EXPECT_TRUE(transaction1->CommitPhaseTwo().ok());
  }

  // Initiate transaction 2 - delete object store
  std::unique_ptr<IndexedDBBackingStore::Transaction> transaction2 =
      std::make_unique<IndexedDBBackingStore::Transaction>(
          backing_store()->AsWeakPtr(),
          blink::mojom::IDBTransactionDurability::Relaxed,
          blink::mojom::IDBTransactionMode::ReadWrite);
  transaction2->Begin(CreateDummyLock());
  IndexedDBValue result_value;
  EXPECT_TRUE(
      backing_store()
          ->ClearObjectStore(transaction2.get(), database_id, object_store_id)
          .ok());

  // Start committing transaction2.
  bool succeeded = false;
  EXPECT_TRUE(
      transaction2->CommitPhaseOne(CreateBlobWriteCallback(&succeeded)).ok());
  task_environment_.RunUntilIdle();

  // Finish committing transaction2.

  EXPECT_TRUE(succeeded);
  EXPECT_TRUE(transaction2->CommitPhaseTwo().ok());

  // Verify blob removals.
  ASSERT_EQ(4UL, backing_store()->removals().size());
  EXPECT_EQ(blob_context_->writes()[0].path, backing_store()->removals()[0]);
  EXPECT_EQ(blob_context_->writes()[1].path, backing_store()->removals()[1]);
  EXPECT_EQ(blob_context_->writes()[2].path, backing_store()->removals()[2]);
  EXPECT_EQ(blob_context_->writes()[3].path, backing_store()->removals()[3]);

  // Clean up on the IDB sequence.
  transaction2.reset();
  task_environment_.RunUntilIdle();
}

}  // namespace indexed_db_backing_store_unittest
}  // namespace content
