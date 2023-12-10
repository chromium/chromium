// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_database.h"

#include <stdint.h>
#include <set>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"
#include "components/services/storage/privileged/mojom/indexed_db_client_state_checker.mojom-forward.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/browser/indexed_db/indexed_db.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_bucket_context.h"
#include "content/browser/indexed_db/indexed_db_class_factory.h"
#include "content/browser/indexed_db/indexed_db_client_state_checker_wrapper.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_cursor.h"
#include "content/browser/indexed_db/indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/indexed_db_factory.h"
#include "content/browser/indexed_db/indexed_db_factory_client.h"
#include "content/browser/indexed_db/indexed_db_fake_backing_store.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "content/browser/indexed_db/mock_indexed_db_factory_client.h"
#include "content/browser/indexed_db/mock_mojo_indexed_db_database_callbacks.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

using blink::IndexedDBDatabaseMetadata;
using blink::IndexedDBIndexKeys;
using blink::IndexedDBKey;
using blink::IndexedDBKeyPath;

namespace content {

class IndexedDBDatabaseTest : public ::testing::Test {
 public:
  IndexedDBDatabaseTest() = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
        /*is_incognito=*/false, temp_dir_.GetPath(),
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        /*special_storage_policy=*/nullptr);

    quota_manager_proxy_ = base::MakeRefCounted<storage::MockQuotaManagerProxy>(
        quota_manager_.get(),
        base::SingleThreadTaskRunner::GetCurrentDefault().get());

    indexed_db_context_ = std::make_unique<IndexedDBContextImpl>(
        temp_dir_.GetPath(), quota_manager_proxy_,
        /*blob_storage_context=*/mojo::NullRemote(),
        /*file_system_access_context=*/mojo::NullRemote(),
        base::SequencedTaskRunner::GetCurrentDefault(),
        base::SequencedTaskRunner::GetCurrentDefault());

    IndexedDBBucketContext::Delegate delegate;
    delegate.on_fatal_error = base::BindRepeating(
        &IndexedDBDatabaseTest::OnFatalError, weak_factory_.GetWeakPtr());
    delegate.on_ready_for_destruction = base::BindRepeating(
        &IndexedDBDatabaseTest::OnBucketContextReadyForDestruction,
        weak_factory_.GetWeakPtr());

    bucket_context_ = std::make_unique<IndexedDBBucketContext>(
        storage::BucketInfo(), std::make_unique<PartitionedLockManager>(),
        std::move(delegate), std::make_unique<IndexedDBFakeBackingStore>(),
        quota_manager_proxy_,
        /*io_task_runner=*/base::SequencedTaskRunner::GetCurrentDefault(),
        /*blob_storage_context=*/mojo::NullRemote(),
        /*file_system_access_context=*/mojo::NullRemote(), base::DoNothing());

    db_ = bucket_context_->AddDatabase(
        u"db", std::make_unique<IndexedDBDatabase>(
                   u"db", *bucket_context_, IndexedDBDatabase::Identifier()));
  }

  void TearDown() override { db_ = nullptr; }

  void OnFatalError(leveldb::Status s) { error_called_ = true; }

  void OnBucketContextReadyForDestruction() { bucket_context_.reset(); }

  void RunPostedTasks() {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  scoped_refptr<IndexedDBClientStateCheckerWrapper>
  CreateTestClientStateWrapper() {
    mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker> remote;
    return base::MakeRefCounted<IndexedDBClientStateCheckerWrapper>(
        std::move(remote));
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<IndexedDBContextImpl> indexed_db_context_;
  std::unique_ptr<IndexedDBBucketContext> bucket_context_;
  scoped_refptr<storage::MockQuotaManager> quota_manager_;
  scoped_refptr<storage::MockQuotaManagerProxy> quota_manager_proxy_;

  // As `this` is owned by `bucket_context_`, tests that cause the database to
  // be destroyed must manually reset this to null to avoid triggering dangling
  // pointer warnings.
  raw_ptr<IndexedDBDatabase> db_ = nullptr;

  bool error_called_ = false;

  base::WeakPtrFactory<IndexedDBDatabaseTest> weak_factory_{this};
};

TEST_F(IndexedDBDatabaseTest, ConnectionLifecycle) {
  MockMojoIndexedDBDatabaseCallbacks database_callbacks;
  MockIndexedDBFactoryClient request1;
  const int64_t transaction_id1 = 1;
  auto connection1 = std::make_unique<IndexedDBPendingConnection>(
      std::make_unique<ThunkFactoryClient>(request1),
      std::make_unique<IndexedDBDatabaseCallbacks>(
          database_callbacks.BindNewEndpointAndPassDedicatedRemote()),
      transaction_id1, IndexedDBDatabaseMetadata::DEFAULT_VERSION,
      mojo::NullAssociatedReceiver());
  db_->ScheduleOpenConnection(std::move(connection1),
                              CreateTestClientStateWrapper());
  RunPostedTasks();

  MockMojoIndexedDBDatabaseCallbacks database_callbacks2;
  MockIndexedDBFactoryClient request2;
  const int64_t transaction_id2 = 2;
  auto connection2 = std::make_unique<IndexedDBPendingConnection>(
      std::make_unique<ThunkFactoryClient>(request2),
      std::make_unique<IndexedDBDatabaseCallbacks>(
          database_callbacks2.BindNewEndpointAndPassDedicatedRemote()),
      transaction_id2, IndexedDBDatabaseMetadata::DEFAULT_VERSION,
      mojo::NullAssociatedReceiver());
  db_->ScheduleOpenConnection(std::move(connection2),
                              CreateTestClientStateWrapper());
  RunPostedTasks();
  db_ = nullptr;

  EXPECT_TRUE(request1.connection());
  request1.connection()->CloseAndReportForceClose();
  EXPECT_FALSE(request1.connection()->IsConnected());

  EXPECT_TRUE(request2.connection());
  request2.connection()->CloseAndReportForceClose();
  EXPECT_FALSE(request2.connection()->IsConnected());

  RunPostedTasks();

  EXPECT_TRUE(bucket_context_->databases().empty());
}

TEST_F(IndexedDBDatabaseTest, ForcedClose) {
  MockMojoIndexedDBDatabaseCallbacks database_callbacks;
  MockIndexedDBFactoryClient request;
  const int64_t upgrade_transaction_id = 3;
  auto connection = std::make_unique<IndexedDBPendingConnection>(
      std::make_unique<ThunkFactoryClient>(request),
      std::make_unique<IndexedDBDatabaseCallbacks>(
          database_callbacks.BindNewEndpointAndPassDedicatedRemote()),
      upgrade_transaction_id, IndexedDBDatabaseMetadata::DEFAULT_VERSION,
      mojo::NullAssociatedReceiver());
  db_->ScheduleOpenConnection(std::move(connection),
                              CreateTestClientStateWrapper());
  RunPostedTasks();

  EXPECT_EQ(db_, request.connection()->database().get());

  const int64_t transaction_id = 123;
  const std::vector<int64_t> scope;
  IndexedDBTransaction* transaction = request.connection()->CreateTransaction(
      mojo::NullAssociatedReceiver(), transaction_id,
      std::set<int64_t>(scope.begin(), scope.end()),
      blink::mojom::IDBTransactionMode::ReadOnly,
      new IndexedDBBackingStore::Transaction(
          bucket_context_->backing_store()->AsWeakPtr(),
          blink::mojom::IDBTransactionDurability::Relaxed,
          blink::mojom::IDBTransactionMode::ReadWrite));
  db_->RegisterAndScheduleTransaction(transaction);
  db_ = nullptr;

  base::RunLoop run_loop;
  EXPECT_CALL(database_callbacks, ForcedClose)
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  request.connection()->CloseAndReportForceClose();
  run_loop.Run();
}

namespace {

class MockFactoryClient : public IndexedDBFactoryClient {
 public:
  MockFactoryClient() : IndexedDBFactoryClient(mojo::NullAssociatedRemote()) {}
  ~MockFactoryClient() override = default;

  MockFactoryClient(const MockFactoryClient&) = delete;
  MockFactoryClient& operator=(const MockFactoryClient&) = delete;

  void OnBlocked(int64_t existing_version) override { blocked_called_ = true; }
  void OnDeleteSuccess(int64_t old_version) override { success_called_ = true; }
  void OnError(const IndexedDBDatabaseError& error) override {
    error_called_ = true;
  }

  bool blocked_called() const { return blocked_called_; }
  bool success_called() const { return success_called_; }
  bool error_called() const { return error_called_; }

 private:
  bool blocked_called_ = false;
  bool success_called_ = false;
  bool error_called_ = false;
};

}  // namespace

TEST_F(IndexedDBDatabaseTest, PendingDelete) {
  MockIndexedDBFactoryClient request1;
  const int64_t transaction_id1 = 1;
  MockMojoIndexedDBDatabaseCallbacks database_callbacks1;
  auto connection = std::make_unique<IndexedDBPendingConnection>(
      std::make_unique<ThunkFactoryClient>(request1),
      std::make_unique<IndexedDBDatabaseCallbacks>(
          database_callbacks1.BindNewEndpointAndPassDedicatedRemote()),
      transaction_id1, IndexedDBDatabaseMetadata::DEFAULT_VERSION,
      mojo::NullAssociatedReceiver());
  db_->ScheduleOpenConnection(std::move(connection),
                              CreateTestClientStateWrapper());
  RunPostedTasks();

  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 0UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 0UL);

  bool deleted = false;
  MockFactoryClient request2;
  db_->ScheduleDeleteDatabase(
      std::make_unique<ThunkFactoryClient>(request2),
      base::BindLambdaForTesting([&]() { deleted = true; }));
  RunPostedTasks();
  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 1UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 0UL);

  EXPECT_FALSE(request2.blocked_called());
  request1.connection()->VersionChangeIgnored();
  EXPECT_TRUE(request2.blocked_called());
  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 1UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 0UL);

  db_->ForceCloseAndRunTasks();
  db_ = nullptr;

  RunPostedTasks();
  EXPECT_FALSE(db_);

  EXPECT_TRUE(deleted);
  EXPECT_TRUE(request2.success_called());
}

TEST_F(IndexedDBDatabaseTest, OpenDeleteClear) {
  const int64_t kDatabaseVersion = 1;

  MockIndexedDBFactoryClient request1(
      /*expect_connection=*/true);
  MockMojoIndexedDBDatabaseCallbacks database_callbacks1;
  const int64_t transaction_id1 = 1;
  auto connection1 = std::make_unique<IndexedDBPendingConnection>(
      std::make_unique<ThunkFactoryClient>(request1),
      std::make_unique<IndexedDBDatabaseCallbacks>(
          database_callbacks1.BindNewEndpointAndPassDedicatedRemote()),
      transaction_id1, kDatabaseVersion, mojo::NullAssociatedReceiver());
  db_->ScheduleOpenConnection(std::move(connection1),
                              CreateTestClientStateWrapper());
  RunPostedTasks();

  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 1UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 0UL);

  MockIndexedDBFactoryClient request2(
      /*expect_connection=*/false);
  MockMojoIndexedDBDatabaseCallbacks database_callbacks2;
  const int64_t transaction_id2 = 2;
  auto connection2 = std::make_unique<IndexedDBPendingConnection>(
      std::make_unique<ThunkFactoryClient>(request2),
      std::make_unique<IndexedDBDatabaseCallbacks>(
          database_callbacks2.BindNewEndpointAndPassDedicatedRemote()),
      transaction_id2, kDatabaseVersion, mojo::NullAssociatedReceiver());
  db_->ScheduleOpenConnection(std::move(connection2),
                              CreateTestClientStateWrapper());
  RunPostedTasks();

  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 1UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 1UL);

  MockIndexedDBFactoryClient request3(
      /*expect_connection=*/false);
  MockMojoIndexedDBDatabaseCallbacks database_callbacks3;
  const int64_t transaction_id3 = 3;
  auto connection3 = std::make_unique<IndexedDBPendingConnection>(
      std::make_unique<ThunkFactoryClient>(request3),
      std::make_unique<IndexedDBDatabaseCallbacks>(
          database_callbacks3.BindNewEndpointAndPassDedicatedRemote()),
      transaction_id3, kDatabaseVersion, mojo::NullAssociatedReceiver());
  db_->ScheduleOpenConnection(std::move(connection3),
                              CreateTestClientStateWrapper());
  RunPostedTasks();

  EXPECT_TRUE(request1.upgrade_called());

  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 1UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 2UL);

  EXPECT_CALL(database_callbacks1, ForcedClose);
  EXPECT_CALL(database_callbacks2, ForcedClose);
  EXPECT_CALL(database_callbacks3, ForcedClose);

  db_->ForceCloseAndRunTasks();
  db_ = nullptr;
  database_callbacks1.FlushForTesting();

  EXPECT_TRUE(request1.error_called());
  EXPECT_TRUE(request2.error_called());
  EXPECT_TRUE(request3.error_called());
}

TEST_F(IndexedDBDatabaseTest, ForceDelete) {
  MockIndexedDBFactoryClient request1;
  MockMojoIndexedDBDatabaseCallbacks database_callbacks;
  const int64_t transaction_id1 = 1;
  auto connection = std::make_unique<IndexedDBPendingConnection>(
      std::make_unique<ThunkFactoryClient>(request1),
      std::make_unique<IndexedDBDatabaseCallbacks>(
          database_callbacks.BindNewEndpointAndPassDedicatedRemote()),
      transaction_id1, IndexedDBDatabaseMetadata::DEFAULT_VERSION,
      mojo::NullAssociatedReceiver());
  db_->ScheduleOpenConnection(std::move(connection),
                              CreateTestClientStateWrapper());
  RunPostedTasks();

  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 0UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 0UL);

  bool deleted = false;
  MockFactoryClient request2;
  db_->ScheduleDeleteDatabase(
      std::make_unique<ThunkFactoryClient>(request2),
      base::BindLambdaForTesting([&]() { deleted = true; }));
  RunPostedTasks();
  EXPECT_FALSE(deleted);
  db_->ForceCloseAndRunTasks();
  db_ = nullptr;
  RunPostedTasks();
  EXPECT_FALSE(db_);
  EXPECT_TRUE(deleted);
  EXPECT_FALSE(request2.blocked_called());
  EXPECT_TRUE(request2.success_called());
}

TEST_F(IndexedDBDatabaseTest, ForceCloseWhileOpenPending) {
  // Verify that pending connection requests are handled correctly during a
  // ForceClose.
  MockIndexedDBFactoryClient request1;
  MockMojoIndexedDBDatabaseCallbacks database_callbacks1;
  const int64_t transaction_id1 = 1;
  auto connection1 = std::make_unique<IndexedDBPendingConnection>(
      std::make_unique<ThunkFactoryClient>(request1),
      std::make_unique<IndexedDBDatabaseCallbacks>(
          database_callbacks1.BindNewEndpointAndPassDedicatedRemote()),
      transaction_id1, IndexedDBDatabaseMetadata::DEFAULT_VERSION,
      mojo::NullAssociatedReceiver());
  db_->ScheduleOpenConnection(std::move(connection1),
                              CreateTestClientStateWrapper());
  RunPostedTasks();

  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 0UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 0UL);

  MockIndexedDBFactoryClient request2(
      /*expect_connection=*/false);
  MockMojoIndexedDBDatabaseCallbacks database_callbacks2;

  const int64_t transaction_id2 = 2;
  auto connection2 = std::make_unique<IndexedDBPendingConnection>(
      std::make_unique<ThunkFactoryClient>(request2),
      std::make_unique<IndexedDBDatabaseCallbacks>(
          database_callbacks2.BindNewEndpointAndPassDedicatedRemote()),
      transaction_id2, 3, mojo::NullAssociatedReceiver());
  db_->ScheduleOpenConnection(std::move(connection2),
                              CreateTestClientStateWrapper());
  RunPostedTasks();

  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 1UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 0UL);

  db_->ForceCloseAndRunTasks();
  db_ = nullptr;
  RunPostedTasks();
  EXPECT_FALSE(db_);
}

TEST_F(IndexedDBDatabaseTest, ForceCloseWhileOpenAndDeletePending) {
  // Verify that pending connection requests are handled correctly during a
  // ForceClose.
  MockIndexedDBFactoryClient request1;
  MockMojoIndexedDBDatabaseCallbacks database_callbacks1;
  const int64_t transaction_id1 = 1;
  auto connection1 = std::make_unique<IndexedDBPendingConnection>(
      std::make_unique<ThunkFactoryClient>(request1),
      std::make_unique<IndexedDBDatabaseCallbacks>(
          database_callbacks1.BindNewEndpointAndPassDedicatedRemote()),
      transaction_id1, IndexedDBDatabaseMetadata::DEFAULT_VERSION,
      mojo::NullAssociatedReceiver());
  db_->ScheduleOpenConnection(std::move(connection1),
                              CreateTestClientStateWrapper());
  RunPostedTasks();

  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 0UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 0UL);

  MockIndexedDBFactoryClient request2(false);
  MockMojoIndexedDBDatabaseCallbacks database_callbacks2;
  const int64_t transaction_id2 = 2;
  auto connection2 = std::make_unique<IndexedDBPendingConnection>(
      std::make_unique<ThunkFactoryClient>(request2),
      std::make_unique<IndexedDBDatabaseCallbacks>(
          database_callbacks2.BindNewEndpointAndPassDedicatedRemote()),
      transaction_id2, 3, mojo::NullAssociatedReceiver());
  db_->ScheduleOpenConnection(std::move(connection2),
                              CreateTestClientStateWrapper());
  RunPostedTasks();

  bool deleted = false;
  auto request3 = std::make_unique<MockFactoryClient>();
  db_->ScheduleDeleteDatabase(
      std::move(request3),
      base::BindLambdaForTesting([&]() { deleted = true; }));
  RunPostedTasks();
  EXPECT_FALSE(deleted);

  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 1UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 1UL);

  db_->ForceCloseAndRunTasks();
  db_ = nullptr;
  RunPostedTasks();
  EXPECT_TRUE(deleted);
}

leveldb::Status DummyOperation(IndexedDBTransaction* transaction) {
  return leveldb::Status::OK();
}

class IndexedDBDatabaseOperationTest : public IndexedDBDatabaseTest {
 public:
  IndexedDBDatabaseOperationTest() : commit_success_(leveldb::Status::OK()) {}

  IndexedDBDatabaseOperationTest(const IndexedDBDatabaseOperationTest&) =
      delete;
  IndexedDBDatabaseOperationTest& operator=(
      const IndexedDBDatabaseOperationTest&) = delete;

  void SetUp() override {
    IndexedDBDatabaseTest::SetUp();

    const int64_t transaction_id = 1;
    auto connection = std::make_unique<IndexedDBPendingConnection>(
        std::make_unique<ThunkFactoryClient>(request_),
        std::make_unique<IndexedDBDatabaseCallbacks>(
            mojo::NullAssociatedRemote()),
        transaction_id, IndexedDBDatabaseMetadata::DEFAULT_VERSION,
        mojo::NullAssociatedReceiver());
    mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker> remote;
    db_->ScheduleOpenConnection(
        std::move(connection),
        base::MakeRefCounted<IndexedDBClientStateCheckerWrapper>(
            std::move(remote)));
    RunPostedTasks();
    EXPECT_EQ(IndexedDBDatabaseMetadata::NO_VERSION, db_->metadata().version);

    EXPECT_TRUE(request_.connection());
    transaction_ = request_.connection()->CreateVersionChangeTransaction(
        transaction_id, /*scope=*/std::set<int64_t>(),
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

 protected:
  MockIndexedDBFactoryClient request_;
  raw_ptr<IndexedDBTransaction, AcrossTasksDanglingUntriaged> transaction_ =
      nullptr;
  leveldb::Status commit_success_;
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
  db_ = nullptr;
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
  db_ = nullptr;

  transaction_->SetCommitFlag();
  RunPostedTasks();
  EXPECT_FALSE(error_called_);
  EXPECT_TRUE(s.ok());
}

}  // namespace content
