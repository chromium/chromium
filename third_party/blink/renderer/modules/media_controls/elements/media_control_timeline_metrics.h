// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_ELEMENTS_MEDIA_CONTROL_TIMELINE_METRICS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_ELEMENTS_MEDIA_CONTROL_TIMELINE_METRICS_H_

#include "third_party/blink/public/common/screen_orientation/web_screen_orientation_type.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// Helpers for tracking and reporting media control timeline metrics to UMA.
class MediaControlTimelineMetrics {
  DISALLOW_NEW();

 public:
  // Start tracking a pointer gesture. |fromThumb| indicates whether the user
  // started dragging from the thumb, as opposed to pressing down their pointer
  // on some other part of the timeline track (causing time to jump).
  void StartGesture(bool from_thumb);
  // Finish tracking and report a pointer gesture.
  void RecordEndGesture(int timeline_width, double media_duration_seconds);

  // Start tracking a keydown. Ok to call multiple times if key repeats.
  void StartKey();
  // Finish tracking and report a keyup. Call only once even if key repeats.
  void RecordEndKey(int timeline_width, int key_code);

  // Track an incremental input event caused by the current pointer gesture or
  // pressed key. Each sequence of calls to this should usually be sandwiched by
  // startGesture/Key and recordEndGesture/Key.
  void OnInput(double from_seconds, double to_seconds);

 private:
  enum class State {
    // No active gesture. Progresses to kKeyDown on |startKey|, or
    // kGestureFromThumb/kGestureFromElsewhere on |startGesture|.
    kInactive,

    // Pointer down on thumb. Progresses to kDragFromThumb in |onInput|.
    kGestureFromThumb,
    // Thumb is being dragged (drag started from thumb).
    kDragFromThumb,

    // Pointer down on track. Progresses to kClick in |onInput|.
    kGestureFromElsewhere,
    // Pointer down followed by input. Assumed to be a click, unless additional
    // |onInput| are received - if so progresses to kDragFromElsewhere.
    kClick,
    // Thumb is being dragged (drag started from track).
    kDragFromElsewhere,

    // A key is currently pressed down.
    kKeyDown
  };

  bool has_never_been_playing_ = true;

  State state_ = State::kInactive;

  // The following are only valid during a pointer gesture.
  base::TimeTicks drag_start_time_ticks_;
  float drag_delta_media_seconds_ = 0;
  float drag_sum_abs_delta_media_seconds_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_ELEMENTS_MEDIA_CONTROL_TIMELINE_METRICS_H_
