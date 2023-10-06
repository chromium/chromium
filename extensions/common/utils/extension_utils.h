// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_UTILS_EXTENSION_UTILS_H_
#define EXTENSIONS_COMMON_UTILS_EXTENSION_UTILS_H_

#include <string>

namespace extensions {

class Extension;

// Returns the extension ID or an empty string if null.
const std::string& MaybeGetExtensionId(const Extension* extension);

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_UTILS_EXTENSION_UTILS_H_
