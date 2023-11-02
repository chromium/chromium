// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_GESTURE_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_GESTURE_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/common/input/synthetic_gesture_params.h"

namespace content {

class SyntheticGestureTarget;

// Base class for synthetic gesture implementations. A synthetic gesture class
// is responsible for forwarding InputEvents, simulating the gesture, to a
// SyntheticGestureTarget.
//
// Adding new gesture types involved the following steps:
//   1) Create a sub-type of SyntheticGesture that implements the gesture.
//   2) Extend SyntheticGesture::Create with the new class.
//   3) Add at least one unit test per supported input source type (touch,
//      mouse, etc) to SyntheticGestureController unit tests. The unit tests
//      only checks basic functionality and termination. If the gesture is
//      hooked up to Telemetry its correctness can additionally be tested there.
class CONTENT_EXPORT SyntheticGesture {
 public:
  SyntheticGesture();

  SyntheticGesture(const SyntheticGesture&) = delete;
  SyntheticGesture& operator=(const SyntheticGesture&) = delete;

  virtual ~SyntheticGesture();

  static std::unique_ptr<SyntheticGesture> Create(
      const SyntheticGestureParams& gesture_params);

  enum Result {
    GESTURE_RUNNING,
    GESTURE_FINISHED,
    // Received when the user input parameters for SyntheticPointerAction are
    // invalid.
    POINTER_ACTION_INPUT_INVALID,
    GESTURE_SOURCE_TYPE_NOT_IMPLEMENTED,
    GESTURE_RESULT_MAX = GESTURE_SOURCE_TYPE_NOT_IMPLEMENTED
  };

  // Update the state of the gesture and forward the appropriate events to the
  // platform. This function is called repeatedly by the synthetic gesture
  // controller until it stops returning GESTURE_RUNNING.
  virtual Result ForwardInputEvents(
      const base::TimeTicks& timestamp, SyntheticGestureTarget* target) = 0;

  virtual void WaitForTargetAck(base::OnceClosure callback,
                                SyntheticGestureTarget* target) const;

  // Returns whether the gesture events can be dispatched at high frequency
  // (e.g. at 120Hz), instead of the regular frequence (at 60Hz). Some gesture
  // interact differently depending on how long they take (e.g. the TAP gesture
  // generates a click only if its duration is longer than a threshold).
  virtual bool AllowHighFrequencyDispatch() const;

 protected:
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_GESTURE_H_
