// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_COPY_TEXTURE_CHROMIUM_H_
#define GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_COPY_TEXTURE_CHROMIUM_H_

#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {

class DecoderContext;

namespace gles2 {

class CopyTexImageResourceManager;

enum class CopyTextureMethod {
  // Use CopyTex{Sub}Image2D to copy from the source to the destination.
  DIRECT_COPY,
  // Draw from the source to the destination texture.
  DIRECT_DRAW,
  // Draw to an intermediate texture, and then copy to the destination texture.
  DRAW_AND_COPY,
  // Draw to an intermediate texture in RGBA format, read back pixels in the
  // intermediate texture from GPU to CPU, and then upload to the destination
  // texture.
  DRAW_AND_READBACK,
  // CopyTexture isn't available.
  NOT_COPYABLE
};

// TODOs(qiankun.miao@intel.com):
// 1. Add readback path for RGB9_E5 and float formats (if extension isn't
// available and they are not color-renderable).
// 2. Support GL_TEXTURE_3D as valid dest_target.
// 3. Support ALPHA, LUMINANCE and LUMINANCE_ALPHA formats on core profile.

// This class encapsulates the resources required to implement the
// GL_CHROMIUM_copy_texture extension.  The copy operation is performed
// via glCopyTexImage2D() or a blit to a framebuffer object.
// The target of |dest_id| texture must be GL_TEXTURE_2D.
class GPU_GLES2_EXPORT CopyTextureCHROMIUMResourceManager {
 public:
  CopyTextureCHROMIUMResourceManager(
      const CopyTextureCHROMIUMResourceManager&) = delete;
  CopyTextureCHROMIUMResourceManager& operator=(
      const CopyTextureCHROMIUMResourceManager&) = delete;

  virtual ~CopyTextureCHROMIUMResourceManager();

  // Factory generating a real implementation.
  static CopyTextureCHROMIUMResourceManager* Create();

  virtual void Initialize(
      const DecoderContext* decoder,
      const gles2::FeatureInfo::FeatureFlags& feature_flags) = 0;
  virtual void Destroy() = 0;

  virtual void DoCopyTexture(
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
      CopyTexImageResourceManager* luma_emulation_blitter) = 0;

  virtual void DoCopySubTexture(
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
      CopyTexImageResourceManager* luma_emulation_blitter) = 0;

  // The attributes used during invocation of the extension.
  static const GLuint kVertexPositionAttrib = 0;

 protected:
  CopyTextureCHROMIUMResourceManager();
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_COPY_TEXTURE_CHROMIUM_H_
