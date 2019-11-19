/*
 * Copyright (c) 2008, 2009, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT{
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,{
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/scroll/scrollbar_theme_aura.h"

#include "build/build_config.h"
#include "cc/input/scrollbar.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_mouse_event.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_theme_engine.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scrollbar.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme_overlay.h"
#include "third_party/blink/renderer/platform/geometry/int_rect_outsets.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/web_test_support.h"

namespace blink {

namespace {

// Use fixed scrollbar thickness for web_tests because many tests are
// expecting that. Rebaselining is relatively easy for platform differences,
// but tens of testharness tests will fail without this on Windows.
// TODO(crbug.com/953847): Adapt testharness tests to native themes and remove
// this.
constexpr int kScrollbarThicknessForWebTests = 15;

// Contains a flag indicating whether WebThemeEngine should paint a UI widget
// for a scrollbar part, and if so, what part and state apply.
//
// If the PartPaintingParams are not affected by a change in the scrollbar
// state, then the corresponding scrollbar part does not need to be repainted.
struct PartPaintingParams {
  PartPaintingParams()
      : should_paint(false),
        part(WebThemeEngine::kPartScrollbarDownArrow),
        state(WebThemeEngine::kStateNormal) {}
  PartPaintingParams(WebThemeEngine::Part part, WebThemeEngine::State state)
      : should_paint(true), part(part), state(state) {}

  bool should_paint;
  WebThemeEngine::Part part;
  WebThemeEngine::State state;
};

bool operator==(const PartPaintingParams& a, const PartPaintingParams& b) {
  return (!a.should_paint && !b.should_paint) ||
         std::tie(a.should_paint, a.part, a.state) ==
             std::tie(b.should_paint, b.part, b.state);
}

bool operator!=(const PartPaintingParams& a, const PartPaintingParams& b) {
  return !(a == b);
}

PartPaintingParams ButtonPartPaintingParams(const Scrollbar& scrollbar,
                                            float position,
                                            ScrollbarPart part) {
  WebThemeEngine::Part paint_part;
  WebThemeEngine::State state = WebThemeEngine::kStateNormal;
  bool check_min = false;
  bool check_max = false;

  if (scrollbar.Orientation() == kHorizontalScrollbar) {
    if (part == kBackButtonStartPart) {
      paint_part = WebThemeEngine::kPartScrollbarLeftArrow;
      check_min = true;
    } else {
      paint_part = WebThemeEngine::kPartScrollbarRightArrow;
      check_max = true;
    }
  } else {
    if (part == kBackButtonStartPart) {
      paint_part = WebThemeEngine::kPartScrollbarUpArrow;
      check_min = true;
    } else {
      paint_part = WebThemeEngine::kPartScrollbarDownArrow;
      check_max = true;
    }
  }

  if ((check_min && (position <= 0)) ||
      (check_max && position >= scrollbar.Maximum())) {
    state = WebThemeEngine::kStateDisabled;
  } else {
    if (part == scrollbar.PressedPart())
      state = WebThemeEngine::kStatePressed;
    else if (part == scrollbar.HoveredPart())
      state = WebThemeEngine::kStateHover;
  }

  return PartPaintingParams(paint_part, state);
}

}  // namespace

ScrollbarTheme& ScrollbarTheme::NativeTheme() {
  if (OverlayScrollbarsEnabled())
    return ScrollbarThemeOverlay::GetInstance();

  DEFINE_STATIC_LOCAL(ScrollbarThemeAura, theme, ());
  return theme;
}

bool ScrollbarThemeAura::SupportsDragSnapBack() const {
// Disable snapback on desktop Linux to better integrate with the desktop
// behavior. Typically, Linux apps do not implement scrollbar snapback (this
// is true for at least GTK and QT apps).
#if (defined(OS_LINUX) && !defined(OS_CHROMEOS))
  return false;
#endif

  return true;
}

int ScrollbarThemeAura::ScrollbarThickness(ScrollbarControlSize control_size) {
  if (WebTestSupport::IsRunningWebTest())
    return kScrollbarThicknessForWebTests;

  // Horiz and Vert scrollbars are the same thickness.
  IntSize scrollbar_size = Platform::Current()->ThemeEngine()->GetSize(
      WebThemeEngine::kPartScrollbarVerticalTrack);
  return scrollbar_size.Width();
}

bool ScrollbarThemeAura::HasThumb(const Scrollbar& scrollbar) {
  // This method is just called as a paint-time optimization to see if
  // painting the thumb can be skipped. We don't have to be exact here.
  return ThumbLength(scrollbar) > 0;
}

IntRect ScrollbarThemeAura::BackButtonRect(const Scrollbar& scrollbar,
                                           ScrollbarPart part) {
  // Windows and Linux just have single arrows.
  if (part == kBackButtonEndPart)
    return IntRect();

  IntSize size = ButtonSize(scrollbar);
  return IntRect(scrollbar.X(), scrollbar.Y(), size.Width(), size.Height());
}

IntRect ScrollbarThemeAura::ForwardButtonRect(const Scrollbar& scrollbar,
                                              ScrollbarPart part) {
  // Windows and Linux just have single arrows.
  if (part == kForwardButtonStartPart)
    return IntRect();

  IntSize size = ButtonSize(scrollbar);
  int x, y;
  if (scrollbar.Orientation() == kHorizontalScrollbar) {
    x = scrollbar.X() + scrollbar.Width() - size.Width();
    y = scrollbar.Y();
  } else {
    x = scrollbar.X();
    y = scrollbar.Y() + scrollbar.Height() - size.Height();
  }
  return IntRect(x, y, size.Width(), size.Height());
}

IntRect ScrollbarThemeAura::TrackRect(const Scrollbar& scrollbar) {
  // The track occupies all space between the two buttons.
  IntSize bs = ButtonSize(scrollbar);
  if (scrollbar.Orientation() == kHorizontalScrollbar) {
    if (scrollbar.Width() <= 2 * bs.Width())
      return IntRect();
    return IntRect(scrollbar.X() + bs.Width(), scrollbar.Y(),
                   scrollbar.Width() - 2 * bs.Width(), scrollbar.Height());
  }
  if (scrollbar.Height() <= 2 * bs.Height())
    return IntRect();
  return IntRect(scrollbar.X(), scrollbar.Y() + bs.Height(), scrollbar.Width(),
                 scrollbar.Height() - 2 * bs.Height());
}

int ScrollbarThemeAura::MinimumThumbLength(const Scrollbar& scrollbar) {
  if (scrollbar.Orientation() == kVerticalScrollbar) {
    return Platform::Current()
        ->ThemeEngine()
        ->GetSize(WebThemeEngine::kPartScrollbarVerticalThumb)
        .height;
  }

  return Platform::Current()
      ->ThemeEngine()
      ->GetSize(WebThemeEngine::kPartScrollbarHorizontalThumb)
      .width;
}

void ScrollbarThemeAura::PaintTrack(GraphicsContext& context,
                                    const Scrollbar& scrollbar,
                                    const IntRect& rect) {
  if (rect.IsEmpty())
    return;

  // We always paint the track as a single piece, so don't support hover state
  // of the back track and forward track.
  auto state = WebThemeEngine::kStateNormal;

  // TODO(wangxianzhu): The extra params for scrollbar track were for painting
  // back and forward tracks separately, which we don't support. Remove them.
  IntRect align_rect = TrackRect(scrollbar);
  WebThemeEngine::ExtraParams extra_params;
  extra_params.scrollbar_track.is_back = false;
  extra_params.scrollbar_track.track_x = align_rect.X();
  extra_params.scrollbar_track.track_y = align_rect.Y();
  extra_params.scrollbar_track.track_width = align_rect.Width();
  extra_params.scrollbar_track.track_height = align_rect.Height();

  Platform::Current()->ThemeEngine()->Paint(
      context.Canvas(),
      scrollbar.Orientation() == kHorizontalScrollbar
          ? WebThemeEngine::kPartScrollbarHorizontalTrack
          : WebThemeEngine::kPartScrollbarVerticalTrack,
      state, WebRect(rect), &extra_params, scrollbar.UsedColorScheme());
}

void ScrollbarThemeAura::PaintButton(GraphicsContext& gc,
                                     const Scrollbar& scrollbar,
                                     const IntRect& rect,
                                     ScrollbarPart part) {
  PartPaintingParams params =
      ButtonPartPaintingParams(scrollbar, scrollbar.CurrentPos(), part);
  if (!params.should_paint)
    return;

  WebThemeEngine::ExtraParams extra_params;
  extra_params.scrollbar_button.zoom = scrollbar.EffectiveZoom();
  extra_params.scrollbar_button.right_to_left =
      scrollbar.ContainerIsRightToLeft();
  Platform::Current()->ThemeEngine()->Paint(
      gc.Canvas(), params.part, params.state, WebRect(rect), &extra_params,
      scrollbar.UsedColorScheme());
}

void ScrollbarThemeAura::PaintThumb(GraphicsContext& gc,
                                    const Scrollbar& scrollbar,
                                    const IntRect& rect) {
  if (DrawingRecorder::UseCachedDrawingIfPossible(gc, scrollbar,
                                                  DisplayItem::kScrollbarThumb))
    return;

  DrawingRecorder recorder(gc, scrollbar, DisplayItem::kScrollbarThumb);

  WebThemeEngine::State state;
  cc::PaintCanvas* canvas = gc.Canvas();
  if (scrollbar.PressedPart() == kThumbPart)
    state = WebThemeEngine::kStatePressed;
  else if (scrollbar.HoveredPart() == kThumbPart)
    state = WebThemeEngine::kStateHover;
  else
    state = WebThemeEngine::kStateNormal;

  Platform::Current()->ThemeEngine()->Paint(
      canvas,
      scrollbar.Orientation() == kHorizontalScrollbar
          ? WebThemeEngine::kPartScrollbarHorizontalThumb
          : WebThemeEngine::kPartScrollbarVerticalThumb,
      state, WebRect(rect), nullptr, scrollbar.UsedColorScheme());
}

bool ScrollbarThemeAura::ShouldRepaintAllPartsOnInvalidation() const {
  // This theme can separately handle thumb invalidation.
  return false;
}

ScrollbarPart ScrollbarThemeAura::PartsToInvalidateOnThumbPositionChange(
    const Scrollbar& scrollbar,
    float old_position,
    float new_position) const {
  ScrollbarPart invalid_parts = kNoPart;
  DCHECK_EQ(ButtonsPlacement(), kWebScrollbarButtonsPlacementSingle);
  static const ScrollbarPart kButtonParts[] = {kBackButtonStartPart,
                                               kForwardButtonEndPart};
  for (ScrollbarPart part : kButtonParts) {
    if (ButtonPartPaintingParams(scrollbar, old_position, part) !=
        ButtonPartPaintingParams(scrollbar, new_position, part))
      invalid_parts = static_cast<ScrollbarPart>(invalid_parts | part);
  }
  return invalid_parts;
}

bool ScrollbarThemeAura::ShouldCenterOnThumb(const Scrollbar& scrollbar,
                                             const WebMouseEvent& event) {
#if (defined(OS_LINUX) && !defined(OS_CHROMEOS))
  if (event.button == WebPointerProperties::Button::kMiddle)
    return true;
#endif
  bool shift_key_pressed = event.GetModifiers() & WebInputEvent::kShiftKey;
  return (event.button == WebPointerProperties::Button::kLeft) &&
         shift_key_pressed;
}

bool ScrollbarThemeAura::ShouldSnapBackToDragOrigin(
    const Scrollbar& scrollbar,
    const WebMouseEvent& event) {
  if (!SupportsDragSnapBack())
    return false;

  // There is a drag rect around the scrollbar outside of which the scrollbar
  // thumb should snap back to its origin.  This rect is infinitely large in
  // the scrollbar's scrolling direction and an expansion of the scrollbar's
  // width or height in the non-scrolling direction. As only one axis triggers
  // snapping back, the code below only uses the thickness of the scrollbar for
  // its calculations.
  bool is_horizontal = scrollbar.Orientation() == kHorizontalScrollbar;
  int thickness = is_horizontal ? TrackRect(scrollbar).Height()
                                : TrackRect(scrollbar).Width();
  // Even if the platform's scrollbar is narrower than the default Windows one,
  // we still want to provide at least as much slop area, since a slightly
  // narrower scrollbar doesn't necessarily imply that users will drag
  // straighter.
  int expansion_amount =
      kOffSideMultiplier * std::max(thickness, kDefaultWinScrollbarThickness);

  int snap_outside_of_min = -expansion_amount;
  int snap_outside_of_max = expansion_amount + thickness;

  IntPoint mouse_position = scrollbar.ConvertFromRootFrame(
      FlooredIntPoint(event.PositionInRootFrame()));
  int mouse_offset_in_scrollbar =
      is_horizontal ? mouse_position.Y() : mouse_position.X();

  return (mouse_offset_in_scrollbar < snap_outside_of_min ||
          mouse_offset_in_scrollbar >= snap_outside_of_max);
}

bool ScrollbarThemeAura::HasScrollbarButtons(
    ScrollbarOrientation orientation) const {
  WebThemeEngine* theme_engine = Platform::Current()->ThemeEngine();
  if (orientation == kVerticalScrollbar) {
    return !theme_engine->GetSize(WebThemeEngine::kPartScrollbarDownArrow)
                .IsEmpty();
  }
  return !theme_engine->GetSize(WebThemeEngine::kPartScrollbarLeftArrow)
              .IsEmpty();
}

IntSize ScrollbarThemeAura::ButtonSize(const Scrollbar& scrollbar) {
  if (!HasScrollbarButtons(scrollbar.Orientation()))
    return IntSize(0, 0);

  if (scrollbar.Orientation() == kVerticalScrollbar) {
    int square_size = scrollbar.Width();
    return IntSize(square_size, scrollbar.Height() < 2 * square_size
                                    ? scrollbar.Height() / 2
                                    : square_size);
  }

  // HorizontalScrollbar
  int square_size = scrollbar.Height();
  return IntSize(
      scrollbar.Width() < 2 * square_size ? scrollbar.Width() / 2 : square_size,
      square_size);
}

}  // namespace blink
