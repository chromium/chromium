// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_INTERCEPTOR_H_
#define GIN_INTERCEPTOR_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "gin/gin_export.h"
#include "v8/include/v8.h"

namespace gin {

class WrappableBase;

// Base class for gin::Wrappable-derived classes that want to implement a
// property interceptor.
class GIN_EXPORT NamedPropertyInterceptor {
 public:
  NamedPropertyInterceptor(v8::Isolate* isolate, WrappableBase* base);
  virtual ~NamedPropertyInterceptor();

  virtual v8::Local<v8::Value> GetNamedProperty(v8::Isolate* isolate,
                                                const std::string& property);
  // Return true if the set was interecepted.
  virtual bool SetNamedProperty(v8::Isolate* isolate,
                                const std::string& property,
                                v8::Local<v8::Value> value);
  virtual std::vector<std::string> EnumerateNamedProperties(
      v8::Isolate* isolate);

 private:
  v8::Isolate* isolate_;
  WrappableBase* base_;

  DISALLOW_COPY_AND_ASSIGN(NamedPropertyInterceptor);
};

class GIN_EXPORT IndexedPropertyInterceptor {
 public:
  IndexedPropertyInterceptor(v8::Isolate* isolate, WrappableBase* base);
  virtual ~IndexedPropertyInterceptor();

  virtual v8::Local<v8::Value> GetIndexedProperty(v8::Isolate* isolate,
                                                  uint32_t index);
  // Return true if the set was interecepted.
  virtual bool SetIndexedProperty(v8::Isolate* isolate,
                                  uint32_t index,
                                  v8::Local<v8::Value> value);
  virtual std::vector<uint32_t> EnumerateIndexedProperties(
      v8::Isolate* isolate);

 private:
  v8::Isolate* isolate_;
  WrappableBase* base_;

  DISALLOW_COPY_AND_ASSIGN(IndexedPropertyInterceptor);
};

}  // namespace gin

#endif  // GIN_INTERCEPTOR_H_
