// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAC_ICLOUD_KEYCHAIN_H_
#define DEVICE_FIDO_MAC_ICLOUD_KEYCHAIN_H_

#include <os/availability.h>

#include <memory>
#include <optional>
#include <string>

#include "base/apple/owned_objc.h"
#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "build/build_config.h"

static_assert(BUILDFLAG(IS_MAC));

namespace base {
class FilePath;
}  // namespace base

namespace device {

class FidoDiscoveryBase;

namespace fido::icloud_keychain {

// IsSupported returns true if iCloud Keychain can be used. This is constant for
// the lifetime of the process.
COMPONENT_EXPORT(DEVICE_FIDO) bool IsSupported();

// NewDiscovery returns a discovery that will immediately find an iCloud
// Keychain authenticator. It is only valid to call this if `IsSupported`
// returned true.
COMPONENT_EXPORT(DEVICE_FIDO)
std::unique_ptr<FidoDiscoveryBase> NewDiscovery(
    base::apple::WeakNSWindow ns_window);

// Returns true if the user has granted passkey enumeration permission, false if
// they have denied it, and `nullopt` if the user hasn't made a choice.
COMPONENT_EXPORT(DEVICE_FIDO)
std::optional<bool> HasPermission();

// SupportsLargeBlob returns true if the current macOS version and the chrome
// feature flag both support large blob extension for iCloud Keychain.
COMPONENT_EXPORT(DEVICE_FIDO) bool SupportsLargeBlob();

// Returns true if iCloud Keychain is configured for passkeys according to the
// macOS AuthenticationServices API.
COMPONENT_EXPORT(DEVICE_FIDO)
API_AVAILABLE(macos(26.2))
bool IsConfiguredForPasskeys();

// Overrides the set of localized Apple Passwords `providerName` strings for the
// lifetime of this object.
class COMPONENT_EXPORT(DEVICE_FIDO)
    ScopedApplePasswordsProviderNamesForTesting {
 public:
  explicit ScopedApplePasswordsProviderNamesForTesting(
      base::flat_set<std::string> names);
  ScopedApplePasswordsProviderNamesForTesting(
      const ScopedApplePasswordsProviderNamesForTesting&) = delete;
  ScopedApplePasswordsProviderNamesForTesting& operator=(
      const ScopedApplePasswordsProviderNamesForTesting&) = delete;
  ~ScopedApplePasswordsProviderNamesForTesting();
};

// Returns the localized display names discovered for the macOS `.app` bundle
// at `app_bundle_path` (from `NSFileManager`, `NSBundle`, and
// `Contents/Resources/InfoPlist.loctable`). Exposed for unit testing.
COMPONENT_EXPORT(DEVICE_FIDO)
base::flat_set<std::string> GetLocalizedAppNamesForTesting(
    const base::FilePath& app_bundle_path);

}  // namespace fido::icloud_keychain
}  // namespace device

#endif  // DEVICE_FIDO_MAC_ICLOUD_KEYCHAIN_H_
