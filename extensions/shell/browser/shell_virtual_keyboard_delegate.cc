// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_virtual_keyboard_delegate.h"

#include <memory>
#include <utility>

#include "base/notreached.h"
#include "base/values.h"

namespace extensions {

void ShellVirtualKeyboardDelegate::GetKeyboardConfig(
    OnKeyboardSettingsCallback on_settings_callback) {
  base::Value::Dict settings;
  settings.Set("hotrodmode", is_hotrod_keyboard_);
  std::move(on_settings_callback).Run(std::move(settings));
}

void ShellVirtualKeyboardDelegate::OnKeyboardConfigChanged() {
  NOTIMPLEMENTED();
}

bool ShellVirtualKeyboardDelegate::HideKeyboard() {
  return false;
}

bool ShellVirtualKeyboardDelegate::InsertText(const std::u16string& text) {
  return false;
}

bool ShellVirtualKeyboardDelegate::OnKeyboardLoaded() {
  return false;
}

void ShellVirtualKeyboardDelegate::SetHotrodKeyboard(bool enable) {
  is_hotrod_keyboard_ = enable;
}

bool ShellVirtualKeyboardDelegate::LockKeyboard(bool state) {
  return false;
}

bool ShellVirtualKeyboardDelegate::SendKeyEvent(const std::string& type,
                                                int char_value,
                                                int key_code,
                                                const std::string& key_name,
                                                int modifiers) {
  return false;
}

bool ShellVirtualKeyboardDelegate::ShowLanguageSettings() {
  return false;
}

bool ShellVirtualKeyboardDelegate::ShowSuggestionSettings() {
  return false;
}

bool ShellVirtualKeyboardDelegate::IsSettingsEnabled() {
  return false;
}

bool ShellVirtualKeyboardDelegate::SetVirtualKeyboardMode(
    api::virtual_keyboard_private::KeyboardMode mode,
    gfx::Rect target_bounds,
    OnSetModeCallback on_set_mode_callback) {
  return false;
}

bool ShellVirtualKeyboardDelegate::SetDraggableArea(
    const api::virtual_keyboard_private::Bounds& rect) {
  return false;
}

bool ShellVirtualKeyboardDelegate::SetRequestedKeyboardState(
    api::virtual_keyboard_private::KeyboardState state) {
  return false;
}

bool ShellVirtualKeyboardDelegate::SetOccludedBounds(
    const std::vector<gfx::Rect>& bounds) {
  return false;
}

bool ShellVirtualKeyboardDelegate::SetHitTestBounds(
    const std::vector<gfx::Rect>& bounds) {
  return false;
}

bool ShellVirtualKeyboardDelegate::SetAreaToRemainOnScreen(
    const gfx::Rect& bounds) {
  return false;
}

bool ShellVirtualKeyboardDelegate::SetWindowBoundsInScreen(
    const gfx::Rect& bounds_in_screen) {
  return false;
}

void ShellVirtualKeyboardDelegate::GetClipboardHistory(
    OnGetClipboardHistoryCallback get_history_callback) {
  NOTIMPLEMENTED();
}

bool ShellVirtualKeyboardDelegate::PasteClipboardItem(
    const std::string& clipboard_item_id) {
  NOTIMPLEMENTED();
  return false;
}

bool ShellVirtualKeyboardDelegate::DeleteClipboardItem(
    const std::string& clipboard_item_id) {
  NOTIMPLEMENTED();
  return false;
}

void ShellVirtualKeyboardDelegate::RestrictFeatures(
    const api::virtual_keyboard::RestrictFeatures::Params& params,
    OnRestrictFeaturesCallback callback) {
  // Return the given parameter as is, since there's no stored values.
  api::virtual_keyboard::FeatureRestrictions update;
  update.spell_check_enabled = params.restrictions.spell_check_enabled;
  update.auto_complete_enabled = params.restrictions.auto_complete_enabled;
  update.auto_correct_enabled = params.restrictions.auto_correct_enabled;
  update.voice_input_enabled = params.restrictions.voice_input_enabled;
  update.handwriting_enabled = params.restrictions.handwriting_enabled;
  std::move(callback).Run(std::move(update));
}

}  // namespace extensions
