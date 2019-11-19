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

#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event_queue.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/indexed_db_names.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_database.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_event_dispatcher.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_index.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_object_store.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_open_db_request.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_request_queue_item.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_tracing.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

IDBTransaction* IDBTransaction::CreateNonVersionChange(
    ScriptState* script_state,
    std::unique_ptr<WebIDBTransaction> transaction_backend,
    int64_t id,
    const HashSet<String>& scope,
    mojom::IDBTransactionMode mode,
    mojom::IDBTransactionDurability durability,
    IDBDatabase* db) {
  DCHECK_NE(mode, mojom::IDBTransactionMode::VersionChange);
  DCHECK(!scope.IsEmpty()) << "Non-version transactions should operate on a "
                              "well-defined set of stores";
  return MakeGarbageCollected<IDBTransaction>(script_state,
                                              std::move(transaction_backend),
                                              id, scope, mode, durability, db);
}

IDBTransaction* IDBTransaction::CreateVersionChange(
    ExecutionContext* execution_context,
    std::unique_ptr<WebIDBTransaction> transaction_backend,
    int64_t id,
    IDBDatabase* db,
    IDBOpenDBRequest* open_db_request,
    const IDBDatabaseMetadata& old_metadata) {
  return MakeGarbageCollected<IDBTransaction>(
      execution_context, std::move(transaction_backend), id, db,
      open_db_request, old_metadata);
}

IDBTransaction::IDBTransaction(
    ScriptState* script_state,
    std::unique_ptr<WebIDBTransaction> transaction_backend,
    int64_t id,
    const HashSet<String>& scope,
    mojom::IDBTransactionMode mode,
    mojom::IDBTransactionDurability durability,
    IDBDatabase* db)
    : ContextLifecycleObserver(ExecutionContext::From(script_state)),
      transaction_backend_(std::move(transaction_backend)),
      id_(id),
      database_(db),
      mode_(mode),
      durability_(durability),
      scope_(scope),
      event_queue_(
          MakeGarbageCollected<EventQueue>(ExecutionContext::From(script_state),
                                           TaskType::kDatabaseAccess)),
      feature_handle_for_scheduler_(
          ExecutionContext::From(script_state)
              ->GetScheduler()
              ->RegisterFeature(
                  SchedulingPolicy::Feature::kOutstandingIndexedDBTransaction,
                  {SchedulingPolicy::RecordMetricsForBackForwardCache()})) {
  DCHECK(database_);
  DCHECK(!scope_.IsEmpty()) << "Non-versionchange transactions must operate "
                               "on a well-defined set of stores";
  DCHECK(mode_ == mojom::IDBTransactionMode::ReadOnly ||
         mode_ == mojom::IDBTransactionMode::ReadWrite)
      << "Invalid transaction mode";

  DCHECK_EQ(state_, kActive);
  V8PerIsolateData::From(script_state->GetIsolate())
      ->AddEndOfScopeTask(
          WTF::Bind(&IDBTransaction::SetActive, WrapPersistent(this), false));

  database_->TransactionCreated(this);
}

IDBTransaction::IDBTransaction(
    ExecutionContext* execution_context,
    std::unique_ptr<WebIDBTransaction> transaction_backend,
    int64_t id,
    IDBDatabase* db,
    IDBOpenDBRequest* open_db_request,
    const IDBDatabaseMetadata& old_metadata)
    : ContextLifecycleObserver(execution_context),
      transaction_backend_(std::move(transaction_backend)),
      id_(id),
      database_(db),
      open_db_request_(open_db_request),
      mode_(mojom::IDBTransactionMode::VersionChange),
      durability_(mojom::IDBTransactionDurability::Default),
      state_(kInactive),
      old_database_metadata_(old_metadata),
      event_queue_(
          MakeGarbageCollected<EventQueue>(execution_context,
                                           TaskType::kDatabaseAccess)) {
  DCHECK(database_);
  DCHECK(open_db_request_);
  DCHECK(scope_.IsEmpty());

  database_->TransactionCreated(this);
}

IDBTransaction::~IDBTransaction() {
  // Note: IDBTransaction is a ContextLifecycleObserver (rather than
  // ContextClient) only in order to be able call upon GetExecutionContext()
  // during this destructor.
  DCHECK(state_ == kFinished || !GetExecutionContext());
  DCHECK(request_list_.IsEmpty() || !GetExecutionContext());
}

void IDBTransaction::Trace(blink::Visitor* visitor) {
  visitor->Trace(database_);
  visitor->Trace(open_db_request_);
  visitor->Trace(error_);
  visitor->Trace(request_list_);
  visitor->Trace(object_store_map_);
  visitor->Trace(old_store_metadata_);
  visitor->Trace(deleted_indexes_);
  visitor->Trace(event_queue_);
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
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
    return it->value;

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
  DCHECK_EQ(mode_, mojom::IDBTransactionMode::VersionChange)
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
  DCHECK_EQ(mode_, mojom::IDBTransactionMode::VersionChange)
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
  DCHECK_EQ(mode_, mojom::IDBTransactionMode::VersionChange)
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
  if (state_ == kFinishing)
    return;
  DCHECK_NE(new_is_active, (state_ == kActive));
  state_ = new_is_active ? kActive : kInactive;

  if (!new_is_active && request_list_.IsEmpty() && transaction_backend())
    transaction_backend()->Commit(num_errors_handled_);
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
  if (state_ == kFinishing || state_ == kFinished) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        IDBDatabase::kTransactionFinishedErrorMessage);
    return;
  }

  state_ = kFinishing;

  if (!GetExecutionContext())
    return;

  AbortOutstandingRequests();
  RevertDatabaseMetadata();

  if (BackendDB())
    BackendDB()->Abort(id_);
}

void IDBTransaction::commit(ExceptionState& exception_state) {
  if (state_ == kFinishing || state_ == kFinished) {
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

  state_ = kFinishing;

  if (transaction_backend())
    transaction_backend()->Commit(num_errors_handled_);
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
#endif  // DCHECK_IS_ON()

  // If we aborted the request, it will already have been removed.
  request_list_.erase(request);
}

void IDBTransaction::EnqueueResult(
    std::unique_ptr<IDBRequestQueueItem> result) {
  DCHECK(result);
  DCHECK(HasQueuedResults() || !result->IsReady());

  result_queue_.push_back(std::move(result));
  // StartLoading() may complete post-processing synchronously, so the result
  // needs to be in the queue before StartLoading() is called.
  result_queue_.back()->StartLoading();
}

void IDBTransaction::OnResultReady() {
  while (!result_queue_.empty()) {
    IDBRequestQueueItem* result = result_queue_.front().get();
    if (!result->IsReady())
      break;

    result->EnqueueResponse();
    result_queue_.pop_front();
  }
}

void IDBTransaction::OnAbort(DOMException* error) {
  IDB_TRACE1("IDBTransaction::onAbort", "txn.id", id_);
  if (!GetExecutionContext()) {
    Finished();
    return;
  }

  DCHECK_NE(state_, kFinished);
  if (state_ != kFinishing) {
    // Abort was not triggered by front-end.
    DCHECK(error);
    SetError(error);

    AbortOutstandingRequests();
    RevertDatabaseMetadata();

    state_ = kFinishing;
  }

  if (IsVersionChange())
    database_->close();

  // Enqueue events before notifying database, as database may close which
  // enqueues more events and order matters.
  EnqueueEvent(Event::CreateBubble(event_type_names::kAbort));
  Finished();
}

void IDBTransaction::OnComplete() {
  IDB_TRACE1("IDBTransaction::onComplete", "txn.id", id_);
  if (!GetExecutionContext()) {
    Finished();
    return;
  }

  DCHECK_NE(state_, kFinished);
  state_ = kFinishing;

  // Enqueue events before notifying database, as database may close which
  // enqueues more events and order matters.
  EnqueueEvent(Event::Create(event_type_names::kComplete));
  Finished();
}

bool IDBTransaction::HasPendingActivity() const {
  // FIXME: In an ideal world, we should return true as long as anyone has a or
  // can get a handle to us or any child request object and any of those have
  // event listeners. This is  in order to handle user generated events
  // properly.
  return has_pending_activity_ && GetExecutionContext();
}

mojom::IDBTransactionMode IDBTransaction::StringToMode(
    const String& mode_string) {
  if (mode_string == indexed_db_names::kReadonly)
    return mojom::IDBTransactionMode::ReadOnly;
  if (mode_string == indexed_db_names::kReadwrite)
    return mojom::IDBTransactionMode::ReadWrite;
  if (mode_string == indexed_db_names::kVersionchange)
    return mojom::IDBTransactionMode::VersionChange;
  NOTREACHED();
  return mojom::IDBTransactionMode::ReadOnly;
}

WebIDBDatabase* IDBTransaction::BackendDB() const {
  return database_->Backend();
}

const String& IDBTransaction::mode() const {
  switch (mode_) {
    case mojom::IDBTransactionMode::ReadOnly:
      return indexed_db_names::kReadonly;

    case mojom::IDBTransactionMode::ReadWrite:
      return indexed_db_names::kReadwrite;

    case mojom::IDBTransactionMode::VersionChange:
      return indexed_db_names::kVersionchange;
  }

  NOTREACHED();
  return indexed_db_names::kReadonly;
}

const String& IDBTransaction::durability() const {
  switch (durability_) {
    case mojom::IDBTransactionDurability::Default:
      return indexed_db_names::kDefault;

    case mojom::IDBTransactionDurability::Strict:
      return indexed_db_names::kStrict;

    case mojom::IDBTransactionDurability::Relaxed:
      return indexed_db_names::kRelaxed;
  }

  NOTREACHED();
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
  return ContextLifecycleObserver::GetExecutionContext();
}

const char* IDBTransaction::InactiveErrorMessage() const {
  switch (state_) {
    case kActive:
      // Callers should check !IsActive() before calling.
      NOTREACHED();
      return nullptr;
    case kInactive:
      return IDBDatabase::kTransactionInactiveErrorMessage;
    case kFinishing:
    case kFinished:
      return IDBDatabase::kTransactionFinishedErrorMessage;
  }
  NOTREACHED();
  return nullptr;
}

DispatchEventResult IDBTransaction::DispatchEventInternal(Event& event) {
  IDB_TRACE1("IDBTransaction::dispatchEvent", "txn.id", id_);
  if (!GetExecutionContext()) {
    state_ = kFinished;
    return DispatchEventResult::kCanceledBeforeDispatch;
  }
  DCHECK_NE(state_, kFinished);
  DCHECK(has_pending_activity_);
  DCHECK(GetExecutionContext());
  DCHECK_EQ(event.target(), this);
  state_ = kFinished;

  HeapVector<Member<EventTarget>> targets;
  targets.push_back(this);
  targets.push_back(db());

  // FIXME: When we allow custom event dispatching, this will probably need to
  // change.
  DCHECK(event.type() == event_type_names::kComplete ||
         event.type() == event_type_names::kAbort);
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

void IDBTransaction::EnqueueEvent(Event* event) {
  DCHECK_NE(state_, kFinished)
      << "A finished transaction tried to enqueue an event of type "
      << event->type() << ".";
  if (!GetExecutionContext())
    return;

  event->SetTarget(this);
  event_queue_->EnqueueEvent(FROM_HERE, *event);
}

void IDBTransaction::AbortOutstandingRequests() {
  for (IDBRequest* request : request_list_)
    request->Abort();
  request_list_.clear();
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

  feature_handle_for_scheduler_.reset();
}

}  // namespace blink
