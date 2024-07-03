// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAC_ICLOUD_KEYCHAIN_H_
#define DEVICE_FIDO_MAC_ICLOUD_KEYCHAIN_H_

#include <memory>
#include <optional>

#include "base/component_export.h"

namespace device {

class FidoDiscoveryBase;

namespace fido::icloud_keychain {

// kFakeNSWindowForTesting can be passed to `NewDiscovery` by tests to indicate
// that they don't have an `NSWindow` to use.
constexpr uintptr_t kFakeNSWindowForTesting = 1;

// IsSupported returns true if iCloud Keychain can be used. This is constant for
// the lifetime of the process.
COMPONENT_EXPORT(DEVICE_FIDO) bool IsSupported();

// NewDiscovery returns a discovery that will immediately find an iCloud
// Keychain authenticator. It is only valid to call this if `IsSupported`
// returned true. It takes an `NSWindow*` to indicate the window which the
// system UI would appear on top of. Since this header file can be included
// by C++ code, the pointer is passed as an integer (see crbug.com/1433041).
//
// (By passing as a uintptr_t, the code assumes that the NSWindow has not
// already been destroyed. This should be true since discovery objects are
// created synchronously after getting the `BrowserWindow` of a `WebContents`.)
COMPONENT_EXPORT(DEVICE_FIDO)
std::unique_ptr<FidoDiscoveryBase> NewDiscovery(uintptr_t ns_window);

// Returns true if the user has granted passkey enumeration permission, false if
// they have denied it, and `nullopt` if the user hasn't made a choice.
COMPONENT_EXPORT(DEVICE_FIDO)
std::optional<bool> HasPermission();

}  // namespace fido::icloud_keychain
}  // namespace device

#endif  // DEVICE_FIDO_MAC_ICLOUD_KEYCHAIN_H_
