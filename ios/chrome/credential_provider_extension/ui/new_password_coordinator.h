// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_NEW_PASSWORD_COORDINATOR_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_NEW_PASSWORD_COORDINATOR_H_

#import <UIKit/UIKit.h>

@class ASCredentialServiceIdentifier;
@protocol CredentialResponseHandler;
@protocol CredentialStore;
@class NewPasswordCoordinator;

@protocol NewPasswordCoordinatorDelegate

// Asks the delegate to stop this coordinator and dismiss it.
- (void)dismissNewPasswordCoordinator:
    (NewPasswordCoordinator*)newPasswordCoordinator;

@end

// The coordinator for the new password feature.
@interface NewPasswordCoordinator : NSObject

// Default initializer. When the coordinator is started it will present on
// `baseViewController`.
- (instancetype)
    initWithBaseViewController:(UIViewController*)baseViewController
            serviceIdentifiers:
                (NSArray<ASCredentialServiceIdentifier*>*)serviceIdentifiers
           existingCredentials:(id<CredentialStore>)existingCredentials
     credentialResponseHandler:
         (id<CredentialResponseHandler>)credentialResponseHandler
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, weak) id<NewPasswordCoordinatorDelegate> delegate;

// Starts the feature.
- (void)start;

// Stops the feature.
- (void)stop;

@end

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_NEW_PASSWORD_COORDINATOR_H_
