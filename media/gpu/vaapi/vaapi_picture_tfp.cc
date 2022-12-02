// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_picture_tfp.h"

#include "media/gpu/vaapi/va_surface.h"
#include "media/gpu/vaapi/vaapi_status.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/future.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_image_glx.h"
#include "ui/gl/scoped_binders.h"

namespace media {

VaapiTFPPicture::VaapiTFPPicture(
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
                   texture_target),
      connection_(x11::Connection::Get()),
      x_pixmap_(x11::Pixmap::None) {
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

  if (x_pixmap_ != x11::Pixmap::None)
    connection_->FreePixmap({x_pixmap_});
}

VaapiStatus VaapiTFPPicture::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(x_pixmap_, x11::Pixmap::None);

  if (make_context_current_cb_ && !make_context_current_cb_.Run())
    return VaapiStatus::Codes::kBadContext;

  glx_image_ = new gl::GLImageGLX(size_, gfx::BufferFormat::BGRX_8888);
  if (!glx_image_->Initialize(x_pixmap_)) {
    // x_pixmap_ will be freed in the destructor.
    DLOG(ERROR) << "Failed creating a GLX Pixmap for TFP";
    return VaapiStatus::Codes::kFailedToInitializeImage;
  }

  gl::ScopedTextureBinder texture_binder(texture_target_, texture_id_);
  if (!glx_image_->BindTexImage(texture_target_)) {
    DLOG(ERROR) << "Failed to bind texture to glx image";
    return VaapiStatus::Codes::kFailedToBindTexture;
  }

  return VaapiStatus::Codes::kOk;
}

VaapiStatus VaapiTFPPicture::Allocate(gfx::BufferFormat format) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (format != gfx::BufferFormat::BGRX_8888 &&
      format != gfx::BufferFormat::BGRA_8888 &&
      format != gfx::BufferFormat::RGBX_8888) {
    DLOG(ERROR) << "Unsupported format";
    return VaapiStatus::Codes::kUnsupportedFormat;
  }

  if (!connection_->Ready())
    return VaapiStatus::Codes::kNoPixmap;

  auto root = connection_->default_root();

  uint8_t depth = 0;
  if (auto reply = connection_->GetGeometry(root).Sync())
    depth = reply->depth;
  else
    return VaapiStatus::Codes::kNoPixmap;

  // TODO(posciak): pass the depth required by libva, not the RootWindow's
  // depth
  auto pixmap = connection_->GenerateId<x11::Pixmap>();
  uint16_t pixmap_width, pixmap_height;
  if (!base::CheckedNumeric<int>(size_.width()).AssignIfValid(&pixmap_width) ||
      !base::CheckedNumeric<int>(size_.height())
           .AssignIfValid(&pixmap_height)) {
    return VaapiStatus::Codes::kNoPixmap;
  }
  auto req = connection_->CreatePixmap(
      {depth, pixmap, root, pixmap_width, pixmap_height});
  if (req.Sync().error) {
    DLOG(ERROR) << "Failed creating an X Pixmap for TFP";
    return VaapiStatus::Codes::kNoPixmap;
  } else {
    x_pixmap_ = pixmap;
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

bool VaapiTFPPicture::DownloadFromSurface(scoped_refptr<VASurface> va_surface) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return vaapi_wrapper_->PutSurfaceIntoPixmap(va_surface->id(), x_pixmap_,
                                              va_surface->size());
}

}  // namespace media
