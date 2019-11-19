/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/editing/editing_behavior.h"

#include <memory>

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace blink {

class KeyboardTest : public testing::Test {
 public:
  // Pass a WebKeyboardEvent into the EditorClient and get back the string
  // name of which editing event that key causes.
  // E.g., sending in the enter key gives back "InsertNewline".
  const char* InterpretKeyEvent(const WebKeyboardEvent& web_keyboard_event) {
    KeyboardEvent* keyboard_event =
        KeyboardEvent::Create(web_keyboard_event, nullptr);
    std::unique_ptr<Settings> settings = std::make_unique<Settings>();
    EditingBehavior behavior(settings->GetEditingBehaviorType());
    return behavior.InterpretKeyEvent(*keyboard_event);
  }

  WebKeyboardEvent CreateFakeKeyboardEvent(char key_code,
                                           int modifiers,
                                           WebInputEvent::Type type,
                                           const String& key = g_empty_string) {
    WebKeyboardEvent event(type, modifiers,
                           WebInputEvent::GetStaticTimeStampForTests());
    event.text[0] = key_code;
    event.windows_key_code = key_code;
    event.dom_key = ui::KeycodeConverter::KeyStringToDomKey(key.Utf8());
    return event;
  }

  // Like interpretKeyEvent, but with pressing down OSModifier+|keyCode|.
  // OSModifier is the platform's standard modifier key: control on most
  // platforms, but meta (command) on Mac.
  const char* InterpretOSModifierKeyPress(char key_code) {
#if defined(OS_MACOSX)
    WebInputEvent::Modifiers os_modifier = WebInputEvent::kMetaKey;
#else
    WebInputEvent::Modifiers os_modifier = WebInputEvent::kControlKey;
#endif
    return InterpretKeyEvent(CreateFakeKeyboardEvent(
        key_code, os_modifier, WebInputEvent::kRawKeyDown));
  }

  // Like interpretKeyEvent, but with pressing down ctrl+|keyCode|.
  const char* InterpretCtrlKeyPress(char key_code) {
    return InterpretKeyEvent(CreateFakeKeyboardEvent(
        key_code, WebInputEvent::kControlKey, WebInputEvent::kRawKeyDown));
  }

  // Like interpretKeyEvent, but with typing a tab.
  const char* InterpretTab(int modifiers) {
    return InterpretKeyEvent(
        CreateFakeKeyboardEvent('\t', modifiers, WebInputEvent::kChar));
  }

  // Like interpretKeyEvent, but with typing a newline.
  const char* InterpretNewLine(int modifiers) {
    return InterpretKeyEvent(
        CreateFakeKeyboardEvent('\r', modifiers, WebInputEvent::kChar));
  }

  const char* InterpretDomKey(const char* key) {
    return InterpretKeyEvent(CreateFakeKeyboardEvent(
        0, kNoModifiers, WebInputEvent::kRawKeyDown, key));
  }

  // A name for "no modifiers set".
  static const int kNoModifiers = 0;
};

TEST_F(KeyboardTest, TestCtrlReturn) {
  EXPECT_STREQ("InsertNewline", InterpretCtrlKeyPress(0xD));
}

TEST_F(KeyboardTest, TestOSModifierZ) {
#if !defined(OS_MACOSX)
  EXPECT_STREQ("Undo", InterpretOSModifierKeyPress('Z'));
#endif
}

TEST_F(KeyboardTest, TestOSModifierY) {
#if !defined(OS_MACOSX)
  EXPECT_STREQ("Redo", InterpretOSModifierKeyPress('Y'));
#endif
}

TEST_F(KeyboardTest, TestOSModifierA) {
#if !defined(OS_MACOSX)
  EXPECT_STREQ("SelectAll", InterpretOSModifierKeyPress('A'));
#endif
}

TEST_F(KeyboardTest, TestOSModifierX) {
#if !defined(OS_MACOSX)
  EXPECT_STREQ("Cut", InterpretOSModifierKeyPress('X'));
#endif
}

TEST_F(KeyboardTest, TestOSModifierC) {
#if !defined(OS_MACOSX)
  EXPECT_STREQ("Copy", InterpretOSModifierKeyPress('C'));
#endif
}

TEST_F(KeyboardTest, TestOSModifierV) {
#if !defined(OS_MACOSX)
  EXPECT_STREQ("Paste", InterpretOSModifierKeyPress('V'));
#endif
}

TEST_F(KeyboardTest, TestEscape) {
  const char* result = InterpretKeyEvent(CreateFakeKeyboardEvent(
      VKEY_ESCAPE, kNoModifiers, WebInputEvent::kRawKeyDown));
  EXPECT_STREQ("Cancel", result);
}

TEST_F(KeyboardTest, TestInsertTab) {
  EXPECT_STREQ("InsertTab", InterpretTab(kNoModifiers));
}

TEST_F(KeyboardTest, TestInsertBackTab) {
  EXPECT_STREQ("InsertBacktab", InterpretTab(WebInputEvent::kShiftKey));
}

TEST_F(KeyboardTest, TestInsertNewline) {
  EXPECT_STREQ("InsertNewline", InterpretNewLine(kNoModifiers));
}

TEST_F(KeyboardTest, TestInsertLineBreak) {
  EXPECT_STREQ("InsertLineBreak", InterpretNewLine(WebInputEvent::kShiftKey));
}

TEST_F(KeyboardTest, TestDomKeyMap) {
  struct TestCase {
    const char* key;
    const char* command;
  } kDomKeyTestCases[] = {
      {"Copy", "Copy"}, {"Cut", "Cut"}, {"Paste", "Paste"},
  };

  for (const auto& test_case : kDomKeyTestCases)
    EXPECT_STREQ(test_case.command, InterpretDomKey(test_case.key));
}

}  // namespace blink
