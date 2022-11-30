// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_X11_KEYBOARD_H_
#define REMOTING_HOST_LINUX_X11_KEYBOARD_H_

#include <stdint.h>

#include <vector>

namespace remoting {

// An interface for accessing the keyboard and changing the keyboard layout.
// An implementation is allowed to delay processing a request until Flush() or
// Sync() is called.
class X11Keyboard {
 public:
  virtual ~X11Keyboard() {}

  // Returns a vector of key codes that are not being mapped to a code point.
  virtual std::vector<uint32_t> GetUnusedKeycodes() = 0;

  // Simulates a key press with the given |keycode| and |modifiers|. Note that
  // the application receiving the key press can decide whether or how the key
  // press is interpreted into a character.
  virtual void PressKey(uint32_t keycode, uint32_t modifiers) = 0;

  // Finds a keycode and set of modifiers that generate character with the
  // specified |code_point|. Returns true if the key code is successfully found.
  // If the keycode is not found, |keycode| and |modifiers| will not be
  // affected.
  virtual bool FindKeycode(uint32_t code_point,
                           uint32_t* keycode,
                           uint32_t* modifiers) = 0;

  // Change the key mapping such that pressing |keycode| will output the
  // character of |code_point|.
  virtual bool ChangeKeyMapping(uint32_t keycode, uint32_t code_point) = 0;

  // Flushes all requests but don't wait for processing the requests.
  virtual void Flush() = 0;

  // Flushes all requests and wait until all requests are processed.
  virtual void Sync() = 0;

 protected:
  X11Keyboard() {}
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_X11_KEYBOARD_H_
