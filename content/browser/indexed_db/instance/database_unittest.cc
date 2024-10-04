// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/database.h"

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
#include "components/services/storage/privileged/mojom/indexed_db_client_state_checker.mojom.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "content/browser/indexed_db/instance/backing_store.h"
#include "content/browser/indexed_db/instance/bucket_context.h"
#include "content/browser/indexed_db/instance/connection.h"
#include "content/browser/indexed_db/instance/cursor.h"
#include "content/browser/indexed_db/instance/database_callbacks.h"
#include "content/browser/indexed_db/instance/factory_client.h"
#include "content/browser/indexed_db/instance/fake_transaction.h"
#include "content/browser/indexed_db/instance/mock_factory_client.h"
#include "content/browser/indexed_db/instance/transaction.h"
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

namespace content::indexed_db {

class DatabaseTest : public ::testing::Test {
 public:
  DatabaseTest() = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
        /*is_incognito=*/false, temp_dir_.GetPath(),
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        /*special_storage_policy=*/nullptr);

    quota_manager_proxy_ = base::MakeRefCounted<storage::MockQuotaManagerProxy>(
        quota_manager_.get(),
        base::SingleThreadTaskRunner::GetCurrentDefault().get());

    BucketContext::Delegate delegate;
    delegate.on_ready_for_destruction =
        base::BindOnce(&DatabaseTest::OnBucketContextReadyForDestruction,
                       weak_factory_.GetWeakPtr());

    bucket_context_ = std::make_unique<BucketContext>(
        storage::BucketInfo(), temp_dir_.GetPath(), std::move(delegate),
        quota_manager_proxy_,
        /*blob_storage_context=*/mojo::NullRemote(),
        /*file_system_access_context=*/mojo::NullRemote(), base::DoNothing());

    bucket_context_->InitBackingStoreIfNeeded(true);
    db_ = bucket_context_->AddDatabase(
        u"db", std::make_unique<Database>(u"db", *bucket_context_,
                                          Database::Identifier()));
  }

  void TearDown() override { db_ = nullptr; }

  void OnBucketContextReadyForDestruction() { bucket_context_.reset(); }

  void RunPostedTasks() {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<BucketContext> bucket_context_;
  scoped_refptr<storage::MockQuotaManager> quota_manager_;
  scoped_refptr<storage::MockQuotaManagerProxy> quota_manager_proxy_;

  // As this is owned by `bucket_context_`, tests that cause the database to
  // be destroyed must manually reset this to null to avoid triggering dangling
  // pointer warnings.
  raw_ptr<Database> db_ = nullptr;

  base::WeakPtrFactory<DatabaseTest> weak_factory_{this};
};

TEST_F(DatabaseTest, ConnectionLifecycle) {
  MockMojoDatabaseCallbacks database_callbacks;
  MockFactoryClient request1;
  const int64_t transaction_id1 = 1;
  auto connection1 = std::make_unique<PendingConnection>(
      std::make_unique<ThunkFactoryClient>(request1),
      std::make_unique<DatabaseCallbacks>(
          database_callbacks.BindNewEndpointAndPassDedicatedRemote()),
      transaction_id1, IndexedDBDatabaseMetadata::DEFAULT_VERSION,
      mojo::NullAssociatedReceiver());
  db_->ScheduleOpenConnection(std::move(connection1));
  RunPostedTasks();

  MockMojoDatabaseCallbacks database_callbacks2;
  MockFactoryClient request2;
  const int64_t transaction_id2 = 2;
  auto connection2 = std::make_unique<PendingConnection>(
      std::make_unique<ThunkFactoryClient>(request2),
      std::make_unique<DatabaseCallbacks>(
          database_callbacks2.BindNewEndpointAndPassDedicatedRemote()),
      transaction_id2, IndexedDBDatabaseMetadata::DEFAULT_VERSION,
      mojo::NullAssociatedReceiver());
  db_->ScheduleOpenConnection(std::move(connection2));
  RunPostedTasks();
  db_ = nullptr;

  EXPECT_TRUE(request1.connection());
  request1.connection()->CloseAndReportForceClose();
  EXPECT_FALSE(request1.connection()->IsConnected());

  EXPECT_TRUE(request2.connection());
  request2.connection()->CloseAndReportForceClose();
  EXPECT_FALSE(request2.connection()->IsConnected());

  RunPostedTasks();

  EXPECT_TRUE(bucket_context_->GetDatabasesForTesting().empty());
}

TEST_F(DatabaseTest, ForcedClose) {
  MockMojoDatabaseCallbacks database_callbacks;
  MockFactoryClient request;
  const int64_t upgrade_transaction_id = 3;
  auto connection = std::make_unique<PendingConnection>(
      std::make_unique<ThunkFactoryClient>(request),
      std::make_unique<DatabaseCallbacks>(
          database_callbacks.BindNewEndpointAndPassDedicatedRemote()),
      upgrade_transaction_id, IndexedDBDatabaseMetadata::DEFAULT_VERSION,
      mojo::NullAssociatedReceiver());
  db_->ScheduleOpenConnection(std::move(connection));
  RunPostedTasks();

  EXPECT_EQ(db_, request.connection()->database().get());

  request.connection()->CreateTransaction(
      mojo::NullAssociatedReceiver(), /*transaction_id=*/123,
      /*object_store_ids=*/{}, blink::mojom::IDBTransactionMode::ReadOnly,
      blink::mojom::IDBTransactionDurability::Relaxed);
  db_ = nullptr;

  base::RunLoop run_loop;
  EXPECT_CALL(database_callbacks, ForcedClose)
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  request.connection()->CloseAndReportForceClose();
  run_loop.Run();
}

namespace {

class FakeFactoryClient : public FactoryClient {
 public:
  FakeFactoryClient() : FactoryClient(mojo::NullAssociatedRemote()) {}
  ~FakeFactoryClient() override = default;

  FakeFactoryClient(const FakeFactoryClient&) = delete;
  FakeFactoryClient& operator=(const FakeFactoryClient&) = delete;

  void OnBlocked(int64_t existing_version) override { blocked_called_ = true; }
  void OnDeleteSuccess(int64_t old_version) override { success_called_ = true; }
  void OnError(const DatabaseError& error) override { error_called_ = true; }

  bool blocked_called() const { return blocked_called_; }
  bool success_called() const { return success_called_; }
  bool error_called() const { return error_called_; }

 private:
  bool blocked_called_ = false;
  bool success_called_ = false;
  bool error_called_ = false;
};

}  // namespace

TEST_F(DatabaseTest, PendingDelete) {
  MockFactoryClient request1;
  const int64_t transaction_id1 = 1;
  MockMojoDatabaseCallbacks database_callbacks1;
  auto connection = std::make_unique<PendingConnection>(
      std::make_unique<ThunkFactoryClient>(request1),
      std::make_unique<DatabaseCallbacks>(
          database_callbacks1.BindNewEndpointAndPassDedicatedRemote()),
      transaction_id1, IndexedDBDatabaseMetadata::DEFAULT_VERSION,
      mojo::NullAssociatedReceiver());
  db_->ScheduleOpenConnection(std::move(connection));
  RunPostedTasks();

  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 0UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 0UL);

  base::RunLoop run_loop;
  FakeFactoryClient request2;
  db_->ScheduleDeleteDatabase(std::make_unique<ThunkFactoryClient>(request2),
                              run_loop.QuitClosure());
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

  run_loop.Run();
  EXPECT_FALSE(db_);

  EXPECT_TRUE(request2.success_called());
}

TEST_F(DatabaseTest, OpenDeleteClear) {
  const int64_t kDatabaseVersion = 1;

  MockFactoryClient request1(
      /*expect_connection=*/true);
  MockMojoDatabaseCallbacks database_callbacks1;
  const int64_t transaction_id1 = 1;
  auto connection1 = std::make_unique<PendingConnection>(
      std::make_unique<ThunkFactoryClient>(request1),
      std::make_unique<DatabaseCallbacks>(
          database_callbacks1.BindNewEndpointAndPassDedicatedRemote()),
      transaction_id1, kDatabaseVersion, mojo::NullAssociatedReceiver());
  db_->ScheduleOpenConnection(std::move(connection1));
  RunPostedTasks();

  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 1UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 0UL);

  MockFactoryClient request2(
      /*expect_connection=*/false);
  MockMojoDatabaseCallbacks database_callbacks2;
  const int64_t transaction_id2 = 2;
  auto connection2 = std::make_unique<PendingConnection>(
      std::make_unique<ThunkFactoryClient>(request2),
      std::make_unique<DatabaseCallbacks>(
          database_callbacks2.BindNewEndpointAndPassDedicatedRemote()),
      transaction_id2, kDatabaseVersion, mojo::NullAssociatedReceiver());
  db_->ScheduleOpenConnection(std::move(connection2));
  RunPostedTasks();

  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 1UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 1UL);

  MockFactoryClient request3(
      /*expect_connection=*/false);
  MockMojoDatabaseCallbacks database_callbacks3;
  const int64_t transaction_id3 = 3;
  auto connection3 = std::make_unique<PendingConnection>(
      std::make_unique<ThunkFactoryClient>(request3),
      std::make_unique<DatabaseCallbacks>(
          database_callbacks3.BindNewEndpointAndPassDedicatedRemote()),
      transaction_id3, kDatabaseVersion, mojo::NullAssociatedReceiver());
  db_->ScheduleOpenConnection(std::move(connection3));
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

TEST_F(DatabaseTest, ForceDelete) {
  MockFactoryClient request1;
  MockMojoDatabaseCallbacks database_callbacks;
  const int64_t transaction_id1 = 1;
  auto connection = std::make_unique<PendingConnection>(
      std::make_unique<ThunkFactoryClient>(request1),
      std::make_unique<DatabaseCallbacks>(
          database_callbacks.BindNewEndpointAndPassDedicatedRemote()),
      transaction_id1, IndexedDBDatabaseMetadata::DEFAULT_VERSION,
      mojo::NullAssociatedReceiver());
  db_->ScheduleOpenConnection(std::move(connection));
  RunPostedTasks();

  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 0UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 0UL);

  base::RunLoop run_loop;
  FakeFactoryClient request2;
  db_->ScheduleDeleteDatabase(std::make_unique<ThunkFactoryClient>(request2),
                              run_loop.QuitClosure());
  RunPostedTasks();
  EXPECT_FALSE(run_loop.AnyQuitCalled());
  db_->ForceCloseAndRunTasks();
  db_ = nullptr;
  run_loop.Run();
  EXPECT_FALSE(db_);
  EXPECT_FALSE(request2.blocked_called());
  EXPECT_TRUE(request2.success_called());
}

TEST_F(DatabaseTest, ForceCloseWhileOpenPending) {
  // Verify that pending connection requests are handled correctly during a
  // ForceClose.
  MockFactoryClient request1;
  MockMojoDatabaseCallbacks database_callbacks1;
  const int64_t transaction_id1 = 1;
  auto connection1 = std::make_unique<PendingConnection>(
      std::make_unique<ThunkFactoryClient>(request1),
      std::make_unique<DatabaseCallbacks>(
          database_callbacks1.BindNewEndpointAndPassDedicatedRemote()),
      transaction_id1, IndexedDBDatabaseMetadata::DEFAULT_VERSION,
      mojo::NullAssociatedReceiver());
  db_->ScheduleOpenConnection(std::move(connection1));
  RunPostedTasks();

  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 0UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 0UL);

  MockFactoryClient request2(/*expect_connection=*/false);
  MockMojoDatabaseCallbacks database_callbacks2;

  const int64_t transaction_id2 = 2;
  auto connection2 = std::make_unique<PendingConnection>(
      std::make_unique<ThunkFactoryClient>(request2),
      std::make_unique<DatabaseCallbacks>(
          database_callbacks2.BindNewEndpointAndPassDedicatedRemote()),
      transaction_id2, 3, mojo::NullAssociatedReceiver());
  db_->ScheduleOpenConnection(std::move(connection2));
  RunPostedTasks();

  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 1UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 0UL);

  db_->ForceCloseAndRunTasks();
  db_ = nullptr;
  RunPostedTasks();
  EXPECT_FALSE(db_);
}

TEST_F(DatabaseTest, ForceCloseWhileOpenAndDeletePending) {
  // Verify that pending connection requests are handled correctly during a
  // ForceClose.
  MockFactoryClient request1;
  MockMojoDatabaseCallbacks database_callbacks1;
  const int64_t transaction_id1 = 1;
  auto connection1 = std::make_unique<PendingConnection>(
      std::make_unique<ThunkFactoryClient>(request1),
      std::make_unique<DatabaseCallbacks>(
          database_callbacks1.BindNewEndpointAndPassDedicatedRemote()),
      transaction_id1, IndexedDBDatabaseMetadata::DEFAULT_VERSION,
      mojo::NullAssociatedReceiver());
  db_->ScheduleOpenConnection(std::move(connection1));
  RunPostedTasks();

  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 0UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 0UL);

  MockFactoryClient request2(false);
  MockMojoDatabaseCallbacks database_callbacks2;
  const int64_t transaction_id2 = 2;
  auto connection2 = std::make_unique<PendingConnection>(
      std::make_unique<ThunkFactoryClient>(request2),
      std::make_unique<DatabaseCallbacks>(
          database_callbacks2.BindNewEndpointAndPassDedicatedRemote()),
      transaction_id2, 3, mojo::NullAssociatedReceiver());
  db_->ScheduleOpenConnection(std::move(connection2));
  RunPostedTasks();

  base::RunLoop run_loop;
  auto request3 = std::make_unique<FakeFactoryClient>();
  db_->ScheduleDeleteDatabase(std::move(request3), run_loop.QuitClosure());
  RunPostedTasks();
  EXPECT_FALSE(run_loop.AnyQuitCalled());

  EXPECT_EQ(db_->ConnectionCount(), 1UL);
  EXPECT_EQ(db_->ActiveOpenDeleteCount(), 1UL);
  EXPECT_EQ(db_->PendingOpenDeleteCount(), 1UL);

  db_->ForceCloseAndRunTasks();
  db_ = nullptr;
  run_loop.Run();
}

Status DummyOperation(Transaction* transaction) {
  return Status::OK();
}

class DatabaseOperationTest : public DatabaseTest {
 public:
  DatabaseOperationTest() = default;
  DatabaseOperationTest(const DatabaseOperationTest&) = delete;
  DatabaseOperationTest& operator=(const DatabaseOperationTest&) = delete;

  void SetUp() override {
    DatabaseTest::SetUp();

    const int64_t transaction_id = 1;
    auto connection = std::make_unique<PendingConnection>(
        std::make_unique<ThunkFactoryClient>(request_),
        std::make_unique<DatabaseCallbacks>(mojo::NullAssociatedRemote()),
        transaction_id, IndexedDBDatabaseMetadata::DEFAULT_VERSION,
        mojo::NullAssociatedReceiver());
    db_->ScheduleOpenConnection(std::move(connection));
    RunPostedTasks();
    EXPECT_EQ(IndexedDBDatabaseMetadata::NO_VERSION, db_->metadata().version);

    EXPECT_TRUE(request_.connection());
    transaction_ = request_.connection()->CreateVersionChangeTransaction(
        transaction_id, /*scope=*/std::set<int64_t>(),
        new FakeTransaction(commit_success_,
                            blink::mojom::IDBTransactionMode::VersionChange,
                            bucket_context_->backing_store()->AsWeakPtr()));

    std::vector<PartitionedLockManager::PartitionedLockRequest> lock_requests =
        {{GetDatabaseLockId(db_->metadata().name),
          PartitionedLockManager::LockType::kExclusive}};
    db_->lock_manager().AcquireLocks(
        std::move(lock_requests), *transaction_->mutable_locks_receiver(),
        base::BindOnce(&Transaction::Start, transaction_->AsWeakPtr()));

    // Add a dummy task which takes the place of the VersionChangeOperation
    // which kicks off the upgrade. This ensures that the transaction has
    // processed at least one task before the CreateObjectStore call.
    transaction_->ScheduleTask(base::BindOnce(&DummyOperation));
    // Run posted tasks to execute the dummy operation and ensure that it is
    // stored in the connection.
    RunPostedTasks();
  }

 protected:
  MockFactoryClient request_;

  // As this is owned by `Connection`, tests that cause the transaction
  // to be committed must manually reset this to null to avoid triggering
  // dangling pointer warnings.
  raw_ptr<Transaction> transaction_ = nullptr;
  Status commit_success_;
};

TEST_F(DatabaseOperationTest, CreateObjectStore) {
  EXPECT_EQ(0ULL, db_->metadata().object_stores.size());
  const int64_t store_id = 1001;
  Status s =
      db_->CreateObjectStoreOperation(store_id, u"store", IndexedDBKeyPath(),
                                      /*auto_increment=*/false, transaction_);
  EXPECT_TRUE(s.ok());
  transaction_->SetCommitFlag();
  transaction_ = nullptr;
  RunPostedTasks();
  EXPECT_TRUE(bucket_context_);
  EXPECT_EQ(1ULL, db_->metadata().object_stores.size());
}

TEST_F(DatabaseOperationTest, CreateIndex) {
  EXPECT_EQ(0ULL, db_->metadata().object_stores.size());
  const int64_t store_id = 1001;
  Status s =
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
  transaction_ = nullptr;
  RunPostedTasks();
  EXPECT_TRUE(bucket_context_);
  EXPECT_EQ(1ULL, db_->metadata().object_stores.size());
  EXPECT_EQ(
      1ULL,
      db_->metadata().object_stores.find(store_id)->second.indexes.size());
}

class DatabaseOperationAbortTest : public DatabaseOperationTest {
 public:
  DatabaseOperationAbortTest() {
    commit_success_ = Status::NotFound("Bummer.");
  }

  DatabaseOperationAbortTest(const DatabaseOperationAbortTest&) = delete;
  DatabaseOperationAbortTest& operator=(const DatabaseOperationAbortTest&) =
      delete;
};

TEST_F(DatabaseOperationAbortTest, CreateObjectStore) {
  EXPECT_EQ(0ULL, db_->metadata().object_stores.size());
  const int64_t store_id = 1001;
  Status s =
      db_->CreateObjectStoreOperation(store_id, u"store", IndexedDBKeyPath(),
                                      /*auto_increment=*/false, transaction_);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(1ULL, db_->metadata().object_stores.size());
  db_ = nullptr;
  transaction_->SetCommitFlag();
  RunPostedTasks();
  // A transaction error results in a deleted db.
  EXPECT_TRUE(bucket_context_->GetDatabasesForTesting().empty());
}

TEST_F(DatabaseOperationAbortTest, CreateIndex) {
  EXPECT_EQ(0ULL, db_->metadata().object_stores.size());
  const int64_t store_id = 1001;
  Status s =
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
  db_ = nullptr;
  transaction_->SetCommitFlag();
  RunPostedTasks();
  // A transaction error results in a deleted db.
  EXPECT_TRUE(bucket_context_->GetDatabasesForTesting().empty());
}

TEST_F(DatabaseOperationTest, CreatePutDelete) {
  EXPECT_EQ(0ULL, db_->metadata().object_stores.size());
  const int64_t store_id = 1001;

  Status s =
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

  auto put_params = std::make_unique<Database::PutOperationParams>();
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
  transaction_ = nullptr;
  RunPostedTasks();
  // A transaction error would have resulted in a deleted db.
  EXPECT_FALSE(bucket_context_->GetDatabasesForTesting().empty());
  EXPECT_TRUE(s.ok());
}

}  // namespace content::indexed_db
