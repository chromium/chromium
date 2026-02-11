// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/credential_provider/passkey_keychain_provider_bridge.h"

#import "base/apple/foundation_util.h"
#import "base/containers/to_vector.h"
#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "components/sync/protocol/webauthn_credential_specifics.pb.h"
#import "components/webauthn/core/browser/passkey_model_utils.h"
#import "ios/chrome/common/credential_provider/archivable_credential+passkey.h"

typedef void (^CheckEnrolledCompletionBlock)(BOOL is_enrolled, NSError* error);
typedef void (^ErrorCompletionBlock)(NSError* error);

namespace {

// Returns whether there's at least one valid key in the keys array.
bool ContainsValidKey(const webauthn::SharedKeyList& keys,
                      id<Credential> credential) {
  for (const webauthn::SharedKey& key : keys) {
    sync_pb::WebauthnCredentialSpecifics_Encrypted decrypted;
    if (webauthn::passkey_model_utils::DecryptWebauthnCredentialSpecificsData(
            key, PasskeyFromCredential(credential), &decrypted)) {
      return true;
    }
  }

  return false;
}

}  // namespace

@implementation PasskeyKeychainProviderBridge {
  // Provider that manages passkey vault keys.
  std::unique_ptr<PasskeyKeychainProvider> _passkeyKeychainProvider;

  // The branded navigation item title view to use in the navigation
  // controller's UIs.
  UIView* _navigationItemTitleView;
}

- (instancetype)initWithEnableLogging:(BOOL)enableLogging
              navigationItemTitleView:(UIView*)navigationItemTitleView {
  self = [super init];
  if (self) {
    _passkeyKeychainProvider =
        std::make_unique<PasskeyKeychainProvider>(enableLogging);
    _navigationItemTitleView = navigationItemTitleView;
  }
  return self;
}

- (instancetype)initWithPasskeyKeychainProvider:
    (std::unique_ptr<PasskeyKeychainProvider>)passkeyKeychainProvider {
  self = [super init];
  if (self) {
    _passkeyKeychainProvider = std::move(passkeyKeychainProvider);
    _navigationItemTitleView = nil;  // Not needed for tests.
  }
  return self;
}

- (void)dealloc {
  if (_passkeyKeychainProvider) {
    _passkeyKeychainProvider.reset();
  }
}

- (void)fetchTrustedVaultKeysForGaia:(NSString*)gaia
                          credential:(id<Credential>)credential
                             purpose:(webauthn::ReauthenticatePurpose)purpose
                          completion:(FetchTrustedVaultKeysCompletionBlock)
                                         fetchTrustedVaultKeysCompletion {
  __weak __typeof(self) weakSelf = self;
  auto checkEnrolledCompletion = ^(BOOL is_enrolled, NSError* error) {
    [weakSelf onIsEnrolledForGaia:gaia
                       credential:credential
                          purpose:purpose
                       completion:fetchTrustedVaultKeysCompletion
                       isEnrolled:is_enrolled
                            error:error];
  };
  [self checkEnrolledForGaia:gaia completion:checkEnrolledCompletion];
}

#pragma mark - Private

// Marks the trusted vault keys as stale and calls the completion block.
- (void)markKeysAsStaleForGaia:(NSString*)gaia
                    completion:(ProceduralBlock)completion {
  _passkeyKeychainProvider->MarkKeysAsStale(gaia, base::BindOnce(^() {
                                              completion();
                                            }));
}

// Checks if the account associated with the provided gaia ID is enrolled and
// calls the completion block.
- (void)checkEnrolledForGaia:(NSString*)gaia
                  completion:(CheckEnrolledCompletionBlock)completion {
  _passkeyKeychainProvider->CheckEnrolled(
      gaia, base::BindOnce(^(BOOL is_enrolled, NSError* error) {
        completion(is_enrolled, error);
      }));
}

// Handles the enrollment status of the account associated with the provided
// gaia ID. If enrolled, fetches the keys for that account. If not, enrolls the
// account.
- (void)onIsEnrolledForGaia:(NSString*)gaia
                 credential:(id<Credential>)credential
                    purpose:(webauthn::ReauthenticatePurpose)purpose
                 completion:(FetchTrustedVaultKeysCompletionBlock)
                                fetchTrustedVaultKeysCompletion
                 isEnrolled:(BOOL)isEnrolled
                      error:(NSError*)error {
  if (isEnrolled) {
    if (error != nil) {
      // Skip fetching keys if there was an error.
      fetchTrustedVaultKeysCompletion(/*trustedVaultKeys=*/{}, error);
      return;
    }

    [self fetchKeysForGaia:gaia
                credential:credential
        canMarkKeysAsStale:YES
                   purpose:purpose
         canReauthenticate:YES
                completion:fetchTrustedVaultKeysCompletion
                     error:nil];
  } else {
    __weak __typeof(self) weakSelf = self;
    auto enrollCompletion = ^(NSError* enroll_error) {
      [weakSelf fetchKeysForGaia:gaia
                      credential:credential
              canMarkKeysAsStale:YES
                         purpose:purpose
               canReauthenticate:NO
                      completion:fetchTrustedVaultKeysCompletion
                           error:enroll_error];
    };
    [self.delegate
        showWelcomeScreenWithPurpose:webauthn::PasskeyWelcomeScreenPurpose::
                                         kEnroll
                          completion:^(
                              UINavigationController* navigationController) {
                            [weakSelf enrollForGaia:gaia
                                navigationController:navigationController
                                          completion:enrollCompletion];
                          }];
  }
}

// Starts the enrollment process for the account associated with the provided
// gaia ID and calls the completion block.
- (void)enrollForGaia:(NSString*)gaia
    navigationController:(UINavigationController*)navigationController
              completion:(ErrorCompletionBlock)completion {
  _passkeyKeychainProvider->Enroll(gaia, navigationController,
                                   _navigationItemTitleView,
                                   base::BindOnce(^(NSError* error) {
                                     completion(error);
                                   }));
}

// Attempts to fetch the keys for the account associated with the provided gaia
// ID if no error occurred at the previous stage. `canReauthenticate` indicates
// whether the user can be asked to reauthenticate by entering their GPM PIN.
// This argument should only be set to `NO` if the user has already been asked
// to reauthenticate.
- (void)fetchKeysForGaia:(NSString*)gaia
              credential:(id<Credential>)credential
      canMarkKeysAsStale:(BOOL)canMarkKeysAsStale
                 purpose:(webauthn::ReauthenticatePurpose)purpose
       canReauthenticate:(BOOL)canReauthenticate
              completion:(FetchTrustedVaultKeysCompletionBlock)
                             fetchTrustedVaultKeysCompletion
                   error:(NSError*)error {
  if (error != nil) {
    // Skip fetching keys if there was an error.
    fetchTrustedVaultKeysCompletion(/*trustedVaultKeys=*/{}, error);
    return;
  }

  __weak __typeof(self) weakSelf = self;
  auto fetchKeysCompletion =
      ^(webauthn::SharedKeyList key_list, NSError* fetchKeysError) {
        [weakSelf onKeysFetchedForGaia:gaia
                            credential:credential
                    canMarkKeysAsStale:canMarkKeysAsStale
                               purpose:purpose
                            completion:fetchTrustedVaultKeysCompletion
                               keyList:std::move(key_list)
                     canReauthenticate:canReauthenticate
                                 error:fetchKeysError];
      };
  [self fetchKeysForGaia:gaia purpose:purpose completion:fetchKeysCompletion];
}

// Fetches the trusted vault keys for the account associated with the provided
// gaia ID and calls the completion block.
- (void)fetchKeysForGaia:(NSString*)gaia
                 purpose:(webauthn::ReauthenticatePurpose)purpose
              completion:(FetchTrustedVaultKeysCompletionBlock)completion {
  _passkeyKeychainProvider->FetchKeys(
      gaia, purpose,
      base::BindOnce(
          ^(webauthn::SharedKeyList trustedVaultKeys, NSError* error) {
            completion(std::move(trustedVaultKeys), error);
          }));
}

// Handles the outcome of the key fetch process.
// If `sharedKeys` is empty, triggers the reauthentication process.
// If not, triggers the `completion`.
- (void)onKeysFetchedForGaia:(NSString*)gaia
                  credential:(id<Credential>)credential
          canMarkKeysAsStale:(BOOL)canMarkKeysAsStale
                     purpose:(webauthn::ReauthenticatePurpose)purpose
                  completion:(FetchTrustedVaultKeysCompletionBlock)completion
                     keyList:(webauthn::SharedKeyList)keyList
           canReauthenticate:(BOOL)canReauthenticate
                       error:(NSError*)error {
  __weak __typeof(self) weakSelf = self;
  if (!keyList.empty()) {
    if (purpose == webauthn::ReauthenticatePurpose::kDecrypt &&
        canMarkKeysAsStale && credential &&
        !ContainsValidKey(keyList, credential)) {
      // Mark keys as stale and try again. `canMarkKeysAsStale` is set to `NO`
      // to avoid getting into an infinite loop.
      [self markKeysAsStaleForGaia:gaia
                        completion:^() {
                          [weakSelf fetchKeysForGaia:gaia
                                          credential:credential
                                  canMarkKeysAsStale:NO
                                             purpose:purpose
                                   canReauthenticate:canReauthenticate
                                          completion:completion
                                               error:nil];
                        }];
      return;
    }

    // On success, check degraded recoverability.
    auto degradedRecoverabilityCompletion = ^(
        NSError* degradedRecoverabilityError) {
      if (degradedRecoverabilityError) {
        completion(/*trustedVaultKeys=*/{}, degradedRecoverabilityError);
      } else {
        [weakSelf performUserVerificationIfNeededAndCallCompletionWithKeys:
                      std::move(keyList)
                                                                completion:
                                                                    completion];
      }
    };
    [self checkDegradedRecoverabilityForGaia:gaia
                                  completion:degradedRecoverabilityCompletion];
  } else {
    if (canReauthenticate) {
      [self.delegate
          showWelcomeScreenWithPurpose:webauthn::PasskeyWelcomeScreenPurpose::
                                           kReauthenticate
                            completion:^(
                                UINavigationController* navigationController) {
                              [weakSelf
                                  reauthenticateForGaia:gaia
                                             credential:credential
                                     canMarkKeysAsStale:canMarkKeysAsStale
                                                purpose:purpose
                                   navigationController:navigationController
                                             completion:completion];
                            }];
    } else {
      completion(/*trustedVaultKeys=*/{}, error);
    }
  }
}

// Starts the reauthentication process for the account associated with the
// provided gaia ID and calls the completion block.
- (void)reauthenticateForGaia:(NSString*)gaia
                   credential:(id<Credential>)credential
           canMarkKeysAsStale:(BOOL)canMarkKeysAsStale
                      purpose:(webauthn::ReauthenticatePurpose)purpose
         navigationController:(UINavigationController*)navigationController
                   completion:(FetchTrustedVaultKeysCompletionBlock)completion {
  __weak __typeof(self) weakSelf = self;
  _passkeyKeychainProvider->Reauthenticate(
      gaia, navigationController, _navigationItemTitleView, purpose,
      base::BindOnce(^(webauthn::SharedKeyList key_list, NSError* error) {
        // If we got nonempty keys, that means the reauthentication was a
        // success. Report this back to the delegate.
        if (!key_list.empty()) {
          [weakSelf.delegate providerDidCompleteReauthentication];
        }

        [weakSelf onKeysFetchedForGaia:gaia
                            credential:credential
                    canMarkKeysAsStale:canMarkKeysAsStale
                               purpose:purpose
                            completion:completion
                               keyList:std::move(key_list)
                     canReauthenticate:NO
                                 error:error];
      }));
}

// Checks if the account associated with the provided gaia ID is in degraded
// recoverability and calls the completion block.
- (void)checkDegradedRecoverabilityForGaia:(NSString*)gaia
                                completion:(ErrorCompletionBlock)completion {
  __weak __typeof(self) weakSelf = self;
  _passkeyKeychainProvider->CheckDegradedRecoverability(
      gaia, base::BindOnce(^(BOOL inDegradedRecoverability, NSError* error) {
        if (inDegradedRecoverability) {
          [weakSelf.delegate
              showWelcomeScreenWithPurpose:webauthn::
                                               PasskeyWelcomeScreenPurpose::
                                                   kFixDegradedRecoverability
                                completion:^(UINavigationController*
                                                 navigationController) {
                                  [weakSelf
                                      fixDegradedRecoverabilityForGaia:gaia
                                                  navigationController:
                                                      navigationController
                                                            completion:
                                                                completion];
                                }];
        } else {
          completion(error);
        }
      }));
}

// Fixes the degraded recoverability state for the account associated with the
// provided gaia ID and calls the completion block.
- (void)fixDegradedRecoverabilityForGaia:(NSString*)gaia
                    navigationController:
                        (UINavigationController*)navigationController
                              completion:(ErrorCompletionBlock)completion {
  _passkeyKeychainProvider->FixDegradedRecoverability(
      gaia, navigationController, _navigationItemTitleView,
      base::BindOnce(^(NSError* error) {
        completion(error);
      }));
}

// Asks the delegate to perform a user verification if needed and calls the
// completion block.
- (void)
    performUserVerificationIfNeededAndCallCompletionWithKeys:
        (webauthn::SharedKeyList)keys
                                                  completion:
                                                      (FetchTrustedVaultKeysCompletionBlock)
                                                          completion {
  [self.delegate performUserVerificationIfNeeded:^{
    completion(std::move(keys), /*error=*/nil);
  }];
}

@end
