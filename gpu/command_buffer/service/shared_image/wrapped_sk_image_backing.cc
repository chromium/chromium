// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/wrapped_sk_image_backing.h"

#include <utility>

#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/ipc/common/gpu_client_ids.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "third_party/skia/include/core/SkTextureCompressionType.h"
#include "third_party/skia/include/gpu/GpuTypes.h"
#include "third_party/skia/include/gpu/ganesh/SkSurfaceGanesh.h"
#include "third_party/skia/include/private/chromium/GrPromiseImageTexture.h"

namespace gpu {

namespace {

// Given a debug label string from the client, construct a string we can pass to
// debugging tools.
std::string GetLabel(const std::string& debug_label) {
  return std::string("WrappedSkImage_" + debug_label);
}

}  // namespace

class WrappedSkImageBacking::SkiaImageRepresentationImpl
    : public SkiaGaneshImageRepresentation {
 public:
  SkiaImageRepresentationImpl(SharedImageManager* manager,
                              SharedImageBacking* backing,
                              MemoryTypeTracker* tracker,
                              scoped_refptr<SharedContextState> context_state)
      : SkiaGaneshImageRepresentation(context_state->gr_context(),
                                      manager,
                                      backing,
                                      tracker),
        context_state_(std::move(context_state)) {}

  ~SkiaImageRepresentationImpl() override { DCHECK(write_surfaces_.empty()); }

  std::vector<sk_sp<SkSurface>> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      const gfx::Rect& update_rect,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<skgpu::MutableTextureState>* end_state) override {
    write_surfaces_ = wrapped_sk_image()->GetSkSurfaces(
        final_msaa_count, surface_props, context_state_);
    for (auto& surface : write_surfaces_) {
      [[maybe_unused]] int save_count = surface->getCanvas()->save();
      DCHECK_EQ(1, save_count);
    }
    return write_surfaces_;
  }

  std::vector<sk_sp<GrPromiseImageTexture>> BeginWriteAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<skgpu::MutableTextureState>* end_state) override {
    return wrapped_sk_image()->GetPromiseTextures();
  }

  void EndWriteAccess() override {
    for (auto& write_surface : write_surfaces_) {
      write_surface->getCanvas()->restoreToCount(1);
    }
    write_surfaces_.clear();

#if DCHECK_IS_ON()
    for (auto& promise_texture : wrapped_sk_image()->GetPromiseTextures()) {
      DCHECK(context_state_->CachedSkSurfaceIsUnique(promise_texture.get()));
    }
#endif
  }

  std::vector<sk_sp<GrPromiseImageTexture>> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<skgpu::MutableTextureState>* end_state) override {
    DCHECK(write_surfaces_.empty());
    return wrapped_sk_image()->GetPromiseTextures();
  }

  void EndReadAccess() override { DCHECK(write_surfaces_.empty()); }

  bool SupportsMultipleConcurrentReadAccess() override { return true; }

 private:
  WrappedSkImageBacking* wrapped_sk_image() {
    return static_cast<WrappedSkImageBacking*>(backing());
  }

  std::vector<sk_sp<SkSurface>> write_surfaces_;
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
    gpu::SharedImageUsageSet usage,
    std::string debug_label,
    scoped_refptr<SharedContextState> context_state,
    const bool thread_safe)
    : ClearTrackingSharedImageBacking(mailbox,
                                      format,
                                      size,
                                      color_space,
                                      surface_origin,
                                      alpha_type,
                                      usage,
                                      std::move(debug_label),
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
                              std::vector<TextureHolder> textures) {
    context_state->MakeCurrent(nullptr);

    for (auto& texture : textures) {
      // Note that if we fail to initialize this backing, |promise_texture|
      // will not be created and hence could be null while backing is
      // destroyed after a failed init.
      if (texture.promise_texture) {
        context_state->EraseCachedSkSurface(texture.promise_texture.get());
        texture.promise_texture.reset();
      }

      if (texture.backend_texture.isValid()) {
        DeleteGrBackendTexture(context_state.get(), &texture.backend_texture);
      }
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
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(destroy_resources, std::move(context_state_),
                                  std::move(textures_)));
  } else {
    destroy_resources(std::move(context_state_), std::move(textures_));
  }
}

bool WrappedSkImageBacking::Initialize(const std::string& debug_label) {
  DCHECK(!format().IsCompressed());

  // MakeCurrent to avoid destroying another client's state because Skia may
  // change GL state to create and upload textures (crbug.com/1095679).
  if (!context_state_->MakeCurrent(nullptr)) {
    return false;
  }
  context_state_->set_need_context_state_reset(true);

  auto mipmap = usage().Has(SHARED_IMAGE_USAGE_MIPMAP) ? skgpu::Mipmapped::kYes
                                                       : skgpu::Mipmapped::kNo;

  int num_planes = format().NumberOfPlanes();
  textures_.resize(num_planes);
  for (int plane = 0; plane < num_planes; ++plane) {
    auto& texture = textures_[plane];
    gfx::Size plane_size = format().GetPlaneSize(plane, size());

    constexpr GrRenderable is_renderable = GrRenderable::kYes;
    constexpr GrProtected is_protected = GrProtected::kNo;
#if DCHECK_IS_ON() && !BUILDFLAG(IS_LINUX)
    // Blue for single-planar and magenta-ish for multi-planar.
    SkColor4f fallback_color =
        format().is_single_plane() ? SkColors::kBlue : SkColors::kWhite;

    // Initializing to a color makes it obvious if the pixels are not properly
    // set before they are displayed (e.g. https://crbug.com/956555).
    // We don't do this on release builds because there is a slight overhead.
    // Filling blue causes slight pixel difference, so linux-ref and
    // linux-blink-ref bots cannot share the same baseline for webtest.
    // So remove this color for this call for dcheck on build for now.
    // TODO(crbug.com/40227119): add it back.
    texture.backend_texture =
        context_state_->gr_context()->createBackendTexture(
            plane_size.width(), plane_size.height(), GetSkColorType(plane),
            fallback_color, mipmap, is_renderable, is_protected, nullptr,
            nullptr, GetLabel(debug_label));

    // Call above has write operation to clear the texture, so it requires the
    // submit before texture can be accessed on the different thread.
    if (is_thread_safe()) {
      auto* gr_context = context_state_->gr_context();
      gr_context->submit();
    }

#else
    texture.backend_texture =
        context_state_->gr_context()->createBackendTexture(
            plane_size.width(), plane_size.height(), GetSkColorType(plane),
            mipmap, is_renderable, is_protected, GetLabel(debug_label));
#endif

    if (!texture.backend_texture.isValid()) {
      DLOG(ERROR) << "createBackendTexture() failed with SkColorType:"
                  << GetSkColorType(plane);
      return false;
    }

    texture.promise_texture =
        GrPromiseImageTexture::Make(texture.backend_texture);
  }

  return true;
}

bool WrappedSkImageBacking::InitializeWithData(
    const std::string& debug_label,
    base::span<const uint8_t> pixels) {
  DCHECK(format().is_single_plane());
  DCHECK(pixels.data());

  // MakeCurrent to avoid destroying another client's state because Skia may
  // change GL state to create and upload textures (crbug.com/1095679).
  if (!context_state_->MakeCurrent(nullptr)) {
    return false;
  }
  context_state_->set_need_context_state_reset(true);

  textures_.resize(1);

  {
    std::optional<gpu::raster::GrShaderCache::ScopedCacheUse> cache_use;
    // ScopedCacheUse is used to avoid the empty/invalid client id DCHECKS
    // caused while accessing GrShaderCache. Even though other clients can
    // create shared images, the context used to create the backend texture
    // here i.e. SharedContextState is the one used by display
    // compositor/OOP-R, and therefore using kDisplayCompositorClientId is the
    // right choice.
    context_state_->UseShaderCache(cache_use, gpu::kDisplayCompositorClientId);
    if (format().IsCompressed()) {
      textures_[0].backend_texture =
          context_state_->gr_context()->createCompressedBackendTexture(
              size().width(), size().height(),
              SkTextureCompressionType::kETC1_RGB8, pixels.data(),
              pixels.size(), skgpu::Mipmapped::kNo, GrProtected::kNo);
    } else {
      auto info = AsSkImageInfo();
      if (pixels.size() != info.computeMinByteSize()) {
        DLOG(ERROR) << "Invalid initial pixel data size";
        return false;
      }
      SkPixmap pixmap(info, pixels.data(), info.minRowBytes());
      textures_[0].backend_texture =
          context_state_->gr_context()->createBackendTexture(
              pixmap, GrRenderable::kYes, GrProtected::kNo, nullptr, nullptr,
              GetLabel(debug_label));
    }
  }

  if (!textures_[0].backend_texture.isValid()) {
    return false;
  }

  SetCleared();

  textures_[0].promise_texture =
      GrPromiseImageTexture::Make(textures_[0].backend_texture);

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
  NOTREACHED_IN_MIGRATION();
}

bool WrappedSkImageBacking::UploadFromMemory(
    const std::vector<SkPixmap>& pixmaps) {
  DCHECK_EQ(pixmaps.size(), textures_.size());

  if (context_state_->context_lost()) {
    return false;
  }

  DCHECK(context_state_->IsCurrent(nullptr));

  bool updated = true;
  for (size_t i = 0; i < textures_.size(); ++i) {
    updated = updated && context_state_->gr_context()->updateBackendTexture(
                             textures_[i].backend_texture, &pixmaps[i],
                             /*numLevels=*/1, nullptr, nullptr);
  }

  return updated;
}

std::vector<sk_sp<GrPromiseImageTexture>>
WrappedSkImageBacking::GetPromiseTextures() {
  std::vector<sk_sp<GrPromiseImageTexture>> promise_textures;
  promise_textures.reserve(textures_.size());
  for (auto& texture : textures_) {
    DCHECK(texture.promise_texture);
    promise_textures.push_back(texture.promise_texture);
  }
  return promise_textures;
}

SkColorType WrappedSkImageBacking::GetSkColorType(int plane_index) {
  return viz::ToClosestSkColorType(/*gpu_compositing=*/true, format(),
                                   plane_index);
}

std::vector<sk_sp<SkSurface>> WrappedSkImageBacking::GetSkSurfaces(
    int final_msaa_count,
    const SkSurfaceProps& surface_props,
    scoped_refptr<SharedContextState> context_state) {
  if (context_state->context_lost()) {
    return {};
  }
  DCHECK(context_state->IsCurrent(nullptr));

  std::vector<sk_sp<SkSurface>> surfaces;
  surfaces.reserve(textures_.size());

  if (context_state == context_state_) {
    for (int plane = 0; plane < format().NumberOfPlanes(); ++plane) {
      auto& texture = textures_[plane];
      // Note that we are using |promise_texture| as a key to the cache below
      // since it is safe to do so. |promise_texture| is not destroyed until we
      // remove the entry from the cache.
      DCHECK(texture.promise_texture);
      auto surface =
          context_state_->GetCachedSkSurface(texture.promise_texture.get());
      if (!surface || final_msaa_count != surface_msaa_count_ ||
          surface_props != surface->props()) {
        surface = SkSurfaces::WrapBackendTexture(
            context_state_->gr_context(), texture.backend_texture,
            surface_origin(), final_msaa_count, GetSkColorType(plane),
            color_space().ToSkColorSpace(), &surface_props);
        if (!surface) {
          LOG(ERROR) << "MakeFromBackendTexture() failed.";
          context_state_->EraseCachedSkSurface(texture.promise_texture.get());
          return {};
        }
        context_state_->CacheSkSurface(texture.promise_texture.get(), surface);
      }
      surfaces.push_back(std::move(surface));
    }
    surface_msaa_count_ = final_msaa_count;
  } else {
    // If we're are going to use surface on a SharedContextState that is
    // different from the one we used to create textures, we can't cache
    // SkSurfaces, so just create them always.
    for (int plane = 0; plane < format().NumberOfPlanes(); ++plane) {
      auto surface = SkSurfaces::WrapBackendTexture(
          context_state->gr_context(), textures_[plane].backend_texture,
          surface_origin(), final_msaa_count, GetSkColorType(plane),
          color_space().ToSkColorSpace(), &surface_props);
      if (!surface) {
        LOG(ERROR) << "MakeFromBackendTexture() failed.";
        return {};
      }
      surfaces.push_back(std::move(surface));
    }
  }
  return surfaces;
}

std::unique_ptr<SkiaGaneshImageRepresentation>
WrappedSkImageBacking::ProduceSkiaGanesh(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  if (context_state_->context_lost()) {
    return nullptr;
  }

  return std::make_unique<SkiaImageRepresentationImpl>(
      manager, this, tracker, std::move(context_state));
}

WrappedSkImageBacking::TextureHolder::TextureHolder() = default;
WrappedSkImageBacking::TextureHolder::TextureHolder(TextureHolder&& other) =
    default;
WrappedSkImageBacking::TextureHolder&
WrappedSkImageBacking::TextureHolder::operator=(TextureHolder&& other) =
    default;
WrappedSkImageBacking::TextureHolder::~TextureHolder() = default;

}  // namespace gpu
