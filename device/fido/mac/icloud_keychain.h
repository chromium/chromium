// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAC_ICLOUD_KEYCHAIN_H_
#define DEVICE_FIDO_MAC_ICLOUD_KEYCHAIN_H_

#include <memory>
#include <optional>

#include "base/apple/owned_objc.h"
#include "base/component_export.h"
#include "build/build_config.h"

static_assert(BUILDFLAG(IS_MAC));

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

}  // namespace fido::icloud_keychain
}  // namespace device

#endif  // DEVICE_FIDO_MAC_ICLOUD_KEYCHAIN_H_
