// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "extensions/common/api/commands/commands_handler.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_test.h"
#include "extensions/common/warnings_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace errors = manifest_errors;

using CommandsManifestTest = ManifestTest;

#if BUILDFLAG(IS_MAC)
constexpr int kControlKey = ui::EF_COMMAND_DOWN;
#else
constexpr int kControlKey = ui::EF_CONTROL_DOWN;
#endif

TEST_F(CommandsManifestTest, CommandManifestParseCommandsBrowserAction) {
  static constexpr char kManifest[] =
      R"({
           "name": "Command test - Browser Action",
           "manifest_version": 2,
           "version": "1",
           "commands": {
             "feature1": {
               "suggested_key": "Ctrl+Shift+F",
               "description": "desc"
             },
             "_execute_browser_action": {
               "suggested_key": "Alt+Shift+F",
               "description": "browser action"
             }
           },
           "browser_action" : {}
         })";

  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess(ManifestData::FromJSON(kManifest));
  ASSERT_TRUE(extension.get());

  const CommandMap* commands = CommandsInfo::GetNamedCommands(extension.get());
  ASSERT_TRUE(commands);
  EXPECT_EQ(1u, commands->size());
  auto iter = commands->begin();
  const Command* named_command = &(*iter).second;
  EXPECT_EQ("feature1", named_command->command_name());
  EXPECT_EQ(u"desc", named_command->description());
  const ui::Accelerator ctrl_shift_f =
      ui::Accelerator(ui::VKEY_F, kControlKey | ui::EF_SHIFT_DOWN);
  EXPECT_EQ(ctrl_shift_f, named_command->accelerator());

  const Command* browser_action =
      CommandsInfo::GetBrowserActionCommand(extension.get());
  ASSERT_TRUE(browser_action);
  EXPECT_EQ("_execute_browser_action", browser_action->command_name());
  EXPECT_EQ(u"", browser_action->description());
  const ui::Accelerator alt_shift_f =
      ui::Accelerator(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN);
  EXPECT_EQ(alt_shift_f, browser_action->accelerator());

  EXPECT_FALSE(warnings_test_util::HasInstallWarning(
      extension,
      manifest_errors::kCommandActionIncorrectForManifestActionType));
}

TEST_F(CommandsManifestTest, CommandManifestParseCommandsPageAction) {
  static constexpr char kManifest[] =
      R"({
           "name": "Command test - page Action",
           "manifest_version": 2,
           "version": "1",
           "commands": {
             "feature1": {
               "suggested_key": "Ctrl+Shift+F",
               "description": "desc"
             },
             "_execute_page_action": {
               "suggested_key": "Ctrl+F",
               "description": ""
             }
           },
           "page_action" : {}
         })";

  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess(ManifestData::FromJSON(kManifest));
  ASSERT_TRUE(extension.get());

  const CommandMap* commands = CommandsInfo::GetNamedCommands(extension.get());
  ASSERT_TRUE(commands);
  EXPECT_EQ(1u, commands->size());
  auto iter = commands->begin();
  const Command* named_command = &(*iter).second;
  EXPECT_EQ("feature1", named_command->command_name());
  EXPECT_EQ(u"desc", named_command->description());

  const Command* page_action =
      CommandsInfo::GetPageActionCommand(extension.get());
  ASSERT_TRUE(page_action);
  EXPECT_EQ("_execute_page_action", page_action->command_name());
  EXPECT_EQ(u"", page_action->description());
  const ui::Accelerator ctrl_f = ui::Accelerator(ui::VKEY_F, kControlKey);
  EXPECT_EQ(ctrl_f, page_action->accelerator());

  EXPECT_FALSE(warnings_test_util::HasInstallWarning(
      extension,
      manifest_errors::kCommandActionIncorrectForManifestActionType));
}

TEST_F(CommandsManifestTest, CommandManifestParseCommandsAction) {
  static constexpr char kManifest[] =
      R"({
           "name": "Command test - Action",
           "manifest_version": 3,
           "version": "1",
           "commands": {
             "feature1": {
               "suggested_key": "Ctrl+Shift+F",
               "description": "desc"
             },
             "_execute_action": {
               "suggested_key": "Ctrl+G",
               "description": ""
             }
           },
           "action" : {}
         })";

  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess(ManifestData::FromJSON(kManifest));
  ASSERT_TRUE(extension.get());

  const CommandMap* commands = CommandsInfo::GetNamedCommands(extension.get());
  ASSERT_TRUE(commands);
  EXPECT_EQ(1u, commands->size());
  auto iter = commands->begin();
  const Command* named_command = &(*iter).second;
  EXPECT_EQ("feature1", named_command->command_name());
  EXPECT_EQ(u"desc", named_command->description());

  const Command* action = CommandsInfo::GetActionCommand(extension.get());
  ASSERT_TRUE(action);
  EXPECT_EQ("_execute_action", action->command_name());
  EXPECT_EQ(u"", action->description());
  const ui::Accelerator ctrl_g = ui::Accelerator(ui::VKEY_G, kControlKey);
  EXPECT_EQ(ctrl_g, action->accelerator());

  EXPECT_FALSE(warnings_test_util::HasInstallWarning(
      extension,
      manifest_errors::kCommandActionIncorrectForManifestActionType));
}

// Tests that when only a custom action command is specified we create a
// default action command for the action type for MV2.
TEST_F(CommandsManifestTest,
       CommandManifestParseCommandsOnlyCustomCommandGetsDefault_MV2) {
  static constexpr char kManifest[] =
      R"({
           "name": "Command test - Only Custom Commands",
           "manifest_version": 2,
           "version": "1",
           "commands": {
             "feature1": {
               "suggested_key": "Ctrl+Shift+F",
               "description": "desc"
             }
           },
           "browser_action" : {}
         })";

  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess(ManifestData::FromJSON(kManifest));
  ASSERT_TRUE(extension.get());

  const CommandMap* commands = CommandsInfo::GetNamedCommands(extension.get());
  ASSERT_TRUE(commands);
  EXPECT_EQ(1u, commands->size());
  auto iter = commands->begin();
  const Command* named_command = &(*iter).second;
  EXPECT_EQ("feature1", named_command->command_name());
  EXPECT_EQ(u"desc", named_command->description());

  const Command* browser_action =
      CommandsInfo::GetBrowserActionCommand(extension.get());
  ASSERT_TRUE(browser_action);
  EXPECT_EQ("",
            browser_action->AcceleratorToString(browser_action->accelerator()));

  EXPECT_FALSE(warnings_test_util::HasInstallWarning(
      extension,
      manifest_errors::kCommandActionIncorrectForManifestActionType));
}

// Tests that when only a custom action command is specified we create a
// default action command for the action type for MV3.
TEST_F(CommandsManifestTest,
       CommandManifestParseCommandsOnlyCustomCommandGetsDefault_MV3) {
  static constexpr char kManifest[] =
      R"({
           "name": "Command test - Only Custom Commands - MV3",
           "manifest_version": 3,
           "version": "1",
           "commands": {
             "feature1": {
               "suggested_key": "Ctrl+Shift+F",
               "description": "desc"
             }
           },
           "action" : {}
         })";

  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess(ManifestData::FromJSON(kManifest));
  ASSERT_TRUE(extension.get());

  const CommandMap* commands = CommandsInfo::GetNamedCommands(extension.get());
  ASSERT_TRUE(commands);
  EXPECT_EQ(1u, commands->size());
  auto iter = commands->begin();
  const Command* named_command = &(*iter).second;
  EXPECT_EQ("feature1", named_command->command_name());
  EXPECT_EQ(u"desc", named_command->description());

  const Command* action = CommandsInfo::GetActionCommand(extension.get());
  ASSERT_TRUE(action);
  EXPECT_EQ("", action->AcceleratorToString(action->accelerator()));

  EXPECT_FALSE(warnings_test_util::HasInstallWarning(
      extension,
      manifest_errors::kCommandActionIncorrectForManifestActionType));
}

// Tests that only the correct action command (_execute_browser_action) is
// used from the manifest for MV2, but others are ignored and we install a
// warning for the incorrect command. See https://crbug.com/1353210.
TEST_F(CommandsManifestTest,
       CommandManifestIgnoreInvalidActionCommandsAndInstallWarning_MV2) {
  static constexpr char kManifest[] =
      R"({
           "name": "Command test - Multiple Action Commands",
           "manifest_version": 2,
           "version": "1",
           "commands": {
             "feature1": {
               "suggested_key": "Ctrl+Shift+F",
               "description": "desc"
             },
             "_execute_browser_action": {
               "suggested_key": "Alt+Shift+F",
               "description": "browser action"
             },
             "_execute_page_action": {
               "suggested_key": "Ctrl+F",
               "description": ""
             }
           },
           "browser_action" : {}
         })";

  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess(ManifestData::FromJSON(kManifest));
  ASSERT_TRUE(extension.get());

  const Command* browser_action =
      CommandsInfo::GetBrowserActionCommand(extension.get());
  ASSERT_TRUE(browser_action);
  EXPECT_EQ("_execute_browser_action", browser_action->command_name());
  EXPECT_EQ(u"", browser_action->description());
  const ui::Accelerator alt_shift_f =
      ui::Accelerator(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN);
  EXPECT_EQ(alt_shift_f, browser_action->accelerator());

  EXPECT_FALSE(CommandsInfo::GetPageActionCommand(extension.get()));
  EXPECT_FALSE(CommandsInfo::GetActionCommand(extension.get()));

  EXPECT_TRUE(warnings_test_util::HasInstallWarning(
      extension,
      manifest_errors::kCommandActionIncorrectForManifestActionType));
}

// Tests that only the correct action command (_execute_action) is used
// from the manifest for MV3, but others are ignored and we install a warning
// for the incorrect command. See https://crbug.com/1353210.
TEST_F(CommandsManifestTest,
       CommandManifestIgnoreInvalidActionCommandsAndInstallWarning_MV3) {
  static constexpr char kManifest[] =
      R"({
           "name": "Command test - Multiple Action Commands - MV3",
           "manifest_version": 3,
           "version": "1",
           "commands": {
             "feature1": {
               "suggested_key": "Ctrl+Shift+F",
               "description": "desc"
             },
             "_execute_action": {
               "suggested_key": "Alt+Shift+F",
               "description": "browser action"
             },
             "_execute_page_action": {
               "suggested_key": "Ctrl+F",
               "description": ""
             }
           },
           "action" : {}
         })";

  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess(ManifestData::FromJSON(kManifest));
  ASSERT_TRUE(extension.get());

  const Command* action = CommandsInfo::GetActionCommand(extension.get());
  ASSERT_TRUE(action);
  EXPECT_EQ("_execute_action", action->command_name());
  EXPECT_EQ(u"", action->description());
  const ui::Accelerator alt_shift_f =
      ui::Accelerator(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN);
  EXPECT_EQ(alt_shift_f, action->accelerator());

  EXPECT_FALSE(CommandsInfo::GetBrowserActionCommand(extension.get()));
  EXPECT_FALSE(CommandsInfo::GetPageActionCommand(extension.get()));

  EXPECT_TRUE(warnings_test_util::HasInstallWarning(
      extension,
      manifest_errors::kCommandActionIncorrectForManifestActionType));
}

// Tests that when only incorrect action commands are specified we install
// a warning and set a default (for MV2). See https://crbug.com/1353210.
TEST_F(CommandsManifestTest,
       CommandManifestAllInvalidActionCommandsInstallWarning_MV2) {
  static constexpr char kManifest[] =
      R"({
           "name": "Command test - Multiple Action Commands But None Correct",
           "manifest_version": 2,
           "version": "1",
           "commands": {
             "feature1": {
               "suggested_key": "Ctrl+Shift+F",
               "description": "desc"
             },
             "_execute_action": {
               "suggested_key": "Alt+Shift+F",
               "description": "browser action"
             },
             "_execute_page_action": {
               "suggested_key": "Ctrl+F",
               "description": ""
             }
           },
           "browser_action" : {}
         })";

  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess(ManifestData::FromJSON(kManifest));
  ASSERT_TRUE(extension.get());

  const Command* browser_action =
      CommandsInfo::GetBrowserActionCommand(extension.get());
  ASSERT_TRUE(browser_action);
  EXPECT_EQ("",
            browser_action->AcceleratorToString(browser_action->accelerator()));

  EXPECT_FALSE(CommandsInfo::GetActionCommand(extension.get()));
  EXPECT_FALSE(CommandsInfo::GetPageActionCommand(extension.get()));

  EXPECT_TRUE(warnings_test_util::HasInstallWarning(
      extension,
      manifest_errors::kCommandActionIncorrectForManifestActionType));
}

// Tests that when only incorrect execute commands are specified we install
// a warning and set a default (for MV3). See https://crbug.com/1353210.
TEST_F(CommandsManifestTest,
       CommandManifestAllInvalidActionCommandsInstallWarning_MV3) {
  static constexpr char kManifest[] =
      R"({
           "name": "Multiple Action Commands But None Correct - MV3",
           "manifest_version": 3,
           "version": "1",
           "commands": {
             "feature1": {
               "suggested_key": "Ctrl+Shift+F",
               "description": "desc"
             },
             "_execute_browser_action": {
               "suggested_key": "Alt+Shift+F",
               "description": "browser action"
             },
             "_execute_page_action": {
               "suggested_key": "Ctrl+F",
               "description": ""
             }
           },
           "action" : {}
         })";

  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess(ManifestData::FromJSON(kManifest));
  ASSERT_TRUE(extension.get());

  const Command* action = CommandsInfo::GetActionCommand(extension.get());
  ASSERT_TRUE(action);
  EXPECT_EQ("", action->AcceleratorToString(action->accelerator()));

  EXPECT_FALSE(CommandsInfo::GetBrowserActionCommand(extension.get()));
  EXPECT_FALSE(CommandsInfo::GetPageActionCommand(extension.get()));

  EXPECT_TRUE(warnings_test_util::HasInstallWarning(
      extension,
      manifest_errors::kCommandActionIncorrectForManifestActionType));
}

TEST_F(CommandsManifestTest, CommandManifestShortcutsTooMany) {
  static constexpr char kManifest[] =
      R"({
           "name": "Command test - too many commands with shortcuts",
           "manifest_version": 2,
           "version": "2",
           "commands": {
             "feature1": {
               "suggested_key": "Ctrl+A",
               "description": "feature1"
             },
             "feature2": {
               "suggested_key": "Ctrl+B",
               "description": "feature2"
             },
             "feature3": {
               "suggested_key": "Ctrl+C",
               "description": "feature3"
             },
             "feature4": {
               "suggested_key": "Ctrl+D",
               "description": "feature4"
             },
             "feature5": {
               "suggested_key": "Ctrl+E",
               "description": "feature5"
             }
           }
         })";
  LoadAndExpectError(ManifestData::FromJSON(kManifest),
                     errors::kInvalidKeyBindingTooMany);
}

TEST_F(CommandsManifestTest, CommandManifestManyButWithinBounds) {
  static constexpr char kManifest[] =
      R"({
           "name": "Command test - many commands, but not too many shortcuts",
           "manifest_version": 2,
           "version": "2",
           "commands": {
             "feature1": {
               "suggested_key": "Ctrl+A",
               "description": "feature1"
             },
             "feature2": {
               "suggested_key": "Ctrl+B",
               "description": "feature2"
             },
             "feature3": {
               "suggested_key": "Ctrl+C",
               "description": "feature3"
             },
             "feature4": {
               "suggested_key": "Ctrl+D",
               "description": "feature4"
             },
             "feature5": {
               "description": "feature5"
             }
           }
         })";
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess(ManifestData::FromJSON(kManifest));
}

TEST_F(CommandsManifestTest, CommandManifestAllowNumbers) {
  static constexpr char kManifest[] =
      R"({
           "name": "Command test - Numbers should be allowed",
           "manifest_version": 2,
           "version": "1",
           "commands": {
             "feature1": {
               "suggested_key": "Ctrl+1",
               "description": "feature1"
             }
           }
         })";
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess(ManifestData::FromJSON(kManifest));
}

TEST_F(CommandsManifestTest, CommandManifestRejectJustShift) {
  static constexpr char kManifest[] =
      R"({
           "name": "Command test - Shift on its own is not a valid modifier",
           "manifest_version": 2,
           "version": "1",
           "commands": {
             "feature1": {
               "suggested_key": "Shift+A",
               "description": "feature1"
             }
           }
         })";
  LoadAndExpectError(ManifestData::FromJSON(kManifest),
                     errors::kInvalidKeyBinding);
}

TEST_F(CommandsManifestTest, BrowserActionSynthesizesCommand) {
  static constexpr char kManifest[] =
      R"({
           "name": "A simple browser action that defines no extension command",
           "version": "1.0",
           "manifest_version": 2,
           "browser_action": {
             "default_title": "Make this page red"
           }
         })";
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess(ManifestData::FromJSON(kManifest));
  // An extension with a browser action but no extension command specified
  // should get a command assigned to it.
  const extensions::Command* command =
      CommandsInfo::GetBrowserActionCommand(extension.get());
  ASSERT_TRUE(command);
  EXPECT_EQ(ui::VKEY_UNKNOWN, command->accelerator().key_code());
}

// An extension with an action but no extension command specified should get a
// command assigned to it.
TEST_F(CommandsManifestTest, ActionSynthesizesCommand) {
  static constexpr char kManifest[] =
      R"({
           "name": "Synthesize action shortcut if commands key is missing",
           "version": "1.0",
           "manifest_version": 3,
           "action": {
             "default_title": "Test"
           }
         })";
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess(ManifestData::FromJSON(kManifest));
  const Command* command = CommandsInfo::GetActionCommand(extension.get());
  ASSERT_TRUE(command);
  EXPECT_EQ(ui::VKEY_UNKNOWN, command->accelerator().key_code());
}

// This test makes sure that the "commands" feature and the "commands.global"
// property load properly.
TEST_F(CommandsManifestTest, LoadsOnStable) {
  static constexpr char kManifest1[] =
      R"({
           "name": "Command test - Extension with Command",
           "manifest_version": 2,
           "version": "1",
           "commands": {
             "feature1": {
               "suggested_key": "Ctrl+Shift+0",
               "description": "desc"
             }
           }
         })";
  scoped_refptr<Extension> extension1 =
      LoadAndExpectSuccess(ManifestData::FromJSON(kManifest1));

  static constexpr char kManifest2[] =
      R"({
           "name": "Command test - App with Command",
           "manifest_version": 2,
           "version": "1",
           "app": {
             "background": { "scripts": ["background.js"] }
           },
           "commands": {
             "feature1": {
               "suggested_key": "Ctrl+Shift+0",
               "description": "desc"
             }
           }
         })";
  scoped_refptr<Extension> extension2 =
      LoadAndExpectSuccess(ManifestData::FromJSON(kManifest2));

  static constexpr char kManifest3[] =
      R"({
           "name": "Command test - Extension with Command that is global",
           "manifest_version": 2,
           "version": "1",
           "commands": {
             "feature1": {
               "suggested_key": "Ctrl+Shift+0",
               "description": "desc",
               "global": true
             }
           }
         })";
  scoped_refptr<Extension> extension3 =
      LoadAndExpectSuccess(ManifestData::FromJSON(kManifest3));

  static constexpr char kManifest4[] =
      R"({
           "name": "Command test - App with Command that is global",
           "manifest_version": 2,
           "version": "1",
           "app": {
             "background": { "scripts": ["background.js"] }
           },
           "commands": {
             "feature1": {
               "suggested_key": "Ctrl+Shift+0",
               "description": "desc",
               "global": true
             }
           }
         })";
  scoped_refptr<Extension> extension4 =
      LoadAndExpectSuccess(ManifestData::FromJSON(kManifest4));
}

TEST_F(CommandsManifestTest, CommandManifestShouldNotCountMediaKeys) {
  // Media keys shouldn't count towards the max of four shortcuts per extension.
  static constexpr char kManifest[] =
      R"({
           "name": "mediaKeys",
           "manifest_version": 2,
           "version": "1",
           "commands": {
             "MediaNextTrack": {
               "suggested_key": "MediaNextTrack",
               "description": "MediaNextTrack"
             },
             "MediaPlayPause": {
               "suggested_key": "MediaPlayPause",
               "description": "MediaPlayPause"
             },
             "MediaPrevTrack": {
               "suggested_key": "MediaPrevTrack",
               "description": "MediaPrevTrack"
             },
             "MediaStop": {
               "suggested_key": "MediaStop",
               "description": "MediaStop"
             },
             "feature1": {
               "suggested_key": "Ctrl+A",
               "description": "feature1"
             },
             "feature2": {
               "suggested_key": "Ctrl+B",
               "description": "feature2"
             }
           }
         })";
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess(ManifestData::FromJSON(kManifest));
}

}  // namespace extensions
