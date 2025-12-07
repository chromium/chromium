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
  SkiaGraphiteDawnImageRepresentation(
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
  std::vector<scoped_refptr<GraphiteTextureHolder>> BeginWriteAccess() override;
  void EndWriteAccess() override;

  std::vector<scoped_refptr<GraphiteTextureHolder>> BeginReadAccess() override;
  void EndReadAccess() override;

  wgpu::Device GetDevice() const;

 protected:
  // This will create a list of non-owning or owning BackendTexture wrappers
  // depending on the implementation defined function below which will be
  // invoked by this function.
  std::vector<scoped_refptr<GraphiteTextureHolder>> CreateBackendTextureHolders(
      wgpu::Texture texture,
      bool readonly);

  // Implementation defined function to create a list of GraphiteTextureHolder.
  // The default implementation will return a list of non owning BackendTexture
  // wrappers which will only be safe to be used inside the access scope.
  virtual std::vector<scoped_refptr<GraphiteTextureHolder>> WrapBackendTextures(
      wgpu::Texture texture,
      std::vector<skgpu::graphite::BackendTexture> backend_textures);

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
