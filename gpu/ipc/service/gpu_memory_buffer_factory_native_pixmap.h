// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_GPU_MEMORY_BUFFER_FACTORY_NATIVE_PIXMAP_H_
#define GPU_IPC_SERVICE_GPU_MEMORY_BUFFER_FACTORY_NATIVE_PIXMAP_H_

#include <vulkan/vulkan.h>

#include <unordered_map>
#include <utility>

#include "base/hash/hash.h"
#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "gpu/ipc/service/gpu_ipc_service_export.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "ui/gfx/native_pixmap.h"

namespace gl {
class GLImage;
}

namespace gpu {

class GPU_IPC_SERVICE_EXPORT GpuMemoryBufferFactoryNativePixmap
    : public GpuMemoryBufferFactory,
      public ImageFactory {
 public:
  GpuMemoryBufferFactoryNativePixmap();
  explicit GpuMemoryBufferFactoryNativePixmap(
      viz::VulkanContextProvider* vulkan_context_provider);
  ~GpuMemoryBufferFactoryNativePixmap() override;

  // Overridden from GpuMemoryBufferFactory:
  gfx::GpuMemoryBufferHandle CreateGpuMemoryBuffer(
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      int client_id,
      SurfaceHandle surface_handle) override;
  void CreateGpuMemoryBufferAsync(
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      int client_id,
      SurfaceHandle surface_handle,
      CreateGpuMemoryBufferAsyncCallback callback) override;
  void DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                              int client_id) override;
  ImageFactory* AsImageFactory() override;

  // Overridden from ImageFactory:
  scoped_refptr<gl::GLImage> CreateImageForGpuMemoryBuffer(
      gfx::GpuMemoryBufferHandle handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      int client_id,
      SurfaceHandle surface_handle) override;
  bool SupportsCreateAnonymousImage() const override;
  scoped_refptr<gl::GLImage> CreateAnonymousImage(const gfx::Size& size,
                                                  gfx::BufferFormat format,
                                                  gfx::BufferUsage usage,
                                                  bool* is_cleared) override;
  unsigned RequiredTextureType() override;

 private:
  using NativePixmapMapKey = std::pair<int, int>;
  using NativePixmapMapKeyHash = base::IntPairHash<NativePixmapMapKey>;
  using NativePixmapMap = std::unordered_map<NativePixmapMapKey,
                                             scoped_refptr<gfx::NativePixmap>,
                                             NativePixmapMapKeyHash>;

  static void OnNativePixmapCreated(
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      int client_id,
      CreateGpuMemoryBufferAsyncCallback callback,
      base::WeakPtr<GpuMemoryBufferFactoryNativePixmap> weak_ptr,
      scoped_refptr<gfx::NativePixmap> pixmap);

  gfx::GpuMemoryBufferHandle CreateGpuMemoryBufferFromNativePixmap(
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      int client_id,
      scoped_refptr<gfx::NativePixmap> pixmap);

  VkDevice GetVulkanDevice();

  scoped_refptr<viz::VulkanContextProvider> vulkan_context_provider_;

  NativePixmapMap native_pixmaps_;
  base::Lock native_pixmaps_lock_;

  base::WeakPtrFactory<GpuMemoryBufferFactoryNativePixmap> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GpuMemoryBufferFactoryNativePixmap);
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_GPU_MEMORY_BUFFER_FACTORY_NATIVE_PIXMAP_H_
