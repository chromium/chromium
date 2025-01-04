// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_INSTALL_PREFS_HELPER_H_
#define EXTENSIONS_BROWSER_INSTALL_PREFS_HELPER_H_

#include <string>

#include "extensions/common/extension_id.h"

namespace base {
class Time;
}  // namespace base

namespace extensions {

class ExtensionPrefs;

// The installation parameter associated with the extension.
std::string GetInstallParam(const ExtensionPrefs* prefs,
                            const ExtensionId& extension_id);
void SetInstallParam(ExtensionPrefs* prefs,
                     const ExtensionId& extension_id,
                     std::string value);

// Returns true if the extension was installed from the Chrome Web Store.
bool IsFromWebStore(const ExtensionPrefs* prefs,
                    const ExtensionId& extension_id);

// Returns true if the extension was installed as a default app.
bool WasInstalledByDefault(const ExtensionPrefs* prefs,
                           const ExtensionId& extension_id);

// Returns true if the extension was installed as an oem app.
bool WasInstalledByOem(const ExtensionPrefs* prefs,
                       const ExtensionId& extension_id);

// Returns the original installation time of an extension.
// Returns base::Time() if the installation time could not be parsed or
// found.
base::Time GetFirstInstallTime(const ExtensionPrefs* prefs,
                               const ExtensionId& extension_id);

// Returns the installation/last update time of an extension.
// Returns base::Time() if the installation time could not be parsed or
// found.
base::Time GetLastUpdateTime(const ExtensionPrefs* prefs,
                             const ExtensionId& extension_id);

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_INSTALL_PREFS_HELPER_H_
