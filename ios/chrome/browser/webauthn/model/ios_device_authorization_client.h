// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBAUTHN_MODEL_IOS_DEVICE_AUTHORIZATION_CLIENT_H_
#define IOS_CHROME_BROWSER_WEBAUTHN_MODEL_IOS_DEVICE_AUTHORIZATION_CLIENT_H_

#import <memory>

#import "base/memory/weak_ptr.h"
#import "components/webauthn/core/browser/device_authorization/device_authorization_client.h"
#import "components/webauthn/ios/passkey_types.h"

@class NSError;

class GaiaId;
class PasskeyKeychainProvider;

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
  void GetCachedKeys(const GaiaId& gaia_id,
                     webauthn::GetCachedKeysCallback callback) override;
  void StoreKeys(const GaiaId& gaia_id,
                 const webauthn::CachedDeviceAuthorizationKeys& keys,
                 webauthn::StoreKeysCallback callback) override;
  void PopulatePlatformData(
      const GaiaId& gaia_id,
      sync_pb::GetDeviceAuthorizationKeyRequest request,
      webauthn::PopulatePlatformDataCallback callback) override;

 private:
  // Invoked when passkey trusted vault keys have been fetched.
  void OnPasskeyKeysFetched(sync_pb::GetDeviceAuthorizationKeyRequest request,
                            webauthn::PopulatePlatformDataCallback callback,
                            webauthn::SharedKeyList keys,
                            NSError* error);

  // Fetches device integrity signals and populates them into `request`.
  void FetchDeviceIntegritySignals(
      sync_pb::GetDeviceAuthorizationKeyRequest request,
      webauthn::PopulatePlatformDataCallback callback);

  std::unique_ptr<PasskeyKeychainProvider> passkey_keychain_provider_;

  base::WeakPtrFactory<IOSDeviceAuthorizationClient> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_WEBAUTHN_MODEL_IOS_DEVICE_AUTHORIZATION_CLIENT_H_
