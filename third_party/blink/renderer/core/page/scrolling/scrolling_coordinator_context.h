// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_SCROLLING_COORDINATOR_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_SCROLLING_COORDINATOR_CONTEXT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation_timeline.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace cc {
class AnimationHost;
}

namespace blink {

// This enscapsulates ScrollingCoordinator state for each local frame root.
// TODO(kenrb): This class could be temporary depending on how
// https://crbug.com/680606 is resolved.
class CORE_EXPORT ScrollingCoordinatorContext final {
  USING_FAST_MALLOC(ScrollingCoordinatorContext);

 public:
  ScrollingCoordinatorContext() {}
  virtual ~ScrollingCoordinatorContext() {}

  void SetAnimationTimeline(std::unique_ptr<CompositorAnimationTimeline>);
  void SetAnimationHost(cc::AnimationHost*);

  CompositorAnimationTimeline* GetCompositorAnimationTimeline();
  cc::AnimationHost* GetCompositorAnimationHost();

  // Non-fast scrollable regions need updating by ScrollingCoordinator.
  bool ScrollGestureRegionIsDirty() const;
  // Touch event target rects need updating by ScrollingCoordinator.
  bool TouchEventTargetRectsAreDirty() const;
  // ScrollingCoordinator should update whether or not scrolling for this
  // subtree has to happen on the main thread.
  bool ShouldScrollOnMainThreadIsDirty() const;
  bool WasScrollable() const;

  // Only ScrollingCoordinator should ever set |dirty| to |false|.
  void SetScrollGestureRegionIsDirty(bool dirty);
  void SetTouchEventTargetRectsAreDirty(bool dirty);
  void SetShouldScrollOnMainThreadIsDirty(bool dirty);

  void SetWasScrollable(bool was_scrollable);

 private:
  std::unique_ptr<CompositorAnimationTimeline> animation_timeline_;
  cc::AnimationHost* animation_host_ = nullptr;

  bool scroll_gesture_region_is_dirty_ = false;
  bool touch_event_target_rects_are_dirty_ = false;
  bool should_scroll_on_main_thread_is_dirty_ = false;
  bool was_scrollable_ = false;

  DISALLOW_COPY_AND_ASSIGN(ScrollingCoordinatorContext);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_SCROLLING_COORDINATOR_CONTEXT_H_
