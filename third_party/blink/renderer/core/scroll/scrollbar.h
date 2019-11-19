/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLLBAR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLLBAR_H_

#include "third_party/blink/public/platform/web_color_scheme.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

class Element;
class GraphicsContext;
class IntRect;
class ChromeClient;
class ScrollableArea;
class ScrollbarTheme;
class WebGestureEvent;
class WebMouseEvent;

class CORE_EXPORT Scrollbar : public GarbageCollected<Scrollbar>,
                              public DisplayItemClient {
 public:
  // Theme object ownership remains with the caller and it must outlive the
  // scrollbar.
  static Scrollbar* CreateForTesting(ScrollableArea* scrollable_area,
                                     ScrollbarOrientation orientation,
                                     ScrollbarControlSize size,
                                     ScrollbarTheme* theme) {
    return MakeGarbageCollected<Scrollbar>(scrollable_area, orientation, size,
                                           nullptr, nullptr, theme);
  }

  Scrollbar(ScrollableArea*,
            ScrollbarOrientation,
            ScrollbarControlSize,
            Element* style_source,
            ChromeClient* = nullptr,
            ScrollbarTheme* = nullptr);
  ~Scrollbar() override;

  int X() const { return frame_rect_.X(); }
  int Y() const { return frame_rect_.Y(); }
  int Width() const { return frame_rect_.Width(); }
  int Height() const { return frame_rect_.Height(); }
  IntSize Size() const { return frame_rect_.Size(); }
  IntPoint Location() const { return frame_rect_.Location(); }

  void SetFrameRect(const IntRect&);
  const IntRect& FrameRect() const { return frame_rect_; }

  ScrollbarOverlayColorTheme GetScrollbarOverlayColorTheme() const;
  bool HasTickmarks() const;
  Vector<IntRect> GetTickmarks() const;
  bool IsScrollableAreaActive() const;

  IntPoint ConvertFromRootFrame(const IntPoint&) const;

  virtual bool IsCustomScrollbar() const { return false; }
  ScrollbarOrientation Orientation() const { return orientation_; }
  bool IsLeftSideVerticalScrollbar() const;

  int Value() const { return static_cast<int>(lroundf(current_pos_)); }
  float CurrentPos() const { return current_pos_; }
  int VisibleSize() const { return visible_size_; }
  int TotalSize() const { return total_size_; }
  int Maximum() const;
  ScrollbarControlSize GetControlSize() const { return control_size_; }

  ScrollbarPart PressedPart() const { return pressed_part_; }
  ScrollbarPart HoveredPart() const { return hovered_part_; }

  virtual void StyleChanged() {}
  void SetScrollbarsHiddenIfOverlay(bool);
  bool Enabled() const { return enabled_; }
  virtual void SetEnabled(bool);

  // This returns device-scale-factor-aware pixel value.
  // e.g. 15 in dsf=1.0, 30 in dsf=2.0.
  // This returns 0 for overlay scrollbars.
  // See also ScrolbarTheme::scrollbatThickness().
  int ScrollbarThickness() const;

  // Called by the ScrollableArea when the scroll offset changes.
  // Will trigger paint invalidation if required.
  void OffsetDidChange();

  virtual void DisconnectFromScrollableArea();
  ScrollableArea* GetScrollableArea() const { return scrollable_area_; }

  int PressedPos() const { return pressed_pos_; }

  virtual void SetHoveredPart(ScrollbarPart);
  virtual void SetPressedPart(ScrollbarPart, WebInputEvent::Type);

  void SetProportion(int visible_size, int total_size);
  void SetPressedPos(int p) { pressed_pos_ = p; }

  void Paint(GraphicsContext&) const;

  virtual bool IsSolidColor() const;
  virtual bool IsOverlayScrollbar() const;
  bool ShouldParticipateInHitTesting();

  bool IsWindowActive() const;

  // Return if the gesture event was handled. |shouldUpdateCapture|
  // will be set to true if the handler should update the capture
  // state for this scrollbar.
  bool GestureEvent(const WebGestureEvent&, bool* should_update_capture);

  // These methods are used for platform scrollbars to give :hover feedback.
  // They will not get called when the mouse went down in a scrollbar, since it
  // is assumed the scrollbar will start
  // grabbing all events in that case anyway.
  void MouseMoved(const WebMouseEvent&);
  void MouseEntered();
  void MouseExited();

  // Used by some platform scrollbars to know when they've been released from
  // capture.
  void MouseUp(const WebMouseEvent&);
  void MouseDown(const WebMouseEvent&);

  ScrollbarTheme& GetTheme() const { return theme_; }

  IntRect ConvertToContainingEmbeddedContentView(const IntRect&) const;
  IntPoint ConvertFromContainingEmbeddedContentView(const IntPoint&) const;

  void MoveThumb(int pos, bool dragging_document = false);

  float ElasticOverscroll() const { return elastic_overscroll_; }
  void SetElasticOverscroll(float elastic_overscroll) {
    elastic_overscroll_ = elastic_overscroll;
  }

  // Use SetNeedsPaintInvalidation to cause the scrollbar (or parts thereof)
  // to repaint. Here "track" includes track, buttons and tickmarks, i.e. all
  // things except the thumb.
  bool TrackNeedsRepaint() const { return track_needs_repaint_; }
  void ClearTrackNeedsRepaint() { track_needs_repaint_ = false; }
  bool ThumbNeedsRepaint() const { return thumb_needs_repaint_; }
  void ClearThumbNeedsRepaint() { thumb_needs_repaint_ = false; }

  // DisplayItemClient methods.
  String DebugName() const final {
    return orientation_ == kHorizontalScrollbar ? "HorizontalScrollbar"
                                                : "VerticalScrollbar";
  }
  IntRect VisualRect() const final { return visual_rect_; }

  virtual void SetVisualRect(const IntRect& r) { visual_rect_ = r; }

  // Marks the scrollbar as needing to be redrawn.
  //
  // If invalid parts are provided, then those parts will also be repainted.
  // Otherwise, the ScrollableArea may redraw using cached renderings of
  // individual parts. For instance, if the scrollbar is composited, the thumb
  // may be cached in a GPU texture (and is only guaranteed to be repainted if
  // ThumbPart is invalidated).
  //
  // Even if no parts are invalidated, the scrollbar may need to be redrawn
  // if, for instance, the thumb moves without changing the appearance of any
  // part.
  void SetNeedsPaintInvalidation(ScrollbarPart invalid_parts);

  CompositorElementId GetElementId();

  float EffectiveZoom() const;
  bool ContainerIsRightToLeft() const;

  // The Element that supplies our style information. If the scrollbar is
  // for a document, this is either the <body> or <html> element. Otherwise, it
  // is the element that owns our PaintLayerScrollableArea.
  Element* StyleSource() const { return style_source_.Get(); }

  WebColorScheme UsedColorScheme() const;

  virtual void Trace(blink::Visitor*);

 protected:
  void AutoscrollTimerFired(TimerBase*);
  void StartTimerIfNeeded(base::TimeDelta delay);
  void StopTimerIfNeeded();
  void AutoscrollPressedPart(base::TimeDelta delay);
  bool HandleTapGesture();
  void InjectScrollGestureForPressedPart(WebInputEvent::Type gesture_type);
  void InjectGestureScrollUpdateForThumbMove(float single_axis_target_offset);
  void InjectScrollGesture(WebInputEvent::Type type,
                           ScrollOffset delta,
                           ScrollGranularity granularity);
  ScrollDirectionPhysical PressedPartScrollDirectionPhysical();
  ScrollGranularity PressedPartScrollGranularity();

  Member<ScrollableArea> scrollable_area_;
  ScrollbarOrientation orientation_;
  ScrollbarControlSize control_size_;
  ScrollbarTheme& theme_;
  Member<ChromeClient> chrome_client_;

  int visible_size_;
  int total_size_;
  float current_pos_;
  float drag_origin_;

  ScrollbarPart hovered_part_;
  ScrollbarPart pressed_part_;
  int pressed_pos_;
  float scroll_pos_;
  bool dragging_document_;
  int document_drag_pos_;

  bool enabled_;

  TaskRunnerTimer<Scrollbar> scroll_timer_;

  float elastic_overscroll_;

 private:
  float ScrollableAreaCurrentPos() const;
  float ScrollableAreaTargetPos() const;
  bool ThumbWillBeUnderMouse() const;
  bool DeltaWillScroll(ScrollOffset delta) const;

  int theme_scrollbar_thickness_;
  bool track_needs_repaint_;
  bool thumb_needs_repaint_;
  bool injected_gesture_scroll_begin_;
  IntRect visual_rect_;
  IntRect frame_rect_;
  Member<Element> style_source_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLLBAR_H_
