// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_PASSKEY_KEYCHAIN_PROVIDER_BRIDGE_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_PASSKEY_KEYCHAIN_PROVIDER_BRIDGE_H_

#import "base/ios/block_types.h"
#import "ios/chrome/credential_provider_extension/passkey_keychain_provider.h"
#import "ios/chrome/credential_provider_extension/ui/credential_response_handler.h"

// Delegate for the PasskeyKeychainProviderBridge.
@protocol PasskeyKeychainProviderBridgeDelegate

// Asks the user to reauthenticate if needed and calls the the completion block.
- (void)performUserVerificationIfNeeded:(ProceduralBlock)completion;

// Presents the passkey enrollment welcome screen.
- (void)showEnrollmentWelcomeScreen:(ProceduralBlock)enrollBlock;

// Presents the passkey "fix degraded recoverability state" welcome screen.
- (void)showFixDegradedRecoverabilityWelcomeScreen:
    (ProceduralBlock)fixDegradedRecoverabilityBlock;

// Presents the passkey reauthentication weclome screen.
- (void)showReauthenticationWelcomeScreen:(ProceduralBlock)reauthenticateBlock;

@end

// Class to bridge the CredentialProviderViewController with the
// PasskeyKeychainProvider.
@interface PasskeyKeychainProviderBridge : NSObject

// Default initializer. `enableLogging` indicates whether metrics logging should
// be enabled in the Credential Provider Extension.
- (instancetype)initWithEnableLogging:(BOOL)enableLogging
                 navigationController:
                     (UINavigationController*)navigationController
              navigationItemTitleView:(UIView*)navigationItemTitleView
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, weak) id<PasskeyKeychainProviderBridgeDelegate> delegate;

// Initiates the process to fetch the security domain secret and calls the
// completion block with the security domain secret the input argument.
// "credential" will be used to validate the security domain secret.
- (void)fetchSecurityDomainSecretForGaia:(NSString*)gaia
                              credential:(id<Credential>)credential
                                 purpose:(PasskeyKeychainProvider::
                                              ReauthenticatePurpose)purpose
                              completion:
                                  (FetchSecurityDomainSecretCompletionBlock)
                                      fetchSecurityDomainSecretCompletion;

@end

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_PASSKEY_KEYCHAIN_PROVIDER_BRIDGE_H_
