// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_VIRTUAL_KEYBOARD_DELEGATE_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_VIRTUAL_KEYBOARD_DELEGATE_H_

#include <string>

#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_delegate.h"
#include "extensions/common/api/virtual_keyboard.h"

namespace extensions {

class ShellVirtualKeyboardDelegate : public VirtualKeyboardDelegate {
 public:
  ShellVirtualKeyboardDelegate() = default;

  ShellVirtualKeyboardDelegate(const ShellVirtualKeyboardDelegate&) = delete;
  ShellVirtualKeyboardDelegate& operator=(const ShellVirtualKeyboardDelegate&) =
      delete;

  ~ShellVirtualKeyboardDelegate() override = default;

 protected:
  // VirtualKeyboardDelegate impl:
  void GetKeyboardConfig(
      OnKeyboardSettingsCallback on_settings_callback) override;
  void OnKeyboardConfigChanged() override;
  bool HideKeyboard() override;
  bool InsertText(const std::u16string& text) override;
  bool OnKeyboardLoaded() override;
  void SetHotrodKeyboard(bool enable) override;
  bool LockKeyboard(bool state) override;
  bool SendKeyEvent(const std::string& type,
                    int char_value,
                    int key_code,
                    const std::string& key_name,
                    int modifiers) override;
  bool ShowLanguageSettings() override;
  bool ShowSuggestionSettings() override;
  bool IsSettingsEnabled() override;
  bool SetVirtualKeyboardMode(api::virtual_keyboard_private::KeyboardMode mode,
                              gfx::Rect target_bounds,
                              OnSetModeCallback on_set_mode_callback) override;
  bool SetDraggableArea(
      const api::virtual_keyboard_private::Bounds& rect) override;
  bool SetRequestedKeyboardState(
      api::virtual_keyboard_private::KeyboardState state) override;
  bool SetOccludedBounds(const std::vector<gfx::Rect>& bounds) override;
  bool SetHitTestBounds(const std::vector<gfx::Rect>& bounds) override;
  bool SetAreaToRemainOnScreen(const gfx::Rect& bounds) override;
  bool SetWindowBoundsInScreen(const gfx::Rect& bounds_in_screen) override;
  void GetClipboardHistory(
      OnGetClipboardHistoryCallback get_history_callback) override;
  bool PasteClipboardItem(const std::string& clipboard_item_id) override;
  bool DeleteClipboardItem(const std::string& clipboard_item_id) override;

  void RestrictFeatures(
      const api::virtual_keyboard::RestrictFeatures::Params& params,
      OnRestrictFeaturesCallback callback) override;

 private:
  bool is_hotrod_keyboard_ = false;
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_VIRTUAL_KEYBOARD_DELEGATE_H_
