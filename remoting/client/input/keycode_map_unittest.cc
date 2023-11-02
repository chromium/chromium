// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/input/keycode_map.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

TEST(KeycodeMapTest, KeypressFromUnicodeWithAsciiCharsOnQwertyLayout) {
  KeypressInfo keypress = KeypressFromUnicode('a');
  EXPECT_EQ(ui::DomCode::US_A, keypress.dom_code);
  EXPECT_EQ(KeypressInfo::Modifier::NONE, keypress.modifiers);

  keypress = KeypressFromUnicode('Q');
  EXPECT_EQ(ui::DomCode::US_Q, keypress.dom_code);
  EXPECT_EQ(KeypressInfo::Modifier::SHIFT, keypress.modifiers);

  keypress = KeypressFromUnicode(' ');
  EXPECT_EQ(ui::DomCode::SPACE, keypress.dom_code);
  EXPECT_EQ(KeypressInfo::Modifier::NONE, keypress.modifiers);

  keypress = KeypressFromUnicode('\n');
  EXPECT_EQ(ui::DomCode::ENTER, keypress.dom_code);
  EXPECT_EQ(KeypressInfo::Modifier::NONE, keypress.modifiers);

  keypress = KeypressFromUnicode('!');
  EXPECT_EQ(ui::DomCode::DIGIT1, keypress.dom_code);
  EXPECT_EQ(KeypressInfo::Modifier::SHIFT, keypress.modifiers);

  keypress = KeypressFromUnicode('`');
  EXPECT_EQ(ui::DomCode::BACKQUOTE, keypress.dom_code);
  EXPECT_EQ(KeypressInfo::Modifier::NONE, keypress.modifiers);
}

TEST(KeycodeMapTest, KeypressFromUnicodeWithUnmappableCharsOnQwertyLayout) {
  // NULL
  KeypressInfo keypress = KeypressFromUnicode(0);
  EXPECT_EQ(ui::DomCode::NONE, keypress.dom_code);
  EXPECT_EQ(KeypressInfo::Modifier::NONE, keypress.modifiers);

  // BELL
  keypress = KeypressFromUnicode(7);
  EXPECT_EQ(ui::DomCode::NONE, keypress.dom_code);
  EXPECT_EQ(KeypressInfo::Modifier::NONE, keypress.modifiers);

  // ESC
  keypress = KeypressFromUnicode(27);
  EXPECT_EQ(ui::DomCode::NONE, keypress.dom_code);
  EXPECT_EQ(KeypressInfo::Modifier::NONE, keypress.modifiers);

  // Non-ASCII character
  keypress = KeypressFromUnicode(128);
  EXPECT_EQ(ui::DomCode::NONE, keypress.dom_code);
  EXPECT_EQ(KeypressInfo::Modifier::NONE, keypress.modifiers);

  // Non-ASCII character
  keypress = KeypressFromUnicode(2000);
  EXPECT_EQ(ui::DomCode::NONE, keypress.dom_code);
  EXPECT_EQ(KeypressInfo::Modifier::NONE, keypress.modifiers);
}

}  // namespace remoting
