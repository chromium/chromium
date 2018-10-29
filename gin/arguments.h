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
class GIN_EXPORT Arguments {
 public:
  Arguments();
  explicit Arguments(const v8::FunctionCallbackInfo<v8::Value>& info);
  ~Arguments();

  template <typename T>
  bool GetHolder(T* out) const {
    return ConvertFromV8(isolate_, info_->Holder(), out);
  }

  template<typename T>
  bool GetData(T* out) {
    return ConvertFromV8(isolate_, info_->Data(), out);
  }

  template<typename T>
  bool GetNext(T* out) {
    if (next_ >= info_->Length()) {
      insufficient_arguments_ = true;
      return false;
    }
    v8::Local<v8::Value> val = (*info_)[next_++];
    return ConvertFromV8(isolate_, val, out);
  }

  template<typename T>
  bool GetRemaining(std::vector<T>* out) {
    if (next_ >= info_->Length()) {
      insufficient_arguments_ = true;
      return false;
    }
    int remaining = info_->Length() - next_;
    out->resize(remaining);
    for (int i = 0; i < remaining; ++i) {
      v8::Local<v8::Value> val = (*info_)[next_++];
      if (!ConvertFromV8(isolate_, val, &out->at(i)))
        return false;
    }
    return true;
  }

  bool Skip() {
    if (next_ >= info_->Length())
      return false;
    next_++;
    return true;
  }

  int Length() const {
    return info_->Length();
  }

  template<typename T>
  void Return(T val) {
    v8::Local<v8::Value> v8_value;
    if (!TryConvertToV8(isolate_, val, &v8_value))
      return;
    info_->GetReturnValue().Set(v8_value);
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
  const v8::FunctionCallbackInfo<v8::Value>* info_;
  int next_;
  bool insufficient_arguments_;
};

}  // namespace gin

#endif  // GIN_ARGUMENTS_H_
