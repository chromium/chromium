// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_picture_factory.h"

#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "media/video/picture.h"
#include "ui/base/ui_base_features.h"
#include "ui/gl/gl_bindings.h"

#if defined(USE_X11)
#include "media/gpu/vaapi/vaapi_picture_native_pixmap_angle.h"
#include "media/gpu/vaapi/vaapi_picture_tfp.h"
#endif
#if defined(USE_OZONE)
#include "media/gpu/vaapi/vaapi_picture_native_pixmap_ozone.h"
#endif
#if defined(USE_EGL)
#include "media/gpu/vaapi/vaapi_picture_native_pixmap_egl.h"
#endif

namespace media {

VaapiPictureFactory::VaapiPictureFactory() {
  vaapi_impl_pairs_.insert(
      std::make_pair(gl::kGLImplementationEGLGLES2,
                     VaapiPictureFactory::kVaapiImplementationDrm));
#if defined(USE_X11)
  vaapi_impl_pairs_.insert(
      std::make_pair(gl::kGLImplementationEGLANGLE,
                     VaapiPictureFactory::kVaapiImplementationAngle));
  if (!features::IsUsingOzonePlatform()) {
    vaapi_impl_pairs_.insert(
        std::make_pair(gl::kGLImplementationDesktopGL,
                       VaapiPictureFactory::kVaapiImplementationX11));
  }
#endif
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

  // Select DRM(egl) / TFP(glx) at runtime with --use-gl=egl / --use-gl=desktop

#if defined(USE_OZONE)
  if (features::IsUsingOzonePlatform())
    return CreateVaapiPictureNativeForOzone(
        vaapi_wrapper, make_context_current_cb, bind_image_cb, picture_buffer,
        visible_size, client_texture_id, service_texture_id);
#endif
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
#if defined(USE_OZONE)
  if (features::IsUsingOzonePlatform())
    return GL_TEXTURE_EXTERNAL_OES;
#endif
  return GL_TEXTURE_2D;
}

gfx::BufferFormat VaapiPictureFactory::GetBufferFormat() {
#if defined(USE_OZONE)
  if (features::IsUsingOzonePlatform())
    return gfx::BufferFormat::YUV_420_BIPLANAR;
#endif
  return gfx::BufferFormat::RGBX_8888;
}

#if defined(USE_OZONE)
std::unique_ptr<VaapiPicture>
VaapiPictureFactory::CreateVaapiPictureNativeForOzone(
    scoped_refptr<VaapiWrapper> vaapi_wrapper,
    const MakeGLContextCurrentCallback& make_context_current_cb,
    const BindGLImageCallback& bind_image_cb,
    const PictureBuffer& picture_buffer,
    const gfx::Size& visible_size,
    uint32_t client_texture_id,
    uint32_t service_texture_id) {
  DCHECK(features::IsUsingOzonePlatform());
  switch (GetVaapiImplementation(gl::GetGLImplementation())) {
    // We can be called without GL initialized, which is valid if we use Ozone.
    case kVaapiImplementationNone:
      FALLTHROUGH;
    case kVaapiImplementationDrm:
      return std::make_unique<VaapiPictureNativePixmapOzone>(
          std::move(vaapi_wrapper), make_context_current_cb, bind_image_cb,
          picture_buffer.id(), picture_buffer.size(), visible_size,
          service_texture_id, client_texture_id,
          picture_buffer.texture_target());
      break;

    default:
      NOTREACHED();
      return nullptr;
  }

  return nullptr;
}
#endif  // USE_OZONE

std::unique_ptr<VaapiPicture> VaapiPictureFactory::CreateVaapiPictureNative(
    scoped_refptr<VaapiWrapper> vaapi_wrapper,
    const MakeGLContextCurrentCallback& make_context_current_cb,
    const BindGLImageCallback& bind_image_cb,
    const PictureBuffer& picture_buffer,
    const gfx::Size& visible_size,
    uint32_t client_texture_id,
    uint32_t service_texture_id) {
  switch (GetVaapiImplementation(gl::GetGLImplementation())) {
#if defined(USE_EGL)
    case kVaapiImplementationDrm:
      return std::make_unique<VaapiPictureNativePixmapEgl>(
          std::move(vaapi_wrapper), make_context_current_cb, bind_image_cb,
          picture_buffer.id(), picture_buffer.size(), visible_size,
          service_texture_id, client_texture_id,
          picture_buffer.texture_target());
#endif  // USE_EGL

#if defined(USE_X11)
    case kVaapiImplementationX11:
      DCHECK(!features::IsUsingOzonePlatform());
      return std::make_unique<VaapiTFPPicture>(
          std::move(vaapi_wrapper), make_context_current_cb, bind_image_cb,
          picture_buffer.id(), picture_buffer.size(), visible_size,
          service_texture_id, client_texture_id,
          picture_buffer.texture_target());
      break;
    case kVaapiImplementationAngle:
      return std::make_unique<VaapiPictureNativePixmapAngle>(
          std::move(vaapi_wrapper), make_context_current_cb, bind_image_cb,
          picture_buffer.id(), picture_buffer.size(), visible_size,
          service_texture_id, client_texture_id,
          picture_buffer.texture_target());
      break;
#endif  // USE_X11

    default:
      NOTREACHED();
      return nullptr;
  }

  return nullptr;
}

}  // namespace media
