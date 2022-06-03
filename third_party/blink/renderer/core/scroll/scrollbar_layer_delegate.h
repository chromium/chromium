// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLLBAR_LAYER_DELEGATE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLLBAR_LAYER_DELEGATE_H_

#include "cc/input/scrollbar.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

class Scrollbar;

// Implementation of cc::Scrollbar, providing a delegate to query about
// scrollbar state and to paint the image in the scrollbar.
class CORE_EXPORT ScrollbarLayerDelegate : public cc::Scrollbar {
 public:
  ScrollbarLayerDelegate(blink::Scrollbar& scrollbar,
                         float device_scale_factor);
  ScrollbarLayerDelegate(const ScrollbarLayerDelegate&) = delete;
  ScrollbarLayerDelegate& operator=(const ScrollbarLayerDelegate&) = delete;

  // cc::Scrollbar implementation.
  bool IsSame(const cc::Scrollbar& other) const override;
  cc::ScrollbarOrientation Orientation() const override;
  bool IsLeftSideVerticalScrollbar() const override;
  bool HasThumb() const override;
  bool IsSolidColor() const override;
  bool IsOverlay() const override;
  bool SupportsDragSnapBack() const override;
  bool JumpOnTrackClick() const override;

  // The following rects are all relative to the scrollbar's origin.
  gfx::Rect ThumbRect() const override;
  gfx::Rect TrackRect() const override;
  gfx::Rect BackButtonRect() const override;
  gfx::Rect ForwardButtonRect() const override;

  float Opacity() const override;
  bool NeedsRepaintPart(cc::ScrollbarPart part) const override;
  bool HasTickmarks() const override;
  void PaintPart(cc::PaintCanvas* canvas,
                 cc::ScrollbarPart part,
                 const gfx::Rect& rect) override;

  bool UsesNinePatchThumbResource() const override;
  gfx::Size NinePatchThumbCanvasSize() const override;
  gfx::Rect NinePatchThumbAperture() const override;

 private:
  ~ScrollbarLayerDelegate() override;

  bool ShouldPaint() const;

  Persistent<blink::Scrollbar> scrollbar_;
  float device_scale_factor_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLLBAR_LAYER_DELEGATE_H_
