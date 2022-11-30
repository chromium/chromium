// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Interface for an object that receives keyboard layout events.

#ifndef REMOTING_PROTOCOL_KEYBOARD_LAYOUT_STUB_H_
#define REMOTING_PROTOCOL_KEYBOARD_LAYOUT_STUB_H_

namespace remoting::protocol {

class KeyboardLayout;

// Interface used to inform the client when the host's keyboard layout changes.
class KeyboardLayoutStub {
 public:
  virtual ~KeyboardLayoutStub() = default;
  KeyboardLayoutStub(const KeyboardLayoutStub&) = delete;
  KeyboardLayoutStub& operator=(const KeyboardLayoutStub&) = delete;

  virtual void SetKeyboardLayout(const KeyboardLayout& keyboard_layout) = 0;

 protected:
  KeyboardLayoutStub() = default;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_KEYBOARD_LAYOUT_STUB_H_
