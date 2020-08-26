// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/gpu_memory_buffer_support.h"

#include "base/check_op.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_shared_memory.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_usage_util.h"

#if defined(OS_MAC)
#include "gpu/ipc/common/gpu_memory_buffer_impl_io_surface.h"
#endif

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#include "ui/gfx/client_native_pixmap_factory.h"
#include "ui/gfx/linux/client_native_pixmap_factory_dmabuf.h"
#endif

#if defined(USE_OZONE)
#include "ui/ozone/public/client_native_pixmap_factory_ozone.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

#if defined(USE_OZONE) || defined(OS_LINUX) || defined(OS_CHROMEOS)
#include "gpu/ipc/common/gpu_memory_buffer_impl_native_pixmap.h"
#endif

#if defined(OS_WIN)
#include "gpu/ipc/common/gpu_memory_buffer_impl_dxgi.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/android_hardware_buffer_compat.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_android_hardware_buffer.h"
#endif

#if defined(USE_X11)
#include "ui/base/ui_base_features.h"
#endif

namespace gpu {

GpuMemoryBufferSupport::GpuMemoryBufferSupport() {
#if defined(USE_OZONE)
  if (features::IsUsingOzonePlatform()) {
    client_native_pixmap_factory_ = ui::CreateClientNativePixmapFactoryOzone();
    return;
  }
#endif
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  client_native_pixmap_factory_.reset(
      gfx::CreateClientNativePixmapFactoryDmabuf());
#endif
}

GpuMemoryBufferSupport::~GpuMemoryBufferSupport() {}

gfx::GpuMemoryBufferType
GpuMemoryBufferSupport::GetNativeGpuMemoryBufferType() {
#if defined(OS_MAC)
  return gfx::IO_SURFACE_BUFFER;
#elif defined(OS_ANDROID)
  return gfx::ANDROID_HARDWARE_BUFFER;
#elif defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(USE_OZONE)
  return gfx::NATIVE_PIXMAP;
#elif defined(OS_WIN)
  return gfx::DXGI_SHARED_HANDLE;
#else
  return gfx::EMPTY_BUFFER;
#endif
}

bool GpuMemoryBufferSupport::IsNativeGpuMemoryBufferConfigurationSupported(
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  DCHECK_NE(gfx::SHARED_MEMORY_BUFFER, GetNativeGpuMemoryBufferType());

#if defined(OS_MAC)
  switch (usage) {
    case gfx::BufferUsage::GPU_READ:
    case gfx::BufferUsage::SCANOUT:
    case gfx::BufferUsage::SCANOUT_CPU_READ_WRITE:
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
      return format == gfx::BufferFormat::BGRA_8888 ||
             format == gfx::BufferFormat::RGBA_8888 ||
             format == gfx::BufferFormat::BGRX_8888 ||
             format == gfx::BufferFormat::R_8 ||
             format == gfx::BufferFormat::RGBA_F16 ||
             format == gfx::BufferFormat::BGRA_1010102 ||
             format == gfx::BufferFormat::YUV_420_BIPLANAR;
    case gfx::BufferUsage::SCANOUT_VDA_WRITE:
    case gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE:
    case gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE:
    case gfx::BufferUsage::SCANOUT_VEA_READ_CAMERA_AND_CPU_READ_WRITE:
      return false;
  }
  NOTREACHED();
  return false;
#elif defined(OS_ANDROID)
  if (!base::AndroidHardwareBufferCompat::IsSupportAvailable()) {
    return false;
  }
  switch (usage) {
    case gfx::BufferUsage::GPU_READ:
    case gfx::BufferUsage::SCANOUT:
      return format == gfx::BufferFormat::RGBA_8888 ||
             format == gfx::BufferFormat::RGBX_8888 ||
             format == gfx::BufferFormat::BGR_565;
    case gfx::BufferUsage::SCANOUT_CPU_READ_WRITE:
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
    case gfx::BufferUsage::SCANOUT_VDA_WRITE:
    case gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE:
    case gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE:
    case gfx::BufferUsage::SCANOUT_VEA_READ_CAMERA_AND_CPU_READ_WRITE:
      return false;
  }
  NOTREACHED();
  return false;
#elif defined(USE_OZONE) || defined(USE_X11)
#if defined(USE_OZONE)
  if (features::IsUsingOzonePlatform()) {
    return ui::OzonePlatform::GetInstance()->IsNativePixmapConfigSupported(
        format, usage);
  }
#endif
  // On X11, GPU memory buffer support can only be determined after GPU
  // initialization.
  // viz::HostGpuMemoryBufferManager::IsNativeGpuMemoryBufferConfiguration()
  // should be used instead.
  NOTREACHED();
  return false;
#elif defined(OS_WIN)
  switch (usage) {
    case gfx::BufferUsage::GPU_READ:
    case gfx::BufferUsage::SCANOUT:
      return format == gfx::BufferFormat::RGBA_8888 ||
             format == gfx::BufferFormat::RGBX_8888;
    case gfx::BufferUsage::SCANOUT_CPU_READ_WRITE:
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
    case gfx::BufferUsage::SCANOUT_VDA_WRITE:
    case gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE:
    case gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE:
    case gfx::BufferUsage::SCANOUT_VEA_READ_CAMERA_AND_CPU_READ_WRITE:
      return false;
  }
  NOTREACHED();
  return false;
#else
  DCHECK_EQ(GetNativeGpuMemoryBufferType(), gfx::EMPTY_BUFFER);
  return false;
#endif
}

bool GpuMemoryBufferSupport::IsConfigurationSupportedForTest(
    gfx::GpuMemoryBufferType type,
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  if (type == GetNativeGpuMemoryBufferType()) {
#if defined(USE_X11)
    // On X11, we require GPUInfo to determine configuration support.
    if (!features::IsUsingOzonePlatform())
      return false;
#endif
    return IsNativeGpuMemoryBufferConfigurationSupported(format, usage);
  }

  if (type == gfx::SHARED_MEMORY_BUFFER) {
    return GpuMemoryBufferImplSharedMemory::IsConfigurationSupported(format,
                                                                     usage);
  }

  NOTREACHED();
  return false;
}

std::unique_ptr<GpuMemoryBufferImpl>
GpuMemoryBufferSupport::CreateGpuMemoryBufferImplFromHandle(
    gfx::GpuMemoryBufferHandle handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    GpuMemoryBufferImpl::DestructionCallback callback) {
  switch (handle.type) {
    case gfx::SHARED_MEMORY_BUFFER:
      return GpuMemoryBufferImplSharedMemory::CreateFromHandle(
          std::move(handle), size, format, usage, std::move(callback));
#if defined(OS_MAC)
    case gfx::IO_SURFACE_BUFFER:
      return GpuMemoryBufferImplIOSurface::CreateFromHandle(
          std::move(handle), size, format, usage, std::move(callback));
#endif
#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(USE_OZONE)
    case gfx::NATIVE_PIXMAP:
      return GpuMemoryBufferImplNativePixmap::CreateFromHandle(
          client_native_pixmap_factory(), std::move(handle), size, format,
          usage, std::move(callback));
#endif
#if defined(OS_WIN)
    case gfx::DXGI_SHARED_HANDLE:
      return GpuMemoryBufferImplDXGI::CreateFromHandle(
          std::move(handle), size, format, usage, std::move(callback));
#endif
#if defined(OS_ANDROID)
    case gfx::ANDROID_HARDWARE_BUFFER:
      return GpuMemoryBufferImplAndroidHardwareBuffer::CreateFromHandle(
          std::move(handle), size, format, usage, std::move(callback));
#endif
    default:
      // TODO(dcheng): Remove default case (https://crbug.com/676224).
      NOTREACHED() << gfx::BufferFormatToString(format) << ", "
                   << gfx::BufferUsageToString(usage);
      return nullptr;
  }
}

}  // namespace gpu
