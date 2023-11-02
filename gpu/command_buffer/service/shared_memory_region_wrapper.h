// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_MEMORY_REGION_WRAPPER_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_MEMORY_REGION_WRAPPER_H_

#include "base/containers/span.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/unguessable_token.h"
#include "components/viz/common/resources/resource_format.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
struct GpuMemoryBufferHandle;
}

namespace gpu {

// Wrapper for shared memory region from a GpuMemoryBuffer with type
// SHARED_MEMORY_BUFFER.
class SharedMemoryRegionWrapper {
 public:
  SharedMemoryRegionWrapper();
  SharedMemoryRegionWrapper(SharedMemoryRegionWrapper&& other);
  SharedMemoryRegionWrapper& operator=(SharedMemoryRegionWrapper&& other);
  ~SharedMemoryRegionWrapper();

  // Validates that size, stride and format parameters make sense and maps
  // memory for shared memory owned by |handle|. Shared memory stays mapped
  // until destruction.
  bool Initialize(const gfx::GpuMemoryBufferHandle& handle,
                  const gfx::Size& size,
                  viz::ResourceFormat format);

  bool IsValid() const;
  uint8_t* GetMemory() const;
  base::span<const uint8_t> GetMemoryAsSpan() const;
  size_t GetStride() const;
  const base::UnguessableToken& GetMappingGuid();

 private:
  base::WritableSharedMemoryMapping mapping_;
  size_t offset_ = 0;
  size_t stride_ = 0;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_MEMORY_REGION_WRAPPER_H_
