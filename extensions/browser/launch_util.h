// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_LAUNCH_UTIL_H_
#define EXTENSIONS_BROWSER_LAUNCH_UTIL_H_

#include <string>

#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "extensions/common/constants.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class Extension;
class ExtensionPrefs;

// Gets the launch type preference. If no preference is set, returns
// LAUNCH_TYPE_DEFAULT.
// Returns LAUNCH_TYPE_WINDOW if there's no preference and
// bookmark apps are enabled.
LaunchType GetLaunchType(const ExtensionPrefs* prefs,
                         const Extension* extension);

// Returns the LaunchType that is set in the prefs. Returns LAUNCH_TYPE_INVALID
// if no value is set in prefs.
LaunchType GetLaunchTypePrefValue(const ExtensionPrefs* prefs,
                                  const std::string& extension_id);

// Sets an extension's launch type preference.
void SetLaunchTypePrefValue(content::BrowserContext* context,
                            const std::string& extension_id,
                            LaunchType launch_type);

// Whether `extension` will launch in a window.
bool LaunchesInWindow(content::BrowserContext* context,
                      const Extension* extension);

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_LAUNCH_UTIL_H_
