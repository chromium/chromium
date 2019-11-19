// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scroll/scrollbar_layer_delegate.h"

#include "cc/paint/paint_canvas.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scrollbar.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"
#include "ui/gfx/skia_util.h"

namespace blink {

namespace {

class ScopedScrollbarPainter {
 public:
  ScopedScrollbarPainter(cc::PaintCanvas& canvas, float device_scale_factor)
      : canvas_(canvas) {
    builder_.Context().SetDeviceScaleFactor(device_scale_factor);
  }
  ~ScopedScrollbarPainter() { canvas_.drawPicture(builder_.EndRecording()); }

  GraphicsContext& Context() { return builder_.Context(); }

 private:
  cc::PaintCanvas& canvas_;
  PaintRecordBuilder builder_;
};

}  // namespace

ScrollbarLayerDelegate::ScrollbarLayerDelegate(blink::Scrollbar& scrollbar,
                                               float device_scale_factor)
    : scrollbar_(&scrollbar), device_scale_factor_(device_scale_factor) {
  // Custom scrollbars are either non-composited or use cc::PictureLayers
  // which don't need ScrollbarLayerDelegate.
  DCHECK(!scrollbar.IsCustomScrollbar());
}

ScrollbarLayerDelegate::~ScrollbarLayerDelegate() = default;

cc::ScrollbarOrientation ScrollbarLayerDelegate::Orientation() const {
  if (scrollbar_->Orientation() == kHorizontalScrollbar)
    return cc::HORIZONTAL;
  return cc::VERTICAL;
}

bool ScrollbarLayerDelegate::IsLeftSideVerticalScrollbar() const {
  return scrollbar_->IsLeftSideVerticalScrollbar();
}

bool ScrollbarLayerDelegate::HasThumb() const {
  return scrollbar_->GetTheme().HasThumb(*scrollbar_);
}

bool ScrollbarLayerDelegate::IsSolidColor() const {
  return scrollbar_->GetTheme().IsSolidColor();
}

bool ScrollbarLayerDelegate::IsOverlay() const {
  return scrollbar_->IsOverlayScrollbar();
}

gfx::Rect ScrollbarLayerDelegate::ThumbRect() const {
  IntRect track_rect = scrollbar_->GetTheme().ThumbRect(*scrollbar_);
  track_rect.MoveBy(-scrollbar_->Location());
  return track_rect;
}

gfx::Rect ScrollbarLayerDelegate::TrackRect() const {
  IntRect track_rect = scrollbar_->GetTheme().TrackRect(*scrollbar_);
  track_rect.MoveBy(-scrollbar_->Location());
  return track_rect;
}

bool ScrollbarLayerDelegate::SupportsDragSnapBack() const {
  return scrollbar_->GetTheme().SupportsDragSnapBack();
}

gfx::Rect ScrollbarLayerDelegate::BackButtonRect() const {
  if (scrollbar_->GetTheme().ButtonsPlacement() ==
      kWebScrollbarButtonsPlacementNone)
    return gfx::Rect();

  IntRect back_button_rect = scrollbar_->GetTheme().BackButtonRect(
      *scrollbar_, blink::kBackButtonStartPart);
  back_button_rect.MoveBy(-scrollbar_->Location());
  return back_button_rect;
}

gfx::Rect ScrollbarLayerDelegate::ForwardButtonRect() const {
  if (scrollbar_->GetTheme().ButtonsPlacement() ==
      kWebScrollbarButtonsPlacementNone)
    return gfx::Rect();

  IntRect forward_button_rect = scrollbar_->GetTheme().ForwardButtonRect(
      *scrollbar_, blink::kForwardButtonEndPart);
  forward_button_rect.MoveBy(-scrollbar_->Location());
  return forward_button_rect;
}

float ScrollbarLayerDelegate::ThumbOpacity() const {
  return scrollbar_->GetTheme().ThumbOpacity(*scrollbar_);
}

bool ScrollbarLayerDelegate::NeedsRepaintPart(cc::ScrollbarPart part) const {
  if (part == cc::THUMB)
    return scrollbar_->ThumbNeedsRepaint();
  return scrollbar_->TrackNeedsRepaint();
}

bool ScrollbarLayerDelegate::UsesNinePatchThumbResource() const {
  return scrollbar_->GetTheme().UsesNinePatchThumbResource();
}

gfx::Size ScrollbarLayerDelegate::NinePatchThumbCanvasSize() const {
  DCHECK(scrollbar_->GetTheme().UsesNinePatchThumbResource());
  return static_cast<gfx::Size>(
      scrollbar_->GetTheme().NinePatchThumbCanvasSize(*scrollbar_));
}

gfx::Rect ScrollbarLayerDelegate::NinePatchThumbAperture() const {
  DCHECK(scrollbar_->GetTheme().UsesNinePatchThumbResource());
  return scrollbar_->GetTheme().NinePatchThumbAperture(*scrollbar_);
}

bool ScrollbarLayerDelegate::ShouldPaint() const {
  // TODO(crbug.com/860499): Remove this condition, it should not occur.
  // Layers may exist and be painted for a |scrollbar_| that has had its
  // ScrollableArea detached. This seems weird because if the area is detached
  // the layer should be destroyed but here we are. https://crbug.com/860499.
  if (!scrollbar_->GetScrollableArea())
    return false;
  // When the frame is throttled, the scrollbar will not be painted because
  // the frame has not had its lifecycle updated. Thus the actual value of
  // HasTickmarks can't be known and may change once the frame is unthrottled.
  if (scrollbar_->GetScrollableArea()->IsThrottled())
    return false;
  return true;
}

bool ScrollbarLayerDelegate::HasTickmarks() const {
  return ShouldPaint() && scrollbar_->HasTickmarks();
}

void ScrollbarLayerDelegate::PaintPart(cc::PaintCanvas* canvas,
                                       cc::ScrollbarPart part,
                                       const gfx::Rect& rect) {
  if (!ShouldPaint())
    return;

  auto& theme = scrollbar_->GetTheme();
  ScopedScrollbarPainter painter(*canvas, device_scale_factor_);
  // The canvas coordinate space is relative to the part's origin.
  switch (part) {
    case cc::THUMB:
      theme.PaintThumb(painter.Context(), *scrollbar_, IntRect(rect));
      scrollbar_->ClearThumbNeedsRepaint();
      break;
    case cc::TRACK_BUTTONS_TICKMARKS: {
      DCHECK_EQ(IntSize(rect.size()), scrollbar_->FrameRect().Size());
      IntPoint offset(IntPoint(rect.origin()) -
                      scrollbar_->FrameRect().Location());
      theme.PaintTrackButtonsTickmarks(painter.Context(), *scrollbar_, offset);
      scrollbar_->ClearTrackNeedsRepaint();
      break;
    }
    default:
      NOTREACHED();
  }
}

}  // namespace blink
