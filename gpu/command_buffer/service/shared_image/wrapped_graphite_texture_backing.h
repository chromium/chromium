// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_WRAPPED_GRAPHITE_TEXTURE_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_WRAPPED_GRAPHITE_TEXTURE_BACKING_H_

#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/pass_key.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/wrapped_graphite_texture_holder.h"
#include "skia/buildflags.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "third_party/skia/include/gpu/graphite/BackendTexture.h"

class SkPixmap;

namespace skgpu::graphite {
class Recorder;
}

namespace gpu {

class WrappedSkImageBackingFactory;

// Holds Skia Graphite allocated BackendTextures. Can only be accessed by
// Skia Graphite backend.
class WrappedGraphiteTextureBacking : public ClearTrackingSharedImageBacking {
 public:
  WrappedGraphiteTextureBacking(base::PassKey<WrappedSkImageBackingFactory>,
                                const Mailbox& mailbox,
                                viz::SharedImageFormat format,
                                const gfx::Size& size,
                                const gfx::ColorSpace& color_space,
                                GrSurfaceOrigin surface_origin,
                                SkAlphaType alpha_type,
                                gpu::SharedImageUsageSet usage,
                                std::string debug_label,
                                scoped_refptr<SharedContextState> context_state,
                                const bool thread_safe);

  WrappedGraphiteTextureBacking(const WrappedGraphiteTextureBacking&) = delete;
  WrappedGraphiteTextureBacking& operator=(
      const WrappedGraphiteTextureBacking&) = delete;

  ~WrappedGraphiteTextureBacking() override;

  // Initializes without pixel data.
  bool Initialize();

  // Initializes with pixel data that is uploaded to texture.
  bool InitializeWithData(base::span<const uint8_t> pixels);

  // SharedImageBacking implementation.
  SharedImageBackingType GetType() const override;
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;
  bool UploadFromMemory(const std::vector<SkPixmap>& pixmaps) override;
  bool ReadbackToMemory(const std::vector<SkPixmap>& pixmaps) override;

 protected:
  std::unique_ptr<SkiaGraphiteImageRepresentation> ProduceSkiaGraphite(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;

#if BUILDFLAG(SKIA_USE_DAWN)
  std::unique_ptr<GLTexturePassthroughImageRepresentation>
  ProduceGLTexturePassthrough(SharedImageManager* manager,
                              MemoryTypeTracker* tracker) override;

  std::unique_ptr<DawnImageRepresentation> ProduceDawn(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      const wgpu::Device& device,
      wgpu::BackendType backend_type,
      std::vector<wgpu::TextureFormat> view_formats,
      scoped_refptr<SharedContextState> context_state) override;
#endif  // BUILDFLAG(SKIA_USE_DAWN)

 private:
  class SkiaGraphiteImageRepresentationImpl;

  const std::vector<scoped_refptr<WrappedGraphiteTextureHolder>>&
  GetWrappedGraphiteTextureHolders();
  std::vector<skgpu::graphite::BackendTexture> GetGraphiteBackendTextures();
  bool InsertRecordingAndSubmit();

  skgpu::graphite::Recorder* recorder() const {
    return context_state_->gpu_main_graphite_recorder();
  }

  scoped_refptr<SharedContextState> context_state_;
  std::vector<scoped_refptr<WrappedGraphiteTextureHolder>> texture_holders_;

  // Only stored for thread safe backings.
  scoped_refptr<base::SingleThreadTaskRunner> created_task_runner_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_WRAPPED_GRAPHITE_TEXTURE_BACKING_H_
