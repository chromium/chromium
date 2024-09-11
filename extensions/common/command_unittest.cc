// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "extensions/common/command.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/android_buildflags.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

using CommandTest = testing::Test;

struct ConstCommandsTestData {
  bool expected_result;
  ui::Accelerator accelerator;
  const char* command_name;
  const char* key;
  const char* description;
};

// Checks the |suggested_key| value parses into a command when specified as a
// string or dictionary of platform specific keys. If
// |platform_specific_only| is true, only the latter is tested. |platforms|
// specifies all platforms to use when populating the |suggested_key|
// dictionary.
void CheckParse(const ConstCommandsTestData& data,
                int i,
                bool platform_specific_only,
                std::vector<std::string>& platforms) {
  SCOPED_TRACE(std::string("Command name: |") + data.command_name + "| key: |" +
               data.key + "| description: |" + data.description +
               "| index: " + base::NumberToString(i));

  extensions::Command command;
  base::Value::Dict input;
  std::u16string error;

  // First, test the parse of a string suggested_key value.
  input.Set("suggested_key", data.key);
  input.Set("description", data.description);

  if (!platform_specific_only) {
    bool result = command.Parse(input, data.command_name, i, &error);
    EXPECT_EQ(data.expected_result, result);
    if (result) {
      EXPECT_STREQ(data.description,
                   base::UTF16ToASCII(command.description()).c_str());
      EXPECT_STREQ(data.command_name, command.command_name().c_str());
      EXPECT_EQ(data.accelerator, command.accelerator());
    }
  }

  // Now, test the parse of a platform dictionary suggested_key value.
  if (data.key[0] != '\0') {
    std::string current_platform = extensions::Command::CommandPlatform();
    if (platform_specific_only &&
        !base::Contains(platforms, current_platform)) {
      // Given a |current_platform| without a |suggested_key|, |default| is
      // used. However, some keys, such as Search on Chrome OS, are only valid
      // for platform specific entries. Skip the test in this case.
      return;
    }

    base::Value::Dict key_dict;
    for (const auto& platform : platforms) {
      key_dict.Set(platform, data.key);
    }

    input.clear();
    input.Set("suggested_key", std::move(key_dict));
    input.Set("description", data.description);

    bool result = command.Parse(input, data.command_name, i, &error);
    EXPECT_EQ(data.expected_result, result);

    if (result) {
      EXPECT_STREQ(data.description,
                   base::UTF16ToASCII(command.description()).c_str());
      EXPECT_STREQ(data.command_name, command.command_name().c_str());
      EXPECT_EQ(data.accelerator, command.accelerator());
    }
  }
}

TEST(CommandTest, ExtensionCommandParsing) {
  const ui::Accelerator none = ui::Accelerator();
  const ui::Accelerator shift_f =
      ui::Accelerator(ui::VKEY_F, ui::EF_SHIFT_DOWN);
#if BUILDFLAG(IS_MAC)
  int ctrl = ui::EF_COMMAND_DOWN;
#else
  int ctrl = ui::EF_CONTROL_DOWN;
#endif

  const ui::Accelerator ctrl_f = ui::Accelerator(ui::VKEY_F, ctrl);
  const ui::Accelerator alt_f = ui::Accelerator(ui::VKEY_F, ui::EF_ALT_DOWN);
  const ui::Accelerator ctrl_shift_f =
      ui::Accelerator(ui::VKEY_F, ctrl | ui::EF_SHIFT_DOWN);
  const ui::Accelerator alt_shift_f =
      ui::Accelerator(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN);
  const ui::Accelerator ctrl_1 = ui::Accelerator(ui::VKEY_1, ctrl);
  const ui::Accelerator ctrl_comma = ui::Accelerator(ui::VKEY_OEM_COMMA, ctrl);
  const ui::Accelerator ctrl_dot = ui::Accelerator(ui::VKEY_OEM_PERIOD, ctrl);
  const ui::Accelerator ctrl_left = ui::Accelerator(ui::VKEY_LEFT, ctrl);
  const ui::Accelerator ctrl_right = ui::Accelerator(ui::VKEY_RIGHT, ctrl);
  const ui::Accelerator ctrl_up = ui::Accelerator(ui::VKEY_UP, ctrl);
  const ui::Accelerator ctrl_down = ui::Accelerator(ui::VKEY_DOWN, ctrl);
  const ui::Accelerator ctrl_ins = ui::Accelerator(ui::VKEY_INSERT, ctrl);
  const ui::Accelerator ctrl_del = ui::Accelerator(ui::VKEY_DELETE, ctrl);
  const ui::Accelerator ctrl_home = ui::Accelerator(ui::VKEY_HOME, ctrl);
  const ui::Accelerator ctrl_end = ui::Accelerator(ui::VKEY_END, ctrl);
  const ui::Accelerator ctrl_pgup = ui::Accelerator(ui::VKEY_PRIOR, ctrl);
  const ui::Accelerator ctrl_pgdwn = ui::Accelerator(ui::VKEY_NEXT, ctrl);
  const ui::Accelerator next_track =
      ui::Accelerator(ui::VKEY_MEDIA_NEXT_TRACK, ui::EF_NONE);
  const ui::Accelerator prev_track =
      ui::Accelerator(ui::VKEY_MEDIA_PREV_TRACK, ui::EF_NONE);
  const ui::Accelerator play_pause =
      ui::Accelerator(ui::VKEY_MEDIA_PLAY_PAUSE, ui::EF_NONE);
  const ui::Accelerator stop =
      ui::Accelerator(ui::VKEY_MEDIA_STOP, ui::EF_NONE);

  ConstCommandsTestData kTests[] = {
      // Negative test (one or more missing required fields). We don't need to
      // test |command_name| being blank as it is used as a key in the manifest,
      // so it can't be blank (and we CHECK() when it is). A blank shortcut is
      // permitted.
      {false, none, "command", "", ""},
      {false, none, "command", "Ctrl+f", ""},
      // Ctrl+Alt is not permitted, see MSDN link in comments in Parse function.
      {false, none, "command", "Ctrl+Alt+F", "description"},
      // Unsupported shortcuts/too many, or missing modifier.
      {false, none, "command", "A", "description"},
      {false, none, "command", "F10", "description"},
      {false, none, "command", "Ctrl+F+G", "description"},
      {false, none, "command", "Ctrl+Alt+Shift+G", "description"},
      // Shift on its own is not supported.
      {false, shift_f, "command", "Shift+F", "description"},
      {false, shift_f, "command", "F+Shift", "description"},
      // Basic tests.
      {true, none, "command", "", "description"},
      {true, ctrl_f, "command", "Ctrl+F", "description"},
      {true, alt_f, "command", "Alt+F", "description"},
      {true, ctrl_shift_f, "command", "Ctrl+Shift+F", "description"},
      {true, alt_shift_f, "command", "Alt+Shift+F", "description"},
      {true, ctrl_1, "command", "Ctrl+1", "description"},
      // Shortcut token order tests.
      {true, ctrl_f, "command", "F+Ctrl", "description"},
      {true, alt_f, "command", "F+Alt", "description"},
      {true, ctrl_shift_f, "command", "F+Ctrl+Shift", "description"},
      {true, ctrl_shift_f, "command", "F+Shift+Ctrl", "description"},
      {true, alt_shift_f, "command", "F+Alt+Shift", "description"},
      {true, alt_shift_f, "command", "F+Shift+Alt", "description"},
      // Case insensitivity is not OK.
      {false, ctrl_f, "command", "Ctrl+f", "description"},
      {false, ctrl_f, "command", "cTrL+F", "description"},
      // Skipping description is OK for browser- and pageActions.
      {true, ctrl_f, "_execute_browser_action", "Ctrl+F", ""},
      {true, ctrl_f, "_execute_page_action", "Ctrl+F", ""},
      // Home, End, Arrow keys, etc.
      {true, ctrl_comma, "_execute_browser_action", "Ctrl+Comma", ""},
      {true, ctrl_dot, "_execute_browser_action", "Ctrl+Period", ""},
      {true, ctrl_left, "_execute_browser_action", "Ctrl+Left", ""},
      {true, ctrl_right, "_execute_browser_action", "Ctrl+Right", ""},
      {true, ctrl_up, "_execute_browser_action", "Ctrl+Up", ""},
      {true, ctrl_down, "_execute_browser_action", "Ctrl+Down", ""},
      {true, ctrl_ins, "_execute_browser_action", "Ctrl+Insert", ""},
      {true, ctrl_del, "_execute_browser_action", "Ctrl+Delete", ""},
      {true, ctrl_home, "_execute_browser_action", "Ctrl+Home", ""},
      {true, ctrl_end, "_execute_browser_action", "Ctrl+End", ""},
      {true, ctrl_pgup, "_execute_browser_action", "Ctrl+PageUp", ""},
      {true, ctrl_pgdwn, "_execute_browser_action", "Ctrl+PageDown", ""},
      // Media keys.
      {true, next_track, "command", "MediaNextTrack", "description"},
      {true, play_pause, "command", "MediaPlayPause", "description"},
      {true, prev_track, "command", "MediaPrevTrack", "description"},
      {true, stop, "command", "MediaStop", "description"},
      {false, none, "_execute_browser_action", "MediaNextTrack", ""},
      {false, none, "_execute_page_action", "MediaPrevTrack", ""},
      {false, none, "command", "Ctrl+Shift+MediaPrevTrack", "description"},
  };
  std::vector<std::string> all_platforms;
  all_platforms.push_back("default");
  all_platforms.push_back("chromeos");
  all_platforms.push_back("linux");
  all_platforms.push_back("mac");
  all_platforms.push_back("windows");

  for (size_t i = 0; i < std::size(kTests); ++i)
    CheckParse(kTests[i], i, false, all_platforms);
}

// TODO(https://crbug.com/356905053): Add/adjust command key support on
// desktop-android platform.
#if BUILDFLAG(IS_DESKTOP_ANDROID)
#define MAYBE_ExtensionCommandParsingFallback \
  DISABLED_ExtensionCommandParsingFallback
#else
#define MAYBE_ExtensionCommandParsingFallback ExtensionCommandParsingFallback
#endif
TEST(CommandTest, MAYBE_ExtensionCommandParsingFallback) {
  std::string description = "desc";
  std::string command_name = "foo";

  // Test that platform specific keys are honored on each platform, despite
  // fallback being given.
  base::Value::Dict input;
  input.Set("description", description);

  base::Value::Dict& key_dict =
      input.Set("suggested_key", base::Value::Dict())->GetDict();
  key_dict.Set("default", "Ctrl+Shift+D");
  key_dict.Set("windows", "Ctrl+Shift+W");
  key_dict.Set("mac", "Ctrl+Shift+M");
  key_dict.Set("linux", "Ctrl+Shift+L");
  key_dict.Set("chromeos", "Ctrl+Shift+C");

  extensions::Command command;
  std::u16string error;
  EXPECT_TRUE(command.Parse(input, command_name, 0, &error));
  EXPECT_STREQ(description.c_str(),
               base::UTF16ToASCII(command.description()).c_str());
  EXPECT_STREQ(command_name.c_str(), command.command_name().c_str());

#if BUILDFLAG(IS_WIN)
  ui::Accelerator accelerator(ui::VKEY_W,
                              ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN);
#elif BUILDFLAG(IS_MAC)
  ui::Accelerator accelerator(ui::VKEY_M,
                              ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN);
#elif BUILDFLAG(IS_CHROMEOS)
  ui::Accelerator accelerator(ui::VKEY_C,
                              ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN);
#elif BUILDFLAG(IS_LINUX)
  ui::Accelerator accelerator(ui::VKEY_L,
                              ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN);
#elif BUILDFLAG(IS_FUCHSIA)
  // TODO(crbug.com/40220501): Change this once we decide on a unique platform
  // key for Fuchsia.
  ui::Accelerator accelerator(ui::VKEY_L,
                              ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN);
#else
  ui::Accelerator accelerator(ui::VKEY_D,
                              ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN);
#endif
  EXPECT_EQ(accelerator, command.accelerator())
      << Command::AcceleratorToString(command.accelerator()) << " vs "
      << Command::AcceleratorToString(accelerator);

  // Misspell a platform.
  key_dict.Set("windosw", "Ctrl+M");
  EXPECT_FALSE(command.Parse(input, command_name, 0, &error));
  EXPECT_TRUE(key_dict.Remove("windosw"));

  // Now remove platform specific keys (leaving just "default") and make sure
  // every platform falls back to the default.
  EXPECT_TRUE(key_dict.Remove("windows"));
  EXPECT_TRUE(key_dict.Remove("mac"));
  EXPECT_TRUE(key_dict.Remove("linux"));
  EXPECT_TRUE(key_dict.Remove("chromeos"));
  EXPECT_TRUE(command.Parse(input, command_name, 0, &error));
  EXPECT_EQ(ui::VKEY_D, command.accelerator().key_code());

  // Now remove "default", leaving no option but failure. Or, in the words of
  // the immortal Adam Savage: "Failure is always an option".
  EXPECT_TRUE(key_dict.Remove("default"));
  EXPECT_FALSE(command.Parse(input, command_name, 0, &error));

  // Make sure Command is not supported for non-Mac platforms.
  key_dict.Set("default", "Command+M");
  EXPECT_FALSE(command.Parse(input, command_name, 0, &error));
  EXPECT_TRUE(key_dict.Remove("default"));
  key_dict.Set("windows", "Command+M");
  EXPECT_FALSE(command.Parse(input, command_name, 0, &error));
  EXPECT_TRUE(key_dict.Remove("windows"));

  // Now add only a valid platform that we are not running on to make sure devs
  // are notified of errors on other platforms.
#if BUILDFLAG(IS_WIN)
  key_dict.Set("mac", "Ctrl+Shift+M");
#else
  key_dict.Set("windows", "Ctrl+Shift+W");
#endif
  EXPECT_FALSE(command.Parse(input, command_name, 0, &error));

  // Make sure Mac specific keys are not processed on other platforms.
#if !BUILDFLAG(IS_MAC)
  key_dict.Set("windows", "Command+Shift+M");
  EXPECT_FALSE(command.Parse(input, command_name, 0, &error));
#endif
}

TEST(CommandTest, ExtensionCommandParsingPlatformSpecific) {
  ui::Accelerator search_a(ui::VKEY_A, ui::EF_COMMAND_DOWN);
  ui::Accelerator search_shift_z(ui::VKEY_Z,
                                 ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN);

  ConstCommandsTestData kChromeOsTests[] = {
      {true, search_shift_z, "command", "Search+Shift+Z", "description"},
      {true, search_a, "command", "Search+A", "description"},
      // Command is not valid on Chrome OS.
      {false, search_shift_z, "command", "Command+Shift+Z", "description"},
  };

  std::vector<std::string> chromeos;
  chromeos.push_back("chromeos");
  for (size_t i = 0; i < std::size(kChromeOsTests); ++i)
    CheckParse(kChromeOsTests[i], i, true, chromeos);

  ConstCommandsTestData kNonChromeOsSearchTests[] = {
      {false, search_shift_z, "command", "Search+Shift+Z", "description"},
  };
  std::vector<std::string> non_chromeos;
  non_chromeos.push_back("default");
  non_chromeos.push_back("windows");
  non_chromeos.push_back("mac");
  non_chromeos.push_back("linux");

  for (size_t i = 0; i < std::size(kNonChromeOsSearchTests); ++i)
    CheckParse(kNonChromeOsSearchTests[i], i, true, non_chromeos);
}

}  // namespace extensions
