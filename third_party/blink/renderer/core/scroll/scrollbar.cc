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

#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_pointer_event.h"
#include "third_party/blink/public/common/input/web_pointer_properties.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/scroll/scroll_animator_base.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {
bool ButtonInteractsWithScrollbar(const WebPointerProperties::Button button) {
  if (button == WebPointerProperties::Button::kMiddle) {
    if (RuntimeEnabledFeatures::MiddleClickAutoscrollEnabled()) {
      return false;
    }

    // The reason to allow middle mouse button clicks is that the
    // ShouldCenterOnThumb mode of the scrollbar theme(such as
    // scroll_theme_aura) uses the middle mouse button.
    return true;
  }
  return button == WebPointerProperties::Button::kLeft;
}

Scrollbar* Scrollbar::CreateForTesting(ScrollableArea* scrollable_area,
                                       ScrollbarOrientation orientation,
                                       ScrollbarTheme* theme) {
  return MakeGarbageCollected<Scrollbar>(
      scrollable_area, orientation, scrollable_area->GetLayoutBox(), theme);
}

Scrollbar::Scrollbar(ScrollableArea* scrollable_area,
                     ScrollbarOrientation orientation,
                     const LayoutObject* style_source,
                     ScrollbarTheme* theme)
    : scrollable_area_(scrollable_area),
      orientation_(orientation),
      theme_(theme ? *theme : scrollable_area->GetPageScrollbarTheme()),
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
      injected_gesture_scroll_begin_(false),
      scrollbar_manipulation_in_progress_on_cc_thread_(false),
      style_source_(style_source) {
  theme_.RegisterScrollbar(*this);
  int thickness =
      theme_.ScrollbarThickness(ScaleFromDIP(), CSSScrollbarWidth());
  frame_rect_ = gfx::Rect(0, 0, thickness, thickness);
  current_pos_ = ScrollableAreaCurrentPos();
}

Scrollbar::~Scrollbar() = default;

void Scrollbar::Trace(Visitor* visitor) const {
  visitor->Trace(scrollable_area_);
  visitor->Trace(scroll_timer_);
  visitor->Trace(style_source_);
  DisplayItemClient::Trace(visitor);
}

void Scrollbar::SetFrameRect(const gfx::Rect& frame_rect) {
  if (frame_rect == frame_rect_)
    return;

  if (!UsesNinePatchTrackAndCanSkipRepaint(frame_rect)) {
    SetNeedsPaintInvalidation(kAllParts);
  }
  frame_rect_ = frame_rect;
  if (scrollable_area_)
    scrollable_area_->ScrollbarFrameRectChanged();
}

bool Scrollbar::HasTickmarks() const {
  return orientation_ == kVerticalScrollbar && scrollable_area_ &&
         scrollable_area_->HasTickmarks();
}

Vector<gfx::Rect> Scrollbar::GetTickmarks() const {
  if (scrollable_area_)
    return scrollable_area_->GetTickmarks();
  return Vector<gfx::Rect>();
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
  if (!scrollable_area_) {
    return 0;
  }
  gfx::Vector2d max_offset = scrollable_area_->MaximumScrollOffsetInt() -
                             scrollable_area_->MinimumScrollOffsetInt();
  return orientation_ == kHorizontalScrollbar ? max_offset.x() : max_offset.y();
}

void Scrollbar::OffsetDidChange(mojom::blink::ScrollType scroll_type) {
  DCHECK(scrollable_area_);
  pending_injected_delta_ = ScrollOffset();

  float position = ScrollableAreaCurrentPos();
  if (position == current_pos_)
    return;

  float old_position = current_pos_;
  int old_thumb_position = GetTheme().ThumbPosition(*this);
  current_pos_ = position;

  ScrollbarPart invalid_parts =
      GetTheme().PartsToInvalidateOnThumbPositionChange(*this, old_position,
                                                        position);
  SetNeedsPaintInvalidation(invalid_parts);

  // Don't update the pressed position if scroll anchoring takes place as
  // otherwise the next thumb movement will undo anchoring.
  if (pressed_part_ == kThumbPart &&
      scroll_type != mojom::blink::ScrollType::kAnchoring) {
    SetPressedPos(pressed_pos_ + GetTheme().ThumbPosition(*this) -
                  old_thumb_position);
  }
}

void Scrollbar::DisconnectFromScrollableArea() {
  scrollable_area_ = nullptr;
}

void Scrollbar::SetProportion(int visible_size, int total_size) {
  if (visible_size == visible_size_ && total_size == total_size_)
    return;

  visible_size_ = visible_size;
  total_size_ = total_size;

  if (UsesNinePatchTrackAndCanSkipRepaint(frame_rect_)) {
    SetNeedsPaintInvalidation(kThumbPart);
    return;
  }

  SetNeedsPaintInvalidation(kAllParts);
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

void Scrollbar::AutoscrollPressedPart(base::TimeDelta delay) {
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

  // Handle the arrows and track by injecting a scroll update.
  InjectScrollGestureForPressedPart(WebInputEvent::Type::kGestureScrollUpdate);

  // Always start timer when user press on button since scrollable area maybe
  // infinite scrolling.
  if (pressed_part_ == kBackButtonStartPart ||
      pressed_part_ == kForwardButtonStartPart ||
      pressed_part_ == kBackButtonEndPart ||
      pressed_part_ == kForwardButtonEndPart ||
      pressed_part_ == kBackTrackPart || pressed_part_ == kForwardTrackPart) {
    StartTimerIfNeeded(delay);
    return;
  }
}

void Scrollbar::StartTimerIfNeeded(base::TimeDelta delay) {
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

ui::ScrollGranularity Scrollbar::PressedPartScrollGranularity() {
  if (pressed_part_ == kBackButtonStartPart ||
      pressed_part_ == kBackButtonEndPart ||
      pressed_part_ == kForwardButtonStartPart ||
      pressed_part_ == kForwardButtonEndPart) {
    return RuntimeEnabledFeatures::PercentBasedScrollingEnabled()
               ? ui::ScrollGranularity::kScrollByPercentage
               : ui::ScrollGranularity::kScrollByLine;
  }
  return ui::ScrollGranularity::kScrollByPage;
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
        (orientation_ == kHorizontalScrollbar ? current_position.x()
                                              : current_position.y()) +
        delta;
    destination_position =
        scrollable_area_->ClampScrollOffset(orientation_, destination_position);
    InjectGestureScrollUpdateForThumbMove(destination_position);
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

    InjectGestureScrollUpdateForThumbMove(new_offset);
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

void Scrollbar::SetPressedPart(ScrollbarPart part, WebInputEvent::Type type) {
  if (pressed_part_ != kNoPart
      // When we no longer have a pressed part, we can start drawing a hovered
      // state on the hovered part.
      || hovered_part_ != kNoPart)
    SetNeedsPaintInvalidation(
        static_cast<ScrollbarPart>(pressed_part_ | hovered_part_ | part));

  if (scrollable_area_ && part != kNoPart) {
    scrollable_area_->DidScrollWithScrollbar(part, Orientation(), type);
  }

  pressed_part_ = part;
}

bool Scrollbar::HandlePointerEvent(const WebPointerEvent& event) {
  WebInputEvent::Type type = event.GetType();
  // PointerEventManager gives us an event whose position is already
  // transformed to the root frame.
  gfx::Point root_frame_position =
      gfx::ToFlooredPoint(event.PositionInWidget());
  switch (type) {
    case WebInputEvent::Type::kPointerDown:
      if (GetTheme().HitTestRootFramePosition(*this, root_frame_position) ==
          kThumbPart) {
        SetPressedPart(kThumbPart, type);
        gfx::Point local_position = ConvertFromRootFrame(root_frame_position);
        pressed_pos_ = Orientation() == kHorizontalScrollbar
                           ? local_position.x()
                           : local_position.y();
        scroll_pos_ = pressed_pos_;
        return true;
      }
      return false;

    case WebInputEvent::Type::kPointerMove:
      if (pressed_part_ == kThumbPart) {
        gfx::Point local_position = ConvertFromRootFrame(root_frame_position);
        scroll_pos_ = Orientation() == kHorizontalScrollbar
                          ? local_position.x()
                          : local_position.y();
        MoveThumb(scroll_pos_, false);
        return true;
      }
      return false;

    case WebInputEvent::Type::kPointerUp:
      if (pressed_part_ == kThumbPart) {
        SetPressedPart(kNoPart, type);
        pressed_pos_ = 0;
        InjectScrollGesture(WebInputEvent::Type::kGestureScrollEnd,
                            ScrollOffset(),
                            ui::ScrollGranularity::kScrollByPrecisePixel);
        return true;
      }
      return false;

    default:
      return false;
  }
}

bool Scrollbar::HandleGestureTapOrPress(const WebGestureEvent& evt) {
  DCHECK(!evt.IsScrollEvent());
  switch (evt.GetType()) {
    case WebInputEvent::Type::kGestureTapDown: {
      gfx::Point position = gfx::ToFlooredPoint(evt.PositionInRootFrame());
      SetPressedPart(GetTheme().HitTestRootFramePosition(*this, position),
                     evt.GetType());
      pressed_pos_ = Orientation() == kHorizontalScrollbar
                         ? ConvertFromRootFrame(position).x()
                         : ConvertFromRootFrame(position).y();
      return true;
    }
    case WebInputEvent::Type::kGestureTapCancel:
      if (pressed_part_ != kThumbPart)
        return false;
      scroll_pos_ = pressed_pos_;
      return true;
    case WebInputEvent::Type::kGestureShortPress:
    case WebInputEvent::Type::kGestureLongPress:
      scroll_pos_ = 0;
      pressed_pos_ = 0;
      SetPressedPart(kNoPart, evt.GetType());
      return false;
    case WebInputEvent::Type::kGestureTap:
      return HandleTapGesture();
    default:
      // By default, we assume that gestures don't deselect the scrollbar.
      return true;
  }
}

bool Scrollbar::HandleTapGesture() {
  if (pressed_part_ != kThumbPart && pressed_part_ != kNoPart &&
      scrollable_area_) {
    ScrollOffset delta = ToScrollDelta(PressedPartScrollDirectionPhysical(), 1);
    if (DeltaWillScroll(delta)) {
      // Taps perform a single scroll begin/update/end sequence of gesture
      // events. There's no autoscroll timer since long press is not treated
      // the same as holding a mouse down.
      InjectScrollGestureForPressedPart(
          WebInputEvent::Type::kGestureScrollBegin);
      InjectScrollGestureForPressedPart(
          WebInputEvent::Type::kGestureScrollUpdate);
      InjectScrollGestureForPressedPart(WebInputEvent::Type::kGestureScrollEnd);

      return true;
    }
  }

  scroll_pos_ = 0;
  pressed_pos_ = 0;
  SetPressedPart(kNoPart, WebInputEvent::Type::kGestureTap);
  return true;
}

void Scrollbar::MouseMoved(const WebMouseEvent& evt) {
  gfx::Point position = gfx::ToFlooredPoint(evt.PositionInRootFrame());
  ScrollbarPart part = GetTheme().HitTestRootFramePosition(*this, position);

  // If the WebMouseEvent was already handled on the compositor thread, simply
  // set up the ScrollbarPart for invalidation and exit.
  if (scrollbar_manipulation_in_progress_on_cc_thread_) {
    SetHoveredPart(part);
    return;
  }

  if (pressed_part_ == kThumbPart) {
    if (GetTheme().ShouldSnapBackToDragOrigin(*this, evt)) {
      if (scrollable_area_) {
        float destination_position =
            drag_origin_ + scrollable_area_->MinimumScrollOffset(orientation_);
        InjectGestureScrollUpdateForThumbMove(destination_position);
      }
    } else {
      MoveThumb(orientation_ == kHorizontalScrollbar
                    ? ConvertFromRootFrame(position).x()
                    : ConvertFromRootFrame(position).y(),
                GetTheme().ShouldDragDocumentInsteadOfThumb(*this, evt));
    }
    return;
  }

  if (pressed_part_ != kNoPart) {
    pressed_pos_ = Orientation() == kHorizontalScrollbar
                       ? ConvertFromRootFrame(position).x()
                       : ConvertFromRootFrame(position).y();
  }

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
  if (theme_.UsesFluentOverlayScrollbars() && scrollable_area_) {
    scrollable_area_->GetLayoutBox()
        ->GetFrameView()
        ->SetPaintArtifactCompositorNeedsUpdate();
  }
}

void Scrollbar::MouseExited() {
  if (scrollable_area_)
    scrollable_area_->MouseExitedScrollbar(*this);
  SetHoveredPart(kNoPart);
  if (theme_.UsesFluentOverlayScrollbars() && scrollable_area_) {
    // If the mouse was hovering over the track and leaves the scrollbar, the
    // call to `SetHoveredPart(kNoPart)` will only invalidate the paint for the
    // track. Overlay Fluent scrollbars always need to invalidate the thumb to
    // change between solid/transparent colors.
    SetNeedsPaintInvalidation(kThumbPart);
    scrollable_area_->GetLayoutBox()
        ->GetFrameView()
        ->SetPaintArtifactCompositorNeedsUpdate();
  }
}

void Scrollbar::MouseUp(const WebMouseEvent& mouse_event) {
  bool is_captured = pressed_part_ == kThumbPart;
  SetPressedPart(kNoPart, mouse_event.GetType());
  if (scrollbar_manipulation_in_progress_on_cc_thread_) {
    scrollbar_manipulation_in_progress_on_cc_thread_ = false;
    return;
  }

  pressed_pos_ = 0;
  dragging_document_ = false;
  StopTimerIfNeeded();

  if (scrollable_area_) {
    if (is_captured)
      scrollable_area_->MouseReleasedScrollbar();

    ScrollbarPart part = GetTheme().HitTestRootFramePosition(
        *this, gfx::ToFlooredPoint(mouse_event.PositionInRootFrame()));
    if (part == kNoPart) {
      SetHoveredPart(kNoPart);
      scrollable_area_->MouseExitedScrollbar(*this);
    }

    InjectScrollGestureForPressedPart(WebInputEvent::Type::kGestureScrollEnd);
  }
}

void Scrollbar::MouseDown(const WebMouseEvent& evt) {
  if (!ButtonInteractsWithScrollbar(evt.button)) {
    return;
  }

  gfx::Point position = gfx::ToFlooredPoint(evt.PositionInRootFrame());
  SetPressedPart(GetTheme().HitTestRootFramePosition(*this, position),
                 evt.GetType());

  // Scrollbar manipulation (for a mouse) always begins with a MouseDown. If
  // this is already being handled by the compositor thread, blink::Scrollbar
  // needs to be made aware of this. It also means that, all the actions which
  // follow (like MouseMove(s) and MouseUp) will also be handled on the cc
  // thread. However, the scrollbar parts still need to be invalidated on the
  // main thread.
  scrollbar_manipulation_in_progress_on_cc_thread_ =
      evt.GetModifiers() &
      WebInputEvent::Modifiers::kScrollbarManipulationHandledOnCompositorThread;
  if (scrollbar_manipulation_in_progress_on_cc_thread_)
    return;

  int pressed_pos = Orientation() == kHorizontalScrollbar
                        ? ConvertFromRootFrame(position).x()
                        : ConvertFromRootFrame(position).y();

  if ((pressed_part_ == kBackTrackPart || pressed_part_ == kForwardTrackPart) &&
      GetTheme().ShouldCenterOnThumb(*this, evt)) {
    SetHoveredPart(kThumbPart);
    SetPressedPart(kThumbPart, evt.GetType());
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

void Scrollbar::InjectScrollGestureForPressedPart(
    WebInputEvent::Type gesture_type) {
  ui::ScrollGranularity granularity = PressedPartScrollGranularity();
  ScrollOffset delta =
      ToScrollDelta(PressedPartScrollDirectionPhysical(),
                    ScrollableArea::DirectionBasedScrollDelta(granularity));
  InjectScrollGesture(gesture_type, delta, granularity);
}

// Injects a GestureScrollUpdate event to change the scroll offset based on
// the passed in parameter. This parameter is the target offset for the axis
// which described by |orientation_|.
void Scrollbar::InjectGestureScrollUpdateForThumbMove(
    float single_axis_target_offset) {
  DCHECK(scrollable_area_);
  DCHECK(pressed_part_ == kThumbPart);

  // Convert the target offset to the delta that will be injected as part of a
  // GestureScrollUpdate event.
  ScrollOffset current_offset =
      scrollable_area_->GetScrollAnimator().CurrentOffset() +
      pending_injected_delta_;
  float desired_x = orientation_ == kHorizontalScrollbar
                        ? single_axis_target_offset
                        : current_offset.x();
  float desired_y = orientation_ == kVerticalScrollbar
                        ? single_axis_target_offset
                        : current_offset.y();
  ScrollOffset desired_offset(desired_x, desired_y);
  ScrollOffset scroll_delta = desired_offset - current_offset;

  InjectScrollGesture(WebInputEvent::Type::kGestureScrollUpdate, scroll_delta,
                      ui::ScrollGranularity::kScrollByPrecisePixel);
}

void Scrollbar::InjectScrollGesture(WebInputEvent::Type gesture_type,
                                    ScrollOffset delta,
                                    ui::ScrollGranularity granularity) {
  DCHECK(scrollable_area_);

  // Speculative fix for crash reports (crbug.com/1307510).
  if (!scrollable_area_)
    return;

  if (gesture_type == WebInputEvent::Type::kGestureScrollEnd &&
      !injected_gesture_scroll_begin_)
    return;

  // Don't inject a GSB/GSU if the expressed delta won't actually scroll. If
  // we do send the GSB, a scroll chain will be set up that excludes the node
  // associated with this scrollbar/ScrollableArea because this ScrollableArea
  // can't scroll in the specified direction. Due to the way the gesture bubbles
  // up the scroll chain, this will apply the scroll updates to a different
  // node.
  // Note that we don't apply the restriction to GSE since we want to send
  // that regardless in order to complete the gesture sequence.
  if ((gesture_type == WebInputEvent::Type::kGestureScrollUpdate ||
       gesture_type == WebInputEvent::Type::kGestureScrollBegin) &&
      !DeltaWillScroll(delta))
    return;

  if (gesture_type == WebInputEvent::Type::kGestureScrollUpdate &&
      !injected_gesture_scroll_begin_) {
    // If we're injecting a scroll update, but haven't yet injected a scroll
    // begin, do so now. This can happen with the following sequence of events:
    // - on mouse down the delta computed won't actually scroll (therefore
    //   GSB/GSU not injected).
    // - node/scrollable area changes size such that its scroll offset is no
    //   longer at the end.
    // - autoscroll timer fires and we inject a scroll update.
    // Additionally, thumb drags via mouse follow this pattern, since we don't
    // know the delta direction until the mouse actually moves.
    InjectScrollGesture(WebInputEvent::Type::kGestureScrollBegin, delta,
                        granularity);
  }

  pending_injected_delta_ += delta;
  scrollable_area_->InjectScrollbarGestureScroll(delta, granularity,
                                                 gesture_type);

  if (gesture_type == WebInputEvent::Type::kGestureScrollBegin) {
    injected_gesture_scroll_begin_ = true;
  } else if (gesture_type == WebInputEvent::Type::kGestureScrollEnd) {
    injected_gesture_scroll_begin_ = false;
  }
}

bool Scrollbar::DeltaWillScroll(ScrollOffset delta) const {
  CHECK(scrollable_area_);
  ScrollOffset current_offset = scrollable_area_->GetScrollOffset();
  ScrollOffset target_offset = current_offset + delta;
  ScrollOffset clamped_offset =
      scrollable_area_->ClampScrollOffset(target_offset);
  return clamped_offset != current_offset;
}

void Scrollbar::SetScrollbarsHiddenFromExternalAnimator(bool hidden) {
  if (scrollable_area_)
    scrollable_area_->SetScrollbarsHiddenFromExternalAnimator(hidden);
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
  if (!thickness || IsCustomScrollbar())
    return thickness;
  return theme_.ScrollbarThickness(ScaleFromDIP(), CSSScrollbarWidth());
}

bool Scrollbar::IsSolidColor() const {
  return theme_.IsSolidColor();
}

bool Scrollbar::IsOverlayScrollbar() const {
  return theme_.UsesOverlayScrollbars();
}

bool Scrollbar::IsFluentOverlayScrollbarMinimalMode() const {
  return theme_.UsesFluentOverlayScrollbars() && hovered_part_ == kNoPart &&
         pressed_part_ != kThumbPart;
}

bool Scrollbar::UsesNinePatchTrackAndCanSkipRepaint(
    const gfx::Rect& new_frame_rect) const {
  if (!theme_.UsesNinePatchTrackAndButtonsResource()) {
    return false;
  }
  // If the scrollbar's thickness is being changed, then a new bitmap needs to
  // be generated to paint the scrollbar arrows appropriately.
  if ((Orientation() == kHorizontalScrollbar &&
       new_frame_rect.height() != frame_rect_.height()) ||
      (Orientation() == kVerticalScrollbar &&
       new_frame_rect.width() != frame_rect_.width())) {
    return false;
  }
  gfx::Size track_canvas_size =
      GetTheme().NinePatchTrackAndButtonsCanvasSize(*this);
  return track_canvas_size.height() < new_frame_rect.height() ||
         track_canvas_size.width() < new_frame_rect.width();
}

bool Scrollbar::ShouldParticipateInHitTesting() {
  CHECK(scrollable_area_);
  // Non-overlay scrollbars should always participate in hit testing.
  if (!IsOverlayScrollbar())
    return true;
  return !scrollable_area_->ScrollbarsHiddenIfOverlay();
}

bool Scrollbar::IsWindowActive() const {
  return scrollable_area_ && scrollable_area_->IsActive();
}

gfx::Point Scrollbar::ConvertFromRootFrame(
    const gfx::Point& point_in_root_frame) const {
  if (scrollable_area_) {
    gfx::Point parent_point;
    if (scrollable_area_->IsRootFrameLayoutViewport()) {
      // When operating on the root frame viewport's scrollbar, use the visual
      // viewport relative position, instead of root frame-relative position.
      // This allows us to operate on the layout viewport's scrollbar when there
      // is a page scale factor and visual viewport offsets, since the layout
      // viewport scrollbars are not affected by these.
      parent_point = scrollable_area_->ConvertFromRootFrameToVisualViewport(
          point_in_root_frame);
    } else {
      parent_point =
          scrollable_area_->ConvertFromRootFrame(point_in_root_frame);
    }
    return scrollable_area_
        ->ConvertFromContainingEmbeddedContentViewToScrollbar(*this,
                                                              parent_point);
  }

  return point_in_root_frame;
}

gfx::Rect Scrollbar::ConvertToContainingEmbeddedContentView(
    const gfx::Rect& local_rect) const {
  if (scrollable_area_) {
    return scrollable_area_
        ->ConvertFromScrollbarToContainingEmbeddedContentView(*this,
                                                              local_rect);
  }

  return local_rect;
}

gfx::Point Scrollbar::ConvertFromContainingEmbeddedContentView(
    const gfx::Point& parent_point) const {
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
    return scrollable_area_->GetScrollOffset().x() -
           scrollable_area_->MinimumScrollOffset().x();
  }

  return scrollable_area_->GetScrollOffset().y() -
         scrollable_area_->MinimumScrollOffset().y();
}

float Scrollbar::ScrollableAreaTargetPos() const {
  if (!scrollable_area_)
    return 0;

  if (orientation_ == kHorizontalScrollbar) {
    return scrollable_area_->GetScrollAnimator().DesiredTargetOffset().x() -
           scrollable_area_->MinimumScrollOffset().x();
  }

  return scrollable_area_->GetScrollAnimator().DesiredTargetOffset().y() -
         scrollable_area_->MinimumScrollOffset().y();
}

void Scrollbar::SetNeedsPaintInvalidation(ScrollbarPart invalid_parts) {
  needs_update_display_ = true;
  if (theme_.ShouldRepaintAllPartsOnInvalidation()) {
    invalid_parts = kAllParts;
  }
  if (invalid_parts & ~kThumbPart) {
    track_and_buttons_need_repaint_ = true;
  }
  if (invalid_parts & kThumbPart) {
    thumb_needs_repaint_ = true;
  }
  if (scrollable_area_) {
    scrollable_area_->SetScrollbarNeedsPaintInvalidation(Orientation());
  }
}

CompositorElementId Scrollbar::GetElementId() const {
  DCHECK(scrollable_area_);
  return scrollable_area_->GetScrollbarElementId(orientation_);
}

float Scrollbar::ScaleFromDIP() const {
  return scrollable_area_ ? scrollable_area_->ScaleFromDIP() : 1.0f;
}

float Scrollbar::EffectiveZoom() const {
  if (style_source_) {
    return style_source_->StyleRef().EffectiveZoom();
  }
  return 1.0;
}

bool Scrollbar::ContainerIsRightToLeft() const {
  if (style_source_) {
    TextDirection dir = style_source_->StyleRef().Direction();
    return IsRtl(dir);
  }
  return false;
}

bool Scrollbar::ContainerIsFormControl() const {
  if (!style_source_) {
    return false;
  }
  if (const auto* element = DynamicTo<Element>(style_source_->GetNode())) {
    return element->IsFormControlElement();
  }
  return false;
}

EScrollbarWidth Scrollbar::CSSScrollbarWidth() const {
  if (style_source_) {
    return style_source_->StyleRef().UsedScrollbarWidth();
  }
  return EScrollbarWidth::kAuto;
}

std::optional<blink::Color> Scrollbar::ScrollbarThumbColor() const {
  if (style_source_) {
    return style_source_->StyleRef().ScrollbarThumbColorResolved();
  }
  return std::nullopt;
}

std::optional<blink::Color> Scrollbar::ScrollbarTrackColor() const {
  if (style_source_) {
    return style_source_->StyleRef().ScrollbarTrackColorResolved();
  }
  return std::nullopt;
}

bool Scrollbar::IsOpaque() const {
  if (IsOverlayScrollbar()) {
    return false;
  }

  std::optional<blink::Color> track_color = ScrollbarTrackColor();
  if (!track_color) {
    // The native themes should ensure opaqueness of non-overlay scrollbars.
    return true;
  }
  return track_color->IsOpaque();
}

mojom::blink::ColorScheme Scrollbar::UsedColorScheme() const {
  if (!scrollable_area_) {
    return mojom::blink::ColorScheme::kLight;
  }
  return IsOverlayScrollbar()
             ? scrollable_area_->GetOverlayScrollbarColorScheme()
             : scrollable_area_->UsedColorSchemeScrollbars();
}

LayoutBox* Scrollbar::GetLayoutBox() const {
  return scrollable_area_ ? scrollable_area_->GetLayoutBox() : nullptr;
}

bool Scrollbar::IsScrollCornerVisible() const {
  return scrollable_area_ && scrollable_area_->IsScrollCornerVisible();
}

bool Scrollbar::ShouldPaint() const {
  // When the frame is throttled, the scrollbar will not be painted because
  // the frame has not had its lifecycle updated.
  return scrollable_area_ && !scrollable_area_->IsThrottled();
}

bool Scrollbar::LastKnownMousePositionInFrameRect() const {
  return scrollable_area_ &&
         FrameRect().Contains(scrollable_area_->LastKnownMousePosition());
}

const ui::ColorProvider* Scrollbar::GetColorProvider(
    mojom::blink::ColorScheme color_scheme) const {
  if (const auto* box = GetLayoutBox()) {
    return box->GetDocument().GetColorProviderForPainting(color_scheme);
  }
  return nullptr;
}

bool Scrollbar::InForcedColorsMode() const {
  if (const auto* box = GetLayoutBox()) {
    return box->GetDocument().InForcedColorsMode();
  }
  return false;
}

}  // namespace blink
