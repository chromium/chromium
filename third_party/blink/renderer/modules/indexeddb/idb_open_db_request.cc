/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/indexeddb/idb_open_db_request.h"

#include <memory>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_database.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_version_change_event.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

IDBOpenDBRequest::IDBOpenDBRequest(
    ScriptState* script_state,
    mojo::PendingAssociatedReceiver<mojom::blink::IDBDatabaseCallbacks>
        callbacks_receiver,
    std::unique_ptr<WebIDBTransaction> transaction_backend,
    int64_t transaction_id,
    int64_t version,
    IDBRequest::AsyncTraceState metrics,
    mojo::PendingRemote<mojom::blink::ObservedFeature> connection_lifetime)
    : IDBRequest(script_state,
                 nullptr,
                 nullptr,
                 std::move(metrics)),
      callbacks_receiver_(std::move(callbacks_receiver)),
      transaction_backend_(std::move(transaction_backend)),
      transaction_id_(transaction_id),
      version_(version),
      connection_lifetime_(std::move(connection_lifetime)),
      start_time_(base::Time::Now()) {
  DCHECK(!ResultAsAny());
}

IDBOpenDBRequest::~IDBOpenDBRequest() = default;

void IDBOpenDBRequest::Trace(Visitor* visitor) const {
  IDBRequest::Trace(visitor);
}

void IDBOpenDBRequest::ContextDestroyed() {
  IDBRequest::ContextDestroyed();
}

const AtomicString& IDBOpenDBRequest::InterfaceName() const {
  return event_target_names::kIDBOpenDBRequest;
}

void IDBOpenDBRequest::EnqueueBlocked(int64_t old_version) {
  TRACE_EVENT0("IndexedDB", "IDBOpenDBRequest::onBlocked()");
  if (!ShouldEnqueueEvent())
    return;
  absl::optional<uint64_t> new_version_nullable;
  if (version_ != IDBDatabaseMetadata::kDefaultVersion) {
    new_version_nullable = version_;
  }
  EnqueueEvent(MakeGarbageCollected<IDBVersionChangeEvent>(
      event_type_names::kBlocked, old_version, new_version_nullable));
}

void IDBOpenDBRequest::EnqueueUpgradeNeeded(
    int64_t old_version,
    std::unique_ptr<WebIDBDatabase> backend,
    const IDBDatabaseMetadata& metadata,
    mojom::IDBDataLoss data_loss,
    String data_loss_message) {
  TRACE_EVENT0("IndexedDB", "IDBOpenDBRequest::onUpgradeNeeded()");
  if (!ShouldEnqueueEvent()) {
    metrics_.RecordAndReset();
    return;
  }

  DCHECK(callbacks_receiver_);

  auto* idb_database = MakeGarbageCollected<IDBDatabase>(
      GetExecutionContext(), std::move(backend), std::move(callbacks_receiver_),
      std::move(connection_lifetime_));
  idb_database->SetMetadata(metadata);

  if (old_version == IDBDatabaseMetadata::kNoVersion) {
    // This database hasn't had a version before.
    old_version = IDBDatabaseMetadata::kDefaultVersion;
  }
  IDBDatabaseMetadata old_database_metadata(
      metadata.name, metadata.id, old_version, metadata.max_object_store_id,
      metadata.was_cold_open);

  transaction_ = IDBTransaction::CreateVersionChange(
      GetExecutionContext(), std::move(transaction_backend_), transaction_id_,
      idb_database, this, old_database_metadata);
  SetResult(MakeGarbageCollected<IDBAny>(idb_database));

  if (version_ == IDBDatabaseMetadata::kNoVersion)
    version_ = 1;
  EnqueueEvent(MakeGarbageCollected<IDBVersionChangeEvent>(
      event_type_names::kUpgradeneeded, old_version, version_, data_loss,
      data_loss_message));
}

void IDBOpenDBRequest::EnqueueResponse(std::unique_ptr<WebIDBDatabase> backend,
                                       const IDBDatabaseMetadata& metadata) {
  TRACE_EVENT0("IndexedDB", "IDBOpenDBRequest::onSuccess()");
  if (!ShouldEnqueueEvent()) {
    metrics_.RecordAndReset();
    return;
  }

  IDBDatabase* idb_database = nullptr;
  if (ResultAsAny()) {
    // Previous OnUpgradeNeeded call delivered the backend.
    DCHECK(!backend.get());
    idb_database = ResultAsAny()->IdbDatabase();
    DCHECK(idb_database);
    DCHECK(!callbacks_receiver_);
  } else {
    DCHECK(backend.get());
    DCHECK(callbacks_receiver_);
    idb_database = MakeGarbageCollected<IDBDatabase>(
        GetExecutionContext(), std::move(backend),
        std::move(callbacks_receiver_), std::move(connection_lifetime_));
    SetResult(MakeGarbageCollected<IDBAny>(idb_database));
  }
  idb_database->SetMetadata(metadata);
  EnqueueEvent(Event::Create(event_type_names::kSuccess));
}

void IDBOpenDBRequest::EnqueueResponse(int64_t old_version) {
  TRACE_EVENT0("IndexedDB", "IDBOpenDBRequest::onSuccess()");
  if (!ShouldEnqueueEvent()) {
    metrics_.RecordAndReset();
    return;
  }
  if (old_version == IDBDatabaseMetadata::kNoVersion) {
    // This database hasn't had an integer version before.
    old_version = IDBDatabaseMetadata::kDefaultVersion;
  }
  SetResult(MakeGarbageCollected<IDBAny>(IDBAny::kUndefinedType));
  EnqueueEvent(MakeGarbageCollected<IDBVersionChangeEvent>(
      event_type_names::kSuccess, old_version, absl::nullopt));
}

bool IDBOpenDBRequest::ShouldEnqueueEvent() const {
  if (!GetExecutionContext())
    return false;
  DCHECK(ready_state_ == PENDING || ready_state_ == DONE);
  if (request_aborted_)
    return false;
  return true;
}

DispatchEventResult IDBOpenDBRequest::DispatchEventInternal(Event& event) {
  // If this event originated from script, it should have no side effects.
  if (!event.isTrusted())
    return IDBRequest::DispatchEventInternal(event);
  DCHECK(event.type() == event_type_names::kSuccess ||
         event.type() == event_type_names::kError ||
         event.type() == event_type_names::kBlocked ||
         event.type() == event_type_names::kUpgradeneeded)
      << "event type was " << event.type();

  // If the connection closed between onUpgradeNeeded and the delivery of the
  // "success" event, an "error" event should be fired instead.
  if (event.type() == event_type_names::kSuccess &&
      ResultAsAny()->GetType() == IDBAny::kIDBDatabaseType &&
      ResultAsAny()->IdbDatabase()->IsClosePending()) {
    SetResult(nullptr);
    HandleResponse(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kAbortError, "The connection was closed."));
    return DispatchEventResult::kCanceledBeforeDispatch;
  }

  if (!open_time_recorded_ &&
      (event.type() == event_type_names::kSuccess ||
       event.type() == event_type_names::kUpgradeneeded) &&
      ResultAsAny()->GetType() == IDBAny::kIDBDatabaseType) {
    // Note: The result type is checked because this request type is also used
    // for calls to DeleteDatabase, which sets the result to undefined (see
    // EnqueueResponse(int64_t) above).
    open_time_recorded_ = true;
    IDBDatabase* idb_database = ResultAsAny()->IdbDatabase();
    base::TimeDelta time_diff = base::Time::Now() - start_time_;
    if (idb_database->Metadata().was_cold_open)
      UMA_HISTOGRAM_MEDIUM_TIMES("WebCore.IndexedDB.OpenTime.Cold", time_diff);
    else
      UMA_HISTOGRAM_MEDIUM_TIMES("WebCore.IndexedDB.OpenTime.Warm", time_diff);
  }

  return IDBRequest::DispatchEventInternal(event);
}

}  // namespace blink
