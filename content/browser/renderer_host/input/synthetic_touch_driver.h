// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_TOUCH_DRIVER_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_TOUCH_DRIVER_H_

#include <array>
#include "content/browser/renderer_host/input/synthetic_pointer_driver.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"

namespace content {

class SyntheticTouchDriver : public SyntheticPointerDriver {
 public:
  SyntheticTouchDriver();
  explicit SyntheticTouchDriver(blink::SyntheticWebTouchEvent touch_event);

  SyntheticTouchDriver(const SyntheticTouchDriver&) = delete;
  SyntheticTouchDriver& operator=(const SyntheticTouchDriver&) = delete;

  ~SyntheticTouchDriver() override;

  void DispatchEvent(SyntheticGestureTarget* target,
                     const base::TimeTicks& timestamp) override;

  void Press(
      float x,
      float y,
      int index,
      SyntheticPointerActionParams::Button button =
          SyntheticPointerActionParams::Button::LEFT,
      int key_modifiers = 0,
      float width = 40.f,
      float height = 40.f,
      float rotation_angle = 0.f,
      float force = 0.5,
      float tangential_pressure = 0.f,
      int tilt_x = 0,
      int tilt_y = 0,
      const base::TimeTicks& timestamp = base::TimeTicks::Now()) override;
  void Move(float x,
            float y,
            int index,
            int key_modifiers = 0,
            float width = 40.f,
            float height = 40.f,
            float rotation_angle = 0.f,
            float force = 0.5,
            float tangential_pressure = 0.f,
            int tilt_x = 0,
            int tilt_y = 0,
            SyntheticPointerActionParams::Button button =
                SyntheticPointerActionParams::Button::NO_BUTTON) override;
  void Release(int index,
               SyntheticPointerActionParams::Button button =
                   SyntheticPointerActionParams::Button::LEFT,
               int key_modifiers = 0) override;
  void Cancel(int index = 0,
              SyntheticPointerActionParams::Button button =
                  SyntheticPointerActionParams::Button::LEFT,
              int key_modifiers = 0) override;
  void Leave(int index = 0) override;

  bool UserInputCheck(
      const SyntheticPointerActionParams& params) const override;

 private:
  using PointerIdIndexMap = std::map<int, int>;

  void ResetPointerIdIndexMap();
  int GetIndexFromMap(int value) const;

  blink::SyntheticWebTouchEvent touch_event_;
  PointerIdIndexMap pointer_id_map_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_TOUCH_DRIVER_H_
