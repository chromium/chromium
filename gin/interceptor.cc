// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/interceptor.h"

#include <stdint.h>

#include <map>
namespace gin {

v8::Local<v8::Value> NamedPropertyInterceptor::GetNamedProperty(
    v8::Isolate* isolate,
    const std::string& property) {
  return v8::Local<v8::Value>();
}

bool NamedPropertyInterceptor::SetNamedProperty(v8::Isolate* isolate,
                                                const std::string& property,
                                                v8::Local<v8::Value> value) {
  return false;
}

std::vector<std::string> NamedPropertyInterceptor::EnumerateNamedProperties(
    v8::Isolate* isolate) {
  return std::vector<std::string>();
}

}  // namespace gin
