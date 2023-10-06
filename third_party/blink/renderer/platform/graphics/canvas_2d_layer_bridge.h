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

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/rand_util.h"
#include "build/build_config.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "third_party/blink/renderer/platform/graphics/canvas_hibernation_handler.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_host.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"

struct SkImageInfo;

namespace blink {

class Canvas2DLayerBridgeTest;
class StaticBitmapImage;

class PLATFORM_EXPORT Canvas2DLayerBridge {
 public:
  explicit Canvas2DLayerBridge();
  Canvas2DLayerBridge(const Canvas2DLayerBridge&) = delete;
  Canvas2DLayerBridge& operator=(const Canvas2DLayerBridge&) = delete;

  virtual ~Canvas2DLayerBridge();

  void FinalizeFrame(FlushReason);
  void PageVisibilityChanged();
  bool Restore();

  // virtual for unit testing
  virtual void WillOverwriteCanvas();

  void DrawFullImage(const cc::PaintImage&);

  // This may recreate CanvasResourceProvider
  cc::PaintCanvas* GetPaintCanvas();
  bool WritePixels(const SkImageInfo&,
                   const void* pixels,
                   size_t row_bytes,
                   int x,
                   int y);
  void SetCanvasResourceHost(CanvasResourceHost* host);

  void Hibernate();
  // This is used for a memory usage experiment: frees canvas resource when
  // canvas is in an invisible tab.
  void LoseContext();
  bool IsHibernating() const { return hibernation_handler_.IsHibernating(); }

  scoped_refptr<StaticBitmapImage> NewImageSnapshot(FlushReason);

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
  void FlushRecording(FlushReason);

  static bool IsHibernationEnabled();

  CanvasHibernationHandler& GetHibernationHandlerForTesting() {
    return hibernation_handler_;
  }

 private:
  friend class Canvas2DLayerBridgeTest;
  friend class CanvasRenderingContext2DTest;
  friend class HTMLCanvasPainterTestForCAP;

  CanvasResourceProvider* ResourceProvider() const;
  void ResetResourceProvider();

  void SkipQueuedDrawCommands();

  // Check if the Raster Mode is GPU and if the GPU context is not lost
  bool ShouldAccelerate() const;

  CanvasHibernationHandler hibernation_handler_;

  std::unique_ptr<Logger> logger_;
  bool hibernation_scheduled_ = false;
  bool context_lost_ = false;
  bool lose_context_in_background_ = false;
  bool lose_context_in_background_scheduled_ = false;


  enum SnapshotState {
    kInitialSnapshotState,
    kDidAcquireSnapshot,
  };
  mutable SnapshotState snapshot_state_;

  raw_ptr<CanvasResourceHost, ExperimentalRenderer> resource_host_;
  viz::TransferableResource previous_frame_resource_;

  base::WeakPtrFactory<Canvas2DLayerBridge> weak_ptr_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_2D_LAYER_BRIDGE_H_
