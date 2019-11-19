/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLL_ANIMATOR_MAC_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLL_ANIMATOR_MAC_H_

#include <memory>

#include "base/mac/scoped_nsobject.h"
#include "base/single_thread_task_runner.h"
#include "third_party/blink/renderer/core/scroll/scroll_animator_base.h"
#include "third_party/blink/renderer/platform/geometry/float_point.h"
#include "third_party/blink/renderer/platform/geometry/float_size.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"
#include "third_party/blink/renderer/platform/timer.h"

@class BlinkScrollAnimationHelperDelegate;
@class BlinkScrollbarPainterControllerDelegate;
@class BlinkScrollbarPainterDelegate;

typedef id ScrollbarPainterController;

namespace blink {

class Scrollbar;

// ScrollAnimatorMac implements keyboard-triggered scroll offset animations,
// scrollbar painting, and scrollbar opacity animations by delegating to native
// Cocoa APIs.
//
// Scroll offset animations are also known as "smooth scrolling".  For the
// non-Mac implementation of user input smooth scrolling, see ScrollAnimator.
// For programmatic (CSSOM) smooth scrolls, see ProgrammaticScrollAnimator.
//
// Unlike ScrollAnimator, ScrollAnimatorMac only smooth-scrolls keyboard
// scrolls, and not mouse wheel scrolls.  It also does not use compositor
// animations or any of the standard Blink animation machinery.
//
// This divergence is mostly historical.  We could probably switch Mac to use
// ScrollAnimator for smooth scrolls if we factored out the scrollbar-related
// logic.  See crbug.com/574283 and crbug.com/682209.
//
// ScrollAnimatorMac's scroll offset animations are implemented by
// NSScrollAnimationHelper which invokes a BlinkScrollAnimationHelperDelegate to
// service an animation frame by performing an immediate scroll to the requested
// offset (via NotifyOffsetChanged).
//
// The "scrollbar painter controller" is an NSScrollerImpPair object, which
// calls back into Blink via BlinkScrollbarPainterControllerDelegate.
//
// The "scrollbar painter" is an NSScrollerImp object, which calls back into
// Blink via BlinkScrollbarPainterDelegate.  The scrollbar painter is registered
// with ScrollbarThemeMac, so that the ScrollbarTheme painting APIs can call
// into it.
//
// The scrollbar painter initiates an overlay scrollbar fade-out animation by
// calling animateKnobAlphaTo on the delegate.  This starts a timer inside the
// BlinkScrollbarPartAnimationTimer.  Each tick evaluates a cubic bezier
// function to obtain the current opacity, which is stored in the scrollbar
// painter with setKnobAlpha.
//
// If the scroller is composited, the opacity value stored on the scrollbar
// painter is subsequently read out through ScrollbarThemeMac::ThumbOpacity and
// plumbed into PaintedScrollbarLayerImpl::thumb_opacity_.
//
// TODO: explain other types of animations (TrackAlpha, UIStateTransition,
// ExpansionTransition), scrollbar paint timer, plumbing of scrollbar paint
// invalidations.

class CORE_EXPORT ScrollAnimatorMac : public ScrollAnimatorBase {
  USING_PRE_FINALIZER(ScrollAnimatorMac, Dispose);

 public:
  ScrollAnimatorMac(ScrollableArea*);
  ~ScrollAnimatorMac() override;

  void Dispose() override;

  void ImmediateScrollToOffsetForScrollAnimation(
      const ScrollOffset& new_offset);
  bool HaveScrolledSincePageLoad() const {
    return have_scrolled_since_page_load_;
  }

  void UpdateScrollerStyle();

  bool ScrollbarPaintTimerIsActive() const;
  void StartScrollbarPaintTimer();
  void StopScrollbarPaintTimer();

  void SendContentAreaScrolledSoon(const ScrollOffset& scroll_delta);

  void Trace(blink::Visitor* visitor) override {
    ScrollAnimatorBase::Trace(visitor);
  }

 private:
  base::scoped_nsobject<id> scroll_animation_helper_;
  base::scoped_nsobject<BlinkScrollAnimationHelperDelegate>
      scroll_animation_helper_delegate_;

  base::scoped_nsobject<ScrollbarPainterController>
      scrollbar_painter_controller_;
  base::scoped_nsobject<BlinkScrollbarPainterControllerDelegate>
      scrollbar_painter_controller_delegate_;
  base::scoped_nsobject<BlinkScrollbarPainterDelegate>
      horizontal_scrollbar_painter_delegate_;
  base::scoped_nsobject<BlinkScrollbarPainterDelegate>
      vertical_scrollbar_painter_delegate_;

  void InitialScrollbarPaintTask();
  TaskHandle initial_scrollbar_paint_task_handle_;

  void SendContentAreaScrolledTask();
  TaskHandle send_content_area_scrolled_task_handle_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  ScrollOffset content_area_scrolled_timer_scroll_delta_;

  ScrollResult UserScroll(ScrollGranularity,
                          const ScrollOffset& delta,
                          ScrollableArea::ScrollCallback on_finish) override;
  void ScrollToOffsetWithoutAnimation(const ScrollOffset&) override;

  void CancelAnimation() override;

  void ContentAreaWillPaint() const override;
  void MouseEnteredContentArea() const override;
  void MouseExitedContentArea() const override;
  void MouseMovedInContentArea() const override;
  void MouseEnteredScrollbar(Scrollbar&) const override;
  void MouseExitedScrollbar(Scrollbar&) const override;
  void ContentsResized() const override;
  void ContentAreaDidShow() const override;
  void ContentAreaDidHide() const override;

  void FinishCurrentScrollAnimations() override;

  void DidAddVerticalScrollbar(Scrollbar&) override;
  void WillRemoveVerticalScrollbar(Scrollbar&) override;
  void DidAddHorizontalScrollbar(Scrollbar&) override;
  void WillRemoveHorizontalScrollbar(Scrollbar&) override;

  void NotifyContentAreaScrolled(const ScrollOffset& delta,
                                 ScrollType) override;

  bool SetScrollbarsVisibleForTesting(bool) override;

  ScrollOffset AdjustScrollOffsetIfNecessary(const ScrollOffset&) const;

  void ImmediateScrollTo(const ScrollOffset&);

  bool have_scrolled_since_page_load_;
  bool needs_scroller_style_update_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLL_ANIMATOR_MAC_H_
