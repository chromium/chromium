// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_RAW_DRAW_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_RAW_DRAW_H_

#include "base/callback.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_checker.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"

class SkPromiseImageTexture;

namespace cc {
class PaintOpBuffer;
}

namespace gpu {

class SharedImageBackingRawDraw : public ClearTrackingSharedImageBacking {
 public:
  SharedImageBackingRawDraw(const Mailbox& mailbox,
                            viz::ResourceFormat format,
                            const gfx::Size& size,
                            const gfx::ColorSpace& color_space,
                            GrSurfaceOrigin surface_origin,
                            SkAlphaType alpha_type,
                            uint32_t usage);
  ~SharedImageBackingRawDraw() override;

  // SharedImageBacking implementation.
  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) override;
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;
  void OnMemoryDump(const std::string& dump_name,
                    base::trace_event::MemoryAllocatorDump* dump,
                    base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t client_tracing_id) override;

 protected:
  std::unique_ptr<SharedImageRepresentationRaster> ProduceRaster(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;
  std::unique_ptr<SharedImageRepresentationSkia> ProduceSkia(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;

 private:
  class RepresentationRaster;
  class RepresentationSkia;

  void ResetPaintOpBuffer() EXCLUSIVE_LOCKS_REQUIRED(lock_);
  bool CreateBackendTextureAndFlushPaintOps(bool flush)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void DestroyBackendTexture() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  cc::PaintOpBuffer* BeginRasterWriteAccess(
      scoped_refptr<SharedContextState> context_state,
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      const absl::optional<SkColor>& clear_color,
      bool visible);
  void EndRasterWriteAccess(base::OnceClosure callback);
  cc::PaintOpBuffer* BeginRasterReadAccess(
      absl::optional<SkColor>& clear_color);
  sk_sp<SkPromiseImageTexture> BeginSkiaReadAccess();
  void EndReadAccess();

  int32_t final_msaa_count_ GUARDED_BY_CONTEXT(thread_checker_) = 0;
  SkSurfaceProps surface_props_ GUARDED_BY_CONTEXT(thread_checker_){};
  absl::optional<SkColor> clear_color_ GUARDED_BY(lock_);
  bool visible_ GUARDED_BY(lock_) = false;
  sk_sp<cc::PaintOpBuffer> paint_op_buffer_ GUARDED_BY(lock_);
  base::OnceClosure paint_op_release_callback_
      GUARDED_BY_CONTEXT(thread_checker_);
  scoped_refptr<SharedContextState> context_state_
      GUARDED_BY_CONTEXT(thread_checker_);
  GrBackendTexture backend_texture_ GUARDED_BY(lock_);
  sk_sp<SkPromiseImageTexture> promise_texture_
      GUARDED_BY_CONTEXT(thread_checker_);

  bool is_write_ GUARDED_BY(lock_) = false;
  int read_count_ GUARDED_BY(lock_) = 0;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_RAW_DRAW_H_
