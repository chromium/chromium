// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_INPUT_KEYCODE_MAP_H_
#define REMOTING_CLIENT_INPUT_KEYCODE_MAP_H_

#include "ui/events/keycodes/dom/dom_code.h"

namespace remoting {

// A class representing a combination of keys (i.e. main key+modifiers) that
// should be pressed at the same time.
struct KeypressInfo {
  enum Modifier {
    NONE = 0,
    SHIFT = 1 << 0,
    // TODO(yuweih): Add more modifiers when needed.
  };

  ui::DomCode dom_code;
  Modifier modifiers;
};

// Gets a keypress that can produce the given unicode on the given keyboard
// layout. If no such a keypress can be found, a keypress with dom_code = NONE
// will be returned.
KeypressInfo KeypressFromUnicode(unsigned int unicode);

}  // namespace remoting
#endif  // REMOTING_CLIENT_INPUT_KEYCODE_MAP_H_
