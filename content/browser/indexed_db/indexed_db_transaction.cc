// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_transaction.h"

#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/base_tracing.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_cursor.h"
#include "content/browser/indexed_db/indexed_db_database.h"
#include "content/browser/indexed_db/indexed_db_database_callbacks.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"
#include "third_party/leveldatabase/env_chromium.h"

namespace content {

namespace {

std::string WriteBlobToFileResultToString(
    storage::mojom::WriteBlobToFileResult result) {
  switch (result) {
    case storage::mojom::WriteBlobToFileResult::kError:
      return "Error";
    case storage::mojom::WriteBlobToFileResult::kBadPath:
      return "BadPath";
    case storage::mojom::WriteBlobToFileResult::kInvalidBlob:
      return "InvalidBlob";
    case storage::mojom::WriteBlobToFileResult::kIOError:
      return "IOError";
    case storage::mojom::WriteBlobToFileResult::kTimestampError:
      return "TimestampError";
    case storage::mojom::WriteBlobToFileResult::kSuccess:
      return "Success";
  }
  NOTREACHED();
  return "";
}

const int64_t kInactivityTimeoutPeriodSeconds = 60;

// Used for UMA metrics - do not change values.
enum UmaIDBException {
  UmaIDBExceptionUnknownError = 0,
  UmaIDBExceptionConstraintError = 1,
  UmaIDBExceptionDataError = 2,
  UmaIDBExceptionVersionError = 3,
  UmaIDBExceptionAbortError = 4,
  UmaIDBExceptionQuotaError = 5,
  UmaIDBExceptionTimeoutError = 6,
  UmaIDBExceptionExclusiveMaxValue = 7
};

// Used for UMA metrics - do not change mappings.
UmaIDBException ExceptionCodeToUmaEnum(blink::mojom::IDBException code) {
  switch (code) {
    case blink::mojom::IDBException::kUnknownError:
      return UmaIDBExceptionUnknownError;
    case blink::mojom::IDBException::kConstraintError:
      return UmaIDBExceptionConstraintError;
    case blink::mojom::IDBException::kDataError:
      return UmaIDBExceptionDataError;
    case blink::mojom::IDBException::kVersionError:
      return UmaIDBExceptionVersionError;
    case blink::mojom::IDBException::kAbortError:
      return UmaIDBExceptionAbortError;
    case blink::mojom::IDBException::kQuotaError:
      return UmaIDBExceptionQuotaError;
    case blink::mojom::IDBException::kTimeoutError:
      return UmaIDBExceptionTimeoutError;
    default:
      NOTREACHED();
  }
  return UmaIDBExceptionUnknownError;
}

}  // namespace

IndexedDBTransaction::TaskQueue::TaskQueue() = default;
IndexedDBTransaction::TaskQueue::~TaskQueue() = default;

void IndexedDBTransaction::TaskQueue::clear() {
  while (!queue_.empty())
    queue_.pop();
}

IndexedDBTransaction::Operation IndexedDBTransaction::TaskQueue::pop() {
  DCHECK(!queue_.empty());
  Operation task = std::move(queue_.front());
  queue_.pop();
  return task;
}

IndexedDBTransaction::TaskStack::TaskStack() = default;
IndexedDBTransaction::TaskStack::~TaskStack() = default;

void IndexedDBTransaction::TaskStack::clear() {
  while (!stack_.empty())
    stack_.pop();
}

IndexedDBTransaction::AbortOperation IndexedDBTransaction::TaskStack::pop() {
  DCHECK(!stack_.empty());
  AbortOperation task = std::move(stack_.top());
  stack_.pop();
  return task;
}

IndexedDBTransaction::IndexedDBTransaction(
    int64_t id,
    IndexedDBConnection* connection,
    const std::set<int64_t>& object_store_ids,
    blink::mojom::IDBTransactionMode mode,
    TasksAvailableCallback tasks_available_callback,
    TearDownCallback tear_down_callback,
    IndexedDBBackingStore::Transaction* backing_store_transaction)
    : id_(id),
      object_store_ids_(object_store_ids),
      mode_(mode),
      connection_(connection->GetWeakPtr()),
      run_tasks_callback_(std::move(tasks_available_callback)),
      tear_down_callback_(std::move(tear_down_callback)),
      transaction_(backing_store_transaction) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("IndexedDB",
                                    "IndexedDBTransaction::lifetime", this);
  callbacks_ = connection_->callbacks();
  database_ = connection_->database();
  if (database_) {
    database_->TransactionCreated();

    for (const PartitionedLockManager::PartitionedLockRequest& lock_request :
         database_->BuildLockRequestsFromTransaction(this)) {
      lock_ids_.insert(lock_request.lock_id);
    }
  }

  diagnostics_.tasks_scheduled = 0;
  diagnostics_.tasks_completed = 0;
  diagnostics_.creation_time = base::Time::Now();
}

IndexedDBTransaction::~IndexedDBTransaction() {
  TRACE_EVENT_NESTABLE_ASYNC_END0("IndexedDB", "IndexedDBTransaction::lifetime",
                                  this);
  // It shouldn't be possible for this object to get deleted until it's either
  // complete or aborted.
  DCHECK_EQ(state_, FINISHED);
  DCHECK(preemptive_task_queue_.empty());
  DCHECK_EQ(pending_preemptive_events_, 0);
  DCHECK(task_queue_.empty());
  DCHECK(abort_task_stack_.empty());
  DCHECK(!processing_event_queue_);
}

void IndexedDBTransaction::SetCommitFlag() {
  is_commit_pending_ = true;
  run_tasks_callback_.Run();
}

void IndexedDBTransaction::ScheduleTask(blink::mojom::IDBTaskType type,
                                        Operation task) {
  if (state_ == FINISHED)
    return;

  timeout_timer_.Stop();
  used_ = true;
  if (type == blink::mojom::IDBTaskType::Normal) {
    task_queue_.push(std::move(task));
    ++diagnostics_.tasks_scheduled;
  } else {
    preemptive_task_queue_.push(std::move(task));
  }
  if (state() == STARTED)
    run_tasks_callback_.Run();
}

void IndexedDBTransaction::ScheduleAbortTask(AbortOperation abort_task) {
  DCHECK_NE(FINISHED, state_);
  DCHECK(used_);
  abort_task_stack_.push(std::move(abort_task));
}

leveldb::Status IndexedDBTransaction::Abort(
    const IndexedDBDatabaseError& error) {
  if (state_ == FINISHED)
    return leveldb::Status::OK();

  base::UmaHistogramEnumeration("WebCore.IndexedDB.TransactionAbortReason",
                                ExceptionCodeToUmaEnum(error.code()),
                                UmaIDBExceptionExclusiveMaxValue);

  aborted_ = true;
  timeout_timer_.Stop();

  state_ = FINISHED;

  if (backing_store_transaction_begun_) {
    leveldb::Status status = transaction_->Rollback();
    if (!status.ok())
      return status;
  }

  // Run the abort tasks, if any.
  while (!abort_task_stack_.empty())
    abort_task_stack_.pop().Run();

  preemptive_task_queue_.clear();
  pending_preemptive_events_ = 0;

  // Backing store resources (held via cursors) must be released
  // before script callbacks are fired, as the script callbacks may
  // release references and allow the backing store itself to be
  // released, and order is critical.
  CloseOpenCursorBindings();

  // Open cursors have to be deleted before we clear the task queue.
  // If we clear the task queue and closures exist in it that refer
  // to callbacks associated with the cursor mojo bindings, the callback
  // deletion will fail due to a mojo assert.  |CloseOpenCursorBindings()|
  // above will clear the binding, which also deletes the owned
  // |IndexedDBCursor| objects.  After that, we can safely clear the
  // task queue.
  task_queue_.clear();

  transaction_->Reset();

  // Transactions must also be marked as completed before the
  // front-end is notified, as the transaction completion unblocks
  // operations like closing connections.
  locks_receiver_.locks.clear();
  locks_receiver_.AbortLockRequest();

  if (callbacks_.get())
    callbacks_->OnAbort(*this, error);

  if (database_)
    database_->TransactionFinished(mode_, false);
  run_tasks_callback_.Run();
  return leveldb::Status::OK();
}

// static
leveldb::Status IndexedDBTransaction::CommitPhaseTwoProxy(
    IndexedDBTransaction* transaction) {
  return transaction->CommitPhaseTwo();
}

bool IndexedDBTransaction::IsTaskQueueEmpty() const {
  return preemptive_task_queue_.empty() && task_queue_.empty();
}

bool IndexedDBTransaction::HasPendingTasks() const {
  return pending_preemptive_events_ || !IsTaskQueueEmpty();
}

void IndexedDBTransaction::RegisterOpenCursor(IndexedDBCursor* cursor) {
  open_cursors_.insert(cursor);
}

void IndexedDBTransaction::UnregisterOpenCursor(IndexedDBCursor* cursor) {
  open_cursors_.erase(cursor);
}

void IndexedDBTransaction::Start() {
  // The transaction has the potential to be aborted after the Start() task was
  // posted.
  if (state_ == FINISHED) {
    DCHECK(locks_receiver_.locks.empty());
    return;
  }
  DCHECK_EQ(CREATED, state_);
  state_ = STARTED;
  DCHECK(!locks_receiver_.locks.empty());
  diagnostics_.start_time = base::Time::Now();
  run_tasks_callback_.Run();
}

void IndexedDBTransaction::EnsureBackingStoreTransactionBegun() {
  if (!backing_store_transaction_begun_) {
    transaction_->Begin(std::move(locks_receiver_.locks));
    backing_store_transaction_begun_ = true;
  }
}

leveldb::Status IndexedDBTransaction::BlobWriteComplete(
    BlobWriteResult result,
    storage::mojom::WriteBlobToFileResult error) {
  TRACE_EVENT0("IndexedDB", "IndexedDBTransaction::BlobWriteComplete");
  if (state_ == FINISHED)  // aborted
    return leveldb::Status::OK();
  DCHECK_EQ(state_, COMMITTING);

  switch (result) {
    case BlobWriteResult::kFailure: {
      leveldb::Status status = Abort(IndexedDBDatabaseError(
          blink::mojom::IDBException::kDataError,
          base::ASCIIToUTF16(base::StringPrintf(
              "Failed to write blobs (%s)",
              WriteBlobToFileResultToString(error).c_str()))));
      if (!status.ok())
        tear_down_callback_.Run(status);
      // The result is ignored.
      return leveldb::Status::OK();
    }
    case BlobWriteResult::kRunPhaseTwoAsync:
      ScheduleTask(base::BindOnce(&CommitPhaseTwoProxy));
      run_tasks_callback_.Run();
      return leveldb::Status::OK();
    case BlobWriteResult::kRunPhaseTwoAndReturnResult: {
      return CommitPhaseTwo();
    }
  }
  NOTREACHED();
}

leveldb::Status IndexedDBTransaction::Commit() {
  TRACE_EVENT1("IndexedDB", "IndexedDBTransaction::Commit", "txn.id", id());

  timeout_timer_.Stop();

  // In multiprocess ports, front-end may have requested a commit but
  // an abort has already been initiated asynchronously by the
  // back-end.
  if (state_ == FINISHED)
    return leveldb::Status::OK();
  DCHECK_NE(state_, COMMITTING);

  is_commit_pending_ = true;

  // Front-end has requested a commit, but this transaction is blocked by
  // other transactions. The commit will be initiated when the transaction
  // coordinator unblocks this transaction.
  if (state_ != STARTED)
    return leveldb::Status::OK();

  // Front-end has requested a commit, but there may be tasks like
  // create_index which are considered synchronous by the front-end
  // but are processed asynchronously.
  if (HasPendingTasks())
    return leveldb::Status::OK();

  // If a transaction is being committed but it has sent more errors to the
  // front end than have been handled at this point, the transaction should be
  // aborted as it is unknown whether or not any errors unaccounted for will be
  // properly handled.
  if (num_errors_sent_ != num_errors_handled_) {
    is_commit_pending_ = false;
    return Abort(
        IndexedDBDatabaseError(blink::mojom::IDBException::kUnknownError));
  }

  state_ = COMMITTING;

  leveldb::Status s;
  if (!used_) {
    s = CommitPhaseTwo();
  } else {
    // CommitPhaseOne will call the callback synchronously if there are no blobs
    // to write.
    s = transaction_->CommitPhaseOne(base::BindOnce(
        [](base::WeakPtr<IndexedDBTransaction> transaction,
           BlobWriteResult result,
           storage::mojom::WriteBlobToFileResult error) {
          if (!transaction)
            return leveldb::Status::OK();
          return transaction->BlobWriteComplete(result, error);
        },
        ptr_factory_.GetWeakPtr()));
  }

  return s;
}

leveldb::Status IndexedDBTransaction::CommitPhaseTwo() {
  // Abort may have been called just as the blob write completed.
  if (state_ == FINISHED)
    return leveldb::Status::OK();

  DCHECK_EQ(state_, COMMITTING);

  state_ = FINISHED;

  leveldb::Status s;
  bool committed;
  if (!used_) {
    committed = true;
  } else {
    base::TimeDelta active_time = base::Time::Now() - diagnostics_.start_time;
    uint64_t size_kb = transaction_->GetTransactionSize() / 1024;
    // All histograms record 1KB to 1GB.
    switch (mode_) {
      case blink::mojom::IDBTransactionMode::ReadOnly:
        UMA_HISTOGRAM_MEDIUM_TIMES(
            "WebCore.IndexedDB.Transaction.ReadOnly.TimeActive", active_time);
        UMA_HISTOGRAM_COUNTS_1M(
            "WebCore.IndexedDB.Transaction.ReadOnly.SizeOnCommit2", size_kb);
        break;
      case blink::mojom::IDBTransactionMode::ReadWrite:
        UMA_HISTOGRAM_MEDIUM_TIMES(
            "WebCore.IndexedDB.Transaction.ReadWrite.TimeActive", active_time);
        UMA_HISTOGRAM_COUNTS_1M(
            "WebCore.IndexedDB.Transaction.ReadWrite.SizeOnCommit2", size_kb);
        break;
      case blink::mojom::IDBTransactionMode::VersionChange:
        UMA_HISTOGRAM_MEDIUM_TIMES(
            "WebCore.IndexedDB.Transaction.VersionChange.TimeActive",
            active_time);
        UMA_HISTOGRAM_COUNTS_1M(
            "WebCore.IndexedDB.Transaction.VersionChange.SizeOnCommit2",
            size_kb);
        break;
      default:
        NOTREACHED();
    }

    s = transaction_->CommitPhaseTwo();
    committed = s.ok();
  }

  // Backing store resources (held via cursors) must be released
  // before script callbacks are fired, as the script callbacks may
  // release references and allow the backing store itself to be
  // released, and order is critical.
  CloseOpenCursors();
  transaction_->Reset();

  // Transactions must also be marked as completed before the
  // front-end is notified, as the transaction completion unblocks
  // operations like closing connections.
  locks_receiver_.locks.clear();

  if (committed) {
    abort_task_stack_.clear();

    {
      TRACE_EVENT1(
          "IndexedDB",
          "IndexedDBTransaction::CommitPhaseTwo.TransactionCompleteCallbacks",
          "txn.id", id());
      callbacks_->OnComplete(*this);
    }
    if (database_)
      database_->TransactionFinished(mode_, true);
    return s;
  } else {
    while (!abort_task_stack_.empty())
      abort_task_stack_.pop().Run();

    IndexedDBDatabaseError error;
    if (leveldb_env::IndicatesDiskFull(s)) {
      error = IndexedDBDatabaseError(
          blink::mojom::IDBException::kQuotaError,
          "Encountered disk full while committing transaction.");
    } else {
      error = IndexedDBDatabaseError(blink::mojom::IDBException::kUnknownError,
                                     "Internal error committing transaction.");
    }
    callbacks_->OnAbort(*this, error);
    if (database_)
      database_->TransactionFinished(mode_, false);
  }
  return s;
}

std::tuple<IndexedDBTransaction::RunTasksResult, leveldb::Status>
IndexedDBTransaction::RunTasks() {
  TRACE_EVENT1("IndexedDB", "IndexedDBTransaction::RunTasks", "txn.id", id());

  DCHECK(!processing_event_queue_);

  // May have been aborted.
  if (aborted_)
    return {RunTasksResult::kAborted, leveldb::Status::OK()};
  if (IsTaskQueueEmpty() && !is_commit_pending_)
    return {RunTasksResult::kNotFinished, leveldb::Status::OK()};

  processing_event_queue_ = true;

  if (!backing_store_transaction_begun_) {
    transaction_->Begin(std::move(locks_receiver_.locks));
    backing_store_transaction_begun_ = true;
  }

  bool run_preemptive_queue =
      !preemptive_task_queue_.empty() || pending_preemptive_events_ != 0;
  TaskQueue* task_queue =
      run_preemptive_queue ? &preemptive_task_queue_ : &task_queue_;
  while (!task_queue->empty() && state_ != FINISHED) {
    DCHECK(state_ == STARTED || state_ == COMMITTING) << state_;
    Operation task(task_queue->pop());
    leveldb::Status result = std::move(task).Run(this);
    if (!run_preemptive_queue) {
      DCHECK(diagnostics_.tasks_completed < diagnostics_.tasks_scheduled);
      ++diagnostics_.tasks_completed;
    }
    if (!result.ok()) {
      processing_event_queue_ = false;
      return {
          RunTasksResult::kError,
          result,
      };
    }

    run_preemptive_queue =
        !preemptive_task_queue_.empty() || pending_preemptive_events_ != 0;
    // Event itself may change which queue should be processed next.
    task_queue = run_preemptive_queue ? &preemptive_task_queue_ : &task_queue_;
  }

  // If there are no pending tasks, we haven't already committed/aborted,
  // and the front-end requested a commit, it is now safe to do so.
  if (!HasPendingTasks() && state_ == STARTED && is_commit_pending_) {
    processing_event_queue_ = false;
    // This can delete |this|.
    leveldb::Status result = Commit();
    if (!result.ok())
      return {RunTasksResult::kError, result};
  }

  // The transaction may have been aborted while processing tasks.
  if (state_ == FINISHED) {
    processing_event_queue_ = false;
    return {aborted_ ? RunTasksResult::kAborted : RunTasksResult::kCommitted,
            leveldb::Status::OK()};
  }

  DCHECK(state_ == STARTED || state_ == COMMITTING) << state_;

  // Otherwise, start a timer in case the front-end gets wedged and
  // never requests further activity. Read-only transactions don't
  // block other transactions, so don't time those out.
  if (!HasPendingTasks() &&
      mode_ != blink::mojom::IDBTransactionMode::ReadOnly &&
      state_ == STARTED) {
    timeout_timer_.Start(FROM_HERE, GetInactivityTimeout(),
                         base::BindOnce(&IndexedDBTransaction::Timeout,
                                        ptr_factory_.GetWeakPtr()));
  }
  processing_event_queue_ = false;
  return {RunTasksResult::kNotFinished, leveldb::Status::OK()};
}

base::TimeDelta IndexedDBTransaction::GetInactivityTimeout() const {
  return base::Seconds(kInactivityTimeoutPeriodSeconds);
}

void IndexedDBTransaction::Timeout() {
  leveldb::Status result = Abort(
      IndexedDBDatabaseError(blink::mojom::IDBException::kTimeoutError,
                             u"Transaction timed out due to inactivity."));
  if (!result.ok())
    tear_down_callback_.Run(result);
}

void IndexedDBTransaction::CloseOpenCursorBindings() {
  TRACE_EVENT1("IndexedDB", "IndexedDBTransaction::CloseOpenCursorBindings",
               "txn.id", id());
  std::vector<IndexedDBCursor*> cursor_ptrs(open_cursors_.begin(),
                                            open_cursors_.end());
  for (auto* cursor_ptr : cursor_ptrs)
    cursor_ptr->RemoveBinding();
}

void IndexedDBTransaction::CloseOpenCursors() {
  TRACE_EVENT1("IndexedDB", "IndexedDBTransaction::CloseOpenCursors", "txn.id",
               id());

  // IndexedDBCursor::Close() indirectly mutates |open_cursors_|, when it calls
  // IndexedDBTransaction::UnregisterOpenCursor().
  std::set<IndexedDBCursor*> open_cursors = std::move(open_cursors_);
  open_cursors_.clear();
  for (auto* cursor : open_cursors)
    cursor->Close();
}

}  // namespace content
