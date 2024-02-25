// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_SYNTHETIC_POINTER_DRIVER_H_
#define CONTENT_COMMON_INPUT_SYNTHETIC_POINTER_DRIVER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/common/input/synthetic_gesture_params.h"
#include "content/common/input/synthetic_pointer_action_params.h"

namespace content {

class SyntheticGestureTarget;

class CONTENT_EXPORT SyntheticPointerDriver {
 public:
  SyntheticPointerDriver();

  SyntheticPointerDriver(const SyntheticPointerDriver&) = delete;
  SyntheticPointerDriver& operator=(const SyntheticPointerDriver&) = delete;

  virtual ~SyntheticPointerDriver();

  static std::unique_ptr<SyntheticPointerDriver> Create(
      content::mojom::GestureSourceType gesture_source_type);
  static std::unique_ptr<SyntheticPointerDriver> Create(
      content::mojom::GestureSourceType gesture_source_type,
      bool from_devtools_debugger);

  virtual void DispatchEvent(SyntheticGestureTarget* target,
                             const base::TimeTicks& timestamp) = 0;

  virtual void Press(
      float x,
      float y,
      int index = 0,
      SyntheticPointerActionParams::Button button =
          SyntheticPointerActionParams::Button::LEFT,
      int key_modifiers = 0,
      float width = 1.f,
      float height = 1.f,
      float rotation_angle = 0.f,
      float force = 1.f,
      float tangential_pressure = 0.f,
      int tilt_x = 0,
      int tilt_y = 0,
      const base::TimeTicks& timestamp = base::TimeTicks::Now()) = 0;
  virtual void Move(float x,
                    float y,
                    int index = 0,
                    int key_modifiers = 0,
                    float width = 1.f,
                    float height = 1.f,
                    float rotation_angle = 0.f,
                    float force = 1.f,
                    float tangential_pressure = 0.f,
                    int tilt_x = 0,
                    int tilt_y = 0,
                    SyntheticPointerActionParams::Button button =
                        SyntheticPointerActionParams::Button::NO_BUTTON) = 0;
  virtual void Release(int index = 0,
                       SyntheticPointerActionParams::Button button =
                           SyntheticPointerActionParams::Button::LEFT,
                       int key_modifiers = 0) = 0;
  virtual void Cancel(int index = 0,
                      SyntheticPointerActionParams::Button button =
                          SyntheticPointerActionParams::Button::LEFT,
                      int key_modifiers = 0) = 0;
  virtual void Leave(int index = 0) = 0;

  // Check if the user inputs in the SyntheticPointerActionParams can generate
  // a valid sequence of pointer actions.
  virtual bool UserInputCheck(
      const SyntheticPointerActionParams& params) const = 0;

  virtual base::WeakPtr<SyntheticPointerDriver> AsWeakPtr() = 0;

 protected:
  bool from_devtools_debugger_ = false;
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_SYNTHETIC_POINTER_DRIVER_H_
