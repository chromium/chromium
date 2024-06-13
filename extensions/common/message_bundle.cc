// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/message_bundle.h"

#include <memory>
#include <string>
#include <vector>

#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/i18n/rtl.h"
#include "base/lazy_instance.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_l10n_util.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

namespace errors = manifest_errors;

const char MessageBundle::kContentKey[] = "content";
const char MessageBundle::kMessageKey[] = "message";
const char MessageBundle::kPlaceholdersKey[] = "placeholders";

const char MessageBundle::kPlaceholderBegin[] = "$";
const char MessageBundle::kPlaceholderEnd[] = "$";
const char MessageBundle::kMessageBegin[] = "__MSG_";
const char MessageBundle::kMessageEnd[] = "__";

// Reserved messages names.
const char MessageBundle::kUILocaleKey[] = "@@ui_locale";
const char MessageBundle::kBidiDirectionKey[] = "@@bidi_dir";
const char MessageBundle::kBidiReversedDirectionKey[] = "@@bidi_reversed_dir";
const char MessageBundle::kBidiStartEdgeKey[] = "@@bidi_start_edge";
const char MessageBundle::kBidiEndEdgeKey[] = "@@bidi_end_edge";
const char MessageBundle::kExtensionIdKey[] = "@@extension_id";

// Reserved messages values.
const char MessageBundle::kBidiLeftEdgeValue[] = "left";
const char MessageBundle::kBidiRightEdgeValue[] = "right";

// Formats message in case we encounter a bad formed key in the JSON object.
// Returns false and sets |error| to actual error message.
static bool BadKeyMessage(const std::string& name, std::string* error) {
  *error = base::StringPrintf(
      "Name of a key \"%s\" is invalid. Only ASCII [a-z], "
      "[A-Z], [0-9] and \"_\" are allowed.",
      name.c_str());
  return false;
}

// static
MessageBundle* MessageBundle::Create(const CatalogVector& locale_catalogs,
                                     std::string* error) {
  std::unique_ptr<MessageBundle> message_bundle(new MessageBundle);
  if (!message_bundle->Init(locale_catalogs, error)) {
    return nullptr;
  }

  return message_bundle.release();
}

bool MessageBundle::Init(const CatalogVector& locale_catalogs,
                         std::string* error) {
  dictionary_.clear();

  for (const auto& catalog : base::Reversed(locale_catalogs)) {
    for (auto message_it : catalog) {
      std::string key(base::ToLowerASCII(message_it.first));
      if (!IsValidName(message_it.first)) {
        return BadKeyMessage(key, error);
      }
      std::string value;
      if (!GetMessageValue(message_it.first, message_it.second, &value,
                           error)) {
        return false;
      }
      // Keys are not case-sensitive.
      dictionary_[key] = value;
    }
  }

  if (!AppendReservedMessagesForLocale(
          extension_l10n_util::CurrentLocaleOrDefault(), error)) {
    return false;
  }

  return true;
}

bool MessageBundle::AppendReservedMessagesForLocale(
    const std::string& app_locale, std::string* error) {
  SubstitutionMap append_messages;
  append_messages[kUILocaleKey] = app_locale;

  // Calling base::i18n::GetTextDirection on non-UI threads doesn't seems safe,
  // so we use GetTextDirectionForLocale instead.
  if (base::i18n::GetTextDirectionForLocale(app_locale.c_str()) ==
      base::i18n::RIGHT_TO_LEFT) {
    append_messages[kBidiDirectionKey] = "rtl";
    append_messages[kBidiReversedDirectionKey] = "ltr";
    append_messages[kBidiStartEdgeKey] = kBidiRightEdgeValue;
    append_messages[kBidiEndEdgeKey] = kBidiLeftEdgeValue;
  } else {
    append_messages[kBidiDirectionKey] = "ltr";
    append_messages[kBidiReversedDirectionKey] = "rtl";
    append_messages[kBidiStartEdgeKey] = kBidiLeftEdgeValue;
    append_messages[kBidiEndEdgeKey] = kBidiRightEdgeValue;
  }

  // Add all reserved messages to the dictionary, but check for collisions.
  auto it = append_messages.begin();
  for (; it != append_messages.end(); ++it) {
    if (base::Contains(dictionary_, it->first)) {
      *error = ErrorUtils::FormatErrorMessage(
          errors::kReservedMessageFound, it->first);
      return false;
    } else {
      dictionary_[it->first] = it->second;
    }
  }

  return true;
}

bool MessageBundle::GetMessageValue(const std::string& key,
                                    const base::Value& name_value,
                                    std::string* value,
                                    std::string* error) const {
  // Get the top level tree for given key (name part).
  const base::Value::Dict* name_tree = name_value.GetIfDict();
  if (!name_tree) {
    *error = base::StringPrintf("Not a valid tree for key %s.", key.c_str());
    return false;
  }
  // Extract message from it.
  const std::string* str = name_tree->FindString(kMessageKey);
  if (!str) {
    *error = base::StringPrintf(
        "There is no \"%s\" element for key %s.", kMessageKey, key.c_str());
    return false;
  }
  *value = *str;

  SubstitutionMap placeholders;
  if (!GetPlaceholders(*name_tree, key, &placeholders, error)) {
    return false;
  }

  if (!ReplacePlaceholders(placeholders, value, error)) {
    return false;
  }

  return true;
}

MessageBundle::MessageBundle() {
}

bool MessageBundle::GetPlaceholders(const base::Value::Dict& name_tree,
                                    const std::string& name_key,
                                    SubstitutionMap* placeholders,
                                    std::string* error) const {
  if (!name_tree.Find(kPlaceholdersKey)) {
    return true;
  }

  const base::Value::Dict* placeholders_tree =
      name_tree.FindDict(kPlaceholdersKey);
  if (!placeholders_tree) {
    *error = base::StringPrintf("Not a valid \"%s\" element for key %s.",
                                kPlaceholdersKey, name_key.c_str());
    return false;
  }

  for (auto it : *placeholders_tree) {
    const std::string& content_key(it.first);
    if (!IsValidName(content_key)) {
      return BadKeyMessage(content_key, error);
    }
    const base::Value::Dict* placeholder = it.second.GetIfDict();
    if (!placeholder) {
      *error = base::StringPrintf("Invalid placeholder %s for key %s",
                                  content_key.c_str(),
                                  name_key.c_str());
      return false;
    }
    const std::string* content = placeholder->FindString(kContentKey);
    if (!content) {
      *error = base::StringPrintf("Invalid \"%s\" element for key %s.",
                                  kContentKey, name_key.c_str());
      return false;
    }
    (*placeholders)[base::ToLowerASCII(content_key)] = *content;
  }

  return true;
}

bool MessageBundle::ReplacePlaceholders(const SubstitutionMap& placeholders,
                                        std::string* message,
                                        std::string* error) const {
  return ReplaceVariables(placeholders,
                          kPlaceholderBegin,
                          kPlaceholderEnd,
                          message,
                          error);
}

bool MessageBundle::ReplaceMessages(std::string* text,
                                    std::string* error) const {
  return ReplaceMessagesWithExternalDictionary(dictionary_, text, error);
}

MessageBundle::~MessageBundle() {
}

// static
bool MessageBundle::ReplaceMessagesWithExternalDictionary(
    const SubstitutionMap& dictionary, std::string* text, std::string* error) {
  return ReplaceVariables(dictionary, kMessageBegin, kMessageEnd, text, error);
}

// static
bool MessageBundle::ReplaceVariables(const SubstitutionMap& variables,
                                     const std::string& var_begin_delimiter,
                                     const std::string& var_end_delimiter,
                                     std::string* message,
                                     std::string* error) {
  std::string::size_type beg_index = 0;
  const std::string::size_type var_begin_delimiter_size =
    var_begin_delimiter.size();
  while (true) {
    beg_index = message->find(var_begin_delimiter, beg_index);
    if (beg_index == std::string::npos) {
      return true;
    }

    // Advance it immediately to the begining of possible variable name.
    beg_index += var_begin_delimiter_size;
    if (beg_index >= message->size()) {
      return true;
    }
    std::string::size_type end_index =
        message->find(var_end_delimiter, beg_index);
    if (end_index == std::string::npos) {
      return true;
    }

    // Looking for 1 in substring of ...$1$....
    const std::string& var_name =
      message->substr(beg_index, end_index - beg_index);
    if (!IsValidName(var_name)) {
      continue;
    }
    auto it = variables.find(base::ToLowerASCII(var_name));
    if (it == variables.end()) {
      *error = base::StringPrintf("Variable %s%s%s used but not defined.",
                                  var_begin_delimiter.c_str(),
                                  var_name.c_str(),
                                  var_end_delimiter.c_str());
      return false;
    }

    // Replace variable with its value.
    std::string value = it->second;
    message->replace(beg_index - var_begin_delimiter_size,
                     end_index - beg_index + var_begin_delimiter_size +
                       var_end_delimiter.size(),
                     value);

    // And position pointer to after the replacement.
    beg_index += value.size() - var_begin_delimiter_size;
  }
}

// static
bool MessageBundle::IsValidName(const std::string& name) {
  if (name.empty()) {
    return false;
  }

  for (const auto& c : name) {
    // Allow only ascii 0-9, a-z, A-Z, and _ in the name.
    if (!base::IsAsciiAlpha(c) && !base::IsAsciiDigit(c) && c != '_' &&
        c != '@') {
      return false;
    }
  }

  return true;
}

// Dictionary interface.

std::string MessageBundle::GetL10nMessage(const std::string& name) const {
  return GetL10nMessage(name, dictionary_);
}

// static
std::string MessageBundle::GetL10nMessage(const std::string& name,
                                          const SubstitutionMap& dictionary) {
  auto it = dictionary.find(base::ToLowerASCII(name));
  if (it != dictionary.end()) {
    return it->second;
  }

  return std::string();
}

}  // namespace extensions
