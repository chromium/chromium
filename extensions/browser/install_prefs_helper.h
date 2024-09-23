// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_INSTALL_PREFS_HELPER_H_
#define EXTENSIONS_BROWSER_INSTALL_PREFS_HELPER_H_

#include <string>

#include "extensions/common/extension_id.h"

namespace extensions {

class ExtensionPrefs;

// The installation parameter associated with the extension.
std::string GetInstallParam(const ExtensionPrefs* prefs,
                            const ExtensionId& extension_id);
void SetInstallParam(ExtensionPrefs* prefs,
                     const ExtensionId& extension_id,
                     std::string value);

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_INSTALL_PREFS_HELPER_H_
