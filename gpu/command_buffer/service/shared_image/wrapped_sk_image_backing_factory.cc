// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/wrapped_sk_image_backing_factory.h"

#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/config/gpu_finch_features.h"
#include "skia/buildflags.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrTypes.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_gl_api_implementation.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "gpu/vulkan/vulkan_implementation.h"
#endif

namespace gpu {

class WrappedSkImageBackingFactory;

namespace {

class WrappedSkImage : public ClearTrackingSharedImageBacking {
 public:
  WrappedSkImage(base::PassKey<WrappedSkImageBackingFactory>,
                 const Mailbox& mailbox,
                 viz::SharedImageFormat format,
                 const gfx::Size& size,
                 const gfx::ColorSpace& color_space,
                 GrSurfaceOrigin surface_origin,
                 SkAlphaType alpha_type,
                 uint32_t usage,
                 size_t estimated_size,
                 scoped_refptr<SharedContextState> context_state,
                 const bool thread_safe)
      : ClearTrackingSharedImageBacking(mailbox,
                                        format,
                                        size,
                                        color_space,
                                        surface_origin,
                                        alpha_type,
                                        usage,
                                        estimated_size,
                                        thread_safe),
        context_state_(std::move(context_state)) {
    DCHECK(!!context_state_);

    // If the backing is meant to be thread safe, then grab the task runner to
    // destroy the object later on same thread on which it was created on. Note
    // that SkSurface and GrBackendTexture are not thread safe and hence should
    // be destroyed on same thread on which it was created on.
    if (is_thread_safe()) {
      // If backing is thread safe, then ensure that we have a task runner to
      // destroy backing on correct thread. Webview doesn't have a task runner
      // but it uses and shares this backing on a single thread (on render
      // passes for display compositor) and DrDc is disabled on webview. Hence
      // using is_thread_safe() to grab task_runner is enough to ensure
      // correctness.
      DCHECK(base::SingleThreadTaskRunner::HasCurrentDefault());
      task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
    }
  }

  WrappedSkImage(const WrappedSkImage&) = delete;
  WrappedSkImage& operator=(const WrappedSkImage&) = delete;

  ~WrappedSkImage() override {
    auto destroy_resources = [](scoped_refptr<SharedContextState> context_state,
                                sk_sp<SkPromiseImageTexture> promise_texture,
                                GrBackendTexture backend_texture) {
      context_state->MakeCurrent(nullptr);

      // Note that if we fail to initialize this backing, |promise_texture| will
      // not be created and hence could be null while backing is destroyed after
      // a failed init.
      if (promise_texture)
        context_state->EraseCachedSkSurface(promise_texture.get());
      promise_texture.reset();

      if (backend_texture.isValid())
        DeleteGrBackendTexture(context_state.get(), &backend_texture);

      if (!context_state->context_lost())
        context_state->set_need_context_state_reset(true);
    };

    // Since the representation from this backing can be created on either gpu
    // main or drdc thread, the last representation ref and hence the backing
    // could be destroyed in any thread irrespective of the thread it was
    // created on. Hence we need to ensure that the resources are destroyed on
    // the thread they were created on.
    if (task_runner_ && !task_runner_->BelongsToCurrentThread()) {
      auto destruction_cb =
          base::BindPostTask(task_runner_, base::BindOnce(destroy_resources));
      std::move(destruction_cb)
          .Run(std::move(context_state_), std::move(promise_texture_),
               std::move(backend_texture_));
    } else {
      destroy_resources(std::move(context_state_), std::move(promise_texture_),
                        std::move(backend_texture_));
    }
  }

  // SharedImageBacking implementation.
  SharedImageBackingType GetType() const override {
    return SharedImageBackingType::kWrappedSkImage;
  }

  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override {
    NOTREACHED();
  }

  bool UploadFromMemory(const SkPixmap& pixmap) override {
    if (context_state_->context_lost())
      return false;

    DCHECK(context_state_->IsCurrent(nullptr));

    return context_state_->gr_context()->updateBackendTexture(
        backend_texture_, &pixmap, /*numLevels=*/1, nullptr, nullptr);
  }

  SkColorType GetSkColorType() {
    return viz::ToClosestSkColorType(/*gpu_compositing=*/true, format());
  }

  sk_sp<SkSurface> GetSkSurface(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      scoped_refptr<SharedContextState> context_state) {
    // This method should only be called on the same thread on which this
    // backing is created on. Hence adding a dcheck on context_state to ensure
    // this.
    DCHECK_EQ(context_state_, context_state);
    if (context_state_->context_lost())
      return nullptr;
    DCHECK(context_state_->IsCurrent(nullptr));

    // Note that we are using |promise_texture_| as a key to the cache below
    // since it is safe to do so. |promise_texture_| is not destroyed until we
    // remove the entry from the cache.
    DCHECK(promise_texture_);
    auto surface = context_state_->GetCachedSkSurface(promise_texture_.get());
    if (!surface || final_msaa_count != surface_msaa_count_ ||
        surface_props != surface->props()) {
      surface = SkSurface::MakeFromBackendTexture(
          context_state_->gr_context(), backend_texture_, surface_origin(),
          final_msaa_count, GetSkColorType(), color_space().ToSkColorSpace(),
          &surface_props);
      if (!surface) {
        LOG(ERROR) << "MakeFromBackendTexture() failed.";
        context_state_->EraseCachedSkSurface(promise_texture_.get());
        return nullptr;
      }
      surface_msaa_count_ = final_msaa_count;
      context_state_->CacheSkSurface(promise_texture_.get(), surface);
    }
    return surface;
  }

  bool SkSurfaceUnique(scoped_refptr<SharedContextState> context_state) {
    // This method should only be called on the same thread on which this
    // backing is created on. Hence adding a dcheck on context_state to ensure
    // this.
    DCHECK_EQ(context_state_, context_state);
    DCHECK(promise_texture_);
    return context_state_->CachedSkSurfaceIsUnique(promise_texture_.get());
  }

  sk_sp<SkPromiseImageTexture> promise_texture() { return promise_texture_; }

 protected:
  std::unique_ptr<SkiaImageRepresentation> ProduceSkia(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;

 private:
  friend class gpu::WrappedSkImageBackingFactory;
  class SkiaImageRepresentationImpl;

  bool Initialize() {
    // MakeCurrent to avoid destroying another client's state because Skia may
    // change GL state to create and upload textures (crbug.com/1095679).
    if (!context_state_->MakeCurrent(nullptr))
      return false;
    context_state_->set_need_context_state_reset(true);

    DCHECK(!format().IsCompressed());
    auto mipmap = usage() & SHARED_IMAGE_USAGE_MIPMAP ? GrMipMapped::kYes
                                                      : GrMipMapped::kNo;
    const std::string label = "WrappedSkImageBackingFactory_Initialize" +
                              CreateLabelForSharedImageUsage(usage());
#if DCHECK_IS_ON() && !BUILDFLAG(IS_LINUX)
    // Initializing to bright green makes it obvious if the pixels are not
    // properly set before they are displayed (e.g. https://crbug.com/956555).
    // We don't do this on release builds because there is a slight overhead.
    // Filling blue causes slight pixel difference, so linux-ref and
    // linux-blink-ref bots cannot share the same baseline for webtest.
    // So remove this color for this call for dcheck on build for now.
    // TODO(crbug.com/1330278): add it back.
    backend_texture_ = context_state_->gr_context()->createBackendTexture(
        size().width(), size().height(), GetSkColorType(), SkColors::kBlue,
        mipmap, GrRenderable::kYes, GrProtected::kNo, nullptr, nullptr, label);
#else
    backend_texture_ = context_state_->gr_context()->createBackendTexture(
        size().width(), size().height(), GetSkColorType(), mipmap,
        GrRenderable::kYes, GrProtected::kNo, label);
#endif

    if (!backend_texture_.isValid()) {
      DLOG(ERROR) << "createBackendTexture() failed with SkColorType:"
                  << GetSkColorType();
      return false;
    }

    promise_texture_ = SkPromiseImageTexture::Make(backend_texture_);

    return true;
  }

  // |pixels| optionally contains pixel data to upload to the texture. If pixel
  // data is provided and the image format is not ETC1 then |stride| is used. If
  // |stride| is non-zero then it's used as the stride, otherwise it will create
  // SkImageInfo from size() and format() and then SkImageInfo::minRowBytes() is
  // used for the stride. For ETC1 textures pixel data must be provided since
  // updating compressed textures is not supported.
  bool InitializeWithData(base::span<const uint8_t> pixels, size_t stride) {
    DCHECK(pixels.data());
    // MakeCurrent to avoid destroying another client's state because Skia may
    // change GL state to create and upload textures (crbug.com/1095679).
    if (!context_state_->MakeCurrent(nullptr))
      return false;
    context_state_->set_need_context_state_reset(true);

    if (format().IsCompressed()) {
      backend_texture_ =
          context_state_->gr_context()->createCompressedBackendTexture(
              size().width(), size().height(), SkImage::kETC1_CompressionType,
              pixels.data(), pixels.size(), GrMipMapped::kNo, GrProtected::kNo);
    } else {
      auto info = AsSkImageInfo();
      if (!stride)
        stride = info.minRowBytes();
      SkPixmap pixmap(info, pixels.data(), stride);
      const std::string label =
          "WrappedSkImageBackingFactory_InitializeWithData" +
          CreateLabelForSharedImageUsage(usage());
      backend_texture_ = context_state_->gr_context()->createBackendTexture(
          pixmap, GrRenderable::kYes, GrProtected::kNo, nullptr, nullptr,
          label);
    }

    if (!backend_texture_.isValid())
      return false;

    SetCleared();

    promise_texture_ = SkPromiseImageTexture::Make(backend_texture_);

    // Note that if the backing is meant to be thread safe (when DrDc and Vulkan
    // is enabled), we need to do additional submit here in order to send the
    // gpu commands in the correct order as per sync token dependencies. For eg
    // tapping a tab tile creates a WrappedSkImage mailbox with the the pixel
    // data in LayerTreeHostImpl::CreateUIResource() which was showing corrupt
    // data without this added synchronization.
    if (is_thread_safe()) {
      auto* gr_context = context_state_->gr_context();
      // Note that all skia calls to GrBackendTexture does not require any
      // flush() since the commands are already recorded by skia into the
      // command buffer. Hence only calling submit here since pushing data to a
      // texture will require sending commands to gpu.
      gr_context->submit();
    }

    return true;
  }

  scoped_refptr<SharedContextState> context_state_;

  GrBackendTexture backend_texture_;
  sk_sp<SkPromiseImageTexture> promise_texture_;
  int surface_msaa_count_ = 0;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

class WrappedSkImage::SkiaImageRepresentationImpl
    : public SkiaImageRepresentation {
 public:
  SkiaImageRepresentationImpl(SharedImageManager* manager,
                              SharedImageBacking* backing,
                              MemoryTypeTracker* tracker,
                              scoped_refptr<SharedContextState> context_state)
      : SkiaImageRepresentation(manager, backing, tracker),
        context_state_(std::move(context_state)) {}

  ~SkiaImageRepresentationImpl() override { DCHECK(!write_surface_); }

  std::vector<sk_sp<SkSurface>> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      const gfx::Rect& update_rect,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) override {
    auto surface = wrapped_sk_image()->GetSkSurface(
        final_msaa_count, surface_props, context_state_);
    if (!surface)
      return {};
    [[maybe_unused]] int save_count = surface->getCanvas()->save();
    DCHECK_EQ(1, save_count);
    write_surface_ = surface;
    return {surface};
  }

  std::vector<sk_sp<SkPromiseImageTexture>> BeginWriteAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) override {
    auto promise_texture = wrapped_sk_image()->promise_texture();
    if (!promise_texture)
      return {};
    return {promise_texture};
  }

  void EndWriteAccess() override {
    if (write_surface_) {
      write_surface_->getCanvas()->restoreToCount(1);
      write_surface_.reset();
      DCHECK(wrapped_sk_image()->SkSurfaceUnique(context_state_));
    }
  }

  std::vector<sk_sp<SkPromiseImageTexture>> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) override {
    DCHECK(!write_surface_);
    auto promise_texture = wrapped_sk_image()->promise_texture();
    if (!promise_texture)
      return {};
    return {promise_texture};
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

  sk_sp<SkSurface> write_surface_;
  scoped_refptr<SharedContextState> context_state_;
};

}  // namespace

WrappedSkImageBackingFactory::WrappedSkImageBackingFactory(
    scoped_refptr<SharedContextState> context_state)
    : context_state_(std::move(context_state)),
      is_drdc_enabled_(
          features::IsDrDcEnabled() &&
          !context_state_->feature_info()->workarounds().disable_drdc) {}

WrappedSkImageBackingFactory::~WrappedSkImageBackingFactory() = default;

std::unique_ptr<SharedImageBacking>
WrappedSkImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    bool is_thread_safe) {
  // Ensure that the backing is treated as thread safe only when DrDc is enabled
  // for vulkan context.
  // TODO(vikassoni): Wire |is_thread_safe| flag in remaining
  // CreateSharedImage() factory methods also. Without this flag, backing will
  // always be considered as thread safe when DrDc is enabled for vulkan mode
  // even though it might be used on a single thread (RenderPass for example).
  // That should be fine for now since we do not have/use any locks in backing.
  DCHECK(!is_thread_safe ||
         (context_state_->GrContextIsVulkan() && is_drdc_enabled_));
  size_t estimated_size =
      viz::ResourceSizes::UncheckedSizeInBytes<size_t>(size, format);
  auto texture = std::make_unique<WrappedSkImage>(
      base::PassKey<WrappedSkImageBackingFactory>(), mailbox, format, size,
      color_space, surface_origin, alpha_type, usage, estimated_size,
      context_state_,
      /*is_thread_safe=*/is_thread_safe &&
          context_state_->GrContextIsVulkan() && is_drdc_enabled_);
  if (!texture->Initialize())
    return nullptr;
  return texture;
}

std::unique_ptr<SharedImageBacking>
WrappedSkImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    base::span<const uint8_t> data) {
  size_t estimated_size =
      viz::ResourceSizes::UncheckedSizeInBytes<size_t>(size, format);
  auto texture = std::make_unique<WrappedSkImage>(
      base::PassKey<WrappedSkImageBackingFactory>(), mailbox, format, size,
      color_space, surface_origin, alpha_type, usage, estimated_size,
      context_state_, /*is_thread_safe=*/context_state_->GrContextIsVulkan() &&
                          is_drdc_enabled_);
  if (!texture->InitializeWithData(data, /*stride=*/0))
    return nullptr;
  return texture;
}

std::unique_ptr<SharedImageBacking>
WrappedSkImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    int client_id,
    gfx::GpuMemoryBufferHandle handle,
    gfx::BufferFormat buffer_format,
    gfx::BufferPlane plane,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage) {
  NOTREACHED();
  return nullptr;
}

bool WrappedSkImageBackingFactory::CanUseWrappedSkImage(
    uint32_t usage,
    GrContextType gr_context_type) const {
  // Ignore for mipmap usage.
  usage &= ~SHARED_IMAGE_USAGE_MIPMAP;
  auto kWrappedSkImageUsage =
      SHARED_IMAGE_USAGE_DISPLAY_READ | SHARED_IMAGE_USAGE_DISPLAY_WRITE |
      SHARED_IMAGE_USAGE_RASTER | SHARED_IMAGE_USAGE_OOP_RASTERIZATION |
      SHARED_IMAGE_USAGE_CPU_UPLOAD;
  return (usage & kWrappedSkImageUsage) && !(usage & ~kWrappedSkImageUsage);
}

bool WrappedSkImageBackingFactory::IsSupported(
    uint32_t usage,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    bool thread_safe,
    gfx::GpuMemoryBufferType gmb_type,
    GrContextType gr_context_type,
    base::span<const uint8_t> pixel_data) {
  if (format.is_multi_plane()) {
    return false;
  }
  // Note that this backing support thread safety only for vulkan mode because
  // the underlying vulkan resources like vulkan images can be shared across
  // multiple vulkan queues. Also note that this backing currently only supports
  // thread safety for DrDc mode where both gpu main and drdc thread uses/shared
  // a single vulkan queue to submit work and hence do not need to synchronize
  // the reads/writes using semaphores. For this backing to support thread
  // safety across multiple queues, we need to synchronize the reads/writes via
  // semaphores.
  if (thread_safe &&
      (!is_drdc_enabled_ || gr_context_type != GrContextType::kVulkan)) {
    return false;
  }

  // Currently, WrappedSkImage does not support LUMINANCE_8 format and this
  // format is used for single channel planes. See https://crbug.com/1252502 for
  // more details.
  if (format.resource_format() == viz::LUMINANCE_8) {
    return false;
  }

  if (!CanUseWrappedSkImage(usage, gr_context_type)) {
    return false;
  }
  if (gmb_type != gfx::EMPTY_BUFFER) {
    return false;
  }

  return true;
}

std::unique_ptr<SkiaImageRepresentation> WrappedSkImage::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  if (context_state_->context_lost())
    return nullptr;

  return std::make_unique<SkiaImageRepresentationImpl>(
      manager, this, tracker, std::move(context_state));
}

}  // namespace gpu
