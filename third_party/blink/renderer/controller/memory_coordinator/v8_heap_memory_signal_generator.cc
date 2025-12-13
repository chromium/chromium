// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/memory_coordinator/v8_heap_memory_signal_generator.h"

#include "third_party/blink/public/platform/platform.h"
#include "v8/include/v8-isolate.h"

namespace blink {

namespace {

void OnV8HeapLastResortGC(v8::Isolate* isolate,
                          v8::GCType gc_type,
                          v8::GCCallbackFlags flags) {
  if (flags & v8::kGCCallbackFlagLastResort) {
    blink::Platform::Current()->OnV8HeapLastResortGC();
  }
}

}  // namespace

// static
void V8HeapMemorySignalGenerator::Initialize(v8::Isolate* isolate) {
  isolate->AddGCPrologueCallback(&OnV8HeapLastResortGC);
}

}  // namespace blink
