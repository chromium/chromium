// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_IMAGE_PBUFFER_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_IMAGE_PBUFFER_BACKING_H_

#include "gpu/command_buffer/service/shared_image/gl_image_pbuffer.h"
#include "gpu/command_buffer/service/shared_image/gl_texture_common_representations.h"
#include "gpu/command_buffer/service/shared_image/gl_texture_image_backing_helper.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {

// Implementation of SharedImageBacking that takes in a caller-created GL
// Texture and GLImagePbuffer, scopes their lifetime, and exposes the texture
// via SharedImageRepresentations. Used with the legacy mailbox implementation
// in //media's DXVA video decoder. DO NOT USE FOR ANY OTHER PURPOSE.
// TODO(crbug.com/1384438): Remove this class.
class GPU_GLES2_EXPORT GLImagePbufferBacking
    : public SharedImageBacking,
      public GLTextureImageRepresentationClient {
 public:
  // Used when GLImagePbufferBacking is serving as a temporary SharedImage
  // wrapper to an already-allocated texture. The returned backing will not
  // create any new textures.
  static std::unique_ptr<GLImagePbufferBacking> CreateFromGLTexture(
      scoped_refptr<GLImagePbuffer> image,
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      GLenum texture_target,
      scoped_refptr<gles2::TexturePassthrough> wrapped_gl_texture);

  GLImagePbufferBacking(const GLImagePbufferBacking& other) = delete;
  GLImagePbufferBacking& operator=(const GLImagePbufferBacking& other) = delete;
  ~GLImagePbufferBacking() override;

  GLenum GetGLTarget() const;
  GLuint GetGLServiceId() const;

 private:
  GLImagePbufferBacking(
      scoped_refptr<GLImagePbuffer> image,
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      const GLTextureImageBackingHelper::InitializeGLTextureParams& params);

  // SharedImageBacking:
  scoped_refptr<gfx::NativePixmap> GetNativePixmap() override;
  void OnMemoryDump(const std::string& dump_name,
                    base::trace_event::MemoryAllocatorDumpGuid client_guid,
                    base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t client_tracing_id) override;
  SharedImageBackingType GetType() const override;
  gfx::Rect ClearedRect() const final;
  void SetClearedRect(const gfx::Rect& cleared_rect) final;
  std::unique_ptr<GLTextureImageRepresentation> ProduceGLTexture(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) final;
  std::unique_ptr<GLTexturePassthroughImageRepresentation>
  ProduceGLTexturePassthrough(SharedImageManager* manager,
                              MemoryTypeTracker* tracker) final;
  std::unique_ptr<OverlayImageRepresentation> ProduceOverlay(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) final;
  std::unique_ptr<DawnImageRepresentation> ProduceDawn(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      WGPUDevice device,
      WGPUBackendType backend_type) final;
  std::unique_ptr<SkiaImageRepresentation> ProduceSkia(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;
  std::unique_ptr<MemoryImageRepresentation> ProduceMemory(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;

  // GLTextureImageRepresentationClient:
  bool GLTextureImageRepresentationBeginAccess(bool readonly) override;
  void GLTextureImageRepresentationEndAccess(bool readonly) override;
  void GLTextureImageRepresentationRelease(bool have_context) override;

  scoped_refptr<GLImagePbuffer> image_;

  void ReleaseGLTexture(bool have_context);

  const GLTextureImageBackingHelper::InitializeGLTextureParams gl_params_;

  // This is the cleared rect used by ClearedRect and SetClearedRect.
  gfx::Rect cleared_rect_;

  scoped_refptr<gles2::TexturePassthrough> passthrough_texture_;

  sk_sp<SkPromiseImageTexture> cached_promise_texture_;

  base::WeakPtrFactory<GLImagePbufferBacking> weak_factory_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_IMAGE_PBUFFER_BACKING_H_
