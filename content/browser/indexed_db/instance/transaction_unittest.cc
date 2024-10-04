// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/transaction.h"

#include <stdint.h>

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "content/browser/indexed_db/instance/bucket_context.h"
#include "content/browser/indexed_db/instance/connection.h"
#include "content/browser/indexed_db/instance/database_callbacks.h"
#include "content/browser/indexed_db/instance/fake_transaction.h"
#include "content/public/common/content_features.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace content::indexed_db {
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

class TransactionTest : public testing::Test {
 public:
  TransactionTest()
      : task_environment_(std::make_unique<base::test::TaskEnvironment>()) {}

  TransactionTest(const TransactionTest&) = delete;
  TransactionTest& operator=(const TransactionTest&) = delete;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
        /*is_incognito=*/false, temp_dir_.GetPath(),
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        /*special_storage_policy=*/nullptr);

    BucketContext::Delegate delegate;
    delegate.on_ready_for_destruction = base::BindOnce(
        &TransactionTest::OnDbReadyForDestruction, base::Unretained(this));

    const blink::StorageKey storage_key =
        blink::StorageKey::CreateFromStringForTesting("http://localhost:81");
    bucket_context_ = std::make_unique<BucketContext>(
        GetOrCreateBucket(
            storage::BucketInitParams::ForDefaultBucket(storage_key)),
        temp_dir_.GetPath(), std::move(delegate), quota_manager_->proxy(),
        /*blob_storage_context=*/mojo::NullRemote(),
        /*file_system_access_context=*/mojo::NullRemote(), base::DoNothing());

    bucket_context_->InitBackingStoreIfNeeded(true);
    db_ = bucket_context_->AddDatabase(
        u"db", std::make_unique<Database>(u"db", *bucket_context_,
                                          Database::Identifier()));
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

  Status DummyOperation(Status result, Transaction* transaction) {
    return result;
  }
  Status AbortableOperation(AbortObserver* observer, Transaction* transaction) {
    transaction->ScheduleAbortTask(
        base::BindOnce(&AbortObserver::AbortTask, base::Unretained(observer)));
    return Status::OK();
  }

  std::unique_ptr<Connection> CreateConnection(int priority = 0) {
    mojo::Remote<storage::mojom::IndexedDBClientStateChecker> remote;
    auto connection = std::make_unique<Connection>(
        *bucket_context_, db_->AsWeakPtr(), base::DoNothing(),
        base::DoNothing(),
        std::make_unique<DatabaseCallbacks>(mojo::NullAssociatedRemote()),
        std::move(remote), base::UnguessableToken::Create(), priority);
    db_->AddConnectionForTesting(connection.get());
    return connection;
  }

  Transaction* CreateTransaction(Connection* connection,
                                 const int64_t id,
                                 const std::vector<int64_t>& object_store_ids,
                                 blink::mojom::IDBTransactionMode mode) {
    connection->CreateTransaction(
        mojo::NullAssociatedReceiver(), id, object_store_ids, mode,
        blink::mojom::IDBTransactionDurability::Relaxed);

    Transaction* transaction = connection->GetTransaction(id);

    // `CreateTransaction()` must not fail in this unit test environment.
    CHECK_NE(transaction, nullptr);
    return transaction;
  }

  // Creates a new transaction and adds it to `connection` using
  // Connection private members.  This enables the use of a fake
  // backing transaction to simulate errors.  Prefer CreateConnection() above
  // for tests that do not need to simulate errors because it uses
  // publicly exposed functionality.
  Transaction* CreateFakeTransactionWithCommitPhaseTwoError(
      Connection* connection,
      const int64_t id,
      const std::set<int64_t>& object_store_ids,
      blink::mojom::IDBTransactionMode mode,
      Status commit_phase_two_error_status) {
    // Use fake transactions to simulate errors only.
    CHECK(!commit_phase_two_error_status.ok());

    std::unique_ptr<Transaction> transaction = std::make_unique<Transaction>(
        id, connection, object_store_ids, mode,
        BucketContextHandle(*bucket_context_),
        new FakeTransaction(commit_phase_two_error_status, mode,
                            bucket_context_->backing_store()->AsWeakPtr()));

    Transaction* transaction_reference = transaction.get();
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
  std::unique_ptr<BucketContext> bucket_context_;
  raw_ptr<Database> db_;
  scoped_refptr<storage::MockQuotaManager> quota_manager_;
};

class TransactionTestMode
    : public TransactionTest,
      public testing::WithParamInterface<blink::mojom::IDBTransactionMode> {
 public:
  TransactionTestMode() = default;

  TransactionTestMode(const TransactionTestMode&) = delete;
  TransactionTestMode& operator=(const TransactionTestMode&) = delete;
};

TEST_F(TransactionTest, Timeout) {
  const std::vector<int64_t> object_store_ids{1};
  std::unique_ptr<Connection> connection = CreateConnection();
  Transaction* transaction =
      CreateTransaction(connection.get(), /*id=*/0, object_store_ids,
                        blink::mojom::IDBTransactionMode::ReadWrite);

  // No conflicting transactions, so coordinator will start it immediately:
  EXPECT_EQ(Transaction::STARTED, transaction->state());
  EXPECT_FALSE(transaction->IsTimeoutTimerRunning());
  EXPECT_EQ(0, transaction->diagnostics().tasks_scheduled);
  EXPECT_EQ(0, transaction->diagnostics().tasks_completed);

  // Schedule a task - timer won't be started until it's processed.
  transaction->ScheduleTask(base::BindOnce(
      &TransactionTest::DummyOperation, base::Unretained(this), Status::OK()));
  EXPECT_FALSE(transaction->IsTimeoutTimerRunning());
  EXPECT_EQ(1, transaction->diagnostics().tasks_scheduled);
  EXPECT_EQ(0, transaction->diagnostics().tasks_completed);

  RunPostedTasks();
  EXPECT_TRUE(transaction->IsTimeoutTimerRunning());

  // Since the transaction isn't blocking another transaction, it's expected to
  // do nothing when the timeout fires.
  transaction->TimeoutFired();
  EXPECT_EQ(0, transaction->timeout_strikes_);
  EXPECT_EQ(Transaction::STARTED, transaction->state());

  // Create a second transaction that's blocked on the first.
  std::unique_ptr<Connection> connection2 = CreateConnection();
  CreateTransaction(connection2.get(),
                    /*id=*/1, object_store_ids,
                    blink::mojom::IDBTransactionMode::ReadWrite);

  // Now firing the timeout starts racking up strikes.
  for (int i = 1; i < Transaction::kMaxTimeoutStrikes; ++i) {
    transaction->TimeoutFired();
    EXPECT_EQ(Transaction::STARTED, transaction->state());
    EXPECT_EQ(i, transaction->timeout_strikes_);
  }

  // ... and eventually causes the transaction to abort.
  transaction->TimeoutFired();
  EXPECT_EQ(Transaction::FINISHED, transaction->state());
  EXPECT_FALSE(transaction->IsTimeoutTimerRunning());
  EXPECT_EQ(1, transaction->diagnostics().tasks_scheduled);
  EXPECT_EQ(1, transaction->diagnostics().tasks_completed);

  // This task will be ignored.
  transaction->ScheduleTask(base::BindOnce(
      &TransactionTest::DummyOperation, base::Unretained(this), Status::OK()));
  EXPECT_EQ(Transaction::FINISHED, transaction->state());
  EXPECT_FALSE(transaction->IsTimeoutTimerRunning());
  EXPECT_EQ(1, transaction->diagnostics().tasks_scheduled);
  EXPECT_EQ(1, transaction->diagnostics().tasks_completed);
}

TEST_F(TransactionTest, TimeoutPreemptive) {
  std::unique_ptr<Connection> connection = CreateConnection();
  Transaction* transaction =
      CreateTransaction(connection.get(), /*id=*/0, /*object_store_ids=*/{},
                        blink::mojom::IDBTransactionMode::ReadWrite);

  // No conflicting transactions, so coordinator will start it immediately:
  EXPECT_EQ(Transaction::STARTED, transaction->state());
  EXPECT_FALSE(transaction->IsTimeoutTimerRunning());
  EXPECT_EQ(0, transaction->diagnostics().tasks_scheduled);
  EXPECT_EQ(0, transaction->diagnostics().tasks_completed);

  // Add a preemptive task.
  transaction->ScheduleTask(
      blink::mojom::IDBTaskType::Preemptive,
      base::BindOnce(&TransactionTest::DummyOperation, base::Unretained(this),
                     Status::OK()));
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
  transaction->ScheduleTask(base::BindOnce(
      &TransactionTest::DummyOperation, base::Unretained(this), Status::OK()));
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

TEST_F(TransactionTest, TimeoutWithPriorities) {
  struct {
    int pri_1;         // The priority of a running transaction.
    int pri_2;         // The priority of a transaction blocked on the running
                       // transaction.
    bool can_timeout;  // Whether the running transaction is a candidate for
                       // timeout.
  } const test_cases[] = {
      {0, 0, true}, {0, 1, true}, {1, 1, false}, {1, 0, true}, {2, 1, true}};

  const std::vector<int64_t> object_store_ids{1};
  int txn_id = 0;

  for (auto test_case : test_cases) {
    std::unique_ptr<Connection> connection = CreateConnection(test_case.pri_1);
    Transaction* transaction =
        CreateTransaction(connection.get(), txn_id++, object_store_ids,
                          blink::mojom::IDBTransactionMode::ReadWrite);

    EXPECT_EQ(Transaction::STARTED, transaction->state());
    EXPECT_FALSE(transaction->IsTimeoutTimerRunning());
    // Schedule a task - timer won't be started until it's processed.
    transaction->ScheduleTask(base::BindOnce(&TransactionTest::DummyOperation,
                                             base::Unretained(this),
                                             Status::OK()));
    EXPECT_TRUE(base::test::RunUntil(
        [&]() { return transaction->IsTimeoutTimerRunning(); }));

    // Since the transaction isn't blocking another transaction, it's expected
    // to do nothing when the timeout fires.
    transaction->TimeoutFired();
    EXPECT_EQ(0, transaction->timeout_strikes_);
    EXPECT_EQ(Transaction::STARTED, transaction->state());

    // Create a second transaction that's blocked on the first.
    std::unique_ptr<Connection> connection2 = CreateConnection(test_case.pri_2);
    CreateTransaction(connection2.get(),
                      /*id=*/txn_id++, object_store_ids,
                      blink::mojom::IDBTransactionMode::ReadWrite);

    // Now firing the timeout starts racking up strikes.
    transaction->TimeoutFired();
    EXPECT_EQ(test_case.can_timeout ? 1 : 0, transaction->timeout_strikes_);

    // Clean up for the next iteration.
    db_->ForceCloseAndRunTasks();
  }
}

TEST_F(TransactionTest, WithoutPrioritization) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kIdbPrioritizeForegroundClients);

  const std::vector<int64_t> object_store_ids{1};
  std::unique_ptr<Connection> low_pri_connection = CreateConnection(1);
  std::unique_ptr<Connection> high_pri_connection = CreateConnection(0);

  // Create transaction that is incidentally low priority.
  // No conflicting transactions, so coordinator will start it immediately:
  Transaction* low_pri_transaction =
      CreateTransaction(low_pri_connection.get(), /*id=*/0, object_store_ids,
                        blink::mojom::IDBTransactionMode::ReadWrite);
  EXPECT_EQ(Transaction::STARTED, low_pri_transaction->state());

  // Create second transaction, which is blocked.
  Transaction* low_pri_transaction2 =
      CreateTransaction(low_pri_connection.get(), /*id=*/1, object_store_ids,
                        blink::mojom::IDBTransactionMode::ReadWrite);
  EXPECT_EQ(Transaction::CREATED, low_pri_transaction2->state());

  // Create a high priority transaction, which also queues up.
  Transaction* high_pri_transaction =
      CreateTransaction(high_pri_connection.get(), /*id=*/2, object_store_ids,
                        blink::mojom::IDBTransactionMode::ReadWrite);
  EXPECT_EQ(Transaction::CREATED, high_pri_transaction->state());

  // Finish the first low priority transaction. Verify the order of queueing of
  // other transactions.
  low_pri_transaction->Abort(DatabaseError(
      blink::mojom::IDBException::kAbortError, "Transaction aborted by user."));
  EXPECT_EQ(Transaction::FINISHED, low_pri_transaction->state());

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return Transaction::STARTED == low_pri_transaction2->state(); }));
  EXPECT_EQ(Transaction::CREATED, high_pri_transaction->state());
}

TEST_F(TransactionTest, WithPrioritization) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kIdbPrioritizeForegroundClients};

  const std::vector<int64_t> object_store_ids{1};
  std::unique_ptr<Connection> low_pri_connection = CreateConnection(1);
  std::unique_ptr<Connection> high_pri_connection = CreateConnection(0);

  // Create transaction that is incidentally low priority.
  // No conflicting transactions, so coordinator will start it immediately:
  Transaction* low_pri_transaction =
      CreateTransaction(low_pri_connection.get(), /*id=*/0, object_store_ids,
                        blink::mojom::IDBTransactionMode::ReadWrite);
  EXPECT_EQ(Transaction::STARTED, low_pri_transaction->state());

  // Create second transaction, which is blocked.
  Transaction* low_pri_transaction2 =
      CreateTransaction(low_pri_connection.get(), /*id=*/1, object_store_ids,
                        blink::mojom::IDBTransactionMode::ReadWrite);
  EXPECT_EQ(Transaction::CREATED, low_pri_transaction2->state());

  // Create a couple high priority transactions, which skip ahead in the queue.
  Transaction* high_pri_transaction =
      CreateTransaction(high_pri_connection.get(), /*id=*/2, object_store_ids,
                        blink::mojom::IDBTransactionMode::ReadWrite);
  EXPECT_EQ(Transaction::CREATED, high_pri_transaction->state());
  Transaction* high_pri_transaction2 =
      CreateTransaction(high_pri_connection.get(), /*id=*/3, object_store_ids,
                        blink::mojom::IDBTransactionMode::ReadWrite);
  EXPECT_EQ(Transaction::CREATED, high_pri_transaction2->state());

  // Finish the first low priority transaction. Verify the order of queueing of
  // other transactions.
  low_pri_transaction->Abort(DatabaseError(
      blink::mojom::IDBException::kAbortError, "Transaction aborted by user."));
  EXPECT_EQ(Transaction::FINISHED, low_pri_transaction->state());

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return Transaction::STARTED == high_pri_transaction->state(); }));
  EXPECT_EQ(Transaction::CREATED, high_pri_transaction2->state());
  EXPECT_EQ(Transaction::CREATED, low_pri_transaction2->state());

  high_pri_transaction->Abort(DatabaseError(
      blink::mojom::IDBException::kAbortError, "Transaction aborted by user."));
  EXPECT_EQ(Transaction::FINISHED, high_pri_transaction->state());

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return Transaction::STARTED == high_pri_transaction2->state();
  }));
  EXPECT_EQ(Transaction::CREATED, low_pri_transaction2->state());

  high_pri_transaction2->Abort(DatabaseError(
      blink::mojom::IDBException::kAbortError, "Transaction aborted by user."));
  EXPECT_EQ(Transaction::FINISHED, high_pri_transaction2->state());

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return Transaction::STARTED == low_pri_transaction2->state(); }));
}

TEST_P(TransactionTestMode, ScheduleNormalTask) {
  std::unique_ptr<Connection> connection = CreateConnection();
  Transaction* transaction =
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
      base::BindOnce(&TransactionTest::DummyOperation, base::Unretained(this),
                     Status::OK()));

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
  EXPECT_EQ(Transaction::STARTED, transaction->state());
  EXPECT_EQ(1, transaction->diagnostics().tasks_scheduled);
  EXPECT_EQ(1, transaction->diagnostics().tasks_completed);

  transaction->SetCommitFlag();
  RunPostedTasks();
  EXPECT_EQ(0UL, connection->transactions().size());
}

TEST_P(TransactionTestMode, TaskFails) {
  std::unique_ptr<Connection> connection = CreateConnection();
  Transaction* transaction =
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
      base::BindOnce(&TransactionTest::DummyOperation, base::Unretained(this),
                     Status::IOError("error")));

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
  EXPECT_EQ(Transaction::FINISHED, transaction->state());
  transaction->SetCommitFlag();
  EXPECT_EQ(1, transaction->diagnostics().tasks_scheduled);
  EXPECT_EQ(1, transaction->diagnostics().tasks_completed);

  // An error was reported which deletes the bucket context.
  EXPECT_FALSE(bucket_context_);
}

TEST_F(TransactionTest, SchedulePreemptiveTask) {
  std::unique_ptr<Connection> connection = CreateConnection();
  Transaction* transaction = CreateFakeTransactionWithCommitPhaseTwoError(
      connection.get(), /*id=*/0, /*object_store_ids=*/{},
      blink::mojom::IDBTransactionMode::ReadWrite, Status::Corruption("Ouch."));
  db_ = nullptr;

  EXPECT_FALSE(transaction->HasPendingTasks());
  EXPECT_TRUE(transaction->IsTaskQueueEmpty());
  EXPECT_TRUE(transaction->task_queue_.empty());
  EXPECT_TRUE(transaction->preemptive_task_queue_.empty());
  EXPECT_EQ(0, transaction->diagnostics().tasks_scheduled);
  EXPECT_EQ(0, transaction->diagnostics().tasks_completed);

  transaction->ScheduleTask(
      blink::mojom::IDBTaskType::Preemptive,
      base::BindOnce(&TransactionTest::DummyOperation, base::Unretained(this),
                     Status::OK()));
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
  EXPECT_EQ(Transaction::STARTED, transaction->state());
  EXPECT_EQ(0, transaction->diagnostics().tasks_scheduled);
  EXPECT_EQ(0, transaction->diagnostics().tasks_completed);

  transaction->DidCompletePreemptiveEvent();
  transaction->SetCommitFlag();
  RunPostedTasks();
  // The bucket context should have been destroyed via
  // `OnDbReadyForDestruction`.
  EXPECT_FALSE(bucket_context_);
}

TEST_P(TransactionTestMode, AbortTasks) {
  std::unique_ptr<Connection> connection = CreateConnection();
  Transaction* transaction = CreateFakeTransactionWithCommitPhaseTwoError(
      connection.get(), /*id=*/0, /*object_store_ids=*/{},
      /*mode=*/GetParam(), Status::Corruption("Ouch."));
  db_ = nullptr;

  AbortObserver observer;
  transaction->ScheduleTask(base::BindOnce(&TransactionTest::AbortableOperation,
                                           base::Unretained(this),
                                           base::Unretained(&observer)));

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

TEST_P(TransactionTestMode, AbortPreemptive) {
  std::unique_ptr<Connection> connection = CreateConnection();
  Transaction* transaction =
      CreateTransaction(connection.get(), /*id=*/0, /*object_store_ids=*/{},
                        /*mode=*/GetParam());

  // No conflicting transactions, so coordinator will start it immediately:
  EXPECT_EQ(Transaction::STARTED, transaction->state());
  EXPECT_FALSE(transaction->IsTimeoutTimerRunning());

  transaction->ScheduleTask(
      blink::mojom::IDBTaskType::Preemptive,
      base::BindOnce(&TransactionTest::DummyOperation, base::Unretained(this),
                     Status::OK()));
  EXPECT_EQ(0, transaction->pending_preemptive_events_);
  transaction->AddPreemptiveEvent();
  EXPECT_EQ(1, transaction->pending_preemptive_events_);

  RunPostedTasks();

  transaction->Abort(DatabaseError(blink::mojom::IDBException::kAbortError,
                                   "Transaction aborted by user."));
  EXPECT_EQ(Transaction::FINISHED, transaction->state());
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
  transaction->ScheduleTask(base::BindOnce(
      &TransactionTest::DummyOperation, base::Unretained(this), Status::OK()));
  EXPECT_EQ(Transaction::FINISHED, transaction->state());
  EXPECT_FALSE(transaction->IsTimeoutTimerRunning());
  EXPECT_FALSE(transaction->HasPendingTasks());
  EXPECT_EQ(transaction->diagnostics().tasks_completed,
            transaction->diagnostics().tasks_scheduled);
}

static const blink::mojom::IDBTransactionMode kTestModes[] = {
    blink::mojom::IDBTransactionMode::ReadOnly,
    blink::mojom::IDBTransactionMode::ReadWrite};

INSTANTIATE_TEST_SUITE_P(Transactions,
                         TransactionTestMode,
                         ::testing::ValuesIn(kTestModes));

TEST_F(TransactionTest, AbortCancelsLockRequest) {
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
  lock_manager().AcquireLocks(lock_requests, temp_lock_receiver,
                              base::BindOnce(SetToTrue, &locks_received));
  EXPECT_TRUE(locks_received);

  // Create and register the transaction, which should request locks and wait
  // for `temp_lock_receiver` to release the locks.
  std::unique_ptr<Connection> connection = CreateConnection();
  Transaction* transaction =
      CreateTransaction(connection.get(), id, {object_store_id},
                        blink::mojom::IDBTransactionMode::ReadWrite);
  EXPECT_EQ(transaction->state(), Transaction::CREATED);

  // Abort the transaction, which should cancel the
  // RegisterAndScheduleTransaction() pending lock request.
  transaction->Abort(DatabaseError(blink::mojom::IDBException::kUnknownError));
  EXPECT_EQ(transaction->state(), Transaction::FINISHED);

  // Clear `temp_lock_receiver` so we can test later that all locks have
  // cleared.
  temp_lock_receiver.locks.clear();

  // Verify that the locks are available for acquisition again, as the
  // transaction should have cancelled its lock request.
  locks_received = false;
  lock_manager().AcquireLocks(lock_requests, temp_lock_receiver,
                              base::BindOnce(SetToTrue, &locks_received));
  EXPECT_TRUE(locks_received);
}

TEST_F(TransactionTest, PostedStartTaskRunAfterAbort) {
  std::unique_ptr<Connection> connection = CreateConnection();

  int64_t id = 0;
  const std::vector<int64_t> object_store_ids = {1ll};
  Transaction* transaction1 =
      CreateTransaction(connection.get(), id, object_store_ids,
                        blink::mojom::IDBTransactionMode::ReadWrite);
  EXPECT_EQ(transaction1->state(), Transaction::STARTED);

  // Register another transaction, which will block on the first transaction.
  Transaction* transaction2 =
      CreateTransaction(connection.get(), ++id, object_store_ids,
                        blink::mojom::IDBTransactionMode::ReadWrite);
  EXPECT_EQ(transaction2->state(), Transaction::CREATED);

  // Flush posted tasks before making the Abort calls since there are
  // posted RunTasksForDatabase() tasks which, if we waited to run them
  // until after Abort is called, would destroy our transactions and mask
  // a potential race condition.
  RunPostedTasks();

  // Abort all of the transactions, which should cause the second transaction's
  // posted Start() task to run.
  connection->AbortAllTransactions(
      DatabaseError(blink::mojom::IDBException::kUnknownError));

  EXPECT_EQ(transaction2->state(), Transaction::FINISHED);

  // Run tasks to ensure Start() is called but does not DCHECK.
  RunPostedTasks();

  // It's not safe to check the state of the transaction at this point since it
  // is freed when the Database::RunTasks call happens via the posted
  // RunTasksForDatabase task.
}

TEST_F(TransactionTest, IsTransactionBlockingOtherClients) {
  std::unique_ptr<Connection> connection = CreateConnection();

  const std::vector<int64_t> object_store_ids = {1ll};
  Transaction* transaction = CreateTransaction(
      connection.get(),
      /*id=*/0, object_store_ids, blink::mojom::IDBTransactionMode::ReadWrite);

  // Register a transaction with ReadWrite mode to object store 1.
  // The transaction should be started and it's not blocking any others.
  EXPECT_EQ(transaction->state(), Transaction::STARTED);
  EXPECT_FALSE(transaction->IsTransactionBlockingOtherClients());

  Transaction* transaction2 = CreateTransaction(
      connection.get(),
      /*id=*/1, object_store_ids, blink::mojom::IDBTransactionMode::ReadWrite);

  // Register another transaction with ReadWrite mode to the same object store.
  // The transaction should be blocked in `CREATED` state, but the previous
  // transaction is *not* blocking other clients because it's the same client.
  EXPECT_EQ(transaction2->state(), Transaction::CREATED);
  EXPECT_FALSE(transaction->IsTransactionBlockingOtherClients());

  // Register a very similar connection, but with a *different* client. Now this
  // one is blocking and `IsTransactionBlockingOtherClients` should be true.
  auto connection2 = CreateConnection();
  Transaction* transaction3 = CreateTransaction(
      connection2.get(),
      /*id=*/1, object_store_ids, blink::mojom::IDBTransactionMode::ReadWrite);

  RunPostedTasks();

  // Abort the blocked transaction, and the previous transaction should not be
  // blocking others anymore.
  transaction3->Abort(DatabaseError(blink::mojom::IDBException::kUnknownError));
  EXPECT_EQ(transaction3->state(), Transaction::FINISHED);
  RunPostedTasks();
  EXPECT_FALSE(transaction->IsTransactionBlockingOtherClients());
}

}  // namespace content::indexed_db
