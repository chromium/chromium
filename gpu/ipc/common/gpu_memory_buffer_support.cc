// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/gpu_memory_buffer_support.h"

#include <inttypes.h>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/process_memory_dump.h"
#include "build/build_config.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_shared_memory.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_usage_util.h"

#if BUILDFLAG(IS_APPLE)
#include "gpu/ipc/common/gpu_memory_buffer_impl_io_surface.h"
#endif

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/client_native_pixmap_factory_ozone.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

#if BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "gpu/ipc/common/gpu_memory_buffer_impl_native_pixmap.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "gpu/ipc/common/gpu_memory_buffer_impl_dxgi.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/android_hardware_buffer_compat.h"
#endif

namespace gpu {

GpuMemoryBufferSupport::GpuMemoryBufferSupport() {
#if BUILDFLAG(IS_OZONE)
  client_native_pixmap_factory_ = ui::CreateClientNativePixmapFactoryOzone();
#endif
}

GpuMemoryBufferSupport::~GpuMemoryBufferSupport() {}

// static
gfx::GpuMemoryBufferType
GpuMemoryBufferSupport::GetNativeGpuMemoryBufferType() {
#if BUILDFLAG(IS_APPLE)
  return gfx::IO_SURFACE_BUFFER;
#elif BUILDFLAG(IS_ANDROID)
  return gfx::ANDROID_HARDWARE_BUFFER;
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_OZONE)
  return gfx::NATIVE_PIXMAP;
#elif BUILDFLAG(IS_WIN)
  return gfx::DXGI_SHARED_HANDLE;
#else
  return gfx::EMPTY_BUFFER;
#endif
}

// static
bool GpuMemoryBufferSupport::IsNativeGpuMemoryBufferConfigurationSupported(
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  DCHECK_NE(gfx::SHARED_MEMORY_BUFFER, GetNativeGpuMemoryBufferType());

#if BUILDFLAG(IS_APPLE)
  switch (usage) {
    case gfx::BufferUsage::GPU_READ:
    case gfx::BufferUsage::SCANOUT:
    case gfx::BufferUsage::SCANOUT_CPU_READ_WRITE:
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
    case gfx::BufferUsage::SCANOUT_FRONT_RENDERING:
    case gfx::BufferUsage::SCANOUT_VEA_CPU_READ:
      return format == gfx::BufferFormat::BGRA_8888 ||
             format == gfx::BufferFormat::RGBA_8888 ||
             format == gfx::BufferFormat::BGRX_8888 ||
             format == gfx::BufferFormat::RGBX_8888 ||
             format == gfx::BufferFormat::R_8 ||
             format == gfx::BufferFormat::RG_88 ||
             format == gfx::BufferFormat::R_16 ||
             format == gfx::BufferFormat::RG_1616 ||
             format == gfx::BufferFormat::RGBA_F16 ||
             format == gfx::BufferFormat::BGRA_1010102 ||
             format == gfx::BufferFormat::YUV_420_BIPLANAR ||
             format == gfx::BufferFormat::YUVA_420_TRIPLANAR ||
             format == gfx::BufferFormat::P010;
    case gfx::BufferUsage::SCANOUT_VDA_WRITE:
    case gfx::BufferUsage::PROTECTED_SCANOUT:
    case gfx::BufferUsage::PROTECTED_SCANOUT_VDA_WRITE:
    case gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE:
    case gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE:
    case gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE:
      return false;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
#elif BUILDFLAG(IS_ANDROID)
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
    case gfx::BufferUsage::PROTECTED_SCANOUT:
    case gfx::BufferUsage::PROTECTED_SCANOUT_VDA_WRITE:
    case gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE:
    case gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE:
    case gfx::BufferUsage::SCANOUT_VEA_CPU_READ:
    case gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE:
    case gfx::BufferUsage::SCANOUT_FRONT_RENDERING:
      return false;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
#elif BUILDFLAG(IS_OZONE)
  return ui::OzonePlatform::GetInstance()->IsNativePixmapConfigSupported(format,
                                                                         usage);
#elif BUILDFLAG(IS_WIN)
  switch (usage) {
    case gfx::BufferUsage::GPU_READ:
    case gfx::BufferUsage::SCANOUT:
      return format == gfx::BufferFormat::RGBA_8888 ||
             format == gfx::BufferFormat::RGBX_8888 ||
             format == gfx::BufferFormat::BGRA_8888 ||
             format == gfx::BufferFormat::BGRX_8888;
    case gfx::BufferUsage::SCANOUT_CPU_READ_WRITE:
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
    case gfx::BufferUsage::SCANOUT_VDA_WRITE:
    case gfx::BufferUsage::PROTECTED_SCANOUT:
    case gfx::BufferUsage::PROTECTED_SCANOUT_VDA_WRITE:
    case gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE:
    case gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE:
    case gfx::BufferUsage::SCANOUT_VEA_CPU_READ:
    case gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE:
    case gfx::BufferUsage::SCANOUT_FRONT_RENDERING:
      return false;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
#else
  DCHECK_EQ(GetNativeGpuMemoryBufferType(), gfx::EMPTY_BUFFER);
  return false;
#endif
}

// static

GpuMemoryBufferConfigurationSet
GpuMemoryBufferSupport::GetNativeGpuMemoryBufferConfigurations() {
  GpuMemoryBufferConfigurationSet configurations;

#if BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_ANDROID)
  const gfx::BufferFormat kBufferFormats[] = {
      gfx::BufferFormat::R_8,
      gfx::BufferFormat::R_16,
      gfx::BufferFormat::RG_88,
      gfx::BufferFormat::RG_1616,
      gfx::BufferFormat::BGR_565,
      gfx::BufferFormat::RGBA_4444,
      gfx::BufferFormat::RGBX_8888,
      gfx::BufferFormat::RGBA_8888,
      gfx::BufferFormat::BGRX_8888,
      gfx::BufferFormat::BGRA_1010102,
      gfx::BufferFormat::RGBA_1010102,
      gfx::BufferFormat::BGRA_8888,
      gfx::BufferFormat::RGBA_F16,
      gfx::BufferFormat::YVU_420,
      gfx::BufferFormat::YUV_420_BIPLANAR,
      gfx::BufferFormat::YUVA_420_TRIPLANAR,
      gfx::BufferFormat::P010};

  const gfx::BufferUsage kUsages[] = {
      gfx::BufferUsage::GPU_READ,
      gfx::BufferUsage::SCANOUT,
      gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE,
      gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE,
      gfx::BufferUsage::SCANOUT_CPU_READ_WRITE,
      gfx::BufferUsage::SCANOUT_VDA_WRITE,
      gfx::BufferUsage::PROTECTED_SCANOUT,
      gfx::BufferUsage::PROTECTED_SCANOUT_VDA_WRITE,
      gfx::BufferUsage::GPU_READ_CPU_READ_WRITE,
      gfx::BufferUsage::SCANOUT_VEA_CPU_READ,
      gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE,
      gfx::BufferUsage::SCANOUT_FRONT_RENDERING,
  };

  for (auto format : kBufferFormats) {
    for (auto usage : kUsages) {
      if (IsNativeGpuMemoryBufferConfigurationSupported(format, usage)) {
        configurations.insert(gfx::BufferUsageAndFormat(usage, format));
      }
    }
  }
#endif  // BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_ANDROID)

  return configurations;
}

bool GpuMemoryBufferSupport::IsConfigurationSupportedForTest(
    gfx::GpuMemoryBufferType type,
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  if (type == GetNativeGpuMemoryBufferType()) {
    return IsNativeGpuMemoryBufferConfigurationSupported(format, usage);
  }

  if (type == gfx::SHARED_MEMORY_BUFFER) {
    return GpuMemoryBufferImplSharedMemory::IsConfigurationSupported(format,
                                                                     usage);
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

// static
bool GpuMemoryBufferSupport::IsSizeValid(const gfx::Size& size) {
  base::CheckedNumeric<int> bytes = size.width();
  bytes *= size.height();
  return bytes.IsValid();
}

std::unique_ptr<GpuMemoryBufferImpl>
GpuMemoryBufferSupport::CreateGpuMemoryBufferImplFromHandle(
    gfx::GpuMemoryBufferHandle handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    GpuMemoryBufferImpl::DestructionCallback callback,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    scoped_refptr<base::UnsafeSharedMemoryPool> pool,
    base::span<uint8_t> premapped_memory) {
  switch (handle.type) {
    case gfx::SHARED_MEMORY_BUFFER:
      return GpuMemoryBufferImplSharedMemory::CreateFromHandle(
          std::move(handle), size, format, usage, std::move(callback));
#if BUILDFLAG(IS_APPLE)
    case gfx::IO_SURFACE_BUFFER:
      return GpuMemoryBufferImplIOSurface::CreateFromHandle(
          std::move(handle), size, format, usage, std::move(callback));
#endif
#if BUILDFLAG(IS_OZONE)
    case gfx::NATIVE_PIXMAP:
      return GpuMemoryBufferImplNativePixmap::CreateFromHandle(
          client_native_pixmap_factory(), std::move(handle), size, format,
          usage, std::move(callback));
#endif
#if BUILDFLAG(IS_WIN)
    case gfx::DXGI_SHARED_HANDLE:
      return GpuMemoryBufferImplDXGI::CreateFromHandle(
          std::move(handle), size, format, usage, std::move(callback),
          gpu_memory_buffer_manager, std::move(pool), premapped_memory);
#endif
#if BUILDFLAG(IS_ANDROID)
    case gfx::ANDROID_HARDWARE_BUFFER:
      return nullptr;
#endif
    default:
      // TODO(dcheng): Remove default case (https://crbug.com/676224).
      NOTREACHED_IN_MIGRATION() << gfx::BufferFormatToString(format) << ", "
                                << gfx::BufferUsageToString(usage);
      return nullptr;
  }
}

AllocatedBufferInfo::AllocatedBufferInfo(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    gfx::BufferFormat format)
    : buffer_id_(handle.id),
      type_(handle.type),
      size_in_bytes_(gfx::BufferSizeForBufferFormat(size, format)) {
  DCHECK_NE(gfx::EMPTY_BUFFER, type_);

  if (type_ == gfx::SHARED_MEMORY_BUFFER) {
    shared_memory_guid_ = handle.region.GetGUID();
  }
}

AllocatedBufferInfo::~AllocatedBufferInfo() = default;

bool AllocatedBufferInfo::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd,
    int client_id,
    uint64_t client_tracing_process_id) const {
  base::trace_event::MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(
      base::StringPrintf("gpu/gpumemorybuffer/client_0x%" PRIX32 "/buffer_%d",
                         client_id, buffer_id_.id));
  if (!dump) {
    return false;
  }

  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  size_in_bytes_);

  // Create the shared ownership edge to avoid double counting memory.
  if (type_ == gfx::SHARED_MEMORY_BUFFER) {
    pmd->CreateSharedMemoryOwnershipEdge(dump->guid(), shared_memory_guid_,
                                         /*importance=*/0);
  } else {
    auto shared_buffer_guid = gfx::GetGenericSharedGpuMemoryGUIDForTracing(
        client_tracing_process_id, buffer_id_);
    pmd->CreateSharedGlobalAllocatorDump(shared_buffer_guid);
    pmd->AddOwnershipEdge(dump->guid(), shared_buffer_guid);
  }

  return true;
}

}  // namespace gpu
