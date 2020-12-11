// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_database.h"

#include <stdint.h>
#include <set>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/services/storage/indexed_db/scopes/disjoint_range_lock_manager.h"
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

using base::ASCIIToUTF16;
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
  IndexedDBDatabaseTest() : lock_manager_(kIndexedDBLockLevelCount) {}

  void SetUp() override {
    backing_store_ = std::make_unique<IndexedDBFakeBackingStore>();
    factory_ = std::make_unique<MockIndexedDBFactory>();
    std::unique_ptr<FakeIndexedDBMetadataCoding> metadata_coding =
        std::make_unique<FakeIndexedDBMetadataCoding>();
    metadata_coding_ = metadata_coding.get();
    leveldb::Status s;

    std::tie(db_, s) = IndexedDBClassFactory::Get()->CreateIndexedDBDatabase(
        ASCIIToUTF16("db"), backing_store_.get(), factory_.get(),
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
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&IndexedDBDatabaseTest::RunTasksForDatabase,
                                    weak_factory_.GetWeakPtr(), false));
      return;
    }
    IndexedDBDatabase::RunTasksResult result;
    leveldb::Status status;
    std::tie(result, status) = db_->RunTasks();
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
  FakeIndexedDBMetadataCoding* metadata_coding_ = nullptr;
  bool error_called_ = false;

 private:
  base::test::TaskEnvironment task_environment_;
  DisjointRangeLockManager lock_manager_;

  base::WeakPtrFactory<IndexedDBDatabaseTest> weak_factory_{this};
};

TEST_F(IndexedDBDatabaseTest, ConnectionLifecycle) {
  scoped_refptr<MockIndexedDBCallbacks> request1(new MockIndexedDBCallbacks());
  scoped_refptr<MockIndexedDBDatabaseCallbacks> callbacks1(
      new MockIndexedDBDatabaseCallbacks());
  const int64_t transaction_id1 = 1;
  auto create_transaction_callback1 =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  std::unique_ptr<IndexedDBPendingConnection> connection1(
      std::make_unique<IndexedDBPendingConnection>(
          request1, callbacks1,
          transaction_id1, IndexedDBDatabaseMetadata::DEFAULT_VERSION,
          std::move(create_transaction_callback1)));
  db_->ScheduleOpenConnection(IndexedDBOriginStateHandle(),
                              std::move(connection1));
  RunPostedTasks();

  scoped_refptr<MockIndexedDBCallbacks> request2(new MockIndexedDBCallbacks());
  scoped_refptr<MockIndexedDBDatabaseCallbacks> callbacks2(
      new MockIndexedDBDatabaseCallbacks());
  const int64_t transaction_id2 = 2;
  auto create_transaction_callback2 =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  std::unique_ptr<IndexedDBPendingConnection> connection2(
      std::make_unique<IndexedDBPendingConnection>(
          request2, callbacks2,
          transaction_id2, IndexedDBDatabaseMetadata::DEFAULT_VERSION,
          std::move(create_transaction_callback2)));
  db_->ScheduleOpenConnection(IndexedDBOriginStateHandle(),
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
  scoped_refptr<MockIndexedDBDatabaseCallbacks> callbacks(
      new MockIndexedDBDatabaseCallbacks());
  scoped_refptr<MockIndexedDBCallbacks> request(new MockIndexedDBCallbacks());
  const int64_t upgrade_transaction_id = 3;
  auto create_transaction_callback =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  std::unique_ptr<IndexedDBPendingConnection> connection(
      std::make_unique<IndexedDBPendingConnection>(
          request, callbacks,
          upgrade_transaction_id, IndexedDBDatabaseMetadata::DEFAULT_VERSION,
          std::move(create_transaction_callback)));
  db_->ScheduleOpenConnection(IndexedDBOriginStateHandle(),
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

class MockCallbacks : public IndexedDBCallbacks {
 public:
  MockCallbacks()
      : IndexedDBCallbacks(nullptr,
                           url::Origin(),
                           mojo::NullAssociatedRemote(),
                           base::ThreadTaskRunnerHandle::Get()) {}

  void OnBlocked(int64_t existing_version) override { blocked_called_ = true; }
  void OnSuccess(int64_t result) override { success_called_ = true; }
  void OnError(const IndexedDBDatabaseError& error) override {
    error_called_ = true;
  }

  bool blocked_called() const { return blocked_called_; }
  bool success_called() const { return success_called_; }
  bool error_called() const { return error_called_; }

 private:
  ~MockCallbacks() override {}

  bool blocked_called_ = false;
  bool success_called_ = false;
  bool error_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(MockCallbacks);
};

TEST_F(IndexedDBDatabaseTest, PendingDelete) {
  scoped_refptr<MockIndexedDBCallbacks> request1(new MockIndexedDBCallbacks());
  scoped_refptr<MockIndexedDBDatabaseCallbacks> callbacks1(
      new MockIndexedDBDatabaseCallbacks());
  const int64_t transaction_id1 = 1;
  auto create_transaction_callback1 =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  std::unique_ptr<IndexedDBPendingConnection> connection(
      std::make_unique<IndexedDBPendingConnection>(
          request1, callbacks1,
          transaction_id1, IndexedDBDatabaseMetadata::DEFAULT_VERSION,
          std::move(create_transaction_callback1)));
  db_->ScheduleOpenConnection(IndexedDBOriginStateHandle(),
                              std::move(connection));
  RunPostedTasks();

  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 0UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 0UL);

  bool deleted = false;
  scoped_refptr<MockCallbacks> request2(new MockCallbacks());
  db_->ScheduleDeleteDatabase(
      IndexedDBOriginStateHandle(), request2,
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

  scoped_refptr<MockIndexedDBCallbacks> request1(
      new MockIndexedDBCallbacks(true));
  scoped_refptr<MockIndexedDBDatabaseCallbacks> callbacks1(
      new MockIndexedDBDatabaseCallbacks());
  const int64_t transaction_id1 = 1;
  auto create_transaction_callback1 =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  std::unique_ptr<IndexedDBPendingConnection> connection1(
      std::make_unique<IndexedDBPendingConnection>(
          request1, callbacks1,
          transaction_id1, kDatabaseVersion,
          std::move(create_transaction_callback1)));
  db_->ScheduleOpenConnection(IndexedDBOriginStateHandle(),
                              std::move(connection1));
  RunPostedTasks();

  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 1UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 0UL);

  scoped_refptr<MockIndexedDBCallbacks> request2(
      new MockIndexedDBCallbacks(false));
  scoped_refptr<MockIndexedDBDatabaseCallbacks> callbacks2(
      new MockIndexedDBDatabaseCallbacks());
  const int64_t transaction_id2 = 2;
  auto create_transaction_callback2 =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  std::unique_ptr<IndexedDBPendingConnection> connection2(
      std::make_unique<IndexedDBPendingConnection>(
          request2, callbacks2,
          transaction_id2, kDatabaseVersion,
          std::move(create_transaction_callback2)));
  db_->ScheduleOpenConnection(IndexedDBOriginStateHandle(),
                              std::move(connection2));
  RunPostedTasks();

  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 1UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 1UL);

  scoped_refptr<MockIndexedDBCallbacks> request3(
      new MockIndexedDBCallbacks(false));
  scoped_refptr<MockIndexedDBDatabaseCallbacks> callbacks3(
      new MockIndexedDBDatabaseCallbacks());
  const int64_t transaction_id3 = 3;
  auto create_transaction_callback3 =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  std::unique_ptr<IndexedDBPendingConnection> connection3(
      std::make_unique<IndexedDBPendingConnection>(
          request3, callbacks3,
          transaction_id3, kDatabaseVersion,
          std::move(create_transaction_callback3)));
  db_->ScheduleOpenConnection(IndexedDBOriginStateHandle(),
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
  scoped_refptr<MockIndexedDBCallbacks> request1(new MockIndexedDBCallbacks());
  scoped_refptr<MockIndexedDBDatabaseCallbacks> callbacks1(
      new MockIndexedDBDatabaseCallbacks());
  const int64_t transaction_id1 = 1;
  auto create_transaction_callback1 =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  std::unique_ptr<IndexedDBPendingConnection> connection(
      std::make_unique<IndexedDBPendingConnection>(
          request1, callbacks1,
          transaction_id1, IndexedDBDatabaseMetadata::DEFAULT_VERSION,
          std::move(create_transaction_callback1)));
  db_->ScheduleOpenConnection(IndexedDBOriginStateHandle(),
                              std::move(connection));
  RunPostedTasks();

  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 0UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 0UL);

  bool deleted = false;
  scoped_refptr<MockCallbacks> request2(new MockCallbacks());
  db_->ScheduleDeleteDatabase(
      IndexedDBOriginStateHandle(), request2,
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
  scoped_refptr<MockIndexedDBCallbacks> request1(new MockIndexedDBCallbacks());
  scoped_refptr<MockIndexedDBDatabaseCallbacks> callbacks1(
      new MockIndexedDBDatabaseCallbacks());
  const int64_t transaction_id1 = 1;
  auto create_transaction_callback1 =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  std::unique_ptr<IndexedDBPendingConnection> connection(
      std::make_unique<IndexedDBPendingConnection>(
          request1, callbacks1,
          transaction_id1, IndexedDBDatabaseMetadata::DEFAULT_VERSION,
          std::move(create_transaction_callback1)));
  db_->ScheduleOpenConnection(IndexedDBOriginStateHandle(),
                              std::move(connection));
  RunPostedTasks();

  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 0UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 0UL);

  scoped_refptr<MockIndexedDBCallbacks> request2(
      new MockIndexedDBCallbacks(false));
  scoped_refptr<MockIndexedDBDatabaseCallbacks> callbacks2(
      new MockIndexedDBDatabaseCallbacks());
  const int64_t transaction_id2 = 2;
  auto create_transaction_callback2 =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  std::unique_ptr<IndexedDBPendingConnection> connection2(
      std::make_unique<IndexedDBPendingConnection>(
          request1, callbacks1,
          transaction_id2, 3, std::move(create_transaction_callback2)));
  db_->ScheduleOpenConnection(IndexedDBOriginStateHandle(),
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
  std::unique_ptr<IndexedDBPendingConnection> connection = std::make_unique<
      IndexedDBPendingConnection>(
      request1, callbacks1,
      transaction_id1, IndexedDBDatabaseMetadata::DEFAULT_VERSION,
      std::move(create_transaction_callback1));
  db_->ScheduleOpenConnection(IndexedDBOriginStateHandle(),
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
  std::unique_ptr<IndexedDBPendingConnection> connection2(
      std::make_unique<IndexedDBPendingConnection>(
          request1, callbacks1,
          transaction_id2, 3, std::move(create_transaction_callback2)));
  db_->ScheduleOpenConnection(IndexedDBOriginStateHandle(),
                              std::move(connection2));
  RunPostedTasks();

  bool deleted = false;
  auto request3 = base::MakeRefCounted<MockCallbacks>();
  db_->ScheduleDeleteDatabase(
      IndexedDBOriginStateHandle(), request3,
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
      : lock_manager_(kIndexedDBLockLevelCount),
        commit_success_(leveldb::Status::OK()),
        factory_(new MockIndexedDBFactory()) {}

  void SetUp() override {
    backing_store_ = std::make_unique<IndexedDBFakeBackingStore>();
    std::unique_ptr<FakeIndexedDBMetadataCoding> metadata_coding =
        std::make_unique<FakeIndexedDBMetadataCoding>();
    metadata_coding_ = metadata_coding.get();
    leveldb::Status s;
    std::tie(db_, s) = IndexedDBClassFactory::Get()->CreateIndexedDBDatabase(
        ASCIIToUTF16("db"), backing_store_.get(), factory_.get(),
        base::BindRepeating(
            &IndexedDBDatabaseOperationTest::RunTasksForDatabase,
            base::Unretained(this), true),
        std::move(metadata_coding), IndexedDBDatabase::Identifier(),
        &lock_manager_);
    ASSERT_TRUE(s.ok());

    request_ = new MockIndexedDBCallbacks();
    callbacks_ = new MockIndexedDBDatabaseCallbacks();
    const int64_t transaction_id = 1;
    auto create_transaction_callback1 =
        base::BindOnce(&CreateAndBindTransactionPlaceholder);
    std::unique_ptr<IndexedDBPendingConnection> connection(
        std::make_unique<IndexedDBPendingConnection>(
            request_, callbacks_,
            transaction_id, IndexedDBDatabaseMetadata::DEFAULT_VERSION,
            std::move(create_transaction_callback1)));
    db_->ScheduleOpenConnection(IndexedDBOriginStateHandle(),
                                std::move(connection));
    RunPostedTasks();
    EXPECT_EQ(IndexedDBDatabaseMetadata::NO_VERSION, db_->metadata().version);

    EXPECT_TRUE(request_->connection());
    transaction_ = request_->connection()->CreateTransaction(
        transaction_id, std::set<int64_t>() /*scope*/,
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
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(&IndexedDBDatabaseOperationTest::RunTasksForDatabase,
                         base::Unretained(this), false));
      return;
    }
    IndexedDBDatabase::RunTasksResult result;
    leveldb::Status status;
    std::tie(result, status) = db_->RunTasks();
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
  // Needs to outlive |db_|.
  base::test::TaskEnvironment task_environment_;

 protected:
  std::unique_ptr<IndexedDBFakeBackingStore> backing_store_;
  std::unique_ptr<IndexedDBDatabase> db_;
  FakeIndexedDBMetadataCoding* metadata_coding_ = nullptr;
  scoped_refptr<MockIndexedDBCallbacks> request_;
  scoped_refptr<MockIndexedDBDatabaseCallbacks> callbacks_;
  IndexedDBTransaction* transaction_ = nullptr;
  DisjointRangeLockManager lock_manager_;
  bool error_called_ = false;

  leveldb::Status commit_success_;

 private:
  std::unique_ptr<MockIndexedDBFactory> factory_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBDatabaseOperationTest);
};

TEST_F(IndexedDBDatabaseOperationTest, CreateObjectStore) {
  EXPECT_EQ(0ULL, db_->metadata().object_stores.size());
  const int64_t store_id = 1001;
  leveldb::Status s = db_->CreateObjectStoreOperation(
      store_id, ASCIIToUTF16("store"), IndexedDBKeyPath(),
      false /*auto_increment*/, transaction_);
  EXPECT_TRUE(s.ok());
  transaction_->SetCommitFlag();
  RunPostedTasks();
  EXPECT_FALSE(error_called_);
  EXPECT_EQ(1ULL, db_->metadata().object_stores.size());
}

TEST_F(IndexedDBDatabaseOperationTest, CreateIndex) {
  EXPECT_EQ(0ULL, db_->metadata().object_stores.size());
  const int64_t store_id = 1001;
  leveldb::Status s = db_->CreateObjectStoreOperation(
      store_id, ASCIIToUTF16("store"), IndexedDBKeyPath(),
      false /*auto_increment*/, transaction_);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(1ULL, db_->metadata().object_stores.size());
  const int64_t index_id = 2002;
  s = db_->CreateIndexOperation(store_id, index_id, ASCIIToUTF16("index"),
                                IndexedDBKeyPath(), false /*unique*/,
                                false /*multi_entry*/, transaction_);
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

 private:
  DISALLOW_COPY_AND_ASSIGN(IndexedDBDatabaseOperationAbortTest);
};

TEST_F(IndexedDBDatabaseOperationAbortTest, CreateObjectStore) {
  EXPECT_EQ(0ULL, db_->metadata().object_stores.size());
  const int64_t store_id = 1001;
  leveldb::Status s = db_->CreateObjectStoreOperation(
      store_id, ASCIIToUTF16("store"), IndexedDBKeyPath(),
      false /*auto_increment*/, transaction_);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(1ULL, db_->metadata().object_stores.size());
  transaction_->SetCommitFlag();
  RunPostedTasks();
  EXPECT_EQ(0ULL, db_->metadata().object_stores.size());
}

TEST_F(IndexedDBDatabaseOperationAbortTest, CreateIndex) {
  EXPECT_EQ(0ULL, db_->metadata().object_stores.size());
  const int64_t store_id = 1001;
  leveldb::Status s = db_->CreateObjectStoreOperation(
      store_id, ASCIIToUTF16("store"), IndexedDBKeyPath(),
      false /*auto_increment*/, transaction_);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(1ULL, db_->metadata().object_stores.size());
  const int64_t index_id = 2002;
  s = db_->CreateIndexOperation(store_id, index_id, ASCIIToUTF16("index"),
                                IndexedDBKeyPath(), false /*unique*/,
                                false /*multi_entry*/, transaction_);
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

  leveldb::Status s = db_->CreateObjectStoreOperation(
      store_id, ASCIIToUTF16("store"), IndexedDBKeyPath(),
      false /*auto_increment*/, transaction_);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(1ULL, db_->metadata().object_stores.size());

  IndexedDBValue value("value1", {});
  std::unique_ptr<IndexedDBKey> key(std::make_unique<IndexedDBKey>("key"));
  std::vector<IndexedDBIndexKeys> index_keys;
  base::MockCallback<blink::mojom::IDBTransaction::PutCallback> callback;

  // Set in-flight memory to a reasonably large number to prevent underflow in
  // |PutOperation|
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
