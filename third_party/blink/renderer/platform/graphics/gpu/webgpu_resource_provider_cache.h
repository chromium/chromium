// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_WEBGPU_RESOURCE_PROVIDER_CACHE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_WEBGPU_RESOURCE_PROVIDER_CACHE_H_

#include "base/threading/thread_checker.h"
#include "gpu/command_buffer/client/webgpu_interface.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

class WebGPURecyclableResourceCache;

struct ResourceCacheKey {
  ResourceCacheKey(const IntSize& size,
                   const CanvasResourceParams& params,
                   bool is_origin_top_left);
  ~ResourceCacheKey() = default;
  bool operator==(const ResourceCacheKey& other) const;
  bool operator!=(const ResourceCacheKey& other) const;

  // If we support more parameters for CreateWebGPUImageProvider(), we should
  // add them here.
  const IntSize size;
  const CanvasResourceParams params;
  const bool is_origin_top_left;
};

class PLATFORM_EXPORT RecyclableCanvasResource {
 public:
  explicit RecyclableCanvasResource(
      std::unique_ptr<CanvasResourceProvider> resource_provider,
      const ResourceCacheKey& cache_key,
      base::WeakPtr<WebGPURecyclableResourceCache> cache);

  ~RecyclableCanvasResource();

  CanvasResourceProvider* resource_provider() {
    return resource_provider_.get();
  }

 private:
  std::unique_ptr<CanvasResourceProvider> resource_provider_;
  const ResourceCacheKey cache_key_;
  base::WeakPtr<WebGPURecyclableResourceCache> cache_;
};

class PLATFORM_EXPORT WebGPURecyclableResourceCache {
 public:
  explicit WebGPURecyclableResourceCache(
      gpu::webgpu::WebGPUInterface* webgpu_interface);
  ~WebGPURecyclableResourceCache() = default;

  std::unique_ptr<RecyclableCanvasResource> GetOrCreateCanvasResource(
      const IntSize& size,
      const CanvasResourceParams& params,
      bool is_origin_top_left);

  // When the holder is destroyed, move the resource provider to
  // |unused_providers_| if the cache is not full.
  void OnDestroyRecyclableResource(
      std::unique_ptr<CanvasResourceProvider> resource_provider);

  void SetWebGPUInterfaceForTesting(
      gpu::webgpu::WebGPUInterface* webgpu_interface);

 private:
  // TODO(magchen@): Increase the size after the timer for cleaning up stale
  // resources is added.
  static constexpr wtf_size_t kMaxRecyclableResourceCaches = 4;
  using DequeResourceProvider =
      WTF::Deque<std::unique_ptr<CanvasResourceProvider>>;

  // Search |unused_providers_| and acquire the canvas resource provider with
  // the same cache key for re-use.
  std::unique_ptr<CanvasResourceProvider> AcquireCachedProvider(
      const ResourceCacheKey& cache_key);

  // This is the place to keep the unused CanvasResourceProviders. They are
  // waiting to be used. MRU is in the front of the deque.
  DequeResourceProvider unused_providers_;

  // The maximum number of unused CanvasResourceProviders that we can cached.
  const wtf_size_t capacity_ = kMaxRecyclableResourceCaches;

  gpu::webgpu::WebGPUInterface* webgpu_interface_;

  THREAD_CHECKER(thread_checker_);
  base::WeakPtr<WebGPURecyclableResourceCache> weak_ptr_;
  base::WeakPtrFactory<WebGPURecyclableResourceCache> weak_ptr_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_WEBGPU_RESOURCE_PROVIDER_CACHE_H_
