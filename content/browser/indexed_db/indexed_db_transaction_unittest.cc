// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_transaction.h"

#include <stdint.h>
#include <memory>

#include "base/check.h"
#include "base/debug/stack_trace.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/default_clock.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"
#include "content/browser/indexed_db/indexed_db_bucket_context.h"
#include "content/browser/indexed_db/indexed_db_class_factory.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_factory.h"
#include "content/browser/indexed_db/indexed_db_fake_backing_store.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "content/browser/indexed_db/mock_indexed_db_database_callbacks.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace content {
namespace indexed_db_transaction_unittest {
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
    indexed_db_context_ = base::MakeRefCounted<IndexedDBContextImpl>(
        temp_dir_.GetPath(), quota_manager_->proxy(),
        base::DefaultClock::GetInstance(),
        /*blob_storage_context=*/mojo::NullRemote(),
        /*file_system_access_context=*/mojo::NullRemote(),
        base::SequencedTaskRunner::GetCurrentDefault(),
        base::SequencedTaskRunner::GetCurrentDefault());

    IndexedDBBucketContext::Delegate delegate;
    delegate.on_tasks_available = CreateRunTasksCallback();

    bucket_context_ = std::make_unique<IndexedDBBucketContext>(
        storage::BucketInfo(), false, base::DefaultClock::GetInstance(),
        std::make_unique<PartitionedLockManager>(), std::move(delegate),
        std::make_unique<IndexedDBFakeBackingStore>(), quota_manager_->proxy(),
        /*io_task_runner=*/base::SequencedTaskRunner::GetCurrentDefault(),
        /*blob_storage_context=*/mojo::NullRemote(),
        /*file_system_access_context=*/mojo::NullRemote(), base::DoNothing());

    // DB is created here instead of the constructor to workaround a
    // "peculiarity of C++". More info at
    // https://github.com/google/googletest/blob/main/docs/faq.md#my-compiler-complains-that-a-constructor-or-destructor-cannot-return-a-value-whats-going-on
    db_ = std::make_unique<IndexedDBDatabase>(u"db", *bucket_context_,
                                              IndexedDBDatabase::Identifier());
  }

  TasksAvailableCallback CreateRunTasksCallback() {
    return base::BindRepeating(&IndexedDBTransactionTest::RunTasksForDatabase,
                               base::Unretained(this), true);
  }

  void RunTasksForDatabase(bool async) {
    if (!db_)
      return;
    if (async) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&IndexedDBTransactionTest::RunTasksForDatabase,
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
        return;
    }
  }

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
    mojo::PendingAssociatedRemote<storage::mojom::IndexedDBClientStateChecker>
        remote;
    auto connection = std::make_unique<IndexedDBConnection>(
        *bucket_context_, db_->AsWeakPtr(), base::DoNothing(),
        base::DoNothing(),
        base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>(),
        base::MakeRefCounted<IndexedDBClientStateCheckerWrapper>(
            std::move(remote)));
    db_->AddConnectionForTesting(connection.get());
    return connection;
  }

  PartitionedLockManager* lock_manager() {
    return bucket_context_->lock_manager();
  }

 protected:
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<base::test::TaskEnvironment> task_environment_;
  std::unique_ptr<IndexedDBBucketContext> bucket_context_;
  std::unique_ptr<IndexedDBDatabase> db_;
  scoped_refptr<IndexedDBContextImpl> indexed_db_context_;
  scoped_refptr<storage::MockQuotaManager> quota_manager_;

  bool error_called_ = false;
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
  const int64_t id = 0;
  const std::set<int64_t> scope;
  const leveldb::Status commit_success = leveldb::Status::OK();
  std::unique_ptr<IndexedDBConnection> connection = CreateConnection();
  IndexedDBTransaction* transaction = connection->CreateTransaction(
      id, scope, blink::mojom::IDBTransactionMode::ReadWrite,
      new IndexedDBFakeBackingStore::FakeTransaction(commit_success));
  db_->RegisterAndScheduleTransaction(transaction);

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

  transaction->Timeout();
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
  const int64_t id = 0;
  const std::set<int64_t> scope;
  const leveldb::Status commit_success = leveldb::Status::OK();
  std::unique_ptr<IndexedDBConnection> connection = CreateConnection();
  IndexedDBTransaction* transaction = connection->CreateTransaction(
      id, scope, blink::mojom::IDBTransactionMode::ReadWrite,
      new IndexedDBFakeBackingStore::FakeTransaction(commit_success));
  db_->RegisterAndScheduleTransaction(transaction);

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

TEST_F(IndexedDBTransactionTest, NoTimeoutReadOnly) {
  const int64_t id = 0;
  const std::set<int64_t> scope;
  const leveldb::Status commit_success = leveldb::Status::OK();
  std::unique_ptr<IndexedDBConnection> connection = CreateConnection();
  IndexedDBTransaction* transaction = connection->CreateTransaction(
      id, scope, blink::mojom::IDBTransactionMode::ReadOnly,
      new IndexedDBFakeBackingStore::FakeTransaction(commit_success));
  db_->RegisterAndScheduleTransaction(transaction);

  // No conflicting transactions, so coordinator will start it immediately:
  EXPECT_EQ(IndexedDBTransaction::STARTED, transaction->state());
  EXPECT_FALSE(transaction->IsTimeoutTimerRunning());

  // Schedule a task - timer won't be started until it's processed.
  transaction->ScheduleTask(
      base::BindOnce(&IndexedDBTransactionTest::DummyOperation,
                     base::Unretained(this), leveldb::Status::OK()));
  EXPECT_FALSE(transaction->IsTimeoutTimerRunning());

  // Transaction is read-only, so no need to time it out.
  RunPostedTasks();
  EXPECT_FALSE(transaction->IsTimeoutTimerRunning());

  // Clean up to avoid leaks.
  transaction->Abort(IndexedDBDatabaseError(
      IndexedDBDatabaseError(blink::mojom::IDBException::kAbortError,
                             "Transaction aborted by user.")));
  EXPECT_EQ(IndexedDBTransaction::FINISHED, transaction->state());
  EXPECT_FALSE(transaction->IsTimeoutTimerRunning());
}

TEST_P(IndexedDBTransactionTestMode, ScheduleNormalTask) {
  const int64_t id = 0;
  const std::set<int64_t> scope;
  const leveldb::Status commit_success = leveldb::Status::OK();
  std::unique_ptr<IndexedDBConnection> connection = CreateConnection();
  IndexedDBTransaction* transaction = connection->CreateTransaction(
      id, scope, GetParam(),
      new IndexedDBFakeBackingStore::FakeTransaction(commit_success));

  EXPECT_FALSE(transaction->HasPendingTasks());
  EXPECT_TRUE(transaction->IsTaskQueueEmpty());
  EXPECT_TRUE(transaction->task_queue_.empty());
  EXPECT_TRUE(transaction->preemptive_task_queue_.empty());
  EXPECT_EQ(0, transaction->diagnostics().tasks_scheduled);
  EXPECT_EQ(0, transaction->diagnostics().tasks_completed);

  db_->RegisterAndScheduleTransaction(transaction);

  EXPECT_FALSE(transaction->HasPendingTasks());
  EXPECT_TRUE(transaction->IsTaskQueueEmpty());
  EXPECT_TRUE(transaction->task_queue_.empty());
  EXPECT_TRUE(transaction->preemptive_task_queue_.empty());

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
  const int64_t id = 0;
  const std::set<int64_t> scope;
  const leveldb::Status commit_success = leveldb::Status::OK();
  std::unique_ptr<IndexedDBConnection> connection = CreateConnection();
  IndexedDBTransaction* transaction = connection->CreateTransaction(
      id, scope, GetParam(),
      new IndexedDBFakeBackingStore::FakeTransaction(commit_success));

  EXPECT_FALSE(transaction->HasPendingTasks());
  EXPECT_TRUE(transaction->IsTaskQueueEmpty());
  EXPECT_TRUE(transaction->task_queue_.empty());
  EXPECT_TRUE(transaction->preemptive_task_queue_.empty());
  EXPECT_EQ(0, transaction->diagnostics().tasks_scheduled);
  EXPECT_EQ(0, transaction->diagnostics().tasks_completed);

  db_->RegisterAndScheduleTransaction(transaction);

  EXPECT_FALSE(transaction->HasPendingTasks());
  EXPECT_TRUE(transaction->IsTaskQueueEmpty());
  EXPECT_TRUE(transaction->task_queue_.empty());
  EXPECT_TRUE(transaction->preemptive_task_queue_.empty());

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
  EXPECT_EQ(IndexedDBTransaction::STARTED, transaction->state());
  EXPECT_EQ(1, transaction->diagnostics().tasks_scheduled);
  EXPECT_EQ(1, transaction->diagnostics().tasks_completed);

  EXPECT_TRUE(error_called_);

  transaction->SetCommitFlag();
  RunPostedTasks();
  EXPECT_EQ(0UL, connection->transactions().size());
}

TEST_F(IndexedDBTransactionTest, SchedulePreemptiveTask) {
  const int64_t id = 0;
  const std::set<int64_t> scope;
  const leveldb::Status commit_failure = leveldb::Status::Corruption("Ouch.");
  std::unique_ptr<IndexedDBConnection> connection = CreateConnection();
  IndexedDBTransaction* transaction = connection->CreateTransaction(
      id, scope, blink::mojom::IDBTransactionMode::VersionChange,
      new IndexedDBFakeBackingStore::FakeTransaction(commit_failure));

  EXPECT_FALSE(transaction->HasPendingTasks());
  EXPECT_TRUE(transaction->IsTaskQueueEmpty());
  EXPECT_TRUE(transaction->task_queue_.empty());
  EXPECT_TRUE(transaction->preemptive_task_queue_.empty());
  EXPECT_EQ(0, transaction->diagnostics().tasks_scheduled);
  EXPECT_EQ(0, transaction->diagnostics().tasks_completed);

  db_->RegisterAndScheduleTransaction(transaction);

  EXPECT_FALSE(transaction->HasPendingTasks());
  EXPECT_TRUE(transaction->IsTaskQueueEmpty());
  EXPECT_TRUE(transaction->task_queue_.empty());
  EXPECT_TRUE(transaction->preemptive_task_queue_.empty());

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
  EXPECT_TRUE(error_called_);
}

TEST_P(IndexedDBTransactionTestMode, AbortTasks) {
  const int64_t id = 0;
  const std::set<int64_t> scope;
  const leveldb::Status commit_failure = leveldb::Status::Corruption("Ouch.");
  std::unique_ptr<IndexedDBConnection> connection = CreateConnection();
  IndexedDBTransaction* transaction = connection->CreateTransaction(
      id, scope, GetParam(),
      new IndexedDBFakeBackingStore::FakeTransaction(commit_failure));
  db_->RegisterAndScheduleTransaction(transaction);

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
  EXPECT_TRUE(error_called_);
}

TEST_P(IndexedDBTransactionTestMode, AbortPreemptive) {
  const int64_t id = 0;
  const std::set<int64_t> scope;
  const leveldb::Status commit_success = leveldb::Status::OK();
  std::unique_ptr<IndexedDBConnection> connection = CreateConnection();
  IndexedDBTransaction* transaction = connection->CreateTransaction(
      id, scope, GetParam(),
      new IndexedDBFakeBackingStore::FakeTransaction(commit_success));
  db_->RegisterAndScheduleTransaction(transaction);

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
    blink::mojom::IDBTransactionMode::ReadWrite,
    blink::mojom::IDBTransactionMode::VersionChange};

INSTANTIATE_TEST_SUITE_P(IndexedDBTransactions,
                         IndexedDBTransactionTestMode,
                         ::testing::ValuesIn(kTestModes));

TEST_F(IndexedDBTransactionTest, AbortCancelsLockRequest) {
  std::u16string name(u"name");
  const int64_t id = 0;
  const int64_t object_store_id = 1ll;
  const std::set<int64_t> scope = {object_store_id};
  std::unique_ptr<IndexedDBConnection> connection = CreateConnection();
  IndexedDBTransaction* transaction = connection->CreateTransaction(
      id, scope, blink::mojom::IDBTransactionMode::ReadWrite,
      new IndexedDBFakeBackingStore::FakeTransaction(leveldb::Status::OK()));

  // Acquire a lock to block the transaction's lock acquisition.
  bool locks_recieved = false;
  std::vector<PartitionedLockManager::PartitionedLockRequest> lock_requests;
  lock_requests.emplace_back(GetDatabaseLockId(name),
                             PartitionedLockManager::LockType::kShared);
  lock_requests.emplace_back(GetObjectStoreLockId(id, object_store_id),
                             PartitionedLockManager::LockType::kExclusive);
  PartitionedLockHolder temp_lock_receiver;
  lock_manager()->AcquireLocks(lock_requests,
                               temp_lock_receiver.weak_factory.GetWeakPtr(),
                               base::BindOnce(SetToTrue, &locks_recieved));
  EXPECT_TRUE(locks_recieved);

  // Register the transaction, which should request locks and wait for
  // `temp_lock_receiver` to release the locks.
  db_->RegisterAndScheduleTransaction(transaction);
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
  locks_recieved = false;
  lock_manager()->AcquireLocks(lock_requests,
                               temp_lock_receiver.weak_factory.GetWeakPtr(),
                               base::BindOnce(SetToTrue, &locks_recieved));
  EXPECT_TRUE(locks_recieved);
}

TEST_F(IndexedDBTransactionTest, PostedStartTaskRunAfterAbort) {
  int64_t id = 0;
  const int64_t object_store_id = 1ll;
  const std::set<int64_t> scope = {object_store_id};
  std::unique_ptr<IndexedDBConnection> connection = CreateConnection();

  IndexedDBTransaction* transaction1 = connection->CreateTransaction(
      id, scope, blink::mojom::IDBTransactionMode::ReadWrite,
      new IndexedDBFakeBackingStore::FakeTransaction(leveldb::Status::OK()));

  db_->RegisterAndScheduleTransaction(transaction1);
  EXPECT_EQ(transaction1->state(), IndexedDBTransaction::STARTED);

  // Register another transaction, which will block on the first transaction.
  IndexedDBTransaction* transaction2 = connection->CreateTransaction(
      ++id, scope, blink::mojom::IDBTransactionMode::ReadWrite,
      new IndexedDBFakeBackingStore::FakeTransaction(leveldb::Status::OK()));

  db_->RegisterAndScheduleTransaction(transaction2);
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

TEST_F(IndexedDBTransactionTest, IsTransactionBlockingOthers) {
  const int64_t id = 0;
  const int64_t object_store_id = 1ll;
  const std::set<int64_t> scope = {object_store_id};
  std::unique_ptr<IndexedDBConnection> connection = CreateConnection();
  IndexedDBTransaction* transaction = connection->CreateTransaction(
      id, scope, blink::mojom::IDBTransactionMode::ReadWrite,
      new IndexedDBFakeBackingStore::FakeTransaction(leveldb::Status::OK()));
  db_->RegisterAndScheduleTransaction(transaction);

  // Register a transaction with ReadWrite mode to object store 1.
  // The transaction should be started and it's not blocking any others.
  EXPECT_EQ(transaction->state(), IndexedDBTransaction::STARTED);
  EXPECT_FALSE(db_->IsTransactionBlockingOthers(transaction));

  const int64_t id2 = 1;
  IndexedDBTransaction* transaction2 = connection->CreateTransaction(
      id2, scope, blink::mojom::IDBTransactionMode::ReadWrite,
      new IndexedDBFakeBackingStore::FakeTransaction(leveldb::Status::OK()));
  db_->RegisterAndScheduleTransaction(transaction2);

  // Register another transaction with ReadWrite mode to the same object store.
  // The transaction should be blocked in `CREATED` state and the previous
  // transaction is now blocking others.
  EXPECT_EQ(transaction2->state(), IndexedDBTransaction::CREATED);
  EXPECT_TRUE(db_->IsTransactionBlockingOthers(transaction));

  RunPostedTasks();

  transaction2->Abort(
      IndexedDBDatabaseError(blink::mojom::IDBException::kUnknownError));

  // Abort the blocked transaction, and the previous transaction should not be
  // blocking others anymore.
  EXPECT_EQ(transaction2->state(), IndexedDBTransaction::FINISHED);
  RunPostedTasks();
  EXPECT_FALSE(db_->IsTransactionBlockingOthers(transaction));
}

}  // namespace indexed_db_transaction_unittest
}  // namespace content
