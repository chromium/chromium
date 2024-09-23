// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_memory_buffer_factory.h"

#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "gpu/ipc/common/gpu_client_ids.h"

#if BUILDFLAG(IS_APPLE)
#include "gpu/ipc/service/gpu_memory_buffer_factory_io_surface.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
#include "gpu/ipc/service/gpu_memory_buffer_factory_native_pixmap.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "gpu/ipc/service/gpu_memory_buffer_factory_dxgi.h"
#endif

namespace gpu {

#if BUILDFLAG(IS_ANDROID)
namespace {

class GpuMemoryBufferFactoryStub : public GpuMemoryBufferFactory {
 public:
  GpuMemoryBufferFactoryStub() = default;
  GpuMemoryBufferFactoryStub(const GpuMemoryBufferFactoryStub&) = delete;
  GpuMemoryBufferFactoryStub& operator=(const GpuMemoryBufferFactoryStub&) =
      delete;
  ~GpuMemoryBufferFactoryStub() override = default;

  // GpuMemoryBufferFactory:
  gfx::GpuMemoryBufferHandle CreateGpuMemoryBuffer(
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      const gfx::Size& framebuffer_size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      int client_id,
      SurfaceHandle surface_handle) override {
    return gfx::GpuMemoryBufferHandle();
  }
  void DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                              int client_id) override {}
  bool FillSharedMemoryRegionWithBufferContents(
      gfx::GpuMemoryBufferHandle buffer_handle,
      base::UnsafeSharedMemoryRegion shared_memory) override {
    return false;
  }
};

}  // namespace
#endif

// static
std::unique_ptr<GpuMemoryBufferFactory>
GpuMemoryBufferFactory::CreateNativeType(
    viz::VulkanContextProvider* vulkan_context_provider,
    scoped_refptr<base::SingleThreadTaskRunner> io_runner) {
#if BUILDFLAG(IS_APPLE)
  return std::make_unique<GpuMemoryBufferFactoryIOSurface>();
#elif BUILDFLAG(IS_ANDROID)
  // Android does not support creating native GMBs (i.e., from
  // AHardwareBuffers), but the codebase is structured such that it is necessary
  // to have a factory that vends invalid GMB handles rather than having no
  // factory at all.
  return std::make_unique<GpuMemoryBufferFactoryStub>();
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
  return std::make_unique<GpuMemoryBufferFactoryNativePixmap>(
      vulkan_context_provider);
#elif BUILDFLAG(IS_WIN)
  return std::make_unique<GpuMemoryBufferFactoryDXGI>(std::move(io_runner));
#else
  return nullptr;
#endif
}

gfx::GpuMemoryBufferHandle GpuMemoryBufferFactory::CreateNativeGmbHandle(
    MappableSIClientGmbId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  // Note that |gmb_id| is used as a cache key in GpuMemoryBufferFactory but
  // since we immediately call DestroyGpuMemoryBuffer here, this value does not
  // matter. Hence its kept as constant for every client calling this method
  // instead of always increasing id. kMappableSIClientId and |gmb_id| will
  // ensure that the cache key is always unique and does not clash with
  // non-mappable GMB cases.
  auto gmb_id = gfx::GpuMemoryBufferId(static_cast<int>(id));
  auto handle = CreateGpuMemoryBuffer(gmb_id, size, /*framebuffer_size=*/size,
                                      format, usage, kMappableSIClientId,
                                      gpu::kNullSurfaceHandle);

  // Note that this removes the handle from the cache in
  // GpuMemoryBufferFactory. Shared image backings caches the handle and still
  // has the ref. So the handle is still alive until the mailbox is destroyed.
  // This is only needed since we are currently using GpuMemoryBufferFactory.
  // TODO(crbug.com/40283108) : Once we remove the GMB abstraction and starts
  // using a separate factory to create the native buffers, we can stop
  // caching the handles in them, remove using gmb_id and also remove the
  // destroy api.
  DestroyGpuMemoryBuffer(gmb_id, kMappableSIClientId);
  return handle;
}

void GpuMemoryBufferFactory::CreateGpuMemoryBufferAsync(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    int client_id,
    SurfaceHandle surface_handle,
    CreateGpuMemoryBufferAsyncCallback callback) {
  // By default, we assume it's ok to allocate GMBs synchronously on the IO
  // thread. However, subclasses can override this assumption.
  std::move(callback).Run(
      CreateGpuMemoryBuffer(id, size, /*framebuffer_size=*/size, format, usage,
                            client_id, surface_handle));
}

}  // namespace gpu
