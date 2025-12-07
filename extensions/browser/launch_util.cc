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
    return LaunchType::kInvalid;
  }
  LaunchType result = LaunchType::kDefault;

  LaunchType value = GetLaunchTypePrefValue(prefs, extension->id());
  if (value != LaunchType::kInvalid) {
    result = value;
  }

  // Force hosted apps that are not locally installed to open in tabs.
  if (extension->is_hosted_app() &&
      !BookmarkAppIsLocallyInstalled(prefs, extension)) {
    result = LaunchType::kRegular;
  } else if (result == LaunchType::kPinned) {
    result = LaunchType::kRegular;
  } else if (result == LaunchType::kFullscreen) {
    result = LaunchType::kWindow;
  }
  return result;
}

LaunchType GetLaunchTypePrefValue(const ExtensionPrefs* prefs,
                                  const std::string& extension_id) {
  int value;
  if (prefs->ReadPrefAsInteger(extension_id, kPrefLaunchType, &value) &&
      value >= static_cast<int>(LaunchType::kFirst) &&
      value < static_cast<int>(LaunchType::kNumLaunchTypes)) {
    return static_cast<LaunchType>(value);
  }
  return LaunchType::kInvalid;
}

void SetLaunchTypePrefValue(content::BrowserContext* context,
                            const std::string& extension_id,
                            LaunchType launch_type) {
  DCHECK(launch_type >= LaunchType::kFirst &&
         launch_type < LaunchType::kNumLaunchTypes);

  ExtensionPrefs::Get(context)->UpdateExtensionPref(
      extension_id, kPrefLaunchType,
      base::Value(static_cast<int>(launch_type)));
}

bool LaunchesInWindow(content::BrowserContext* context,
                      const Extension* extension) {
  return GetLaunchType(ExtensionPrefs::Get(context), extension) ==
         LaunchType::kWindow;
}

}  // namespace extensions
