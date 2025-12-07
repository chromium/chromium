// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/ei_keymap.h"

#include <string_view>
#include <xkbcommon/xkbcommon.h>

#include "base/files/file.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "remoting/host/linux/keyboard_layout_monitor_utils.h"
#include "remoting/proto/control.pb.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace remoting {

EiKeymap::EiKeymap(EiDevicePtr keyboard) : keyboard_(keyboard) {}

EiKeymap::~EiKeymap() = default;

base::WeakPtr<EiKeymap> EiKeymap::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void EiKeymap::Load(base::OnceClosure callback) {
  base::ScopedClosureRunner closure_runner(std::move(callback));
  // Get the keymap file descriptor and verify that it's (at time of writing)
  // the only supported type.
  struct ei_keymap* keymap = ei_device_keyboard_get_keymap(keyboard_.get());
  if (!keymap) {
    LOG(ERROR) << "No keymap found for the current keyboard";
    return;
  }
  auto type = ei_keymap_get_type(keymap);
  if (type != EI_KEYMAP_TYPE_XKB) {
    LOG(ERROR) << "Unsupported keymap type: " << type;
    return;
  }
  // The file descriptor is owned by libei so needs to be dup'd before using
  // ScopedFD.
  auto original_fd = ei_keymap_get_fd(keymap);
  base::ScopedFD fd(HANDLE_EINTR(dup(original_fd)));
  if (!fd.is_valid()) {
    LOG(ERROR) << "Failed to duplicate keymap file descriptor";
    return;
  }
  // Since the reader as a member variable, base::Unretained is safe.
  reader_ = FdStringReader::ReadFromFile(
      std::move(fd),
      base::BindOnce(&EiKeymap::OnKeymapLoaded, base::Unretained(this),
                     closure_runner.Release()));
}

bool EiKeymap::IsValid() const {
  return keymap_ != nullptr;
}

xkb_keymap* EiKeymap::Get() {
  return keymap_.get();
}

const protocol::KeyboardLayout& EiKeymap::GetLayoutProto() const {
  return layout_proto_;
}

EiKeymap::Recipe::Recipe(uint32_t usb_code) : usb_code(usb_code) {}
EiKeymap::Recipe::Recipe(const EiKeymap::Recipe& other) = default;
EiKeymap::Recipe::~Recipe() = default;

EiKeymap::Recipe EiKeymap::GetRecipeForCodepoint(
    uint32_t codepoint) const {
  auto it = codepoint_to_usb_code_and_shift_level_.find(codepoint);
  if (it == codepoint_to_usb_code_and_shift_level_.end()) {
    return Recipe(0);
  }
  const auto& [usb_code, shift_level] = it->second;
  Recipe result(usb_code);
  if (shift_level & 1) {
    result.modifiers.insert(shift_key_usb_code_);
  }
  if (shift_level & 2) {
    result.modifiers.insert(altgr_key_usb_code_);
  }
  return result;
}

void EiKeymap::OnKeymapLoaded(base::OnceClosure callback,
                              base::expected<std::string, Loggable> result) {
  base::ScopedClosureRunner closure_runner(std::move(callback));
  if (!result.has_value()) {
    LOG(ERROR) << "Reading keymap failed: " << result.error();
    return;
  }
  // Create an xkb_keymap object from the mapped string.
  std::unique_ptr<xkb_context, ui::XkbContextDeleter> ctx{
      xkb_context_new(XKB_CONTEXT_NO_FLAGS)};
  if (!ctx) {
    LOG(ERROR) << "Failed to create XKB context";
    return;
  }
  keymap_.reset(xkb_keymap_new_from_string(ctx.get(), result.value().c_str(),
                                           XKB_KEYMAP_FORMAT_TEXT_V1,
                                           XKB_KEYMAP_COMPILE_NO_FLAGS));
  auto numlock_index = xkb_keymap_mod_get_index(keymap_.get(), "NumLock");
  auto numlock_mask = (numlock_index == XKB_MOD_INVALID) ? 0 : 1 << numlock_index;
  auto shift_index = xkb_keymap_mod_get_index(keymap_.get(), "Shift");
  auto shift_mask = (shift_index == XKB_MOD_INVALID) ? 0 : 1 << shift_index;
  auto altgr_index = xkb_keymap_mod_get_index(keymap_.get(), "Mod5");
  auto altgr_mask = (altgr_index == XKB_MOD_INVALID) ? 0 : 1 << altgr_index;
  shift_level_to_mask_[0] = numlock_mask;
  shift_level_to_mask_[1] = shift_mask | numlock_mask;
  if (altgr_mask) {
    shift_level_to_mask_[2] = altgr_mask | numlock_mask;
    shift_level_to_mask_[3] = shift_mask | altgr_mask | numlock_mask;
  }
  xkb_state_.reset(xkb_state_new(keymap_.get()));
  layout_proto_ = protocol::KeyboardLayout();
  xkb_keymap_key_for_each(keymap_.get(), &EiKeymap::ProcessKey, this);
}

bool EiKeymap::CanAutoRepeatUsbCode(uint32_t usb_code) const {
  return idempotent_usb_codes_.contains(usb_code);
}

void EiKeymap::ProcessKey(xkb_keymap* keymap,
                          xkb_keycode_t keycode,
                          void* data) {
  auto* self = static_cast<EiKeymap*>(data);
  auto usb_keycode = ui::KeycodeConverter::NativeKeycodeToUsbKeycode(keycode);
  auto& actions =
      *(*self->layout_proto_.mutable_keys())[usb_keycode].mutable_actions();
  bool is_idempotent = false;
  for (auto [level, mask] : self->shift_level_to_mask_) {
    xkb_state_update_mask(self->xkb_state_.get(), mask, 0, 0, 0, 0, 0);
    auto keysym = xkb_state_key_get_one_sym(self->xkb_state_.get(), keycode);
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
    // Convert the key to a function and a codepoint. Keys may have either,
    // both, or neither of these.
    auto function = KeyvalToFunction(keysym);
    auto codepoint = xkb_keysym_to_utf32(keysym);
    // Use the function to update the modifier map. Note that we assume that
    // modifier keys do the same thing regardless of shift level, so we don't
    // record the shift level.
    switch (function) {
      case protocol::LayoutKeyFunction::SHIFT:
        self->shift_key_usb_code_ = usb_keycode;
        break;
      case protocol::LayoutKeyFunction::ALT_GR:
        self->altgr_key_usb_code_ = usb_keycode;
        break;
      default:
        break;
    }
    // Use the character to update the character map if it doesn't already
    // exist with a lower shift level. Because of the way shift levels are
    // numbered and the fact that we only support Shift and AltGr, a lower
    // shift level indicates fewer modifiers, which is better for synthesizing
    // input using key presses.
    //
    // TODO(crbug.com/440652982): We've previously assumed that NumLock is on
    // when computing key actions. This means that injecting text events will
    // not work correctly if NumLock is off.
    if (codepoint) {
      auto& existing = self->codepoint_to_usb_code_and_shift_level_[codepoint];
      if (existing.usb_code == 0 || existing.shift_level > level) {
        existing.usb_code = usb_keycode;
        existing.shift_level = level;
      }
    }
    // Decide whether or not the key is idempotent, based on its function and
    // whether or not it has a printable codepoint at shift level 0. Note that
    // this doesn't handle complex scenarios where a key is idempotent at one
    // shift level but not another, but the code that suppresses auto-repeat
    // doesn't handle that case either.
    if (level == 0) {
      switch (function) {
        // Keys with codepoints that are nevertheless idempotent.
        case protocol::LayoutKeyFunction::ESCAPE:
          is_idempotent = true;
          break;

        // Keys without codepoints that are nevertheless not idempotent.
        case protocol::LayoutKeyFunction::ARROW_DOWN:
        case protocol::LayoutKeyFunction::ARROW_LEFT:
        case protocol::LayoutKeyFunction::ARROW_RIGHT:
        case protocol::LayoutKeyFunction::ARROW_UP:
        case protocol::LayoutKeyFunction::SCROLL_LOCK:
        case protocol::LayoutKeyFunction::PRINT_SCREEN:
        case protocol::LayoutKeyFunction::PAGE_UP:
        case protocol::LayoutKeyFunction::PAGE_DOWN:
        case protocol::LayoutKeyFunction::INSERT:
          is_idempotent = false;
          break;

        // For all other keys, idempotent-ness is equated with having a
        // codepoint.
        default:
          is_idempotent = (codepoint == 0);
          break;
      }
    }
    // The function takes precedence over the character so that the correct
    // symbol is displayed by the client for things like the Enter key.
    if (function != protocol::LayoutKeyFunction::UNKNOWN) {
      actions[level].set_function(function);
      continue;
    }
    // Handle regular characters next.
    if (codepoint) {
      std::string utf8;
      base::WriteUnicodeCharacter(codepoint, &utf8);
      actions[level].set_character(utf8);
      continue;
    }
    // Handle dead keys last.
    const char* dead_key_utf8 = DeadKeyToUtf8String(keysym);
    if (dead_key_utf8) {
      actions[level].set_character(dead_key_utf8);
      continue;
    }
  }
  // Delete the entry that was implicitly added by the []-operator if
  // there are no actions.
  if (actions.empty()) {
    self->layout_proto_.mutable_keys()->erase(usb_keycode);
  }
  // Update the set of idempotent keys.
  if (usb_keycode && is_idempotent) {
    self->idempotent_usb_codes_.insert(usb_keycode);
  }
}

}  // namespace remoting
