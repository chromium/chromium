// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DAWN_IMAGE_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DAWN_IMAGE_BACKING_H_

#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"

namespace gpu {

// Implementation of SharedImageBacking that uses webgpu texture as storage.
// Note that this backing will be used in non-production code as of now since
// in production, we do have other backings which supports Dawn representation.
class GPU_GLES2_EXPORT DawnImageBacking : public SharedImageBacking {
 public:
  DawnImageBacking(const Mailbox& mailbox,
                   viz::SharedImageFormat format,
                   const gfx::Size& size,
                   const gfx::ColorSpace& color_space,
                   GrSurfaceOrigin surface_origin,
                   SkAlphaType alpha_type,
                   SharedImageUsageSet usage,
                   std::string debug_label);
  ~DawnImageBacking() override;

  wgpu::Device device() const { return device_; }
  wgpu::Texture GetTexture() const { return texture_; }

  void InitializeForTesting(const wgpu::Device& device);

 private:
  // SharedImageBacking:
  SharedImageBackingType GetType() const override;
  gfx::Rect ClearedRect() const final;
  void SetClearedRect(const gfx::Rect& cleared_rect) final;
  std::unique_ptr<DawnImageRepresentation> ProduceDawn(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      const wgpu::Device& device,
      wgpu::BackendType backend_type,
      std::vector<wgpu::TextureFormat> view_formats,
      scoped_refptr<SharedContextState> context_state) final;
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;

  wgpu::Device device_;
  wgpu::Texture texture_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DAWN_IMAGE_BACKING_H_
