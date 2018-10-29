// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/skia_utils.h"

#include "gpu/command_buffer/service/error_state.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/command_buffer/service/wrapped_sk_image.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"
#include "ui/gl/gl_gl_api_implementation.h"

namespace gpu {

bool GetGrBackendTexture(const gl::GLVersionInfo& version_info,
                         const TextureBase& texture_base,
                         GLint sk_color_type,
                         GrBackendTexture* gr_texture) {
  if (texture_base.GetType() != TextureBase::Type::kValidated) {
    NOTIMPLEMENTED();
    return false;
  }
  const auto* texture = static_cast<const gles2::Texture*>(&texture_base);

  int width;
  int height;
  int depth;
  if (!texture->GetLevelSize(texture->target(), 0, &width, &height, &depth)) {
    LOG(ERROR) << "GetGrBackendTexture: missing texture size info.";
    return false;
  }
  GLenum type;
  GLenum internal_format;
  if (!texture->GetLevelType(texture->target(), 0, &type, &internal_format)) {
    LOG(ERROR) << "GetGrBackendTexture: missing texture type info.";
    return false;
  }

  GLenum driver_internal_format =
      GetInternalFormat(&version_info, internal_format);
  return GetGrBackendTexture(texture->target(), gfx::Size(width, height),
                             internal_format, driver_internal_format,
                             texture->service_id(), sk_color_type, gr_texture);
}

bool GetGrBackendTexture(GLenum target,
                         const gfx::Size& size,
                         GLenum internal_format,
                         GLenum driver_internal_format,
                         GLuint service_id,
                         GLint sk_color_type,
                         GrBackendTexture* gr_texture) {
  if (target != GL_TEXTURE_2D && target != GL_TEXTURE_RECTANGLE_ARB) {
    LOG(ERROR) << "GetGrBackendTexture: invalid texture target.";
    return false;
  }

  GrGLTextureInfo texture_info;
  texture_info.fID = service_id;
  texture_info.fTarget = target;

  // |driver_internal_format| may be a base internal format but Skia requires a
  // sized internal format. So this may be adjusted below.
  texture_info.fFormat = driver_internal_format;
  switch (sk_color_type) {
    case kARGB_4444_SkColorType:
      if (internal_format != GL_RGBA4 && internal_format != GL_RGBA) {
        LOG(ERROR) << "GetGrBackendTexture: color type mismatch.";
        return false;
      }
      if (texture_info.fFormat == GL_RGBA)
        texture_info.fFormat = GL_RGBA4;
      break;
    case kRGBA_8888_SkColorType:
      if (internal_format != GL_RGBA8_OES && internal_format != GL_RGBA) {
        LOG(ERROR) << "GetGrBackendTexture: missing texture type info.";
        return false;
      }
      if (texture_info.fFormat == GL_RGBA)
        texture_info.fFormat = GL_RGBA8_OES;
      break;
    case kBGRA_8888_SkColorType:
      if (internal_format != GL_BGRA_EXT && internal_format != GL_BGRA8_EXT) {
        LOG(ERROR) << "GetGrBackendTexture: missing texture type info.";
        return false;
      }
      if (texture_info.fFormat == GL_BGRA_EXT)
        texture_info.fFormat = GL_BGRA8_EXT;
      if (texture_info.fFormat == GL_RGBA)
        texture_info.fFormat = GL_RGBA8_OES;
      break;
    default:
      LOG(ERROR) << "GetGrBackendTexture: unsupported color type.";
      return false;
  }

  *gr_texture = GrBackendTexture(size.width(), size.height(), GrMipMapped::kNo,
                                 texture_info);
  return true;
}

}  // namespace gpu
