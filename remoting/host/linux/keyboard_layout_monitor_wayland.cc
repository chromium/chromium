// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/keyboard_layout_monitor_wayland.h"

#include <xkbcommon/xkbcommon.h>

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/host/linux/keyboard_layout_monitor_utils.h"
#include "remoting/host/linux/wayland_manager.h"
#include "remoting/proto/control.pb.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace remoting {

KeyboardLayoutMonitorWayland::KeyboardLayoutMonitorWayland(
    base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback)
    : layout_changed_callback_(std::move(callback)) {}

KeyboardLayoutMonitorWayland::~KeyboardLayoutMonitorWayland() {
  if (xkb_state_) {
    xkb_state_unref(xkb_state_.get());
  }
}

void KeyboardLayoutMonitorWayland::Start() {
  WaylandManager::Get()->SetKeyboardLayoutCallback(
      base::BindRepeating(&KeyboardLayoutMonitorWayland::ProcessKeymaps,
                          weak_factory_.GetWeakPtr()));
  WaylandManager::Get()->AddKeyboardModifiersCallback(base::BindRepeating(
      &KeyboardLayoutMonitorWayland::ProcessModifiersAndNotifyCallbacks,
      weak_factory_.GetWeakPtr()));
}

void KeyboardLayoutMonitorWayland::UpdateState() {
  if (xkb_state_) {
    xkb_state_unref(xkb_state_.get());
  }
  xkb_state_ = xkb_state_new(keymap_.get());
}

void KeyboardLayoutMonitorWayland::ProcessKeymaps(
    std::unique_ptr<struct xkb_keymap, XkbKeyMapDeleter> keymap) {
  keymap_ = std::move(keymap);
  UpdateState();
}

protocol::KeyboardLayout
KeyboardLayoutMonitorWayland::GenerateProtocolLayoutMessage() {
  protocol::KeyboardLayout layout_message;

  bool have_altgr = false;

  for (ui::DomCode key : KeyboardLayoutMonitor::kSupportedKeys) {
    // Skip single-layout IME keys for now, as they are always present in the
    // keyboard map but not present on most keyboards. Client-side IME is likely
    // more convenient, anyway.
    // TODO(rkjnsn): Figure out how to show these keys only when relevant.
    if (key == ui::DomCode::LANG1 || key == ui::DomCode::LANG2 ||
        key == ui::DomCode::CONVERT || key == ui::DomCode::NON_CONVERT ||
        key == ui::DomCode::KANA_MODE) {
      continue;
    }

    std::uint32_t usb_code = ui::KeycodeConverter::DomCodeToUsbKeycode(key);
    int keycode = ui::KeycodeConverter::DomCodeToNativeKeycode(key);
    // Insert entry for USB code. It's fine to overwrite if we somehow process
    // the same USB code twice, since the actions will be the same.
    auto& key_actions =
        *(*layout_message.mutable_keys())[usb_code].mutable_actions();
    for (int shift_level = 0; shift_level < 8; ++shift_level) {
      // Don't bother capturing higher shift levels if there's no configured way
      // to access them.
      if ((shift_level & 2 && !have_altgr) || (shift_level & 4)) {
        continue;
      }
      // Always consider NumLock set and CapsLock unset.
      constexpr uint32_t SHIFT_MODIFIER = 1;
      constexpr uint32_t CAPSLOCK_MODIFIER = 1;
      constexpr uint32_t NUMLOCK_MODIFIER = 16;
      constexpr uint32_t ALTGR_MODIFIER = 128;
      uint32_t mods_locked = NUMLOCK_MODIFIER;
      uint32_t mods_latched = 0;
      if (shift_level & 1) {
        mods_locked |= SHIFT_MODIFIER;
      }
      if (shift_level & 2) {
        mods_locked &= ~CAPSLOCK_MODIFIER;
        mods_latched |= ALTGR_MODIFIER;
      }
      xkb_state_update_mask(xkb_state_.get(), 0, mods_latched, mods_locked, 0,
                            0, current_group_);
      uint32_t keyval = xkb_state_key_get_one_sym(xkb_state_.get(), keycode);

      if (keyval == XKB_KEY_NoSymbol) {
        LOG(WARNING) << "Either no symbol OR multiple symbols found for a key";
        continue;
      }

      uint32_t unicode = xkb_keysym_to_utf32(keyval);
      if (unicode != 0) {
        switch (unicode) {
          case 0x08:
            key_actions[shift_level].set_function(
                protocol::LayoutKeyFunction::BACKSPACE);
            break;
          case 0x09:
            key_actions[shift_level].set_function(
                protocol::LayoutKeyFunction::TAB);
            break;
          case 0x0D:
            key_actions[shift_level].set_function(
                protocol::LayoutKeyFunction::ENTER);
            break;
          case 0x1B:
            key_actions[shift_level].set_function(
                protocol::LayoutKeyFunction::ESCAPE);
            break;
          case 0x7F:
            key_actions[shift_level].set_function(
                protocol::LayoutKeyFunction::DELETE_);
            break;
          default:
            std::string utf8;
            base::WriteUnicodeCharacter(unicode, &utf8);
            key_actions[shift_level].set_character(utf8);
        }
        continue;
      }

      const char* dead_key_utf8 = DeadKeyToUtf8String(keyval);
      if (dead_key_utf8) {
        key_actions[shift_level].set_character(dead_key_utf8);
        continue;
      }
      if (keyval == XKB_KEY_Caps_Lock || keyval == XKB_KEY_Num_Lock) {
        // Don't include Num Lock or Caps Lock until we decide if / how we want
        // to handle them.
        // TODO(rkjnsn): Determine if supporting Num Lock / Caps Lock provides
        // enough utility to warrant support by the soft keyboard.
        continue;
      }

      protocol::LayoutKeyFunction function = KeyvalToFunction(keyval);
      if (function == protocol::LayoutKeyFunction::ALT_GR) {
        have_altgr = true;
      }
      key_actions[shift_level].set_function(function);
    }

    if (key_actions.empty()) {
      layout_message.mutable_keys()->erase(usb_code);
    }
  }

  return layout_message;
}

void KeyboardLayoutMonitorWayland::ProcessModifiersAndNotifyCallbacks(
    uint32_t group) {
  if (current_group_ != XKB_LAYOUT_INVALID &&
      group == static_cast<uint32_t>(current_group_)) {
    return;
  }

  current_group_ = static_cast<xkb_layout_index_t>(group);

  if (!xkb_state_) {
    LOG(WARNING) << "Received modifier without keymap?";
    return;
  }

  DCHECK(keymap_);

  layout_changed_callback_.Run(GenerateProtocolLayoutMessage());
}

}  // namespace remoting
