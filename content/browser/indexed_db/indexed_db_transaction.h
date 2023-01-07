// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_TRANSACTION_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_TRANSACTION_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <tuple>

#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/containers/stack.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_external_object_storage.h"
#include "content/browser/indexed_db/indexed_db_task_helper.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/indexeddb/web_idb_types.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-forward.h"

namespace content {

class IndexedDBCursor;
class IndexedDBDatabaseCallbacks;

namespace indexed_db_transaction_unittest {
class IndexedDBTransactionTestMode;
class IndexedDBTransactionTest;
FORWARD_DECLARE_TEST(IndexedDBTransactionTestMode, AbortPreemptive);
FORWARD_DECLARE_TEST(IndexedDBTransactionTestMode, AbortTasks);
FORWARD_DECLARE_TEST(IndexedDBTransactionTest, NoTimeoutReadOnly);
FORWARD_DECLARE_TEST(IndexedDBTransactionTest, SchedulePreemptiveTask);
FORWARD_DECLARE_TEST(IndexedDBTransactionTestMode, ScheduleNormalTask);
FORWARD_DECLARE_TEST(IndexedDBTransactionTestMode, TaskFails);
FORWARD_DECLARE_TEST(IndexedDBTransactionTest, Timeout);
FORWARD_DECLARE_TEST(IndexedDBTransactionTest, TimeoutPreemptive);
}  // namespace indexed_db_transaction_unittest

class CONTENT_EXPORT IndexedDBTransaction {
 public:
  using TearDownCallback = base::RepeatingCallback<void(leveldb::Status)>;
  using Operation = base::OnceCallback<leveldb::Status(IndexedDBTransaction*)>;
  using AbortOperation = base::OnceClosure;

  enum State {
    CREATED,     // Created, but not yet started by coordinator.
    STARTED,     // Started by the coordinator.
    COMMITTING,  // In the process of committing, possibly waiting for blobs
                 // to be written.
    FINISHED,    // Either aborted or committed.
  };

  virtual ~IndexedDBTransaction();

  // Signals the transaction for commit.
  void SetCommitFlag();

  // Returns false if the transaction has been signalled to commit, is in the
  // process of committing, or finished committing or was aborted. Essentially
  // when this returns false no tasks should be scheduled that try to modify
  // the transaction.
  bool IsAcceptingRequests() {
    return !is_commit_pending_ && state_ != COMMITTING && state_ != FINISHED;
  }

  // This transaction is ultimately backed by a LevelDBScope. Aborting a
  // transaction rolls back the LevelDBScopes, which (if LevelDBScopes is in
  // single-sequence mode) can fail. This returns the result of that rollback,
  // if applicable.
  leveldb::Status Abort(const IndexedDBDatabaseError& error);

  // Called by the scopes lock manager when this transaction is unblocked.
  void Start();

  blink::mojom::IDBTransactionMode mode() const { return mode_; }
  const std::set<int64_t>& scope() const { return object_store_ids_; }

  void ScheduleTask(Operation task) {
    ScheduleTask(blink::mojom::IDBTaskType::Normal, std::move(task));
  }
  void ScheduleTask(blink::mojom::IDBTaskType, Operation task);
  void ScheduleAbortTask(AbortOperation abort_task);
  void RegisterOpenCursor(IndexedDBCursor* cursor);
  void UnregisterOpenCursor(IndexedDBCursor* cursor);
  void AddPreemptiveEvent() { pending_preemptive_events_++; }
  void DidCompletePreemptiveEvent() {
    pending_preemptive_events_--;
    DCHECK_GE(pending_preemptive_events_, 0);
  }

  void EnsureBackingStoreTransactionBegun();

  enum class RunTasksResult { kError, kNotFinished, kCommitted, kAborted };
  std::tuple<RunTasksResult, leveldb::Status> RunTasks();

  IndexedDBBackingStore::Transaction* BackingStoreTransaction() {
    return transaction_.get();
  }
  int64_t id() const { return id_; }

  base::WeakPtr<IndexedDBDatabase> database() const { return database_; }
  IndexedDBDatabaseCallbacks* callbacks() const { return callbacks_.get(); }
  IndexedDBConnection* connection() const { return connection_.get(); }
  bool is_commit_pending() const { return is_commit_pending_; }
  int64_t num_errors_sent() const { return num_errors_sent_; }
  int64_t num_errors_handled() const { return num_errors_handled_; }
  void IncrementNumErrorsSent() { ++num_errors_sent_; }
  void SetNumErrorsHandled(int64_t num_errors_handled) {
    num_errors_handled_ = num_errors_handled;
  }

  State state() const { return state_; }
  bool aborted() const { return aborted_; }
  bool IsTimeoutTimerRunning() const { return timeout_timer_.IsRunning(); }

  struct Diagnostics {
    base::Time creation_time;
    base::Time start_time;
    int tasks_scheduled;
    int tasks_completed;
  };

  const Diagnostics& diagnostics() const { return diagnostics_; }

  void set_size(int64_t size) { size_ = size; }
  int64_t size() const { return size_; }

  base::WeakPtr<IndexedDBTransaction> AsWeakPtr() {
    return ptr_factory_.GetWeakPtr();
  }

  PartitionedLockHolder* mutable_locks_receiver() { return &locks_receiver_; }

  // in_flight_memory() is used to keep track of all memory scheduled to be
  // written using ScheduleTask. This is reported to memory dumps.
  base::CheckedNumeric<size_t>& in_flight_memory() { return in_flight_memory_; }

 protected:
  // Test classes may derive, but most creation should be done via
  // IndexedDBClassFactory.
  IndexedDBTransaction(
      int64_t id,
      IndexedDBConnection* connection,
      const std::set<int64_t>& object_store_ids,
      blink::mojom::IDBTransactionMode mode,
      TasksAvailableCallback tasks_available_callback,
      TearDownCallback tear_down_callback,
      IndexedDBBackingStore::Transaction* backing_store_transaction);

  // May be overridden in tests.
  virtual base::TimeDelta GetInactivityTimeout() const;

 private:
  friend class IndexedDBClassFactory;
  friend class IndexedDBConnection;
  friend class base::RefCounted<IndexedDBTransaction>;

  FRIEND_TEST_ALL_PREFIXES(
      indexed_db_transaction_unittest::IndexedDBTransactionTestMode,
      AbortPreemptive);
  FRIEND_TEST_ALL_PREFIXES(
      indexed_db_transaction_unittest::IndexedDBTransactionTestMode,
      AbortTasks);
  FRIEND_TEST_ALL_PREFIXES(
      indexed_db_transaction_unittest::IndexedDBTransactionTest,
      NoTimeoutReadOnly);
  FRIEND_TEST_ALL_PREFIXES(
      indexed_db_transaction_unittest::IndexedDBTransactionTest,
      SchedulePreemptiveTask);
  FRIEND_TEST_ALL_PREFIXES(
      indexed_db_transaction_unittest::IndexedDBTransactionTestMode,
      ScheduleNormalTask);
  FRIEND_TEST_ALL_PREFIXES(
      indexed_db_transaction_unittest::IndexedDBTransactionTestMode,
      TaskFails);
  FRIEND_TEST_ALL_PREFIXES(
      indexed_db_transaction_unittest::IndexedDBTransactionTest,
      Timeout);
  FRIEND_TEST_ALL_PREFIXES(
      indexed_db_transaction_unittest::IndexedDBTransactionTest,
      TimeoutPreemptive);

  leveldb::Status Commit();

  // Helper for posting a task to call IndexedDBTransaction::CommitPhaseTwo when
  // we know the transaction had no requests and therefore the commit must
  // succeed.
  static leveldb::Status CommitPhaseTwoProxy(IndexedDBTransaction* transaction);

  bool IsTaskQueueEmpty() const;
  bool HasPendingTasks() const;

  leveldb::Status BlobWriteComplete(
      BlobWriteResult result,
      storage::mojom::WriteBlobToFileResult error);
  void CloseOpenCursorBindings();
  void CloseOpenCursors();
  leveldb::Status CommitPhaseTwo();
  void Timeout();

  const int64_t id_;
  const std::set<int64_t> object_store_ids_;
  const blink::mojom::IDBTransactionMode mode_;

  bool used_ = false;
  State state_ = CREATED;
  PartitionedLockHolder locks_receiver_;
  bool is_commit_pending_ = false;

  // We are owned by the connection object, but during force closes sometimes
  // there are issues if there is a pending OpenRequest. So use a WeakPtr.
  base::WeakPtr<IndexedDBConnection> connection_;
  scoped_refptr<IndexedDBDatabaseCallbacks> callbacks_;
  base::WeakPtr<IndexedDBDatabase> database_;
  TasksAvailableCallback run_tasks_callback_;
  // Note: calling this will tear down the IndexedDBOriginState (and probably
  // destroy this object).
  TearDownCallback tear_down_callback_;

  // Metrics for quota.
  int64_t size_ = 0;

  base::CheckedNumeric<size_t> in_flight_memory_ = 0;

  class TaskQueue {
   public:
    TaskQueue();

    TaskQueue(const TaskQueue&) = delete;
    TaskQueue& operator=(const TaskQueue&) = delete;

    ~TaskQueue();
    bool empty() const { return queue_.empty(); }
    void push(Operation task) { queue_.push(std::move(task)); }
    Operation pop();
    void clear();

   private:
    base::queue<Operation> queue_;
  };

  class TaskStack {
   public:
    TaskStack();

    TaskStack(const TaskStack&) = delete;
    TaskStack& operator=(const TaskStack&) = delete;

    ~TaskStack();
    bool empty() const { return stack_.empty(); }
    void push(AbortOperation task) { stack_.push(std::move(task)); }
    AbortOperation pop();
    void clear();

   private:
    base::stack<AbortOperation> stack_;
  };

  TaskQueue task_queue_;
  TaskQueue preemptive_task_queue_;
  TaskStack abort_task_stack_;

  std::unique_ptr<IndexedDBBackingStore::Transaction> transaction_;
  bool backing_store_transaction_begun_ = false;

  int pending_preemptive_events_ = 0;
  bool processing_event_queue_ = false;
  bool aborted_ = false;

  int64_t num_errors_sent_ = 0;
  int64_t num_errors_handled_ = 0;

  std::set<IndexedDBCursor*> open_cursors_;

  // This timer is started after requests have been processed. If no subsequent
  // requests are processed before the timer fires, assume the script is
  // unresponsive and abort to unblock the transaction queue.
  base::OneShotTimer timeout_timer_;

  Diagnostics diagnostics_;

  base::WeakPtrFactory<IndexedDBTransaction> ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_TRANSACTION_H_
