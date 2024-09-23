// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_memory_heap_info.h"

namespace blink {

GPUMemoryHeapInfo::GPUMemoryHeapInfo(const wgpu::MemoryHeapInfo& info)
    : info_(info) {}

uint64_t GPUMemoryHeapInfo::size() const {
  return info_.size;
}

uint32_t GPUMemoryHeapInfo::properties() const {
  return static_cast<uint32_t>(info_.properties);
}

}  // namespace blink
