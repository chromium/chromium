// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/internal/mappable_buffer_io_surface.h"

#include "base/apple/mach_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notimplemented.h"
#include "base/numerics/safe_conversions.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
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

MappableBufferIOSurface::MappableBufferIOSurface(
    const gfx::Size& size,
    viz::SharedImageFormat format,
    gfx::GpuMemoryBufferHandle handle,
    uint32_t lock_flags)
    : size_(size),
      format_(format),
      handle_(std::move(handle)),
      lock_flags_(lock_flags) {}

MappableBufferIOSurface::~MappableBufferIOSurface() {
#if DCHECK_IS_ON()
  {
    base::AutoLock auto_lock(map_lock_);
    DCHECK_EQ(map_count_, 0u);
  }
#endif
}

void MappableBufferIOSurface::AssertMapped() {
#if DCHECK_IS_ON()
  base::AutoLock auto_lock(map_lock_);
  DCHECK_GT(map_count_, 0u);
#endif
}

// static
std::unique_ptr<MappableBufferIOSurface>
MappableBufferIOSurface::CreateFromHandleForTesting(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    viz::SharedImageFormat format,
    gfx::BufferUsage usage) {
  return CreateFromHandleImpl(std::move(handle), size, format,
                              LockFlags(usage));
}

// static
base::OnceClosure MappableBufferIOSurface::AllocateForTesting(
    const gfx::Size& size,
    viz::SharedImageFormat format,
    gfx::BufferUsage usage,
    gfx::GpuMemoryBufferHandle* handle) {
  *handle = gfx::GpuMemoryBufferHandle(gfx::CreateIOSurface(size, format));
  return base::DoNothing();
}

// static
std::unique_ptr<MappableBufferIOSurface>
MappableBufferIOSurface::CreateFromHandle(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    viz::SharedImageFormat format,
    bool is_read_only_cpu_usage) {
  uint32_t lock_flags = is_read_only_cpu_usage ? kIOSurfaceLockReadOnly : 0;
  return CreateFromHandleImpl(std::move(handle), size, format, lock_flags);
}

// static
std::unique_ptr<MappableBufferIOSurface>
MappableBufferIOSurface::CreateFromHandleImpl(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    viz::SharedImageFormat format,
    int32_t lock_flags) {
  // The maximum number of times to dump before throttling (to avoid sending
  // thousands of crash dumps).
  constexpr int kMaxCrashDumps = 10;
  static int dump_counter = kMaxCrashDumps;
#if BUILDFLAG(IS_IOS)
  if (!handle.io_surface_shared_memory_region().IsValid()) {
    LOG(ERROR) << "Invalid shared memory region returned to client.";
    if (dump_counter) {
      dump_counter -= 1;
      base::debug::DumpWithoutCrashing();
    }
    return nullptr;
  }
#else
  if (!handle.io_surface()) {
    LOG(ERROR) << "Failed to open IOSurface via mach port returned to client.";
    if (dump_counter) {
      dump_counter -= 1;
      base::debug::DumpWithoutCrashing();
    }
    return nullptr;
  }

  int64_t io_surface_width = IOSurfaceGetWidth(handle.io_surface().get());
  int64_t io_surface_height = IOSurfaceGetHeight(handle.io_surface().get());
  if (io_surface_width < size.width() || io_surface_height < size.height()) {
    DLOG(ERROR) << "IOSurface size does not match handle.";
    return nullptr;
  }
#endif

  return base::WrapUnique(
      new MappableBufferIOSurface(size, format, handle.Clone(), lock_flags));
}

bool MappableBufferIOSurface::Map() {
  base::AutoLock auto_lock(map_lock_);
  if (map_count_++) {
    return true;
  }

#if BUILDFLAG(IS_IOS)
  if (!shared_memory_mapping_.IsValid()) {
    shared_memory_mapping_ = handle_.io_surface_shared_memory_region().Map();
  }
  if (!shared_memory_mapping_.IsValid()) {
    LOG(ERROR) << "Invalid shared memory mapping";
    return false;
  }
#else
  kern_return_t kr =
      IOSurfaceLock(handle_.io_surface().get(), lock_flags_, nullptr);
  DCHECK_EQ(kr, KERN_SUCCESS) << " lock_flags_: " << lock_flags_;
  MACH_LOG_IF(ERROR, kr != KERN_SUCCESS, kr)
      << "MappableBufferIOSurface::Map IOSurfaceLock lock_flags_: "
      << lock_flags_;
#endif
  return true;
}

void* MappableBufferIOSurface::memory(size_t plane) {
  AssertMapped();
  CHECK_LT(base::checked_cast<int>(plane), format_.NumberOfPlanes());
#if BUILDFLAG(IS_IOS)
  // SAFETY: We trust the GPU process to allocate the IOSurface and initialize
  // the shared memory region from it correctly and we assert that below too.
  CHECK(shared_memory_mapping_.IsValid());
  CHECK_LT(plane, gfx::kMaxIOSurfacePlanes);
  const size_t plane_offset = handle_.io_surface_plane_offset(plane);
  CHECK_LE(plane_offset, shared_memory_mapping_.mapped_size());
  return UNSAFE_BUFFERS(shared_memory_mapping_.data() + plane_offset);
#else
  return IOSurfaceGetBaseAddressOfPlane(handle_.io_surface().get(), plane);
#endif
}

void MappableBufferIOSurface::Unmap() {
  base::AutoLock auto_lock(map_lock_);
  DCHECK_GT(map_count_, 0u);
  if (--map_count_) {
    return;
  }
#if !BUILDFLAG(IS_IOS)
  kern_return_t kr =
      IOSurfaceUnlock(handle_.io_surface().get(), lock_flags_, nullptr);
  DCHECK_EQ(kr, KERN_SUCCESS) << " lock_flags_: " << lock_flags_;
  MACH_LOG_IF(ERROR, kr != KERN_SUCCESS, kr)
      << "MappableBufferIOSurface::Unmap IOSurfaceUnlock lock_flags_: "
      << lock_flags_;
#endif
}

int MappableBufferIOSurface::stride(size_t plane) const {
  CHECK_LT(base::checked_cast<int>(plane), format_.NumberOfPlanes());
#if BUILDFLAG(IS_IOS)
  CHECK_LT(plane, gfx::kMaxIOSurfacePlanes);
  return handle_.io_surface_plane_stride(plane);
#else
  return IOSurfaceGetBytesPerRowOfPlane(handle_.io_surface().get(), plane);
#endif
}

gfx::GpuMemoryBufferType MappableBufferIOSurface::GetType() const {
  DCHECK_EQ(handle_.type, gfx::IO_SURFACE_BUFFER);
  return handle_.type;
}

gfx::GpuMemoryBufferHandle MappableBufferIOSurface::CloneHandle() const {
  return handle_.Clone();
}

void MappableBufferIOSurface::MapAsync(
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(Map());
}

bool MappableBufferIOSurface::AsyncMappingIsNonBlocking() const {
  return false;
}

}  // namespace gpu
