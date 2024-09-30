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

#include "third_party/blink/renderer/modules/indexeddb/idb_transaction.h"

#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/format_macros.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable_creation_key.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/indexed_db_names.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_cursor.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_database.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_event_dispatcher.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_index.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_object_store.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_open_db_request.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_request_queue_item.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

IDBTransaction* IDBTransaction::CreateNonVersionChange(
    ScriptState* script_state,
    TransactionMojoRemote remote,
    int64_t id,
    const HashSet<String>& scope,
    mojom::blink::IDBTransactionMode mode,
    mojom::blink::IDBTransactionDurability durability,
    IDBDatabase* db) {
  DCHECK_NE(mode, mojom::blink::IDBTransactionMode::VersionChange);
  DCHECK(!scope.empty()) << "Non-version transactions should operate on a "
                            "well-defined set of stores";

  return MakeGarbageCollected<IDBTransaction>(script_state, std::move(remote),
                                              id, scope, mode, durability, db);
}

IDBTransaction* IDBTransaction::CreateVersionChange(
    ExecutionContext* execution_context,
    TransactionMojoRemote remote,
    int64_t id,
    IDBDatabase* db,
    IDBOpenDBRequest* open_db_request,
    const IDBDatabaseMetadata& old_metadata) {
  return MakeGarbageCollected<IDBTransaction>(execution_context,
                                              std::move(remote), id, db,
                                              open_db_request, old_metadata);
}

IDBTransaction::IDBTransaction(
    ScriptState* script_state,
    TransactionMojoRemote remote,
    int64_t id,
    const HashSet<String>& scope,
    mojom::blink::IDBTransactionMode mode,
    mojom::blink::IDBTransactionDurability durability,
    IDBDatabase* db)
    : ActiveScriptWrappable<IDBTransaction>({}),
      ExecutionContextLifecycleObserver(ExecutionContext::From(script_state)),
      remote_(std::move(remote)),
      id_(id),
      database_(db),
      mode_(mode),
      durability_(durability),
      scope_(scope),
      state_(kActive) {
  DCHECK(database_);
  DCHECK(!scope_.empty()) << "Non-versionchange transactions must operate "
                             "on a well-defined set of stores";
  DCHECK(mode_ == mojom::blink::IDBTransactionMode::ReadOnly ||
         mode_ == mojom::blink::IDBTransactionMode::ReadWrite)
      << "Invalid transaction mode";

  ExecutionContext::From(script_state)
      ->GetAgent()
      ->event_loop()
      ->EnqueueEndOfMicrotaskCheckpointTask(WTF::BindOnce(
          &IDBTransaction::SetActive, WrapPersistent(this), false));

  database_->TransactionCreated(this);
}

IDBTransaction::IDBTransaction(ExecutionContext* execution_context,
                               TransactionMojoRemote remote,
                               int64_t id,
                               IDBDatabase* db,
                               IDBOpenDBRequest* open_db_request,
                               const IDBDatabaseMetadata& old_metadata)
    : ActiveScriptWrappable<IDBTransaction>({}),
      ExecutionContextLifecycleObserver(execution_context),
      remote_(std::move(remote)),
      id_(id),
      database_(db),
      open_db_request_(open_db_request),
      mode_(mojom::blink::IDBTransactionMode::VersionChange),
      durability_(mojom::blink::IDBTransactionDurability::Default),
      state_(kInactive),
      old_database_metadata_(old_metadata) {
  DCHECK(database_);
  DCHECK(open_db_request_);
  DCHECK(scope_.empty());

  database_->TransactionCreated(this);
}

IDBTransaction::~IDBTransaction() {
  // Note: IDBTransaction is a ExecutionContextLifecycleObserver (rather than
  // ContextClient) only in order to be able call upon GetExecutionContext()
  // during this destructor.
  DCHECK(state_ == kFinished || !GetExecutionContext());
  DCHECK(request_list_.empty() || !GetExecutionContext());
}

void IDBTransaction::Trace(Visitor* visitor) const {
  visitor->Trace(remote_);
  visitor->Trace(database_);
  visitor->Trace(open_db_request_);
  visitor->Trace(error_);
  visitor->Trace(request_list_);
  visitor->Trace(object_store_map_);
  visitor->Trace(old_store_metadata_);
  visitor->Trace(deleted_indexes_);
  EventTarget::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

void IDBTransaction::SetError(DOMException* error) {
  DCHECK_NE(state_, kFinished);
  DCHECK(error);

  // The first error to be set is the true cause of the
  // transaction abort.
  if (!error_)
    error_ = error;
}

IDBObjectStore* IDBTransaction::objectStore(const String& name,
                                            ExceptionState& exception_state) {
  if (IsFinished() || IsFinishing()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        IDBDatabase::kTransactionFinishedErrorMessage);
    return nullptr;
  }

  IDBObjectStoreMap::iterator it = object_store_map_.find(name);
  if (it != object_store_map_.end())
    return it->value.Get();

  if (!IsVersionChange() && !scope_.Contains(name)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotFoundError,
        IDBDatabase::kNoSuchObjectStoreErrorMessage);
    return nullptr;
  }

  int64_t object_store_id = database_->FindObjectStoreId(name);
  if (object_store_id == IDBObjectStoreMetadata::kInvalidId) {
    DCHECK(IsVersionChange());
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotFoundError,
        IDBDatabase::kNoSuchObjectStoreErrorMessage);
    return nullptr;
  }

  DCHECK(database_->Metadata().object_stores.Contains(object_store_id));
  scoped_refptr<IDBObjectStoreMetadata> object_store_metadata =
      database_->Metadata().object_stores.at(object_store_id);
  DCHECK(object_store_metadata.get());

  auto* object_store = MakeGarbageCollected<IDBObjectStore>(
      std::move(object_store_metadata), this);
  DCHECK(!object_store_map_.Contains(name));
  object_store_map_.Set(name, object_store);

  if (IsVersionChange()) {
    DCHECK(!object_store->IsNewlyCreated())
        << "Object store IDs are not assigned sequentially";
    scoped_refptr<IDBObjectStoreMetadata> backup_metadata =
        object_store->Metadata().CreateCopy();
    old_store_metadata_.Set(object_store, std::move(backup_metadata));
  }
  return object_store;
}

void IDBTransaction::ObjectStoreCreated(const String& name,
                                        IDBObjectStore* object_store) {
  DCHECK_NE(state_, kFinished)
      << "A finished transaction created an object store";
  DCHECK_EQ(mode_, mojom::blink::IDBTransactionMode::VersionChange)
      << "A non-versionchange transaction created an object store";
  DCHECK(!object_store_map_.Contains(name))
      << "An object store was created with the name of an existing store";
  DCHECK(object_store->IsNewlyCreated())
      << "Object store IDs are not assigned sequentially";
  object_store_map_.Set(name, object_store);
}

void IDBTransaction::ObjectStoreDeleted(const int64_t object_store_id,
                                        const String& name) {
  DCHECK_NE(state_, kFinished)
      << "A finished transaction deleted an object store";
  DCHECK_EQ(mode_, mojom::blink::IDBTransactionMode::VersionChange)
      << "A non-versionchange transaction deleted an object store";
  IDBObjectStoreMap::iterator it = object_store_map_.find(name);
  if (it == object_store_map_.end()) {
    // No IDBObjectStore instance was created for the deleted store in this
    // transaction. This happens if IDBDatabase.deleteObjectStore() is called
    // with the name of a store that wasn't instantated. We need to be able to
    // revert the metadata change if the transaction aborts, in order to return
    // correct values from IDB{Database, Transaction}.objectStoreNames.
    DCHECK(database_->Metadata().object_stores.Contains(object_store_id));
    scoped_refptr<IDBObjectStoreMetadata> metadata =
        database_->Metadata().object_stores.at(object_store_id);
    DCHECK(metadata.get());
    DCHECK_EQ(metadata->name, name);
    deleted_object_stores_.push_back(std::move(metadata));
  } else {
    IDBObjectStore* object_store = it->value;
    object_store_map_.erase(name);
    object_store->MarkDeleted();
    if (object_store->Id() > old_database_metadata_.max_object_store_id) {
      // The store was created and deleted in this transaction, so it will
      // not be restored even if the transaction aborts. We have just
      // removed our last reference to it.
      DCHECK(!old_store_metadata_.Contains(object_store));
      object_store->ClearIndexCache();
    } else {
      // The store was created before this transaction, and we created an
      // IDBObjectStore instance for it. When that happened, we must have
      // snapshotted the store's metadata as well.
      DCHECK(old_store_metadata_.Contains(object_store));
    }
  }
}

void IDBTransaction::ObjectStoreRenamed(const String& old_name,
                                        const String& new_name) {
  DCHECK_NE(state_, kFinished)
      << "A finished transaction renamed an object store";
  DCHECK_EQ(mode_, mojom::blink::IDBTransactionMode::VersionChange)
      << "A non-versionchange transaction renamed an object store";

  DCHECK(!object_store_map_.Contains(new_name));
  DCHECK(object_store_map_.Contains(old_name))
      << "The object store had to be accessed in order to be renamed.";
  object_store_map_.Set(new_name, object_store_map_.Take(old_name));
}

void IDBTransaction::IndexDeleted(IDBIndex* index) {
  DCHECK(index);
  DCHECK(!index->IsDeleted()) << "IndexDeleted called twice for the same index";

  IDBObjectStore* object_store = index->objectStore();
  DCHECK_EQ(object_store->transaction(), this);
  DCHECK(object_store_map_.Contains(object_store->name()))
      << "An index was deleted without accessing its object store";

  const auto& object_store_iterator = old_store_metadata_.find(object_store);
  if (object_store_iterator == old_store_metadata_.end()) {
    // The index's object store was created in this transaction, so this
    // index was also created (and deleted) in this transaction, and will
    // not be restored if the transaction aborts.
    //
    // Subtle proof for the first sentence above: Deleting an index requires
    // calling deleteIndex() on the store's IDBObjectStore instance.
    // Whenever we create an IDBObjectStore instance for a previously
    // created store, we snapshot the store's metadata. So, deleting an
    // index of an "old" store can only be done after the store's metadata
    // is snapshotted.
    return;
  }

  const IDBObjectStoreMetadata* old_store_metadata =
      object_store_iterator->value.get();
  DCHECK(old_store_metadata);
  if (!old_store_metadata->indexes.Contains(index->Id())) {
    // The index's object store was created before this transaction, but the
    // index was created (and deleted) in this transaction, so it will not
    // be restored if the transaction aborts.
    return;
  }

  deleted_indexes_.push_back(index);
}

void IDBTransaction::SetActive(bool new_is_active) {
  DCHECK_NE(state_, kFinished)
      << "A finished transaction tried to SetActive(" << new_is_active << ")";
  if (IsFinishing())
    return;

  DCHECK_NE(new_is_active, (state_ == kActive));
  state_ = new_is_active ? kActive : kInactive;

  if (!new_is_active && request_list_.empty()) {
    remote_->Commit(num_errors_handled_);
  }
}

void IDBTransaction::SetActiveDuringSerialization(bool new_is_active) {
  if (new_is_active) {
    DCHECK_EQ(state_, kInactive)
        << "Incorrect state restore during Structured Serialization";
    state_ = kActive;
  } else {
    DCHECK_EQ(state_, kActive)
        << "Structured serialization attempted while transaction is inactive";
    state_ = kInactive;
  }
}

void IDBTransaction::abort(ExceptionState& exception_state) {
  if (IsFinishing() || IsFinished()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        IDBDatabase::kTransactionFinishedErrorMessage);
    return;
  }
  StartAborting(nullptr);
}

void IDBTransaction::commit(ExceptionState& exception_state) {
  if (IsFinishing() || IsFinished()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        IDBDatabase::kTransactionFinishedErrorMessage);
    return;
  }

  if (state_ == kInactive) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        IDBDatabase::kTransactionInactiveErrorMessage);
    return;
  }

  if (!GetExecutionContext())
    return;

  state_ = kCommitting;
  remote_->Commit(num_errors_handled_);
}

void IDBTransaction::RegisterRequest(IDBRequest* request) {
  DCHECK(request);
  DCHECK(!request_list_.Contains(request));
  DCHECK_EQ(state_, kActive);
  request_list_.insert(request);
}

void IDBTransaction::UnregisterRequest(IDBRequest* request) {
  DCHECK(request);
#if DCHECK_IS_ON()
  // Make sure that no pending IDBRequest gets left behind in the result queue.
  DCHECK(!request->QueueItem() || request->QueueItem()->IsReady());
#endif

  // If we aborted the request, it will already have been removed.
  request_list_.erase(request);
}

void IDBTransaction::EnqueueResult(
    std::unique_ptr<IDBRequestQueueItem> result) {
  result_queue_.push_back(std::move(result));
  // StartLoading() may complete post-processing synchronously, so the result
  // needs to be in the queue before StartLoading() is called.
  result_queue_.back()->StartLoading();
}

void IDBTransaction::OnResultReady() {
  // Re-entrancy can occur when sending a result causes the transaction to
  // abort, which cancels loading on other pending results.
  if (handling_ready_) {
    return;
  }
  base::AutoReset reset(&handling_ready_, true);

  while (!result_queue_.empty() && result_queue_.front()->IsReady()) {
    result_queue_.TakeFirst()->SendResult();
  }
}

void IDBTransaction::OnAbort(DOMException* error) {
  TRACE_EVENT1("IndexedDB", "IDBTransaction::onAbort", "txn.id", id_);
  if (!GetExecutionContext()) {
    Finished();
    return;
  }

  DCHECK_NE(state_, kFinished);
  if (state_ != kAborting) {
    // Abort was not triggered by front-end.
    StartAborting(error, /*from_frontend=*/false);
  }

  if (IsVersionChange())
    database_->close();

  // Step 6 of https://w3c.github.io/IndexedDB/#abort-a-transaction
  // requires that these steps are asynchronous:
  //
  //   Queue a task to run these steps:
  //     1. If transaction is an upgrade transaction, then set transaction’s
  //     connection's associated database's upgrade transaction to null.
  //     2. [...]
  //
  // However, `OnAbort` is a result of a round trip through the browser, so it
  // was already queued and we don't have to re-enqueue.

  // First set the database/connection's upgrade transaction to null.
  database_->TransactionWillFinish(this);
  // Then fire the abort event. (This will also set the request's transaction to
  // null after dispatching.)
  DispatchEvent(*Event::CreateBubble(event_type_names::kAbort));
  // Now do final cleanup.
  Finished();
}

void IDBTransaction::OnComplete() {
  TRACE_EVENT1("IndexedDB", "IDBTransaction::onComplete", "txn.id", id_);
  if (!GetExecutionContext()) {
    Finished();
    return;
  }

  DCHECK_NE(state_, kFinished);
  state_ = kCommitting;

  // See comments in `OnAbort()` on importance of ordering.
  database_->TransactionWillFinish(this);
  DispatchEvent(*Event::Create(event_type_names::kComplete));
  Finished();
}

void IDBTransaction::StartAborting(DOMException* error, bool from_frontend) {
  // Backend aborts must always come with an error.
  DCHECK(error || from_frontend);

  if (error) {
    SetError(error);
  }
  if (IsFinished() || IsFinishing()) {
    return;
  }

  state_ = kAborting;

  if (!GetExecutionContext()) {
    return;
  }

  // As per the spec, the first step in aborting a transaction is to mark object
  // stores and indexes as deleted. The (two-step) process of aborting
  // outstanding requests is later (the 5th step).
  // https://w3c.github.io/IndexedDB/#abort-a-transaction
  RevertDatabaseMetadata();
  // Step 5 of the algorithm requires this step to be queued rather than
  // executed synchronously, but if the abort was initiated by the backend (e.g.
  // due to a constraint error), we're already asynchronous.
  AbortOutstandingRequests(/*queue_tasks=*/from_frontend);

  if (from_frontend && database_->IsConnectionOpen()) {
    database_->Abort(id_);
  }
}

void IDBTransaction::CreateObjectStore(int64_t object_store_id,
                                       const String& name,
                                       const IDBKeyPath& key_path,
                                       bool auto_increment) {
  if (remote_.is_connected()) {
    remote_->CreateObjectStore(object_store_id, name, key_path, auto_increment);
  }
}

void IDBTransaction::DeleteObjectStore(int64_t object_store_id) {
  if (remote_.is_connected()) {
    remote_->DeleteObjectStore(object_store_id);
  }
}

void IDBTransaction::Put(int64_t object_store_id,
                         std::unique_ptr<IDBValue> value,
                         std::unique_ptr<IDBKey> primary_key,
                         mojom::blink::IDBPutMode put_mode,
                         Vector<IDBIndexKeys> index_keys,
                         mojom::blink::IDBTransaction::PutCallback callback) {
  if (!remote_.is_connected()) {
    std::move(callback).Run(
        mojom::blink::IDBTransactionPutResult::NewErrorResult(
            mojom::blink::IDBError::New(
                mojom::blink::IDBException::kUnknownError,
                "Unknown transaction")));
    return;
  }

  IDBCursor::ResetCursorPrefetchCaches(id_, nullptr);

  size_t index_keys_size = 0;
  for (const auto& index_key : index_keys) {
    index_keys_size++;  // Account for index_key.first (int64_t).
    for (const auto& key : index_key.keys) {
      // Because all size estimates are based on RAM usage, it is impossible to
      // overflow index_keys_size.
      index_keys_size += key->SizeEstimate();
    }
  }

  size_t arg_size =
      value->DataSize() + primary_key->SizeEstimate() + index_keys_size;

  const size_t max_put_value_size = max_put_value_size_override_.value_or(
      mojom::blink::kIDBMaxMessageSize - mojom::blink::kIDBMaxMessageOverhead);
  if (arg_size >= max_put_value_size) {
    std::move(callback).Run(
        mojom::blink::IDBTransactionPutResult::NewErrorResult(
            mojom::blink::IDBError::New(
                mojom::blink::IDBException::kUnknownError,
                String::Format("The serialized keys and/or value are too large"
                               " (size=%" PRIuS " bytes, max=%" PRIuS
                               " bytes).",
                               arg_size, max_put_value_size))));
    return;
  }

  remote_->Put(object_store_id, std::move(value), std::move(primary_key),
               put_mode, std::move(index_keys), std::move(callback));
}

void IDBTransaction::FlushForTesting() {
  remote_.FlushForTesting();
}

bool IDBTransaction::HasPendingActivity() const {
  // FIXME: In an ideal world, we should return true as long as anyone has a or
  // can get a handle to us or any child request object and any of those have
  // event listeners. This is  in order to handle user generated events
  // properly.
  return has_pending_activity_ && GetExecutionContext();
}

mojom::blink::IDBTransactionMode IDBTransaction::EnumToMode(
    V8IDBTransactionMode::Enum mode) {
  switch (mode) {
    case V8IDBTransactionMode::Enum::kReadonly:
      return mojom::blink::IDBTransactionMode::ReadOnly;
    case V8IDBTransactionMode::Enum::kReadwrite:
      return mojom::blink::IDBTransactionMode::ReadWrite;
    case V8IDBTransactionMode::Enum::kVersionchange:
      return mojom::blink::IDBTransactionMode::VersionChange;
  }
}

V8IDBTransactionMode IDBTransaction::mode() const {
  switch (mode_) {
    case mojom::blink::IDBTransactionMode::ReadOnly:
      return V8IDBTransactionMode(V8IDBTransactionMode::Enum::kReadonly);

    case mojom::blink::IDBTransactionMode::ReadWrite:
      return V8IDBTransactionMode(V8IDBTransactionMode::Enum::kReadwrite);

    case mojom::blink::IDBTransactionMode::VersionChange:
      return V8IDBTransactionMode(V8IDBTransactionMode::Enum::kVersionchange);
  }
}

const String& IDBTransaction::durability() const {
  switch (durability_) {
    case mojom::blink::IDBTransactionDurability::Default:
      return indexed_db_names::kDefault;

    case mojom::blink::IDBTransactionDurability::Strict:
      return indexed_db_names::kStrict;

    case mojom::blink::IDBTransactionDurability::Relaxed:
      return indexed_db_names::kRelaxed;
  }

  NOTREACHED_IN_MIGRATION();
}

DOMStringList* IDBTransaction::objectStoreNames() const {
  if (IsVersionChange())
    return database_->objectStoreNames();

  auto* object_store_names = MakeGarbageCollected<DOMStringList>();
  for (const String& object_store_name : scope_)
    object_store_names->Append(object_store_name);
  object_store_names->Sort();
  return object_store_names;
}

const AtomicString& IDBTransaction::InterfaceName() const {
  return event_target_names::kIDBTransaction;
}

ExecutionContext* IDBTransaction::GetExecutionContext() const {
  return ExecutionContextLifecycleObserver::GetExecutionContext();
}

const char* IDBTransaction::InactiveErrorMessage() const {
  switch (state_) {
    case kActive:
      // Callers should check !IsActive() before calling.
      NOTREACHED_IN_MIGRATION();
      return nullptr;
    case kInactive:
      return IDBDatabase::kTransactionInactiveErrorMessage;
    case kCommitting:
    case kAborting:
    case kFinished:
      return IDBDatabase::kTransactionFinishedErrorMessage;
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

DispatchEventResult IDBTransaction::DispatchEventInternal(Event& event) {
  TRACE_EVENT1("IndexedDB", "IDBTransaction::dispatchEvent", "txn.id", id_);

  event.SetTarget(this);

  // Per spec: "A transaction's get the parent algorithm returns the
  // transaction’s connection."
  HeapVector<Member<EventTarget>> targets;
  targets.push_back(this);
  targets.push_back(db());

  // If this event originated from script, it should have no side effects.
  if (!event.isTrusted())
    return IDBEventDispatcher::Dispatch(event, targets);
  DCHECK(event.type() == event_type_names::kComplete ||
         event.type() == event_type_names::kAbort);

  if (!GetExecutionContext()) {
    state_ = kFinished;
    return DispatchEventResult::kCanceledBeforeDispatch;
  }
  DCHECK_NE(state_, kFinished);
  DCHECK(has_pending_activity_);
  DCHECK(GetExecutionContext());
  DCHECK_EQ(event.target(), this);
  state_ = kFinished;

  DispatchEventResult dispatch_result =
      IDBEventDispatcher::Dispatch(event, targets);
  // FIXME: Try to construct a test where |this| outlives openDBRequest and we
  // get a crash.
  if (open_db_request_) {
    DCHECK(IsVersionChange());
    open_db_request_->TransactionDidFinishAndDispatch();
  }
  has_pending_activity_ = false;
  return dispatch_result;
}

void IDBTransaction::AbortOutstandingRequests(bool queue_tasks) {
  decltype(request_list_) request_list;
  request_list.Swap(request_list_);
  for (IDBRequest* request : request_list) {
    request->Abort(queue_tasks);
  }
}

void IDBTransaction::RevertDatabaseMetadata() {
  DCHECK_NE(state_, kActive);
  if (!IsVersionChange())
    return;

  // Mark stores created by this transaction as deleted.
  for (auto& object_store : object_store_map_.Values()) {
    const int64_t object_store_id = object_store->Id();
    if (!object_store->IsNewlyCreated()) {
      DCHECK(old_store_metadata_.Contains(object_store));
      continue;
    }

    DCHECK(!old_store_metadata_.Contains(object_store));
    database_->RevertObjectStoreCreation(object_store_id);
    object_store->MarkDeleted();
  }

  for (auto& it : old_store_metadata_) {
    IDBObjectStore* object_store = it.key;
    scoped_refptr<IDBObjectStoreMetadata> old_metadata = it.value;

    database_->RevertObjectStoreMetadata(old_metadata);
    object_store->RevertMetadata(old_metadata);
  }
  for (auto& index : deleted_indexes_)
    index->objectStore()->RevertDeletedIndexMetadata(*index);
  for (auto& old_medata : deleted_object_stores_)
    database_->RevertObjectStoreMetadata(std::move(old_medata));

  // We only need to revert the database's own metadata because we have reverted
  // the metadata for the database's object stores above.
  database_->SetDatabaseMetadata(old_database_metadata_);
}

void IDBTransaction::Finished() {
#if DCHECK_IS_ON()
  DCHECK(!finish_called_);
  finish_called_ = true;
#endif  // DCHECK_IS_ON()

  database_->TransactionFinished(this);

  // Remove references to the IDBObjectStore and IDBIndex instances held by
  // this transaction, so Oilpan can garbage-collect the instances that aren't
  // used by JavaScript.

  for (auto& it : object_store_map_) {
    IDBObjectStore* object_store = it.value;
    if (!IsVersionChange() || object_store->IsNewlyCreated()) {
      DCHECK(!old_store_metadata_.Contains(object_store));
      object_store->ClearIndexCache();
    } else {
      // We'll call ClearIndexCache() on this store in the loop below.
      DCHECK(old_store_metadata_.Contains(object_store));
    }
  }
  object_store_map_.clear();

  for (auto& it : old_store_metadata_) {
    IDBObjectStore* object_store = it.key;
    object_store->ClearIndexCache();
  }
  old_store_metadata_.clear();

  deleted_indexes_.clear();
  deleted_object_stores_.clear();
}

}  // namespace blink
