// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_memory_heap_info.h"

namespace blink {

GPUMemoryHeapInfo::GPUMemoryHeapInfo(const WGPUMemoryHeapInfo& info)
    : size_(info.size), properties_(info.properties) {}

const uint64_t& GPUMemoryHeapInfo::size() const {
  return size_;
}

const uint32_t& GPUMemoryHeapInfo::properties() const {
  return properties_;
}

}  // namespace blink
