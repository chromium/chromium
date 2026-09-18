// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_shared_image_cache.h"

#include "base/containers/adapters.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

bool IsGpuContextLost(
    WebGraphicsContext3DProviderWrapper* context_provider_wrapper) {
  if (!context_provider_wrapper) {
    return true;
  }
  auto* raster_interface =
      context_provider_wrapper->ContextProvider().RasterInterface();
  return !raster_interface ||
         raster_interface->GetGraphicsResetStatusKHR() != GL_NO_ERROR;
}

WebGpuSharedImageLease::WebGpuSharedImageLease(
    Resource resource,
    base::WeakPtr<WebGpuSharedImageCache> cache)
    : resource_(std::move(resource)), cache_(cache) {
  CanvasMemoryDumpProvider::Instance()->RegisterClient(this);
}

WebGpuSharedImageLease::~WebGpuSharedImageLease() {
  CanvasMemoryDumpProvider::Instance()->UnregisterClient(this);
  if (cache_ && resource_.shared_image_) {
    cache_->ReturnResource(std::move(resource_));
  }
}

void WebGpuSharedImageLease::WaitSyncToken(const gpu::SyncToken& sync_token) {
  if (sync_token.HasData()) {
    resource_.sync_token_ = sync_token;
    resource_.shared_image_->UpdateDestructionSyncToken(resource_.sync_token_);
  }
}

scoped_refptr<gpu::ClientSharedImage> WebGpuSharedImageLease::GetSharedImage()
    const {
  if (IsGpuContextLost()) {
    return nullptr;
  }
  return resource_.shared_image_;
}

gpu::SyncToken WebGpuSharedImageLease::GetSyncToken() const {
  if (IsGpuContextLost()) {
    return gpu::SyncToken();
  }
  return resource_.sync_token_;
}

bool WebGpuSharedImageLease::IsGpuContextLost() const {
  return ::blink::IsGpuContextLost(resource_.context_provider_wrapper_.get());
}

void WebGpuSharedImageLease::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd) {
  std::string path = base::StringPrintf("canvas/ResourceProvider_0x%" PRIXPTR,
                                        reinterpret_cast<uintptr_t>(this));

  std::string dump_name =
      base::StringPrintf("%s/CanvasResource_0x%" PRIXPTR, path.c_str(),
                         reinterpret_cast<uintptr_t>(this));
  auto* dump = pmd->CreateAllocatorDump(dump_name);
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  GetSize());

  resource_.shared_image_->OnMemoryDump(
      pmd, dump->guid(),
      static_cast<int>(gpu::TracingImportance::kClientOwner));
}

size_t WebGpuSharedImageLease::GetSize() const {
  return resource_.resource_size_;
}

WebGpuSharedImageCache::WebGpuSharedImageCache(
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : context_provider_(std::move(context_provider)),
      task_runner_(std::move(task_runner)) {
  weak_ptr_ = weak_ptr_factory_.GetWeakPtr();
  timer_func_ = blink::BindRepeating(
      &WebGpuSharedImageCache::ReleaseStaleResources, weak_ptr_);

  DCHECK_LE(kTimerDurationInSeconds, kCleanUpDelayInSeconds);
  CanvasMemoryDumpProvider::Instance()->RegisterClient(this);
}

WebGpuSharedImageCache::~WebGpuSharedImageCache() {
  CanvasMemoryDumpProvider::Instance()->UnregisterClient(this);
}

void WebGpuSharedImageCache::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd) {
  for (const auto& unused_resource : unused_resources_) {
    std::string path = base::StringPrintf(
        "canvas/ResourceProvider_0x%" PRIXPTR,
        reinterpret_cast<uintptr_t>(unused_resource.shared_image_.get()));

    std::string dump_name = base::StringPrintf(
        "%s/CanvasResource_0x%" PRIXPTR, path.c_str(),
        reinterpret_cast<uintptr_t>(unused_resource.shared_image_.get()));
    auto* dump = pmd->CreateAllocatorDump(dump_name);
    dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                    base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                    unused_resource.resource_size_);

    unused_resource.shared_image_->OnMemoryDump(
        pmd, dump->guid(),
        static_cast<int>(gpu::TracingImportance::kClientOwner));
  }
}

size_t WebGpuSharedImageCache::GetSize() const {
  return base::checked_cast<size_t>(total_unused_resources_in_bytes_);
}

std::unique_ptr<WebGpuSharedImageLease>
WebGpuSharedImageCache::LeaseSharedImage(viz::SharedImageFormat format,
                                         gfx::Size size,
                                         const gfx::ColorSpace& color_space,
                                         SkAlphaType alpha_type) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  std::optional<Resource> resource =
      AcquireCachedResource(size, format, alpha_type, color_space);
  if (!resource) {
    auto context_provider_wrapper = SharedGpuContext::ContextProviderWrapper();

    // IsGpuCompositingEnabled can re-create the context if it has been lost, do
    // this up front so that we can fail early and not expose ourselves to
    // use after free bugs (crbug.com/1126424)
    std::ignore = SharedGpuContext::IsGpuCompositingEnabled();

    // If the context is lost we don't want to re-create it here, the resulting
    // resource provider would be invalid anyway
    if (!context_provider_wrapper ||
        !context_provider_wrapper->ContextProvider().RasterInterface() ||
        context_provider_wrapper->ContextProvider().IsContextLost()) {
      return nullptr;
    }

    const auto& capabilities =
        context_provider_wrapper->ContextProvider().GetCapabilities();
    if ((size.width() < 1 || size.height() < 1 ||
         size.width() > capabilities.max_texture_size ||
         size.height() > capabilities.max_texture_size)) {
      return nullptr;
    }

#if BUILDFLAG(IS_LINUX)
    // WebGpu preferred canvas on linux is RGBA and interop (vk on gl) is
    // dependent on canvas copies being RGBA (not BGRA).
    if (format != viz::SinglePlaneFormat::kRGBA_F16) {
      format = viz::SinglePlaneFormat::kRGBA_8888;
    }
#endif

    auto* sii =
        context_provider_wrapper->ContextProvider().SharedImageInterface();
    // The SharedImages created by this cache serve as intermediate buffers to
    // import VideoFrames, canvas resources, or static bitmap images into WebGPU
    // (e.g., via CreateExternalTexture() or CopyTextureForBrowser()).
    // Data is written into these SharedImages via the raster interface
    // (requiring RASTER_WRITE), and then read/sampled by WebGPU (requiring
    // WEBGPU_READ).
    //
    // Note on other usages:
    // - WEBGPU_WRITE is currently required because FromStaticBitmapImage() in
    //   external_image_utils.cc passes wgpu::TextureUsage::CopyDst when
    //   creating the WebGPUMailboxTexture wrapper, triggering a non-readonly
    //   check in WebGPUTextureScopedAccess.
    gpu::SharedImageUsageSet shared_image_usage_flags =
        gpu::SHARED_IMAGE_USAGE_WEBGPU_READ |
        gpu::SHARED_IMAGE_USAGE_WEBGPU_WRITE |
        gpu::SHARED_IMAGE_USAGE_RASTER_WRITE;

    auto shared_image = sii->CreateSharedImage(
        {format, size, color_space, kTopLeft_GrSurfaceOrigin, alpha_type,
         shared_image_usage_flags, "WebGpuSharedImageCache"},
        gpu::kNullSurfaceHandle);

    gpu::SyncToken creation_sync_token = shared_image->creation_sync_token();
    if (creation_sync_token.HasData()) {
      shared_image->UpdateDestructionSyncToken(creation_sync_token);
    }

    if (IsGpuContextLost(context_provider_wrapper.get())) {
      return nullptr;
    }

    resource.emplace(std::move(shared_image), creation_sync_token,
                     std::move(context_provider_wrapper));
  }

  return std::make_unique<WebGpuSharedImageLease>(std::move(*resource),
                                                  weak_ptr_);
}

void WebGpuSharedImageCache::ReturnResource(Resource resource) {
  if (context_provider_) {
    resource.timer_id_ = current_timer_id_;
    total_unused_resources_in_bytes_ += resource.resource_size_;
    unused_resources_.push_front(std::move(resource));
  }

  // If the cache is full, release LRU from the back.
  while (total_unused_resources_in_bytes_ > kMaxSharedImageCacheInBytes) {
    total_unused_resources_in_bytes_ -= unused_resources_.back().resource_size_;
    unused_resources_.pop_back();
  }

  StartResourceCleanUpTimer();
}

WebGpuSharedImageCache::Resource::Resource(
    scoped_refptr<gpu::ClientSharedImage> shared_image,
    const gpu::SyncToken& sync_token,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper)
    : shared_image_(std::move(shared_image)),
      sync_token_(sync_token),
      context_provider_wrapper_(std::move(context_provider_wrapper)),
      resource_size_(
          shared_image_->format().EstimatedSizeInBytes(shared_image_->size())) {
}

WebGpuSharedImageCache::Resource::Resource(Resource&& that) noexcept = default;

WebGpuSharedImageCache::Resource& WebGpuSharedImageCache::Resource::operator=(
    Resource&& that) noexcept = default;

WebGpuSharedImageCache::Resource::~Resource() = default;

std::optional<WebGpuSharedImageCache::Resource>
WebGpuSharedImageCache::AcquireCachedResource(
    const gfx::Size& size,
    const viz::SharedImageFormat& format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space) {
  // Loop from MRU to LRU
  DequeResource::iterator it;
  for (it = unused_resources_.begin(); it != unused_resources_.end(); ++it) {
    if (it->shared_image_->size() == size &&
        it->shared_image_->format() == format &&
        it->shared_image_->alpha_type() == alpha_type &&
        it->shared_image_->color_space() == color_space) {
      break;
    }
  }

  // Found one.
  if (it != unused_resources_.end()) {
    Resource resource = std::move(*it);
    total_unused_resources_in_bytes_ -= resource.resource_size_;
    // TODO(magchen@): If the cache capacity increases a lot, will erase(it)
    // becomes inefficient?
    // Remove the resource from |unused_resources_|.
    unused_resources_.erase(it);

    return resource;
  }
  return std::nullopt;
}

void WebGpuSharedImageCache::ReleaseStaleResources() {
  timer_is_running_ = false;

  // Loop from LRU to MRU
  int stale_resource_count = 0;
  for (const auto& unused_resource : base::Reversed(unused_resources_)) {
    if ((current_timer_id_ - unused_resource.timer_id_) <
        kTimerIdDeltaForDeletion) {
      // These are the resources which are recycled and stay in the cache for
      // less than kCleanUpDelayInSeconds. They are not to be deleted this time.
      break;
    }
    stale_resource_count++;
  }

  // Delete all stale resources.
  for (int i = 0; i < stale_resource_count; ++i) {
    total_unused_resources_in_bytes_ -= unused_resources_.back().resource_size_;
    unused_resources_.pop_back();
  }

  current_timer_id_++;
  StartResourceCleanUpTimer();
}
void WebGpuSharedImageCache::StartResourceCleanUpTimer() {
  if (unused_resources_.size() > 0 && !timer_is_running_) {
    task_runner_->PostDelayedTask(FROM_HERE, timer_func_,
                                  base::Seconds(kTimerDurationInSeconds));
    timer_is_running_ = true;
  }
}

wtf_size_t WebGpuSharedImageCache::CleanUpResourcesAndReturnSizeForTesting() {
  ReleaseStaleResources();
  return unused_resources_.size();
}

}  // namespace blink
