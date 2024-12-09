// Copyright 2014 The Chromium Authors
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
#include "third_party/blink/renderer/platform/web_test_support.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace blink {

namespace {

class ScopedScrollbarPainter {
  STACK_ALLOCATED();

 public:
  explicit ScopedScrollbarPainter(cc::PaintCanvas& canvas) : canvas_(canvas) {}
  ~ScopedScrollbarPainter() { canvas_.drawPicture(builder_.EndRecording()); }

  GraphicsContext& Context() { return builder_.Context(); }

 private:
  cc::PaintCanvas& canvas_;
  PaintRecordBuilder builder_;
};

}  // namespace

ScrollbarLayerDelegate::ScrollbarLayerDelegate(blink::Scrollbar& scrollbar)
    : scrollbar_(&scrollbar) {
  // Custom scrollbars are either non-composited or use cc::PictureLayers
  // which don't need ScrollbarLayerDelegate.
  DCHECK(!scrollbar.IsCustomScrollbar());
}

ScrollbarLayerDelegate::~ScrollbarLayerDelegate() = default;

bool ScrollbarLayerDelegate::IsSame(const cc::Scrollbar& other) const {
  return scrollbar_.Get() ==
         static_cast<const ScrollbarLayerDelegate&>(other).scrollbar_.Get();
}

cc::ScrollbarOrientation ScrollbarLayerDelegate::Orientation() const {
  if (scrollbar_->Orientation() == kHorizontalScrollbar)
    return cc::ScrollbarOrientation::kHorizontal;
  return cc::ScrollbarOrientation::kVertical;
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

bool ScrollbarLayerDelegate::IsRunningWebTest() const {
  return WebTestSupport::IsRunningWebTest();
}

bool ScrollbarLayerDelegate::IsFluentOverlayScrollbarMinimalMode() const {
  return scrollbar_->IsFluentOverlayScrollbarMinimalMode();
}

gfx::Rect ScrollbarLayerDelegate::ShrinkMainThreadedMinimalModeThumbRect(
    gfx::Rect& rect) const {
  return scrollbar_->GetTheme().ShrinkMainThreadedMinimalModeThumbRect(
      *scrollbar_, rect);
}

gfx::Rect ScrollbarLayerDelegate::ThumbRect() const {
  gfx::Rect thumb_rect = scrollbar_->GetTheme().ThumbRect(*scrollbar_);
  thumb_rect.Offset(-scrollbar_->Location().OffsetFromOrigin());
  return thumb_rect;
}

gfx::Rect ScrollbarLayerDelegate::TrackRect() const {
  gfx::Rect track_rect = scrollbar_->GetTheme().TrackRect(*scrollbar_);
  track_rect.Offset(-scrollbar_->Location().OffsetFromOrigin());
  return track_rect;
}

bool ScrollbarLayerDelegate::SupportsDragSnapBack() const {
  return scrollbar_->GetTheme().SupportsDragSnapBack();
}

bool ScrollbarLayerDelegate::JumpOnTrackClick() const {
  return scrollbar_->GetTheme().JumpOnTrackClick();
}

bool ScrollbarLayerDelegate::IsOpaque() const {
  return scrollbar_->IsOpaque();
}

gfx::Rect ScrollbarLayerDelegate::BackButtonRect() const {
  gfx::Rect back_button_rect =
      scrollbar_->GetTheme().BackButtonRect(*scrollbar_);
  if (!back_button_rect.IsEmpty())
    back_button_rect.Offset(-scrollbar_->Location().OffsetFromOrigin());
  return back_button_rect;
}

gfx::Rect ScrollbarLayerDelegate::ForwardButtonRect() const {
  gfx::Rect forward_button_rect =
      scrollbar_->GetTheme().ForwardButtonRect(*scrollbar_);
  if (!forward_button_rect.IsEmpty())
    forward_button_rect.Offset(-scrollbar_->Location().OffsetFromOrigin());
  return forward_button_rect;
}

float ScrollbarLayerDelegate::Opacity() const {
  return scrollbar_->GetTheme().Opacity(*scrollbar_);
}

bool ScrollbarLayerDelegate::ThumbNeedsRepaint() const {
  return scrollbar_->ThumbNeedsRepaint();
}

void ScrollbarLayerDelegate::ClearThumbNeedsRepaint() {
  scrollbar_->ClearThumbNeedsRepaint();
}

bool ScrollbarLayerDelegate::TrackAndButtonsNeedRepaint() const {
  return scrollbar_->TrackAndButtonsNeedRepaint();
}

bool ScrollbarLayerDelegate::NeedsUpdateDisplay() const {
  return scrollbar_->NeedsUpdateDisplay();
}

void ScrollbarLayerDelegate::ClearNeedsUpdateDisplay() {
  scrollbar_->ClearNeedsUpdateDisplay();
}

bool ScrollbarLayerDelegate::UsesNinePatchThumbResource() const {
  return scrollbar_->GetTheme().UsesNinePatchThumbResource();
}

gfx::Size ScrollbarLayerDelegate::NinePatchThumbCanvasSize() const {
  DCHECK(UsesNinePatchThumbResource());
  return scrollbar_->GetTheme().NinePatchThumbCanvasSize(*scrollbar_);
}

gfx::Rect ScrollbarLayerDelegate::NinePatchThumbAperture() const {
  DCHECK(scrollbar_->GetTheme().UsesNinePatchThumbResource());
  return scrollbar_->GetTheme().NinePatchThumbAperture(*scrollbar_);
}

bool ScrollbarLayerDelegate::UsesSolidColorThumb() const {
  return scrollbar_->GetTheme().UsesSolidColorThumb();
}

gfx::Insets ScrollbarLayerDelegate::SolidColorThumbInsets() const {
  return scrollbar_->GetTheme().SolidColorThumbInsets(*scrollbar_);
}

bool ScrollbarLayerDelegate::UsesNinePatchTrackAndButtonsResource() const {
  return scrollbar_->GetTheme().UsesNinePatchTrackAndButtonsResource();
}

gfx::Size ScrollbarLayerDelegate::NinePatchTrackAndButtonsCanvasSize() const {
  CHECK(UsesNinePatchTrackAndButtonsResource());
  return scrollbar_->GetTheme().NinePatchTrackAndButtonsCanvasSize(*scrollbar_);
}

gfx::Rect ScrollbarLayerDelegate::NinePatchTrackAndButtonsAperture() const {
  CHECK(UsesNinePatchTrackAndButtonsResource());
  return scrollbar_->GetTheme().NinePatchTrackAndButtonsAperture(*scrollbar_);
}

bool ScrollbarLayerDelegate::ShouldPaint() const {
  return scrollbar_->ShouldPaint();
}

bool ScrollbarLayerDelegate::HasTickmarks() const {
  return ShouldPaint() && scrollbar_->HasTickmarks();
}

void ScrollbarLayerDelegate::PaintThumb(cc::PaintCanvas& canvas,
                                        const gfx::Rect& rect) {
  if (!ShouldPaint()) {
    return;
  }
  auto& theme = scrollbar_->GetTheme();
  ScopedScrollbarPainter painter(canvas);
  theme.PaintThumb(painter.Context(), *scrollbar_, rect);
  scrollbar_->ClearThumbNeedsRepaint();
}

void ScrollbarLayerDelegate::PaintTrackAndButtons(cc::PaintCanvas& canvas,
                                                  const gfx::Rect& rect) {
  if (!ShouldPaint()) {
    return;
  }
  auto& theme = scrollbar_->GetTheme();
  ScopedScrollbarPainter painter(canvas);
  theme.PaintTrackAndButtons(painter.Context(), *scrollbar_, rect);
  scrollbar_->ClearTrackAndButtonsNeedRepaint();
}

SkColor4f ScrollbarLayerDelegate::ThumbColor() const {
  CHECK(IsSolidColor() || UsesSolidColorThumb());
  return scrollbar_->GetTheme().ThumbColor(*scrollbar_);
}

}  // namespace blink
