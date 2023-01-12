// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VAAPI_PICTURE_NATIVE_PIXMAP_EGL_H_
#define MEDIA_GPU_VAAPI_VAAPI_PICTURE_NATIVE_PIXMAP_EGL_H_

#include <stdint.h>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "media/gpu/vaapi/vaapi_picture_native_pixmap.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
class NativePixmap;
}  // namespace gfx

namespace gl {
class GLImageNativePixmap;
}

namespace media {

class VaapiWrapper;

// Implementation of VaapiPictureNativePixmap for EGL backends, see
// https://crbug.com/785201.
class VaapiPictureNativePixmapEgl : public VaapiPictureNativePixmap {
 public:
  VaapiPictureNativePixmapEgl(
      scoped_refptr<VaapiWrapper> vaapi_wrapper,
      const MakeGLContextCurrentCallback& make_context_current_cb,
      const BindGLImageCallback& bind_image_cb_,
      int32_t picture_buffer_id,
      const gfx::Size& size,
      const gfx::Size& visible_size,
      uint32_t texture_id,
      uint32_t client_texture_id,
      uint32_t texture_target);

  VaapiPictureNativePixmapEgl(const VaapiPictureNativePixmapEgl&) = delete;
  VaapiPictureNativePixmapEgl& operator=(const VaapiPictureNativePixmapEgl&) =
      delete;

  ~VaapiPictureNativePixmapEgl() override;

  // VaapiPicture implementation.
  VaapiStatus Allocate(gfx::BufferFormat format) override;
  bool ImportGpuMemoryBufferHandle(
      gfx::BufferFormat format,
      gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle) override;

 private:
  VaapiStatus Initialize(scoped_refptr<gfx::NativePixmap> pixmap);

  // GLImage bound to the GL textures used by the VDA client.
  scoped_refptr<gl::GLImageNativePixmap> gl_image_;
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_PICTURE_NATIVE_PIXMAP_EGL_H_
