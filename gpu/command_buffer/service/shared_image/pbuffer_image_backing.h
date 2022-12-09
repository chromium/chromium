// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_PBUFFER_IMAGE_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_PBUFFER_IMAGE_BACKING_H_

#include "gpu/command_buffer/service/shared_image/gl_texture_common_representations.h"
#include "gpu/command_buffer/service/shared_image/gl_texture_image_backing_helper.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {

// Implementation of SharedImageBacking that takes in a caller-created GL
// Texture, scopes its lifetime, and exposes it via SharedImageRepresentations.
// Used with the legacy mailbox implementation in //media's DXVA video decoder.
// DO NOT USE FOR ANY OTHER PURPOSE.
// TODO(crbug.com/1384438): Remove this class.
class GPU_GLES2_EXPORT PbufferImageBacking
    : public ClearTrackingSharedImageBacking,
      public GLTextureImageRepresentationClient {
 public:
  // PbufferImageBacking serves as a SharedImage wrapper to an already-allocated
  // texture. The returned backing will not create any new textures.
  // |on_destruction_closure| is invoked on destruction of this object.
  PbufferImageBacking(
      base::OnceClosure on_destruction_closure,
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      scoped_refptr<gles2::TexturePassthrough> passthrough_texture);

  PbufferImageBacking(const PbufferImageBacking& other) = delete;
  PbufferImageBacking& operator=(const PbufferImageBacking& other) = delete;
  ~PbufferImageBacking() override;

 private:
  // SharedImageBacking:
  scoped_refptr<gfx::NativePixmap> GetNativePixmap() override;
  SharedImageBackingType GetType() const override;
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
      WGPUBackendType backend_type,
      std::vector<WGPUTextureFormat> view_formats) final;
  std::unique_ptr<SkiaImageRepresentation> ProduceSkia(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;

  // GLTextureImageRepresentationClient:
  bool GLTextureImageRepresentationBeginAccess(bool readonly) override;
  void GLTextureImageRepresentationEndAccess(bool readonly) override;
  void GLTextureImageRepresentationRelease(bool have_context) override;

  base::ScopedClosureRunner on_destruction_closure_runner_;

  scoped_refptr<gles2::TexturePassthrough> passthrough_texture_;

  sk_sp<SkPromiseImageTexture> cached_promise_texture_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_PBUFFER_IMAGE_BACKING_H_
