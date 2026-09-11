// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_WEBGPU_SHARED_IMAGE_CACHE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_WEBGPU_SHARED_IMAGE_CACHE_H_

#include <optional>

#include "base/functional/function_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/webgpu_interface.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/blink/renderer/platform/instrumentation/canvas_memory_dump_provider.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

class SkPixmap;

namespace cc {
class PaintCanvas;
}  // namespace cc

namespace gpu {
namespace raster {
class RasterInterface;
}  // namespace raster
}  // namespace gpu

namespace blink {

class WebGraphicsContext3DProviderWrapper;
class WebGpuSharedImageLease;

class PLATFORM_EXPORT WebGpuSharedImageCache final
    : public CanvasMemoryDumpClient {
 public:
  struct PLATFORM_EXPORT Resource {
    Resource(scoped_refptr<gpu::ClientSharedImage> shared_image,
             const gpu::SyncToken& sync_token,
             base::WeakPtr<WebGraphicsContext3DProviderWrapper>
                 context_provider_wrapper);
    Resource(Resource&& that) noexcept;
    Resource(const Resource&) = delete;
    Resource& operator=(const Resource&) = delete;
    Resource& operator=(Resource&& that) noexcept;
    ~Resource();

    scoped_refptr<gpu::ClientSharedImage> shared_image_;
    gpu::SyncToken sync_token_;
    bool is_cleared_ = false;
    base::WeakPtr<WebGraphicsContext3DProviderWrapper>
        context_provider_wrapper_;
    unsigned int timer_id_ = 0;
    size_t resource_size_ = 0;
  };
  explicit WebGpuSharedImageCache(
      base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~WebGpuSharedImageCache();

  // CanvasMemoryDumpClient implementation.
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd) override;
  size_t GetSize() const override;

  std::unique_ptr<WebGpuSharedImageLease> LeaseSharedImage(
      viz::SharedImageFormat format,
      gfx::Size size,
      const gfx::ColorSpace& color_space,
      SkAlphaType alpha_type);

  // When the lease is destroyed, move the shared image to
  // |unused_resources_| if the cache is not full.
  void ReturnResource(Resource resource);

  wtf_size_t CleanUpResourcesAndReturnSizeForTesting();

  int GetWaitCountBeforeDeletionForTesting() {
    return kTimerIdDeltaForDeletion;
  }

 private:
  // The maximum size of unused shared images, 128 MB.
  static constexpr int kMaxSharedImageCacheInKB = 128 * 1024;
  static constexpr int kMaxSharedImageCacheInBytes =
      kMaxSharedImageCacheInKB * 1024;

  // A resource is deleted from the cache if it's not reused after this delay.
  static constexpr int kCleanUpDelayInSeconds = 2;

  // The duration set to the resource clean-up timer function.
  // Because the resource clean-up function runs every kCleanUpDelayInSeconds
  // and the stale resource can only be deleted in the call to
  // ReleaseStaleResources(). The actually delay could be as long as
  // (kCleanUpDelayInSeconds + kCleanUpDelayInSeconds).
  static constexpr int kTimerDurationInSeconds = 1;

  // The time it takes to increase the Timer Id by this delta is equivalent to
  // kCleanUpDelayInSeconds.
  static constexpr int kTimerIdDeltaForDeletion =
      kCleanUpDelayInSeconds / kTimerDurationInSeconds;

  using DequeResource = Deque<Resource>;

  // Search |unused_resources_| and acquire the cached resource
  // with the same cache key for reuse.
  std::optional<Resource> AcquireCachedResource(
      const gfx::Size& size,
      const viz::SharedImageFormat& format,
      SkAlphaType alpha_type,
      const gfx::ColorSpace& color_space);

  // Release the stale resources which are recycled before the last clean-up.
  void ReleaseStaleResources();

  // Start the clean-up function runs when there are unused resources.
  void StartResourceCleanUpTimer();

  // This is the place to keep the unused resources.
  // They are waiting to be used. MRU is in the front of the deque.
  DequeResource unused_resources_;

  uint64_t total_unused_resources_in_bytes_ = 0;

  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::RepeatingCallback<void()> timer_func_;

  // This ensures only one timer task is scheduled.
  bool timer_is_running_ = false;

  // |current_timer_id_| increases by 1 when the clean-up timer function is
  // called. This id is saved in Resource when the resource is recycled and is
  // checked later to determine whether this resource is stale.
  unsigned int current_timer_id_ = 0;

  THREAD_CHECKER(thread_checker_);
  base::WeakPtr<WebGpuSharedImageCache> weak_ptr_;
  base::WeakPtrFactory<WebGpuSharedImageCache> weak_ptr_factory_{this};
};

class PLATFORM_EXPORT WebGpuSharedImageLease final
    : public CanvasMemoryDumpClient {
 public:
  using Resource = WebGpuSharedImageCache::Resource;

  WebGpuSharedImageLease(Resource resource,
                         base::WeakPtr<WebGpuSharedImageCache> cache);

  ~WebGpuSharedImageLease();

  scoped_refptr<gpu::ClientSharedImage> GetSharedImage() const;
  gpu::SyncToken GetSyncToken() const;

  bool UploadToBackingSharedImage(const SkPixmap& pixmap,
                                  uint32_t src_x,
                                  uint32_t src_y);

  void DrawToBackingSharedImage(
      base::FunctionRef<void(cc::PaintCanvas&)> draw_callback);


  std::optional<gpu::SyncToken> CopyToBackingSharedImage(
      const scoped_refptr<gpu::ClientSharedImage>& shared_image,
      uint32_t src_x,
      uint32_t src_y,
      const gpu::SyncToken& ready_sync_token);

  void WaitSyncToken(const gpu::SyncToken& sync_token);

  // CanvasMemoryDumpClient implementation.
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd) override;
  size_t GetSize() const override;

 private:
  gpu::raster::RasterInterface* RasterInterface() const;
  bool IsGpuContextLost() const;

  Resource resource_;
  base::WeakPtr<WebGpuSharedImageCache> cache_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_WEBGPU_SHARED_IMAGE_CACHE_H_
