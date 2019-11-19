// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_picture_native_pixmap_ozone.h"

#include "media/gpu/vaapi/va_surface.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_image_native_pixmap.h"
#include "ui/gl/scoped_binders.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace media {

VaapiPictureNativePixmapOzone::VaapiPictureNativePixmapOzone(
    const scoped_refptr<VaapiWrapper>& vaapi_wrapper,
    const MakeGLContextCurrentCallback& make_context_current_cb,
    const BindGLImageCallback& bind_image_cb,
    int32_t picture_buffer_id,
    const gfx::Size& size,
    uint32_t texture_id,
    uint32_t client_texture_id,
    uint32_t texture_target)
    : VaapiPictureNativePixmap(vaapi_wrapper,
                               make_context_current_cb,
                               bind_image_cb,
                               picture_buffer_id,
                               size,
                               texture_id,
                               client_texture_id,
                               texture_target) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Either |texture_id| and |client_texture_id| are both zero, or not.
  DCHECK((texture_id == 0 && client_texture_id == 0) ||
         (texture_id != 0 && client_texture_id != 0));
}

VaapiPictureNativePixmapOzone::~VaapiPictureNativePixmapOzone() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (gl_image_ && make_context_current_cb_.Run()) {
    gl_image_->ReleaseTexImage(texture_target_);
    DCHECK_EQ(glGetError(), static_cast<GLenum>(GL_NO_ERROR));
  }
}

bool VaapiPictureNativePixmapOzone::Initialize(
    scoped_refptr<gfx::NativePixmap> pixmap) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pixmap);
  DCHECK(pixmap->AreDmaBufFdsValid());
  // Create a |va_surface_| from dmabuf fds (pixmap->GetDmaBufFd)
  va_surface_ = vaapi_wrapper_->CreateVASurfaceForPixmap(pixmap);
  if (!va_surface_) {
    LOG(ERROR) << "Failed creating VASurface for NativePixmap";
    return false;
  }

  // ARC++ has no texture ids.
  if (texture_id_ == 0 && client_texture_id_ == 0)
    return true;

  // Import dmabuf fds into the output gl texture through EGLImage.
  if (make_context_current_cb_ && !make_context_current_cb_.Run())
    return false;

  gl::ScopedTextureBinder texture_binder(texture_target_, texture_id_);

  const gfx::BufferFormat format = pixmap->GetBufferFormat();

  auto image = base::MakeRefCounted<gl::GLImageNativePixmap>(size_, format);
  if (!image->Initialize(std::move(pixmap))) {
    LOG(ERROR) << "Failed to create GLImage";
    return false;
  }
  gl_image_ = image;
  if (!gl_image_->BindTexImage(texture_target_)) {
    LOG(ERROR) << "Failed to bind texture to GLImage";
    return false;
  }

  if (bind_image_cb_ &&
      !bind_image_cb_.Run(client_texture_id_, texture_target_, gl_image_,
                          true /* can_bind_to_sampler */)) {
    LOG(ERROR) << "Failed to bind client_texture_id";
    return false;
  }

  return true;
}

bool VaapiPictureNativePixmapOzone::Allocate(gfx::BufferFormat format) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ui::OzonePlatform* platform = ui::OzonePlatform::GetInstance();
  ui::SurfaceFactoryOzone* factory = platform->GetSurfaceFactoryOzone();
  auto pixmap = factory->CreateNativePixmap(
      gfx::kNullAcceleratedWidget, VK_NULL_HANDLE, size_, format,
      gfx::BufferUsage::SCANOUT_VDA_WRITE);
  if (!pixmap) {
    LOG(ERROR) << "Failed allocating a pixmap";
    return false;
  }

  return Initialize(std::move(pixmap));
}

bool VaapiPictureNativePixmapOzone::ImportGpuMemoryBufferHandle(
    gfx::BufferFormat format,
    gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ui::OzonePlatform* platform = ui::OzonePlatform::GetInstance();
  ui::SurfaceFactoryOzone* factory = platform->GetSurfaceFactoryOzone();
  // CreateNativePixmapFromHandle() will take ownership of the handle.
  auto pixmap = factory->CreateNativePixmapFromHandle(
      gfx::kNullAcceleratedWidget, size_, format,
      std::move(gpu_memory_buffer_handle.native_pixmap_handle));

  if (!pixmap) {
    LOG(ERROR) << "Failed creating a pixmap from a native handle";
    return false;
  }

  return Initialize(std::move(pixmap));
}

}  // namespace media
