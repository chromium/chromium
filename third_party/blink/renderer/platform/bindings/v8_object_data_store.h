// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_V8_OBJECT_DATA_STORE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_V8_OBJECT_DATA_STORE_H_

#include "third_party/blink/renderer/platform/bindings/multi_worlds_v8_reference.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "v8/include/v8.h"

namespace blink {

class V8ObjectDataStore final : public GarbageCollected<V8ObjectDataStore> {
 public:
  using key_type = MultiWorldsV8Reference*;
  using value_type = v8::Local<v8::Object>;

  V8ObjectDataStore() = default;
  V8ObjectDataStore(const V8ObjectDataStore&) = delete;
  V8ObjectDataStore& operator=(const V8ObjectDataStore&) = delete;

  value_type Get(v8::Isolate* isolate, key_type key);

  void Set(v8::Isolate* isolate, key_type key, value_type value);

  virtual void Trace(Visitor*) const;

 private:
  HeapHashMap<WeakMember<MultiWorldsV8Reference>,
              TraceWrapperV8Reference<v8::Object>>
      v8_object_map_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_V8_OBJECT_DATA_STORE_H_
