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
#include "third_party/blink/public/platform/web_blob_info.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/modules/indexed_db_names.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_metadata.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_request.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_request_queue_item.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_cursor.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_database.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

WebIDBCallbacksImpl::WebIDBCallbacksImpl(IDBRequest* request)
    : request_(request) {
  task_runner_ =
      request_->GetExecutionContext()->GetTaskRunner(TaskType::kDatabaseAccess);
  async_task_context_.Schedule(request_->GetExecutionContext(),
                               indexed_db_names::kIndexedDB);
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
    async_task_context_.Cancel();
#if DCHECK_IS_ON()
    DCHECK_EQ(static_cast<WebIDBCallbacks*>(this), request_->WebCallbacks());
#endif  // DCHECK_IS_ON()
    request_->WebCallbacksDestroyed();
  }
}

void WebIDBCallbacksImpl::DetachRequestFromCallback() {
  request_.Clear();
}

void WebIDBCallbacksImpl::SetState(int64_t transaction_id) {
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

  probe::AsyncTask async_task(request_->GetExecutionContext(),
                              &async_task_context_, "error");
  IDBRequest* request = request_.Get();
  Detach();
  request->HandleResponse(MakeGarbageCollected<DOMException>(
      static_cast<DOMExceptionCode>(code), message));
}

void WebIDBCallbacksImpl::SuccessDatabase(
    mojo::PendingAssociatedRemote<mojom::blink::IDBDatabase> pending_database,
    const IDBDatabaseMetadata& metadata) {
  std::unique_ptr<WebIDBDatabase> db;
  if (pending_database.is_valid()) {
    db = std::make_unique<WebIDBDatabase>(std::move(pending_database),
                                          task_runner_);
  }
  if (request_) {
    probe::AsyncTask async_task(request_->GetExecutionContext(),
                                &async_task_context_, "success");
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

  probe::AsyncTask async_task(request_->GetExecutionContext(),
                              &async_task_context_, "success");
  IDBRequest* request = request_.Get();
  Detach();
  request->HandleResponse(std::move(key));
}

void WebIDBCallbacksImpl::SuccessValue(
    mojom::blink::IDBReturnValuePtr return_value) {
  if (!request_)
    return;

  std::unique_ptr<IDBValue> value = IDBValue::ConvertReturnValue(return_value);
  probe::AsyncTask async_task(request_->GetExecutionContext(),
                              &async_task_context_, "success");
  value->SetIsolate(request_->GetIsolate());
  IDBRequest* request = request_.Get();
  Detach();
  request->HandleResponse(std::move(value));
}

void WebIDBCallbacksImpl::SuccessArray(
    Vector<mojom::blink::IDBReturnValuePtr> values) {
  if (!request_)
    return;

  probe::AsyncTask async_task(request_->GetExecutionContext(),
                              &async_task_context_, "success");
  Vector<std::unique_ptr<IDBValue>> idb_values;
  idb_values.ReserveInitialCapacity(values.size());
  for (const mojom::blink::IDBReturnValuePtr& value : values) {
    std::unique_ptr<IDBValue> idb_value = IDBValue::ConvertReturnValue(value);
    idb_value->SetIsolate(request_->GetIsolate());
    idb_values.emplace_back(std::move(idb_value));
  }
  IDBRequest* request = request_.Get();
  Detach();
  request->HandleResponse(std::move(idb_values));
}

void WebIDBCallbacksImpl::SuccessArrayArray(
    Vector<Vector<mojom::blink::IDBReturnValuePtr>> all_values) {
  if (!request_)
    return;

  probe::AsyncTask async_task(request_->GetExecutionContext(),
                              &async_task_context_, "success");
  Vector<Vector<std::unique_ptr<IDBValue>>> all_idb_values;
  for (const auto& values : all_values) {
    Vector<std::unique_ptr<IDBValue>> idb_values;
    idb_values.ReserveInitialCapacity(values.size());
    for (const mojom::blink::IDBReturnValuePtr& value : values) {
      std::unique_ptr<IDBValue> idb_value = IDBValue::ConvertReturnValue(value);
      idb_value->SetIsolate(request_->GetIsolate());
      idb_values.push_back(std::move(idb_value));
    }
    all_idb_values.push_back(std::move(idb_values));
  }
  IDBRequest* request = request_.Get();
  Detach();
  request->HandleResponse(std::move(all_idb_values));
}

void WebIDBCallbacksImpl::SuccessInteger(int64_t value) {
  if (!request_)
    return;

  probe::AsyncTask async_task(request_->GetExecutionContext(),
                              &async_task_context_, "success");
  IDBRequest* request = request_.Get();
  Detach();
  request->HandleResponse(value);
}

void WebIDBCallbacksImpl::ReceiveGetAllResults(
    bool key_only,
    mojo::PendingReceiver<mojom::blink::IDBDatabaseGetAllResultSink> receiver) {
  if (!request_)
    return;

  // This may turn into an error, but treat this like a success for now.
  probe::AsyncTask async_task(request_->GetExecutionContext(),
                              &async_task_context_, "success");
  IDBRequest* request = request_.Get();
  Detach();
  request->HandleResponse(key_only, std::move(receiver));
}

void WebIDBCallbacksImpl::Blocked(int64_t old_version) {
  if (!request_)
    return;

  probe::AsyncTask async_task(request_->GetExecutionContext(),
                              &async_task_context_, "blocked");
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
    db = std::make_unique<WebIDBDatabase>(std::move(pending_database),
                                          task_runner_);
  }
  if (request_) {
    probe::AsyncTask async_task(request_->GetExecutionContext(),
                                &async_task_context_, "upgradeNeeded");
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
