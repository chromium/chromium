// Copyright 2018 The Chromium Authors
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

v8::Local<v8::Object> CallbackMethodRetriever::GetPrototypeObject(
    ExceptionState& exception_state) {
  DCHECK(prototype_object_.IsEmpty()) << "Do not call GetPrototypeObject twice";
  // https://html.spec.whatwg.org/C/custom-elements.html#element-definition
  // step 10.1. Let prototype be Get(constructor, "prototype"). Rethrow any
  //   exceptions.
  TryRethrowScope rethrow_scope(isolate_, exception_state);
  v8::Local<v8::Value> prototype;
  if (!constructor_->CallbackObject()
           ->Get(current_context_, V8AtomicString(isolate_, "prototype"))
           .ToLocal(&prototype)) {
    return v8::Local<v8::Object>();
  }
  // step 10.2. If Type(prototype) is not Object, then throw a TypeError
  //   exception.
  if (!prototype->IsObject()) {
    exception_state.ThrowTypeError("constructor prototype is not an object");
    return v8::Local<v8::Object>();
  }
  prototype_object_ = prototype.As<v8::Object>();
  return prototype_object_;
}

v8::Local<v8::Value> CallbackMethodRetriever::GetFunctionOrUndefined(
    v8::Local<v8::Object> object,
    const StringView& property,
    ExceptionState& exception_state) {
  DCHECK(prototype_object_->IsObject());

  TryRethrowScope rethrow_scope(isolate_, exception_state);
  v8::Local<v8::Value> value;
  if (!object->Get(current_context_, V8AtomicString(isolate_, property))
           .ToLocal(&value)) {
    return v8::Local<v8::Function>();
  }
  if (!value->IsUndefined() && !value->IsFunction()) {
    exception_state.ThrowTypeError(
        String::Format("\"%s\" is not a function", property.Characters8()));
    return v8::Local<v8::Function>();
  }
  return value;
}

v8::Local<v8::Function> CallbackMethodRetriever::GetFunctionOrThrow(
    v8::Local<v8::Object> object,
    const StringView& property,
    ExceptionState& exception_state) {
  v8::Local<v8::Value> value =
      GetFunctionOrUndefined(object, property, exception_state);
  if (exception_state.HadException())
    return v8::Local<v8::Function>();
  if (value->IsUndefined()) {
    exception_state.ThrowTypeError(String::Format(
        "Property \"%s\" doesn't exist", property.Characters8()));
    return v8::Local<v8::Function>();
  }
  return value.As<v8::Function>();
}

}  // namespace blink
