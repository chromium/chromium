// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DCOMP_SURFACE_IMAGE_REPRESENTATION_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DCOMP_SURFACE_IMAGE_REPRESENTATION_H_

#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "ui/gl/buildflags.h"

namespace gpu {

// See DCompSurfaceImageBacking::ProduceOverlay for more information.
class DCompSurfaceOverlayImageRepresentation
    : public OverlayImageRepresentation {
 public:
  DCompSurfaceOverlayImageRepresentation(SharedImageManager* manager,
                                         SharedImageBacking* backing,
                                         MemoryTypeTracker* tracker);
  ~DCompSurfaceOverlayImageRepresentation() override;

 protected:
  std::optional<gl::DCLayerOverlayImage> GetDCLayerOverlayImage() override;

 private:
  bool BeginReadAccess(gfx::GpuFenceHandle& acquire_fence) override;
  void EndReadAccess(gfx::GpuFenceHandle release_fence) override;
};

// See DCompSurfaceImageBacking::ProduceSkiaGanesh for more information.
class DCompSurfaceSkiaGaneshImageRepresentation
    : public SkiaGaneshImageRepresentation {
 public:
  DCompSurfaceSkiaGaneshImageRepresentation(
      scoped_refptr<SharedContextState> context_state,
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker);
  ~DCompSurfaceSkiaGaneshImageRepresentation() override;

 protected:
  std::vector<sk_sp<SkSurface>> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      const gfx::Rect& update_rect,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<skgpu::MutableTextureState>* end_state) override;
  void EndWriteAccess() override;

  // These operations don't mean much for DComp surfaces.
  std::vector<sk_sp<GrPromiseImageTexture>> BeginWriteAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<skgpu::MutableTextureState>* end_state) override;
  std::vector<sk_sp<GrPromiseImageTexture>> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<skgpu::MutableTextureState>* end_state) override;
  void EndReadAccess() override;

 private:
  scoped_refptr<SharedContextState> context_state_;
};

// See DCompSurfaceImageBacking::ProduceSkiaGraphite for more information.
class DCompSurfaceDawnImageRepresentation : public DawnImageRepresentation {
 public:
  DCompSurfaceDawnImageRepresentation(SharedImageManager* manager,
                                      SharedImageBacking* backing,
                                      MemoryTypeTracker* tracker,
                                      const wgpu::Device& device,
                                      wgpu::BackendType backend_type);
  ~DCompSurfaceDawnImageRepresentation() override;

  wgpu::Texture BeginAccess(wgpu::TextureUsage usage,
                            wgpu::TextureUsage internal_usage,
                            const gfx::Rect& update_rect) override;
  wgpu::Texture BeginAccess(wgpu::TextureUsage usage,
                            wgpu::TextureUsage internal_usage) override;
  void EndAccess() override;

 private:
  const wgpu::Device device_;
  wgpu::Texture texture_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DCOMP_SURFACE_IMAGE_REPRESENTATION_H_
