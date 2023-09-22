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
  // ContextScope represents a stack of ExceptionContext in order to represent
  // nested exception contexts such like an IDL dictionary in another IDL
  // dictionary.
  class ContextScope {
    STACK_ALLOCATED();

   public:
    explicit ContextScope(const ExceptionContext& context,
                          ExceptionState& exception_state)
        : exception_state_(exception_state), context_(context) {
      exception_state_.PushContextScope(this);
    }
    ContextScope(const ContextScope&) = delete;
    ContextScope& operator=(const ContextScope&) = delete;

    ~ContextScope() { exception_state_.PopContextScope(); }

    // This is used for a performance hack to reduce the number of construction
    // and destruction times of ContextScope when iterating over properties.
    // Only the generated bindings code is allowed to use this hack.
    void ChangePropertyNameAsOptimizationHack(const char* property_name) {
      context_.ChangePropertyNameAsOptimizationHack(property_name);
    }

   private:
    void SetParent(const ContextScope* parent) { parent_ = parent; }
    const ContextScope* GetParent() const { return parent_; }
    const ExceptionContext& GetContext() const { return context_; }

    ExceptionState& exception_state_;
    const ContextScope* parent_ = nullptr;
    ExceptionContext context_;

    friend class ExceptionState;
  };

  // A function pointer type that creates a DOMException.
  using CreateDOMExceptionFunction =
      v8::Local<v8::Value> (*)(v8::Isolate*,
                               DOMExceptionCode,
                               const String& sanitized_message,
                               const String& unsanitized_message);

  // Sets the function to create a DOMException. Must be called only once.
  static void SetCreateDOMExceptionFunction(CreateDOMExceptionFunction);

  ExceptionState(v8::Isolate* isolate, const ExceptionContext& context)
      : main_context_(context), isolate_(isolate) {}

  ExceptionState(v8::Isolate* isolate, ExceptionContext&& context)
      : main_context_(std::move(context)), isolate_(isolate) {}

  ExceptionState(v8::Isolate* isolate,
                 ExceptionContextType context_type,
                 const char* interface_name,
                 const char* property_name)
      : ExceptionState(
            isolate,
            ExceptionContext(context_type, interface_name, property_name)) {}

  ExceptionState(v8::Isolate* isolate,
                 ExceptionContextType context_type,
                 const char* interface_name)
      : ExceptionState(isolate,
                       ExceptionContext(context_type, interface_name)) {}

  // This constructor opts in to special handling for a dynamic `property_name`,
  // which is only needed for named and indexed interceptors.
  enum ForInterceptor { kForInterceptor };
  ExceptionState(v8::Isolate* isolate,
                 ExceptionContextType context_type,
                 const char* interface_name,
                 const AtomicString& property_name,
                 ExceptionState::ForInterceptor)
      : ExceptionState(
            isolate,
            ExceptionContext(context_type, interface_name, property_name)) {}

  ExceptionState(const ExceptionState&) = delete;
  ExceptionState& operator=(const ExceptionState&) = delete;

  ~ExceptionState() {
    if (UNLIKELY(!exception_.IsEmpty())) {
      PropagateException();
    }
  }

  // Throws an appropriate exception due to the given exception code. The
  // exception will be either of ECMAScript Error object or DOMException.
  void ThrowException(ExceptionCode, const String& message);

  // Throws a DOMException due to the given exception code.
  virtual void ThrowDOMException(DOMExceptionCode, const String& message);

  // Throws a DOMException with SECURITY_ERR.
  virtual void ThrowSecurityError(const String& sanitized_message,
                                  const String& unsanitized_message = String());

  // Throws an ECMAScript Error object.
  virtual void ThrowRangeError(const String& message);
  virtual void ThrowTypeError(const String& message);

  // Throws WebAssembly Error object.
  virtual void ThrowWasmCompileError(const String& message);

  // These overloads reduce the binary code size because the call sites do not
  // need the conversion by String::String(const char*) that is inlined at each
  // call site. As there are many call sites that pass in a const char*, this
  // size optimization is effective (32kb reduction as of June 2018).
  // See also https://crbug.com/849743
  void ThrowDOMException(DOMExceptionCode, const char* message);
  void ThrowSecurityError(const char* sanitized_message,
                          const char* unsanitized_message = nullptr);
  void ThrowRangeError(const char* message);
  void ThrowTypeError(const char* message);
  void ThrowWasmCompileError(const char* message);

  // Rethrows a v8::Value as an exception.
  virtual void RethrowV8Exception(v8::Local<v8::Value>);

  // Returns true if there is a pending exception.
  //
  // Note that this function returns true even when |exception_| is empty, and
  // that V8ThrowDOMException::CreateOrEmpty may return an empty handle.
  bool HadException() const { return code_; }

  virtual void ClearException();

  ExceptionCode Code() const { return code_; }

  template <typename T>
  T CodeAs() const {
    return static_cast<T>(Code());
  }

  const String& Message() const { return message_; }

  virtual v8::Local<v8::Value> GetException() {
    DCHECK(!exception_.IsEmpty());
    return exception_.Get(isolate_);
  }

  // Returns the context of what Web API is currently being executed.
  const ExceptionContext& GetContext() const {
    DCHECK(!context_stack_top_);
    return main_context_;
  }

  // Returns the innermost context of the nested exception contexts.
  const ExceptionContext& GetInnerMostContext() const {
    if (context_stack_top_)
      return context_stack_top_->GetContext();
    return main_context_;
  }

 protected:
  void SetException(ExceptionCode, const String&, v8::Local<v8::Value>);
  void SetExceptionCode(ExceptionCode);
  v8::Isolate* GetIsolate() { return isolate_; }

 private:
  void PushContextScope(ContextScope* scope);
  void PopContextScope();
  void PropagateException();

  String AddExceptionContext(const String&) const;

  // Since DOMException is defined in core/, we need a dependency injection in
  // order to create a DOMException in platform/.
  static CreateDOMExceptionFunction s_create_dom_exception_func_;

  // The main context represents what Web API is currently being executed.
  // This is embedded without using ContextScope in order to avoid an overhead
  // of ContextScope.
  ExceptionContext main_context_;

  // `context_stack_top_` points to the top of the context stack which
  // represents additional (nested) contexts such as an IDL dictionary in a
  // member of another IDL dictionary.  nullptr means no additional context.
  const ContextScope* context_stack_top_ = nullptr;

  v8::Isolate* isolate_;
  ExceptionCode code_ = 0;
  String message_;
  // The exception is empty when it was thrown through
  // DummyExceptionStateForTesting.
  TraceWrapperV8Reference<v8::Value> exception_;

  friend class ContextScope;
};

// NonThrowableExceptionState never allow call sites to throw an exception.
// Should be used if an exception must not be thrown.
class PLATFORM_EXPORT NonThrowableExceptionState final : public ExceptionState {
 public:
  NonThrowableExceptionState();
  NonThrowableExceptionState(const char*, int);

  void ThrowDOMException(DOMExceptionCode, const String& message) override;
  void ThrowTypeError(const String& message) override;
  void ThrowSecurityError(const String& sanitized_message,
                          const String& unsanitized_message) override;
  void ThrowRangeError(const String& message) override;
  void ThrowWasmCompileError(const String& message) override;
  void RethrowV8Exception(v8::Local<v8::Value>) override;
  ExceptionState& ReturnThis() { return *this; }

 private:
  const char* file_;
  const int line_;
};

// Syntax sugar for NonThrowableExceptionState.
// This can be used as a default value of an ExceptionState parameter like this:
//
//     Node* removeChild(Node*, ExceptionState& = ASSERT_NO_EXCEPTION);
#if DCHECK_IS_ON()
#define ASSERT_NO_EXCEPTION \
  (::blink::NonThrowableExceptionState(__FILE__, __LINE__).ReturnThis())
#else
#define ASSERT_NO_EXCEPTION \
  (::blink::DummyExceptionStateForTesting().ReturnThis())
#endif

// DummyExceptionStateForTesting ignores all thrown exceptions. You should not
// use DummyExceptionStateForTesting in production code, where you need to
// handle all exceptions properly. If you really need to ignore exceptions in
// production code for some special reason, explicitly call clearException().
class PLATFORM_EXPORT DummyExceptionStateForTesting final
    : public ExceptionState {
 public:
  DummyExceptionStateForTesting()
      : ExceptionState(nullptr,
                       ExceptionContextType::kUnknown,
                       nullptr,
                       nullptr) {}
  ~DummyExceptionStateForTesting() {
    // Prevent the base class throw an exception.
    if (HadException()) {
      ClearException();
    }
  }
  void ThrowDOMException(DOMExceptionCode, const String& message) override;
  void ThrowTypeError(const String& message) override;
  void ThrowSecurityError(const String& sanitized_message,
                          const String& unsanitized_message) override;
  void ThrowRangeError(const String& message) override;
  void ThrowWasmCompileError(const String& message) override;
  void RethrowV8Exception(v8::Local<v8::Value>) override;
  ExceptionState& ReturnThis() { return *this; }
  v8::Local<v8::Value> GetException() override {
    return v8::Local<v8::Value>();
  }
};

// Syntax sugar for DummyExceptionStateForTesting.
// This can be used as a default value of an ExceptionState parameter like this:
//
//     Node* removeChild(Node*, ExceptionState& = IGNORE_EXCEPTION_FOR_TESTING);
#define IGNORE_EXCEPTION_FOR_TESTING \
  (::blink::DummyExceptionStateForTesting().ReturnThis())

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_EXCEPTION_STATE_H_
