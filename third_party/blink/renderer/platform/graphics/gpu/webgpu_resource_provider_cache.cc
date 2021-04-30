// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_resource_provider_cache.h"

namespace blink {

ResourceCacheKey::ResourceCacheKey(const IntSize& size,
                                   const CanvasResourceParams& params,
                                   bool is_origin_top_left)
    : size(size), params(params), is_origin_top_left(is_origin_top_left) {}

bool ResourceCacheKey::operator==(const ResourceCacheKey& other) const {
  return (size == other.size &&
          params.ColorSpace() == other.params.ColorSpace() &&
          params.GetSkColorType() == other.params.GetSkColorType() &&
          params.GetSkAlphaType() == other.params.GetSkAlphaType() &&
          is_origin_top_left == other.is_origin_top_left);
}

bool ResourceCacheKey::operator!=(const ResourceCacheKey& other) const {
  return !(*this == other);
}

RecyclableCanvasResource::RecyclableCanvasResource(
    std::unique_ptr<CanvasResourceProvider> resource_provider,
    const ResourceCacheKey& cache_key,
    base::WeakPtr<WebGPURecyclableResourceCache> cache)
    : resource_provider_(std::move(resource_provider)),
      cache_key_(cache_key),
      cache_(cache) {}

RecyclableCanvasResource::~RecyclableCanvasResource() {
  if (!resource_provider_)
    return;

  // If the cache key is converted to a different value in
  // CanvasResourceProvider creation, it will cause cache miss, such as
  // kBGRA_8888_SkColorType to kRGBA_8888_SkColorType.
  // TODO(magchen@):Remove the DCHECKs if we must create CanvasResourceProvider
  // with unsupported parameters and if it's fine to lose the cache. Or, we can
  // save the cache key in |unused_providers_| and only compare the saved cache
  // key instead of the one in CanvasResourceProvider.
  DCHECK(cache_key_.size == resource_provider_->Size());
  DCHECK(cache_key_.params.ColorSpace() ==
         resource_provider_->ColorParams().ColorSpace());
  DCHECK(cache_key_.params.GetSkColorType() ==
         resource_provider_->ColorParams().GetSkColorType());
  DCHECK(cache_key_.params.GetSkAlphaType() ==
         resource_provider_->ColorParams().GetSkAlphaType());
  DCHECK(cache_key_.is_origin_top_left ==
         resource_provider_->IsOriginTopLeft());

  if (cache_) {
    cache_->OnDestroyRecyclableResource(std::move(resource_provider_));
  }
}

WebGPURecyclableResourceCache::WebGPURecyclableResourceCache(
    gpu::webgpu::WebGPUInterface* webgpu_interface)
    : webgpu_interface_(webgpu_interface) {
  weak_ptr_ = weak_ptr_factory_.GetWeakPtr();
}

std::unique_ptr<RecyclableCanvasResource>
WebGPURecyclableResourceCache::GetOrCreateCanvasResource(
    const IntSize& size,
    const CanvasResourceParams& params,
    bool is_origin_top_left) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  const ResourceCacheKey cache_key(size, params, is_origin_top_left);
  std::unique_ptr<CanvasResourceProvider> provider =
      AcquireCachedProvider(cache_key);
  if (!provider) {
    provider = CanvasResourceProvider::CreateWebGPUImageProvider(
        size, params, is_origin_top_left);
    if (!provider)
      return nullptr;
  }

  return std::make_unique<RecyclableCanvasResource>(std::move(provider),
                                                    cache_key, weak_ptr_);
}

void WebGPURecyclableResourceCache::OnDestroyRecyclableResource(
    std::unique_ptr<CanvasResourceProvider> resource_provider) {
  // WaitSyncToken on the canvas resource.
  gpu::SyncToken finished_access_token;
  webgpu_interface_->GenUnverifiedSyncTokenCHROMIUM(
      finished_access_token.GetData());
  resource_provider->OnDestroyRecyclableCanvasResource(finished_access_token);

  // Transfer to |unused_providers_|.  MRU goes to the front.
  unused_providers_.push_front(std::move(resource_provider));

  // If the cache is full, release the LRU one.
  if (unused_providers_.size() >= capacity_) {
    unused_providers_.pop_back();
  }
}

std::unique_ptr<CanvasResourceProvider>
WebGPURecyclableResourceCache::AcquireCachedProvider(
    const ResourceCacheKey& cache_key) {
  // Loop from MRU to LRU
  DequeResourceProvider::iterator it;
  for (it = unused_providers_.begin(); it != unused_providers_.end(); ++it) {
    CanvasResourceProvider* resource_provider = it->get();
    const auto it_cache_key = ResourceCacheKey(
        resource_provider->Size(), resource_provider->ColorParams(),
        resource_provider->IsOriginTopLeft());

    if (cache_key == it_cache_key) {
      break;
    }
  }

  // Found one.
  if (it != unused_providers_.end()) {
    std::unique_ptr<CanvasResourceProvider> provider = (std::move(*it));
    // TODO(magchen@): If the cache capacity increases a lot, will erase(it)
    // becomes inefficient?
    // Remove the provider from the |unused_providers_|.
    unused_providers_.erase(it);
    provider->OnAcquireRecyclableCanvasResource();
    return provider;
  }
  return nullptr;
}

void WebGPURecyclableResourceCache::SetWebGPUInterfaceForTesting(
    gpu::webgpu::WebGPUInterface* webgpu_interface) {
  webgpu_interface_ = webgpu_interface;
}

}  // namespace blink
