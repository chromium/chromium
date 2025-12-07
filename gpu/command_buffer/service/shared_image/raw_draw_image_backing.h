// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_RAW_DRAW_IMAGE_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_RAW_DRAW_IMAGE_BACKING_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_checker.h"
#include "cc/paint/paint_op_buffer.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSurface.h"

class GrPromiseImageTexture;

namespace gpu {

class RawDrawImageBacking : public ClearTrackingSharedImageBacking {
 public:
  RawDrawImageBacking(const Mailbox& mailbox,
                      viz::SharedImageFormat format,
                      const gfx::Size& size,
                      const gfx::ColorSpace& color_space,
                      GrSurfaceOrigin surface_origin,
                      SkAlphaType alpha_type,
                      gpu::SharedImageUsageSet usage,
                      std::string debug_label);
  ~RawDrawImageBacking() override;

  // SharedImageBacking implementation.
  SharedImageBackingType GetType() const override;
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;

 protected:
  std::unique_ptr<RasterImageRepresentation> ProduceRaster(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;
  std::unique_ptr<SkiaGaneshImageRepresentation> ProduceSkiaGanesh(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;

 private:
  class RasterRawDrawImageRepresentation;
  class SkiaRawDrawImageRepresentation;

  void ResetPaintOpBuffer() EXCLUSIVE_LOCKS_REQUIRED(lock_);
  bool CreateBackendTextureAndFlushPaintOps(bool flush)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void DestroyBackendTexture() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  cc::PaintOpBuffer* BeginRasterWriteAccess(
      scoped_refptr<SharedContextState> context_state,
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      const std::optional<SkColor4f>& clear_color,
      bool visible);
  void EndRasterWriteAccess(base::OnceClosure callback);
  cc::PaintOpBuffer* BeginRasterReadAccess(
      std::optional<SkColor4f>& clear_color);
  sk_sp<GrPromiseImageTexture> BeginSkiaReadAccess();
  void EndReadAccess();

  int32_t final_msaa_count_ GUARDED_BY_CONTEXT(thread_checker_) = 0;
  SkSurfaceProps surface_props_ GUARDED_BY_CONTEXT(thread_checker_){};
  std::optional<SkColor4f> clear_color_ GUARDED_BY(lock_);
  bool visible_ GUARDED_BY(lock_) = false;
  std::optional<cc::PaintOpBuffer> paint_op_buffer_ GUARDED_BY(lock_);
  base::OnceClosure paint_op_release_callback_
      GUARDED_BY_CONTEXT(thread_checker_);
  scoped_refptr<SharedContextState> context_state_
      GUARDED_BY_CONTEXT(thread_checker_);
  GrBackendTexture backend_texture_ GUARDED_BY(lock_);
  sk_sp<GrPromiseImageTexture> promise_texture_
      GUARDED_BY_CONTEXT(thread_checker_);

  bool is_write_ GUARDED_BY(lock_) = false;
  int read_count_ GUARDED_BY(lock_) = 0;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_RAW_DRAW_IMAGE_BACKING_H_
