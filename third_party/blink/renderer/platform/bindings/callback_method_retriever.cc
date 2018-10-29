// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/callback_method_retriever.h"

#include "third_party/blink/renderer/platform/bindings/callback_function_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"

namespace blink {

CallbackMethodRetriever::CallbackMethodRetriever(
    CallbackFunctionBase* constructor)
    : constructor_(constructor),
      isolate_(constructor_->GetIsolate()),
      current_context_(isolate_->GetCurrentContext()) {
  DCHECK(constructor_->IsConstructor());
}

void CallbackMethodRetriever::GetPrototypeObject(
    ExceptionState& exception_state) {
  // https://html.spec.whatwg.org/C/custom-elements.html#element-definition
  // step 10.1. Let prototype be Get(constructor, "prototype"). Rethrow any
  //   exceptions.
  v8::TryCatch try_catch(isolate_);
  v8::Local<v8::Value> prototype;
  if (!constructor_->CallbackObject()
           ->Get(current_context_, V8AtomicString(isolate_, "prototype"))
           .ToLocal(&prototype)) {
    exception_state.RethrowV8Exception(try_catch.Exception());
    return;
  }
  // step 10.2. If Type(prototype) is not Object, then throw a TypeError
  //   exception.
  if (!prototype->IsObject()) {
    exception_state.ThrowTypeError("constructor prototype is not an object");
    return;
  }
  prototype_object_ = prototype.As<v8::Object>();
}

v8::Local<v8::Value> CallbackMethodRetriever::GetFunctionOrUndefined(
    v8::Local<v8::Object> object,
    const StringView& property,
    ExceptionState& exception_state) {
  DCHECK(prototype_object_->IsObject());

  v8::TryCatch try_catch(isolate_);
  v8::Local<v8::Value> value;
  if (!object->Get(current_context_, V8AtomicString(isolate_, property))
           .ToLocal(&value)) {
    exception_state.RethrowV8Exception(try_catch.Exception());
    return v8::Local<v8::Function>();
  }
  if (!value->IsUndefined() && !value->IsFunction()) {
    exception_state.ThrowTypeError(
        String::Format("\"%s\" is not a function", property.Characters8()));
    return v8::Local<v8::Function>();
  }
  return value;
}

}  // namespace blink
