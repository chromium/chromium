// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_factory_raw_draw.h"

#include "base/logging.h"
#include "base/thread_annotations.h"
#include "cc/paint/paint_op_buffer.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image_representation.h"

namespace gpu {

class SharedImageBackingFactoryRawDraw;

namespace {

class RawDrawBacking : public ClearTrackingSharedImageBacking {
 public:
  RawDrawBacking(const Mailbox& mailbox,
                 viz::ResourceFormat format,
                 const gfx::Size& size,
                 const gfx::ColorSpace& color_space,
                 GrSurfaceOrigin surface_origin,
                 SkAlphaType alpha_type,
                 uint32_t usage,
                 size_t estimated_size)
      : ClearTrackingSharedImageBacking(mailbox,
                                        format,
                                        size,
                                        color_space,
                                        surface_origin,
                                        alpha_type,
                                        usage,
                                        estimated_size,
                                        true /* is_thread_safe */) {}

  ~RawDrawBacking() override {
    AutoLock auto_lock(this);
    DCHECK_EQ(read_count_, 0);
    DCHECK(!is_write_);
    ResetPaintOpBuffer();
  }

  // SharedImageBacking implementation.
  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) override {
    NOTREACHED() << "Not supported.";
    return false;
  }

  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override {
    NOTREACHED() << "Not supported.";
  }

  void OnMemoryDump(const std::string& dump_name,
                    base::trace_event::MemoryAllocatorDump* dump,
                    base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t client_tracing_id) override {}

 protected:
  std::unique_ptr<SharedImageRepresentationRaster> ProduceRaster(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;

 private:
  class RepresentationRaster;

  void ResetPaintOpBuffer() {
    if (!paint_op_buffer_) {
      DCHECK(!clear_color_);
      DCHECK(!paint_op_release_callback_);
      return;
    }

    final_msaa_count_ = 0;
    clear_color_.reset();
    paint_op_buffer_->Reset();

    if (paint_op_release_callback_)
      std::move(paint_op_release_callback_).Run();
  }

  int32_t final_msaa_count_ = 0;
  absl::optional<SkColor> clear_color_;
  sk_sp<cc::PaintOpBuffer> paint_op_buffer_;
  base::OnceClosure paint_op_release_callback_;

  bool is_write_ GUARDED_BY(lock_) = false;
  int read_count_ GUARDED_BY(lock_) = 0;
};

class RawDrawBacking::RepresentationRaster
    : public SharedImageRepresentationRaster {
 public:
  RepresentationRaster(SharedImageManager* manager,
                       SharedImageBacking* backing,
                       MemoryTypeTracker* tracker)
      : SharedImageRepresentationRaster(manager, backing, tracker) {}
  ~RepresentationRaster() override = default;

  cc::PaintOpBuffer* BeginReadAccess(
      absl::optional<SkColor>& clear_color) override {
    AutoLock auto_lock(raw_draw_backing());
    if (raw_draw_backing()->is_write_) {
      LOG(ERROR) << "The backing is being written.";
      return nullptr;
    }

    raw_draw_backing()->read_count_++;

    if (!raw_draw_backing()->paint_op_buffer_) {
      raw_draw_backing()->paint_op_buffer_ = sk_make_sp<cc::PaintOpBuffer>();
    }

    clear_color = raw_draw_backing()->clear_color_;
    return raw_draw_backing()->paint_op_buffer_.get();
  }

  void EndReadAccess() override {
    AutoLock auto_lock(raw_draw_backing());
    DCHECK_GE(raw_draw_backing()->read_count_, 0);
    DCHECK(!raw_draw_backing()->is_write_);
    raw_draw_backing()->read_count_--;
  }

  cc::PaintOpBuffer* BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      const absl::optional<SkColor>& clear_color) override {
    AutoLock auto_lock(raw_draw_backing());
    if (raw_draw_backing()->read_count_) {
      LOG(ERROR) << "The backing is being read.";
      return nullptr;
    }

    if (raw_draw_backing()->is_write_) {
      LOG(ERROR) << "The backing is being written.";
      return nullptr;
    }

    raw_draw_backing()->is_write_ = true;

    raw_draw_backing()->ResetPaintOpBuffer();
    if (!raw_draw_backing()->paint_op_buffer_) {
      raw_draw_backing()->paint_op_buffer_ = sk_make_sp<cc::PaintOpBuffer>();
    }
    raw_draw_backing()->final_msaa_count_ = final_msaa_count;
    raw_draw_backing()->clear_color_ = clear_color;

    return raw_draw_backing()->paint_op_buffer_.get();
  }

  void EndWriteAccess(base::OnceClosure callback) override {
    AutoLock auto_lock(raw_draw_backing());
    DCHECK_EQ(raw_draw_backing()->read_count_, 0);
    DCHECK(raw_draw_backing()->is_write_);

    raw_draw_backing()->is_write_ = false;

    if (callback) {
      DCHECK(!raw_draw_backing()->paint_op_release_callback_);
      raw_draw_backing()->paint_op_release_callback_ = std::move(callback);
    }
  }

 private:
  RawDrawBacking* raw_draw_backing() {
    return static_cast<RawDrawBacking*>(backing());
  }
};

std::unique_ptr<SharedImageRepresentationRaster> RawDrawBacking::ProduceRaster(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  return std::make_unique<RepresentationRaster>(manager, this, tracker);
}

}  // namespace

SharedImageBackingFactoryRawDraw::SharedImageBackingFactoryRawDraw() = default;
SharedImageBackingFactoryRawDraw::~SharedImageBackingFactoryRawDraw() = default;

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryRawDraw::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    bool is_thread_safe) {
  DCHECK(!is_thread_safe);
  auto texture = std::make_unique<RawDrawBacking>(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      /*estimated_size=*/0);
  return texture;
}

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryRawDraw::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    base::span<const uint8_t> data) {
  NOTREACHED() << "Not supported";
  return nullptr;
}

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactoryRawDraw::CreateSharedImage(
    const Mailbox& mailbox,
    int client_id,
    gfx::GpuMemoryBufferHandle handle,
    gfx::BufferFormat buffer_format,
    gfx::BufferPlane plane,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage) {
  NOTREACHED() << "Not supported";
  return nullptr;
}

bool SharedImageBackingFactoryRawDraw::CanUseRawDrawBacking(
    uint32_t usage,
    GrContextType gr_context_type) const {
  // Ignore for mipmap usage.
  usage &= ~SHARED_IMAGE_USAGE_MIPMAP;

  auto kRawDrawBackingUsage =
      SHARED_IMAGE_USAGE_DISPLAY | SHARED_IMAGE_USAGE_RASTER |
      SHARED_IMAGE_USAGE_OOP_RASTERIZATION | SHARED_IMAGE_USAGE_RAW_DRAW;
  return usage == kRawDrawBackingUsage;
}

bool SharedImageBackingFactoryRawDraw::IsSupported(
    uint32_t usage,
    viz::ResourceFormat format,
    bool thread_safe,
    gfx::GpuMemoryBufferType gmb_type,
    GrContextType gr_context_type,
    bool* allow_legacy_mailbox,
    bool is_pixel_used) {
  if (!CanUseRawDrawBacking(usage, gr_context_type)) {
    return false;
  }

  if (is_pixel_used) {
    return false;
  }

  if (gmb_type != gfx::EMPTY_BUFFER) {
    return false;
  }

  *allow_legacy_mailbox = false;
  return true;
}

}  // namespace gpu
