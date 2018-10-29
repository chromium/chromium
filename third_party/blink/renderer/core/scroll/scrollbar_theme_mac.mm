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
#include "third_party/blink/renderer/platform/mac/local_current_graphics_context.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/retain_ptr.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

// FIXME: There are repainting problems due to Aqua scroll bar buttons' visual
// overflow.

@interface NSColor (WebNSColorDetails)
+ (NSImage*)_linenPatternImage;
@end

@interface BlinkScrollbarObserver : NSObject {
  blink::Scrollbar* _scrollbar;
  RetainPtr<ScrollbarPainter> _scrollbarPainter;
  BOOL _suppressSetScrollbarsHidden;
}
- (id)initWithScrollbar:(blink::Scrollbar*)scrollbar
                painter:(const RetainPtr<ScrollbarPainter>&)painter;
@end

@implementation BlinkScrollbarObserver

- (id)initWithScrollbar:(blink::Scrollbar*)scrollbar
                painter:(const RetainPtr<ScrollbarPainter>&)painter {
  if (!(self = [super init]))
    return nil;
  _scrollbar = scrollbar;
  _scrollbarPainter = painter;
  [_scrollbarPainter.Get() addObserver:self
                            forKeyPath:@"knobAlpha"
                               options:0
                               context:nil];
  return self;
}

- (id)painter {
  return _scrollbarPainter.Get();
}

- (void)setSuppressSetScrollbarsHidden:(BOOL)value {
  _suppressSetScrollbarsHidden = value;
}

- (void)dealloc {
  [_scrollbarPainter.Get() removeObserver:self forKeyPath:@"knobAlpha"];
  [super dealloc];
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  if ([keyPath isEqualToString:@"knobAlpha"]) {
    if (!_suppressSetScrollbarsHidden) {
      BOOL visible = [_scrollbarPainter.Get() knobAlpha] > 0;
      _scrollbar->SetScrollbarsHiddenIfOverlay(!visible);
    }
  }
}

@end

namespace blink {

typedef HeapHashSet<WeakMember<Scrollbar>> ScrollbarSet;

static ScrollbarSet& GetScrollbarSet() {
  DEFINE_STATIC_LOCAL(Persistent<ScrollbarSet>, set, (new ScrollbarSet));
  return *set;
}

typedef HeapHashMap<WeakMember<Scrollbar>, RetainPtr<BlinkScrollbarObserver>>
    ScrollbarPainterMap;

static ScrollbarPainterMap& GetScrollbarPainterMap() {
  DEFINE_STATIC_LOCAL(Persistent<ScrollbarPainterMap>, map,
                      (new ScrollbarPainterMap));
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
  WebScrollbarTheme::RegisterClient(*this);
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
         (WebScrollbarTheme::JumpOnTrackClick() != alt_key_pressed);
}

ScrollbarThemeMac::~ScrollbarThemeMac() {
  WebScrollbarTheme::UnregisterClient(*this);
}

TimeDelta ScrollbarThemeMac::InitialAutoscrollTimerDelay() {
  return TimeDelta::FromSecondsD(WebScrollbarTheme::InitialButtonDelay());
}

TimeDelta ScrollbarThemeMac::AutoscrollTimerDelay() {
  return TimeDelta::FromSecondsD(WebScrollbarTheme::AutoscrollButtonDelay());
}

bool ScrollbarThemeMac::ShouldDragDocumentInsteadOfThumb(
    const Scrollbar&,
    const WebMouseEvent& event) {
  return (event.GetModifiers() & WebInputEvent::Modifiers::kAltKey) != 0;
}

int ScrollbarThemeMac::ScrollbarPartToHIPressedState(ScrollbarPart part) {
  switch (part) {
    case kBackButtonStartPart:
      return kThemeTopOutsideArrowPressed;
    case kBackButtonEndPart:
      // This does not make much sense.  For some reason the outside constant
      // is required.
      return kThemeTopOutsideArrowPressed;
    case kForwardButtonStartPart:
      return kThemeTopInsideArrowPressed;
    case kForwardButtonEndPart:
      return kThemeBottomOutsideArrowPressed;
    case kThumbPart:
      return kThemeThumbPressed;
    default:
      return 0;
  }
}

ScrollbarPart ScrollbarThemeMac::InvalidateOnThumbPositionChange(
    const Scrollbar& scrollbar,
    float old_position,
    float new_position) const {
  // ScrollAnimatorMac will invalidate scrollbar parts if necessary.
  return kNoPart;
}

void ScrollbarThemeMac::RegisterScrollbar(Scrollbar& scrollbar) {
  GetScrollbarSet().insert(&scrollbar);

  bool is_horizontal = scrollbar.Orientation() == kHorizontalScrollbar;
  RetainPtr<ScrollbarPainter> scrollbar_painter(
      kAdoptNS,
      [[NSClassFromString(@"NSScrollerImp")
          scrollerImpWithStyle:RecommendedScrollerStyle()
                   controlSize:(NSControlSize)scrollbar.GetControlSize()
                    horizontal:is_horizontal
          replacingScrollerImp:nil] retain]);
  RetainPtr<BlinkScrollbarObserver> observer(
      kAdoptNS,
      [[BlinkScrollbarObserver alloc] initWithScrollbar:&scrollbar
                                                painter:scrollbar_painter]);

  GetScrollbarPainterMap().insert(&scrollbar, observer);
  UpdateEnabledState(scrollbar);
  UpdateScrollbarOverlayColorTheme(scrollbar);
}

void ScrollbarThemeMac::UnregisterScrollbar(Scrollbar& scrollbar) {
  GetScrollbarPainterMap().erase(&scrollbar);
  GetScrollbarSet().erase(&scrollbar);
}

void ScrollbarThemeMac::SetNewPainterForScrollbar(
    Scrollbar& scrollbar,
    ScrollbarPainter new_painter) {
  RetainPtr<ScrollbarPainter> scrollbar_painter(kAdoptNS, [new_painter retain]);
  RetainPtr<BlinkScrollbarObserver> observer(
      kAdoptNS,
      [[BlinkScrollbarObserver alloc] initWithScrollbar:&scrollbar
                                                painter:scrollbar_painter]);
  GetScrollbarPainterMap().Set(&scrollbar, observer);
  UpdateEnabledState(scrollbar);
  UpdateScrollbarOverlayColorTheme(scrollbar);
}

ScrollbarPainter ScrollbarThemeMac::PainterForScrollbar(
    const Scrollbar& scrollbar) const {
  return [GetScrollbarPainterMap()
              .at(const_cast<Scrollbar*>(&scrollbar))
              .Get() painter];
}

void ScrollbarThemeMac::PaintTrackBackground(GraphicsContext& context,
                                             const Scrollbar& scrollbar,
                                             const IntRect& rect) {
  if (DrawingRecorder::UseCachedDrawingIfPossible(
          context, scrollbar, DisplayItem::kScrollbarTrackBackground))
    return;

  DrawingRecorder recorder(context, scrollbar,
                           DisplayItem::kScrollbarTrackBackground);

  GraphicsContextStateSaver state_saver(context);
  context.Translate(rect.X(), rect.Y());
  LocalCurrentGraphicsContext local_context(context,
                                            IntRect(IntPoint(), rect.Size()));

  CGRect frame_rect = CGRect(scrollbar.FrameRect());
  ScrollbarPainter scrollbar_painter = PainterForScrollbar(scrollbar);
  [scrollbar_painter setEnabled:scrollbar.Enabled()];
  [scrollbar_painter setBoundsSize:NSSizeFromCGSize(frame_rect.size)];
  NSRect track_rect =
      NSMakeRect(0, 0, frame_rect.size.width, frame_rect.size.height);
  [scrollbar_painter drawKnobSlotInRect:track_rect highlight:NO];
}

void ScrollbarThemeMac::PaintThumbInternal(GraphicsContext& context,
                                           const Scrollbar& scrollbar,
                                           const IntRect& rect,
                                           float opacity) {
  if (DrawingRecorder::UseCachedDrawingIfPossible(context, scrollbar,
                                                  DisplayItem::kScrollbarThumb))
    return;

  DrawingRecorder recorder(context, scrollbar, DisplayItem::kScrollbarThumb);

  GraphicsContextStateSaver state_saver(context);
  context.Translate(rect.X(), rect.Y());
  IntRect local_rect(IntPoint(), rect.Size());

  if (opacity != 1.0f) {
    FloatRect float_local_rect(local_rect);
    context.BeginLayer(opacity, SkBlendMode::kSrcOver, &float_local_rect);
  }

  {
    LocalCurrentGraphicsContext local_context(context, local_rect);
    RetainPtr<BlinkScrollbarObserver> observer =
        GetScrollbarPainterMap().at(const_cast<Scrollbar*>(&scrollbar));
    ScrollbarPainter scrollbar_painter = [observer.Get() painter];
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

    [observer.Get() setSuppressSetScrollbarsHidden:YES];
    CGFloat old_knob_alpha = [scrollbar_painter knobAlpha];
    [scrollbar_painter setKnobAlpha:1];

    if (scrollbar.Enabled())
      [scrollbar_painter drawKnob];

    // If this state is not set, then moving the cursor over the scrollbar area
    // will only cause the scrollbar to engorge when moved over the top of the
    // scrollbar area.
    [scrollbar_painter
        setBoundsSize:NSSizeFromCGSize(CGSize(scrollbar.FrameRect().Size()))];
    [scrollbar_painter setKnobAlpha:old_knob_alpha];
    [observer.Get() setSuppressSetScrollbarsHidden:NO];
  }

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
                                          ScrollbarPart part,
                                          bool painting) {
  DCHECK_EQ(ButtonsPlacement(), kWebScrollbarButtonsPlacementNone);
  return IntRect();
}

IntRect ScrollbarThemeMac::ForwardButtonRect(const Scrollbar& scrollbar,
                                             ScrollbarPart part,
                                             bool painting) {
  DCHECK_EQ(ButtonsPlacement(), kWebScrollbarButtonsPlacementNone);
  return IntRect();
}

IntRect ScrollbarThemeMac::TrackRect(const Scrollbar& scrollbar,
                                     bool painting) {
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

void ScrollbarThemeMac::PreferencesChanged() {
  for (const auto& scrollbar : GetScrollbarSet()) {
    scrollbar->StyleChanged();
    scrollbar->SetNeedsPaintInvalidation(kAllParts);
  }
}

// static
NSScrollerStyle ScrollbarThemeMac::RecommendedScrollerStyle() {
  if (RuntimeEnabledFeatures::OverlayScrollbarsEnabled())
    return NSScrollerStyleOverlay;
  return static_cast<NSScrollerStyle>(
      WebScrollbarTheme::PreferredScrollerStyle());
}

}  // namespace blink
