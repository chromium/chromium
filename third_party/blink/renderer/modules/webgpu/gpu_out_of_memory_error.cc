// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_out_of_memory_error.h"

namespace blink {

// static
GPUOutOfMemoryError* GPUOutOfMemoryError::Create(const String& message) {
  return MakeGarbageCollected<GPUOutOfMemoryError>(message);
}

GPUOutOfMemoryError::GPUOutOfMemoryError(const String& message)
    : GPUError(message) {}

}  // namespace blink
