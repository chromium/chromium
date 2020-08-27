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

#include "base/notreached.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"

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

  const String& processed_message = AddExceptionContext(message);

  v8::Local<v8::Value> exception;
  switch (static_cast<ESErrorType>(exception_code)) {
    case ESErrorType::kError:
      exception = V8ThrowException::CreateError(isolate_, processed_message);
      break;
    case ESErrorType::kRangeError:
      exception =
          V8ThrowException::CreateRangeError(isolate_, processed_message);
      break;
    case ESErrorType::kReferenceError:
      exception =
          V8ThrowException::CreateReferenceError(isolate_, processed_message);
      break;
    case ESErrorType::kSyntaxError:
      exception =
          V8ThrowException::CreateSyntaxError(isolate_, processed_message);
      break;
    case ESErrorType::kTypeError:
      exception =
          V8ThrowException::CreateTypeError(isolate_, processed_message);
      break;
    default:
      if (IsDOMExceptionCode(exception_code)) {
        exception = s_create_dom_exception_func_(
            isolate_, static_cast<DOMExceptionCode>(exception_code),
            processed_message, String());
      } else {
        NOTREACHED();
        exception = s_create_dom_exception_func_(
            isolate_, DOMExceptionCode::kUnknownError, processed_message,
            String());
      }
  }

  SetException(exception_code, processed_message, exception);
}

void ExceptionState::ThrowDOMException(DOMExceptionCode exception_code,
                                       const String& message) {
  // SecurityError is thrown via ThrowSecurityError, and _careful_ consideration
  // must be given to the data exposed to JavaScript via |sanitized_message|.
  DCHECK_NE(exception_code, DOMExceptionCode::kSecurityError);

  const String& processed_message = AddExceptionContext(message);
  SetException(ToExceptionCode(exception_code), processed_message,
               s_create_dom_exception_func_(isolate_, exception_code,
                                            processed_message, String()));
}

void ExceptionState::ThrowSecurityError(const String& sanitized_message,
                                        const String& unsanitized_message) {
  const String& final_sanitized = AddExceptionContext(sanitized_message);
  const String& final_unsanitized = AddExceptionContext(unsanitized_message);
  SetException(
      ToExceptionCode(DOMExceptionCode::kSecurityError), final_sanitized,
      s_create_dom_exception_func_(isolate_, DOMExceptionCode::kSecurityError,
                                   final_sanitized, final_unsanitized));
}

void ExceptionState::ThrowRangeError(const String& message) {
  SetException(ToExceptionCode(ESErrorType::kRangeError), message,
               V8ThrowException::CreateRangeError(
                   isolate_, AddExceptionContext(message)));
}

void ExceptionState::ThrowTypeError(const String& message) {
  SetException(ToExceptionCode(ESErrorType::kTypeError), message,
               V8ThrowException::CreateTypeError(isolate_,
                                                 AddExceptionContext(message)));
}

void ExceptionState::ThrowWasmCompileError(const String& message) {
  SetException(ToExceptionCode(ESErrorType::kWasmCompileError), message,
               V8ThrowException::CreateWasmCompileError(
                   isolate_, AddExceptionContext(message)));
}

void ExceptionState::ThrowDOMException(DOMExceptionCode exception_code,
                                       const char* message) {
  ThrowDOMException(exception_code, String(message));
}

void ExceptionState::ThrowSecurityError(const char* sanitized_message,
                                        const char* unsanitized_message) {
  ThrowSecurityError(String(sanitized_message), String(unsanitized_message));
}

void ExceptionState::ThrowRangeError(const char* message) {
  ThrowRangeError(String(message));
}

void ExceptionState::ThrowTypeError(const char* message) {
  ThrowTypeError(String(message));
}

void ExceptionState::ThrowWasmCompileError(const char* message) {
  ThrowWasmCompileError(String(message));
}

void ExceptionState::RethrowV8Exception(v8::Local<v8::Value> value) {
  SetException(
      static_cast<ExceptionCode>(InternalExceptionType::kRethrownException),
      String(), value);
}

void ExceptionState::ClearException() {
  code_ = 0;
  message_ = String();
  exception_.Clear();
}

void ExceptionState::SetException(ExceptionCode exception_code,
                                  const String& message,
                                  v8::Local<v8::Value> exception) {
  CHECK(exception_code);

  code_ = exception_code;
  message_ = message;
  if (exception.IsEmpty()) {
    exception_.Clear();
  } else {
    DCHECK(isolate_);
    exception_.Set(isolate_, exception);
  }
}

String ExceptionState::AddExceptionContext(const String& message) const {
  if (message.IsEmpty())
    return message;

  const char* i = InterfaceName();
  const char* p = PropertyName();
  const auto& m = message;

  if (i && p) {
    switch (context_) {
      case kConstructionContext:
        return ExceptionMessages::FailedToConstruct(i, m);
      case kExecutionContext:
        return ExceptionMessages::FailedToExecute(p, i, m);
      case kDeletionContext:
        return ExceptionMessages::FailedToDelete(p, i, m);
      case kGetterContext:
        return ExceptionMessages::FailedToGet(p, i, m);
      case kSetterContext:
        return ExceptionMessages::FailedToSet(p, i, m);
      case kEnumerationContext:
        return ExceptionMessages::FailedToEnumerate(i, m);
      case kQueryContext:
        break;
      case kIndexedGetterContext:
        return ExceptionMessages::FailedToGetIndexed(i, m);
      case kIndexedSetterContext:
        return ExceptionMessages::FailedToSetIndexed(i, m);
      case kIndexedDeletionContext:
        return ExceptionMessages::FailedToDeleteIndexed(i, m);
      case kNamedGetterContext:
        return ExceptionMessages::FailedToGetNamed(i, m);
      case kNamedSetterContext:
        return ExceptionMessages::FailedToSetNamed(i, m);
      case kNamedDeletionContext:
        return ExceptionMessages::FailedToDeleteNamed(i, m);
      case kUnknownContext:
        break;
    }
  }

  if (i) {
    switch (context_) {
      case kConstructionContext:
        return ExceptionMessages::FailedToConstruct(i, m);
      case kExecutionContext:
        break;
      case kDeletionContext:
        break;
      case kGetterContext:
        break;
      case kSetterContext:
        break;
      case kEnumerationContext:
        return ExceptionMessages::FailedToEnumerate(i, m);
      case kQueryContext:
        break;
      case kIndexedGetterContext:
        return ExceptionMessages::FailedToGetIndexed(i, m);
      case kIndexedSetterContext:
        return ExceptionMessages::FailedToSetIndexed(i, m);
      case kIndexedDeletionContext:
        return ExceptionMessages::FailedToDeleteIndexed(i, m);
      case kNamedGetterContext:
        return ExceptionMessages::FailedToGetNamed(i, m);
      case kNamedSetterContext:
        return ExceptionMessages::FailedToSetNamed(i, m);
      case kNamedDeletionContext:
        return ExceptionMessages::FailedToDeleteNamed(i, m);
      case kUnknownContext:
        break;
    }
  }

  return message;
}

NonThrowableExceptionState::NonThrowableExceptionState()
    : ExceptionState(nullptr,
                     ExceptionState::kUnknownContext,
                     nullptr,
                     nullptr),
      file_(""),
      line_(0) {}

NonThrowableExceptionState::NonThrowableExceptionState(const char* file,
                                                       int line)
    : ExceptionState(nullptr,
                     ExceptionState::kUnknownContext,
                     nullptr,
                     nullptr),
      file_(file),
      line_(line) {}

void NonThrowableExceptionState::ThrowDOMException(
    DOMExceptionCode exception_code,
    const String& message) {
  DCHECK_AT(false, file_, line_) << "DOMExeption should not be thrown.";
}

void NonThrowableExceptionState::ThrowRangeError(const String& message) {
  DCHECK_AT(false, file_, line_) << "RangeError should not be thrown.";
}

void NonThrowableExceptionState::ThrowSecurityError(
    const String& sanitized_message,
    const String&) {
  DCHECK_AT(false, file_, line_) << "SecurityError should not be thrown.";
}

void NonThrowableExceptionState::ThrowTypeError(const String& message) {
  DCHECK_AT(false, file_, line_) << "TypeError should not be thrown.";
}

void NonThrowableExceptionState::ThrowWasmCompileError(const String& message) {
  DCHECK_AT(false, file_, line_)
      << "WebAssembly.CompileError should not be thrown.";
}

void NonThrowableExceptionState::RethrowV8Exception(v8::Local<v8::Value>) {
  DCHECK_AT(false, file_, line_) << "An exception should not be rethrown.";
}

void DummyExceptionStateForTesting::ThrowDOMException(
    DOMExceptionCode exception_code,
    const String& message) {
  SetException(ToExceptionCode(exception_code), message,
               v8::Local<v8::Value>());
}

void DummyExceptionStateForTesting::ThrowRangeError(const String& message) {
  SetException(ToExceptionCode(ESErrorType::kRangeError), message,
               v8::Local<v8::Value>());
}

void DummyExceptionStateForTesting::ThrowSecurityError(
    const String& sanitized_message,
    const String&) {
  SetException(ToExceptionCode(DOMExceptionCode::kSecurityError),
               sanitized_message, v8::Local<v8::Value>());
}

void DummyExceptionStateForTesting::ThrowTypeError(const String& message) {
  SetException(ToExceptionCode(ESErrorType::kTypeError), message,
               v8::Local<v8::Value>());
}

void DummyExceptionStateForTesting::ThrowWasmCompileError(
    const String& message) {
  SetException(ToExceptionCode(ESErrorType::kWasmCompileError), message,
               v8::Local<v8::Value>());
}

void DummyExceptionStateForTesting::RethrowV8Exception(v8::Local<v8::Value>) {
  SetException(
      static_cast<ExceptionCode>(InternalExceptionType::kRethrownException),
      String(), v8::Local<v8::Value>());
}

}  // namespace blink
