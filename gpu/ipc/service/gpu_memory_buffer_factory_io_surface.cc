// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_memory_buffer_factory_io_surface.h"

#include <vector>

#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/ipc/common/gpu_client_ids.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/mac/io_surface.h"
#include "ui/gl/buffer_format_utils.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_image_io_surface.h"

namespace gpu {

namespace {

// A GpuMemoryBuffer with client_id = 0 behaves like anonymous shared memory.
const int kAnonymousClientId = 0;

}  // namespace

GpuMemoryBufferFactoryIOSurface::GpuMemoryBufferFactoryIOSurface() {
}

GpuMemoryBufferFactoryIOSurface::~GpuMemoryBufferFactoryIOSurface() {
}

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
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface(
      gfx::CreateIOSurface(size, format, should_clear));
  if (!io_surface) {
    LOG(ERROR) << "Failed to allocate IOSurface.";
    return gfx::GpuMemoryBufferHandle();
  }

  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::IO_SURFACE_BUFFER;
  handle.id = id;
  handle.io_surface = io_surface;

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

bool GpuMemoryBufferFactoryIOSurface::FillSharedMemoryRegionWithBufferContents(
    gfx::GpuMemoryBufferHandle buffer_handle,
    base::UnsafeSharedMemoryRegion shared_memory) {
  return false;
}

ImageFactory* GpuMemoryBufferFactoryIOSurface::AsImageFactory() {
  return this;
}

scoped_refptr<gl::GLImage>
GpuMemoryBufferFactoryIOSurface::CreateImageForGpuMemoryBuffer(
    gfx::GpuMemoryBufferHandle handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    const gfx::ColorSpace& color_space,
    gfx::BufferPlane plane,
    int client_id,
    SurfaceHandle surface_handle) {
  if (handle.type != gfx::IO_SURFACE_BUFFER)
    return nullptr;

  base::AutoLock lock(io_surfaces_lock_);

  base::ScopedCFTypeRef<IOSurfaceRef> io_surface;
  if (handle.id.is_valid()) {
    // Look up by the handle's ID, if one was specified.
    IOSurfaceMapKey key(handle.id, client_id);
    IOSurfaceMap::iterator it = io_surfaces_.find(key);
    if (it != io_surfaces_.end())
      io_surface = it->second;
  } else if (handle.io_surface) {
    io_surface = handle.io_surface;
    if (!io_surface) {
      DLOG(ERROR) << "Failed to open IOSurface from handle.";
      return nullptr;
    }
    // Ensure that the IOSurface has the same size and pixel format as those
    // specified by |size| and |format|. A malicious client could lie about
    // |size| or |format|, which, if subsequently used to determine parameters
    // for bounds checking, could result in an out-of-bounds memory access.
    uint32_t io_surface_format = IOSurfaceGetPixelFormat(io_surface);
    if (io_surface_format != BufferFormatToIOSurfacePixelFormat(format)) {
      DLOG(ERROR)
          << "IOSurface pixel format does not match specified buffer format.";
      return nullptr;
    }
    gfx::Size io_surface_size(IOSurfaceGetWidth(io_surface),
                              IOSurfaceGetHeight(io_surface));
    if (io_surface_size != size) {
      DLOG(ERROR) << "IOSurface size does not match specified size.";
      return nullptr;
    }
  }
  if (!io_surface) {
    DLOG(ERROR) << "Failed to find IOSurface based on key or handle.";
    return nullptr;
  }

  gfx::Size plane_size = GetPlaneSize(plane, size);

  gfx::BufferFormat plane_format = GetPlaneBufferFormat(plane, format);
  scoped_refptr<gl::GLImageIOSurface> image(
      gl::GLImageIOSurface::Create(plane_size));
  if (color_space.IsValid())
    image->SetColorSpace(color_space);

  uint32_t io_surface_plane = GetPlaneIndex(plane, format);
  if (!image->Initialize(io_surface, io_surface_plane, handle.id,
                         plane_format)) {
    DLOG(ERROR) << "Failed to initialize GLImage for IOSurface.";
    return scoped_refptr<gl::GLImage>();
  }

  return image;
}

}  // namespace gpu
