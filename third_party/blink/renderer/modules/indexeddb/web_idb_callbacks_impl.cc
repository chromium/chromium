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

#include "base/memory/ptr_util.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_database_error.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_key.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_name_and_version.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_value.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/modules/indexed_db_names.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_metadata.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_request.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_cursor.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_database.h"
#include "third_party/blink/renderer/platform/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

using blink::WebIDBCursor;
using blink::WebIDBDatabase;
using blink::WebIDBDatabaseError;
using blink::WebIDBKey;
using blink::WebIDBKeyPath;
using blink::WebIDBMetadata;
using blink::WebIDBNameAndVersion;
using blink::WebIDBValue;
using blink::WebVector;

namespace blink {

// static
std::unique_ptr<WebIDBCallbacksImpl> WebIDBCallbacksImpl::Create(
    IDBRequest* request) {
  return base::WrapUnique(new WebIDBCallbacksImpl(request));
}

WebIDBCallbacksImpl::WebIDBCallbacksImpl(IDBRequest* request)
    : request_(request) {
  probe::AsyncTaskScheduled(request_->GetExecutionContext(),
                            IndexedDBNames::IndexedDB, this);
}

WebIDBCallbacksImpl::~WebIDBCallbacksImpl() {
  if (request_) {
    probe::AsyncTaskCanceled(request_->GetExecutionContext(), this);
#if DCHECK_IS_ON()
    DCHECK_EQ(static_cast<WebIDBCallbacks*>(this), request_->WebCallbacks());
#endif  // DCHECK_IS_ON()
    request_->WebCallbacksDestroyed();
  }
}

void WebIDBCallbacksImpl::OnError(const WebIDBDatabaseError& error) {
  if (!request_)
    return;

  probe::AsyncTask async_task(request_->GetExecutionContext(), this, "error");
  request_->HandleResponse(DOMException::Create(
      static_cast<DOMExceptionCode>(error.Code()), error.Message()));
}

void WebIDBCallbacksImpl::OnSuccess(
    const WebVector<WebIDBNameAndVersion>& web_name_and_version_list) {
  // Only implemented in idb_factory.cc for the promise-based databases() call.
  NOTREACHED();
}

void WebIDBCallbacksImpl::OnSuccess(
    const WebVector<WebString>& web_string_list) {
  if (!request_)
    return;

  Vector<String> string_list;
  for (size_t i = 0; i < web_string_list.size(); ++i)
    string_list.push_back(web_string_list[i]);
  probe::AsyncTask async_task(request_->GetExecutionContext(), this, "success");
#if DCHECK_IS_ON()
  DCHECK(!request_->TransactionHasQueuedResults());
#endif  // DCHECK_IS_ON()
  request_->EnqueueResponse(string_list);
}

void WebIDBCallbacksImpl::OnSuccess(WebIDBCursor* cursor,
                                    WebIDBKey key,
                                    WebIDBKey primary_key,
                                    WebIDBValue value) {
  if (!request_)
    return;

  probe::AsyncTask async_task(request_->GetExecutionContext(), this, "success");
  std::unique_ptr<IDBValue> idb_value = value.ReleaseIdbValue();
  idb_value->SetIsolate(request_->GetIsolate());
  request_->HandleResponse(base::WrapUnique(cursor), key.ReleaseIdbKey(),
                           primary_key.ReleaseIdbKey(), std::move(idb_value));
}

void WebIDBCallbacksImpl::OnSuccess(WebIDBDatabase* backend,
                                    const WebIDBMetadata& metadata) {
  std::unique_ptr<WebIDBDatabase> db = base::WrapUnique(backend);
  if (request_) {
    probe::AsyncTask async_task(request_->GetExecutionContext(), this,
                                "success");
#if DCHECK_IS_ON()
    DCHECK(!request_->TransactionHasQueuedResults());
#endif  // DCHECK_IS_ON()
    request_->EnqueueResponse(std::move(db), IDBDatabaseMetadata(metadata));
  } else if (db) {
    db->Close();
  }
}

void WebIDBCallbacksImpl::OnSuccess(WebIDBKey key) {
  if (!request_)
    return;

  probe::AsyncTask async_task(request_->GetExecutionContext(), this, "success");
  request_->HandleResponse(key.ReleaseIdbKey());
}

void WebIDBCallbacksImpl::OnSuccess(WebIDBValue value) {
  if (!request_)
    return;

  probe::AsyncTask async_task(request_->GetExecutionContext(), this, "success");
  std::unique_ptr<IDBValue> idb_value = value.ReleaseIdbValue();
  idb_value->SetIsolate(request_->GetIsolate());
  request_->HandleResponse(std::move(idb_value));
}

void WebIDBCallbacksImpl::OnSuccess(WebVector<WebIDBValue> values) {
  if (!request_)
    return;

  probe::AsyncTask async_task(request_->GetExecutionContext(), this, "success");
  Vector<std::unique_ptr<IDBValue>> idb_values;
  idb_values.ReserveInitialCapacity(SafeCast<wtf_size_t>(values.size()));
  for (WebIDBValue& value : values) {
    std::unique_ptr<IDBValue> idb_value = value.ReleaseIdbValue();
    idb_value->SetIsolate(request_->GetIsolate());
    idb_values.emplace_back(std::move(idb_value));
  }
  request_->HandleResponse(std::move(idb_values));
}

void WebIDBCallbacksImpl::OnSuccess(long long value) {
  if (!request_)
    return;

  probe::AsyncTask async_task(request_->GetExecutionContext(), this, "success");
  request_->HandleResponse(value);
}

void WebIDBCallbacksImpl::OnSuccess() {
  if (!request_)
    return;

  probe::AsyncTask async_task(request_->GetExecutionContext(), this, "success");
  request_->HandleResponse();
}

void WebIDBCallbacksImpl::OnSuccess(WebIDBKey key,
                                    WebIDBKey primary_key,
                                    WebIDBValue value) {
  if (!request_)
    return;

  probe::AsyncTask async_task(request_->GetExecutionContext(), this, "success");
  std::unique_ptr<IDBValue> idb_value = value.ReleaseIdbValue();
  idb_value->SetIsolate(request_->GetIsolate());
  request_->HandleResponse(key.ReleaseIdbKey(), primary_key.ReleaseIdbKey(),
                           std::move(idb_value));
}

void WebIDBCallbacksImpl::OnBlocked(long long old_version) {
  if (!request_)
    return;

  probe::AsyncTask async_task(request_->GetExecutionContext(), this, "blocked");
#if DCHECK_IS_ON()
  DCHECK(!request_->TransactionHasQueuedResults());
#endif  // DCHECK_IS_ON()
  request_->EnqueueBlocked(old_version);
}

void WebIDBCallbacksImpl::OnUpgradeNeeded(long long old_version,
                                          WebIDBDatabase* database,
                                          const WebIDBMetadata& metadata,
                                          unsigned short data_loss,
                                          WebString data_loss_message) {
  std::unique_ptr<WebIDBDatabase> db = base::WrapUnique(database);
  if (request_) {
    probe::AsyncTask async_task(request_->GetExecutionContext(), this,
                                "upgradeNeeded");
#if DCHECK_IS_ON()
    DCHECK(!request_->TransactionHasQueuedResults());
#endif  // DCHECK_IS_ON()
    request_->EnqueueUpgradeNeeded(
        old_version, std::move(db), IDBDatabaseMetadata(metadata),
        static_cast<WebIDBDataLoss>(data_loss), data_loss_message);
  } else {
    db->Close();
  }
}

void WebIDBCallbacksImpl::Detach() {
  request_.Clear();
}

}  // namespace blink
