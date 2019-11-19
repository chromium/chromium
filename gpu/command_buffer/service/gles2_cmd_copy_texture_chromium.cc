// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_cmd_copy_texture_chromium.h"

#include <stddef.h>

#include <algorithm>
#include <unordered_map>

#include "gpu/command_buffer/service/context_state.h"
#include "gpu/command_buffer/service/decoder_context.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/gles2_cmd_copy_tex_image.h"
#include "gpu/command_buffer/service/shader_manager.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gl/gl_enums.h"
#include "ui/gl/gl_version_info.h"

namespace gpu {
namespace gles2 {

namespace {

enum {
  SAMPLER_2D,
  SAMPLER_RECTANGLE_ARB,
  SAMPLER_EXTERNAL_OES,
  NUM_SAMPLERS
};

enum {
  S_FORMAT_ALPHA,
  S_FORMAT_LUMINANCE,
  S_FORMAT_LUMINANCE_ALPHA,
  S_FORMAT_RED,
  S_FORMAT_RGB,
  S_FORMAT_RGBA,
  S_FORMAT_RGB8,
  S_FORMAT_RGBA8,
  S_FORMAT_BGRA_EXT,
  S_FORMAT_BGRA8_EXT,
  S_FORMAT_RGB_YCBCR_420V_CHROMIUM,
  S_FORMAT_RGB_YCBCR_422_CHROMIUM,
  S_FORMAT_COMPRESSED,
  S_FORMAT_RGB10_A2,
  S_FORMAT_RGB_YCBCR_P010_CHROMIUM,
  NUM_S_FORMAT
};

enum {
  D_FORMAT_RGB,
  D_FORMAT_RGBA,
  D_FORMAT_RGB8,
  D_FORMAT_RGBA8,
  D_FORMAT_BGRA_EXT,
  D_FORMAT_BGRA8_EXT,
  D_FORMAT_SRGB_EXT,
  D_FORMAT_SRGB_ALPHA_EXT,
  D_FORMAT_R8,
  D_FORMAT_R8UI,
  D_FORMAT_RG8,
  D_FORMAT_RG8UI,
  D_FORMAT_SRGB8,
  D_FORMAT_RGB565,
  D_FORMAT_RGB8UI,
  D_FORMAT_SRGB8_ALPHA8,
  D_FORMAT_RGB5_A1,
  D_FORMAT_RGBA4,
  D_FORMAT_RGBA8UI,
  D_FORMAT_RGB9_E5,
  D_FORMAT_R16F,
  D_FORMAT_R32F,
  D_FORMAT_RG16F,
  D_FORMAT_RG32F,
  D_FORMAT_RGB16F,
  D_FORMAT_RGB32F,
  D_FORMAT_RGBA16F,
  D_FORMAT_RGBA32F,
  D_FORMAT_R11F_G11F_B10F,
  D_FORMAT_RGB10_A2,
  NUM_D_FORMAT
};

const unsigned kAlphaSize = 4;
const unsigned kDitherSize = 2;
const unsigned kNumVertexShaders = NUM_SAMPLERS;
const unsigned kNumFragmentShaders =
    kAlphaSize * kDitherSize * NUM_SAMPLERS * NUM_S_FORMAT * NUM_D_FORMAT;

typedef unsigned ShaderId;

ShaderId GetVertexShaderId(GLenum target) {
  ShaderId id = 0;
  switch (target) {
    case GL_TEXTURE_2D:
      id = SAMPLER_2D;
      break;
    case GL_TEXTURE_RECTANGLE_ARB:
      id = SAMPLER_RECTANGLE_ARB;
      break;
    case GL_TEXTURE_EXTERNAL_OES:
      id = SAMPLER_EXTERNAL_OES;
      break;
    default:
      NOTREACHED();
      break;
  }
  return id;
}

// Returns the correct fragment shader id to evaluate the copy operation for
// the premultiply alpha pixel store settings and target.
ShaderId GetFragmentShaderId(bool premultiply_alpha,
                             bool unpremultiply_alpha,
                             bool dither,
                             GLenum target,
                             GLenum source_format,
                             GLenum dest_format) {
  unsigned alphaIndex = 0;
  unsigned ditherIndex = 0;
  unsigned targetIndex = 0;
  unsigned sourceFormatIndex = 0;
  unsigned destFormatIndex = 0;

  alphaIndex = (premultiply_alpha   ? (1 << 0) : 0) |
               (unpremultiply_alpha ? (1 << 1) : 0);
  ditherIndex = dither ? 1 : 0;

  switch (target) {
    case GL_TEXTURE_2D:
      targetIndex = SAMPLER_2D;
      break;
    case GL_TEXTURE_RECTANGLE_ARB:
      targetIndex = SAMPLER_RECTANGLE_ARB;
      break;
    case GL_TEXTURE_EXTERNAL_OES:
      targetIndex = SAMPLER_EXTERNAL_OES;
      break;
    default:
      NOTREACHED();
      break;
  }

  switch (source_format) {
    case GL_ALPHA:
      sourceFormatIndex = S_FORMAT_ALPHA;
      break;
    case GL_LUMINANCE:
      sourceFormatIndex = S_FORMAT_LUMINANCE;
      break;
    case GL_LUMINANCE_ALPHA:
      sourceFormatIndex = S_FORMAT_LUMINANCE_ALPHA;
      break;
    case GL_RED:
    case GL_R16_EXT:
      sourceFormatIndex = S_FORMAT_RED;
      break;
    case GL_RGB:
      sourceFormatIndex = S_FORMAT_RGB;
      break;
    case GL_RGBA:
      sourceFormatIndex = S_FORMAT_RGBA;
      break;
    case GL_RGB8:
      sourceFormatIndex = S_FORMAT_RGB8;
      break;
    case GL_RGBA8:
      sourceFormatIndex = S_FORMAT_RGBA8;
      break;
    case GL_BGRA_EXT:
      sourceFormatIndex = S_FORMAT_BGRA_EXT;
      break;
    case GL_BGRA8_EXT:
      sourceFormatIndex = S_FORMAT_BGRA8_EXT;
      break;
    case GL_RGB_YCBCR_420V_CHROMIUM:
      sourceFormatIndex = S_FORMAT_RGB_YCBCR_420V_CHROMIUM;
      break;
    case GL_RGB_YCBCR_422_CHROMIUM:
      sourceFormatIndex = S_FORMAT_RGB_YCBCR_422_CHROMIUM;
      break;
    case GL_ATC_RGB_AMD:
    case GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD:
    case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
    case GL_ETC1_RGB8_OES:
      sourceFormatIndex = S_FORMAT_COMPRESSED;
      break;
    case GL_RGB10_A2:
      sourceFormatIndex = S_FORMAT_RGB10_A2;
      break;
    case GL_RGB_YCBCR_P010_CHROMIUM:
      sourceFormatIndex = S_FORMAT_RGB_YCBCR_P010_CHROMIUM;
      break;
    default:
      NOTREACHED() << "Invalid source format "
                   << gl::GLEnums::GetStringEnum(source_format);
      break;
  }

  switch (dest_format) {
    case GL_RGB:
      destFormatIndex = D_FORMAT_RGB;
      break;
    case GL_RGBA:
      destFormatIndex = D_FORMAT_RGBA;
      break;
    case GL_RGB8:
      destFormatIndex = D_FORMAT_RGB8;
      break;
    case GL_RGBA8:
      destFormatIndex = D_FORMAT_RGBA8;
      break;
    case GL_BGRA_EXT:
      destFormatIndex = D_FORMAT_BGRA_EXT;
      break;
    case GL_BGRA8_EXT:
      destFormatIndex = D_FORMAT_BGRA8_EXT;
      break;
    case GL_SRGB_EXT:
      destFormatIndex = D_FORMAT_SRGB_EXT;
      break;
    case GL_SRGB_ALPHA_EXT:
      destFormatIndex = D_FORMAT_SRGB_ALPHA_EXT;
      break;
    case GL_R8:
      destFormatIndex = D_FORMAT_R8;
      break;
    case GL_R8UI:
      destFormatIndex = D_FORMAT_R8UI;
      break;
    case GL_RG8:
      destFormatIndex = D_FORMAT_RG8;
      break;
    case GL_RG8UI:
      destFormatIndex = D_FORMAT_RG8UI;
      break;
    case GL_SRGB8:
      destFormatIndex = D_FORMAT_SRGB8;
      break;
    case GL_RGB565:
      destFormatIndex = D_FORMAT_RGB565;
      break;
    case GL_RGB8UI:
      destFormatIndex = D_FORMAT_RGB8UI;
      break;
    case GL_SRGB8_ALPHA8:
      destFormatIndex = D_FORMAT_SRGB8_ALPHA8;
      break;
    case GL_RGB5_A1:
      destFormatIndex = D_FORMAT_RGB5_A1;
      break;
    case GL_RGBA4:
      destFormatIndex = D_FORMAT_RGBA4;
      break;
    case GL_RGBA8UI:
      destFormatIndex = D_FORMAT_RGBA8UI;
      break;
    case GL_RGB9_E5:
      destFormatIndex = D_FORMAT_RGB9_E5;
      break;
    case GL_R16F:
      destFormatIndex = D_FORMAT_R16F;
      break;
    case GL_R32F:
      destFormatIndex = D_FORMAT_R32F;
      break;
    case GL_RG16F:
      destFormatIndex = D_FORMAT_RG16F;
      break;
    case GL_RG32F:
      destFormatIndex = D_FORMAT_RG32F;
      break;
    case GL_RGB16F:
      destFormatIndex = D_FORMAT_RGB16F;
      break;
    case GL_RGB32F:
      destFormatIndex = D_FORMAT_RGB32F;
      break;
    case GL_RGBA16F:
      destFormatIndex = D_FORMAT_RGBA16F;
      break;
    case GL_RGBA32F:
      destFormatIndex = D_FORMAT_RGBA32F;
      break;
    case GL_R11F_G11F_B10F:
      destFormatIndex = D_FORMAT_R11F_G11F_B10F;
      break;
    case GL_RGB10_A2:
      destFormatIndex = D_FORMAT_RGB10_A2;
      break;
    default:
      NOTREACHED() << "Invalid destination format "
                   << gl::GLEnums::GetStringEnum(dest_format);
      break;
  }

  return alphaIndex + ditherIndex * kAlphaSize +
         targetIndex * kAlphaSize * kDitherSize +
         sourceFormatIndex * kAlphaSize * kDitherSize * NUM_SAMPLERS +
         destFormatIndex * kAlphaSize * kDitherSize * NUM_SAMPLERS *
             NUM_S_FORMAT;
}

const char* kShaderPrecisionPreamble =
    "#ifdef GL_ES\n"
    "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
    "precision highp float;\n"
    "#define TexCoordPrecision highp\n"
    "#else\n"
    "precision mediump float;\n"
    "#define TexCoordPrecision mediump\n"
    "#endif\n"
    "#else\n"
    "#define TexCoordPrecision\n"
    "#endif\n";

std::string GetVertexShaderSource(const gl::GLVersionInfo& gl_version_info,
                                  GLenum target) {
  std::string source;

  if (gl_version_info.is_es || gl_version_info.IsLowerThanGL(3, 2)) {
    if (gl_version_info.is_es3 && target != GL_TEXTURE_EXTERNAL_OES) {
      source += "#version 300 es\n";
      source +=
          "#define ATTRIBUTE in\n"
          "#define VARYING out\n";
    } else {
      source +=
          "#define ATTRIBUTE attribute\n"
          "#define VARYING varying\n";
    }
  } else {
    source += "#version 150\n";
    source +=
        "#define ATTRIBUTE in\n"
        "#define VARYING out\n";
  }

  // Preamble for texture precision.
  source += kShaderPrecisionPreamble;

  // Main shader source.
  source +=
      "uniform vec2 u_vertex_source_mult;\n"
      "uniform vec2 u_vertex_source_add;\n"
      "ATTRIBUTE vec2 a_position;\n"
      "VARYING TexCoordPrecision vec2 v_uv;\n"
      "void main(void) {\n"
      "  gl_Position = vec4(0, 0, 0, 1);\n"
      "  gl_Position.xy = a_position.xy;\n"
      "  v_uv = a_position.xy * u_vertex_source_mult + u_vertex_source_add;\n"
      "}\n";

  return source;
}

std::string GetFragmentShaderSource(const gl::GLVersionInfo& gl_version_info,
                                    bool premultiply_alpha,
                                    bool unpremultiply_alpha,
                                    bool dither,
                                    bool nv_egl_stream_consumer_external,
                                    GLenum target,
                                    GLenum source_format,
                                    GLenum dest_format) {
  std::string source;

  // Preamble for core and compatibility mode.
  if (gl_version_info.is_es || gl_version_info.IsLowerThanGL(3, 2)) {
    if (gl_version_info.is_es3 && target != GL_TEXTURE_EXTERNAL_OES) {
      source += "#version 300 es\n";
    }
    if (target == GL_TEXTURE_EXTERNAL_OES) {
      source += "#extension GL_OES_EGL_image_external : enable\n";

      if (nv_egl_stream_consumer_external) {
        source += "#extension GL_NV_EGL_stream_consumer_external : enable\n";
      }
    }
  } else {
    source += "#version 150\n";
  }

  // Preamble for texture precision.
  source += kShaderPrecisionPreamble;

  // According to the spec, |dest_format| can be unsigned integer format, float
  // format or unsigned normalized fixed-point format. |source_format| can only
  // be unsigned normalized fixed-point format.
  if (gpu::gles2::GLES2Util::IsUnsignedIntegerFormat(dest_format)) {
    source += "#define TextureType uvec4\n";
    source += "#define ZERO 0u\n";
    source += "#define MAX_COLOR 255u\n";
    source += "#define ScaleValue 255.0\n";
  } else {
    DCHECK(!gpu::gles2::GLES2Util::IsIntegerFormat(dest_format));
    source += "#define TextureType vec4\n";
    source += "#define ZERO 0.0\n";
    source += "#define MAX_COLOR 1.0\n";
    source += "#define ScaleValue 1.0\n";
  }
  if (gl_version_info.is_es2 || gl_version_info.IsLowerThanGL(3, 2) ||
      target == GL_TEXTURE_EXTERNAL_OES) {
    switch (target) {
      case GL_TEXTURE_2D:
      case GL_TEXTURE_EXTERNAL_OES:
        source += "#define TextureLookup texture2D\n";
        break;
      case GL_TEXTURE_RECTANGLE_ARB:
        source += "#define TextureLookup texture2DRect\n";
        break;
      default:
        NOTREACHED();
        break;
    }

    source +=
        "#define VARYING varying\n"
        "#define FRAGCOLOR gl_FragColor\n";
  } else {
    source +=
        "#define VARYING in\n"
        "out TextureType frag_color;\n"
        "#define FRAGCOLOR frag_color\n"
        "#define TextureLookup texture\n";
  }

  // Preamble for sampler type.
  switch (target) {
    case GL_TEXTURE_2D:
      source += "#define SamplerType sampler2D\n";
      break;
    case GL_TEXTURE_RECTANGLE_ARB:
      source += "#define SamplerType sampler2DRect\n";
      break;
    case GL_TEXTURE_EXTERNAL_OES:
      source += "#define SamplerType samplerExternalOES\n";
      break;
    default:
      NOTREACHED();
      break;
  }

  // Main shader source.
  source +=
      "uniform SamplerType u_sampler;\n"
      "uniform mat4 u_tex_coord_transform;\n"
      "VARYING TexCoordPrecision vec2 v_uv;\n"
      "void main(void) {\n"
      "  TexCoordPrecision vec4 uv =\n"
      "      u_tex_coord_transform * vec4(v_uv, 0, 1);\n"
      "  vec4 color = TextureLookup(u_sampler, uv.st);\n";

  // Premultiply or un-premultiply alpha. Must always do this, even
  // if the destination format doesn't have an alpha channel.
  if (premultiply_alpha) {
    source += "  color.rgb *= color.a;\n";
  } else if (unpremultiply_alpha) {
    source += "  if (color.a > 0.0) {\n";
    source += "    color.rgb /= color.a;\n";
    source += "  }\n";
  }

  // Dither after moving us to our desired alpha format.
  if (dither) {
    // Simulate a 4x4 dither pattern using mod/step. This code was tested for
    // performance in Skia.
    source +=
        "  float range = 1.0 / 15.0;\n"
        "  vec4 modValues = mod(gl_FragCoord.xyxy, vec4(2.0, 2.0, 4.0, 4.0));\n"
        "  vec4 stepValues = step(modValues, vec4(1.0, 1.0, 2.0, 2.0));\n"
        "  float dither_value = \n"
        "      dot(stepValues, \n"
        "          vec4(8.0 / 16.0, 4.0 / 16.0, 2.0 / 16.0, 1.0 / 16.0)) -\n"
        "      15.0 / 32.0;\n";
    // Apply the dither offset to the color. Only dither alpha if non-opaque.
    source +=
        "  if (color.a < 1.0) {\n"
        "    color += dither_value * range;\n"
        "  } else {\n"
        "    color.rgb += dither_value * range;\n"
        "  }\n";
  }

  source += "  FRAGCOLOR = TextureType(color * ScaleValue);\n";

  // Main function end.
  source += "}\n";

  return source;
}

GLenum getIntermediateFormat(GLenum format) {
  switch (format) {
    case GL_LUMINANCE_ALPHA:
    case GL_LUMINANCE:
    case GL_ALPHA:
      return GL_RGBA;
    case GL_SRGB_EXT:
      return GL_SRGB_ALPHA_EXT;
    case GL_RGB16F:
      return GL_RGBA16F;
    case GL_RGB9_E5:
    case GL_RGB32F:
      return GL_RGBA32F;
    case GL_SRGB8:
      return GL_SRGB8_ALPHA8;
    case GL_RGB8UI:
      return GL_RGBA8UI;
    default:
      return format;
  }
}

void DeleteShader(GLuint shader) {
  if (shader)
    glDeleteShader(shader);
}

bool BindFramebufferTexture2D(GLenum target,
                              GLuint texture_id,
                              GLint level,
                              GLuint framebuffer) {
  GLenum binding_target =
      gpu::gles2::GLES2Util::GLFaceTargetToTextureTarget(target);

  DCHECK(binding_target == GL_TEXTURE_2D ||
         binding_target == GL_TEXTURE_RECTANGLE_ARB ||
         binding_target == GL_TEXTURE_CUBE_MAP);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(binding_target, texture_id);
  // NVidia drivers require texture settings to be a certain way
  // or they won't report FRAMEBUFFER_COMPLETE.
  if (level > 0)
    glTexParameteri(binding_target, GL_TEXTURE_BASE_LEVEL, level);
  glTexParameterf(binding_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameterf(binding_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(binding_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(binding_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, framebuffer);
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, target,
                            texture_id, level);

#ifndef NDEBUG
  GLenum fb_status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER);
  if (GL_FRAMEBUFFER_COMPLETE != fb_status) {
    DLOG(ERROR) << "CopyTextureCHROMIUM: Incomplete framebuffer.";
    return false;
  }
#endif
  return true;
}

void DoCopyTexImage2D(
    gpu::DecoderContext* decoder,
    GLenum source_target,
    GLuint source_id,
    GLint source_level,
    GLenum source_internal_format,
    GLenum dest_target,
    GLuint dest_id,
    GLint dest_level,
    GLenum dest_internal_format,
    GLsizei width,
    GLsizei height,
    GLuint framebuffer,
    gpu::gles2::CopyTexImageResourceManager* luma_emulation_blitter) {
  DCHECK_EQ(static_cast<GLenum>(GL_TEXTURE_2D), source_target);
  GLenum dest_binding_target =
      gpu::gles2::GLES2Util::GLFaceTargetToTextureTarget(dest_target);
  DCHECK(dest_binding_target == GL_TEXTURE_2D ||
         dest_binding_target == GL_TEXTURE_CUBE_MAP);
  DCHECK(source_level == 0 || decoder->GetFeatureInfo()->IsES3Capable());
  if (BindFramebufferTexture2D(source_target, source_id, source_level,
                               framebuffer)) {
    glBindTexture(dest_binding_target, dest_id);
    glTexParameterf(dest_binding_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(dest_binding_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(dest_binding_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(dest_binding_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    // The blitter will only be non-null if we're on the desktop core
    // profile. Use it only if it's needed.
    if (luma_emulation_blitter &&
        gpu::gles2::CopyTexImageResourceManager::CopyTexImageRequiresBlit(
            decoder->GetFeatureInfo(), dest_internal_format)) {
      luma_emulation_blitter->DoCopyTexImage2DToLUMACompatibilityTexture(
          decoder, dest_id, dest_binding_target, dest_target,
          dest_internal_format,
          gpu::gles2::TextureManager::ExtractTypeFromStorageFormat(
              dest_internal_format),
          dest_level, dest_internal_format, 0 /* x */, 0 /* y */, width, height,
          framebuffer, source_internal_format);
    } else {
      glCopyTexImage2D(dest_target, dest_level, dest_internal_format, 0 /* x */,
                       0 /* y */, width, height, 0 /* border */);
    }
  }

  decoder->RestoreTextureState(source_id);
  decoder->RestoreTextureState(dest_id);
  decoder->RestoreTextureUnitBindings(0);
  decoder->RestoreActiveTexture();
  decoder->RestoreFramebufferBindings();
}

void DoCopyTexSubImage2D(
    gpu::DecoderContext* decoder,
    GLenum source_target,
    GLuint source_id,
    GLint source_level,
    GLenum source_internal_format,
    GLenum dest_target,
    GLuint dest_id,
    GLint dest_level,
    GLenum dest_internal_format,
    GLint xoffset,
    GLint yoffset,
    GLint source_x,
    GLint source_y,
    GLsizei source_width,
    GLsizei source_height,
    GLuint framebuffer,
    gpu::gles2::CopyTexImageResourceManager* luma_emulation_blitter) {
  DCHECK(source_target == GL_TEXTURE_2D ||
         source_target == GL_TEXTURE_RECTANGLE_ARB);
  GLenum dest_binding_target =
      gpu::gles2::GLES2Util::GLFaceTargetToTextureTarget(dest_target);
  DCHECK(dest_binding_target == GL_TEXTURE_2D ||
         dest_binding_target == GL_TEXTURE_CUBE_MAP);
  DCHECK(source_level == 0 || decoder->GetFeatureInfo()->IsES3Capable());
  if (BindFramebufferTexture2D(source_target, source_id, source_level,
                               framebuffer)) {
    glBindTexture(dest_binding_target, dest_id);
    glTexParameterf(dest_binding_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(dest_binding_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(dest_binding_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(dest_binding_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    // The blitter will only be non-null if we're on the desktop core
    // profile. Use it only if it's needed.
    if (luma_emulation_blitter &&
        gpu::gles2::CopyTexImageResourceManager::CopyTexImageRequiresBlit(
            decoder->GetFeatureInfo(), dest_internal_format)) {
      luma_emulation_blitter->DoCopyTexSubImageToLUMACompatibilityTexture(
          decoder, dest_id, dest_binding_target, dest_target,
          dest_internal_format,
          gpu::gles2::TextureManager::ExtractTypeFromStorageFormat(
              dest_internal_format),
          dest_level, xoffset, yoffset, 0 /* zoffset */, source_x, source_y,
          source_width, source_height, framebuffer, source_internal_format);
    } else {
      glCopyTexSubImage2D(dest_target, dest_level, xoffset, yoffset, source_x,
                          source_y, source_width, source_height);
    }
  }

  decoder->RestoreTextureState(source_id);
  decoder->RestoreTextureState(dest_id);
  decoder->RestoreTextureUnitBindings(0);
  decoder->RestoreActiveTexture();
  decoder->RestoreFramebufferBindings();
}

// Convert RGBA/UNSIGNED_BYTE source to RGB/UNSIGNED_BYTE destination.
void convertToRGB(const uint8_t* source,
                  uint8_t* destination,
                  unsigned length) {
  for (unsigned i = 0; i < length; ++i) {
    destination[0] = source[0];
    destination[1] = source[1];
    destination[2] = source[2];
    source += 4;
    destination += 3;
  }
}

// Convert RGBA/UNSIGNED_BYTE source to RGB/FLOAT destination.
void convertToRGBFloat(const uint8_t* source,
                       float* destination,
                       unsigned length) {
  const float scaleFactor = 1.0f / 255.0f;
  for (unsigned i = 0; i < length; ++i) {
    destination[0] = source[0] * scaleFactor;
    destination[1] = source[1] * scaleFactor;
    destination[2] = source[2] * scaleFactor;
    source += 4;
    destination += 3;
  }
}

// Prepare the image data to be uploaded to a texture in pixel unpack buffer.
void prepareUnpackBuffer(GLuint buffer[2],
                         bool is_es,
                         GLenum format,
                         GLenum type,
                         GLsizei width,
                         GLsizei height) {
  uint32_t pixel_num = width * height;

  // Result of glReadPixels with format == GL_RGB and type == GL_UNSIGNED_BYTE
  // from read framebuffer in RGBA fromat is not correct on desktop core
  // profile on both Linux Mesa and Linux NVIDIA. This may be a driver bug.
  bool is_rgb_unsigned_byte = format == GL_RGB && type == GL_UNSIGNED_BYTE;
  if ((!is_es && !is_rgb_unsigned_byte) ||
      (format == GL_RGBA && type == GL_UNSIGNED_BYTE)) {
    uint32_t bytes_per_group =
        gpu::gles2::GLES2Util::ComputeImageGroupSize(format, type);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, buffer[0]);
    glBufferData(GL_PIXEL_PACK_BUFFER, pixel_num * bytes_per_group, 0,
                 GL_STATIC_READ);
    glReadPixels(0, 0, width, height, format, type, 0);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buffer[0]);
    return;
  }

  uint32_t bytes_per_group =
      gpu::gles2::GLES2Util::ComputeImageGroupSize(GL_RGBA, GL_UNSIGNED_BYTE);
  uint32_t buf_size = pixel_num * bytes_per_group;

  if (format == GL_RGB && type == GL_FLOAT) {
#if defined(OS_ANDROID)
    // Reading pixels to pbo with glReadPixels will cause random failures of
    // GLCopyTextureCHROMIUMES3Test.FormatCombinations in gl_tests. This is seen
    // on Nexus 5 but not Nexus 4. Read pixels to client memory, then upload to
    // pixel unpack buffer with glBufferData.
    std::unique_ptr<uint8_t[]> pixels(new uint8_t[width * height * 4]);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.get());
    std::unique_ptr<float[]> data(new float[width * height * 3]);
    convertToRGBFloat(pixels.get(), data.get(), pixel_num);
    bytes_per_group =
        gpu::gles2::GLES2Util::ComputeImageGroupSize(format, type);
    buf_size = pixel_num * bytes_per_group;
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buffer[1]);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, buf_size, data.get(), GL_STATIC_DRAW);
#else
    glBindBuffer(GL_PIXEL_PACK_BUFFER, buffer[0]);
    glBufferData(GL_PIXEL_PACK_BUFFER, buf_size, 0, GL_STATIC_READ);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    void* pixels =
        glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, buf_size, GL_MAP_READ_BIT);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buffer[1]);
    bytes_per_group =
        gpu::gles2::GLES2Util::ComputeImageGroupSize(format, type);
    buf_size = pixel_num * bytes_per_group;
    glBufferData(GL_PIXEL_UNPACK_BUFFER, buf_size, 0, GL_STATIC_DRAW);
    void* data =
        glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, buf_size, GL_MAP_WRITE_BIT);
    convertToRGBFloat(static_cast<uint8_t*>(pixels), static_cast<float*>(data),
                      pixel_num);
    glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
#endif
    return;
  }

  if (format == GL_RGB && type == GL_UNSIGNED_BYTE) {
    glBindBuffer(GL_PIXEL_PACK_BUFFER, buffer[0]);
    glBufferData(GL_PIXEL_PACK_BUFFER, buf_size, 0, GL_DYNAMIC_DRAW);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    void* pixels = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, buf_size,
                                    GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);
    void* data = pixels;
    convertToRGB((uint8_t*)pixels, (uint8_t*)data, pixel_num);
    glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buffer[0]);
    return;
  }

  NOTREACHED();
}

enum TexImageCommandType {
  kTexImage,
  kTexSubImage,
};

void DoReadbackAndTexImage(TexImageCommandType command_type,
                           gpu::DecoderContext* decoder,
                           GLenum source_target,
                           GLuint source_id,
                           GLint source_level,
                           GLenum dest_target,
                           GLuint dest_id,
                           GLint dest_level,
                           GLenum dest_internal_format,
                           GLint xoffset,
                           GLint yoffset,
                           GLsizei width,
                           GLsizei height,
                           GLuint framebuffer) {
  DCHECK_EQ(static_cast<GLenum>(GL_TEXTURE_2D), source_target);
  GLenum dest_binding_target =
      gpu::gles2::GLES2Util::GLFaceTargetToTextureTarget(dest_target);
  DCHECK(dest_binding_target == GL_TEXTURE_2D ||
         dest_binding_target == GL_TEXTURE_CUBE_MAP);
  DCHECK(source_level == 0 || decoder->GetFeatureInfo()->IsES3Capable());
  if (BindFramebufferTexture2D(source_target, source_id, source_level,
                               framebuffer)) {
    glBindTexture(dest_binding_target, dest_id);
    glTexParameterf(dest_binding_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(dest_binding_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(dest_binding_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(dest_binding_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    GLenum format = GL_RGBA;
    GLenum type = GL_UNSIGNED_BYTE;
    switch (dest_internal_format) {
      case GL_RGB9_E5:
        format = GL_RGB;
        type = GL_FLOAT;
        break;
      case GL_SRGB_EXT:
      case GL_SRGB8:
        format = GL_RGB;
        break;
      case GL_RGB5_A1:
      case GL_SRGB_ALPHA_EXT:
      case GL_SRGB8_ALPHA8:
        break;
      default:
        NOTREACHED();
        break;
    }

    // TODO(qiankun.miao@intel.com): PIXEL_PACK_BUFFER and PIXEL_UNPACK_BUFFER
    // are not supported in ES2.
    bool is_es = decoder->GetFeatureInfo()->gl_version_info().is_es;
    DCHECK(!decoder->GetFeatureInfo()->gl_version_info().is_es2);

    uint32_t buffer_num = is_es && format == GL_RGB && type == GL_FLOAT ? 2 : 1;
    GLuint buffer[2] = {0u};
    glGenBuffersARB(buffer_num, buffer);
    prepareUnpackBuffer(buffer, is_es, format, type, width, height);

    if (command_type == kTexImage) {
      glTexImage2D(dest_target, dest_level, dest_internal_format, width, height,
                   0, format, type, 0);
    } else {
      glTexSubImage2D(dest_target, dest_level, xoffset, yoffset, width, height,
                      format, type, 0);
    }
    glDeleteBuffersARB(buffer_num, buffer);
  }

  decoder->RestoreTextureState(source_id);
  decoder->RestoreTextureState(dest_id);
  decoder->RestoreTextureUnitBindings(0);
  decoder->RestoreActiveTexture();
  decoder->RestoreFramebufferBindings();
  decoder->RestoreBufferBindings();
}

class CopyTextureResourceManagerImpl
    : public CopyTextureCHROMIUMResourceManager {
 public:
  CopyTextureResourceManagerImpl();
  ~CopyTextureResourceManagerImpl() override;

  // CopyTextureCHROMIUMResourceManager implementation.
  void Initialize(
      const DecoderContext* decoder,
      const gles2::FeatureInfo::FeatureFlags& feature_flags) override;
  void Destroy() override;
  void DoCopyTexture(
      DecoderContext* decoder,
      GLenum source_target,
      GLuint source_id,
      GLint source_level,
      GLenum source_internal_format,
      GLenum dest_target,
      GLuint dest_id,
      GLint dest_level,
      GLenum dest_internal_format,
      GLsizei width,
      GLsizei height,
      bool flip_y,
      bool premultiply_alpha,
      bool unpremultiply_alpha,
      bool dither,
      CopyTextureMethod method,
      CopyTexImageResourceManager* luma_emulation_blitter) override;
  void DoCopySubTexture(
      DecoderContext* decoder,
      GLenum source_target,
      GLuint source_id,
      GLint source_level,
      GLenum source_internal_format,
      GLenum dest_target,
      GLuint dest_id,
      GLint dest_level,
      GLenum dest_internal_format,
      GLint xoffset,
      GLint yoffset,
      GLint x,
      GLint y,
      GLsizei width,
      GLsizei height,
      GLsizei dest_width,
      GLsizei dest_height,
      GLsizei source_width,
      GLsizei source_height,
      bool flip_y,
      bool premultiply_alpha,
      bool unpremultiply_alpha,
      bool dither,
      CopyTextureMethod method,
      CopyTexImageResourceManager* luma_emulation_blitter) override;
  void DoCopySubTextureWithTransform(
      DecoderContext* decoder,
      GLenum source_target,
      GLuint source_id,
      GLint source_level,
      GLenum source_internal_format,
      GLenum dest_target,
      GLuint dest_id,
      GLint dest_level,
      GLenum dest_internal_format,
      GLint xoffset,
      GLint yoffset,
      GLint x,
      GLint y,
      GLsizei width,
      GLsizei height,
      GLsizei dest_width,
      GLsizei dest_height,
      GLsizei source_width,
      GLsizei source_height,
      bool flip_y,
      bool premultiply_alpha,
      bool unpremultiply_alpha,
      bool dither,
      const GLfloat transform_matrix[16],
      CopyTexImageResourceManager* luma_emulation_blitter) override;
  void DoCopyTextureWithTransform(
      DecoderContext* decoder,
      GLenum source_target,
      GLuint source_id,
      GLint source_level,
      GLenum source_format,
      GLenum dest_target,
      GLuint dest_id,
      GLint dest_level,
      GLenum dest_format,
      GLsizei width,
      GLsizei height,
      bool flip_y,
      bool premultiply_alpha,
      bool unpremultiply_alpha,
      bool dither,
      const GLfloat transform_matrix[16],
      CopyTextureMethod method,
      CopyTexImageResourceManager* luma_emulation_blitter) override;

 private:
  struct ProgramInfo {
    ProgramInfo()
        : program(0u),
          vertex_source_mult_handle(0u),
          vertex_source_add_handle(0u),
          tex_coord_transform_handle(0u),
          sampler_handle(0u) {}

    GLuint program;

    // Transformations that map from the original quad coordinates [-1, 1] into
    // the source texture's texture coordinates.
    GLuint vertex_source_mult_handle;
    GLuint vertex_source_add_handle;

    GLuint tex_coord_transform_handle;
    GLuint sampler_handle;
  };

  void DoCopyTextureInternal(
      DecoderContext* decoder,
      GLenum source_target,
      GLuint source_id,
      GLint source_level,
      GLenum source_format,
      GLenum dest_target,
      GLuint dest_id,
      GLint dest_level,
      GLenum dest_format,
      GLint xoffset,
      GLint yoffset,
      GLint x,
      GLint y,
      GLsizei width,
      GLsizei height,
      GLsizei dest_width,
      GLsizei dest_height,
      GLsizei source_width,
      GLsizei source_height,
      bool flip_y,
      bool premultiply_alpha,
      bool unpremultiply_alpha,
      bool dither,
      const GLfloat transform_matrix[16],
      CopyTexImageResourceManager* luma_emulation_blitter);

  bool initialized_;
  bool nv_egl_stream_consumer_external_;
  typedef std::vector<GLuint> ShaderVector;
  ShaderVector vertex_shaders_;
  ShaderVector fragment_shaders_;
  typedef int ProgramMapKey;
  typedef std::unordered_map<ProgramMapKey, ProgramInfo> ProgramMap;
  ProgramMap programs_;
  GLuint vertex_array_object_id_;
  GLuint buffer_id_;
  GLuint framebuffer_;
};

CopyTextureResourceManagerImpl::CopyTextureResourceManagerImpl()
    : initialized_(false),
      nv_egl_stream_consumer_external_(false),
      vertex_shaders_(kNumVertexShaders, 0u),
      fragment_shaders_(kNumFragmentShaders, 0u),
      vertex_array_object_id_(0u),
      buffer_id_(0u),
      framebuffer_(0u) {}

CopyTextureResourceManagerImpl::~CopyTextureResourceManagerImpl() {
  // |buffer_id_| and |framebuffer_| can be not-null because when GPU context is
  // lost, this class can be deleted without releasing resources like
  // GLES2DecoderImpl.
}

void CopyTextureResourceManagerImpl::Initialize(
    const DecoderContext* decoder,
    const gles2::FeatureInfo::FeatureFlags& feature_flags) {
  static_assert(
      kVertexPositionAttrib == 0u,
      "kVertexPositionAttrib must be 0");
  DCHECK(!buffer_id_);
  DCHECK(!vertex_array_object_id_);
  DCHECK(!framebuffer_);
  DCHECK(programs_.empty());

  nv_egl_stream_consumer_external_ =
      feature_flags.nv_egl_stream_consumer_external;

  if (feature_flags.native_vertex_array_object) {
    glGenVertexArraysOES(1, &vertex_array_object_id_);
    glBindVertexArrayOES(vertex_array_object_id_);
  }

  // Initialize all of the GPU resources required to perform the copy.
  glGenBuffersARB(1, &buffer_id_);
  glBindBuffer(GL_ARRAY_BUFFER, buffer_id_);
  const GLfloat kQuadVertices[] = {-1.0f, -1.0f,
                                    1.0f, -1.0f,
                                    1.0f,  1.0f,
                                   -1.0f,  1.0f};
  glBufferData(
      GL_ARRAY_BUFFER, sizeof(kQuadVertices), kQuadVertices, GL_STATIC_DRAW);

  glGenFramebuffersEXT(1, &framebuffer_);

  if (vertex_array_object_id_) {
    glEnableVertexAttribArray(kVertexPositionAttrib);
    glVertexAttribPointer(kVertexPositionAttrib, 2, GL_FLOAT, GL_FALSE, 0, 0);
    decoder->RestoreAllAttributes();
  }

  decoder->RestoreBufferBindings();

  initialized_ = true;
}

void CopyTextureResourceManagerImpl::Destroy() {
  if (!initialized_)
    return;

  if (vertex_array_object_id_) {
    glDeleteVertexArraysOES(1, &vertex_array_object_id_);
    vertex_array_object_id_ = 0;
  }

  glDeleteFramebuffersEXT(1, &framebuffer_);
  framebuffer_ = 0;

  std::for_each(
      vertex_shaders_.begin(), vertex_shaders_.end(), DeleteShader);
  std::for_each(
      fragment_shaders_.begin(), fragment_shaders_.end(), DeleteShader);

  for (ProgramMap::const_iterator it = programs_.begin(); it != programs_.end();
       ++it) {
    const ProgramInfo& info = it->second;
    glDeleteProgram(info.program);
  }

  glDeleteBuffersARB(1, &buffer_id_);
  buffer_id_ = 0;
}

void CopyTextureResourceManagerImpl::DoCopyTexture(
    DecoderContext* decoder,
    GLenum source_target,
    GLuint source_id,
    GLint source_level,
    GLenum source_internal_format,
    GLenum dest_target,
    GLuint dest_id,
    GLint dest_level,
    GLenum dest_internal_format,
    GLsizei width,
    GLsizei height,
    bool flip_y,
    bool premultiply_alpha,
    bool unpremultiply_alpha,
    bool dither,
    CopyTextureMethod method,
    gpu::gles2::CopyTexImageResourceManager* luma_emulation_blitter) {
  // Use kIdentityMatrix if no transform passed in.
  DoCopyTextureWithTransform(
      decoder, source_target, source_id, source_level, source_internal_format,
      dest_target, dest_id, dest_level, dest_internal_format, width, height,
      flip_y, premultiply_alpha, unpremultiply_alpha, dither, kIdentityMatrix,
      method, luma_emulation_blitter);
}

void CopyTextureResourceManagerImpl::DoCopySubTexture(
    DecoderContext* decoder,
    GLenum source_target,
    GLuint source_id,
    GLint source_level,
    GLenum source_internal_format,
    GLenum dest_target,
    GLuint dest_id,
    GLint dest_level,
    GLenum dest_internal_format,
    GLint xoffset,
    GLint yoffset,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLsizei dest_width,
    GLsizei dest_height,
    GLsizei source_width,
    GLsizei source_height,
    bool flip_y,
    bool premultiply_alpha,
    bool unpremultiply_alpha,
    bool dither,
    CopyTextureMethod method,
    gpu::gles2::CopyTexImageResourceManager* luma_emulation_blitter) {
  if (method == CopyTextureMethod::DIRECT_COPY) {
    DoCopyTexSubImage2D(decoder, source_target, source_id, source_level,
                        source_internal_format, dest_target, dest_id,
                        dest_level, dest_internal_format, xoffset, yoffset, x,
                        y, width, height, framebuffer_, luma_emulation_blitter);
    return;
  }

  // Draw to level 0 of an intermediate GL_TEXTURE_2D texture.
  GLint dest_xoffset = xoffset;
  GLint dest_yoffset = yoffset;
  GLuint dest_texture = dest_id;
  GLint original_dest_level = dest_level;
  GLenum original_dest_target = dest_target;
  GLuint intermediate_texture = 0;
  GLenum original_internal_format = dest_internal_format;
  if (method == CopyTextureMethod::DRAW_AND_COPY ||
      method == CopyTextureMethod::DRAW_AND_READBACK) {
    GLenum adjusted_internal_format =
        method == CopyTextureMethod::DRAW_AND_READBACK
            ? GL_RGBA
            : getIntermediateFormat(dest_internal_format);
    dest_target = GL_TEXTURE_2D;
    glGenTextures(1, &intermediate_texture);
    glBindTexture(dest_target, intermediate_texture);
    GLenum format = TextureManager::ExtractFormatFromStorageFormat(
        adjusted_internal_format);
    GLenum type =
        TextureManager::ExtractTypeFromStorageFormat(adjusted_internal_format);

    glTexImage2D(dest_target, 0, adjusted_internal_format, width, height, 0,
                 format, type, nullptr);
    dest_texture = intermediate_texture;
    dest_level = 0;
    dest_internal_format = adjusted_internal_format;
    dest_xoffset = 0;
    dest_yoffset = 0;
    dest_width = width;
    dest_height = height;
  }

  DoCopySubTextureWithTransform(
      decoder, source_target, source_id, source_level, source_internal_format,
      dest_target, dest_texture, dest_level, dest_internal_format, dest_xoffset,
      dest_yoffset, x, y, width, height, dest_width, dest_height, source_width,
      source_height, flip_y, premultiply_alpha, unpremultiply_alpha, dither,
      kIdentityMatrix, luma_emulation_blitter);

  if (method == CopyTextureMethod::DRAW_AND_COPY ||
      method == CopyTextureMethod::DRAW_AND_READBACK) {
    source_level = 0;
    if (method == CopyTextureMethod::DRAW_AND_COPY) {
      DoCopyTexSubImage2D(decoder, dest_target, intermediate_texture,
                          source_level, dest_internal_format,
                          original_dest_target, dest_id, original_dest_level,
                          original_internal_format, xoffset, yoffset, 0, 0,
                          width, height, framebuffer_, luma_emulation_blitter);
    } else if (method == CopyTextureMethod::DRAW_AND_READBACK) {
      DoReadbackAndTexImage(kTexSubImage, decoder, dest_target,
                            intermediate_texture, source_level,
                            original_dest_target, dest_id, original_dest_level,
                            original_internal_format, xoffset, yoffset, width,
                            height, framebuffer_);
    }
    glDeleteTextures(1, &intermediate_texture);
  }
}

void CopyTextureResourceManagerImpl::DoCopySubTextureWithTransform(
    DecoderContext* decoder,
    GLenum source_target,
    GLuint source_id,
    GLint source_level,
    GLenum source_internal_format,
    GLenum dest_target,
    GLuint dest_id,
    GLint dest_level,
    GLenum dest_internal_format,
    GLint xoffset,
    GLint yoffset,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLsizei dest_width,
    GLsizei dest_height,
    GLsizei source_width,
    GLsizei source_height,
    bool flip_y,
    bool premultiply_alpha,
    bool unpremultiply_alpha,
    bool dither,
    const GLfloat transform_matrix[16],
    gpu::gles2::CopyTexImageResourceManager* luma_emulation_blitter) {
  DoCopyTextureInternal(
      decoder, source_target, source_id, source_level, source_internal_format,
      dest_target, dest_id, dest_level, dest_internal_format, xoffset, yoffset,
      x, y, width, height, dest_width, dest_height, source_width, source_height,
      flip_y, premultiply_alpha, unpremultiply_alpha, dither, transform_matrix,
      luma_emulation_blitter);
}

void CopyTextureResourceManagerImpl::DoCopyTextureWithTransform(
    DecoderContext* decoder,
    GLenum source_target,
    GLuint source_id,
    GLint source_level,
    GLenum source_internal_format,
    GLenum dest_target,
    GLuint dest_id,
    GLint dest_level,
    GLenum dest_internal_format,
    GLsizei width,
    GLsizei height,
    bool flip_y,
    bool premultiply_alpha,
    bool unpremultiply_alpha,
    bool dither,
    const GLfloat transform_matrix[16],
    CopyTextureMethod method,
    gpu::gles2::CopyTexImageResourceManager* luma_emulation_blitter) {
  GLsizei dest_width = width;
  GLsizei dest_height = height;
  if (method == CopyTextureMethod::DIRECT_COPY) {
    DoCopyTexImage2D(decoder, source_target, source_id, source_level,
                     source_internal_format, dest_target, dest_id, dest_level,
                     dest_internal_format, width, height, framebuffer_,
                     luma_emulation_blitter);
    return;
  }

  // Draw to level 0 of an intermediate GL_TEXTURE_2D texture.
  GLuint dest_texture = dest_id;
  GLint original_dest_level = dest_level;
  GLenum original_dest_target = dest_target;
  GLuint intermediate_texture = 0;
  GLenum original_internal_format = dest_internal_format;
  if (method == CopyTextureMethod::DRAW_AND_COPY ||
      method == CopyTextureMethod::DRAW_AND_READBACK) {
    GLenum adjusted_internal_format =
        method == CopyTextureMethod::DRAW_AND_READBACK
            ? GL_RGBA
            : getIntermediateFormat(dest_internal_format);
    dest_target = GL_TEXTURE_2D;
    glGenTextures(1, &intermediate_texture);
    glBindTexture(dest_target, intermediate_texture);
    GLenum format = TextureManager::ExtractFormatFromStorageFormat(
        adjusted_internal_format);
    GLenum type =
        TextureManager::ExtractTypeFromStorageFormat(adjusted_internal_format);
    glTexImage2D(dest_target, 0, adjusted_internal_format, width, height, 0,
                 format, type, nullptr);
    dest_texture = intermediate_texture;
    dest_level = 0;
    dest_internal_format = adjusted_internal_format;
  }

  DoCopyTextureInternal(decoder, source_target, source_id, source_level,
                        source_internal_format, dest_target, dest_texture,
                        dest_level, dest_internal_format, 0, 0, 0, 0, width,
                        height, dest_width, dest_height, width, height, flip_y,
                        premultiply_alpha, unpremultiply_alpha, dither,
                        transform_matrix, luma_emulation_blitter);

  if (method == CopyTextureMethod::DRAW_AND_COPY ||
      method == CopyTextureMethod::DRAW_AND_READBACK) {
    source_level = 0;
    if (method == CopyTextureMethod::DRAW_AND_COPY) {
      DoCopyTexImage2D(decoder, dest_target, intermediate_texture, source_level,
                       dest_internal_format, original_dest_target, dest_id,
                       original_dest_level, original_internal_format, width,
                       height, framebuffer_, luma_emulation_blitter);
    } else if (method == CopyTextureMethod::DRAW_AND_READBACK) {
      DoReadbackAndTexImage(
          kTexSubImage, decoder, dest_target, intermediate_texture,
          source_level, original_dest_target, dest_id, original_dest_level,
          original_internal_format, 0, 0, width, height, framebuffer_);
    }
    glDeleteTextures(1, &intermediate_texture);
  }
}

void CopyTextureResourceManagerImpl::DoCopyTextureInternal(
    DecoderContext* decoder,
    GLenum source_target,
    GLuint source_id,
    GLint source_level,
    GLenum source_format,
    GLenum dest_target,
    GLuint dest_id,
    GLint dest_level,
    GLenum dest_format,
    GLint xoffset,
    GLint yoffset,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLsizei dest_width,
    GLsizei dest_height,
    GLsizei source_width,
    GLsizei source_height,
    bool flip_y,
    bool premultiply_alpha,
    bool unpremultiply_alpha,
    bool dither,
    const GLfloat transform_matrix[16],
    gpu::gles2::CopyTexImageResourceManager* luma_emulation_blitter) {
  DCHECK(source_target == GL_TEXTURE_2D ||
         source_target == GL_TEXTURE_RECTANGLE_ARB ||
         source_target == GL_TEXTURE_EXTERNAL_OES);
  DCHECK(dest_target == GL_TEXTURE_2D ||
         dest_target == GL_TEXTURE_RECTANGLE_ARB ||
         (dest_target >= GL_TEXTURE_CUBE_MAP_POSITIVE_X &&
          dest_target <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z));
  DCHECK_GE(source_level, 0);
  DCHECK_GE(dest_level, 0);
  DCHECK_GE(xoffset, 0);
  DCHECK_LE(xoffset + width, dest_width);
  DCHECK_GE(yoffset, 0);
  DCHECK_LE(yoffset + height, dest_height);
  if (dest_width == 0 || dest_height == 0 || source_width == 0 ||
      source_height == 0) {
    return;
  }

  if (!initialized_) {
    DLOG(ERROR) << "CopyTextureCHROMIUM: Uninitialized manager.";
    return;
  }
  const gl::GLVersionInfo& gl_version_info =
      decoder->GetFeatureInfo()->gl_version_info();

  if (vertex_array_object_id_) {
    glBindVertexArrayOES(vertex_array_object_id_);
  } else {
    if (!gl_version_info.is_desktop_core_profile) {
      decoder->ClearAllAttributes();
    }
    glEnableVertexAttribArray(kVertexPositionAttrib);
    glBindBuffer(GL_ARRAY_BUFFER, buffer_id_);
    glVertexAttribPointer(kVertexPositionAttrib, 2, GL_FLOAT, GL_FALSE, 0, 0);
  }

  ShaderId vertex_shader_id = GetVertexShaderId(source_target);
  DCHECK_LT(static_cast<size_t>(vertex_shader_id), vertex_shaders_.size());
  ShaderId fragment_shader_id =
      GetFragmentShaderId(premultiply_alpha, unpremultiply_alpha, dither,
                          source_target, source_format, dest_format);
  DCHECK_LT(static_cast<size_t>(fragment_shader_id), fragment_shaders_.size());

  ProgramMapKey key(fragment_shader_id);
  ProgramInfo* info = &programs_[key];
  // Create program if necessary.
  if (!info->program) {
    info->program = glCreateProgram();
    GLuint* vertex_shader = &vertex_shaders_[vertex_shader_id];
    if (!*vertex_shader) {
      *vertex_shader = glCreateShader(GL_VERTEX_SHADER);
      std::string source =
          GetVertexShaderSource(gl_version_info, source_target);
      CompileShaderWithLog(*vertex_shader, source.c_str());
    }
    glAttachShader(info->program, *vertex_shader);
    GLuint* fragment_shader = &fragment_shaders_[fragment_shader_id];
    if (!*fragment_shader) {
      *fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
      std::string source = GetFragmentShaderSource(
          gl_version_info, premultiply_alpha, unpremultiply_alpha, dither,
          nv_egl_stream_consumer_external_, source_target, source_format,
          dest_format);
      CompileShaderWithLog(*fragment_shader, source.c_str());
    }
    glAttachShader(info->program, *fragment_shader);
    glBindAttribLocation(info->program, kVertexPositionAttrib, "a_position");
    glLinkProgram(info->program);

#if DCHECK_IS_ON()
    {
      GLint linked;
      glGetProgramiv(info->program, GL_LINK_STATUS, &linked);
      if (!linked) {
        char buffer[1024];
        GLsizei length = 0;
        glGetProgramInfoLog(info->program, sizeof(buffer), &length, buffer);
        std::string log(buffer, length);
        DLOG(ERROR) << "CopyTextureCHROMIUM: program link failure: " << log;
      }
    }
#endif
    info->vertex_source_mult_handle =
        glGetUniformLocation(info->program, "u_vertex_source_mult");
    info->vertex_source_add_handle =
        glGetUniformLocation(info->program, "u_vertex_source_add");

    info->tex_coord_transform_handle =
        glGetUniformLocation(info->program, "u_tex_coord_transform");
    info->sampler_handle = glGetUniformLocation(info->program, "u_sampler");
  }
  glUseProgram(info->program);

  glUniformMatrix4fv(info->tex_coord_transform_handle, 1, GL_FALSE,
                     transform_matrix);

  // Note: For simplicity, the calculations in this comment block use a single
  // dimension. All calculations trivially extend to the x-y plane.
  // The target subrange in the source texture has coordinates [x, x + width].
  // The full source texture has range [0, source_width]. We need to transform
  // the subrange into texture space ([0, M]), assuming that [0, source_width]
  // gets mapped to [0, M]. If source_target == GL_TEXTURE_RECTANGLE_ARB, M =
  // source_width. Otherwise, M = 1.
  //
  // We want to find A and B such that:
  //   A * X + B = Y
  //   C * Y + D = Z
  //
  // where X = [-1, 1], Z = [x, x + width]
  // and C, D satisfy the relationship C * [0, M] + D = [0, source_width].
  //
  // Math shows:
  //   D = 0
  //   C = source_width / M
  //   Y = [x * M / source_width, (x + width) * M / source_width]
  //   B = (x + w/2) * M / source_width
  //   A = (w/2) * M / source_width
  //
  // When flip_y is true, we run the same calcluation, but with Z = [x + width,
  // x]. (I'm intentionally keeping the x-plane notation, although the
  // calculation only gets applied to the y-plane).
  //
  // Math shows:
  //   D = 0
  //   C = source_width / M
  //   Y = [(x + width) * M / source_width, x * M / source_width]
  //   B = (x + w/2) * M / source_width
  //   A = (-w/2) * M / source_width
  //
  // So everything is the same but the sign of A is flipped.
  GLfloat m_x = source_target == GL_TEXTURE_RECTANGLE_ARB ? source_width : 1;
  GLfloat m_y = source_target == GL_TEXTURE_RECTANGLE_ARB ? source_height : 1;
  GLfloat sign_a = flip_y ? -1 : 1;
  glUniform2f(info->vertex_source_mult_handle, width / 2.f * m_x / source_width,
              height / 2.f * m_y / source_height * sign_a);
  glUniform2f(info->vertex_source_add_handle,
              (x + width / 2.f) * m_x / source_width,
              (y + height / 2.f) * m_y / source_height);

  DCHECK(dest_level == 0 || decoder->GetFeatureInfo()->IsES3Capable());
  if (BindFramebufferTexture2D(dest_target, dest_id, dest_level,
                               framebuffer_)) {
#ifndef NDEBUG
    // glValidateProgram of MACOSX validates FBO unlike other platforms, so
    // glValidateProgram must be called after FBO binding. crbug.com/463439
    glValidateProgram(info->program);
    GLint validation_status;
    glGetProgramiv(info->program, GL_VALIDATE_STATUS, &validation_status);
    if (GL_TRUE != validation_status) {
      DLOG(ERROR) << "CopyTextureCHROMIUM: Invalid shader.";
      return;
    }
#endif

    if (decoder->GetFeatureInfo()->IsWebGL2OrES3OrHigherContext())
      glBindSampler(0, 0);
    glUniform1i(info->sampler_handle, 0);

    glBindTexture(source_target, source_id);
    DCHECK(source_level == 0 || decoder->GetFeatureInfo()->IsES3Capable());
    if (source_level > 0)
      glTexParameteri(source_target, GL_TEXTURE_BASE_LEVEL, source_level);
    glTexParameterf(source_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(source_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(source_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(source_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_CULL_FACE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);

    bool need_scissor =
        xoffset || yoffset || width != dest_width || height != dest_height;
    if (need_scissor) {
      glEnable(GL_SCISSOR_TEST);
      glScissor(xoffset, yoffset, width, height);
    } else {
      glDisable(GL_SCISSOR_TEST);
    }
    if (decoder->GetFeatureInfo()->feature_flags().ext_window_rectangles) {
      glWindowRectanglesEXT(GL_EXCLUSIVE_EXT, 0, nullptr);
    }
    glViewport(xoffset, yoffset, width, height);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
  }

  if (decoder->GetFeatureInfo()->IsWebGL2OrES3OrHigherContext())
    decoder->GetContextState()->RestoreSamplerBinding(0, nullptr);
  decoder->RestoreAllAttributes();
  decoder->RestoreTextureState(source_id);
  decoder->RestoreTextureState(dest_id);
  decoder->RestoreTextureUnitBindings(0);
  decoder->RestoreActiveTexture();
  decoder->RestoreProgramBindings();
  decoder->RestoreBufferBindings();
  decoder->RestoreFramebufferBindings();
  decoder->RestoreGlobalState();
}

}  // namespace

CopyTextureCHROMIUMResourceManager::CopyTextureCHROMIUMResourceManager() =
    default;
CopyTextureCHROMIUMResourceManager::~CopyTextureCHROMIUMResourceManager() =
    default;

// static
CopyTextureCHROMIUMResourceManager*
CopyTextureCHROMIUMResourceManager::Create() {
  return new CopyTextureResourceManagerImpl();
}

}  // namespace gles2
}  // namespace gpu
