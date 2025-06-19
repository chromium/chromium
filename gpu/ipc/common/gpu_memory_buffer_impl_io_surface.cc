// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/gpu_memory_buffer_impl_io_surface.h"

#include "base/apple/mach_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notimplemented.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/mac/io_surface.h"

namespace gpu {
namespace {

uint32_t LockFlags(gfx::BufferUsage usage) {
  switch (usage) {
    case gfx::BufferUsage::SCANOUT_VEA_CPU_READ:
      // This constant is used for buffers used by video capture. On macOS,
      // these buffers are only ever written to in the capture process,
      // directly as IOSurfaces.
      // Once they are sent to other processes, no CPU writes are performed.
      return kIOSurfaceLockReadOnly;
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
    case gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE:
    case gfx::BufferUsage::GPU_READ:
    case gfx::BufferUsage::SCANOUT:
    case gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE:
    case gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE:
    case gfx::BufferUsage::SCANOUT_CPU_READ_WRITE:
    case gfx::BufferUsage::SCANOUT_VDA_WRITE:
    case gfx::BufferUsage::PROTECTED_SCANOUT:
    case gfx::BufferUsage::PROTECTED_SCANOUT_VDA_WRITE:
    case gfx::BufferUsage::SCANOUT_FRONT_RENDERING:
      return 0;
  }
  NOTREACHED();
}

}  // namespace

GpuMemoryBufferImplIOSurface::GpuMemoryBufferImplIOSurface(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    DestructionCallback callback,
    gfx::GpuMemoryBufferHandle handle,
    uint32_t lock_flags)
    : GpuMemoryBufferImpl(id, size, format, std::move(callback)),
      handle_(std::move(handle)),
      lock_flags_(lock_flags) {}

GpuMemoryBufferImplIOSurface::~GpuMemoryBufferImplIOSurface() {}

// static
std::unique_ptr<GpuMemoryBufferImplIOSurface>
GpuMemoryBufferImplIOSurface::CreateFromHandle(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    DestructionCallback callback) {
  // The maximum number of times to dump before throttling (to avoid sending
  // thousands of crash dumps).
  constexpr int kMaxCrashDumps = 10;
  static int dump_counter = kMaxCrashDumps;
#if BUILDFLAG(IS_IOS)
  if (!handle.io_surface_shared_memory_region.IsValid()) {
    LOG(ERROR) << "Invalid shared memory region returned to client.";
    if (dump_counter) {
      dump_counter -= 1;
      base::debug::DumpWithoutCrashing();
    }
    return nullptr;
  }
#else
  if (!handle.io_surface) {
    LOG(ERROR) << "Failed to open IOSurface via mach port returned to client.";
    if (dump_counter) {
      dump_counter -= 1;
      base::debug::DumpWithoutCrashing();
    }
    return nullptr;
  }

  int64_t io_surface_width = IOSurfaceGetWidth(handle.io_surface.get());
  int64_t io_surface_height = IOSurfaceGetHeight(handle.io_surface.get());
  if (io_surface_width < size.width() || io_surface_height < size.height()) {
    DLOG(ERROR) << "IOSurface size does not match handle.";
    return nullptr;
  }
#endif

  return base::WrapUnique(new GpuMemoryBufferImplIOSurface(
      handle.id, size, format, std::move(callback), handle.Clone(),
      LockFlags(usage)));
}

// static
base::OnceClosure GpuMemoryBufferImplIOSurface::AllocateForTesting(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gfx::GpuMemoryBufferHandle* handle) {
  gfx::GpuMemoryBufferId kBufferId(1);
  handle->type = gfx::IO_SURFACE_BUFFER;
  handle->id = kBufferId;
  handle->io_surface = gfx::CreateIOSurface(size, format);
  DCHECK(handle->io_surface);
  return base::DoNothing();
}

bool GpuMemoryBufferImplIOSurface::Map() {
  base::AutoLock auto_lock(map_lock_);
  if (map_count_++)
    return true;

#if BUILDFLAG(IS_IOS)
  if (!shared_memory_mapping_.IsValid()) {
    shared_memory_mapping_ = handle_.io_surface_shared_memory_region.Map();
  }
  if (!shared_memory_mapping_.IsValid()) {
    LOG(ERROR) << "Invalid shared memory mapping";
    return false;
  }
#else
  kern_return_t kr =
      IOSurfaceLock(handle_.io_surface.get(), lock_flags_, nullptr);
  DCHECK_EQ(kr, KERN_SUCCESS) << " lock_flags_: " << lock_flags_;
  MACH_LOG_IF(ERROR, kr != KERN_SUCCESS, kr)
      << "GpuMemoryBufferImplIOSurface::Map IOSurfaceLock lock_flags_: "
      << lock_flags_;
#endif
  return true;
}

void* GpuMemoryBufferImplIOSurface::memory(size_t plane) {
  AssertMapped();
  DCHECK_LT(plane, gfx::NumberOfPlanesForLinearBufferFormat(format_));
#if BUILDFLAG(IS_IOS)
  // SAFETY: We trust the GPU process to allocate the IOSurface and initialize
  // the shared memory region from it correctly and we assert that below too.
  CHECK(shared_memory_mapping_.IsValid());
  CHECK_LT(plane, gfx::GpuMemoryBufferHandle::kMaxIOSurfacePlanes);
  const size_t plane_offset = handle_.io_surface_plane_offsets[plane];
  CHECK_LE(plane_offset, shared_memory_mapping_.mapped_size());
  return UNSAFE_BUFFERS(shared_memory_mapping_.data() + plane_offset);
#else
  return IOSurfaceGetBaseAddressOfPlane(handle_.io_surface.get(), plane);
#endif
}

void GpuMemoryBufferImplIOSurface::Unmap() {
  base::AutoLock auto_lock(map_lock_);
  DCHECK_GT(map_count_, 0u);
  if (--map_count_)
    return;
#if !BUILDFLAG(IS_IOS)
  kern_return_t kr =
      IOSurfaceUnlock(handle_.io_surface.get(), lock_flags_, nullptr);
  DCHECK_EQ(kr, KERN_SUCCESS) << " lock_flags_: " << lock_flags_;
  MACH_LOG_IF(ERROR, kr != KERN_SUCCESS, kr)
      << "GpuMemoryBufferImplIOSurface::Unmap IOSurfaceUnlock lock_flags_: "
      << lock_flags_;
#endif
}

int GpuMemoryBufferImplIOSurface::stride(size_t plane) const {
  DCHECK_LT(plane, gfx::NumberOfPlanesForLinearBufferFormat(format_));
#if BUILDFLAG(IS_IOS)
  CHECK_LT(plane, gfx::GpuMemoryBufferHandle::kMaxIOSurfacePlanes);
  return handle_.io_surface_plane_strides[plane];
#else
  return IOSurfaceGetBytesPerRowOfPlane(handle_.io_surface.get(), plane);
#endif
}

void GpuMemoryBufferImplIOSurface::SetColorSpace(
    const gfx::ColorSpace& color_space) {
  if (color_space == color_space_)
    return;
  color_space_ = color_space;
#if BUILDFLAG(IS_IOS)
  NOTIMPLEMENTED();
#else
  IOSurfaceSetColorSpace(handle_.io_surface.get(), color_space);
#endif
}

gfx::GpuMemoryBufferType GpuMemoryBufferImplIOSurface::GetType() const {
  DCHECK_EQ(handle_.type, gfx::IO_SURFACE_BUFFER);
  return handle_.type;
}

gfx::GpuMemoryBufferHandle GpuMemoryBufferImplIOSurface::CloneHandle() const {
  return handle_.Clone();
}

}  // namespace gpu
