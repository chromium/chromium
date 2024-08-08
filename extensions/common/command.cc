// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/command.h"

#include <stddef.h>

#include <string_view>

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
#include "ui/base/accelerators/media_keys_listener.h"

namespace extensions {

namespace errors = manifest_errors;
namespace keys = manifest_keys;
namespace values = manifest_values;

namespace {

static const char kMissing[] = "Missing";

static const char kCommandKeyNotSupported[] =
    "Command key is not supported. Note: Ctrl means Command on Mac";

#if BUILDFLAG(IS_CHROMEOS)
// ChromeOS supports an additional modifier 'Search', which can result in longer
// sequences.
static const int kMaxTokenSize = 4;
#else
static const int kMaxTokenSize = 3;
#endif  // BUILDFLAG(IS_CHROMEOS)

bool DoesRequireModifier(const std::string& accelerator) {
  return accelerator != values::kKeyMediaNextTrack &&
         accelerator != values::kKeyMediaPlayPause &&
         accelerator != values::kKeyMediaPrevTrack &&
         accelerator != values::kKeyMediaStop;
}

// Parse an |accelerator| for a given platform (specified by |platform_key|) and
// return the result as a ui::Accelerator if successful, or VKEY_UNKNOWN if not.
// |index| is used when constructing an |error| messages to show which command
// in the manifest is failing and |should_parse_media_keys| specifies whether
// media keys are to be considered for parsing.
// Note: If the parsing rules here are changed, make sure to update the
// corresponding extension_command_list.js validation, which validates the user
// input for chrome://extensions/configureCommands.
ui::Accelerator ParseImpl(const std::string& accelerator,
                          const std::string& platform_key,
                          int index,
                          bool should_parse_media_keys,
                          std::u16string* error) {
  error->clear();
  if (platform_key != values::kKeybindingPlatformWin &&
      platform_key != values::kKeybindingPlatformMac &&
      platform_key != values::kKeybindingPlatformChromeOs &&
      platform_key != values::kKeybindingPlatformLinux &&
      platform_key != values::kKeybindingPlatformDefault) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidKeyBindingUnknownPlatform, base::NumberToString(index),
        platform_key);
    return ui::Accelerator();
  }

  std::vector<std::string> tokens = base::SplitString(
      accelerator, "+", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (tokens.size() == 0 ||
      (tokens.size() == 1 && DoesRequireModifier(accelerator)) ||
      tokens.size() > kMaxTokenSize) {
    *error = ErrorUtils::FormatErrorMessageUTF16(errors::kInvalidKeyBinding,
                                                 base::NumberToString(index),
                                                 platform_key, accelerator);
    return ui::Accelerator();
  }

  // Now, parse it into an accelerator.
  int modifiers = ui::EF_NONE;
  ui::KeyboardCode key = ui::VKEY_UNKNOWN;
  for (size_t i = 0; i < tokens.size(); i++) {
    if (tokens[i] == values::kKeyCtrl) {
      modifiers |= ui::EF_CONTROL_DOWN;
    } else if (tokens[i] == values::kKeyCommand) {
      if (platform_key == values::kKeybindingPlatformMac) {
        // Either the developer specified Command+foo in the manifest for Mac or
        // they specified Ctrl and it got normalized to Command (to get Ctrl on
        // Mac the developer has to specify MacCtrl). Therefore we treat this
        // as Command.
        modifiers |= ui::EF_COMMAND_DOWN;
#if BUILDFLAG(IS_MAC)
      } else if (platform_key == values::kKeybindingPlatformDefault) {
        // If we see "Command+foo" in the Default section it can mean two
        // things, depending on the platform:
        // The developer specified "Ctrl+foo" for Default and it got normalized
        // on Mac to "Command+foo". This is fine. Treat it as Command.
        modifiers |= ui::EF_COMMAND_DOWN;
#endif
      } else {
        // No other platform supports Command.
        key = ui::VKEY_UNKNOWN;
        break;
      }
    } else if (tokens[i] == values::kKeySearch) {
      // Search is a special modifier only on ChromeOS and maps to 'Command'.
      if (platform_key == values::kKeybindingPlatformChromeOs) {
        modifiers |= ui::EF_COMMAND_DOWN;
      } else {
        // No other platform supports Search.
        key = ui::VKEY_UNKNOWN;
        break;
      }
    } else if (tokens[i] == values::kKeyAlt) {
      modifiers |= ui::EF_ALT_DOWN;
    } else if (tokens[i] == values::kKeyShift) {
      modifiers |= ui::EF_SHIFT_DOWN;
    } else if (tokens[i].size() == 1 ||  // A-Z, 0-9.
               tokens[i] == values::kKeyComma ||
               tokens[i] == values::kKeyPeriod || tokens[i] == values::kKeyUp ||
               tokens[i] == values::kKeyDown || tokens[i] == values::kKeyLeft ||
               tokens[i] == values::kKeyRight || tokens[i] == values::kKeyIns ||
               tokens[i] == values::kKeyDel || tokens[i] == values::kKeyHome ||
               tokens[i] == values::kKeyEnd || tokens[i] == values::kKeyPgUp ||
               tokens[i] == values::kKeyPgDwn ||
               tokens[i] == values::kKeySpace || tokens[i] == values::kKeyTab ||
               tokens[i] == values::kKeyMediaNextTrack ||
               tokens[i] == values::kKeyMediaPlayPause ||
               tokens[i] == values::kKeyMediaPrevTrack ||
               tokens[i] == values::kKeyMediaStop) {
      if (key != ui::VKEY_UNKNOWN) {
        // Multiple key assignments.
        key = ui::VKEY_UNKNOWN;
        break;
      }

      if (tokens[i] == values::kKeyComma) {
        key = ui::VKEY_OEM_COMMA;
      } else if (tokens[i] == values::kKeyPeriod) {
        key = ui::VKEY_OEM_PERIOD;
      } else if (tokens[i] == values::kKeyUp) {
        key = ui::VKEY_UP;
      } else if (tokens[i] == values::kKeyDown) {
        key = ui::VKEY_DOWN;
      } else if (tokens[i] == values::kKeyLeft) {
        key = ui::VKEY_LEFT;
      } else if (tokens[i] == values::kKeyRight) {
        key = ui::VKEY_RIGHT;
      } else if (tokens[i] == values::kKeyIns) {
        key = ui::VKEY_INSERT;
      } else if (tokens[i] == values::kKeyDel) {
        key = ui::VKEY_DELETE;
      } else if (tokens[i] == values::kKeyHome) {
        key = ui::VKEY_HOME;
      } else if (tokens[i] == values::kKeyEnd) {
        key = ui::VKEY_END;
      } else if (tokens[i] == values::kKeyPgUp) {
        key = ui::VKEY_PRIOR;
      } else if (tokens[i] == values::kKeyPgDwn) {
        key = ui::VKEY_NEXT;
      } else if (tokens[i] == values::kKeySpace) {
        key = ui::VKEY_SPACE;
      } else if (tokens[i] == values::kKeyTab) {
        key = ui::VKEY_TAB;
      } else if (tokens[i] == values::kKeyMediaNextTrack &&
                 should_parse_media_keys) {
        key = ui::VKEY_MEDIA_NEXT_TRACK;
      } else if (tokens[i] == values::kKeyMediaPlayPause &&
                 should_parse_media_keys) {
        key = ui::VKEY_MEDIA_PLAY_PAUSE;
      } else if (tokens[i] == values::kKeyMediaPrevTrack &&
                 should_parse_media_keys) {
        key = ui::VKEY_MEDIA_PREV_TRACK;
      } else if (tokens[i] == values::kKeyMediaStop &&
                 should_parse_media_keys) {
        key = ui::VKEY_MEDIA_STOP;
      } else if (tokens[i].size() == 1 && base::IsAsciiUpper(tokens[i][0])) {
        key = static_cast<ui::KeyboardCode>(ui::VKEY_A + (tokens[i][0] - 'A'));
      } else if (tokens[i].size() == 1 && base::IsAsciiDigit(tokens[i][0])) {
        key = static_cast<ui::KeyboardCode>(ui::VKEY_0 + (tokens[i][0] - '0'));
      } else {
        key = ui::VKEY_UNKNOWN;
        break;
      }
    } else {
      *error = ErrorUtils::FormatErrorMessageUTF16(errors::kInvalidKeyBinding,
                                                   base::NumberToString(index),
                                                   platform_key, accelerator);
      return ui::Accelerator();
    }
  }

  bool command = (modifiers & ui::EF_COMMAND_DOWN) != 0;
  bool ctrl = (modifiers & ui::EF_CONTROL_DOWN) != 0;
  bool alt = (modifiers & ui::EF_ALT_DOWN) != 0;
  bool shift = (modifiers & ui::EF_SHIFT_DOWN) != 0;

  // We support Ctrl+foo, Alt+foo, Ctrl+Shift+foo, Alt+Shift+foo, but not
  // Ctrl+Alt+foo and not Shift+foo either. For a more detailed reason why we
  // don't support Ctrl+Alt+foo see this article:
  // http://blogs.msdn.com/b/oldnewthing/archive/2004/03/29/101121.aspx.
  // On Mac Command can also be used in combination with Shift or on its own,
  // as a modifier.
  if (key == ui::VKEY_UNKNOWN || (ctrl && alt) || (command && alt) ||
      (shift && !ctrl && !alt && !command)) {
    *error = ErrorUtils::FormatErrorMessageUTF16(errors::kInvalidKeyBinding,
                                                 base::NumberToString(index),
                                                 platform_key, accelerator);
    return ui::Accelerator();
  }

  if (ui::MediaKeysListener::IsMediaKeycode(key) &&
      (shift || ctrl || alt || command)) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidKeyBindingMediaKeyWithModifier,
        base::NumberToString(index), platform_key, accelerator);
    return ui::Accelerator();
  }

  return ui::Accelerator(key, modifiers);
}

// For Mac, we convert "Ctrl" to "Command" and "MacCtrl" to "Ctrl". Other
// platforms leave the shortcut untouched.
std::string NormalizeShortcutSuggestion(const std::string& suggestion,
                                        const std::string& platform) {
  bool normalize = false;
  if (platform == values::kKeybindingPlatformMac) {
    normalize = true;
  } else if (platform == values::kKeybindingPlatformDefault) {
#if BUILDFLAG(IS_MAC)
    normalize = true;
#endif
  }

  if (!normalize) {
    return suggestion;
  }

  std::vector<std::string_view> tokens = base::SplitStringPiece(
      suggestion, "+", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (auto& token : tokens) {
    if (token == values::kKeyCtrl) {
      token = values::kKeyCommand;
    } else if (token == values::kKeyMacCtrl) {
      token = values::kKeyCtrl;
    }
  }
  return base::JoinString(tokens, "+");
}

}  // namespace

Command::Command() : global_(false) {}

Command::Command(const std::string& command_name,
                 const std::u16string& description,
                 const std::string& accelerator,
                 bool global)
    : command_name_(command_name), description_(description), global_(global) {
  if (!accelerator.empty()) {
    std::u16string error;
    accelerator_ = ParseImpl(accelerator, CommandPlatform(), 0,
                             !IsActionRelatedCommand(command_name), &error);
  }
}

Command::Command(const Command& other) = default;

Command::~Command() = default;

// static
std::string Command::CommandPlatform() {
#if BUILDFLAG(IS_WIN)
  return values::kKeybindingPlatformWin;
#elif BUILDFLAG(IS_MAC)
  return values::kKeybindingPlatformMac;
#elif BUILDFLAG(IS_CHROMEOS)
  return values::kKeybindingPlatformChromeOs;
#elif BUILDFLAG(IS_LINUX)
  return values::kKeybindingPlatformLinux;
#elif BUILDFLAG(IS_FUCHSIA)
  // TODO(crbug.com/40220501): Change this once we decide what string should be
  // used for Fuchsia.
  return values::kKeybindingPlatformLinux;
#elif BUILDFLAG(IS_DESKTOP_ANDROID)
  // For now, we use linux keybindings on desktop android.
  // TODO(https://crbug.com/356905053): Should this be ChromeOS keybindings?
  return values::kKeybindingPlatformLinux;
#else
#error Unsupported platform
#endif
}

// static
ui::Accelerator Command::StringToAccelerator(const std::string& accelerator,
                                             const std::string& command_name) {
  std::u16string error;
  ui::Accelerator parsed =
      ParseImpl(accelerator, Command::CommandPlatform(), 0,
                !IsActionRelatedCommand(command_name), &error);
  return parsed;
}

// static
std::string Command::AcceleratorToString(const ui::Accelerator& accelerator) {
  std::string shortcut;

  // Ctrl and Alt are mutually exclusive.
  if (accelerator.IsCtrlDown()) {
    shortcut += values::kKeyCtrl;
  } else if (accelerator.IsAltDown()) {
    shortcut += values::kKeyAlt;
  }
  if (!shortcut.empty()) {
    shortcut += values::kKeySeparator;
  }

  if (accelerator.IsCmdDown()) {
#if BUILDFLAG(IS_CHROMEOS)
    // Chrome OS treats the Search key like the Command key.
    shortcut += values::kKeySearch;
#else
    shortcut += values::kKeyCommand;
#endif
    shortcut += values::kKeySeparator;
  }

  if (accelerator.IsShiftDown()) {
    shortcut += values::kKeyShift;
    shortcut += values::kKeySeparator;
  }

  if (accelerator.key_code() >= ui::VKEY_0 &&
      accelerator.key_code() <= ui::VKEY_9) {
    shortcut += '0' + (accelerator.key_code() - ui::VKEY_0);
  } else if (accelerator.key_code() >= ui::VKEY_A &&
             accelerator.key_code() <= ui::VKEY_Z) {
    shortcut += 'A' + (accelerator.key_code() - ui::VKEY_A);
  } else {
    switch (accelerator.key_code()) {
      case ui::VKEY_OEM_COMMA:
        shortcut += values::kKeyComma;
        break;
      case ui::VKEY_OEM_PERIOD:
        shortcut += values::kKeyPeriod;
        break;
      case ui::VKEY_UP:
        shortcut += values::kKeyUp;
        break;
      case ui::VKEY_DOWN:
        shortcut += values::kKeyDown;
        break;
      case ui::VKEY_LEFT:
        shortcut += values::kKeyLeft;
        break;
      case ui::VKEY_RIGHT:
        shortcut += values::kKeyRight;
        break;
      case ui::VKEY_INSERT:
        shortcut += values::kKeyIns;
        break;
      case ui::VKEY_DELETE:
        shortcut += values::kKeyDel;
        break;
      case ui::VKEY_HOME:
        shortcut += values::kKeyHome;
        break;
      case ui::VKEY_END:
        shortcut += values::kKeyEnd;
        break;
      case ui::VKEY_PRIOR:
        shortcut += values::kKeyPgUp;
        break;
      case ui::VKEY_NEXT:
        shortcut += values::kKeyPgDwn;
        break;
      case ui::VKEY_SPACE:
        shortcut += values::kKeySpace;
        break;
      case ui::VKEY_TAB:
        shortcut += values::kKeyTab;
        break;
      case ui::VKEY_MEDIA_NEXT_TRACK:
        shortcut += values::kKeyMediaNextTrack;
        break;
      case ui::VKEY_MEDIA_PLAY_PAUSE:
        shortcut += values::kKeyMediaPlayPause;
        break;
      case ui::VKEY_MEDIA_PREV_TRACK:
        shortcut += values::kKeyMediaPrevTrack;
        break;
      case ui::VKEY_MEDIA_STOP:
        shortcut += values::kKeyMediaStop;
        break;
      default:
        return "";
    }
  }
  return shortcut;
}

// static
bool Command::IsMediaKey(const ui::Accelerator& accelerator) {
  if (accelerator.modifiers() != 0) {
    return false;
  }

  return ui::MediaKeysListener::IsMediaKeycode(accelerator.key_code());
}

// static
bool Command::IsActionRelatedCommand(const std::string& command_name) {
  return command_name == values::kActionCommandEvent ||
         command_name == values::kBrowserActionCommandEvent ||
         command_name == values::kPageActionCommandEvent;
}

bool Command::Parse(const base::Value::Dict& command,
                    const std::string& command_name,
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
      suggestions[values::kKeybindingPlatformDefault] = *suggested_key_string;
    } else {
      suggestions[values::kKeybindingPlatformDefault] = "";
    }
  }

  // Check if this is a global or a regular shortcut.
  bool global = command.FindBoolByDottedPath(keys::kGlobal).value_or(false);

  // Normalize the suggestions.
  for (auto iter = suggestions.begin(); iter != suggestions.end(); ++iter) {
    // Before we normalize Ctrl to Command we must detect when the developer
    // specified Command in the Default section, which will work on Mac after
    // normalization but only fail on other platforms when they try it out on
    // other platforms, which is not what we want.
    if (iter->first == values::kKeybindingPlatformDefault &&
        iter->second.find("Command+") != std::string::npos) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidKeyBinding, base::NumberToString(index),
          keys::kSuggestedKey, kCommandKeyNotSupported);
      return false;
    }

    suggestions[iter->first] =
        NormalizeShortcutSuggestion(iter->second, iter->first);
  }

  std::string platform = CommandPlatform();
  std::string key = platform;
  if (suggestions.find(key) == suggestions.end()) {
    key = values::kKeybindingPlatformDefault;
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
      // Note that we pass iter->first to pretend we are on a platform we're not
      // on.
      accelerator = ParseImpl(iter->second, iter->first, index,
                              !IsActionRelatedCommand(command_name), error);
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
      accelerator_ = accelerator;
      command_name_ = command_name;
      description_ = description;
      global_ = global;
    }
  }
  return true;
}

}  // namespace extensions
