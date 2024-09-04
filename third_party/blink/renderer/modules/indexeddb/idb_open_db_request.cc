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
#include <optional>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_database.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_version_change_event.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

IDBOpenDBRequest::IDBOpenDBRequest(
    ScriptState* script_state,
    mojo::PendingAssociatedReceiver<mojom::blink::IDBDatabaseCallbacks>
        callbacks_receiver,
    IDBTransaction::TransactionMojoRemote transaction_remote,
    int64_t transaction_id,
    int64_t version,
    IDBRequest::AsyncTraceState metrics,
    mojo::PendingRemote<mojom::blink::ObservedFeature> connection_lifetime)
    : IDBRequest(script_state, nullptr, nullptr, std::move(metrics)),
      callbacks_receiver_(std::move(callbacks_receiver)),
      transaction_remote_(std::move(transaction_remote)),
      transaction_id_(transaction_id),
      version_(version),
      connection_lifetime_(std::move(connection_lifetime)),
      start_time_(base::Time::Now()) {
  DCHECK(!ResultAsAny());
}

IDBOpenDBRequest::~IDBOpenDBRequest() = default;

void IDBOpenDBRequest::Trace(Visitor* visitor) const {
  visitor->Trace(transaction_remote_);
  IDBRequest::Trace(visitor);
}

void IDBOpenDBRequest::ContextDestroyed() {
  IDBRequest::ContextDestroyed();
  if (factory_client_) {
    factory_client_->DetachRequest();
    factory_client_ = nullptr;
  }
}

std::unique_ptr<IDBFactoryClient> IDBOpenDBRequest::CreateFactoryClient() {
  DCHECK(!factory_client_);
  auto client = std::make_unique<IDBFactoryClient>(this);
  factory_client_ = client.get();
  return client;
}

void IDBOpenDBRequest::FactoryClientDestroyed(
    IDBFactoryClient* factory_client) {
  DCHECK_EQ(factory_client_, factory_client);
  factory_client_ = nullptr;
}

const AtomicString& IDBOpenDBRequest::InterfaceName() const {
  return event_target_names::kIDBOpenDBRequest;
}

void IDBOpenDBRequest::OnBlocked(int64_t old_version) {
  TRACE_EVENT0("IndexedDB", "IDBOpenDBRequest::onBlocked()");
  probe::AsyncTask async_task(GetExecutionContext(), async_task_context(),
                              "blocked");
  if (!CanStillSendResult()) {
    return;
  }
  std::optional<uint64_t> new_version_nullable;
  if (version_ != IDBDatabaseMetadata::kDefaultVersion) {
    new_version_nullable = version_;
  }
  DispatchEvent(*MakeGarbageCollected<IDBVersionChangeEvent>(
      event_type_names::kBlocked, old_version, new_version_nullable));
}

void IDBOpenDBRequest::OnUpgradeNeeded(
    int64_t old_version,
    mojo::PendingAssociatedRemote<mojom::blink::IDBDatabase> pending_database,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const IDBDatabaseMetadata& metadata,
    mojom::blink::IDBDataLoss data_loss,
    String data_loss_message) {
  TRACE_EVENT0("IndexedDB", "IDBOpenDBRequest::onUpgradeNeeded()");
  probe::AsyncTask async_task(GetExecutionContext(), async_task_context(),
                              "upgradeNeeded");
  if (!CanStillSendResult()) {
    metrics_.RecordAndReset();
    return;
  }

  DCHECK(callbacks_receiver_);

  auto* idb_database = MakeGarbageCollected<IDBDatabase>(
      GetExecutionContext(), std::move(callbacks_receiver_),
      std::move(connection_lifetime_), std::move(pending_database),
      connection_priority_);
  idb_database->SetMetadata(metadata);

  if (old_version == IDBDatabaseMetadata::kNoVersion) {
    // This database hasn't had a version before.
    old_version = IDBDatabaseMetadata::kDefaultVersion;
  }
  IDBDatabaseMetadata old_database_metadata(
      metadata.name, metadata.id, old_version, metadata.max_object_store_id,
      metadata.was_cold_open);

  transaction_ = IDBTransaction::CreateVersionChange(
      GetExecutionContext(), std::move(transaction_remote_), transaction_id_,
      idb_database, this, old_database_metadata);
  SetResult(MakeGarbageCollected<IDBAny>(idb_database));

  if (version_ == IDBDatabaseMetadata::kNoVersion)
    version_ = 1;
  DispatchEvent(*MakeGarbageCollected<IDBVersionChangeEvent>(
      event_type_names::kUpgradeneeded, old_version, version_, data_loss,
      data_loss_message));
}

void IDBOpenDBRequest::OnOpenDBSuccess(
    mojo::PendingAssociatedRemote<mojom::blink::IDBDatabase> pending_database,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const IDBDatabaseMetadata& metadata) {
  TRACE_EVENT0("IndexedDB", "IDBOpenDBRequest::onSuccess(database)");
  probe::AsyncTask async_task(GetExecutionContext(), async_task_context(),
                              "success");

  if (!CanStillSendResult()) {
    metrics_.RecordAndReset();
    return;
  }

  IDBDatabase* idb_database = nullptr;
  if (ResultAsAny()) {
    DCHECK(!pending_database.is_valid());
    idb_database = ResultAsAny()->IdbDatabase();
    DCHECK(idb_database);
    DCHECK(!callbacks_receiver_);
  } else {
    DCHECK(pending_database);
    DCHECK(callbacks_receiver_);

    idb_database = MakeGarbageCollected<IDBDatabase>(
        GetExecutionContext(), std::move(callbacks_receiver_),
        std::move(connection_lifetime_), std::move(pending_database),
        connection_priority_);
    SetResult(MakeGarbageCollected<IDBAny>(idb_database));
  }
  idb_database->SetMetadata(metadata);
  DispatchEvent(*Event::Create(event_type_names::kSuccess));
}

void IDBOpenDBRequest::OnDeleteDBSuccess(int64_t old_version) {
  TRACE_EVENT0("IndexedDB", "IDBOpenDBRequest::onDeleteDBSuccess(int64_t)");
  probe::AsyncTask async_task(GetExecutionContext(), async_task_context(),
                              "success");
  if (!CanStillSendResult()) {
    metrics_.RecordAndReset();
    return;
  }
  if (old_version == IDBDatabaseMetadata::kNoVersion) {
    // This database hasn't had an integer version before.
    old_version = IDBDatabaseMetadata::kDefaultVersion;
  }
  SetResult(MakeGarbageCollected<IDBAny>(IDBAny::kUndefinedType));
  DispatchEvent(*MakeGarbageCollected<IDBVersionChangeEvent>(
      event_type_names::kSuccess, old_version, std::nullopt));
}

void IDBOpenDBRequest::OnDBFactoryError(DOMException* error) {
  SendError(error);
}

bool IDBOpenDBRequest::CanStillSendResult() const {
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
    SendError(MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError,
                                                 "The connection was closed."));
    return DispatchEventResult::kCanceledBeforeDispatch;
  }

  if (!open_time_recorded_ &&
      (event.type() == event_type_names::kSuccess ||
       event.type() == event_type_names::kUpgradeneeded) &&
      ResultAsAny()->GetType() == IDBAny::kIDBDatabaseType) {
    // Note: The result type is checked because this request type is also used
    // for calls to DeleteDatabase, which sets the result to undefined (see
    // SendResult(int64_t) above).
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
