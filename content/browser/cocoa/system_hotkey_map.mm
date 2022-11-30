// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/cocoa/system_hotkey_map.h"

#import <Carbon/Carbon.h>

#include "base/mac/scoped_nsobject.h"

#pragma mark - NSDictionary Helper Functions

namespace {

// All 4 following functions return nil if the object doesn't exist, or isn't of
// the right class.
id ObjectForKey(NSDictionary* dict, NSString* key, Class aClass) {
  id object = [dict objectForKey:key];
  if (![object isKindOfClass:aClass])
    return nil;
  return object;
}

NSDictionary* DictionaryForKey(NSDictionary* dict, NSString* key) {
  return ObjectForKey(dict, key, [NSDictionary class]);
}

NSArray* ArrayForKey(NSDictionary* dict, NSString* key) {
  return ObjectForKey(dict, key, [NSArray class]);
}

NSNumber* NumberForKey(NSDictionary* dict, NSString* key) {
  return ObjectForKey(dict, key, [NSNumber class]);
}

NSString* StringForKey(NSDictionary* dict, NSString* key) {
  return ObjectForKey(dict, key, [NSString class]);
}

}  // namespace

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

  if (!dictionary)
    return false;

  NSDictionary* user_hotkey_dictionaries =
      DictionaryForKey(dictionary, @"AppleSymbolicHotKeys");
  if (!user_hotkey_dictionaries)
    return false;

  // Start with a dictionary of default OS X hotkeys that are not necessarily
  // listed in com.apple.symbolichotkeys.plist, but should still be handled as
  // reserved.
  // If the user has overridden or disabled any of these hotkeys,
  // -NSMutableDictionary addEntriesFromDictionary:] will ensure that the new
  // values are used.
  // See https://crbug.com/145062#c8
  base::scoped_nsobject<NSMutableDictionary> hotkey_dictionaries([@{
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
  } mutableCopy]);
  [hotkey_dictionaries addEntriesFromDictionary:user_hotkey_dictionaries];

  for (NSString* hotkey_system_effect in [hotkey_dictionaries allKeys]) {
    if (![hotkey_system_effect isKindOfClass:[NSString class]])
      continue;

    NSDictionary* hotkey_dictionary =
        [hotkey_dictionaries objectForKey:hotkey_system_effect];
    if (![hotkey_dictionary isKindOfClass:[NSDictionary class]])
      continue;

    NSNumber* enabled = NumberForKey(hotkey_dictionary, @"enabled");
    if (!enabled || enabled.boolValue == NO)
      continue;

    NSDictionary* value = DictionaryForKey(hotkey_dictionary, @"value");
    if (!value)
      continue;

    NSString* type = StringForKey(value, @"type");
    if (!type || ![type isEqualToString:@"standard"])
      continue;

    NSArray* parameters = ArrayForKey(value, @"parameters");
    if (!parameters || [parameters count] != 3)
      continue;

    NSNumber* key_code = [parameters objectAtIndex:1];
    if (![key_code isKindOfClass:[NSNumber class]])
      continue;

    NSNumber* modifiers = [parameters objectAtIndex:2];
    if (![modifiers isKindOfClass:[NSNumber class]])
      continue;

    ReserveHotkey(key_code.unsignedShortValue,
                  modifiers.unsignedIntegerValue,
                  hotkey_system_effect);
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
    if (it->key_code == key_code && it->modifiers == modifiers)
      return true;
  }
  return false;
}

void SystemHotkeyMap::ReserveHotkey(unsigned short key_code,
                                    NSUInteger modifiers,
                                    NSString* system_effect) {
  ReserveHotkey(key_code, modifiers);

  // If a hotkey exists for toggling through the windows of an application, then
  // adding shift to that hotkey toggles through the windows backwards.
  if ([system_effect isEqualToString:@"27"])
    ReserveHotkey(key_code, modifiers | NSEventModifierFlagShift);
}

void SystemHotkeyMap::ReserveHotkey(unsigned short key_code,
                                    NSUInteger modifiers) {
  // Hotkeys require at least one of control, command, or alternate keys to be
  // down.
  NSUInteger required_modifiers = NSEventModifierFlagControl |
                                  NSEventModifierFlagCommand |
                                  NSEventModifierFlagOption;
  if ((modifiers & required_modifiers) == 0)
    return;

  SystemHotkey hotkey;
  hotkey.key_code = key_code;
  hotkey.modifiers = modifiers;
  system_hotkeys_.push_back(hotkey);
}

}  // namespace content
