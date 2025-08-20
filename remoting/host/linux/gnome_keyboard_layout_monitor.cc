// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/gnome_keyboard_layout_monitor.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "remoting/host/linux/keyboard_layout_monitor_utils.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/scoped_xkb.h"

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

struct Context {
  Context(protocol::KeyboardLayout& proto, struct xkb_keymap* keymap)
      : proto(proto), xkb_state(xkb_state_new(keymap)) {
    auto numlock_index = xkb_keymap_mod_get_index(keymap, "NumLock");
    auto numlock_mask = (numlock_index == XKB_MOD_INVALID) ? 0 : 1 << numlock_index;
    auto shift_index = xkb_keymap_mod_get_index(keymap, "Shift");
    auto shift_mask = (shift_index == XKB_MOD_INVALID) ? 0 : 1 << shift_index;
    auto altgr_index = xkb_keymap_mod_get_index(keymap, "Mod5");
    auto altgr_mask = (altgr_index == XKB_MOD_INVALID) ? 0 : 1 << altgr_index;
    shift_level_to_mask[0] = numlock_mask;
    shift_level_to_mask[1] = shift_mask | numlock_mask;
    if (altgr_mask) {
      shift_level_to_mask[2] = altgr_mask | numlock_mask;
      shift_level_to_mask[3] = shift_mask | altgr_mask | numlock_mask;
    }
  }

  raw_ref<protocol::KeyboardLayout> proto;
  std::map<int, int> shift_level_to_mask;
  std::unique_ptr<xkb_state, ui::XkbStateDeleter> xkb_state;
};

void ProcessKeyCode(struct xkb_keymap* keymap,
                    xkb_keycode_t keycode,
                    void* data) {
  auto* ctx = static_cast<Context*>(data);
  for (auto [level, mask] : ctx->shift_level_to_mask) {
    xkb_state_update_mask(ctx->xkb_state.get(), mask, 0, 0, 0, 0, 0);
    auto keysym =
        xkb_state_key_get_one_sym(ctx->xkb_state.get(), keycode);
    if (keysym == XKB_KEY_NoSymbol) {
      // Keys with no symbols are fairly common, and should be ignored. In
      // theory, keys with multiple symbols might exist, and could perhaps
      // be supported (for example, if all symbols correspond to characters
      // then they could be concatenated), but it seems to be very uncommon
      // so they are ignored for now.
      continue;
    }
    if (keysym == XKB_KEY_Num_Lock || keysym == XKB_KEY_Caps_Lock) {
      // Don't include Num Lock or Caps Lock because the client keyboard
      // doesn't support them.
      continue;
    }
    auto usb_keycode = ui::KeycodeConverter::NativeKeycodeToUsbKeycode(keycode);
    auto& actions = *(*ctx->proto->mutable_keys())[usb_keycode].mutable_actions();
    bool had_existing_actions = !actions.empty();
    if (!SetActionFromLayoutFunction(keysym, actions[level]) &&
        !SetActionFromUtfCodepoint(keysym, actions[level]) &&
        !SetActionFromDeadKey(keysym, actions[level]) &&
        !had_existing_actions) {
      // Delete the entry that was implicitly added by the []-operator if
      // there are no actions.
      ctx->proto->mutable_keys()->erase(usb_keycode);
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
    Context ctx(layout_proto_, keymap);
    xkb_keymap_key_for_each(keymap, &ProcessKeyCode, &ctx);
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
