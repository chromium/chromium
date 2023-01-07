// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>

#include "base/files/file_path.h"
#import "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/path_service.h"
#import "content/browser/cocoa/system_hotkey_map.h"
#include "content/public/common/content_paths.h"

namespace content {

class SystemHotkeyMapTest : public ::testing::Test {
 protected:
  SystemHotkeyMapTest() {}

  NSDictionary* DictionaryFromTestFile(const char* file) {
    base::FilePath test_data_dir;
    bool result = base::PathService::Get(DIR_TEST_DATA, &test_data_dir);
    if (!result)
      return nil;

    base::FilePath test_path = test_data_dir.AppendASCII(file);
    return [NSDictionary
        dictionaryWithContentsOfURL:base::mac::FilePathToNSURL(test_path)];
  }

  void AddEntryToDictionary(BOOL enabled,
                            unsigned short key_code,
                            NSUInteger modifiers) {
    NSMutableArray* parameters = [NSMutableArray array];
    // The first parameter is unused.
    [parameters addObject:[NSNumber numberWithInt:65535]];
    [parameters addObject:[NSNumber numberWithUnsignedShort:key_code]];
    [parameters addObject:[NSNumber numberWithUnsignedInteger:modifiers]];

    NSMutableDictionary* value_dictionary = [NSMutableDictionary dictionary];
    [value_dictionary setObject:parameters forKey:@"parameters"];
    [value_dictionary setObject:@"standard" forKey:@"type"];

    NSMutableDictionary* outer_dictionary = [NSMutableDictionary dictionary];
    [outer_dictionary setObject:value_dictionary forKey:@"value"];

    NSNumber* enabled_number = [NSNumber numberWithBool:enabled];
    [outer_dictionary setObject:enabled_number forKey:@"enabled"];

    NSString* key = [NSString stringWithFormat:@"%d", count_];
    [system_hotkey_inner_dictionary_ setObject:outer_dictionary forKey:key];
    ++count_;
  }

  void SetUp() override {
    system_hotkey_dictionary_.reset([[NSMutableDictionary alloc] init]);
    system_hotkey_inner_dictionary_.reset([[NSMutableDictionary alloc] init]);
    [system_hotkey_dictionary_ setObject:system_hotkey_inner_dictionary_
                                  forKey:@"AppleSymbolicHotKeys"];
    count_ = 100;
  }

  void TearDown() override {
    system_hotkey_dictionary_.reset();
    system_hotkey_inner_dictionary_.reset();
  }

  // A constructed dictionary that matches the format of the one that would be
  // parsed from the system hotkeys plist.
  base::scoped_nsobject<NSMutableDictionary> system_hotkey_dictionary_;

 private:
  // A reference to the mutable dictionary to which new entries are added.
  base::scoped_nsobject<NSMutableDictionary> system_hotkey_inner_dictionary_;
  // Each entry in the system_hotkey_inner_dictionary_ needs to have a unique
  // key. This count is used to generate those unique keys.
  int count_;
};

TEST_F(SystemHotkeyMapTest, Parse) {
  // This plist was pulled from a real machine. It is extensively populated,
  // and has no missing or incomplete entries.
  NSDictionary* dictionary =
      DictionaryFromTestFile("mac/mac_system_hotkeys.plist");
  ASSERT_TRUE(dictionary);

  SystemHotkeyMap map;
  bool result = map.ParseDictionary(dictionary);
  EXPECT_TRUE(result);

  // Command + ` is a common key binding. It should exist.
  unsigned short key_code = kVK_ANSI_Grave;
  NSUInteger modifiers = NSEventModifierFlagCommand;
  EXPECT_TRUE(map.IsHotkeyReserved(key_code, modifiers));

  // Command + Shift + ` is a common key binding. It should exist.
  modifiers = NSEventModifierFlagCommand | NSEventModifierFlagShift;
  EXPECT_TRUE(map.IsHotkeyReserved(key_code, modifiers));

  // Command + Shift + Ctr + ` is not a common key binding.
  modifiers = NSEventModifierFlagCommand | NSEventModifierFlagShift |
              NSEventModifierFlagControl;
  EXPECT_FALSE(map.IsHotkeyReserved(key_code, modifiers));

  // Command + L is not a common key binding.
  key_code = kVK_ANSI_L;
  modifiers = NSEventModifierFlagCommand;
  EXPECT_FALSE(map.IsHotkeyReserved(key_code, modifiers));
}

TEST_F(SystemHotkeyMapTest, ParseNil) {
  NSDictionary* dictionary = nil;

  SystemHotkeyMap map;
  bool result = map.ParseDictionary(dictionary);
  EXPECT_FALSE(result);
}

TEST_F(SystemHotkeyMapTest, ParseMouse) {
  // This plist was pulled from a real machine. It has missing entries,
  // incomplete entries, and mouse hotkeys.
  NSDictionary* dictionary =
      DictionaryFromTestFile("mac/mac_system_hotkeys_sparse.plist");
  ASSERT_TRUE(dictionary);

  SystemHotkeyMap map;
  bool result = map.ParseDictionary(dictionary);
  EXPECT_TRUE(result);

  // Command + ` is a common key binding. It is missing, but since OS X uses the
  // default value the hotkey should still be reserved.
  // https://crbug.com/383558
  // https://crbug.com/145062
  unsigned short key_code = kVK_ANSI_Grave;
  NSUInteger modifiers = NSEventModifierFlagCommand;
  EXPECT_TRUE(map.IsHotkeyReserved(key_code, modifiers));

  // There is a mouse keybinding for 0x08. It should not apply to keyboard
  // hotkeys.
  key_code = kVK_ANSI_C;
  modifiers = 0;
  EXPECT_FALSE(map.IsHotkeyReserved(key_code, modifiers));

  // Command + Alt + = is an accessibility shortcut. Its entry in the plist is
  // incomplete.
  // TODO(erikchen): OSX uses the default bindings, so this hotkey should still
  // be reserved.
  // http://crbug.com/383558
  key_code = kVK_ANSI_Equal;
  modifiers = NSEventModifierFlagCommand | NSEventModifierFlagOption;
  EXPECT_FALSE(map.IsHotkeyReserved(key_code, modifiers));
}

TEST_F(SystemHotkeyMapTest, ParseCustomEntries) {
  unsigned short key_code = kVK_ANSI_C;

  AddEntryToDictionary(YES, key_code, 0);
  AddEntryToDictionary(YES, key_code, NSEventModifierFlagCapsLock);
  AddEntryToDictionary(YES, key_code, NSEventModifierFlagShift);
  AddEntryToDictionary(YES, key_code, NSEventModifierFlagControl);
  AddEntryToDictionary(YES, key_code, NSEventModifierFlagFunction);
  AddEntryToDictionary(
      YES, key_code, NSEventModifierFlagFunction | NSEventModifierFlagControl);
  AddEntryToDictionary(NO, key_code, NSEventModifierFlagOption);

  SystemHotkeyMap map;

  bool result = map.ParseDictionary(system_hotkey_dictionary_.get());
  EXPECT_TRUE(result);

  // Entries without control, command, or alternate key mask should not be
  // reserved.
  EXPECT_FALSE(map.IsHotkeyReserved(key_code, 0));
  EXPECT_FALSE(map.IsHotkeyReserved(key_code, NSEventModifierFlagCapsLock));
  EXPECT_FALSE(map.IsHotkeyReserved(key_code, NSEventModifierFlagShift));
  EXPECT_FALSE(map.IsHotkeyReserved(key_code, NSEventModifierFlagFunction));

  // Unlisted entries should not be reserved.
  EXPECT_FALSE(map.IsHotkeyReserved(key_code, NSEventModifierFlagCommand));

  // Disabled entries should not be reserved.
  EXPECT_FALSE(map.IsHotkeyReserved(key_code, NSEventModifierFlagOption));

  // Other entries should be reserved.
  EXPECT_TRUE(map.IsHotkeyReserved(key_code, NSEventModifierFlagControl));
  EXPECT_TRUE(map.IsHotkeyReserved(
      key_code, NSEventModifierFlagFunction | NSEventModifierFlagControl));
}

}  // namespace content
