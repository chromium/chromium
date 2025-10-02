// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GPU_MEMORY_BUFFER_FACTORY_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GPU_MEMORY_BUFFER_FACTORY_H_

#include "base/memory/unsafe_shared_memory_region.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/gpu_gles2_export.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"

namespace gpu {

class GPU_GLES2_EXPORT GpuMemoryBufferFactory {
 public:
  GpuMemoryBufferFactory() = default;
  GpuMemoryBufferFactory(const GpuMemoryBufferFactory&) = delete;
  GpuMemoryBufferFactory& operator=(const GpuMemoryBufferFactory&) = delete;

  virtual ~GpuMemoryBufferFactory() = default;

#if !BUILDFLAG(IS_ANDROID)
  // Creation of native buffer handles is not supported on Android (the
  // only way that a non-null GpuMemoryBufferHandle can be created on
  // Android is by importing an external AHB).
  // Creates a native GpuMemoryBufferHandle for MappableSI.
  virtual gfx::GpuMemoryBufferHandle CreateNativeGmbHandle(
      const gfx::Size& size,
      viz::SharedImageFormat format,
      gfx::BufferUsage usage) = 0;
#endif

#if BUILDFLAG(IS_WIN)
  // Fills |shared_memory| with the contents of the provided |buffer_handle|.
  // Returns whether the operation succeeded.
  virtual bool FillSharedMemoryRegionWithBufferContents(
      gfx::GpuMemoryBufferHandle buffer_handle,
      base::UnsafeSharedMemoryRegion shared_memory) = 0;
#endif
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GPU_MEMORY_BUFFER_FACTORY_H_
