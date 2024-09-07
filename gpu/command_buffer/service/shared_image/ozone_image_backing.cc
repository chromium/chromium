// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/ozone_image_backing.h"

#include <dawn/webgpu.h>

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/not_fatal_until.h"
#include "base/numerics/checked_math.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/gl_ozone_image_representation.h"
#include "gpu/command_buffer/service/shared_image/ozone_image_gl_textures_holder.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image/skia_gl_image_representation.h"
#include "gpu/command_buffer/service/shared_memory_region_wrapper.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/config/gpu_finch_features.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSemaphore.h"
#include "third_party/skia/include/private/chromium/GrPromiseImageTexture.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/scoped_make_current_unsafe.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "gpu/command_buffer/service/shared_image/skia_vk_ozone_image_representation.h"
#include "gpu/command_buffer/service/shared_image/vulkan_ozone_image_representation.h"
#include "gpu/vulkan/vulkan_image.h"
#include "gpu/vulkan/vulkan_implementation.h"
#endif  // BUILDFLAG(ENABLE_VULKAN)

#if BUILDFLAG(USE_DAWN)
#include "gpu/command_buffer/service/dawn_context_provider.h"
#include "gpu/command_buffer/service/shared_image/dawn_ozone_image_representation.h"
#include "gpu/command_buffer/service/shared_image/skia_graphite_dawn_image_representation.h"
#include "third_party/skia/include/gpu/graphite/Context.h"
#include "third_party/skia/include/gpu/graphite/Recorder.h"
#endif  // BUILDFLAG(USE_DAWN)

namespace gpu {
namespace {

size_t GetPixmapSizeInBytes(const gfx::NativePixmap& pixmap) {
  return gfx::BufferSizeForBufferFormat(pixmap.GetBufferSize(),
                                        pixmap.GetBufferFormat());
}

bool IsExoTexture(std::string_view label) {
  // TODO(crbug.com/40263319): Replace this hacky way of detecting exo shared
  // images with an explicit bool in a shared image info struct marking at as
  // exo-imported.
  return label.starts_with(gpu::kExoTextureLabelPrefix);
}

}  // namespace

class OzoneImageBacking::OverlayOzoneImageRepresentation
    : public OverlayImageRepresentation {
 public:
  OverlayOzoneImageRepresentation(SharedImageManager* manager,
                                  SharedImageBacking* backing,
                                  MemoryTypeTracker* tracker)
      : OverlayImageRepresentation(manager, backing, tracker) {}
  ~OverlayOzoneImageRepresentation() override = default;

 private:
  bool BeginReadAccess(gfx::GpuFenceHandle& acquire_fence) override {
    auto* ozone_backing = static_cast<OzoneImageBacking*>(backing());
    std::vector<gfx::GpuFenceHandle> fences;
    bool need_end_fence;
    if (!ozone_backing->BeginAccess(/*readonly=*/true, AccessStream::kOverlay,
                                    &fences, need_end_fence)) {
      return false;
    }
    // Always need an end fence when finish reading from overlays.
    DCHECK(need_end_fence);
    if (!fences.empty()) {
      DCHECK(fences.size() == 1);
      acquire_fence = std::move(fences.front());
    }
    return true;
  }
  void EndReadAccess(gfx::GpuFenceHandle release_fence) override {
    auto* ozone_backing = static_cast<OzoneImageBacking*>(backing());
    ozone_backing->EndAccess(/*readonly=*/true, AccessStream::kOverlay,
                             std::move(release_fence));
  }
};

SharedImageBackingType OzoneImageBacking::GetType() const {
  return SharedImageBackingType::kOzone;
}

void OzoneImageBacking::Update(std::unique_ptr<gfx::GpuFence> in_fence) {
  if (in_fence) {
    external_write_fence_ = in_fence->GetGpuFenceHandle().Clone();
  }
}

scoped_refptr<gfx::NativePixmap> OzoneImageBacking::GetNativePixmap() {
  return pixmap_;
}

bool OzoneImageBacking::IsImportedFromExo() {
  return imported_from_exo_;
}

gfx::GpuMemoryBufferHandle OzoneImageBacking::GetGpuMemoryBufferHandle() {
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::GpuMemoryBufferType::NATIVE_PIXMAP;
  handle.native_pixmap_handle = pixmap_->ExportHandle();
  return handle;
}

gfx::GpuMemoryBufferHandle
OzoneImageBacking::GetSinglePlaneGpuMemoryBufferHandle(uint32_t index) {
  gfx::GpuMemoryBufferHandle gmb_handle = GetGpuMemoryBufferHandle();
#if BUILDFLAG(IS_FUCHSIA)
  NOTREACHED_IN_MIGRATION() << "Cannot get single plane from GPU memory buffer";
  return gmb_handle;
#else
  DCHECK(gmb_handle.native_pixmap_handle.modifier == 0);
  auto& planes = gmb_handle.native_pixmap_handle.planes;
  DCHECK(index < planes.size());
  gfx::NativePixmapPlane plane = std::move(planes[index]);
  planes.clear();
  planes.push_back(std::move(plane));
  return gmb_handle;
#endif  // BUILDFLAG(IS_FUCHSIA)
}

std::unique_ptr<DawnImageRepresentation> OzoneImageBacking::ProduceDawn(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    const wgpu::Device& device,
    wgpu::BackendType backend_type,
    std::vector<wgpu::TextureFormat> view_formats,
    scoped_refptr<SharedContextState> context_state) {
#if BUILDFLAG(USE_DAWN)
  wgpu::TextureFormat webgpu_format = ToDawnFormat(format());
  if (webgpu_format == wgpu::TextureFormat::Undefined) {
    return nullptr;
  }

  return std::make_unique<DawnOzoneImageRepresentation>(
      manager, this, tracker, device, webgpu_format, std::move(view_formats),
      pixmap_);
#else  // !BUILDFLAG(USE_DAWN)
  return nullptr;
#endif
}

std::unique_ptr<SkiaGraphiteImageRepresentation>
OzoneImageBacking::ProduceSkiaGraphite(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  CHECK(context_state);
  CHECK(context_state->IsGraphiteDawn());
#if BUILDFLAG(SKIA_USE_DAWN)
  auto device = context_state->dawn_context_provider()->GetDevice();
  auto backend_type = context_state->dawn_context_provider()->backend_type();
  auto dawn_representation = ProduceDawn(manager, tracker, device, backend_type,
                                         /*view_formats=*/{}, context_state);
  if (!dawn_representation) {
    LOG(ERROR) << "Could not create Dawn Representation";
    return nullptr;
  }

  // Use GPU main recorder since this should only be called for
  // fulfilling Graphite promise images on GPU main thread.
  return SkiaGraphiteDawnImageRepresentation::Create(
      std::move(dawn_representation), context_state,
      context_state->gpu_main_graphite_recorder(), manager, this, tracker);
#else
  NOTREACHED_IN_MIGRATION();
  return nullptr;
#endif
}

scoped_refptr<OzoneImageGLTexturesHolder>
OzoneImageBacking::RetainGLTexturePerContextCache() {
  gl::GLContext* current_context = gl::GLContext::GetCurrent();
  if (!current_context) {
    LOG(ERROR) << "No GLContext current.";
    return nullptr;
  }

  // We have four paths now:
  // 0) The context doesn't have a stored offscreen surface meaning caching is
  // not possible.
  // 1) There is already a texture holder for the given context that can be
  // reused.
  // 2) There is no texture holder for the given context, but there a
  // texture holder can be reused if it was created by a compatible context.
  // 3) Non of the above applies, it's a new entry.

  // Case 0: caching is not possible.
  if (!current_context->default_surface()) {
    return OzoneImageGLTexturesHolder::CreateAndInitTexturesHolder(this,
                                                                   pixmap_);
  }

  // Case 1: if entry is found, reuse it.
  auto found = per_context_cached_textures_holders_.find(current_context);
  if (found != per_context_cached_textures_holders_.end()) {
    auto& holder = found->second;
    CHECK(!holder->WasContextLost());
    if (!format().PrefersExternalSampler()) {
      DCHECK_EQ(static_cast<int>(holder->GetNumberOfTextures()),
                format().NumberOfPlanes());
    }
    return holder;
  }

  // Case 2. Try to find a compatible context. There are some drivers that
  // fail when doing multiple reimport of dmas (and creating multiple textures
  // from a single image). See https://crbug.com/1498703.
  scoped_refptr<OzoneImageGLTexturesHolder> new_holder;
  const auto context_cache_pair = base::ranges::find_if(
      per_context_cached_textures_holders_.begin(),
      per_context_cached_textures_holders_.end(),
      [current_context](const auto& holder_per_context) {
        return holder_per_context.first->CanShareTexturesWithContext(
            current_context);
      });

  if (context_cache_pair != per_context_cached_textures_holders_.end()) {
    new_holder = context_cache_pair->second;
    CHECK(!new_holder->WasContextLost());
  } else {
    // Case 3. No entries found. Create a new holder.
    new_holder =
        OzoneImageGLTexturesHolder::CreateAndInitTexturesHolder(this, pixmap_);
  }

  if (!new_holder) {
    return nullptr;
  }

  // It'll be a new entry. |this| must observe current context and react on
  // context losses or destructions accordingly. Otherwise, it'll keep the
  // texture holders until it dies, which is not what we want from the resource
  // management point of view. Also, textures must be destroyed/marked with
  // context lost to avoid wrong behaviour (eg textures are reused despite a
  // context lost).
  current_context->AddObserver(this);

  auto result = per_context_cached_textures_holders_.insert(
      std::make_pair(current_context, std::move(new_holder)));
  DCHECK(result.second);

  // Notify this holder that it has been added to cache. This counter is used
  // to know if there is only one cache instance left. When the cache counter
  // is <= 1, this backing can either command to destroy the textures or mark
  // them as context lost. See ::OnGLContextLostOrDestroy. PS: while the
  // TextureHolder is refcounted, it's hard to correctly determine if there are
  // no users left. Eg. 1 cache entry and 1 image representation give 2 refs.
  result.first->second->OnAddedToCache();

  return result.first->second;
}

std::unique_ptr<GLTexturePassthroughImageRepresentation>
OzoneImageBacking::ProduceGLTexturePassthrough(SharedImageManager* manager,
                                               MemoryTypeTracker* tracker) {
  auto texture_holder = RetainGLTexturePerContextCache();
  if (!texture_holder) {
    return nullptr;
  }
  // If this holder has not been added in the cache, the image rep must manage
  // context lost by itself.
  const bool should_mark_context_lost_textures_holder_ =
      (texture_holder->GetCacheCount() == 0);
  return std::make_unique<GLTexturePassthroughOzoneImageRepresentation>(
      manager, this, tracker, std::move(texture_holder),
      should_mark_context_lost_textures_holder_);
}

std::unique_ptr<SkiaGaneshImageRepresentation>
OzoneImageBacking::ProduceSkiaGanesh(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  if (context_state->GrContextIsGL()) {
    auto gl_representation = ProduceGLTexturePassthrough(manager, tracker);
    if (!gl_representation) {
      LOG(ERROR) << "OzoneImageBacking::ProduceSkiaGanesh failed to create GL "
                    "representation";
      return nullptr;
    }
    auto skia_representation = SkiaGLImageRepresentation::Create(
        std::move(gl_representation), std::move(context_state), manager, this,
        tracker);
    if (!skia_representation) {
      LOG(ERROR) << "OzoneImageBacking::ProduceSkiaGanesh failed to create "
                    "Skia representation";
      return nullptr;
    }
    return skia_representation;
  }
  if (context_state->GrContextIsVulkan()) {
#if BUILDFLAG(ENABLE_VULKAN)
    auto* device_queue = context_state->vk_context_provider()->GetDeviceQueue();
    auto* vulkan_implementation =
        context_state->vk_context_provider()->GetVulkanImplementation();

    std::vector<std::unique_ptr<VulkanImage>> vulkan_images;
    if (format().is_single_plane() || format().PrefersExternalSampler()) {
      // For multi-planar formats that are externally sampled, we import the
      // entire GMB.
      gfx::GpuMemoryBufferHandle gmb_handle = GetGpuMemoryBufferHandle();
      auto vk_format = format().is_single_plane()
                           ? ToVkFormat(format(), /*plane_index=*/0)
                           : ToVkFormatExternalSampler(format());
      auto vulkan_image = vulkan_implementation->CreateImageFromGpuMemoryHandle(
          device_queue, std::move(gmb_handle), size(), vk_format,
          gfx::ColorSpace());
      if (!vulkan_image) {
        return nullptr;
      }
      vulkan_images.push_back(std::move(vulkan_image));
    } else {
      // For multi-planar SharedImages, we create a VkImage per plane. We
      // also need to pass the correct plane when creating the VulkanImage.
      for (int i = 0; i < format().NumberOfPlanes(); i++) {
        gfx::GpuMemoryBufferHandle gmb_handle =
            GetSinglePlaneGpuMemoryBufferHandle(i);
        gfx::Size plane_size = format().GetPlaneSize(i, size());
        VkFormat vk_format = ToVkFormat(format(), i);
        auto vulkan_image =
            vulkan_implementation->CreateImageFromGpuMemoryHandle(
                device_queue, std::move(gmb_handle), plane_size, vk_format,
                gfx::ColorSpace());
        if (!vulkan_image) {
          return nullptr;
        }
        vulkan_images.push_back(std::move(vulkan_image));
      }
    }

    return std::make_unique<SkiaVkOzoneImageRepresentation>(
        manager, this, std::move(context_state), std::move(vulkan_images),
        tracker);
#else
    NOTREACHED_IN_MIGRATION() << "Vulkan is disabled.";
    return nullptr;
#endif  // BUILDFLAG(ENABLE_VULKAN)
  }
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

std::unique_ptr<OverlayImageRepresentation> OzoneImageBacking::ProduceOverlay(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  return std::make_unique<OverlayOzoneImageRepresentation>(manager, this,
                                                           tracker);
}

OzoneImageBacking::OzoneImageBacking(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    SharedImageUsageSet usage,
    std::string debug_label,
    scoped_refptr<SharedContextState> context_state,
    scoped_refptr<gfx::NativePixmap> pixmap,
    const GpuDriverBugWorkarounds& workarounds,
    std::optional<gfx::BufferUsage> buffer_usage)
    : ClearTrackingSharedImageBacking(mailbox,
                                      format,
                                      size,
                                      color_space,
                                      surface_origin,
                                      alpha_type,
                                      usage,
                                      std::move(debug_label),
                                      GetPixmapSizeInBytes(*pixmap),
                                      false,
                                      std::move(buffer_usage)),
      pixmap_(std::move(pixmap)),
      context_state_(std::move(context_state)),
      workarounds_(workarounds),
      imported_from_exo_(IsExoTexture(this->debug_label())) {
  bool used_by_skia = usage.HasAny(SHARED_IMAGE_USAGE_RASTER_READ |
                                   SHARED_IMAGE_USAGE_RASTER_WRITE |
                                   SHARED_IMAGE_USAGE_DISPLAY_READ);
  bool used_by_gl =
      (HasGLES2ReadOrWriteUsage(usage)) ||
      (used_by_skia && context_state_->gr_context_type() == GrContextType::kGL);
  bool used_by_vulkan = used_by_skia && context_state_->gr_context_type() ==
                                            GrContextType::kVulkan;
  bool used_by_webgpu = usage.HasAny(SHARED_IMAGE_USAGE_WEBGPU_READ |
                                     SHARED_IMAGE_USAGE_WEBGPU_WRITE);
  write_streams_count_ = 0;
  if (used_by_gl)
    write_streams_count_++;  // gl can write
  if (used_by_vulkan)
    write_streams_count_++;  // vulkan can write
  if (used_by_webgpu)
    write_streams_count_++;  // webgpu can write

  if (write_streams_count_ == 1) {
    // Initialize last_write_stream_ if its a single stream for cases where
    // read happens before write eg. video decoder with usage set as SCANOUT.
    last_write_stream_ = used_by_gl ? AccessStream::kGL
                                    : (used_by_vulkan ? AccessStream::kVulkan
                                                      : AccessStream::kWebGPU);
  }
}

OzoneImageBacking::~OzoneImageBacking() {
  for (auto& [context, holder] : per_context_cached_textures_holders_) {
    context->RemoveObserver(this);
    // We only need to remove textures here. If the context was lost or
    // destroyed, there would be no entries in the cache as this is managed
    // via ::OnGLContextLostOrDestroy.
    if (holder->GetCacheCount() <= 1) {
      DestroyTexturesOnContext(holder.get(), context);
    }
    holder->OnRemovedFromCache();
  }
}

#if BUILDFLAG(ENABLE_VULKAN)
std::unique_ptr<VulkanImageRepresentation> OzoneImageBacking::ProduceVulkan(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    gpu::VulkanDeviceQueue* vulkan_device_queue,
    gpu::VulkanImplementation& vulkan_impl,
    bool needs_detiling) {
  viz::SharedImageFormat image_format = format();
  gfx::Size image_size = size();
  gfx::GpuMemoryBufferHandle gmb_handle = GetGpuMemoryBufferHandle();
  if (needs_detiling && image_format == viz::MultiPlaneFormat::kP010) {
    // This buffer is actually an MT2T buffer. MT2T is a 10-bit pixel format
    // that only occupies 1.25 bytes per element. We plumb it as P010 since
    // that's the closest existing pixel format, but import it into Vulkan as a
    // "tall" NV12 image. The height adjustment is designed to make the plane
    // size math work out, since NV12 is an 8-bit pixel format. This is how
    // downstream detiling shaders expect the VkImage to be constructed.

    CHECK(image_size.height() % 4 == 0);
    constexpr int kMT2TBppNumerator = 5;
    constexpr int kMT2TBppDenominator = 4;
    image_format = viz::MultiPlaneFormat::kNV12;
    image_format.SetPrefersExternalSampler();
    image_size =
        gfx::Size(image_size.width(), image_size.height() * kMT2TBppNumerator /
                                          kMT2TBppDenominator);
    base::CheckedNumeric<uint32_t> stride =
        gmb_handle.native_pixmap_handle.planes[0].stride;
    stride *= kMT2TBppDenominator;
    stride /= kMT2TBppNumerator;
    if (!stride.IsValid()) {
      return nullptr;
    }
    gmb_handle.native_pixmap_handle.planes[0].stride = stride.ValueOrDie();
    gmb_handle.native_pixmap_handle.planes[1].stride =
        gmb_handle.native_pixmap_handle.planes[0].stride;
    gmb_handle.native_pixmap_handle.planes[0].size = image_size.GetArea();
    gmb_handle.native_pixmap_handle.planes[1].offset = image_size.GetArea();
    gmb_handle.native_pixmap_handle.planes[1].size = image_size.GetArea() / 2;
  }
  auto vulkan_image = vulkan_impl.CreateImageFromGpuMemoryHandle(
      vulkan_device_queue, std::move(gmb_handle), image_size,
      image_format.PrefersExternalSampler()
          ? ToVkFormatExternalSampler(image_format)
          : ToVkFormatSinglePlanar(image_format),
      color_space());

  if (!vulkan_image) {
    return nullptr;
  }

  return std::make_unique<VulkanOzoneImageRepresentation>(
      manager, this, tracker, std::move(vulkan_image), vulkan_device_queue,
      vulkan_impl);
}
#endif

bool OzoneImageBacking::UploadFromMemory(const std::vector<SkPixmap>& pixmaps) {
  if (context_state_->context_lost()) {
    return false;
  }
  DCHECK(context_state_->IsCurrent(nullptr));

#if BUILDFLAG(USE_DAWN)
  if (context_state_->IsGraphiteDawn()) {
    return UploadFromMemoryGraphite(pixmaps);
  }
#endif  // BUILDFLAG(USE_DAWN)

  auto representation = ProduceSkiaGanesh(
      nullptr, context_state_->memory_type_tracker(), context_state_);
  DCHECK_EQ(pixmaps.size(), representation->NumPlanesExpected());

  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;
  // Allow uncleared access, as we manually handle clear tracking.
  auto dest_scoped_access = representation->BeginScopedWriteAccess(
      &begin_semaphores, &end_semaphores,
      SharedImageRepresentation::AllowUnclearedAccess::kYes,
      /*use_sk_surface=*/false);
  if (!dest_scoped_access) {
    return false;
  }
  if (!begin_semaphores.empty()) {
    bool result = context_state_->gr_context()->wait(
        begin_semaphores.size(), begin_semaphores.data(),
        /*deleteSemaphoresAfterWait=*/false);
    DCHECK(result);
  }

  bool written = true;
  for (int plane = 0; plane < format().NumberOfPlanes(); ++plane) {
    GrBackendTexture backend_texture =
        dest_scoped_access->promise_image_texture(plane)->backendTexture();
    if (!context_state_->gr_context()->updateBackendTexture(
            backend_texture, &pixmaps[plane],
            /*numLevels=*/1, surface_origin(), nullptr, nullptr)) {
      written = false;
    }
  }

  dest_scoped_access->ApplyBackendSurfaceEndState();
  FlushAndSubmitIfNecessary(std::move(end_semaphores), context_state_.get());
  if (written && !IsCleared()) {
    SetCleared();
  }
  return written;
}

#if BUILDFLAG(USE_DAWN)
bool OzoneImageBacking::UploadFromMemoryGraphite(
    const std::vector<SkPixmap>& pixmaps) {
  DCHECK(context_state_->IsGraphiteDawn());
  auto representation = ProduceSkiaGraphite(
      nullptr, context_state_->memory_type_tracker(), context_state_);
  DCHECK_EQ(pixmaps.size(), representation->NumPlanesExpected());

  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;
  // Allow uncleared access, as we manually handle clear tracking.
  auto dest_scoped_access = representation->BeginScopedWriteAccess(
      &begin_semaphores, &end_semaphores,
      SharedImageRepresentation::AllowUnclearedAccess::kYes,
      /*use_sk_surface=*/false);
  if (!dest_scoped_access) {
    return false;
  }
  CHECK(begin_semaphores.empty());

  bool written = true;
  for (int plane = 0; plane < format().NumberOfPlanes(); ++plane) {
    skgpu::graphite::BackendTexture backend_texture =
        dest_scoped_access->graphite_texture(plane);
    if (!context_state_->gpu_main_graphite_recorder()->updateBackendTexture(
            backend_texture, &pixmaps[plane],
            /*numLevels=*/1)) {
      written = false;
    }
  }

  auto recording = context_state_->gpu_main_graphite_recorder()->snap();
  skgpu::graphite::InsertRecordingInfo info;
  info.fRecording = recording.get();
  context_state_->graphite_context()->insertRecording(info);
  context_state_->graphite_context()->submit();

  if (written && !IsCleared()) {
    SetCleared();
  }
  return written;
}
#endif  // BUILDFLAG(USE_DAWN)

void OzoneImageBacking::FlushAndSubmitIfNecessary(
    std::vector<GrBackendSemaphore> signal_semaphores,
    SharedContextState* const shared_context_state) {
  bool sync_cpu = gpu::ShouldVulkanSyncCpuForSkiaSubmit(
      shared_context_state->vk_context_provider());
  GrFlushInfo flush_info = {};
  if (!signal_semaphores.empty()) {
    flush_info = {
        .fNumSemaphores = signal_semaphores.size(),
        .fSignalSemaphores = signal_semaphores.data(),
    };
    gpu::AddVulkanCleanupTaskForSkiaFlush(
        shared_context_state->vk_context_provider(), &flush_info);
  }

  shared_context_state->gr_context()->flush(flush_info);
  if (sync_cpu || !signal_semaphores.empty()) {
    shared_context_state->gr_context()->submit();
  }
}

bool OzoneImageBacking::BeginAccess(bool readonly,
                                    AccessStream access_stream,
                                    std::vector<gfx::GpuFenceHandle>* fences,
                                    bool& need_end_fence) {
  // Track reads and writes if not being used for concurrent read/writes.
  if (!usage().Has(SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE)) {
    if (is_write_in_progress_) {
      DLOG(ERROR) << "Unable to begin read or write access because another "
                     "write access is in progress";
      return false;
    }

    if (reads_in_progress_ && !readonly) {
      DLOG(ERROR) << "Unable to begin write access because a read access is in "
                     "progress ";
      return false;
    }

    if (readonly) {
      ++reads_in_progress_;
    } else {
      is_write_in_progress_ = true;
    }
  }

  // We don't wait for read-after-read.
  if (!readonly) {
    for (auto& [stream, fence] : read_fences_) {
      // Wait on fence only if reading from stream different than current
      // stream.
      if (stream != access_stream) {
        DCHECK(!fence.is_null());
        fences->emplace_back(std::move(fence));
      }
    }
    read_fences_.clear();
  }

  // Always wait on an `external_write_fence_` if present.
  if (!external_write_fence_.is_null()) {
    DCHECK(write_fence_.is_null());  // `write_fence_` should be null.
    // For write access we expect new `write_fence_` so we can move the
    // old fence here.
    if (!readonly)
      fences->emplace_back(std::move(external_write_fence_));
    else
      fences->emplace_back(external_write_fence_.Clone());
  }

  // If current stream is different than `last_write_stream_` then wait on that
  // stream's `write_fence_`.
  if (!write_fence_.is_null() && last_write_stream_ != access_stream) {
    DCHECK(external_write_fence_
               .is_null());  // `external_write_fence_` should be null.
    // For write access we expect new `write_fence_` so we can move the old
    // fence here.
    if (!readonly)
      fences->emplace_back(std::move(write_fence_));
    else
      fences->emplace_back(write_fence_.Clone());
  }

  if (readonly) {
    // Optimization for single write streams. Normally we need a read fence to
    // wait before write on a write stream. But if it single write stream, we
    // can skip read fence for it because there's no need to wait for fences on
    // the same stream.
    need_end_fence =
        (write_streams_count_ > 1) || (last_write_stream_ != access_stream);
  } else {
    // Log if it's a single write stream and write comes from a different stream
    // than expected (GL, Vulkan or WebGPU).
    LOG_IF(DFATAL,
           write_streams_count_ == 1 && last_write_stream_ != access_stream)
        << "Unexpected write stream: " << static_cast<int>(access_stream)
        << ", " << static_cast<int>(last_write_stream_) << ", "
        << write_streams_count_;
    // Always need end fence for multiple write streams. For single write stream
    // need an end fence for all usages except for raster using delegated
    // compositing. If the image will be used for delegated compositing, no need
    // to put fences at this moment as there are many raster tasks in the CPU gl
    // context that end up creating a big number of fences, which may have some
    // performance overhead depending on the gpu. Instead, when these images
    // will be scheduled as overlays, a single fence will be created.
    // TODO(crbug.com/40199420): this block of code shall be removed after cc is
    // able to set a single (duplicated) fence for bunch of tiles instead of
    // having the SI framework creating fences for each single message when
    // write access ends.

    // TODO(crbug.com/41495896): Implement vk fence optimization in the case of
    // raster delegation.
    const bool skip_fence_in_delegation =
        usage().Has(SHARED_IMAGE_USAGE_RASTER_DELEGATED_COMPOSITING) &&
        context_state_->GrContextIsGL();

    need_end_fence = (write_streams_count_ > 1) || !skip_fence_in_delegation;
  }

  return true;
}

void OzoneImageBacking::EndAccess(bool readonly,
                                  AccessStream access_stream,
                                  gfx::GpuFenceHandle fence) {
  // Track reads and writes if not being used for concurrent read/writes.
  if (!usage().Has(SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE)) {
    if (readonly) {
      DCHECK_GT(reads_in_progress_, 0u);
      --reads_in_progress_;
    } else {
      DCHECK(is_write_in_progress_);
      is_write_in_progress_ = false;
    }
  }

  if (readonly) {
    if (!fence.is_null()) {
      read_fences_[access_stream] = std::move(fence);
    }
  } else {
    DCHECK(!base::Contains(read_fences_, access_stream));
    write_fence_ = std::move(fence);
    last_write_stream_ = access_stream;
  }
}

void OzoneImageBacking::OnGLContextLost(gl::GLContext* context) {
  OnGLContextLostOrDestroy(context, /*mark_context_lost=*/true);
}

void OzoneImageBacking::OnGLContextWillDestroy(gl::GLContext* context) {
  OnGLContextLostOrDestroy(context, /*mark_context_lost=*/false);
}

void OzoneImageBacking::OnGLContextLostOrDestroy(gl::GLContext* context,
                                                 bool mark_context_lost) {
  auto it = per_context_cached_textures_holders_.find(context);
  CHECK(it != per_context_cached_textures_holders_.end(),
        base::NotFatalUntil::M130);

  // Given the TextureHolder can be used by N contexts (the contexts are
  // compatible with the original one that was used to create the holder), the
  // backing must ensure that the cache counter is <= 1 before it can command to
  // destroy textures or mark them as context lost.
  if (it->second->GetCacheCount() <= 1) {
    if (mark_context_lost) {
      it->second->MarkContextLost();
    } else {
      DestroyTexturesOnContext(it->second.get(), context);
    }
  }
  it->second->OnRemovedFromCache();
  per_context_cached_textures_holders_.erase(it);
  context->RemoveObserver(this);
}

void OzoneImageBacking::DestroyTexturesOnContext(
    OzoneImageGLTexturesHolder* holder,
    gl::GLContext* context) {
  std::optional<ui::ScopedMakeCurrentUnsafe> scoped_current;
  // If the context is already current, do not create a scoped one as it'll
  // release the context upon destruction.
  if (!context->IsCurrent(nullptr)) {
    scoped_current.emplace(context, context->default_surface());
    if (!scoped_current->IsContextCurrent()) {
      holder->MarkContextLost();
    }
  }
  holder->DestroyTextures();
}

}  // namespace gpu
