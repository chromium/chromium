// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/synthetic_pointer_action_params.h"

namespace content {

SyntheticPointerActionParams::SyntheticPointerActionParams() = default;

SyntheticPointerActionParams::SyntheticPointerActionParams(
    PointerActionType action_type)
    : pointer_action_type_(action_type),
      button_(action_type == PointerActionType::MOVE ? Button::NO_BUTTON
                                                     : Button::LEFT) {}

SyntheticPointerActionParams::SyntheticPointerActionParams(
    const SyntheticPointerActionParams& other) = default;

SyntheticPointerActionParams::~SyntheticPointerActionParams() = default;

// static
unsigned SyntheticPointerActionParams::GetWebMouseEventModifier(
    SyntheticPointerActionParams::Button button) {
  switch (button) {
    case SyntheticPointerActionParams::Button::LEFT:
      return blink::WebMouseEvent::kLeftButtonDown;
    case SyntheticPointerActionParams::Button::MIDDLE:
      return blink::WebMouseEvent::kMiddleButtonDown;
    case SyntheticPointerActionParams::Button::RIGHT:
      return blink::WebMouseEvent::kRightButtonDown;
    case SyntheticPointerActionParams::Button::BACK:
      return blink::WebMouseEvent::kBackButtonDown;
    case SyntheticPointerActionParams::Button::FORWARD:
      return blink::WebMouseEvent::kForwardButtonDown;
    case SyntheticPointerActionParams::Button::NO_BUTTON:
      return blink::WebMouseEvent::kNoModifiers;
  }
  NOTREACHED_IN_MIGRATION();
  return blink::WebMouseEvent::kNoModifiers;
}

// static
blink::WebMouseEvent::Button
SyntheticPointerActionParams::GetWebMouseEventButton(
    SyntheticPointerActionParams::Button button) {
  switch (button) {
    case SyntheticPointerActionParams::Button::LEFT:
      return blink::WebMouseEvent::Button::kLeft;
    case SyntheticPointerActionParams::Button::MIDDLE:
      return blink::WebMouseEvent::Button::kMiddle;
    case SyntheticPointerActionParams::Button::RIGHT:
      return blink::WebMouseEvent::Button::kRight;
    case SyntheticPointerActionParams::Button::BACK:
      return blink::WebMouseEvent::Button::kBack;
    case SyntheticPointerActionParams::Button::FORWARD:
      return blink::WebMouseEvent::Button::kForward;
    case SyntheticPointerActionParams::Button::NO_BUTTON:
      return blink::WebMouseEvent::Button::kNoButton;
  }
  NOTREACHED_IN_MIGRATION();
  return blink::WebMouseEvent::Button::kNoButton;
}

// static
blink::WebMouseEvent::Button
SyntheticPointerActionParams::GetWebMouseEventButtonFromModifier(
    unsigned modifiers) {
  blink::WebMouseEvent::Button button = blink::WebMouseEvent::Button::kNoButton;
  if (modifiers & blink::WebMouseEvent::kLeftButtonDown)
    button = blink::WebMouseEvent::Button::kLeft;
  if (modifiers & blink::WebMouseEvent::kMiddleButtonDown)
    button = blink::WebMouseEvent::Button::kMiddle;
  if (modifiers & blink::WebMouseEvent::kRightButtonDown)
    button = blink::WebMouseEvent::Button::kRight;
  return button;
}

}  // namespace content
