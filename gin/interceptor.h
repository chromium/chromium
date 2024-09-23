// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_INTERCEPTOR_H_
#define GIN_INTERCEPTOR_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "gin/gin_export.h"
#include "v8/include/v8-forward.h"

namespace gin {

class WrappableBase;

// Base class for gin::Wrappable-derived classes that want to implement a
// property interceptor.
class GIN_EXPORT NamedPropertyInterceptor {
 public:
  NamedPropertyInterceptor(v8::Isolate* isolate, WrappableBase* base);
  NamedPropertyInterceptor(const NamedPropertyInterceptor&) = delete;
  NamedPropertyInterceptor& operator=(const NamedPropertyInterceptor&) = delete;
  virtual ~NamedPropertyInterceptor();

  // Return non-empty handle if the get was interecepted.
  virtual v8::Local<v8::Value> GetNamedProperty(v8::Isolate* isolate,
                                                const std::string& property);
  // Return true if the set was interecepted.
  virtual bool SetNamedProperty(v8::Isolate* isolate,
                                const std::string& property,
                                v8::Local<v8::Value> value);
  virtual std::vector<std::string> EnumerateNamedProperties(
      v8::Isolate* isolate);

  void ClearForTesting();

 private:
  raw_ptr<v8::Isolate> isolate_;
  raw_ptr<WrappableBase> base_;
};

class GIN_EXPORT IndexedPropertyInterceptor {
 public:
  IndexedPropertyInterceptor(v8::Isolate* isolate, WrappableBase* base);
  IndexedPropertyInterceptor(const IndexedPropertyInterceptor&) = delete;
  IndexedPropertyInterceptor& operator=(const IndexedPropertyInterceptor&) =
      delete;
  virtual ~IndexedPropertyInterceptor();

  // Return non-empty handle if the get was interecepted.
  virtual v8::Local<v8::Value> GetIndexedProperty(v8::Isolate* isolate,
                                                  uint32_t index);
  // Return true if the set was interecepted.
  virtual bool SetIndexedProperty(v8::Isolate* isolate,
                                  uint32_t index,
                                  v8::Local<v8::Value> value);
  virtual std::vector<uint32_t> EnumerateIndexedProperties(
      v8::Isolate* isolate);

  void ClearForTesting();

 private:
  raw_ptr<v8::Isolate> isolate_;
  raw_ptr<WrappableBase> base_;
};

}  // namespace gin

#endif  // GIN_INTERCEPTOR_H_
