// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_DEVICE_AUTHORIZATION_KEY_STORE_H_
#define IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_DEVICE_AUTHORIZATION_KEY_STORE_H_

#import <Foundation/Foundation.h>

#import <optional>
#import <string>

#import "components/webauthn/core/browser/device_authorization/proto/device_authorization_key.pb.h"

// Type alias for device authorization keys proto message.
using DeviceAuthorizationKey =
    sync_pb::GetDeviceAuthorizationKeyResponse::DeviceAuthorizationKey;
using DeviceAuthorizationKeys =
    sync_pb::GetDeviceAuthorizationKeyResponse::DeviceAuthorizationKeys;

// Saves or updates all device authorization keys for the account identified by
// `gaia_id` in the local iOS Keychain. Overwrites any existing keys for this
// account. Returns true on success, false if any error occurs.
// TODO(crbug.com/405036154): Support `cache_version` parameter and
// invalidation.
bool StoreDeviceAuthorizationKeys(const std::string& gaia_id,
                                  const DeviceAuthorizationKeys& keys);

// Retrieves all stored device authorization keys for the account identified by
// `gaia_id` from the local iOS Keychain. Returns `std::nullopt` if no keys are
// found or if an error occurs.
std::optional<DeviceAuthorizationKeys> GetDeviceAuthorizationKeys(
    const std::string& gaia_id);

#endif  // IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_DEVICE_AUTHORIZATION_KEY_STORE_H_
