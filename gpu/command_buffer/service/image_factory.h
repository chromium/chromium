// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_IMAGE_FACTORY_H_
#define GPU_COMMAND_BUFFER_SERVICE_IMAGE_FACTORY_H_

#include "base/memory/ref_counted.h"
#include "build/buildflag.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gl {
class GLImage;
}

namespace gpu {
namespace gles2 {
class GLES2DecoderImpl;
}

class GPU_EXPORT ImageFactory {
 protected:
  ImageFactory();
  virtual ~ImageFactory();

 private:
  // This class is used by validating command decoder for NaCL swapchain and
  // IOSurfaceImageBackingFactory for getting IOSurface from GMB.
  friend class gles2::GLES2DecoderImpl;
  friend class IOSurfaceImageBackingFactory;

  // Create an anonymous GLImage backed by a GpuMemoryBuffer that doesn't have a
  // client_id. It can't be passed to other processes. Used only by validating
  // command decoder to support NaCL swap chain.
  virtual bool SupportsCreateAnonymousImage() const;
  virtual scoped_refptr<gl::GLImage> CreateAnonymousImage(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      SurfaceHandle surface_handle,
      bool* is_cleared);

  // An image can only be bound to a texture with the appropriate type.
  virtual unsigned RequiredTextureType();

  // Whether a created image can have format GL_RGB.
  virtual bool SupportsFormatRGB();
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_IMAGE_FACTORY_H_
