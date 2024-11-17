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
  using CheckEnrolledCallback = base::OnceCallback<void(BOOL, NSError*)>;
  using EnrollCallback = base::OnceCallback<void(NSError*)>;
  using KeysFetchedCallback = base::OnceCallback<void(const SharedKeyList&)>;
  using KeysMarkedAsAsStaleCallback = base::OnceCallback<void(void)>;
  using CheckDegradedRecoverabilityCallback =
      base::OnceCallback<void(BOOL, NSError*)>;
  using FixDegradedRecoverabilityCallback = base::OnceCallback<void(NSError*)>;

  PasskeyKeychainProvider(bool metrics_reporting_enabled);

  PasskeyKeychainProvider(const PasskeyKeychainProvider&) = delete;
  PasskeyKeychainProvider& operator=(const PasskeyKeychainProvider&) = delete;

  ~PasskeyKeychainProvider();

  // Checks if the identity identified by `gaia` is enrolled and invokes
  // `callback` with the result.
  // - "gaia" is used to identify the account.
  // - "callback" is called once the enrollment status is known and receives
  // the result and the potential error as input.
  void CheckEnrolled(NSString* gaia, CheckEnrolledCallback callback);

  // Asynchronously enrolls the identity identified by `gaia` and invokes
  // `callback`.
  // - "gaia" is used to identify the account.
  // - "navigation_controller" is used to display UI for the user to enter
  //   credentials.
  // - "navigation_item_title_view" is a branded title view of the
  //   password manager.
  // - "callback" is called once the enrollment process is finished and
  // receives the potential error as input.
  void Enroll(NSString* gaia,
              UINavigationController* navigation_controller,
              UIView* navigation_item_title_view,
              EnrollCallback callback);

  // Asynchronously fetches the shared keys for the identity identified by
  // `gaia` and invokes `callback` with the fetched keys.
  // - "gaia" is used to identify the account.
  // - "purpose" is used to specify if the keys will be used to encrypt or
  //   decrypt. This is mostly for logging purposes and has no effect on the
  //   fetched keys.
  // - "callback" is called once the keys are fetched and receives the fetched
  //   keys as input (the array will be empty on failure).
  void FetchKeys(NSString* gaia,
                 ReauthenticatePurpose purpose,
                 KeysFetchedCallback callback);

  // Asynchronously marks the keys as stale for the identity identified by
  // `gaia` and invokes `callback` after completion. This should be invoked
  // only after attempting and failing to decrypt a passkey using the keys
  // received from the "FetchKeys" function above.
  // - "gaia" is used to identify the account.
  // - "callback" is called once the keys are marked as stale.
  void MarkKeysAsStale(NSString* gaia, KeysMarkedAsAsStaleCallback callback);

  // Asynchronously reauthenticates the identity identified by `gaia` after the
  // keys were fetched and invokes `callback` with the fetched keys.
  // - "gaia" is used to identify the account.
  // - "navigation_controller" is used to display UI for the user to enter
  //   credentials.
  // - "navigation_item_title_view" is a branded title view of the
  //   password manager.
  // - "purpose" is used to specify if the keys will be used to encrypt or
  //   decrypt. This is mostly for logging purposes and has no effect on the
  //   fetched keys.
  // - "callback" is called once the keys are fetched and receives the fetched
  //   keys as input (the array will be empty on failure).
  void Reauthenticate(NSString* gaia,
                      UINavigationController* navigation_controller,
                      UIView* navigation_item_title_view,
                      ReauthenticatePurpose purpose,
                      KeysFetchedCallback callback);

  // Checks if the identity identified by `gaia` is in the degraded
  // recoverability state.
  // - "gaia" is used to identify the account.
  // - "callback" is called once the degraded recoverability status is known and
  // receives the result and the potential error as input.
  void CheckDegradedRecoverability(
      NSString* gaia,
      CheckDegradedRecoverabilityCallback callback);

  // Asynchronously fixes the degraded recoverability state for the identity
  // identified by `gaia` and invokes `callback`.
  // - "gaia" is used to identify the account.
  // - "navigation_controller" is used to display UI for the user to enter
  //   credentials.
  // - "navigation_item_title_view" is a branded title view of the
  //   password manager.
  // - "callback" is called once the degraded recoverability fix is completed
  // and receives the potential error as input.
  void FixDegradedRecoverability(NSString* gaia,
                                 UINavigationController* navigation_controller,
                                 UIView* navigation_item_title_view,
                                 FixDegradedRecoverabilityCallback callback);

 private:
  // Folsom service.
  GCRSSOFolsomService* gcr_sso_folsom_service_;
};

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_PASSKEY_KEYCHAIN_PROVIDER_H_
