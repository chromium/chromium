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

#include "base/check.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_binding_for_modules.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_idb_request.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_idbcursor_idbindex_idbobjectstore.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_idbindex_idbobjectstore.h"
#include "third_party/blink/renderer/modules/indexed_db_names.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_any.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_cursor_with_value.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_database.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_object_store.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_request.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_transaction.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_private_property.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"

namespace blink {

namespace {

using CursorSet = HeapHashSet<WeakMember<IDBCursor>>;

CursorSet& GetGlobalCursorSet() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<Persistent<CursorSet>>,
                                  thread_specific_instance, ());
  if (!*thread_specific_instance) {
    *thread_specific_instance = MakeGarbageCollected<CursorSet>();
  }
  return **thread_specific_instance;
}

void RegisterCursor(IDBCursor* cursor) {
  CursorSet& cursor_set = GetGlobalCursorSet();
  CHECK(!cursor_set.Contains(cursor));
  cursor_set.insert(cursor);
}

}  // namespace

IDBCursor::IDBCursor(
    mojo::PendingAssociatedRemote<mojom::blink::IDBCursor> pending_cursor,
    mojom::IDBCursorDirection direction,
    IDBRequest* request,
    const Source* source,
    IDBTransaction* transaction)
    : remote_(request->GetExecutionContext()),
      request_(request),
      direction_(direction),
      source_(source),
      transaction_(transaction) {
  DCHECK(request_);
  DCHECK(source_);
  DCHECK(transaction_);
  remote_.Bind(std::move(pending_cursor),
               request_->GetExecutionContext()->GetTaskRunner(
                   TaskType::kDatabaseAccess));
  RegisterCursor(this);
}

IDBCursor::~IDBCursor() = default;

void IDBCursor::Trace(Visitor* visitor) const {
  visitor->Trace(remote_);
  visitor->Trace(request_);
  visitor->Trace(source_);
  visitor->Trace(transaction_);
  visitor->Trace(value_);
  ScriptWrappable::Trace(visitor);
}

void IDBCursor::ContextWillBeDestroyed() {
  ResetPrefetchCache();
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
        .Set(wrapper, request_->ToV8(isolate, wrapper));
  }
  return wrapper;
}

IDBRequest* IDBCursor::update(ScriptState* script_state,
                              const ScriptValue& value,
                              ExceptionState& exception_state) {
  TRACE_EVENT0("IndexedDB", "IDBCursor::updateRequestSetup");
  static const char kReadOnlyUpdateErrorMessage[] =
      "The record may not be updated inside a read-only transaction.";
  if (!CheckForCommonExceptions(exception_state, kReadOnlyUpdateErrorMessage)) {
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
  if (!CheckForCommonExceptions(exception_state, nullptr)) {
    return;
  }

  request_->SetPendingCursor(this);
  request_->AssignNewMetrics(std::move(metrics));
  got_value_ = false;

  CHECK(remote_.is_bound());
  AdvanceImpl(count, request_);
}

void IDBCursor::Continue(ScriptState* script_state,
                         const ScriptValue& key_value,
                         ExceptionState& exception_state) {
  TRACE_EVENT0("IndexedDB", "IDBCursor::continueRequestSetup");
  IDBRequest::AsyncTraceState metrics(
      IDBRequest::TypeForMetrics::kCursorContinue);

  if (!CheckForCommonExceptions(exception_state, nullptr)) {
    return;
  }

  std::unique_ptr<IDBKey> key =
      key_value.IsUndefined() || key_value.IsNull()
          ? nullptr
          : CreateIDBKeyFromValue(script_state->GetIsolate(),
                                  key_value.V8Value(), exception_state);
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

  if (!source_->IsIDBIndex()) {
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

  // Some of the checks in this helper will be redundant with those above, but
  // this is necessary to retain a specific ordering (see WPT
  // idbcursor-continuePrimaryKey-exception-order.html).
  if (!CheckForCommonExceptions(exception_state, nullptr)) {
    return;
  }

  std::unique_ptr<IDBKey> key = CreateIDBKeyFromValue(
      script_state->GetIsolate(), key_value.V8Value(), exception_state);
  if (exception_state.HadException()) {
    return;
  }
  if (!key->IsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      IDBDatabase::kNotValidKeyErrorMessage);
    return;
  }

  std::unique_ptr<IDBKey> primary_key = CreateIDBKeyFromValue(
      script_state->GetIsolate(), primary_key_value.V8Value(), exception_state);
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
  CHECK(remote_.is_bound());
  CursorContinue(key.get(), primary_key.get(), request_);
}

IDBRequest* IDBCursor::Delete(ScriptState* script_state,
                              ExceptionState& exception_state) {
  TRACE_EVENT0("IndexedDB", "IDBCursor::deleteRequestSetup");
  IDBRequest::AsyncTraceState metrics(
      IDBRequest::TypeForMetrics::kCursorDelete);
  static const char kReadOnlyDeleteErrorMessage[] =
      "The record may not be deleted inside a read-only transaction.";
  if (!CheckForCommonExceptions(exception_state, kReadOnlyDeleteErrorMessage)) {
    return nullptr;
  }

  IDBRequest* request = IDBRequest::Create(
      script_state, this, transaction_.Get(), std::move(metrics));
  transaction_->db().Delete(
      transaction_->Id(), EffectiveObjectStore()->Id(), IdbPrimaryKey(),
      WTF::BindOnce(&IDBRequest::OnDelete, WrapPersistent(request)));
  return request;
}

void IDBCursor::Close() {
  value_ = nullptr;
  request_.Clear();
  remote_.reset();
  ResetPrefetchCache();
  GetGlobalCursorSet().erase(this);
}

ScriptValue IDBCursor::key(ScriptState* script_state) {
  key_dirty_ = false;
  return ScriptValue(script_state->GetIsolate(), key_->ToV8(script_state));
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
  return ScriptValue(script_state->GetIsolate(),
                     primary_key->ToV8(script_state));
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
  return ScriptValue(script_state->GetIsolate(), value->ToV8(script_state));
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
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

bool IDBCursor::IsDeleted() const {
  switch (source_->GetContentType()) {
    case Source::ContentType::kIDBIndex:
      return source_->GetAsIDBIndex()->IsDeleted();
    case Source::ContentType::kIDBObjectStore:
      return source_->GetAsIDBObjectStore()->IsDeleted();
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

// static
mojom::blink::IDBCursorDirection IDBCursor::V8EnumToDirection(
    V8IDBCursorDirection::Enum mode) {
  switch (mode) {
    case V8IDBCursorDirection::Enum::kNext:
      return mojom::blink::IDBCursorDirection::Next;
    case V8IDBCursorDirection::Enum::kNextunique:
      return mojom::blink::IDBCursorDirection::NextNoDuplicate;
    case V8IDBCursorDirection::Enum::kPrev:
      return mojom::blink::IDBCursorDirection::Prev;
    case V8IDBCursorDirection::Enum::kPrevunique:
      return mojom::blink::IDBCursorDirection::PrevNoDuplicate;
  }
}

// static
void IDBCursor::ResetCursorPrefetchCaches(int64_t transaction_id,
                                          IDBCursor* except_cursor) {
  CursorSet& cursor_set = GetGlobalCursorSet();

  for (IDBCursor* cursor : cursor_set) {
    if (cursor != except_cursor &&
        cursor->GetTransactionId() == transaction_id) {
      cursor->ResetPrefetchCache();
    }
  }
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
      NOTREACHED_IN_MIGRATION();
      return indexed_db_names::kNext;
  }
}

void IDBCursor::AdvanceImpl(uint32_t count, IDBRequest* request) {
  if (count <= prefetch_keys_.size()) {
    CachedAdvance(count, request);
    return;
  }
  ResetPrefetchCache();

  // Reset all cursor prefetch caches except for this cursor.
  ResetCursorPrefetchCaches(transaction_->Id(), this);

  remote_->Advance(
      count, WTF::BindOnce(&IDBCursor::AdvanceCallback, WrapPersistent(this),
                           WrapWeakPersistent(request)));
}

void IDBCursor::AdvanceCallback(IDBRequest* request,
                                mojom::blink::IDBCursorResultPtr result) {
  // May be null in tests.
  if (request) {
    request->OnAdvanceCursor(std::move(result));
  }
}

void IDBCursor::CursorContinue(const IDBKey* key,
                               const IDBKey* primary_key,
                               IDBRequest* request) {
  DCHECK(key && primary_key);

  if (key->GetType() == mojom::blink::IDBKeyType::None &&
      primary_key->GetType() == mojom::blink::IDBKeyType::None) {
    // No key(s), so this would qualify for a prefetch.
    ++continue_count_;

    if (!prefetch_keys_.empty()) {
      // We have a prefetch cache, so serve the result from that.
      CachedContinue(request);
      return;
    }

    if (continue_count_ > kPrefetchContinueThreshold) {
      // Request pre-fetch.
      ++pending_onsuccess_callbacks_;

      remote_->Prefetch(
          prefetch_amount_,
          WTF::BindOnce(&IDBCursor::PrefetchCallback, WrapPersistent(this),
                        WrapWeakPersistent(request)));

      // Increase prefetch_amount_ exponentially.
      prefetch_amount_ *= 2;
      if (prefetch_amount_ > kMaxPrefetchAmount) {
        prefetch_amount_ = kMaxPrefetchAmount;
      }

      return;
    }
  } else {
    // Key argument supplied. We couldn't prefetch this.
    ResetPrefetchCache();
  }

  // Reset all cursor prefetch caches except for this cursor.
  ResetCursorPrefetchCaches(transaction_->Id(), this);
  remote_->Continue(
      IDBKey::Clone(key), IDBKey::Clone(primary_key),
      WTF::BindOnce(&IDBCursor::AdvanceCallback, WrapPersistent(this),
                    WrapWeakPersistent(request)));
}

void IDBCursor::PrefetchCallback(IDBRequest* request,
                                 mojom::blink::IDBCursorResultPtr result) {
  if (!result->is_error_result() && !result->is_empty() &&
      result->get_values()->keys.size() ==
          result->get_values()->primary_keys.size() &&
      result->get_values()->keys.size() ==
          result->get_values()->values.size()) {
    SetPrefetchData(std::move(result->get_values()->keys),
                    std::move(result->get_values()->primary_keys),
                    std::move(result->get_values()->values));
    CachedContinue(request);
  } else if (request) {
    // This is the error case. We want error handling to match the AdvanceCursor
    // case.
    request->OnAdvanceCursor(std::move(result));
  }
}

void IDBCursor::PostSuccessHandlerCallback() {
  pending_onsuccess_callbacks_--;

  // If the onsuccess callback called continue()/advance() on the cursor
  // again, and that request was served by the prefetch cache, then
  // pending_onsuccess_callbacks_ would be incremented. If not, it means the
  // callback did something else, or nothing at all, in which case we need to
  // reset the cache.

  if (pending_onsuccess_callbacks_ == 0) {
    ResetPrefetchCache();
  }
}

void IDBCursor::SetPrefetchData(Vector<std::unique_ptr<IDBKey>> keys,
                                Vector<std::unique_ptr<IDBKey>> primary_keys,
                                Vector<std::unique_ptr<IDBValue>> values) {
  // Keys and values are stored in reverse order so that a cache'd continue can
  // pop a value off of the back and prevent new memory allocations.
  prefetch_keys_.AppendRange(std::make_move_iterator(keys.rbegin()),
                             std::make_move_iterator(keys.rend()));
  prefetch_primary_keys_.AppendRange(
      std::make_move_iterator(primary_keys.rbegin()),
      std::make_move_iterator(primary_keys.rend()));
  prefetch_values_.AppendRange(std::make_move_iterator(values.rbegin()),
                               std::make_move_iterator(values.rend()));

  used_prefetches_ = 0;
  pending_onsuccess_callbacks_ = 0;
}

void IDBCursor::CachedAdvance(uint32_t count, IDBRequest* request) {
  DCHECK_GE(prefetch_keys_.size(), count);
  DCHECK_EQ(prefetch_primary_keys_.size(), prefetch_keys_.size());
  DCHECK_EQ(prefetch_values_.size(), prefetch_keys_.size());

  while (count > 1) {
    prefetch_keys_.pop_back();
    prefetch_primary_keys_.pop_back();
    prefetch_values_.pop_back();
    ++used_prefetches_;
    --count;
  }

  CachedContinue(request);
}

void IDBCursor::CachedContinue(IDBRequest* request) {
  DCHECK_GT(prefetch_keys_.size(), 0ul);
  DCHECK_EQ(prefetch_primary_keys_.size(), prefetch_keys_.size());
  DCHECK_EQ(prefetch_values_.size(), prefetch_keys_.size());

  // Keys and values are stored in reverse order so that a cache'd continue can
  // pop a value off of the back and prevent new memory allocations.
  std::unique_ptr<IDBKey> key = std::move(prefetch_keys_.back());
  std::unique_ptr<IDBKey> primary_key =
      std::move(prefetch_primary_keys_.back());
  std::unique_ptr<IDBValue> value = std::move(prefetch_values_.back());

  prefetch_keys_.pop_back();
  prefetch_primary_keys_.pop_back();
  prefetch_values_.pop_back();
  ++used_prefetches_;

  ++pending_onsuccess_callbacks_;

  if (!continue_count_) {
    // The cache was invalidated by a call to ResetPrefetchCache()
    // after the RequestIDBCursorPrefetch() was made. Now that the
    // initiating continue() call has been satisfied, discard
    // the rest of the cache.
    ResetPrefetchCache();
  }

  // May be null in tests.
  if (request) {
    // Since the cached request is not round tripping through the browser
    // process, the request has to be explicitly queued. See step 11 of
    // https://www.w3.org/TR/IndexedDB/#dom-idbcursor-continue
    // This is prevented from becoming out-of-order with other requests that
    // do travel through the browser process by the fact that any previous
    // request currently making its way through the browser would have already
    // cleared this cache via `ResetCursorPrefetchCaches()`.
    request->GetExecutionContext()
        ->GetTaskRunner(TaskType::kDatabaseAccess)
        ->PostTask(FROM_HERE,
                   WTF::BindOnce(&IDBRequest::HandleResponseAdvanceCursor,
                                 WrapWeakPersistent(request), std::move(key),
                                 std::move(primary_key), std::move(value)));
  }
}

void IDBCursor::ResetPrefetchCache() {
  continue_count_ = 0;
  prefetch_amount_ = kMinPrefetchAmount;

  if (prefetch_keys_.empty()) {
    // No prefetch cache, so no need to reset the cursor in the back-end.
    return;
  }

  // Reset the back-end cursor.
  if (remote_.is_bound()) {
    remote_->PrefetchReset(used_prefetches_);
  }

  // Reset the prefetch cache.
  prefetch_keys_.clear();
  prefetch_primary_keys_.clear();
  prefetch_values_.clear();

  pending_onsuccess_callbacks_ = 0;
}

int64_t IDBCursor::GetTransactionId() const {
  return transaction_->Id();
}

bool IDBCursor::CheckForCommonExceptions(ExceptionState& exception_state,
                                         const char* read_only_error_message) {
  if (!transaction_->IsActive()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kTransactionInactiveError,
        transaction_->InactiveErrorMessage());
    return false;
  }
  if (read_only_error_message && transaction_->IsReadOnly()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kReadOnlyError,
                                      read_only_error_message);
    return false;
  }
  if (IsDeleted()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kSourceDeletedErrorMessage);
    return false;
  }
  if (!got_value_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kNoValueErrorMessage);
    return false;
  }
  if (read_only_error_message && IsKeyCursor()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kIsKeyCursorErrorMessage);
    return false;
  }
  ExecutionContext* context =
      request_ ? request_->GetExecutionContext() : nullptr;
  if (!context || context->IsContextDestroyed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kDatabaseClosedErrorMessage);
    return false;
  }

  return true;
}

}  // namespace blink
