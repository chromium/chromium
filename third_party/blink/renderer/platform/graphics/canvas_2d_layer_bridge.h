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

#include "base/memory/raw_ptr.h"
#include "third_party/blink/renderer/platform/graphics/canvas_hibernation_handler.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

class CanvasResourceHost;
class CanvasResourceProvider;

class PLATFORM_EXPORT Canvas2DLayerBridge {
 public:
  explicit Canvas2DLayerBridge(CanvasResourceHost* resource_host);
  Canvas2DLayerBridge(const Canvas2DLayerBridge&) = delete;
  Canvas2DLayerBridge& operator=(const Canvas2DLayerBridge&) = delete;

  virtual ~Canvas2DLayerBridge();

  void PageVisibilityChanged();

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

  CanvasResourceProvider* GetOrCreateResourceProvider();

  // Allow access to the hibernation handler while Canvas2DLayerBridge is being
  // incrementally folded into CanvasRenderingContext2D.
  // TODO(crbug.com/40280152): Eliminate Canvas2DLayerBridge entirely.
  CanvasHibernationHandler& GetHibernationHandler() {
    return hibernation_handler_;
  }

 private:
  static void HibernateOrLogFailure(base::WeakPtr<Canvas2DLayerBridge> bridge,
                                    base::TimeTicks /*idleDeadline*/);
  void Hibernate();

  CanvasHibernationHandler hibernation_handler_;

  bool hibernation_scheduled_ = false;

  raw_ptr<CanvasResourceHost> resource_host_;

  base::WeakPtrFactory<Canvas2DLayerBridge> weak_ptr_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_2D_LAYER_BRIDGE_H_
