// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_IMAGE_FACTORY_NATIVE_PIXMAP_H_
#define GPU_COMMAND_BUFFER_SERVICE_IMAGE_FACTORY_NATIVE_PIXMAP_H_

#include "gpu/command_buffer/service/image_factory.h"
#include "gpu/gpu_gles2_export.h"

namespace gl {
class GLImageNativePixmap;
}

namespace gpu {

class GPU_GLES2_EXPORT ImageFactoryNativePixmap : public ImageFactory {
 public:
  ImageFactoryNativePixmap();

  ImageFactoryNativePixmap(const ImageFactoryNativePixmap&) = delete;
  ImageFactoryNativePixmap& operator=(const ImageFactoryNativePixmap&) = delete;

  ~ImageFactoryNativePixmap() override;

  // Create an anonymous GLImage backed by a GpuMemoryBuffer that doesn't have a
  // client_id. It can't be passed to other processes. Used only by validating
  // command decoder to support NaCL swap chain.
  bool SupportsCreateAnonymousImage() const;
  scoped_refptr<gl::GLImageNativePixmap> CreateAnonymousImage(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      SurfaceHandle surface_handle,
      bool* is_cleared);

  // Overridden from ImageFactory:
  unsigned RequiredTextureType() override;
  ImageFactoryNativePixmap* AsImageFactoryNativePixmap() override;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_IMAGE_FACTORY_NATIVE_PIXMAP_H_
