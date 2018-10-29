// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_RASTER_IMPLEMENTATION_GLES_H_
#define GPU_COMMAND_BUFFER_CLIENT_RASTER_IMPLEMENTATION_GLES_H_

#include <unordered_map>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/optional.h"
#include "gpu/command_buffer/client/client_font_manager.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/raster_export.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorSpace.h"

namespace gpu {
namespace raster {

struct Capabilities;

// An implementation of RasterInterface on top of GLES2Interface.
class RASTER_EXPORT RasterImplementationGLES : public RasterInterface {
 public:
  RasterImplementationGLES(gles2::GLES2Interface* gl,
                           const gpu::Capabilities& caps);
  ~RasterImplementationGLES() override;

  // Command buffer Flush / Finish.
  void Finish() override;
  void Flush() override;
  void ShallowFlushCHROMIUM() override;
  void OrderingBarrierCHROMIUM() override;

  // SyncTokens.
  void GenSyncTokenCHROMIUM(GLbyte* sync_token) override;
  void GenUnverifiedSyncTokenCHROMIUM(GLbyte* sync_token) override;
  void VerifySyncTokensCHROMIUM(GLbyte** sync_tokens, GLsizei count) override;
  void WaitSyncTokenCHROMIUM(const GLbyte* sync_token) override;

  // Command buffer state.
  GLenum GetError() override;
  GLenum GetGraphicsResetStatusKHR() override;
  void GetIntegerv(GLenum pname, GLint* params) override;
  void LoseContextCHROMIUM(GLenum current, GLenum other) override;

  // Queries: GL_COMMANDS_ISSUED_CHROMIUM / GL_COMMANDS_COMPLETED_CHROMIUM.
  void GenQueriesEXT(GLsizei n, GLuint* queries) override;
  void DeleteQueriesEXT(GLsizei n, const GLuint* queries) override;
  void BeginQueryEXT(GLenum target, GLuint id) override;
  void EndQueryEXT(GLenum target) override;
  void GetQueryObjectuivEXT(GLuint id, GLenum pname, GLuint* params) override;

  // Texture objects.
  GLuint CreateTexture(bool use_buffer,
                       gfx::BufferUsage buffer_usage,
                       viz::ResourceFormat format) override;
  void DeleteTextures(GLsizei n, const GLuint* textures) override;
  void SetColorSpaceMetadata(GLuint texture_id,
                             GLColorSpace color_space) override;
  void TexParameteri(GLuint texture_id, GLenum pname, GLint param) override;

  // Mailboxes.
  void ProduceTextureDirect(GLuint texture, GLbyte* mailbox) override;
  GLuint CreateAndConsumeTexture(bool use_buffer,
                                 gfx::BufferUsage buffer_usage,
                                 viz::ResourceFormat format,
                                 const GLbyte* mailbox) override;

  // Image objects.
  GLuint CreateImageCHROMIUM(ClientBuffer buffer,
                             GLsizei width,
                             GLsizei height,
                             GLenum internalformat) override;
  void BindTexImage2DCHROMIUM(GLuint texture_id, GLint image_id) override;
  void ReleaseTexImage2DCHROMIUM(GLuint texture_id, GLint image_id) override;
  void DestroyImageCHROMIUM(GLuint image_id) override;

  // Texture allocation and copying.
  void TexStorage2D(GLuint texture_id,
                    GLsizei width,
                    GLsizei height) override;

  void CopySubTexture(GLuint source_id,
                      GLuint dest_id,
                      GLint xoffset,
                      GLint yoffset,
                      GLint x,
                      GLint y,
                      GLsizei width,
                      GLsizei height) override;
  void UnpremultiplyAndDitherCopyCHROMIUM(GLuint source_id,
                                          GLuint dest_id,
                                          GLint x,
                                          GLint y,
                                          GLsizei width,
                                          GLsizei height) override;

  // OOP-Raster
  void BeginRasterCHROMIUM(GLuint sk_color,
                           GLuint msaa_sample_count,
                           GLboolean can_use_lcd_text,
                           GLint color_type,
                           const cc::RasterColorSpace& raster_color_space,
                           const GLbyte* mailbox) override;
  void RasterCHROMIUM(const cc::DisplayItemList* list,
                      cc::ImageProvider* provider,
                      const gfx::Size& content_size,
                      const gfx::Rect& full_raster_rect,
                      const gfx::Rect& playback_rect,
                      const gfx::Vector2dF& post_translate,
                      GLfloat post_scale,
                      bool requires_clear) override;
  void EndRasterCHROMIUM() override;

  // Raster via GrContext.
  void BeginGpuRaster() override;
  void EndGpuRaster() override;

  void TraceBeginCHROMIUM(const char* category_name,
                          const char* trace_name) override;
  void TraceEndCHROMIUM() override;

  void SetActiveURLCHROMIUM(const char* url) override;

 private:
  struct Texture {
    Texture(GLuint id,
            GLenum target,
            bool use_buffer,
            gfx::BufferUsage buffer_usage,
            viz::ResourceFormat format);
    GLuint id;
    GLenum target;
    bool use_buffer;
    gfx::BufferUsage buffer_usage;
    viz::ResourceFormat format;
  };

  Texture* GetTexture(GLuint texture_id);
  Texture* EnsureTextureBound(Texture* texture);

  gles2::GLES2Interface* gl_;
  gpu::Capabilities caps_;
  bool use_texture_storage_;
  bool use_texture_storage_image_;

  std::unordered_map<GLuint, Texture> texture_info_;

  DISALLOW_COPY_AND_ASSIGN(RasterImplementationGLES);
};

}  // namespace raster
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_RASTER_IMPLEMENTATION_GLES_H_
