// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/test/mock_keyboard_driver_win.h"

#include <stddef.h>
#include <string.h>

#include "base/check.h"
#include "content/test/mock_keyboard.h"

namespace content {

MockKeyboardDriverWin::MockKeyboardDriverWin() {
  // Save the keyboard layout and status of the application.
  // This class changes the keyboard layout and status of this application.
  // This change may break succeeding tests. To prevent this possible break, we
  // should save the layout and status here to restore when this instance is
  // destroyed.
  original_keyboard_layout_ = GetKeyboardLayout(0);
  active_keyboard_layout_ = original_keyboard_layout_;
  GetKeyboardState(&original_keyboard_states_[0]);

  const UINT num_keyboard_layouts = GetKeyboardLayoutList(0, NULL);
  DCHECK(num_keyboard_layouts > 0);

  orig_keyboard_layouts_list_.resize(num_keyboard_layouts);
  GetKeyboardLayoutList(num_keyboard_layouts, &orig_keyboard_layouts_list_[0]);

  memset(&keyboard_states_[0], 0, sizeof(keyboard_states_));
}

MockKeyboardDriverWin::~MockKeyboardDriverWin() {
  // Unload the keyboard-layout driver, restore the keyboard state, and reset
  // the keyboard layout for succeeding tests.
  MaybeUnloadActiveLayout();
  SetKeyboardState(&original_keyboard_states_[0]);
  ActivateKeyboardLayout(original_keyboard_layout_, KLF_RESET);
}

void MockKeyboardDriverWin::MaybeUnloadActiveLayout() {
  // Workaround for http://crbug.com/12093
  // Only unload a keyboard layout if it was loaded by this mock driver.
  // Contrary to the documentation on MSDN unloading a keyboard layout
  // previously loaded by the system causes that layout to stop working.
  // We have confirmation of this behavior on XP & Vista.
  for (size_t i = 0; i < orig_keyboard_layouts_list_.size(); ++i) {
    if (orig_keyboard_layouts_list_[i] == active_keyboard_layout_)
      return;
  }

  // If we got here, this keyboard layout wasn't loaded by the system so it's
  // safe to unload it ourselve's.
  UnloadKeyboardLayout(active_keyboard_layout_);
  active_keyboard_layout_ = original_keyboard_layout_;
}

bool MockKeyboardDriverWin::SetLayout(int layout) {
  // Unload the current keyboard-layout driver and load a new keyboard-layout
  // driver for mapping a virtual key-code to a Unicode character.
  MaybeUnloadActiveLayout();

  // Scan the mapping table and retrieve a Language ID for the input layout.
  // Load the keyboard-layout driver when we find a Language ID.
  // This Language IDs are copied from the registry
  //   "HKLM\SYSTEM\CurrentControlSet\Control\Keyboard layouts".
  // TODO(hbono): Add more keyboard-layout drivers.
  static const struct {
    const wchar_t* language;
    MockKeyboard::Layout keyboard_layout;
  } kLanguageIDs[] = {
    {L"00000401", MockKeyboard::LAYOUT_ARABIC},
    {L"00000402", MockKeyboard::LAYOUT_BULGARIAN},
    {L"00000404", MockKeyboard::LAYOUT_CHINESE_TRADITIONAL},
    {L"00000405", MockKeyboard::LAYOUT_CZECH},
    {L"00000406", MockKeyboard::LAYOUT_DANISH},
    {L"00000407", MockKeyboard::LAYOUT_GERMAN},
    {L"00000408", MockKeyboard::LAYOUT_GREEK},
    {L"00000409", MockKeyboard::LAYOUT_UNITED_STATES},
    {L"0000040a", MockKeyboard::LAYOUT_SPANISH},
    {L"0000040b", MockKeyboard::LAYOUT_FINNISH},
    {L"0000040c", MockKeyboard::LAYOUT_FRENCH},
    {L"0000040d", MockKeyboard::LAYOUT_HEBREW},
    {L"0000040e", MockKeyboard::LAYOUT_HUNGARIAN},
    {L"00000410", MockKeyboard::LAYOUT_ITALIAN},
    {L"00000411", MockKeyboard::LAYOUT_JAPANESE},
    {L"00000412", MockKeyboard::LAYOUT_KOREAN},
    {L"00000415", MockKeyboard::LAYOUT_POLISH},
    {L"00000416", MockKeyboard::LAYOUT_PORTUGUESE_BRAZILIAN},
    {L"00000418", MockKeyboard::LAYOUT_ROMANIAN},
    {L"00000419", MockKeyboard::LAYOUT_RUSSIAN},
    {L"0000041a", MockKeyboard::LAYOUT_CROATIAN},
    {L"0000041b", MockKeyboard::LAYOUT_SLOVAK},
    {L"0000041e", MockKeyboard::LAYOUT_THAI},
    {L"0000041d", MockKeyboard::LAYOUT_SWEDISH},
    {L"0000041f", MockKeyboard::LAYOUT_TURKISH_Q},
    {L"0000042a", MockKeyboard::LAYOUT_VIETNAMESE},
    {L"00000439", MockKeyboard::LAYOUT_DEVANAGARI_INSCRIPT},
    {L"00000816", MockKeyboard::LAYOUT_PORTUGUESE},
    {L"00001409", MockKeyboard::LAYOUT_UNITED_STATES_DVORAK},
    {L"00001009", MockKeyboard::LAYOUT_CANADIAN_FRENCH},
  };

  for (size_t i = 0; i < std::size(kLanguageIDs); ++i) {
    if (layout == kLanguageIDs[i].keyboard_layout) {
      HKL new_keyboard_layout = LoadKeyboardLayout(kLanguageIDs[i].language,
                                                   KLF_ACTIVATE);
      // loaded_keyboard_layout_ must always have a valid keyboard handle
      // so we only assign upon success.
      if (new_keyboard_layout) {
        active_keyboard_layout_ = new_keyboard_layout;
        return true;
      }

      return false;
    }
  }

  // Return false if there are not any matching drivers.
  return false;
}

bool MockKeyboardDriverWin::SetModifiers(int modifiers) {
  // Over-write the keyboard status with our modifier-key status.
  // WebInputEventFactory::keyboardEvent() uses GetKeyState() to retrive
  // modifier-key status. So, we update the modifier-key status with this
  // SetKeyboardState() call before creating NativeWebKeyboardEvent
  // instances.
  memset(&keyboard_states_[0], 0, sizeof(keyboard_states_));
  static const struct {
    int key_code;
    int mask;
  } kModifierMasks[] = {
    {VK_SHIFT,    MockKeyboard::LEFT_SHIFT | MockKeyboard::RIGHT_SHIFT},
    {VK_CONTROL,  MockKeyboard::LEFT_CONTROL | MockKeyboard::RIGHT_CONTROL},
    {VK_MENU,     MockKeyboard::LEFT_ALT | MockKeyboard::RIGHT_ALT},
    {VK_LSHIFT,   MockKeyboard::LEFT_SHIFT},
    {VK_LCONTROL, MockKeyboard::LEFT_CONTROL},
    {VK_LMENU,    MockKeyboard::LEFT_ALT},
    {VK_RSHIFT,   MockKeyboard::RIGHT_SHIFT},
    {VK_RCONTROL, MockKeyboard::RIGHT_CONTROL},
    {VK_RMENU,    MockKeyboard::RIGHT_ALT},
  };
  for (size_t i = 0; i < std::size(kModifierMasks); ++i) {
    const int kKeyDownMask = 0x80;
    if (modifiers & kModifierMasks[i].mask)
      keyboard_states_[kModifierMasks[i].key_code] = kKeyDownMask;
  }
  SetKeyboardState(&keyboard_states_[0]);

  return true;
}

int MockKeyboardDriverWin::GetCharacters(int key_code,
                                         std::wstring* output) {
  // Retrieve Unicode characters composed from the input key-code and
  // the mofifiers.
  CHECK(output);
  wchar_t code[16];
  int length =
      ToUnicodeEx(key_code, MapVirtualKey(key_code, 0), &keyboard_states_[0],
                  &code[0], std::size(code), 0, active_keyboard_layout_);
  if (length > 0)
    output->assign(code);
  return length;
}

}  // namespace content
