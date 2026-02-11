// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_PASSKEY_KEYCHAIN_PROVIDER_BRIDGE_H_
#define IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_PASSKEY_KEYCHAIN_PROVIDER_BRIDGE_H_

#include <memory>

#import "base/ios/block_types.h"
#import "components/webauthn/ios/passkey_types.h"
#import "ios/chrome/common/credential_provider/passkey_keychain_provider.h"

@protocol Credential;

typedef void (^FetchTrustedVaultKeysCompletionBlock)(
    webauthn::SharedKeyList trustedVaultKeys,
    NSError* error);

// Delegate for the PasskeyKeychainProviderBridge.
@protocol PasskeyKeychainProviderBridgeDelegate

// Asks the user to reauthenticate if needed and calls the the completion block.
- (void)performUserVerificationIfNeeded:(ProceduralBlock)completion;

// Presents the passkey welcome screen for `purpose`.
- (void)showWelcomeScreenWithPurpose:
            (webauthn::PasskeyWelcomeScreenPurpose)purpose
                          completion:
                              (webauthn::PasskeyWelcomeScreenAction)completion;

// Informs the delegate that the user completed a reauthentication facilitated
// by the provider.
- (void)providerDidCompleteReauthentication;

@end

// Class to bridge the CredentialProviderViewController with the
// PasskeyKeychainProvider.
@interface PasskeyKeychainProviderBridge : NSObject

// Default initializer. `enableLogging` indicates whether metrics logging should
// be enabled in the Credential Provider Extension.
- (instancetype)initWithEnableLogging:(BOOL)enableLogging
              navigationItemTitleView:(UIView*)navigationItemTitleView
    NS_DESIGNATED_INITIALIZER;

// Initializer for testing that allows injecting a fake PasskeyKeychainProvider.
- (instancetype)initWithPasskeyKeychainProvider:
    (std::unique_ptr<PasskeyKeychainProvider>)passkeyKeychainProvider
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, weak) id<PasskeyKeychainProviderBridgeDelegate> delegate;

// Initiates the process to fetch the trusted vault key and calls the completion
// block with the trusted vault key the input argument. "credential" will be
// used to validate the trusted vault key.
- (void)fetchTrustedVaultKeysForGaia:(NSString*)gaia
                          credential:(id<Credential>)credential
                             purpose:(webauthn::ReauthenticatePurpose)purpose
                          completion:(FetchTrustedVaultKeysCompletionBlock)
                                         fetchTrustedVaultKeysCompletionBlock;

@end

#endif  // IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_PASSKEY_KEYCHAIN_PROVIDER_BRIDGE_H_
