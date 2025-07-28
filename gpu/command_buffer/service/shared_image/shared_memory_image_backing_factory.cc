// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/shared_memory_image_backing_factory.h"

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_memory_image_backing.h"
#include "gpu/command_buffer/service/shared_memory_region_wrapper.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"

namespace gpu {

// static
bool SharedMemoryImageBackingFactory::IsBufferUsageSupported(
    gfx::BufferUsage buffer_usage) {
  switch (buffer_usage) {
    case gfx::BufferUsage::GPU_READ:
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
    case gfx::BufferUsage::SCANOUT_CPU_READ_WRITE:
    case gfx::BufferUsage::SCANOUT_FRONT_RENDERING:
      return true;
    case gfx::BufferUsage::SCANOUT:
    case gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE:
    case gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE:
    case gfx::BufferUsage::SCANOUT_VDA_WRITE:
    case gfx::BufferUsage::PROTECTED_SCANOUT:
    case gfx::BufferUsage::PROTECTED_SCANOUT_VDA_WRITE:
    case gfx::BufferUsage::SCANOUT_VEA_CPU_READ:
    case gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE:
      return false;
  }
  NOTREACHED();
}

// static
bool SharedMemoryImageBackingFactory::IsSizeValidForFormat(
    const gfx::Size& size,
    viz::SharedImageFormat format) {
  auto buffer_format = ToBufferFormat(format);
  switch (buffer_format) {
    case gfx::BufferFormat::R_8:
    case gfx::BufferFormat::R_16:
    case gfx::BufferFormat::RG_88:
    case gfx::BufferFormat::RG_1616:
    case gfx::BufferFormat::BGR_565:
    case gfx::BufferFormat::RGBA_4444:
    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::BGRA_8888:
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::BGRA_1010102:
    case gfx::BufferFormat::RGBA_1010102:
    case gfx::BufferFormat::RGBA_F16:
      return true;
    case gfx::BufferFormat::YVU_420:
    case gfx::BufferFormat::YUV_420_BIPLANAR:
    case gfx::BufferFormat::YUVA_420_TRIPLANAR:
    case gfx::BufferFormat::P010: {
      size_t num_planes =
          gfx::NumberOfPlanesForLinearBufferFormat(buffer_format);
      for (size_t i = 0; i < num_planes; ++i) {
        size_t factor = gfx::SubsamplingFactorForBufferFormat(buffer_format, i);
        if (size.width() % factor || size.height() % factor) {
          return false;
        }
      }
      return true;
    }
  }

  NOTREACHED();
}

// static
gfx::GpuMemoryBufferHandle
SharedMemoryImageBackingFactory::CreateGpuMemoryBufferHandle(
    const gfx::Size& size,
    gfx::BufferFormat buffer_format,
    gfx::BufferUsage buffer_usage) {
  size_t buffer_size = 0u;
  if (!gfx::BufferSizeForBufferFormatChecked(size, buffer_format,
                                             &buffer_size)) {
    return gfx::GpuMemoryBufferHandle();
  }

  auto shared_memory_region =
      base::UnsafeSharedMemoryRegion::Create(buffer_size);
  if (!shared_memory_region.IsValid()) {
    return gfx::GpuMemoryBufferHandle();
  }

  gfx::GpuMemoryBufferHandle handle(std::move(shared_memory_region));
  handle.type = gfx::SHARED_MEMORY_BUFFER;
  handle.offset = 0;
  handle.stride = static_cast<uint32_t>(
      gfx::RowSizeForBufferFormat(size.width(), buffer_format, 0));
  return handle;
}

SharedMemoryImageBackingFactory::SharedMemoryImageBackingFactory()
    : SharedImageBackingFactory(SHARED_IMAGE_USAGE_CPU_WRITE_ONLY |
                                SHARED_IMAGE_USAGE_CPU_READ |
                                SHARED_IMAGE_USAGE_RASTER_COPY_SOURCE) {}

SharedMemoryImageBackingFactory::~SharedMemoryImageBackingFactory() = default;

std::unique_ptr<SharedImageBacking>
SharedMemoryImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    SharedImageUsageSet usage,
    std::string debug_label,
    bool is_thread_safe,
    gfx::GpuMemoryBufferHandle handle) {
  CHECK(handle.type == gfx::SHARED_MEMORY_BUFFER);
  SharedMemoryRegionWrapper shm_wrapper;
  if (!shm_wrapper.Initialize(handle, size, ToBufferFormat(format))) {
    return nullptr;
  }
  return std::make_unique<SharedMemoryImageBacking>(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(debug_label), std::move(shm_wrapper));
}

std::unique_ptr<SharedImageBacking>
SharedMemoryImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    SharedImageUsageSet usage,
    std::string debug_label,
    bool is_thread_safe,
    gfx::BufferUsage buffer_usage) {
  auto buffer_format = ToBufferFormat(format);
  gfx::GpuMemoryBufferHandle handle;
  if (IsBufferUsageSupported(buffer_usage)) {
    handle = CreateGpuMemoryBufferHandle(size, buffer_format, buffer_usage);
  }
  SharedMemoryRegionWrapper shm_wrapper;
  if (!shm_wrapper.Initialize(handle, size, buffer_format)) {
    return nullptr;
  }
  auto backing = std::make_unique<SharedMemoryImageBacking>(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(debug_label), std::move(shm_wrapper), std::move(handle),
      std::move(buffer_usage));
  return backing;
}

bool SharedMemoryImageBackingFactory::IsSupported(
    SharedImageUsageSet usage,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    bool thread_safe,
    gfx::GpuMemoryBufferType gmb_type,
    GrContextType gr_context_type,
    base::span<const uint8_t> pixel_data) {
  if (gmb_type != gfx::SHARED_MEMORY_BUFFER) {
    return false;
  }

  return true;
}

SharedImageBackingType SharedMemoryImageBackingFactory::GetBackingType() {
  return SharedImageBackingType::kSharedMemory;
}

}  // namespace gpu
