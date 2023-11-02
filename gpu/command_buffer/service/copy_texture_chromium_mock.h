// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_COPY_TEXTURE_CHROMIUM_MOCK_H_
#define GPU_COMMAND_BUFFER_SERVICE_COPY_TEXTURE_CHROMIUM_MOCK_H_

#include "gpu/command_buffer/service/gles2_cmd_copy_tex_image.h"
#include "gpu/command_buffer/service/gles2_cmd_copy_texture_chromium.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace gpu {
namespace gles2 {

class MockCopyTexImageResourceManager final
    : public CopyTexImageResourceManager {
 public:
  MockCopyTexImageResourceManager(const gles2::FeatureInfo* feature_info);

  MockCopyTexImageResourceManager(const MockCopyTexImageResourceManager&) =
      delete;
  MockCopyTexImageResourceManager& operator=(
      const MockCopyTexImageResourceManager&) = delete;

  ~MockCopyTexImageResourceManager() final;

  MOCK_METHOD1(Initialize, void(const DecoderContext* decoder));
  MOCK_METHOD0(Destroy, void());

  // Cannot MOCK_METHOD more than 10 args.
  void DoCopyTexImage2DToLUMACompatibilityTexture(
      const DecoderContext* decoder,
      GLuint dest_texture,
      GLenum dest_texture_target,
      GLenum dest_target,
      GLenum luma_format,
      GLenum luma_type,
      GLint level,
      GLenum internal_format,
      GLint x,
      GLint y,
      GLsizei width,
      GLsizei height,
      GLuint source_framebuffer,
      GLenum source_framebuffer_internal_format) override {}

  void DoCopyTexSubImageToLUMACompatibilityTexture(
      const DecoderContext* decoder,
      GLuint dest_texture,
      GLenum dest_texture_target,
      GLenum dest_target,
      GLenum luma_format,
      GLenum luma_type,
      GLint level,
      GLint xoffset,
      GLint yoffset,
      GLint zoffset,
      GLint x,
      GLint y,
      GLsizei width,
      GLsizei height,
      GLuint source_framebuffer,
      GLenum source_framebuffer_internal_format) override {}
};

class MockCopyTextureResourceManager final
    : public CopyTextureCHROMIUMResourceManager {
 public:
  MockCopyTextureResourceManager();

  MockCopyTextureResourceManager(const MockCopyTextureResourceManager&) =
      delete;
  MockCopyTextureResourceManager& operator=(
      const MockCopyTextureResourceManager&) = delete;

  ~MockCopyTextureResourceManager() final;

  MOCK_METHOD2(Initialize,
               void(const DecoderContext* decoder,
                    const gles2::FeatureInfo::FeatureFlags& feature_flags));
  MOCK_METHOD0(Destroy, void());

  // Cannot MOCK_METHOD more than 10 args.
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
      CopyTextureMethod method,
      CopyTexImageResourceManager* luma_emulation_blitter) override {}
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
      CopyTextureMethod method,
      CopyTexImageResourceManager* luma_emulation_blitter) override {}
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_COPY_TEXTURE_CHROMIUM_MOCK_H_
