// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_VIRTUAL_KEYBOARD_DELEGATE_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_VIRTUAL_KEYBOARD_DELEGATE_H_

#include <string>

#include "base/macros.h"
#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_delegate.h"
#include "extensions/common/api/virtual_keyboard.h"

namespace extensions {

class ShellVirtualKeyboardDelegate : public VirtualKeyboardDelegate {
 public:
  ShellVirtualKeyboardDelegate();
  ~ShellVirtualKeyboardDelegate() override = default;

 protected:
  // VirtualKeyboardDelegate impl:
  void GetKeyboardConfig(
      OnKeyboardSettingsCallback on_settings_callback) override;
  void OnKeyboardConfigChanged() override;
  bool HideKeyboard() override;
  bool InsertText(const base::string16& text) override;
  bool OnKeyboardLoaded() override;
  void SetHotrodKeyboard(bool enable) override;
  bool LockKeyboard(bool state) override;
  bool SendKeyEvent(const std::string& type,
                    int char_value,
                    int key_code,
                    const std::string& key_name,
                    int modifiers) override;
  bool ShowLanguageSettings() override;
  bool IsLanguageSettingsEnabled() override;
  bool SetVirtualKeyboardMode(int mode_enum,
                              base::Optional<gfx::Rect> target_bounds,
                              OnSetModeCallback on_set_mode_callback) override;
  bool SetDraggableArea(
      const api::virtual_keyboard_private::Bounds& rect) override;
  bool SetRequestedKeyboardState(int state_enum) override;
  bool SetOccludedBounds(const std::vector<gfx::Rect>& bounds) override;
  bool SetHitTestBounds(const std::vector<gfx::Rect>& bounds) override;
  bool SetAreaToRemainOnScreen(const gfx::Rect& bounds) override;

  api::virtual_keyboard::FeatureRestrictions RestrictFeatures(
      const api::virtual_keyboard::RestrictFeatures::Params& params) override;

 private:
  bool is_hotrod_keyboard_ = false;

  DISALLOW_COPY_AND_ASSIGN(ShellVirtualKeyboardDelegate);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_VIRTUAL_KEYBOARD_DELEGATE_H_
