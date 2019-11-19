// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_picture_native_pixmap_egl.h"

#include "base/file_descriptor_posix.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "media/gpu/vaapi/va_surface.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "ui/gfx/linux/native_pixmap_dmabuf.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_image_native_pixmap.h"
#include "ui/gl/scoped_binders.h"

namespace media {

VaapiPictureNativePixmapEgl::VaapiPictureNativePixmapEgl(
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
  DCHECK(texture_id);
  DCHECK(client_texture_id);
}

VaapiPictureNativePixmapEgl::~VaapiPictureNativePixmapEgl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (gl_image_ && make_context_current_cb_.Run()) {
    gl_image_->ReleaseTexImage(texture_target_);
    DCHECK_EQ(glGetError(), static_cast<GLenum>(GL_NO_ERROR));
  }
}

bool VaapiPictureNativePixmapEgl::Initialize(
    scoped_refptr<gfx::NativePixmap> pixmap) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pixmap);
  DCHECK(pixmap->AreDmaBufFdsValid());

  // Create a |va_surface_| from dmabuf fds (pixmap->GetDmaBufFd)
  va_surface_ = vaapi_wrapper_->CreateVASurfaceForPixmap(std::move(pixmap));
  if (!va_surface_) {
    LOG(ERROR) << "Failed creating VASurface for NativePixmap";
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

bool VaapiPictureNativePixmapEgl::Allocate(gfx::BufferFormat format) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Export the gl texture as dmabuf.
  if (make_context_current_cb_ && !make_context_current_cb_.Run())
    return false;

  auto image = base::MakeRefCounted<gl::GLImageNativePixmap>(size_, format);
  // Create an EGLImage from a gl texture
  if (!image->InitializeFromTexture(texture_id_)) {
    DLOG(ERROR) << "Failed to initialize eglimage from texture id: "
                << texture_id_;
    return false;
  }

  // Export the EGLImage as dmabuf.
  gfx::NativePixmapHandle native_pixmap_handle = image->ExportHandle();
  if (!native_pixmap_handle.planes.size()) {
    DLOG(ERROR) << "Failed to export EGLImage as dmabuf fds";
    return false;
  }

  // Convert NativePixmapHandle to NativePixmapDmaBuf.
  scoped_refptr<gfx::NativePixmap> native_pixmap_dmabuf(
      new gfx::NativePixmapDmaBuf(size_, format,
                                  std::move(native_pixmap_handle)));
  if (!native_pixmap_dmabuf->AreDmaBufFdsValid()) {
    DLOG(ERROR) << "Invalid dmabuf fds";
    return false;
  }

  if (!image->BindTexImage(texture_target_)) {
    DLOG(ERROR) << "Failed to bind texture to GLImage";
    return false;
  }

  // The |va_surface_| created from |native_pixmap_dmabuf| shares the ownership
  // of the buffer. So the only reason to keep a reference on the image is
  // because the GPU service needs to track this image as it will be attached
  // to a client texture.
  gl_image_ = image;

  return Initialize(std::move(native_pixmap_dmabuf));
}

bool VaapiPictureNativePixmapEgl::ImportGpuMemoryBufferHandle(
    gfx::BufferFormat format,
    gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  return false;
}

}  // namespace media
