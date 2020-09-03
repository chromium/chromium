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
#include <random>
#include <utility>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/numerics/checked_math.h"
#include "build/build_config.h"
#include "cc/layers/texture_layer_client.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/graphics/canvas_color_params.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_host.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/color_space.h"

struct SkImageInfo;

namespace cc {
class Layer;
class TextureLayer;
}  // namespace cc

namespace blink {

class Canvas2DLayerBridgeTest;
class SharedContextRateLimiter;
class StaticBitmapImage;

#if defined(OS_MAC)
// Canvas hibernation is currently disabled on MacOS X due to a bug that causes
// content loss. TODO: Find a better fix for crbug.com/588434
#define CANVAS2D_HIBERNATION_ENABLED 0
#else
#define CANVAS2D_HIBERNATION_ENABLED 1
#endif

class PLATFORM_EXPORT Canvas2DLayerBridge : public cc::TextureLayerClient {
 public:
  Canvas2DLayerBridge(const IntSize&, RasterMode, const CanvasColorParams&);

  ~Canvas2DLayerBridge() override;

  // cc::TextureLayerClient implementation.
  bool PrepareTransferableResource(
      cc::SharedBitmapIdRegistrar* bitmap_registrar,
      viz::TransferableResource* out_resource,
      std::unique_ptr<viz::SingleReleaseCallback>* out_release_callback)
      override;

  void FinalizeFrame();
  void SetIsInHiddenPage(bool);
  void SetIsBeingDisplayed(bool);
  void SetFilterQuality(SkFilterQuality filter_quality);
  void DidDraw(const FloatRect&);
  void DoPaintInvalidation(const FloatRect& dirty_rect);
  cc::Layer* Layer();
  bool Restore();

  // virtual for unit testing
  virtual void WillOverwriteCanvas();
  virtual void DrawFullImage(const cc::PaintImage&);
  virtual void DidRestoreCanvasMatrixClipStack(cc::PaintCanvas*) {}
  virtual bool IsAccelerated() const;

  // This may recreate CanvasResourceProvider
  cc::PaintCanvas* GetPaintCanvas();
  bool IsValid();
  bool WritePixels(const SkImageInfo&,
                   const void* pixels,
                   size_t row_bytes,
                   int x,
                   int y);
  void DontUseIdleSchedulingForTesting() {
    dont_use_idle_scheduling_for_testing_ = true;
  }
  void SetCanvasResourceHost(CanvasResourceHost* host);

  void Hibernate();
  bool IsHibernating() const { return hibernation_image_ != nullptr; }
  const CanvasColorParams& ColorParams() const { return color_params_; }

  bool HasRecordedDrawCommands() { return have_recorded_draw_commands_; }

  scoped_refptr<StaticBitmapImage> NewImageSnapshot();

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
  CanvasResourceProvider* ResourceProvider() const;
  void FlushRecording();

  sk_sp<cc::PaintRecord> getLastRecord() {
    return last_record_tainted_by_write_pixels_ ? nullptr : last_recording_;
  }

  // This is called when the Canvas element has cleared the frame, so the 2D
  // bridge knows that there's no previous content on the resource.
  void ClearFrame() { clear_frame_ = true; }

  bool HasRateLimiterForTesting();

 private:
  friend class Canvas2DLayerBridgeTest;
  friend class CanvasRenderingContext2DTest;
  friend class HTMLCanvasPainterTestForCAP;

  bool IsHidden() { return is_hidden_; }
  bool CheckResourceProviderValid();
  void ResetResourceProvider();

  void SkipQueuedDrawCommands();

  // Check if the Raster Mode is GPU and if the GPU context is not lost
  bool ShouldAccelerate() const;

  sk_sp<SkImage> hibernation_image_;
  scoped_refptr<cc::TextureLayer> layer_;
  std::unique_ptr<SharedContextRateLimiter> rate_limiter_;
  std::unique_ptr<Logger> logger_;
  int frames_since_last_commit_ = 0;
  bool have_recorded_draw_commands_;
  bool is_hidden_;
  bool is_being_displayed_;
  bool hibernation_scheduled_ = false;
  bool dont_use_idle_scheduling_for_testing_ = false;
  bool context_lost_ = false;
  bool clear_frame_ = true;

  // WritePixels content is not saved in recording. If a call was made to
  // WritePixels, the recording is now missing that information.
  bool last_record_tainted_by_write_pixels_ = false;

  const RasterMode raster_mode_;
  const CanvasColorParams color_params_;
  const IntSize size_;

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

  std::mt19937 random_generator_;
  std::bernoulli_distribution bernoulli_distribution_;
  Deque<RasterTimer> pending_raster_timers_;

  sk_sp<cc::PaintRecord> last_recording_;

  base::WeakPtrFactory<Canvas2DLayerBridge> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(Canvas2DLayerBridge);
};

}  // namespace blink

#endif
