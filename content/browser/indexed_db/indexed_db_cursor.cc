// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_cursor.h"

#include <stddef.h>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/trace_event/base_tracing.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/browser/indexed_db/indexed_db_callback_helpers.h"
#include "content/browser/indexed_db/indexed_db_callbacks.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

using blink::IndexedDBKey;

namespace content {
namespace {
// This should never be script visible: the cursor should either be closed when
// it hits the end of the range (and script throws an error before the call
// could be made), if the transaction has finished (ditto), or if there's an
// incoming request from the front end but the transaction has aborted on the
// back end; in that case the tx will already have sent an abort to the request
// so this would be ignored.
IndexedDBDatabaseError CreateCursorClosedError() {
  return IndexedDBDatabaseError(blink::mojom::IDBException::kUnknownError,
                                "The cursor has been closed.");
}

IndexedDBDatabaseError CreateError(
    blink::mojom::IDBException code,
    const char* message,
    base::WeakPtr<IndexedDBTransaction> transaction) {
  if (transaction)
    transaction->IncrementNumErrorsSent();
  return IndexedDBDatabaseError(code, message);
}

}  // namespace

IndexedDBCursor::IndexedDBCursor(
    std::unique_ptr<IndexedDBBackingStore::Cursor> cursor,
    indexed_db::CursorType cursor_type,
    blink::mojom::IDBTaskType task_type,
    base::WeakPtr<IndexedDBTransaction> transaction)
    : bucket_locator_(transaction->BackingStoreTransaction()
                          ->backing_store()
                          ->bucket_locator()),
      task_type_(task_type),
      cursor_type_(cursor_type),
      transaction_(std::move(transaction)),
      cursor_(std::move(cursor)),
      closed_(false) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("IndexedDB", "IndexedDBCursor::open", this);
}

IndexedDBCursor::~IndexedDBCursor() {
  // Call to make sure we complete our lifetime trace.
  Close();
}

void IndexedDBCursor::Advance(
    uint32_t count,
    base::WeakPtr<content::IndexedDBDispatcherHost> dispatcher_host,
    blink::mojom::IDBCursor::AdvanceCallback callback) {
  TRACE_EVENT0("IndexedDB", "IndexedDBCursor::Advance");

  if (!transaction_)
    Close();
  if (closed_) {
    const IndexedDBDatabaseError error(CreateCursorClosedError());
    std::move(callback).Run(blink::mojom::IDBCursorResult::NewErrorResult(
        blink::mojom::IDBError::New(error.code(), error.message())));
    return;
  }

  blink::mojom::IDBCursor::AdvanceCallback aborting_callback =
      CreateCallbackAbortOnDestruct<blink::mojom::IDBCursor::AdvanceCallback,
                                    blink::mojom::IDBCursorResultPtr>(
          std::move(callback), transaction_);

  transaction_->ScheduleTask(
      task_type_,
      BindWeakOperation<IndexedDBCursor>(
          &IndexedDBCursor::CursorAdvanceOperation, ptr_factory_.GetWeakPtr(),
          count, std::move(dispatcher_host), std::move(aborting_callback)));
}

leveldb::Status IndexedDBCursor::CursorAdvanceOperation(
    uint32_t count,
    base::WeakPtr<IndexedDBDispatcherHost> dispatcher_host,
    blink::mojom::IDBCursor::AdvanceCallback callback,
    IndexedDBTransaction* /*transaction*/) {
  TRACE_EVENT0("IndexedDB", "IndexedDBCursor::CursorAdvanceOperation");
  leveldb::Status s = leveldb::Status::OK();
  if (!dispatcher_host)
    return s;

  if (!cursor_ || !cursor_->Advance(count, &s)) {
    cursor_.reset();

    if (s.ok()) {
      std::move(callback).Run(blink::mojom::IDBCursorResult::NewEmpty(true));
      return s;
    }

    // CreateError() needs to be called before calling Close() so
    // |transaction_| is alive.
    auto error = CreateError(blink::mojom::IDBException::kUnknownError,
                             "Error advancing cursor", transaction_);
    Close();
    std::move(callback).Run(blink::mojom::IDBCursorResult::NewErrorResult(
        blink::mojom::IDBError::New(error.code(), error.message())));
    return s;
  }

  blink::mojom::IDBValuePtr mojo_value;
  std::vector<IndexedDBExternalObject> external_objects;
  IndexedDBValue* value = Value();
  if (value) {
    mojo_value = IndexedDBValue::ConvertAndEraseValue(value);
    external_objects.swap(value->external_objects);
    dispatcher_host->CreateAllExternalObjects(bucket_locator_, external_objects,
                                              &mojo_value->external_objects);
  } else {
    mojo_value = blink::mojom::IDBValue::New();
  }

  std::vector<IndexedDBKey> keys = {key()};
  std::vector<IndexedDBKey> primary_keys = {primary_key()};
  std::vector<blink::mojom::IDBValuePtr> values;
  values.push_back(std::move(mojo_value));
  std::move(callback).Run(blink::mojom::IDBCursorResult::NewValues(
      blink::mojom::IDBCursorValue::New(
          std::move(keys), std::move(primary_keys), std::move(values))));
  return s;
}

void IndexedDBCursor::Continue(
    base::WeakPtr<content::IndexedDBDispatcherHost> dispatcher_host,
    std::unique_ptr<IndexedDBKey> key,
    std::unique_ptr<IndexedDBKey> primary_key,
    blink::mojom::IDBCursor::CursorContinueCallback callback) {
  TRACE_EVENT0("IndexedDB", "IndexedDBCursor::Continue");
  if (!transaction_)
    Close();
  if (closed_) {
    const IndexedDBDatabaseError error(CreateCursorClosedError());
    std::move(callback).Run(blink::mojom::IDBCursorResult::NewErrorResult(
        blink::mojom::IDBError::New(error.code(), error.message())));
    return;
  }

  blink::mojom::IDBCursor::CursorContinueCallback aborting_callback =
      CreateCallbackAbortOnDestruct<
          blink::mojom::IDBCursor::CursorContinueCallback,
          blink::mojom::IDBCursorResultPtr>(std::move(callback), transaction_);

  transaction_->ScheduleTask(
      task_type_,
      BindWeakOperation<IndexedDBCursor>(
          &IndexedDBCursor::CursorContinueOperation, ptr_factory_.GetWeakPtr(),
          std::move(dispatcher_host), std::move(key), std::move(primary_key),
          std::move(aborting_callback)));
}

leveldb::Status IndexedDBCursor::CursorContinueOperation(
    base::WeakPtr<IndexedDBDispatcherHost> dispatcher_host,
    std::unique_ptr<IndexedDBKey> key,
    std::unique_ptr<IndexedDBKey> primary_key,
    blink::mojom::IDBCursor::CursorContinueCallback callback,
    IndexedDBTransaction* /*transaction*/) {
  TRACE_EVENT0("IndexedDB", "IndexedDBCursor::CursorContinueOperation");
  leveldb::Status s = leveldb::Status::OK();
  if (!dispatcher_host)
    return s;

  if (!cursor_ || !cursor_->Continue(key.get(), primary_key.get(),
                                     IndexedDBBackingStore::Cursor::SEEK, &s)) {
    cursor_.reset();
    if (s.ok()) {
      // This happens if we reach the end of the iterator and can't continue.
      std::move(callback).Run(blink::mojom::IDBCursorResult::NewEmpty(true));
      return s;
    }

    // |transaction_| must be valid for CreateError(), so we can't call
    // Close() until after calling CreateError().
    IndexedDBDatabaseError error =
        CreateError(blink::mojom::IDBException::kUnknownError,
                    "Error continuing cursor.", transaction_);
    Close();
    std::move(callback).Run(blink::mojom::IDBCursorResult::NewErrorResult(
        blink::mojom::IDBError::New(error.code(), error.message())));
    return s;
  }

  blink::mojom::IDBValuePtr mojo_value;
  std::vector<IndexedDBExternalObject> external_objects;
  IndexedDBValue* value = Value();
  if (value) {
    mojo_value = IndexedDBValue::ConvertAndEraseValue(value);
    external_objects.swap(value->external_objects);
    dispatcher_host->CreateAllExternalObjects(bucket_locator_, external_objects,
                                              &mojo_value->external_objects);
  } else {
    mojo_value = blink::mojom::IDBValue::New();
  }

  std::vector<IndexedDBKey> keys = {this->key()};
  std::vector<IndexedDBKey> primary_keys = {this->primary_key()};
  std::vector<blink::mojom::IDBValuePtr> values;
  values.push_back(std::move(mojo_value));
  std::move(callback).Run(blink::mojom::IDBCursorResult::NewValues(
      blink::mojom::IDBCursorValue::New(
          std::move(keys), std::move(primary_keys), std::move(values))));
  return s;
}

void IndexedDBCursor::PrefetchContinue(
    base::WeakPtr<IndexedDBDispatcherHost> dispatcher_host,
    int number_to_fetch,
    blink::mojom::IDBCursor::PrefetchCallback callback) {
  TRACE_EVENT0("IndexedDB", "IndexedDBCursor::PrefetchContinue");

  if (!transaction_)
    Close();
  if (closed_) {
    const IndexedDBDatabaseError error(CreateCursorClosedError());
    std::move(callback).Run(blink::mojom::IDBCursorResult::NewErrorResult(
        blink::mojom::IDBError::New(error.code(), error.message())));
    return;
  }

  blink::mojom::IDBCursor::PrefetchCallback aborting_callback =
      CreateCallbackAbortOnDestruct<blink::mojom::IDBCursor::PrefetchCallback,
                                    blink::mojom::IDBCursorResultPtr>(
          std::move(callback), transaction_);

  transaction_->ScheduleTask(
      task_type_, BindWeakOperation<IndexedDBCursor>(
                      &IndexedDBCursor::CursorPrefetchIterationOperation,
                      ptr_factory_.GetWeakPtr(), std::move(dispatcher_host),
                      number_to_fetch, std::move(aborting_callback)));
}

leveldb::Status IndexedDBCursor::CursorPrefetchIterationOperation(
    base::WeakPtr<IndexedDBDispatcherHost> dispatcher_host,
    int number_to_fetch,
    blink::mojom::IDBCursor::PrefetchCallback callback,
    IndexedDBTransaction* /*transaction*/) {
  TRACE_EVENT0("IndexedDB",
               "IndexedDBCursor::CursorPrefetchIterationOperation");
  leveldb::Status s = leveldb::Status::OK();
  if (!dispatcher_host)
    return s;

  std::vector<IndexedDBKey> found_keys;
  std::vector<IndexedDBKey> found_primary_keys;
  std::vector<IndexedDBValue> found_values;

  saved_cursor_.reset();
  // TODO(cmumford): Use IPC::Channel::kMaximumMessageSize
  const size_t max_size_estimate = 10 * 1024 * 1024;
  size_t size_estimate = 0;

  // TODO(cmumford): Handle this error (crbug.com/363397). Although this will
  //                 properly fail, caller will not know why, and any corruption
  //                 will be ignored.
  for (int i = 0; i < number_to_fetch; ++i) {
    if (!cursor_ || !cursor_->Continue(&s)) {
      cursor_.reset();
      if (s.ok()) {
        // We've reached the end, so just return what we have.
        break;
      }
      // |transaction_| must be valid for CreateError(), so we can't call
      // Close() until after calling CreateError().
      IndexedDBDatabaseError error =
          CreateError(blink::mojom::IDBException::kUnknownError,
                      "Error continuing cursor.", transaction_);
      Close();
      std::move(callback).Run(blink::mojom::IDBCursorResult::NewErrorResult(
          blink::mojom::IDBError::New(error.code(), error.message())));
      return s;
    }

    if (i == 0) {
      // First prefetched result is always used, so that's the position
      // a cursor should be reset to if the prefetch is invalidated.
      saved_cursor_ = cursor_->Clone();
    }

    found_keys.push_back(cursor_->key());
    found_primary_keys.push_back(cursor_->primary_key());

    switch (cursor_type_) {
      case indexed_db::CURSOR_KEY_ONLY:
        found_values.push_back(IndexedDBValue());
        break;
      case indexed_db::CURSOR_KEY_AND_VALUE: {
        IndexedDBValue value;
        value.swap(*cursor_->value());
        size_estimate += value.SizeEstimate();
        found_values.push_back(value);
        break;
      }
      default:
        NOTREACHED();
    }
    size_estimate += cursor_->key().size_estimate();
    size_estimate += cursor_->primary_key().size_estimate();

    if (size_estimate > max_size_estimate)
      break;
  }

  if (found_keys.empty()) {
    std::move(callback).Run(blink::mojom::IDBCursorResult::NewEmpty(true));
    return s;
  }

  DCHECK_EQ(found_keys.size(), found_primary_keys.size());
  DCHECK_EQ(found_keys.size(), found_values.size());

  std::vector<blink::mojom::IDBValuePtr> mojo_values;
  mojo_values.reserve(found_values.size());
  for (size_t i = 0; i < found_values.size(); ++i) {
    mojo_values.push_back(
        IndexedDBValue::ConvertAndEraseValue(&found_values[i]));
    dispatcher_host->CreateAllExternalObjects(
        bucket_locator_, found_values[i].external_objects,
        &mojo_values[i]->external_objects);
  }

  std::move(callback).Run(blink::mojom::IDBCursorResult::NewValues(
      blink::mojom::IDBCursorValue::New(std::move(found_keys),
                                        std::move(found_primary_keys),
                                        std::move(mojo_values))));
  return s;
}

leveldb::Status IndexedDBCursor::PrefetchReset(int used_prefetches,
                                               int /* unused_prefetches */) {
  TRACE_EVENT0("IndexedDB", "IndexedDBCursor::PrefetchReset");
  cursor_.swap(saved_cursor_);
  saved_cursor_.reset();
  leveldb::Status s;

  if (closed_)
    return s;
  // First prefetched result is always used.
  if (cursor_) {
    DCHECK_GT(used_prefetches, 0);
    for (int i = 0; i < used_prefetches - 1; ++i) {
      bool ok = cursor_->Continue(&s);
      DCHECK(ok);
    }
  }

  return s;
}

void IndexedDBCursor::OnRemoveBinding(base::OnceClosure remove_binding_cb) {
  remove_binding_cb_ = std::move(remove_binding_cb);
}

void IndexedDBCursor::RemoveBinding() {
  std::move(remove_binding_cb_).Run();
}

void IndexedDBCursor::Close() {
  if (closed_)
    return;
  TRACE_EVENT_NESTABLE_ASYNC_END0("IndexedDB", "IndexedDBCursor::open", this);
  TRACE_EVENT0("IndexedDB", "IndexedDBCursor::Close");
  closed_ = true;
  cursor_.reset();
  saved_cursor_.reset();
  if (transaction_)
    transaction_->UnregisterOpenCursor(this);
  transaction_.reset();
}

}  // namespace content
