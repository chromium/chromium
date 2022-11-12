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
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("command_simple_browser_action.json");
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
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("command_simple_page_action.json");
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
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("command_simple_action.json");
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
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("command_simple_only_custom_command.json");
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
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("command_simple_only_custom_command_v3.json");
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
  scoped_refptr<Extension> extension = LoadAndExpectSuccess(
      "command_multiple_action_commands_install_warning.json");
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
  scoped_refptr<Extension> extension = LoadAndExpectSuccess(
      "command_multiple_action_commands_install_warning_v3.json");
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
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("command_action_incorrect_install_warnings.json");
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
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("command_action_incorrect_install_warnings_v3.json");
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
  LoadAndExpectError("command_too_many.json",
                     errors::kInvalidKeyBindingTooMany);
}

TEST_F(CommandsManifestTest, CommandManifestManyButWithinBounds) {
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("command_many_but_shortcuts_under_limit.json");
}

TEST_F(CommandsManifestTest, CommandManifestAllowNumbers) {
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("command_allow_numbers.json");
}

TEST_F(CommandsManifestTest, CommandManifestRejectJustShift) {
  LoadAndExpectError("command_reject_just_shift.json",
                     errors::kInvalidKeyBinding);
}

TEST_F(CommandsManifestTest, BrowserActionSynthesizesCommand) {
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("browser_action_synthesizes_command.json");
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
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("action_synthesizes_command.json");
  const Command* command = CommandsInfo::GetActionCommand(extension.get());
  ASSERT_TRUE(command);
  EXPECT_EQ(ui::VKEY_UNKNOWN, command->accelerator().key_code());
}

// This test makes sure that the "commands" feature and the "commands.global"
// property load properly.
TEST_F(CommandsManifestTest, LoadsOnStable) {
  scoped_refptr<Extension> extension1 =
      LoadAndExpectSuccess("command_ext.json");
  scoped_refptr<Extension> extension2 =
      LoadAndExpectSuccess("command_app.json");
  scoped_refptr<Extension> extension3 =
      LoadAndExpectSuccess("command_ext_global.json");
  scoped_refptr<Extension> extension4 =
      LoadAndExpectSuccess("command_app_global.json");
}

TEST_F(CommandsManifestTest, CommandManifestShouldNotCountMediaKeys) {
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("command_should_not_count_media_keys.json");
}

}  // namespace extensions
