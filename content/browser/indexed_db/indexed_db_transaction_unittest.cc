// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_transaction.h"

#include <stdint.h>

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"
#include "content/browser/indexed_db/indexed_db_bucket_context.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_fake_backing_store.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace content {
namespace {

void SetToTrue(bool* value) {
  *value = true;
}

}  // namespace

class AbortObserver {
 public:
  AbortObserver() = default;

  AbortObserver(const AbortObserver&) = delete;
  AbortObserver& operator=(const AbortObserver&) = delete;

  void AbortTask() { abort_task_called_ = true; }

  bool abort_task_called() const { return abort_task_called_; }

 private:
  bool abort_task_called_ = false;
};

class IndexedDBTransactionTest : public testing::Test {
 public:
  IndexedDBTransactionTest()
      : task_environment_(std::make_unique<base::test::TaskEnvironment>()) {}

  IndexedDBTransactionTest(const IndexedDBTransactionTest&) = delete;
  IndexedDBTransactionTest& operator=(const IndexedDBTransactionTest&) = delete;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
        /*is_incognito=*/false, temp_dir_.GetPath(),
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        /*special_storage_policy=*/nullptr);

    IndexedDBBucketContext::Delegate delegate;
    delegate.on_ready_for_destruction =
        base::BindOnce(&IndexedDBTransactionTest::OnDbReadyForDestruction,
                       base::Unretained(this));

    const blink::StorageKey storage_key =
        blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
    bucket_context_ = std::make_unique<IndexedDBBucketContext>(
        GetOrCreateBucket(
            storage::BucketInitParams::ForDefaultBucket(storage_key)),
        temp_dir_.GetPath(), std::move(delegate), quota_manager_->proxy(),
        /*io_task_runner=*/base::SequencedTaskRunner::GetCurrentDefault(),
        /*blob_storage_context=*/mojo::NullRemote(),
        /*file_system_access_context=*/mojo::NullRemote(), base::DoNothing());

    bucket_context_->InitBackingStoreIfNeeded(true);
    db_ = bucket_context_->AddDatabase(
        u"db", std::make_unique<IndexedDBDatabase>(
                   u"db", *bucket_context_, IndexedDBDatabase::Identifier()));
  }

  void TearDown() override { db_ = nullptr; }

  storage::BucketInfo GetOrCreateBucket(
      const storage::BucketInitParams& params) {
    base::test::TestFuture<storage::QuotaErrorOr<storage::BucketInfo>> future;
    quota_manager_->proxy()->UpdateOrCreateBucket(
        params, base::SingleThreadTaskRunner::GetCurrentDefault(),
        future.GetCallback());
    return future.Take().value();
  }

  void OnDbReadyForDestruction() { bucket_context_.reset(); }

  void RunPostedTasks() { base::RunLoop().RunUntilIdle(); }

  leveldb::Status DummyOperation(leveldb::Status result,
                                 IndexedDBTransaction* transaction) {
    return result;
  }
  leveldb::Status AbortableOperation(AbortObserver* observer,
                                     IndexedDBTransaction* transaction) {
    transaction->ScheduleAbortTask(
        base::BindOnce(&AbortObserver::AbortTask, base::Unretained(observer)));
    return leveldb::Status::OK();
  }

  std::unique_ptr<IndexedDBConnection> CreateConnection() {
    mojo::Remote<storage::mojom::IndexedDBClientStateChecker> remote;
    auto connection = std::make_unique<IndexedDBConnection>(
        *bucket_context_, db_->AsWeakPtr(), base::DoNothing(),
        base::DoNothing(),
        std::make_unique<IndexedDBDatabaseCallbacks>(
            mojo::NullAssociatedRemote()),
        std::move(remote), base::UnguessableToken::Create());
    db_->AddConnectionForTesting(connection.get());
    return connection;
  }

  IndexedDBTransaction* CreateTransaction(
      IndexedDBConnection* connection,
      const int64_t id,
      const std::vector<int64_t>& object_store_ids,
      blink::mojom::IDBTransactionMode mode) {
    connection->CreateTransaction(
        mojo::NullAssociatedReceiver(), id, object_store_ids, mode,
        blink::mojom::IDBTransactionDurability::Relaxed);

    IndexedDBTransaction* transaction = connection->GetTransaction(id);

    // `CreateTransaction()` must not fail in this unit test environment.
    CHECK_NE(transaction, nullptr);
    return transaction;
  }

  // Creates a new transaction and adds it to `connection` using
  // IndexedDBConnection private members.  This enables the use of a fake
  // backing transaction to simulate errors.  Prefer CreateConnection() above
  // for tests that do not need to simulate errors because it uses
  // publicly exposed functionality.
  IndexedDBTransaction* CreateFakeTransactionWithCommitPhaseTwoError(
      IndexedDBConnection* connection,
      const int64_t id,
      const std::set<int64_t>& object_store_ids,
      blink::mojom::IDBTransactionMode mode,
      leveldb::Status commit_phase_two_error_status) {
    // Use fake transactions to simulate errors only.
    CHECK(!commit_phase_two_error_status.ok());

    std::unique_ptr<IndexedDBTransaction> transaction =
        std::make_unique<IndexedDBTransaction>(
            id, connection, object_store_ids, mode,
            IndexedDBBucketContextHandle(*bucket_context_),
            new FakeTransaction(commit_phase_two_error_status, mode,
                                bucket_context_->backing_store()->AsWeakPtr()));

    IndexedDBTransaction* transaction_reference = transaction.get();
    connection->transactions_[id] = std::move(transaction);

    db_->RegisterAndScheduleTransaction(transaction_reference);
    return transaction_reference;
  }

  PartitionedLockManager& lock_manager() {
    return bucket_context_->lock_manager();
  }

 protected:
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<base::test::TaskEnvironment> task_environment_;
  std::unique_ptr<IndexedDBBucketContext> bucket_context_;
  raw_ptr<IndexedDBDatabase> db_;
  scoped_refptr<storage::MockQuotaManager> quota_manager_;
};

class IndexedDBTransactionTestMode
    : public IndexedDBTransactionTest,
      public testing::WithParamInterface<blink::mojom::IDBTransactionMode> {
 public:
  IndexedDBTransactionTestMode() = default;

  IndexedDBTransactionTestMode(const IndexedDBTransactionTestMode&) = delete;
  IndexedDBTransactionTestMode& operator=(const IndexedDBTransactionTestMode&) =
      delete;
};

TEST_F(IndexedDBTransactionTest, Timeout) {
  const std::vector<int64_t> object_store_ids{1};
  std::unique_ptr<IndexedDBConnection> connection = CreateConnection();
  IndexedDBTransaction* transaction =
      CreateTransaction(connection.get(), /*id=*/0, object_store_ids,
                        blink::mojom::IDBTransactionMode::ReadWrite);

  // No conflicting transactions, so coordinator will start it immediately:
  EXPECT_EQ(IndexedDBTransaction::STARTED, transaction->state());
  EXPECT_FALSE(transaction->IsTimeoutTimerRunning());
  EXPECT_EQ(0, transaction->diagnostics().tasks_scheduled);
  EXPECT_EQ(0, transaction->diagnostics().tasks_completed);

  // Schedule a task - timer won't be started until it's processed.
  transaction->ScheduleTask(
      base::BindOnce(&IndexedDBTransactionTest::DummyOperation,
                     base::Unretained(this), leveldb::Status::OK()));
  EXPECT_FALSE(transaction->IsTimeoutTimerRunning());
  EXPECT_EQ(1, transaction->diagnostics().tasks_scheduled);
  EXPECT_EQ(0, transaction->diagnostics().tasks_completed);

  RunPostedTasks();
  EXPECT_TRUE(transaction->IsTimeoutTimerRunning());

  // Since the transaction isn't blocking another transaction, it's expected to
  // do nothing when the timeout fires.
  transaction->TimeoutFired();
  EXPECT_EQ(0, transaction->timeout_strikes_);
  EXPECT_EQ(IndexedDBTransaction::STARTED, transaction->state());

  // Create a second transaction that's blocked on the first.
  std::unique_ptr<IndexedDBConnection> connection2 = CreateConnection();
  CreateTransaction(connection2.get(),
                    /*id=*/1, object_store_ids,
                    blink::mojom::IDBTransactionMode::ReadWrite);

  // Now firing the timeout starts racking up strikes.
  for (int i = 1; i < IndexedDBTransaction::kMaxTimeoutStrikes; ++i) {
    transaction->TimeoutFired();
    EXPECT_EQ(IndexedDBTransaction::STARTED, transaction->state());
    EXPECT_EQ(i, transaction->timeout_strikes_);
  }

  // ... and eventually causes the transaction to abort.
  transaction->TimeoutFired();
  EXPECT_EQ(IndexedDBTransaction::FINISHED, transaction->state());
  EXPECT_FALSE(transaction->IsTimeoutTimerRunning());
  EXPECT_EQ(1, transaction->diagnostics().tasks_scheduled);
  EXPECT_EQ(1, transaction->diagnostics().tasks_completed);

  // This task will be ignored.
  transaction->ScheduleTask(
      base::BindOnce(&IndexedDBTransactionTest::DummyOperation,
                     base::Unretained(this), leveldb::Status::OK()));
  EXPECT_EQ(IndexedDBTransaction::FINISHED, transaction->state());
  EXPECT_FALSE(transaction->IsTimeoutTimerRunning());
  EXPECT_EQ(1, transaction->diagnostics().tasks_scheduled);
  EXPECT_EQ(1, transaction->diagnostics().tasks_completed);
}

TEST_F(IndexedDBTransactionTest, TimeoutPreemptive) {
  std::unique_ptr<IndexedDBConnection> connection = CreateConnection();
  IndexedDBTransaction* transaction =
      CreateTransaction(connection.get(), /*id=*/0, /*object_store_ids=*/{},
                        blink::mojom::IDBTransactionMode::ReadWrite);

  // No conflicting transactions, so coordinator will start it immediately:
  EXPECT_EQ(IndexedDBTransaction::STARTED, transaction->state());
  EXPECT_FALSE(transaction->IsTimeoutTimerRunning());
  EXPECT_EQ(0, transaction->diagnostics().tasks_scheduled);
  EXPECT_EQ(0, transaction->diagnostics().tasks_completed);

  // Add a preemptive task.
  transaction->ScheduleTask(
      blink::mojom::IDBTaskType::Preemptive,
      base::BindOnce(&IndexedDBTransactionTest::DummyOperation,
                     base::Unretained(this), leveldb::Status::OK()));
  transaction->AddPreemptiveEvent();

  EXPECT_TRUE(transaction->HasPendingTasks());
  EXPECT_FALSE(transaction->IsTimeoutTimerRunning());
  EXPECT_TRUE(transaction->task_queue_.empty());
  EXPECT_FALSE(transaction->preemptive_task_queue_.empty());

  // Pump the message loop so that the transaction completes all pending tasks,
  // otherwise it will defer the commit.
  RunPostedTasks();
  EXPECT_TRUE(transaction->HasPendingTasks());
  EXPECT_FALSE(transaction->IsTimeoutTimerRunning());
  EXPECT_TRUE(transaction->task_queue_.empty());
  EXPECT_TRUE(transaction->preemptive_task_queue_.empty());

  // Schedule a task - timer won't be started until preemptive tasks are done.
  transaction->ScheduleTask(
      base::BindOnce(&IndexedDBTransactionTest::DummyOperation,
                     base::Unretained(this), leveldb::Status::OK()));
  EXPECT_FALSE(transaction->IsTimeoutTimerRunning());
  EXPECT_EQ(1, transaction->diagnostics().tasks_scheduled);
  EXPECT_EQ(0, transaction->diagnostics().tasks_completed);

  // This shouldn't do anything - the preemptive task is still lurking.
  RunPostedTasks();
  EXPECT_TRUE(transaction->HasPendingTasks());
  EXPECT_FALSE(transaction->IsTimeoutTimerRunning());
  EXPECT_EQ(1, transaction->diagnostics().tasks_scheduled);
  EXPECT_EQ(0, transaction->diagnostics().tasks_completed);

  // Finish the preemptive task, which unblocks regular tasks.
  transaction->DidCompletePreemptiveEvent();
  // TODO(dmurph): Should this explicit call be necessary?
  transaction->RunTasks();

  // The task's completion should start the timer.
  EXPECT_FALSE(transaction->HasPendingTasks());
  EXPECT_TRUE(transaction->IsTimeoutTimerRunning());
  EXPECT_EQ(1, transaction->diagnostics().tasks_scheduled);
  EXPECT_EQ(1, transaction->diagnostics().tasks_completed);
}

TEST_P(IndexedDBTransactionTestMode, ScheduleNormalTask) {
  std::unique_ptr<IndexedDBConnection> connection = CreateConnection();
  IndexedDBTransaction* transaction =
      CreateTransaction(connection.get(), /*id=*/0, /*object_store_ids=*/{},
                        /*mode=*/GetParam());

  EXPECT_FALSE(transaction->HasPendingTasks());
  EXPECT_TRUE(transaction->IsTaskQueueEmpty());
  EXPECT_TRUE(transaction->task_queue_.empty());
  EXPECT_TRUE(transaction->preemptive_task_queue_.empty());
  EXPECT_EQ(0, transaction->diagnostics().tasks_scheduled);
  EXPECT_EQ(0, transaction->diagnostics().tasks_completed);

  transaction->ScheduleTask(
      blink::mojom::IDBTaskType::Normal,
      base::BindOnce(&IndexedDBTransactionTest::DummyOperation,
                     base::Unretained(this), leveldb::Status::OK()));

  EXPECT_EQ(1, transaction->diagnostics().tasks_scheduled);
  EXPECT_EQ(0, transaction->diagnostics().tasks_completed);

  EXPECT_TRUE(transaction->HasPendingTasks());
  EXPECT_FALSE(transaction->IsTaskQueueEmpty());
  EXPECT_FALSE(transaction->task_queue_.empty());
  EXPECT_TRUE(transaction->preemptive_task_queue_.empty());

  // Pump the message loop so that the transaction completes all pending tasks,
  // otherwise it will defer the commit.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(transaction->HasPendingTasks());
  EXPECT_TRUE(transaction->IsTaskQueueEmpty());
  EXPECT_TRUE(transaction->task_queue_.empty());
  EXPECT_TRUE(transaction->preemptive_task_queue_.empty());
  EXPECT_EQ(IndexedDBTransaction::STARTED, transaction->state());
  EXPECT_EQ(1, transaction->diagnostics().tasks_scheduled);
  EXPECT_EQ(1, transaction->diagnostics().tasks_completed);

  transaction->SetCommitFlag();
  RunPostedTasks();
  EXPECT_EQ(0UL, connection->transactions().size());
}

TEST_P(IndexedDBTransactionTestMode, TaskFails) {
  std::unique_ptr<IndexedDBConnection> connection = CreateConnection();
  IndexedDBTransaction* transaction =
      CreateTransaction(connection.get(), /*id=*/0, /*object_store_ids=*/{},
                        /*mode=*/GetParam());

  EXPECT_FALSE(transaction->HasPendingTasks());
  EXPECT_TRUE(transaction->IsTaskQueueEmpty());
  EXPECT_TRUE(transaction->task_queue_.empty());
  EXPECT_TRUE(transaction->preemptive_task_queue_.empty());
  EXPECT_EQ(0, transaction->diagnostics().tasks_scheduled);
  EXPECT_EQ(0, transaction->diagnostics().tasks_completed);
  db_ = nullptr;

  transaction->ScheduleTask(
      blink::mojom::IDBTaskType::Normal,
      base::BindOnce(&IndexedDBTransactionTest::DummyOperation,
                     base::Unretained(this),
                     leveldb::Status::IOError("error")));

  EXPECT_EQ(1, transaction->diagnostics().tasks_scheduled);
  EXPECT_EQ(0, transaction->diagnostics().tasks_completed);

  EXPECT_TRUE(transaction->HasPendingTasks());
  EXPECT_FALSE(transaction->IsTaskQueueEmpty());
  EXPECT_FALSE(transaction->task_queue_.empty());
  EXPECT_TRUE(transaction->preemptive_task_queue_.empty());

  // Pump the message loop so that the transaction completes all pending tasks,
  // otherwise it will defer the commit.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(transaction->HasPendingTasks());
  EXPECT_TRUE(transaction->IsTaskQueueEmpty());
  EXPECT_TRUE(transaction->task_queue_.empty());
  EXPECT_TRUE(transaction->preemptive_task_queue_.empty());
  /// Transaction aborted due to the error.
  EXPECT_EQ(IndexedDBTransaction::FINISHED, transaction->state());
  transaction->SetCommitFlag();
  EXPECT_EQ(1, transaction->diagnostics().tasks_scheduled);
  EXPECT_EQ(1, transaction->diagnostics().tasks_completed);

  // An error was reported which deletes the bucket context.
  EXPECT_FALSE(bucket_context_);
}

TEST_F(IndexedDBTransactionTest, SchedulePreemptiveTask) {
  std::unique_ptr<IndexedDBConnection> connection = CreateConnection();
  IndexedDBTransaction* transaction =
      CreateFakeTransactionWithCommitPhaseTwoError(
          connection.get(), /*id=*/0, /*object_store_ids=*/{},
          blink::mojom::IDBTransactionMode::ReadWrite,
          leveldb::Status::Corruption("Ouch."));
  db_ = nullptr;

  EXPECT_FALSE(transaction->HasPendingTasks());
  EXPECT_TRUE(transaction->IsTaskQueueEmpty());
  EXPECT_TRUE(transaction->task_queue_.empty());
  EXPECT_TRUE(transaction->preemptive_task_queue_.empty());
  EXPECT_EQ(0, transaction->diagnostics().tasks_scheduled);
  EXPECT_EQ(0, transaction->diagnostics().tasks_completed);

  transaction->ScheduleTask(
      blink::mojom::IDBTaskType::Preemptive,
      base::BindOnce(&IndexedDBTransactionTest::DummyOperation,
                     base::Unretained(this), leveldb::Status::OK()));
  transaction->AddPreemptiveEvent();

  EXPECT_TRUE(transaction->HasPendingTasks());
  EXPECT_FALSE(transaction->IsTaskQueueEmpty());
  EXPECT_TRUE(transaction->task_queue_.empty());
  EXPECT_FALSE(transaction->preemptive_task_queue_.empty());

  // Pump the message loop so that the transaction completes all pending tasks,
  // otherwise it will defer the commit.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(transaction->HasPendingTasks());
  EXPECT_TRUE(transaction->IsTaskQueueEmpty());
  EXPECT_TRUE(transaction->task_queue_.empty());
  EXPECT_TRUE(transaction->preemptive_task_queue_.empty());
  EXPECT_EQ(IndexedDBTransaction::STARTED, transaction->state());
  EXPECT_EQ(0, transaction->diagnostics().tasks_scheduled);
  EXPECT_EQ(0, transaction->diagnostics().tasks_completed);

  transaction->DidCompletePreemptiveEvent();
  transaction->SetCommitFlag();
  RunPostedTasks();
  // The bucket context should have been destroyed via
  // `OnDbReadyForDestruction`.
  EXPECT_FALSE(bucket_context_);
}

TEST_P(IndexedDBTransactionTestMode, AbortTasks) {
  std::unique_ptr<IndexedDBConnection> connection = CreateConnection();
  IndexedDBTransaction* transaction =
      CreateFakeTransactionWithCommitPhaseTwoError(
          connection.get(), /*id=*/0, /*object_store_ids=*/{},
          /*mode=*/GetParam(), leveldb::Status::Corruption("Ouch."));
  db_ = nullptr;

  AbortObserver observer;
  transaction->ScheduleTask(
      base::BindOnce(&IndexedDBTransactionTest::AbortableOperation,
                     base::Unretained(this), base::Unretained(&observer)));

  // Pump the message loop so that the transaction completes all pending tasks,
  // otherwise it will defer the commit.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(observer.abort_task_called());
  transaction->SetCommitFlag();
  RunPostedTasks();
  EXPECT_TRUE(observer.abort_task_called());
  // An error was reported which deletes the backing store, as well as the
  // bucket context by way of `OnDbReadyForDestruction`.
  EXPECT_FALSE(bucket_context_);
}

TEST_P(IndexedDBTransactionTestMode, AbortPreemptive) {
  std::unique_ptr<IndexedDBConnection> connection = CreateConnection();
  IndexedDBTransaction* transaction =
      CreateTransaction(connection.get(), /*id=*/0, /*object_store_ids=*/{},
                        /*mode=*/GetParam());

  // No conflicting transactions, so coordinator will start it immediately:
  EXPECT_EQ(IndexedDBTransaction::STARTED, transaction->state());
  EXPECT_FALSE(transaction->IsTimeoutTimerRunning());

  transaction->ScheduleTask(
      blink::mojom::IDBTaskType::Preemptive,
      base::BindOnce(&IndexedDBTransactionTest::DummyOperation,
                     base::Unretained(this), leveldb::Status::OK()));
  EXPECT_EQ(0, transaction->pending_preemptive_events_);
  transaction->AddPreemptiveEvent();
  EXPECT_EQ(1, transaction->pending_preemptive_events_);

  RunPostedTasks();

  transaction->Abort(IndexedDBDatabaseError(
      blink::mojom::IDBException::kAbortError, "Transaction aborted by user."));
  EXPECT_EQ(IndexedDBTransaction::FINISHED, transaction->state());
  EXPECT_FALSE(transaction->IsTimeoutTimerRunning());
  EXPECT_EQ(0, transaction->pending_preemptive_events_);
  EXPECT_TRUE(transaction->preemptive_task_queue_.empty());
  EXPECT_TRUE(transaction->task_queue_.empty());
  EXPECT_FALSE(transaction->HasPendingTasks());
  EXPECT_EQ(transaction->diagnostics().tasks_completed,
            transaction->diagnostics().tasks_scheduled);
  EXPECT_TRUE(transaction->backing_store_transaction_begun_);
  EXPECT_TRUE(transaction->used_);
  EXPECT_FALSE(transaction->is_commit_pending_);

  // This task will be ignored.
  transaction->ScheduleTask(
      base::BindOnce(&IndexedDBTransactionTest::DummyOperation,
                     base::Unretained(this), leveldb::Status::OK()));
  EXPECT_EQ(IndexedDBTransaction::FINISHED, transaction->state());
  EXPECT_FALSE(transaction->IsTimeoutTimerRunning());
  EXPECT_FALSE(transaction->HasPendingTasks());
  EXPECT_EQ(transaction->diagnostics().tasks_completed,
            transaction->diagnostics().tasks_scheduled);
}

static const blink::mojom::IDBTransactionMode kTestModes[] = {
    blink::mojom::IDBTransactionMode::ReadOnly,
    blink::mojom::IDBTransactionMode::ReadWrite};

INSTANTIATE_TEST_SUITE_P(IndexedDBTransactions,
                         IndexedDBTransactionTestMode,
                         ::testing::ValuesIn(kTestModes));

TEST_F(IndexedDBTransactionTest, AbortCancelsLockRequest) {
  const int64_t id = 0;
  const int64_t object_store_id = 1ll;

  // Acquire a lock to block the transaction's lock acquisition.
  std::vector<PartitionedLockManager::PartitionedLockRequest> lock_requests;
  lock_requests.emplace_back(GetDatabaseLockId(u"name"),
                             PartitionedLockManager::LockType::kShared);
  lock_requests.emplace_back(GetObjectStoreLockId(id, object_store_id),
                             PartitionedLockManager::LockType::kExclusive);
  bool locks_received = false;
  PartitionedLockHolder temp_lock_receiver;
  lock_manager().AcquireLocks(lock_requests,
                              temp_lock_receiver.weak_factory.GetWeakPtr(),
                              base::BindOnce(SetToTrue, &locks_received));
  EXPECT_TRUE(locks_received);

  // Create and register the transaction, which should request locks and wait
  // for `temp_lock_receiver` to release the locks.
  std::unique_ptr<IndexedDBConnection> connection = CreateConnection();
  IndexedDBTransaction* transaction =
      CreateTransaction(connection.get(), id, {object_store_id},
                        blink::mojom::IDBTransactionMode::ReadWrite);
  EXPECT_EQ(transaction->state(), IndexedDBTransaction::CREATED);

  // Abort the transaction, which should cancel the
  // RegisterAndScheduleTransaction() pending lock request.
  transaction->Abort(
      IndexedDBDatabaseError(blink::mojom::IDBException::kUnknownError));
  EXPECT_EQ(transaction->state(), IndexedDBTransaction::FINISHED);

  // Clear `temp_lock_receiver` so we can test later that all locks have
  // cleared.
  temp_lock_receiver.locks.clear();

  // Verify that the locks are available for acquisition again, as the
  // transaction should have cancelled its lock request.
  locks_received = false;
  lock_manager().AcquireLocks(lock_requests,
                              temp_lock_receiver.weak_factory.GetWeakPtr(),
                              base::BindOnce(SetToTrue, &locks_received));
  EXPECT_TRUE(locks_received);
}

TEST_F(IndexedDBTransactionTest, PostedStartTaskRunAfterAbort) {
  std::unique_ptr<IndexedDBConnection> connection = CreateConnection();

  int64_t id = 0;
  const std::vector<int64_t> object_store_ids = {1ll};
  IndexedDBTransaction* transaction1 =
      CreateTransaction(connection.get(), id, object_store_ids,
                        blink::mojom::IDBTransactionMode::ReadWrite);
  EXPECT_EQ(transaction1->state(), IndexedDBTransaction::STARTED);

  // Register another transaction, which will block on the first transaction.
  IndexedDBTransaction* transaction2 =
      CreateTransaction(connection.get(), ++id, object_store_ids,
                        blink::mojom::IDBTransactionMode::ReadWrite);
  EXPECT_EQ(transaction2->state(), IndexedDBTransaction::CREATED);

  // Flush posted tasks before making the Abort calls since there are
  // posted RunTasksForDatabase() tasks which, if we waited to run them
  // until after Abort is called, would destroy our transactions and mask
  // a potential race condition.
  RunPostedTasks();

  // Abort all of the transactions, which should cause the second transaction's
  // posted Start() task to run.
  connection->AbortAllTransactions(
      IndexedDBDatabaseError(blink::mojom::IDBException::kUnknownError));

  EXPECT_EQ(transaction2->state(), IndexedDBTransaction::FINISHED);

  // Run tasks to ensure Start() is called but does not DCHECK.
  RunPostedTasks();

  // It's not safe to check the state of the transaction at this point since it
  // is freed when the IndexedDBDatabase::RunTasks call happens via the posted
  // RunTasksForDatabase task.
}

TEST_F(IndexedDBTransactionTest, IsTransactionBlockingOtherClients) {
  std::unique_ptr<IndexedDBConnection> connection = CreateConnection();

  const std::vector<int64_t> object_store_ids = {1ll};
  IndexedDBTransaction* transaction = CreateTransaction(
      connection.get(),
      /*id=*/0, object_store_ids, blink::mojom::IDBTransactionMode::ReadWrite);

  // Register a transaction with ReadWrite mode to object store 1.
  // The transaction should be started and it's not blocking any others.
  EXPECT_EQ(transaction->state(), IndexedDBTransaction::STARTED);
  EXPECT_FALSE(transaction->IsTransactionBlockingOtherClients());

  IndexedDBTransaction* transaction2 = CreateTransaction(
      connection.get(),
      /*id=*/1, object_store_ids, blink::mojom::IDBTransactionMode::ReadWrite);

  // Register another transaction with ReadWrite mode to the same object store.
  // The transaction should be blocked in `CREATED` state, but the previous
  // transaction is *not* blocking other clients because it's the same client.
  EXPECT_EQ(transaction2->state(), IndexedDBTransaction::CREATED);
  EXPECT_FALSE(transaction->IsTransactionBlockingOtherClients());

  // Register a very similar connection, but with a *different* client. Now this
  // one is blocking and `IsTransactionBlockingOtherClients` should be true.
  auto connection2 = CreateConnection();
  IndexedDBTransaction* transaction3 = CreateTransaction(
      connection2.get(),
      /*id=*/1, object_store_ids, blink::mojom::IDBTransactionMode::ReadWrite);

  RunPostedTasks();

  // Abort the blocked transaction, and the previous transaction should not be
  // blocking others anymore.
  transaction3->Abort(
      IndexedDBDatabaseError(blink::mojom::IDBException::kUnknownError));
  EXPECT_EQ(transaction3->state(), IndexedDBTransaction::FINISHED);
  RunPostedTasks();
  EXPECT_FALSE(transaction->IsTransactionBlockingOtherClients());
}

}  // namespace content
