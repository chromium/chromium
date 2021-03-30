// Copyright 2014 The Chromium Authors. All rights reserved.
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
    gfx::Size io_surface_size(IOSurfaceGetWidthOfPlane(io_surface, 0),
                              IOSurfaceGetHeightOfPlane(io_surface, 0));
    if (io_surface_format != BufferFormatToIOSurfacePixelFormat(format)) {
      DLOG(ERROR)
          << "IOSurface pixel format does not match specified buffer format.";
      return nullptr;
    }
    if (io_surface_size != size) {
      DLOG(ERROR) << "IOSurface size does not match specified size.";
      return nullptr;
    }
  }
  if (!io_surface) {
    DLOG(ERROR) << "Failed to find IOSurface based on key or handle.";
    return nullptr;
  }

  unsigned internalformat = gl::BufferFormatToGLInternalFormat(format);
  scoped_refptr<gl::GLImageIOSurface> image(
      gl::GLImageIOSurface::Create(size, internalformat));
  if (!image->Initialize(io_surface, handle.id, format)) {
    DLOG(ERROR) << "Failed to initialize GLImage for IOSurface.";
    return scoped_refptr<gl::GLImage>();
  }

  return image;
}

scoped_refptr<gl::GLImage>
GpuMemoryBufferFactoryIOSurface::CreateAnonymousImage(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    SurfaceHandle surface_handle,
    bool* is_cleared) {
  bool should_clear = false;
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface(
      gfx::CreateIOSurface(size, format, should_clear));
  if (!io_surface) {
    LOG(ERROR) << "Failed to allocate IOSurface.";
    return nullptr;
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
