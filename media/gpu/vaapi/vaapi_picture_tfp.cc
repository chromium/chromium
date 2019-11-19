// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_picture_tfp.h"

#include "media/gpu/vaapi/va_surface.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_image_glx.h"
#include "ui/gl/scoped_binders.h"

namespace media {

VaapiTFPPicture::VaapiTFPPicture(
    const scoped_refptr<VaapiWrapper>& vaapi_wrapper,
    const MakeGLContextCurrentCallback& make_context_current_cb,
    const BindGLImageCallback& bind_image_cb,
    int32_t picture_buffer_id,
    const gfx::Size& size,
    uint32_t texture_id,
    uint32_t client_texture_id,
    uint32_t texture_target)
    : VaapiPicture(vaapi_wrapper,
                   make_context_current_cb,
                   bind_image_cb,
                   picture_buffer_id,
                   size,
                   texture_id,
                   client_texture_id,
                   texture_target),
      x_display_(gfx::GetXDisplay()),
      x_pixmap_(0) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(texture_id);
  DCHECK(client_texture_id);
}

VaapiTFPPicture::~VaapiTFPPicture() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (glx_image_.get() && make_context_current_cb_.Run()) {
    glx_image_->ReleaseTexImage(texture_target_);
    DCHECK_EQ(glGetError(), static_cast<GLenum>(GL_NO_ERROR));
  }

  if (x_pixmap_)
    XFreePixmap(x_display_, x_pixmap_);
}

bool VaapiTFPPicture::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(x_pixmap_);

  if (make_context_current_cb_ && !make_context_current_cb_.Run())
    return false;

  glx_image_ = new gl::GLImageGLX(size_, GL_RGB);
  if (!glx_image_->Initialize(x_pixmap_)) {
    // x_pixmap_ will be freed in the destructor.
    DLOG(ERROR) << "Failed creating a GLX Pixmap for TFP";
    return false;
  }

  gl::ScopedTextureBinder texture_binder(texture_target_, texture_id_);
  if (!glx_image_->BindTexImage(texture_target_)) {
    DLOG(ERROR) << "Failed to bind texture to glx image";
    return false;
  }

  return true;
}

bool VaapiTFPPicture::Allocate(gfx::BufferFormat format) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (format != gfx::BufferFormat::BGRX_8888 &&
      format != gfx::BufferFormat::BGRA_8888 &&
      format != gfx::BufferFormat::RGBX_8888) {
    DLOG(ERROR) << "Unsupported format";
    return false;
  }

  XWindowAttributes win_attr;
  int screen = DefaultScreen(x_display_);
  XGetWindowAttributes(x_display_, XRootWindow(x_display_, screen), &win_attr);
  // TODO(posciak): pass the depth required by libva, not the RootWindow's
  // depth
  x_pixmap_ = XCreatePixmap(x_display_, XRootWindow(x_display_, screen),
                            size_.width(), size_.height(), win_attr.depth);
  if (!x_pixmap_) {
    DLOG(ERROR) << "Failed creating an X Pixmap for TFP";
    return false;
  }

  return Initialize();
}

bool VaapiTFPPicture::ImportGpuMemoryBufferHandle(
    gfx::BufferFormat format,
    gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED() << "GpuMemoryBufferHandle import not implemented";
  return false;
}

bool VaapiTFPPicture::DownloadFromSurface(
    const scoped_refptr<VASurface>& va_surface) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return vaapi_wrapper_->PutSurfaceIntoPixmap(va_surface->id(), x_pixmap_,
                                              va_surface->size());
}

}  // namespace media
