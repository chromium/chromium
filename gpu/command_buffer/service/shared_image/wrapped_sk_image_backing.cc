// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/wrapped_sk_image_backing.h"

#include <utility>

#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"

namespace gpu {

class WrappedSkImageBacking::SkiaImageRepresentationImpl
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
    if (!surface) {
      return {};
    }
    [[maybe_unused]] int save_count = surface->getCanvas()->save();
    DCHECK_EQ(1, save_count);
    write_surface_ = surface;
    return {surface};
  }

  std::vector<sk_sp<SkPromiseImageTexture>> BeginWriteAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) override {
    auto promise_texture = wrapped_sk_image()->GetPromiseTexture();
    if (!promise_texture) {
      return {};
    }
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
    auto promise_texture = wrapped_sk_image()->GetPromiseTexture();
    if (!promise_texture) {
      return {};
    }
    return {promise_texture};
  }

  void EndReadAccess() override { DCHECK(!write_surface_); }

  bool SupportsMultipleConcurrentReadAccess() override { return true; }

 private:
  WrappedSkImageBacking* wrapped_sk_image() {
    return static_cast<WrappedSkImageBacking*>(backing());
  }

  sk_sp<SkSurface> write_surface_;
  scoped_refptr<SharedContextState> context_state_;
};

WrappedSkImageBacking::WrappedSkImageBacking(
    base::PassKey<WrappedSkImageBackingFactory>,
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    scoped_refptr<SharedContextState> context_state,
    const bool thread_safe)
    : ClearTrackingSharedImageBacking(mailbox,
                                      format,
                                      size,
                                      color_space,
                                      surface_origin,
                                      alpha_type,
                                      usage,
                                      format.EstimatedSizeInBytes(size),
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

WrappedSkImageBacking::~WrappedSkImageBacking() {
  auto destroy_resources = [](scoped_refptr<SharedContextState> context_state,
                              sk_sp<SkPromiseImageTexture> promise_texture,
                              GrBackendTexture backend_texture) {
    context_state->MakeCurrent(nullptr);

    // Note that if we fail to initialize this backing, |promise_texture| will
    // not be created and hence could be null while backing is destroyed after
    // a failed init.
    if (promise_texture) {
      context_state->EraseCachedSkSurface(promise_texture.get());
    }
    promise_texture.reset();

    if (backend_texture.isValid()) {
      DeleteGrBackendTexture(context_state.get(), &backend_texture);
    }

    if (!context_state->context_lost()) {
      context_state->set_need_context_state_reset(true);
    }
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

bool WrappedSkImageBacking::Initialize() {
  // MakeCurrent to avoid destroying another client's state because Skia may
  // change GL state to create and upload textures (crbug.com/1095679).
  if (!context_state_->MakeCurrent(nullptr)) {
    return false;
  }
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

bool WrappedSkImageBacking::InitializeWithData(base::span<const uint8_t> pixels,
                                               size_t stride) {
  DCHECK(pixels.data());
  // MakeCurrent to avoid destroying another client's state because Skia may
  // change GL state to create and upload textures (crbug.com/1095679).
  if (!context_state_->MakeCurrent(nullptr)) {
    return false;
  }
  context_state_->set_need_context_state_reset(true);

  if (format().IsCompressed()) {
    backend_texture_ =
        context_state_->gr_context()->createCompressedBackendTexture(
            size().width(), size().height(), SkImage::kETC1_CompressionType,
            pixels.data(), pixels.size(), GrMipMapped::kNo, GrProtected::kNo);
  } else {
    auto info = AsSkImageInfo();
    if (!stride) {
      stride = info.minRowBytes();
    }
    SkPixmap pixmap(info, pixels.data(), stride);
    const std::string label =
        "WrappedSkImageBackingFactory_InitializeWithData" +
        CreateLabelForSharedImageUsage(usage());
    backend_texture_ = context_state_->gr_context()->createBackendTexture(
        pixmap, GrRenderable::kYes, GrProtected::kNo, nullptr, nullptr, label);
  }

  if (!backend_texture_.isValid()) {
    return false;
  }

  SetCleared();

  promise_texture_ = SkPromiseImageTexture::Make(backend_texture_);

  // Note that if the backing is meant to be thread safe (when DrDc and Vulkan
  // is enabled), we need to do additional submit here in order to send the
  // gpu commands in the correct order as per sync token dependencies. For eg
  // tapping a tab tile creates a WrappedSkImageBacking mailbox with the the
  // pixel data in LayerTreeHostImpl::CreateUIResource() which was showing
  // corrupt data without this added synchronization.
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

SharedImageBackingType WrappedSkImageBacking::GetType() const {
  return SharedImageBackingType::kWrappedSkImage;
}

void WrappedSkImageBacking::Update(std::unique_ptr<gfx::GpuFence> in_fence) {
  NOTREACHED();
}

bool WrappedSkImageBacking::UploadFromMemory(
    const std::vector<SkPixmap>& pixmaps) {
  DCHECK_EQ(pixmaps.size(), 1u);

  if (context_state_->context_lost()) {
    return false;
  }

  DCHECK(context_state_->IsCurrent(nullptr));

  return context_state_->gr_context()->updateBackendTexture(
      backend_texture_, &pixmaps[0], /*numLevels=*/1, nullptr, nullptr);
}

sk_sp<SkPromiseImageTexture> WrappedSkImageBacking::GetPromiseTexture() {
  return promise_texture_;
}

SkColorType WrappedSkImageBacking::GetSkColorType() {
  return viz::ToClosestSkColorType(/*gpu_compositing=*/true, format());
}

sk_sp<SkSurface> WrappedSkImageBacking::GetSkSurface(
    int final_msaa_count,
    const SkSurfaceProps& surface_props,
    scoped_refptr<SharedContextState> context_state) {
  // This method should only be called on the same thread on which this
  // backing is created on. Hence adding a dcheck on context_state to ensure
  // this.
  DCHECK_EQ(context_state_, context_state);
  if (context_state_->context_lost()) {
    return nullptr;
  }
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

bool WrappedSkImageBacking::SkSurfaceUnique(
    scoped_refptr<SharedContextState> context_state) {
  // This method should only be called on the same thread on which this
  // backing is created on. Hence adding a dcheck on context_state to ensure
  // this.
  DCHECK_EQ(context_state_, context_state);
  DCHECK(promise_texture_);
  return context_state_->CachedSkSurfaceIsUnique(promise_texture_.get());
}

std::unique_ptr<SkiaImageRepresentation> WrappedSkImageBacking::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  if (context_state_->context_lost()) {
    return nullptr;
  }

  return std::make_unique<SkiaImageRepresentationImpl>(
      manager, this, tracker, std::move(context_state));
}

}  // namespace gpu
