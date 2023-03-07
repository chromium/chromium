// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_GL_IMAGE_GL_TEXTURE_H_
#define MEDIA_GPU_VAAPI_GL_IMAGE_GL_TEXTURE_H_

#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "ui/gfx/native_pixmap_handle.h"
#include "ui/gl/gl_image.h"

namespace media {

class VaapiPictureNativePixmapEgl;

// GLImage subclass that is used by VaapiPictureNativePixmapEgl.
// NOTE: No new usage of this class should be introduced, as it is in the
// process of being eliminated.
class GLImageGLTexture : public gl::GLImage {
 private:
  friend VaapiPictureNativePixmapEgl;

  // Create an EGLImage from a given GL texture.
  static scoped_refptr<GLImageGLTexture> CreateFromTexture(
      const gfx::Size& size,
      gfx::BufferFormat format,
      uint32_t texture_id);

  // Export the wrapped EGLImage to dmabuf fds.
  gfx::NativePixmapHandle ExportHandle();

 public:
  // Allow usage from test contexts that are difficult to friend.
  static scoped_refptr<GLImageGLTexture> CreateFromTextureForTesting(
      const gfx::Size& size,
      gfx::BufferFormat format,
      uint32_t texture_id) {
    return CreateFromTexture(size, format, texture_id);
  }
  gfx::NativePixmapHandle ExportHandleForTesting() { return ExportHandle(); }

 private:
  // Overridden from GLImage:
  gfx::Size GetSize() override;

  // Binds image to texture currently bound to |target|.
  void BindTexImage(unsigned target);

  ~GLImageGLTexture() override;

  GLImageGLTexture(const gfx::Size& size, gfx::BufferFormat format);
  // Create an EGLImage from a given GL texture. This EGLImage can be converted
  // to an external resource to be shared with other client APIs.
  bool InitializeFromTexture(uint32_t texture_id);

  // Get the GL internal format of the image.
  // It is aligned with glTexImage{2|3}D's parameter |internalformat|.
  unsigned GetInternalFormat();

  raw_ptr<void, DanglingUntriaged> egl_image_ /* EGLImageKHR */;
  const gfx::Size size_;
  THREAD_CHECKER(thread_checker_);
  gfx::BufferFormat format_;
  bool has_image_dma_buf_export_;
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_GL_IMAGE_GL_TEXTURE_H_
