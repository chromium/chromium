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
  if (format.is_single_plane()) {
    return true;
  }
  auto [width_scale, height_scale] = format.GetSubsamplingScale();
  if (size.width() % width_scale || size.height() % height_scale) {
    return false;
  }
  return true;
}

// static
gfx::GpuMemoryBufferHandle
SharedMemoryImageBackingFactory::CreateGpuMemoryBufferHandle(
    const gfx::Size& size,
    viz::SharedImageFormat format) {
  CHECK(viz::HasEquivalentBufferFormat(format));
  std::optional<size_t> buffer_size =
      viz::SharedMemorySizeForSharedImageFormat(format, size);
  if (!buffer_size) {
    return gfx::GpuMemoryBufferHandle();
  }

  auto shared_memory_region =
      base::UnsafeSharedMemoryRegion::Create(buffer_size.value());
  if (!shared_memory_region.IsValid()) {
    return gfx::GpuMemoryBufferHandle();
  }

  gfx::GpuMemoryBufferHandle handle(std::move(shared_memory_region));
  handle.type = gfx::SHARED_MEMORY_BUFFER;
  handle.offset = 0;
  handle.stride =
      static_cast<uint32_t>(viz::SharedMemoryRowSizeForSharedImageFormat(
                                format, /*plane=*/0, size.width())
                                .value());
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
  if (!shm_wrapper.Initialize(handle, size, format)) {
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
  gfx::GpuMemoryBufferHandle handle;
  if (IsBufferUsageSupported(buffer_usage)) {
    handle = CreateGpuMemoryBufferHandle(size, format);
  }
  SharedMemoryRegionWrapper shm_wrapper;
  if (!shm_wrapper.Initialize(handle, size, format)) {
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
