// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/cocoa/system_hotkey_map.h"

#import <Carbon/Carbon.h>

#include "base/apple/foundation_util.h"

#pragma mark - SystemHotkey

namespace content {

struct SystemHotkey {
  unsigned short key_code;
  NSUInteger modifiers;
};

#pragma mark - SystemHotkeyMap

SystemHotkeyMap::SystemHotkeyMap() = default;
SystemHotkeyMap::SystemHotkeyMap(SystemHotkeyMap&&) = default;
SystemHotkeyMap::~SystemHotkeyMap() = default;

bool SystemHotkeyMap::ParseDictionary(NSDictionary* dictionary) {
  system_hotkeys_.clear();

  if (!dictionary) {
    return false;
  }

  NSDictionary* user_hotkey_dictionaries =
      base::apple::ObjCCast<NSDictionary>(dictionary[@"AppleSymbolicHotKeys"]);
  if (!user_hotkey_dictionaries) {
    return false;
  }

  // Start with a dictionary of default macOS hotkeys that are not necessarily
  // listed in com.apple.symbolichotkeys.plist, but should still be handled as
  // reserved.
  // If the user has overridden or disabled any of these hotkeys,
  // [NSMutableDictionary addEntriesFromDictionary:] will ensure that the new
  // values are used.
  // See https://crbug.com/145062#c8
  NSMutableDictionary* hotkey_dictionaries = [@{
    // Default Window switch key binding: Command + `
    // Note: The first parameter @96 is not used by |SystemHotkeyMap|.
    @"27" : @{
      @"enabled" : @YES,
      @"value" : @{
        @"type" : @"standard",
        @"parameters" : @[
          @96 /* unused */, @(kVK_ANSI_Grave), @(NSEventModifierFlagCommand)
        ],
      }
    }
  } mutableCopy];
  [hotkey_dictionaries addEntriesFromDictionary:user_hotkey_dictionaries];

  // The meanings of the keys in `user_hotkey_dictionaries` are listed here:
  // https://web.archive.org/web/20141112224103/http://hintsforums.macworld.com/showthread.php?t=114785
  // Of particular interest are the following:
  //
  // # Spaces Left - Control, Left
  // 79 = { enabled = 1; ... };
  //
  // # Spaces Right - Control, Right
  // 81 = { enabled = 1; ... };
  //
  // Apparently, when you change the shortcuts for Spaces Left/Right, macOS
  // also inserts entries at slots 80 and 82 which differ from the previous
  // entries by the addition of the Shift key. This is similar to entries 60
  // and 61 as documented in the web page above where Command, Option, Space
  // cycles to the previous input source and Command, Option, Space, Shift
  // cycles to the next. This approach doesn't make sense for moving between
  // Spaces using the arrow keys. Maybe there's legacy machinery in the AppKit
  // that automatically creates the shifted versions and macOS knows to ignore
  // them (not expecting any non-system applications to read this file).
  //
  // Treating these shortcuts as valid results in unexpected behavior. For
  // example, in the case of "Spaces Left" being mapped to Option Left Arrow,
  // Chrome would silently ignore Shift-Option Left Arrow, the shortcut which
  // appends the current text selection by one word to the left. To avoid
  // this, we'll ignore these two undocumented shortcuts.
  // See https://crbug.com/874219 .
  const NSString* kSpacesLeftShiftedHotkeyId = @"80";
  const NSString* kSpacesRightShiftedHotkeyId = @"82";
  [hotkey_dictionaries removeObjectForKey:kSpacesLeftShiftedHotkeyId];
  [hotkey_dictionaries removeObjectForKey:kSpacesRightShiftedHotkeyId];

  for (NSString* system_hotkey_identifier in [hotkey_dictionaries allKeys]) {
    if (![system_hotkey_identifier isKindOfClass:[NSString class]]) {
      continue;
    }

    NSDictionary* hotkey_dictionary = base::apple::ObjCCast<NSDictionary>(
        hotkey_dictionaries[system_hotkey_identifier]);
    if (!hotkey_dictionary) {
      continue;
    }

    NSNumber* enabled =
        base::apple::ObjCCast<NSNumber>(hotkey_dictionary[@"enabled"]);
    if (!enabled.boolValue) {
      continue;
    }

    NSDictionary* value =
        base::apple::ObjCCast<NSDictionary>(hotkey_dictionary[@"value"]);
    if (!value) {
      continue;
    }

    NSString* type = base::apple::ObjCCast<NSString>(value[@"type"]);
    if (![type isEqualToString:@"standard"]) {
      continue;
    }

    NSArray* parameters = base::apple::ObjCCast<NSArray>(value[@"parameters"]);
    if (parameters.count != 3) {
      continue;
    }

    const int kKeyCodeIndex = 1;
    NSNumber* key_code =
        base::apple::ObjCCast<NSNumber>(parameters[kKeyCodeIndex]);
    if (!key_code) {
      continue;
    }

    const int kModifierIndex = 2;
    NSNumber* modifiers =
        base::apple::ObjCCast<NSNumber>(parameters[kModifierIndex]);
    if (!modifiers) {
      continue;
    }

    ReserveHotkey(key_code.unsignedShortValue, modifiers.unsignedIntegerValue,
                  system_hotkey_identifier);
  }

  return true;
}

bool SystemHotkeyMap::IsEventReserved(NSEvent* event) const {
  return IsHotkeyReserved(event.keyCode, event.modifierFlags);
}

bool SystemHotkeyMap::IsHotkeyReserved(unsigned short key_code,
                                       NSUInteger modifiers) const {
  modifiers &= NSEventModifierFlagDeviceIndependentFlagsMask;
  std::vector<SystemHotkey>::const_iterator it;
  for (it = system_hotkeys_.begin(); it != system_hotkeys_.end(); ++it) {
    if (it->key_code == key_code && it->modifiers == modifiers) {
      return true;
    }
  }
  return false;
}

void SystemHotkeyMap::ReserveHotkey(unsigned short key_code,
                                    NSUInteger modifiers,
                                    NSString* system_hotkey_identifier) {
  ReserveHotkey(key_code, modifiers);

  // If a hotkey exists for cycling through the windows of an application, then
  // adding shift to that hotkey cycles through the windows backwards.
  NSString* kCycleThroughWindowsHotkeyId = @"27";
  const NSUInteger kCycleBackwardsModifier =
      modifiers | NSEventModifierFlagShift;
  if ([system_hotkey_identifier isEqualToString:kCycleThroughWindowsHotkeyId] &&
      modifiers != kCycleBackwardsModifier) {
    ReserveHotkey(key_code, kCycleBackwardsModifier);
  }
}

void SystemHotkeyMap::ReserveHotkey(unsigned short key_code,
                                    NSUInteger modifiers) {
  // Hotkeys require at least one of control, command, or alternate keys to be
  // down.
  NSUInteger required_modifiers = NSEventModifierFlagControl |
                                  NSEventModifierFlagCommand |
                                  NSEventModifierFlagOption;
  if ((modifiers & required_modifiers) == 0) {
    return;
  }

  SystemHotkey hotkey;
  hotkey.key_code = key_code;
  hotkey.modifiers = modifiers;
  system_hotkeys_.push_back(hotkey);
}

}  // namespace content
