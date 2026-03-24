// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_MOCK_PAINT_TIMING_CALLBACK_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_MOCK_PAINT_TIMING_CALLBACK_MANAGER_H_

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"

namespace blink {

// `MockPaintTimingCallbackManager` is used to mock presentation time callbacks
// int unit tests.
class MockPaintTimingCallbackManager final
    : public GarbageCollected<MockPaintTimingCallbackManager>,
      public PaintTiming::CallbackManager {
 public:
  explicit MockPaintTimingCallbackManager();

  // `PaintTimingCallbackManager` implementation:
  void RegisterCallback(PaintTiming::ReportTimeCallback) override;

  // `PaintTimingMixin` implementation:
  void Trace(Visitor* visitor) const override {}

  // Inserts a frame boundary used to differentiate pending callbacks. See
  // `InvokeCallbacksForOneAnimationFrame()`.
  void OnAnimationFrameComplete();

  // Invokes presentation time callbacks for one frame based on frame boundaries
  // set by `OnFrameComplete()`.
  void InvokeCallbacksForOneAnimationFrame(base::TimeTicks presentation_time);

 private:
  void InvokeCallback(base::TimeTicks presentation_time);

  Deque<PaintTiming::ReportTimeCallback> callbacks_;
  bool is_fence_set_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_MOCK_PAINT_TIMING_CALLBACK_MANAGER_H_
