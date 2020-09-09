// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/x11_keyboard_impl.h"

#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "remoting/host/linux/unicode_to_keysym.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/gfx/x/xkb.h"
#include "ui/gfx/x/xtest.h"

namespace {

bool FindKeycodeForKeySym(Display* display,
                          KeySym key_sym,
                          uint32_t* keycode,
                          uint32_t* modifiers) {
  uint32_t found_keycode = XKeysymToKeycode(display, key_sym);

  const uint32_t kModifiersToTry[] = {
      0,
      ShiftMask,
      Mod2Mask,
      Mod3Mask,
      Mod4Mask,
      ShiftMask | Mod2Mask,
      ShiftMask | Mod3Mask,
      ShiftMask | Mod4Mask,
  };

  // TODO(sergeyu): Is there a better way to find modifiers state?
  for (auto i : kModifiersToTry) {
    unsigned long key_sym_with_mods;
    if (XkbLookupKeySym(display, found_keycode, i, nullptr,
                        &key_sym_with_mods) &&
        key_sym_with_mods == key_sym) {
      *modifiers = i;
      *keycode = found_keycode;
      return true;
    }
  }
  return false;
}

}  // namespace

namespace remoting {

X11KeyboardImpl::X11KeyboardImpl(x11::Connection* connection)
    : connection_(connection), display_(connection->display()) {}

X11KeyboardImpl::~X11KeyboardImpl() = default;

std::vector<uint32_t> X11KeyboardImpl::GetUnusedKeycodes() {
  std::vector<uint32_t> unused_keycodes_;
  int min_keycode;
  int max_keycode;
  XDisplayKeycodes(display_, &min_keycode, &max_keycode);
  uint32_t keycode_count = max_keycode - min_keycode + 1;

  int sym_per_key;
  gfx::XScopedPtr<KeySym> mapping(
      XGetKeyboardMapping(display_, min_keycode, keycode_count, &sym_per_key));
  for (int keycode = max_keycode; keycode >= min_keycode; keycode--) {
    bool used = false;
    int offset = (keycode - min_keycode) * sym_per_key;
    for (int level = 0; level < sym_per_key; level++) {
      if (mapping.get()[offset + level]) {
        used = true;
        break;
      }
    }
    if (!used) {
      unused_keycodes_.push_back(keycode);
    }
  }
  return unused_keycodes_;
}

void X11KeyboardImpl::PressKey(uint32_t keycode, uint32_t modifiers) {
  XkbLockModifiers(display_, static_cast<unsigned>(x11::Xkb::Id::UseCoreKbd),
                   modifiers, modifiers);

  connection_->xtest().FakeInput({x11::KeyEvent::Press, keycode});
  connection_->xtest().FakeInput({x11::KeyEvent::Release, keycode});

  XkbLockModifiers(display_, static_cast<unsigned>(x11::Xkb::Id::UseCoreKbd),
                   modifiers, 0);
}

bool X11KeyboardImpl::FindKeycode(uint32_t code_point,
                                  uint32_t* keycode,
                                  uint32_t* modifiers) {
  for (uint32_t keysym : GetKeySymsForUnicode(code_point)) {
    if (FindKeycodeForKeySym(display_, keysym, keycode, modifiers)) {
      return true;
    }
  }
  return false;
}

bool X11KeyboardImpl::ChangeKeyMapping(uint32_t keycode, uint32_t code_point) {
  KeySym sym = NoSymbol;
  if (code_point > 0) {
    std::string sym_hex = base::StringPrintf("U%x", code_point);
    sym = XStringToKeysym(sym_hex.c_str());
    if (sym == NoSymbol) {
      // The server may not support Unicode-to-KeySym translation.
      return false;
    }
  }

  KeySym syms[2]{sym, sym};  // {lower-case, upper-case}
  XChangeKeyboardMapping(display_, keycode, 2, syms, 1);
  return true;
}

void X11KeyboardImpl::Flush() {
  XFlush(display_);
}

void X11KeyboardImpl::Sync() {
  XSync(display_, false);
}

}  // namespace remoting
