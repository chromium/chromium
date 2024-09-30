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
#include <optional>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-blink.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_string_stringsequence.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_binding_for_modules.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/indexed_db_names.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_any.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_cursor.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_event_dispatcher.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_index.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key_path.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key_range.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_version_change_event.h"
#include "third_party/blink/renderer/modules/indexeddb/indexed_db_blink_mojom_traits.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
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
    mojo::PendingAssociatedReceiver<mojom::blink::IDBDatabaseCallbacks>
        callbacks_receiver,
    mojo::PendingRemote<mojom::blink::ObservedFeature> connection_lifetime,
    mojo::PendingAssociatedRemote<mojom::blink::IDBDatabase> pending_database,
    int connection_priority)
    : ActiveScriptWrappable<IDBDatabase>({}),
      ExecutionContextLifecycleObserver(context),
      database_remote_(context),
      connection_lifetime_(std::move(connection_lifetime)),
      scheduling_priority_(connection_priority),
      callbacks_receiver_(this, context) {
  database_remote_.Bind(std::move(pending_database),
                        context->GetTaskRunner(TaskType::kDatabaseAccess));
  callbacks_receiver_.Bind(std::move(callbacks_receiver),
                           context->GetTaskRunner(TaskType::kDatabaseAccess));

  // Invokes the callback immediately.
  scheduler_observer_ = context->GetScheduler()->AddLifecycleObserver(
      FrameOrWorkerScheduler::ObserverType::kWorkerScheduler,
      WTF::BindRepeating(&IDBDatabase::OnSchedulerLifecycleStateChanged,
                         WrapWeakPersistent(this)));
}

void IDBDatabase::Trace(Visitor* visitor) const {
  visitor->Trace(database_remote_);
  visitor->Trace(version_change_transaction_);
  visitor->Trace(transactions_);
  visitor->Trace(callbacks_receiver_);
  EventTarget::Trace(visitor);
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

void IDBDatabase::TransactionWillFinish(const IDBTransaction* transaction) {
  if (version_change_transaction_ && transaction->IsVersionChange()) {
    DCHECK_EQ(version_change_transaction_, transaction);
    version_change_transaction_ = nullptr;
  }
}

void IDBDatabase::TransactionFinished(const IDBTransaction* transaction) {
  DCHECK(transaction);
  DCHECK(transactions_.Contains(transaction->Id()));
  DCHECK_EQ(transactions_.at(transaction->Id()), transaction);
  transactions_.erase(transaction->Id());

  TransactionWillFinish(transaction);

  if (close_pending_ && transactions_.empty()) {
    CloseConnection();
  }
}

void IDBDatabase::ForcedClose() {
  for (const auto& it : transactions_) {
    it.value->StartAborting(nullptr);
  }
  this->close();
  DispatchEvent(*Event::Create(event_type_names::kClose));
}

void IDBDatabase::VersionChange(int64_t old_version, int64_t new_version) {
  TRACE_EVENT0("IndexedDB", "IDBDatabase::onVersionChange");
  if (!GetExecutionContext()) {
    return;
  }

  if (close_pending_) {
    // If we're pending, that means there's a busy transaction. We won't
    // fire 'versionchange' but since we're not closing immediately the
    // back-end should still send out 'blocked'.
    VersionChangeIgnored();
    return;
  }

  std::optional<uint64_t> new_version_nullable;
  if (new_version != IDBDatabaseMetadata::kNoVersion) {
    new_version_nullable = new_version;
  }
  DispatchEvent(*MakeGarbageCollected<IDBVersionChangeEvent>(
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
  for (const auto& it : metadata_.object_stores) {
    object_store_names->Append(it.value->name);
  }
  object_store_names->Sort();
  return object_store_names;
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

  if (!database_remote_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kDatabaseClosedErrorMessage);
    return nullptr;
  }

  int64_t object_store_id = metadata_.max_object_store_id + 1;
  DCHECK_NE(object_store_id, IDBObjectStoreMetadata::kInvalidId);
  version_change_transaction_->CreateObjectStore(object_store_id, name,
                                                 key_path, auto_increment);

  scoped_refptr<IDBObjectStoreMetadata> store_metadata = base::AdoptRef(
      new IDBObjectStoreMetadata(name, object_store_id, key_path,
                                 auto_increment, IDBDatabase::kMinimumIndexId));
  auto* object_store = MakeGarbageCollected<IDBObjectStore>(
      store_metadata, version_change_transaction_.Get());
  version_change_transaction_->ObjectStoreCreated(name, object_store);
  metadata_.object_stores.Set(object_store_id, std::move(store_metadata));
  ++metadata_.max_object_store_id;

  return object_store;
}

IDBTransaction* IDBDatabase::transaction(
    ScriptState* script_state,
    const V8UnionStringOrStringSequence* store_names,
    const V8IDBTransactionMode& v8_mode,
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
      for (const String& name : store_names->GetAsStringSequence()) {
        scope.insert(name);
      }
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

  if (!database_remote_.is_bound()) {
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

  mojom::blink::IDBTransactionMode mode =
      IDBTransaction::EnumToMode(v8_mode.AsEnum());
  if (mode != mojom::blink::IDBTransactionMode::ReadOnly &&
      mode != mojom::blink::IDBTransactionMode::ReadWrite) {
    exception_state.ThrowTypeError(
        "The mode provided ('" + v8_mode.AsString() +
        "') is not one of 'readonly' or 'readwrite'.");
    return nullptr;
  }

  mojom::blink::IDBTransactionDurability durability =
      mojom::blink::IDBTransactionDurability::Default;
  DCHECK(options);
  if (options->durability() == indexed_db_names::kRelaxed) {
    durability = mojom::blink::IDBTransactionDurability::Relaxed;
  } else if (options->durability() == indexed_db_names::kStrict) {
    durability = mojom::blink::IDBTransactionDurability::Strict;
  }

  // TODO(cmp): Delete |transaction_id| once all users are removed.
  int64_t transaction_id = NextTransactionId();
  auto* execution_context = ExecutionContext::From(script_state);
  IDBTransaction::TransactionMojoRemote transaction_remote(execution_context);
  mojo::PendingAssociatedReceiver<mojom::blink::IDBTransaction> receiver =
      transaction_remote.BindNewEndpointAndPassReceiver(
          execution_context->GetTaskRunner(TaskType::kDatabaseAccess));
  CreateTransaction(std::move(receiver), transaction_id, object_store_ids, mode,
                    durability);

  return IDBTransaction::CreateNonVersionChange(
      script_state, std::move(transaction_remote), transaction_id, scope, mode,
      durability, this);
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

  if (!database_remote_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kDatabaseClosedErrorMessage);
    return;
  }

  version_change_transaction_->DeleteObjectStore(object_store_id);
  version_change_transaction_->ObjectStoreDeleted(object_store_id, name);
  metadata_.object_stores.erase(object_store_id);
}

void IDBDatabase::close() {
  TRACE_EVENT0("IndexedDB", "IDBDatabase::close");
  if (close_pending_) {
    return;
  }

  connection_lifetime_.reset();
  close_pending_ = true;

  if (transactions_.empty()) {
    CloseConnection();
  }
}

void IDBDatabase::CloseConnection() {
  DCHECK(close_pending_);
  DCHECK(transactions_.empty());

  if (database_remote_.is_bound()) {
    database_remote_.reset();
  }

  if (callbacks_receiver_.is_bound()) {
    callbacks_receiver_.reset();
  }
}

DispatchEventResult IDBDatabase::DispatchEventInternal(Event& event) {
  TRACE_EVENT0("IndexedDB", "IDBDatabase::dispatchEvent");

  event.SetTarget(this);

  // If this event originated from script, it should have no side effects.
  if (!event.isTrusted()) {
    return EventTarget::DispatchEventInternal(event);
  }
  DCHECK(event.type() == event_type_names::kVersionchange ||
         event.type() == event_type_names::kClose);

  if (!GetExecutionContext()) {
    return DispatchEventResult::kCanceledBeforeDispatch;
  }

  DispatchEventResult dispatch_result =
      EventTarget::DispatchEventInternal(event);

  if (event.type() == event_type_names::kVersionchange && !close_pending_ &&
      database_remote_.is_bound()) {
    VersionChangeIgnored();
  }
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
      << "Object store renamed on database without a versionchange "
         "transaction";
  DCHECK(version_change_transaction_->IsActive())
      << "Object store renamed when versionchange transaction is not active";
  DCHECK(metadata_.object_stores.Contains(object_store_id));

  RenameObjectStore(version_change_transaction_->Id(), object_store_id,
                    new_name);

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
  if (database_remote_.is_bound()) {
    database_remote_.reset();
  }
  connection_lifetime_.reset();
}

void IDBDatabase::ContextEnteredBackForwardCache() {
  if (database_remote_.is_bound()) {
    DidBecomeInactive();
  }
}

bool IDBDatabase::IsConnectionOpen() const {
  return database_remote_.is_bound();
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

void IDBDatabase::Get(
    int64_t transaction_id,
    int64_t object_store_id,
    int64_t index_id,
    const IDBKeyRange* key_range,
    bool key_only,
    base::OnceCallback<void(mojom::blink::IDBDatabaseGetResultPtr)>
        result_callback) {
  IDBCursor::ResetCursorPrefetchCaches(transaction_id, nullptr);

  mojom::blink::IDBKeyRangePtr key_range_ptr =
      mojom::blink::IDBKeyRange::From(key_range);
  database_remote_->Get(transaction_id, object_store_id, index_id,
                        std::move(key_range_ptr), key_only,
                        std::move(result_callback));
}

void IDBDatabase::GetAll(int64_t transaction_id,
                         int64_t object_store_id,
                         int64_t index_id,
                         const IDBKeyRange* key_range,
                         int64_t max_count,
                         bool key_only,
                         IDBRequest* request) {
  IDBCursor::ResetCursorPrefetchCaches(transaction_id, nullptr);

  mojom::blink::IDBKeyRangePtr key_range_ptr =
      mojom::blink::IDBKeyRange::From(key_range);
  database_remote_->GetAll(
      transaction_id, object_store_id, index_id, std::move(key_range_ptr),
      key_only, max_count,
      WTF::BindOnce(&IDBRequest::OnGetAll, WrapWeakPersistent(request),
                    key_only));
}

void IDBDatabase::SetIndexKeys(int64_t transaction_id,
                               int64_t object_store_id,
                               std::unique_ptr<IDBKey> primary_key,
                               Vector<IDBIndexKeys> index_keys) {
  database_remote_->SetIndexKeys(transaction_id, object_store_id,
                                 std::move(primary_key), std::move(index_keys));
}

void IDBDatabase::SetIndexesReady(int64_t transaction_id,
                                  int64_t object_store_id,
                                  const Vector<int64_t>& index_ids) {
  database_remote_->SetIndexesReady(transaction_id, object_store_id,
                                    std::move(index_ids));
}

void IDBDatabase::OpenCursor(int64_t object_store_id,
                             int64_t index_id,
                             const IDBKeyRange* key_range,
                             mojom::blink::IDBCursorDirection direction,
                             bool key_only,
                             mojom::blink::IDBTaskType task_type,
                             IDBRequest* request) {
  IDBCursor::ResetCursorPrefetchCaches(request->transaction()->Id(), nullptr);

  mojom::blink::IDBKeyRangePtr key_range_ptr =
      mojom::blink::IDBKeyRange::From(key_range);
  database_remote_->OpenCursor(
      request->transaction()->Id(), object_store_id, index_id,
      std::move(key_range_ptr), direction, key_only, task_type,
      WTF::BindOnce(&IDBRequest::OnOpenCursor, WrapWeakPersistent(request)));
}

void IDBDatabase::Count(int64_t transaction_id,
                        int64_t object_store_id,
                        int64_t index_id,
                        const IDBKeyRange* key_range,
                        mojom::blink::IDBDatabase::CountCallback callback) {
  IDBCursor::ResetCursorPrefetchCaches(transaction_id, nullptr);

  database_remote_->Count(transaction_id, object_store_id, index_id,
                          mojom::blink::IDBKeyRange::From(key_range),
                          std::move(callback));
}

void IDBDatabase::Delete(int64_t transaction_id,
                         int64_t object_store_id,
                         const IDBKey* primary_key,
                         base::OnceCallback<void(bool)> success_callback) {
  IDBCursor::ResetCursorPrefetchCaches(transaction_id, nullptr);

  mojom::blink::IDBKeyRangePtr key_range_ptr =
      mojom::blink::IDBKeyRange::From(IDBKeyRange::Create(primary_key));
  database_remote_->DeleteRange(transaction_id, object_store_id,
                                std::move(key_range_ptr),
                                std::move(success_callback));
}

void IDBDatabase::DeleteRange(int64_t transaction_id,
                              int64_t object_store_id,
                              const IDBKeyRange* key_range,
                              base::OnceCallback<void(bool)> success_callback) {
  IDBCursor::ResetCursorPrefetchCaches(transaction_id, nullptr);

  mojom::blink::IDBKeyRangePtr key_range_ptr =
      mojom::blink::IDBKeyRange::From(key_range);
  database_remote_->DeleteRange(transaction_id, object_store_id,
                                std::move(key_range_ptr),
                                std::move(success_callback));
}

void IDBDatabase::GetKeyGeneratorCurrentNumber(
    int64_t transaction_id,
    int64_t object_store_id,
    mojom::blink::IDBDatabase::GetKeyGeneratorCurrentNumberCallback callback) {
  database_remote_->GetKeyGeneratorCurrentNumber(
      transaction_id, object_store_id, std::move(callback));
}

void IDBDatabase::Clear(
    int64_t transaction_id,
    int64_t object_store_id,
    mojom::blink::IDBDatabase::ClearCallback success_callback) {
  IDBCursor::ResetCursorPrefetchCaches(transaction_id, nullptr);
  database_remote_->Clear(transaction_id, object_store_id,
                          std::move(success_callback));
}

void IDBDatabase::CreateIndex(int64_t transaction_id,
                              int64_t object_store_id,
                              int64_t index_id,
                              const String& name,
                              const IDBKeyPath& key_path,
                              bool unique,
                              bool multi_entry) {
  database_remote_->CreateIndex(transaction_id, object_store_id, index_id, name,
                                key_path, unique, multi_entry);
}

void IDBDatabase::DeleteIndex(int64_t transaction_id,
                              int64_t object_store_id,
                              int64_t index_id) {
  database_remote_->DeleteIndex(transaction_id, object_store_id, index_id);
}

void IDBDatabase::RenameIndex(int64_t transaction_id,
                              int64_t object_store_id,
                              int64_t index_id,
                              const String& new_name) {
  DCHECK(!new_name.IsNull());
  database_remote_->RenameIndex(transaction_id, object_store_id, index_id,
                                new_name);
}

void IDBDatabase::Abort(int64_t transaction_id) {
  if (database_remote_.is_bound()) {
    database_remote_->Abort(transaction_id);
  }
}

void IDBDatabase::OnSchedulerLifecycleStateChanged(
    scheduler::SchedulingLifecycleState lifecycle_state) {
  int new_priority = GetSchedulingPriority(lifecycle_state);
  if (new_priority == scheduling_priority_) {
    return;
  }
  if (database_remote_) {
    database_remote_->UpdatePriority(scheduling_priority_);
  }
}

// static
int IDBDatabase::GetSchedulingPriority(
    scheduler::SchedulingLifecycleState lifecycle_state) {
  switch (lifecycle_state) {
    case scheduler::SchedulingLifecycleState::kNotThrottled:
      return 0;
    case scheduler::SchedulingLifecycleState::kHidden:
      return 1;
    case scheduler::SchedulingLifecycleState::kThrottled:
      return 2;
    case scheduler::SchedulingLifecycleState::kStopped:
      return 3;
  }

  return 0;
}

}  // namespace blink
