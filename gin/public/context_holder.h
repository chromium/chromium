// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_PUBLIC_CONTEXT_HOLDER_H_
#define GIN_PUBLIC_CONTEXT_HOLDER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "gin/gin_export.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-persistent-handle.h"

namespace gin {

// Gin embedder that store embedder data in v8::Contexts must do so in a
// single field with the index kPerContextDataStartIndex + GinEmbedder-enum.
// The field at kDebugIdIndex is treated specially by V8 and is reserved for
// a V8 debugger implementation (not used by gin).
enum ContextEmbedderDataFields {
  kDebugIdIndex = v8::Context::kDebugIdIndex,
  kPerContextDataStartIndex,
};

class PerContextData;

// ContextHolder is a generic class for holding a v8::Context.
class GIN_EXPORT ContextHolder {
 public:
  explicit ContextHolder(v8::Isolate* isolate);
  ContextHolder(const ContextHolder&) = delete;
  ContextHolder& operator=(const ContextHolder&) = delete;
  ~ContextHolder();

  v8::Isolate* isolate() const { return isolate_; }

  v8::Local<v8::Context> context() const {
    return v8::Local<v8::Context>::New(isolate_, context_);
  }

  void SetContext(v8::Local<v8::Context> context);

 private:
  raw_ptr<v8::Isolate> isolate_;
  v8::UniquePersistent<v8::Context> context_;
  std::unique_ptr<PerContextData> data_;
};

}  // namespace gin

#endif  // GIN_PUBLIC_CONTEXT_HOLDER_H_
