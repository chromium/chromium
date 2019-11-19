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

#include <Carbon/Carbon.h>
#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_policy.h"
#include "skia/ext/skia_utils_mac.h"
#include "third_party/blink/public/platform/mac/web_scrollbar_theme.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_mouse_event.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_theme_engine.h"
#include "third_party/blink/renderer/core/scroll/ns_scroller_imp_details.h"
#include "third_party/blink/renderer/core/scroll/scroll_animator_mac.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/mac/color_mac.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "ui/gfx/mac/cocoa_scrollbar_painter.h"

using gfx::CocoaScrollbarPainter;

@interface BlinkScrollbarObserver : NSObject {
  blink::Scrollbar* _scrollbar;
  base::scoped_nsobject<ScrollbarPainter> _scrollbarPainter;
  BOOL _suppressSetScrollbarsHidden;
  CGFloat _saved_knob_alpha;
}
- (id)initWithScrollbar:(blink::Scrollbar*)scrollbar
                painter:(const base::scoped_nsobject<ScrollbarPainter>&)painter;
@end

@implementation BlinkScrollbarObserver

- (id)initWithScrollbar:(blink::Scrollbar*)scrollbar
                painter:
                    (const base::scoped_nsobject<ScrollbarPainter>&)painter {
  if (!(self = [super init]))
    return nil;
  _scrollbar = scrollbar;
  _scrollbarPainter = painter;
  [_scrollbarPainter addObserver:self
                      forKeyPath:@"knobAlpha"
                         options:0
                         context:nil];
  return self;
}

- (id)painter {
  return _scrollbarPainter;
}

- (void)setSuppressSetScrollbarsHidden:(BOOL)value {
  _suppressSetScrollbarsHidden = value;
  if (value) {
    _saved_knob_alpha = [_scrollbarPainter knobAlpha];
  } else {
    [_scrollbarPainter setKnobAlpha:_saved_knob_alpha];
    _scrollbar->SetScrollbarsHiddenIfOverlay(_saved_knob_alpha == 0);
  }
}

- (void)dealloc {
  [_scrollbarPainter removeObserver:self forKeyPath:@"knobAlpha"];
  [super dealloc];
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  if ([keyPath isEqualToString:@"knobAlpha"]) {
    if (!_suppressSetScrollbarsHidden) {
      BOOL visible = [_scrollbarPainter knobAlpha] > 0;
      _scrollbar->SetScrollbarsHiddenIfOverlay(!visible);
    }
  }
}

@end

namespace blink {

static float s_initial_button_delay = 0.5f;
static float s_autoscroll_button_delay = 0.05f;
static NSScrollerStyle s_preferred_scroller_style = NSScrollerStyleLegacy;
static bool s_jump_on_track_click = false;

typedef HeapHashSet<WeakMember<Scrollbar>> ScrollbarSet;

static ScrollbarSet& GetScrollbarSet() {
  DEFINE_STATIC_LOCAL(Persistent<ScrollbarSet>, set,
                      (MakeGarbageCollected<ScrollbarSet>()));
  return *set;
}

typedef HeapHashMap<WeakMember<Scrollbar>,
                    base::scoped_nsobject<BlinkScrollbarObserver>>
    ScrollbarPainterMap;

static ScrollbarPainterMap& GetScrollbarPainterMap() {
  DEFINE_STATIC_LOCAL(Persistent<ScrollbarPainterMap>, map,
                      (MakeGarbageCollected<ScrollbarPainterMap>()));
  return *map;
}

static bool SupportsExpandedScrollbars() {
  // FIXME: This is temporary until all platforms that support ScrollbarPainter
  // support this part of the API.
  static bool global_supports_expanded_scrollbars =
      [NSClassFromString(@"NSScrollerImp")
          instancesRespondToSelector:@selector(setExpanded:)];
  return global_supports_expanded_scrollbars;
}

ScrollbarThemeMac::ScrollbarThemeMac() {
}

ScrollbarTheme& ScrollbarTheme::NativeTheme() {
  DEFINE_STATIC_LOCAL(ScrollbarThemeMac, overlay_theme, ());
  return overlay_theme;
}

void ScrollbarThemeMac::PaintTickmarks(GraphicsContext& context,
                                       const Scrollbar& scrollbar,
                                       const IntRect& rect) {
  IntRect tickmark_track_rect = rect;
  tickmark_track_rect.SetX(tickmark_track_rect.X() + 1);
  tickmark_track_rect.SetWidth(tickmark_track_rect.Width() - 1);
  ScrollbarTheme::PaintTickmarks(context, scrollbar, tickmark_track_rect);
}

bool ScrollbarThemeMac::ShouldCenterOnThumb(const Scrollbar& scrollbar,
                                            const WebMouseEvent& event) {
  bool alt_key_pressed = event.GetModifiers() & WebInputEvent::kAltKey;
  return (event.button == WebPointerProperties::Button::kLeft) &&
         (s_jump_on_track_click != alt_key_pressed);
}

ScrollbarThemeMac::~ScrollbarThemeMac() {
}

base::TimeDelta ScrollbarThemeMac::InitialAutoscrollTimerDelay() {
  return base::TimeDelta::FromSecondsD(s_initial_button_delay);
}

base::TimeDelta ScrollbarThemeMac::AutoscrollTimerDelay() {
  return base::TimeDelta::FromSecondsD(s_autoscroll_button_delay);
}

bool ScrollbarThemeMac::ShouldDragDocumentInsteadOfThumb(
    const Scrollbar&,
    const WebMouseEvent& event) {
  return (event.GetModifiers() & WebInputEvent::Modifiers::kAltKey) != 0;
}

ScrollbarPart ScrollbarThemeMac::PartsToInvalidateOnThumbPositionChange(
    const Scrollbar& scrollbar,
    float old_position,
    float new_position) const {
  // ScrollAnimatorMac will invalidate scrollbar parts if necessary.
  return kNoPart;
}

void ScrollbarThemeMac::RegisterScrollbar(Scrollbar& scrollbar) {
  GetScrollbarSet().insert(&scrollbar);

  bool is_horizontal = scrollbar.Orientation() == kHorizontalScrollbar;
  base::scoped_nsobject<ScrollbarPainter> scrollbar_painter(
      [[NSClassFromString(@"NSScrollerImp")
          scrollerImpWithStyle:RecommendedScrollerStyle()
                   controlSize:(NSControlSize)scrollbar.GetControlSize()
                    horizontal:is_horizontal
          replacingScrollerImp:nil] retain]);
  base::scoped_nsobject<BlinkScrollbarObserver> observer(
      [[BlinkScrollbarObserver alloc] initWithScrollbar:&scrollbar
                                                painter:scrollbar_painter]);

  GetScrollbarPainterMap().insert(&scrollbar, observer);
  UpdateEnabledState(scrollbar);
  UpdateScrollbarOverlayColorTheme(scrollbar);
}

void ScrollbarThemeMac::SetNewPainterForScrollbar(
    Scrollbar& scrollbar,
    ScrollbarPainter new_painter) {
  base::scoped_nsobject<ScrollbarPainter> scrollbar_painter(
      [new_painter retain]);
  base::scoped_nsobject<BlinkScrollbarObserver> observer(
      [[BlinkScrollbarObserver alloc] initWithScrollbar:&scrollbar
                                                painter:scrollbar_painter]);
  GetScrollbarPainterMap().Set(&scrollbar, observer);
  UpdateEnabledState(scrollbar);
  UpdateScrollbarOverlayColorTheme(scrollbar);
}

ScrollbarPainter ScrollbarThemeMac::PainterForScrollbar(
    const Scrollbar& scrollbar) const {
  return
      [GetScrollbarPainterMap().at(const_cast<Scrollbar*>(&scrollbar)) painter];
}

CocoaScrollbarPainter::Params GetPaintParams(const Scrollbar& scrollbar,
                                             bool overlay) {
  CocoaScrollbarPainter::Params params;
  params.orientation = CocoaScrollbarPainter::Orientation::kVerticalOnRight;
  if (scrollbar.Orientation() == kHorizontalScrollbar)
    params.orientation = CocoaScrollbarPainter::Orientation::kHorizontal;
  if (scrollbar.IsLeftSideVerticalScrollbar())
    params.orientation = CocoaScrollbarPainter::Orientation::kVerticalOnLeft;

  params.overlay = overlay;
  // Only enable dark mode for overlay scrollbars (for now).
  if (overlay)
    params.dark_mode = scrollbar.GetScrollbarOverlayColorTheme() ==
                       kScrollbarOverlayColorThemeLight;
  params.hovered = scrollbar.HoveredPart() != ScrollbarPart::kNoPart;
  return params;
}

void ScrollbarThemeMac::PaintTrack(GraphicsContext& context,
                                   const Scrollbar& scrollbar,
                                   const IntRect& rect) {
  GraphicsContextStateSaver state_saver(context);
  context.Translate(rect.X(), rect.Y());

  // The track opacity will be read from the ScrollbarPainter.
  float opacity = 1.f;

  // The following incantations are done to update the state of the
  // ScrollbarPainter in ways that are unknown. It is important to leave
  // these in place because we use ScrollbarPainter to populate |opacity|
  // and because the ScrollAnimator doesn't animate correctly without them.
  {
    CGRect frame_rect = CGRect(scrollbar.FrameRect());
    ScrollbarPainter scrollbar_painter = PainterForScrollbar(scrollbar);
    [scrollbar_painter setEnabled:scrollbar.Enabled()];
    [scrollbar_painter setBoundsSize:NSSizeFromCGSize(frame_rect.size)];
    opacity = [scrollbar_painter trackAlpha];
  }

  if (opacity == 0)
    return;

  if (opacity != 1)
    context.BeginLayer(opacity);
  CocoaScrollbarPainter::Params params =
      GetPaintParams(scrollbar, UsesOverlayScrollbars());
  SkIRect bounds = SkIRect::MakeXYWH(0, 0, scrollbar.FrameRect().Width(),
                                     scrollbar.FrameRect().Height());
  CocoaScrollbarPainter::PaintTrack(context.Canvas(), bounds, params);
  if (opacity != 1)
    context.EndLayer();
}

void ScrollbarThemeMac::PaintScrollCorner(GraphicsContext& context,
                                          const Scrollbar* vertical_scrollbar,
                                          const DisplayItemClient& item,
                                          const IntRect& rect,
                                          WebColorScheme color_scheme) {
  if (!vertical_scrollbar) {
    ScrollbarTheme::PaintScrollCorner(context, vertical_scrollbar, item, rect,
                                      color_scheme);
    return;
  }
  if (DrawingRecorder::UseCachedDrawingIfPossible(context, item,
                                                  DisplayItem::kScrollCorner)) {
    return;
  }
  DrawingRecorder recorder(context, item, DisplayItem::kScrollCorner);

  GraphicsContextStateSaver state_saver(context);
  context.Translate(rect.X(), rect.Y());
  SkIRect bounds = SkIRect::MakeXYWH(0, 0, rect.Width(), rect.Height());
  CocoaScrollbarPainter::Params params =
      GetPaintParams(*vertical_scrollbar, UsesOverlayScrollbars());
  CocoaScrollbarPainter::PaintCorner(context.Canvas(), bounds, params);
}

void ScrollbarThemeMac::PaintThumbInternal(GraphicsContext& context,
                                           const Scrollbar& scrollbar,
                                           const IntRect& rect,
                                           float opacity) {
  if (DrawingRecorder::UseCachedDrawingIfPossible(
          context, scrollbar, DisplayItem::kScrollbarThumb)) {
    return;
  }
  DrawingRecorder recorder(context, scrollbar, DisplayItem::kScrollbarThumb);

  GraphicsContextStateSaver state_saver(context);
  context.Translate(rect.X(), rect.Y());
  IntRect local_rect(IntPoint(), rect.Size());

  // The thumb size will be read from the ScrollbarPainter.
  int thumb_size = 0;

  // The following incantations are done to update the state of the
  // ScrollbarPainter in ways that are unknown. It is important to leave
  // these in place because we use ScrollbarPainter to populate |thumb_size|
  // and because the ScrollAnimator doesn't animate correctly without them.
  {
    base::scoped_nsobject<BlinkScrollbarObserver> observer(
        GetScrollbarPainterMap().at(const_cast<Scrollbar*>(&scrollbar)),
        base::scoped_policy::RETAIN);
    ScrollbarPainter scrollbar_painter = [observer painter];
    [scrollbar_painter setEnabled:scrollbar.Enabled()];
    // drawKnob aligns the thumb to right side of the draw rect.
    // If the vertical overlay scrollbar is on the left, use trackWidth instead
    // of scrollbar width, to avoid the gap on the left side of the thumb.
    IntRect draw_rect = IntRect(rect);
    if (UsesOverlayScrollbars() && scrollbar.IsLeftSideVerticalScrollbar()) {
      int thumb_width = [scrollbar_painter trackWidth];
      draw_rect.SetWidth(thumb_width);
    }
    [scrollbar_painter
        setBoundsSize:NSSizeFromCGSize(CGSize(draw_rect.Size()))];

    [scrollbar_painter setDoubleValue:0];
    [scrollbar_painter setKnobProportion:1];
    [observer setSuppressSetScrollbarsHidden:YES];
    [scrollbar_painter setKnobAlpha:1];

    // If this state is not set, then moving the cursor over the scrollbar area
    // will only cause the scrollbar to engorge when moved over the top of the
    // scrollbar area.
    [scrollbar_painter
        setBoundsSize:NSSizeFromCGSize(CGSize(scrollbar.FrameRect().Size()))];
    [observer setSuppressSetScrollbarsHidden:NO];

    thumb_size = [scrollbar_painter trackBoxWidth];
  }

  if (!scrollbar.Enabled())
    return;

  CocoaScrollbarPainter::Params params =
      GetPaintParams(scrollbar, UsesOverlayScrollbars());

  // Compute the bounds for the thumb, accounting for lack of engorgement.
  SkIRect bounds;
  switch (params.orientation) {
    case CocoaScrollbarPainter::Orientation::kVerticalOnRight:
      bounds = SkIRect::MakeXYWH(rect.Width() - thumb_size, 0, thumb_size,
                                 rect.Height());
      break;
    case CocoaScrollbarPainter::Orientation::kVerticalOnLeft:
      bounds = SkIRect::MakeXYWH(0, 0, thumb_size, rect.Height());
      break;
    case CocoaScrollbarPainter::Orientation::kHorizontal:
      bounds = SkIRect::MakeXYWH(0, rect.Height() - thumb_size, rect.Width(),
                                 thumb_size);
      break;
  }

  if (opacity != 1.0f) {
    FloatRect float_local_rect(local_rect);
    context.BeginLayer(opacity, SkBlendMode::kSrcOver, &float_local_rect);
  }
  CocoaScrollbarPainter::PaintThumb(context.Canvas(), bounds, params);
  if (opacity != 1.0f)
    context.EndLayer();
}

int ScrollbarThemeMac::ScrollbarThickness(ScrollbarControlSize control_size) {
  NSControlSize ns_control_size = static_cast<NSControlSize>(control_size);
  ScrollbarPainter scrollbar_painter = [NSClassFromString(@"NSScrollerImp")
      scrollerImpWithStyle:RecommendedScrollerStyle()
               controlSize:ns_control_size
                horizontal:NO
      replacingScrollerImp:nil];
  BOOL was_expanded = NO;
  if (SupportsExpandedScrollbars()) {
    was_expanded = [scrollbar_painter isExpanded];
    [scrollbar_painter setExpanded:YES];
  }
  int thickness = [scrollbar_painter trackBoxWidth];
  if (SupportsExpandedScrollbars())
    [scrollbar_painter setExpanded:was_expanded];
  return thickness;
}

bool ScrollbarThemeMac::UsesOverlayScrollbars() const {
  return RecommendedScrollerStyle() == NSScrollerStyleOverlay;
}

void ScrollbarThemeMac::UpdateScrollbarOverlayColorTheme(
    const Scrollbar& scrollbar) {
  ScrollbarPainter painter = PainterForScrollbar(scrollbar);
  switch (scrollbar.GetScrollbarOverlayColorTheme()) {
    case kScrollbarOverlayColorThemeDark:
      [painter setKnobStyle:NSScrollerKnobStyleDark];
      break;
    case kScrollbarOverlayColorThemeLight:
      [painter setKnobStyle:NSScrollerKnobStyleLight];
      break;
  }
}

WebScrollbarButtonsPlacement ScrollbarThemeMac::ButtonsPlacement() const {
  return kWebScrollbarButtonsPlacementNone;
}

bool ScrollbarThemeMac::HasThumb(const Scrollbar& scrollbar) {
  ScrollbarPainter painter = PainterForScrollbar(scrollbar);
  int min_length_for_thumb =
      [painter knobMinLength] + [painter trackOverlapEndInset] +
      [painter knobOverlapEndInset] +
      2 * ([painter trackEndInset] + [painter knobEndInset]);
  return scrollbar.Enabled() &&
         (scrollbar.Orientation() == kHorizontalScrollbar
              ? scrollbar.Width()
              : scrollbar.Height()) >= min_length_for_thumb;
}

IntRect ScrollbarThemeMac::BackButtonRect(const Scrollbar& scrollbar,
                                          ScrollbarPart part) {
  DCHECK_EQ(ButtonsPlacement(), kWebScrollbarButtonsPlacementNone);
  return IntRect();
}

IntRect ScrollbarThemeMac::ForwardButtonRect(const Scrollbar& scrollbar,
                                             ScrollbarPart part) {
  DCHECK_EQ(ButtonsPlacement(), kWebScrollbarButtonsPlacementNone);
  return IntRect();
}

IntRect ScrollbarThemeMac::TrackRect(const Scrollbar& scrollbar) {
  DCHECK(!HasButtons(scrollbar));
  return scrollbar.FrameRect();
}

int ScrollbarThemeMac::MinimumThumbLength(const Scrollbar& scrollbar) {
  return [PainterForScrollbar(scrollbar) knobMinLength];
}

void ScrollbarThemeMac::UpdateEnabledState(const Scrollbar& scrollbar) {
  [PainterForScrollbar(scrollbar) setEnabled:scrollbar.Enabled()];
}

float ScrollbarThemeMac::ThumbOpacity(const Scrollbar& scrollbar) const {
  ScrollbarPainter scrollbar_painter = PainterForScrollbar(scrollbar);
  return [scrollbar_painter knobAlpha];
}

// static
void ScrollbarThemeMac::UpdateScrollbarsWithNSDefaults(
    base::Optional<float> initial_button_delay,
    base::Optional<float> autoscroll_button_delay,
    NSScrollerStyle preferred_scroller_style,
    bool redraw,
    bool jump_on_track_click) {
  s_initial_button_delay =
      initial_button_delay.value_or(s_initial_button_delay);
  s_autoscroll_button_delay =
      autoscroll_button_delay.value_or(s_autoscroll_button_delay);
  s_preferred_scroller_style = preferred_scroller_style;
  s_jump_on_track_click = jump_on_track_click;
  if (redraw) {
    for (const auto& scrollbar : GetScrollbarSet()) {
      scrollbar->StyleChanged();
      scrollbar->SetNeedsPaintInvalidation(kAllParts);
    }
  }
}

// static
NSScrollerStyle ScrollbarThemeMac::RecommendedScrollerStyle() {
  if (OverlayScrollbarsEnabled())
    return NSScrollerStyleOverlay;
  return s_preferred_scroller_style;
}

}  // namespace blink
