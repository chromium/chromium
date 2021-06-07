// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_INPUT_EVENT_TRACKER_H_
#define REMOTING_PROTOCOL_INPUT_EVENT_TRACKER_H_

#include <stdint.h>

#include <set>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "remoting/protocol/input_stub.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace remoting {
namespace protocol {

// Filtering InputStub which tracks mouse and keyboard input events before
// passing them on to |input_stub|, and can dispatch release events to
// |input_stub| for all currently-pressed keys and buttons when necessary.
class InputEventTracker : public InputStub {
 public:
  InputEventTracker();
  explicit InputEventTracker(InputStub* input_stub);
  ~InputEventTracker() override;

  void set_input_stub(InputStub* input_stub) {
    input_stub_ = input_stub;
  }

  // Returns true if the key with the specified USB code is currently pressed.
  bool IsKeyPressed(ui::DomCode usb_keycode) const;

  // Returns the count of keys currently pressed.
  int PressedKeyCount() const;

  // Dispatch release events for all currently-pressed keys, mouse buttons, and
  // touch points to the InputStub.
  void ReleaseAll();

  // Similar to ReleaseAll, but conditional on a modifier key tracked by this
  // class being pressed without the corresponding parameter indicating that it
  // should be.
  void ReleaseAllIfModifiersStuck(bool alt_expected, bool ctrl_expected,
                                  bool os_expected, bool shift_expected);

  // InputStub interface.
  void InjectKeyEvent(const KeyEvent& event) override;
  void InjectTextEvent(const TextEvent& event) override;
  void InjectMouseEvent(const MouseEvent& event) override;
  void InjectTouchEvent(const TouchEvent& event) override;

 private:
  InputStub* input_stub_ = nullptr;

  std::set<ui::DomCode> pressed_keys_;

  webrtc::DesktopVector mouse_pos_;
  uint32_t mouse_button_state_ = 0;

  std::set<uint32_t> touch_point_ids_;

  DISALLOW_COPY_AND_ASSIGN(InputEventTracker);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_INPUT_EVENT_TRACKER_H_
