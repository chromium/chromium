// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/credential_provider/passkey_keychain_provider_bridge.h"

#import <algorithm>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "components/sync/protocol/webauthn_credential_specifics.pb.h"
#import "components/trusted_vault/trusted_vault_key_and_version.h"
#import "components/webauthn/core/browser/passkey_model_utils.h"
#import "ios/chrome/common/credential_provider/archivable_credential+passkey.h"

typedef void (^CheckEnrolledCompletionBlock)(BOOL is_enrolled, NSError* error);
typedef void (^ErrorCompletionBlock)(NSError* error);

NSString* const kPasskeyKeychainProviderBridgeErrorDomain =
    @"PasskeyKeychainProviderBridgeErrorDomain";

namespace {

// Controls whether trusted vault key fetching may present user-facing prompts.
enum class KeyFetchInteractionMode {
  // Allow enrollment, reauthentication, degraded-recoverability, and user
  // verification UI.
  kInteractive,
  // Allow degraded-recoverability and user verification UI, but do not
  // re-prompt for reauthentication (used after enrollment or reauthentication
  // already ran).
  kInteractiveNoReauth,
  // Never present enrollment, reauthentication, degraded-recoverability, or
  // user verification UI.
  kSilent,
};

// Returns whether there's at least one valid key in `keys`.
bool ContainsValidKey(
    const std::vector<trusted_vault::TrustedVaultKeyAndVersion>& keys,
    id<Credential> credential) {
  for (const trusted_vault::TrustedVaultKeyAndVersion& key_with_version :
       keys) {
    sync_pb::WebauthnCredentialSpecifics_Encrypted decrypted;
    // TODO(crbug.com/405036154): Validate only the trusted vault key for
    // passkeys in the `security_domain_encrypted` format.
    // TODO(crbug.com/588274186): Check if `key_version` of
    // `WebauthnCredentialSpecifics` matches `key_with_version.version` before
    // attempting decryption, and consider extracting a shared helper.
    if (webauthn::passkey_model_utils::DecryptWebauthnCredentialSpecificsData(
            key_with_version.key, /*device_authorization_key=*/{},
            PasskeyFromCredential(credential), &decrypted)) {
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
                             allowUI:(BOOL)allowUI
                          completion:(FetchTrustedVaultKeysCompletionBlock)
                                         fetchTrustedVaultKeysCompletion {
  [self fetchTrustedVaultKeysForGaia:gaia
                          credential:credential
                             purpose:purpose
                     interactionMode:allowUI
                                         ? KeyFetchInteractionMode::kInteractive
                                         : KeyFetchInteractionMode::kSilent
                          completion:fetchTrustedVaultKeysCompletion];
}

#pragma mark - Private

// Shared implementation for fetching trusted vault keys using the specified
// `interactionMode`.
- (void)fetchTrustedVaultKeysForGaia:(NSString*)gaia
                          credential:(id<Credential>)credential
                             purpose:(webauthn::ReauthenticatePurpose)purpose
                     interactionMode:(KeyFetchInteractionMode)interactionMode
                          completion:(FetchTrustedVaultKeysCompletionBlock)
                                         fetchTrustedVaultKeysCompletion {
  // Ensure that the completion is invoked only once, since for the main app,
  // it's wrapped from `base::OnceCallback`, which would lead to a crash in
  // case of e.g. a double tap. More details in crbug.com/525411441.
  __block bool completion_executed = false;
  FetchTrustedVaultKeysCompletionBlock completion =
      ^(std::vector<trusted_vault::TrustedVaultKeyAndVersion> keyList,
        NSError* error) {
        if (!completion_executed) {
          completion_executed = true;
          fetchTrustedVaultKeysCompletion(std::move(keyList), error);
        }
      };

  __weak __typeof(self) weakSelf = self;
  auto checkEnrolledCompletion = ^(BOOL is_enrolled, NSError* error) {
    [weakSelf onIsEnrolledForGaia:gaia
                       credential:credential
                          purpose:purpose
                  interactionMode:interactionMode
                       completion:completion
                       isEnrolled:is_enrolled
                            error:error];
  };
  [self checkEnrolledForGaia:gaia completion:checkEnrolledCompletion];
}

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
// `gaia` ID. If enrolled, fetches the keys for that account. If not and
// `interactionMode` is interactive, enrolls the account; otherwise completes
// with empty keys.
- (void)onIsEnrolledForGaia:(NSString*)gaia
                 credential:(id<Credential>)credential
                    purpose:(webauthn::ReauthenticatePurpose)purpose
            interactionMode:(KeyFetchInteractionMode)interactionMode
                 completion:(FetchTrustedVaultKeysCompletionBlock)
                                fetchTrustedVaultKeysCompletion
                 isEnrolled:(BOOL)isEnrolled
                      error:(NSError*)error {
  if (error != nil) {
    // Skip fetching keys if there was an error.
    fetchTrustedVaultKeysCompletion(/*trustedVaultKeys=*/{}, error);
    return;
  }

  if (isEnrolled) {
    [self fetchKeysForGaia:gaia
                credential:credential
        canMarkKeysAsStale:YES
                   purpose:purpose
           interactionMode:interactionMode
                completion:fetchTrustedVaultKeysCompletion
                     error:nil];
  } else if (interactionMode == KeyFetchInteractionMode::kSilent) {
    fetchTrustedVaultKeysCompletion(/*trustedVaultKeys=*/{}, /*error=*/nil);
  } else {
    __weak __typeof(self) weakSelf = self;
    auto enrollCompletion = ^(NSError* enroll_error) {
      [weakSelf fetchKeysForGaia:gaia
                      credential:credential
              canMarkKeysAsStale:YES
                         purpose:purpose
                 interactionMode:KeyFetchInteractionMode::kInteractiveNoReauth
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

// Attempts to fetch the keys for the account associated with the provided
// `gaia` ID if no error occurred at the previous stage. `interactionMode`
// controls whether reauthentication or degraded-recoverability UI may be shown.
- (void)fetchKeysForGaia:(NSString*)gaia
              credential:(id<Credential>)credential
      canMarkKeysAsStale:(BOOL)canMarkKeysAsStale
                 purpose:(webauthn::ReauthenticatePurpose)purpose
         interactionMode:(KeyFetchInteractionMode)interactionMode
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
      ^(std::vector<trusted_vault::TrustedVaultKeyAndVersion> keyList,
        NSError* fetchKeysError) {
        [weakSelf onKeysFetchedForGaia:gaia
                            credential:credential
                    canMarkKeysAsStale:canMarkKeysAsStale
                               purpose:purpose
                       interactionMode:interactionMode
                            completion:fetchTrustedVaultKeysCompletion
                               keyList:std::move(keyList)
                                 error:fetchKeysError];
      };
  [self fetchKeysForGaia:gaia purpose:purpose completion:fetchKeysCompletion];
}

// Fetches the trusted vault keys for the account associated with the provided
// gaia ID and calls the completion block.
- (void)fetchKeysForGaia:(NSString*)gaia
                 purpose:(webauthn::ReauthenticatePurpose)purpose
              completion:(FetchTrustedVaultKeysCompletionBlock)completion {
  CHECK(completion);
  _passkeyKeychainProvider->FetchKeys(gaia, purpose,
                                      base::BindOnce(completion));
}

// Handles the outcome of the key fetch process.
// If `keyList` is empty and `interactionMode` is `kInteractive`, triggers the
// reauthentication process. If not empty, checks degraded recoverability when
// interactive and finishes via `completion`.
- (void)onKeysFetchedForGaia:(NSString*)gaia
                  credential:(id<Credential>)credential
          canMarkKeysAsStale:(BOOL)canMarkKeysAsStale
                     purpose:(webauthn::ReauthenticatePurpose)purpose
             interactionMode:(KeyFetchInteractionMode)interactionMode
                  completion:(FetchTrustedVaultKeysCompletionBlock)completion
                     keyList:
                         (std::vector<trusted_vault::TrustedVaultKeyAndVersion>)
                             keyList
                       error:(NSError*)error {
  CHECK(std::ranges::is_sorted(
      keyList, {}, &trusted_vault::TrustedVaultKeyAndVersion::version));

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
                                     interactionMode:interactionMode
                                          completion:completion
                                               error:nil];
                        }];
      return;
    }

    if (interactionMode == KeyFetchInteractionMode::kSilent) {
      // Skip degraded-recoverability and user verification prompts during
      // silent fetches so conditional passkey creation succeeds silently
      // whenever valid keys are present.
      completion(std::move(keyList), /*error=*/nil);
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
    if (interactionMode == KeyFetchInteractionMode::kInteractive) {
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
      base::BindOnce(^(std::vector<trusted_vault::TrustedVaultKeyAndVersion>
                           trustedVaultKeys,
                       NSError* error) {
        // If we got nonempty keys, that means the reauthentication was a
        // success. Report this back to the delegate.
        if (!trustedVaultKeys.empty()) {
          [weakSelf.delegate providerDidCompleteReauthentication];
        }

        [weakSelf
            onKeysFetchedForGaia:gaia
                      credential:credential
              canMarkKeysAsStale:canMarkKeysAsStale
                         purpose:purpose
                 interactionMode:KeyFetchInteractionMode::kInteractiveNoReauth
                      completion:completion
                         keyList:std::move(trustedVaultKeys)
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
        (std::vector<trusted_vault::TrustedVaultKeyAndVersion>)keys
                                                  completion:
                                                      (FetchTrustedVaultKeysCompletionBlock)
                                                          completion {
  [self.delegate performUserVerificationIfNeeded:^(BOOL success) {
    if (success) {
      completion(std::move(keys), /*error=*/nil);
    } else {
      NSError* error = [NSError
          errorWithDomain:kPasskeyKeychainProviderBridgeErrorDomain
                     code:static_cast<NSInteger>(
                              PasskeyKeychainError::kUserVerificationFailed)
                 userInfo:nil];
      completion(/*trustedVaultKeys=*/{}, error);
    }
  }];
}

@end
