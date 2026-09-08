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
// in unit tests. It separates paint, setting presentation time, and running
// presentation callbacks to enable tests to control when these steps happen.
class MockPaintTimingCallbackManager final
    : public GarbageCollected<MockPaintTimingCallbackManager>,
      public PaintTiming::CallbackManager {
 public:
  explicit MockPaintTimingCallbackManager();

  // `PaintTimingCallbackManager` implementation:
  void RegisterCallback(PaintTiming::ReportTimeCallback) override;

  // `PaintTimingMixin` implementation:
  void Trace(Visitor* visitor) const override {}

  // Inserts a frame boundary used to differentiate pending callbacks.
  void OnAnimationFrameComplete();

  // Sets the presentation time for the next unpresented frame, but does not run
  // callbacks.
  void OnAnimationFramePresented(base::TimeTicks presentation_time);

  // Invokes presentation time callbacks for the next frame based on frame
  // boundaries set by `OnAnimationFrameComplete()` and the presentation time
  // set in `OnAnimationFramePresented()`.
  void InvokeCallbacksForNextAnimationFrame();

  // Invokes presentation time callbacks for the last frame based on frame
  // boundaries set by `OnAnimationFrameComplete()` and the presentation time
  // set in `OnAnimationFramePresented()`. This can be used to test out-of-order
  // presentation feedback.
  void InvokeCallbacksForLastAnimationFrame();

  void Shutdown();

 private:
  struct FrameData {
    Vector<PaintTiming::ReportTimeCallback> callbacks;
    base::TimeTicks presentation_time;
  };

  void InvokeCallbacksForFrameData(FrameData&);

  // `FrameData` for the current animation frame. New callbacks are added here.
  FrameData current_frame_data_;

  // `FrameData` for frames that have been completed and are either pending
  // presentation time (`OnFramePresented`) or pending execution.
  Deque<FrameData> pending_frame_data_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_MOCK_PAINT_TIMING_CALLBACK_MANAGER_H_
