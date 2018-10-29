// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/input/normalizing_input_filter_mac.h"

#include <map>
#include <vector>

#include "base/logging.h"
#include "remoting/proto/event.pb.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace remoting {

NormalizingInputFilterMac::NormalizingInputFilterMac(
    protocol::InputStub* input_stub)
    : protocol::InputFilter(input_stub) {}

NormalizingInputFilterMac::~NormalizingInputFilterMac() = default;

void NormalizingInputFilterMac::InjectKeyEvent(
    const protocol::KeyEvent& event_arg) {
  DCHECK(event_arg.has_usb_keycode());

  // Mac OS X doesn't have a concept of num lock, so unset the field.
  protocol::KeyEvent event(event_arg);
  event.clear_num_lock_state();

  ui::DomCode dom_code = static_cast<ui::DomCode>(event.usb_keycode());

  bool is_special_key = dom_code == ui::DomCode::CONTROL_LEFT ||
                        dom_code == ui::DomCode::SHIFT_LEFT ||
                        dom_code == ui::DomCode::ALT_LEFT ||
                        dom_code == ui::DomCode::CONTROL_RIGHT ||
                        dom_code == ui::DomCode::SHIFT_RIGHT ||
                        dom_code == ui::DomCode::ALT_RIGHT ||
                        dom_code == ui::DomCode::TAB;

  bool is_cmd_key =
      dom_code == ui::DomCode::META_LEFT || dom_code == ui::DomCode::META_RIGHT;

  if (dom_code == ui::DomCode::CAPS_LOCK) {
    // Mac OS X generates keydown/keyup on lock-state transitions, rather than
    // when the key is pressed & released, so fake keydown/keyup on each event.
    protocol::KeyEvent newEvent(event);

    newEvent.set_pressed(true);
    InputFilter::InjectKeyEvent(newEvent);
    newEvent.set_pressed(false);
    InputFilter::InjectKeyEvent(newEvent);

    return;
  } else if (!is_cmd_key && !is_special_key) {
    // Track keydown/keyup events for non-modifiers, so we can release them if
    // necessary (see below).
    if (event.pressed()) {
      key_pressed_map_[event.usb_keycode()] = event;
    } else {
      key_pressed_map_.erase(event.usb_keycode());
    }
  }

  if (is_cmd_key && !event.pressed()) {
    // Mac OS X will not generate release events for keys pressed while Cmd is
    // pressed, so release all pressed keys when Cmd is released.
    GenerateKeyupEvents();
  }

  InputFilter::InjectKeyEvent(event);
}

void NormalizingInputFilterMac::GenerateKeyupEvents() {
  for (auto i = key_pressed_map_.begin(); i != key_pressed_map_.end(); ++i) {
    // The generated key up event will have the same key code and lock states
    // as the original key down event.
    protocol::KeyEvent event = i->second;
    event.set_pressed(false);
    InputFilter::InjectKeyEvent(event);
  }

  // Clearing the map now that we have released all the pressed keys.
  key_pressed_map_.clear();
}

}  // namespace remoting
