// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VAAPI_PICTURE_TFP_H_
#define MEDIA_GPU_VAAPI_VAAPI_PICTURE_TFP_H_

#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "media/gpu/vaapi/vaapi_picture.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/x/connection.h"
#include "ui/gl/gl_bindings.h"

namespace gl {
class GLImageGLX;
}

namespace media {

class VaapiWrapper;

// Implementation of VaapiPicture for the X11 backends with Texture-From-Pixmap
// extension.
class VaapiTFPPicture : public VaapiPicture {
 public:
  VaapiTFPPicture(scoped_refptr<VaapiWrapper> vaapi_wrapper,
                  const MakeGLContextCurrentCallback& make_context_current_cb,
                  const BindGLImageCallback& bind_image_cb,
                  int32_t picture_buffer_id,
                  const gfx::Size& size,
                  const gfx::Size& visible_size,
                  uint32_t texture_id,
                  uint32_t client_texture_id,
                  uint32_t texture_target);

  VaapiTFPPicture(const VaapiTFPPicture&) = delete;
  VaapiTFPPicture& operator=(const VaapiTFPPicture&) = delete;

  ~VaapiTFPPicture() override;

  // VaapiPicture implementation.
  VaapiStatus Allocate(gfx::BufferFormat format) override;
  bool ImportGpuMemoryBufferHandle(
      gfx::BufferFormat format,
      gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle) override;
  bool DownloadFromSurface(scoped_refptr<VASurface> va_surface) override;

 private:
  VaapiStatus Initialize();

  const raw_ptr<x11::Connection> connection_;

  x11::Pixmap x_pixmap_;
  scoped_refptr<gl::GLImageGLX> glx_image_;
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_PICTURE_TFP_H_
