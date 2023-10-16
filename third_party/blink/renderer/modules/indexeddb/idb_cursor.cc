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

#include "third_party/blink/renderer/modules/indexeddb/idb_cursor.h"

#include <limits>
#include <memory>
#include <utility>

#include "third_party/blink/renderer/bindings/modules/v8/to_v8_for_modules.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_binding_for_modules.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_idb_request.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_idbcursor_idbindex_idbobjectstore.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_idbindex_idbobjectstore.h"
#include "third_party/blink/renderer/modules/indexed_db_names.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_any.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_cursor_with_value.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_database.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_object_store.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_transaction.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_database.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_private_property.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

IDBCursor::IDBCursor(std::unique_ptr<WebIDBCursor> backend,
                     mojom::IDBCursorDirection direction,
                     IDBRequest* request,
                     const Source* source,
                     IDBTransaction* transaction)
    : backend_(std::move(backend)),
      request_(request),
      direction_(direction),
      source_(source),
      transaction_(transaction) {
  DCHECK(backend_);
  DCHECK(request_);
  DCHECK(source_);
  DCHECK(transaction_);
}

IDBCursor::~IDBCursor() = default;

void IDBCursor::Trace(Visitor* visitor) const {
  visitor->Trace(request_);
  visitor->Trace(source_);
  visitor->Trace(transaction_);
  visitor->Trace(value_);
  ScriptWrappable::Trace(visitor);
}

// Keep the request's wrapper alive as long as the cursor's wrapper is alive,
// so that the same script object is seen each time the cursor is used.
v8::Local<v8::Object> IDBCursor::AssociateWithWrapper(
    v8::Isolate* isolate,
    const WrapperTypeInfo* wrapper_type,
    v8::Local<v8::Object> wrapper) {
  wrapper =
      ScriptWrappable::AssociateWithWrapper(isolate, wrapper_type, wrapper);
  if (!wrapper.IsEmpty()) {
    static const V8PrivateProperty::SymbolKey kPrivatePropertyRequest;
    V8PrivateProperty::GetSymbol(isolate, kPrivatePropertyRequest)
        .Set(wrapper, ToV8(request_.Get(), wrapper, isolate));
  }
  return wrapper;
}

IDBRequest* IDBCursor::update(ScriptState* script_state,
                              const ScriptValue& value,
                              ExceptionState& exception_state) {
  TRACE_EVENT0("IndexedDB", "IDBCursor::updateRequestSetup");
  if (!transaction_->IsActive()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kTransactionInactiveError,
        transaction_->InactiveErrorMessage());
    return nullptr;
  }
  if (transaction_->IsReadOnly()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kReadOnlyError,
        "The record may not be updated inside a read-only transaction.");
    return nullptr;
  }
  if (IsDeleted()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kSourceDeletedErrorMessage);
    return nullptr;
  }
  if (!got_value_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kNoValueErrorMessage);
    return nullptr;
  }
  if (IsKeyCursor()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kIsKeyCursorErrorMessage);
    return nullptr;
  }

  IDBObjectStore* object_store = EffectiveObjectStore();
  return object_store->DoPut(script_state, mojom::IDBPutMode::CursorUpdate,
                             MakeGarbageCollected<IDBRequest::Source>(this),
                             value, IdbPrimaryKey(), exception_state);
}

void IDBCursor::advance(unsigned count, ExceptionState& exception_state) {
  TRACE_EVENT0("IndexedDB", "IDBCursor::advanceRequestSetup");
  IDBRequest::AsyncTraceState metrics(
      IDBRequest::TypeForMetrics::kCursorAdvance);
  if (!count) {
    exception_state.ThrowTypeError(
        "A count argument with value 0 (zero) was supplied, must be greater "
        "than 0.");
    return;
  }
  if (!transaction_->IsActive()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kTransactionInactiveError,
        transaction_->InactiveErrorMessage());
    return;
  }
  if (IsDeleted()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kSourceDeletedErrorMessage);
    return;
  }
  if (!got_value_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kNoValueErrorMessage);
    return;
  }

  request_->SetPendingCursor(this);
  request_->AssignNewMetrics(std::move(metrics));
  got_value_ = false;
  backend_->Advance(count, request_);
}

void IDBCursor::Continue(ScriptState* script_state,
                         const ScriptValue& key_value,
                         ExceptionState& exception_state) {
  TRACE_EVENT0("IndexedDB", "IDBCursor::continueRequestSetup");
  IDBRequest::AsyncTraceState metrics(
      IDBRequest::TypeForMetrics::kCursorContinue);

  if (!transaction_->IsActive()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kTransactionInactiveError,
        transaction_->InactiveErrorMessage());
    return;
  }
  if (!got_value_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kNoValueErrorMessage);
    return;
  }
  if (IsDeleted()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kSourceDeletedErrorMessage);
    return;
  }

  std::unique_ptr<IDBKey> key =
      key_value.IsUndefined() || key_value.IsNull()
          ? nullptr
          : ScriptValue::To<std::unique_ptr<IDBKey>>(
                script_state->GetIsolate(), key_value, exception_state);
  if (exception_state.HadException())
    return;
  if (key && !key->IsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      IDBDatabase::kNotValidKeyErrorMessage);
    return;
  }
  Continue(std::move(key), nullptr, std::move(metrics), exception_state);
}

void IDBCursor::continuePrimaryKey(ScriptState* script_state,
                                   const ScriptValue& key_value,
                                   const ScriptValue& primary_key_value,
                                   ExceptionState& exception_state) {
  TRACE_EVENT0("IndexedDB", "IDBCursor::continuePrimaryKeyRequestSetup");
  IDBRequest::AsyncTraceState metrics(
      IDBRequest::TypeForMetrics::kCursorContinuePrimaryKey);

  if (!transaction_->IsActive()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kTransactionInactiveError,
        transaction_->InactiveErrorMessage());
    return;
  }

  if (IsDeleted()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kSourceDeletedErrorMessage);
    return;
  }

  if (
      !source_->IsIDBIndex()
  ) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The cursor's source is not an index.");
    return;
  }

  if (direction_ != mojom::IDBCursorDirection::Next &&
      direction_ != mojom::IDBCursorDirection::Prev) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "The cursor's direction is not 'next' or 'prev'.");
    return;
  }

  if (!got_value_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kNoValueErrorMessage);
    return;
  }

  std::unique_ptr<IDBKey> key = ScriptValue::To<std::unique_ptr<IDBKey>>(
      script_state->GetIsolate(), key_value, exception_state);
  if (exception_state.HadException())
    return;
  if (!key->IsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      IDBDatabase::kNotValidKeyErrorMessage);
    return;
  }

  std::unique_ptr<IDBKey> primary_key =
      ScriptValue::To<std::unique_ptr<IDBKey>>(
          script_state->GetIsolate(), primary_key_value, exception_state);
  if (exception_state.HadException())
    return;
  if (!primary_key->IsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      IDBDatabase::kNotValidKeyErrorMessage);
    return;
  }

  Continue(std::move(key), std::move(primary_key), std::move(metrics),
           exception_state);
}

void IDBCursor::Continue(std::unique_ptr<IDBKey> key,
                         std::unique_ptr<IDBKey> primary_key,
                         IDBRequest::AsyncTraceState metrics,
                         ExceptionState& exception_state) {
  DCHECK(transaction_->IsActive());
  DCHECK(got_value_);
  DCHECK(!IsDeleted());
  DCHECK(!primary_key || (key && primary_key));

  const IDBKey* current_primary_key = IdbPrimaryKey();

  if (!key)
    key = IDBKey::CreateNone();

  if (key->GetType() != mojom::IDBKeyType::None) {
    DCHECK(key_);
    if (direction_ == mojom::IDBCursorDirection::Next ||
        direction_ == mojom::IDBCursorDirection::NextNoDuplicate) {
      const bool ok = key_->IsLessThan(key.get()) ||
                      (primary_key && key_->IsEqual(key.get()) &&
                       current_primary_key->IsLessThan(primary_key.get()));
      if (!ok) {
        exception_state.ThrowDOMException(
            DOMExceptionCode::kDataError,
            "The parameter is less than or equal to this cursor's position.");
        return;
      }

    } else {
      const bool ok = key->IsLessThan(key_.get()) ||
                      (primary_key && key->IsEqual(key_.get()) &&
                       primary_key->IsLessThan(current_primary_key));
      if (!ok) {
        exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                          "The parameter is greater than or "
                                          "equal to this cursor's position.");
        return;
      }
    }
  }

  if (!primary_key)
    primary_key = IDBKey::CreateNone();

  // FIXME: We're not using the context from when continue was called, which
  // means the callback will be on the original context openCursor was called
  // on. Is this right?
  request_->SetPendingCursor(this);
  request_->AssignNewMetrics(std::move(metrics));
  got_value_ = false;
  backend_->CursorContinue(key.get(), primary_key.get(), request_);
}

IDBRequest* IDBCursor::Delete(ScriptState* script_state,
                              ExceptionState& exception_state) {
  TRACE_EVENT0("IndexedDB", "IDBCursor::deleteRequestSetup");
  IDBRequest::AsyncTraceState metrics(
      IDBRequest::TypeForMetrics::kCursorDelete);
  if (!transaction_->IsActive()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kTransactionInactiveError,
        transaction_->InactiveErrorMessage());
    return nullptr;
  }
  if (transaction_->IsReadOnly()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kReadOnlyError,
        "The record may not be deleted inside a read-only transaction.");
    return nullptr;
  }
  if (IsDeleted()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kSourceDeletedErrorMessage);
    return nullptr;
  }
  if (!got_value_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kNoValueErrorMessage);
    return nullptr;
  }
  if (IsKeyCursor()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kIsKeyCursorErrorMessage);
    return nullptr;
  }
  if (!transaction_->BackendDB()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kDatabaseClosedErrorMessage);
    return nullptr;
  }

  IDBRequest* request = IDBRequest::Create(
      script_state, this, transaction_.Get(), std::move(metrics));
  transaction_->BackendDB()->Delete(
      transaction_->Id(), EffectiveObjectStore()->Id(), IdbPrimaryKey(),
      WTF::BindOnce(&IDBRequest::OnDelete, WrapPersistent(request)));
  return request;
}

void IDBCursor::PostSuccessHandlerCallback() {
  if (backend_)
    backend_->PostSuccessHandlerCallback();
}

void IDBCursor::Close() {
  value_ = nullptr;
  request_.Clear();
  backend_.reset();
}

ScriptValue IDBCursor::key(ScriptState* script_state) {
  key_dirty_ = false;
  return ScriptValue::From(script_state, key_.get());
}

ScriptValue IDBCursor::primaryKey(ScriptState* script_state) {
  primary_key_dirty_ = false;
  const IDBKey* primary_key = primary_key_unless_injected_.get();
  if (!primary_key) {
#if DCHECK_IS_ON()
    DCHECK(value_has_injected_primary_key_);

    IDBObjectStore* object_store = EffectiveObjectStore();
    DCHECK(object_store->autoIncrement() &&
           !object_store->IdbKeyPath().IsNull());
#endif  // DCHECK_IS_ON()

    primary_key = value_->Value()->PrimaryKey();
  }
  return ScriptValue::From(script_state, primary_key);
}

ScriptValue IDBCursor::value(ScriptState* script_state) {
  DCHECK(IsA<IDBCursorWithValue>(this));

  IDBAny* value;
  if (value_) {
    value = value_;
#if DCHECK_IS_ON()
    if (value_has_injected_primary_key_) {
      IDBObjectStore* object_store = EffectiveObjectStore();
      DCHECK(object_store->autoIncrement() &&
             !object_store->IdbKeyPath().IsNull());
      AssertPrimaryKeyValidOrInjectable(script_state, value_->Value());
    }
#endif  // DCHECK_IS_ON()

  } else {
    value = MakeGarbageCollected<IDBAny>(IDBAny::kUndefinedType);
  }

  value_dirty_ = false;
  ScriptValue script_value = ScriptValue::From(script_state, value);
  return script_value;
}

const IDBCursor::Source* IDBCursor::source() const {
  return source_.Get();
}

void IDBCursor::SetValueReady(std::unique_ptr<IDBKey> key,
                              std::unique_ptr<IDBKey> primary_key,
                              std::unique_ptr<IDBValue> value) {
  key_ = std::move(key);
  key_dirty_ = true;

  primary_key_unless_injected_ = std::move(primary_key);
  primary_key_dirty_ = true;

  got_value_ = true;

  if (!IsA<IDBCursorWithValue>(this))
    return;

  value_dirty_ = true;
#if DCHECK_IS_ON()
  value_has_injected_primary_key_ = false;
#endif  // DCHECK_IS_ON()

  if (!value) {
    value_ = nullptr;
    return;
  }

  IDBObjectStore* object_store = EffectiveObjectStore();
  if (object_store->autoIncrement() && !object_store->IdbKeyPath().IsNull()) {
    value->SetInjectedPrimaryKey(std::move(primary_key_unless_injected_),
                                 object_store->IdbKeyPath());
#if DCHECK_IS_ON()
    value_has_injected_primary_key_ = true;
#endif  // DCHECK_IS_ON()
  }

  value_ = MakeGarbageCollected<IDBAny>(std::move(value));
}

const IDBKey* IDBCursor::IdbPrimaryKey() const {
  if (primary_key_unless_injected_ || !value_)
    return primary_key_unless_injected_.get();

#if DCHECK_IS_ON()
  DCHECK(value_has_injected_primary_key_);
#endif  // DCHECK_IS_ON()
  return value_->Value()->PrimaryKey();
}

IDBObjectStore* IDBCursor::EffectiveObjectStore() const {
  switch (source_->GetContentType()) {
    case Source::ContentType::kIDBIndex:
      return source_->GetAsIDBIndex()->objectStore();
    case Source::ContentType::kIDBObjectStore:
      return source_->GetAsIDBObjectStore();
  }
  NOTREACHED();
  return nullptr;
}

bool IDBCursor::IsDeleted() const {
  switch (source_->GetContentType()) {
    case Source::ContentType::kIDBIndex:
      return source_->GetAsIDBIndex()->IsDeleted();
    case Source::ContentType::kIDBObjectStore:
      return source_->GetAsIDBObjectStore()->IsDeleted();
  }
  NOTREACHED();
  return false;
}

mojom::IDBCursorDirection IDBCursor::StringToDirection(
    const String& direction_string) {
  if (direction_string == indexed_db_names::kNext)
    return mojom::IDBCursorDirection::Next;
  if (direction_string == indexed_db_names::kNextunique)
    return mojom::IDBCursorDirection::NextNoDuplicate;
  if (direction_string == indexed_db_names::kPrev)
    return mojom::IDBCursorDirection::Prev;
  if (direction_string == indexed_db_names::kPrevunique)
    return mojom::IDBCursorDirection::PrevNoDuplicate;

  NOTREACHED();
  return mojom::IDBCursorDirection::Next;
}

const String& IDBCursor::direction() const {
  switch (direction_) {
    case mojom::IDBCursorDirection::Next:
      return indexed_db_names::kNext;

    case mojom::IDBCursorDirection::NextNoDuplicate:
      return indexed_db_names::kNextunique;

    case mojom::IDBCursorDirection::Prev:
      return indexed_db_names::kPrev;

    case mojom::IDBCursorDirection::PrevNoDuplicate:
      return indexed_db_names::kPrevunique;

    default:
      NOTREACHED();
      return indexed_db_names::kNext;
  }
}

}  // namespace blink
