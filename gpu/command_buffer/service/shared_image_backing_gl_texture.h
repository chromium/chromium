// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_GL_TEXTURE_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_GL_TEXTURE_H_

#include "gpu/command_buffer/service/shared_image_backing_gl_common.h"

namespace gl {
class GLImageEGL;
}

namespace gpu {

// Implementation of SharedImageBacking that creates a GL Texture that is not
// backed by a GLImage.
class SharedImageBackingGLTexture : public ClearTrackingSharedImageBacking {
 public:
  SharedImageBackingGLTexture(const Mailbox& mailbox,
                              viz::ResourceFormat format,
                              const gfx::Size& size,
                              const gfx::ColorSpace& color_space,
                              GrSurfaceOrigin surface_origin,
                              SkAlphaType alpha_type,
                              uint32_t usage,
                              bool is_passthrough);
  SharedImageBackingGLTexture(const SharedImageBackingGLTexture&) = delete;
  SharedImageBackingGLTexture& operator=(const SharedImageBackingGLTexture&) =
      delete;
  ~SharedImageBackingGLTexture() override;

  void InitializeGLTexture(
      GLuint service_id,
      const SharedImageBackingGLCommon::InitializeGLTextureParams& params);
  void SetCompatibilitySwizzle(
      const gles2::Texture::CompatibilitySwizzle* swizzle);

  GLenum GetGLTarget() const;
  GLuint GetGLServiceId() const;
  void CreateEGLImage();

 private:
  // SharedImageBacking:
  void OnMemoryDump(const std::string& dump_name,
                    base::trace_event::MemoryAllocatorDump* dump,
                    base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t client_tracing_id) override;
  gfx::Rect ClearedRect() const final;
  void SetClearedRect(const gfx::Rect& cleared_rect) final;
  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) final;
  std::unique_ptr<SharedImageRepresentationGLTexture> ProduceGLTexture(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) final;
  std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
  ProduceGLTexturePassthrough(SharedImageManager* manager,
                              MemoryTypeTracker* tracker) final;
  std::unique_ptr<SharedImageRepresentationDawn> ProduceDawn(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      WGPUDevice device,
      WGPUBackendType backend_type) final;
  std::unique_ptr<SharedImageRepresentationSkia> ProduceSkia(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;

  bool IsPassthrough() const { return is_passthrough_; }

  const bool is_passthrough_;
  gles2::Texture* texture_ = nullptr;
  scoped_refptr<gles2::TexturePassthrough> passthrough_texture_;

  sk_sp<SkPromiseImageTexture> cached_promise_texture_;
  scoped_refptr<gl::GLImageEGL> image_egl_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_GL_TEXTURE_H_
