// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_X11_KEYBOARD_IMPL_H_
#define REMOTING_HOST_LINUX_X11_KEYBOARD_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "remoting/host/linux/x11_keyboard.h"

#include "ui/gfx/x/connection.h"

namespace remoting {

class X11KeyboardImpl : public X11Keyboard {
 public:
  explicit X11KeyboardImpl(x11::Connection* connection);

  X11KeyboardImpl(const X11KeyboardImpl&) = delete;
  X11KeyboardImpl& operator=(const X11KeyboardImpl&) = delete;

  ~X11KeyboardImpl() override;

  // KeyboardInterface overrides.
  std::vector<uint32_t> GetUnusedKeycodes() override;

  void PressKey(uint32_t keycode, uint32_t modifiers) override;

  bool FindKeycode(uint32_t code_point,
                   uint32_t* keycode,
                   uint32_t* modifiers) override;

  bool ChangeKeyMapping(uint32_t keycode, uint32_t code_point) override;

  void Flush() override;

  void Sync() override;

 private:
  // X11 graphics context.
  raw_ptr<x11::Connection> connection_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_X11_KEYBOARD_IMPL_H_
