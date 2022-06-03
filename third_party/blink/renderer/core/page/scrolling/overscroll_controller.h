// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_OVERSCROLL_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_OVERSCROLL_CONTROLLER_H_

#include "cc/input/overscroll_behavior.h"
#include "third_party/blink/renderer/platform/geometry/float_size.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class ChromeClient;
class FloatPoint;
struct ScrollResult;
class VisualViewport;

// Handles overscroll logic that occurs when the user scolls past the end of the
// document's scroll extents. Currently, this applies only to document scrolling
// and only on the root frame. We "accumulate" overscroll deltas from separate
// scroll events if the user continuously scrolls past the extent but reset it
// as soon as a gesture ends.
class OverscrollController : public GarbageCollected<OverscrollController> {
 public:
  OverscrollController(const VisualViewport&, ChromeClient&);

  void ResetAccumulated(bool reset_x, bool reset_y);

  // Reports unused scroll as overscroll to the content layer. The position
  // argument is the most recent location of the gesture, the finger position
  // for touch scrolling and the cursor position for wheel. Velocity is used
  // in the case of a fling gesture where we want the overscroll to feel like
  // it has momentum.
  void HandleOverscroll(const ScrollResult&,
                        const FloatPoint& position_in_root_frame,
                        const FloatSize& velocity_in_root_frame);

  void Trace(Visitor*) const;

 private:
  WeakMember<const VisualViewport> visual_viewport_;
  WeakMember<ChromeClient> chrome_client_;

  gfx::Vector2dF accumulated_root_overscroll_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_OVERSCROLL_CONTROLLER_H_
