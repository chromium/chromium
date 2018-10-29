// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_TAP_GESTURE_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_TAP_GESTURE_H_

#include "base/macros.h"
#include "content/browser/renderer_host/input/synthetic_gesture.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target.h"
#include "content/browser/renderer_host/input/synthetic_pointer_driver.h"
#include "content/common/content_export.h"
#include "content/common/input/synthetic_tap_gesture_params.h"

namespace content {

class CONTENT_EXPORT SyntheticTapGesture : public SyntheticGesture {
 public:
  explicit SyntheticTapGesture(const SyntheticTapGestureParams& params);
  ~SyntheticTapGesture() override;

  SyntheticGesture::Result ForwardInputEvents(
      const base::TimeTicks& timestamp,
      SyntheticGestureTarget* target) override;
  bool AllowHighFrequencyDispatch() const override;

 private:
  enum GestureState {
    SETUP,
    PRESS,
    WAITING_TO_RELEASE,
    DONE
  };

  void ForwardTouchOrMouseInputEvents(const base::TimeTicks& timestamp,
                                      SyntheticGestureTarget* target);

  base::TimeDelta GetDuration() const;

  SyntheticTapGestureParams params_;
  std::unique_ptr<SyntheticPointerDriver> synthetic_pointer_driver_;
  base::TimeTicks start_time_;
  SyntheticGestureParams::GestureSourceType gesture_source_type_;
  GestureState state_;

  DISALLOW_COPY_AND_ASSIGN(SyntheticTapGesture);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_TAP_GESTURE_H_
