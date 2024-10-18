// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_PASSKEY_KEYCHAIN_PROVIDER_BRIDGE_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_PASSKEY_KEYCHAIN_PROVIDER_BRIDGE_H_

#import "base/ios/block_types.h"
#import "ios/chrome/credential_provider_extension/passkey_keychain_provider.h"
#import "ios/chrome/credential_provider_extension/ui/credential_response_handler.h"

// Class to bridge the CredentialProviderViewController with the
// PasskeyKeychainProvider.
@interface PasskeyKeychainProviderBridge : NSObject

// Default initializer. `enableLogging` indicates whether metrics logging should
// be enabled in the Credential Provider Extension.
- (instancetype)initWithEnableLogging:(BOOL)enableLogging
                 navigationController:
                     (UINavigationController*)navigationController
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Initiates the process to fetch the security domain secret and calls the
// completion block with the security domain secret the input argument.
- (void)fetchSecurityDomainSecretForGaia:(NSString*)gaia
                                 purpose:(PasskeyKeychainProvider::
                                              ReauthenticatePurpose)purpose
                              completion:
                                  (FetchSecurityDomainSecretCompletionBlock)
                                      fetchSecurityDomainSecretCompletion;

// Marks the security domain secret vault keys as stale and calls the completion
// block.
- (void)markKeysAsStaleForGaia:(NSString*)gaia
                    completion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_PASSKEY_KEYCHAIN_PROVIDER_BRIDGE_H_
