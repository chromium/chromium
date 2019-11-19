// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_backing_store.h"

#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/time/default_clock.h"
#include "components/services/storage/indexed_db/scopes/disjoint_range_lock_manager.h"
#include "components/services/storage/indexed_db/transactional_leveldb/leveldb_write_batch.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_database.h"
#include "content/browser/indexed_db/indexed_db_class_factory.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_factory_impl.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "content/browser/indexed_db/indexed_db_leveldb_operations.h"
#include "content/browser/indexed_db/indexed_db_metadata_coding.h"
#include "content/browser/indexed_db/indexed_db_origin_state.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/indexeddb/web_idb_types.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

using base::ASCIIToUTF16;
using blink::IndexedDBDatabaseMetadata;
using blink::IndexedDBIndexMetadata;
using blink::IndexedDBKey;
using blink::IndexedDBKeyPath;
using blink::IndexedDBKeyRange;
using blink::IndexedDBObjectStoreMetadata;
using url::Origin;

namespace content {
namespace indexed_db_backing_store_unittest {

// Write |content| to |file|. Returns true on success.
bool WriteFile(const base::FilePath& file, base::StringPiece content) {
  int write_size = base::WriteFile(file, content.data(), content.length());
  return write_size >= 0 && write_size == static_cast<int>(content.length());
}

class TestableIndexedDBBackingStore : public IndexedDBBackingStore {
 public:
  TestableIndexedDBBackingStore(
      IndexedDBBackingStore::Mode backing_store_mode,
      TransactionalLevelDBFactory* leveldb_factory,
      const url::Origin& origin,
      const base::FilePath& blob_path,
      std::unique_ptr<TransactionalLevelDBDatabase> db,
      BlobFilesCleanedCallback blob_files_cleaned,
      ReportOutstandingBlobsCallback report_outstanding_blobs,
      base::SequencedTaskRunner* task_runner)
      : IndexedDBBackingStore(backing_store_mode,
                              leveldb_factory,
                              origin,
                              blob_path,
                              std::move(db),
                              std::move(blob_files_cleaned),
                              std::move(report_outstanding_blobs),
                              task_runner),
        database_id_(0) {}
  ~TestableIndexedDBBackingStore() override = default;

  const std::vector<IndexedDBBackingStore::Transaction::WriteDescriptor>&
  writes() const {
    return writes_;
  }
  void ClearWrites() { writes_.clear(); }
  const std::vector<int64_t>& removals() const { return removals_; }
  void ClearRemovals() { removals_.clear(); }

  void StartJournalCleaningTimer() override {
    IndexedDBBackingStore::StartJournalCleaningTimer();
  }

 protected:
  bool WriteBlobFile(
      int64_t database_id,
      const Transaction::WriteDescriptor& descriptor,
      Transaction::ChainedBlobWriter* chained_blob_writer) override {
    if (KeyPrefix::IsValidDatabaseId(database_id_)) {
      if (database_id_ != database_id) {
        return false;
      }
    } else {
      database_id_ = database_id;
    }
    writes_.push_back(descriptor);
    task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&Transaction::ChainedBlobWriter::ReportWriteCompletion,
                       chained_blob_writer, true, 1));
    return true;
  }

  bool RemoveBlobFile(int64_t database_id, int64_t key) const override {
    if (database_id_ != database_id ||
        !KeyPrefix::IsValidDatabaseId(database_id)) {
      return false;
    }
    removals_.push_back(key);
    return true;
  }

 private:
  int64_t database_id_;
  std::vector<Transaction::WriteDescriptor> writes_;

  // This is modified in an overridden virtual function that is properly const
  // in the real implementation, therefore must be mutable here.
  mutable std::vector<int64_t> removals_;

  DISALLOW_COPY_AND_ASSIGN(TestableIndexedDBBackingStore);
};

// Factory subclass to allow the test to use the
// TestableIndexedDBBackingStore subclass.
class TestIDBFactory : public IndexedDBFactoryImpl {
 public:
  explicit TestIDBFactory(IndexedDBContextImpl* idb_context)
      : IndexedDBFactoryImpl(idb_context,
                             IndexedDBClassFactory::Get(),
                             base::DefaultClock::GetInstance()) {}
  ~TestIDBFactory() override = default;

 protected:
  std::unique_ptr<IndexedDBBackingStore> CreateBackingStore(
      IndexedDBBackingStore::Mode backing_store_mode,
      TransactionalLevelDBFactory* leveldb_factory,
      const url::Origin& origin,
      const base::FilePath& blob_path,
      std::unique_ptr<TransactionalLevelDBDatabase> db,
      IndexedDBBackingStore::BlobFilesCleanedCallback blob_files_cleaned,
      IndexedDBBackingStore::ReportOutstandingBlobsCallback
          report_outstanding_blobs,
      base::SequencedTaskRunner* task_runner) override {
    return std::make_unique<TestableIndexedDBBackingStore>(
        backing_store_mode, leveldb_factory, origin, blob_path, std::move(db),
        std::move(blob_files_cleaned), std::move(report_outstanding_blobs),
        task_runner);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestIDBFactory);
};

class IndexedDBBackingStoreTest : public testing::Test {
 public:
  IndexedDBBackingStoreTest()
      : special_storage_policy_(
            base::MakeRefCounted<MockSpecialStoragePolicy>()),
        quota_manager_proxy_(
            base::MakeRefCounted<MockQuotaManagerProxy>(nullptr, nullptr)) {}

  void SetUp() override {
    special_storage_policy_->SetAllUnlimited(true);
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    idb_context_ = base::MakeRefCounted<IndexedDBContextImpl>(
        temp_dir_.GetPath(), special_storage_policy_, quota_manager_proxy_,
        base::DefaultClock::GetInstance(),
        base::SequencedTaskRunnerHandle::Get());

    CreateFactoryAndBackingStore();

    // useful keys and values during tests
    value1_ = IndexedDBValue("value1", std::vector<IndexedDBBlobInfo>());
    value2_ = IndexedDBValue("value2", std::vector<IndexedDBBlobInfo>());

    key1_ = IndexedDBKey(99, blink::mojom::IDBKeyType::Number);
    key2_ = IndexedDBKey(ASCIIToUTF16("key2"));
  }

  void CreateFactoryAndBackingStore() {
    const Origin origin = Origin::Create(GURL("http://localhost:81"));
    idb_factory_ = std::make_unique<TestIDBFactory>(idb_context_.get());

    leveldb::Status s;
    std::tie(origin_state_handle_, s, std::ignore, data_loss_info_,
             std::ignore) =
        idb_factory_->GetOrOpenOriginFactory(origin, idb_context_->data_path(),
                                             /*create_if_missing=*/true);
    if (!origin_state_handle_.IsHeld()) {
      backing_store_ = nullptr;
      return;
    }
    backing_store_ = static_cast<TestableIndexedDBBackingStore*>(
        origin_state_handle_.origin_state()->backing_store());
    lock_manager_ = origin_state_handle_.origin_state()->lock_manager();
  }

  std::vector<ScopeLock> CreateDummyLock() {
    base::RunLoop loop;
    ScopesLocksHolder locks_receiver;
    bool success = lock_manager_->AcquireLocks(
        {{0, {"01", "11"}, ScopesLockManager::LockType::kShared}},
        locks_receiver.AsWeakPtr(),
        base::BindLambdaForTesting([&loop]() { loop.Quit(); }));
    EXPECT_TRUE(success);
    if (success)
      loop.Run();
    return std::move(locks_receiver.locks);
  }

  void DestroyFactoryAndBackingStore() {
    origin_state_handle_.Release();
    idb_factory_.reset();
    backing_store_ = nullptr;
  }

  void TearDown() override {
    DestroyFactoryAndBackingStore();
    quota_manager_proxy_->SimulateQuotaManagerDestroyed();

    if (idb_context_ && !idb_context_->IsInMemoryContext()) {
      IndexedDBFactoryImpl* factory = idb_context_->GetIDBFactory();

      // Loop through all open origins, and force close them, and request the
      // deletion of the leveldb state. Once the states are no longer around,
      // delete all of the databases on disk.
      auto open_factory_origins = factory->GetOpenOrigins();

      for (auto origin : open_factory_origins) {
        base::RunLoop loop;
        IndexedDBOriginState* per_origin_factory =
            factory->GetOriginFactory(origin);
        per_origin_factory->backing_store()
            ->db()
            ->leveldb_state()
            ->RequestDestruction(loop.QuitClosure(),
                                 base::SequencedTaskRunnerHandle::Get());
        idb_context_->ForceClose(
            origin, IndexedDBContextImpl::FORCE_CLOSE_DELETE_ORIGIN);
        loop.Run();
      }
      // All leveldb databases are closed, and they can be deleted.
      for (auto origin : idb_context_->GetAllOrigins()) {
        idb_context_->DeleteForOrigin(origin);
      }
    }
    if (temp_dir_.IsValid())
      ASSERT_TRUE(temp_dir_.Delete());

    // Wait until the context has fully destroyed.
    scoped_refptr<base::SequencedTaskRunner> task_runner =
        idb_context_->TaskRunner();
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
    idb_context_->TaskRunner()->PostTask(FROM_HERE, cycle_loop.QuitClosure());
    cycle_loop.Run();
  }

 protected:
  BrowserTaskEnvironment task_environment_;

  base::ScopedTempDir temp_dir_;
  scoped_refptr<MockSpecialStoragePolicy> special_storage_policy_;
  scoped_refptr<MockQuotaManagerProxy> quota_manager_proxy_;
  scoped_refptr<IndexedDBContextImpl> idb_context_;
  std::unique_ptr<TestIDBFactory> idb_factory_;
  DisjointRangeLockManager* lock_manager_;

  IndexedDBOriginStateHandle origin_state_handle_;
  TestableIndexedDBBackingStore* backing_store_ = nullptr;
  IndexedDBDataLossInfo data_loss_info_;

  // Sample keys and values that are consistent.
  IndexedDBKey key1_;
  IndexedDBKey key2_;
  IndexedDBValue value1_;
  IndexedDBValue value2_;

 private:
  DISALLOW_COPY_AND_ASSIGN(IndexedDBBackingStoreTest);
};

class IndexedDBBackingStoreTestWithBlobs : public IndexedDBBackingStoreTest {
 public:
  IndexedDBBackingStoreTestWithBlobs() {}

  void SetUp() override {
    IndexedDBBackingStoreTest::SetUp();

    blob_context_ = std::make_unique<storage::BlobStorageContext>();

    // useful keys and values during tests
    blob_info_.push_back(
        IndexedDBBlobInfo(CreateBlob(), base::UTF8ToUTF16("blob type"), 1));
    blob_info_.push_back(IndexedDBBlobInfo(
        CreateBlob(), base::FilePath(FILE_PATH_LITERAL("path/to/file")),
        base::UTF8ToUTF16("file name"), base::UTF8ToUTF16("file type")));
    blob_info_.push_back(IndexedDBBlobInfo(CreateBlob(), base::FilePath(),
                                           base::UTF8ToUTF16("file name"),
                                           base::UTF8ToUTF16("file type")));
    value3_ = IndexedDBValue("value3", blob_info_);

    key3_ = IndexedDBKey(ASCIIToUTF16("key3"));
  }

  std::unique_ptr<storage::BlobDataHandle> CreateBlob() {
    return blob_context_->AddFinishedBlob(
        std::make_unique<storage::BlobDataBuilder>(base::GenerateGUID()));
  }

  // This just checks the data that survive getting stored and recalled, e.g.
  // the file path and UUID will change and thus aren't verified.
  bool CheckBlobInfoMatches(const std::vector<IndexedDBBlobInfo>& reads) const {
    DCHECK(idb_context_->TaskRunner()->RunsTasksInCurrentSequence());

    if (blob_info_.size() != reads.size())
      return false;
    for (size_t i = 0; i < blob_info_.size(); ++i) {
      const IndexedDBBlobInfo& a = blob_info_[i];
      const IndexedDBBlobInfo& b = reads[i];
      if (a.is_file() != b.is_file())
        return false;
      if (a.type() != b.type())
        return false;
      if (a.is_file()) {
        if (a.file_name() != b.file_name())
          return false;
      } else {
        if (a.size() != b.size())
          return false;
      }
    }
    return true;
  }

  bool CheckBlobReadsMatchWrites(
      const std::vector<IndexedDBBlobInfo>& reads) const {
    DCHECK(idb_context_->TaskRunner()->RunsTasksInCurrentSequence());

    if (backing_store_->writes().size() != reads.size())
      return false;
    std::set<int64_t> ids;
    for (const auto& write : backing_store_->writes())
      ids.insert(write.key());
    if (ids.size() != backing_store_->writes().size())
      return false;
    for (const auto& read : reads) {
      if (ids.count(read.key()) != 1)
        return false;
    }
    return true;
  }

  bool CheckBlobWrites() const {
    DCHECK(idb_context_->TaskRunner()->RunsTasksInCurrentSequence());

    if (backing_store_->writes().size() != blob_info_.size())
      return false;
    for (size_t i = 0; i < backing_store_->writes().size(); ++i) {
      const IndexedDBBackingStore::Transaction::WriteDescriptor& desc =
          backing_store_->writes()[i];
      const IndexedDBBlobInfo& info = blob_info_[i];
      if (desc.is_file() != info.is_file()) {
        if (!info.is_file() || !info.file_path().empty())
          return false;
      } else if (desc.is_file()) {
        if (desc.file_path() != info.file_path())
          return false;
      } else {
        if (desc.blob()->uuid() != info.blob_handle()->uuid())
          return false;
      }
    }
    return true;
  }

  bool CheckBlobRemovals() const {
    DCHECK(idb_context_->TaskRunner()->RunsTasksInCurrentSequence());

    if (backing_store_->removals().size() != backing_store_->writes().size())
      return false;
    for (size_t i = 0; i < backing_store_->writes().size(); ++i) {
      if (backing_store_->writes()[i].key() != backing_store_->removals()[i])
        return false;
    }
    return true;
  }

  // Sample keys and values that are consistent. Public so that posted lambdas
  // passed |this| can access them.
  IndexedDBKey key3_;
  IndexedDBValue value3_;

 private:
  std::unique_ptr<storage::BlobStorageContext> blob_context_;

  // Blob details referenced by |value3_|. The various CheckBlob*() methods
  // can be used to verify the state as a test progresses.
  std::vector<IndexedDBBlobInfo> blob_info_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBBackingStoreTestWithBlobs);
};

class TestCallback {
 public:
  TestCallback() = default;
  ~TestCallback() = default;

  IndexedDBBackingStore::BlobWriteCallback CreateCallback() {
    return base::BindLambdaForTesting(
        [this](IndexedDBBackingStore::BlobWriteResult result) {
          this->called = true;
          switch (result) {
            case IndexedDBBackingStore::BlobWriteResult::kFailure:
              // Not tested.
              this->succeeded = false;
              break;
            case IndexedDBBackingStore::BlobWriteResult::kRunPhaseTwoAsync:
            case IndexedDBBackingStore::BlobWriteResult::
                kRunPhaseTwoAndReturnResult:
              this->succeeded = true;
              break;
          }
          return leveldb::Status::OK();
        });
  }
  bool called = false;
  bool succeeded = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestCallback);
};

TEST_F(IndexedDBBackingStoreTest, PutGetConsistency) {
  base::RunLoop loop;
  idb_context_->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        const IndexedDBKey key = key1_;
        IndexedDBValue value = value1_;
        {
          IndexedDBBackingStore::Transaction transaction1(
              backing_store()->AsWeakPtr(),
              blink::mojom::IDBTransactionDurability::Relaxed);
          transaction1.Begin(CreateDummyLock());
          IndexedDBBackingStore::RecordIdentifier record;
          leveldb::Status s = backing_store()->PutRecord(&transaction1, 1, 1,
                                                         key, &value, &record);
          EXPECT_TRUE(s.ok());
          TestCallback callback_creator;
          EXPECT_TRUE(
              transaction1.CommitPhaseOne(callback_creator.CreateCallback())
                  .ok());
          EXPECT_TRUE(callback_creator.called);
          EXPECT_TRUE(callback_creator.succeeded);
          EXPECT_TRUE(transaction1.CommitPhaseTwo().ok());
        }

        {
          IndexedDBBackingStore::Transaction transaction2(
              backing_store()->AsWeakPtr(),
              blink::mojom::IDBTransactionDurability::Relaxed);
          transaction2.Begin(CreateDummyLock());
          IndexedDBValue result_value;
          EXPECT_TRUE(backing_store()
                          ->GetRecord(&transaction2, 1, 1, key, &result_value)
                          .ok());
          TestCallback callback_creator;
          EXPECT_TRUE(
              transaction2.CommitPhaseOne(callback_creator.CreateCallback())
                  .ok());
          EXPECT_TRUE(callback_creator.called);
          EXPECT_TRUE(callback_creator.succeeded);
          EXPECT_TRUE(transaction2.CommitPhaseTwo().ok());
          EXPECT_EQ(value.bits, result_value.bits);
        }
        loop.Quit();
      }));
  loop.Run();

  CycleIDBTaskRunner();
}

TEST_F(IndexedDBBackingStoreTestWithBlobs, PutGetConsistencyWithBlobs) {
  std::unique_ptr<IndexedDBBackingStore::Transaction> transaction1;
  TestCallback callback_creator1;
  std::unique_ptr<IndexedDBBackingStore::Transaction> transaction3;
  TestCallback callback_creator3;

  idb_context_->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        // Initiate transaction1 - writing blobs.
        transaction1 = std::make_unique<IndexedDBBackingStore::Transaction>(
            backing_store()->AsWeakPtr(),
            blink::mojom::IDBTransactionDurability::Relaxed);
        transaction1->Begin(CreateDummyLock());
        IndexedDBBackingStore::RecordIdentifier record;
        EXPECT_TRUE(
            backing_store()
                ->PutRecord(transaction1.get(), 1, 1, key3_, &value3_, &record)
                .ok());
        EXPECT_TRUE(
            transaction1->CommitPhaseOne(callback_creator1.CreateCallback())
                .ok());
      }));
  RunAllTasksUntilIdle();

  idb_context_->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        // Finish up transaction1, verifying blob writes.
        EXPECT_TRUE(callback_creator1.called);
        EXPECT_TRUE(callback_creator1.succeeded);
        EXPECT_TRUE(CheckBlobWrites());
        EXPECT_TRUE(transaction1->CommitPhaseTwo().ok());

        // Initiate transaction2, reading blobs.
        IndexedDBBackingStore::Transaction transaction2(
            backing_store()->AsWeakPtr(),
            blink::mojom::IDBTransactionDurability::Relaxed);
        transaction2.Begin(CreateDummyLock());
        IndexedDBValue result_value;
        EXPECT_TRUE(backing_store()
                        ->GetRecord(&transaction2, 1, 1, key3_, &result_value)
                        .ok());

        // Finish up transaction2, verifying blob reads.
        TestCallback callback_creator;
        EXPECT_TRUE(
            transaction2.CommitPhaseOne(callback_creator.CreateCallback())
                .ok());
        EXPECT_TRUE(callback_creator.called);
        EXPECT_TRUE(callback_creator.succeeded);
        EXPECT_TRUE(transaction2.CommitPhaseTwo().ok());
        EXPECT_EQ(value3_.bits, result_value.bits);
        EXPECT_TRUE(CheckBlobInfoMatches(result_value.blob_info));
        EXPECT_TRUE(CheckBlobReadsMatchWrites(result_value.blob_info));

        // Initiate transaction3, deleting blobs.
        transaction3 = std::make_unique<IndexedDBBackingStore::Transaction>(
            backing_store()->AsWeakPtr(),
            blink::mojom::IDBTransactionDurability::Relaxed);
        transaction3->Begin(CreateDummyLock());
        EXPECT_TRUE(backing_store()
                        ->DeleteRange(transaction3.get(), 1, 1,
                                      IndexedDBKeyRange(key3_))
                        .ok());
        EXPECT_TRUE(
            transaction3->CommitPhaseOne(callback_creator3.CreateCallback())
                .ok());
      }));
  RunAllTasksUntilIdle();

  idb_context_->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        // Finish up transaction 3, verifying blob deletes.
        EXPECT_TRUE(transaction3->CommitPhaseTwo().ok());
        EXPECT_TRUE(CheckBlobRemovals());

        // Clean up on the IDB sequence.
        transaction1.reset();
        transaction3.reset();
      }));
  RunAllTasksUntilIdle();
}

TEST_F(IndexedDBBackingStoreTestWithBlobs, DeleteRange) {
  const std::vector<IndexedDBKey> keys = {
      IndexedDBKey(ASCIIToUTF16("key0")), IndexedDBKey(ASCIIToUTF16("key1")),
      IndexedDBKey(ASCIIToUTF16("key2")), IndexedDBKey(ASCIIToUTF16("key3"))};
  const IndexedDBKeyRange ranges[] = {
      IndexedDBKeyRange(keys[1], keys[2], false, false),
      IndexedDBKeyRange(keys[1], keys[2], false, false),
      IndexedDBKeyRange(keys[0], keys[2], true, false),
      IndexedDBKeyRange(keys[1], keys[3], false, true),
      IndexedDBKeyRange(keys[0], keys[3], true, true)};

  for (size_t i = 0; i < base::size(ranges); ++i) {
    const int64_t database_id = 1;
    const int64_t object_store_id = i + 1;
    const IndexedDBKeyRange& range = ranges[i];

    std::unique_ptr<IndexedDBBackingStore::Transaction> transaction1;
    TestCallback callback_creator1;
    std::unique_ptr<IndexedDBBackingStore::Transaction> transaction2;
    TestCallback callback_creator2;
    std::vector<std::unique_ptr<storage::BlobDataHandle>> blobs;

    for (size_t j = 0; j < 4; ++j)
      blobs.push_back(CreateBlob());

    idb_context_->TaskRunner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          // Reset from previous iteration.
          backing_store()->ClearWrites();
          backing_store()->ClearRemovals();

          std::vector<IndexedDBValue> values = {
              IndexedDBValue(
                  "value0",
                  {IndexedDBBlobInfo(
                      std::make_unique<storage::BlobDataHandle>(*blobs[0]),
                      base::UTF8ToUTF16("type 0"), 1)}),
              IndexedDBValue(
                  "value1",
                  {IndexedDBBlobInfo(
                      std::make_unique<storage::BlobDataHandle>(*blobs[1]),
                      base::UTF8ToUTF16("type 1"), 1)}),
              IndexedDBValue(
                  "value2",
                  {IndexedDBBlobInfo(
                      std::make_unique<storage::BlobDataHandle>(*blobs[2]),
                      base::UTF8ToUTF16("type 2"), 1)}),
              IndexedDBValue(
                  "value3",
                  {IndexedDBBlobInfo(
                      std::make_unique<storage::BlobDataHandle>(*blobs[3]),
                      base::UTF8ToUTF16("type 3"), 1)})};
          ASSERT_GE(keys.size(), values.size());

          // Initiate transaction1 - write records.
          transaction1 = std::make_unique<IndexedDBBackingStore::Transaction>(
              backing_store()->AsWeakPtr(),
              blink::mojom::IDBTransactionDurability::Relaxed);
          transaction1->Begin(CreateDummyLock());
          IndexedDBBackingStore::RecordIdentifier record;
          for (size_t i = 0; i < values.size(); ++i) {
            EXPECT_TRUE(backing_store()
                            ->PutRecord(transaction1.get(), database_id,
                                        object_store_id, keys[i], &values[i],
                                        &record)
                            .ok());
          }

          // Start committing transaction1.
          EXPECT_TRUE(
              transaction1->CommitPhaseOne(callback_creator1.CreateCallback())
                  .ok());
        }));
    RunAllTasksUntilIdle();

    idb_context_->TaskRunner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          // Finish committing transaction1.
          EXPECT_TRUE(callback_creator1.called);
          EXPECT_TRUE(callback_creator1.succeeded);
          EXPECT_TRUE(transaction1->CommitPhaseTwo().ok());

          // Initiate transaction 2 - delete range.
          transaction2 = std::make_unique<IndexedDBBackingStore::Transaction>(
              backing_store()->AsWeakPtr(),
              blink::mojom::IDBTransactionDurability::Relaxed);
          transaction2->Begin(CreateDummyLock());
          IndexedDBValue result_value;
          EXPECT_TRUE(backing_store()
                          ->DeleteRange(transaction2.get(), database_id,
                                        object_store_id, range)
                          .ok());

          // Start committing transaction2.
          EXPECT_TRUE(
              transaction2->CommitPhaseOne(callback_creator2.CreateCallback())
                  .ok());
        }));
    RunAllTasksUntilIdle();

    idb_context_->TaskRunner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          // Finish committing transaction2.
          EXPECT_TRUE(callback_creator2.called);
          EXPECT_TRUE(callback_creator2.succeeded);
          EXPECT_TRUE(transaction2->CommitPhaseTwo().ok());

          // Verify blob removals.
          ASSERT_EQ(2UL, backing_store()->removals().size());
          EXPECT_EQ(backing_store()->writes()[1].key(),
                    backing_store()->removals()[0]);
          EXPECT_EQ(backing_store()->writes()[2].key(),
                    backing_store()->removals()[1]);

          // Clean up on the IDB sequence.
          transaction1.reset();
          transaction2.reset();
        }));
    RunAllTasksUntilIdle();
  }
}

TEST_F(IndexedDBBackingStoreTestWithBlobs, DeleteRangeEmptyRange) {
  const std::vector<IndexedDBKey> keys = {
      IndexedDBKey(ASCIIToUTF16("key0")), IndexedDBKey(ASCIIToUTF16("key1")),
      IndexedDBKey(ASCIIToUTF16("key2")), IndexedDBKey(ASCIIToUTF16("key3")),
      IndexedDBKey(ASCIIToUTF16("key4"))};
  const IndexedDBKeyRange ranges[] = {
      IndexedDBKeyRange(keys[3], keys[4], true, false),
      IndexedDBKeyRange(keys[2], keys[1], false, false),
      IndexedDBKeyRange(keys[2], keys[1], true, true)};

  for (size_t i = 0; i < base::size(ranges); ++i) {
    const int64_t database_id = 1;
    const int64_t object_store_id = i + 1;
    const IndexedDBKeyRange& range = ranges[i];

    std::unique_ptr<IndexedDBBackingStore::Transaction> transaction1;
    TestCallback callback_creator1;
    std::unique_ptr<IndexedDBBackingStore::Transaction> transaction2;
    TestCallback callback_creator2;
    std::vector<std::unique_ptr<storage::BlobDataHandle>> blobs;

    for (size_t j = 0; j < 4; ++j)
      blobs.push_back(CreateBlob());

    idb_context_->TaskRunner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          // Reset from previous iteration.
          backing_store()->ClearWrites();
          backing_store()->ClearRemovals();

          std::vector<IndexedDBValue> values = {
              IndexedDBValue(
                  "value0",
                  {IndexedDBBlobInfo(
                      std::make_unique<storage::BlobDataHandle>(*blobs[0]),
                      base::UTF8ToUTF16("type 0"), 1)}),
              IndexedDBValue(
                  "value1",
                  {IndexedDBBlobInfo(
                      std::make_unique<storage::BlobDataHandle>(*blobs[1]),
                      base::UTF8ToUTF16("type 1"), 1)}),
              IndexedDBValue(
                  "value2",
                  {IndexedDBBlobInfo(
                      std::make_unique<storage::BlobDataHandle>(*blobs[2]),
                      base::UTF8ToUTF16("type 2"), 1)}),
              IndexedDBValue(
                  "value3",
                  {IndexedDBBlobInfo(
                      std::make_unique<storage::BlobDataHandle>(*blobs[3]),
                      base::UTF8ToUTF16("type 3"), 1)})};
          ASSERT_GE(keys.size(), values.size());

          // Initiate transaction1 - write records.
          transaction1 = std::make_unique<IndexedDBBackingStore::Transaction>(
              backing_store()->AsWeakPtr(),
              blink::mojom::IDBTransactionDurability::Relaxed);
          transaction1->Begin(CreateDummyLock());

          IndexedDBBackingStore::RecordIdentifier record;
          for (size_t i = 0; i < values.size(); ++i) {
            EXPECT_TRUE(backing_store()
                            ->PutRecord(transaction1.get(), database_id,
                                        object_store_id, keys[i], &values[i],
                                        &record)
                            .ok());
          }
          // Start committing transaction1.
          EXPECT_TRUE(
              transaction1->CommitPhaseOne(callback_creator1.CreateCallback())
                  .ok());
        }));
    RunAllTasksUntilIdle();

    idb_context_->TaskRunner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          // Finish committing transaction1.
          EXPECT_TRUE(callback_creator1.called);
          EXPECT_TRUE(callback_creator1.succeeded);
          EXPECT_TRUE(transaction1->CommitPhaseTwo().ok());

          // Initiate transaction 2 - delete range.
          transaction2 = std::make_unique<IndexedDBBackingStore::Transaction>(
              backing_store()->AsWeakPtr(),
              blink::mojom::IDBTransactionDurability::Relaxed);
          transaction2->Begin(CreateDummyLock());
          IndexedDBValue result_value;
          EXPECT_TRUE(backing_store()
                          ->DeleteRange(transaction2.get(), database_id,
                                        object_store_id, range)
                          .ok());

          // Start committing transaction2.
          EXPECT_TRUE(
              transaction2->CommitPhaseOne(callback_creator2.CreateCallback())
                  .ok());
        }));
    RunAllTasksUntilIdle();

    idb_context_->TaskRunner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          // Finish committing transaction2.
          EXPECT_TRUE(callback_creator2.called);
          EXPECT_TRUE(callback_creator2.succeeded);
          EXPECT_TRUE(transaction2->CommitPhaseTwo().ok());

          // Verify blob removals.
          EXPECT_EQ(0UL, backing_store()->removals().size());

          // Clean on the IDB sequence.
          transaction1.reset();
          transaction2.reset();
        }));
    RunAllTasksUntilIdle();
  }
}

TEST_F(IndexedDBBackingStoreTestWithBlobs, BlobJournalInterleavedTransactions) {
  std::unique_ptr<IndexedDBBackingStore::Transaction> transaction1;
  TestCallback callback_creator1;
  std::unique_ptr<IndexedDBBackingStore::Transaction> transaction2;
  TestCallback callback_creator2;

  idb_context_->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        // Initiate transaction1.
        transaction1 = std::make_unique<IndexedDBBackingStore::Transaction>(
            backing_store()->AsWeakPtr(),
            blink::mojom::IDBTransactionDurability::Relaxed);
        transaction1->Begin(CreateDummyLock());
        IndexedDBBackingStore::RecordIdentifier record1;
        EXPECT_TRUE(
            backing_store()
                ->PutRecord(transaction1.get(), 1, 1, key3_, &value3_, &record1)
                .ok());
        EXPECT_TRUE(
            transaction1->CommitPhaseOne(callback_creator1.CreateCallback())
                .ok());
      }));
  RunAllTasksUntilIdle();

  idb_context_->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        // Verify transaction1 phase one completed.
        EXPECT_TRUE(callback_creator1.called);
        EXPECT_TRUE(callback_creator1.succeeded);
        EXPECT_TRUE(CheckBlobWrites());
        EXPECT_EQ(0U, backing_store()->removals().size());

        // Initiate transaction2.
        transaction2 = std::make_unique<IndexedDBBackingStore::Transaction>(
            backing_store()->AsWeakPtr(),
            blink::mojom::IDBTransactionDurability::Relaxed);
        transaction2->Begin(CreateDummyLock());
        IndexedDBBackingStore::RecordIdentifier record2;
        EXPECT_TRUE(
            backing_store()
                ->PutRecord(transaction2.get(), 1, 1, key1_, &value1_, &record2)
                .ok());
        EXPECT_TRUE(
            transaction2->CommitPhaseOne(callback_creator2.CreateCallback())
                .ok());
      }));
  RunAllTasksUntilIdle();

  idb_context_->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        // Verify transaction2 phase one completed.
        EXPECT_TRUE(callback_creator2.called);
        EXPECT_TRUE(callback_creator2.succeeded);
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
      }));
  RunAllTasksUntilIdle();
}

TEST_F(IndexedDBBackingStoreTestWithBlobs, LiveBlobJournal) {
  std::unique_ptr<IndexedDBBackingStore::Transaction> transaction1;
  TestCallback callback_creator1;
  std::unique_ptr<IndexedDBBackingStore::Transaction> transaction3;
  TestCallback callback_creator3;
  IndexedDBValue read_result_value;

  idb_context_->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        transaction1 = std::make_unique<IndexedDBBackingStore::Transaction>(
            backing_store()->AsWeakPtr(),
            blink::mojom::IDBTransactionDurability::Relaxed);
        transaction1->Begin(CreateDummyLock());
        IndexedDBBackingStore::RecordIdentifier record;
        EXPECT_TRUE(
            backing_store()
                ->PutRecord(transaction1.get(), 1, 1, key3_, &value3_, &record)
                .ok());
        EXPECT_TRUE(
            transaction1->CommitPhaseOne(callback_creator1.CreateCallback())
                .ok());
      }));
  RunAllTasksUntilIdle();

  idb_context_->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        EXPECT_TRUE(callback_creator1.called);
        EXPECT_TRUE(callback_creator1.succeeded);
        EXPECT_TRUE(CheckBlobWrites());
        EXPECT_TRUE(transaction1->CommitPhaseTwo().ok());

        IndexedDBBackingStore::Transaction transaction2(
            backing_store()->AsWeakPtr(),
            blink::mojom::IDBTransactionDurability::Relaxed);
        transaction2.Begin(CreateDummyLock());
        EXPECT_TRUE(
            backing_store()
                ->GetRecord(&transaction2, 1, 1, key3_, &read_result_value)
                .ok());
        TestCallback callback_creator;
        EXPECT_TRUE(
            transaction2.CommitPhaseOne(callback_creator.CreateCallback())
                .ok());
        EXPECT_TRUE(callback_creator.called);
        EXPECT_TRUE(callback_creator.succeeded);
        EXPECT_TRUE(transaction2.CommitPhaseTwo().ok());
        EXPECT_EQ(value3_.bits, read_result_value.bits);
        EXPECT_TRUE(CheckBlobInfoMatches(read_result_value.blob_info));
        EXPECT_TRUE(CheckBlobReadsMatchWrites(read_result_value.blob_info));
        for (size_t i = 0; i < read_result_value.blob_info.size(); ++i) {
          read_result_value.blob_info[i].mark_used_callback().Run();
        }

        transaction3 = std::make_unique<IndexedDBBackingStore::Transaction>(
            backing_store()->AsWeakPtr(),
            blink::mojom::IDBTransactionDurability::Relaxed);
        transaction3->Begin(CreateDummyLock());
        EXPECT_TRUE(backing_store()
                        ->DeleteRange(transaction3.get(), 1, 1,
                                      IndexedDBKeyRange(key3_))
                        .ok());
        EXPECT_TRUE(
            transaction3->CommitPhaseOne(callback_creator3.CreateCallback())
                .ok());
      }));
  RunAllTasksUntilIdle();

  idb_context_->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        EXPECT_TRUE(callback_creator3.called);
        EXPECT_TRUE(callback_creator3.succeeded);
        EXPECT_TRUE(transaction3->CommitPhaseTwo().ok());
        EXPECT_EQ(0U, backing_store()->removals().size());
        for (size_t i = 0; i < read_result_value.blob_info.size(); ++i) {
          read_result_value.blob_info[i].release_callback().Run(
              read_result_value.blob_info[i].file_path());
        }
      }));
  RunAllTasksUntilIdle();

  idb_context_->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        EXPECT_TRUE(backing_store()->IsBlobCleanupPending());
#if DCHECK_IS_ON()
        EXPECT_EQ(
            3,
            backing_store()->NumAggregatedJournalCleaningRequestsForTesting());
#endif
        for (int i = 3; i < IndexedDBBackingStore::kMaxJournalCleanRequests;
             ++i) {
          backing_store()->StartJournalCleaningTimer();
        }
        EXPECT_NE(0U, backing_store()->removals().size());
        EXPECT_TRUE(CheckBlobRemovals());
#if DCHECK_IS_ON()
        EXPECT_EQ(0, backing_store()->NumBlobFilesDeletedForTesting());
#endif
        EXPECT_FALSE(backing_store()->IsBlobCleanupPending());

        // Clean on the IDB sequence.
        transaction1.reset();
        transaction3.reset();
        IndexedDBValue read_result_value;
      }));
  RunAllTasksUntilIdle();
}

// Make sure that using very high ( more than 32 bit ) values for database_id
// and object_store_id still work.
TEST_F(IndexedDBBackingStoreTest, HighIds) {
  base::RunLoop loop;
  idb_context_->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
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
              blink::mojom::IDBTransactionDurability::Relaxed);
          transaction1.Begin(CreateDummyLock());
          IndexedDBBackingStore::RecordIdentifier record;
          leveldb::Status s = backing_store()->PutRecord(
              &transaction1, high_database_id, high_object_store_id, key1,
              &value1, &record);
          EXPECT_TRUE(s.ok());

          s = backing_store()->PutIndexDataForRecord(
              &transaction1, high_database_id, high_object_store_id,
              invalid_high_index_id, index_key, record);
          EXPECT_FALSE(s.ok());

          s = backing_store()->PutIndexDataForRecord(
              &transaction1, high_database_id, high_object_store_id,
              high_index_id, index_key, record);
          EXPECT_TRUE(s.ok());

          TestCallback callback_creator;
          EXPECT_TRUE(
              transaction1.CommitPhaseOne(callback_creator.CreateCallback())
                  .ok());
          EXPECT_TRUE(callback_creator.called);
          EXPECT_TRUE(callback_creator.succeeded);
          EXPECT_TRUE(transaction1.CommitPhaseTwo().ok());
        }

        {
          IndexedDBBackingStore::Transaction transaction2(
              backing_store()->AsWeakPtr(),
              blink::mojom::IDBTransactionDurability::Relaxed);
          transaction2.Begin(CreateDummyLock());
          IndexedDBValue result_value;
          leveldb::Status s = backing_store()->GetRecord(
              &transaction2, high_database_id, high_object_store_id, key1,
              &result_value);
          EXPECT_TRUE(s.ok());
          EXPECT_EQ(value1.bits, result_value.bits);

          std::unique_ptr<IndexedDBKey> new_primary_key;
          s = backing_store()->GetPrimaryKeyViaIndex(
              &transaction2, high_database_id, high_object_store_id,
              invalid_high_index_id, index_key, &new_primary_key);
          EXPECT_FALSE(s.ok());

          s = backing_store()->GetPrimaryKeyViaIndex(
              &transaction2, high_database_id, high_object_store_id,
              high_index_id, index_key, &new_primary_key);
          EXPECT_TRUE(s.ok());
          EXPECT_TRUE(new_primary_key->Equals(key1));

          TestCallback callback_creator;
          EXPECT_TRUE(
              transaction2.CommitPhaseOne(callback_creator.CreateCallback())
                  .ok());
          EXPECT_TRUE(callback_creator.called);
          EXPECT_TRUE(callback_creator.succeeded);
          EXPECT_TRUE(transaction2.CommitPhaseTwo().ok());
        }
        loop.Quit();
      }));
  loop.Run();

  CycleIDBTaskRunner();
}

// Make sure that other invalid ids do not crash.
TEST_F(IndexedDBBackingStoreTest, InvalidIds) {
  base::RunLoop loop;
  idb_context_->TaskRunner()->PostTask(
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
            blink::mojom::IDBTransactionDurability::Relaxed);
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
  idb_context_->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        const base::string16 database_name(ASCIIToUTF16("db1"));
        int64_t database_id;
        const int64_t version = 9;

        const int64_t object_store_id = 99;
        const base::string16 object_store_name(ASCIIToUTF16("object_store1"));
        const bool auto_increment = true;
        const IndexedDBKeyPath object_store_key_path(
            ASCIIToUTF16("object_store_key"));

        const int64_t index_id = 999;
        const base::string16 index_name(ASCIIToUTF16("index1"));
        const bool unique = true;
        const bool multi_entry = true;
        const IndexedDBKeyPath index_key_path(ASCIIToUTF16("index_key"));

        IndexedDBMetadataCoding metadata_coding;

        {
          IndexedDBDatabaseMetadata database;
          leveldb::Status s = metadata_coding.CreateDatabase(
              backing_store()->db(), backing_store()->origin_identifier(),
              database_name, version, &database);
          EXPECT_TRUE(s.ok());
          EXPECT_GT(database.id, 0);
          database_id = database.id;

          IndexedDBBackingStore::Transaction transaction(
              backing_store()->AsWeakPtr(),
              blink::mojom::IDBTransactionDurability::Relaxed);
          transaction.Begin(CreateDummyLock());

          IndexedDBObjectStoreMetadata object_store;
          s = metadata_coding.CreateObjectStore(
              transaction.transaction(), database.id, object_store_id,
              object_store_name, object_store_key_path, auto_increment,
              &object_store);
          EXPECT_TRUE(s.ok());

          IndexedDBIndexMetadata index;
          s = metadata_coding.CreateIndex(
              transaction.transaction(), database.id, object_store.id, index_id,
              index_name, index_key_path, unique, multi_entry, &index);
          EXPECT_TRUE(s.ok());

          TestCallback callback_creator;
          EXPECT_TRUE(
              transaction.CommitPhaseOne(callback_creator.CreateCallback())
                  .ok());
          EXPECT_TRUE(callback_creator.called);
          EXPECT_TRUE(callback_creator.succeeded);
          EXPECT_TRUE(transaction.CommitPhaseTwo().ok());
        }

        {
          IndexedDBDatabaseMetadata database;
          bool found;
          leveldb::Status s = metadata_coding.ReadMetadataForDatabaseName(
              backing_store()->db(), backing_store()->origin_identifier(),
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
    idb_context_->TaskRunner()->PostTask(FROM_HERE, cycle_loop.QuitClosure());
    cycle_loop.Run();
  }
}

TEST_F(IndexedDBBackingStoreTest, GetDatabaseNames) {
  base::RunLoop loop;
  idb_context_->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        const base::string16 db1_name(ASCIIToUTF16("db1"));
        const int64_t db1_version = 1LL;

        // Database records with DEFAULT_VERSION represent
        // stale data, and should not be enumerated.
        const base::string16 db2_name(ASCIIToUTF16("db2"));
        const int64_t db2_version = IndexedDBDatabaseMetadata::DEFAULT_VERSION;
        IndexedDBMetadataCoding metadata_coding;

        IndexedDBDatabaseMetadata db1;
        leveldb::Status s = metadata_coding.CreateDatabase(
            backing_store()->db(), backing_store()->origin_identifier(),
            db1_name, db1_version, &db1);
        EXPECT_TRUE(s.ok());
        EXPECT_GT(db1.id, 0LL);

        IndexedDBDatabaseMetadata db2;
        s = metadata_coding.CreateDatabase(backing_store()->db(),
                                           backing_store()->origin_identifier(),
                                           db2_name, db2_version, &db2);
        EXPECT_TRUE(s.ok());
        EXPECT_GT(db2.id, db1.id);

        std::vector<base::string16> names;
        s = metadata_coding.ReadDatabaseNames(
            backing_store()->db(), backing_store()->origin_identifier(),
            &names);
        EXPECT_TRUE(s.ok());
        ASSERT_EQ(1U, names.size());
        EXPECT_EQ(db1_name, names[0]);
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(IndexedDBBackingStoreTest, ReadCorruptionInfo) {
  // No |path_base|.
  EXPECT_TRUE(
      indexed_db::ReadCorruptionInfo(base::FilePath(), Origin()).empty());

  const base::FilePath path_base = temp_dir_.GetPath();
  const Origin origin = Origin::Create(GURL("http://www.google.com/"));
  ASSERT_FALSE(path_base.empty());
  ASSERT_TRUE(PathIsWritable(path_base));

  // File not found.
  EXPECT_TRUE(indexed_db::ReadCorruptionInfo(path_base, origin).empty());

  const base::FilePath info_path =
      path_base.AppendASCII("http_www.google.com_0.indexeddb.leveldb")
          .AppendASCII("corruption_info.json");
  ASSERT_TRUE(CreateDirectory(info_path.DirName()));

  // Empty file.
  std::string dummy_data;
  ASSERT_TRUE(WriteFile(info_path, dummy_data));
  EXPECT_TRUE(indexed_db::ReadCorruptionInfo(path_base, origin).empty());
  EXPECT_FALSE(PathExists(info_path));

  // File size > 4 KB.
  dummy_data.resize(5000, 'c');
  ASSERT_TRUE(WriteFile(info_path, dummy_data));
  EXPECT_TRUE(indexed_db::ReadCorruptionInfo(path_base, origin).empty());
  EXPECT_FALSE(PathExists(info_path));

  // Random string.
  ASSERT_TRUE(WriteFile(info_path, "foo bar"));
  EXPECT_TRUE(indexed_db::ReadCorruptionInfo(path_base, origin).empty());
  EXPECT_FALSE(PathExists(info_path));

  // Not a dictionary.
  ASSERT_TRUE(WriteFile(info_path, "[]"));
  EXPECT_TRUE(indexed_db::ReadCorruptionInfo(path_base, origin).empty());
  EXPECT_FALSE(PathExists(info_path));

  // Empty dictionary.
  ASSERT_TRUE(WriteFile(info_path, "{}"));
  EXPECT_TRUE(indexed_db::ReadCorruptionInfo(path_base, origin).empty());
  EXPECT_FALSE(PathExists(info_path));

  // Dictionary, no message key.
  ASSERT_TRUE(WriteFile(info_path, "{\"foo\":\"bar\"}"));
  EXPECT_TRUE(indexed_db::ReadCorruptionInfo(path_base, origin).empty());
  EXPECT_FALSE(PathExists(info_path));

  // Dictionary, message key.
  ASSERT_TRUE(WriteFile(info_path, "{\"message\":\"bar\"}"));
  std::string message = indexed_db::ReadCorruptionInfo(path_base, origin);
  EXPECT_FALSE(message.empty());
  EXPECT_FALSE(PathExists(info_path));
  EXPECT_EQ("bar", message);

  // Dictionary, message key and more.
  ASSERT_TRUE(WriteFile(info_path, "{\"message\":\"foo\",\"bar\":5}"));
  message = indexed_db::ReadCorruptionInfo(path_base, origin);
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
  idb_context_->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        const base::string16 database_name(ASCIIToUTF16("db1"));
        const int64_t version = 9;

        const base::string16 object_store_name(ASCIIToUTF16("object_store1"));
        const bool auto_increment = true;
        const IndexedDBKeyPath object_store_key_path(
            ASCIIToUTF16("object_store_key"));

        IndexedDBMetadataCoding metadata_coding;

        {
          IndexedDBDatabaseMetadata database;
          leveldb::Status s = metadata_coding.CreateDatabase(
              backing_store()->db(), backing_store()->origin_identifier(),
              database_name, version, &database);
          EXPECT_TRUE(s.ok());
          EXPECT_GT(database.id, 0);
          database_id = database.id;

          IndexedDBBackingStore::Transaction transaction(
              backing_store()->AsWeakPtr(),
              blink::mojom::IDBTransactionDurability::Relaxed);
          transaction.Begin(CreateDummyLock());

          IndexedDBObjectStoreMetadata object_store;
          s = metadata_coding.CreateObjectStore(
              transaction.transaction(), database.id, object_store_id,
              object_store_name, object_store_key_path, auto_increment,
              &object_store);
          EXPECT_TRUE(s.ok());

          TestCallback callback_creator;
          EXPECT_TRUE(
              transaction.CommitPhaseOne(callback_creator.CreateCallback())
                  .ok());
          EXPECT_TRUE(callback_creator.called);
          EXPECT_TRUE(callback_creator.succeeded);
          EXPECT_TRUE(transaction.CommitPhaseTwo().ok());
        }
      }));
  RunAllTasksUntilIdle();
  idb_context_->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        const IndexedDBKey key = key1_;
        IndexedDBValue value = value1_;

        // Save a value.
        IndexedDBBackingStore::Transaction transaction1(
            backing_store()->AsWeakPtr(),
            blink::mojom::IDBTransactionDurability::Relaxed);
        transaction1.Begin(CreateDummyLock());
        IndexedDBBackingStore::RecordIdentifier record;
        leveldb::Status s = backing_store()->PutRecord(
            &transaction1, database_id, object_store_id, key, &value, &record);
        EXPECT_TRUE(s.ok());
        TestCallback callback_creator;
        EXPECT_TRUE(
            transaction1.CommitPhaseOne(callback_creator.CreateCallback())
                .ok());
        EXPECT_TRUE(callback_creator.called);
        EXPECT_TRUE(callback_creator.succeeded);
        EXPECT_TRUE(transaction1.CommitPhaseTwo().ok());

        // Set the schema to 2, which was before blob support.
        std::unique_ptr<LevelDBWriteBatch> write_batch =
            LevelDBWriteBatch::Create();
        const std::string schema_version_key = SchemaVersionKey::Encode();
        ignore_result(
            indexed_db::PutInt(write_batch.get(), schema_version_key, 2));
        ASSERT_TRUE(backing_store()->db()->Write(write_batch.get()).ok());
      }));
  RunAllTasksUntilIdle();

  DestroyFactoryAndBackingStore();
  CreateFactoryAndBackingStore();

  idb_context_->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        const IndexedDBKey key = key1_;
        IndexedDBValue value = value1_;

        IndexedDBBackingStore::Transaction transaction2(
            backing_store()->AsWeakPtr(),
            blink::mojom::IDBTransactionDurability::Relaxed);
        transaction2.Begin(CreateDummyLock());
        IndexedDBValue result_value;
        EXPECT_TRUE(backing_store()
                        ->GetRecord(&transaction2, database_id, object_store_id,
                                    key, &result_value)
                        .ok());
        TestCallback callback_creator;
        EXPECT_TRUE(
            transaction2.CommitPhaseOne(callback_creator.CreateCallback())
                .ok());
        EXPECT_TRUE(callback_creator.called);
        EXPECT_TRUE(callback_creator.succeeded);
        EXPECT_TRUE(transaction2.CommitPhaseTwo().ok());
        EXPECT_EQ(value.bits, result_value.bits);

        // Test that we upgraded.
        const std::string schema_version_key = SchemaVersionKey::Encode();
        int64_t found_int = 0;
        bool found = false;
        bool success =
            indexed_db::GetInt(backing_store()->db(), schema_version_key,
                               &found_int, &found)
                .ok();
        ASSERT_TRUE(success);

        EXPECT_TRUE(found);
        EXPECT_EQ(3, found_int);
      }));
  RunAllTasksUntilIdle();
}

// Our v2->v3 schema migration code forgot to bump the on-disk version number.
// This test covers migrating a v3 database mislabeled as v2 to a properly
// labeled v3 database. When the mislabeled database has blob entries, we must
// treat it as corrupt and delete it.
// https://crbug.com/756447, https://crbug.com/829125, https://crbug.com/829141
TEST_F(IndexedDBBackingStoreTestWithBlobs, SchemaUpgradeWithBlobsCorrupt) {
  int64_t database_id;
  const int64_t object_store_id = 99;
  std::unique_ptr<IndexedDBBackingStore::Transaction> transaction1;
  TestCallback callback_creator1;

  // The database metadata needs to be written so the blob entry keys can
  // be detected.
  idb_context_->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        const base::string16 database_name(ASCIIToUTF16("db1"));
        const int64_t version = 9;

        const base::string16 object_store_name(ASCIIToUTF16("object_store1"));
        const bool auto_increment = true;
        const IndexedDBKeyPath object_store_key_path(
            ASCIIToUTF16("object_store_key"));

        IndexedDBMetadataCoding metadata_coding;

        {
          IndexedDBDatabaseMetadata database;
          leveldb::Status s = metadata_coding.CreateDatabase(
              backing_store()->db(), backing_store()->origin_identifier(),
              database_name, version, &database);
          EXPECT_TRUE(s.ok());
          EXPECT_GT(database.id, 0);
          database_id = database.id;

          IndexedDBBackingStore::Transaction transaction(
              backing_store()->AsWeakPtr(),
              blink::mojom::IDBTransactionDurability::Relaxed);
          transaction.Begin(CreateDummyLock());

          IndexedDBObjectStoreMetadata object_store;
          s = metadata_coding.CreateObjectStore(
              transaction.transaction(), database.id, object_store_id,
              object_store_name, object_store_key_path, auto_increment,
              &object_store);
          EXPECT_TRUE(s.ok());

          TestCallback callback_creator;
          EXPECT_TRUE(
              transaction.CommitPhaseOne(callback_creator.CreateCallback())
                  .ok());
          EXPECT_TRUE(callback_creator.called);
          EXPECT_TRUE(callback_creator.succeeded);
          EXPECT_TRUE(transaction.CommitPhaseTwo().ok());
        }
      }));
  RunAllTasksUntilIdle();

  idb_context_->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        // Initiate transaction1 - writing blobs.
        transaction1 = std::make_unique<IndexedDBBackingStore::Transaction>(
            backing_store()->AsWeakPtr(),
            blink::mojom::IDBTransactionDurability::Relaxed);
        transaction1->Begin(CreateDummyLock());
        IndexedDBBackingStore::RecordIdentifier record;
        EXPECT_TRUE(backing_store()
                        ->PutRecord(transaction1.get(), database_id,
                                    object_store_id, key3_, &value3_, &record)
                        .ok());
        EXPECT_TRUE(
            transaction1->CommitPhaseOne(callback_creator1.CreateCallback())
                .ok());
      }));
  RunAllTasksUntilIdle();

  idb_context_->TaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        // Finish up transaction1, verifying blob writes.
        EXPECT_TRUE(callback_creator1.called);
        EXPECT_TRUE(callback_creator1.succeeded);
        EXPECT_TRUE(CheckBlobWrites());
        EXPECT_TRUE(transaction1->CommitPhaseTwo().ok());

        // Set the schema to 2, which was before blob support.
        std::unique_ptr<LevelDBWriteBatch> write_batch =
            LevelDBWriteBatch::Create();
        const std::string schema_version_key = SchemaVersionKey::Encode();
        ignore_result(
            indexed_db::PutInt(write_batch.get(), schema_version_key, 2));
        ASSERT_TRUE(backing_store()->db()->Write(write_batch.get()).ok());

        // Clean up on the IDB sequence.
        transaction1.reset();
      }));
  RunAllTasksUntilIdle();

  DestroyFactoryAndBackingStore();
  CreateFactoryAndBackingStore();

  // The factory returns a null backing store pointer when there is a corrupt
  // database.
  EXPECT_TRUE(data_loss_info_.status == blink::mojom::IDBDataLoss::Total);
}

}  // namespace indexed_db_backing_store_unittest
}  // namespace content
