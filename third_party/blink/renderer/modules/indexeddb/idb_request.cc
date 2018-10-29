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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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

#include "third_party/blink/renderer/modules/indexeddb/idb_request.h"

#include <atomic>
#include <memory>
#include <utility>

#include "third_party/blink/public/platform/web_blob_info.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/bindings/modules/v8/idb_object_store_or_idb_index_or_idb_cursor.h"
#include "third_party/blink/renderer/bindings/modules/v8/to_v8_for_modules.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_binding_for_modules.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event_queue.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/indexed_db_names.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_cursor_with_value.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_database.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_event_dispatcher.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_request_queue_item.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_tracing.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value_wrapping.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_callbacks_impl.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

using blink::WebIDBCursor;

namespace blink {

IDBRequest::AsyncTraceState::AsyncTraceState(const char* trace_event_name)
    : trace_event_name_(nullptr) {
  // If PopulateForNewEvent is called, it sets trace_event_name_ to
  // trace_event_name. Otherwise, trace_event_name_ is nullptr, so this instance
  // is considered empty. This roundabout initialization lets us avoid calling
  // TRACE_EVENT_ASYNC_END0 with an uninitalized ID.
  TRACE_EVENT_ASYNC_BEGIN0("IndexedDB", trace_event_name,
                           PopulateForNewEvent(trace_event_name));
}

void IDBRequest::AsyncTraceState::RecordAndReset() {
  if (trace_event_name_) {
    TRACE_EVENT_ASYNC_END0("IndexedDB", trace_event_name_, id_);
    trace_event_name_ = nullptr;
  }
}

IDBRequest::AsyncTraceState::~AsyncTraceState() {
  if (trace_event_name_)
    TRACE_EVENT_ASYNC_END0("IndexedDB", trace_event_name_, id_);
}

size_t IDBRequest::AsyncTraceState::PopulateForNewEvent(
    const char* trace_event_name) {
  DCHECK(trace_event_name);
  DCHECK(!trace_event_name_);
  trace_event_name_ = trace_event_name;

  static std::atomic<size_t> counter(0);
  id_ = counter.fetch_add(1, std::memory_order_relaxed);
  return id_;
}

IDBRequest* IDBRequest::Create(ScriptState* script_state,
                               IDBIndex* source,
                               IDBTransaction* transaction,
                               IDBRequest::AsyncTraceState metrics) {
  return IDBRequest::Create(script_state, Source::FromIDBIndex(source),
                            transaction, std::move(metrics));
}

IDBRequest* IDBRequest::Create(ScriptState* script_state,
                               IDBObjectStore* source,
                               IDBTransaction* transaction,
                               IDBRequest::AsyncTraceState metrics) {
  return IDBRequest::Create(script_state, Source::FromIDBObjectStore(source),
                            transaction, std::move(metrics));
}

IDBRequest* IDBRequest::Create(ScriptState* script_state,
                               IDBCursor* source,
                               IDBTransaction* transaction,
                               IDBRequest::AsyncTraceState metrics) {
  return IDBRequest::Create(script_state, Source::FromIDBCursor(source),
                            transaction, std::move(metrics));
}

IDBRequest* IDBRequest::Create(ScriptState* script_state,
                               const Source& source,
                               IDBTransaction* transaction,
                               IDBRequest::AsyncTraceState metrics) {
  IDBRequest* request =
      new IDBRequest(script_state, source, transaction, std::move(metrics));
  request->PauseIfNeeded();
  // Requests associated with IDBFactory (open/deleteDatabase/getDatabaseNames)
  // do not have an associated transaction.
  if (transaction)
    transaction->RegisterRequest(request);
  return request;
}

IDBRequest::IDBRequest(ScriptState* script_state,
                       const Source& source,
                       IDBTransaction* transaction,
                       AsyncTraceState metrics)
    : PausableObject(ExecutionContext::From(script_state)),
      transaction_(transaction),
      isolate_(script_state->GetIsolate()),
      metrics_(std::move(metrics)),
      source_(source),
      event_queue_(EventQueue::Create(ExecutionContext::From(script_state),
                                      TaskType::kInternalIndexedDB)) {}

IDBRequest::~IDBRequest() {
  DCHECK((ready_state_ == DONE && metrics_.IsEmpty()) ||
         ready_state_ == kEarlyDeath || !GetExecutionContext());
}

void IDBRequest::Trace(blink::Visitor* visitor) {
  visitor->Trace(transaction_);
  visitor->Trace(source_);
  visitor->Trace(result_);
  visitor->Trace(error_);
  visitor->Trace(event_queue_);
  visitor->Trace(pending_cursor_);
  EventTargetWithInlineData::Trace(visitor);
  PausableObject::Trace(visitor);
}

ScriptValue IDBRequest::result(ScriptState* script_state,
                               ExceptionState& exception_state) {
  if (ready_state_ != DONE) {
    // Must throw if returning an empty value. Message is arbitrary since it
    // will never be seen.
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        IDBDatabase::kRequestNotFinishedErrorMessage);
    return ScriptValue();
  }
  if (!GetExecutionContext()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kDatabaseClosedErrorMessage);
    return ScriptValue();
  }
  result_dirty_ = false;
  ScriptValue value = ScriptValue::From(script_state, result_);
  return value;
}

DOMException* IDBRequest::error(ExceptionState& exception_state) const {
  if (ready_state_ != DONE) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        IDBDatabase::kRequestNotFinishedErrorMessage);
    return nullptr;
  }
  return error_;
}

void IDBRequest::source(ScriptState* script_state,
                        IDBObjectStoreOrIDBIndexOrIDBCursor& source) const {
  if (!GetExecutionContext()) {
    source = Source();
  }
  source = source_;
}

const String& IDBRequest::readyState() const {
  DCHECK(ready_state_ == PENDING || ready_state_ == DONE);

  if (ready_state_ == PENDING)
    return IndexedDBNames::pending;

  return IndexedDBNames::done;
}

std::unique_ptr<WebIDBCallbacks> IDBRequest::CreateWebCallbacks() {
  DCHECK(!web_callbacks_);
  std::unique_ptr<WebIDBCallbacks> callbacks =
      WebIDBCallbacksImpl::Create(this);
  web_callbacks_ = callbacks.get();
  return callbacks;
}

void IDBRequest::Abort() {
  DCHECK(!request_aborted_);
  if (queue_item_) {
    queue_item_->CancelLoading();

    // A transaction's requests are aborted in order, so each aborted request
    // should immediately get out of the result queue.
    DCHECK(!queue_item_);
  }

  if (!GetExecutionContext())
    return;
  DCHECK(ready_state_ == PENDING || ready_state_ == DONE);
  if (ready_state_ == DONE)
    return;

  event_queue_->CancelAllEvents();

  error_.Clear();
  result_.Clear();
  EnqueueResponse(DOMException::Create(
      DOMExceptionCode::kAbortError,
      "The transaction was aborted, so the request cannot be fulfilled."));
  request_aborted_ = true;
}

void IDBRequest::SetCursorDetails(IndexedDB::CursorType cursor_type,
                                  WebIDBCursorDirection direction) {
  DCHECK_EQ(ready_state_, PENDING);
  DCHECK(!pending_cursor_);
  cursor_type_ = cursor_type;
  cursor_direction_ = direction;
}

void IDBRequest::SetPendingCursor(IDBCursor* cursor) {
  DCHECK_EQ(ready_state_, DONE);
  DCHECK(GetExecutionContext());
  DCHECK(transaction_);
  DCHECK(!pending_cursor_);
  DCHECK_EQ(cursor, GetResultCursor());

  has_pending_activity_ = true;
  pending_cursor_ = cursor;
  SetResult(nullptr);
  ready_state_ = PENDING;
  error_.Clear();
  transaction_->RegisterRequest(this);
}

IDBCursor* IDBRequest::GetResultCursor() const {
  if (!result_)
    return nullptr;
  if (result_->GetType() == IDBAny::kIDBCursorType)
    return result_->IdbCursor();
  if (result_->GetType() == IDBAny::kIDBCursorWithValueType)
    return result_->IdbCursorWithValue();
  return nullptr;
}

void IDBRequest::SetResultCursor(IDBCursor* cursor,
                                 std::unique_ptr<IDBKey> key,
                                 std::unique_ptr<IDBKey> primary_key,
                                 std::unique_ptr<IDBValue> value) {
  DCHECK_EQ(ready_state_, PENDING);
  cursor_key_ = std::move(key);
  cursor_primary_key_ = std::move(primary_key);
  cursor_value_ = std::move(value);

  EnqueueResultInternal(IDBAny::Create(cursor));
}

bool IDBRequest::ShouldEnqueueEvent() const {
  const ExecutionContext* execution_context = GetExecutionContext();

  // https://crbug.com/733642 - Document::Shutdown() calls
  // LocalDOMWindow::ClearEventQueue(), which nulls out the context's event
  // queue, before calling ExecutionContext::NotifyContextDestroyed(). The
  // latter eventually calls IDBRequest::ContextDestroyed(), which aborts the
  // request. As an aborted IDBRequest is removed from its' IDBTransaction
  // result queue, it may unblock another request whose result is already
  // available. If the unblocked request hasn't received a
  // NotifyContextDestroyed() call yet, it will hang onto an ExecutionContext
  // whose event queue has been nulled out. The event queue null check covers
  // these specific circumstances.
  if (!execution_context)
    return false;

  DCHECK(ready_state_ == PENDING || ready_state_ == DONE);
  if (request_aborted_)
    return false;
  DCHECK_EQ(ready_state_, PENDING);
  DCHECK(!error_ && !result_);
  return true;
}

void IDBRequest::HandleResponse(DOMException* error) {
  transit_blob_handles_.clear();
  if (!transaction_ || !transaction_->HasQueuedResults())
    return EnqueueResponse(error);
  transaction_->EnqueueResult(std::make_unique<IDBRequestQueueItem>(
      this, error,
      WTF::Bind(&IDBTransaction::OnResultReady,
                WrapPersistent(transaction_.Get()))));
}

void IDBRequest::HandleResponse(std::unique_ptr<IDBKey> key) {
  transit_blob_handles_.clear();
  DCHECK(transaction_);
  if (!transaction_->HasQueuedResults())
    return EnqueueResponse(std::move(key));
  transaction_->EnqueueResult(std::make_unique<IDBRequestQueueItem>(
      this, std::move(key),
      WTF::Bind(&IDBTransaction::OnResultReady,
                WrapPersistent(transaction_.Get()))));
}

void IDBRequest::HandleResponse(int64_t value_or_old_version) {
  DCHECK(transit_blob_handles_.IsEmpty());
  if (!transaction_ || !transaction_->HasQueuedResults())
    return EnqueueResponse(value_or_old_version);
  transaction_->EnqueueResult(std::make_unique<IDBRequestQueueItem>(
      this, value_or_old_version,
      WTF::Bind(&IDBTransaction::OnResultReady,
                WrapPersistent(transaction_.Get()))));
}

void IDBRequest::HandleResponse() {
  DCHECK(transit_blob_handles_.IsEmpty());
  if (!transaction_ || !transaction_->HasQueuedResults())
    return EnqueueResponse();
  transaction_->EnqueueResult(std::make_unique<IDBRequestQueueItem>(
      this, WTF::Bind(&IDBTransaction::OnResultReady,
                      WrapPersistent(transaction_.Get()))));
}

void IDBRequest::HandleResponse(std::unique_ptr<WebIDBCursor> backend,
                                std::unique_ptr<IDBKey> key,
                                std::unique_ptr<IDBKey> primary_key,
                                std::unique_ptr<IDBValue> value) {
  DCHECK(transit_blob_handles_.IsEmpty());
  DCHECK(transaction_);
  bool is_wrapped = IDBValueUnwrapper::IsWrapped(value.get());
  if (!transaction_->HasQueuedResults() && !is_wrapped) {
    return EnqueueResponse(std::move(backend), std::move(key),
                           std::move(primary_key), std::move(value));
  }
  transaction_->EnqueueResult(std::make_unique<IDBRequestQueueItem>(
      this, std::move(backend), std::move(key), std::move(primary_key),
      std::move(value), is_wrapped,
      WTF::Bind(&IDBTransaction::OnResultReady,
                WrapPersistent(transaction_.Get()))));
}

void IDBRequest::HandleResponse(std::unique_ptr<IDBValue> value) {
  DCHECK(transit_blob_handles_.IsEmpty());
  DCHECK(transaction_);
  bool is_wrapped = IDBValueUnwrapper::IsWrapped(value.get());
  if (!transaction_->HasQueuedResults() && !is_wrapped)
    return EnqueueResponse(std::move(value));
  transaction_->EnqueueResult(std::make_unique<IDBRequestQueueItem>(
      this, std::move(value), is_wrapped,
      WTF::Bind(&IDBTransaction::OnResultReady,
                WrapPersistent(transaction_.Get()))));
}

void IDBRequest::HandleResponse(Vector<std::unique_ptr<IDBValue>> values) {
  DCHECK(transit_blob_handles_.IsEmpty());
  DCHECK(transaction_);
  bool is_wrapped = IDBValueUnwrapper::IsWrapped(values);
  if (!transaction_->HasQueuedResults() && !is_wrapped)
    return EnqueueResponse(std::move(values));
  transaction_->EnqueueResult(std::make_unique<IDBRequestQueueItem>(
      this, std::move(values), is_wrapped,
      WTF::Bind(&IDBTransaction::OnResultReady,
                WrapPersistent(transaction_.Get()))));
}

void IDBRequest::HandleResponse(std::unique_ptr<IDBKey> key,
                                std::unique_ptr<IDBKey> primary_key,
                                std::unique_ptr<IDBValue> value) {
  DCHECK(transit_blob_handles_.IsEmpty());
  DCHECK(transaction_);
  bool is_wrapped = IDBValueUnwrapper::IsWrapped(value.get());
  if (!transaction_->HasQueuedResults() && !is_wrapped) {
    return EnqueueResponse(std::move(key), std::move(primary_key),
                           std::move(value));
  }

  transaction_->EnqueueResult(std::make_unique<IDBRequestQueueItem>(
      this, std::move(key), std::move(primary_key), std::move(value),
      is_wrapped,
      WTF::Bind(&IDBTransaction::OnResultReady,
                WrapPersistent(transaction_.Get()))));
}

void IDBRequest::EnqueueResponse(DOMException* error) {
  IDB_TRACE("IDBRequest::EnqueueResponse(DOMException)");
  if (!ShouldEnqueueEvent()) {
    metrics_.RecordAndReset();
    return;
  }

  error_ = error;
  SetResult(IDBAny::CreateUndefined());
  pending_cursor_.Clear();
  EnqueueEvent(Event::CreateCancelableBubble(EventTypeNames::error));
  metrics_.RecordAndReset();
}

void IDBRequest::EnqueueResponse(const Vector<String>& string_list) {
  IDB_TRACE("IDBRequest::onSuccess(StringList)");
  if (!ShouldEnqueueEvent()) {
    metrics_.RecordAndReset();
    return;
  }

  DOMStringList* dom_string_list = DOMStringList::Create();
  for (const auto& item : string_list)
    dom_string_list->Append(item);
  EnqueueResultInternal(IDBAny::Create(dom_string_list));
  metrics_.RecordAndReset();
}

void IDBRequest::EnqueueResponse(std::unique_ptr<WebIDBCursor> backend,
                                 std::unique_ptr<IDBKey> key,
                                 std::unique_ptr<IDBKey> primary_key,
                                 std::unique_ptr<IDBValue> value) {
  IDB_TRACE1("IDBRequest::EnqueueResponse(IDBCursor)", "size",
             value ? value->DataSize() : 0);
  if (!ShouldEnqueueEvent()) {
    metrics_.RecordAndReset();
    return;
  }

  DCHECK(!pending_cursor_);
  IDBCursor* cursor = nullptr;
  IDBObjectStoreOrIDBIndex source;

  if (source_.IsIDBObjectStore()) {
    source =
        IDBCursor::Source::FromIDBObjectStore(source_.GetAsIDBObjectStore());
  } else if (source_.IsIDBIndex()) {
    source = IDBCursor::Source::FromIDBIndex(source_.GetAsIDBIndex());
  }
  DCHECK(!source.IsNull());

  switch (cursor_type_) {
    case IndexedDB::kCursorKeyOnly:
      cursor = IDBCursor::Create(std::move(backend), cursor_direction_, this,
                                 source, transaction_.Get());
      break;
    case IndexedDB::kCursorKeyAndValue:
      cursor = IDBCursorWithValue::Create(std::move(backend), cursor_direction_,
                                          this, source, transaction_.Get());
      break;
    default:
      NOTREACHED();
  }
  SetResultCursor(cursor, std::move(key), std::move(primary_key),
                  std::move(value));
  metrics_.RecordAndReset();
}

void IDBRequest::EnqueueResponse(std::unique_ptr<IDBKey> idb_key) {
  IDB_TRACE("IDBRequest::EnqueueResponse(IDBKey)");
  if (!ShouldEnqueueEvent()) {
    metrics_.RecordAndReset();
    return;
  }

  if (idb_key && idb_key->IsValid())
    EnqueueResultInternal(IDBAny::Create(std::move(idb_key)));
  else
    EnqueueResultInternal(IDBAny::CreateUndefined());
  metrics_.RecordAndReset();
}

namespace {
size_t SizeOfValues(const Vector<std::unique_ptr<IDBValue>>& values) {
  size_t size = 0;
  for (const auto& value : values)
    size += value->DataSize();
  return size;
}
}  // namespace

void IDBRequest::EnqueueResponse(Vector<std::unique_ptr<IDBValue>> values) {
  IDB_TRACE1("IDBRequest::EnqueueResponse([IDBValue])", "size",
             SizeOfValues(values));
  if (!ShouldEnqueueEvent()) {
    metrics_.RecordAndReset();
    return;
  }

  EnqueueResultInternal(IDBAny::Create(std::move(values)));
  metrics_.RecordAndReset();
}

#if DCHECK_IS_ON()
static IDBObjectStore* EffectiveObjectStore(const IDBRequest::Source& source) {
  if (source.IsIDBObjectStore())
    return source.GetAsIDBObjectStore();
  if (source.IsIDBIndex())
    return source.GetAsIDBIndex()->objectStore();

  NOTREACHED();
  return nullptr;
}
#endif  // DCHECK_IS_ON()

void IDBRequest::EnqueueResponse(std::unique_ptr<IDBValue> value) {
  IDB_TRACE1("IDBRequest::EnqueueResponse(IDBValue)", "size",
             value ? value->DataSize() : 0);
  if (!ShouldEnqueueEvent()) {
    metrics_.RecordAndReset();
    return;
  }

  if (pending_cursor_) {
    // Value should be null, signifying the end of the cursor's range.
    DCHECK(value->IsNull());
    DCHECK(!value->BlobInfo().size());
    pending_cursor_->Close();
    pending_cursor_.Clear();
  }

#if DCHECK_IS_ON()
  DCHECK(!value->PrimaryKey() ||
         value->KeyPath() == EffectiveObjectStore(source_)->IdbKeyPath());
#endif

  EnqueueResultInternal(IDBAny::Create(std::move(value)));
  metrics_.RecordAndReset();
}

void IDBRequest::EnqueueResponse(int64_t value) {
  IDB_TRACE("IDBRequest::EnqueueResponse(int64_t)");
  if (!ShouldEnqueueEvent()) {
    metrics_.RecordAndReset();
    return;
  }
  EnqueueResultInternal(IDBAny::Create(value));
  metrics_.RecordAndReset();
}

void IDBRequest::EnqueueResponse() {
  IDB_TRACE("IDBRequest::EnqueueResponse()");
  if (!ShouldEnqueueEvent()) {
    metrics_.RecordAndReset();
    return;
  }
  EnqueueResultInternal(IDBAny::CreateUndefined());
  metrics_.RecordAndReset();
}

void IDBRequest::EnqueueResultInternal(IDBAny* result) {
  DCHECK(GetExecutionContext());
  DCHECK(!pending_cursor_);
  DCHECK(transit_blob_handles_.IsEmpty());
  SetResult(result);
  EnqueueEvent(Event::Create(EventTypeNames::success));
}

void IDBRequest::SetResult(IDBAny* result) {
  result_ = result;
  result_dirty_ = true;
}

void IDBRequest::EnqueueResponse(std::unique_ptr<IDBKey> key,
                                 std::unique_ptr<IDBKey> primary_key,
                                 std::unique_ptr<IDBValue> value) {
  IDB_TRACE("IDBRequest::EnqueueResponse(IDBKey, IDBKey primaryKey, IDBValue)");
  if (!ShouldEnqueueEvent()) {
    metrics_.RecordAndReset();
    return;
  }

  DCHECK(pending_cursor_);
  SetResultCursor(pending_cursor_.Release(), std::move(key),
                  std::move(primary_key), std::move(value));
  metrics_.RecordAndReset();
}

bool IDBRequest::HasPendingActivity() const {
  // FIXME: In an ideal world, we should return true as long as anyone has a or
  //        can get a handle to us and we have event listeners. This is order to
  //        handle user generated events properly.
  return has_pending_activity_ && GetExecutionContext();
}

void IDBRequest::ContextDestroyed(ExecutionContext*) {
  if (ready_state_ == PENDING) {
    ready_state_ = kEarlyDeath;
    if (queue_item_)
      queue_item_->CancelLoading();
    if (transaction_)
      transaction_->UnregisterRequest(this);
  }

  if (source_.IsIDBCursor())
    source_.GetAsIDBCursor()->ContextWillBeDestroyed();
  if (result_)
    result_->ContextWillBeDestroyed();
  if (pending_cursor_)
    pending_cursor_->ContextWillBeDestroyed();
  if (web_callbacks_) {
    web_callbacks_->Detach();
    web_callbacks_ = nullptr;
  }
}

const AtomicString& IDBRequest::InterfaceName() const {
  return EventTargetNames::IDBRequest;
}

ExecutionContext* IDBRequest::GetExecutionContext() const {
  return PausableObject::GetExecutionContext();
}

DispatchEventResult IDBRequest::DispatchEventInternal(Event& event) {
  IDB_TRACE("IDBRequest::dispatchEvent");
  if (!GetExecutionContext())
    return DispatchEventResult::kCanceledBeforeDispatch;
  DCHECK_EQ(ready_state_, PENDING);
  DCHECK(has_pending_activity_);
  DCHECK_EQ(event.target(), this);

  if (event.type() != EventTypeNames::blocked)
    ready_state_ = DONE;

  HeapVector<Member<EventTarget>> targets;
  targets.push_back(this);
  if (transaction_ && !prevent_propagation_) {
    targets.push_back(transaction_);
    // If there ever are events that are associated with a database but
    // that do not have a transaction, then this will not work and we need
    // this object to actually hold a reference to the database (to ensure
    // it stays alive).
    targets.push_back(transaction_->db());
  }

  // Cursor properties should not be updated until the success event is being
  // dispatched.
  IDBCursor* cursor_to_notify = nullptr;
  if (event.type() == EventTypeNames::success) {
    cursor_to_notify = GetResultCursor();
    if (cursor_to_notify) {
      cursor_to_notify->SetValueReady(std::move(cursor_key_),
                                      std::move(cursor_primary_key_),
                                      std::move(cursor_value_));
    }
  }

  if (event.type() == EventTypeNames::upgradeneeded) {
    DCHECK(!did_fire_upgrade_needed_event_);
    did_fire_upgrade_needed_event_ = true;
  }

  // FIXME: When we allow custom event dispatching, this will probably need to
  // change.
  DCHECK(event.type() == EventTypeNames::success ||
         event.type() == EventTypeNames::error ||
         event.type() == EventTypeNames::blocked ||
         event.type() == EventTypeNames::upgradeneeded)
      << "event type was " << event.type();
  const bool set_transaction_active =
      transaction_ &&
      (event.type() == EventTypeNames::success ||
       event.type() == EventTypeNames::upgradeneeded ||
       (event.type() == EventTypeNames::error && !request_aborted_));

  if (set_transaction_active)
    transaction_->SetActive(true);

  // The request must be unregistered from the transaction before the event
  // handler is invoked, because the handler can call an IDBCursor method that
  // reuses this request, like continue() or advance(). http://crbug.com/724109
  // describes the consequences of getting this wrong.
  if (transaction_ && ready_state_ == DONE)
    transaction_->UnregisterRequest(this);

  event.SetTarget(this);
  DispatchEventResult dispatch_result =
      IDBEventDispatcher::Dispatch(event, targets);

  if (transaction_) {
    // Possibly abort the transaction. This must occur after unregistering (so
    // this request doesn't receive a second error) and before deactivating
    // (which might trigger commit).
    if (!request_aborted_) {
      // Transactions should be aborted after event dispatch if an exception was
      // not caught.
      if (event.LegacyDidListenersThrow()) {
        transaction_->SetError(
            DOMException::Create(DOMExceptionCode::kAbortError,
                                 "Uncaught exception in event handler."));
        transaction_->abort(IGNORE_EXCEPTION_FOR_TESTING);
      } else if (event.type() == EventTypeNames::error &&
                 dispatch_result == DispatchEventResult::kNotCanceled) {
        transaction_->SetError(error_);
        transaction_->abort(IGNORE_EXCEPTION_FOR_TESTING);
      }
    }

    // If this was the last request in the transaction's list, it may commit
    // here.
    if (set_transaction_active)
      transaction_->SetActive(false);
  }

  if (cursor_to_notify)
    cursor_to_notify->PostSuccessHandlerCallback();

  // An upgradeneeded event will always be followed by a success or error event,
  // so must be kept alive.
  if (ready_state_ == DONE && event.type() != EventTypeNames::upgradeneeded)
    has_pending_activity_ = false;

  return dispatch_result;
}

void IDBRequest::TransactionDidFinishAndDispatch() {
  DCHECK(transaction_);
  DCHECK(transaction_->IsVersionChange());
  DCHECK(did_fire_upgrade_needed_event_);
  DCHECK_EQ(ready_state_, DONE);
  DCHECK(GetExecutionContext());
  transaction_.Clear();

  if (!GetExecutionContext())
    return;

  ready_state_ = PENDING;
}

void IDBRequest::EnqueueEvent(Event* event) {
  DCHECK(ready_state_ == PENDING || ready_state_ == DONE);

  if (!GetExecutionContext())
    return;

  DCHECK(ready_state_ == PENDING || did_fire_upgrade_needed_event_)
      << "When queueing event " << event->type() << ", ready_state_ was "
      << ready_state_;

  event->SetTarget(this);

  event_queue_->EnqueueEvent(FROM_HERE, *event);
}

}  // namespace blink
