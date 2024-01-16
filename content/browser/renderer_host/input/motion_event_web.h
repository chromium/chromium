// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_MOTION_EVENT_WEB_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_MOTION_EVENT_WEB_H_

#include <stddef.h>
#include <stdint.h>

#include "content/common/content_export.h"
#include "third_party/blink/public/common/input/web_touch_event.h"
#include "ui/events/velocity_tracker/motion_event.h"

namespace content {

// Implementation of ui::MotionEvent wrapping a WebTouchEvent.
class CONTENT_EXPORT MotionEventWeb : public ui::MotionEvent {
 public:
  explicit MotionEventWeb(const blink::WebTouchEvent& event);

  MotionEventWeb(const MotionEventWeb&) = delete;
  MotionEventWeb& operator=(const MotionEventWeb&) = delete;

  ~MotionEventWeb() override;

  // ui::MotionEvent
  uint32_t GetUniqueEventId() const override;
  Action GetAction() const override;
  int GetActionIndex() const override;
  size_t GetPointerCount() const override;
  int GetPointerId(size_t pointer_index) const override;
  float GetX(size_t pointer_index) const override;
  float GetY(size_t pointer_index) const override;
  float GetRawX(size_t pointer_index) const override;
  float GetRawY(size_t pointer_index) const override;
  float GetTouchMajor(size_t pointer_index) const override;
  float GetTouchMinor(size_t pointer_index) const override;
  float GetOrientation(size_t pointer_index) const override;
  float GetPressure(size_t pointer_index) const override;
  float GetTiltX(size_t pointer_index) const override;
  float GetTiltY(size_t pointer_index) const override;
  float GetTwist(size_t pointer_index) const override;
  float GetTangentialPressure(size_t pointer_index) const override;
  base::TimeTicks GetEventTime() const override;
  ToolType GetToolType(size_t pointer_index) const override;
  int GetButtonState() const override;
  int GetFlags() const override;

 private:
  blink::WebTouchEvent event_;
  Action cached_action_;
  int cached_action_index_;
  const uint32_t unique_event_id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_MOTION_EVENT_WEB_H_
