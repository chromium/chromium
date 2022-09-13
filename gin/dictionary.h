// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_DICTIONARY_H_
#define GIN_DICTIONARY_H_

#include "base/memory/raw_ptr.h"
#include "gin/converter.h"
#include "gin/gin_export.h"

namespace gin {

// Dictionary is useful when writing bindings for a function that either
// receives an arbitrary JavaScript object as an argument or returns an
// arbitrary JavaScript object as a result. For example, Dictionary is useful
// when you might use the |dictionary| type in WebIDL:
//
//   https://webidl.spec.whatwg.org/#idl-dictionaries
//
// WARNING: You cannot retain a Dictionary object in the heap. The underlying
//          storage for Dictionary is tied to the closest enclosing
//          v8::HandleScope. Generally speaking, you should store a Dictionary
//          on the stack.
//
class GIN_EXPORT Dictionary {
 public:
  explicit Dictionary(v8::Isolate* isolate);
  Dictionary(v8::Isolate* isolate, v8::Local<v8::Object> object);
  Dictionary(const Dictionary& other);
  ~Dictionary();

  static Dictionary CreateEmpty(v8::Isolate* isolate);

  template<typename T>
  bool Get(const std::string& key, T* out) {
    v8::Local<v8::Value> val;
    if (!object_->Get(isolate_->GetCurrentContext(), StringToV8(isolate_, key))
             .ToLocal(&val)) {
      return false;
    }
    return ConvertFromV8(isolate_, val, out);
  }

  template <typename T>
  bool Set(const std::string& key, const T& val) {
    v8::Local<v8::Value> v8_value;
    if (!TryConvertToV8(isolate_, val, &v8_value))
      return false;
    v8::Maybe<bool> result =
        object_->Set(isolate_->GetCurrentContext(), StringToV8(isolate_, key),
                    v8_value);
    return !result.IsNothing() && result.FromJust();
  }

  v8::Isolate* isolate() const { return isolate_; }

 private:
  friend struct Converter<Dictionary>;

  // TODO(aa): Remove this. Instead, get via FromV8(), Set(), and Get().
  raw_ptr<v8::Isolate> isolate_;
  v8::Local<v8::Object> object_;
};

template<>
struct GIN_EXPORT Converter<Dictionary> {
  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate,
                                    Dictionary val);
  static bool FromV8(v8::Isolate* isolate,
                     v8::Local<v8::Value> val,
                     Dictionary* out);
};

}  // namespace gin

#endif  // GIN_DICTIONARY_H_
