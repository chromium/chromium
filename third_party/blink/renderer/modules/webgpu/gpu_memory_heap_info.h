// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_MEMORY_HEAP_INFO_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_MEMORY_HEAP_INFO_H_

#include <dawn/webgpu.h>

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class GPUMemoryHeapInfo : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit GPUMemoryHeapInfo(const WGPUMemoryHeapInfo&);

  GPUMemoryHeapInfo(const GPUMemoryHeapInfo&) = delete;
  GPUMemoryHeapInfo& operator=(const GPUMemoryHeapInfo&) = delete;

  // gpu_memory_heap_info.idl
  const uint64_t& size() const;
  const uint32_t& properties() const;

 private:
  uint64_t size_;
  uint32_t properties_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_MEMORY_HEAP_INFO_H_
