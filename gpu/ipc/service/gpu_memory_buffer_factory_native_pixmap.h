// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_GPU_MEMORY_BUFFER_FACTORY_NATIVE_PIXMAP_H_
#define GPU_IPC_SERVICE_GPU_MEMORY_BUFFER_FACTORY_NATIVE_PIXMAP_H_

#include "gpu/ipc/service/gpu_ipc_service_export.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "ui/gfx/native_pixmap.h"

namespace gpu {

class GPU_IPC_SERVICE_EXPORT GpuMemoryBufferFactoryNativePixmap
    : public GpuMemoryBufferFactory {
 public:
  GpuMemoryBufferFactoryNativePixmap();
  explicit GpuMemoryBufferFactoryNativePixmap(
      viz::VulkanContextProvider* vulkan_context_provider);

  GpuMemoryBufferFactoryNativePixmap(
      const GpuMemoryBufferFactoryNativePixmap&) = delete;
  GpuMemoryBufferFactoryNativePixmap& operator=(
      const GpuMemoryBufferFactoryNativePixmap&) = delete;

  ~GpuMemoryBufferFactoryNativePixmap() override;

  // Overridden from GpuMemoryBufferFactory:
  gfx::GpuMemoryBufferHandle CreateNativeGmbHandle(
      const gfx::Size& size,
      viz::SharedImageFormat format,
      gfx::BufferUsage usage) override;

 private:
  scoped_refptr<viz::VulkanContextProvider> vulkan_context_provider_;
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_GPU_MEMORY_BUFFER_FACTORY_NATIVE_PIXMAP_H_
