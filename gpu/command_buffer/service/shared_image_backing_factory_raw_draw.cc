// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_factory_raw_draw.h"

#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "cc/paint/paint_op_buffer.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "skia/buildflags.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrTypes.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/trace_util.h"

namespace gpu {
namespace raster {

class SharedImageBackingFactoryRawDraw;

namespace {

class RawDrawBacking : public ClearTrackingSharedImageBacking {
 public:
  RawDrawBacking(base::PassKey<SharedImageBackingFactoryRawDraw>,
                 const Mailbox& mailbox,
                 viz::ResourceFormat format,
                 const gfx::Size& size,
                 const gfx::ColorSpace& color_space,
                 GrSurfaceOrigin surface_origin,
                 SkAlphaType alpha_type,
                 uint32_t usage,
                 size_t estimated_size,
                 scoped_refptr<SharedContextState> context_state)
      : ClearTrackingSharedImageBacking(mailbox,
                                        format,
                                        size,
                                        color_space,
                                        surface_origin,
                                        alpha_type,
                                        usage,
                                        estimated_size,
                                        false /* is_thread_safe */),
        context_state_(std::move(context_state)) {
    DCHECK(!!context_state_);
  }

  ~RawDrawBacking() override {
    context_state_->MakeCurrent(nullptr);
    promise_texture_.reset();
    context_state_->EraseCachedSkSurface(this);

    ResetPaintOpBuffer();

    if (backend_texture_.isValid())
      DeleteGrBackendTexture(context_state_.get(), &backend_texture_);

    if (!context_state_->context_lost())
      context_state_->set_need_context_state_reset(true);
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

  SkColorType GetSkColorType() {
    return viz::ResourceFormatToClosestSkColorType(
        /*gpu_compositing=*/true, format());
  }

  sk_sp<SkSurface> GetSkSurface(int final_msaa_count,
                                const SkSurfaceProps& surface_props) {
    if (context_state_->context_lost()) {
      LOG(ERROR) << "Context is lost.";
      return nullptr;
    }

    DCHECK(context_state_->IsCurrent(nullptr));

    auto surface = context_state_->GetCachedSkSurface(this);
    if (!surface || final_msaa_count != surface_msaa_count_ ||
        surface_props != surface->props()) {
      DCHECK(backend_texture_.isValid());
      surface = SkSurface::MakeFromBackendTexture(
          context_state_->gr_context(), backend_texture_, surface_origin(),
          final_msaa_count, GetSkColorType(), color_space().ToSkColorSpace(),
          &surface_props);
      if (!surface) {
        LOG(ERROR) << "MakeFromBackendTexture() failed.";
        context_state_->EraseCachedSkSurface(this);
        return nullptr;
      }
      surface_msaa_count_ = final_msaa_count;
      context_state_->CacheSkSurface(this, surface);
    }
    return surface;
  }

  bool SkSurfaceUnique() {
    return context_state_->CachedSkSurfaceIsUnique(this);
  }

 protected:
  std::unique_ptr<SharedImageRepresentationSkia> ProduceSkia(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;

  std::unique_ptr<SharedImageRepresentationRaster> ProduceRaster(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;

 private:
  friend class gpu::raster::SharedImageBackingFactoryRawDraw;
  class RepresentationSkia;
  class RepresentationRaster;

  bool Initialize() {
    if (context_state_->context_lost()) {
      LOG(ERROR) << "Context is lost.";
      return false;
    }

    // MakeCurrent to avoid destroying another client's state because Skia may
    // change GL state to create and upload textures (crbug.com/1095679).
    context_state_->MakeCurrent(nullptr);
    context_state_->set_need_context_state_reset(true);

    DCHECK_NE(format(), viz::ResourceFormat::ETC1);
    auto mipmap = usage() & SHARED_IMAGE_USAGE_MIPMAP ? GrMipMapped::kYes
                                                      : GrMipMapped::kNo;
#if DCHECK_IS_ON()
    // Initializing to bright green makes it obvious if the pixels are not
    // properly set before they are displayed (e.g. https://crbug.com/956555).
    // We don't do this on release builds because there is a slight overhead.
    backend_texture_ = context_state_->gr_context()->createBackendTexture(
        size().width(), size().height(), GetSkColorType(), SkColors::kBlue,
        mipmap, GrRenderable::kYes, GrProtected::kNo);
#else
    backend_texture_ = context_state_->gr_context()->createBackendTexture(
        size().width(), size().height(), GetSkColorType(), mipmap,
        GrRenderable::kYes, GrProtected::kNo);
#endif

    if (!backend_texture_.isValid()) {
      DLOG(ERROR) << "createBackendTexture() failed with SkColorType:"
                  << GetSkColorType();
      return false;
    }

    promise_texture_ = SkPromiseImageTexture::Make(backend_texture_);

    return true;
  }

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

  void FlushPaintOpBuffer() {
    bool need_flush =
        clear_color_ || (paint_op_buffer_ && paint_op_buffer_->size());

    if (!need_flush)
      return;
    auto surface = GetSkSurface(final_msaa_count_, surface_props_);
    auto* canvas = surface->getCanvas();

    if (clear_color_)
      canvas->drawColor(*clear_color_, SkBlendMode::kSrc);

    if (paint_op_buffer_ && paint_op_buffer_->size()) {
      cc::PlaybackParams playback_params(nullptr, SkM44());
      paint_op_buffer_->Playback(canvas, playback_params);
    }

    surface->flush();
  }

  scoped_refptr<SharedContextState> context_state_;

  GrBackendTexture backend_texture_;
  sk_sp<SkPromiseImageTexture> promise_texture_;
  int surface_msaa_count_ = 0;

  int32_t final_msaa_count_ = 0;

  SkSurfaceProps surface_props_{/*flags=*/0, kUnknown_SkPixelGeometry};
  absl::optional<SkColor> clear_color_;
  sk_sp<cc::PaintOpBuffer> paint_op_buffer_;
  base::OnceClosure paint_op_release_callback_;
};

class RawDrawBacking::RepresentationSkia
    : public SharedImageRepresentationSkia {
 public:
  RepresentationSkia(SharedImageManager* manager,
                     SharedImageBacking* backing,
                     MemoryTypeTracker* tracker)
      : SharedImageRepresentationSkia(manager, backing, tracker) {}

  ~RepresentationSkia() override = default;

  sk_sp<SkSurface> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores) override {
    NOTREACHED() << "Not supported";
    return nullptr;
  }

  sk_sp<SkPromiseImageTexture> BeginWriteAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) override {
    NOTREACHED() << "Not supported";
    return nullptr;
  }

  void EndWriteAccess(sk_sp<SkSurface> surface) override { NOTREACHED(); }

  // Skia read access is supported temporarily. It will be removed when RawDraw
  // is fully supported.
  sk_sp<SkPromiseImageTexture> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores) override {
    raw_draw_backing()->FlushPaintOpBuffer();
    raw_draw_backing()->ResetPaintOpBuffer();
    return raw_draw_backing()->promise_texture_;
  }

  void EndReadAccess() override {}

  bool SupportsMultipleConcurrentReadAccess() override { return true; }

 private:
  RawDrawBacking* raw_draw_backing() {
    return static_cast<RawDrawBacking*>(backing());
  }
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
    if (!raw_draw_backing()->paint_op_buffer_) {
      raw_draw_backing()->paint_op_buffer_ = sk_make_sp<cc::PaintOpBuffer>();
    }
    clear_color = raw_draw_backing()->clear_color_;
    return raw_draw_backing()->paint_op_buffer_.get();
  }

  void EndReadAccess() override {}

  cc::PaintOpBuffer* BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      const absl::optional<SkColor>& clear_color) override {
    raw_draw_backing()->ResetPaintOpBuffer();
    if (!raw_draw_backing()->paint_op_buffer_) {
      raw_draw_backing()->paint_op_buffer_ = sk_make_sp<cc::PaintOpBuffer>();
    }
    raw_draw_backing()->final_msaa_count_ = final_msaa_count;
    raw_draw_backing()->surface_props_ = surface_props;
    raw_draw_backing()->clear_color_ = clear_color;

    return raw_draw_backing()->paint_op_buffer_.get();
  }

  void EndWriteAccess(base::OnceClosure callback) override {
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

// Remove skia representation support when raw draw is fully supported.
std::unique_ptr<SharedImageRepresentationSkia> RawDrawBacking::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  if (context_state_->context_lost())
    return nullptr;

  DCHECK_EQ(context_state_, context_state.get());
  return std::make_unique<RepresentationSkia>(manager, this, tracker);
}

std::unique_ptr<SharedImageRepresentationRaster> RawDrawBacking::ProduceRaster(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  return std::make_unique<RepresentationRaster>(manager, this, tracker);
}

}  // namespace

SharedImageBackingFactoryRawDraw::SharedImageBackingFactoryRawDraw(
    scoped_refptr<SharedContextState> context_state)
    : context_state_(std::move(context_state)) {}

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
      base::PassKey<SharedImageBackingFactoryRawDraw>(), mailbox, format, size,
      color_space, surface_origin, alpha_type, usage, /*estimated_size=*/0,
      context_state_);
  if (!texture->Initialize())
    return nullptr;
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

}  // namespace raster
}  // namespace gpu
