/*
 * Copyright (C) 2008, 2011 Apple Inc. All Rights Reserved.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/scroll/scrollbar_theme_mac.h"

#include "skia/ext/skia_utils_mac.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/platform/mac/web_scrollbar_theme.h"
#include "third_party/blink/public/platform/web_theme_engine.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/scroll/mac_scrollbar_animator.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/theme/web_theme_engine_helper.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

static float s_initial_button_delay = 0.5f;
static float s_autoscroll_button_delay = 0.05f;
static bool s_prefer_overlay_scroller_style = false;
static bool s_jump_on_track_click = false;

typedef HeapHashSet<WeakMember<Scrollbar>> ScrollbarSet;

static ScrollbarSet& GetScrollbarSet() {
  DEFINE_STATIC_LOCAL(Persistent<ScrollbarSet>, set,
                      (MakeGarbageCollected<ScrollbarSet>()));
  return *set;
}

// Values returned by NSScrollerImp's methods for querying sizes of various
// elements.
struct NSScrollerImpValues {
  float track_width;
  float track_box_width;
  float knob_min_length;
  float track_overlap_end_inset;
  float knob_overlap_end_inset;
  float track_end_inset;
  float knob_end_inset;
};
const NSScrollerImpValues& GetScrollbarPainterValues(bool overlay,
                                                     EScrollbarWidth width) {
  static NSScrollerImpValues overlay_small_values = {
      14.0, 14.0, 26.0, 0.0, 0.0, 0.0, 1.0,
  };
  static NSScrollerImpValues overlay_regular_values = {
      16.0, 16.0, 26.0, 0.0, 0.0, 0.0, 1.0,
  };
  static NSScrollerImpValues legacy_small_values = {
      11.0, 11.0, 16.0, 0.0, 0.0, 0.0, 2.0,
  };
  static NSScrollerImpValues legacy_regular_values = {
      15.0, 15.0, 20.0, 0.0, 0.0, 0.0, 2.0,
  };
  if (overlay) {
    return (width == EScrollbarWidth::kThin) ? overlay_small_values
                                             : overlay_regular_values;
  } else {
    return (width == EScrollbarWidth::kThin) ? legacy_small_values
                                             : legacy_regular_values;
  }
}

const NSScrollerImpValues& GetScrollbarPainterValues(
    const Scrollbar& scrollbar) {
  return GetScrollbarPainterValues(
      ScrollbarThemeMac::PreferOverlayScrollerStyle(),
      scrollbar.CSSScrollbarWidth());
}

ScrollbarThemeMac::ScrollbarThemeMac() {}

ScrollbarTheme& ScrollbarTheme::NativeTheme() {
  DEFINE_STATIC_LOCAL(ScrollbarThemeMac, overlay_theme, ());
  return overlay_theme;
}

void ScrollbarThemeMac::PaintTickmarks(GraphicsContext& context,
                                       const Scrollbar& scrollbar,
                                       const gfx::Rect& rect) {
  gfx::Rect tickmark_track_rect = rect;
  tickmark_track_rect.set_x(tickmark_track_rect.x() + 1);
  tickmark_track_rect.set_width(tickmark_track_rect.width() - 1);
  ScrollbarTheme::PaintTickmarks(context, scrollbar, tickmark_track_rect);
}

bool ScrollbarThemeMac::ShouldCenterOnThumb(const Scrollbar& scrollbar,
                                            const WebMouseEvent& event) const {
  bool alt_key_pressed = event.GetModifiers() & WebInputEvent::kAltKey;
  return (event.button == WebPointerProperties::Button::kLeft) &&
         (s_jump_on_track_click != alt_key_pressed);
}

ScrollbarThemeMac::~ScrollbarThemeMac() {}

base::TimeDelta ScrollbarThemeMac::InitialAutoscrollTimerDelay() const {
  return base::Seconds(s_initial_button_delay);
}

base::TimeDelta ScrollbarThemeMac::AutoscrollTimerDelay() const {
  return base::Seconds(s_autoscroll_button_delay);
}

bool ScrollbarThemeMac::ShouldDragDocumentInsteadOfThumb(
    const Scrollbar&,
    const WebMouseEvent& event) const {
  return (event.GetModifiers() & WebInputEvent::Modifiers::kAltKey) != 0;
}

ScrollbarPart ScrollbarThemeMac::PartsToInvalidateOnThumbPositionChange(
    const Scrollbar& scrollbar,
    float old_position,
    float new_position) const {
  // MacScrollbarAnimatorImpl will invalidate scrollbar parts if necessary.
  return kNoPart;
}

void ScrollbarThemeMac::RegisterScrollbar(Scrollbar& scrollbar) {
  GetScrollbarSet().insert(&scrollbar);
}

bool ScrollbarThemeMac::IsScrollbarRegistered(Scrollbar& scrollbar) const {
  return GetScrollbarSet().Contains(&scrollbar);
}

void ScrollbarThemeMac::SetNewPainterForScrollbar(Scrollbar& scrollbar) {
  UpdateEnabledState(scrollbar);
}

WebThemeEngine::ExtraParams GetPaintParams(const Scrollbar& scrollbar,
                                           bool overlay) {
  WebThemeEngine::ScrollbarExtraParams scrollbar_extra;
  scrollbar_extra.orientation =
      WebThemeEngine::ScrollbarOrientation::kVerticalOnRight;
  if (scrollbar.Orientation() == kHorizontalScrollbar) {
    scrollbar_extra.orientation =
        WebThemeEngine::ScrollbarOrientation::kHorizontal;
  } else if (scrollbar.IsLeftSideVerticalScrollbar()) {
    scrollbar_extra.orientation =
        WebThemeEngine::ScrollbarOrientation::kVerticalOnLeft;
  }

  scrollbar_extra.is_overlay = overlay;
  scrollbar_extra.is_hovering =
      scrollbar.HoveredPart() != ScrollbarPart::kNoPart;
  scrollbar_extra.scale_from_dip = scrollbar.ScaleFromDIP();

  if (scrollbar.ScrollbarThumbColor().has_value()) {
    scrollbar_extra.thumb_color =
        scrollbar.ScrollbarThumbColor().value().toSkColor4f().toSkColor();
  }

  if (scrollbar.ScrollbarTrackColor().has_value()) {
    scrollbar_extra.track_color =
        scrollbar.ScrollbarTrackColor().value().toSkColor4f().toSkColor();
  }

  return WebThemeEngine::ExtraParams(scrollbar_extra);
}

void ScrollbarThemeMac::PaintTrackBackground(GraphicsContext& context,
                                             const Scrollbar& scrollbar,
                                             const gfx::Rect& rect) {
  GraphicsContextStateSaver state_saver(context);
  context.Translate(rect.x(), rect.y());

  auto* mac_scrollbar = MacScrollbar::GetForScrollbar(scrollbar);
  if (!mac_scrollbar)
    return;

  // The track opacity will be read from the ScrollbarPainter.
  float opacity = mac_scrollbar->GetTrackAlpha();
  if (opacity == 0)
    return;

  if (opacity != 1)
    context.BeginLayer(opacity);
  WebThemeEngine::ExtraParams params =
      GetPaintParams(scrollbar, UsesOverlayScrollbars());
  const auto& scrollbar_extra =
      absl::get<WebThemeEngine::ScrollbarExtraParams>(params);
  gfx::Rect bounds(0, 0, scrollbar.FrameRect().width(),
                   scrollbar.FrameRect().height());
  WebThemeEngine::Part track_part =
      scrollbar_extra.orientation ==
              WebThemeEngine::ScrollbarOrientation::kHorizontal
          ? WebThemeEngine::Part::kPartScrollbarHorizontalTrack
          : WebThemeEngine::Part::kPartScrollbarVerticalTrack;
  mojom::blink::ColorScheme color_scheme = scrollbar.UsedColorScheme();
  WebThemeEngineHelper::GetNativeThemeEngine()->Paint(
      context.Canvas(), track_part, WebThemeEngine::State::kStateNormal, bounds,
      &params, color_scheme, scrollbar.InForcedColorsMode(),
      scrollbar.GetColorProvider(color_scheme));
  if (opacity != 1)
    context.EndLayer();
}

void ScrollbarThemeMac::PaintScrollCorner(GraphicsContext& context,
                                          const ScrollableArea& scrollable_area,
                                          const DisplayItemClient& item,
                                          const gfx::Rect& rect) {
  const Scrollbar* vertical_scrollbar = scrollable_area.VerticalScrollbar();
  if (!vertical_scrollbar) {
    ScrollbarTheme::PaintScrollCorner(context, scrollable_area, item, rect);
    return;
  }
  if (DrawingRecorder::UseCachedDrawingIfPossible(context, item,
                                                  DisplayItem::kScrollCorner)) {
    return;
  }
  DrawingRecorder recorder(context, item, DisplayItem::kScrollCorner, rect);

  GraphicsContextStateSaver state_saver(context);
  context.Translate(rect.x(), rect.y());
  gfx::Rect bounds(0, 0, rect.width(), rect.height());
  WebThemeEngine::ExtraParams params =
      GetPaintParams(*vertical_scrollbar, UsesOverlayScrollbars());
  mojom::blink::ColorScheme color_scheme =
      vertical_scrollbar->UsedColorScheme();
  WebThemeEngineHelper::GetNativeThemeEngine()->Paint(
      context.Canvas(), WebThemeEngine::Part::kPartScrollbarCorner,
      WebThemeEngine::State::kStateNormal, bounds, &params, color_scheme,
      vertical_scrollbar->InForcedColorsMode(),
      vertical_scrollbar->GetColorProvider(color_scheme));
}

void ScrollbarThemeMac::PaintThumb(GraphicsContext& context,
                                   const Scrollbar& scrollbar,
                                   const gfx::Rect& rect) {
  if (DrawingRecorder::UseCachedDrawingIfPossible(
          context, scrollbar, DisplayItem::kScrollbarThumb)) {
    return;
  }
  DrawingRecorder recorder(context, scrollbar, DisplayItem::kScrollbarThumb,
                           rect);

  GraphicsContextStateSaver state_saver(context);
  context.Translate(rect.x(), rect.y());

  if (!scrollbar.Enabled())
    return;

  auto* mac_scrollbar = MacScrollbar::GetForScrollbar(scrollbar);
  if (!mac_scrollbar)
    return;

  // The thumb size will be read from the ScrollbarPainter.
  const int thumb_size =
      mac_scrollbar->GetTrackBoxWidth() * scrollbar.ScaleFromDIP();

  WebThemeEngine::ExtraParams params =
      GetPaintParams(scrollbar, UsesOverlayScrollbars());
  const auto& scrollbar_extra =
      absl::get<WebThemeEngine::ScrollbarExtraParams>(params);

  // Compute the bounds for the thumb, accounting for lack of engorgement.
  gfx::Rect bounds;
  switch (scrollbar_extra.orientation) {
    case WebThemeEngine::ScrollbarOrientation::kVerticalOnRight:
      bounds =
          gfx::Rect(rect.width() - thumb_size, 0, thumb_size, rect.height());
      break;
    case WebThemeEngine::ScrollbarOrientation::kVerticalOnLeft:
      bounds = gfx::Rect(0, 0, thumb_size, rect.height());
      break;
    case WebThemeEngine::ScrollbarOrientation::kHorizontal:
      bounds =
          gfx::Rect(0, rect.height() - thumb_size, rect.width(), thumb_size);
      break;
  }

  WebThemeEngine::Part thumb_part =
      scrollbar_extra.orientation ==
              WebThemeEngine::ScrollbarOrientation::kHorizontal
          ? WebThemeEngine::Part::kPartScrollbarHorizontalThumb
          : WebThemeEngine::Part::kPartScrollbarVerticalThumb;
  mojom::blink::ColorScheme color_scheme = scrollbar.UsedColorScheme();
  WebThemeEngineHelper::GetNativeThemeEngine()->Paint(
      context.Canvas(), thumb_part, WebThemeEngine::State::kStateNormal, bounds,
      &params, color_scheme, scrollbar.InForcedColorsMode(),
      scrollbar.GetColorProvider(color_scheme));
}

int ScrollbarThemeMac::ScrollbarThickness(
    float scale_from_dip,
    EScrollbarWidth scrollbar_width) const {
  if (scrollbar_width == EScrollbarWidth::kNone)
    return 0;
  const auto& painter_values =
      GetScrollbarPainterValues(UsesOverlayScrollbars(), scrollbar_width);
  return painter_values.track_box_width * scale_from_dip;
}

bool ScrollbarThemeMac::UsesOverlayScrollbars() const {
  return PreferOverlayScrollerStyle();
}

bool ScrollbarThemeMac::HasThumb(const Scrollbar& scrollbar) const {
  const auto& painter_values = GetScrollbarPainterValues(scrollbar);
  int min_length_for_thumb =
      painter_values.knob_min_length + painter_values.track_overlap_end_inset +
      painter_values.knob_overlap_end_inset +
      2 * (painter_values.track_end_inset + painter_values.knob_end_inset);
  return scrollbar.Enabled() &&
         (scrollbar.Orientation() == kHorizontalScrollbar
              ? scrollbar.Width()
              : scrollbar.Height()) >= min_length_for_thumb;
}

gfx::Rect ScrollbarThemeMac::BackButtonRect(const Scrollbar& scrollbar) const {
  return gfx::Rect();
}

gfx::Rect ScrollbarThemeMac::ForwardButtonRect(
    const Scrollbar& scrollbar) const {
  return gfx::Rect();
}

gfx::Rect ScrollbarThemeMac::TrackRect(const Scrollbar& scrollbar) const {
  return scrollbar.FrameRect();
}

int ScrollbarThemeMac::MinimumThumbLength(const Scrollbar& scrollbar) const {
  const auto& painter_values = GetScrollbarPainterValues(scrollbar);
  return painter_values.knob_min_length;
}

void ScrollbarThemeMac::UpdateEnabledState(const Scrollbar& scrollbar) {
  if (auto* mac_scrollbar = MacScrollbar::GetForScrollbar(scrollbar))
    return mac_scrollbar->SetEnabled(scrollbar.Enabled());
}

float ScrollbarThemeMac::Opacity(const Scrollbar& scrollbar) const {
  if (auto* mac_scrollbar = MacScrollbar::GetForScrollbar(scrollbar))
    return mac_scrollbar->GetKnobAlpha();
  return 1.f;
}

bool ScrollbarThemeMac::JumpOnTrackClick() const {
  return s_jump_on_track_click;
}

// static
void ScrollbarThemeMac::UpdateScrollbarsWithNSDefaults(
    std::optional<float> initial_button_delay,
    std::optional<float> autoscroll_button_delay,
    bool prefer_overlay_scroller_style,
    bool redraw,
    bool jump_on_track_click) {
  s_initial_button_delay =
      initial_button_delay.value_or(s_initial_button_delay);
  s_autoscroll_button_delay =
      autoscroll_button_delay.value_or(s_autoscroll_button_delay);
  if (s_prefer_overlay_scroller_style != prefer_overlay_scroller_style) {
    s_prefer_overlay_scroller_style = prefer_overlay_scroller_style;
    Page::UsesOverlayScrollbarsChanged();
  }
  s_jump_on_track_click = jump_on_track_click;
  if (redraw) {
    for (const auto& scrollbar : GetScrollbarSet()) {
      scrollbar->StyleChanged();
      scrollbar->SetNeedsPaintInvalidation(kAllParts);
    }
  }
}

// static
bool ScrollbarThemeMac::PreferOverlayScrollerStyle() {
  if (OverlayScrollbarsEnabled())
    return true;
  return s_prefer_overlay_scroller_style;
}

}  // namespace blink
