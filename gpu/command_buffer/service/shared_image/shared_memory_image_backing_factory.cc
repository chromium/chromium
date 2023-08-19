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
#include "gpu/ipc/common/gpu_memory_buffer_impl_shared_memory.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gpu {

SharedMemoryImageBackingFactory::SharedMemoryImageBackingFactory()
    : SharedImageBackingFactory(SHARED_IMAGE_USAGE_CPU_WRITE) {}

SharedMemoryImageBackingFactory::~SharedMemoryImageBackingFactory() = default;

std::unique_ptr<SharedImageBacking>
SharedMemoryImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    std::string debug_label,
    bool is_thread_safe) {
  NOTREACHED();
  return nullptr;
}

std::unique_ptr<SharedImageBacking>
SharedMemoryImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    std::string debug_label,
    base::span<const uint8_t> pixel_data) {
  NOTREACHED();
  return nullptr;
}

std::unique_ptr<SharedImageBacking>
SharedMemoryImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    std::string debug_label,
    gfx::GpuMemoryBufferHandle handle) {
  return CreateSharedImage(mailbox, std::move(handle), ToBufferFormat(format),
                           gfx::BufferPlane::DEFAULT, size, color_space,
                           surface_origin, alpha_type, usage, debug_label);
}

std::unique_ptr<SharedImageBacking>
SharedMemoryImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    gfx::GpuMemoryBufferHandle handle,
    gfx::BufferFormat buffer_format,
    gfx::BufferPlane plane,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    std::string debug_label) {
  DCHECK(handle.type == gfx::SHARED_MEMORY_BUFFER);
  SharedMemoryRegionWrapper shm_wrapper;
  if (!shm_wrapper.Initialize(handle, size, buffer_format, plane)) {
    return nullptr;
  }
  const auto format = viz::GetSinglePlaneSharedImageFormat(buffer_format);
  auto backing = std::make_unique<SharedMemoryImageBacking>(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(shm_wrapper));
  return backing;
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
    uint32_t usage,
    std::string debug_label,
    bool is_thread_safe,
    gfx::BufferUsage buffer_usage) {
  auto buffer_format = ToBufferFormat(format);
  auto handle = GpuMemoryBufferImplSharedMemory::CreateGpuMemoryBuffer(
      gfx::GpuMemoryBufferId(0), size, buffer_format, buffer_usage);
  SharedMemoryRegionWrapper shm_wrapper;
  if (!shm_wrapper.Initialize(handle, size, buffer_format,
                              gfx::BufferPlane::DEFAULT)) {
    return nullptr;
  }
  auto backing = std::make_unique<SharedMemoryImageBacking>(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(shm_wrapper), std::move(handle), std::move(buffer_usage));
  return backing;
}

bool SharedMemoryImageBackingFactory::IsSupported(
    uint32_t usage,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    bool thread_safe,
    gfx::GpuMemoryBufferType gmb_type,
    GrContextType gr_context_type,
    base::span<const uint8_t> pixel_data) {
  if (gmb_type != gfx::SHARED_MEMORY_BUFFER) {
    return false;
  }

  if (usage != SHARED_IMAGE_USAGE_CPU_WRITE) {
    return false;
  }

  return true;
}

}  // namespace gpu
