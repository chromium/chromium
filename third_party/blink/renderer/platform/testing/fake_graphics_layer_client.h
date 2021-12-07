// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_FAKE_GRAPHICS_LAYER_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_FAKE_GRAPHICS_LAYER_CLIENT_H_

#include "third_party/blink/renderer/platform/graphics/graphics_layer_client.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

// A simple GraphicsLayerClient implementation suitable for use in unit tests.
class FakeGraphicsLayerClient
    : public GarbageCollected<FakeGraphicsLayerClient>,
      public GraphicsLayerClient {
 public:
  // GraphicsLayerClient implementation.
  gfx::Rect ComputeInterestRect(const GraphicsLayer*,
                                const gfx::Rect&) const override {
    return gfx::Rect();
  }
  gfx::Rect PaintableRegion(const GraphicsLayer*) const override {
    return gfx::Rect();
  }
  PaintArtifactCompositor* GetPaintArtifactCompositor() override {
    return nullptr;
  }
  String DebugName(const GraphicsLayer*) const override { return String(); }
  bool IsTrackingRasterInvalidations() const override {
    return is_tracking_raster_invalidations_;
  }
  bool NeedsRepaint(const GraphicsLayer&) const override {
    return needs_repaint_;
  }
  void PaintContents(const GraphicsLayer* layer,
                     GraphicsContext& context,
                     GraphicsLayerPaintingPhase phase,
                     const gfx::Rect& rect) const override {
    if (painter_)
      painter_(layer, context, phase, rect);
  }

  void SetIsTrackingRasterInvalidations(bool is_tracking_raster_invalidations) {
    is_tracking_raster_invalidations_ = is_tracking_raster_invalidations;
  }

  void SetNeedsRepaint(bool needs_repaint) { needs_repaint_ = needs_repaint; }

  using Painter = std::function<void(const GraphicsLayer*,
                                     GraphicsContext&,
                                     GraphicsLayerPaintingPhase,
                                     const gfx::Rect&)>;
  void SetPainter(const Painter& painter) { painter_ = painter; }

  void Trace(Visitor* visitor) const override {
    GraphicsLayerClient::Trace(visitor);
  }

 private:
  Painter painter_ = nullptr;
  bool is_tracking_raster_invalidations_ = false;
  bool needs_repaint_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_FAKE_GRAPHICS_LAYER_CLIENT_H_
