// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_INSTALL_FLAG_H_
#define EXTENSIONS_BROWSER_INSTALL_FLAG_H_

namespace extensions {

// Flags used when installing an extension, through ExtensionService and
// ExtensionPrefs and beyond.
enum InstallFlag {
  kInstallFlagNone = 0,

  // The requirements of the extension weren't met (for example graphics
  // capabilities).
  kInstallFlagHasRequirementErrors = 1 << 0,

  // Extension is blocklisted for being malware.
  kInstallFlagIsBlocklistedForMalware = 1 << 1,

  // This is an ephemeral app.
  kInstallFlagIsEphemeral_Deprecated = 1 << 2,

  // Install the extension immediately, don't wait until idle.
  kInstallFlagInstallImmediately = 1 << 3,

  // Do not sync the installed extension.
  kInstallFlagDoNotSync = 1 << 4,

  // The user clicked through the install friction dialog when the extension is
  // not included in the Enhanced Safe Browsing CRX allowlist and the user has
  // enabled Enhanced Protection.
  kInstallFlagBypassedSafeBrowsingFriction = 1 << 5,
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_INSTALL_FLAG_H_
