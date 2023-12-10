// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/no_alloc_direct_call_exception_state.h"

#include "base/notreached.h"

namespace blink {

void NoAllocDirectCallExceptionState::ClearException() {
  ExceptionState::ClearException();
  deferred_exception_.Reset();
}

void NoAllocDirectCallExceptionState::DoThrowDOMException(
    DOMExceptionCode code,
    const String& message) {
  deferred_exception_ = WTF::BindOnce(
      [](v8::Isolate* isolate, ExceptionContext&& exception_context,
         DOMExceptionCode code, const String& message) {
        ExceptionState exception_state(isolate, std::move(exception_context));
        exception_state.ThrowDOMException(code, message);
      },
      WTF::Unretained(GetIsolate()), GetContext(), code, message);
  SetExceptionCode(ToExceptionCode(code));
}

void NoAllocDirectCallExceptionState::DoThrowTypeError(const String& message) {
  deferred_exception_ = WTF::BindOnce(
      [](v8::Isolate* isolate, ExceptionContext&& exception_context,
         const String& message) {
        ExceptionState exception_state(isolate, std::move(exception_context));
        exception_state.ThrowTypeError(message);
      },
      WTF::Unretained(GetIsolate()), GetContext(), message);
  SetExceptionCode(ToExceptionCode(ESErrorType::kTypeError));
}

void NoAllocDirectCallExceptionState::DoThrowSecurityError(
    const String& sanitized_message,
    const String& unsanitized_message) {
  deferred_exception_ = WTF::BindOnce(
      [](v8::Isolate* isolate, ExceptionContext&& exception_context,
         const String& sanitized_message, const String& unsanitized_message) {
        ExceptionState exception_state(isolate, std::move(exception_context));
        exception_state.ThrowSecurityError(sanitized_message,
                                           unsanitized_message);
      },
      WTF::Unretained(GetIsolate()), GetContext(), sanitized_message,
      unsanitized_message);
  SetExceptionCode(ToExceptionCode(DOMExceptionCode::kSecurityError));
}

void NoAllocDirectCallExceptionState::DoThrowRangeError(const String& message) {
  deferred_exception_ = WTF::BindOnce(
      [](v8::Isolate* isolate, ExceptionContext&& exception_context,
         const String& message) {
        ExceptionState exception_state(isolate, std::move(exception_context));
        exception_state.ThrowRangeError(message);
      },
      WTF::Unretained(GetIsolate()), GetContext(), message);
  SetExceptionCode(ToExceptionCode(ESErrorType::kRangeError));
}

void NoAllocDirectCallExceptionState::DoThrowWasmCompileError(
    const String& message) {
  NOTREACHED_NORETURN();
}

void NoAllocDirectCallExceptionState::DoRethrowV8Exception(
    v8::Local<v8::Value>) {
  NOTREACHED_NORETURN();
}

}  // namespace blink
