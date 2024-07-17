// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_MEMORY_REGION_WRAPPER_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_MEMORY_REGION_WRAPPER_H_

#include "base/containers/span.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/unguessable_token.h"
#include "gpu/gpu_gles2_export.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
struct GpuMemoryBufferHandle;
}

namespace gpu {

// Wrapper for shared memory region from a GpuMemoryBuffer with type
// SHARED_MEMORY_BUFFER.
class GPU_GLES2_EXPORT SharedMemoryRegionWrapper {
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
                  gfx::BufferFormat format);

  bool IsValid() const;
  uint8_t* GetMemory(int plane_index) const;
  size_t GetStride(int plane_index) const;

  // Returns SkPixmap pointing to memory for offset.
  SkPixmap MakePixmapForPlane(const SkImageInfo& info, int plane_index) const;

  const base::UnguessableToken& GetMappingGuid() const;

 private:
  struct PlaneData {
    size_t offset = 0;
    size_t stride = 0;
  };

  base::WritableSharedMemoryMapping mapping_;
  std::vector<PlaneData> planes_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_MEMORY_REGION_WRAPPER_H_
