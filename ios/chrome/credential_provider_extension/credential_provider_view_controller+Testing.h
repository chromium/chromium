// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_CREDENTIAL_PROVIDER_VIEW_CONTROLLER_TESTING_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_CREDENTIAL_PROVIDER_VIEW_CONTROLLER_TESTING_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/common/credential_provider/passkey_keychain_provider_bridge.h"
#import "ios/chrome/common/credential_provider/ui/passkey_welcome_screen_view_controller.h"
#import "ios/chrome/credential_provider_extension/credential_provider_view_controller.h"
#import "ios/chrome/credential_provider_extension/ui/credential_response_handler.h"

@protocol AccountVerificationProvider;
@protocol CredentialStore;
@protocol ReauthenticationProtocol;

// Exposes CredentialProviderViewController's dependency injection seams and
// protocol conformances for tests only.
@interface CredentialProviderViewController (Testing) <
    CredentialResponseHandler,
    PasskeyKeychainProviderBridgeDelegate,
    PasskeyWelcomeScreenViewControllerDelegate,
    UIAdaptivePresentationControllerDelegate>

// Interface for the persistent credential store.
@property(nonatomic, strong) id<CredentialStore> credentialStore;

// Reauthentication module used for reauthentication.
@property(nonatomic, strong) id<ReauthenticationProtocol>
    reauthenticationModule;

// Interface for verifying that accounts are still valid.
@property(nonatomic, strong) id<AccountVerificationProvider> accountVerificator;

// Bridge to the `PasskeyKeychainProvider` that manages passkey vault keys.
@property(nonatomic, strong)
    PasskeyKeychainProviderBridge* passkeyKeychainProviderBridge;

@end

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_CREDENTIAL_PROVIDER_VIEW_CONTROLLER_TESTING_H_
