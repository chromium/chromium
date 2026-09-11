// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webauthn/model/ios_device_authorization_client.h"

#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/common/credential_provider/device_authorization_key_store.h"

IOSDeviceAuthorizationClient::IOSDeviceAuthorizationClient() = default;

IOSDeviceAuthorizationClient::~IOSDeviceAuthorizationClient() = default;

std::optional<webauthn::DeviceAuthorizationKeys>
IOSDeviceAuthorizationClient::GetCachedKeys(const GaiaId& gaia_id) {
  return GetDeviceAuthorizationKeys(gaia_id.ToString());
}

bool IOSDeviceAuthorizationClient::StoreKeys(
    const GaiaId& gaia_id,
    const webauthn::DeviceAuthorizationKeys& keys) {
  return StoreDeviceAuthorizationKeys(gaia_id.ToString(), keys);
}

void IOSDeviceAuthorizationClient::CreateDeviceAuthorizationRequest(
    webauthn::CreateDeviceAuthRequestCallback callback) {
  // TODO(crbug.com/405036154): Implement.
}
