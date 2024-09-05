// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_TEST_MOUSE_EVENT_BUILDER_H_
#define PDF_TEST_MOUSE_EVENT_BUILDER_H_

#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_pointer_properties.h"
#include "ui/gfx/geometry/point_f.h"

namespace chrome_pdf {

class MouseEventBuilder {
 public:
  MouseEventBuilder();
  MouseEventBuilder(const MouseEventBuilder&) = delete;
  MouseEventBuilder& operator=(const MouseEventBuilder&) = delete;
  ~MouseEventBuilder();

  MouseEventBuilder& SetType(blink::WebInputEvent::Type type) {
    type_ = type;
    return *this;
  }

  MouseEventBuilder& SetPosition(const gfx::PointF& position) {
    position_ = position;
    return *this;
  }

  MouseEventBuilder& SetButton(blink::WebPointerProperties::Button button) {
    button_ = button;
    return *this;
  }

  MouseEventBuilder& SetClickCount(int click_count) {
    click_count_ = click_count;
    return *this;
  }

  MouseEventBuilder& SetModifiers(int modifiers) {
    modifiers_ = modifiers;
    return *this;
  }

  // Commonly used - provided for convenience.
  MouseEventBuilder& CreateLeftClickAtPosition(const gfx::PointF& position) {
    SetType(blink::WebInputEvent::Type::kMouseDown);
    SetPosition(position);
    SetButton(blink::WebPointerProperties::Button::kLeft);
    return SetClickCount(1);
  }
  MouseEventBuilder& CreateLeftMouseUpAtPosition(const gfx::PointF& position) {
    SetType(blink::WebInputEvent::Type::kMouseUp);
    SetPosition(position);
    SetButton(blink::WebPointerProperties::Button::kLeft);
    return SetClickCount(1);
  }

  blink::WebMouseEvent Build() const;

 private:
  blink::WebInputEvent::Type type_ = blink::WebInputEvent::Type::kUndefined;
  gfx::PointF position_;
  blink::WebPointerProperties::Button button_ =
      blink::WebPointerProperties::Button::kNoButton;
  int click_count_ = 0;
  int modifiers_ = blink::WebInputEvent::Modifiers::kNoModifiers;
};

}  // namespace chrome_pdf

#endif  // PDF_TEST_MOUSE_EVENT_BUILDER_H_
