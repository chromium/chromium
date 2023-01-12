// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/x11_keyboard_impl.h"

#include "remoting/host/linux/unicode_to_keysym.h"
#include "ui/gfx/x/future.h"
#include "ui/gfx/x/xkb.h"
#include "ui/gfx/x/xproto.h"
#include "ui/gfx/x/xtest.h"

namespace {

bool FindKeycodeForKeySym(x11::Connection* connection,
                          uint32_t key_sym,
                          uint32_t* keycode,
                          uint32_t* modifiers) {
  auto found_keycode = connection->KeysymToKeycode(key_sym);

  const x11::KeyButMask kModifiersToTry[] = {
      {},
      x11::KeyButMask::Shift,
      x11::KeyButMask::Mod2,
      x11::KeyButMask::Mod3,
      x11::KeyButMask::Mod4,
      x11::KeyButMask::Shift | x11::KeyButMask::Mod2,
      x11::KeyButMask::Shift | x11::KeyButMask::Mod3,
      x11::KeyButMask::Shift | x11::KeyButMask::Mod4,
  };

  // TODO(sergeyu): Is there a better way to find modifiers state?
  for (auto i : kModifiersToTry) {
    auto mods = static_cast<uint32_t>(i);
    if (connection->KeycodeToKeysym(found_keycode, mods) == key_sym) {
      *modifiers = mods;
      *keycode = static_cast<uint8_t>(found_keycode);
      return true;
    }
  }
  return false;
}

// This is ported from XStringToKeysym
// https://gitlab.freedesktop.org/xorg/lib/libx11/-/blob/2b7598221d87049d03e9a95fcb541c37c8728184/src/StrKeysym.c#L147-154
uint32_t UnicodeToKeysym(uint32_t u) {
  if (u > 0x10ffff || u < 0x20 || (u > 0x7e && u < 0xa0)) {
    return 0;
  }
  if (u < 0x100) {
    return u;
  }
  return u | 0x01000000;
}

}  // namespace

namespace remoting {

X11KeyboardImpl::X11KeyboardImpl(x11::Connection* connection)
    : connection_(connection) {}

X11KeyboardImpl::~X11KeyboardImpl() = default;

std::vector<uint32_t> X11KeyboardImpl::GetUnusedKeycodes() {
  std::vector<uint32_t> unused_keycodes_;
  uint8_t min_keycode = static_cast<uint8_t>(connection_->setup().min_keycode);
  uint8_t max_keycode = static_cast<uint8_t>(connection_->setup().max_keycode);
  uint8_t keycode_count = max_keycode - min_keycode + 1;

  auto req = connection_->GetKeyboardMapping(
      {connection_->setup().min_keycode, keycode_count});
  if (auto reply = req.Sync()) {
    for (int keycode = max_keycode; keycode >= min_keycode; keycode--) {
      bool used = false;
      int offset = (keycode - min_keycode) * reply->keysyms_per_keycode;
      for (int level = 0; level < reply->keysyms_per_keycode; level++) {
        if (reply->keysyms[offset + level] != x11::KeySym{}) {
          used = true;
          break;
        }
      }
      if (!used) {
        unused_keycodes_.push_back(keycode);
      }
    }
  }
  return unused_keycodes_;
}

void X11KeyboardImpl::PressKey(uint32_t keycode, uint32_t modifiers) {
  connection_->xkb().LatchLockState(
      {static_cast<x11::Xkb::DeviceSpec>(x11::Xkb::Id::UseCoreKbd),
       static_cast<x11::ModMask>(modifiers),
       static_cast<x11::ModMask>(modifiers)});

  connection_->xtest().FakeInput(
      {x11::KeyEvent::Press, static_cast<uint8_t>(keycode)});
  connection_->xtest().FakeInput(
      {x11::KeyEvent::Release, static_cast<uint8_t>(keycode)});

  connection_->xkb().LatchLockState(
      {static_cast<x11::Xkb::DeviceSpec>(x11::Xkb::Id::UseCoreKbd),
       static_cast<x11::ModMask>(modifiers), x11::ModMask{}});
}

bool X11KeyboardImpl::FindKeycode(uint32_t code_point,
                                  uint32_t* keycode,
                                  uint32_t* modifiers) {
  for (uint32_t keysym : GetKeySymsForUnicode(code_point)) {
    if (FindKeycodeForKeySym(connection_, keysym, keycode, modifiers)) {
      return true;
    }
  }
  return false;
}

bool X11KeyboardImpl::ChangeKeyMapping(uint32_t keycode, uint32_t code_point) {
  if (!code_point) {
    return false;
  }
  auto keysym = UnicodeToKeysym(code_point);
  if (!keysym) {
    return false;
  }
  connection_->ChangeKeyboardMapping({
      .keycode_count = 1,
      .first_keycode = static_cast<x11::KeyCode>(keycode),
      .keysyms_per_keycode = 2,
      .keysyms = {static_cast<x11::KeySym>(keysym) /* lower-case */,
                  static_cast<x11::KeySym>(keysym) /* upper-case */},
  });
  return true;
}

void X11KeyboardImpl::Flush() {
  connection_->Flush();
}

void X11KeyboardImpl::Sync() {
  connection_->Sync();
}

}  // namespace remoting
