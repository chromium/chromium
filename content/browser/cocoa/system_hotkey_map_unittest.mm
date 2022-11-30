// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#import "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/path_service.h"
#include "base/test/task_environment.h"
#include "content/browser/cocoa/system_hotkey_helper_mac.h"
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
                            NSUInteger modifiers,
                            int index = 0) {
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

    NSString* key = index ? [NSString stringWithFormat:@"%d", index]
                          : [NSString stringWithFormat:@"%d", count_];

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

// Tests that we skip over certain undocumented shortcut entries that appear in
// AppleSymbolicHotKeys. See https://crbug.com/874219 .
TEST_F(SystemHotkeyMapTest, IgnoreUndocumentedShortcutEntries) {
  const int kSpacesLeftIndex = 79;
  const int kShiftedSpacesLeftIndex = 80;
  const int kSpacesRightIndex = 81;
  const int kShiftedSpacesRightIndex = 82;
  AddEntryToDictionary(YES, kVK_ANSI_A, NSEventModifierFlagControl,
                       kSpacesLeftIndex);
  AddEntryToDictionary(YES, kVK_ANSI_B, NSEventModifierFlagControl,
                       kShiftedSpacesLeftIndex);
  AddEntryToDictionary(YES, kVK_ANSI_C, NSEventModifierFlagControl,
                       kSpacesRightIndex);
  AddEntryToDictionary(YES, kVK_ANSI_D, NSEventModifierFlagControl,
                       kShiftedSpacesRightIndex);

  SystemHotkeyMap map;

  bool result = map.ParseDictionary(system_hotkey_dictionary_.get());
  EXPECT_TRUE(result);

  EXPECT_TRUE(map.IsHotkeyReserved(kVK_ANSI_A, NSEventModifierFlagControl));
  EXPECT_FALSE(map.IsHotkeyReserved(kVK_ANSI_B, NSEventModifierFlagControl));
  EXPECT_TRUE(map.IsHotkeyReserved(kVK_ANSI_C, NSEventModifierFlagControl));
  EXPECT_FALSE(map.IsHotkeyReserved(kVK_ANSI_D, NSEventModifierFlagControl));
}

// Tests that the SystemHotkeyMap gets flushed when the user switches away and
// back to Chrome.
TEST_F(SystemHotkeyMapTest, ReloadShortcutsAfterAppActivation) {
  base::test::TaskEnvironment task_environment;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  constexpr auto* kSystemHotkeyPlistPath = "com.apple.symbolichotkeys.plist";
  base::FilePath test_path =
      temp_dir.GetPath().AppendASCII(kSystemHotkeyPlistPath);
  SetSystemHotkeyPlistPathForTesting(test_path);

  // Create an entry for symbolic hotkey number 79.
  const NSString* kTypeKey = @"type";
  const NSString* kStandardValue = @"standard";
  const NSString* kParametersKey = @"parameters";
  NSArray* parameters = @[ @65535, @123, @10747904 ];
  NSDictionary* values =
      @{kTypeKey : kStandardValue, kParametersKey : parameters};

  const NSString* kEnabledKey = @"enabled";
  const NSString* kValueKey = @"value";
  NSDictionary* entrySeventyNine = @{kEnabledKey : @YES, kValueKey : values};

  const NSString* kSeventyNineKey = @"79";
  NSDictionary* symbolic_hotkeys = @{kSeventyNineKey : entrySeventyNine};

  const NSString* kAppleSymbolicHotKeysKey = @"AppleSymbolicHotKeys";
  NSDictionary* root = @{kAppleSymbolicHotKeysKey : symbolic_hotkeys};

  auto* hotkey_plist_url = base::mac::FilePathToNSURL(test_path);
  [root writeToURL:hotkey_plist_url error:nullptr];

  // Prepare to respond to filesystem changes.
  WaitForEventsForTesting();

  // Make sure we load what we expect from the hotkey file.
  EXPECT_TRUE(GetSystemHotkeyMap()->IsHotkeyReserved(123, 10747904));

  // Pause to let events propagate.
  WaitForEventsForTesting();

  // Change the parameters of hotkey number 79.
  parameters = @[ @65535, @321, @10747904 ];
  values = @{kTypeKey : kStandardValue, kParametersKey : parameters};
  entrySeventyNine = @{kEnabledKey : @YES, kValueKey : values};
  symbolic_hotkeys = @{kSeventyNineKey : entrySeventyNine};
  root = @{kAppleSymbolicHotKeysKey : symbolic_hotkeys};
  [root writeToURL:hotkey_plist_url error:nullptr];

  // Wait for changes to be noticed.
  WaitForEventsForTesting();

  // Expect that the old parameters no longer exist, in favor of the new ones.
  EXPECT_FALSE(GetSystemHotkeyMap()->IsHotkeyReserved(123, 10747904));
  EXPECT_TRUE(GetSystemHotkeyMap()->IsHotkeyReserved(321, 10747904));
}

}  // namespace content
