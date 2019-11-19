// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_PROGRAMMATIC_SCROLL_ANIMATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_PROGRAMMATIC_SCROLL_ANIMATOR_H_

#include <memory>
#include "third_party/blink/renderer/core/scroll/scroll_animator_compositor_coordinator.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class ScrollableArea;
class CompositorAnimationTimeline;
class CompositorScrollOffsetAnimationCurve;

// ProgrammaticScrollAnimator manages scroll offset animations ("smooth
// scrolls") triggered by web APIs such as "scroll-behavior: smooth" which are
// standardized by the CSSOM View Module (https://www.w3.org/TR/cssom-view-1/).
//
// For scroll animations triggered by user input, see ScrollAnimator and
// ScrollAnimatorMac.

class ProgrammaticScrollAnimator : public ScrollAnimatorCompositorCoordinator {
  DISALLOW_COPY_AND_ASSIGN(ProgrammaticScrollAnimator);

 public:
  explicit ProgrammaticScrollAnimator(ScrollableArea*);
  ~ProgrammaticScrollAnimator() override;

  void ScrollToOffsetWithoutAnimation(const ScrollOffset&,
                                      bool is_sequenced_scroll);
  void AnimateToOffset(const ScrollOffset&,
                       bool is_sequenced_scroll = false,
                       ScrollableArea::ScrollCallback on_finish =
                           ScrollableArea::ScrollCallback());

  // ScrollAnimatorCompositorCoordinator implementation.
  void ResetAnimationState() override;
  void CancelAnimation() override;
  void TakeOverCompositorAnimation() override {}
  ScrollableArea* GetScrollableArea() const override {
    return scrollable_area_;
  }
  void TickAnimation(double monotonic_time) override;
  void UpdateCompositorAnimations() override;
  void NotifyCompositorAnimationFinished(int group_id) override;
  void NotifyCompositorAnimationAborted(int group_id) override {}
  void LayerForCompositedScrollingDidChange(
      CompositorAnimationTimeline*) override;

  void Trace(blink::Visitor*) override;

 private:
  void NotifyOffsetChanged(const ScrollOffset&);
  void AnimationFinished();

  Member<ScrollableArea> scrollable_area_;
  std::unique_ptr<CompositorScrollOffsetAnimationCurve> animation_curve_;
  ScrollOffset target_offset_;
  double start_time_;
  // is_sequenced_scroll_ is true for the entire duration of an animated scroll
  // as well as during an instant scroll if that scroll is part of a sequence.
  // It resets to false at the end of the scroll. It controls whether we should
  // abort the smooth scroll sequence after an instant SetScrollOffset.
  bool is_sequenced_scroll_;
  // on_finish_ is a callback to call on animation finished, cancelled, or
  // otherwise interrupted in any way.
  ScrollableArea::ScrollCallback on_finish_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_PROGRAMMATIC_SCROLL_ANIMATOR_H_
