// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_picture_factory.h"

#include "base/containers/contains.h"
#include "build/build_config.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "media/video/picture.h"
#include "ui/gl/gl_bindings.h"

#if BUILDFLAG(IS_OZONE)
#include "media/gpu/vaapi/vaapi_picture_native_pixmap_ozone.h"
#endif  // BUILDFLAG(IS_OZONE)
#if BUILDFLAG(USE_VAAPI_X11)
#include "media/gpu/vaapi/vaapi_picture_native_pixmap_angle.h"
#endif  // BUILDFLAG(USE_VAAPI_X11)
#if defined(USE_EGL)
#include "media/gpu/vaapi/vaapi_picture_native_pixmap_egl.h"
#endif

namespace media {

namespace {

template <typename PictureType>
std::unique_ptr<VaapiPicture> CreateVaapiPictureNativeImpl(
    scoped_refptr<VaapiWrapper> vaapi_wrapper,
    const MakeGLContextCurrentCallback& make_context_current_cb,
    const BindGLImageCallback& bind_image_cb,
    const PictureBuffer& picture_buffer,
    const gfx::Size& visible_size,
    uint32_t client_texture_id,
    uint32_t service_texture_id) {
  return std::make_unique<PictureType>(
      std::move(vaapi_wrapper), make_context_current_cb, bind_image_cb,
      picture_buffer.id(), picture_buffer.size(), visible_size,
      service_texture_id, client_texture_id, picture_buffer.texture_target());
}

}  // namespace

VaapiPictureFactory::VaapiPictureFactory() {
  vaapi_impl_pairs_.insert(
      std::make_pair(gl::kGLImplementationEGLGLES2,
                     VaapiPictureFactory::kVaapiImplementationDrm));
#if BUILDFLAG(USE_VAAPI_X11)
  vaapi_impl_pairs_.insert(
      std::make_pair(gl::kGLImplementationEGLANGLE,
                     VaapiPictureFactory::kVaapiImplementationAngle));
#elif BUILDFLAG(IS_OZONE)
  vaapi_impl_pairs_.insert(
      std::make_pair(gl::kGLImplementationEGLANGLE,
                     VaapiPictureFactory::kVaapiImplementationDrm));
#endif

  DeterminePictureCreationAndDownloadingMechanism();
}

VaapiPictureFactory::~VaapiPictureFactory() = default;

std::unique_ptr<VaapiPicture> VaapiPictureFactory::Create(
    scoped_refptr<VaapiWrapper> vaapi_wrapper,
    const MakeGLContextCurrentCallback& make_context_current_cb,
    const BindGLImageCallback& bind_image_cb,
    const PictureBuffer& picture_buffer,
    const gfx::Size& visible_size) {
  // ARC++ sends |picture_buffer| with no texture_target().
  DCHECK(picture_buffer.texture_target() == GetGLTextureTarget() ||
         picture_buffer.texture_target() == 0u);

  // |client_texture_ids| and |service_texture_ids| are empty from ARC++.
  const uint32_t client_texture_id =
      !picture_buffer.client_texture_ids().empty()
          ? picture_buffer.client_texture_ids()[0]
          : 0;
  const uint32_t service_texture_id =
      !picture_buffer.service_texture_ids().empty()
          ? picture_buffer.service_texture_ids()[0]
          : 0;

  return CreateVaapiPictureNative(vaapi_wrapper, make_context_current_cb,
                                  bind_image_cb, picture_buffer, visible_size,
                                  client_texture_id, service_texture_id);
}

VaapiPictureFactory::VaapiImplementation
VaapiPictureFactory::GetVaapiImplementation(gl::GLImplementation gl_impl) {
  if (base::Contains(vaapi_impl_pairs_, gl_impl))
    return vaapi_impl_pairs_[gl_impl];
  return kVaapiImplementationNone;
}

uint32_t VaapiPictureFactory::GetGLTextureTarget() {
#if BUILDFLAG(USE_VAAPI_X11)
  return GL_TEXTURE_2D;
#else
  return GL_TEXTURE_EXTERNAL_OES;
#endif
}

gfx::BufferFormat VaapiPictureFactory::GetBufferFormat() {
#if BUILDFLAG(USE_VAAPI_X11)
  return gfx::BufferFormat::RGBX_8888;
#else
  return gfx::BufferFormat::YUV_420_BIPLANAR;
#endif
}

void VaapiPictureFactory::DeterminePictureCreationAndDownloadingMechanism() {
  switch (GetVaapiImplementation(gl::GetGLImplementation())) {
#if BUILDFLAG(IS_OZONE)
    // We can be called without GL initialized, which is valid if we use Ozone.
    case kVaapiImplementationNone:
      create_picture_cb_ = base::BindRepeating(
          &CreateVaapiPictureNativeImpl<VaapiPictureNativePixmapOzone>);
      needs_vpp_for_downloading_ = true;
      break;
#endif  // BUILDFLAG(IS_OZONE)
#if BUILDFLAG(USE_VAAPI_X11)
    case kVaapiImplementationAngle:
      create_picture_cb_ = base::BindRepeating(
          &CreateVaapiPictureNativeImpl<VaapiPictureNativePixmapAngle>);
      // Neither VaapiTFPPicture or VaapiPictureNativePixmapAngle needs the VPP.
      needs_vpp_for_downloading_ = false;
      break;
#endif  // BUILDFLAG(USE_VAAPI_X11)
    case kVaapiImplementationDrm:
#if BUILDFLAG(IS_OZONE)
      create_picture_cb_ = base::BindRepeating(
          &CreateVaapiPictureNativeImpl<VaapiPictureNativePixmapOzone>);
      needs_vpp_for_downloading_ = true;
      break;
#elif defined(USE_EGL)
      create_picture_cb_ = base::BindRepeating(
          &CreateVaapiPictureNativeImpl<VaapiPictureNativePixmapEgl>);
      needs_vpp_for_downloading_ = true;
      break;
#else
      // ozone or egl must be used to use the DRM implementation.
      [[fallthrough]];
#endif
    default:
      NOTREACHED();
      break;
  }
}

bool VaapiPictureFactory::NeedsProcessingPipelineForDownloading() const {
  return needs_vpp_for_downloading_;
}

std::unique_ptr<VaapiPicture> VaapiPictureFactory::CreateVaapiPictureNative(
    scoped_refptr<VaapiWrapper> vaapi_wrapper,
    const MakeGLContextCurrentCallback& make_context_current_cb,
    const BindGLImageCallback& bind_image_cb,
    const PictureBuffer& picture_buffer,
    const gfx::Size& visible_size,
    uint32_t client_texture_id,
    uint32_t service_texture_id) {
  CHECK(create_picture_cb_);
  return create_picture_cb_.Run(
      std::move(vaapi_wrapper), make_context_current_cb, bind_image_cb,
      picture_buffer, visible_size, client_texture_id, service_texture_id);
}

}  // namespace media
