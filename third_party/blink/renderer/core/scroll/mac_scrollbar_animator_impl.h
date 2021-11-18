// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_MAC_SCROLLBAR_ANIMATOR_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_MAC_SCROLLBAR_ANIMATOR_IMPL_H_

#include "base/mac/scoped_nsobject.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/core/scroll/mac_scrollbar_animator.h"
#include "third_party/blink/renderer/core/scroll/scroll_animator_base.h"
#include "third_party/blink/renderer/platform/geometry/float_size.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "ui/gfx/geometry/point_f.h"

@class BlinkScrollbarObserver;
@class BlinkScrollbarPainterControllerDelegate;
@class BlinkScrollbarPainterDelegate;

typedef id ScrollbarPainterController;
typedef id ScrollbarPainter;

namespace blink {

class ScrollbarThemeMac;

class PLATFORM_EXPORT MacScrollbarImpl : public MacScrollbar {
 public:
  MacScrollbarImpl(Scrollbar&,
                   base::scoped_nsobject<ScrollbarPainterController>,
                   scoped_refptr<base::SingleThreadTaskRunner>,
                   ScrollbarThemeMac*,
                   std::unique_ptr<MacScrollbarImpl> old_scrollbar);
  ~MacScrollbarImpl() override;

  static MacScrollbarImpl* GetForScrollbar(const Scrollbar&);

  void SetEnabled(bool) final;
  void SetOverlayColorTheme(ScrollbarOverlayColorTheme) final;
  float GetKnobAlpha() final;
  float GetTrackAlpha() final;
  int GetTrackBoxWidth() final;

  BlinkScrollbarPainterDelegate* painter_delegate() {
    return painter_delegate_.get();
  }
  BlinkScrollbarObserver* observer() { return observer_.get(); }
  ScrollbarPainter painter();

 private:
  const bool is_horizontal_;

  // `painter_controller_` is also owned by the MacScrollbarAnimatorImpl
  // that owns `this`.
  base::scoped_nsobject<ScrollbarPainterController> painter_controller_;

  base::scoped_nsobject<BlinkScrollbarPainterDelegate> painter_delegate_;

  base::scoped_nsobject<BlinkScrollbarObserver> observer_;
};

// This class handles scrollbar opacity animations by delegating to native
// Cocoa APIs (.mm).
// It was created with the goal of solving (crbug.com/682209), but we still
// need to replace the Cocoa APIs calls by platform-agnostic code.
//
// The animations handled are:
// - knob alpha : thumb transparency animation
// - track alpha : track transparency animation
// - ui state transition
// - expansion transition
// (these are enumerated by |FeatureToAnimate|)
//
// All these animation are theme-related, it means that they're specific to
// the ScrollbarThemeMac theme, and use the Cocoa private APIs to drive that
// animation.
//
// The objective-c classes defined on the .mm file are used to glue our blink
// code with the Cocoa APIs for animation.
//
// - |BlinkScrollbarPartAnimation|: animates the properties of a
// |ScrollbarPainter| (|NSScrollerImp|) object based on a |FeatureToAnimate|.
// - |BlinkScrollbarPartAnimationTimer|: Implements the curve that maps the time
// elapsed in the animation to the animation progress.
// - |BlinkScrollbarPainterControllerDelegate|: Delegates tasks from a
// |ScrollbarPainterController| (|NSScrollerImpPair|) to the |ScrollableArea|
// where the ScrollbarPainter is painting
// - |BlinkScrollbarPainterDelegate|: Delegates the creation and running of all
// 4 |BlinkScrollbarPartAnimation| on a |ScrollbarPainter|.
//
//
// The usage of these classes follow:
//
// The "scrollbar painter controller" calls back into Blink via
// BlinkScrollbarPainterControllerDelegate.
//
// The "scrollbar painter" calls back into Blink via
// BlinkScrollbarPainterDelegate.  The scrollbar painter is registered
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
class PLATFORM_EXPORT MacScrollbarAnimatorImpl : public MacScrollbarAnimator {
 public:
  MacScrollbarAnimatorImpl(ScrollableArea*);
  virtual ~MacScrollbarAnimatorImpl() = default;

  bool needs_scroller_style_update_ = false;
  ScrollOffset content_area_scrolled_timer_scroll_delta_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  TaskHandle initial_scrollbar_paint_task_handle_;
  TaskHandle send_content_area_scrolled_task_handle_;

  // MacScrollbarAnimator overrides
  void ContentAreaWillPaint() const override;
  void MouseEnteredContentArea() const override;
  void MouseExitedContentArea() const override;
  void MouseMovedInContentArea() const override;
  void MouseEnteredScrollbar(Scrollbar&) const override;
  void MouseExitedScrollbar(Scrollbar&) const override;
  void ContentsResized() const override;
  void DidAddVerticalScrollbar(Scrollbar&) override;
  void WillRemoveVerticalScrollbar(Scrollbar&) override;
  void DidAddHorizontalScrollbar(Scrollbar&) override;
  void WillRemoveHorizontalScrollbar(Scrollbar&) override;
  bool SetScrollbarsVisibleForTesting(bool) override;
  void DidChangeUserVisibleScrollOffset(
      const ScrollOffset& offset_delta) override;

  void UpdateScrollerStyle() override;

  void InitialScrollbarPaintTask();
  void SendContentAreaScrolledTask();

  bool ScrollbarPaintTimerIsActive() const override;
  void StartScrollbarPaintTimer() override;
  void StopScrollbarPaintTimer() override;

  void Dispose() override;

  void Trace(Visitor* visitor) const override {
    visitor->Trace(scrollable_area_);
    MacScrollbarAnimator::Trace(visitor);
  }

 protected:
  // Recreate the scrollbar painter for the specified scrollbar.
  void RecreateScrollbarPainter(Scrollbar& scrollbar);

  base::scoped_nsobject<ScrollbarPainterController>
      scrollbar_painter_controller_;
  base::scoped_nsobject<BlinkScrollbarPainterControllerDelegate>
      scrollbar_painter_controller_delegate_;

  std::unique_ptr<MacScrollbarImpl> horizontal_scrollbar_;
  std::unique_ptr<MacScrollbarImpl> vertical_scrollbar_;

  Member<ScrollableArea> scrollable_area_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_MAC_SCROLLBAR_ANIMATOR_IMPL_H_
