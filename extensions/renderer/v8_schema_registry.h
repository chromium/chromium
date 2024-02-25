// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_V8_SCHEMA_REGISTRY_H_
#define EXTENSIONS_RENDERER_V8_SCHEMA_REGISTRY_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "gin/public/context_holder.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-util.h"

namespace extensions {
class NativeHandler;

// A registry for the v8::Value representations of extension API schemas.
// In a way, the v8 counterpart to ExtensionAPI.
class V8SchemaRegistry {
 public:
  V8SchemaRegistry();

  V8SchemaRegistry(const V8SchemaRegistry&) = delete;
  V8SchemaRegistry& operator=(const V8SchemaRegistry&) = delete;

  ~V8SchemaRegistry();

  // Creates a NativeHandler wrapper |this|. Supports GetSchema.
  std::unique_ptr<NativeHandler> AsNativeHandler(v8::Isolate* isolate);

  // Returns a v8::Array with all the schemas for the APIs in |apis|.
  v8::Local<v8::Array> GetSchemas(v8::Isolate* isolate,
                                  const std::vector<std::string>& apis);

  // Returns a v8::Object for the schema for |api|, possibly from the cache.
  v8::Local<v8::Object> GetSchema(v8::Isolate* isolate, const std::string& api);

 private:
  // Gets the separate context that backs the registry, creating a new one if
  // if necessary. Will also initialize schema_cache_.
  v8::Local<v8::Context> GetOrCreateContext(v8::Isolate* isolate);

  // Cache of schemas. Created lazily by GetOrCreateContext.
  typedef v8::StdGlobalValueMap<std::string, v8::Object> SchemaCache;
  std::unique_ptr<SchemaCache> schema_cache_;

  // Single per-instance gin::ContextHolder to create v8::Values.
  // Created lazily via GetOrCreateContext.
  std::unique_ptr<gin::ContextHolder> context_holder_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_V8_SCHEMA_REGISTRY_H_
