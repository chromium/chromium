// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLER_HELPERS_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLER_HELPERS_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/values.h"

class ExtensionIconSet;

namespace base {
class FilePath;
}

namespace extensions {

class Extension;

namespace manifest_handler_helpers {

// Tokenize a dictionary path.
std::vector<std::string_view> TokenizeDictionaryPath(std::string_view path);

// Returns an optional size as an `int` from a valid input string.
std::optional<int> LoadValidSizeFromString(const std::string& string_size);

// Loads icon paths defined in dictionary `icons_value` into ExtensionIconSet
// `icons`. `icons_value` is a dictionary value {icon size -> icon path}.
// Returns success. If load fails, `error` will be set. Files which can't be
// used as icons will be ignored and a warning will be added to `warnings`.
bool LoadIconsFromDictionary(const Extension& extension,
                             const base::Value::Dict& icons_value,
                             ExtensionIconSet* icons,
                             std::u16string* error,
                             std::vector<std::string>* warnings);

// Returns true if the given path's mime type is supported to be used as an
// extension image, such as an icon or a theme.
bool IsSupportedExtensionImageMimeType(const base::FilePath& relative_path);

// Returns true if the given path's mime type can be used for an icon.
bool IsIconMimeTypeValid(const base::FilePath& relative_path);

}  // namespace manifest_handler_helpers

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLER_HELPERS_H_
