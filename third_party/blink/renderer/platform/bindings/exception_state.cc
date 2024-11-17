/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/bindings/exception_state.h"

#include "base/check.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

ExceptionState::CreateDOMExceptionFunction
    ExceptionState::s_create_dom_exception_func_ = nullptr;

// static
void ExceptionState::SetCreateDOMExceptionFunction(
    CreateDOMExceptionFunction func) {
  DCHECK(!s_create_dom_exception_func_);
  s_create_dom_exception_func_ = func;
  DCHECK(s_create_dom_exception_func_);
}

NOINLINE void ExceptionState::ThrowSecurityError(
    const char* sanitized_message,
    const char* unsanitized_message) {
  ThrowSecurityError(String(sanitized_message), String(unsanitized_message));
}

NOINLINE void ExceptionState::ThrowRangeError(const char* message) {
  ThrowRangeError(String(message));
}

NOINLINE void ExceptionState::ThrowTypeError(const char* message) {
  ThrowTypeError(String(message));
}

NOINLINE void ExceptionState::ThrowWasmCompileError(const char* message) {
  ThrowWasmCompileError(String(message));
}

NOINLINE void ExceptionState::ThrowDOMException(DOMExceptionCode exception_code,
                                                const char* message) {
  ThrowDOMException(exception_code, String(message));
}

void ExceptionState::SetExceptionInfo(ExceptionCode exception_code,
                                      const String& message) {
  had_exception_ = true;
  if (!swallow_all_exceptions_) {
    return;
  }
  CHECK(exception_code);
  // `swallow_all_exceptions_` is only set to true in the delegated constructor
  // for `DummyExceptionStateForTesting`, so this static_cast is safe.
  auto* dummy_this = static_cast<DummyExceptionStateForTesting*>(this);
  dummy_this->code_ = exception_code;
  dummy_this->message_ = message;
}

void ExceptionState::ThrowDOMException(DOMExceptionCode exception_code,
                                       const String& message) {
  // SecurityError is thrown via ThrowSecurityError, and _careful_ consideration
  // must be given to the data exposed to JavaScript via |sanitized_message|.
  DCHECK_NE(exception_code, DOMExceptionCode::kSecurityError);
#if DCHECK_IS_ON()
  DCHECK_AT(!assert_no_exceptions_, file_, line_)
      << "DOMException should not be thrown.";
#endif

  SetExceptionInfo(ToExceptionCode(exception_code), message);
  if (isolate_) {
    v8::Local<v8::Value> exception = s_create_dom_exception_func_(
        isolate_, exception_code, message, String());
    V8ThrowException::ThrowException(isolate_, exception);
  }
}

void ExceptionState::ThrowSecurityError(const String& sanitized_message,
                                        const String& unsanitized_message) {
#if DCHECK_IS_ON()
  DCHECK_AT(!assert_no_exceptions_, file_, line_)
      << "SecurityError should not be thrown.";
#endif
  SetExceptionInfo(ToExceptionCode(DOMExceptionCode::kSecurityError),
                   sanitized_message);
  if (isolate_) {
    v8::Local<v8::Value> exception =
        s_create_dom_exception_func_(isolate_, DOMExceptionCode::kSecurityError,
                                     sanitized_message, unsanitized_message);
    V8ThrowException::ThrowException(isolate_, exception);
  }
}

void ExceptionState::ThrowRangeError(const String& message) {
#if DCHECK_IS_ON()
  DCHECK_AT(!assert_no_exceptions_, file_, line_)
      << "RangeError should not be thrown.";
#endif
  SetExceptionInfo(ToExceptionCode(ESErrorType::kRangeError), message);
  if (isolate_) {
    V8ThrowException::ThrowRangeError(isolate_, message);
  }
}

void ExceptionState::ThrowTypeError(const String& message) {
#if DCHECK_IS_ON()
  DCHECK_AT(!assert_no_exceptions_, file_, line_)
      << "TypeError should not be thrown.";
#endif
  SetExceptionInfo(ToExceptionCode(ESErrorType::kTypeError), message);
  if (isolate_) {
    V8ThrowException::ThrowTypeError(isolate_, message);
  }
}

void ExceptionState::ThrowWasmCompileError(const String& message) {
#if DCHECK_IS_ON()
  DCHECK_AT(!assert_no_exceptions_, file_, line_)
      << "WebAssembly.CompileError should not be thrown.";
#endif
  SetExceptionInfo(ToExceptionCode(ESErrorType::kWasmCompileError), message);
  if (isolate_) {
    V8ThrowException::ThrowWasmCompileError(isolate_, message);
  }
}

void ExceptionState::RethrowV8Exception(v8::TryCatch& try_catch) {
#if DCHECK_IS_ON()
  DCHECK_AT(!assert_no_exceptions_, file_, line_)
      << "A V8 exception should not be thrown.";
#endif
  SetExceptionInfo(
      static_cast<ExceptionCode>(InternalExceptionType::kRethrownException),
      String());
  if (isolate_) {
    try_catch.ReThrow();
  }
}

ExceptionState::ExceptionState(DummyExceptionStateForTesting& dummy_derived)
    : context_(
          ExceptionContext(v8::ExceptionContext::kUnknown, nullptr, nullptr)),
      isolate_(nullptr),
      swallow_all_exceptions_(true) {
  DCHECK(this == &dummy_derived);
}

}  // namespace blink
