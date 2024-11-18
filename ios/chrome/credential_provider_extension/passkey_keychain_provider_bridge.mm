// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/passkey_keychain_provider_bridge.h"

#import "base/functional/callback.h"
#import "ios/chrome/credential_provider_extension/passkey_util.h"

typedef void (^CheckEnrolledCompletionBlock)(BOOL is_enrolled, NSError* error);
typedef void (^ErrorCompletionBlock)(NSError* error);
typedef void (^FetchKeysCompletionBlock)(
    const PasskeyKeychainProvider::SharedKeyList& key_list);

namespace {

// Returns an array of security domain secrets from the vault keys.
NSArray<NSData*>* GetSecurityDomainSecret(
    const PasskeyKeychainProvider::SharedKeyList keys) {
  NSMutableArray<NSData*>* security_domain_secrets =
      [NSMutableArray arrayWithCapacity:keys.size()];
  for (const auto& key : keys) {
    [security_domain_secrets addObject:[NSData dataWithBytes:key.data()
                                                      length:key.size()]];
  }
  return security_domain_secrets;
}

// Returns whether there's at least one valid key in the keys array.
bool ContainsValidKey(const PasskeyKeychainProvider::SharedKeyList keys,
                      id<Credential> credential) {
  NSArray<NSData*>* security_domain_secrets = GetSecurityDomainSecret(keys);
  std::string private_key =
      DecryptPrivateKey(credential, security_domain_secrets);
  return !private_key.empty();
}

}  // namespace

@implementation PasskeyKeychainProviderBridge {
  // Provider that manages passkey vault keys.
  std::unique_ptr<PasskeyKeychainProvider> _passkeyKeychainProvider;

  // Navigation controller needed by `_passkeyKeychainProvider` to display some
  // UI to the user.
  UINavigationController* _navigationController;

  // The branded navigation item title view to use in the navigation
  // controller's UIs.
  UIView* _navigationItemTitleView;
}

- (instancetype)initWithEnableLogging:(BOOL)enableLogging
                 navigationController:
                     (UINavigationController*)navigationController
              navigationItemTitleView:(UIView*)navigationItemTitleView {
  self = [super init];
  if (self) {
    _passkeyKeychainProvider =
        std::make_unique<PasskeyKeychainProvider>(enableLogging);
    _navigationController = navigationController;
    _navigationItemTitleView = navigationItemTitleView;
  }
  return self;
}

- (void)dealloc {
  if (_passkeyKeychainProvider) {
    _passkeyKeychainProvider.reset();
  }
}

- (void)fetchSecurityDomainSecretForGaia:(NSString*)gaia
                              credential:(id<Credential>)credential
                                 purpose:(PasskeyKeychainProvider::
                                              ReauthenticatePurpose)purpose
                              completion:
                                  (FetchSecurityDomainSecretCompletionBlock)
                                      fetchSecurityDomainSecretCompletion {
  if (_navigationController) {
    __weak __typeof(self) weakSelf = self;
    auto checkEnrolledCompletion = ^(BOOL is_enrolled, NSError* error) {
      [weakSelf onIsEnrolledForGaia:gaia
                         credential:credential
                            purpose:purpose
                         completion:fetchSecurityDomainSecretCompletion
                         isEnrolled:is_enrolled
                              error:error];
    };
    [self checkEnrolledForGaia:gaia completion:checkEnrolledCompletion];
  } else {
    // If there's no valid navigation controller to show the enrollment UI, it
    // won't be possible to enroll, so only attempt to fetch keys.
    [self fetchKeysForGaia:gaia
                credential:credential
        canMarkKeysAsStale:YES
                   purpose:purpose
         canReauthenticate:YES
                completion:fetchSecurityDomainSecretCompletion
                     error:nil];
  }
}

#pragma mark - Private

// Marks the security domain secret vault keys as stale and calls the completion
// block.
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
                    purpose:
                        (PasskeyKeychainProvider::ReauthenticatePurpose)purpose
                 completion:(FetchSecurityDomainSecretCompletionBlock)
                                fetchSecurityDomainSecretCompletion
                 isEnrolled:(BOOL)isEnrolled
                      error:(NSError*)error {
  if (isEnrolled) {
    if (error != nil) {
      // Skip fetching keys if there was an error.
      fetchSecurityDomainSecretCompletion(nil);
      return;
    }

    [self fetchKeysForGaia:gaia
                credential:credential
        canMarkKeysAsStale:YES
                   purpose:purpose
         canReauthenticate:YES
                completion:fetchSecurityDomainSecretCompletion
                     error:nil];
  } else {
    __weak __typeof(self) weakSelf = self;
    auto enrollCompletion = ^(NSError* enroll_error) {
      [weakSelf fetchKeysForGaia:gaia
                      credential:credential
              canMarkKeysAsStale:YES
                         purpose:purpose
               canReauthenticate:NO
                      completion:fetchSecurityDomainSecretCompletion
                           error:enroll_error];
    };
    [self.delegate showEnrollmentWelcomeScreen:^{
      [weakSelf enrollForGaia:gaia completion:enrollCompletion];
    }];
  }
}

// Starts the enrollment process for the account associated with the provided
// gaia ID and calls the completion block.
- (void)enrollForGaia:(NSString*)gaia
           completion:(ErrorCompletionBlock)completion {
  _passkeyKeychainProvider->Enroll(gaia, _navigationController,
                                   _navigationItemTitleView,
                                   base::BindOnce(^(NSError* error) {
                                     completion(error);
                                   }));
}

// Attempts to fetch the keys for the account associated with the provided gaia
// ID if no error occured at the previous stage. `canReauthenticate` indicates
// whether the user can be asked to reauthenticate by entering their GPM PIN.
// This argument should only be set to `NO` if the user has already been asked
// to reauthenticate.
- (void)fetchKeysForGaia:(NSString*)gaia
              credential:(id<Credential>)credential
      canMarkKeysAsStale:(BOOL)canMarkKeysAsStale
                 purpose:(PasskeyKeychainProvider::ReauthenticatePurpose)purpose
       canReauthenticate:(BOOL)canReauthenticate
              completion:(FetchSecurityDomainSecretCompletionBlock)
                             fetchSecurityDomainSecretCompletion
                   error:(NSError*)error {
  if (error != nil) {
    // Skip fetching keys if there was an error.
    fetchSecurityDomainSecretCompletion(nil);
    return;
  }

  __weak __typeof(self) weakSelf = self;
  auto fetchKeysCompletion =
      ^(const PasskeyKeychainProvider::SharedKeyList& key_list) {
        [weakSelf onKeysFetchedForGaia:gaia
                            credential:credential
                    canMarkKeysAsStale:canMarkKeysAsStale
                               purpose:purpose
                            completion:fetchSecurityDomainSecretCompletion
                               keyList:key_list
                     canReauthenticate:canReauthenticate];
      };
  [self fetchKeysForGaia:gaia purpose:purpose completion:fetchKeysCompletion];
}

// Fetches the security domain secret vault keys for the account associated with
// the provided gaia ID and calls the completion block.
- (void)fetchKeysForGaia:(NSString*)gaia
                 purpose:(PasskeyKeychainProvider::ReauthenticatePurpose)purpose
              completion:(FetchKeysCompletionBlock)completion {
  _passkeyKeychainProvider->FetchKeys(
      gaia, purpose,
      base::BindOnce(^(const PasskeyKeychainProvider::SharedKeyList& key_list) {
        completion(key_list);
      }));
}

// Handles the outcome of the key fetch process.
// If `sharedKeys` is empty, triggers the reauthentication process.
// If not, triggers the `completion`.
- (void)
    onKeysFetchedForGaia:(NSString*)gaia
              credential:(id<Credential>)credential
      canMarkKeysAsStale:(BOOL)canMarkKeysAsStale
                 purpose:(PasskeyKeychainProvider::ReauthenticatePurpose)purpose
              completion:(FetchSecurityDomainSecretCompletionBlock)completion
                 keyList:(const PasskeyKeychainProvider::SharedKeyList&)keyList
       canReauthenticate:(BOOL)canReauthenticate {
  __weak __typeof(self) weakSelf = self;
  if (!keyList.empty()) {
    if (purpose == PasskeyKeychainProvider::ReauthenticatePurpose::kDecrypt &&
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

    const PasskeyKeychainProvider::SharedKeyList keys = std::move(keyList);
    // On success, check degraded recoverability.
    auto degradedRecoverabilityCompletion = ^(NSError* error) {
      if (error) {
        completion(nil);
      } else {
        [weakSelf
            performUserVerificationIfNeededAndCallCompletionWithKeys:std::move(
                                                                         keys)
                                                          completion:
                                                              completion];
      }
    };
    [self checkDegradedRecoverabilityForGaia:gaia
                                  completion:degradedRecoverabilityCompletion];
  } else {
    if (_navigationController && canReauthenticate) {
      // A valid navigation controller is needed to show the reauthentication
      // UI. Otherwise, it won't be possible to perform reauthentication.
      [self.delegate showReauthenticationWelcomeScreen:^{
        [weakSelf reauthenticateForGaia:gaia
                             credential:credential
                     canMarkKeysAsStale:canMarkKeysAsStale
                                purpose:purpose
                             completion:completion];
      }];
    } else {
      completion(nil);
    }
  }
}

// Starts the reauthentication process for the account associated with the
// provided gaia ID and calls the completion block.
- (void)reauthenticateForGaia:(NSString*)gaia
                   credential:(id<Credential>)credential
           canMarkKeysAsStale:(BOOL)canMarkKeysAsStale
                      purpose:(PasskeyKeychainProvider::ReauthenticatePurpose)
                                  purpose
                   completion:
                       (FetchSecurityDomainSecretCompletionBlock)completion {
  _passkeyKeychainProvider->Reauthenticate(
      gaia, _navigationController, _navigationItemTitleView, purpose,
      base::BindOnce(^(const PasskeyKeychainProvider::SharedKeyList& key_list) {
        [self onKeysFetchedForGaia:gaia
                        credential:credential
                canMarkKeysAsStale:canMarkKeysAsStale
                           purpose:purpose
                        completion:completion
                           keyList:key_list
                 canReauthenticate:NO];
      }));
}

// Checks if the account associated with the provided gaia ID is in degraded
// recoverability and calls the completion block.
- (void)checkDegradedRecoverabilityForGaia:(NSString*)gaia
                                completion:(ErrorCompletionBlock)completion {
  __weak __typeof(self) weakSelf = self;
  _passkeyKeychainProvider->CheckDegradedRecoverability(
      gaia, base::BindOnce(^(BOOL inDegradedRecoverability, NSError* error) {
        if (weakSelf.navigationController && inDegradedRecoverability) {
          // A valid navigation controller is needed to show the "fix degraded
          // recoverability state" UI. Otherwise, it won't be possible to
          // perform the GPM pin creation required to fix the degraded
          // recoverability state.
          [weakSelf.delegate showFixDegradedRecoverabilityWelcomeScreen:^{
            [weakSelf fixDegradedRecoverabilityForGaia:gaia
                                            completion:completion];
          }];
        } else {
          completion(error);
        }
      }));
}

// Fixes the degraded recoverability state for the account associated with the
// provided gaia ID and calls the completion block.
- (void)fixDegradedRecoverabilityForGaia:(NSString*)gaia
                              completion:(ErrorCompletionBlock)completion {
  _passkeyKeychainProvider->FixDegradedRecoverability(
      gaia, _navigationController, _navigationItemTitleView,
      base::BindOnce(^(NSError* error) {
        completion(error);
      }));
}

// Private accessor for the `_navigationController` ivar.
- (UINavigationController*)navigationController {
  return _navigationController;
}

// Asks the delegate to perform a user verification if needed and calls the
// completion block.
- (void)
    performUserVerificationIfNeededAndCallCompletionWithKeys:
        (const PasskeyKeychainProvider::SharedKeyList)keys
                                                  completion:
                                                      (FetchSecurityDomainSecretCompletionBlock)
                                                          completion {
  [self.delegate performUserVerificationIfNeeded:^{
    completion(GetSecurityDomainSecret(std::move(keys)));
  }];
}

@end
