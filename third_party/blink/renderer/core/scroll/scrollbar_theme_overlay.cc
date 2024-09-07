/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/scroll/scrollbar_theme_overlay.h"

#include "third_party/blink/public/platform/web_theme_engine.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scrollbar.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/theme/web_theme_engine_helper.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "ui/gfx/geometry/transform.h"

#include <algorithm>

namespace blink {

ScrollbarThemeOverlay& ScrollbarThemeOverlay::GetInstance() {
  DEFINE_STATIC_LOCAL(
      ScrollbarThemeOverlay, theme,
      (WebThemeEngineHelper::GetNativeThemeEngine()
           ->GetSize(WebThemeEngine::kPartScrollbarVerticalThumb)
           .width(),
       0,
       WebThemeEngineHelper::GetNativeThemeEngine()
           ->GetSize(WebThemeEngine::kPartScrollbarVerticalThumb)
           .width(),
       0));
  return theme;
}

ScrollbarThemeOverlay::ScrollbarThemeOverlay(int thumb_thickness_default_dip,
                                             int scrollbar_margin_default_dip,
                                             int thumb_thickness_thin_dip,
                                             int scrollbar_margin_thin_dip)
    : thumb_thickness_default_dip_(thumb_thickness_default_dip),
      scrollbar_margin_default_dip_(scrollbar_margin_default_dip),
      thumb_thickness_thin_dip_(thumb_thickness_thin_dip),
      scrollbar_margin_thin_dip_(scrollbar_margin_thin_dip) {}

bool ScrollbarThemeOverlay::ShouldRepaintAllPartsOnInvalidation() const {
  return false;
}

ScrollbarPart ScrollbarThemeOverlay::PartsToInvalidateOnThumbPositionChange(
    const Scrollbar&,
    float old_position,
    float new_position) const {
  return kNoPart;
}

int ScrollbarThemeOverlay::ScrollbarThickness(
    float scale_from_dip,
    EScrollbarWidth scrollbar_width) const {
  return ThumbThickness(scale_from_dip, scrollbar_width) +
         ScrollbarMargin(scale_from_dip, scrollbar_width);
}

int ScrollbarThemeOverlay::ScrollbarMargin(
    float scale_from_dip,
    EScrollbarWidth scrollbar_width) const {
  if (scrollbar_width == EScrollbarWidth::kNone)
    return 0;
  else if (scrollbar_width == EScrollbarWidth::kThin)
    return scrollbar_margin_thin_dip_ * scale_from_dip;
  else
    return scrollbar_margin_default_dip_ * scale_from_dip;
}

bool ScrollbarThemeOverlay::UsesOverlayScrollbars() const {
  return true;
}

base::TimeDelta ScrollbarThemeOverlay::OverlayScrollbarFadeOutDelay() const {
  WebThemeEngine::ScrollbarStyle style;
  WebThemeEngineHelper::GetNativeThemeEngine()->GetOverlayScrollbarStyle(
      &style);
  return style.fade_out_delay;
}

base::TimeDelta ScrollbarThemeOverlay::OverlayScrollbarFadeOutDuration() const {
  WebThemeEngine::ScrollbarStyle style;
  WebThemeEngineHelper::GetNativeThemeEngine()->GetOverlayScrollbarStyle(
      &style);
  return style.fade_out_duration;
}

int ScrollbarThemeOverlay::ThumbLength(const Scrollbar& scrollbar) const {
  int track_len = TrackLength(scrollbar);

  if (!scrollbar.TotalSize())
    return track_len;

  float proportion =
      static_cast<float>(scrollbar.VisibleSize()) / scrollbar.TotalSize();
  int length = round(proportion * track_len);
  int min_len = std::min(MinimumThumbLength(scrollbar), track_len);
  length = ClampTo(length, min_len, track_len);
  return length;
}

int ScrollbarThemeOverlay::ThumbThickness(
    float scale_from_dip,
    EScrollbarWidth scrollbar_width) const {
  if (scrollbar_width == EScrollbarWidth::kNone)
    return 0;
  else if (scrollbar_width == EScrollbarWidth::kThin)
    return thumb_thickness_thin_dip_ * scale_from_dip;
  else
    return thumb_thickness_default_dip_ * scale_from_dip;
}

bool ScrollbarThemeOverlay::HasThumb(const Scrollbar& scrollbar) const {
  return true;
}

gfx::Rect ScrollbarThemeOverlay::BackButtonRect(const Scrollbar&) const {
  return gfx::Rect();
}

gfx::Rect ScrollbarThemeOverlay::ForwardButtonRect(const Scrollbar&) const {
  return gfx::Rect();
}

gfx::Rect ScrollbarThemeOverlay::TrackRect(const Scrollbar& scrollbar) const {
  gfx::Rect rect = scrollbar.FrameRect();
  int scrollbar_margin =
      ScrollbarMargin(scrollbar.ScaleFromDIP(), scrollbar.CSSScrollbarWidth());
  if (scrollbar.Orientation() == kHorizontalScrollbar)
    rect.Inset(gfx::Insets::VH(0, scrollbar_margin));
  else
    rect.Inset(gfx::Insets::VH(scrollbar_margin, 0));
  return rect;
}

gfx::Rect ScrollbarThemeOverlay::ThumbRect(const Scrollbar& scrollbar) const {
  gfx::Rect rect = ScrollbarTheme::ThumbRect(scrollbar);
  EScrollbarWidth scrollbar_width = scrollbar.CSSScrollbarWidth();
  if (scrollbar.Orientation() == kHorizontalScrollbar) {
    rect.set_height(ThumbThickness(scrollbar.ScaleFromDIP(), scrollbar_width));
  } else {
    if (scrollbar.IsLeftSideVerticalScrollbar()) {
      rect.Offset(ScrollbarMargin(scrollbar.ScaleFromDIP(), scrollbar_width),
                  0);
    }
    rect.set_width(ThumbThickness(scrollbar.ScaleFromDIP(), scrollbar_width));
  }
  return rect;
}

void ScrollbarThemeOverlay::PaintThumb(GraphicsContext& context,
                                       const Scrollbar& scrollbar,
                                       const gfx::Rect& rect) {
  if (DrawingRecorder::UseCachedDrawingIfPossible(context, scrollbar,
                                                  DisplayItem::kScrollbarThumb))
    return;

  DrawingRecorder recorder(context, scrollbar, DisplayItem::kScrollbarThumb,
                           rect);

  WebThemeEngine::State state = WebThemeEngine::kStateNormal;

  if (!scrollbar.Enabled())
    state = WebThemeEngine::kStateDisabled;
  else if (scrollbar.PressedPart() == kThumbPart)
    state = WebThemeEngine::kStatePressed;
  else if (scrollbar.HoveredPart() == kThumbPart)
    state = WebThemeEngine::kStateHover;

  cc::PaintCanvas* canvas = context.Canvas();

  WebThemeEngine::Part part = WebThemeEngine::kPartScrollbarHorizontalThumb;
  if (scrollbar.Orientation() == kVerticalScrollbar)
    part = WebThemeEngine::kPartScrollbarVerticalThumb;

  blink::WebThemeEngine::ScrollbarThumbExtraParams scrollbar_thumb;
  if (scrollbar.ScrollbarThumbColor().has_value()) {
    scrollbar_thumb.thumb_color =
        scrollbar.ScrollbarThumbColor().value().toSkColor4f().toSkColor();
  }

  // Horizontally flip the canvas if it is left vertical scrollbar.
  if (scrollbar.IsLeftSideVerticalScrollbar()) {
    canvas->save();
    canvas->translate(rect.width(), 0);
    canvas->scale(-1, 1);
  }

  blink::WebThemeEngine::ExtraParams params(scrollbar_thumb);

  mojom::blink::ColorScheme color_scheme = scrollbar.UsedColorScheme();
  WebThemeEngineHelper::GetNativeThemeEngine()->Paint(
      canvas, part, state, rect, &params, color_scheme,
      scrollbar.InForcedColorsMode(), scrollbar.GetColorProvider(color_scheme));

  if (scrollbar.IsLeftSideVerticalScrollbar())
    canvas->restore();
}

ScrollbarPart ScrollbarThemeOverlay::HitTest(const Scrollbar& scrollbar,
                                             const gfx::Point& position) const {
  ScrollbarPart part = ScrollbarTheme::HitTest(scrollbar, position);
  if (part != kThumbPart)
    return kNoPart;

  return kThumbPart;
}

bool ScrollbarThemeOverlay::UsesNinePatchThumbResource() const {
  // Thumb orientation doesn't matter here.
  return WebThemeEngineHelper::GetNativeThemeEngine()->SupportsNinePatch(
      WebThemeEngine::kPartScrollbarVerticalThumb);
}

gfx::Size ScrollbarThemeOverlay::NinePatchThumbCanvasSize(
    const Scrollbar& scrollbar) const {
  DCHECK(UsesNinePatchThumbResource());

  WebThemeEngine::Part part =
      scrollbar.Orientation() == kVerticalScrollbar
          ? WebThemeEngine::kPartScrollbarVerticalThumb
          : WebThemeEngine::kPartScrollbarHorizontalThumb;

  return WebThemeEngineHelper::GetNativeThemeEngine()->NinePatchCanvasSize(
      part);
}

gfx::Rect ScrollbarThemeOverlay::NinePatchThumbAperture(
    const Scrollbar& scrollbar) const {
  DCHECK(UsesNinePatchThumbResource());

  WebThemeEngine::Part part = WebThemeEngine::kPartScrollbarHorizontalThumb;
  if (scrollbar.Orientation() == kVerticalScrollbar)
    part = WebThemeEngine::kPartScrollbarVerticalThumb;

  return WebThemeEngineHelper::GetNativeThemeEngine()->NinePatchAperture(part);
}

int ScrollbarThemeOverlay::MinimumThumbLength(
    const Scrollbar& scrollbar) const {
  if (scrollbar.Orientation() == kVerticalScrollbar) {
    return WebThemeEngineHelper::GetNativeThemeEngine()
        ->GetSize(WebThemeEngine::kPartScrollbarVerticalThumb)
        .height();
  }

  return WebThemeEngineHelper::GetNativeThemeEngine()
      ->GetSize(WebThemeEngine::kPartScrollbarHorizontalThumb)
      .width();
}

}  // namespace blink
