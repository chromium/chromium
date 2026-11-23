// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/isolated_mode_extension_settings.h"

#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/pref_names.h"

namespace extensions {

IsolatedModeForceInstalledExtensionsSetting
GetIsolatedModeForceInstalledExtensionsSetting(
    content::BrowserContext* context) {
  const PrefService* prefs = user_prefs::UserPrefs::Get(context);
  if (!prefs ||
      !prefs->IsManagedPreference(
          pref_names::kIsolatedModeForceInstalledExtensionsSettings)) {
    return IsolatedModeForceInstalledExtensionsSetting::kAlwaysEnabled;
  }
  return static_cast<IsolatedModeForceInstalledExtensionsSetting>(
      prefs->GetInteger(
          pref_names::kIsolatedModeForceInstalledExtensionsSettings));
}

}  // namespace extensions
