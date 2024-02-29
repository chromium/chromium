// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_VIRTUAL_KEYBOARD_PRIVATE_VIRTUAL_KEYBOARD_DELEGATE_H_
#define EXTENSIONS_BROWSER_API_VIRTUAL_KEYBOARD_PRIVATE_VIRTUAL_KEYBOARD_DELEGATE_H_

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/values.h"
#include "extensions/common/api/virtual_keyboard.h"
#include "extensions/common/api/virtual_keyboard_private.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {
class ClipboardHistoryItem;
}  // namespace ash

namespace extensions {

class VirtualKeyboardDelegate {
 public:
  virtual ~VirtualKeyboardDelegate() = default;

  using OnKeyboardSettingsCallback =
      base::OnceCallback<void(std::optional<base::Value::Dict> settings)>;

  using OnSetModeCallback = base::OnceCallback<void(bool success)>;

  using OnGetClipboardHistoryCallback =
      base::OnceCallback<void(std::vector<ash::ClipboardHistoryItem> history)>;

  using OnRestrictFeaturesCallback = base::OnceCallback<void(
      api::virtual_keyboard::FeatureRestrictions update)>;

  // Fetch information about the preferred configuration of the keyboard. On
  // exit, |settings| is populated with the keyboard configuration if execution
  // is successful, otherwise it's set to nullptr.
  virtual void GetKeyboardConfig(
      OnKeyboardSettingsCallback on_settings_callback) = 0;

  // Notify keyboard config change through
  // |chrome.virtualKeyboard.onKeyboardConfigChanged| event.
  virtual void OnKeyboardConfigChanged() = 0;

  // Dismiss the virtual keyboard without changing input focus. Returns true if
  // successful.
  virtual bool HideKeyboard() = 0;

  // Insert |text| verbatim into a text area. Returns true if successful.
  virtual bool InsertText(const std::u16string& text) = 0;

  // Notifiy system that keyboard loading is complete. Used in UMA stats to
  // track loading performance. Returns true if the notification was handled.
  virtual bool OnKeyboardLoaded() = 0;

  // Indicate if settings are accessible and enabled based on current state.
  // For example, settings should be blocked when the session is locked.
  virtual bool IsSettingsEnabled() = 0;

  // Sets the state of the hotrod virtual keyboad.
  virtual void SetHotrodKeyboard(bool enable) = 0;

  // Activate and lock the virtual keyboad on screen or dismiss the keyboard
  // regardless of the state of text focus. Used in a11y mode to allow typing
  // hotkeys without the need for text focus. Returns true if successful.
  virtual bool LockKeyboard(bool state) = 0;

  // Dispatches a virtual key event. |type| indicates if the event is a keydown
  // or keyup event. |char_value| is the unicode value for the key. |key_code|
  // is the code assigned to the key, which is independent of the state of
  // modifier keys. |key_name| is the standardized w3c name for the key.
  // |modifiers| indicates which modifier keys are active. Returns true if
  // successful.
  virtual bool SendKeyEvent(const std::string& type,
                            int char_value,
                            int key_code,
                            const std::string& key_name,
                            int modifiers) = 0;

  // Launches the settings app. Returns true if successful.
  virtual bool ShowLanguageSettings() = 0;

  // Launches Suggestions page in settings app. Retusn true is successful.
  virtual bool ShowSuggestionSettings() = 0;

  // Sets virtual keyboard window mode.
  virtual bool SetVirtualKeyboardMode(
      api::virtual_keyboard_private::KeyboardMode mode_enum,
      gfx::Rect target_bounds,
      OnSetModeCallback on_set_mode_callback) = 0;

  // Sets virtual keyboard draggable area bounds.
  // Returns whether the draggable area is set successful.
  virtual bool SetDraggableArea(
      const api::virtual_keyboard_private::Bounds& bounds) = 0;

  // Sets requested virtual keyboard state.
  virtual bool SetRequestedKeyboardState(
      api::virtual_keyboard_private::KeyboardState state) = 0;

  // Sets the area on the screen that is occluded by the keyboard.
  virtual bool SetOccludedBounds(const std::vector<gfx::Rect>& bounds) = 0;

  // Sets the areas on the keyboard window where events are handled.
  virtual bool SetHitTestBounds(const std::vector<gfx::Rect>& bounds) = 0;

  // Sets the area of the keyboard window that should remain on screen
  // whenever the user moves the keyboard around their screen.
  virtual bool SetAreaToRemainOnScreen(const gfx::Rect& bounds) = 0;

  // Sets the bounds of the keyboard window in screen coordinates.
  virtual bool SetWindowBoundsInScreen(const gfx::Rect& bounds_in_screen) = 0;

  // Calls the |get_history_callback| function and passes a value containing the
  // current cipboard history items.
  virtual void GetClipboardHistory(
      OnGetClipboardHistoryCallback get_history_callback) = 0;

  // Paste a clipboard item from the clipboard history. Returns whether the
  // paste is successful.
  virtual bool PasteClipboardItem(const std::string& clipboard_item_id) = 0;

  // Delete a clipboard item from the clipboard history. Returns whether the
  // deletion is successful.
  virtual bool DeleteClipboardItem(const std::string& clipboard_item_id) = 0;

  // Restricts the virtual keyboard IME features.
  // callback is called with the values which were updated.
  virtual void RestrictFeatures(
      const api::virtual_keyboard::RestrictFeatures::Params& params,
      OnRestrictFeaturesCallback callback) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_VIRTUAL_KEYBOARD_PRIVATE_VIRTUAL_KEYBOARD_DELEGATE_H_
