// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_memory_buffer_factory_io_surface.h"

#include <vector>

#include "base/apple/mach_logging.h"
#include "base/logging.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/numerics/safe_conversions.h"
#include "base/unguessable_token.h"
#include "gpu/ipc/common/gpu_client_ids.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/mac/io_surface.h"

namespace gpu {

namespace {

// A GpuMemoryBuffer with client_id = 0 behaves like anonymous shared memory.
const int kAnonymousClientId = 0;

}  // namespace

GpuMemoryBufferFactoryIOSurface::GpuMemoryBufferFactoryIOSurface() = default;
GpuMemoryBufferFactoryIOSurface::~GpuMemoryBufferFactoryIOSurface() = default;

gfx::GpuMemoryBufferHandle
GpuMemoryBufferFactoryIOSurface::CreateGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    const gfx::Size& framebuffer_size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    int client_id,
    SurfaceHandle surface_handle) {
  DCHECK_NE(client_id, kAnonymousClientId);
  DCHECK_EQ(framebuffer_size, size);

  bool should_clear = true;
  base::apple::ScopedCFTypeRef<IOSurfaceRef> io_surface =
      gfx::CreateIOSurface(size, format, should_clear);
  if (!io_surface) {
    LOG(ERROR) << "Failed to allocate IOSurface.";
    return {};
  }

  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::IO_SURFACE_BUFFER;
  handle.id = id;
  handle.io_surface = io_surface;

#if BUILDFLAG(IS_IOS)
  handle.io_surface_mach_port.reset(IOSurfaceCreateMachPort(io_surface.get()));

  const void* io_surface_base_addr = IOSurfaceGetBaseAddress(io_surface.get());
  const size_t io_surface_alloc_size = IOSurfaceGetAllocSize(io_surface.get());

  memory_object_size_t alloc_size = io_surface_alloc_size;
  base::apple::ScopedMachSendRight named_right;
  kern_return_t kr = mach_make_memory_entry_64(
      mach_task_self(), &alloc_size,
      reinterpret_cast<memory_object_offset_t>(io_surface_base_addr),
      VM_PROT_READ | VM_PROT_WRITE,
      base::apple::ScopedMachSendRight::Receiver(named_right).get(),
      MACH_PORT_NULL);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "GpuMemoryBufferFactoryIOSurface::"
                           "CreateGpuMemoryBuffer mach_make_memory_entry_64";
    return {};
  }
  DCHECK_GE(alloc_size, io_surface_alloc_size);

  using base::subtle::PlatformSharedMemoryRegion;
  auto platform_shared_memory_region = PlatformSharedMemoryRegion::Take(
      std::move(named_right), PlatformSharedMemoryRegion::Mode::kUnsafe,
      alloc_size, base::UnguessableToken::Create());
  if (!platform_shared_memory_region.IsValid()) {
    LOG(ERROR) << "Failed to create PlatformSharedMemoryRegion";
    return {};
  }

  handle.io_surface_shared_memory_region =
      base::UnsafeSharedMemoryRegion::Deserialize(
          std::move(platform_shared_memory_region));
  if (!handle.io_surface_shared_memory_region.IsValid()) {
    LOG(ERROR) << "Failed to create UnsafeSharedMemoryRegion";
    return {};
  }

  for (size_t plane = 0;
       plane < gfx::NumberOfPlanesForLinearBufferFormat(format); plane++) {
    handle.io_surface_plane_strides[plane] = base::checked_cast<uint32_t>(
        IOSurfaceGetBytesPerRowOfPlane(io_surface.get(), plane));

    const void* io_surface_plane_addr =
        IOSurfaceGetBaseAddressOfPlane(io_surface.get(), plane);
    handle.io_surface_plane_offsets[plane] = base::checked_cast<uint32_t>(
        reinterpret_cast<intptr_t>(io_surface_plane_addr) -
        reinterpret_cast<intptr_t>(io_surface_base_addr));
  }
#endif

  return handle;
}

void GpuMemoryBufferFactoryIOSurface::DestroyGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    int client_id) {}

bool GpuMemoryBufferFactoryIOSurface::FillSharedMemoryRegionWithBufferContents(
    gfx::GpuMemoryBufferHandle buffer_handle,
    base::UnsafeSharedMemoryRegion shared_memory) {
  return false;
}

}  // namespace gpu
