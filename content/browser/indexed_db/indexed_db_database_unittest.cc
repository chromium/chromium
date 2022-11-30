// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_database.h"

#include <stdint.h>
#include <set>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/browser/indexed_db/fake_indexed_db_metadata_coding.h"
#include "content/browser/indexed_db/indexed_db.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_callbacks.h"
#include "content/browser/indexed_db/indexed_db_class_factory.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_cursor.h"
#include "content/browser/indexed_db/indexed_db_factory_impl.h"
#include "content/browser/indexed_db/indexed_db_fake_backing_store.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "content/browser/indexed_db/mock_indexed_db_callbacks.h"
#include "content/browser/indexed_db/mock_indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/mock_indexed_db_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

using blink::IndexedDBDatabaseMetadata;
using blink::IndexedDBIndexKeys;
using blink::IndexedDBKey;
using blink::IndexedDBKeyPath;

namespace content {
namespace {

void CreateAndBindTransactionPlaceholder(
    base::WeakPtr<IndexedDBTransaction> transaction) {}

}  // namespace

class IndexedDBDatabaseTest : public ::testing::Test {
 public:
  IndexedDBDatabaseTest() = default;

  void SetUp() override {
    backing_store_ = std::make_unique<IndexedDBFakeBackingStore>();
    factory_ = std::make_unique<MockIndexedDBFactory>();
    std::unique_ptr<FakeIndexedDBMetadataCoding> metadata_coding =
        std::make_unique<FakeIndexedDBMetadataCoding>();
    metadata_coding_ = metadata_coding.get();
    leveldb::Status s;

    std::tie(db_, s) = IndexedDBClassFactory::Get()->CreateIndexedDBDatabase(
        u"db", backing_store_.get(), factory_.get(),
        base::BindRepeating(&IndexedDBDatabaseTest::RunTasksForDatabase,
                            weak_factory_.GetWeakPtr(), true),
        std::move(metadata_coding), IndexedDBDatabase::Identifier(),
        &lock_manager_);
    ASSERT_TRUE(s.ok());
  }

  void RunTasksForDatabase(bool async) {
    if (!db_)
      return;
    if (async) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&IndexedDBDatabaseTest::RunTasksForDatabase,
                                    weak_factory_.GetWeakPtr(), false));
      return;
    }
    auto [result, status] = db_->RunTasks();
    switch (result) {
      case IndexedDBDatabase::RunTasksResult::kDone:
        return;
      case IndexedDBDatabase::RunTasksResult::kError:
        error_called_ = true;
        return;
      case IndexedDBDatabase::RunTasksResult::kCanBeDestroyed:
        db_.reset();
        metadata_coding_ = nullptr;
        return;
    }
  }

  void RunPostedTasks() { base::RunLoop().RunUntilIdle(); }

 protected:
  std::unique_ptr<IndexedDBFakeBackingStore> backing_store_;
  std::unique_ptr<MockIndexedDBFactory> factory_;
  std::unique_ptr<IndexedDBDatabase> db_;
  raw_ptr<FakeIndexedDBMetadataCoding> metadata_coding_ = nullptr;
  bool error_called_ = false;

 private:
  base::test::TaskEnvironment task_environment_;
  PartitionedLockManager lock_manager_;

  base::WeakPtrFactory<IndexedDBDatabaseTest> weak_factory_{this};
};

TEST_F(IndexedDBDatabaseTest, ConnectionLifecycle) {
  auto request1 = base::MakeRefCounted<MockIndexedDBCallbacks>();
  auto callbacks1 = base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();
  const int64_t transaction_id1 = 1;
  auto create_transaction_callback1 =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  auto connection1 = std::make_unique<IndexedDBPendingConnection>(
      request1, callbacks1, transaction_id1,
      IndexedDBDatabaseMetadata::DEFAULT_VERSION,
      std::move(create_transaction_callback1));
  db_->ScheduleOpenConnection(IndexedDBBucketStateHandle(),
                              std::move(connection1));
  RunPostedTasks();

  auto request2 = base::MakeRefCounted<MockIndexedDBCallbacks>();
  auto callbacks2 = base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();
  const int64_t transaction_id2 = 2;
  auto create_transaction_callback2 =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  auto connection2 = std::make_unique<IndexedDBPendingConnection>(
      request2, callbacks2, transaction_id2,
      IndexedDBDatabaseMetadata::DEFAULT_VERSION,
      std::move(create_transaction_callback2));
  db_->ScheduleOpenConnection(IndexedDBBucketStateHandle(),
                              std::move(connection2));
  RunPostedTasks();

  EXPECT_TRUE(request1->connection());
  request1->connection()->CloseAndReportForceClose();
  EXPECT_FALSE(request1->connection()->IsConnected());

  EXPECT_TRUE(request2->connection());
  request2->connection()->CloseAndReportForceClose();
  EXPECT_FALSE(request2->connection()->IsConnected());

  RunPostedTasks();

  EXPECT_FALSE(db_);
}

TEST_F(IndexedDBDatabaseTest, ForcedClose) {
  auto callbacks = base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();
  auto request = base::MakeRefCounted<MockIndexedDBCallbacks>();
  const int64_t upgrade_transaction_id = 3;
  auto create_transaction_callback =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  auto connection = std::make_unique<IndexedDBPendingConnection>(
      request, callbacks, upgrade_transaction_id,
      IndexedDBDatabaseMetadata::DEFAULT_VERSION,
      std::move(create_transaction_callback));
  db_->ScheduleOpenConnection(IndexedDBBucketStateHandle(),
                              std::move(connection));
  RunPostedTasks();

  EXPECT_EQ(db_.get(), request->connection()->database().get());

  const int64_t transaction_id = 123;
  const std::vector<int64_t> scope;
  IndexedDBTransaction* transaction = request->connection()->CreateTransaction(
      transaction_id, std::set<int64_t>(scope.begin(), scope.end()),
      blink::mojom::IDBTransactionMode::ReadOnly,
      new IndexedDBBackingStore::Transaction(
          backing_store_->AsWeakPtr(),
          blink::mojom::IDBTransactionDurability::Relaxed,
          blink::mojom::IDBTransactionMode::ReadWrite));
  db_->RegisterAndScheduleTransaction(transaction);

  request->connection()->CloseAndReportForceClose();

  EXPECT_TRUE(callbacks->abort_called());
}

namespace {

class MockCallbacks : public IndexedDBCallbacks {
 public:
  MockCallbacks()
      : IndexedDBCallbacks(nullptr,
                           absl::nullopt,
                           mojo::NullAssociatedRemote(),
                           base::SingleThreadTaskRunner::GetCurrentDefault()) {}

  MockCallbacks(const MockCallbacks&) = delete;
  MockCallbacks& operator=(const MockCallbacks&) = delete;

  void OnBlocked(int64_t existing_version) override { blocked_called_ = true; }
  void OnSuccess(int64_t result) override { success_called_ = true; }
  void OnError(const IndexedDBDatabaseError& error) override {
    error_called_ = true;
  }

  bool blocked_called() const { return blocked_called_; }
  bool success_called() const { return success_called_; }
  bool error_called() const { return error_called_; }

 private:
  ~MockCallbacks() override = default;

  bool blocked_called_ = false;
  bool success_called_ = false;
  bool error_called_ = false;
};

}  // namespace

TEST_F(IndexedDBDatabaseTest, PendingDelete) {
  auto request1 = base::MakeRefCounted<MockIndexedDBCallbacks>();
  auto callbacks1 = base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();
  const int64_t transaction_id1 = 1;
  auto create_transaction_callback1 =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  auto connection = std::make_unique<IndexedDBPendingConnection>(
      request1, callbacks1, transaction_id1,
      IndexedDBDatabaseMetadata::DEFAULT_VERSION,
      std::move(create_transaction_callback1));
  db_->ScheduleOpenConnection(IndexedDBBucketStateHandle(),
                              std::move(connection));
  RunPostedTasks();

  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 0UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 0UL);

  bool deleted = false;
  auto request2 = base::MakeRefCounted<MockCallbacks>();
  db_->ScheduleDeleteDatabase(
      IndexedDBBucketStateHandle(), request2,
      base::BindLambdaForTesting([&]() { deleted = true; }));
  RunPostedTasks();
  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 1UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 0UL);

  EXPECT_FALSE(request2->blocked_called());
  request1->connection()->VersionChangeIgnored();
  EXPECT_TRUE(request2->blocked_called());
  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 1UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 0UL);

  db_->ForceCloseAndRunTasks();

  RunPostedTasks();
  EXPECT_FALSE(db_);

  EXPECT_TRUE(deleted);
  EXPECT_TRUE(request2->success_called());
}

TEST_F(IndexedDBDatabaseTest, OpenDeleteClear) {
  const int64_t kDatabaseVersion = 1;

  auto request1 = base::MakeRefCounted<MockIndexedDBCallbacks>(
      /*expect_connection=*/true);
  auto callbacks1 = base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();
  const int64_t transaction_id1 = 1;
  auto create_transaction_callback1 =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  auto connection1 = std::make_unique<IndexedDBPendingConnection>(
      request1, callbacks1, transaction_id1, kDatabaseVersion,
      std::move(create_transaction_callback1));
  db_->ScheduleOpenConnection(IndexedDBBucketStateHandle(),
                              std::move(connection1));
  RunPostedTasks();

  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 1UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 0UL);

  auto request2 = base::MakeRefCounted<MockIndexedDBCallbacks>(
      /*expect_connection=*/false);
  auto callbacks2 = base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();
  const int64_t transaction_id2 = 2;
  auto create_transaction_callback2 =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  auto connection2 = std::make_unique<IndexedDBPendingConnection>(
      request2, callbacks2, transaction_id2, kDatabaseVersion,
      std::move(create_transaction_callback2));
  db_->ScheduleOpenConnection(IndexedDBBucketStateHandle(),
                              std::move(connection2));
  RunPostedTasks();

  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 1UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 1UL);

  auto request3 = base::MakeRefCounted<MockIndexedDBCallbacks>(
      /*expect_connection=*/false);
  auto callbacks3 = base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();
  const int64_t transaction_id3 = 3;
  auto create_transaction_callback3 =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  auto connection3 = std::make_unique<IndexedDBPendingConnection>(
      request3, callbacks3, transaction_id3, kDatabaseVersion,
      std::move(create_transaction_callback3));
  db_->ScheduleOpenConnection(IndexedDBBucketStateHandle(),
                              std::move(connection3));
  RunPostedTasks();

  EXPECT_TRUE(request1->upgrade_called());

  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 1UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 2UL);

  db_->ForceCloseAndRunTasks();
  RunPostedTasks();
  EXPECT_FALSE(db_);

  EXPECT_TRUE(callbacks1->forced_close_called());
  EXPECT_TRUE(request1->error_called());
  EXPECT_TRUE(callbacks2->forced_close_called());
  EXPECT_TRUE(request2->error_called());
  EXPECT_TRUE(callbacks3->forced_close_called());
  EXPECT_TRUE(request3->error_called());
}

TEST_F(IndexedDBDatabaseTest, ForceDelete) {
  auto request1 = base::MakeRefCounted<MockIndexedDBCallbacks>();
  auto callbacks1 = base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();
  const int64_t transaction_id1 = 1;
  auto create_transaction_callback1 =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  auto connection = std::make_unique<IndexedDBPendingConnection>(
      request1, callbacks1, transaction_id1,
      IndexedDBDatabaseMetadata::DEFAULT_VERSION,
      std::move(create_transaction_callback1));
  db_->ScheduleOpenConnection(IndexedDBBucketStateHandle(),
                              std::move(connection));
  RunPostedTasks();

  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 0UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 0UL);

  bool deleted = false;
  auto request2 = base::MakeRefCounted<MockCallbacks>();
  db_->ScheduleDeleteDatabase(
      IndexedDBBucketStateHandle(), request2,
      base::BindLambdaForTesting([&]() { deleted = true; }));
  RunPostedTasks();
  EXPECT_FALSE(deleted);
  db_->ForceCloseAndRunTasks();
  RunPostedTasks();
  EXPECT_FALSE(db_);
  EXPECT_TRUE(deleted);
  EXPECT_FALSE(request2->blocked_called());
  EXPECT_TRUE(request2->success_called());
}

TEST_F(IndexedDBDatabaseTest, ForceCloseWhileOpenPending) {
  // Verify that pending connection requests are handled correctly during a
  // ForceClose.
  auto request1 = base::MakeRefCounted<MockIndexedDBCallbacks>();
  auto callbacks1 = base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();
  const int64_t transaction_id1 = 1;
  auto create_transaction_callback1 =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  auto connection = std::make_unique<IndexedDBPendingConnection>(
      request1, callbacks1, transaction_id1,
      IndexedDBDatabaseMetadata::DEFAULT_VERSION,
      std::move(create_transaction_callback1));
  db_->ScheduleOpenConnection(IndexedDBBucketStateHandle(),
                              std::move(connection));
  RunPostedTasks();

  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 0UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 0UL);

  auto request2 = base::MakeRefCounted<MockIndexedDBCallbacks>(
      /*expect_connection=*/false);
  auto callbacks2 = base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();
  const int64_t transaction_id2 = 2;
  auto create_transaction_callback2 =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  auto connection2 = std::make_unique<IndexedDBPendingConnection>(
      request1, callbacks1, transaction_id2, 3,
      std::move(create_transaction_callback2));
  db_->ScheduleOpenConnection(IndexedDBBucketStateHandle(),
                              std::move(connection2));
  RunPostedTasks();

  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 1UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 0UL);

  db_->ForceCloseAndRunTasks();
  RunPostedTasks();
  EXPECT_FALSE(db_);
}

TEST_F(IndexedDBDatabaseTest, ForceCloseWhileOpenAndDeletePending) {
  // Verify that pending connection requests are handled correctly during a
  // ForceClose.
  auto request1 = base::MakeRefCounted<MockIndexedDBCallbacks>();
  auto callbacks1 = base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();
  const int64_t transaction_id1 = 1;
  auto create_transaction_callback1 =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  auto connection = std::make_unique<IndexedDBPendingConnection>(
      request1, callbacks1, transaction_id1,
      IndexedDBDatabaseMetadata::DEFAULT_VERSION,
      std::move(create_transaction_callback1));
  db_->ScheduleOpenConnection(IndexedDBBucketStateHandle(),
                              std::move(connection));
  RunPostedTasks();

  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 0UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 0UL);

  auto request2 = base::MakeRefCounted<MockIndexedDBCallbacks>(false);
  auto callbacks2 = base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();
  const int64_t transaction_id2 = 2;
  auto create_transaction_callback2 =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  auto connection2 = std::make_unique<IndexedDBPendingConnection>(
      request1, callbacks1, transaction_id2, 3,
      std::move(create_transaction_callback2));
  db_->ScheduleOpenConnection(IndexedDBBucketStateHandle(),
                              std::move(connection2));
  RunPostedTasks();

  bool deleted = false;
  auto request3 = base::MakeRefCounted<MockCallbacks>();
  db_->ScheduleDeleteDatabase(
      IndexedDBBucketStateHandle(), request3,
      base::BindLambdaForTesting([&]() { deleted = true; }));
  RunPostedTasks();
  EXPECT_FALSE(deleted);

  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 1UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 1UL);

  db_->ForceCloseAndRunTasks();
  RunPostedTasks();
  EXPECT_TRUE(deleted);
  EXPECT_FALSE(db_);
}

leveldb::Status DummyOperation(IndexedDBTransaction* transaction) {
  return leveldb::Status::OK();
}

class IndexedDBDatabaseOperationTest : public testing::Test {
 public:
  IndexedDBDatabaseOperationTest()
      : commit_success_(leveldb::Status::OK()),
        factory_(std::make_unique<MockIndexedDBFactory>()) {}

  IndexedDBDatabaseOperationTest(const IndexedDBDatabaseOperationTest&) =
      delete;
  IndexedDBDatabaseOperationTest& operator=(
      const IndexedDBDatabaseOperationTest&) = delete;

  void SetUp() override {
    backing_store_ = std::make_unique<IndexedDBFakeBackingStore>();
    std::unique_ptr<FakeIndexedDBMetadataCoding> metadata_coding =
        std::make_unique<FakeIndexedDBMetadataCoding>();
    metadata_coding_ = metadata_coding.get();
    leveldb::Status s;
    std::tie(db_, s) = IndexedDBClassFactory::Get()->CreateIndexedDBDatabase(
        u"db", backing_store_.get(), factory_.get(),
        base::BindRepeating(
            &IndexedDBDatabaseOperationTest::RunTasksForDatabase,
            base::Unretained(this), true),
        std::move(metadata_coding), IndexedDBDatabase::Identifier(),
        &lock_manager_);
    ASSERT_TRUE(s.ok());

    request_ = base::MakeRefCounted<MockIndexedDBCallbacks>();
    callbacks_ = base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();
    const int64_t transaction_id = 1;
    auto create_transaction_callback1 =
        base::BindOnce(&CreateAndBindTransactionPlaceholder);
    auto connection = std::make_unique<IndexedDBPendingConnection>(
        request_, callbacks_, transaction_id,
        IndexedDBDatabaseMetadata::DEFAULT_VERSION,
        std::move(create_transaction_callback1));
    db_->ScheduleOpenConnection(IndexedDBBucketStateHandle(),
                                std::move(connection));
    RunPostedTasks();
    EXPECT_EQ(IndexedDBDatabaseMetadata::NO_VERSION, db_->metadata().version);

    EXPECT_TRUE(request_->connection());
    transaction_ = request_->connection()->CreateTransaction(
        transaction_id, /*scope=*/std::set<int64_t>(),
        blink::mojom::IDBTransactionMode::VersionChange,
        new IndexedDBFakeBackingStore::FakeTransaction(commit_success_));
    db_->RegisterAndScheduleTransaction(transaction_);

    // Add a dummy task which takes the place of the VersionChangeOperation
    // which kicks off the upgrade. This ensures that the transaction has
    // processed at least one task before the CreateObjectStore call.
    transaction_->ScheduleTask(base::BindOnce(&DummyOperation));
    // Run posted tasks to execute the dummy operation and ensure that it is
    // stored in the connection.
    RunPostedTasks();
  }

  void RunTasksForDatabase(bool async) {
    if (!db_)
      return;
    if (async) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&IndexedDBDatabaseOperationTest::RunTasksForDatabase,
                         base::Unretained(this), false));
      return;
    }
    auto [result, status] = db_->RunTasks();
    switch (result) {
      case IndexedDBDatabase::RunTasksResult::kDone:
        return;
      case IndexedDBDatabase::RunTasksResult::kError:
        error_called_ = true;
        return;
      case IndexedDBDatabase::RunTasksResult::kCanBeDestroyed:
        db_.reset();
        metadata_coding_ = nullptr;
        return;
    }
  }

  void RunPostedTasks() { base::RunLoop().RunUntilIdle(); }

 private:
  // Needs to outlive `db_`.
  base::test::TaskEnvironment task_environment_;

 protected:
  std::unique_ptr<IndexedDBFakeBackingStore> backing_store_;
  std::unique_ptr<IndexedDBDatabase> db_;
  raw_ptr<FakeIndexedDBMetadataCoding> metadata_coding_ = nullptr;
  scoped_refptr<MockIndexedDBCallbacks> request_;
  scoped_refptr<MockIndexedDBDatabaseCallbacks> callbacks_;
  raw_ptr<IndexedDBTransaction> transaction_ = nullptr;
  PartitionedLockManager lock_manager_;
  bool error_called_ = false;

  leveldb::Status commit_success_;

 private:
  std::unique_ptr<MockIndexedDBFactory> factory_;
};

TEST_F(IndexedDBDatabaseOperationTest, CreateObjectStore) {
  EXPECT_EQ(0ULL, db_->metadata().object_stores.size());
  const int64_t store_id = 1001;
  leveldb::Status s =
      db_->CreateObjectStoreOperation(store_id, u"store", IndexedDBKeyPath(),
                                      /*auto_increment=*/false, transaction_);
  EXPECT_TRUE(s.ok());
  transaction_->SetCommitFlag();
  RunPostedTasks();
  EXPECT_FALSE(error_called_);
  EXPECT_EQ(1ULL, db_->metadata().object_stores.size());
}

TEST_F(IndexedDBDatabaseOperationTest, CreateIndex) {
  EXPECT_EQ(0ULL, db_->metadata().object_stores.size());
  const int64_t store_id = 1001;
  leveldb::Status s =
      db_->CreateObjectStoreOperation(store_id, u"store", IndexedDBKeyPath(),
                                      /*auto_increment=*/false, transaction_);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(1ULL, db_->metadata().object_stores.size());
  const int64_t index_id = 2002;
  s = db_->CreateIndexOperation(store_id, index_id, u"index",
                                IndexedDBKeyPath(), /*unique=*/false,
                                /*multi_entry=*/false, transaction_);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(
      1ULL,
      db_->metadata().object_stores.find(store_id)->second.indexes.size());
  transaction_->SetCommitFlag();
  RunPostedTasks();
  EXPECT_FALSE(error_called_);
  EXPECT_EQ(1ULL, db_->metadata().object_stores.size());
  EXPECT_EQ(
      1ULL,
      db_->metadata().object_stores.find(store_id)->second.indexes.size());
}

class IndexedDBDatabaseOperationAbortTest
    : public IndexedDBDatabaseOperationTest {
 public:
  IndexedDBDatabaseOperationAbortTest() {
    commit_success_ = leveldb::Status::NotFound("Bummer.");
  }

  IndexedDBDatabaseOperationAbortTest(
      const IndexedDBDatabaseOperationAbortTest&) = delete;
  IndexedDBDatabaseOperationAbortTest& operator=(
      const IndexedDBDatabaseOperationAbortTest&) = delete;
};

TEST_F(IndexedDBDatabaseOperationAbortTest, CreateObjectStore) {
  EXPECT_EQ(0ULL, db_->metadata().object_stores.size());
  const int64_t store_id = 1001;
  leveldb::Status s =
      db_->CreateObjectStoreOperation(store_id, u"store", IndexedDBKeyPath(),
                                      /*auto_increment=*/false, transaction_);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(1ULL, db_->metadata().object_stores.size());
  transaction_->SetCommitFlag();
  RunPostedTasks();
  EXPECT_EQ(0ULL, db_->metadata().object_stores.size());
}

TEST_F(IndexedDBDatabaseOperationAbortTest, CreateIndex) {
  EXPECT_EQ(0ULL, db_->metadata().object_stores.size());
  const int64_t store_id = 1001;
  leveldb::Status s =
      db_->CreateObjectStoreOperation(store_id, u"store", IndexedDBKeyPath(),
                                      /*auto_increment=*/false, transaction_);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(1ULL, db_->metadata().object_stores.size());
  const int64_t index_id = 2002;
  s = db_->CreateIndexOperation(store_id, index_id, u"index",
                                IndexedDBKeyPath(), /*unique=*/false,
                                /*multi_entry=*/false, transaction_);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(
      1ULL,
      db_->metadata().object_stores.find(store_id)->second.indexes.size());
  transaction_->SetCommitFlag();
  RunPostedTasks();
  EXPECT_TRUE(error_called_);
  EXPECT_EQ(0ULL, db_->metadata().object_stores.size());
}

TEST_F(IndexedDBDatabaseOperationTest, CreatePutDelete) {
  EXPECT_EQ(0ULL, db_->metadata().object_stores.size());
  const int64_t store_id = 1001;

  leveldb::Status s =
      db_->CreateObjectStoreOperation(store_id, u"store", IndexedDBKeyPath(),
                                      /*auto_increment=*/false, transaction_);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(1ULL, db_->metadata().object_stores.size());

  IndexedDBValue value("value1", {});
  std::unique_ptr<IndexedDBKey> key(std::make_unique<IndexedDBKey>("key"));
  std::vector<IndexedDBIndexKeys> index_keys;
  base::MockCallback<blink::mojom::IDBTransaction::PutCallback> callback;

  // Set in-flight memory to a reasonably large number to prevent underflow in
  // `PutOperation`
  transaction_->in_flight_memory() += 1000;

  auto put_params = std::make_unique<IndexedDBDatabase::PutOperationParams>();
  put_params->object_store_id = store_id;
  put_params->value = value;
  put_params->key = std::move(key);
  put_params->put_mode = blink::mojom::IDBPutMode::AddOnly;
  put_params->callback = callback.Get();
  put_params->index_keys = index_keys;
  s = db_->PutOperation(std::move(put_params), transaction_);
  EXPECT_TRUE(s.ok());

  s = db_->DeleteObjectStoreOperation(store_id, transaction_);
  EXPECT_TRUE(s.ok());

  EXPECT_EQ(0ULL, db_->metadata().object_stores.size());

  transaction_->SetCommitFlag();
  RunPostedTasks();
  EXPECT_FALSE(error_called_);
  EXPECT_TRUE(s.ok());
}

}  // namespace content
