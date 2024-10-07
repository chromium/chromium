// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_PASSKEY_KEYCHAIN_PROVIDER_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_PASSKEY_KEYCHAIN_PROVIDER_H_

#import <UIKit/UIKit.h>

#import <vector>

#import "base/functional/callback_forward.h"

@class GCRSSOFolsomService;
@class UIViewController;

// Class to manage passkey vault keys.
class PasskeyKeychainProvider {
 public:
  // The client-defined purpose of the reauthentication flow.
  enum class ReauthenticatePurpose {
    // Unspecified action.
    kUnspecified,
    // The client is trying to encrypt using the shared key.
    kEncrypt,
    // The user is trying to decrypt using the shared key.
    kDecrypt,
  };

  // Helper types representing a key and a list of key respectively.
  using SharedKey = std::vector<uint8_t>;
  using SharedKeyList = std::vector<SharedKey>;

  // Types for the different callbacks.
  using KeyFetchedCallback = base::OnceCallback<void(const SharedKeyList&)>;
  using KeysMarkedAsAsStaleCallback = base::OnceCallback<void(void)>;

  PasskeyKeychainProvider();

  PasskeyKeychainProvider(const PasskeyKeychainProvider&) = delete;
  PasskeyKeychainProvider& operator=(const PasskeyKeychainProvider&) = delete;

  ~PasskeyKeychainProvider();

  // Asynchronously fetches the shared keys for the identity identified by
  // `gaia` and invokes `callback` with the fetched keys.
  // - "gaia" is used to identify the account.
  // - "navigation_controller" is used to display UI for the user to enter
  //   credentials. Can be nil, in which case FetchKeys still attempts to fetch
  //   keys, but fails immediately if any user interaction is required.
  // - "purpose" is used to specify if the keys will be used to encrypt or
  //   decrypt. This is mostly for logging purposes and has no effect on the
  //   keys fetched.
  // - "callback" is called once the keys are fetched and receives the fetched
  //   keys as input (the array will be empty on failure).
  void FetchKeys(NSString* gaia,
                 UINavigationController* navigation_controller,
                 ReauthenticatePurpose purpose,
                 KeyFetchedCallback callback);

  // Asynchronously marks the keys as stale for the identity identified by
  // `gaia` and invokes `callback` after completion. This should be invoked
  // only after attempting and failing to decrypt a passkey using the keys
  // received from the "FetchKeys" function above.
  // - "gaia" is used to identify the account.
  // - "callback" is called once the keys are marked as stale.
  void MarkKeysAsStale(NSString* gaia, KeysMarkedAsAsStaleCallback callback);

 private:
  // Folsom service.
  GCRSSOFolsomService* gcr_sso_folsom_service_;
};

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_PASSKEY_KEYCHAIN_PROVIDER_H_
