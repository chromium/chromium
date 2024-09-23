// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLER_HELPERS_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLER_HELPERS_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/values.h"

class ExtensionIconSet;

namespace extensions::manifest_handler_helpers {

// Tokenize a dictionary path.
std::vector<std::string_view> TokenizeDictionaryPath(std::string_view path);

// Strips leading slashes from the file path. Returns true iff the final path is
// not empty.
bool NormalizeAndValidatePath(std::string* path);
bool NormalizeAndValidatePath(const std::string& path,
                              std::string* normalized_path);

// Returns an optional size as an `int` from a valid input string.
std::optional<int> LoadValidSizeFromString(const std::string& string_size);

// Loads icon paths defined in dictionary |icons_value| into ExtensionIconSet
// `icons`. `icons_value` is a dictionary value {icon size -> icon path}.
// Returns success. If load fails, `error` will be set.
bool LoadIconsFromDictionary(const base::Value::Dict& icons_value,
                             ExtensionIconSet* icons,
                             std::u16string* error);

}  // namespace extensions::manifest_handler_helpers

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLER_HELPERS_H_
