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

#include "third_party/blink/renderer/modules/indexeddb/idb_factory_client.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-blink.h"
#include "third_party/blink/public/platform/web_blob_info.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/indexed_db_names.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_metadata.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_open_db_request.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

IDBFactoryClient::IDBFactoryClient(IDBOpenDBRequest* request)
    : request_(request) {
  task_runner_ =
      request_->GetExecutionContext()->GetTaskRunner(TaskType::kDatabaseAccess);
}

IDBFactoryClient::~IDBFactoryClient() {
  Detach();
}

void IDBFactoryClient::Detach() {
  DetachFromRequest();
  DetachRequest();
}

void IDBFactoryClient::DetachFromRequest() {
  if (request_) {
    request_->FactoryClientDestroyed(this);
  }
}

void IDBFactoryClient::DetachRequest() {
  request_.Clear();
}

void IDBFactoryClient::Error(mojom::blink::IDBException code,
                             const String& message) {
  if (!request_) {
    return;
  }

  // In some cases, the backend clears the pending transaction task queue which
  // destroys all pending tasks.  If our callback was queued with a task that
  // gets cleared, we'll get a signal with an IgnorableAbortError as the task is
  // torn down.  This means the error response can be safely ignored.
  if (code == mojom::blink::IDBException::kIgnorableAbortError) {
    Detach();
    return;
  }

  IDBOpenDBRequest* request = request_.Get();
  Detach();
  request->OnDBFactoryError(MakeGarbageCollected<DOMException>(
      static_cast<DOMExceptionCode>(code), message));
}

void IDBFactoryClient::OpenSuccess(
    mojo::PendingAssociatedRemote<mojom::blink::IDBDatabase> pending_database,
    const IDBDatabaseMetadata& metadata) {
  if (!request_) {
    return;
  }

#if DCHECK_IS_ON()
    DCHECK(!request_->TransactionHasQueuedResults());
#endif  // DCHECK_IS_ON()
    IDBOpenDBRequest* request = request_.Get();
    Detach();
    request->OnOpenDBSuccess(std::move(pending_database), task_runner_,
                             IDBDatabaseMetadata(metadata));
    // `this` may be deleted because event dispatch can run a nested loop.
}

void IDBFactoryClient::DeleteSuccess(int64_t old_version) {
  if (!request_) {
    return;
  }

  IDBOpenDBRequest* request = request_.Get();
  Detach();
  request->OnDeleteDBSuccess(old_version);
  // `this` may be deleted because event dispatch can run a nested loop.
}

void IDBFactoryClient::Blocked(int64_t old_version) {
  if (!request_) {
    return;
  }

#if DCHECK_IS_ON()
  DCHECK(!request_->TransactionHasQueuedResults());
#endif  // DCHECK_IS_ON()
  request_->OnBlocked(old_version);
  // `this` may be deleted because event dispatch can run a nested loop.
  // Not resetting |request_|.  In this instance we will have to forward at
  // least one other call in the set UpgradeNeeded() / OpenSuccess() /
  // Error().
}

void IDBFactoryClient::UpgradeNeeded(
    mojo::PendingAssociatedRemote<mojom::blink::IDBDatabase> pending_database,
    int64_t old_version,
    mojom::blink::IDBDataLoss data_loss,
    const String& data_loss_message,
    const IDBDatabaseMetadata& metadata) {
  if (!request_) {
    return;
  }

#if DCHECK_IS_ON()
    DCHECK(!request_->TransactionHasQueuedResults());
#endif  // DCHECK_IS_ON()
    request_->OnUpgradeNeeded(old_version, std::move(pending_database),
                              task_runner_, IDBDatabaseMetadata(metadata),
                              data_loss, data_loss_message);
    // `this` may be deleted because event dispatch can run a nested loop.
    // Not resetting |request_|.  In this instance we will have to forward at
    // least one other call in the set UpgradeNeeded() / OpenSuccess() /
    // Error().
}

}  // namespace blink
