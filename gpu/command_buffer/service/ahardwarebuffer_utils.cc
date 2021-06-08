// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/ahardwarebuffer_utils.h"

#include <android/hardware_buffer.h>

#include "base/android/scoped_hardware_buffer_handle.h"
#include "base/check.h"
#include "base/notreached.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/vulkan/vulkan_image.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_image_ahardwarebuffer.h"
#include "ui/gl/scoped_binders.h"

namespace gpu {
namespace {

gles2::Texture* MakeGLTexture(
    GLenum target,
    GLuint service_id,
    scoped_refptr<gl::GLImageAHardwareBuffer> egl_image,
    const gfx::Size& size,
    const gfx::Rect& cleared_rect) {
  auto* texture = new gles2::Texture(service_id);
  texture->SetLightweightRef();
  texture->SetTarget(target, 1);
  texture->set_min_filter(GL_LINEAR);
  texture->set_mag_filter(GL_LINEAR);
  texture->set_wrap_t(GL_CLAMP_TO_EDGE);
  texture->set_wrap_s(GL_CLAMP_TO_EDGE);

  texture->SetLevelInfo(target, 0, egl_image->GetInternalFormat(), size.width(),
                        size.height(), 1, 0, egl_image->GetDataFormat(),
                        egl_image->GetDataType(), cleared_rect);
  texture->SetLevelImage(target, 0, egl_image.get(), gles2::Texture::BOUND);
  texture->SetImmutable(true, false);
  return texture;
}

scoped_refptr<gles2::TexturePassthrough> MakeGLTexturePassthrough(
    GLenum target,
    GLuint service_id,
    scoped_refptr<gl::GLImageAHardwareBuffer> egl_image,
    const size_t estimated_size) {
  auto passthrough_texture =
      base::MakeRefCounted<gles2::TexturePassthrough>(service_id, target);
  passthrough_texture->SetEstimatedSize(estimated_size);
  passthrough_texture->SetLevelImage(target, 0, egl_image.get());
  passthrough_texture->set_is_bind_pending(false);
  return passthrough_texture;
}

void GenGLTextureInternal(
    AHardwareBuffer* buffer,
    GLenum target,
    const gfx::ColorSpace& color_space,
    const gfx::Size& size,
    const size_t estimated_size,
    const gfx::Rect& cleared_rect,
    scoped_refptr<gles2::TexturePassthrough>* passthrough_texture,
    gles2::Texture** texture) {
  gl::GLApi* api = gl::g_current_gl_context;
  GLuint service_id = 0;
  api->glGenTexturesFn(1, &service_id);
  gl::ScopedTextureBinder texture_binder(target, service_id);

  api->glTexParameteriFn(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  api->glTexParameteriFn(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  api->glTexParameteriFn(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  api->glTexParameteriFn(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  // Create an egl image using AHardwareBuffer.
  auto egl_image = base::MakeRefCounted<gl::GLImageAHardwareBuffer>(size);
  if (!egl_image->Initialize(buffer, false)) {
    LOG(ERROR) << "Failed to create EGL image";
    api->glDeleteTexturesFn(1, &service_id);
    return;
  }

  if (!egl_image->BindTexImage(target)) {
    LOG(ERROR) << "Failed to bind egl image";
    api->glDeleteTexturesFn(1, &service_id);
    return;
  }
  egl_image->SetColorSpace(color_space);

  if (passthrough_texture) {
    *passthrough_texture = MakeGLTexturePassthrough(
        target, service_id, std::move(egl_image), estimated_size);
  } else {
    *texture = MakeGLTexture(target, service_id, std::move(egl_image), size,
                             cleared_rect);
  }
}

}  // namespace

bool AHardwareBufferSupportedFormat(viz::ResourceFormat format) {
  switch (format) {
    case viz::RGBA_8888:
    case viz::RGB_565:
    case viz::BGR_565:
    case viz::RGBA_F16:
    case viz::RGBX_8888:
    case viz::RGBA_1010102:
      return true;
    default:
      return false;
  }
}

unsigned int AHardwareBufferFormat(viz::ResourceFormat format) {
  DCHECK(AHardwareBufferSupportedFormat(format));
  switch (format) {
    case viz::RGBA_8888:
      return AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
    case viz::RGB_565:
      return AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM;
    case viz::BGR_565:
      return AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM;
    case viz::RGBA_F16:
      return AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT;
    case viz::RGBX_8888:
      return AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM;
    case viz::RGBA_1010102:
      return AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM;
    default:
      NOTREACHED();
      return AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
  }
}

gles2::Texture* GenGLTexture(AHardwareBuffer* buffer,
                             GLenum target,
                             const gfx::ColorSpace& color_space,
                             const gfx::Size& size,
                             const size_t estimated_size,
                             const gfx::Rect& cleared_rect) {
  gles2::Texture* texture = nullptr;
  GenGLTextureInternal(buffer, target, color_space, size, estimated_size,
                       cleared_rect, nullptr /* passthrough_texture */,
                       &texture);
  return texture;
}

scoped_refptr<gles2::TexturePassthrough> GenGLTexturePassthrough(
    AHardwareBuffer* buffer,
    GLenum target,
    const gfx::ColorSpace& color_space,
    const gfx::Size& size,
    const size_t estimated_size,
    const gfx::Rect& cleared_rect) {
  scoped_refptr<gles2::TexturePassthrough> passthrough_texture;
  GenGLTextureInternal(buffer, target, color_space, size, estimated_size,
                       cleared_rect, &passthrough_texture,
                       nullptr /* texture */);
  return passthrough_texture;
}

std::unique_ptr<VulkanImage> CreateVkImageFromAhbHandle(
    base::android::ScopedHardwareBufferHandle ahb_handle,
    SharedContextState* context_state,
    const gfx::Size& size,
    const viz::ResourceFormat& format) {
  DCHECK(context_state);
  DCHECK(context_state->GrContextIsVulkan());

  auto* device_queue = context_state->vk_context_provider()->GetDeviceQueue();
  gfx::GpuMemoryBufferHandle gmb_handle(std::move(ahb_handle));
  return VulkanImage::CreateFromGpuMemoryBufferHandle(
      device_queue, std::move(gmb_handle), size, ToVkFormat(format),
      0 /* usage */);
}

}  // namespace gpu
