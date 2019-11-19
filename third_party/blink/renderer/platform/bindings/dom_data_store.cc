// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/dom_data_store.h"

namespace blink {

DOMDataStore::DOMDataStore(v8::Isolate* isolate, bool is_main_world)
    : is_main_world_(is_main_world) {}

void DOMDataStore::Dispose() {
  for (const auto& it : wrapper_map_) {
    // Explicitly reset references so that a following V8 GC will not find them
    // and treat them as roots. There's optimizations (see
    // EmbedderHeapTracer::IsRootForNonTracingGC) that would not treat them as
    // roots and then Blink would not be able to find and remove them from a DOM
    // world. Explicitly resetting on disposal avoids that problem
    it.value->ref.Clear();
  }
}

void DOMDataStore::WrappedReference::Trace(Visitor* visitor) {
  visitor->Trace(ref);
}

void DOMDataStore::Trace(Visitor* visitor) {
  visitor->Trace(wrapper_map_);
}

}  // namespace blink
