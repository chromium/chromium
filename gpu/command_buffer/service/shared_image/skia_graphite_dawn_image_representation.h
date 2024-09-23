// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SKIA_GRAPHITE_DAWN_IMAGE_REPRESENTATION_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SKIA_GRAPHITE_DAWN_IMAGE_REPRESENTATION_H_

#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"

#include <webgpu/webgpu_cpp.h>

namespace gpu {
// This is a wrapper class for SkiaGraphiteImageRepresentation to be used in
// Dawn mode.
class GPU_GLES2_EXPORT SkiaGraphiteDawnImageRepresentation
    : public SkiaGraphiteImageRepresentation {
 public:
  static std::unique_ptr<SkiaGraphiteDawnImageRepresentation> Create(
      std::unique_ptr<DawnImageRepresentation> dawn_representation,
      scoped_refptr<SharedContextState> context_state,
      skgpu::graphite::Recorder* recorder,
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      int array_slice = 0);

  ~SkiaGraphiteDawnImageRepresentation() override;

  std::vector<sk_sp<SkSurface>> BeginWriteAccess(
      const SkSurfaceProps& surface_props,
      const gfx::Rect& update_rect) override;
  std::vector<skgpu::graphite::BackendTexture> BeginWriteAccess() override;
  void EndWriteAccess() override;

  std::vector<skgpu::graphite::BackendTexture> BeginReadAccess() override;
  void EndReadAccess() override;
  bool SupportsMultipleConcurrentReadAccess() override;

 private:
  SkiaGraphiteDawnImageRepresentation(
      std::unique_ptr<DawnImageRepresentation> dawn_representation,
      skgpu::graphite::Recorder* recorder,
      scoped_refptr<SharedContextState> context_state,
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      int array_slice,
      wgpu::TextureUsage supported_tex_usages);

  std::vector<skgpu::graphite::BackendTexture> CreateBackendTextures(
      wgpu::Texture texture,
      bool readonly);

  std::unique_ptr<DawnImageRepresentation> dawn_representation_;
  std::unique_ptr<DawnImageRepresentation::ScopedAccess> dawn_scoped_access_;
  scoped_refptr<SharedContextState> context_state_;
  const raw_ptr<skgpu::graphite::Recorder> recorder_;
  const int array_slice_;
  const wgpu::TextureUsage supported_tex_usages_;
  RepresentationAccessMode mode_ = RepresentationAccessMode::kNone;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SKIA_GRAPHITE_DAWN_IMAGE_REPRESENTATION_H_
