// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COCOA_SYSTEM_HOTKEY_MAP_H_
#define CONTENT_BROWSER_COCOA_SYSTEM_HOTKEY_MAP_H_

#import <Cocoa/Cocoa.h>

#include <vector>

#include "base/gtest_prod_util.h"
#include "content/common/content_export.h"

namespace content {

struct SystemHotkey;

// Maintains a listing of all macOS system hotkeys (e.g. cmd+`). These hotkeys
// should have higher priority than web content, so NSEvents that correspond to
// a system hotkey should not be passed to the renderer.
class CONTENT_EXPORT SystemHotkeyMap {
 public:
  SystemHotkeyMap();
  SystemHotkeyMap(SystemHotkeyMap&&);

  SystemHotkeyMap(const SystemHotkeyMap&) = delete;
  SystemHotkeyMap& operator=(const SystemHotkeyMap&) = delete;

  ~SystemHotkeyMap();

  // Parses the property list data commonly stored at
  // ~/Library/Preferences/com.apple.symbolichotkeys.plist
  // Returns false on encountering an irrecoverable error.
  // Can be called multiple times. Only the results from the most recent
  // invocation are stored.
  bool ParseDictionary(NSDictionary* dictionary);

  // Whether the event corresponds to a hotkey that has been reserved by the
  // system.
  bool IsEventReserved(NSEvent* event) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(SystemHotkeyMapTest, Parse);
  FRIEND_TEST_ALL_PREFIXES(SystemHotkeyMapTest, ParseCustomEntries);
  FRIEND_TEST_ALL_PREFIXES(SystemHotkeyMapTest, ParseMouse);
  FRIEND_TEST_ALL_PREFIXES(SystemHotkeyMapTest,
                           ReverseWindowCyclingHotkeyExists);
  FRIEND_TEST_ALL_PREFIXES(SystemHotkeyMapTest,
                           IgnoreUndocumentedShortcutEntries);

  // Whether the hotkey has been reserved by the user.
  bool IsHotkeyReserved(unsigned short key_code, NSUInteger modifiers) const;

  // Creates at least one record of a hotkey that is reserved by the user.
  // Certain system hotkeys automatically reserve multiple key combinations.
  void ReserveHotkey(unsigned short key_code,
                     NSUInteger modifiers,
                     NSString* system_hotkey_identifier);

  // Create a record of a hotkey that is reserved by the user.
  void ReserveHotkey(unsigned short key_code, NSUInteger modifiers);

  std::vector<SystemHotkey> system_hotkeys_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_COCOA_SYSTEM_HOTKEY_MAP_H_
