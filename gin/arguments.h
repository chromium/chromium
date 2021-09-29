// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_ARGUMENTS_H_
#define GIN_ARGUMENTS_H_

#include "gin/converter.h"
#include "gin/gin_export.h"

namespace gin {

// Arguments is a wrapper around v8::FunctionCallbackInfo that integrates
// with Converter to make it easier to marshall arguments and return values
// between V8 and C++.
//
// If constructed instead with a v8::PropertyCallbackInfo, behaves as though a
// function with no arguments had been called.
class GIN_EXPORT Arguments {
 public:
  Arguments();
  explicit Arguments(const v8::FunctionCallbackInfo<v8::Value>& info);
  explicit Arguments(const v8::PropertyCallbackInfo<v8::Value>& info);
  ~Arguments();

  template <typename T>
  bool GetHolder(T* out) const {
    v8::Local<v8::Object> holder = is_for_property_
                                       ? info_for_property_->Holder()
                                       : info_for_function_->Holder();
    return ConvertFromV8(isolate_, holder, out);
  }

  template<typename T>
  bool GetData(T* out) {
    v8::Local<v8::Value> data = is_for_property_ ? info_for_property_->Data()
                                                 : info_for_function_->Data();
    return ConvertFromV8(isolate_, data, out);
  }

  template<typename T>
  bool GetNext(T* out) {
    if (is_for_property_ || next_ >= info_for_function_->Length()) {
      insufficient_arguments_ = true;
      return false;
    }
    v8::Local<v8::Value> val = (*info_for_function_)[next_++];
    return ConvertFromV8(isolate_, val, out);
  }

  template<typename T>
  bool GetRemaining(std::vector<T>* out) {
    if (is_for_property_ || next_ >= info_for_function_->Length()) {
      insufficient_arguments_ = true;
      return false;
    }
    int remaining = info_for_function_->Length() - next_;
    out->resize(remaining);
    for (int i = 0; i < remaining; ++i) {
      v8::Local<v8::Value> val = (*info_for_function_)[next_++];
      if (!ConvertFromV8(isolate_, val, &out->at(i)))
        return false;
    }
    return true;
  }

  bool Skip() {
    if (is_for_property_)
      return false;
    if (next_ >= info_for_function_->Length())
      return false;
    next_++;
    return true;
  }

  int Length() const {
    return is_for_property_ ? 0 : info_for_function_->Length();
  }

  template<typename T>
  void Return(T val) {
    v8::Local<v8::Value> v8_value;
    if (!TryConvertToV8(isolate_, val, &v8_value))
      return;
    (is_for_property_ ? info_for_property_->GetReturnValue()
                      : info_for_function_->GetReturnValue())
        .Set(v8_value);
  }

  // Returns the creation context of the Holder.
  v8::Local<v8::Context> GetHolderCreationContext() const;

  // Always check the return value whether the handle is empty before
  // dereferencing the handle.
  v8::Local<v8::Value> PeekNext() const;

  // Returns all arguments. Since this doesn't require any conversion, it
  // cannot fail. This does not rely on or modify the current position in the
  // array used by Get/PeekNext().
  std::vector<v8::Local<v8::Value>> GetAll() const;

  void ThrowError() const;
  void ThrowTypeError(const std::string& message) const;

  v8::Isolate* isolate() const { return isolate_; }

  // Allows the function handler to distinguish between normal invocation
  // and object construction.
  bool IsConstructCall() const;

 private:
  v8::Isolate* isolate_;
  union {
    const v8::FunctionCallbackInfo<v8::Value>* info_for_function_;
    const v8::PropertyCallbackInfo<v8::Value>* info_for_property_;
  };
  int next_ = 0;
  bool insufficient_arguments_ = false;
  bool is_for_property_ = false;
};

}  // namespace gin

#endif  // GIN_ARGUMENTS_H_
