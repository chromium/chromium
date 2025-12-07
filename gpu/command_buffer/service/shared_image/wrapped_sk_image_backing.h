// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_WRAPPED_SK_IMAGE_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_WRAPPED_SK_IMAGE_BACKING_H_

#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/pass_key.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"
#include "third_party/skia/include/private/chromium/GrPromiseImageTexture.h"

class SkSurface;
class SkPixmap;

namespace gpu {

class WrappedSkImageBackingFactory;

// Holds a Skia Ganesh allocated GrBackendTextures and GrPromiseImageTextures.
// Can only be accessed by Skia Ganesh backend.
class WrappedSkImageBacking : public ClearTrackingSharedImageBacking {
 public:
  WrappedSkImageBacking(base::PassKey<WrappedSkImageBackingFactory>,
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

  WrappedSkImageBacking(const WrappedSkImageBacking&) = delete;
  WrappedSkImageBacking& operator=(const WrappedSkImageBacking&) = delete;

  ~WrappedSkImageBacking() override;

  // Initializes without pixel data.
  bool Initialize(const std::string& debug_label);

  // Initializes with pixel data that is uploaded to texture. For ETC1 textures
  // pixel data must be provided since updating compressed textures is not
  // supported.
  bool InitializeWithData(const std::string& debug_label,
                          base::span<const uint8_t> pixels);

  // SharedImageBacking implementation.
  SharedImageBackingType GetType() const override;
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;
  bool UploadFromMemory(const std::vector<SkPixmap>& pixmaps) override;

 protected:
  std::unique_ptr<SkiaGaneshImageRepresentation> ProduceSkiaGanesh(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;

 private:
  class SkiaImageRepresentationImpl;

  struct TextureHolder {
    TextureHolder();
    TextureHolder(TextureHolder&& other);
    TextureHolder& operator=(TextureHolder&& other);
    ~TextureHolder();

    GrBackendTexture backend_texture;
    sk_sp<GrPromiseImageTexture> promise_texture;
  };

  SkColorType GetSkColorType(int plane_index);
  std::vector<sk_sp<SkSurface>> GetSkSurfaces(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      scoped_refptr<SharedContextState> context_state);
  std::vector<sk_sp<GrPromiseImageTexture>> GetPromiseTextures();

  scoped_refptr<SharedContextState> context_state_;

  std::vector<TextureHolder> textures_;
  int surface_msaa_count_ = 0;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_WRAPPED_SK_IMAGE_BACKING_H_
