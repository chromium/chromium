/*
 * Copyright (C) 2006, 2007 Apple, Inc.  All rights reserved.
 * Copyright (C) 2012 Google, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/editing/editing_behavior.h"

#include "build/build_config.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

namespace {

//
// The below code was adapted from the WebKit file webview.cpp
//

const unsigned kCtrlKey = WebInputEvent::kControlKey;
const unsigned kAltKey = WebInputEvent::kAltKey;
const unsigned kShiftKey = WebInputEvent::kShiftKey;
const unsigned kMetaKey = WebInputEvent::kMetaKey;
#if defined(OS_MACOSX)
// Aliases for the generic key defintions to make kbd shortcuts definitions more
// readable on OS X.
const unsigned kOptionKey = kAltKey;

// Do not use this constant for anything but cursor movement commands. Keys
// with cmd set have their |isSystemKey| bit set, so chances are the shortcut
// will not be executed. Another, less important, reason is that shortcuts
// defined in the layoutObject do not blink the menu item that they triggered.
// See http://crbug.com/25856 and the bugs linked from there for details.
const unsigned kCommandKey = kMetaKey;
#endif

// Keys with special meaning. These will be delegated to the editor using
// the execCommand() method
struct KeyboardCodeKeyDownEntry {
  unsigned virtual_key;
  unsigned modifiers;
  const char* name;
};

struct KeyboardCodeKeyPressEntry {
  unsigned char_code;
  unsigned modifiers;
  const char* name;
};

// DomKey has a broader range than KeyboardCode, we need DomKey to handle some
// special keys.
// Note: We cannot use DomKey for printable keys since it may vary based on
// locale.
struct DomKeyKeyDownEntry {
  const char* key;
  unsigned modifiers;
  const char* name;
};

// Key bindings with command key on Mac and alt key on other platforms are
// marked as system key events and will be ignored (with the exception
// of Command-B and Command-I) so they shouldn't be added here.
const KeyboardCodeKeyDownEntry kKeyboardCodeKeyDownEntries[] = {
    {VKEY_LEFT, 0, "MoveLeft"},
    {VKEY_LEFT, kShiftKey, "MoveLeftAndModifySelection"},
#if defined(OS_MACOSX)
    {VKEY_LEFT, kOptionKey, "MoveWordLeft"},
    {VKEY_LEFT, kOptionKey | kShiftKey, "MoveWordLeftAndModifySelection"},
#else
    {VKEY_LEFT, kCtrlKey, "MoveWordLeft"},
    {VKEY_LEFT, kCtrlKey | kShiftKey, "MoveWordLeftAndModifySelection"},
#endif
    {VKEY_RIGHT, 0, "MoveRight"},
    {VKEY_RIGHT, kShiftKey, "MoveRightAndModifySelection"},
#if defined(OS_MACOSX)
    {VKEY_RIGHT, kOptionKey, "MoveWordRight"},
    {VKEY_RIGHT, kOptionKey | kShiftKey, "MoveWordRightAndModifySelection"},
#else
    {VKEY_RIGHT, kCtrlKey, "MoveWordRight"},
    {VKEY_RIGHT, kCtrlKey | kShiftKey, "MoveWordRightAndModifySelection"},
#endif
    {VKEY_UP, 0, "MoveUp"},
    {VKEY_UP, kShiftKey, "MoveUpAndModifySelection"},
    {VKEY_PRIOR, kShiftKey, "MovePageUpAndModifySelection"},
    {VKEY_DOWN, 0, "MoveDown"},
    {VKEY_DOWN, kShiftKey, "MoveDownAndModifySelection"},
    {VKEY_NEXT, kShiftKey, "MovePageDownAndModifySelection"},
#if !defined(OS_MACOSX)
    {VKEY_UP, kCtrlKey, "MoveParagraphBackward"},
    {VKEY_UP, kCtrlKey | kShiftKey, "MoveParagraphBackwardAndModifySelection"},
    {VKEY_DOWN, kCtrlKey, "MoveParagraphForward"},
    {VKEY_DOWN, kCtrlKey | kShiftKey, "MoveParagraphForwardAndModifySelection"},
    {VKEY_PRIOR, 0, "MovePageUp"},
    {VKEY_NEXT, 0, "MovePageDown"},
#endif
    {VKEY_HOME, 0, "MoveToBeginningOfLine"},
    {VKEY_HOME, kShiftKey, "MoveToBeginningOfLineAndModifySelection"},
#if defined(OS_MACOSX)
    {VKEY_PRIOR, kOptionKey, "MovePageUp"},
    {VKEY_NEXT, kOptionKey, "MovePageDown"},
#endif
#if !defined(OS_MACOSX)
    {VKEY_HOME, kCtrlKey, "MoveToBeginningOfDocument"},
    {VKEY_HOME, kCtrlKey | kShiftKey,
     "MoveToBeginningOfDocumentAndModifySelection"},
#endif
    {VKEY_END, 0, "MoveToEndOfLine"},
    {VKEY_END, kShiftKey, "MoveToEndOfLineAndModifySelection"},
#if !defined(OS_MACOSX)
    {VKEY_END, kCtrlKey, "MoveToEndOfDocument"},
    {VKEY_END, kCtrlKey | kShiftKey, "MoveToEndOfDocumentAndModifySelection"},
#endif
    {VKEY_BACK, 0, "DeleteBackward"},
    {VKEY_BACK, kShiftKey, "DeleteBackward"},
    {VKEY_DELETE, 0, "DeleteForward"},
#if defined(OS_MACOSX)
    {VKEY_BACK, kOptionKey, "DeleteWordBackward"},
    {VKEY_DELETE, kOptionKey, "DeleteWordForward"},
#else
    {VKEY_BACK, kCtrlKey, "DeleteWordBackward"},
    {VKEY_DELETE, kCtrlKey, "DeleteWordForward"},
#endif
#if defined(OS_MACOSX)
    {'B', kCommandKey, "ToggleBold"},
    {'I', kCommandKey, "ToggleItalic"},
#else
    {'B', kCtrlKey, "ToggleBold"},
    {'I', kCtrlKey, "ToggleItalic"},
#endif
    {'U', kCtrlKey, "ToggleUnderline"},
    {VKEY_ESCAPE, 0, "Cancel"},
    {VKEY_OEM_PERIOD, kCtrlKey, "Cancel"},
    {VKEY_TAB, 0, "InsertTab"},
    {VKEY_TAB, kShiftKey, "InsertBacktab"},
    {VKEY_RETURN, 0, "InsertNewline"},
    {VKEY_RETURN, kCtrlKey, "InsertNewline"},
    {VKEY_RETURN, kAltKey, "InsertNewline"},
    {VKEY_RETURN, kAltKey | kShiftKey, "InsertNewline"},
    {VKEY_RETURN, kShiftKey, "InsertLineBreak"},
    {VKEY_INSERT, kCtrlKey, "Copy"},
    {VKEY_INSERT, kShiftKey, "Paste"},
    {VKEY_DELETE, kShiftKey, "Cut"},
#if !defined(OS_MACOSX)
    // On OS X, we pipe these back to the browser, so that it can do menu item
    // blinking.
    {'C', kCtrlKey, "Copy"},
    {'V', kCtrlKey, "Paste"},
    {'V', kCtrlKey | kShiftKey, "PasteAndMatchStyle"},
    {'X', kCtrlKey, "Cut"},
    {'A', kCtrlKey, "SelectAll"},
    {'Z', kCtrlKey, "Undo"},
    {'Z', kCtrlKey | kShiftKey, "Redo"},
    {'Y', kCtrlKey, "Redo"},
#endif
    {VKEY_INSERT, 0, "OverWrite"},
};

const KeyboardCodeKeyPressEntry kKeyboardCodeKeyPressEntries[] = {
    {'\t', 0, "InsertTab"},
    {'\t', kShiftKey, "InsertBacktab"},
    {'\r', 0, "InsertNewline"},
    {'\r', kShiftKey, "InsertLineBreak"},
};

const DomKeyKeyDownEntry kDomKeyKeyDownEntries[] = {
    {"Copy", 0, "Copy"},
    {"Cut", 0, "Cut"},
    {"Paste", 0, "Paste"},
};

const char* LookupCommandNameFromDomKeyKeyDown(const String& key,
                                               unsigned modifiers) {
  // This table is not likely to grow, so sequential search is fine here.
  for (const auto& entry : kDomKeyKeyDownEntries) {
    if (key == entry.key && modifiers == entry.modifiers)
      return entry.name;
  }
  return nullptr;
}

}  // anonymous namespace

const char* EditingBehavior::InterpretKeyEvent(
    const KeyboardEvent& event) const {
  const WebKeyboardEvent* key_event = event.KeyEvent();
  if (!key_event)
    return "";

  static HashMap<int, const char*>* key_down_commands_map = nullptr;
  static HashMap<int, const char*>* key_press_commands_map = nullptr;

  if (!key_down_commands_map) {
    key_down_commands_map = new HashMap<int, const char*>;
    key_press_commands_map = new HashMap<int, const char*>;

    for (const auto& entry : kKeyboardCodeKeyDownEntries) {
      key_down_commands_map->Set(entry.modifiers << 16 | entry.virtual_key,
                                 entry.name);
    }

    for (const auto& entry : kKeyboardCodeKeyPressEntries) {
      key_press_commands_map->Set(entry.modifiers << 16 | entry.char_code,
                                  entry.name);
    }
  }

  unsigned modifiers =
      key_event->GetModifiers() & (kShiftKey | kAltKey | kCtrlKey | kMetaKey);

  if (key_event->GetType() == WebInputEvent::kRawKeyDown) {
    int map_key = modifiers << 16 | event.keyCode();
    const char* name = map_key ? key_down_commands_map->at(map_key) : nullptr;
    if (!name)
      name = LookupCommandNameFromDomKeyKeyDown(event.key(), modifiers);
    return name;
  }

  int map_key = modifiers << 16 | event.charCode();
  return map_key ? key_press_commands_map->at(map_key) : nullptr;
}

bool EditingBehavior::ShouldInsertCharacter(const KeyboardEvent& event) const {
  if (event.KeyEvent()->text[1] != 0)
    return true;

  // On Gtk/Linux, it emits key events with ASCII text and ctrl on for ctrl-<x>.
  // In Webkit, EditorClient::handleKeyboardEvent in
  // WebKit/gtk/WebCoreSupport/EditorClientGtk.cpp drop such events.
  // On Mac, it emits key events with ASCII text and meta on for Command-<x>.
  // These key events should not emit text insert event.
  // Alt key would be used to insert alternative character, so we should let
  // through. Also note that Ctrl-Alt combination equals to AltGr key which is
  // also used to insert alternative character.
  // http://code.google.com/p/chromium/issues/detail?id=10846
  // Windows sets both alt and meta are on when "Alt" key pressed.
  // http://code.google.com/p/chromium/issues/detail?id=2215
  // Also, we should not rely on an assumption that keyboards don't
  // send ASCII characters when pressing a control key on Windows,
  // which may be configured to do it so by user.
  // See also http://en.wikipedia.org/wiki/Keyboard_Layout
  // FIXME(ukai): investigate more detail for various keyboard layout.
  UChar ch = event.KeyEvent()->text[0U];

  // Don't insert null or control characters as they can result in
  // unexpected behaviour
  if (ch < ' ')
    return false;
#if defined(OS_LINUX)
  // According to XKB map no keyboard combinations with ctrl key are mapped to
  // printable characters, however we need the filter as the DomKey/text could
  // contain printable characters.
  if (event.ctrlKey())
    return false;
#elif !defined(OS_WIN)
  // Don't insert ASCII character if ctrl w/o alt or meta is on.
  // On Mac, we should ignore events when meta is on (Command-<x>).
  if (ch < 0x80) {
    if (event.ctrlKey() && !event.altKey())
      return false;
#if defined(OS_MACOSX)
    if (event.metaKey())
      return false;
#endif
  }
#endif

  return true;
}

STATIC_ASSERT_ENUM(WebSettings::EditingBehavior::kMac, kEditingMacBehavior);
STATIC_ASSERT_ENUM(WebSettings::EditingBehavior::kWin, kEditingWindowsBehavior);
STATIC_ASSERT_ENUM(WebSettings::EditingBehavior::kUnix, kEditingUnixBehavior);
STATIC_ASSERT_ENUM(WebSettings::EditingBehavior::kAndroid,
                   kEditingAndroidBehavior);

}  // namespace blink
