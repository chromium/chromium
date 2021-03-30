// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_NO_ALLOC_DIRECT_CALL_EXCEPTION_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_NO_ALLOC_DIRECT_CALL_EXCEPTION_STATE_H_

#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/no_alloc_direct_call_host.h"

namespace blink {

// Sub-class of ExceptionState that is designed to work with V8 fast API calls.
// When a Throw* method is called, the creation of the exception object is
// deferred using the deferral mechanism provided by NoAllocDirectCallHost.
class PLATFORM_EXPORT NoAllocDirectCallExceptionState : public ExceptionState {
 public:
  NoAllocDirectCallExceptionState(NoAllocDirectCallHost* host,
                                  v8::Isolate* isolate,
                                  ExceptionState::ContextType context_type,
                                  const char* interface_name,
                                  const char* property_name)
      : ExceptionState(isolate, context_type, interface_name, property_name),
        host_(host) {}

  virtual ~NoAllocDirectCallExceptionState();

  void ThrowDOMException(DOMExceptionCode, const String& message) override;
  void ThrowTypeError(const String& message) override;
  void ThrowSecurityError(const String& sanitized_message,
                          const String& unsanitized_message) override;
  void ThrowRangeError(const String& message) override;
  void ThrowWasmCompileError(const String& message) override { NOTREACHED(); }
  void RethrowV8Exception(v8::Local<v8::Value>) override { NOTREACHED(); }

  void ClearException() override;

 private:
  NoAllocDirectCallHost* host_;
  NoAllocDirectCallHost::DeferrableAction deferred_exception_;
};

}  // namespace blink

#endif
