// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_IMAGE_NATIVE_PIXMAP_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_IMAGE_NATIVE_PIXMAP_H_

#include <stdint.h>

#include "gpu/gpu_gles2_export.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_image.h"

namespace media {
class V4L2SliceVideoDecodeAccelerator;
class VaapiPictureNativePixmapOzone;
}  // namespace media

namespace ui {
class NativePixmapGLBinding;
}

namespace gpu {

namespace gles2 {
class GLES2DecoderImpl;
}

class GPU_GLES2_EXPORT GLImageNativePixmap : public gl::GLImage {
 private:
  // Create an EGLImage from a given NativePixmap and bind |texture_id| to
  // |target| following by binding the image to |target|.
  // NOTE: As we are in the process of eliminating this class, there should be
  // no new usages of it introduced.
  static scoped_refptr<GLImageNativePixmap> Create(
      const gfx::Size& size,
      gfx::BufferFormat format,
      scoped_refptr<gfx::NativePixmap> pixmap,
      GLenum target,
      GLuint texture_id);

 public:
  // Wrapper to allow for creation in testing contexts that are difficult to
  // friend.
  static scoped_refptr<GLImageNativePixmap> CreateForTesting(
      const gfx::Size& size,
      gfx::BufferFormat format,
      scoped_refptr<gfx::NativePixmap> pixmap,
      GLenum target,
      GLuint texture_id) {
    return Create(size, format, pixmap, target, texture_id);
  }

 private:
  friend class gles2::GLES2DecoderImpl;
  friend class media::V4L2SliceVideoDecodeAccelerator;
  friend class media::VaapiPictureNativePixmapOzone;

  // Overridden from GLImage:
  gfx::Size GetSize() override;

  explicit GLImageNativePixmap(const gfx::Size& size);
  ~GLImageNativePixmap() override;

  // Create a NativePixmapGLBinding from a given NativePixmap. Returns true iff
  // the binding was successfully created.
  bool InitializeFromNativePixmap(gfx::BufferFormat format,
                                  scoped_refptr<gfx::NativePixmap> pixmap,
                                  GLenum target,
                                  GLuint texture_id);

  std::unique_ptr<ui::NativePixmapGLBinding> pixmap_gl_binding_;
  const gfx::Size size_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_IMAGE_NATIVE_PIXMAP_H_
