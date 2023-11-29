// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_SYNTHETIC_POINTER_ACTION_H_
#define CONTENT_COMMON_INPUT_SYNTHETIC_POINTER_ACTION_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/common/input/synthetic_gesture.h"
#include "content/common/input/synthetic_gesture_target.h"
#include "content/common/input/synthetic_pointer_action_list_params.h"
#include "content/common/input/synthetic_pointer_action_params.h"
#include "content/common/input/synthetic_pointer_driver.h"

namespace content {

// It generates and dispatches the synthetic events of touch, mouse and pen
// inputs. The synthetic events are dispatched to each platform in browser and
// sent to renderer to manipulate the DOM elements on the web pages.
class CONTENT_EXPORT SyntheticPointerAction
    : public SyntheticGestureBase<SyntheticPointerActionListParams> {
 public:
  // SyntheticPointerActionGestureParams contains a list of lists of pointer
  // actions, that each list of pointer actions will be dispatched together.
  explicit SyntheticPointerAction(
      const SyntheticPointerActionListParams& params);

  SyntheticPointerAction(const SyntheticPointerAction&) = delete;
  SyntheticPointerAction& operator=(const SyntheticPointerAction&) = delete;

  ~SyntheticPointerAction() override;

  SyntheticGesture::Result ForwardInputEvents(
      const base::TimeTicks& timestamp,
      SyntheticGestureTarget* target) override;
  bool AllowHighFrequencyDispatch() const override;
  void WaitForTargetAck(base::OnceClosure callback,
                        SyntheticGestureTarget* target) const override;

  void SetSyntheticPointerDriver(
      base::WeakPtr<SyntheticPointerDriver> synthetic_pointer_driver) {
    DCHECK(!internal_synthetic_pointer_driver_);
    DCHECK(!external_synthetic_pointer_driver_);
    external_synthetic_pointer_driver_ = synthetic_pointer_driver;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(SyntheticPointerActionTest,
                           UsesCorrectPointerDriver);

  enum class GestureState { UNINITIALIZED, RUNNING, INVALID, DONE };

  GestureState ForwardTouchOrMouseInputEvents(const base::TimeTicks& timestamp,
                                              SyntheticGestureTarget* target);

  SyntheticPointerDriver* PointerDriver() const;

  // An internal pointer driver createad and used only when an
  // external_synthetic_pointer_driver_ isn't provided before the gesture is
  // initialized. Use PointerDriver() rather than accessing this pointer
  // directly.
  std::unique_ptr<SyntheticPointerDriver> internal_synthetic_pointer_driver_;

  // A pointer driver that's owned externally (by the DevTools InputHandler
  // class). Allows the InputHandler to keep track of state from previous
  // synthetic events when a sequence of actions are dispatched one by one. Use
  // PointerDriver() rather than accessing this pointer directly.
  base::WeakPtr<SyntheticPointerDriver> external_synthetic_pointer_driver_;

  content::mojom::GestureSourceType gesture_source_type_ =
      content::mojom::GestureSourceType::kDefaultInput;
  GestureState state_ = GestureState::UNINITIALIZED;
  size_t num_actions_dispatched_ = 0U;
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_SYNTHETIC_POINTER_ACTION_H_
