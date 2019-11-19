// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/wrapped_sk_image.h"

#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "skia/buildflags.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrTypes.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/trace_util.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "gpu/vulkan/vulkan_implementation.h"
#endif

namespace gpu {
namespace raster {

namespace {

class WrappedSkImage : public SharedImageBacking {
 public:
  ~WrappedSkImage() override {
    DCHECK(context_state_->context_lost() ||
           context_state_->IsCurrent(nullptr));
    if (!context_state_->context_lost())
      context_state_->set_need_context_state_reset(true);
  }

  // SharedImageBacking implementation.
  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) override {
    return false;
  }

  void Destroy() override {
    promise_texture_.reset();
    image_.reset();
  }

  bool IsCleared() const override { return cleared_; }

  void SetCleared() override { cleared_ = true; }

  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override {}

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

  SkColorType GetSkColorType() {
    return viz::ResourceFormatToClosestSkColorType(
        /*gpu_compositing=*/true, format());
  }

  sk_sp<SkSurface> GetSkSurface(int final_msaa_count,
                                const SkSurfaceProps& surface_props) {
    if (context_state_->context_lost())
      return nullptr;
    DCHECK(context_state_->IsCurrent(nullptr));

    return SkSurface::MakeFromBackendTexture(
        context_state_->gr_context(), image_->getBackendTexture(false),
        kTopLeft_GrSurfaceOrigin, final_msaa_count, GetSkColorType(),
        color_space().ToSkColorSpace(), &surface_props);
  }

  sk_sp<SkPromiseImageTexture> promise_texture() { return promise_texture_; }

 protected:
  std::unique_ptr<SharedImageRepresentationSkia> ProduceSkia(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;

 private:
  friend class gpu::raster::WrappedSkImageFactory;

  WrappedSkImage(const Mailbox& mailbox,
                 viz::ResourceFormat format,
                 const gfx::Size& size,
                 const gfx::ColorSpace& color_space,
                 uint32_t usage,
                 size_t estimated_size,
                 SharedContextState* context_state)
      : SharedImageBacking(mailbox,
                           format,
                           size,
                           color_space,
                           usage,
                           estimated_size,
                           false /* is_thread_safe */),
        context_state_(context_state) {
    DCHECK(!!context_state_);
  }

  bool Initialize(const SkImageInfo& info, base::span<const uint8_t> data) {
    if (context_state_->context_lost())
      return false;
    DCHECK(context_state_->IsCurrent(nullptr));

    context_state_->set_need_context_state_reset(true);

#if BUILDFLAG(ENABLE_VULKAN)
    auto is_protected = context_state_->GrContextIsVulkan() &&
                                context_state_->vk_context_provider()
                                    ->GetVulkanImplementation()
                                    ->enforce_protected_memory()
                            ? GrProtected::kYes
                            : GrProtected::kNo;
#else
    auto is_protected = GrProtected::kNo;
#endif

    if (!data.empty()) {
      if (format() == viz::ResourceFormat::ETC1) {
        auto sk_data = SkData::MakeWithCopy(data.data(), data.size());
        image_ = SkImage::MakeFromCompressed(
            context_state_->gr_context(), sk_data, size().width(),
            size().height(), SkImage::kETC1_CompressionType);
      } else {
        SkBitmap bitmap;
        if (!bitmap.installPixels(info, const_cast<uint8_t*>(data.data()),
                                  info.minRowBytes())) {
          return false;
        }
        image_ = SkImage::MakeFromBitmap(bitmap);
        // Move image to GPU
        if (image_)
          image_ = image_->makeTextureImage(context_state_->gr_context());
      }

      if (!image_)
        return false;

      OnWriteSucceeded();
    } else {
      // Initializing to bright green makes it obvious if the pixels are not
      // properly set before they are displayed (e.g. https://crbug.com/956555).
      // We don't do this on release builds because there is a slight overhead.

#if DCHECK_IS_ON()
      auto backend_texture = context_state_->gr_context()->createBackendTexture(
          size().width(), size().height(), GetSkColorType(), SkColors::kBlue,
          GrMipMapped::kNo, GrRenderable::kYes, is_protected);
#else
      auto backend_texture = context_state_->gr_context()->createBackendTexture(
          size().width(), size().height(), GetSkColorType(), GrMipMapped::kNo,
          GrRenderable::kYes, is_protected);
#endif
      image_ = SkImage::MakeFromAdoptedTexture(
          context_state_->gr_context(), backend_texture,
          GrSurfaceOrigin::kTopLeft_GrSurfaceOrigin, info.colorType(),
          info.alphaType(), color_space().ToSkColorSpace());
    }

    auto backend_texture = image_->getBackendTexture(true);
    DCHECK(backend_texture.isValid());

    promise_texture_ = SkPromiseImageTexture::Make(backend_texture);

    switch (backend_texture.backend()) {
      case GrBackendApi::kOpenGL: {
        GrGLTextureInfo tex_info;
        if (backend_texture.getGLTextureInfo(&tex_info))
          tracing_id_ = tex_info.fID;
        break;
      }
      case GrBackendApi::kVulkan: {
        GrVkImageInfo image_info;
        if (backend_texture.getVkImageInfo(&image_info))
          tracing_id_ = reinterpret_cast<uint64_t>(image_info.fImage);
        break;
      }
#if BUILDFLAG(SKIA_USE_DAWN)
      case GrBackendApi::kDawn: {
        GrDawnImageInfo image_info;
        if (backend_texture.getDawnImageInfo(&image_info))
          tracing_id_ = reinterpret_cast<uint64_t>(image_info.fTexture.Get());
        break;
      }
#endif
      default:
        NOTREACHED();
        return false;
    }

    return true;
  }

  SharedContextState* const context_state_;

  sk_sp<SkPromiseImageTexture> promise_texture_;
  sk_sp<SkImage> image_;

  bool cleared_ = false;

  uint64_t tracing_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(WrappedSkImage);
};

class WrappedSkImageRepresentation : public SharedImageRepresentationSkia {
 public:
  WrappedSkImageRepresentation(SharedImageManager* manager,
                               SharedImageBacking* backing,
                               MemoryTypeTracker* tracker)
      : SharedImageRepresentationSkia(manager, backing, tracker) {}

  ~WrappedSkImageRepresentation() override { DCHECK(!write_surface_); }

  sk_sp<SkSurface> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores) override {
    auto surface =
        wrapped_sk_image()->GetSkSurface(final_msaa_count, surface_props);
    write_surface_ = surface.get();
    return surface;
  }

  void EndWriteAccess(sk_sp<SkSurface> surface) override {
    DCHECK_EQ(surface.get(), write_surface_);
    DCHECK(surface->unique());
    write_surface_ = nullptr;
  }

  sk_sp<SkPromiseImageTexture> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores) override {
    DCHECK(!write_surface_);
    return wrapped_sk_image()->promise_texture();
  }

  void EndReadAccess() override {
    DCHECK(!write_surface_);
    // TODO(ericrk): Handle begin/end correctness checks.
  }

 private:
  WrappedSkImage* wrapped_sk_image() {
    return static_cast<WrappedSkImage*>(backing());
  }

  SkSurface* write_surface_ = nullptr;
};

}  // namespace

WrappedSkImageFactory::WrappedSkImageFactory(SharedContextState* context_state)
    : context_state_(context_state) {}

WrappedSkImageFactory::~WrappedSkImageFactory() = default;

std::unique_ptr<SharedImageBacking> WrappedSkImageFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage,
    bool is_thread_safe) {
  DCHECK(!is_thread_safe);
  return CreateSharedImage(mailbox, format, size, color_space, usage,
                           base::span<uint8_t>());
}

std::unique_ptr<SharedImageBacking> WrappedSkImageFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage,
    base::span<const uint8_t> data) {
  auto info = SkImageInfo::Make(size.width(), size.height(),
                                ResourceFormatToClosestSkColorType(
                                    /*gpu_compositing=*/true, format),
                                kOpaque_SkAlphaType);
  size_t estimated_size = info.computeMinByteSize();
  std::unique_ptr<WrappedSkImage> texture(
      new WrappedSkImage(mailbox, format, size, color_space, usage,
                         estimated_size, context_state_));
  if (!texture->Initialize(info, data))
    return nullptr;
  return texture;
}

std::unique_ptr<SharedImageBacking> WrappedSkImageFactory::CreateSharedImage(
    const Mailbox& mailbox,
    int client_id,
    gfx::GpuMemoryBufferHandle handle,
    gfx::BufferFormat buffer_format,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage) {
  NOTREACHED();
  return nullptr;
}

bool WrappedSkImageFactory::CanImportGpuMemoryBuffer(
    gfx::GpuMemoryBufferType memory_buffer_type) {
  return false;
}

std::unique_ptr<SharedImageRepresentationSkia> WrappedSkImage::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  DCHECK_EQ(context_state_, context_state.get());
  return std::make_unique<WrappedSkImageRepresentation>(manager, this, tracker);
}

}  // namespace raster
}  // namespace gpu
