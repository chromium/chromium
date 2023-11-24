// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/interceptor.h"

#include <stdint.h>

#include <map>

#include "gin/per_isolate_data.h"

namespace gin {

NamedPropertyInterceptor::NamedPropertyInterceptor(v8::Isolate* isolate,
                                                   WrappableBase* base)
    : isolate_(isolate), base_(base) {
  PerIsolateData::From(isolate_)->SetNamedPropertyInterceptor(base_, this);
}

NamedPropertyInterceptor::~NamedPropertyInterceptor() {
  PerIsolateData::From(isolate_)->ClearNamedPropertyInterceptor(base_, this);
}

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

void NamedPropertyInterceptor::ClearForTesting() {
  PerIsolateData::From(isolate_)->ClearNamedPropertyInterceptor(base_, this);
  isolate_ = nullptr;
}

IndexedPropertyInterceptor::IndexedPropertyInterceptor(v8::Isolate* isolate,
                                                       WrappableBase* base)
    : isolate_(isolate), base_(base) {
  PerIsolateData::From(isolate_)->SetIndexedPropertyInterceptor(base_, this);
}

IndexedPropertyInterceptor::~IndexedPropertyInterceptor() {
  PerIsolateData::From(isolate_)->ClearIndexedPropertyInterceptor(base_, this);
}

v8::Local<v8::Value> IndexedPropertyInterceptor::GetIndexedProperty(
    v8::Isolate* isolate,
    uint32_t index) {
  return v8::Local<v8::Value>();
}

bool IndexedPropertyInterceptor::SetIndexedProperty(
    v8::Isolate* isolate,
    uint32_t index,
    v8::Local<v8::Value> value) {
  return false;
}

std::vector<uint32_t> IndexedPropertyInterceptor::EnumerateIndexedProperties(
    v8::Isolate* isolate) {
  return std::vector<uint32_t>();
}

void IndexedPropertyInterceptor::ClearForTesting() {
  PerIsolateData::From(isolate_)->ClearIndexedPropertyInterceptor(base_, this);
  isolate_ = nullptr;
}

}  // namespace gin
