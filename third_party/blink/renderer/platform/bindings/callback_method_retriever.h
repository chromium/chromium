// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_CALLBACK_METHOD_RETRIEVER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_CALLBACK_METHOD_RETRIEVER_H_

#include "third_party/blink/renderer/platform/bindings/callback_function_base.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "v8/include/v8.h"

namespace blink {

class CallbackFunctionBase;
class ExceptionState;

// Helper class to run step 10 of custom element definition, which part of the
// algorithm is expected to be defined in Web IDL so that other standards can
// apply the same algorithm.
//
// https://html.spec.whatwg.org/C/custom-elements.html#element-definition
// step 10. Run the following substeps while catching any exceptions: ...
class PLATFORM_EXPORT CallbackMethodRetriever {
  STACK_ALLOCATED();

 public:
  explicit CallbackMethodRetriever(CallbackFunctionBase* constructor);

  // Get the prototype object from the callback function. Must be invoked prior
  // to GetMethod or GetStaticMethod.
  void GetPrototypeObject(ExceptionState&);

  // Returns a function extracted from the prototype chain, or undefined.
  // Throws if the property is neither of function nor undefined.
  v8::Local<v8::Value> GetMethodOrUndefined(const StringView& method_name,
                                            ExceptionState& exception_state) {
    return GetFunctionOrUndefined(prototype_object_, method_name,
                                  exception_state);
  }

  // Returns a function extracted from the callback function, or undefined.
  // Throws if the property is neither of function nor undefined.
  v8::Local<v8::Value> GetStaticMethodOrUndefined(
      const StringView& method_name,
      ExceptionState& exception_state) {
    return GetFunctionOrUndefined(constructor_->CallbackObject(), method_name,
                                  exception_state);
  }

 private:
  // Gets |property| from |object|. Throws an exception if the property is
  // neither of function nor undefined.
  v8::Local<v8::Value> GetFunctionOrUndefined(v8::Local<v8::Object> object,
                                              const StringView& property,
                                              ExceptionState&);

  Member<CallbackFunctionBase> constructor_;
  v8::Isolate* isolate_;
  v8::Local<v8::Context> current_context_;
  v8::Local<v8::Object> prototype_object_;

  DISALLOW_COPY_AND_ASSIGN(CallbackMethodRetriever);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_CALLBACK_METHOD_RETRIEVER_H_
