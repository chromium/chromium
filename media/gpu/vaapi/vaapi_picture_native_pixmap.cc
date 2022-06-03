// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_picture_native_pixmap.h"

#include "media/gpu/vaapi/va_surface.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/linux/native_pixmap_dmabuf.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_image_native_pixmap.h"

namespace media {

VaapiPictureNativePixmap::VaapiPictureNativePixmap(
    scoped_refptr<VaapiWrapper> vaapi_wrapper,
    const MakeGLContextCurrentCallback& make_context_current_cb,
    const BindGLImageCallback& bind_image_cb,
    int32_t picture_buffer_id,
    const gfx::Size& size,
    const gfx::Size& visible_size,
    uint32_t texture_id,
    uint32_t client_texture_id,
    uint32_t texture_target)
    : VaapiPicture(std::move(vaapi_wrapper),
                   make_context_current_cb,
                   bind_image_cb,
                   picture_buffer_id,
                   size,
                   visible_size,
                   texture_id,
                   client_texture_id,
                   texture_target) {}

VaapiPictureNativePixmap::~VaapiPictureNativePixmap() = default;

bool VaapiPictureNativePixmap::DownloadFromSurface(
    scoped_refptr<VASurface> va_surface) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return vaapi_wrapper_->BlitSurface(*va_surface, *va_surface_);
}

bool VaapiPictureNativePixmap::AllowOverlay() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return true;
}

VASurfaceID VaapiPictureNativePixmap::va_surface_id() const {
  return va_surface_->id();
}

}  // namespace media
