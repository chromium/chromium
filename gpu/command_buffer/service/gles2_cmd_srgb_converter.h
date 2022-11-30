// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_SRGB_CONVERTER_H_
#define GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_SRGB_CONVERTER_H_

#include <array>

#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {
namespace gles2 {

class GLES2Decoder;

// This class encapsulates the resources required to implement the
// glBlitFramebuffer command, which somtimes requires to convert sRGB
// to linear (RGBA) color format, or vice versa.
class GPU_GLES2_EXPORT SRGBConverter {
 public:
  explicit SRGBConverter(const gles2::FeatureInfo* feature_info);

  SRGBConverter(const SRGBConverter&) = delete;
  SRGBConverter& operator=(const SRGBConverter&) = delete;

  ~SRGBConverter();

  void InitializeSRGBConverter(const gles2::GLES2Decoder* decoder);
  void Destroy();

  void Blit(
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
      bool enable_scissor_test);

  void GenerateMipmap(gles2::GLES2Decoder* decoder,
                      Texture* tex,
                      GLenum target);

 private:
  void InitializeSRGBConverterProgram();
  scoped_refptr<const gles2::FeatureInfo> feature_info_;

  bool srgb_converter_initialized_ = false;

  GLuint srgb_converter_program_ = 0;

  std::array<GLuint, 2> srgb_converter_textures_ = {{0, 0}};
  GLuint srgb_decoder_fbo_ = 0;
  GLuint srgb_encoder_fbo_ = 0;

  GLuint srgb_converter_vao_ = 0;
};

}  // namespace gles2.
}  // namespace gpu.

#endif  // GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_SRGB_CONVERTER_H_
