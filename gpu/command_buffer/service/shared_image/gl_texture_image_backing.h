// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_TEXTURE_IMAGE_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_TEXTURE_IMAGE_BACKING_H_

#include "gpu/command_buffer/service/shared_image/gl_common_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/gl_texture_holder.h"

class GrPromiseImageTexture;

namespace gpu {

// Implementation of SharedImageBacking that uses GL Textures as storage.
class GLTextureImageBacking : public ClearTrackingSharedImageBacking {
 public:
  static bool SupportsPixelUploadWithFormat(viz::SharedImageFormat format);
  static bool SupportsPixelReadbackWithFormat(viz::SharedImageFormat format);

  GLTextureImageBacking(const Mailbox& mailbox,
                        viz::SharedImageFormat format,
                        const gfx::Size& size,
                        const gfx::ColorSpace& color_space,
                        GrSurfaceOrigin surface_origin,
                        SkAlphaType alpha_type,
                        SharedImageUsageSet usage,
                        std::string debug_layer,
                        bool is_passthrough);
  GLTextureImageBacking(const GLTextureImageBacking&) = delete;
  GLTextureImageBacking& operator=(const GLTextureImageBacking&) = delete;
  ~GLTextureImageBacking() override;

  void InitializeGLTexture(
      const std::vector<GLCommonImageBackingFactory::FormatInfo>& format_info,
      base::span<const uint8_t> pixel_data,
      gl::ProgressReporter* progress_reporter,
      bool framebuffer_attachment_angle);

 private:
  // SharedImageBacking:
  SharedImageBackingType GetType() const override;
  gfx::Rect ClearedRect() const final;
  void SetClearedRect(const gfx::Rect& cleared_rect) final;
  std::unique_ptr<GLTextureImageRepresentation> ProduceGLTexture(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) final;
  std::unique_ptr<GLTexturePassthroughImageRepresentation>
  ProduceGLTexturePassthrough(SharedImageManager* manager,
                              MemoryTypeTracker* tracker) final;
  std::unique_ptr<DawnImageRepresentation> ProduceDawn(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      const wgpu::Device& device,
      wgpu::BackendType backend_type,
      std::vector<wgpu::TextureFormat> view_formats,
      scoped_refptr<SharedContextState> context_state) final;
  std::unique_ptr<SkiaGaneshImageRepresentation> ProduceSkiaGanesh(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;
  bool UploadFromMemory(const std::vector<SkPixmap>& pixmaps) override;
  bool ReadbackToMemory(const std::vector<SkPixmap>& pixmaps) override;

  bool IsPassthrough() const { return is_passthrough_; }

  const bool is_passthrough_;

  std::vector<GLTextureHolder> textures_;
  std::vector<sk_sp<GrPromiseImageTexture>> cached_promise_textures_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_TEXTURE_IMAGE_BACKING_H_
