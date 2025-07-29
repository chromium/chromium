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
#include "gin/wrappable.h"
#include "v8/include/v8-forward.h"

namespace gin {

class GIN_EXPORT NamedPropertyInterceptor {
 public:
  NamedPropertyInterceptor() = default;
  NamedPropertyInterceptor(const NamedPropertyInterceptor&) = delete;
  NamedPropertyInterceptor& operator=(const NamedPropertyInterceptor&) = delete;
  virtual ~NamedPropertyInterceptor() = default;

  // Return non-empty handle if the get was interecepted.
  virtual v8::Local<v8::Value> GetNamedProperty(v8::Isolate* isolate,
                                                const std::string& property);
  // Return true if the set was interecepted.
  virtual bool SetNamedProperty(v8::Isolate* isolate,
                                const std::string& property,
                                v8::Local<v8::Value> value);
  virtual std::vector<std::string> EnumerateNamedProperties(
      v8::Isolate* isolate);
};

// Base class for gin::Wrappable-derived classes that want to implement a
// property interceptor.
template <typename T>
class GIN_EXPORT WrappableWithNamedPropertyInterceptor
    : public Wrappable<T>,
      public NamedPropertyInterceptor {
 public:
  WrappableWithNamedPropertyInterceptor() = default;
  WrappableWithNamedPropertyInterceptor(
      const WrappableWithNamedPropertyInterceptor&) = delete;
  WrappableWithNamedPropertyInterceptor& operator=(
      const WrappableWithNamedPropertyInterceptor&) = delete;
  ~WrappableWithNamedPropertyInterceptor() override = default;
  NamedPropertyInterceptor* GetNamedPropertyInterceptor() override {
    return static_cast<NamedPropertyInterceptor*>(this);
  }
};

}  // namespace gin

#endif  // GIN_INTERCEPTOR_H_
