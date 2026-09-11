// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBAUTHN_MODEL_IOS_DEVICE_AUTHORIZATION_CLIENT_H_
#define IOS_CHROME_BROWSER_WEBAUTHN_MODEL_IOS_DEVICE_AUTHORIZATION_CLIENT_H_

#import <optional>

#import "components/webauthn/core/browser/device_authorization/device_authorization_client.h"

class GaiaId;

// iOS implementation of the `DeviceAuthorizationClient` interface.
class IOSDeviceAuthorizationClient
    : public webauthn::DeviceAuthorizationClient {
 public:
  IOSDeviceAuthorizationClient();
  IOSDeviceAuthorizationClient(const IOSDeviceAuthorizationClient&) = delete;
  IOSDeviceAuthorizationClient& operator=(const IOSDeviceAuthorizationClient&) =
      delete;
  ~IOSDeviceAuthorizationClient() override;

  // webauthn::DeviceAuthorizationClient implementation.
  std::optional<webauthn::DeviceAuthorizationKeys> GetCachedKeys(
      const GaiaId& gaia_id) override;
  bool StoreKeys(const GaiaId& gaia_id,
                 const webauthn::DeviceAuthorizationKeys& keys) override;
};

#endif  // IOS_CHROME_BROWSER_WEBAUTHN_MODEL_IOS_DEVICE_AUTHORIZATION_CLIENT_H_
