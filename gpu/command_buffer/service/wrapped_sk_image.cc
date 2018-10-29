// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/wrapped_sk_image.h"

#include "base/hash.h"
#include "base/logging.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/raster_decoder_context_state.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrTypes.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/trace_util.h"

namespace gpu {
namespace raster {

namespace {

class WrappedSkImage : public SharedImageBacking {
 public:
  ~WrappedSkImage() override {
    DCHECK(context_state_->context_lost ||
           context_state_->context->IsCurrent(nullptr));
    if (!context_state_->context_lost)
      context_state_->need_context_state_reset = true;
  }

  // SharedImageBacking implementation.
  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) override {
    return false;
  }

  void Destroy() override {
    DCHECK(!!image_);
    image_.reset();
  }

  bool IsCleared() const override { return cleared_; }

  void SetCleared() override { cleared_ = true; }

  size_t EstimatedSize() const override { return estimated_size_; }

  void OnMemoryDump(const std::string& dump_name,
                    base::trace_event::MemoryAllocatorDump* dump,
                    base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t client_tracing_id) override {
    // Add a |service_guid| which expresses shared ownership between the
    // various GPU dumps.
    auto client_guid = GetSharedImageGUIDForTracing(mailbox());
    auto service_guid = gl::GetGLTextureServiceGUIDForTracing(tracing_id_);
    pmd->CreateSharedGlobalAllocatorDump(service_guid);
    // TODO(piman): coalesce constant with TextureManager::DumpTextureRef.
    int importance = 2;  // This client always owns the ref.

    pmd->AddOwnershipEdge(client_guid, service_guid, importance);
  }

  sk_sp<SkSurface> GetSkSurface(int final_msaa_count,
                                SkColorType color_type,
                                const SkSurfaceProps& surface_props) {
    if (context_state_->context_lost)
      return nullptr;
    DCHECK(context_state_->context->IsCurrent(context_state_->surface.get()));
    GrBackendTexture gr_texture =
        image_->getBackendTexture(/*flushPendingGrContextIO=*/true);
    DCHECK(gr_texture.isValid());
    return SkSurface::MakeFromBackendTextureAsRenderTarget(
        context_state_->gr_context.get(), gr_texture, kTopLeft_GrSurfaceOrigin,
        final_msaa_count, color_type, /*colorSpace=*/nullptr, &surface_props);
  }

  bool GetGrBackendTexture(GrBackendTexture* gr_texture) const {
    context_state_->need_context_state_reset = true;
    *gr_texture = image_->getBackendTexture(/*flushPendingGrContextIO=*/true);
    return gr_texture->isValid();
  }

 protected:
  std::unique_ptr<SharedImageRepresentationSkia> ProduceSkia(
      SharedImageManager* manager) override;

 private:
  friend class gpu::raster::WrappedSkImageFactory;

  WrappedSkImage(const Mailbox& mailbox,
                 viz::ResourceFormat format,
                 const gfx::Size& size,
                 const gfx::ColorSpace& color_space,
                 uint32_t usage,
                 raster::RasterDecoderContextState* context_state)
      : SharedImageBacking(mailbox, format, size, color_space, usage),
        context_state_(context_state) {
    DCHECK(!!context_state_);
  }

  bool Initialize() {
    if (context_state_->context_lost)
      return false;
    DCHECK(context_state_->context->IsCurrent(nullptr));

    context_state_->need_context_state_reset = true;

    SkImageInfo info = SkImageInfo::Make(
        size().width(), size().height(),
        ResourceFormatToClosestSkColorType(/*gpu_compositing=*/true, format()),
        kOpaque_SkAlphaType);
    size_t stride = info.minRowBytes();
    estimated_size_ = info.computeByteSize(stride);

    auto surface = SkSurface::MakeRenderTarget(context_state_->gr_context.get(),
                                               SkBudgeted::kNo, info);
    if (!surface)
      return false;

    image_ = surface->makeImageSnapshot();
    if (!image_ || !image_->isTextureBacked())
      return false;

    auto gr_texture =
        image_->getBackendTexture(/*flushPendingGrContextIO=*/false);
    if (!gr_texture.isValid())
      return false;

    switch (gr_texture.backend()) {
      case GrBackendApi::kOpenGL: {
        GrGLTextureInfo tex_info;
        if (gr_texture.getGLTextureInfo(&tex_info))
          tracing_id_ = tex_info.fID;
        break;
      }
      case GrBackendApi::kVulkan: {
        GrVkImageInfo image_info;
        if (gr_texture.getVkImageInfo(&image_info))
          tracing_id_ = reinterpret_cast<uint64_t>(image_info.fImage);
        break;
      }
      default:
        NOTREACHED();
        return false;
    }
    return true;
  }

  RasterDecoderContextState* const context_state_;

  sk_sp<SkImage> image_;
  uint32_t estimated_size_ = 0;
  bool cleared_ = false;

  uint64_t tracing_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(WrappedSkImage);
};

class WrappedSkImageRepresentation : public SharedImageRepresentationSkia {
 public:
  WrappedSkImageRepresentation(SharedImageManager* manager,
                               SharedImageBacking* backing)
      : SharedImageRepresentationSkia(manager, backing) {}

  ~WrappedSkImageRepresentation() override { DCHECK(!write_surface_); }

  sk_sp<SkSurface> BeginWriteAccess(
      GrContext* gr_context,
      int final_msaa_count,
      SkColorType color_type,
      const SkSurfaceProps& surface_props) override {
    auto surface = wrapped_sk_image()->GetSkSurface(final_msaa_count,
                                                    color_type, surface_props);
    write_surface_ = surface.get();
    return surface;
  }

  void EndWriteAccess(sk_sp<SkSurface> surface) override {
    DCHECK_EQ(surface.get(), write_surface_);
    DCHECK(surface->unique());
    write_surface_ = nullptr;
  }

  bool BeginReadAccess(SkColorType color_type,
                       GrBackendTexture* backend_texture) override {
    if (!wrapped_sk_image()->GetGrBackendTexture(backend_texture))
      return false;
    return true;
  }

  void EndReadAccess() override {
    // TODO(ericrk): Handle begin/end correctness checks.
  }

 private:
  WrappedSkImage* wrapped_sk_image() {
    return static_cast<WrappedSkImage*>(backing());
  }

  SkSurface* write_surface_ = nullptr;
};

}  // namespace

WrappedSkImageFactory::WrappedSkImageFactory(
    RasterDecoderContextState* context_state)
    : context_state_(context_state) {}

WrappedSkImageFactory::~WrappedSkImageFactory() = default;

std::unique_ptr<SharedImageBacking> WrappedSkImageFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage) {
  std::unique_ptr<WrappedSkImage> texture(new WrappedSkImage(
      mailbox, format, size, color_space, usage, context_state_));
  if (!texture->Initialize())
    return nullptr;
  return texture;
}

std::unique_ptr<SharedImageRepresentationSkia> WrappedSkImage::ProduceSkia(
    SharedImageManager* manager) {
  return std::make_unique<WrappedSkImageRepresentation>(manager, this);
}

}  // namespace raster
}  // namespace gpu
