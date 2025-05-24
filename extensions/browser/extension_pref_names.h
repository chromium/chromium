// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_PREF_NAMES_H_
#define EXTENSIONS_BROWSER_EXTENSION_PREF_NAMES_H_

namespace extensions {

// An installation parameter bundled with an extension.
inline constexpr char kPrefInstallParameter[] = "install_parameter";

// A preference that indicates whether the extension was installed from the
// Chrome Web Store.
inline constexpr char kPrefFromWebStore[] = "from_webstore";

// A preference that indicates whether the extension was installed as a
// default app.
inline constexpr char kPrefWasInstalledByDefault[] = "was_installed_by_default";

// A preference that indicates whether the extension was installed as an
// OEM app.
inline constexpr char kPrefWasInstalledByOem[] = "was_installed_by_oem";

// A preference that indicates when an extension was first installed.
// This preference is created when an extension is installed and deleted when
// it is removed. It is NOT updated when the extension is updated.
inline constexpr char kPrefFirstInstallTime[] = "first_install_time";

// A preference that indicates when an extension was last installed/updated.
inline constexpr char kPrefLastUpdateTime[] = "last_update_time";

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_PREF_NAMES_H_
