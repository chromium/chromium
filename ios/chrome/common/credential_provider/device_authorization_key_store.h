// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_DEVICE_AUTHORIZATION_KEY_STORE_H_
#define IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_DEVICE_AUTHORIZATION_KEY_STORE_H_

#import <Foundation/Foundation.h>

#import <optional>
#import <string>

#import "components/webauthn/core/browser/device_authorization/device_authorization_types.h"

// Synchronously saves or updates all device authorization keys for the account
// identified by `gaia_id` in the local iOS Keychain. Overwrites any existing
// keys for this account. Returns true on success, false if any error occurs.
//
// Should not be called on the main thread, as the iOS Keychain APIs may
// block the calling thread.
bool StoreDeviceAuthorizationKeys(
    const std::string& gaia_id,
    const webauthn::CachedDeviceAuthorizationKeys& keys);

// Synchronously retrieves all stored device authorization keys for the account
// identified by `gaia_id` from the local iOS Keychain. Returns `std::nullopt`
// if no keys are found or if an error occurs.
//
// Should not be called on the main thread, as the iOS Keychain APIs may
// block the calling thread.
std::optional<webauthn::CachedDeviceAuthorizationKeys>
GetDeviceAuthorizationKeys(const std::string& gaia_id);

#endif  // IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_DEVICE_AUTHORIZATION_KEY_STORE_H_
