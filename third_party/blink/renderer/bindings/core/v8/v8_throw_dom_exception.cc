// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"

#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_set_return_value.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

// static
void V8ThrowDOMException::Init() {
  ExceptionState::SetCreateDOMExceptionFunction(
      V8ThrowDOMException::CreateOrEmpty);
}

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
  return AttachStackProperty(isolate, dom_exception);
}

v8::Local<v8::Value> V8ThrowDOMException::CreateOrDie(
    v8::Isolate* isolate,
    DOMExceptionCode exception_code,
    const String& sanitized_message,
    const String& unsanitized_message) {
  v8::Local<v8::Value> v8_value = CreateOrEmpty(
      isolate, exception_code, sanitized_message, unsanitized_message);
  CHECK(!v8_value.IsEmpty());
  return v8_value;
}

void V8ThrowDOMException::Throw(v8::Isolate* isolate,
                                DOMExceptionCode exception_code,
                                const String& sanitized_message,
                                const String& unsanitized_message) {
  v8::Local<v8::Value> v8_value = CreateOrEmpty(
      isolate, exception_code, sanitized_message, unsanitized_message);
  if (!v8_value.IsEmpty()) {
    V8ThrowException::ThrowException(isolate, v8_value);
  }
}

v8::Local<v8::Value> V8ThrowDOMException::AttachStackProperty(
    v8::Isolate* isolate,
    DOMException* dom_exception) {
  if (isolate->IsExecutionTerminating())
    return v8::Local<v8::Value>();

  // We use the isolate's current context here because we are creating an
  // exception object.
  auto current_context = isolate->GetCurrentContext();
  v8::Local<v8::Object> exception_obj =
      dom_exception->ToV8(ScriptState::From(isolate, current_context))
          .As<v8::Object>();
  v8::Exception::CaptureStackTrace(current_context, exception_obj);
  return exception_obj;
}

}  // namespace blink
