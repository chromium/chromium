// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_POINTER_ACTION_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_POINTER_ACTION_H_

#include "base/macros.h"
#include "content/browser/renderer_host/input/synthetic_gesture.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target.h"
#include "content/browser/renderer_host/input/synthetic_pointer_driver.h"
#include "content/common/content_export.h"
#include "content/common/input/synthetic_pointer_action_list_params.h"
#include "content/common/input/synthetic_pointer_action_params.h"

namespace content {

class CONTENT_EXPORT SyntheticPointerAction : public SyntheticGesture {
 public:
  explicit SyntheticPointerAction(
      const SyntheticPointerActionListParams& params);
  ~SyntheticPointerAction() override;

  SyntheticGesture::Result ForwardInputEvents(
      const base::TimeTicks& timestamp,
      SyntheticGestureTarget* target) override;
  bool AllowHighFrequencyDispatch() const override;

 private:
  enum GestureState { UNINITIALIZED, RUNNING, INVALID, DONE };

  GestureState ForwardTouchOrMouseInputEvents(const base::TimeTicks& timestamp,
                                              SyntheticGestureTarget* target);

  // params_ contains a list of lists of pointer actions, that each list of
  // pointer actions will be dispatched together.
  SyntheticPointerActionListParams params_;
  std::unique_ptr<SyntheticPointerDriver> synthetic_pointer_driver_;
  SyntheticGestureParams::GestureSourceType gesture_source_type_;
  GestureState state_;
  size_t num_actions_dispatched_;

  DISALLOW_COPY_AND_ASSIGN(SyntheticPointerAction);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_POINTER_ACTION_H_
