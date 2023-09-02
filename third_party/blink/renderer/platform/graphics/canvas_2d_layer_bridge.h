/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_2D_LAYER_BRIDGE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_2D_LAYER_BRIDGE_H_

#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/numerics/checked_math.h"
#include "base/rand_util.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/memory_dump_provider.h"
#include "base/types/optional_util.h"
#include "build/build_config.h"
#include "cc/layers/texture_layer_client.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_host.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"

struct SkImageInfo;

namespace cc {
class Layer;
class TextureLayer;
}  // namespace cc

namespace blink {

class Canvas2DLayerBridgeTest;
class SharedContextRateLimiter;
class StaticBitmapImage;

// All the fields are main-thread only. See DCheckInvariant() for invariants.
class PLATFORM_EXPORT HibernationHandler {
 public:
  ~HibernationHandler();
  // Semi-arbitrary threshold. Some past experiments (e.g. tile discard) have
  // shown that taking action after 5 minutes has a positive impact on memory,
  // and a minimal impact on tab switching latency (and on needless
  // compression).
  static constexpr base::TimeDelta kBeforeCompressionDelay = base::Minutes(5);

  void TakeHibernationImage(sk_sp<SkImage>&& image);
  // Returns the uncompressed image for this hibernation image. Does not
  // invalidate the hibernated image. Must call `Clear()` if invalidation is
  // required.
  sk_sp<SkImage> GetImage();
  // Invalidate the hibernated image.
  void Clear();

  bool IsHibernating() const {
    DCheckInvariant();
    return image_ != nullptr || encoded_ != nullptr;
  }
  bool is_encoded() const {
    DCheckInvariant();
    return encoded_ != nullptr;
  }
  size_t memory_size() const;
  size_t original_memory_size() const;
  int width() const { return width_; }
  int height() const { return height_; }

  void SetTaskRunnersForTesting(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner>
          background_thread_task_runner) {
    main_thread_task_runner_for_testing_ = main_thread_task_runner;
    background_thread_task_runner_for_testing_ = background_thread_task_runner;
  }

 private:
  struct BackgroundTaskParams final {
    BackgroundTaskParams(
        sk_sp<SkImage> image,
        uint64_t epoch,
        base::WeakPtr<HibernationHandler> weak_instance,
        scoped_refptr<base::SingleThreadTaskRunner> reply_task_runner)
        : image(image),
          epoch(epoch),
          weak_instance(weak_instance),
          reply_task_runner(reply_task_runner) {}

    BackgroundTaskParams(const BackgroundTaskParams&) = delete;
    BackgroundTaskParams& operator=(const BackgroundTaskParams&) = delete;
    ~BackgroundTaskParams() { DCHECK(IsMainThread()); }

    const sk_sp<SkImage> image;
    const uint64_t epoch;
    const base::WeakPtr<HibernationHandler> weak_instance;
    const scoped_refptr<base::SingleThreadTaskRunner> reply_task_runner;
  };

  void DCheckInvariant() const {
    DCHECK(IsMainThread());
    DCHECK(!((image_ != nullptr) && (encoded_ != nullptr)));
  }
  void OnAfterHibernation(uint64_t initial_epoch);
  static void Encode(std::unique_ptr<BackgroundTaskParams> params);
  void OnEncoded(
      std::unique_ptr<HibernationHandler::BackgroundTaskParams> params,
      sk_sp<SkData> encoded);
  scoped_refptr<base::SingleThreadTaskRunner> GetMainThreadTaskRunner() const;
  static size_t ImageMemorySize(const SkImage& image);

  // Incremented each time the canvas is hibernated.
  uint64_t epoch_ = 0;
  // Uncompressed hibernation image.
  sk_sp<SkImage> image_ = nullptr;
  // Compressed hibernation image.
  sk_sp<SkData> encoded_ = nullptr;
  scoped_refptr<base::SingleThreadTaskRunner>
      main_thread_task_runner_for_testing_;
  scoped_refptr<base::SingleThreadTaskRunner>
      background_thread_task_runner_for_testing_;
  int width_;
  int height_;
  int bytes_per_pixel_;

  base::WeakPtrFactory<HibernationHandler> weak_ptr_factory_{this};
};

// memory-infra metrics for all hibernated canvases in this process. Main thread
// only.
class PLATFORM_EXPORT HibernatedCanvasMemoryDumpProvider
    : public base::trace_event::MemoryDumpProvider {
 public:
  static HibernatedCanvasMemoryDumpProvider& GetInstance();
  void Register(HibernationHandler* handler);
  void Unregister(HibernationHandler* handler);

  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

 private:
  friend class base::NoDestructor<HibernatedCanvasMemoryDumpProvider>;
  HibernatedCanvasMemoryDumpProvider();

  base::Lock lock_;
  WTF::HashSet<HibernationHandler*> handlers_ GUARDED_BY(lock_);
};

class PLATFORM_EXPORT Canvas2DLayerBridge : public cc::TextureLayerClient {
 public:
  Canvas2DLayerBridge(const gfx::Size&, OpacityMode opacity_mode);
  Canvas2DLayerBridge(const Canvas2DLayerBridge&) = delete;
  Canvas2DLayerBridge& operator=(const Canvas2DLayerBridge&) = delete;

  ~Canvas2DLayerBridge() override;

  // cc::TextureLayerClient implementation.
  bool PrepareTransferableResource(
      cc::SharedBitmapIdRegistrar* bitmap_registrar,
      viz::TransferableResource* out_resource,
      viz::ReleaseCallback* out_release_callback) override;

  void FinalizeFrame(CanvasResourceProvider::FlushReason);
  void SetIsInHiddenPage(bool);
  void SetIsBeingDisplayed(bool);
  void SetFilterQuality(cc::PaintFlags::FilterQuality filter_quality);
  void SetHdrMetadata(const gfx::HDRMetadata& hdr_metadata);
  void DidDraw();
  void DoPaintInvalidation(const gfx::Rect& dirty_rect);
  cc::Layer* Layer();
  bool Restore();

  // virtual for unit testing
  virtual void WillOverwriteCanvas();
  virtual void DrawFullImage(const cc::PaintImage&);
  virtual void DidRestoreCanvasMatrixClipStack(cc::PaintCanvas*) {}
  virtual bool IsAccelerated() const;

  bool IsComposited() const;

  // This may recreate CanvasResourceProvider
  cc::PaintCanvas* GetPaintCanvas();
  bool IsValid();
  bool WritePixels(const SkImageInfo&,
                   const void* pixels,
                   size_t row_bytes,
                   int x,
                   int y);
  void AlwaysMeasureForTesting() { always_measure_for_testing_ = true; }
  void SetCanvasResourceHost(CanvasResourceHost* host);

  void Hibernate();
  // This is used for a memory usage experiment: frees canvas resource when
  // canvas is in an invisible tab.
  void LoseContext();
  bool IsHibernating() const { return hibernation_handler_.IsHibernating(); }

  bool HasRecordedDrawCommands() { return have_recorded_draw_commands_; }

  scoped_refptr<StaticBitmapImage> NewImageSnapshot(
      CanvasResourceProvider::FlushReason);

  cc::TextureLayer* layer_for_testing() { return layer_.get(); }

  // The values of the enum entries must not change because they are used for
  // usage metrics histograms. New values can be added to the end.
  enum HibernationEvent {
    kHibernationScheduled = 0,
    kHibernationAbortedDueToDestructionWhileHibernatePending = 1,
    // kHibernationAbortedDueToPendingDestruction = 2, (obsolete)
    kHibernationAbortedDueToVisibilityChange = 3,
    kHibernationAbortedDueGpuContextLoss = 4,
    kHibernationAbortedDueToSwitchToUnacceleratedRendering = 5,
    // kHibernationAbortedDueToAllocationFailure = 6, (obsolete)
    kHibernationAbortedDueSnapshotFailure = 7,
    kHibernationEndedNormally = 8,
    kHibernationEndedWithSwitchToBackgroundRendering = 9,
    kHibernationEndedWithFallbackToSW = 10,
    kHibernationEndedWithTeardown = 11,
    kHibernationAbortedBecauseNoSurface = 12,
    kMaxValue = kHibernationAbortedBecauseNoSurface,
  };

  class PLATFORM_EXPORT Logger {
   public:
    virtual void ReportHibernationEvent(HibernationEvent);
    virtual void DidStartHibernating() {}
    virtual ~Logger() = default;
  };

  void SetLoggerForTesting(std::unique_ptr<Logger> logger) {
    logger_ = std::move(logger);
  }
  CanvasResourceProvider* GetOrCreateResourceProvider();
  void FlushRecording(CanvasResourceProvider::FlushReason);

  cc::PaintRecord* getLastRecord() {
    return last_record_tainted_by_write_pixels_
               ? nullptr
               : base::OptionalToPtr(last_recording_);
  }

  bool HasRateLimiterForTesting();

  static bool IsHibernationEnabled();

  HibernationHandler& GetHibernationHandlerForTesting() {
    return hibernation_handler_;
  }

 private:
  friend class Canvas2DLayerBridgeTest;
  friend class CanvasRenderingContext2DTest;
  friend class HTMLCanvasPainterTestForCAP;

  bool IsHidden() { return is_hidden_; }
  bool CheckResourceProviderValid();
  CanvasResourceProvider* ResourceProvider() const;
  void ResetResourceProvider();

  void SkipQueuedDrawCommands();

  // Check if the Raster Mode is GPU and if the GPU context is not lost
  bool ShouldAccelerate() const;

  HibernationHandler hibernation_handler_;

  scoped_refptr<cc::TextureLayer> layer_;
  std::unique_ptr<SharedContextRateLimiter> rate_limiter_;
  std::unique_ptr<Logger> logger_;
  int frames_since_last_commit_ = 0;
  bool have_recorded_draw_commands_;
  bool is_hidden_;
  bool is_being_displayed_;
  bool hibernation_scheduled_ = false;
  bool always_measure_for_testing_ = false;
  bool context_lost_ = false;
  bool lose_context_in_background_ = false;
  bool lose_context_in_background_scheduled_ = false;

  // WritePixels content is not saved in recording. If a call was made to
  // WritePixels, the recording is now missing that information.
  bool last_record_tainted_by_write_pixels_ = false;

  const OpacityMode opacity_mode_;
  const gfx::Size size_;

  enum SnapshotState {
    kInitialSnapshotState,
    kDidAcquireSnapshot,
  };
  mutable SnapshotState snapshot_state_;

  void ClearPendingRasterTimers();
  void FinishRasterTimers(gpu::raster::RasterInterface*);
  struct RasterTimer {
    // The id for querying the duration of the gpu-side of the draw
    GLuint gl_query_id = 0u;

    // The duration of the CPU-side of the draw
    base::TimeDelta cpu_raster_duration;
  };

  CanvasResourceHost* resource_host_;
  viz::TransferableResource previous_frame_resource_;

  // For measuring a sample of frames for end-to-end raster time
  // Every frame has a 1% chance of being sampled
  static constexpr float kRasterMetricProbability = 0.01;
  base::MetricsSubSampler metrics_subsampler_;
  Deque<RasterTimer> pending_raster_timers_;

  absl::optional<cc::PaintRecord> last_recording_;

  base::WeakPtrFactory<Canvas2DLayerBridge> weak_ptr_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_2D_LAYER_BRIDGE_H_
