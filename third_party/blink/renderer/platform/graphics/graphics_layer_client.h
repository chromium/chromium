/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GRAPHICS_LAYER_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GRAPHICS_LAYER_CLIENT_H_

#include "third_party/blink/renderer/platform/geometry/layout_size.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class GraphicsContext;
class GraphicsLayer;
class IntRect;
class ScrollableArea;

enum GraphicsLayerPaintingPhaseFlags {
  kGraphicsLayerPaintBackground = (1 << 0),
  kGraphicsLayerPaintForeground = (1 << 1),
  kGraphicsLayerPaintMask = (1 << 2),
  kGraphicsLayerPaintOverflowContents = (1 << 3),
  kGraphicsLayerPaintCompositedScroll = (1 << 4),
  kGraphicsLayerPaintDecoration = (1 << 5),
  kGraphicsLayerPaintAllWithOverflowClip =
      (kGraphicsLayerPaintBackground | kGraphicsLayerPaintForeground |
       kGraphicsLayerPaintMask | kGraphicsLayerPaintDecoration)
};
typedef unsigned GraphicsLayerPaintingPhase;

class PLATFORM_EXPORT GraphicsLayerClient {
 public:
  virtual ~GraphicsLayerClient() = default;

  virtual IntRect ComputeInterestRect(
      const GraphicsLayer*,
      const IntRect& previous_interest_rect) const = 0;
  virtual LayoutSize SubpixelAccumulation() const { return LayoutSize(); }
  // Returns whether the client needs to be repainted with respect to the given
  // graphics layer.
  virtual bool NeedsRepaint(const GraphicsLayer&) const = 0;
  virtual void PaintContents(const GraphicsLayer*,
                             GraphicsContext&,
                             GraphicsLayerPaintingPhase,
                             const IntRect& interest_rect) const = 0;

  // Returns true if the GraphicsLayer is under a frame that should not render
  // (see LocalFrameView::ShouldThrottleRendering()).
  virtual bool ShouldThrottleRendering() const { return false; }

  // Content under a LayoutSVGHiddenContainer is an auxiliary resource for
  // painting and hit testing.
  virtual bool IsUnderSVGHiddenContainer() const { return false; }

  virtual bool IsSVGRoot() const { return false; }

  virtual bool IsTrackingRasterInvalidations() const { return false; }

  virtual void GraphicsLayersDidChange() {}

  virtual String DebugName(const GraphicsLayer*) const = 0;

  virtual const ScrollableArea* GetScrollableAreaForTesting(
      const GraphicsLayer*) const {
    return nullptr;
  }

  // Returns true if this client is prevented from painting by its own
  // display-lock (in case of target = kSelf) or by any of its ancestors (in
  // case of target = kSelf or kChildren).
  virtual bool PaintBlockedByDisplayLockIncludingAncestors() const {
    return false;
  }
  virtual void NotifyDisplayLockNeedsGraphicsLayerCollection() {}

#if DCHECK_IS_ON()
  // CompositedLayerMapping overrides this to verify that it is not
  // currently painting contents. An ASSERT fails, if it is.
  // This is executed in GraphicsLayer construction and destruction
  // to verify that we don't create or destroy GraphicsLayers
  // while painting.
  virtual void VerifyNotPainting() {}
#endif
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GRAPHICS_LAYER_CLIENT_H_
