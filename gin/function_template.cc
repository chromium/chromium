// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/function_template.h"

#include "base/strings/strcat.h"
#include "gin/converter.h"
#include "v8/include/v8-primitive.h"

namespace gin::internal {

CallbackHolderBase::CallbackHolderBase(uintptr_t type_identifier)
    : type_identifier_(type_identifier) {}

CallbackHolderBase::~CallbackHolderBase() = default;

v8::Local<v8::CppHeapExternal> CallbackHolderBase::GetHandle(
    v8::Isolate* isolate) {
  return v8::CppHeapExternal::New<CallbackHolderBase>(
      isolate, this, static_cast<v8::CppHeapPointerTag>(kCallbackHolderBase));
}

// static
CallbackHolderBase* CallbackHolderBase::FromV8(v8::Isolate* isolate,
                                               v8::Local<v8::Data> data) {
  if (data.IsEmpty() || !data->IsCppHeapExternal()) {
    return nullptr;
  }
  constexpr auto kTag = static_cast<v8::CppHeapPointerTag>(kCallbackHolderBase);
  return v8::CppHeapExternal::Cast(*data)->Value<CallbackHolderBase>(
      isolate, {kTag, kTag});
}

v8::Local<v8::Value> HolderTypeData(v8::Isolate* isolate,
                                    const char* holder_type) {
  if (!holder_type) {
    return v8::Local<v8::Value>();
  }
  return StringToSymbol(isolate, holder_type);
}

void ThrowConversionError(Arguments* args,
                          const InvokerOptions& invoker_options,
                          size_t index) {
  if (index == 0 && invoker_options.holder_is_first_argument) {
    // Failed to get the appropriate `this` object. This can happen if a
    // method is invoked using Function.prototype.[call|apply] and passed an
    // invalid (or null) `this` argument.
    std::string holder_type;
    if (invoker_options.holder_type) {
      holder_type = invoker_options.holder_type;
    } else {
      // Templates made by CreateFunctionTemplate<kTarget>() keep the type
      // name in their data so that the successful path never reads it.
      v8::Local<v8::Value> data;
      if (args->GetData(&data) && !data.IsEmpty() && data->IsString()) {
        holder_type = V8ToString(args->isolate(), data);
      }
    }
    std::string error =
        holder_type.empty()
            ? "Illegal invocation"
            : base::StrCat({"Illegal invocation: Function must be "
                            "called on an object of type ",
                            holder_type});
    args->ThrowTypeError(error);
  } else {
    // Otherwise, this failed parsing on a different argument.
    // Arguments::ThrowError() will try to include appropriate information.
    // Ideally we would include the expected c++ type in the error message
    // here, too (which we can access via typeid(ArgType).name()), however we
    // compile with no-rtti, which disables typeid.
    args->ThrowError();
  }
}

}  // namespace gin::internal
