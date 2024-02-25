// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_SYNTHETIC_GESTURE_H_
#define CONTENT_COMMON_INPUT_SYNTHETIC_GESTURE_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/common/input/synthetic_gesture_params.h"

namespace content {

class SyntheticGestureTarget;
class SyntheticGestureController;

// Abstract base class for synthetic gesture implementations. A synthetic
// gesture class is responsible for forwarding InputEvents, simulating the
// gesture, to a SyntheticGestureTarget.
//
// Adding new gesture types involved the following steps:
//   1) Create a sub-type of SyntheticGestureBase that implements the gesture.
//   2) Extend SyntheticGesture::Create with the new class.
//   3) Add at least one unit test per supported input source type (touch,
//      mouse, etc) to SyntheticGestureController unit tests. The unit tests
//      only checks basic functionality and termination. If the gesture is
//      hooked up to Telemetry its correctness can additionally be tested there.
class CONTENT_EXPORT SyntheticGesture {
 public:
  explicit SyntheticGesture(std::unique_ptr<SyntheticGestureParams> params);

  SyntheticGesture(const SyntheticGesture&) = delete;
  SyntheticGesture& operator=(const SyntheticGesture&) = delete;

  virtual ~SyntheticGesture();

  enum Result {
    GESTURE_RUNNING,
    GESTURE_FINISHED,
    // Received when the user input parameters for SyntheticPointerAction are
    // invalid.
    POINTER_ACTION_INPUT_INVALID,
    GESTURE_SOURCE_TYPE_NOT_IMPLEMENTED,
    // Returned when the gesture causes the destruction of the dispatching
    // controller.
    GESTURE_ABORT,
    GESTURE_RESULT_MAX = GESTURE_ABORT
  };

  // Update the state of the gesture and forward the appropriate events to the
  // platform. This function is called repeatedly by the synthetic gesture
  // controller until it stops returning GESTURE_RUNNING.
  virtual Result ForwardInputEvents(
      const base::TimeTicks& timestamp, SyntheticGestureTarget* target) = 0;

  virtual void WaitForTargetAck(base::OnceClosure callback,
                                SyntheticGestureTarget* target) const;

  // Returns whether the gesture events can be dispatched at high frequency
  // (e.g. at 120Hz), instead of the regular frequency (at 60Hz). Some gesture
  // interact differently depending on how long they take (e.g. the TAP gesture
  // generates a click only if its duration is longer than a threshold).
  virtual bool AllowHighFrequencyDispatch() const;

  // Called when the gesture is queued with a SyntheticGestureController.
  void DidQueue(base::WeakPtr<SyntheticGestureController> controller);

  bool IsFromDevToolsDebugger() const;
  float GetVsyncOffsetMs() const;
  content::mojom::InputEventPattern InputEventPattern() const;

 protected:
  // This is null until the gesture is queued with a controller. It must be set
  // before calling ForwardInputEvents. A gesture can cause the destruction of
  // the WebContents hosting its controller (e.g. click on the tab-close
  // button). This WeakPtr is necessary to know if this happens and abort.
  base::WeakPtr<SyntheticGestureController> dispatching_controller_;

  std::unique_ptr<SyntheticGestureParams> params_;
};

template <class ParamType>
class SyntheticGestureBase : public SyntheticGesture {
 public:
  explicit SyntheticGestureBase(const SyntheticGestureParams& params)
      : SyntheticGesture(std::make_unique<ParamType>(
            *static_cast<const ParamType*>(&params))) {}
  ~SyntheticGestureBase() override = default;

  SyntheticGestureBase(const SyntheticGesture&) = delete;
  SyntheticGestureBase<ParamType>& operator=(
      const SyntheticGestureBase<ParamType>&) = delete;

 protected:
  const ParamType& params() const {
    CHECK(params_);
    return *static_cast<const ParamType*>(params_.get());
  }
  ParamType& params() {
    CHECK(params_);
    return *static_cast<ParamType*>(params_.get());
  }
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_SYNTHETIC_GESTURE_H_
