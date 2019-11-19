/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. AND ITS CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE INC.
 * OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/indexeddb/web_idb_callbacks_impl.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/modules/indexed_db_names.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_metadata.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_name_and_version.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_request.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_cursor.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_cursor_impl.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_database.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_database_impl.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

namespace {

std::unique_ptr<IDBValue> ConvertReturnValue(
    const mojom::blink::IDBReturnValuePtr& input) {
  if (!input) {
    return std::make_unique<IDBValue>(scoped_refptr<SharedBuffer>(),
                                      Vector<WebBlobInfo>());
  }

  std::unique_ptr<IDBValue> output = std::move(input->value);
  output->SetInjectedPrimaryKey(std::move(input->primary_key), input->key_path);
  return output;
}

}  // namespace

WebIDBCallbacksImpl::WebIDBCallbacksImpl(IDBRequest* request)
    : request_(request) {
  task_runner_ =
      request_->GetExecutionContext()->GetTaskRunner(TaskType::kDatabaseAccess);
  probe::AsyncTaskScheduled(request_->GetExecutionContext(),
                            indexed_db_names::kIndexedDB, &async_task_id_);
}

WebIDBCallbacksImpl::~WebIDBCallbacksImpl() {
  Detach();
}

void WebIDBCallbacksImpl::Detach() {
  DetachCallbackFromRequest();
  DetachRequestFromCallback();
}

void WebIDBCallbacksImpl::DetachCallbackFromRequest() {
  if (request_) {
    probe::AsyncTaskCanceled(request_->GetExecutionContext(), &async_task_id_);
#if DCHECK_IS_ON()
    DCHECK_EQ(static_cast<WebIDBCallbacks*>(this), request_->WebCallbacks());
#endif  // DCHECK_IS_ON()
    request_->WebCallbacksDestroyed();
  }
}

void WebIDBCallbacksImpl::DetachRequestFromCallback() {
  request_.Clear();
}

void WebIDBCallbacksImpl::SetState(base::WeakPtr<WebIDBCursorImpl> cursor,
                                   int64_t transaction_id) {
  cursor_ = cursor;
  transaction_id_ = transaction_id;
}

void WebIDBCallbacksImpl::Error(mojom::blink::IDBException code,
                                const String& message) {
  if (!request_)
    return;

  // In some cases, the backend clears the pending transaction task queue which
  // destroys all pending tasks.  If our callback was queued with a task that
  // gets cleared, we'll get a signal with an IgnorableAbortError as the task is
  // torn down.  This means the error response can be safely ignored.
  if (code == mojom::blink::IDBException::kIgnorableAbortError) {
    Detach();
    return;
  }

  probe::AsyncTask async_task(request_->GetExecutionContext(), &async_task_id_,
                              "error");
  IDBRequest* request = request_.Get();
  Detach();
  request->HandleResponse(MakeGarbageCollected<DOMException>(
      static_cast<DOMExceptionCode>(code), message));
}

void WebIDBCallbacksImpl::SuccessNamesAndVersionsList(
    Vector<mojom::blink::IDBNameAndVersionPtr> name_and_version_list) {
  // Only implemented in idb_factory.cc for the promise-based databases() call.
  NOTREACHED();
}

void WebIDBCallbacksImpl::SuccessStringList(const Vector<String>& string_list) {
  if (!request_)
    return;

  probe::AsyncTask async_task(request_->GetExecutionContext(), &async_task_id_,
                              "success");
#if DCHECK_IS_ON()
  DCHECK(!request_->TransactionHasQueuedResults());
#endif  // DCHECK_IS_ON()
  IDBRequest* request = request_.Get();
  Detach();
  request->EnqueueResponse(std::move(string_list));
}

void WebIDBCallbacksImpl::SuccessCursor(
    mojo::PendingAssociatedRemote<mojom::blink::IDBCursor> cursor_info,
    std::unique_ptr<IDBKey> key,
    std::unique_ptr<IDBKey> primary_key,
    base::Optional<std::unique_ptr<IDBValue>> optional_value) {
  if (!request_)
    return;

  std::unique_ptr<WebIDBCursorImpl> cursor = std::make_unique<WebIDBCursorImpl>(
      std::move(cursor_info), transaction_id_, task_runner_);
  std::unique_ptr<IDBValue> value;
  if (optional_value.has_value()) {
    value = std::move(optional_value.value());
  } else {
    value = std::make_unique<IDBValue>(scoped_refptr<SharedBuffer>(),
                                       Vector<WebBlobInfo>());
  }
  DCHECK(value);

  probe::AsyncTask async_task(request_->GetExecutionContext(), &async_task_id_,
                              "success");
  value->SetIsolate(request_->GetIsolate());
  IDBRequest* request = request_.Get();
  Detach();
  request->HandleResponse(std::move(cursor), std::move(key),
                          std::move(primary_key), std::move(value));
}

void WebIDBCallbacksImpl::SuccessCursorPrefetch(
    Vector<std::unique_ptr<IDBKey>> keys,
    Vector<std::unique_ptr<IDBKey>> primary_keys,
    Vector<std::unique_ptr<IDBValue>> values) {
  if (cursor_) {
    cursor_->SetPrefetchData(std::move(keys), std::move(primary_keys),
                             std::move(values));
    cursor_->CachedContinue(this);
  }
  Detach();
}

void WebIDBCallbacksImpl::SuccessDatabase(
    mojo::PendingAssociatedRemote<mojom::blink::IDBDatabase> pending_database,
    const IDBDatabaseMetadata& metadata) {
  std::unique_ptr<WebIDBDatabase> db;
  if (pending_database.is_valid()) {
    db = std::make_unique<WebIDBDatabaseImpl>(std::move(pending_database),
                                              task_runner_);
  }
  if (request_) {
    probe::AsyncTask async_task(request_->GetExecutionContext(),
                                &async_task_id_, "success");
#if DCHECK_IS_ON()
    DCHECK(!request_->TransactionHasQueuedResults());
#endif  // DCHECK_IS_ON()
    IDBRequest* request = request_.Get();
    Detach();
    request->EnqueueResponse(std::move(db), IDBDatabaseMetadata(metadata));
  } else if (db) {
    db->Close();
  }
}

void WebIDBCallbacksImpl::SuccessKey(std::unique_ptr<IDBKey> key) {
  if (!request_)
    return;

  probe::AsyncTask async_task(request_->GetExecutionContext(), &async_task_id_,
                              "success");
  IDBRequest* request = request_.Get();
  Detach();
  request->HandleResponse(std::move(key));
}

void WebIDBCallbacksImpl::SuccessValue(
    mojom::blink::IDBReturnValuePtr return_value) {
  if (!request_)
    return;

  std::unique_ptr<IDBValue> value = ConvertReturnValue(return_value);
  probe::AsyncTask async_task(request_->GetExecutionContext(), &async_task_id_,
                              "success");
  value->SetIsolate(request_->GetIsolate());
  IDBRequest* request = request_.Get();
  Detach();
  request->HandleResponse(std::move(value));
}

void WebIDBCallbacksImpl::SuccessArray(
    Vector<mojom::blink::IDBReturnValuePtr> values) {
  if (!request_)
    return;

  probe::AsyncTask async_task(request_->GetExecutionContext(), &async_task_id_,
                              "success");
  Vector<std::unique_ptr<IDBValue>> idb_values;
  idb_values.ReserveInitialCapacity(values.size());
  for (const mojom::blink::IDBReturnValuePtr& value : values) {
    std::unique_ptr<IDBValue> idb_value = ConvertReturnValue(value);
    idb_value->SetIsolate(request_->GetIsolate());
    idb_values.emplace_back(std::move(idb_value));
  }
  IDBRequest* request = request_.Get();
  Detach();
  request->HandleResponse(std::move(idb_values));
}

void WebIDBCallbacksImpl::SuccessInteger(int64_t value) {
  if (!request_)
    return;

  probe::AsyncTask async_task(request_->GetExecutionContext(), &async_task_id_,
                              "success");
  IDBRequest* request = request_.Get();
  Detach();
  request->HandleResponse(value);
}

void WebIDBCallbacksImpl::Success() {
  if (!request_)
    return;

  probe::AsyncTask async_task(request_->GetExecutionContext(), &async_task_id_,
                              "success");
  IDBRequest* request = request_.Get();
  Detach();
  request->HandleResponse();
}

void WebIDBCallbacksImpl::SuccessCursorContinue(
    std::unique_ptr<IDBKey> key,
    std::unique_ptr<IDBKey> primary_key,
    base::Optional<std::unique_ptr<IDBValue>> optional_value) {
  if (!request_)
    return;

  probe::AsyncTask async_task(request_->GetExecutionContext(), &async_task_id_,
                              "success");
  std::unique_ptr<IDBValue> value;
  if (optional_value.has_value()) {
    value = std::move(optional_value.value());
  } else {
    value = std::make_unique<IDBValue>(scoped_refptr<SharedBuffer>(),
                                       Vector<WebBlobInfo>());
  }
  DCHECK(value);
  value->SetIsolate(request_->GetIsolate());
  IDBRequest* request = request_.Get();
  Detach();
  request->HandleResponse(std::move(key), std::move(primary_key),
                          std::move(value));
}

void WebIDBCallbacksImpl::Blocked(int64_t old_version) {
  if (!request_)
    return;

  probe::AsyncTask async_task(request_->GetExecutionContext(), &async_task_id_,
                              "blocked");
#if DCHECK_IS_ON()
  DCHECK(!request_->TransactionHasQueuedResults());
#endif  // DCHECK_IS_ON()
  request_->EnqueueBlocked(old_version);
  // Not resetting |request_|.  In this instance we will have to forward at
  // least one other call in the set UpgradeNeeded() / Success() /
  // Error().
}

void WebIDBCallbacksImpl::UpgradeNeeded(
    mojo::PendingAssociatedRemote<mojom::blink::IDBDatabase> pending_database,
    int64_t old_version,
    mojom::IDBDataLoss data_loss,
    const String& data_loss_message,
    const IDBDatabaseMetadata& metadata) {
  std::unique_ptr<WebIDBDatabase> db;
  if (pending_database.is_valid()) {
    db = std::make_unique<WebIDBDatabaseImpl>(std::move(pending_database),
                                              task_runner_);
  }
  if (request_) {
    probe::AsyncTask async_task(request_->GetExecutionContext(),
                                &async_task_id_, "upgradeNeeded");
#if DCHECK_IS_ON()
    DCHECK(!request_->TransactionHasQueuedResults());
#endif  // DCHECK_IS_ON()
    request_->EnqueueUpgradeNeeded(old_version, std::move(db),
                                   IDBDatabaseMetadata(metadata), data_loss,
                                   data_loss_message);
    // Not resetting |request_|.  In this instance we will have to forward at
    // least one other call in the set UpgradeNeeded() / Success() /
    // Error().
  } else if (db) {
    db->Close();
  }
}

}  // namespace blink
