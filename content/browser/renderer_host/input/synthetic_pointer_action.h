// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_POINTER_ACTION_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_POINTER_ACTION_H_

#include <memory>

#include "base/macros.h"
#include "content/browser/renderer_host/input/synthetic_gesture.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target.h"
#include "content/browser/renderer_host/input/synthetic_pointer_driver.h"
#include "content/common/content_export.h"
#include "content/common/input/synthetic_pointer_action_list_params.h"
#include "content/common/input/synthetic_pointer_action_params.h"

namespace content {

// It generates and dispatches the synthetic events of touch, mouse and pen
// inputs. The synthetic events are dispatched to each platform in browser and
// sent to renderer to manipulate the DOM elements on the web pages.
class CONTENT_EXPORT SyntheticPointerAction : public SyntheticGesture {
 public:
  explicit SyntheticPointerAction(
      const SyntheticPointerActionListParams& params);
  ~SyntheticPointerAction() override;

  SyntheticGesture::Result ForwardInputEvents(
      const base::TimeTicks& timestamp,
      SyntheticGestureTarget* target) override;
  bool AllowHighFrequencyDispatch() const override;
  void WaitForTargetAck(base::OnceClosure callback,
                        SyntheticGestureTarget* target) const override;

  void SetSyntheticPointerDriver(
      SyntheticPointerDriver* synthetic_pointer_driver) {
    synthetic_pointer_driver_ = synthetic_pointer_driver;
  }

 private:
  enum class GestureState { UNINITIALIZED, RUNNING, INVALID, DONE };

  GestureState ForwardTouchOrMouseInputEvents(const base::TimeTicks& timestamp,
                                              SyntheticGestureTarget* target);

  // params_ contains a list of lists of pointer actions, that each list of
  // pointer actions will be dispatched together.
  SyntheticPointerActionListParams params_;

  // It is owned by this class, which is used when synthetic_pointer_driver_ is
  // not initialized, where the SyntheticPointerAction object is not created
  // in InputHandler class.
  std::unique_ptr<SyntheticPointerDriver> owned_synthetic_pointer_driver_;
  // It is owned by InputHandler class, which is used to keep the states of the
  // previous synthetic events when a sequence of actions are dispatched one by
  // one.
  SyntheticPointerDriver* synthetic_pointer_driver_ = nullptr;
  SyntheticGestureParams::GestureSourceType gesture_source_type_;
  GestureState state_;
  size_t num_actions_dispatched_;

  DISALLOW_COPY_AND_ASSIGN(SyntheticPointerAction);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_POINTER_ACTION_H_
