// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/gpu_memory_buffer_impl_io_surface.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/mac/io_surface.h"

namespace gpu {
namespace {

// The maximum number of times to dump before throttling (to avoid sending
// thousands of crash dumps).
const int kMaxCrashDumps = 10;

uint32_t LockFlags(gfx::BufferUsage usage) {
  switch (usage) {
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
      return kIOSurfaceLockAvoidSync;
    case gfx::BufferUsage::GPU_READ:
    case gfx::BufferUsage::SCANOUT:
    case gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE:
    case gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE:
    case gfx::BufferUsage::SCANOUT_CPU_READ_WRITE:
    case gfx::BufferUsage::SCANOUT_VDA_WRITE:
    case gfx::BufferUsage::SCANOUT_VEA_READ_CAMERA_AND_CPU_READ_WRITE:
      return 0;
  }
  NOTREACHED();
  return 0;
}

}  // namespace

GpuMemoryBufferImplIOSurface::GpuMemoryBufferImplIOSurface(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    DestructionCallback callback,
    IOSurfaceRef io_surface,
    uint32_t lock_flags)
    : GpuMemoryBufferImpl(id, size, format, std::move(callback)),
      io_surface_(io_surface),
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
  if (!handle.mach_port) {
    LOG(ERROR) << "Invalid IOSurface mach port returned to client.";
    return nullptr;
  }

  base::ScopedCFTypeRef<IOSurfaceRef> io_surface(
      IOSurfaceLookupFromMachPort(handle.mach_port.get()));
  if (!io_surface) {
    LOG(ERROR) << "Failed to open IOSurface via mach port returned to client.";
    static int dump_counter = kMaxCrashDumps;
    if (dump_counter) {
      dump_counter -= 1;
      base::debug::DumpWithoutCrashing();
    }
    return nullptr;
  }
  int64_t io_surface_width = IOSurfaceGetWidth(io_surface);
  int64_t io_surface_height = IOSurfaceGetHeight(io_surface);
  if (io_surface_width < size.width() || io_surface_height < size.height()) {
    DLOG(ERROR) << "IOSurface size does not match handle.";
    return nullptr;
  }

  return base::WrapUnique(new GpuMemoryBufferImplIOSurface(
      handle.id, size, format, std::move(callback), io_surface.release(),
      LockFlags(usage)));
}

// static
base::OnceClosure GpuMemoryBufferImplIOSurface::AllocateForTesting(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gfx::GpuMemoryBufferHandle* handle) {
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface(
      gfx::CreateIOSurface(size, format));
  DCHECK(io_surface);
  gfx::GpuMemoryBufferId kBufferId(1);
  handle->type = gfx::IO_SURFACE_BUFFER;
  handle->id = kBufferId;
  handle->mach_port.reset(IOSurfaceCreateMachPort(io_surface));
  return base::DoNothing();
}

bool GpuMemoryBufferImplIOSurface::Map() {
  DCHECK(!mapped_);
  IOReturn status = IOSurfaceLock(io_surface_, lock_flags_, nullptr);
  DCHECK_NE(status, kIOReturnCannotLock);
  mapped_ = true;
  return true;
}

void* GpuMemoryBufferImplIOSurface::memory(size_t plane) {
  DCHECK(mapped_);
  DCHECK_LT(plane, gfx::NumberOfPlanesForLinearBufferFormat(format_));
  return IOSurfaceGetBaseAddressOfPlane(io_surface_, plane);
}

void GpuMemoryBufferImplIOSurface::Unmap() {
  DCHECK(mapped_);
  IOSurfaceUnlock(io_surface_, lock_flags_, nullptr);
  mapped_ = false;
}

int GpuMemoryBufferImplIOSurface::stride(size_t plane) const {
  DCHECK_LT(plane, gfx::NumberOfPlanesForLinearBufferFormat(format_));
  return IOSurfaceGetBytesPerRowOfPlane(io_surface_, plane);
}

void GpuMemoryBufferImplIOSurface::SetColorSpace(
    const gfx::ColorSpace& color_space) {
  if (color_space == color_space_)
    return;
  color_space_ = color_space;
  IOSurfaceSetColorSpace(io_surface_, color_space);
}

gfx::GpuMemoryBufferType GpuMemoryBufferImplIOSurface::GetType() const {
  return gfx::IO_SURFACE_BUFFER;
}

gfx::GpuMemoryBufferHandle GpuMemoryBufferImplIOSurface::CloneHandle() const {
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::IO_SURFACE_BUFFER;
  handle.id = id_;
  handle.mach_port.reset(IOSurfaceCreateMachPort(io_surface_));
  return handle;
}

}  // namespace gpu
