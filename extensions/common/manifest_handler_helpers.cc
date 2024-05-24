// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handler_helpers.h"

#include <stddef.h>

#include <optional>
#include <string_view>

#include "base/check.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/icons/extension_icon_set.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

namespace errors = manifest_errors;

namespace manifest_handler_helpers {

std::vector<std::string_view> TokenizeDictionaryPath(std::string_view path) {
  return base::SplitStringPiece(path, ".", base::TRIM_WHITESPACE,
                                base::SPLIT_WANT_ALL);
}

bool NormalizeAndValidatePath(std::string* path) {
  return NormalizeAndValidatePath(*path, path);
}

bool NormalizeAndValidatePath(const std::string& path,
                              std::string* normalized_path) {
  size_t first_non_slash = path.find_first_not_of('/');
  if (first_non_slash == std::string::npos) {
    *normalized_path = "";
    return false;
  }

  *normalized_path = path.substr(first_non_slash);
  return true;
}

std::optional<int> LoadValidSizeFromString(const std::string& string_size) {
  int size = 0;
  bool is_valid = base::StringToInt(string_size, &size) && size > 0 &&
                  size <= extension_misc::EXTENSION_ICON_GIGANTOR * 4;
  return is_valid ? std::make_optional(size) : std::nullopt;
}

bool LoadIconsFromDictionary(const base::Value::Dict& icons_value,
                             ExtensionIconSet* icons,
                             std::u16string* error) {
  DCHECK(icons);
  DCHECK(error);
  for (auto entry : icons_value) {
    std::optional<int> size = LoadValidSizeFromString(entry.first);
    if (!size.has_value()) {
      *error = ErrorUtils::FormatErrorMessageUTF16(errors::kInvalidIconKey,
                                                   entry.first);
      return false;
    }
    std::string icon_path;
    if (!entry.second.is_string() ||
        !NormalizeAndValidatePath(entry.second.GetString(), &icon_path)) {
      *error = ErrorUtils::FormatErrorMessageUTF16(errors::kInvalidIconPath,
                                                   entry.first);
      return false;
    }

    icons->Add(size.value(), icon_path);
  }
  return true;
}

}  // namespace manifest_handler_helpers

}  // namespace extensions
