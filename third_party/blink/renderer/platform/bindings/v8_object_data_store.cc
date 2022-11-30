// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/v8_object_data_store.h"

namespace blink {

V8ObjectDataStore::value_type V8ObjectDataStore::Get(v8::Isolate* isolate,
                                                     key_type key) {
  auto it = v8_object_map_.find(key);
  if (it == v8_object_map_.end()) {
    return value_type();
  }
  return it->value.Get(isolate);
}

void V8ObjectDataStore::Set(v8::Isolate* isolate,
                            key_type key,
                            value_type value) {
  v8_object_map_.insert(key,
                        TraceWrapperV8Reference<v8::Object>(isolate, value));
}

void V8ObjectDataStore::Trace(Visitor* visitor) const {
  visitor->Trace(v8_object_map_);
}

}  // namespace blink
