// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_HANDLE_H_
#define GIN_HANDLE_H_

#include "base/memory/raw_ptr.h"
#include "gin/converter.h"

namespace gin {

// You can use gin::Handle on the stack to retain a gin::Wrappable object.
// Currently we don't have a mechanism for retaining a gin::Wrappable object
// in the C++ heap because strong references from C++ to V8 can cause memory
// leaks.
template<typename T>
class Handle {
 public:
  Handle() : object_(nullptr) {}

  Handle(v8::Local<v8::Value> wrapper, T* object)
    : wrapper_(wrapper),
      object_(object) {
  }

  bool IsEmpty() const { return !object_; }

  void Clear() {
    wrapper_.Clear();
    object_ = NULL;
  }

  T* operator->() const { return object_; }
  v8::Local<v8::Value> ToV8() const { return wrapper_; }
  T* get() const { return object_; }

 private:
  v8::Local<v8::Value> wrapper_;
  raw_ptr<T> object_;
};

template<typename T>
struct Converter<gin::Handle<T> > {
  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate,
                                    const gin::Handle<T>& val) {
    return val.ToV8();
  }
  static bool FromV8(v8::Isolate* isolate, v8::Local<v8::Value> val,
                     gin::Handle<T>* out) {
    T* object = NULL;
    if (!Converter<T*>::FromV8(isolate, val, &object)) {
      return false;
    }
    *out = gin::Handle<T>(val, object);
    return true;
  }
};

// This function is a convenient way to create a handle from a raw pointer
// without having to write out the type of the object explicitly.
template<typename T>
gin::Handle<T> CreateHandle(v8::Isolate* isolate, T* object) {
  v8::Local<v8::Object> wrapper;
  if (!object->GetWrapper(isolate).ToLocal(&wrapper) || wrapper.IsEmpty())
    return gin::Handle<T>();
  return gin::Handle<T>(wrapper, object);
}

}  // namespace gin

#endif  // GIN_HANDLE_H_
