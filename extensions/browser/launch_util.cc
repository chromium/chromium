// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/launch_util.h"

#include "build/build_config.h"
#include "extensions/browser/bookmark_app_util.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension.h"

namespace extensions {
namespace {

// A preference set by the the NTP to persist the desired launch container type
// used for apps.
constexpr char kPrefLaunchType[] = "launchType";

}  // namespace

LaunchType GetLaunchType(const ExtensionPrefs* prefs,
                         const Extension* extension) {
  if (!extension) {
    return LAUNCH_TYPE_INVALID;
  }
  LaunchType result = LAUNCH_TYPE_DEFAULT;

  int value = GetLaunchTypePrefValue(prefs, extension->id());
  if (value >= LAUNCH_TYPE_FIRST && value < NUM_LAUNCH_TYPES) {
    result = static_cast<LaunchType>(value);
  }

  // Force hosted apps that are not locally installed to open in tabs.
  if (extension->is_hosted_app() &&
      !BookmarkAppIsLocallyInstalled(prefs, extension)) {
    result = LAUNCH_TYPE_REGULAR;
  } else if (result == LAUNCH_TYPE_PINNED) {
    result = LAUNCH_TYPE_REGULAR;
  } else if (result == LAUNCH_TYPE_FULLSCREEN) {
    result = LAUNCH_TYPE_WINDOW;
  }
  return result;
}

LaunchType GetLaunchTypePrefValue(const ExtensionPrefs* prefs,
                                  const std::string& extension_id) {
  int value = LAUNCH_TYPE_INVALID;
  return prefs->ReadPrefAsInteger(extension_id, kPrefLaunchType, &value)
             ? static_cast<LaunchType>(value)
             : LAUNCH_TYPE_INVALID;
}

void SetLaunchTypePrefValue(content::BrowserContext* context,
                            const std::string& extension_id,
                            LaunchType launch_type) {
  DCHECK(launch_type >= LAUNCH_TYPE_FIRST && launch_type < NUM_LAUNCH_TYPES);

  ExtensionPrefs::Get(context)->UpdateExtensionPref(
      extension_id, kPrefLaunchType,
      base::Value(static_cast<int>(launch_type)));
}

bool LaunchesInWindow(content::BrowserContext* context,
                      const Extension* extension) {
  return GetLaunchType(ExtensionPrefs::Get(context), extension) ==
         LAUNCH_TYPE_WINDOW;
}

}  // namespace extensions
