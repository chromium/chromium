// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/arguments.h"

#include "base/strings/stringprintf.h"
#include "gin/converter.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-template.h"

namespace gin {

Arguments::Arguments()
    : isolate_(nullptr), info_for_function_(nullptr), is_for_property_(false) {}

Arguments::Arguments(const v8::FunctionCallbackInfo<v8::Value>& info)
    : isolate_(info.GetIsolate()),
      info_for_function_(&info),
      is_for_property_(false) {}

Arguments::Arguments(const v8::PropertyCallbackInfo<v8::Value>& info)
    : isolate_(info.GetIsolate()),
      info_for_property_(&info),
      is_for_property_(true) {}

Arguments::~Arguments() = default;

v8::Local<v8::Value> Arguments::PeekNext() const {
  if (is_for_property_)
    return v8::Local<v8::Value>();
  if (next_ >= info_for_function_->Length())
    return v8::Local<v8::Value>();
  return (*info_for_function_)[next_];
}

v8::LocalVector<v8::Value> Arguments::GetAll() const {
  v8::LocalVector<v8::Value> result(isolate_);
  if (is_for_property_)
    return result;

  int length = info_for_function_->Length();
  if (length == 0)
    return result;

  result.reserve(length);
  for (int i = 0; i < length; ++i)
    result.push_back((*info_for_function_)[i]);

  return result;
}

v8::Local<v8::Context> Arguments::GetHolderCreationContext() const {
  v8::Local<v8::Object> holder = is_for_property_ ? info_for_property_->Holder()
                                                  : info_for_function_->This();
  return holder->GetCreationContextChecked(isolate_);
}

std::string V8TypeAsString(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  if (value.IsEmpty())
    return "<empty handle>";
  if (value->IsUndefined())
    return "undefined";
  if (value->IsNull())
    return "null";
  std::string result;
  if (!ConvertFromV8(isolate, value, &result))
    return std::string();
  return result;
}

void Arguments::ThrowError() const {
  if (is_for_property_)
    return ThrowTypeError("Error processing property accessor arguments.");

  if (insufficient_arguments_)
    return ThrowTypeError("Insufficient number of arguments.");

  v8::Local<v8::Value> value = (*info_for_function_)[next_ - 1];
  return ThrowTypeError(base::StringPrintf(
      "Error processing argument at index %d, conversion failure from %s",
      next_ - 1, V8TypeAsString(isolate_, value).c_str()));
}

void Arguments::ThrowTypeError(const std::string& message) const {
  isolate_->ThrowException(v8::Exception::TypeError(
      StringToV8(isolate_, message)));
}

bool Arguments::IsConstructCall() const {
  return !is_for_property_ && info_for_function_->IsConstructCall();
}

}  // namespace gin
