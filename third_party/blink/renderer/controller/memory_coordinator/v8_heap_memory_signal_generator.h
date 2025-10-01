// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CONTROLLER_MEMORY_COORDINATOR_V8_HEAP_MEMORY_SIGNAL_GENERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CONTROLLER_MEMORY_COORDINATOR_V8_HEAP_MEMORY_SIGNAL_GENERATOR_H_

#include "third_party/blink/renderer/controller/controller_export.h"

namespace v8 {
class Isolate;
}

namespace blink {

// Notifies the platform implementation when the V8 heap is running its last
// resort GC.
class CONTROLLER_EXPORT V8HeapMemorySignalGenerator {
 public:
  static void Initialize(v8::Isolate*);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CONTROLLER_MEMORY_COORDINATOR_V8_HEAP_MEMORY_SIGNAL_GENERATOR_H_
