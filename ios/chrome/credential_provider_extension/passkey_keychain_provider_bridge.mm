// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/passkey_keychain_provider_bridge.h"

#import "base/functional/callback.h"

namespace {

// Returns the security domain secret from the vault keys.
NSData* GetSecurityDomainSecret(
    const PasskeyKeychainProvider::SharedKeyList& keyList) {
  if (keyList.empty()) {
    return nil;
  }
  // TODO(crbug.com/355041765): Do we need to handle multiple keys?
  return [NSData dataWithBytes:keyList[0].data() length:keyList[0].size()];
}

}  // namespace

@implementation PasskeyKeychainProviderBridge {
  // Provider that manages passkey vault keys.
  std::unique_ptr<PasskeyKeychainProvider> _passkeyKeychainProvider;

  // Navigation controller needed by `_passkeyKeychainProvider` to display some
  // UI to the user.
  UINavigationController* _navigationController;
}

- (instancetype)initWithEnableLogging:(BOOL)enableLogging
                 navigationController:
                     (UINavigationController*)navigationController {
  self = [super init];
  if (self) {
    // TODO(crbug.com/370513825): Pass `enableLogging` to the
    // PasskeyKeychainProvider's constructor.
    _passkeyKeychainProvider = std::make_unique<PasskeyKeychainProvider>();
    _navigationController = navigationController;
  }
  return self;
}

- (void)dealloc {
  if (_passkeyKeychainProvider) {
    _passkeyKeychainProvider.reset();
  }
}

- (void)fetchSecurityDomainSecretForGaia:(NSString*)gaia
                                 purpose:(PasskeyKeychainProvider::
                                              ReauthenticatePurpose)purpose
                              completion:
                                  (FetchSecurityDomainSecretCompletionBlock)
                                      fetchSecurityDomainSecretCompletion {
  if (_navigationController) {
    __weak __typeof(self) weakSelf = self;
    auto checkEnrolledCompletion = ^(BOOL is_enrolled, NSError* error) {
      [weakSelf onIsEnrolledForGaia:gaia
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
                   purpose:purpose
                completion:fetchSecurityDomainSecretCompletion
                     error:nil];
  }
}

- (void)markKeysAsStaleForGaia:(NSString*)gaia
                    completion:(ProceduralBlock)completion {
  _passkeyKeychainProvider->MarkKeysAsStale(gaia, base::BindOnce(^() {
                                              completion();
                                            }));
}

#pragma mark - Private

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
                   purpose:purpose
                completion:fetchSecurityDomainSecretCompletion
                     error:nil];
  } else {
    // TODO(crbug.com/355042392): Show enroll UI.
    __weak __typeof(self) weakSelf = self;
    auto enrollCompletion = ^(NSError* enroll_error) {
      [weakSelf fetchKeysForGaia:gaia
                         purpose:purpose
                      completion:fetchSecurityDomainSecretCompletion
                           error:enroll_error];
    };
    [self enrollForGaia:gaia completion:enrollCompletion];
  }
}

// Starts the enrollment process for the account associated with the provided
// gaia ID and calls the completion block.
- (void)enrollForGaia:(NSString*)gaia
           completion:(EnrollCompletionBlock)completion {
  _passkeyKeychainProvider->Enroll(gaia, _navigationController,
                                   base::BindOnce(^(NSError* error) {
                                     completion(error);
                                   }));
}

// Attempts to fetch the keys for the account associated with the provided gaia
// ID if no error occured at the previous stage.
- (void)fetchKeysForGaia:(NSString*)gaia
                 purpose:(PasskeyKeychainProvider::ReauthenticatePurpose)purpose
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
                               purpose:purpose
                            completion:fetchSecurityDomainSecretCompletion
                               keyList:key_list];
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
                 purpose:(PasskeyKeychainProvider::ReauthenticatePurpose)purpose
              completion:(FetchSecurityDomainSecretCompletionBlock)completion
                 keyList:
                     (const PasskeyKeychainProvider::SharedKeyList&)keyList {
  if (keyList.size() == 0 && _navigationController) {
    // A valid navigation controller is needed to show the reauthentication UI.
    // Otherwise, it won't be possible to perform reauthentication.
    // TODO(crbug.com/355042392): Show reauth UI.
    [self reauthenticateForGaia:gaia purpose:purpose completion:completion];
  } else {
    completion(GetSecurityDomainSecret(keyList));
  }
}

// Starts the reauthentication process for the account associated with the
// provided gaia ID and calls the completion block.
- (void)reauthenticateForGaia:(NSString*)gaia
                      purpose:(PasskeyKeychainProvider::ReauthenticatePurpose)
                                  purpose
                   completion:
                       (FetchSecurityDomainSecretCompletionBlock)completion {
  _passkeyKeychainProvider->Reauthenticate(
      gaia, _navigationController, purpose,
      base::BindOnce(^(const PasskeyKeychainProvider::SharedKeyList& key_list) {
        completion(GetSecurityDomainSecret(key_list));
      }));
}

@end
