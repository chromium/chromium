// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_memory_buffer_factory.h"

#include <memory>

#include "build/build_config.h"

#if BUILDFLAG(IS_MAC)
#include "gpu/ipc/service/gpu_memory_buffer_factory_io_surface.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
#include "gpu/ipc/service/gpu_memory_buffer_factory_native_pixmap.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "gpu/ipc/service/gpu_memory_buffer_factory_dxgi.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "gpu/ipc/service/gpu_memory_buffer_factory_android_hardware_buffer.h"
#endif

namespace gpu {

// static
std::unique_ptr<GpuMemoryBufferFactory>
GpuMemoryBufferFactory::CreateNativeType(
    viz::VulkanContextProvider* vulkan_context_provider,
    scoped_refptr<base::SingleThreadTaskRunner> io_runner) {
#if BUILDFLAG(IS_MAC)
  return std::make_unique<GpuMemoryBufferFactoryIOSurface>();
#elif BUILDFLAG(IS_ANDROID)
  return std::make_unique<GpuMemoryBufferFactoryAndroidHardwareBuffer>();
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
  return std::make_unique<GpuMemoryBufferFactoryNativePixmap>(
      vulkan_context_provider);
#elif BUILDFLAG(IS_WIN)
  return std::make_unique<GpuMemoryBufferFactoryDXGI>(std::move(io_runner));
#else
  return nullptr;
#endif
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
