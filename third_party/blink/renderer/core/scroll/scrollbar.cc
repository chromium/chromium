/*
 * Copyright (C) 2004, 2006, 2008 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/scroll/scrollbar.h"

#include <algorithm>
#include "third_party/blink/public/platform/web_gesture_event.h"
#include "third_party/blink/public/platform/web_mouse_event.h"
#include "third_party/blink/public/platform/web_scrollbar_overlay_color_theme.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/scroll/scroll_animator_base.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"

namespace blink {

Scrollbar::Scrollbar(ScrollableArea* scrollable_area,
                     ScrollbarOrientation orientation,
                     ScrollbarControlSize control_size,
                     ChromeClient* chrome_client,
                     ScrollbarTheme* theme)
    : scrollable_area_(scrollable_area),
      orientation_(orientation),
      control_size_(control_size),
      theme_(theme ? *theme : scrollable_area->GetPageScrollbarTheme()),
      chrome_client_(chrome_client),
      visible_size_(0),
      total_size_(0),
      current_pos_(0),
      drag_origin_(0),
      hovered_part_(kNoPart),
      pressed_part_(kNoPart),
      pressed_pos_(0),
      scroll_pos_(0),
      dragging_document_(false),
      document_drag_pos_(0),
      enabled_(true),
      scroll_timer_(scrollable_area->GetTimerTaskRunner(),
                    this,
                    &Scrollbar::AutoscrollTimerFired),
      elastic_overscroll_(0),
      track_needs_repaint_(true),
      thumb_needs_repaint_(true) {
  theme_.RegisterScrollbar(*this);

  // FIXME: This is ugly and would not be necessary if we fix cross-platform
  // code to actually query for scrollbar thickness and use it when sizing
  // scrollbars (rather than leaving one dimension of the scrollbar alone when
  // sizing).
  int thickness = theme_.ScrollbarThickness(control_size);
  theme_scrollbar_thickness_ = thickness;
  if (chrome_client_)
    thickness = chrome_client_->WindowToViewportScalar(thickness);
  frame_rect_ = IntRect(0, 0, thickness, thickness);

  current_pos_ = ScrollableAreaCurrentPos();
}

Scrollbar::~Scrollbar() {
  theme_.UnregisterScrollbar(*this);
}

void Scrollbar::Trace(blink::Visitor* visitor) {
  visitor->Trace(scrollable_area_);
  visitor->Trace(chrome_client_);
}

void Scrollbar::SetFrameRect(const IntRect& frame_rect) {
  if (frame_rect == frame_rect_)
    return;

  frame_rect_ = frame_rect;
  SetNeedsPaintInvalidation(kAllParts);
  if (scrollable_area_)
    scrollable_area_->ScrollbarFrameRectChanged();
}

ScrollbarOverlayColorTheme Scrollbar::GetScrollbarOverlayColorTheme() const {
  return scrollable_area_ ? scrollable_area_->GetScrollbarOverlayColorTheme()
                          : kScrollbarOverlayColorThemeDark;
}

void Scrollbar::GetTickmarks(Vector<IntRect>& tickmarks) const {
  if (scrollable_area_)
    scrollable_area_->GetTickmarks(tickmarks);
}

bool Scrollbar::IsScrollableAreaActive() const {
  return scrollable_area_ && scrollable_area_->IsActive();
}

bool Scrollbar::IsLeftSideVerticalScrollbar() const {
  if (orientation_ == kVerticalScrollbar && scrollable_area_)
    return scrollable_area_->ShouldPlaceVerticalScrollbarOnLeft();
  return false;
}

int Scrollbar::Maximum() const {
  IntSize max_offset = scrollable_area_->MaximumScrollOffsetInt() -
                       scrollable_area_->MinimumScrollOffsetInt();
  return orientation_ == kHorizontalScrollbar ? max_offset.Width()
                                              : max_offset.Height();
}

void Scrollbar::OffsetDidChange() {
  DCHECK(scrollable_area_);

  float position = ScrollableAreaCurrentPos();
  if (position == current_pos_)
    return;

  float old_position = current_pos_;
  int old_thumb_position = GetTheme().ThumbPosition(*this);
  current_pos_ = position;

  ScrollbarPart invalid_parts =
      GetTheme().InvalidateOnThumbPositionChange(*this, old_position, position);
  SetNeedsPaintInvalidation(invalid_parts);

  if (pressed_part_ == kThumbPart)
    SetPressedPos(pressed_pos_ + GetTheme().ThumbPosition(*this) -
                  old_thumb_position);
}

void Scrollbar::DisconnectFromScrollableArea() {
  scrollable_area_ = nullptr;
}

void Scrollbar::SetProportion(int visible_size, int total_size) {
  if (visible_size == visible_size_ && total_size == total_size_)
    return;

  visible_size_ = visible_size;
  total_size_ = total_size;

  SetNeedsPaintInvalidation(kAllParts);
}

void Scrollbar::Paint(GraphicsContext& context,
                      const CullRect& cull_rect) const {
  if (!cull_rect.IntersectsCullRect(FrameRect()))
    return;

  GetTheme().Paint(*this, context, cull_rect);
}

void Scrollbar::AutoscrollTimerFired(TimerBase*) {
  AutoscrollPressedPart(GetTheme().AutoscrollTimerDelay());
}

bool Scrollbar::ThumbWillBeUnderMouse() const {
  int thumb_pos = GetTheme().TrackPosition(*this) +
                  GetTheme().ThumbPosition(*this, ScrollableAreaTargetPos());
  int thumb_length = GetTheme().ThumbLength(*this);
  return PressedPos() >= thumb_pos && PressedPos() < thumb_pos + thumb_length;
}

void Scrollbar::AutoscrollPressedPart(TimeDelta delay) {
  if (!scrollable_area_)
    return;

  // Don't do anything for the thumb or if nothing was pressed.
  if (pressed_part_ == kThumbPart || pressed_part_ == kNoPart)
    return;

  // Handle the track.
  if ((pressed_part_ == kBackTrackPart || pressed_part_ == kForwardTrackPart) &&
      ThumbWillBeUnderMouse()) {
    SetHoveredPart(kThumbPart);
    return;
  }

  // Handle the arrows and track.
  bool did_scroll =
      scrollable_area_
          ->UserScroll(PressedPartScrollGranularity(),
                       ToScrollDelta(PressedPartScrollDirectionPhysical(), 1))
          .DidScroll();

  // Always start timer when user press on button since scrollable area maybe
  // infinite scrolling.
  if (pressed_part_ == kBackButtonStartPart ||
      pressed_part_ == kForwardButtonStartPart ||
      pressed_part_ == kBackButtonEndPart ||
      pressed_part_ == kForwardButtonEndPart) {
    StartTimerIfNeeded(delay);
    return;
  }

  if ((pressed_part_ == kBackTrackPart || pressed_part_ == kForwardTrackPart) &&
      did_scroll) {
    StartTimerIfNeeded(delay);
    return;
  }
}

void Scrollbar::StartTimerIfNeeded(TimeDelta delay) {
  // Don't do anything for the thumb.
  if (pressed_part_ == kThumbPart)
    return;

  // Handle the track.  We halt track scrolling once the thumb is level
  // with us.
  if ((pressed_part_ == kBackTrackPart || pressed_part_ == kForwardTrackPart) &&
      ThumbWillBeUnderMouse()) {
    SetHoveredPart(kThumbPart);
    return;
  }

  scroll_timer_.StartOneShot(delay, FROM_HERE);
}

void Scrollbar::StopTimerIfNeeded() {
  scroll_timer_.Stop();
}

ScrollDirectionPhysical Scrollbar::PressedPartScrollDirectionPhysical() {
  if (orientation_ == kHorizontalScrollbar) {
    if (pressed_part_ == kBackButtonStartPart ||
        pressed_part_ == kBackButtonEndPart || pressed_part_ == kBackTrackPart)
      return kScrollLeft;
    return kScrollRight;
  } else {
    if (pressed_part_ == kBackButtonStartPart ||
        pressed_part_ == kBackButtonEndPart || pressed_part_ == kBackTrackPart)
      return kScrollUp;
    return kScrollDown;
  }
}

ScrollGranularity Scrollbar::PressedPartScrollGranularity() {
  if (pressed_part_ == kBackButtonStartPart ||
      pressed_part_ == kBackButtonEndPart ||
      pressed_part_ == kForwardButtonStartPart ||
      pressed_part_ == kForwardButtonEndPart)
    return kScrollByLine;
  return kScrollByPage;
}

void Scrollbar::MoveThumb(int pos, bool dragging_document) {
  if (!scrollable_area_)
    return;

  int delta = pos - pressed_pos_;

  if (dragging_document) {
    if (dragging_document_)
      delta = pos - document_drag_pos_;
    dragging_document_ = true;
    ScrollOffset current_position =
        scrollable_area_->GetScrollAnimator().CurrentOffset();
    float destination_position =
        (orientation_ == kHorizontalScrollbar ? current_position.Width()
                                              : current_position.Height()) +
        delta;
    destination_position =
        scrollable_area_->ClampScrollOffset(orientation_, destination_position);
    scrollable_area_->SetScrollOffsetSingleAxis(
        orientation_, destination_position, kUserScroll);
    document_drag_pos_ = pos;
    return;
  }

  if (dragging_document_) {
    delta += pressed_pos_ - document_drag_pos_;
    dragging_document_ = false;
  }

  // Drag the thumb.
  int thumb_pos = GetTheme().ThumbPosition(*this);
  int thumb_len = GetTheme().ThumbLength(*this);
  int track_len = GetTheme().TrackLength(*this);
  DCHECK_LE(thumb_len, track_len);
  if (thumb_len == track_len)
    return;

  if (delta > 0)
    delta = std::min(track_len - thumb_len - thumb_pos, delta);
  else if (delta < 0)
    delta = std::max(-thumb_pos, delta);

  float min_offset = scrollable_area_->MinimumScrollOffset(orientation_);
  float max_offset = scrollable_area_->MaximumScrollOffset(orientation_);
  if (delta) {
    float new_offset = static_cast<float>(thumb_pos + delta) *
                           (max_offset - min_offset) / (track_len - thumb_len) +
                       min_offset;
    scrollable_area_->SetScrollOffsetSingleAxis(orientation_, new_offset,
                                                kUserScroll);
  }
}

void Scrollbar::SetHoveredPart(ScrollbarPart part) {
  if (part == hovered_part_)
    return;

  if (((hovered_part_ == kNoPart || part == kNoPart) &&
       GetTheme().InvalidateOnMouseEnterExit())
      // When there's a pressed part, we don't draw a hovered state, so there's
      // no reason to invalidate.
      || pressed_part_ == kNoPart)
    SetNeedsPaintInvalidation(static_cast<ScrollbarPart>(hovered_part_ | part));

  hovered_part_ = part;
}

void Scrollbar::SetPressedPart(ScrollbarPart part) {
  if (pressed_part_ != kNoPart
      // When we no longer have a pressed part, we can start drawing a hovered
      // state on the hovered part.
      || hovered_part_ != kNoPart)
    SetNeedsPaintInvalidation(
        static_cast<ScrollbarPart>(pressed_part_ | hovered_part_ | part));

  if (GetScrollableArea())
    GetScrollableArea()->DidScrollWithScrollbar(part, Orientation());

  pressed_part_ = part;
}

bool Scrollbar::GestureEvent(const WebGestureEvent& evt,
                             bool* should_update_capture) {
  DCHECK(should_update_capture);
  switch (evt.GetType()) {
    case WebInputEvent::kGestureTapDown: {
      IntPoint position = FlooredIntPoint(evt.PositionInRootFrame());
      SetPressedPart(GetTheme().HitTest(*this, position));
      pressed_pos_ = Orientation() == kHorizontalScrollbar
                         ? ConvertFromRootFrame(position).X()
                         : ConvertFromRootFrame(position).Y();
      *should_update_capture = true;
      return true;
    }
    case WebInputEvent::kGestureTapCancel:
      if (pressed_part_ != kThumbPart)
        return false;
      scroll_pos_ = pressed_pos_;
      return true;
    case WebInputEvent::kGestureScrollBegin:
      switch (evt.SourceDevice()) {
        case kWebGestureDeviceSyntheticAutoscroll:
        case kWebGestureDeviceTouchpad:
          // Update the state on GSB for touchpad since GestureTapDown
          // is not generated by that device. Touchscreen uses the tap down
          // gesture since the scrollbar enters a visual active state.
          SetPressedPart(kNoPart);
          pressed_pos_ = 0;
          return false;
        case kWebGestureDeviceTouchscreen:
          if (pressed_part_ != kThumbPart)
            return false;
          scroll_pos_ = pressed_pos_;
          return true;
        default:
          NOTREACHED();
          return true;
      }
      break;
    case WebInputEvent::kGestureScrollUpdate:
      switch (evt.SourceDevice()) {
        case kWebGestureDeviceSyntheticAutoscroll:
        case kWebGestureDeviceTouchpad:
          return false;
        case kWebGestureDeviceTouchscreen:
          if (pressed_part_ != kThumbPart)
            return false;
          scroll_pos_ += Orientation() == kHorizontalScrollbar
                             ? evt.DeltaXInRootFrame()
                             : evt.DeltaYInRootFrame();
          MoveThumb(scroll_pos_, false);
          return true;
        default:
          NOTREACHED();
          return true;
      }
      break;
    case WebInputEvent::kGestureScrollEnd:
    case WebInputEvent::kGestureLongPress:
    case WebInputEvent::kGestureFlingStart:
      scroll_pos_ = 0;
      pressed_pos_ = 0;
      SetPressedPart(kNoPart);
      return false;
    case WebInputEvent::kGestureTap: {
      if (pressed_part_ != kThumbPart && pressed_part_ != kNoPart &&
          scrollable_area_ &&
          scrollable_area_
              ->UserScroll(
                  PressedPartScrollGranularity(),
                  ToScrollDelta(PressedPartScrollDirectionPhysical(), 1))
              .DidScroll()) {
        return true;
      }
      scroll_pos_ = 0;
      pressed_pos_ = 0;
      SetPressedPart(kNoPart);
      return false;
    }
    default:
      // By default, we assume that gestures don't deselect the scrollbar.
      return true;
  }
}

void Scrollbar::MouseMoved(const WebMouseEvent& evt) {
  IntPoint position = FlooredIntPoint(evt.PositionInRootFrame());
  if (pressed_part_ == kThumbPart) {
    if (GetTheme().ShouldSnapBackToDragOrigin(*this, evt)) {
      if (scrollable_area_) {
        scrollable_area_->SetScrollOffsetSingleAxis(
            orientation_,
            drag_origin_ + scrollable_area_->MinimumScrollOffset(orientation_),
            kUserScroll);
      }
    } else {
      MoveThumb(orientation_ == kHorizontalScrollbar
                    ? ConvertFromRootFrame(position).X()
                    : ConvertFromRootFrame(position).Y(),
                GetTheme().ShouldDragDocumentInsteadOfThumb(*this, evt));
    }
    return;
  }

  if (pressed_part_ != kNoPart) {
    pressed_pos_ = Orientation() == kHorizontalScrollbar
                       ? ConvertFromRootFrame(position).X()
                       : ConvertFromRootFrame(position).Y();
  }

  ScrollbarPart part = GetTheme().HitTest(*this, position);
  if (part != hovered_part_) {
    if (pressed_part_ != kNoPart) {
      if (part == pressed_part_) {
        // The mouse is moving back over the pressed part.  We
        // need to start up the timer action again.
        StartTimerIfNeeded(GetTheme().AutoscrollTimerDelay());
      } else if (hovered_part_ == pressed_part_) {
        // The mouse is leaving the pressed part.  Kill our timer
        // if needed.
        StopTimerIfNeeded();
      }
    }

    SetHoveredPart(part);
  }

  return;
}

void Scrollbar::MouseEntered() {
  if (scrollable_area_)
    scrollable_area_->MouseEnteredScrollbar(*this);
}

void Scrollbar::MouseExited() {
  if (scrollable_area_)
    scrollable_area_->MouseExitedScrollbar(*this);
  SetHoveredPart(kNoPart);
}

void Scrollbar::MouseUp(const WebMouseEvent& mouse_event) {
  bool is_captured = pressed_part_ == kThumbPart;
  SetPressedPart(kNoPart);
  pressed_pos_ = 0;
  dragging_document_ = false;
  StopTimerIfNeeded();

  if (scrollable_area_) {
    if (is_captured)
      scrollable_area_->MouseReleasedScrollbar();
    scrollable_area_->SnapAfterScrollbarScrolling(orientation_);

    ScrollbarPart part = GetTheme().HitTest(
        *this, FlooredIntPoint(mouse_event.PositionInRootFrame()));
    if (part == kNoPart) {
      SetHoveredPart(kNoPart);
      scrollable_area_->MouseExitedScrollbar(*this);
    }
  }
}

void Scrollbar::MouseDown(const WebMouseEvent& evt) {
  // Early exit for right click
  if (evt.button == WebPointerProperties::Button::kRight)
    return;

  IntPoint position = FlooredIntPoint(evt.PositionInRootFrame());
  SetPressedPart(GetTheme().HitTest(*this, position));
  int pressed_pos = Orientation() == kHorizontalScrollbar
                        ? ConvertFromRootFrame(position).X()
                        : ConvertFromRootFrame(position).Y();

  if ((pressed_part_ == kBackTrackPart || pressed_part_ == kForwardTrackPart) &&
      GetTheme().ShouldCenterOnThumb(*this, evt)) {
    SetHoveredPart(kThumbPart);
    SetPressedPart(kThumbPart);
    drag_origin_ = current_pos_;
    int thumb_len = GetTheme().ThumbLength(*this);
    int desired_pos = pressed_pos;
    // Set the pressed position to the middle of the thumb so that when we do
    // the move, the delta will be from the current pixel position of the thumb
    // to the new desired position for the thumb.
    pressed_pos_ = GetTheme().TrackPosition(*this) +
                   GetTheme().ThumbPosition(*this) + thumb_len / 2;
    MoveThumb(desired_pos);
    return;
  }
  if (pressed_part_ == kThumbPart) {
    drag_origin_ = current_pos_;
    if (scrollable_area_)
      scrollable_area_->MouseCapturedScrollbar();
  }

  pressed_pos_ = pressed_pos;

  AutoscrollPressedPart(GetTheme().InitialAutoscrollTimerDelay());
}

void Scrollbar::SetScrollbarsHiddenIfOverlay(bool hidden) {
  if (scrollable_area_)
    scrollable_area_->SetScrollbarsHiddenIfOverlay(hidden);
}

void Scrollbar::SetEnabled(bool e) {
  if (enabled_ == e)
    return;
  enabled_ = e;
  GetTheme().UpdateEnabledState(*this);

  // We can skip thumb/track repaint when hiding an overlay scrollbar, but not
  // when showing (since the proportions may have changed while hidden).
  bool skipPartsRepaint = IsOverlayScrollbar() && scrollable_area_ &&
                          scrollable_area_->ScrollbarsHiddenIfOverlay();
  SetNeedsPaintInvalidation(skipPartsRepaint ? kNoPart : kAllParts);
}

int Scrollbar::ScrollbarThickness() const {
  int thickness = Orientation() == kHorizontalScrollbar ? Height() : Width();
  if (!thickness || !chrome_client_)
    return thickness;
  return chrome_client_->WindowToViewportScalar(theme_scrollbar_thickness_);
}

bool Scrollbar::IsOverlayScrollbar() const {
  return theme_.UsesOverlayScrollbars();
}

bool Scrollbar::ShouldParticipateInHitTesting() {
  // Non-overlay scrollbars should always participate in hit testing.
  if (!IsOverlayScrollbar())
    return true;
  return !scrollable_area_->ScrollbarsHiddenIfOverlay();
}

bool Scrollbar::IsWindowActive() const {
  return scrollable_area_ && scrollable_area_->IsActive();
}

IntPoint Scrollbar::ConvertFromRootFrame(
    const IntPoint& point_in_root_frame) const {
  if (scrollable_area_) {
    IntPoint parent_point =
        scrollable_area_->ConvertFromRootFrame(point_in_root_frame);
    return scrollable_area_
        ->ConvertFromContainingEmbeddedContentViewToScrollbar(*this,
                                                              parent_point);
  }

  return point_in_root_frame;
}

IntRect Scrollbar::ConvertToContainingEmbeddedContentView(
    const IntRect& local_rect) const {
  if (scrollable_area_) {
    return scrollable_area_
        ->ConvertFromScrollbarToContainingEmbeddedContentView(*this,
                                                              local_rect);
  }

  return local_rect;
}

IntPoint Scrollbar::ConvertFromContainingEmbeddedContentView(
    const IntPoint& parent_point) const {
  if (scrollable_area_) {
    return scrollable_area_
        ->ConvertFromContainingEmbeddedContentViewToScrollbar(*this,
                                                              parent_point);
  }

  return parent_point;
}

float Scrollbar::ScrollableAreaCurrentPos() const {
  if (!scrollable_area_)
    return 0;

  if (orientation_ == kHorizontalScrollbar) {
    return scrollable_area_->GetScrollOffset().Width() -
           scrollable_area_->MinimumScrollOffset().Width();
  }

  return scrollable_area_->GetScrollOffset().Height() -
         scrollable_area_->MinimumScrollOffset().Height();
}

float Scrollbar::ScrollableAreaTargetPos() const {
  if (!scrollable_area_)
    return 0;

  if (orientation_ == kHorizontalScrollbar) {
    return scrollable_area_->GetScrollAnimator().DesiredTargetOffset().Width() -
           scrollable_area_->MinimumScrollOffset().Width();
  }

  return scrollable_area_->GetScrollAnimator().DesiredTargetOffset().Height() -
         scrollable_area_->MinimumScrollOffset().Height();
}

void Scrollbar::SetNeedsPaintInvalidation(ScrollbarPart invalid_parts) {
  if (theme_.ShouldRepaintAllPartsOnInvalidation())
    invalid_parts = kAllParts;
  if (invalid_parts & ~kThumbPart)
    track_needs_repaint_ = true;
  if (invalid_parts & kThumbPart)
    thumb_needs_repaint_ = true;
  if (scrollable_area_)
    scrollable_area_->SetScrollbarNeedsPaintInvalidation(Orientation());
}

CompositorElementId Scrollbar::GetElementId() {
  DCHECK(scrollable_area_);
  return scrollable_area_->GetScrollbarElementId(orientation_);
}

STATIC_ASSERT_ENUM(kWebScrollbarOverlayColorThemeDark,
                   kScrollbarOverlayColorThemeDark);
STATIC_ASSERT_ENUM(kWebScrollbarOverlayColorThemeLight,
                   kScrollbarOverlayColorThemeLight);

}  // namespace blink
