// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_cmd_srgb_converter.h"

#include "gpu/command_buffer/service/shader_manager.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gl/gl_version_info.h"

namespace gpu {
namespace gles2 {

SRGBConverter::SRGBConverter(
    const gles2::FeatureInfo* feature_info)
    : feature_info_(feature_info) {
}

SRGBConverter::~SRGBConverter() = default;

void SRGBConverter::InitializeSRGBConverterProgram() {
  if (srgb_converter_program_) {
    return;
  }

  srgb_converter_program_ = glCreateProgram();

  const char* kShaderPreamble =
      "#ifdef GL_ES\n"
      "precision mediump float;\n"
      "#define TexCoordPrecision mediump\n"
      "#else\n"
      "#define TexCoordPrecision\n"
      "#endif\n";

  std::string vs_source;
  if (feature_info_->gl_version_info().is_es) {
    if (feature_info_->gl_version_info().is_es3) {
      vs_source += "#version 300 es\n";
      vs_source +=
          "#define ATTRIBUTE in\n"
          "#define VARYING out\n";
    } else {
      vs_source +=
          "#define ATTRIBUTE attribute\n"
          "#define VARYING varying\n";
    }
  } else {
    vs_source += "#version 150\n";
    vs_source +=
        "#define ATTRIBUTE in\n"
        "#define VARYING out\n";
  }

  vs_source += kShaderPreamble;

  // TODO(yizhou): gles 2.0 does not support gl_VertexID.
  // Compile the vertex shader
  vs_source +=
      "VARYING TexCoordPrecision vec2 v_texcoord;\n"
      "\n"
      "void main()\n"
      "{\n"
      "    const vec2 quad_positions[6] = vec2[6]\n"
      "    (\n"
      "        vec2(0.0f, 0.0f),\n"
      "        vec2(0.0f, 1.0f),\n"
      "        vec2(1.0f, 0.0f),\n"
      "\n"
      "        vec2(0.0f, 1.0f),\n"
      "        vec2(1.0f, 0.0f),\n"
      "        vec2(1.0f, 1.0f)\n"
      "    );\n"
      "\n"
      "    vec2 xy = vec2((quad_positions[gl_VertexID] * 2.0) - 1.0);\n"
      "    gl_Position = vec4(xy, 0.0, 1.0);\n"
      "    v_texcoord = quad_positions[gl_VertexID];\n"
      "}\n";
  GLuint vs = glCreateShader(GL_VERTEX_SHADER);
  CompileShaderWithLog(vs, vs_source.c_str());
  glAttachShader(srgb_converter_program_, vs);
  glDeleteShader(vs);

  // Compile the fragment shader

  // Sampling texels from a srgb texture to a linear image, it will convert
  // the srgb color space to linear color space automatically as a part of
  // filtering. See the section <sRGB Texture Color Conversion> in GLES and
  // OpenGL spec. So during decoding, we don't need to use the equation to
  // explicitly decode srgb to linear in fragment shader.
  // Drawing to a srgb image, it will convert linear to srgb automatically.
  // See the section <sRGB Conversion> in GLES and OpenGL spec. So during
  // encoding, we don't need to use the equation to explicitly encode linear
  // to srgb in fragment shader.
  // As a result, we just use a simple fragment shader to do srgb conversion.
  std::string fs_source;
  if (feature_info_->gl_version_info().is_es) {
    if (feature_info_->gl_version_info().is_es3) {
      fs_source += "#version 300 es\n";
    }
  } else {
    fs_source += "#version 150\n";
  }

  fs_source += kShaderPreamble;

  if (feature_info_->gl_version_info().is_es) {
    if (feature_info_->gl_version_info().is_es3) {
      fs_source +=
          "#define VARYING in\n"
          "out vec4 frag_color;\n"
          "#define FRAGCOLOR frag_color\n"
          "#define TextureLookup texture\n";
    } else {
      fs_source +=
          "#define VARYING varying\n"
          "#define FRAGCOLOR gl_FragColor\n"
          "#define TextureLookup texture2D\n";
    }
  } else {
    fs_source +=
        "#define VARYING in\n"
        "out vec4 frag_color;\n"
        "#define FRAGCOLOR frag_color\n"
        "#define TextureLookup texture\n";
  }

  fs_source +=
      "uniform mediump sampler2D u_source_texture;\n"
      "VARYING TexCoordPrecision vec2 v_texcoord;\n"
      "\n"
      "void main()\n"
      "{\n"
      "    vec4 c = TextureLookup(u_source_texture, v_texcoord);\n"
      "    FRAGCOLOR = c;\n"
      "}\n";

  GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
  CompileShaderWithLog(fs, fs_source.c_str());
  glAttachShader(srgb_converter_program_, fs);
  glDeleteShader(fs);

  glLinkProgram(srgb_converter_program_);
#ifndef NDEBUG
  GLint linked = 0;
  glGetProgramiv(srgb_converter_program_, GL_LINK_STATUS, &linked);
  if (!linked) {
    DLOG(ERROR) << "BlitFramebuffer: program link failure.";
  }
#endif

  GLuint texture_uniform =
      glGetUniformLocation(srgb_converter_program_, "u_source_texture");
  glUseProgram(srgb_converter_program_);
  glUniform1i(texture_uniform, 0);
}

void SRGBConverter::InitializeSRGBConverter(
    const gles2::GLES2Decoder* decoder) {
  if (srgb_converter_initialized_) {
    return;
  }

  InitializeSRGBConverterProgram();

  glGenTextures(
      srgb_converter_textures_.size(), srgb_converter_textures_.data());
  glActiveTexture(GL_TEXTURE0);
  for (auto srgb_converter_texture : srgb_converter_textures_) {
    glBindTexture(GL_TEXTURE_2D, srgb_converter_texture);

    // Use linear, non-mipmapped sampling with the srgb converter texture
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  }

  glGenFramebuffersEXT(1, &srgb_decoder_fbo_);
  glGenFramebuffersEXT(1, &srgb_encoder_fbo_);

  glGenVertexArraysOES(1, &srgb_converter_vao_);

  decoder->RestoreTextureUnitBindings(0);
  decoder->RestoreActiveTexture();
  decoder->RestoreProgramBindings();

  srgb_converter_initialized_ = true;
}

void SRGBConverter::Destroy() {
  if (srgb_converter_initialized_) {
    glDeleteTextures(srgb_converter_textures_.size(),
                     srgb_converter_textures_.data());
    srgb_converter_textures_.fill(0);

    glDeleteFramebuffersEXT(1, &srgb_decoder_fbo_);
    srgb_decoder_fbo_ = 0;
    glDeleteFramebuffersEXT(1, &srgb_encoder_fbo_);
    srgb_encoder_fbo_ = 0;

    glDeleteVertexArraysOES(1, &srgb_converter_vao_);
    srgb_converter_vao_ = 0;

    glDeleteProgram(srgb_converter_program_);
    srgb_converter_program_ = 0;

    srgb_converter_initialized_ = false;
  }
}

void SRGBConverter::Blit(
    const gles2::GLES2Decoder* decoder,
    GLint srcX0,
    GLint srcY0,
    GLint srcX1,
    GLint srcY1,
    GLint dstX0,
    GLint dstY0,
    GLint dstX1,
    GLint dstY1,
    GLbitfield mask,
    GLenum filter,
    const gfx::Size& framebuffer_size,
    GLuint src_framebuffer,
    GLenum src_framebuffer_internal_format,
    GLenum src_framebuffer_format,
    GLenum src_framebuffer_type,
    GLuint dst_framebuffer,
    bool decode,
    bool encode,
    bool enable_scissor_test) {
  // This function blits srgb image in src fb to srgb image in dst fb.
  // The steps are:
  // 1) Copy and crop pixels from source srgb image to the 1st texture(srgb).
  // 2) Sampling from the 1st texture and drawing to the 2nd texture(linear).
  //    During this step, color space is converted from srgb to linear.
  // 3) Blit pixels from the 2nd texture to the 3rd texture(linear).
  // 4) Sampling from the 3rd texture and drawing to the dst image(srgb).
  //    During this step, color space is converted from linear to srgb.
  // If we need to blit from linear to srgb or vice versa, some steps will be
  // skipped.
  DCHECK(srgb_converter_initialized_);
  // Use RGBA32F as the temp texture's internalformat to prevent precision
  // loss during srgb conversion. But it is not color-renderable and
  // texture-filterable in ES context.
  DCHECK(!feature_info_->gl_version_info().is_es);

  // Set the states
  glActiveTexture(GL_TEXTURE0);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_STENCIL_TEST);
  glDisable(GL_CULL_FACE);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glDepthMask(GL_FALSE);
  glDisable(GL_BLEND);
  glDisable(GL_DITHER);
  if (decoder->GetFeatureInfo()->feature_flags().ext_window_rectangles) {
    glWindowRectanglesEXT(GL_EXCLUSIVE_EXT, 0, nullptr);
  }

  // Copy the image from read buffer to the 1st texture(srgb).
  // TODO(yunchao) If the read buffer is a fbo texture, we can sample
  // directly from that texture. In this way, we can save gpu memory.
  GLuint width_read = 0, height_read = 0, xoffset = 0, yoffset = 0;
  if (decode) {
    glBindFramebufferEXT(GL_FRAMEBUFFER, src_framebuffer);
    glBindTexture(GL_TEXTURE_2D, srgb_converter_textures_[0]);

    // We should not copy pixels outside of the read framebuffer. If we read
    // these pixels, they would become in-bound during BlitFramebuffer. However,
    // Out-of-bounds pixels will be initialized to 0 in CopyTexSubImage.
    // But they should read as if the GL_CLAMP_TO_EDGE texture mapping mode
    // were applied during BlitFramebuffer when the filter is GL_LINEAR.
    GLuint x = srcX1 > srcX0 ? srcX0 : srcX1;
    GLuint y = srcY1 > srcY0 ? srcY0 : srcY1;
    width_read = srcX1 > srcX0 ? srcX1 - srcX0 : srcX0 - srcX1;
    height_read = srcY1 > srcY0 ? srcY1 - srcY0 : srcY0 - srcY1;
    gfx::Rect c(0, 0, framebuffer_size.width(), framebuffer_size.height());
    c.Intersect(gfx::Rect(x, y, width_read, height_read));
    xoffset = c.x() - x;
    yoffset = c.y() - y;
    glCopyTexImage2D(GL_TEXTURE_2D, 0, src_framebuffer_internal_format,
                     c.x(), c.y(), c.width(), c.height(), 0);

    // Make a temporary linear texture as the 2nd texture, where we
    // render the converted (srgb to linear) result to.
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    glBindTexture(GL_TEXTURE_2D, srgb_converter_textures_[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, c.width(), c.height(), 0,
                 GL_RGBA, GL_FLOAT, nullptr);
    glBindFramebufferEXT(GL_FRAMEBUFFER, srgb_decoder_fbo_);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_TEXTURE_2D, srgb_converter_textures_[1], 0);

    // Sampling from the 1st texture(srgb) and drawing to the
    // 2nd texture(linear),
    glUseProgram(srgb_converter_program_);
    glViewport(0, 0, width_read, height_read);

    glBindTexture(GL_TEXTURE_2D, srgb_converter_textures_[0]);
    glBindVertexArrayOES(srgb_converter_vao_);

    glDrawArrays(GL_TRIANGLES, 0, 6);
  } else {
    // Set approriate read framebuffer if decoding is skipped.
    glBindFramebufferEXT(GL_READ_FRAMEBUFFER, src_framebuffer);
  }

  // Create the 3rd texture(linear) as encoder_fbo's draw buffer. But we can
  // reuse the 1st texture and re-allocate the image. Then Blit framebuffer
  // from the 2nd texture(linear) to the 3rd texture. Filtering is done
  // during bliting. Note that the src and dst coordinates may be reversed.
  GLuint width_draw = 0, height_draw = 0;
  if (encode) {
    glBindTexture(GL_TEXTURE_2D, srgb_converter_textures_[0]);

    width_draw = dstX1 > dstX0 ? dstX1 - dstX0 : dstX0 - dstX1;
    height_draw = dstY1 > dstY0 ? dstY1 - dstY0 : dstY0 - dstY1;
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glTexImage2D(
        GL_TEXTURE_2D, 0, decode ? GL_RGBA32F : src_framebuffer_internal_format,
        width_draw, height_draw, 0, decode ? GL_RGBA : src_framebuffer_format,
        decode ? GL_FLOAT : src_framebuffer_type, nullptr);

    glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, srgb_encoder_fbo_);
    glFramebufferTexture2DEXT(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_TEXTURE_2D, srgb_converter_textures_[0], 0);
  } else {
    // Set approriate draw framebuffer if encoding is skipped.
    glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, dst_framebuffer);

    if (enable_scissor_test) {
      glEnable(GL_SCISSOR_TEST);
    }
  }

  glBlitFramebuffer(
      decode ? (srcX0 < srcX1 ? 0 - xoffset : width_read - xoffset) : srcX0,
      decode ? (srcY0 < srcY1 ? 0 - yoffset : height_read - yoffset) : srcY0,
      decode ? (srcX0 < srcX1 ? width_read - xoffset : 0 - xoffset) : srcX1,
      decode ? (srcY0 < srcY1 ? height_read - yoffset : 0 - yoffset) : srcY1,
      encode ? (dstX0 < dstX1 ? 0 : width_draw) : dstX0,
      encode ? (dstY0 < dstY1 ? 0 : height_draw) : dstY0,
      encode ? (dstX0 < dstX1 ? width_draw : 0) : dstX1,
      encode ? (dstY0 < dstY1 ? height_draw : 0) : dstY1,
      mask, filter);

  // Sampling from the 3rd texture(linear) and drawing to the target srgb image.
  // During this step, color space is converted from linear to srgb. We should
  // set appropriate viewport to draw to the correct location in target FB.
  if (encode) {
    GLuint xstart = dstX0 < dstX1 ? dstX0 : dstX1;
    GLuint ystart = dstY0 < dstY1 ? dstY0 : dstY1;
    glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, dst_framebuffer);
    glUseProgram(srgb_converter_program_);
    glViewport(xstart, ystart, width_draw, height_draw);

    glBindTexture(GL_TEXTURE_2D, srgb_converter_textures_[0]);
    glBindVertexArrayOES(srgb_converter_vao_);

    if (enable_scissor_test) {
      glEnable(GL_SCISSOR_TEST);
    }

    glDrawArrays(GL_TRIANGLES, 0, 6);
  }

  // Restore state
  decoder->RestoreAllAttributes();
  decoder->RestoreTextureUnitBindings(0);
  decoder->RestoreActiveTexture();
  decoder->RestoreProgramBindings();
  decoder->RestoreBufferBindings();
  decoder->RestoreFramebufferBindings();
  decoder->RestoreGlobalState();
}

void SRGBConverter::GenerateMipmap(gles2::GLES2Decoder* decoder,
                                   Texture* tex,
                                   GLenum target) {
  // This function generateMipmap for srgb texture.
  // The steps are:
  // 1) Do sampling from the base level of the sRGB texture and draw into
  // a linear texture. During sampling, the sRGB format is converted to
  // Linear format
  // 2) Perform the glGenerateMipmap call against the linear texture
  // 3) Iterate each mipmap level of the linear texture and draw back into
  // the sRGB texture's corresponding mipmap. During drawing, the linear
  // format is converted to sRGB format
  DCHECK(srgb_converter_initialized_);

  GLsizei width;
  GLsizei height;
  GLsizei depth;
  GLenum type = 0;
  GLenum internal_format = 0;
  GLenum format = 0;
  GLsizei base_level = tex->base_level();
  GLsizei max_level = tex->max_level();
  tex->GetLevelSize(target, base_level, &width, &height, &depth);
  tex->GetLevelType(target, base_level, &type, &internal_format);
  format = TextureManager::ExtractFormatFromStorageFormat(internal_format);
  GLint mipmap_levels;
  if (tex->IsImmutable()) {
    mipmap_levels = tex->GetImmutableLevels();
  } else {
    mipmap_levels =
        TextureManager::ComputeMipMapCount(target, width, height, depth);
  }
  GLint max_mipmap_available_level;
  base::CheckedNumeric<GLint> max = base_level;
  max = max - 1 + mipmap_levels;
  if (!max.IsValid() || max.ValueOrDie() > max_level) {
    max_mipmap_available_level = max_level;
  } else {
    max_mipmap_available_level = max.ValueOrDie();
  }

  glBindTexture(GL_TEXTURE_2D, srgb_converter_textures_[1]);
  if (feature_info_->ext_color_buffer_float_available() &&
      feature_info_->oes_texture_float_linear_available()) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA,
                 GL_FLOAT, nullptr);
  } else {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
  }
  glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, srgb_decoder_fbo_);
  glFramebufferTexture2DEXT(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                            GL_TEXTURE_2D, srgb_converter_textures_[1], 0);

  // bind texture with srgb format and render with srgb_converter_program_
  glUseProgram(srgb_converter_program_);
  glViewport(0, 0, width, height);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_STENCIL_TEST);
  glDisable(GL_CULL_FACE);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glDepthMask(GL_FALSE);
  glDisable(GL_BLEND);
  glDisable(GL_DITHER);
  if (decoder->GetFeatureInfo()->feature_flags().ext_window_rectangles) {
    glWindowRectanglesEXT(GL_EXCLUSIVE_EXT, 0, nullptr);
  }

  glBindVertexArrayOES(srgb_converter_vao_);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, tex->service_id());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

  glDrawArrays(GL_TRIANGLES, 0, 6);

  glBindTexture(GL_TEXTURE_2D, srgb_converter_textures_[1]);
  glGenerateMipmapEXT(GL_TEXTURE_2D);

  // bind tex with rgba format and render with srgb_converter_program_
  glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, srgb_encoder_fbo_);
  glBindTexture(GL_TEXTURE_2D, srgb_converter_textures_[1]);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_NEAREST_MIPMAP_NEAREST);

  width = (width == 1) ? 1 : width >> 1;
  height = (height == 1) ? 1 : height >> 1;

  base::CheckedNumeric<GLint> level = base_level;
  level += 1;

  if (!tex->IsImmutable()) {
    glBindTexture(GL_TEXTURE_2D, tex->service_id());
    GLsizei level_width = width;
    GLsizei level_height = height;
    for (base::CheckedNumeric<GLint> i = level;
         i.IsValid() && i.ValueOrDie() <= max_mipmap_available_level; ++i) {
      glTexImage2D(GL_TEXTURE_2D, i.ValueOrDie(), internal_format, level_width,
                   level_height, 0, format, type, nullptr);
      level_width = (level_width == 1) ? 1 : level_width >> 1;
      level_height = (level_height == 1) ? 1 : level_height >> 1;
    }
  }

  glBindTexture(GL_TEXTURE_2D, srgb_converter_textures_[1]);
  for (base::CheckedNumeric<GLint> i = level;
       i.IsValid() && i.ValueOrDie() <= max_mipmap_available_level; ++i) {
    // copy mipmaps level by level from srgb_converter_textures_[1] to tex
    // generate mipmap for tex manually
    glFramebufferTexture2DEXT(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_TEXTURE_2D, tex->service_id(), i.ValueOrDie());

    DCHECK_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
              glCheckFramebufferStatusEXT(GL_DRAW_FRAMEBUFFER));

    glViewport(0, 0, width, height);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    width = (width == 1) ? 1 : width >> 1;
    height = (height == 1) ? 1 : height >> 1;
  }

  // Restore state
  decoder->RestoreAllAttributes();
  decoder->RestoreTextureUnitBindings(0);
  decoder->RestoreActiveTexture();
  decoder->RestoreProgramBindings();
  decoder->RestoreBufferBindings();
  decoder->RestoreFramebufferBindings();
  decoder->RestoreGlobalState();
  decoder->RestoreTextureState(tex->service_id());
}

}  // namespace gles2.
}  // namespace gpu
