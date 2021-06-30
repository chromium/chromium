// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/input/keycode_map.h"

#include <array>
#include <limits>
#include <ostream>

#include "base/check.h"
#include "base/no_destructor.h"

namespace remoting {

namespace {

// TODO(yuweih): Using char to store the characters may not work if we decide
// to support AZERTY or other keyboard.
const size_t kMaxAsciiConvertibleLength =
    std::numeric_limits<unsigned char>::max();

// An array with character code as its index and KeycodeInfo as its value.
using KeycodeMap = std::array<KeypressInfo, kMaxAsciiConvertibleLength>;

struct KeycodeMapEntry {
  ui::DomCode dom_code;
  unsigned char normal_character;
  // 0 if there is no shift character.
  unsigned char shift_character;
};

constexpr KeycodeMapEntry kKeycodeMapEntriesQwerty[] = {
    {ui::DomCode::US_A, 'a', 'A'},
    {ui::DomCode::US_B, 'b', 'B'},
    {ui::DomCode::US_C, 'c', 'C'},
    {ui::DomCode::US_D, 'd', 'D'},
    {ui::DomCode::US_E, 'e', 'E'},
    {ui::DomCode::US_F, 'f', 'F'},
    {ui::DomCode::US_G, 'g', 'G'},
    {ui::DomCode::US_H, 'h', 'H'},
    {ui::DomCode::US_I, 'i', 'I'},
    {ui::DomCode::US_J, 'j', 'J'},
    {ui::DomCode::US_K, 'k', 'K'},
    {ui::DomCode::US_L, 'l', 'L'},
    {ui::DomCode::US_M, 'm', 'M'},
    {ui::DomCode::US_N, 'n', 'N'},
    {ui::DomCode::US_O, 'o', 'O'},
    {ui::DomCode::US_P, 'p', 'P'},
    {ui::DomCode::US_Q, 'q', 'Q'},
    {ui::DomCode::US_R, 'r', 'R'},
    {ui::DomCode::US_S, 's', 'S'},
    {ui::DomCode::US_T, 't', 'T'},
    {ui::DomCode::US_U, 'u', 'U'},
    {ui::DomCode::US_V, 'v', 'V'},
    {ui::DomCode::US_W, 'w', 'W'},
    {ui::DomCode::US_X, 'x', 'X'},
    {ui::DomCode::US_Y, 'y', 'Y'},
    {ui::DomCode::US_Z, 'z', 'Z'},
    {ui::DomCode::DIGIT1, '1', '!'},
    {ui::DomCode::DIGIT2, '2', '@'},
    {ui::DomCode::DIGIT3, '3', '#'},
    {ui::DomCode::DIGIT4, '4', '$'},
    {ui::DomCode::DIGIT5, '5', '%'},
    {ui::DomCode::DIGIT6, '6', '^'},
    {ui::DomCode::DIGIT7, '7', '&'},
    {ui::DomCode::DIGIT8, '8', '*'},
    {ui::DomCode::DIGIT9, '9', '('},
    {ui::DomCode::DIGIT0, '0', ')'},
    {ui::DomCode::SPACE, ' ', 0},
    {ui::DomCode::ENTER, '\n', 0},
    {ui::DomCode::MINUS, '-', '_'},
    {ui::DomCode::EQUAL, '=', '+'},
    {ui::DomCode::BRACKET_LEFT, '[', '{'},
    {ui::DomCode::BRACKET_RIGHT, ']', '}'},
    {ui::DomCode::BACKSLASH, '\\', '|'},
    {ui::DomCode::SEMICOLON, ';', ':'},
    {ui::DomCode::QUOTE, '\'', '"'},
    {ui::DomCode::BACKQUOTE, '`', '~'},
    {ui::DomCode::COMMA, ',', '<'},
    {ui::DomCode::PERIOD, '.', '>'},
    {ui::DomCode::SLASH, '/', '?'},
};

template <size_t N>
KeycodeMap CreateKeycodeMapFromMapEntries(const KeycodeMapEntry (&entries)[N]) {
  KeycodeMap map;
  // Initialize keycodes with NONE.
  for (size_t i = 0; i < map.size(); i++) {
    map[i] = {ui::DomCode::NONE, KeypressInfo::Modifier::NONE};
  }
  for (size_t i = 0; i < N; i++) {
    const KeycodeMapEntry& entry = entries[i];
    DCHECK(entry.dom_code != ui::DomCode::NONE);
    DCHECK(entry.normal_character != 0);

    DCHECK(map[entry.normal_character].dom_code == ui::DomCode::NONE)
        << "Character " << entry.normal_character << " is already in the map.";
    map[entry.normal_character].dom_code = entry.dom_code;

    if (entry.shift_character != 0) {
      DCHECK(map[entry.shift_character].dom_code == ui::DomCode::NONE)
          << "Character " << entry.shift_character << " is already in the map.";
      map[entry.shift_character].dom_code = entry.dom_code;
      map[entry.shift_character].modifiers = KeypressInfo::Modifier::SHIFT;
    }
  }
  return map;
}

const KeycodeMap& GetKeycodeMapQwerty() {
  static const base::NoDestructor<KeycodeMap> map(
      CreateKeycodeMapFromMapEntries(kKeycodeMapEntriesQwerty));
  return *map;
}

}  // namespace

KeypressInfo KeypressFromUnicode(unsigned int unicode) {
  if (unicode >= kMaxAsciiConvertibleLength) {
    return {ui::DomCode::NONE, KeypressInfo::Modifier::NONE};
  } else {
    return GetKeycodeMapQwerty()[unicode];
  }
}

}  // namespace remoting
