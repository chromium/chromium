// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_memory_buffer_factory_io_surface.h"

#include <vector>

#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/mac/io_surface.h"
#include "ui/gl/buffer_format_utils.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_image_io_surface.h"

namespace gpu {

namespace {
// A GpuMemoryBuffer with client_id = 0 behaves like anonymous shared memory.
const int kAnonymousClientId = 0;

// The maximum number of times to dump before throttling (to avoid sending
// thousands of crash dumps).
const int kMaxCrashDumps = 10;
}  // namespace

GpuMemoryBufferFactoryIOSurface::GpuMemoryBufferFactoryIOSurface() {
}

GpuMemoryBufferFactoryIOSurface::~GpuMemoryBufferFactoryIOSurface() {
}

gfx::GpuMemoryBufferHandle
GpuMemoryBufferFactoryIOSurface::CreateGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    int client_id,
    SurfaceHandle surface_handle) {
  DCHECK_NE(client_id, kAnonymousClientId);

  bool should_clear = true;
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface(
      gfx::CreateIOSurface(size, format, should_clear));
  if (!io_surface) {
    LOG(ERROR) << "Failed to allocate IOSurface.";
    return gfx::GpuMemoryBufferHandle();
  }

  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::IO_SURFACE_BUFFER;
  handle.id = id;
  handle.mach_port.reset(IOSurfaceCreateMachPort(io_surface));
  CHECK(handle.mach_port);

  // This IOSurface will be opened via mach port in the client process. It has
  // been observed in https://crbug.com/574014 that these ports sometimes fail
  // to be opened in the client process. It has further been observed in
  // https://crbug.com/795649#c30 that these ports fail to be opened in creating
  // process. To determine if these failures are independent, attempt to open
  // the creating process first (and don't not return those that fail).
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface_from_mach_port(
      IOSurfaceLookupFromMachPort(handle.mach_port.get()));
  if (!io_surface_from_mach_port) {
    LOG(ERROR) << "Failed to locally open IOSurface from mach port to be "
                  "returned to client, not returning to client.";
    static int dump_counter = kMaxCrashDumps;
    if (dump_counter) {
      dump_counter -= 1;
      base::debug::DumpWithoutCrashing();
    }
    return gfx::GpuMemoryBufferHandle();
  }

  {
    base::AutoLock lock(io_surfaces_lock_);
    IOSurfaceMapKey key(id, client_id);
    DCHECK(io_surfaces_.find(key) == io_surfaces_.end());
    io_surfaces_[key] = io_surface;
  }

  return handle;
}

void GpuMemoryBufferFactoryIOSurface::DestroyGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    int client_id) {
  {
    base::AutoLock lock(io_surfaces_lock_);

    IOSurfaceMapKey key(id, client_id);
    DCHECK(io_surfaces_.find(key) != io_surfaces_.end());
    io_surfaces_.erase(key);
  }
}

ImageFactory* GpuMemoryBufferFactoryIOSurface::AsImageFactory() {
  return this;
}

bool GpuMemoryBufferFactoryIOSurface::SupportsCreateAnonymousImage() const {
  return true;
}

scoped_refptr<gl::GLImage>
GpuMemoryBufferFactoryIOSurface::CreateImageForGpuMemoryBuffer(
    gfx::GpuMemoryBufferHandle handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    int client_id,
    SurfaceHandle surface_handle) {
  if (handle.type != gfx::IO_SURFACE_BUFFER)
    return nullptr;

  base::AutoLock lock(io_surfaces_lock_);

  IOSurfaceMapKey key(handle.id, client_id);
  IOSurfaceMap::iterator it = io_surfaces_.find(key);
  if (it == io_surfaces_.end()) {
    DLOG(ERROR) << "Failed to find IOSurface based on key.";
    return scoped_refptr<gl::GLImage>();
  }

  unsigned internalformat = gl::BufferFormatToGLInternalFormat(format);
  scoped_refptr<gl::GLImageIOSurface> image(
      gl::GLImageIOSurface::Create(size, internalformat));
  if (!image->Initialize(it->second.get(), handle.id, format)) {
    DLOG(ERROR) << "Failed to initialize GLImage for IOSurface.";
    return scoped_refptr<gl::GLImage>();
  }

  return image;
}

scoped_refptr<gl::GLImage>
GpuMemoryBufferFactoryIOSurface::CreateAnonymousImage(const gfx::Size& size,
                                                      gfx::BufferFormat format,
                                                      gfx::BufferUsage usage,
                                                      bool* is_cleared) {
  bool should_clear = false;
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface(
      gfx::CreateIOSurface(size, format, should_clear));
  if (!io_surface) {
    LOG(ERROR) << "Failed to allocate IOSurface.";
    return nullptr;
  }

  // This IOSurface does not require passing via a mach port, but attempt to
  // locally open via a mach port to gather data to include in a Radar about
  // this failure.
  // https://crbug.com/795649
  gfx::ScopedRefCountedIOSurfaceMachPort mach_port(
      IOSurfaceCreateMachPort(io_surface));
  if (mach_port) {
    base::ScopedCFTypeRef<IOSurfaceRef> io_surface_from_mach_port(
        IOSurfaceLookupFromMachPort(mach_port.get()));
    if (!io_surface_from_mach_port) {
      LOG(ERROR) << "Failed to locally open anonymous IOSurface mach port "
                    "(ignoring failure).";
      static int dump_counter = kMaxCrashDumps;
      if (dump_counter) {
        dump_counter -= 1;
        base::debug::DumpWithoutCrashing();
      }
    }
  } else {
    LOG(ERROR) << "Failed to create IOSurface mach port.";
  }

  unsigned internalformat = gl::BufferFormatToGLInternalFormat(format);
  scoped_refptr<gl::GLImageIOSurface> image(
      gl::GLImageIOSurface::Create(size, internalformat));
  // Use an invalid GMB id so that we can differentiate between anonymous and
  // shared GMBs by using gfx::GenericSharedMemoryId::is_valid().
  if (!image->Initialize(io_surface.get(), gfx::GenericSharedMemoryId(),
                         format)) {
    DLOG(ERROR) << "Failed to initialize anonymous GLImage.";
    return scoped_refptr<gl::GLImage>();
  }

  *is_cleared = false;
  return image;
}

unsigned GpuMemoryBufferFactoryIOSurface::RequiredTextureType() {
  return GL_TEXTURE_RECTANGLE_ARB;
}

bool GpuMemoryBufferFactoryIOSurface::SupportsFormatRGB() {
  return false;
}

}  // namespace gpu
