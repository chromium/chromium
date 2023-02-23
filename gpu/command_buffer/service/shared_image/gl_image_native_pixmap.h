// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_IMAGE_NATIVE_PIXMAP_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_IMAGE_NATIVE_PIXMAP_H_

#include <stdint.h>

#include "gpu/gpu_gles2_export.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_image.h"

namespace ui {
class NativePixmapGLBinding;
}

namespace gpu {

class GPU_GLES2_EXPORT GLImageNativePixmap : public gl::GLImage {
 public:
  // Create an EGLImage from a given NativePixmap and bind |texture_id| to
  // |target| following by binding the image to |target|.
  static scoped_refptr<GLImageNativePixmap> Create(
      const gfx::Size& size,
      gfx::BufferFormat format,
      scoped_refptr<gfx::NativePixmap> pixmap,
      GLenum target,
      GLuint texture_id);

  // Create an EGLImage from a given NativePixmap and plane and bind
  // |texture_id| to |target| followed by binding the image to |target|. The
  // color space is for the external sampler: When we sample the YUV buffer as
  // RGB, we need to tell it the encoding (BT.601, BT.709, or BT.2020) and range
  // (limited or null), and |color_space| conveys this.
  static scoped_refptr<GLImageNativePixmap> CreateForPlane(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferPlane plane,
      scoped_refptr<gfx::NativePixmap> pixmap,
      const gfx::ColorSpace& color_space,
      GLenum target,
      GLuint texture_id);

  // Overridden from GLImage:
  gfx::Size GetSize() override;

 private:
  explicit GLImageNativePixmap(const gfx::Size& size);
  ~GLImageNativePixmap() override;

  // Create a NativePixmapGLBinding from a given NativePixmap. Returns true iff
  // the binding was successfully created.
  bool InitializeFromNativePixmap(gfx::BufferFormat format,
                                  gfx::BufferPlane plane,
                                  scoped_refptr<gfx::NativePixmap> pixmap,
                                  const gfx::ColorSpace& color_space,
                                  GLenum target,
                                  GLuint texture_id);

  std::unique_ptr<ui::NativePixmapGLBinding> pixmap_gl_binding_;
  const gfx::Size size_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_IMAGE_NATIVE_PIXMAP_H_
