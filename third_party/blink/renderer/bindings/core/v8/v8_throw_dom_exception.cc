// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"

#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

// extern
const V8PrivateProperty::SymbolKey kPrivatePropertyDOMExceptionError;

// static
void V8ThrowDOMException::Init() {
  ExceptionState::SetCreateDOMExceptionFunction(
      V8ThrowDOMException::CreateOrEmpty);
}

namespace {

void DomExceptionStackGetter(v8::Local<v8::Name> name,
                             const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Value> value;
  if (info.Data()
          .As<v8::Object>()
          ->Get(isolate->GetCurrentContext(), V8AtomicString(isolate, "stack"))
          .ToLocal(&value))
    V8SetReturnValue(info, value);
}

void DomExceptionStackSetter(v8::Local<v8::Name> name,
                             v8::Local<v8::Value> value,
                             const v8::PropertyCallbackInfo<void>& info) {
  v8::Maybe<bool> unused = info.Data().As<v8::Object>()->Set(
      info.GetIsolate()->GetCurrentContext(),
      V8AtomicString(info.GetIsolate(), "stack"), value);
  ALLOW_UNUSED_LOCAL(unused);
}

}  // namespace

v8::Local<v8::Value> V8ThrowDOMException::CreateOrEmpty(
    v8::Isolate* isolate,
    DOMExceptionCode exception_code,
    const String& sanitized_message,
    const String& unsanitized_message) {
  DCHECK(IsDOMExceptionCode(ToExceptionCode(exception_code)));
  DCHECK(exception_code == DOMExceptionCode::kSecurityError ||
         unsanitized_message.IsNull());

  if (isolate->IsExecutionTerminating())
    return v8::Local<v8::Value>();

  auto* dom_exception = MakeGarbageCollected<DOMException>(
      exception_code, sanitized_message, unsanitized_message);
  v8::Local<v8::Object> exception_obj =
      ToV8(dom_exception, isolate->GetCurrentContext()->Global(), isolate)
          .As<v8::Object>();
  // Attach an Error object to the DOMException. This is then lazily used to
  // get the stack value.
  v8::Local<v8::Value> error =
      v8::Exception::Error(V8String(isolate, dom_exception->message()));
  exception_obj
      ->SetAccessor(isolate->GetCurrentContext(),
                    V8AtomicString(isolate, "stack"), DomExceptionStackGetter,
                    DomExceptionStackSetter, error)
      .ToChecked();

  auto private_error =
      V8PrivateProperty::GetSymbol(isolate, kPrivatePropertyDOMExceptionError);
  private_error.Set(exception_obj, error);

  return exception_obj;
}

}  // namespace blink
