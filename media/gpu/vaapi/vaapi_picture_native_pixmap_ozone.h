// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VAAPI_PICTURE_NATIVE_PIXMAP_OZONE_H_
#define MEDIA_GPU_VAAPI_VAAPI_PICTURE_NATIVE_PIXMAP_OZONE_H_

#include <stdint.h>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "media/gpu/vaapi/vaapi_picture_native_pixmap.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
class NativePixmap;
}  // namespace gfx

namespace gpu {
class GLImageNativePixmap;
}

namespace media {

class VaapiWrapper;

// Implementation of VaapiPictureNativePixmap using Ozone.
class VaapiPictureNativePixmapOzone : public VaapiPictureNativePixmap {
 public:
  VaapiPictureNativePixmapOzone(
      scoped_refptr<VaapiWrapper> vaapi_wrapper,
      const MakeGLContextCurrentCallback& make_context_current_cb,
      const BindGLImageCallback& bind_image_cb_,
      int32_t picture_buffer_id,
      const gfx::Size& size,
      const gfx::Size& visible_size,
      uint32_t texture_id,
      uint32_t client_texture_id,
      uint32_t texture_target);

  VaapiPictureNativePixmapOzone(const VaapiPictureNativePixmapOzone&) = delete;
  VaapiPictureNativePixmapOzone& operator=(
      const VaapiPictureNativePixmapOzone&) = delete;

  ~VaapiPictureNativePixmapOzone() override;

  // VaapiPicture implementation.
  VaapiStatus Allocate(gfx::BufferFormat format) override;
  bool ImportGpuMemoryBufferHandle(
      gfx::BufferFormat format,
      gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle) override;

 private:
  VaapiStatus Initialize(scoped_refptr<gfx::NativePixmap> pixmap);

  // GLImage bound to the GL textures used by the VDA client.
  scoped_refptr<gpu::GLImageNativePixmap> gl_image_;
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_PICTURE_NATIVE_PIXMAP_OZONE_H_
