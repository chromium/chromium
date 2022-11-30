// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/dictionary.h"

namespace gin {

Dictionary::Dictionary(v8::Isolate* isolate)
    : isolate_(isolate) {
}

Dictionary::Dictionary(v8::Isolate* isolate,
                       v8::Local<v8::Object> object)
    : isolate_(isolate),
      object_(object) {
}

Dictionary::Dictionary(const Dictionary& other) = default;

Dictionary::~Dictionary() = default;

Dictionary Dictionary::CreateEmpty(v8::Isolate* isolate) {
  Dictionary dictionary(isolate);
  dictionary.object_ = v8::Object::New(isolate);
  return dictionary;
}

v8::Local<v8::Value> Converter<Dictionary>::ToV8(v8::Isolate* isolate,
                                                  Dictionary val) {
  return val.object_;
}

bool Converter<Dictionary>::FromV8(v8::Isolate* isolate,
                                   v8::Local<v8::Value> val,
                                   Dictionary* out) {
  if (!val->IsObject())
    return false;
  *out = Dictionary(isolate, v8::Local<v8::Object>::Cast(val));
  return true;
}

}  // namespace gin
