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
#include "build/build_config.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_memory_region_wrapper.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "skia/buildflags.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrTypes.h"
#include "ui/gfx/buffer_format_util.h"
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

SkImageInfo MakeSkImageInfo(const gfx::Size& size, viz::ResourceFormat format) {
  return SkImageInfo::Make(size.width(), size.height(),
                           ResourceFormatToClosestSkColorType(
                               /*gpu_compositing=*/true, format),
                           kOpaque_SkAlphaType);
}

class WrappedSkImage : public ClearTrackingSharedImageBacking {
 public:
  ~WrappedSkImage() override {
    context_state_->MakeCurrent(nullptr);
    promise_texture_.reset();
    context_state_->EraseCachedSkSurface(this);

    if (backend_texture_.isValid())
      DeleteGrBackendTexture(context_state_.get(), &backend_texture_);

    if (!context_state_->context_lost())
      context_state_->set_need_context_state_reset(true);
  }

  // SharedImageBacking implementation.
  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) override {
    return false;
  }

  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override {
    if (shared_memory_wrapper_.IsValid()) {
      DCHECK(!in_fence);

      if (context_state_->context_lost())
        return;

      DCHECK(context_state_->IsCurrent(nullptr));

      SkImageInfo info = MakeSkImageInfo(size(), format());
      SkPixmap pixmap(info, shared_memory_wrapper_.GetMemory(),
                      shared_memory_wrapper_.GetStride());
      if (!context_state_->gr_context()->updateBackendTexture(
              backend_texture_, &pixmap, /*levels=*/1, nullptr, nullptr)) {
        DLOG(ERROR) << "Failed to update WrappedSkImage texture";
      }
    }
  }

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

    auto surface = context_state_->GetCachedSkSurface(this);
    if (!surface || final_msaa_count != surface_msaa_count_ ||
        surface_props != surface->props()) {
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

  sk_sp<SkPromiseImageTexture> promise_texture() { return promise_texture_; }

  const SharedMemoryRegionWrapper& shared_memory_wrapper() {
    return shared_memory_wrapper_;
  }

 protected:
  std::unique_ptr<SharedImageRepresentationSkia> ProduceSkia(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;

  std::unique_ptr<SharedImageRepresentationMemory> ProduceMemory(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;

 private:
  friend class gpu::raster::WrappedSkImageFactory;

  WrappedSkImage(const Mailbox& mailbox,
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

  bool InitializeGMB(const SkImageInfo& info,
                     SharedMemoryRegionWrapper shm_wrapper) {
    if (Initialize(info, shm_wrapper.GetMemoryAsSpan(),
                   shm_wrapper.GetStride())) {
      shared_memory_wrapper_ = std::move(shm_wrapper);
      return true;
    }
    return false;
  }

  // |pixels| optionally contains pixel data to upload to the texture. If pixel
  // data is provided and the image format is not ETC1 then |stride| is used. If
  // |stride| is non-zero then it's used as the stride, otherwise
  // SkImageInfo::minRowBytes() is used for the stride. For ETC1 textures pixel
  // data must be provided since updating compressed textures is not supported.
  bool Initialize(const SkImageInfo& info,
                  base::span<const uint8_t> pixels,
                  size_t stride) {
    if (context_state_->context_lost())
      return false;

    // MakeCurrent to avoid destroying another client's state because Skia may
    // change GL state to create and upload textures (crbug.com/1095679).
    context_state_->MakeCurrent(nullptr);
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

    if (pixels.data()) {
      if (format() == viz::ResourceFormat::ETC1) {
        backend_texture_ =
            context_state_->gr_context()->createCompressedBackendTexture(
                size().width(), size().height(), SkImage::kETC1_CompressionType,
                pixels.data(), pixels.size(), GrMipMapped::kNo, is_protected);
      } else {
        if (!stride)
          stride = info.minRowBytes();
        SkPixmap pixmap(info, pixels.data(), stride);
        backend_texture_ = context_state_->gr_context()->createBackendTexture(
            pixmap, GrRenderable::kNo, is_protected);
      }

      if (!backend_texture_.isValid())
        return false;

      SetCleared();
    } else {
      DCHECK_NE(format(), viz::ResourceFormat::ETC1);
#if DCHECK_IS_ON()
      // Initializing to bright green makes it obvious if the pixels are not
      // properly set before they are displayed (e.g. https://crbug.com/956555).
      // We don't do this on release builds because there is a slight overhead.
      backend_texture_ = context_state_->gr_context()->createBackendTexture(
          size().width(), size().height(), GetSkColorType(), SkColors::kBlue,
          GrMipMapped::kNo, GrRenderable::kYes, is_protected);
#else
      backend_texture_ = context_state_->gr_context()->createBackendTexture(
          size().width(), size().height(), GetSkColorType(), GrMipMapped::kNo,
          GrRenderable::kYes, is_protected);
#endif

      if (!backend_texture_.isValid()) {
        DLOG(ERROR) << "createBackendTexture() failed with SkColorType:"
                    << GetSkColorType();
        return false;
      }
    }

    promise_texture_ = SkPromiseImageTexture::Make(backend_texture_);

    switch (backend_texture_.backend()) {
      case GrBackendApi::kOpenGL: {
        GrGLTextureInfo tex_info;
        if (backend_texture_.getGLTextureInfo(&tex_info))
          tracing_id_ = tex_info.fID;
        break;
      }
#if defined(OS_MAC)
      case GrBackendApi::kMetal: {
        GrMtlTextureInfo image_info;
        if (backend_texture_.getMtlTextureInfo(&image_info))
          tracing_id_ = reinterpret_cast<uint64_t>(image_info.fTexture.get());
        break;
      }
#endif
      case GrBackendApi::kVulkan: {
        GrVkImageInfo image_info;
        if (backend_texture_.getVkImageInfo(&image_info))
          tracing_id_ = reinterpret_cast<uint64_t>(image_info.fImage);
        break;
      }
#if BUILDFLAG(SKIA_USE_DAWN)
      case GrBackendApi::kDawn: {
        GrDawnTextureInfo tex_info;
        if (backend_texture_.getDawnTextureInfo(&tex_info))
          tracing_id_ = reinterpret_cast<uint64_t>(tex_info.fTexture.Get());
        break;
      }
#endif
      default:
        NOTREACHED();
        return false;
    }

    return true;
  }

  scoped_refptr<SharedContextState> context_state_;

  GrBackendTexture backend_texture_;
  sk_sp<SkPromiseImageTexture> promise_texture_;
  int surface_msaa_count_ = 0;

  // Set for shared memory GMB.
  SharedMemoryRegionWrapper shared_memory_wrapper_;

  uint64_t tracing_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(WrappedSkImage);
};

class WrappedSkImageRepresentationSkia : public SharedImageRepresentationSkia {
 public:
  WrappedSkImageRepresentationSkia(SharedImageManager* manager,
                                   SharedImageBacking* backing,
                                   MemoryTypeTracker* tracker)
      : SharedImageRepresentationSkia(manager, backing, tracker) {}

  ~WrappedSkImageRepresentationSkia() override { DCHECK(!write_surface_); }

  sk_sp<SkSurface> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores) override {
    auto surface =
        wrapped_sk_image()->GetSkSurface(final_msaa_count, surface_props);
    if (!surface)
      return nullptr;
    int save_count = surface->getCanvas()->save();
    ALLOW_UNUSED_LOCAL(save_count);
    DCHECK_EQ(1, save_count);
    write_surface_ = surface.get();
    return surface;
  }

  sk_sp<SkPromiseImageTexture> BeginWriteAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) override {
    return wrapped_sk_image()->promise_texture();
  }

  void EndWriteAccess(sk_sp<SkSurface> surface) override {
    if (surface) {
      DCHECK_EQ(surface.get(), write_surface_);
      surface->getCanvas()->restoreToCount(1);
      surface.reset();
      write_surface_ = nullptr;

      DCHECK(wrapped_sk_image()->SkSurfaceUnique());
    }
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

  bool SupportsMultipleConcurrentReadAccess() override { return true; }

 private:
  WrappedSkImage* wrapped_sk_image() {
    return static_cast<WrappedSkImage*>(backing());
  }

  SkSurface* write_surface_ = nullptr;
};

class WrappedSkImageRepresentationMemory
    : public SharedImageRepresentationMemory {
 public:
  WrappedSkImageRepresentationMemory(SharedImageManager* manager,
                                     SharedImageBacking* backing,
                                     MemoryTypeTracker* tracker)
      : SharedImageRepresentationMemory(manager, backing, tracker) {}

 protected:
  SkPixmap BeginReadAccess() override {
    SkImageInfo info = MakeSkImageInfo(wrapped_sk_image()->size(),
                                       wrapped_sk_image()->format());
    return SkPixmap(info,
                    wrapped_sk_image()->shared_memory_wrapper().GetMemory(),
                    wrapped_sk_image()->shared_memory_wrapper().GetStride());
  }

 private:
  WrappedSkImage* wrapped_sk_image() {
    return static_cast<WrappedSkImage*>(backing());
  }
};

}  // namespace

WrappedSkImageFactory::WrappedSkImageFactory(
    scoped_refptr<SharedContextState> context_state)
    : context_state_(std::move(context_state)) {}

WrappedSkImageFactory::~WrappedSkImageFactory() = default;

std::unique_ptr<SharedImageBacking> WrappedSkImageFactory::CreateSharedImage(
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
  return CreateSharedImage(mailbox, format, size, color_space, surface_origin,
                           alpha_type, usage, base::span<uint8_t>());
}

std::unique_ptr<SharedImageBacking> WrappedSkImageFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    base::span<const uint8_t> data) {
  auto info = MakeSkImageInfo(size, format);
  size_t estimated_size = info.computeMinByteSize();
  std::unique_ptr<WrappedSkImage> texture(
      new WrappedSkImage(mailbox, format, size, color_space, surface_origin,
                         alpha_type, usage, estimated_size, context_state_));
  if (!texture->Initialize(info, data, /*stride=*/0))
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
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage) {
  DCHECK_EQ(handle.type, gfx::SHARED_MEMORY_BUFFER);

  if (!gpu::IsImageSizeValidForGpuMemoryBufferFormat(size, buffer_format)) {
    DLOG(ERROR) << "Invalid image size for format.";
    return nullptr;
  }

  if (gfx::NumberOfPlanesForLinearBufferFormat(buffer_format) != 1) {
    DLOG(ERROR) << "Invalid image format.";
    return nullptr;
  }

  viz::ResourceFormat format = viz::GetResourceFormat(buffer_format);

  // The Skia API to handle compressed texture is limited and not compatible
  // with updating the texture or custom strides.
  DCHECK_NE(format, viz::ResourceFormat::ETC1);

  SharedMemoryRegionWrapper shm_wrapper;
  if (!shm_wrapper.Initialize(handle, size, format))
    return nullptr;

  auto info = MakeSkImageInfo(size, format);
  std::unique_ptr<WrappedSkImage> texture(new WrappedSkImage(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      info.computeMinByteSize(), context_state_));
  if (!texture->InitializeGMB(info, std::move(shm_wrapper)))
    return nullptr;

  return texture;
}

bool WrappedSkImageFactory::CanImportGpuMemoryBuffer(
    gfx::GpuMemoryBufferType memory_buffer_type) {
  return memory_buffer_type == gfx::SHARED_MEMORY_BUFFER;
}

std::unique_ptr<SharedImageRepresentationSkia> WrappedSkImage::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  if (context_state_->context_lost())
    return nullptr;

  DCHECK_EQ(context_state_, context_state.get());
  return std::make_unique<WrappedSkImageRepresentationSkia>(manager, this,
                                                            tracker);
}

std::unique_ptr<SharedImageRepresentationMemory> WrappedSkImage::ProduceMemory(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  if (!shared_memory_wrapper_.IsValid())
    return nullptr;

  return std::make_unique<WrappedSkImageRepresentationMemory>(manager, this,
                                                              tracker);
}

}  // namespace raster
}  // namespace gpu
