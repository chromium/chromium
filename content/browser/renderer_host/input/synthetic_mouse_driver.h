// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_MOUSE_DRIVER_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_MOUSE_DRIVER_H_

#include "content/browser/renderer_host/input/synthetic_pointer_driver.h"

namespace content {

class SyntheticMouseDriver : public SyntheticPointerDriver {
 public:
  SyntheticMouseDriver();

  SyntheticMouseDriver(const SyntheticMouseDriver&) = delete;
  SyntheticMouseDriver& operator=(const SyntheticMouseDriver&) = delete;

  ~SyntheticMouseDriver() override;

  void DispatchEvent(SyntheticGestureTarget* target,
                     const base::TimeTicks& timestamp) override;

  void Press(
      float x,
      float y,
      int index = 0,
      SyntheticPointerActionParams::Button button =
          SyntheticPointerActionParams::Button::LEFT,
      int key_modifiers = 0,
      float width = 1.f,
      float height = 1.f,
      float rotation_angle = 0.f,
      float force = 0.5,
      float tangential_pressure = 0.f,
      int tilt_x = 0,
      int tilt_y = 0,
      const base::TimeTicks& timestamp = base::TimeTicks::Now()) override;
  void Move(float x,
            float y,
            int index = 0,
            int key_modifiers = 0,
            float width = 1.f,
            float height = 1.f,
            float rotation_angle = 0.f,
            float force = 0.5,
            float tangential_pressure = 0.f,
            int tilt_x = 0,
            int tilt_y = 0,
            SyntheticPointerActionParams::Button button =
                SyntheticPointerActionParams::Button::NO_BUTTON) override;
  void Release(int index = 0,
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

 protected:
  blink::WebMouseEvent mouse_event_;
  unsigned last_modifiers_ = 0;

 private:
  int ComputeClickCount(const base::TimeTicks& timestamp,
                        blink::WebMouseEvent::Button pressed_button,
                        float x,
                        float y);
  int click_count_ = 0;
  base::TimeTicks last_mouse_click_time_ = base::TimeTicks::Now();
  float last_x_ = 0;
  float last_y_ = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_MOUSE_DRIVER_H_
