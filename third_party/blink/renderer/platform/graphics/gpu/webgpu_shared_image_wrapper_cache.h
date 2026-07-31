// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_WEBGPU_SHARED_IMAGE_WRAPPER_CACHE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_WEBGPU_SHARED_IMAGE_WRAPPER_CACHE_H_

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

class WebGpuSharedImageWrapper;
class WebGpuSharedImageWrapperCache;
class WebGraphicsContext3DProviderWrapper;

class PLATFORM_EXPORT WebGpuSharedImageWrapperLease final
    : public CanvasMemoryDumpClient {
 public:
  WebGpuSharedImageWrapperLease(
      std::unique_ptr<WebGpuSharedImageWrapper> shared_image_wrapper,
      base::WeakPtr<WebGpuSharedImageWrapperCache> cache);

  ~WebGpuSharedImageWrapperLease();

  WebGpuSharedImageWrapper* shared_image_wrapper() {
    return shared_image_wrapper_.get();
  }

  scoped_refptr<gpu::ClientSharedImage> GetSharedImage() const;
  gpu::SyncToken GetSyncToken() const;

  bool UploadToBackingSharedImage(const SkPixmap& pixmap,
                                  uint32_t src_x,
                                  uint32_t src_y);

  void DrawToBackingSharedImage(
      base::FunctionRef<void(cc::PaintCanvas&)> draw_callback);

  const gpu::SyncToken& acquire_sync_token() const;
  void set_release_sync_token(const gpu::SyncToken& token);

  // Invokes `overwrite_callback` with the ClientSharedImage backing this
  // instance and a SyncToken that should be waited on before writing to the
  // contents. When the callback finishes, it should return the SyncToken
  // that should be waited on to ensure that the service-side operations of the
  // overwrite have completed.
  void WriteToBackingSharedImage(
      base::FunctionRef<
          gpu::SyncToken(const scoped_refptr<gpu::ClientSharedImage>&,
                         const gpu::SyncToken&)> overwrite_callback);

  bool CopyToBackingSharedImage(
      const scoped_refptr<gpu::ClientSharedImage>& shared_image,
      uint32_t src_x,
      uint32_t src_y,
      const gpu::SyncToken& ready_sync_token,
      gpu::SyncToken& completion_sync_token);

  void SetCompletionSyncToken(const gpu::SyncToken& completion_sync_token) {
    completion_sync_token_ = completion_sync_token;
  }

  // CanvasMemoryDumpClient implementation.
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd) override;
  size_t GetSize() const override;

 private:
  gpu::raster::RasterInterface* RasterInterface() const;
  bool IsGpuContextLost() const;
  std::unique_ptr<WebGpuSharedImageWrapper> shared_image_wrapper_;
  base::WeakPtr<WebGpuSharedImageWrapperCache> cache_;
  gpu::SyncToken completion_sync_token_;
};

class PLATFORM_EXPORT WebGpuSharedImageWrapperCache final
    : public CanvasMemoryDumpClient {
 public:
  explicit WebGpuSharedImageWrapperCache(
      base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~WebGpuSharedImageWrapperCache();

  // CanvasMemoryDumpClient implementation.
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd) override;
  size_t GetSize() const override;

  std::unique_ptr<WebGpuSharedImageWrapperLease> LeaseWebGpuSharedImageWrapper(
      viz::SharedImageFormat format,
      gfx::Size size,
      const gfx::ColorSpace& color_space,
      SkAlphaType alpha_type);

  // When the lease is destroyed, move the shared image wrapper to
  // |unused_wrappers_| if the cache is not full.
  void ReturnWebGpuSharedImageWrapper(
      std::unique_ptr<WebGpuSharedImageWrapper> shared_image_wrapper,
      const gpu::SyncToken& completion_sync_token);

  wtf_size_t CleanUpResourcesAndReturnSizeForTesting();

  int GetWaitCountBeforeDeletionForTesting() {
    return kTimerIdDeltaForDeletion;
  }

 private:
  // The maximum number of unused WebGpuSharedImageWrappers size, 128 MB.
  static constexpr int kMaxSharedImageWrapperCachesInKB = 128 * 1024;
  static constexpr int kMaxSharedImageWrapperCachesInBytes =
      kMaxSharedImageWrapperCachesInKB * 1024;

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

  struct PLATFORM_EXPORT Resource {
    Resource(std::unique_ptr<WebGpuSharedImageWrapper> shared_image_wrapper,
             unsigned int timer_id,
             size_t resource_size);
    Resource(Resource&& that) noexcept;
    ~Resource();

    std::unique_ptr<WebGpuSharedImageWrapper> shared_image_wrapper_;
    unsigned int timer_id_;
    size_t resource_size_;
  };

  using DequeSharedImageWrapper = Deque<Resource>;

  // Search |unused_wrappers_| and acquire the WebGPU shared image wrapper
  // with the same cache key for reuse.
  std::unique_ptr<WebGpuSharedImageWrapper> AcquireCachedWrapper(
      const gfx::Size& size,
      const viz::SharedImageFormat& format,
      SkAlphaType alpha_type,
      const gfx::ColorSpace& color_space);

  // Release the stale resources which are recycled before the last clean-up.
  void ReleaseStaleResources();

  // Start the clean-up function runs when there are unused resources.
  void StartResourceCleanUpTimer();

  // This is the place to keep the unused WebGpuSharedImageWrappers.
  // They are waiting to be used. MRU is in the front of the deque.
  DequeSharedImageWrapper unused_wrappers_;

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
  base::WeakPtr<WebGpuSharedImageWrapperCache> weak_ptr_;
  base::WeakPtrFactory<WebGpuSharedImageWrapperCache> weak_ptr_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_WEBGPU_SHARED_IMAGE_WRAPPER_CACHE_H_
