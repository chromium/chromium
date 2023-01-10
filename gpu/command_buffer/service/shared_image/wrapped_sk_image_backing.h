// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_WRAPPED_SK_IMAGE_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_WRAPPED_SK_IMAGE_BACKING_H_

#include <memory>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/pass_key.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"

namespace gpu {

class WrappedSkImageBackingFactory;

// Backing type which holds a Skia allocated image. Can only be accessed by
// Skia.
class WrappedSkImageBacking : public ClearTrackingSharedImageBacking {
 public:
  WrappedSkImageBacking(base::PassKey<WrappedSkImageBackingFactory>,
                        const Mailbox& mailbox,
                        viz::SharedImageFormat format,
                        const gfx::Size& size,
                        const gfx::ColorSpace& color_space,
                        GrSurfaceOrigin surface_origin,
                        SkAlphaType alpha_type,
                        uint32_t usage,
                        scoped_refptr<SharedContextState> context_state,
                        const bool thread_safe);

  WrappedSkImageBacking(const WrappedSkImageBacking&) = delete;
  WrappedSkImageBacking& operator=(const WrappedSkImageBacking&) = delete;

  ~WrappedSkImageBacking() override;

  // Initializes without pixel data.
  bool Initialize();

  // Initializes with pixel data that is uploaded to texture. If pixel data is
  // provided and the image format is not ETC1 then |stride| is used. If
  // |stride| is non-zero then it's used as the stride, otherwise it will create
  // SkImageInfo from size() and format() and then SkImageInfo::minRowBytes() is
  // used for the stride. For ETC1 textures pixel data must be provided since
  // updating compressed textures is not supported.
  bool InitializeWithData(base::span<const uint8_t> pixels, size_t stride);

  // SharedImageBacking implementation.
  SharedImageBackingType GetType() const override;
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;
  bool UploadFromMemory(const std::vector<SkPixmap>& pixmaps) override;

 protected:
  std::unique_ptr<SkiaImageRepresentation> ProduceSkia(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;

 private:
  class SkiaImageRepresentationImpl;

  SkColorType GetSkColorType();
  sk_sp<SkSurface> GetSkSurface(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      scoped_refptr<SharedContextState> context_state);
  bool SkSurfaceUnique(scoped_refptr<SharedContextState> context_state);
  sk_sp<SkPromiseImageTexture> GetPromiseTexture();

  scoped_refptr<SharedContextState> context_state_;

  GrBackendTexture backend_texture_;
  sk_sp<SkPromiseImageTexture> promise_texture_;
  int surface_msaa_count_ = 0;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_WRAPPED_SK_IMAGE_BACKING_H_
