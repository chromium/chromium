// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_SYNTHETIC_GESTURE_TARGET_H_
#define CONTENT_COMMON_INPUT_SYNTHETIC_GESTURE_TARGET_H_

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "content/common/input/synthetic_gesture_params.h"

namespace blink {
class WebInputEvent;
}

namespace content {

// Interface between the synthetic gesture controller and the RenderWidgetHost.
class SyntheticGestureTarget {
 public:
  SyntheticGestureTarget() {}
  virtual ~SyntheticGestureTarget() {}

  // Allows synthetic gestures to insert input events in the highest level of
  // input processing on the target platform (e.g. Java on Android), so that
  // the event traverses the entire input processing stack.
  virtual void DispatchInputEventToPlatform(
      const blink::WebInputEvent& event) = 0;

  // Returns the default gesture source type for the target.
  virtual content::mojom::GestureSourceType
  GetDefaultSyntheticGestureSourceType() const = 0;

  // After how much time of inaction does the target assume that a pointer has
  // stopped moving.
  virtual base::TimeDelta PointerAssumedStoppedTime() const = 0;

  // Returns the maximum number of DIPs a touch pointer can move without being
  // considered moving by the platform.
  virtual float GetTouchSlopInDips() const = 0;

  // Returns the maximum change in span (distance between touch points) in DIPs
  // before being considered a pinch-zoom gesture by the platform.
  virtual float GetSpanSlopInDips() const = 0;

  // Returns the minimum number of DIPs two touch pointers have to be apart
  // to perform a pinch-zoom.
  virtual float GetMinScalingSpanInDips() const = 0;

  // If mouse wheels can only specify the number of ticks of some static
  // multiplier constant, this method returns that constant (in DIPs). If mouse
  // wheels can specify an arbitrary delta this returns 0.
  virtual int GetMouseWheelMinimumGranularity() const = 0;

  // This method will cause the system to flush and process all input before
  // resolving the given callback. This is used to ensure that all effects of a
  // gesture have been fully propagated through the system before performing
  // further actions.
  virtual void WaitForTargetAck(SyntheticGestureParams::GestureType type,
                                content::mojom::GestureSourceType source,
                                base::OnceClosure callback) const = 0;

  virtual void GetVSyncParameters(base::TimeTicks& timebase,
                                  base::TimeDelta& interval) const = 0;
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_SYNTHETIC_GESTURE_TARGET_H_
