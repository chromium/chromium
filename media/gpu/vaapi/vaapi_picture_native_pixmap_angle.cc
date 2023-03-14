// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_picture_native_pixmap_angle.h"

#include "media/gpu/vaapi/gl_image_egl_pixmap.h"
#include "media/gpu/vaapi/va_surface.h"
#include "media/gpu/vaapi/vaapi_status.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/future.h"
#include "ui/gfx/x/xproto.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/scoped_binders.h"

namespace media {

namespace {

x11::Pixmap CreatePixmap(const gfx::Size& size) {
  auto* connection = x11::Connection::Get();
  if (!connection->Ready())
    return x11::Pixmap::None;

  auto root = connection->default_root();

  uint8_t depth = 0;
  if (auto reply = connection->GetGeometry(root).Sync())
    depth = reply->depth;
  else
    return x11::Pixmap::None;

  // TODO(tmathmeyer) should we use the depth from libva instead of root window?
  auto pixmap = connection->GenerateId<x11::Pixmap>();
  uint16_t pixmap_width, pixmap_height;
  if (!base::CheckedNumeric<int>(size.width()).AssignIfValid(&pixmap_width) ||
      !base::CheckedNumeric<int>(size.height()).AssignIfValid(&pixmap_height)) {
    return x11::Pixmap::None;
  }
  auto req = connection->CreatePixmap(
      {depth, pixmap, root, pixmap_width, pixmap_height});
  if (req.Sync().error)
    pixmap = x11::Pixmap::None;
  return pixmap;
}

}  // namespace

VaapiPictureNativePixmapAngle::VaapiPictureNativePixmapAngle(
    scoped_refptr<VaapiWrapper> vaapi_wrapper,
    const MakeGLContextCurrentCallback& make_context_current_cb,
    const BindGLImageCallback& bind_image_cb,
    int32_t picture_buffer_id,
    const gfx::Size& size,
    const gfx::Size& visible_size,
    uint32_t service_texture_id,
    uint32_t client_texture_id,
    uint32_t texture_target)
    : VaapiPictureNativePixmap(std::move(vaapi_wrapper),
                               make_context_current_cb,
                               bind_image_cb,
                               picture_buffer_id,
                               size,
                               visible_size,
                               service_texture_id,
                               client_texture_id,
                               texture_target) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Check that they're both not 0
  DCHECK(service_texture_id);
  DCHECK(client_texture_id);
}

VaapiPictureNativePixmapAngle::~VaapiPictureNativePixmapAngle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (gl_image_ && make_context_current_cb_.Run()) {
    gl_image_->ReleaseEGLImage();
    DCHECK_EQ(glGetError(), static_cast<GLenum>(GL_NO_ERROR));
  }

  if (x_pixmap_ != x11::Pixmap::None)
    x11::Connection::Get()->FreePixmap({x_pixmap_});

  // Reset |va_surface_| before |gl_image_| to preserve the order of destruction
  // before the refactoring done in
  // https://chromium-review.googlesource.com/c/chromium/src/+/4025005.
  // TODO(crbug.com/1366367): Determine whether preserving this order matters
  // and remove these calls if not.
  va_surface_.reset();
  gl_image_.reset();
}

VaapiStatus VaapiPictureNativePixmapAngle::Allocate(gfx::BufferFormat format) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!(texture_id_ || client_texture_id_))
    return VaapiStatus::Codes::kNoTexture;

  if (!make_context_current_cb_ || !make_context_current_cb_.Run())
    return VaapiStatus::Codes::kBadContext;

  auto image = base::WrapRefCounted<GLImageEGLPixmap>(
      new GLImageEGLPixmap(visible_size_));
  if (!image)
    return VaapiStatus::Codes::kNoImage;

  x_pixmap_ = CreatePixmap(visible_size_);
  if (x_pixmap_ == x11::Pixmap::None)
    return VaapiStatus::Codes::kNoPixmap;

  if (!image->Initialize(x_pixmap_))
    return VaapiStatus::Codes::kFailedToInitializeImage;

  gl::ScopedTextureBinder texture_binder(texture_target_, texture_id_);
  if (!image->BindTexImage(texture_target_))
    return VaapiStatus::Codes::kFailedToBindTexture;

  gl_image_ = image;

  DCHECK(bind_image_cb_);
  if (!bind_image_cb_.Run(client_texture_id_, texture_target_, gl_image_)) {
    return VaapiStatus::Codes::kFailedToBindImage;
  }

  return VaapiStatus::Codes::kOk;
}

bool VaapiPictureNativePixmapAngle::ImportGpuMemoryBufferHandle(
    gfx::BufferFormat format,
    gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle) {
  NOTREACHED();
  return false;
}

bool VaapiPictureNativePixmapAngle::DownloadFromSurface(
    scoped_refptr<VASurface> va_surface) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!make_context_current_cb_ || !make_context_current_cb_.Run())
    return false;

  DCHECK(texture_id_);
  gl::ScopedTextureBinder texture_binder(texture_target_, texture_id_);

  // GL needs to re-bind the texture after the pixmap content is updated so that
  // the compositor sees the updated contents (we found this out experimentally)
  gl_image_->ReleaseEGLImage();

  DCHECK(gfx::Rect(va_surface->size()).Contains(gfx::Rect(visible_size_)));
  if (!vaapi_wrapper_->PutSurfaceIntoPixmap(va_surface->id(), x_pixmap_,
                                            visible_size_)) {
    return false;
  }
  return gl_image_->BindTexImage(texture_target_);
}

VASurfaceID VaapiPictureNativePixmapAngle::va_surface_id() const {
  return VA_INVALID_ID;
}

}  // namespace media
