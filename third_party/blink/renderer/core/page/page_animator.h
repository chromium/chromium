// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_PAGE_ANIMATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_PAGE_ANIMATOR_H_

#include "third_party/blink/renderer/core/animation/animation_clock.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/document_lifecycle.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class LocalFrame;
class Page;

class CORE_EXPORT PageAnimator final : public GarbageCollected<PageAnimator> {
 public:
  explicit PageAnimator(Page&);

  void Trace(blink::Visitor*);
  void ScheduleVisualUpdate(LocalFrame*);
  void ServiceScriptedAnimations(
      base::TimeTicks monotonic_animation_start_time);
  void PostAnimate();

  bool IsServicingAnimations() const { return servicing_animations_; }

  // TODO(alancutter): Remove the need for this by implementing frame request
  // suppression logic at the BeginMainFrame level. This is a temporary
  // workaround to fix a perf regression.
  // DO NOT use this outside of crbug.com/704763.
  void SetSuppressFrameRequestsWorkaroundFor704763Only(bool);

  // See documents of methods with the same names in LocalFrameView class.
  void UpdateAllLifecyclePhases(
      LocalFrame& root_frame,
      DocumentLifecycle::LifecycleUpdateReason reason);
  void UpdateAllLifecyclePhasesExceptPaint(LocalFrame& root_frame);
  void UpdateLifecycleToLayoutClean(LocalFrame& root_frame);
  AnimationClock& Clock() { return animation_clock_; }

 private:
  Member<Page> page_;
  bool servicing_animations_;
  bool updating_layout_and_style_for_painting_;
  bool suppress_frame_requests_workaround_for704763_only_ = false;
  AnimationClock animation_clock_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_PAGE_ANIMATOR_H_
