// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/commands/commands_handler.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/command.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"

namespace extensions {

namespace keys = manifest_keys;

namespace {
// The maximum number of commands (including page action/browser actions) with a
// keybinding an extension can have.
const int kMaxCommandsWithKeybindingPerExtension = 4;
}  // namespace

CommandsInfo::CommandsInfo() = default;
CommandsInfo::~CommandsInfo() = default;

// static
const Command* CommandsInfo::GetBrowserActionCommand(
    const Extension* extension) {
  auto* info =
      static_cast<CommandsInfo*>(extension->GetManifestData(keys::kCommands));
  return info ? info->browser_action_command.get() : nullptr;
}

// static
const Command* CommandsInfo::GetPageActionCommand(const Extension* extension) {
  auto* info =
      static_cast<CommandsInfo*>(extension->GetManifestData(keys::kCommands));
  return info ? info->page_action_command.get() : nullptr;
}

// static
const Command* CommandsInfo::GetActionCommand(const Extension* extension) {
  auto* info =
      static_cast<CommandsInfo*>(extension->GetManifestData(keys::kCommands));
  return info ? info->action_command.get() : nullptr;
}

// static
const CommandMap* CommandsInfo::GetNamedCommands(const Extension* extension) {
  auto* info =
      static_cast<CommandsInfo*>(extension->GetManifestData(keys::kCommands));
  return info ? &info->named_commands : nullptr;
}

CommandsHandler::CommandsHandler() = default;
CommandsHandler::~CommandsHandler() = default;

bool CommandsHandler::Parse(Extension* extension, std::u16string* error) {
  if (!extension->manifest()->FindKey(keys::kCommands)) {
    std::unique_ptr<CommandsInfo> commands_info(new CommandsInfo);
    MaybeSetActionDefault(extension, commands_info.get());
    extension->SetManifestData(keys::kCommands, std::move(commands_info));
    return true;
  }

  const base::Value::Dict* dict =
      extension->manifest()->available_values().FindDict(keys::kCommands);
  if (!dict) {
    *error = manifest_errors::kInvalidCommandsKey;
    return false;
  }

  std::unique_ptr<CommandsInfo> commands_info(new CommandsInfo);

  bool invalid_action_command_specified = false;
  int command_index = 0;
  int keybindings_found = 0;
  for (const auto item : *dict) {
    ++command_index;

    const base::Value::Dict* command = item.second.GetIfDict();
    if (!command) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          manifest_errors::kInvalidKeyBindingDictionary,
          base::NumberToString(command_index));
      return false;
    }

    std::unique_ptr<extensions::Command> binding(new Command());
    if (!binding->Parse(*command, item.first, command_index, error))
      return false;  // |error| already set.

    if (binding->accelerator().key_code() != ui::VKEY_UNKNOWN) {
      // Only media keys are allowed to work without modifiers, and because
      // media keys aren't registered exclusively they should not count towards
      // the max of four shortcuts per extension.
      if (!Command::IsMediaKey(binding->accelerator()))
        ++keybindings_found;

      if (keybindings_found > kMaxCommandsWithKeybindingPerExtension &&
          !PermissionsParser::HasAPIPermission(
              extension, mojom::APIPermissionID::kCommandsAccessibility)) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            manifest_errors::kInvalidKeyBindingTooMany,
            base::NumberToString(kMaxCommandsWithKeybindingPerExtension));
        return false;
      }
    }

    std::string command_name = binding->command_name();
    // Set the command only if it's correct for the manifest's action type. This
    // relies on the fact that manifests cannot have multiple action types.
    if (command_name == manifest_values::kActionCommandEvent) {
      if (extension->manifest()->FindKey(keys::kAction))
        commands_info->action_command = std::move(binding);
      else
        invalid_action_command_specified = true;
    } else if (command_name == manifest_values::kBrowserActionCommandEvent) {
      if (extension->manifest()->FindKey(keys::kBrowserAction))
        commands_info->browser_action_command = std::move(binding);
      else
        invalid_action_command_specified = true;
    } else if (command_name == manifest_values::kPageActionCommandEvent) {
      if (extension->manifest()->FindKey(keys::kPageAction))
        commands_info->page_action_command = std::move(binding);
      else
        invalid_action_command_specified = true;
    } else if (command_name[0] != '_') {  // Commands w/underscore are reserved.
      commands_info->named_commands[command_name] = *binding;
    }
  }

  if (invalid_action_command_specified) {
    extension->AddInstallWarning(InstallWarning(
        manifest_errors::kCommandActionIncorrectForManifestActionType,
        manifest_keys::kCommands));
  }

  MaybeSetActionDefault(extension, commands_info.get());
  extension->SetManifestData(keys::kCommands, std::move(commands_info));
  return true;
}

bool CommandsHandler::AlwaysParseForType(Manifest::Type type) const {
  return type == Manifest::TYPE_EXTENSION ||
         type == Manifest::TYPE_LEGACY_PACKAGED_APP ||
         type == Manifest::TYPE_PLATFORM_APP;
}

void CommandsHandler::MaybeSetActionDefault(const Extension* extension,
                                            CommandsInfo* info) {
  if (extension->manifest()->FindKey(keys::kAction) &&
      !info->action_command.get()) {
    info->action_command =
        std::make_unique<Command>(manifest_values::kActionCommandEvent,
                                  std::u16string(), std::string(), false);
  } else if (extension->manifest()->FindKey(keys::kBrowserAction) &&
             !info->browser_action_command.get()) {
    info->browser_action_command =
        std::make_unique<Command>(manifest_values::kBrowserActionCommandEvent,
                                  std::u16string(), std::string(), false);
  }
}

base::span<const char* const> CommandsHandler::Keys() const {
  static constexpr const char* kKeys[] = {keys::kCommands};
  return kKeys;
}

}  // namespace extensions
