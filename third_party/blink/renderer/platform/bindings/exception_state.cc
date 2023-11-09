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
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
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

NOINLINE void ExceptionState::ThrowDOMException(DOMExceptionCode exception_code,
                                                const String& message) {
  DoThrowDOMException(exception_code, message);
}

NOINLINE void ExceptionState::ThrowSecurityError(
    const String& sanitized_message,
    const String& unsanitized_message) {
  DoThrowSecurityError(sanitized_message, unsanitized_message);
}

NOINLINE void ExceptionState::ThrowRangeError(const String& message) {
  DoThrowRangeError(message);
}

NOINLINE void ExceptionState::ThrowTypeError(const String& message) {
  DoThrowTypeError(message);
}

NOINLINE void ExceptionState::ThrowWasmCompileError(const String& message) {
  DoThrowWasmCompileError(message);
}

NOINLINE void ExceptionState::ThrowDOMException(DOMExceptionCode exception_code,
                                                const char* message) {
  DoThrowDOMException(exception_code, message);
}

NOINLINE void ExceptionState::ThrowSecurityError(
    const char* sanitized_message,
    const char* unsanitized_message) {
  DoThrowSecurityError(sanitized_message, unsanitized_message);
}

NOINLINE void ExceptionState::ThrowRangeError(const char* message) {
  DoThrowRangeError(message);
}

NOINLINE void ExceptionState::ThrowTypeError(const char* message) {
  DoThrowTypeError(message);
}

NOINLINE void ExceptionState::ThrowWasmCompileError(const char* message) {
  DoThrowWasmCompileError(message);
}

NOINLINE void ExceptionState::RethrowV8Exception(v8::Local<v8::Value> value) {
  DoRethrowV8Exception(value);
}

void ExceptionState::ClearException() {
  code_ = 0;
  message_ = String();
  exception_.Reset();
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

void ExceptionState::SetExceptionCode(ExceptionCode exception_code) {
  CHECK(exception_code);
  DCHECK(message_.empty());
  DCHECK(exception_.IsEmpty());
  code_ = exception_code;
}

void ExceptionState::DoThrowDOMException(DOMExceptionCode exception_code,
                                         const String& message) {
  // SecurityError is thrown via ThrowSecurityError, and _careful_ consideration
  // must be given to the data exposed to JavaScript via |sanitized_message|.
  DCHECK_NE(exception_code, DOMExceptionCode::kSecurityError);

  const String& processed_message = AddExceptionContext(message);
  SetException(ToExceptionCode(exception_code), processed_message,
               s_create_dom_exception_func_(isolate_, exception_code,
                                            processed_message, String()));
}

void ExceptionState::DoThrowSecurityError(const String& sanitized_message,
                                          const String& unsanitized_message) {
  const String& final_sanitized = AddExceptionContext(sanitized_message);
  const String& final_unsanitized = AddExceptionContext(unsanitized_message);
  SetException(
      ToExceptionCode(DOMExceptionCode::kSecurityError), final_sanitized,
      s_create_dom_exception_func_(isolate_, DOMExceptionCode::kSecurityError,
                                   final_sanitized, final_unsanitized));
}

void ExceptionState::DoThrowRangeError(const String& message) {
  SetException(ToExceptionCode(ESErrorType::kRangeError), message,
               V8ThrowException::CreateRangeError(
                   isolate_, AddExceptionContext(message)));
}

void ExceptionState::DoThrowTypeError(const String& message) {
  SetException(ToExceptionCode(ESErrorType::kTypeError), message,
               V8ThrowException::CreateTypeError(isolate_,
                                                 AddExceptionContext(message)));
}

void ExceptionState::DoThrowWasmCompileError(const String& message) {
  SetException(ToExceptionCode(ESErrorType::kWasmCompileError), message,
               V8ThrowException::CreateWasmCompileError(
                   isolate_, AddExceptionContext(message)));
}

void ExceptionState::DoRethrowV8Exception(v8::Local<v8::Value> value) {
  SetException(
      static_cast<ExceptionCode>(InternalExceptionType::kRethrownException),
      String(), value);
}

void ExceptionState::PushContextScope(ContextScope* scope) {
  scope->SetParent(context_stack_top_);
  context_stack_top_ = scope;
}

void ExceptionState::PopContextScope() {
  DCHECK(context_stack_top_);
  context_stack_top_ = context_stack_top_->GetParent();
}

namespace {

String AddContextToMessage(const String& message,
                           const ExceptionContext& context) {
  const char* c = context.GetClassName();
  const String& p = context.GetPropertyName();
  const String& m = message;

  switch (context.GetType()) {
    case ExceptionContextType::kConstructorOperationInvoke:
      return ExceptionMessages::FailedToConstruct(c, m);
    case ExceptionContextType::kOperationInvoke:
      return ExceptionMessages::FailedToExecute(p, c, m);
    case ExceptionContextType::kAttributeGet:
      return ExceptionMessages::FailedToGet(p, c, m);
    case ExceptionContextType::kAttributeSet:
      return ExceptionMessages::FailedToSet(p, c, m);
    case ExceptionContextType::kNamedPropertyEnumerator:
      return ExceptionMessages::FailedToEnumerate(c, m);
    case ExceptionContextType::kNamedPropertyQuery:
      break;
    case ExceptionContextType::kIndexedPropertyGetter:
    case ExceptionContextType::kIndexedPropertyDescriptor:
      return ExceptionMessages::FailedToGetIndexed(p, c, m);
    case ExceptionContextType::kIndexedPropertySetter:
    case ExceptionContextType::kIndexedPropertyDefiner:
      return ExceptionMessages::FailedToSetIndexed(p, c, m);
    case ExceptionContextType::kIndexedPropertyDeleter:
      return ExceptionMessages::FailedToDeleteIndexed(p, c, m);
    case ExceptionContextType::kNamedPropertyGetter:
    case ExceptionContextType::kNamedPropertyDescriptor:
      return ExceptionMessages::FailedToGetNamed(p, c, m);
    case ExceptionContextType::kNamedPropertySetter:
    case ExceptionContextType::kNamedPropertyDefiner:
      return ExceptionMessages::FailedToSetNamed(p, c, m);
    case ExceptionContextType::kNamedPropertyDeleter:
      return ExceptionMessages::FailedToDeleteNamed(p, c, m);
    case ExceptionContextType::kDictionaryMemberGet:
      return ExceptionMessages::FailedToGet(p, c, m);
    case ExceptionContextType::kUnknown:
      break;
    default:
      NOTREACHED();
      break;
  }
  return m;
}

}  // namespace

String ExceptionState::AddExceptionContext(
    const String& original_message) const {
  if (original_message.empty())
    return original_message;

  String message = original_message;
  for (const ContextScope* scope = context_stack_top_; scope;
       scope = scope->GetParent()) {
    message = AddContextToMessage(message, scope->GetContext());
  }
  message = AddContextToMessage(message, main_context_);
  return message;
}

void ExceptionState::PropagateException() {
  // This is the non-inlined part of the destructor. Not inlining this part
  // deoptimizes use cases where exceptions are thrown, but it reduces binary
  // size and results in better performance due to improved code locality in
  // the bindings for the most frequently used code path (cases where no
  // exception is thrown).
  V8ThrowException::ThrowException(isolate_, exception_.Get(isolate_));
}

NonThrowableExceptionState::NonThrowableExceptionState()
    : ExceptionState(nullptr, ExceptionContextType::kUnknown, nullptr, nullptr),
      file_(""),
      line_(0) {}

NonThrowableExceptionState::NonThrowableExceptionState(const char* file,
                                                       int line)
    : ExceptionState(nullptr, ExceptionContextType::kUnknown, nullptr, nullptr),
      file_(file),
      line_(line) {}

void NonThrowableExceptionState::DoThrowDOMException(
    DOMExceptionCode exception_code,
    const String& message) {
  ComplainAbout("DOMException");
}

void NonThrowableExceptionState::DoThrowRangeError(const String& message) {
  ComplainAbout("RangeError");
}

void NonThrowableExceptionState::DoThrowSecurityError(
    const String& sanitized_message,
    const String&) {
  ComplainAbout("SecurityError");
}

void NonThrowableExceptionState::DoThrowTypeError(const String& message) {
  ComplainAbout("TypeError");
}

void NonThrowableExceptionState::DoThrowWasmCompileError(
    const String& message) {
  ComplainAbout("WebAssembly.CompileError");
}

void NonThrowableExceptionState::DoRethrowV8Exception(v8::Local<v8::Value>) {
  ComplainAbout("A V8 exception");
}

void NonThrowableExceptionState::ComplainAbout(const char* exception) {
  DCHECK_AT(false, file_, line_) << exception << " should not be thrown.";
}

void DummyExceptionStateForTesting::DoThrowDOMException(
    DOMExceptionCode exception_code,
    const String& message) {
  DoThrowInternal(exception_code, message);
}

void DummyExceptionStateForTesting::DoThrowRangeError(const String& message) {
  DoThrowInternal(ESErrorType::kRangeError, message);
}

void DummyExceptionStateForTesting::DoThrowSecurityError(
    const String& sanitized_message,
    const String&) {
  DoThrowInternal(DOMExceptionCode::kSecurityError, sanitized_message);
}

void DummyExceptionStateForTesting::DoThrowTypeError(const String& message) {
  DoThrowInternal(ESErrorType::kTypeError, message);
}

void DummyExceptionStateForTesting::DoThrowWasmCompileError(
    const String& message) {
  DoThrowInternal(ESErrorType::kWasmCompileError, message);
}

void DummyExceptionStateForTesting::DoRethrowV8Exception(v8::Local<v8::Value>) {
  DoThrowInternal(
      static_cast<ExceptionCode>(InternalExceptionType::kRethrownException),
      String());
}

void DummyExceptionStateForTesting::DoThrowInternal(ESErrorType error_type,
                                                    const String& message) {
  DoThrowInternal(ToExceptionCode(error_type), message);
}

void DummyExceptionStateForTesting::DoThrowInternal(DOMExceptionCode dom_code,
                                                    const String& message) {
  DoThrowInternal(ToExceptionCode(dom_code), message);
}

void DummyExceptionStateForTesting::DoThrowInternal(ExceptionCode code,
                                                    const String& message) {
  SetException(code, message, v8::Local<v8::Value>());
}

}  // namespace blink
