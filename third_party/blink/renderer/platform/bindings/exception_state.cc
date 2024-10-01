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

void ExceptionState::ThrowException(ExceptionCode exception_code,
                                    const String& message) {
  // SecurityError is thrown via ThrowSecurityError, and _careful_ consideration
  // must be given to the data exposed to JavaScript via |sanitized_message|.
  DCHECK_NE(exception_code, ToExceptionCode(DOMExceptionCode::kSecurityError));

#if DCHECK_IS_ON()
  DCHECK_AT(!assert_no_exceptions_, file_, line_)
      << "ThrowException must not be called.";
#endif
  if (!isolate_) {
    SetException(exception_code, message, v8::Local<v8::Value>());
    return;
  }

  v8::Local<v8::Value> exception;
  switch (static_cast<ESErrorType>(exception_code)) {
    case ESErrorType::kError:
      exception = V8ThrowException::CreateError(isolate_, message);
      break;
    case ESErrorType::kRangeError:
      exception = V8ThrowException::CreateRangeError(isolate_, message);
      break;
    case ESErrorType::kReferenceError:
      exception = V8ThrowException::CreateReferenceError(isolate_, message);
      break;
    case ESErrorType::kSyntaxError:
      exception = V8ThrowException::CreateSyntaxError(isolate_, message);
      break;
    case ESErrorType::kTypeError:
      exception = V8ThrowException::CreateTypeError(isolate_, message);
      break;
    default:
      if (IsDOMExceptionCode(exception_code)) {
        exception = s_create_dom_exception_func_(
            isolate_, static_cast<DOMExceptionCode>(exception_code), message,
            String());
      } else {
        NOTREACHED_IN_MIGRATION();
        exception = s_create_dom_exception_func_(
            isolate_, DOMExceptionCode::kUnknownError, message, String());
      }
  }

  SetException(exception_code, message, exception);
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

void ExceptionState::SetException(ExceptionCode exception_code,
                                  const String& message,
                                  v8::Local<v8::Value> exception) {
  CHECK(exception_code);

  code_ = exception_code;
  message_ = message;
  if (exception.IsEmpty()) {
    exception_.Reset();
  } else {
    DCHECK(isolate_);
    exception_.Reset(isolate_, exception);
  }
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

  v8::Local<v8::Value> exception =
      isolate_ ? s_create_dom_exception_func_(isolate_, exception_code, message,
                                              String())
               : v8::Local<v8::Value>();
  SetException(ToExceptionCode(exception_code), message, exception);
}

void ExceptionState::ThrowSecurityError(const String& sanitized_message,
                                        const String& unsanitized_message) {
#if DCHECK_IS_ON()
  DCHECK_AT(!assert_no_exceptions_, file_, line_)
      << "SecurityError should not be thrown.";
#endif
  v8::Local<v8::Value> exception =
      isolate_ ? s_create_dom_exception_func_(
                     isolate_, DOMExceptionCode::kSecurityError,
                     sanitized_message, unsanitized_message)
               : v8::Local<v8::Value>();
  SetException(ToExceptionCode(DOMExceptionCode::kSecurityError),
               sanitized_message, exception);
}

void ExceptionState::ThrowRangeError(const String& message) {
#if DCHECK_IS_ON()
  DCHECK_AT(!assert_no_exceptions_, file_, line_)
      << "RangeError should not be thrown.";
#endif
  v8::Local<v8::Value> exception =
      isolate_ ? V8ThrowException::CreateRangeError(isolate_, message)
               : v8::Local<v8::Value>();
  SetException(ToExceptionCode(ESErrorType::kRangeError), message, exception);
}

void ExceptionState::ThrowTypeError(const String& message) {
#if DCHECK_IS_ON()
  DCHECK_AT(!assert_no_exceptions_, file_, line_)
      << "TypeError should not be thrown.";
#endif
  v8::Local<v8::Value> exception =
      isolate_ ? V8ThrowException::CreateTypeError(isolate_, message)
               : v8::Local<v8::Value>();
  SetException(ToExceptionCode(ESErrorType::kTypeError), message, exception);
}

void ExceptionState::ThrowWasmCompileError(const String& message) {
#if DCHECK_IS_ON()
  DCHECK_AT(!assert_no_exceptions_, file_, line_)
      << "WebAssembly.CompileError should not be thrown.";
#endif
  v8::Local<v8::Value> exception =
      isolate_ ? V8ThrowException::CreateWasmCompileError(isolate_, message)
               : v8::Local<v8::Value>();
  SetException(ToExceptionCode(ESErrorType::kWasmCompileError), message,
               exception);
}

void ExceptionState::RethrowV8Exception(v8::TryCatch& try_catch) {
#if DCHECK_IS_ON()
  DCHECK_AT(!assert_no_exceptions_, file_, line_)
      << "A V8 exception should not be thrown.";
#endif
  SetException(
      static_cast<ExceptionCode>(InternalExceptionType::kRethrownException),
      String(), isolate_ ? try_catch.Exception() : v8::Local<v8::Value>());
  if (isolate_) {
    thrown_via_v8_trycatch_ = true;
    try_catch.ReThrow();
  }
}

void ExceptionState::PropagateException() {
  // This is the non-inlined part of the destructor. Not inlining this part
  // deoptimizes use cases where exceptions are thrown, but it reduces binary
  // size and results in better performance due to improved code locality in
  // the bindings for the most frequently used code path (cases where no
  // exception is thrown).
  if (!thrown_via_v8_trycatch_) {
    V8ThrowException::ThrowException(isolate_, exception_.Get(isolate_));
  }
}

}  // namespace blink
