// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_MOCK_KEYBOARD_H_
#define CONTENT_TEST_MOCK_KEYBOARD_H_

#include <string>

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "content/test/mock_keyboard_driver_win.h"
#endif

namespace content {

// A mock keyboard interface.
// This class defines a pseudo keyboard device, which implements mappings from
// a tuple (layout, key code, modifiers) to Unicode characters so that
// engineers can write RenderViewTest cases without taking care of such
// mappings. (This mapping is not trivial when using non-US keyboards.)
// A pseudo keyboard device consists of two parts: a platform-independent part
// and a platform-dependent part. This class implements the platform-independent
// part. The platform-dependet part is implemented in the MockKeyboardWin class.
// This class is usually called from RenderViewTest::SendKeyEvent().
class MockKeyboard {
 public:
  // Represents keyboard-layouts.
  enum Layout {
    LAYOUT_NULL,
    LAYOUT_ARABIC,
    LAYOUT_BULGARIAN,
    LAYOUT_CHINESE_TRADITIONAL,
    LAYOUT_CZECH,
    LAYOUT_DANISH,
    LAYOUT_GERMAN,
    LAYOUT_GREEK,
    LAYOUT_UNITED_STATES,
    LAYOUT_SPANISH,
    LAYOUT_FINNISH,
    LAYOUT_FRENCH,
    LAYOUT_HEBREW,
    LAYOUT_HUNGARIAN,
    LAYOUT_ICELANDIC,
    LAYOUT_ITALIAN,
    LAYOUT_JAPANESE,
    LAYOUT_KOREAN,
    LAYOUT_POLISH,
    LAYOUT_PORTUGUESE_BRAZILIAN,
    LAYOUT_ROMANIAN,
    LAYOUT_RUSSIAN,
    LAYOUT_CROATIAN,
    LAYOUT_SLOVAK,
    LAYOUT_THAI,
    LAYOUT_SWEDISH,
    LAYOUT_TURKISH_Q,
    LAYOUT_VIETNAMESE,
    LAYOUT_DEVANAGARI_INSCRIPT,
    LAYOUT_PORTUGUESE,
    LAYOUT_UNITED_STATES_DVORAK,
    LAYOUT_CANADIAN_FRENCH,
  };

  // Enumerates keyboard modifiers.
  // These modifiers explicitly distinguish left-keys and right-keys because we
  // should emulate AltGr (right-alt) key, used by many European keyboards to
  // input alternate graph characters.
  enum Modifiers {
    INVALID = -1,
    NONE = 0,
    LEFT_SHIFT = 1 << 0,
    LEFT_CONTROL = 1 << 1,
    LEFT_ALT = 1 << 2,
    LEFT_META = 1 << 3,
    RIGHT_SHIFT = 1 << 4,
    RIGHT_CONTROL = 1 << 5,
    RIGHT_ALT = 1 << 6,
    RIGHT_META = 1 << 7,
    KEYPAD = 1 << 8,
    AUTOREPEAAT = 1 << 9,
  };

  MockKeyboard();

  MockKeyboard(const MockKeyboard&) = delete;
  MockKeyboard& operator=(const MockKeyboard&) = delete;

  ~MockKeyboard();

  // Retrieves Unicode characters composed from the the specified keyboard
  // layout, key code, and modifiers, i.e. characters returned when we type
  // specified keys on a specified layout.
  // This function returns the length of Unicode characters filled in the
  // |output| parameter.
  int GetCharacters(Layout layout,
                    int key_code,
                    Modifiers modifiers,
                    std::u16string* output);

 private:
#if BUILDFLAG(IS_WIN)
  Layout keyboard_layout_ = LAYOUT_NULL;
  Modifiers keyboard_modifiers_ = INVALID;
  MockKeyboardDriverWin driver_;
#endif
};

}  // namespace content

#endif  // CONTENT_TEST_MOCK_KEYBOARD_H_
