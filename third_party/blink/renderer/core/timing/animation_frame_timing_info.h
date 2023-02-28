// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_ANIMATION_FRAME_TIMING_INFO_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_ANIMATION_FRAME_TIMING_INFO_H_

#include "base/time/time.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

// It would be marginally more efficient to have this as a struct and copy
// without GC, but this class is going to be expanded to hold a vector of
// scripts so making it GCed from the get go as the overhead would be
// negligible.
class AnimationFrameTimingInfo
    : public GarbageCollected<AnimationFrameTimingInfo> {
 public:
  explicit AnimationFrameTimingInfo(base::TimeTicks start_time)
      : frame_start_time(start_time) {}
  void SetRenderStartTime(base::TimeTicks time) { render_start_time = time; }

  void SetStyleAndLayoutStartTime(base::TimeTicks time) {
    style_and_layout_start_time = time;
  }

  void SetRenderEndTime(base::TimeTicks time) { render_end_time = time; }

  base::TimeTicks FrameStartTime() const { return frame_start_time; }
  base::TimeTicks RenderStartTime() const { return render_start_time; }
  base::TimeTicks StyleAndLayoutStartTime() const {
    return style_and_layout_start_time;
  }
  base::TimeTicks RenderEndTime() const { return render_end_time; }
  base::TimeDelta Duration() const {
    return RenderEndTime() - FrameStartTime();
  }

  virtual void Trace(Visitor*) const {}

 private:
  // Measured at the beginning of the first task that caused a frame update,
  // or at the beginning of rendering.
  base::TimeTicks frame_start_time;

  // Measured right before BeginMainFrame ("update the rendering").
  base::TimeTicks render_start_time;

  // Measured when we start the main frame lifecycle of styling and layouting.
  base::TimeTicks style_and_layout_start_time;

  // Measured after BeginMainFrame, or at the end of a task that did not trigger
  // a main frame update
  base::TimeTicks render_end_time;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_ANIMATION_FRAME_TIMING_INFO_H_
