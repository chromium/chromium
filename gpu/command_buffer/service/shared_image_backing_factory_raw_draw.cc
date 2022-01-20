// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_factory_raw_draw.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/process_memory_dump.h"
#include "cc/paint/paint_op_buffer.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "ui/gl/trace_util.h"

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
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    AutoLock auto_lock(this);
    DCHECK_EQ(read_count_, 0);
    DCHECK(!is_write_);
    ResetPaintOpBuffer();
    DestroyBackendTexture();
  }

  // SharedImageBacking implementation.
  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) override {
    NOTIMPLEMENTED();
    return false;
  }

  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override {
    NOTIMPLEMENTED();
  }

  void OnMemoryDump(const std::string& dump_name,
                    base::trace_event::MemoryAllocatorDump* dump,
                    base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t client_tracing_id) override {
    if (auto tracing_id = GrBackendTextureTracingID(backend_texture_)) {
      // Add a |service_guid| which expresses shared ownership between the
      // various GPU dumps.
      auto client_guid = GetSharedImageGUIDForTracing(mailbox());
      auto service_guid = gl::GetGLTextureServiceGUIDForTracing(tracing_id);
      pmd->CreateSharedGlobalAllocatorDump(service_guid);

      std::string format_dump_name =
          base::StringPrintf("%s/format=%d", dump_name.c_str(), format());
      base::trace_event::MemoryAllocatorDump* format_dump =
          pmd->CreateAllocatorDump(format_dump_name);
      format_dump->AddScalar(
          base::trace_event::MemoryAllocatorDump::kNameSize,
          base::trace_event::MemoryAllocatorDump::kUnitsBytes,
          static_cast<uint64_t>(EstimatedSizeForMemTracking()));

      int importance = 2;  // This client always owns the ref.
      pmd->AddOwnershipEdge(client_guid, service_guid, importance);
    }
  }

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

  void ResetPaintOpBuffer() {
    if (!paint_op_buffer_) {
      DCHECK(!clear_color_);
      DCHECK(!paint_op_release_callback_);
      DCHECK(!backend_texture_.isValid());
      DCHECK(!promise_texture_);
      return;
    }

    clear_color_.reset();
    paint_op_buffer_->Reset();

    if (paint_op_release_callback_)
      std::move(paint_op_release_callback_).Run();
  }

  void DestroyBackendTexture() {
    if (backend_texture_.isValid()) {
      DCHECK(context_state_);
      DeleteGrBackendTexture(context_state_.get(), &backend_texture_);
      backend_texture_ = {};
      promise_texture_.reset();
    }
  }

  cc::PaintOpBuffer* BeginRasterWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      const absl::optional<SkColor>& clear_color) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    AutoLock auto_lock(this);
    if (read_count_) {
      LOG(ERROR) << "The backing is being read.";
      return nullptr;
    }

    if (is_write_) {
      LOG(ERROR) << "The backing is being written.";
      return nullptr;
    }

    is_write_ = true;

    ResetPaintOpBuffer();
    // Should we keep the backing?
    DestroyBackendTexture();

    if (!paint_op_buffer_)
      paint_op_buffer_ = sk_make_sp<cc::PaintOpBuffer>();

    final_msaa_count_ = final_msaa_count;
    surface_props_ = surface_props;
    clear_color_ = clear_color;

    return paint_op_buffer_.get();
  }

  void EndRasterWriteAccess(base::OnceClosure callback) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    AutoLock auto_lock(this);
    DCHECK_EQ(read_count_, 0);
    DCHECK(is_write_);

    is_write_ = false;

    if (callback) {
      DCHECK(!paint_op_release_callback_);
      paint_op_release_callback_ = std::move(callback);
    }
  }

  cc::PaintOpBuffer* BeginRasterReadAccess(
      absl::optional<SkColor>& clear_color) {
    // paint ops will be read on compositor thread, so do not check thread with
    // DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    AutoLock auto_lock(this);
    if (is_write_) {
      LOG(ERROR) << "The backing is being written.";
      return nullptr;
    }

    // If |backend_texture_| is valid, |paint_op_buffer_| should be played back
    // to the |backend_texture_| already, and |paint_op_buffer_| could be
    // released already. So we return nullptr here, and then SkiaRenderer will
    // fallback to using |backend_texture_|.
    if (backend_texture_.isValid())
      return nullptr;

    read_count_++;

    if (!paint_op_buffer_) {
      paint_op_buffer_ = sk_make_sp<cc::PaintOpBuffer>();
    }

    clear_color = clear_color_;
    return paint_op_buffer_.get();
  }

  sk_sp<SkPromiseImageTexture> BeginSkiaReadAccess() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    AutoLock auto_lock(this);
    if (backend_texture_.isValid()) {
      DCHECK(promise_texture_);
      read_count_++;
      return promise_texture_;
    }

    auto mipmap = usage() & SHARED_IMAGE_USAGE_MIPMAP ? GrMipMapped::kYes
                                                      : GrMipMapped::kNo;
    auto sk_color = viz::ResourceFormatToClosestSkColorType(
        /*gpu_compositing=*/true, format());
    backend_texture_ = context_state_->gr_context()->createBackendTexture(
        size().width(), size().height(), sk_color, mipmap, GrRenderable::kYes,
        GrProtected::kNo);
    if (!backend_texture_.isValid()) {
      DLOG(ERROR) << "createBackendTexture() failed with SkColorType:"
                  << sk_color;
      return nullptr;
    }
    promise_texture_ = SkPromiseImageTexture::Make(backend_texture_);

    auto surface = SkSurface::MakeFromBackendTexture(
        context_state_->gr_context(), backend_texture_, surface_origin(),
        final_msaa_count_, sk_color, color_space().ToSkColorSpace(),
        &surface_props_);

    if (clear_color_)
      surface->getCanvas()->clear(*clear_color_);

    if (paint_op_buffer_) {
      cc::PlaybackParams playback_params(nullptr, SkM44());
      paint_op_buffer_->Playback(surface->getCanvas(), playback_params);
    }

    read_count_++;
    return promise_texture_;
  }

  void EndReadAccess() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    AutoLock auto_lock(this);
    DCHECK_GE(read_count_, 0);
    DCHECK(!is_write_);
    read_count_--;

    // If the |backend_texture_| is valid, the |paint_op_buffer_| should have
    // been played back to the |backend_texture_| already, so we can release
    // the |paint_op_buffer_| now.
    if (read_count_ == 0 && backend_texture_.isValid())
      ResetPaintOpBuffer();
  }

  int32_t final_msaa_count_ = 0;
  SkSurfaceProps surface_props_{};
  absl::optional<SkColor> clear_color_;
  sk_sp<cc::PaintOpBuffer> paint_op_buffer_;
  base::OnceClosure paint_op_release_callback_;
  scoped_refptr<SharedContextState> context_state_;
  GrBackendTexture backend_texture_;
  sk_sp<SkPromiseImageTexture> promise_texture_;

  bool is_write_ GUARDED_BY(lock_) = false;
  int read_count_ GUARDED_BY(lock_) = 0;

  THREAD_CHECKER(thread_checker_);
};

class RawDrawBacking::RepresentationRaster
    : public SharedImageRepresentationRaster {
 public:
  RepresentationRaster(SharedImageManager* manager,
                       SharedImageBacking* backing,
                       MemoryTypeTracker* tracker)
      : SharedImageRepresentationRaster(manager, backing, tracker) {}
  ~RepresentationRaster() override = default;

  cc::PaintOpBuffer* BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      const absl::optional<SkColor>& clear_color) override {
    return raw_draw_backing()->BeginRasterWriteAccess(
        final_msaa_count, surface_props, clear_color);
  }

  void EndWriteAccess(base::OnceClosure callback) override {
    raw_draw_backing()->EndRasterWriteAccess(std::move(callback));
  }

  cc::PaintOpBuffer* BeginReadAccess(
      absl::optional<SkColor>& clear_color) override {
    return raw_draw_backing()->BeginRasterReadAccess(clear_color);
  }

  void EndReadAccess() override { raw_draw_backing()->EndReadAccess(); }

 private:
  RawDrawBacking* raw_draw_backing() {
    return static_cast<RawDrawBacking*>(backing());
  }
};

class RawDrawBacking::RepresentationSkia
    : public SharedImageRepresentationSkia {
 public:
  RepresentationSkia(SharedImageManager* manager,
                     SharedImageBacking* backing,
                     MemoryTypeTracker* tracker)
      : SharedImageRepresentationSkia(manager, backing, tracker) {}

  bool SupportsMultipleConcurrentReadAccess() override { return true; }

  sk_sp<SkPromiseImageTexture> BeginWriteAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) override {
    NOTIMPLEMENTED();
    return nullptr;
  }

  void EndWriteAccess(sk_sp<SkSurface> surface) override { NOTIMPLEMENTED(); }

  sk_sp<SkPromiseImageTexture> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) override {
    return raw_draw_backing()->BeginSkiaReadAccess();
  }

  void EndReadAccess() override { raw_draw_backing()->EndReadAccess(); }

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

std::unique_ptr<SharedImageRepresentationSkia> RawDrawBacking::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  if (!context_state_)
    context_state_ = context_state;
  DCHECK(context_state_ == context_state);
  return std::make_unique<RepresentationSkia>(manager, this, tracker);
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
