// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_ISOLATED_MODE_EXTENSION_SETTINGS_H_
#define EXTENSIONS_BROWSER_ISOLATED_MODE_EXTENSION_SETTINGS_H_

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

// Values for the `pref_names::kIsolatedModeUserInstalledExtensionsSettings`
// preference and `IsolatedModeUserInstalledExtensionsSettings` policy.
//
// Please note that the integer values must match
// IsolatedModeUserInstalledExtensionsSettings.yaml and must not be renumbered.
enum class IsolatedModeUserInstalledExtensionsSetting {
  kEnabledByDefault = 0,
  kDisabledByDefault = 1,
  kBlocked = 2,
  kMaxValue = kBlocked,
};

// Values for the `pref_names::kIsolatedModeForceInstalledExtensionsSettings`
// preference and `IsolatedModeForceInstalledExtensionsSettings` policy.
//
// Please note that the integer values must match
// IsolatedModeForceInstalledExtensionsSettings.yaml and must not be renumbered.
enum class IsolatedModeForceInstalledExtensionsSetting {
  kAlwaysEnabled = 0,
  kEnabledByDefault = 1,
  kMaxValue = kEnabledByDefault,
};

// Returns the policy setting controlling force-installed extension behavior in
// Enterprise Isolated Mode. Returns `kAlwaysEnabled` if the preference is unset
// or not managed by policy.
IsolatedModeForceInstalledExtensionsSetting
GetIsolatedModeForceInstalledExtensionsSetting(
    content::BrowserContext* context);

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_ISOLATED_MODE_EXTENSION_SETTINGS_H_
