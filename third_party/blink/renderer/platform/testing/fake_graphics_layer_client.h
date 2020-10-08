// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_FAKE_GRAPHICS_LAYER_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_FAKE_GRAPHICS_LAYER_CLIENT_H_

#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer_client.h"

namespace blink {

// A simple GraphicsLayerClient implementation suitable for use in unit tests.
class FakeGraphicsLayerClient : public GraphicsLayerClient {
 public:
  // GraphicsLayerClient implementation.
  IntRect ComputeInterestRect(const GraphicsLayer*,
                              const IntRect&) const override {
    return IntRect();
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
                     const IntRect& rect) const override {
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
                                     const IntRect&)>;
  void SetPainter(const Painter& painter) { painter_ = painter; }

 private:
  Painter painter_ = nullptr;
  bool is_tracking_raster_invalidations_ = false;
  bool needs_repaint_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_FAKE_GRAPHICS_LAYER_CLIENT_H_
