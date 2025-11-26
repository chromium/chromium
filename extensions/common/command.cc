// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/command.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/android_buildflags.h"
#include "build/build_config.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "ui/base/accelerators/command.h"
#include "ui/base/accelerators/command_constants.h"
#include "ui/base/accelerators/media_keys_listener.h"

namespace extensions {

namespace errors = manifest_errors;
namespace keys = manifest_keys;
namespace values = manifest_values;

namespace {

static const char kMissing[] = "Missing";

static const char kCommandKeyNotSupported[] =
    "Command key is not supported. Note: Ctrl means Command on Mac";
static const char kOptionKeyNotSupported[] = "Option key is not supported.";

// For Mac, we convert "Ctrl" to "Command", "MacCtrl" to "Ctrl", and "Option" to
// "Alt". Other platforms leave the shortcut untouched.
std::string NormalizeShortcutSuggestion(std::string_view suggestion,
                                        std::string_view platform) {
  bool is_mac_platform =
      (platform == ui::kKeybindingPlatformMac) ||
      (platform == ui::kKeybindingPlatformDefault &&
       Command::CommandPlatform() == ui::kKeybindingPlatformMac);
  if (!is_mac_platform) {
    return std::string{suggestion};
  }

  std::vector<std::string_view> tokens = base::SplitStringPiece(
      suggestion, "+", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (auto& token : tokens) {
    if (token == ui::kKeyCtrl) {
      token = ui::kKeyCommand;
    } else if (token == ui::kKeyMacCtrl) {
      token = ui::kKeyCtrl;
    } else if (token == ui::kKeyOption) {
      token = ui::kKeyAlt;
    }
  }
  return base::JoinString(tokens, "+");
}

void SetAcceleratorParseErrorMessage(std::u16string* error,
                                     int index,
                                     std::string_view platform_key,
                                     std::string_view accelerator_string,
                                     ui::AcceleratorParseError parse_error) {
  error->clear();
  switch (parse_error) {
    case ui::AcceleratorParseError::kMalformedInput:
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidKeyBinding, base::NumberToString(index), platform_key,
          accelerator_string);
      break;
    case ui::AcceleratorParseError::kMediaKeyWithModifier:
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidKeyBindingMediaKeyWithModifier,
          base::NumberToString(index), platform_key, accelerator_string);
      break;
    case ui::AcceleratorParseError::kUnsupportedPlatform:
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidKeyBindingUnknownPlatform,
          base::NumberToString(index), platform_key);
      break;
  }
}

}  // namespace

Command::Command(std::string_view command_name,
                 std::u16string_view description,
                 std::string_view accelerator,
                 bool global)
    : ui::Command(command_name, description, global) {
  if (!accelerator.empty()) {
    std::u16string error;
    AcceleratorParseErrorCallback on_parse_error =
        base::BindOnce(SetAcceleratorParseErrorMessage, &error, 0,
                       CommandPlatform(), accelerator);
    set_accelerator(ParseImpl(accelerator, CommandPlatform(),
                              !IsActionRelatedCommand(command_name),
                              std::move(on_parse_error)));
  }
}

// static
std::string Command::CommandPlatform() {
#if BUILDFLAG(IS_WIN)
  return ui::kKeybindingPlatformWin;
#elif BUILDFLAG(IS_MAC)
  return ui::kKeybindingPlatformMac;
#elif BUILDFLAG(IS_CHROMEOS)
  return ui::kKeybindingPlatformChromeOs;
#elif BUILDFLAG(IS_LINUX)
  return ui::kKeybindingPlatformLinux;
#elif BUILDFLAG(IS_DESKTOP_ANDROID)
  // For now, we use linux keybindings on desktop android.
  return ui::kKeybindingPlatformLinux;
#else
#error Unsupported platform
#endif
}

// static
ui::Accelerator Command::StringToAccelerator(std::string_view accelerator,
                                             std::string_view command_name) {
  std::u16string error;
  AcceleratorParseErrorCallback on_parse_error =
      base::BindOnce(SetAcceleratorParseErrorMessage, &error, 0,
                     CommandPlatform(), accelerator);
  ui::Accelerator parsed = ParseImpl(accelerator, CommandPlatform(),
                                     !IsActionRelatedCommand(command_name),
                                     std::move(on_parse_error));
  return parsed;
}

// static
bool Command::IsActionRelatedCommand(std::string_view command_name) {
  return command_name == values::kActionCommandEvent ||
         command_name == values::kBrowserActionCommandEvent ||
         command_name == values::kPageActionCommandEvent;
}

bool Command::Parse(const base::Value::Dict& command,
                    std::string_view command_name,
                    int index,
                    std::u16string* error) {
  DCHECK(!command_name.empty());

  std::u16string description;
  if (!IsActionRelatedCommand(command_name)) {
    const std::string* description_ptr = command.FindString(keys::kDescription);
    if (!description_ptr || description_ptr->empty()) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidKeyBindingDescription, base::NumberToString(index));
      return false;
    }
    description = base::UTF8ToUTF16(*description_ptr);
  }

  // We'll build up a map of platform-to-shortcut suggestions.
  using SuggestionMap = std::map<const std::string, std::string>;
  SuggestionMap suggestions;

  // First try to parse the |suggested_key| as a dictionary.

  if (const base::Value::Dict* suggested_key_dict =
          command.FindDict(keys::kSuggestedKey)) {
    for (const auto item : *suggested_key_dict) {
      // For each item in the dictionary, extract the platforms specified.
      const std::string* suggested_key_string = item.second.GetIfString();
      if (suggested_key_string && !suggested_key_string->empty()) {
        // Found a platform, add it to the suggestions list.
        suggestions[item.first] = *suggested_key_string;
      } else {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidKeyBinding, base::NumberToString(index),
            keys::kSuggestedKey, kMissing);
        return false;
      }
    }
  } else {
    // No dictionary was found, fall back to using just a string, so developers
    // don't have to specify a dictionary if they just want to use one default
    // for all platforms.
    const std::string* suggested_key_string =
        command.FindString(keys::kSuggestedKey);
    if (suggested_key_string && !suggested_key_string->empty()) {
      // If only a single string is provided, it must be default for all.
      suggestions[ui::kKeybindingPlatformDefault] = *suggested_key_string;
    } else {
      suggestions[ui::kKeybindingPlatformDefault] = "";
    }
  }

  // Check if this is a global or a regular shortcut.
  bool global = command.FindBoolByDottedPath(keys::kGlobal).value_or(false);

  // Pre-normalize validation of the suggestions.
  for (auto iter = suggestions.begin(); iter != suggestions.end(); ++iter) {
    // Before we normalize Ctrl to Command we must detect when the developer
    // specified Command in the Default section, which will work on Mac after
    // normalization but only fail on other platforms when they try it out on
    // other platforms, which is not what we want.
    if (iter->first != ui::kKeybindingPlatformMac) {
      std::vector<std::string_view> tokens = base::SplitStringPiece(
          iter->second, "+", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
      for (const auto& token : tokens) {
        if (token == ui::kKeyCommand) {
          *error = ErrorUtils::FormatErrorMessageUTF16(
              errors::kInvalidKeyBinding, base::NumberToString(index),
              keys::kSuggestedKey, kCommandKeyNotSupported);
          return false;
        }
        if (token == ui::kKeyOption) {
          *error = ErrorUtils::FormatErrorMessageUTF16(
              errors::kInvalidKeyBinding, base::NumberToString(index),
              keys::kSuggestedKey, kOptionKeyNotSupported);
          return false;
        }
      }
    }
  }

  std::string platform = CommandPlatform();
  std::string key = platform;
  if (suggestions.find(key) == suggestions.end()) {
    key = ui::kKeybindingPlatformDefault;
  }
  if (suggestions.find(key) == suggestions.end()) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidKeyBindingMissingPlatform, base::NumberToString(index),
        keys::kSuggestedKey, platform);
    return false;  // No platform specified and no fallback. Bail.
  }

  // For developer convenience, we parse all the suggestions (and complain about
  // errors for platforms other than the current one) but use only what we need.
  std::map<const std::string, std::string>::const_iterator iter =
      suggestions.begin();
  for (; iter != suggestions.end(); ++iter) {
    ui::Accelerator accelerator;
    if (!iter->second.empty()) {
      std::string normalized_shortcut =
          NormalizeShortcutSuggestion(iter->second, iter->first);
      // Note that we pass iter->first to pretend we are on a platform we're not
      // on.
      AcceleratorParseErrorCallback on_parse_error =
          base::BindOnce(SetAcceleratorParseErrorMessage, error, index,
                         iter->first, iter->second);
      accelerator = ParseImpl(normalized_shortcut, iter->first,
                              !IsActionRelatedCommand(command_name),
                              std::move(on_parse_error));
      if (accelerator.key_code() == ui::VKEY_UNKNOWN) {
        if (error->empty()) {
          *error = ErrorUtils::FormatErrorMessageUTF16(
              errors::kInvalidKeyBinding, base::NumberToString(index),
              iter->first, iter->second);
        }
        return false;
      }
    }

    if (iter->first == key) {
      // This platform is our platform, so grab this key.
      set_accelerator(accelerator);
      set_command_name(command_name);
      set_description(description);
      set_global(global);
    }
  }
  return true;
}

}  // namespace extensions
