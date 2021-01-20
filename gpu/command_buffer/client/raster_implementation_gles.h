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
#include "third_party/skia/include/core/SkColorSpace.h"

namespace gpu {

class ContextSupport;
class GLHelper;

namespace raster {

// An implementation of RasterInterface on top of GLES2Interface.
class RASTER_EXPORT RasterImplementationGLES : public RasterInterface {
 public:
  explicit RasterImplementationGLES(gles2::GLES2Interface* gl,
                                    ContextSupport* context_support);
  ~RasterImplementationGLES() override;

  // Command buffer Flush / Finish.
  void Finish() override;
  void Flush() override;
  void ShallowFlushCHROMIUM() override;
  void OrderingBarrierCHROMIUM() override;

  // Command buffer state.
  GLenum GetError() override;
  GLenum GetGraphicsResetStatusKHR() override;
  void LoseContextCHROMIUM(GLenum current, GLenum other) override;

  // Queries:
  // - GL_COMMANDS_ISSUED_CHROMIUM
  // - GL_COMMANDS_ISSUED_TIMESTAMP_CHROMIUM
  // - GL_COMMANDS_COMPLETED_CHROMIUM
  void GenQueriesEXT(GLsizei n, GLuint* queries) override;
  void DeleteQueriesEXT(GLsizei n, const GLuint* queries) override;
  void BeginQueryEXT(GLenum target, GLuint id) override;
  void EndQueryEXT(GLenum target) override;
  void QueryCounterEXT(GLuint id, GLenum target) override;
  void GetQueryObjectuivEXT(GLuint id, GLenum pname, GLuint* params) override;
  void GetQueryObjectui64vEXT(GLuint id,
                              GLenum pname,
                              GLuint64* params) override;

  // Texture copying.
  void CopySubTexture(const gpu::Mailbox& source_mailbox,
                      const gpu::Mailbox& dest_mailbox,
                      GLenum dest_target,
                      GLint xoffset,
                      GLint yoffset,
                      GLint x,
                      GLint y,
                      GLsizei width,
                      GLsizei height,
                      GLboolean unpack_flip_y,
                      GLboolean unpack_premultiply_alpha) override;

  void WritePixels(const gpu::Mailbox& dest_mailbox,
                   int dst_x_offset,
                   int dst_y_offset,
                   GLenum texture_target,
                   GLuint row_bytes,
                   const SkImageInfo& src_info,
                   const void* src_pixels) override;

  void ConvertYUVAMailboxesToRGB(
      const gpu::Mailbox& dest_mailbox,
      SkYUVColorSpace planes_yuv_color_space,
      SkYUVAInfo::PlaneConfig plane_config,
      SkYUVAInfo::Subsampling subsampling,
      const gpu::Mailbox yuva_plane_mailboxes[]) override;

  // OOP-Raster
  void BeginRasterCHROMIUM(GLuint sk_color,
                           GLuint msaa_sample_count,
                           GLboolean can_use_lcd_text,
                           const gfx::ColorSpace& color_space,
                           const GLbyte* mailbox) override;
  void RasterCHROMIUM(const cc::DisplayItemList* list,
                      cc::ImageProvider* provider,
                      const gfx::Size& content_size,
                      const gfx::Rect& full_raster_rect,
                      const gfx::Rect& playback_rect,
                      const gfx::Vector2dF& post_translate,
                      GLfloat post_scale,
                      bool requires_clear,
                      size_t* max_op_size_hint) override;
  void EndRasterCHROMIUM() override;

  // Image decode acceleration.
  SyncToken ScheduleImageDecode(base::span<const uint8_t> encoded_data,
                                const gfx::Size& output_size,
                                uint32_t transfer_cache_entry_id,
                                const gfx::ColorSpace& target_color_space,
                                bool needs_mips) override;

  void ReadbackARGBPixelsAsync(
      const gpu::Mailbox& source_mailbox,
      GLenum source_target,
      const gfx::Size& dst_size,
      unsigned char* out,
      GLenum format,
      base::OnceCallback<void(bool)> callback) override;

  void ReadbackYUVPixelsAsync(
      const gpu::Mailbox& source_mailbox,
      GLenum source_target,
      const gfx::Size& source_size,
      const gfx::Rect& output_rect,
      bool vertically_flip_texture,
      int y_plane_row_stride_bytes,
      unsigned char* y_plane_data,
      int u_plane_row_stride_bytes,
      unsigned char* u_plane_data,
      int v_plane_row_stride_bytes,
      unsigned char* v_plane_data,
      const gfx::Point& paste_location,
      base::OnceCallback<void()> release_mailbox,
      base::OnceCallback<void(bool)> readback_done) override;

  void ReadbackImagePixels(const gpu::Mailbox& source_mailbox,
                           const SkImageInfo& dst_info,
                           GLuint dst_row_bytes,
                           int src_x,
                           int src_y,
                           void* dst_pixels) override;

  // Raster via GrContext.
  GLuint CreateAndConsumeForGpuRaster(const gpu::Mailbox& mailbox) override;
  void DeleteGpuRasterTexture(GLuint texture) override;
  void BeginGpuRaster() override;
  void EndGpuRaster() override;
  void BeginSharedImageAccessDirectCHROMIUM(GLuint texture,
                                            GLenum mode) override;
  void EndSharedImageAccessDirectCHROMIUM(GLuint texture) override;

  void InitializeDiscardableTextureCHROMIUM(GLuint texture) override;
  void UnlockDiscardableTextureCHROMIUM(GLuint texture) override;
  bool LockDiscardableTextureCHROMIUM(GLuint texture) override;

  void TraceBeginCHROMIUM(const char* category_name,
                          const char* trace_name) override;
  void TraceEndCHROMIUM() override;

  void SetActiveURLCHROMIUM(const char* url) override;

  // InterfaceBase implementation.
  void GenSyncTokenCHROMIUM(GLbyte* sync_token) override;
  void GenUnverifiedSyncTokenCHROMIUM(GLbyte* sync_token) override;
  void VerifySyncTokensCHROMIUM(GLbyte** sync_tokens, GLsizei count) override;
  void WaitSyncTokenCHROMIUM(const GLbyte* sync_token) override;

 private:
  GLHelper* GetGLHelper();
  void OnReadARGBPixelsAsync(GLuint texture_id,
                             base::OnceCallback<void(bool)> callback,
                             bool success);
  void OnReadYUVPixelsAsync(GLuint copy_texture_id,
                            base::OnceCallback<void()> on_release_mailbox,
                            base::OnceCallback<void(bool)> readback_done,
                            bool success);
  void OnReleaseMailbox(GLuint shared_texture_id,
                        base::OnceCallback<void()> release_mailbox);

  gles2::GLES2Interface* gl_;
  ContextSupport* context_support_;
  std::unique_ptr<GLHelper> gl_helper_;
  base::WeakPtrFactory<RasterImplementationGLES> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RasterImplementationGLES);
};

}  // namespace raster
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_RASTER_IMPLEMENTATION_GLES_H_
