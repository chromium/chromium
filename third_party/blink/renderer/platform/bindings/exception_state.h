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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_EXCEPTION_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_EXCEPTION_STATE_H_

#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/dcheck_is_on.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_context.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "v8/include/v8.h"

namespace blink {

// ExceptionState is a scope-like class and provides a way to throw an exception
// with an option to cancel it.  An exception message may be auto-generated.
class PLATFORM_EXPORT ExceptionState {
  STACK_ALLOCATED();

 public:
  // A function pointer type that creates a DOMException.
  using CreateDOMExceptionFunction =
      v8::Local<v8::Value> (*)(v8::Isolate*,
                               DOMExceptionCode,
                               const String& sanitized_message,
                               const String& unsanitized_message);

  // Sets the function to create a DOMException. Must be called only once.
  static void SetCreateDOMExceptionFunction(CreateDOMExceptionFunction);

  // If `isolate` is nullptr, this ExceptionState will ignore all exceptions.
  explicit ExceptionState(v8::Isolate* isolate)
      : context_(v8::ExceptionContext::kUnknown, nullptr, String()),
        isolate_(isolate) {}

  ExceptionState(v8::Isolate* isolate, const ExceptionContext& context)
      : context_(context), isolate_(isolate) {}

  ExceptionState(v8::Isolate* isolate, ExceptionContext&& context)
      : context_(std::move(context)), isolate_(isolate) {}

  ExceptionState(v8::Isolate* isolate,
                 v8::ExceptionContext context_type,
                 const char* interface_name,
                 const char* property_name)
      : ExceptionState(
            isolate,
            ExceptionContext(context_type, interface_name, property_name)) {}

  ExceptionState(v8::Isolate* isolate,
                 v8::ExceptionContext context_type,
                 const char* interface_name)
      : ExceptionState(isolate,
                       ExceptionContext(context_type, interface_name)) {}

  // This constructor opts in to special handling for a dynamic `property_name`,
  // which is only needed for named and indexed interceptors.
  enum ForInterceptor { kForInterceptor };
  ExceptionState(v8::Isolate* isolate,
                 v8::ExceptionContext context_type,
                 const char* interface_name,
                 const AtomicString& property_name,
                 ExceptionState::ForInterceptor)
      : ExceptionState(
            isolate,
            ExceptionContext(context_type, interface_name, property_name)) {}

  ExceptionState(const ExceptionState&) = delete;
  ExceptionState& operator=(const ExceptionState&) = delete;

  ~ExceptionState() {
    if (!exception_.IsEmpty()) [[unlikely]] {
      PropagateException();
    }
  }

  // Throws an appropriate exception due to the given exception code. The
  // exception will be either of ECMAScript Error object or DOMException.
  NOINLINE void ThrowException(ExceptionCode, const String& message);

  // Throws a DOMException due to the given exception code.
  NOINLINE void ThrowDOMException(DOMExceptionCode, const String& message);

  // Throws a DOMException with SECURITY_ERR.
  NOINLINE void ThrowSecurityError(
      const String& sanitized_message,
      const String& unsanitized_message = String());

  // Throws an ECMAScript Error object.
  NOINLINE void ThrowRangeError(const String& message);
  NOINLINE void ThrowTypeError(const String& message);

  // Throws WebAssembly Error object.
  NOINLINE void ThrowWasmCompileError(const String& message);

  // These overloads reduce the binary code size because the call sites do not
  // need the conversion by String::String(const char*) that is inlined at each
  // call site. As there are many call sites that pass in a const char*, this
  // size optimization is effective (32kb reduction as of June 2018).
  // See also https://crbug.com/849743
  NOINLINE void ThrowDOMException(DOMExceptionCode, const char* message);
  NOINLINE void ThrowSecurityError(const char* sanitized_message,
                                   const char* unsanitized_message = nullptr);
  NOINLINE void ThrowRangeError(const char* message);
  NOINLINE void ThrowTypeError(const char* message);
  NOINLINE void ThrowWasmCompileError(const char* message);

  // Report the given value as the exception being thrown, but rethrow it
  // immediately via the v8::TryCatch instead of in the destructor.
  NOINLINE void RethrowV8Exception(v8::TryCatch&);

  bool DidRethrowViaV8TryCatch() const { return thrown_via_v8_trycatch_; }

  // Returns true if there is a pending exception.
  //
  // Note that this function returns true even when |exception_| is empty, and
  // that V8ThrowDOMException::CreateOrEmpty may return an empty handle.
  bool HadException() const { return code_; }

  ExceptionCode Code() const { return code_; }

  template <typename T>
  T CodeAs() const {
    return static_cast<T>(Code());
  }

  const String& Message() const { return message_; }

  v8::Local<v8::Value> GetException() {
    return isolate_ ? exception_.Get(isolate_) : v8::Local<v8::Value>();
  }

  // Returns the context of what Web API is currently being executed.
  const ExceptionContext& GetContext() const { return context_; }

  ExceptionState& ReturnThis() { return *this; }

 protected:
  // Delegated constructor for NonThrowableExceptionState
  enum ForNonthrowable { kNonthrowable };
  ExceptionState(const char* file, int line, ForNonthrowable)
      : context_(v8::ExceptionContext::kUnknown, nullptr, String()),
        isolate_(nullptr) {
#if DCHECK_IS_ON()
    file_ = file;
    line_ = line;
    assert_no_exceptions_ = true;
#endif
  }

 private:
  void SetException(ExceptionCode, const String&, v8::Local<v8::Value>);
  void PropagateException();

  // Since DOMException is defined in core/, we need a dependency injection in
  // order to create a DOMException in platform/.
  static CreateDOMExceptionFunction s_create_dom_exception_func_;

  // The main context represents what Web API is currently being executed.
  ExceptionContext context_;

  v8::Isolate* isolate_;
  ExceptionCode code_ = 0;
  String message_;
  // The exception is empty when it was thrown through
  // DummyExceptionStateForTesting.
  TraceWrapperV8Reference<v8::Value> exception_;
  bool thrown_via_v8_trycatch_ = false;

#if DCHECK_IS_ON()
  const char* file_ = "";
  int line_ = 0;
  bool assert_no_exceptions_ = false;
#endif
};

// Syntactic sugar for creating an ExceptionState that will throw as soon as
// the function it's passed to returns.
// This is useful for when a v8::TryCatch is on the stack.
class PassThroughException {
  STACK_ALLOCATED();

 public:
  explicit PassThroughException(v8::Isolate* isolate)
      : exception_state_(isolate) {}

  operator ExceptionState&() & = delete;
  operator ExceptionState&() && { return exception_state_; }

 private:
  ExceptionState exception_state_;
};

// NonThrowableExceptionState never allow call sites to throw an exception.
// Should be used if an exception must not be thrown.
class PLATFORM_EXPORT NonThrowableExceptionState final : public ExceptionState {
 public:
  NonThrowableExceptionState(const char* file = "", int line = 0)
      : ExceptionState(file, line, kNonthrowable) {}
};

// DummyExceptionStateForTesting ignores all thrown exceptions. Syntactic sugar
// for ExceptionState(nullptr)
class PLATFORM_EXPORT DummyExceptionStateForTesting final
    : public ExceptionState {
 public:
  DummyExceptionStateForTesting() : ExceptionState(nullptr) {}
};

class PLATFORM_EXPORT TryRethrowScope {
  STACK_ALLOCATED();

 public:
  TryRethrowScope(v8::Isolate* isolate, ExceptionState& exception_state)
      : try_catch_(isolate), exception_state_(exception_state) {}

  ~TryRethrowScope() {
    if (try_catch_.HasCaught()) [[unlikely]] {
      exception_state_.RethrowV8Exception(try_catch_);
    }
  }

  bool HasCaught() { return try_catch_.HasCaught(); }
  void SwallowException() { return try_catch_.Reset(); }
  v8::Local<v8::Value> GetException() { return try_catch_.Exception(); }

  v8::TryCatch try_catch_;
  ExceptionState& exception_state_;
};

// Syntax sugar for ExceptionState(nullptr)
// This can be used as a default value of an ExceptionState parameter like this:
//
//     Node* removeChild(Node*, ExceptionState& = IGNORE_EXCEPTION);
#define IGNORE_EXCEPTION (::blink::ExceptionState(nullptr).ReturnThis())
#define IGNORE_EXCEPTION_FOR_TESTING IGNORE_EXCEPTION

// Syntax sugar for NonThrowableExceptionState.
// This can be used as a default value of an ExceptionState parameter like this:
//
//     Node* removeChild(Node*, ExceptionState& = ASSERT_NO_EXCEPTION);
#if DCHECK_IS_ON()
#define ASSERT_NO_EXCEPTION \
  (::blink::NonThrowableExceptionState(__FILE__, __LINE__).ReturnThis())
#else
#define ASSERT_NO_EXCEPTION IGNORE_EXCEPTION
#endif
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_EXCEPTION_STATE_H_
