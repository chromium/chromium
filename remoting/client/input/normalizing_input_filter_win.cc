// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/input/normalizing_input_filter_win.h"

#include <stdint.h>

#include "base/check_op.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace remoting {

static bool IsLeftControl(uint32_t code) {
  return code == static_cast<uint32_t>(ui::DomCode::CONTROL_LEFT);
}

static bool IsRightAlt(uint32_t code) {
  return code == static_cast<uint32_t>(ui::DomCode::ALT_RIGHT);
}

NormalizingInputFilterWin::NormalizingInputFilterWin(
    protocol::InputStub* input_stub)
    : protocol::InputFilter(input_stub) {}

NormalizingInputFilterWin::~NormalizingInputFilterWin() = default;

void NormalizingInputFilterWin::InjectKeyEvent(
    const protocol::KeyEvent& event) {
  DCHECK(event.has_usb_keycode());
  DCHECK(event.has_pressed());

  if (event.pressed()) {
    ProcessKeyDown(event);
  } else {
    ProcessKeyUp(event);
  }
}

void NormalizingInputFilterWin::InjectMouseEvent(
    const protocol::MouseEvent& event) {
  FlushDeferredKeydownEvent();
  InputFilter::InjectMouseEvent(event);
}

void NormalizingInputFilterWin::ProcessKeyDown(
    const protocol::KeyEvent& event) {
  if (IsRightAlt(event.usb_keycode())) {
    // If there's a deferred LeftControl event then treat it as part of an
    // AltGr press, so swallow it.
    if (deferred_control_keydown_.has_usb_keycode()) {
      altgr_is_pressed_ = true;
      deferred_control_keydown_ = protocol::KeyEvent();
    }
  }

  // If a keydown for something other than RightAlt is received while there
  // is a deferred LeftControl event then treat it as an actual LeftControl
  // event, and flush it.
  FlushDeferredKeydownEvent();

  if (IsLeftControl(event.usb_keycode())) {
    // If AltGr is pressed then ignore the LeftControl keydown repeat.
    if (altgr_is_pressed_)
      return;

    // If LeftControl is not already pressed then defer the event.
    if (!left_control_is_pressed_) {
      deferred_control_keydown_ = event;
      return;
    }
  }

  InputFilter::InjectKeyEvent(event);
}

void NormalizingInputFilterWin::ProcessKeyUp(const protocol::KeyEvent& event) {
  if (IsLeftControl(event.usb_keycode())) {
    // If we treated the LeftControl as part of AltGr, and never sent the
    // keydown, then ignore the keyup as well.
    if (altgr_is_pressed_) {
      altgr_is_pressed_ = false;
      return;
    }

    // If there is still a deferred event then flush it, so that LeftControl
    // press-and-release works.
    FlushDeferredKeydownEvent();

    left_control_is_pressed_ = false;
  }

  InputFilter::InjectKeyEvent(event);
}

void NormalizingInputFilterWin::FlushDeferredKeydownEvent() {
  if (!deferred_control_keydown_.has_usb_keycode())
    return;

  DCHECK_EQ(static_cast<uint32_t>(ui::DomCode::CONTROL_LEFT),
            deferred_control_keydown_.usb_keycode());

  left_control_is_pressed_ = true;
  InputFilter::InjectKeyEvent(deferred_control_keydown_);
  deferred_control_keydown_ = protocol::KeyEvent();
}

}  // namespace remoting
