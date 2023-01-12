// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VAAPI_PICTURE_NATIVE_PIXMAP_ANGLE_H_
#define MEDIA_GPU_VAAPI_VAAPI_PICTURE_NATIVE_PIXMAP_ANGLE_H_

#include <stdint.h>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "media/gpu/vaapi/vaapi_picture_native_pixmap.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/x/xproto.h"
#include "ui/gl/gl_bindings.h"

namespace gl {
class GLImageEGLPixmap;
}

namespace media {

class VaapiWrapper;

// Implementation of VaapiPictureNativePixmap for ANGLE backends.
class VaapiPictureNativePixmapAngle : public VaapiPictureNativePixmap {
 public:
  VaapiPictureNativePixmapAngle(
      scoped_refptr<VaapiWrapper> vaapi_wrapper,
      const MakeGLContextCurrentCallback& make_context_current_cb,
      const BindGLImageCallback& bind_image_cb_,
      int32_t picture_buffer_id,
      const gfx::Size& size,
      const gfx::Size& visible_size,
      uint32_t texture_id,
      uint32_t client_texture_id,
      uint32_t texture_target);

  VaapiPictureNativePixmapAngle(const VaapiPictureNativePixmapAngle&) = delete;
  VaapiPictureNativePixmapAngle& operator=(
      const VaapiPictureNativePixmapAngle&) = delete;

  ~VaapiPictureNativePixmapAngle() override;

  // VaapiPicture implementation.
  VaapiStatus Allocate(gfx::BufferFormat format) override;
  bool ImportGpuMemoryBufferHandle(
      gfx::BufferFormat format,
      gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle) override;
  bool DownloadFromSurface(scoped_refptr<VASurface> va_surface) override;

  // This native pixmap implementation never instantiates its own VASurfaces.
  VASurfaceID va_surface_id() const override;

 private:
  x11::Pixmap x_pixmap_ = x11::Pixmap::None;

  // GLImage bound to the GL textures used by the VDA client.
  scoped_refptr<gl::GLImageEGLPixmap> gl_image_;
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_PICTURE_NATIVE_PIXMAP_ANGLE_H_
