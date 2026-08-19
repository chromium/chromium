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

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_database.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_factory.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_version_change_event.h"
#include "third_party/blink/renderer/modules/indexeddb/shared_idb_database_connection.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

IDBOpenDBRequest::IDBOpenDBRequest(
    ScriptState* script_state,
    IDBFactory* factory,
    const String& name,
    mojo::PendingAssociatedReceiver<mojom::blink::IDBDatabaseCallbacks>
        callbacks_receiver,
    IDBTransaction::TransactionMojoRemote transaction_remote,
    int64_t transaction_id,
    int64_t version,
    IDBRequest::AsyncTraceState metrics)
    : IDBRequest(script_state, nullptr, nullptr, std::move(metrics)),
      callbacks_receiver_(std::move(callbacks_receiver)),
      factory_(factory),
      db_name_(name),
      transaction_remote_(std::move(transaction_remote)),
      transaction_id_(transaction_id),
      version_(version) {
  DCHECK(factory_);
  DCHECK(!ResultAsAny());
}

IDBOpenDBRequest::~IDBOpenDBRequest() {
  CHECK(!shared_connection_target_);
  CHECK(shared_requests_.empty());
}

void IDBOpenDBRequest::BindToConnection(
    SharedIDBDatabaseConnection* connection) {
  CHECK(connection);
  CHECK(!shared_connection_target_);
  CHECK_EQ(ready_state_, PENDING);
  shared_connection_target_ = connection;
  // Increment the pending sharing count to lock the connection and prevent
  // it from closing if all active database frontends are closed before this
  // request completes. The count is decremented in OnRequestComplete().
  connection->IncrementPendingSharingCount();
}

void IDBOpenDBRequest::OnRequestComplete() {
  if (!shared_requests_.empty() && GetExecutionContext() &&
      !GetExecutionContext()->IsContextDestroyed()) {
    factory_->PromoteSharedRequest(this, shared_requests_);
  } else {
    factory_->UnregisterPendingRequest(this);
  }
  if (shared_connection_target_) {
    shared_connection_target_->DecrementPendingSharingCount();
    shared_connection_target_ = nullptr;
  }
  shared_requests_.clear();
}

void IDBOpenDBRequest::RegisterSharedConnection(
    SharedIDBDatabaseConnection* connection,
    const String& name) {
  DCHECK(base::FeatureList::IsEnabled(
      features::kIndexedDBConnectionDeduplication));
  factory_->RegisterSharedConnection(name, connection);
  for (auto& shared : shared_requests_) {
    shared->BindToConnection(connection);
  }
  shared_requests_.clear();
}

void IDBOpenDBRequest::Trace(Visitor* visitor) const {
  visitor->Trace(factory_);
  visitor->Trace(shared_connection_target_);

  visitor->Trace(shared_requests_);
  visitor->Trace(transaction_remote_);
  IDBRequest::Trace(visitor);
}

void IDBOpenDBRequest::ContextDestroyed() {
  IDBRequest::ContextDestroyed();
  if (factory_client_) {
    factory_client_->DetachRequest();
    factory_client_ = nullptr;
  }
  OnRequestComplete();
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

  IDBDatabase* idb_database = nullptr;
  if (base::FeatureList::IsEnabled(
          features::kIndexedDBConnectionDeduplication)) {
    auto* shared_connection = MakeGarbageCollected<SharedIDBDatabaseConnection>(
        GetExecutionContext(), std::move(callbacks_receiver_),
        std::move(pending_database), metadata);
    BindToConnection(shared_connection);
    idb_database = MakeGarbageCollected<IDBDatabase>(
        GetExecutionContext(), shared_connection, connection_priority_);
  } else {
    idb_database = MakeGarbageCollected<IDBDatabase>(
        GetExecutionContext(), std::move(callbacks_receiver_),
        std::move(pending_database), connection_priority_);
  }

  idb_database->SetMetadata(metadata);

  if (old_version == IDBDatabaseMetadata::kNoVersion) {
    // This database hasn't had a version before.
    old_version = IDBDatabaseMetadata::kDefaultVersion;
  }
  IDBDatabaseMetadata old_database_metadata;
  old_database_metadata.CopyFrom(metadata);
  old_database_metadata.version = old_version;

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
    if (base::FeatureList::IsEnabled(
            features::kIndexedDBConnectionDeduplication)) {
      CHECK(shared_connection_target_);
      RegisterSharedConnection(shared_connection_target_, metadata.name);
    }
  } else {
    DCHECK(callbacks_receiver_);
    if (base::FeatureList::IsEnabled(
            features::kIndexedDBConnectionDeduplication)) {
      SharedIDBDatabaseConnection* shared_connection;
      // If the browser did not return a new database remote, it means we are
      // sharing an existing connection.
      if (!pending_database.is_valid()) {
        // The primary request should have already pushed the connection target
        // to us when it succeeded.
        CHECK(shared_connection_target_);
        shared_connection = shared_connection_target_;
        // Discard the callbacks receiver; the shared connection already
        // receives callbacks for this pipe.
        callbacks_receiver_.reset();
      } else {
        // This request established a new connection. Create the shared wrapper
        // and register it in the factory cache for future requests to reuse.
        shared_connection = MakeGarbageCollected<SharedIDBDatabaseConnection>(
            GetExecutionContext(), std::move(callbacks_receiver_),
            std::move(pending_database), metadata);
        RegisterSharedConnection(shared_connection, metadata.name);
      }

      idb_database = MakeGarbageCollected<IDBDatabase>(
          GetExecutionContext(), shared_connection, connection_priority_);
    } else {
      DCHECK(pending_database);
      idb_database = MakeGarbageCollected<IDBDatabase>(
          GetExecutionContext(), std::move(callbacks_receiver_),
          std::move(pending_database), connection_priority_);
    }
    SetResult(MakeGarbageCollected<IDBAny>(idb_database));
  }
  idb_database->SetMetadata(metadata);
  OnRequestComplete();
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
  OnRequestComplete();
  // The spec requires oldVersion to be 0 if the database does not exist:
  // https://w3c.github.io/IndexedDB/#delete-a-database.
  CHECK_GE(old_version, 0);
  SetResult(MakeGarbageCollected<IDBAny>(IDBAny::kUndefinedType));
  DispatchEvent(*MakeGarbageCollected<IDBVersionChangeEvent>(
      event_type_names::kSuccess, old_version, std::nullopt));
}

void IDBOpenDBRequest::OnDBFactoryError(DOMException* error) {
  OnRequestComplete();
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

  return IDBRequest::DispatchEventInternal(event);
}

}  // namespace blink
