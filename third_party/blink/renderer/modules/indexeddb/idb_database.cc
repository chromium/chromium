/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/modules/indexeddb/idb_database.h"

#include <limits>
#include <memory>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/feature_list.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/indexeddb/web_idb_types.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-blink.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_string_stringsequence.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_binding_for_modules.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event_queue.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/indexed_db_names.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_any.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_event_dispatcher.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_index.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key_path.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_version_change_event.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_transaction.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

const char IDBDatabase::kIndexDeletedErrorMessage[] =
    "The index or its object store has been deleted.";
const char IDBDatabase::kIndexNameTakenErrorMessage[] =
    "An index with the specified name already exists.";
const char IDBDatabase::kIsKeyCursorErrorMessage[] =
    "The cursor is a key cursor.";
const char IDBDatabase::kNoKeyOrKeyRangeErrorMessage[] =
    "No key or key range specified.";
const char IDBDatabase::kNoSuchIndexErrorMessage[] =
    "The specified index was not found.";
const char IDBDatabase::kNoSuchObjectStoreErrorMessage[] =
    "The specified object store was not found.";
const char IDBDatabase::kNoValueErrorMessage[] =
    "The cursor is being iterated or has iterated past its end.";
const char IDBDatabase::kNotValidKeyErrorMessage[] =
    "The parameter is not a valid key.";
const char IDBDatabase::kNotVersionChangeTransactionErrorMessage[] =
    "The database is not running a version change transaction.";
const char IDBDatabase::kObjectStoreDeletedErrorMessage[] =
    "The object store has been deleted.";
const char IDBDatabase::kObjectStoreNameTakenErrorMessage[] =
    "An object store with the specified name already exists.";
const char IDBDatabase::kRequestNotFinishedErrorMessage[] =
    "The request has not finished.";
const char IDBDatabase::kSourceDeletedErrorMessage[] =
    "The cursor's source or effective object store has been deleted.";
const char IDBDatabase::kTransactionInactiveErrorMessage[] =
    "The transaction is not active.";
const char IDBDatabase::kTransactionFinishedErrorMessage[] =
    "The transaction has finished.";
const char IDBDatabase::kTransactionReadOnlyErrorMessage[] =
    "The transaction is read-only.";
const char IDBDatabase::kDatabaseClosedErrorMessage[] =
    "The database connection is closed.";

IDBDatabase::IDBDatabase(
    ExecutionContext* context,
    std::unique_ptr<WebIDBDatabase> backend,
    mojo::PendingAssociatedReceiver<mojom::blink::IDBDatabaseCallbacks>
        callbacks_receiver,
    mojo::PendingRemote<mojom::blink::ObservedFeature> connection_lifetime)
    : ExecutionContextLifecycleObserver(context),
      backend_(std::move(backend)),
      connection_lifetime_(std::move(connection_lifetime)),
      event_queue_(
          MakeGarbageCollected<EventQueue>(context, TaskType::kDatabaseAccess)),
      callbacks_receiver_(this, context) {
  if (!base::FeatureList::IsEnabled(
          features::kAllowPageWithIDBConnectionInBFCache) &&
      context) {
    feature_handle_for_scheduler_ = context->GetScheduler()->RegisterFeature(
        SchedulingPolicy::Feature::kIndexedDBConnection,
        {SchedulingPolicy::DisableBackForwardCache()});
  }
  callbacks_receiver_.Bind(std::move(callbacks_receiver),
                           context->GetTaskRunner(TaskType::kDatabaseAccess));
}

IDBDatabase::~IDBDatabase() {
  if (!close_pending_ && backend_)
    backend_->Close();
}

void IDBDatabase::Trace(Visitor* visitor) const {
  visitor->Trace(version_change_transaction_);
  visitor->Trace(transactions_);
  visitor->Trace(event_queue_);
  visitor->Trace(callbacks_receiver_);
  EventTargetWithInlineData::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

int64_t IDBDatabase::NextTransactionId() {
  // Starts at 1, unlike AtomicSequenceNumber.
  // Only keep a 32-bit counter to allow ports to use the other 32
  // bits of the id.
  static base::AtomicSequenceNumber current_transaction_id;
  return current_transaction_id.GetNext() + 1;
}

void IDBDatabase::SetMetadata(const IDBDatabaseMetadata& metadata) {
  metadata_ = metadata;
}

void IDBDatabase::SetDatabaseMetadata(const IDBDatabaseMetadata& metadata) {
  metadata_.CopyFrom(metadata);
}

void IDBDatabase::TransactionCreated(IDBTransaction* transaction) {
  DCHECK(transaction);
  DCHECK(!transactions_.Contains(transaction->Id()));
  transactions_.insert(transaction->Id(), transaction);

  if (transaction->IsVersionChange()) {
    DCHECK(!version_change_transaction_);
    version_change_transaction_ = transaction;
  }
}

void IDBDatabase::TransactionFinished(const IDBTransaction* transaction) {
  DCHECK(transaction);
  DCHECK(transactions_.Contains(transaction->Id()));
  DCHECK_EQ(transactions_.at(transaction->Id()), transaction);
  transactions_.erase(transaction->Id());

  if (transaction->IsVersionChange()) {
    DCHECK_EQ(version_change_transaction_, transaction);
    version_change_transaction_ = nullptr;
  }

  if (close_pending_ && transactions_.empty())
    CloseConnection();
}

void IDBDatabase::ForcedClose() {
  for (const auto& it : transactions_)
    it.value->abort(IGNORE_EXCEPTION_FOR_TESTING);
  this->close();
  EnqueueEvent(Event::Create(event_type_names::kClose));
}

void IDBDatabase::VersionChange(int64_t old_version, int64_t new_version) {
  TRACE_EVENT0("IndexedDB", "IDBDatabase::onVersionChange");
  if (!GetExecutionContext())
    return;

  if (close_pending_) {
    // If we're pending, that means there's a busy transaction. We won't
    // fire 'versionchange' but since we're not closing immediately the
    // back-end should still send out 'blocked'.
    backend_->VersionChangeIgnored();
    return;
  }

  absl::optional<uint64_t> new_version_nullable;
  if (new_version != IDBDatabaseMetadata::kNoVersion) {
    new_version_nullable = new_version;
  }
  EnqueueEvent(MakeGarbageCollected<IDBVersionChangeEvent>(
      event_type_names::kVersionchange, old_version, new_version_nullable));
}

void IDBDatabase::Abort(int64_t transaction_id,
                        mojom::blink::IDBException code,
                        const WTF::String& message) {
  DCHECK(transactions_.Contains(transaction_id));
  transactions_.at(transaction_id)
      ->OnAbort(MakeGarbageCollected<DOMException>(
          static_cast<DOMExceptionCode>(code), message));
}

void IDBDatabase::Complete(int64_t transaction_id) {
  DCHECK(transactions_.Contains(transaction_id));
  transactions_.at(transaction_id)->OnComplete();
}

DOMStringList* IDBDatabase::objectStoreNames() const {
  auto* object_store_names = MakeGarbageCollected<DOMStringList>();
  for (const auto& it : metadata_.object_stores)
    object_store_names->Append(it.value->name);
  object_store_names->Sort();
  return object_store_names;
}

const String& IDBDatabase::GetObjectStoreName(int64_t object_store_id) const {
  const auto& it = metadata_.object_stores.find(object_store_id);
  DCHECK(it != metadata_.object_stores.end());
  return it->value->name;
}

IDBObjectStore* IDBDatabase::createObjectStore(
    const String& name,
    const IDBKeyPath& key_path,
    bool auto_increment,
    ExceptionState& exception_state) {
  TRACE_EVENT0("IndexedDB", "IDBDatabase::createObjectStore");

  if (!version_change_transaction_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        IDBDatabase::kNotVersionChangeTransactionErrorMessage);
    return nullptr;
  }
  if (!version_change_transaction_->IsActive()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kTransactionInactiveError,
        version_change_transaction_->InactiveErrorMessage());
    return nullptr;
  }

  if (!key_path.IsNull() && !key_path.IsValid()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The keyPath option is not a valid key path.");
    return nullptr;
  }

  if (ContainsObjectStore(name)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kConstraintError,
        IDBDatabase::kObjectStoreNameTakenErrorMessage);
    return nullptr;
  }

  if (auto_increment && ((key_path.GetType() == mojom::IDBKeyPathType::String &&
                          key_path.GetString().empty()) ||
                         key_path.GetType() == mojom::IDBKeyPathType::Array)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "The autoIncrement option was set but the "
        "keyPath option was empty or an array.");
    return nullptr;
  }

  if (!backend_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kDatabaseClosedErrorMessage);
    return nullptr;
  }

  int64_t object_store_id = metadata_.max_object_store_id + 1;
  DCHECK_NE(object_store_id, IDBObjectStoreMetadata::kInvalidId);
  version_change_transaction_->transaction_backend()->CreateObjectStore(
      object_store_id, name, key_path, auto_increment);

  scoped_refptr<IDBObjectStoreMetadata> store_metadata =
      base::AdoptRef(new IDBObjectStoreMetadata(
          name, object_store_id, key_path, auto_increment,
          WebIDBDatabase::kMinimumIndexId));
  auto* object_store = MakeGarbageCollected<IDBObjectStore>(
      store_metadata, version_change_transaction_.Get());
  version_change_transaction_->ObjectStoreCreated(name, object_store);
  metadata_.object_stores.Set(object_store_id, std::move(store_metadata));
  ++metadata_.max_object_store_id;

  return object_store;
}

void IDBDatabase::deleteObjectStore(const String& name,
                                    ExceptionState& exception_state) {
  TRACE_EVENT0("IndexedDB", "IDBDatabase::deleteObjectStore");
  if (!version_change_transaction_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        IDBDatabase::kNotVersionChangeTransactionErrorMessage);
    return;
  }
  if (!version_change_transaction_->IsActive()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kTransactionInactiveError,
        version_change_transaction_->InactiveErrorMessage());
    return;
  }

  int64_t object_store_id = FindObjectStoreId(name);
  if (object_store_id == IDBObjectStoreMetadata::kInvalidId) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotFoundError,
        "The specified object store was not found.");
    return;
  }

  if (!backend_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kDatabaseClosedErrorMessage);
    return;
  }

  version_change_transaction_->transaction_backend()->DeleteObjectStore(
      object_store_id);
  version_change_transaction_->ObjectStoreDeleted(object_store_id, name);
  metadata_.object_stores.erase(object_store_id);
}

IDBTransaction* IDBDatabase::transaction(
    ScriptState* script_state,
    const V8UnionStringOrStringSequence* store_names,
    const String& mode_string,
    const IDBTransactionOptions* options,
    ExceptionState& exception_state) {
  TRACE_EVENT0("IndexedDB", "IDBDatabase::transaction");

  HashSet<String> scope;
  DCHECK(store_names);
  switch (store_names->GetContentType()) {
    case V8UnionStringOrStringSequence::ContentType::kString:
      scope.insert(store_names->GetAsString());
      break;
    case V8UnionStringOrStringSequence::ContentType::kStringSequence:
      for (const String& name : store_names->GetAsStringSequence())
        scope.insert(name);
      break;
  }

  if (version_change_transaction_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "A version change transaction is running.");
    return nullptr;
  }

  if (close_pending_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The database connection is closing.");
    return nullptr;
  }

  if (!backend_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kDatabaseClosedErrorMessage);
    return nullptr;
  }

  if (scope.empty()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The storeNames parameter was empty.");
    return nullptr;
  }

  Vector<int64_t> object_store_ids;
  for (const String& name : scope) {
    int64_t object_store_id = FindObjectStoreId(name);
    if (object_store_id == IDBObjectStoreMetadata::kInvalidId) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kNotFoundError,
          "One of the specified object stores was not found.");
      return nullptr;
    }
    object_store_ids.push_back(object_store_id);
  }

  mojom::IDBTransactionMode mode = IDBTransaction::StringToMode(mode_string);
  if (mode != mojom::IDBTransactionMode::ReadOnly &&
      mode != mojom::IDBTransactionMode::ReadWrite) {
    exception_state.ThrowTypeError(
        "The mode provided ('" + mode_string +
        "') is not one of 'readonly' or 'readwrite'.");
    return nullptr;
  }

  // TODO(cmp): Delete |transaction_id| once all users are removed.
  int64_t transaction_id = NextTransactionId();
  auto transaction_backend = std::make_unique<WebIDBTransaction>(
      ExecutionContext::From(script_state)
          ->GetTaskRunner(TaskType::kDatabaseAccess),
      transaction_id);

  mojom::blink::IDBTransactionDurability durability =
      mojom::blink::IDBTransactionDurability::Default;
  DCHECK(options);
  if (options->durability() == indexed_db_names::kRelaxed) {
    durability = mojom::blink::IDBTransactionDurability::Relaxed;
  } else if (options->durability() == indexed_db_names::kStrict) {
    durability = mojom::blink::IDBTransactionDurability::Strict;
  }

  backend_->CreateTransaction(transaction_backend->CreateReceiver(),
                              transaction_id, object_store_ids, mode,
                              durability);

  return IDBTransaction::CreateNonVersionChange(
      script_state, std::move(transaction_backend), transaction_id, scope, mode,
      durability, this);
}

void IDBDatabase::close() {
  TRACE_EVENT0("IndexedDB", "IDBDatabase::close");
  if (close_pending_)
    return;

  connection_lifetime_.reset();
  close_pending_ = true;
  feature_handle_for_scheduler_.reset();

  if (transactions_.empty())
    CloseConnection();
}

void IDBDatabase::CloseConnection() {
  DCHECK(close_pending_);
  DCHECK(transactions_.empty());

  if (backend_) {
    backend_->Close();
    backend_.reset();
  }

  if (callbacks_receiver_.is_bound())
    callbacks_receiver_.reset();

  if (!GetExecutionContext())
    return;

  // Remove any pending versionchange events scheduled to fire on this
  // connection. They would have been scheduled by the backend when another
  // connection attempted an upgrade, but the frontend connection is being
  // closed before they could fire.
  event_queue_->CancelAllEvents();
}

void IDBDatabase::EnqueueEvent(Event* event) {
  DCHECK(GetExecutionContext());
  event->SetTarget(this);
  event_queue_->EnqueueEvent(FROM_HERE, *event);
}

DispatchEventResult IDBDatabase::DispatchEventInternal(Event& event) {
  TRACE_EVENT0("IndexedDB", "IDBDatabase::dispatchEvent");

  event.SetTarget(this);

  // If this event originated from script, it should have no side effects.
  if (!event.isTrusted())
    return EventTarget::DispatchEventInternal(event);
  DCHECK(event.type() == event_type_names::kVersionchange ||
         event.type() == event_type_names::kClose);

  if (!GetExecutionContext())
    return DispatchEventResult::kCanceledBeforeDispatch;

  DispatchEventResult dispatch_result =
      EventTarget::DispatchEventInternal(event);
  if (event.type() == event_type_names::kVersionchange && !close_pending_ &&
      backend_)
    backend_->VersionChangeIgnored();
  return dispatch_result;
}

int64_t IDBDatabase::FindObjectStoreId(const String& name) const {
  for (const auto& it : metadata_.object_stores) {
    if (it.value->name == name) {
      DCHECK_NE(it.key, IDBObjectStoreMetadata::kInvalidId);
      return it.key;
    }
  }
  return IDBObjectStoreMetadata::kInvalidId;
}

void IDBDatabase::RenameObjectStore(int64_t object_store_id,
                                    const String& new_name) {
  DCHECK(version_change_transaction_)
      << "Object store renamed on database without a versionchange transaction";
  DCHECK(version_change_transaction_->IsActive())
      << "Object store renamed when versionchange transaction is not active";
  DCHECK(backend_) << "Object store renamed after database connection closed";
  DCHECK(metadata_.object_stores.Contains(object_store_id));

  backend_->RenameObjectStore(version_change_transaction_->Id(),
                              object_store_id, new_name);

  IDBObjectStoreMetadata* object_store_metadata =
      metadata_.object_stores.at(object_store_id);
  version_change_transaction_->ObjectStoreRenamed(object_store_metadata->name,
                                                  new_name);
  object_store_metadata->name = new_name;
}

void IDBDatabase::RevertObjectStoreCreation(int64_t object_store_id) {
  DCHECK(version_change_transaction_) << "Object store metadata reverted on "
                                         "database without a versionchange "
                                         "transaction";
  DCHECK(!version_change_transaction_->IsActive())
      << "Object store metadata reverted when versionchange transaction is "
         "still active";
  DCHECK(metadata_.object_stores.Contains(object_store_id));
  metadata_.object_stores.erase(object_store_id);
}

void IDBDatabase::RevertObjectStoreMetadata(
    scoped_refptr<IDBObjectStoreMetadata> old_metadata) {
  DCHECK(version_change_transaction_) << "Object store metadata reverted on "
                                         "database without a versionchange "
                                         "transaction";
  DCHECK(!version_change_transaction_->IsActive())
      << "Object store metadata reverted when versionchange transaction is "
         "still active";
  DCHECK(old_metadata.get());
  metadata_.object_stores.Set(old_metadata->id, std::move(old_metadata));
}

bool IDBDatabase::HasPendingActivity() const {
  // The script wrapper must not be collected before the object is closed or
  // we can't fire a "versionchange" event to let script manually close the
  // connection.
  return !close_pending_ && GetExecutionContext() && HasEventListeners();
}

void IDBDatabase::ContextDestroyed() {
  // Immediately close the connection to the back end. Don't attempt a
  // normal close() since that may wait on transactions which require a
  // round trip to the back-end to abort.
  if (backend_) {
    backend_->Close();
    backend_.reset();
  }

  connection_lifetime_.reset();
}

void IDBDatabase::ContextEnteredBackForwardCache() {
  if (features::IsAllowPageWithIDBConnectionAndTransactionInBFCacheEnabled()) {
    if (backend_) {
      backend_->DidBecomeInactive();
    }
  }
}

const AtomicString& IDBDatabase::InterfaceName() const {
  return event_target_names::kIDBDatabase;
}

ExecutionContext* IDBDatabase::GetExecutionContext() const {
  return ExecutionContextLifecycleObserver::GetExecutionContext();
}

STATIC_ASSERT_ENUM(mojom::blink::IDBException::kNoError,
                   DOMExceptionCode::kNoError);
STATIC_ASSERT_ENUM(mojom::blink::IDBException::kUnknownError,
                   DOMExceptionCode::kUnknownError);
STATIC_ASSERT_ENUM(mojom::blink::IDBException::kConstraintError,
                   DOMExceptionCode::kConstraintError);
STATIC_ASSERT_ENUM(mojom::blink::IDBException::kDataError,
                   DOMExceptionCode::kDataError);
STATIC_ASSERT_ENUM(mojom::blink::IDBException::kVersionError,
                   DOMExceptionCode::kVersionError);
STATIC_ASSERT_ENUM(mojom::blink::IDBException::kAbortError,
                   DOMExceptionCode::kAbortError);
STATIC_ASSERT_ENUM(mojom::blink::IDBException::kQuotaError,
                   DOMExceptionCode::kQuotaExceededError);
STATIC_ASSERT_ENUM(mojom::blink::IDBException::kTimeoutError,
                   DOMExceptionCode::kTimeoutError);

}  // namespace blink
