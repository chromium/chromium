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
#include <optional>
#include <utility>

#include "base/debug/stack_trace.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "third_party/blink/public/platform/web_blob_info.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_binding_for_modules.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_idbcursor_idbindex_idbobjectstore.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_idbindex_idbobjectstore.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/modules/indexed_db_names.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_cursor_with_value.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_database.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_event_dispatcher.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_factory_client.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_request_queue_item.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value_wrapping.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

const char* RequestTypeToName(IDBRequest::TypeForMetrics type) {
  switch (type) {
    case IDBRequest::TypeForMetrics::kCursorAdvance:
      return "IDBCursor::advance";
    case IDBRequest::TypeForMetrics::kCursorContinue:
      return "IDBCursor::continue";
    case IDBRequest::TypeForMetrics::kCursorContinuePrimaryKey:
      return "IDBCursor::continuePrimaryKEy";
    case IDBRequest::TypeForMetrics::kCursorDelete:
      return "IDBCursor::delete";

    case IDBRequest::TypeForMetrics::kFactoryOpen:
      return "IDBFactory::open";
    case IDBRequest::TypeForMetrics::kFactoryDeleteDatabase:
      return "IDBFactory::deleteDatabase";

    case IDBRequest::TypeForMetrics::kIndexOpenCursor:
      return "IDBIndex::openCursor";
    case IDBRequest::TypeForMetrics::kIndexCount:
      return "IDBIndex::count";
    case IDBRequest::TypeForMetrics::kIndexOpenKeyCursor:
      return "IDBIndex::openKeyCursor";
    case IDBRequest::TypeForMetrics::kIndexGet:
      return "IDBIndex::get";
    case IDBRequest::TypeForMetrics::kIndexGetAll:
      return "IDBIndex::getAll";
    case IDBRequest::TypeForMetrics::kIndexGetAllKeys:
      return "IDBIndex::getAllKeys";
    case IDBRequest::TypeForMetrics::kIndexGetKey:
      return "IDBIndex::getKey";

    case IDBRequest::TypeForMetrics::kObjectStoreGet:
      return "IDBObjectStore::get";
    case IDBRequest::TypeForMetrics::kObjectStoreGetKey:
      return "IDBObjectStore::getKey";
    case IDBRequest::TypeForMetrics::kObjectStoreGetAll:
      return "IDBObjectStore::getAll";
    case IDBRequest::TypeForMetrics::kObjectStoreGetAllKeys:
      return "IDBObjectStore::getAllKeys";
    case IDBRequest::TypeForMetrics::kObjectStoreDelete:
      return "IDBObjectStore::delete";
    case IDBRequest::TypeForMetrics::kObjectStoreClear:
      return "IDBObjectStore::clear";
    case IDBRequest::TypeForMetrics::kObjectStoreCreateIndex:
      return "IDBObjectStore::createIndex";

    case IDBRequest::TypeForMetrics::kObjectStorePut:
      return "IDBObjectStore::put";
    case IDBRequest::TypeForMetrics::kObjectStoreAdd:
      return "IDBObjectStore::add";
    case IDBRequest::TypeForMetrics::kObjectStoreUpdate:
      return "IDBObjectStore::update";
    case IDBRequest::TypeForMetrics::kObjectStoreOpenCursor:
      return "IDBObjectStore::openCursor";
    case IDBRequest::TypeForMetrics::kObjectStoreOpenKeyCursor:
      return "IDBObjectStore::openKeyCursor";
    case IDBRequest::TypeForMetrics::kObjectStoreCount:
      return "IDBObjectStore::count";
  }
}

void RecordHistogram(IDBRequest::TypeForMetrics type,
                     bool success,
                     base::TimeDelta duration) {
  switch (type) {
    case IDBRequest::TypeForMetrics::kObjectStorePut:
      UMA_HISTOGRAM_TIMES("WebCore.IndexedDB.RequestDuration2.ObjectStorePut",
                          duration);
      base::UmaHistogramBoolean(
          "WebCore.IndexedDB.RequestDispatchOutcome.ObjectStorePut", success);
      break;
    case IDBRequest::TypeForMetrics::kObjectStoreAdd:
      UMA_HISTOGRAM_TIMES("WebCore.IndexedDB.RequestDuration2.ObjectStoreAdd",
                          duration);
      base::UmaHistogramBoolean(
          "WebCore.IndexedDB.RequestDispatchOutcome.ObjectStoreAdd", success);
      break;
    case IDBRequest::TypeForMetrics::kObjectStoreGet:
      UMA_HISTOGRAM_TIMES("WebCore.IndexedDB.RequestDuration2.ObjectStoreGet",
                          duration);
      base::UmaHistogramBoolean(
          "WebCore.IndexedDB.RequestDispatchOutcome.ObjectStoreGet", success);
      break;

    case IDBRequest::TypeForMetrics::kFactoryOpen:
      UMA_HISTOGRAM_TIMES("WebCore.IndexedDB.RequestDuration2.Open", duration);
      base::UmaHistogramBoolean("WebCore.IndexedDB.RequestDispatchOutcome.Open",
                                success);
      break;

    case IDBRequest::TypeForMetrics::kCursorAdvance:
    case IDBRequest::TypeForMetrics::kCursorContinue:
    case IDBRequest::TypeForMetrics::kCursorContinuePrimaryKey:
    case IDBRequest::TypeForMetrics::kCursorDelete:
    case IDBRequest::TypeForMetrics::kFactoryDeleteDatabase:
    case IDBRequest::TypeForMetrics::kIndexOpenCursor:
    case IDBRequest::TypeForMetrics::kIndexCount:
    case IDBRequest::TypeForMetrics::kIndexOpenKeyCursor:
    case IDBRequest::TypeForMetrics::kIndexGet:
    case IDBRequest::TypeForMetrics::kIndexGetAll:
    case IDBRequest::TypeForMetrics::kIndexGetAllKeys:
    case IDBRequest::TypeForMetrics::kIndexGetKey:
    case IDBRequest::TypeForMetrics::kObjectStoreGetKey:
    case IDBRequest::TypeForMetrics::kObjectStoreGetAll:
    case IDBRequest::TypeForMetrics::kObjectStoreGetAllKeys:
    case IDBRequest::TypeForMetrics::kObjectStoreDelete:
    case IDBRequest::TypeForMetrics::kObjectStoreClear:
    case IDBRequest::TypeForMetrics::kObjectStoreCreateIndex:
    case IDBRequest::TypeForMetrics::kObjectStoreUpdate:
    case IDBRequest::TypeForMetrics::kObjectStoreOpenCursor:
    case IDBRequest::TypeForMetrics::kObjectStoreOpenKeyCursor:
    case IDBRequest::TypeForMetrics::kObjectStoreCount:
      break;
  }
}

}  // namespace

IDBRequest::AsyncTraceState::AsyncTraceState(TypeForMetrics type)
    : type_(type), start_time_(base::TimeTicks::Now()) {
  static std::atomic<size_t> counter(0);
  id_ = counter.fetch_add(1, std::memory_order_relaxed);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("IndexedDB", RequestTypeToName(type),
                                    TRACE_ID_LOCAL(id_));
}

void IDBRequest::AsyncTraceState::WillDispatchResult(bool success) {
  if (type_) {
    RecordHistogram(*type_, success, base::TimeTicks::Now() - start_time_);
    RecordAndReset();
  }
}

void IDBRequest::AsyncTraceState::RecordAndReset() {
  if (type_) {
    TRACE_EVENT_NESTABLE_ASYNC_END0("IndexedDB", RequestTypeToName(*type_),
                                    TRACE_ID_LOCAL(id_));
    type_.reset();
  }
}

IDBRequest::AsyncTraceState::~AsyncTraceState() {
  RecordAndReset();
}

IDBRequest* IDBRequest::Create(ScriptState* script_state,
                               IDBIndex* source,
                               IDBTransaction* transaction,
                               IDBRequest::AsyncTraceState metrics) {
  return Create(script_state,
                source ? MakeGarbageCollected<Source>(source) : nullptr,
                transaction, std::move(metrics));
}

IDBRequest* IDBRequest::Create(ScriptState* script_state,
                               IDBObjectStore* source,
                               IDBTransaction* transaction,
                               IDBRequest::AsyncTraceState metrics) {
  return Create(script_state,
                source ? MakeGarbageCollected<Source>(source) : nullptr,
                transaction, std::move(metrics));
}

IDBRequest* IDBRequest::Create(ScriptState* script_state,
                               IDBCursor* source,
                               IDBTransaction* transaction,
                               IDBRequest::AsyncTraceState metrics) {
  return Create(script_state,
                source ? MakeGarbageCollected<Source>(source) : nullptr,
                transaction, std::move(metrics));
}

IDBRequest* IDBRequest::Create(ScriptState* script_state,
                               const Source* source,
                               IDBTransaction* transaction,
                               IDBRequest::AsyncTraceState metrics) {
  IDBRequest* request = MakeGarbageCollected<IDBRequest>(
      script_state, source, transaction, std::move(metrics));
  // Requests associated with IDBFactory (open/deleteDatabase/getDatabaseNames)
  // do not have an associated transaction.
  if (transaction)
    transaction->RegisterRequest(request);
  return request;
}

IDBRequest::IDBRequest(ScriptState* script_state,
                       const Source* source,
                       IDBTransaction* transaction,
                       AsyncTraceState metrics)
    : ActiveScriptWrappable<IDBRequest>({}),
      ExecutionContextLifecycleObserver(ExecutionContext::From(script_state)),
      transaction_(transaction),
      isolate_(script_state->GetIsolate()),
      metrics_(std::move(metrics)),
      source_(source) {
  async_task_context_.Schedule(ExecutionContext::From(script_state),
                               indexed_db_names::kIndexedDB);
}

IDBRequest::~IDBRequest() {
  if (!GetExecutionContext())
    return;
  if (ready_state_ == DONE)
    DCHECK(metrics_.IsEmpty()) << metrics_.id();
  else
    DCHECK_EQ(ready_state_, kEarlyDeath);
}

void IDBRequest::Trace(Visitor* visitor) const {
  visitor->Trace(transaction_);
  visitor->Trace(source_);
  visitor->Trace(result_);
  visitor->Trace(error_);
  visitor->Trace(pending_cursor_);
  EventTarget::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
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
  v8::Local<v8::Value> value;
  if (!result_) {
    value = v8::Null(script_state->GetIsolate());
  } else {
    value = result_->ToV8(script_state);
  }
  return ScriptValue(script_state->GetIsolate(), value);
}

DOMException* IDBRequest::error(ExceptionState& exception_state) const {
  if (ready_state_ != DONE) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        IDBDatabase::kRequestNotFinishedErrorMessage);
    return nullptr;
  }
  return error_.Get();
}

const IDBRequest::Source* IDBRequest::source(ScriptState* script_state) const {
  if (!GetExecutionContext())
    return nullptr;
  return source_.Get();
}

const String& IDBRequest::readyState() const {
  if (!GetExecutionContext()) {
    DCHECK(ready_state_ == DONE || ready_state_ == kEarlyDeath);
    return indexed_db_names::kDone;
  }

  DCHECK(ready_state_ == PENDING || ready_state_ == DONE);

  if (ready_state_ == PENDING)
    return indexed_db_names::kPending;

  return indexed_db_names::kDone;
}

void IDBRequest::Abort(bool queue_dispatch) {
  DCHECK(!request_aborted_);
  if (queue_item_) {
    queue_item_->CancelLoading();
  }

  if (!GetExecutionContext() || GetExecutionContext()->IsContextDestroyed()) {
    return;
  }
  DCHECK(ready_state_ == PENDING || ready_state_ == DONE) << ready_state_;
  if (ready_state_ == DONE)
    return;

  request_aborted_ = true;
  auto send_exception =
      WTF::BindOnce(&IDBRequest::SendError, WrapWeakPersistent(this),
                    WrapPersistent(MakeGarbageCollected<DOMException>(
                        DOMExceptionCode::kAbortError,
                        "The transaction was aborted, so the "
                        "request cannot be fulfilled.")),
                    /*force=*/true);
  if (queue_dispatch) {
    GetExecutionContext()
        ->GetTaskRunner(TaskType::kDatabaseAccess)
        ->PostTask(FROM_HERE, std::move(send_exception));
  } else {
    std::move(send_exception).Run();
  }
}

void IDBRequest::SetCursorDetails(indexed_db::CursorType cursor_type,
                                  mojom::IDBCursorDirection direction) {
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

void IDBRequest::SendResultCursorInternal(IDBCursor* cursor,
                                          std::unique_ptr<IDBKey> key,
                                          std::unique_ptr<IDBKey> primary_key,
                                          std::unique_ptr<IDBValue> value) {
  DCHECK_EQ(ready_state_, PENDING);
  cursor_key_ = std::move(key);
  cursor_primary_key_ = std::move(primary_key);
  cursor_value_ = std::move(value);

  SendResult(MakeGarbageCollected<IDBAny>(cursor));
}

bool IDBRequest::CanStillSendResult() const {
  // It's possible to attempt event dispatch after a context is destroyed,
  // but before `ContextDestroyed()` has been called. See
  // https://crbug.com/733642
  const ExecutionContext* execution_context = GetExecutionContext();
  if (!execution_context || execution_context->IsContextDestroyed()) {
    return false;
  }

  DCHECK(ready_state_ == PENDING || ready_state_ == DONE);
  if (request_aborted_)
    return false;
  DCHECK_EQ(ready_state_, PENDING);
  DCHECK(!error_ && !result_);
  return true;
}

void IDBRequest::HandleResponse(std::unique_ptr<IDBKey> key) {
  transit_blob_handles_.clear();
  transaction_->EnqueueResult(std::make_unique<IDBRequestQueueItem>(
      this, std::move(key),
      WTF::BindOnce(&IDBTransaction::OnResultReady,
                    WrapPersistent(transaction_.Get()))));
}

void IDBRequest::HandleResponse(int64_t value) {
  DCHECK(transit_blob_handles_.empty());
  transaction_->EnqueueResult(std::make_unique<IDBRequestQueueItem>(
      this, value,
      WTF::BindOnce(&IDBTransaction::OnResultReady,
                    WrapPersistent(transaction_.Get()))));
}

void IDBRequest::HandleResponse() {
  transit_blob_handles_.clear();
  transaction_->EnqueueResult(std::make_unique<IDBRequestQueueItem>(
      this, WTF::BindOnce(&IDBTransaction::OnResultReady,
                          WrapPersistent(transaction_.Get()))));
}

void IDBRequest::HandleResponse(std::unique_ptr<IDBValue> value) {
  DCHECK(transit_blob_handles_.empty());
  value->SetIsolate(GetIsolate());
  transaction_->EnqueueResult(std::make_unique<IDBRequestQueueItem>(
      this, std::move(value),
      WTF::BindOnce(&IDBTransaction::OnResultReady,
                    WrapPersistent(transaction_.Get()))));
}

void IDBRequest::HandleResponseAdvanceCursor(
    std::unique_ptr<IDBKey> key,
    std::unique_ptr<IDBKey> primary_key,
    std::unique_ptr<IDBValue> optional_value) {
  DCHECK(transit_blob_handles_.empty());

  std::unique_ptr<IDBValue> value =
      optional_value
          ? std::move(optional_value)
          : std::make_unique<IDBValue>(Vector<char>(), Vector<WebBlobInfo>());
  value->SetIsolate(GetIsolate());
  transaction_->EnqueueResult(std::make_unique<IDBRequestQueueItem>(
      this, std::move(key), std::move(primary_key), std::move(value),
      WTF::BindOnce(&IDBTransaction::OnResultReady,
                    WrapPersistent(transaction_.Get()))));
}

void IDBRequest::OnClear(bool success) {
  if (success) {
    probe::AsyncTask async_task(GetExecutionContext(), &async_task_context_,
                                "clear");
    HandleResponse();
  }
}

void IDBRequest::OnGetAll(
    bool key_only,
    mojo::PendingAssociatedReceiver<mojom::blink::IDBDatabaseGetAllResultSink>
        receiver) {
  probe::AsyncTask async_task(GetExecutionContext(), &async_task_context_,
                              "success");
  DCHECK(transit_blob_handles_.empty());
  transaction_->EnqueueResult(std::make_unique<IDBRequestQueueItem>(
      this, key_only, std::move(receiver),
      WTF::BindOnce(&IDBTransaction::OnResultReady,
                    WrapPersistent(transaction_.Get()))));
}

void IDBRequest::OnDelete(bool success) {
  if (success) {
    probe::AsyncTask async_task(GetExecutionContext(), &async_task_context_,
                                "delete");
    HandleResponse();
  }
}

void IDBRequest::OnCount(bool success, uint32_t count) {
  if (success) {
    HandleResponse(count);
  }
}

void IDBRequest::OnPut(mojom::blink::IDBTransactionPutResultPtr result) {
  if (result->is_error_result()) {
    HandleError(std::move(result->get_error_result()));
    return;
  }

  probe::AsyncTask async_task(GetExecutionContext(), &async_task_context_,
                              "put");
  DCHECK(result->is_key());
  HandleResponse(std::move(result->get_key()));
}

void IDBRequest::OnGet(mojom::blink::IDBDatabaseGetResultPtr result) {
  if (result->is_error_result()) {
    HandleError(std::move(result->get_error_result()));
    return;
  }

  probe::AsyncTask async_task(GetExecutionContext(), &async_task_context_,
                              "get");
  if (result->is_empty()) {
    HandleResponse();
  } else if (result->is_key()) {
    HandleResponse(std::move(result->get_key()));
  } else if (result->is_value()) {
    std::unique_ptr<IDBValue> value =
        IDBValue::ConvertReturnValue(result->get_value());
    HandleResponse(std::move(value));
  }
}

void IDBRequest::OnOpenCursor(
    mojom::blink::IDBDatabaseOpenCursorResultPtr result) {
  if (result->is_error_result()) {
    HandleError(std::move(result->get_error_result()));
    return;
  }

  probe::AsyncTask async_task(GetExecutionContext(), &async_task_context_,
                              "openCursor");
  if (result->is_empty()) {
    std::unique_ptr<IDBValue> value = IDBValue::ConvertReturnValue(nullptr);
    HandleResponse(std::move(value));
    return;
  }

  std::unique_ptr<IDBValue> value;
  if (result->get_value()->value) {
    value = std::move(*result->get_value()->value);
  } else {
    value = std::make_unique<IDBValue>(Vector<char>(), Vector<WebBlobInfo>());
  }

  value->SetIsolate(GetIsolate());

  transaction_->EnqueueResult(std::make_unique<IDBRequestQueueItem>(
      this, std::move(result->get_value()->cursor),
      std::move(result->get_value()->key),
      std::move(result->get_value()->primary_key), std::move(value),
      WTF::BindOnce(&IDBTransaction::OnResultReady,
                    WrapPersistent(transaction_.Get()))));
}

void IDBRequest::OnAdvanceCursor(mojom::blink::IDBCursorResultPtr result) {
  if (result->is_error_result()) {
    HandleError(std::move(result->get_error_result()));
    return;
  }

  if (result->is_empty() && !result->get_empty()) {
    HandleError(nullptr);
    return;
  }

  if (!result->is_empty() && (result->get_values()->keys.size() != 1u ||
                              result->get_values()->primary_keys.size() != 1u ||
                              result->get_values()->values.size() != 1u)) {
    HandleError(nullptr);
    return;
  }

  probe::AsyncTask async_task(GetExecutionContext(), &async_task_context_,
                              "advanceCursor");

  if (result->is_empty()) {
    std::unique_ptr<IDBValue> value = IDBValue::ConvertReturnValue(nullptr);
    HandleResponse(std::move(value));
    return;
  }

  HandleResponseAdvanceCursor(std::move(result->get_values()->keys[0]),
                              std::move(result->get_values()->primary_keys[0]),
                              std::move(result->get_values()->values[0]));
}

void IDBRequest::OnGotKeyGeneratorCurrentNumber(
    int64_t number,
    mojom::blink::IDBErrorPtr error) {
  if (error) {
    HandleError(std::move(error));
  } else {
    DCHECK_GE(number, 0);
    HandleResponse(number);
  }
}

void IDBRequest::SendError(DOMException* error, bool force) {
  TRACE_EVENT0("IndexedDB", "IDBRequest::SendError()");
  probe::AsyncTask async_task(GetExecutionContext(), async_task_context(),
                              "error");
  if (!GetExecutionContext() || (request_aborted_ && !force)) {
    metrics_.RecordAndReset();
    return;
  }

  error_ = error;
  SetResult(MakeGarbageCollected<IDBAny>(IDBAny::kUndefinedType));
  pending_cursor_.Clear();
  DispatchEvent(*Event::CreateCancelableBubble(event_type_names::kError));
}

void IDBRequest::HandleError(mojom::blink::IDBErrorPtr error) {
  mojom::blink::IDBException code =
      error ? error->error_code : mojom::blink::IDBException::kUnknownError;
  // In some cases, the backend clears the pending transaction task queue
  // which destroys all pending tasks.  If our callback was queued with a task
  // that gets cleared, we'll get a signal with an IgnorableAbortError as the
  // task is torn down.  This means the error response can be safely ignored.
  if (code == mojom::blink::IDBException::kIgnorableAbortError) {
    return;
  }
  probe::AsyncTask async_task(GetExecutionContext(), &async_task_context_,
                              "error");
  auto* exception = MakeGarbageCollected<DOMException>(
      static_cast<DOMExceptionCode>(code),
      error ? error->error_message : "Invalid response");

  transit_blob_handles_.clear();
  transaction_->EnqueueResult(std::make_unique<IDBRequestQueueItem>(
      this, exception,
      WTF::BindOnce(&IDBTransaction::OnResultReady,
                    WrapPersistent(transaction_.Get()))));
}

void IDBRequest::SendResultCursor(
    mojo::PendingAssociatedRemote<mojom::blink::IDBCursor>
        pending_remote_cursor,
    std::unique_ptr<IDBKey> key,
    std::unique_ptr<IDBKey> primary_key,
    std::unique_ptr<IDBValue> value) {
  if (!CanStillSendResult()) {
    metrics_.RecordAndReset();
    return;
  }

  DCHECK(!pending_cursor_);
  IDBCursor* cursor = nullptr;
  IDBCursor::Source* source = nullptr;

  DCHECK(source_);
  switch (source_->GetContentType()) {
    case Source::ContentType::kIDBCursor:
      break;
    case Source::ContentType::kIDBIndex:
      source =
          MakeGarbageCollected<IDBCursor::Source>(source_->GetAsIDBIndex());
      break;
    case Source::ContentType::kIDBObjectStore:
      source = MakeGarbageCollected<IDBCursor::Source>(
          source_->GetAsIDBObjectStore());
      break;
  }
  DCHECK(source);

  switch (cursor_type_) {
    case indexed_db::kCursorKeyOnly:
      cursor = MakeGarbageCollected<IDBCursor>(std::move(pending_remote_cursor),
                                               cursor_direction_, this, source,
                                               transaction_.Get());
      break;
    case indexed_db::kCursorKeyAndValue:
      cursor = MakeGarbageCollected<IDBCursorWithValue>(
          std::move(pending_remote_cursor), cursor_direction_, this, source,
          transaction_.Get());
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  SendResultCursorInternal(cursor, std::move(key), std::move(primary_key),
                           std::move(value));
}

#if DCHECK_IS_ON()
static IDBObjectStore* EffectiveObjectStore(const IDBRequest::Source* source) {
  DCHECK(source);
  switch (source->GetContentType()) {
    case IDBRequest::Source::ContentType::kIDBCursor:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
    case IDBRequest::Source::ContentType::kIDBIndex:
      return source->GetAsIDBIndex()->objectStore();
    case IDBRequest::Source::ContentType::kIDBObjectStore:
      return source->GetAsIDBObjectStore();
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}
#endif  // DCHECK_IS_ON()

void IDBRequest::SendResult(IDBAny* result) {
  TRACE_EVENT1("IndexedDB", "IDBRequest::SendResult", "type",
               result->GetType());

  if (!CanStillSendResult()) {
    metrics_.RecordAndReset();
    return;
  }

  DCHECK(!pending_cursor_);
  DCHECK(transit_blob_handles_.empty());
  SetResult(result);
  DispatchEvent(*Event::Create(event_type_names::kSuccess));
}

void IDBRequest::SetResult(IDBAny* result) {
  result_ = result;
  result_dirty_ = true;
}

void IDBRequest::SendResultValue(std::unique_ptr<IDBValue> value) {
  // See crbug.com/1519989
  if (!CanStillSendResult()) {
    metrics_.RecordAndReset();
    return;
  }

  if (pending_cursor_) {
    // Value should be empty, signifying the end of the cursor's range.
    DCHECK(!value->DataSize());
    DCHECK(!value->BlobInfo().size());
    pending_cursor_->Close();
    pending_cursor_.Clear();
  }

#if DCHECK_IS_ON()
  DCHECK(!value->PrimaryKey() ||
         value->KeyPath() == EffectiveObjectStore(source_)->IdbKeyPath());
#endif

  SendResult(MakeGarbageCollected<IDBAny>(std::move(value)));
}

void IDBRequest::SendResultAdvanceCursor(std::unique_ptr<IDBKey> key,
                                         std::unique_ptr<IDBKey> primary_key,
                                         std::unique_ptr<IDBValue> value) {
  if (!CanStillSendResult()) {
    metrics_.RecordAndReset();
    return;
  }

  DCHECK(pending_cursor_);
  SendResultCursorInternal(pending_cursor_.Release(), std::move(key),
                           std::move(primary_key), std::move(value));
}

bool IDBRequest::HasPendingActivity() const {
  // FIXME: In an ideal world, we should return true as long as anyone has a or
  //        can get a handle to us and we have event listeners. This is order to
  //        handle user generated events properly.
  return has_pending_activity_ && GetExecutionContext();
}

void IDBRequest::ContextDestroyed() {
  if (ready_state_ == PENDING) {
    ready_state_ = kEarlyDeath;
    if (queue_item_)
      queue_item_->CancelLoading();
    if (transaction_)
      transaction_->UnregisterRequest(this);
  }

  if (source_ && source_->IsIDBCursor())
    source_->GetAsIDBCursor()->ContextWillBeDestroyed();
  if (result_)
    result_->ContextWillBeDestroyed();
  if (pending_cursor_)
    pending_cursor_->ContextWillBeDestroyed();
}

const AtomicString& IDBRequest::InterfaceName() const {
  return event_target_names::kIDBRequest;
}

ExecutionContext* IDBRequest::GetExecutionContext() const {
  return ExecutionContextLifecycleObserver::GetExecutionContext();
}

DispatchEventResult IDBRequest::DispatchEventInternal(Event& event) {
  TRACE_EVENT0("IndexedDB", "IDBRequest::dispatchEvent");

  event.SetTarget(this);

  HeapVector<Member<EventTarget>> targets;
  targets.push_back(this);
  if (transaction_ && !prevent_propagation_) {
    // Per spec: "A request's get the parent algorithm returns the request’s
    // transaction."
    targets.push_back(transaction_);
    // Per spec: "A transaction's get the parent algorithm returns the
    // transaction’s connection."
    targets.push_back(transaction_->db());
  }

  // If this event originated from script, it should have no side effects.
  if (!event.isTrusted())
    return IDBEventDispatcher::Dispatch(event, targets);
  DCHECK(event.type() == event_type_names::kSuccess ||
         event.type() == event_type_names::kError ||
         event.type() == event_type_names::kBlocked ||
         event.type() == event_type_names::kUpgradeneeded)
      << "event type was " << event.type();

  if (!GetExecutionContext())
    return DispatchEventResult::kCanceledBeforeDispatch;
  DCHECK_EQ(ready_state_, PENDING);
  DCHECK(has_pending_activity_);
  DCHECK_EQ(event.target(), this);

  if (event.type() != event_type_names::kBlocked) {
    ready_state_ = DONE;
  }

  // Cursor properties should not be updated until the success event is being
  // dispatched.
  IDBCursor* cursor_to_notify = nullptr;
  if (event.type() == event_type_names::kSuccess) {
    cursor_to_notify = GetResultCursor();
    if (cursor_to_notify) {
      cursor_to_notify->SetValueReady(std::move(cursor_key_),
                                      std::move(cursor_primary_key_),
                                      std::move(cursor_value_));
    }
  }

  if (event.type() == event_type_names::kUpgradeneeded) {
    DCHECK(!did_fire_upgrade_needed_event_);
    did_fire_upgrade_needed_event_ = true;
  }

  const bool set_transaction_active =
      transaction_ &&
      (event.type() == event_type_names::kSuccess ||
       event.type() == event_type_names::kUpgradeneeded ||
       (event.type() == event_type_names::kError && !request_aborted_));

  if (set_transaction_active) {
    transaction_->SetActive(true);
  }

  // The request must be unregistered from the transaction before the event
  // handler is invoked, because the handler can call an IDBCursor method that
  // reuses this request, like continue() or advance(). http://crbug.com/724109
  // describes the consequences of getting this wrong.
  if (transaction_ && ready_state_ == DONE)
    transaction_->UnregisterRequest(this);

  if (event.type() == event_type_names::kError && transaction_)
    transaction_->IncrementNumErrorsHandled();

  // Now that the event dispatching has been triggered, record that the metric
  // has completed.
  metrics_.WillDispatchResult(/*success=*/
                              event.type() != event_type_names::kError);

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
        transaction_->StartAborting(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kAbortError,
            "Uncaught exception in event handler."));
      } else if (event.type() == event_type_names::kError &&
                 dispatch_result == DispatchEventResult::kNotCanceled) {
        transaction_->StartAborting(error_);
      }
    }

    // If this was the last request in the transaction's list, it may commit
    // here.
    if (set_transaction_active) {
      transaction_->SetActive(false);
    }
  }

  if (cursor_to_notify)
    cursor_to_notify->PostSuccessHandlerCallback();

  // An upgradeneeded event will always be followed by a success or error event,
  // so must be kept alive.
  if (ready_state_ == DONE && event.type() != event_type_names::kUpgradeneeded)
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

}  // namespace blink
