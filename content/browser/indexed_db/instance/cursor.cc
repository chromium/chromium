// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/cursor.h"

#include <stddef.h>

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_external_object.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "content/browser/indexed_db/instance/callback_helpers.h"
#include "content/browser/indexed_db/instance/transaction.h"
#include "content/browser/indexed_db/status.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

using blink::IndexedDBKey;

namespace content::indexed_db {
namespace {
// This should never be script visible: the cursor should either be closed when
// it hits the end of the range (and script throws an error before the call
// could be made), if the transaction has finished (ditto), or if there's an
// incoming request from the front end but the transaction has aborted on the
// back end; in that case the tx will already have sent an abort to the request
// so this would be ignored.
DatabaseError CreateCursorClosedError() {
  return DatabaseError(blink::mojom::IDBException::kUnknownError,
                       "The cursor has been closed.");
}

DatabaseError CreateError(blink::mojom::IDBException code,
                          const char* message,
                          base::WeakPtr<Transaction> transaction) {
  if (transaction) {
    transaction->IncrementNumErrorsSent();
  }
  return DatabaseError(code, message);
}

}  // namespace

// static
Cursor* Cursor::CreateAndBind(
    std::unique_ptr<BackingStore::Cursor> cursor,
    indexed_db::CursorType cursor_type,
    blink::mojom::IDBTaskType task_type,
    base::WeakPtr<Transaction> transaction,
    mojo::PendingAssociatedRemote<blink::mojom::IDBCursor>& pending_remote) {
  auto instance = base::WrapUnique(
      new Cursor(std::move(cursor), cursor_type, task_type, transaction));
  Cursor* instance_ptr = instance.get();
  mojo::MakeSelfOwnedAssociatedReceiver(
      std::move(instance), pending_remote.InitWithNewEndpointAndPassReceiver());
  return instance_ptr;
}

Cursor::Cursor(std::unique_ptr<BackingStore::Cursor> cursor,
               indexed_db::CursorType cursor_type,
               blink::mojom::IDBTaskType task_type,
               base::WeakPtr<Transaction> transaction)
    : bucket_locator_(transaction->bucket_context()->bucket_locator()),
      task_type_(task_type),
      cursor_type_(cursor_type),
      transaction_(std::move(transaction)),
      cursor_(std::move(cursor)) {
  TRACE_EVENT_BEGIN("IndexedDB", "Cursor::open",
                    perfetto::Track::FromPointer(this));
}

Cursor::~Cursor() {
  // Call to make sure we complete our lifetime trace.
  Close();
}

void Cursor::Advance(uint32_t count,
                     blink::mojom::IDBCursor::AdvanceCallback callback) {
  TRACE_EVENT0("IndexedDB", "Cursor::Advance");

  if (!transaction_) {
    Close();
  }
  if (closed_) {
    const DatabaseError error(CreateCursorClosedError());
    std::move(callback).Run(blink::mojom::IDBCursorResult::NewErrorResult(
        blink::mojom::IDBError::New(error.code(), error.message())));
    return;
  }

  blink::mojom::IDBCursor::AdvanceCallback aborting_callback =
      CreateCallbackAbortOnDestruct<blink::mojom::IDBCursor::AdvanceCallback,
                                    blink::mojom::IDBCursorResultPtr>(
          std::move(callback), transaction_);

  transaction_->ScheduleTask(
      task_type_, "AdvanceCursor",
      BindWeakOperation<Cursor>(&Cursor::AdvanceOperation,
                                ptr_factory_.GetWeakPtr(), count,
                                std::move(aborting_callback)));
}

Status Cursor::AdvanceOperation(
    uint32_t count,
    blink::mojom::IDBCursor::AdvanceCallback callback,
    Transaction* /*transaction*/) {
  TRACE_EVENT0("IndexedDB", "Cursor::AdvanceOperation");

  if (!cursor_) {
    std::move(callback).Run(blink::mojom::IDBCursorResult::NewEmpty(true));
    return Status::OK();
  }

  if (StatusOr<bool> result = cursor_->Advance(count);
      !result.has_value() || !*result) {
    cursor_.reset();

    if (result.has_value()) {
      std::move(callback).Run(blink::mojom::IDBCursorResult::NewEmpty(true));
      return Status::OK();
    }

    // CreateError() needs to be called before calling Close() so
    // |transaction_| is alive.
    auto error = CreateError(blink::mojom::IDBException::kUnknownError,
                             "Error advancing cursor", transaction_);
    Close();
    std::move(callback).Run(blink::mojom::IDBCursorResult::NewErrorResult(
        blink::mojom::IDBError::New(error.code(), error.message())));
    return result.error();
  }

  blink::mojom::IDBValuePtr mojo_value;
  IndexedDBValue* value = Value();
  if (value) {
    mojo_value = transaction_->BuildMojoValue(std::move(*value));
  } else {
    mojo_value = blink::mojom::IDBValue::New();
  }

  std::vector<IndexedDBKey> keys;
  keys.emplace_back(key().Clone());
  std::vector<IndexedDBKey> primary_keys;
  primary_keys.emplace_back(primary_key().Clone());
  std::vector<blink::mojom::IDBValuePtr> values;
  values.push_back(std::move(mojo_value));
  std::move(callback).Run(blink::mojom::IDBCursorResult::NewValues(
      blink::mojom::IDBCursorValue::New(
          std::move(keys), std::move(primary_keys), std::move(values))));
  return Status::OK();
}

void Cursor::Continue(IndexedDBKey key,
                      IndexedDBKey primary_key,
                      blink::mojom::IDBCursor::ContinueCallback callback) {
  TRACE_EVENT0("IndexedDB", "Cursor::Continue");
  if (!transaction_) {
    Close();
  }
  if (closed_) {
    const DatabaseError error(CreateCursorClosedError());
    std::move(callback).Run(blink::mojom::IDBCursorResult::NewErrorResult(
        blink::mojom::IDBError::New(error.code(), error.message())));
    return;
  }

  blink::mojom::IDBCursor::ContinueCallback aborting_callback =
      CreateCallbackAbortOnDestruct<blink::mojom::IDBCursor::ContinueCallback,
                                    blink::mojom::IDBCursorResultPtr>(
          std::move(callback), transaction_);

  transaction_->ScheduleTask(
      task_type_, "ContinueCursor",
      BindWeakOperation<Cursor>(
          &Cursor::ContinueOperation, ptr_factory_.GetWeakPtr(), std::move(key),
          std::move(primary_key), std::move(aborting_callback)));
}

Status Cursor::ContinueOperation(
    IndexedDBKey key,
    IndexedDBKey primary_key,
    blink::mojom::IDBCursor::ContinueCallback callback,
    Transaction* /*transaction*/) {
  TRACE_EVENT0("IndexedDB", "Cursor::ContinueOperation");

  if (!cursor_) {
    std::move(callback).Run(blink::mojom::IDBCursorResult::NewEmpty(true));
    return Status::OK();
  }

  if (StatusOr<bool> result = cursor_->Continue(key, primary_key);
      !result.has_value() || !*result) {
    cursor_.reset();
    if (result.has_value()) {
      // This happens if we reach the end of the iterator and can't continue.
      std::move(callback).Run(blink::mojom::IDBCursorResult::NewEmpty(true));
      return Status::OK();
    }

    // |transaction_| must be valid for CreateError(), so we can't call
    // Close() until after calling CreateError().
    DatabaseError error = CreateError(blink::mojom::IDBException::kUnknownError,
                                      "Error continuing cursor.", transaction_);
    Close();
    std::move(callback).Run(blink::mojom::IDBCursorResult::NewErrorResult(
        blink::mojom::IDBError::New(error.code(), error.message())));
    return result.error();
  }

  blink::mojom::IDBValuePtr mojo_value;
  IndexedDBValue* value = Value();
  if (value) {
    mojo_value = transaction_->BuildMojoValue(std::move(*value));
  } else {
    mojo_value = blink::mojom::IDBValue::New();
  }

  std::vector<IndexedDBKey> keys;
  keys.emplace_back(this->key().Clone());
  std::vector<IndexedDBKey> primary_keys;
  primary_keys.emplace_back(this->primary_key().Clone());
  std::vector<blink::mojom::IDBValuePtr> values;
  values.push_back(std::move(mojo_value));
  std::move(callback).Run(blink::mojom::IDBCursorResult::NewValues(
      blink::mojom::IDBCursorValue::New(
          std::move(keys), std::move(primary_keys), std::move(values))));
  return Status::OK();
}

void Cursor::Prefetch(int number_to_fetch,
                      blink::mojom::IDBCursor::PrefetchCallback callback) {
  TRACE_EVENT0("IndexedDB", "Cursor::Prefetch");

  if (!transaction_) {
    Close();
  }
  if (closed_) {
    const DatabaseError error(CreateCursorClosedError());
    std::move(callback).Run(blink::mojom::IDBCursorResult::NewErrorResult(
        blink::mojom::IDBError::New(error.code(), error.message())));
    return;
  }

  blink::mojom::IDBCursor::PrefetchCallback aborting_callback =
      CreateCallbackAbortOnDestruct<blink::mojom::IDBCursor::PrefetchCallback,
                                    blink::mojom::IDBCursorResultPtr>(
          std::move(callback), transaction_);

  transaction_->ScheduleTask(
      task_type_, "PrefetchCursor",
      BindWeakOperation<Cursor>(&Cursor::PrefetchIterationOperation,
                                ptr_factory_.GetWeakPtr(), number_to_fetch,
                                std::move(aborting_callback)));
}

Status Cursor::PrefetchIterationOperation(
    int number_to_fetch,
    blink::mojom::IDBCursor::PrefetchCallback callback,
    Transaction* /*transaction*/) {
  TRACE_EVENT0("IndexedDB", "Cursor::PrefetchIterationOperation");

  Status s = Status::OK();
  std::vector<IndexedDBKey> found_keys;
  std::vector<IndexedDBKey> found_primary_keys;
  std::vector<IndexedDBValue> found_values;

  // TODO(cmumford): Use IPC::mojom::kChannelMaximumMessageSize
  const size_t max_size_estimate = 10 * 1024 * 1024;
  size_t size_estimate = 0;

  // TODO(cmumford): Handle this error (crbug.com/363397). Although this will
  //                 properly fail, caller will not know why, and any corruption
  //                 will be ignored.
  for (int i = 0; i < number_to_fetch; ++i) {
    if (!cursor_ || reached_end_during_prefetch_) {
      break;
    }

    StatusOr<bool> result = cursor_->Continue();
    if (!result.has_value()) {
      cursor_.reset();
      // |transaction_| must be valid for CreateError(), so we can't call
      // Close() until after calling CreateError().
      DatabaseError error =
          CreateError(blink::mojom::IDBException::kUnknownError,
                      "Error continuing cursor.", transaction_);
      Close();
      std::move(callback).Run(blink::mojom::IDBCursorResult::NewErrorResult(
          blink::mojom::IDBError::New(error.code(), error.message())));
      return result.error();
    }

    if (!*result) {
      // We've reached the end, so just return what we have.
      reached_end_during_prefetch_ = true;
      break;
    }

    if (i == 0) {
      // First prefetched result is always used, so that's the position
      // a cursor should be reset to if the prefetch is invalidated.
      cursor_->SavePosition();
    }

    found_keys.emplace_back(cursor_->GetKey().Clone());
    found_primary_keys.emplace_back(cursor_->GetPrimaryKey().Clone());

    switch (cursor_type_) {
      case indexed_db::CursorType::kKeyOnly:
        found_values.push_back(IndexedDBValue());
        break;
      case indexed_db::CursorType::kKeyAndValue: {
        found_values.push_back(std::move(cursor_->GetValue()));
        size_estimate += found_values.back().SizeEstimate();
        break;
      }
      default:
        NOTREACHED();
    }
    size_estimate += cursor_->GetKey().size_estimate();
    size_estimate += cursor_->GetPrimaryKey().size_estimate();

    if (size_estimate > max_size_estimate) {
      break;
    }
  }

  if (found_keys.empty()) {
    std::move(callback).Run(blink::mojom::IDBCursorResult::NewEmpty(true));
    return Status::OK();
  }

  DCHECK_EQ(found_keys.size(), found_primary_keys.size());
  DCHECK_EQ(found_keys.size(), found_values.size());

  std::vector<blink::mojom::IDBValuePtr> mojo_values;
  mojo_values.reserve(found_values.size());
  for (IndexedDBValue& value : found_values) {
    mojo_values.emplace_back(transaction_->BuildMojoValue(std::move(value)));
  }

  std::move(callback).Run(blink::mojom::IDBCursorResult::NewValues(
      blink::mojom::IDBCursorValue::New(std::move(found_keys),
                                        std::move(found_primary_keys),
                                        std::move(mojo_values))));
  return Status::OK();
}

void Cursor::PrefetchReset(int used_prefetches) {
  TRACE_EVENT0("IndexedDB", "Cursor::PrefetchReset");
  if (closed_) {
    return;
  }

  reached_end_during_prefetch_ = false;
  Status s = cursor_->TryResetToLastSavedPosition();
  if (!s.ok()) {
    if (s.IsInvalidArgument()) {
      mojo::ReportBadMessage(s.ToString());
    }
    cursor_.reset();
  }

  // First prefetched result is always used.
  if (cursor_) {
    DCHECK_GT(used_prefetches, 0);
    if (used_prefetches > 1) {
      auto result = cursor_->Advance(used_prefetches - 1);
      DCHECK(!result.has_value() || result.value());
    }
  }
}

void Cursor::Close() {
  if (closed_) {
    return;
  }
  // Corresponds to the TRACE_EVENT_BEGIN in the constructor.
  TRACE_EVENT_END("IndexedDB", perfetto::Track::FromPointer(this));
  TRACE_EVENT0("IndexedDB", "Cursor::Close");
  closed_ = true;
  cursor_.reset();
  if (transaction_) {
    transaction_->UnregisterOpenCursor(this);
  }
  transaction_.reset();
}

}  // namespace content::indexed_db
