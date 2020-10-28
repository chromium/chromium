// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VAAPI_PICTURE_NATIVE_PIXMAP_ANGLE_H_
#define MEDIA_GPU_VAAPI_VAAPI_PICTURE_NATIVE_PIXMAP_ANGLE_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "media/gpu/vaapi/vaapi_picture_native_pixmap.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/x/xproto.h"
#include "ui/gl/gl_bindings.h"

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

  ~VaapiPictureNativePixmapAngle() override;

  // VaapiPicture implementation.
  Status Allocate(gfx::BufferFormat format) override;
  bool ImportGpuMemoryBufferHandle(
      gfx::BufferFormat format,
      gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle) override;
  bool DownloadFromSurface(scoped_refptr<VASurface> va_surface) override;

  // This native pixmap implementation never instantiates its own VASurfaces.
  VASurfaceID va_surface_id() const override;

 private:
  x11::Pixmap x_pixmap_ = x11::Pixmap::None;

  DISALLOW_COPY_AND_ASSIGN(VaapiPictureNativePixmapAngle);
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_PICTURE_NATIVE_PIXMAP_ANGLE_H_
