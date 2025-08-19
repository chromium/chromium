// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/gnome_keyboard_layout_monitor.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "remoting/host/linux/keyboard_layout_monitor_utils.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace remoting {

namespace {

bool SetActionFromLayoutFunction(xkb_keysym_t keysym,
                                 protocol::KeyboardLayout::KeyAction& action) {
  auto function = KeyvalToFunction(keysym);
  if (function == protocol::LayoutKeyFunction::UNKNOWN) {
    return false;
  }
  action.set_function(function);
  return true;
}

bool SetActionFromUtfCodepoint(xkb_keysym_t keysym,
                               protocol::KeyboardLayout::KeyAction& action) {
  auto codepoint = xkb_keysym_to_utf32(keysym);
  if (codepoint == 0) {
    return false;
  }
  std::string utf8;
  base::WriteUnicodeCharacter(codepoint, &utf8);
  action.set_character(utf8);
  return true;
}

bool SetActionFromDeadKey(xkb_keysym_t keysym,
                          protocol::KeyboardLayout::KeyAction& action) {
  const char* utf8 = DeadKeyToUtf8String(keysym);
  if (!utf8) {
    return false;
  }
  action.set_character(utf8);
  return true;
}

void ProcessKeyCode(struct xkb_keymap* keymap,
                    xkb_keycode_t keycode,
                    void* data) {
  auto* proto = static_cast<protocol::KeyboardLayout*>(data);
  int layout = 0;
  xkb_level_index_t num_levels =
      xkb_keymap_num_levels_for_key(keymap, keycode, layout);
  for (xkb_level_index_t level = 0; level < num_levels; level++) {
    const xkb_keysym_t* syms;
    auto num_syms =
        xkb_keymap_key_get_syms_by_level(keymap, keycode, layout, level, &syms);
    if (num_syms != 1) {
      // Keys with no symbols are fairly common, and should be ignored. In
      // theory, keys with multiple symbols might exist, and could perhaps
      // be supported (for example, if all symbols correspond to characters
      // then they could be concatenated), but it seems to be very uncommon
      // so they are ignored for now.
      continue;
    }
    if (syms[0] == XKB_KEY_Num_Lock || syms[0] == XKB_KEY_Caps_Lock) {
      // Don't include Num Lock or Caps Lock because the client keyboard
      // doesn't support them.
      continue;
    }
    auto usb_keycode = ui::KeycodeConverter::NativeKeycodeToUsbKeycode(keycode);
    auto& actions = *(*proto->mutable_keys())[usb_keycode].mutable_actions();
    bool had_existing_actions = !actions.empty();
    if (!SetActionFromLayoutFunction(syms[0], actions[level]) &&
        !SetActionFromUtfCodepoint(syms[0], actions[level]) &&
        !SetActionFromDeadKey(syms[0], actions[level]) &&
        !had_existing_actions) {
      // Delete the entry that was implicitly added by the []-operator if
      // there are no actions.
      proto->mutable_keys()->erase(usb_keycode);
    }
  }
}

}  // namespace

GnomeKeyboardLayoutMonitor::GnomeKeyboardLayoutMonitor(
    base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback)
    : callback_(callback) {}

GnomeKeyboardLayoutMonitor::~GnomeKeyboardLayoutMonitor() = default;

void GnomeKeyboardLayoutMonitor::Start() {
  started_ = true;
  callback_.Run(layout_proto_);
}

void GnomeKeyboardLayoutMonitor::OnKeymapChanged(xkb_keymap* keymap) {
  layout_proto_ = protocol::KeyboardLayout();
  if (keymap) {
    // Based on experimentation, there will typically be multiple keymaps: one
    // for each installed layout (in the order they are listed in the Keyboard
    // configuration applet), and a default one for US English at the end. The
    // default is present even if the only installed layout is US English. The
    // layout changes when the configured order is changed, but *not* when the
    // selected layout changes. Hence, if there are more than two layouts (ie,
    // more then one, plus the default), then it's impossible to know which is
    // active. Since the layout can be configured per-window it's probably not
    // possible to detect this solely using libei + XKB. Log a warning in this
    // situation.
    auto num_layouts = xkb_keymap_num_layouts(keymap);
    if (num_layouts > 2) {
      LOG(WARNING) << "Keyboard has " << num_layouts << " layouts. Using "
                   << xkb_keymap_layout_get_name(keymap, 0)
                   << " for the client keyboard. Re-order the layouts to"
                   << " change this.";
    }
    xkb_keymap_key_for_each(keymap, &ProcessKeyCode, &layout_proto_);
    if (started_) {
      callback_.Run(layout_proto_);
    }
  }
}

base::WeakPtr<GnomeKeyboardLayoutMonitor>
GnomeKeyboardLayoutMonitor::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace remoting
