// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_SYNTHETIC_TAP_GESTURE_H_
#define CONTENT_COMMON_INPUT_SYNTHETIC_TAP_GESTURE_H_

#include <memory>

#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/common/input/synthetic_gesture.h"
#include "content/common/input/synthetic_gesture_target.h"
#include "content/common/input/synthetic_pointer_driver.h"
#include "content/common/input/synthetic_tap_gesture_params.h"

namespace content {

class CONTENT_EXPORT SyntheticTapGesture
    : public SyntheticGestureBase<SyntheticTapGestureParams> {
 public:
  explicit SyntheticTapGesture(const SyntheticTapGestureParams& params);

  SyntheticTapGesture(const SyntheticTapGesture&) = delete;
  SyntheticTapGesture& operator=(const SyntheticTapGesture&) = delete;

  ~SyntheticTapGesture() override;

  SyntheticGesture::Result ForwardInputEvents(
      const base::TimeTicks& timestamp,
      SyntheticGestureTarget* target) override;
  void WaitForTargetAck(base::OnceClosure callback,
                        SyntheticGestureTarget* target) const override;
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

  std::unique_ptr<SyntheticPointerDriver> synthetic_pointer_driver_;
  base::TimeTicks start_time_;
  content::mojom::GestureSourceType gesture_source_type_;
  GestureState state_;
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_SYNTHETIC_TAP_GESTURE_H_
