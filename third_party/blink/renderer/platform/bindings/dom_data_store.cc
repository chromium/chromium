// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/dom_data_store.h"

namespace blink {

DOMDataStore::DOMDataStore(v8::Isolate* isolate, bool can_use_inline_storage)
    : can_use_inline_storage_(can_use_inline_storage) {}

void DOMDataStore::Dispose() {
  for (auto& it : wrapper_map_) {
    // Explicitly reset references so that a following V8 GC will not find them
    // and treat them as roots. There's optimizations (see
    // EmbedderHeapTracer::IsRootForNonTracingGC) that would not treat them as
    // roots and then Blink would not be able to find and remove them from a DOM
    // world. Explicitly resetting on disposal avoids that problem
    it.value.Reset();
  }
}

void DOMDataStore::Trace(Visitor* visitor) const {
  visitor->Trace(wrapper_map_);
}

}  // namespace blink
