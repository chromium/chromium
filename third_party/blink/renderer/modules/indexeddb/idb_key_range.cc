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

#include "third_party/blink/renderer/modules/indexeddb/idb_key_range.h"

#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_binding_for_modules.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_idb_key_range.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_database.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

IDBKeyRange* IDBKeyRange::FromScriptValue(ExecutionContext* context,
                                          const ScriptValue& value,
                                          ExceptionState& exception_state) {
  if (value.IsUndefined() || value.IsNull())
    return nullptr;

  IDBKeyRange* const range =
      V8IDBKeyRange::ToWrappable(context->GetIsolate(), value.V8Value());
  if (range)
    return range;

  std::unique_ptr<IDBKey> key = CreateIDBKeyFromValue(
      context->GetIsolate(), value.V8Value(), exception_state);
  if (exception_state.HadException())
    return nullptr;
  if (!key || !key->IsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      IDBDatabase::kNotValidKeyErrorMessage);
    return nullptr;
  }

  IDBKey* const upper_compressed = key.get();
  return MakeGarbageCollected<IDBKeyRange>(std::move(key), upper_compressed,
                                           nullptr, kLowerBoundClosed,
                                           kUpperBoundClosed);
}

IDBKeyRange::IDBKeyRange(std::unique_ptr<IDBKey> lower,
                         IDBKey* upper,
                         std::unique_ptr<IDBKey> upper_if_distinct,
                         LowerBoundType lower_type,
                         UpperBoundType upper_type)
    : lower_(std::move(lower)),
      upper_if_distinct_(std::move(upper_if_distinct)),
      upper_(upper),
      lower_type_(lower_type),
      upper_type_(upper_type) {
  DCHECK(!upper_if_distinct_ || upper == upper_if_distinct_.get())
      << "In the normal representation, upper must point to upper_if_distinct.";
  DCHECK(upper != lower.get() || !upper_if_distinct_)
      << "In the compressed representation, upper_if_distinct_ must be null.";
  DCHECK(lower_ || lower_type_ == kLowerBoundOpen);
  DCHECK(upper_ || upper_type_ == kUpperBoundOpen);
}

ScriptValue IDBKeyRange::LowerValue(ScriptState* script_state) const {
  if (auto* lower = Lower()) {
    return ScriptValue(script_state->GetIsolate(), lower->ToV8(script_state));
  }
  return ScriptValue();
}

ScriptValue IDBKeyRange::UpperValue(ScriptState* script_state) const {
  if (auto* upper = Upper()) {
    return ScriptValue(script_state->GetIsolate(), upper->ToV8(script_state));
  }
  return ScriptValue();
}

IDBKeyRange* IDBKeyRange::only(std::unique_ptr<IDBKey> key,
                               ExceptionState& exception_state) {
  if (!key || !key->IsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      IDBDatabase::kNotValidKeyErrorMessage);
    return nullptr;
  }

  IDBKey* const upper_compressed = key.get();
  return MakeGarbageCollected<IDBKeyRange>(std::move(key), upper_compressed,
                                           nullptr, kLowerBoundClosed,
                                           kUpperBoundClosed);
}

IDBKeyRange* IDBKeyRange::only(ScriptState* script_state,
                               const ScriptValue& key_value,
                               ExceptionState& exception_state) {
  std::unique_ptr<IDBKey> key = CreateIDBKeyFromValue(
      script_state->GetIsolate(), key_value.V8Value(), exception_state);
  if (exception_state.HadException())
    return nullptr;
  if (!key || !key->IsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      IDBDatabase::kNotValidKeyErrorMessage);
    return nullptr;
  }

  IDBKey* const upper_compressed = key.get();
  return MakeGarbageCollected<IDBKeyRange>(std::move(key), upper_compressed,
                                           nullptr, kLowerBoundClosed,
                                           kUpperBoundClosed);
}

IDBKeyRange* IDBKeyRange::lowerBound(ScriptState* script_state,
                                     const ScriptValue& bound_value,
                                     bool open,
                                     ExceptionState& exception_state) {
  std::unique_ptr<IDBKey> bound =
      CreateIDBKeyFromValue(ExecutionContext::From(script_state)->GetIsolate(),
                            bound_value.V8Value(), exception_state);
  if (exception_state.HadException())
    return nullptr;
  if (!bound || !bound->IsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      IDBDatabase::kNotValidKeyErrorMessage);
    return nullptr;
  }

  return IDBKeyRange::Create(std::move(bound), nullptr,
                             open ? kLowerBoundOpen : kLowerBoundClosed,
                             kUpperBoundOpen);
}

IDBKeyRange* IDBKeyRange::upperBound(ScriptState* script_state,
                                     const ScriptValue& bound_value,
                                     bool open,
                                     ExceptionState& exception_state) {
  std::unique_ptr<IDBKey> bound =
      CreateIDBKeyFromValue(ExecutionContext::From(script_state)->GetIsolate(),
                            bound_value.V8Value(), exception_state);
  if (exception_state.HadException())
    return nullptr;
  if (!bound || !bound->IsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      IDBDatabase::kNotValidKeyErrorMessage);
    return nullptr;
  }

  return IDBKeyRange::Create(nullptr, std::move(bound), kLowerBoundOpen,
                             open ? kUpperBoundOpen : kUpperBoundClosed);
}

IDBKeyRange* IDBKeyRange::bound(ScriptState* script_state,
                                const ScriptValue& lower_value,
                                const ScriptValue& upper_value,
                                bool lower_open,
                                bool upper_open,
                                ExceptionState& exception_state) {
  std::unique_ptr<IDBKey> lower =
      CreateIDBKeyFromValue(ExecutionContext::From(script_state)->GetIsolate(),
                            lower_value.V8Value(), exception_state);
  if (exception_state.HadException())
    return nullptr;
  if (!lower || !lower->IsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      IDBDatabase::kNotValidKeyErrorMessage);
    return nullptr;
  }

  std::unique_ptr<IDBKey> upper =
      CreateIDBKeyFromValue(ExecutionContext::From(script_state)->GetIsolate(),
                            upper_value.V8Value(), exception_state);

  if (exception_state.HadException())
    return nullptr;
  if (!upper || !upper->IsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      IDBDatabase::kNotValidKeyErrorMessage);
    return nullptr;
  }

  if (upper->IsLessThan(lower.get())) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        "The lower key is greater than the upper key.");
    return nullptr;
  }
  if (upper->IsEqual(lower.get()) && (lower_open || upper_open)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        "The lower key and upper key are equal and one of the bounds is open.");
    return nullptr;
  }

  // This always builds a normal representation. We could save a tiny bit of
  // memory by building a compressed representation if the two keys are equal,
  // but this seems rare, so it's not worth the extra code size.
  return IDBKeyRange::Create(std::move(lower), std::move(upper),
                             lower_open ? kLowerBoundOpen : kLowerBoundClosed,
                             upper_open ? kUpperBoundOpen : kUpperBoundClosed);
}

bool IDBKeyRange::includes(ScriptState* script_state,
                           const ScriptValue& key_value,
                           ExceptionState& exception_state) {
  std::unique_ptr<IDBKey> key =
      CreateIDBKeyFromValue(ExecutionContext::From(script_state)->GetIsolate(),
                            key_value.V8Value(), exception_state);
  if (exception_state.HadException())
    return false;
  if (!key || !key->IsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      IDBDatabase::kNotValidKeyErrorMessage);
    return false;
  }

  if (lower_) {
    const int compared_with_lower = key->Compare(lower_.get());
    if (compared_with_lower < 0 || (compared_with_lower == 0 && lowerOpen()))
      return false;
  }

  if (upper_) {
    const int compared_with_upper = key->Compare(upper_);
    if (compared_with_upper > 0 || (compared_with_upper == 0 && upperOpen()))
      return false;
  }

  return true;
}

}  // namespace blink
