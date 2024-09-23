// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_resource_provider_cache.h"

#include "base/containers/adapters.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

RecyclableCanvasResource::RecyclableCanvasResource(
    std::unique_ptr<CanvasResourceProvider> resource_provider,
    base::WeakPtr<WebGPURecyclableResourceCache> cache)
    : resource_provider_(std::move(resource_provider)), cache_(cache) {}

RecyclableCanvasResource::~RecyclableCanvasResource() {
  if (cache_ && resource_provider_) {
    cache_->OnDestroyRecyclableResource(std::move(resource_provider_),
                                        completion_sync_token_);
  }
}

WebGPURecyclableResourceCache::WebGPURecyclableResourceCache(
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : context_provider_(std::move(context_provider)),
      task_runner_(std::move(task_runner)) {
  weak_ptr_ = weak_ptr_factory_.GetWeakPtr();
  timer_func_ = WTF::BindRepeating(
      &WebGPURecyclableResourceCache::ReleaseStaleResources, weak_ptr_);

  DCHECK_LE(kTimerDurationInSeconds, kCleanUpDelayInSeconds);
}

std::unique_ptr<RecyclableCanvasResource>
WebGPURecyclableResourceCache::GetOrCreateCanvasResource(
    const SkImageInfo& info) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  std::unique_ptr<CanvasResourceProvider> provider =
      AcquireCachedProvider(info);
  if (!provider) {
    provider = CanvasResourceProvider::CreateWebGPUImageProvider(info);
    if (!provider)
      return nullptr;
  }

  return std::make_unique<RecyclableCanvasResource>(std::move(provider),
                                                    weak_ptr_);
}

void WebGPURecyclableResourceCache::OnDestroyRecyclableResource(
    std::unique_ptr<CanvasResourceProvider> resource_provider,
    const gpu::SyncToken& completion_sync_token) {
  int resource_size = resource_provider->Size().width() *
                      resource_provider->Size().height() *
                      resource_provider->GetSkImageInfo().bytesPerPixel();
  if (context_provider_) {
    total_unused_resources_in_bytes_ += resource_size;

    // WaitSyncToken on the canvas resource.
    resource_provider->OnDestroyRecyclableCanvasResource(completion_sync_token);

    unused_providers_.push_front(Resource(std::move(resource_provider),
                                          current_timer_id_, resource_size));
  }

  // If the cache is full, release LRU from the back.
  while (total_unused_resources_in_bytes_ >
         kMaxRecyclableResourceCachesInBytes) {
    total_unused_resources_in_bytes_ -= unused_providers_.back().resource_size_;
    unused_providers_.pop_back();
  }

  StartResourceCleanUpTimer();
}

WebGPURecyclableResourceCache::Resource::Resource(
    std::unique_ptr<CanvasResourceProvider> resource_provider,
    unsigned int timer_id,
    int resource_size)
    : resource_provider_(std::move(resource_provider)),
      timer_id_(timer_id),
      resource_size_(resource_size) {}

WebGPURecyclableResourceCache::Resource::Resource(Resource&& that) noexcept =
    default;

WebGPURecyclableResourceCache::Resource::~Resource() = default;

std::unique_ptr<CanvasResourceProvider>
WebGPURecyclableResourceCache::AcquireCachedProvider(
    const SkImageInfo& image_info) {
  // Loop from MRU to LRU
  DequeResourceProvider::iterator it;
  for (it = unused_providers_.begin(); it != unused_providers_.end(); ++it) {
    CanvasResourceProvider* resource_provider = it->resource_provider_.get();
    if (image_info == resource_provider->GetSkImageInfo()) {
      break;
    }
  }

  // Found one.
  if (it != unused_providers_.end()) {
    std::unique_ptr<CanvasResourceProvider> provider =
        (std::move(it->resource_provider_));
    total_unused_resources_in_bytes_ -= it->resource_size_;
    // TODO(magchen@): If the cache capacity increases a lot, will erase(it)
    // becomes inefficient?
    // Remove the provider from the |unused_providers_|.
    unused_providers_.erase(it);
    provider->OnAcquireRecyclableCanvasResource();

    return provider;
  }
  return nullptr;
}

void WebGPURecyclableResourceCache::ReleaseStaleResources() {
  timer_is_running_ = false;

  // Loop from LRU to MRU
  int stale_resource_count = 0;
  for (const auto& unused_provider : base::Reversed(unused_providers_)) {
    if ((current_timer_id_ - unused_provider.timer_id_) <
        kTimerIdDeltaForDeletion) {
      // These are the resources which are recycled and stay in the cache for
      // less than kCleanUpDelayInSeconds. They are not to be deleted this time.
      break;
    }
    stale_resource_count++;
  }

  // Delete all stale resources.
  for (int i = 0; i < stale_resource_count; ++i) {
    total_unused_resources_in_bytes_ -= unused_providers_.back().resource_size_;
    unused_providers_.pop_back();
  }

  current_timer_id_++;
  StartResourceCleanUpTimer();
}
void WebGPURecyclableResourceCache::StartResourceCleanUpTimer() {
  if (unused_providers_.size() > 0 && !timer_is_running_) {
    task_runner_->PostDelayedTask(FROM_HERE, timer_func_,
                                  base::Seconds(kTimerDurationInSeconds));
    timer_is_running_ = true;
  }
}

wtf_size_t
WebGPURecyclableResourceCache::CleanUpResourcesAndReturnSizeForTesting() {
  ReleaseStaleResources();
  return unused_providers_.size();
}

}  // namespace blink
