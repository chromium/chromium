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

#include "base/containers/queue.h"
#include "base/containers/stack.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_id.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_external_object_storage.h"
#include "content/browser/indexed_db/indexed_db_task_helper.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-forward.h"

namespace content {

class IndexedDBCursor;
class IndexedDBDatabaseCallbacks;

class CONTENT_EXPORT IndexedDBTransaction
    : public blink::mojom::IDBTransaction {
 public:
  using Operation = base::OnceCallback<leveldb::Status(IndexedDBTransaction*)>;
  using AbortOperation = base::OnceClosure;

  enum State {
    CREATED,     // Created, but not yet started by coordinator.
    STARTED,     // Started by the coordinator.
    COMMITTING,  // In the process of committing, possibly waiting for blobs
                 // to be written.
    FINISHED,    // Either aborted or committed.
  };

  static void DisableInactivityTimeoutForTesting();

  IndexedDBTransaction(
      int64_t id,
      IndexedDBConnection* connection,
      const std::set<int64_t>& object_store_ids,
      blink::mojom::IDBTransactionMode mode,
      IndexedDBBucketContextHandle bucket_context,
      IndexedDBBackingStore::Transaction* backing_store_transaction);
  ~IndexedDBTransaction() override;

  void BindReceiver(
      mojo::PendingAssociatedReceiver<blink::mojom::IDBTransaction>
          mojo_receiver);

  // Signals the transaction for commit.
  void SetCommitFlag();

  // Returns false if the transaction has been signalled to commit, is in the
  // process of committing, or finished committing or was aborted. Essentially
  // when this returns false no tasks should be scheduled that try to modify
  // the transaction.
  // TODO(https://crbug.com/1249908): If the transaction was already committed
  // (or is in the process of being committed), and this object receives a new
  // Mojo message, we should kill the renderer. This branch however also
  // includes cases where the browser process aborted the transaction, as
  // currently we don't distinguish that state from the transaction having been
  // committed. So for now simply ignore the request.
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

  enum class RunTasksResult { kError, kNotFinished, kCommitted, kAborted };
  std::tuple<RunTasksResult, leveldb::Status> RunTasks();

  IndexedDBBackingStore::Transaction* BackingStoreTransaction() {
    return backing_store_transaction_.get();
  }
  int64_t id() const { return id_; }

  base::WeakPtr<IndexedDBDatabase> database() const { return database_; }
  IndexedDBDatabaseCallbacks* callbacks() const {
    return connection()->callbacks();
  }
  IndexedDBConnection* connection() const { return connection_.get(); }
  bool is_commit_pending() const { return is_commit_pending_; }
  int64_t num_errors_sent() const { return num_errors_sent_; }
  int64_t num_errors_handled() const { return num_errors_handled_; }
  void IncrementNumErrorsSent() { ++num_errors_sent_; }

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

  base::WeakPtr<IndexedDBTransaction> AsWeakPtr() {
    return ptr_factory_.GetWeakPtr();
  }

  IndexedDBBucketContext* bucket_context() {
    return bucket_context_.bucket_context();
  }

  const base::flat_set<PartitionedLockId> lock_ids() const { return lock_ids_; }
  PartitionedLockHolder* mutable_locks_receiver() { return &locks_receiver_; }

  // in_flight_memory() is used to keep track of all memory scheduled to be
  // written using ScheduleTask. This is reported to memory dumps.
  base::CheckedNumeric<size_t>& in_flight_memory() { return in_flight_memory_; }

 private:
  friend class IndexedDBClassFactory;
  friend class IndexedDBConnection;
  friend class base::RefCounted<IndexedDBTransaction>;

  FRIEND_TEST_ALL_PREFIXES(IndexedDBTransactionTestMode, AbortPreemptive);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBTransactionTestMode, AbortTasks);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBTransactionTest, NoTimeoutReadOnly);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBTransactionTest, SchedulePreemptiveTask);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBTransactionTestMode, ScheduleNormalTask);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBTransactionTestMode, TaskFails);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBTransactionTest, Timeout);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBTransactionTest, TimeoutPreemptive);

  // blink::mojom::IDBTransaction:
  void CreateObjectStore(int64_t object_store_id,
                         const std::u16string& name,
                         const blink::IndexedDBKeyPath& key_path,
                         bool auto_increment) override;
  void DeleteObjectStore(int64_t object_store_id) override;
  void Put(int64_t object_store_id,
           blink::mojom::IDBValuePtr value,
           const blink::IndexedDBKey& key,
           blink::mojom::IDBPutMode mode,
           const std::vector<blink::IndexedDBIndexKeys>& index_keys,
           blink::mojom::IDBTransaction::PutCallback callback) override;
  void Commit(int64_t num_errors_handled) override;

  void OnQuotaCheckDone(bool allowed);

  // Turns an IDBValue into a set of IndexedDBExternalObjects in
  // |external_objects|.
  uint64_t CreateExternalObjects(
      blink::mojom::IDBValuePtr& value,
      std::vector<IndexedDBExternalObject>* external_objects);

  leveldb::Status DoPendingCommit();

  // Helper for posting a task to call IndexedDBTransaction::CommitPhaseTwo when
  // we know the transaction had no requests and therefore the commit must
  // succeed.
  static leveldb::Status CommitPhaseTwoProxy(IndexedDBTransaction* transaction);

  bool IsTaskQueueEmpty() const;
  bool HasPendingTasks() const;

  leveldb::Status BlobWriteComplete(
      BlobWriteResult result,
      storage::mojom::WriteBlobToFileResult error);
  void CloseOpenCursors();
  leveldb::Status CommitPhaseTwo();
  void Timeout();

  const int64_t id_;
  const std::set<int64_t> object_store_ids_;
  const blink::mojom::IDBTransactionMode mode_;

  bool used_ = false;
  State state_ = CREATED;
  base::flat_set<PartitionedLockId> lock_ids_;
  PartitionedLockHolder locks_receiver_;
  bool is_commit_pending_ = false;

  // We are owned by the connection object, but during force closes sometimes
  // there are issues if there is a pending OpenRequest. So use a WeakPtr.
  base::WeakPtr<IndexedDBConnection> connection_;
  base::WeakPtr<IndexedDBDatabase> database_;

  IndexedDBBucketContextHandle bucket_context_;

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

  std::unique_ptr<IndexedDBBackingStore::Transaction>
      backing_store_transaction_;
  bool backing_store_transaction_begun_ = false;

  int pending_preemptive_events_ = 0;
  bool processing_event_queue_ = false;
  bool aborted_ = false;

  int64_t num_errors_sent_ = 0;
  int64_t num_errors_handled_ = 0;

  // In bytes, the estimated additional space used on disk after this
  // transaction is committed. Note that this is a very approximate view of the
  // changes associated with this transaction:
  //
  //   * It ignores the additional overhead needed for meta records such as
  //     object stores.
  //   * It ignores compression which may be applied before rows are flushed to
  //     disk.
  //   * It ignores space freed up by deletions, which currently flow through
  //     DatabaseImpl::DeleteRange(), and which can't easily be calculated a
  //     priori.
  //
  // As such, it's only useful as a rough upper bound for the amount of
  // additional space required by this transaction, used to abandon transactions
  // that would likely exceed quota caps, but not used to calculate ultimate
  // quota usage.
  //
  // See crbug.com/1493696 for discussion of how this should be improved.
  int64_t preliminary_size_estimate_ = 0;

  std::set<IndexedDBCursor*> open_cursors_;

  // This timer is started after requests have been processed. If no subsequent
  // requests are processed before the timer fires, assume the script is
  // unresponsive and abort to unblock the transaction queue.
  // TODO(crbug.com/1474996): this will not be necessary when each backing store
  // has its own task runner.
  base::OneShotTimer timeout_timer_;

  Diagnostics diagnostics_;

  mojo::AssociatedReceiver<blink::mojom::IDBTransaction> receiver_;

  base::WeakPtrFactory<IndexedDBTransaction> ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_TRANSACTION_H_
