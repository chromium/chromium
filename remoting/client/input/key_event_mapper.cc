// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/input/key_event_mapper.h"

#include "base/containers/contains.h"
#include "remoting/proto/event.pb.h"

namespace remoting {

KeyEventMapper::KeyEventMapper() = default;

KeyEventMapper::KeyEventMapper(InputStub* stub) : protocol::InputFilter(stub) {}

KeyEventMapper::~KeyEventMapper() = default;

void KeyEventMapper::SetTrapCallback(KeyTrapCallback callback) {
  trap_callback = callback;
}

void KeyEventMapper::TrapKey(uint32_t usb_keycode, bool trap_key) {
  if (trap_key) {
    trapped_keys.insert(usb_keycode);
  } else {
    trapped_keys.erase(usb_keycode);
  }
}

void KeyEventMapper::RemapKey(uint32_t in_usb_keycode,
                              uint32_t out_usb_keycode) {
  if (in_usb_keycode == out_usb_keycode) {
    mapped_keys.erase(in_usb_keycode);
  } else {
    mapped_keys[in_usb_keycode] = out_usb_keycode;
  }
}

void KeyEventMapper::InjectKeyEvent(const protocol::KeyEvent& event) {
  if (event.has_usb_keycode()) {
    // Deliver trapped keys to the callback, not the next stub.
    if (!trap_callback.is_null() && event.has_pressed() &&
        base::Contains(trapped_keys, event.usb_keycode())) {
      trap_callback.Run(event);
      return;
    }

    // Re-map mapped keys to the new value before passing them on.
    auto mapped = mapped_keys.find(event.usb_keycode());
    if (mapped != mapped_keys.end()) {
      protocol::KeyEvent new_event(event);
      new_event.set_usb_keycode(mapped->second);
      InputFilter::InjectKeyEvent(new_event);
      return;
    }
  }

  InputFilter::InjectKeyEvent(event);
}

}  // namespace remoting
