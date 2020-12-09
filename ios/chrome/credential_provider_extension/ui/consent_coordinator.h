// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CONSENT_COORDINATOR_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CONSENT_COORDINATOR_H_

#import <Foundation/Foundation.h>

@class ASCredentialProviderExtensionContext;
@class ConsentCoordinator;
@class ReauthenticationHandler;
@class UIViewController;

@protocol ConsentCoordinatorDelegate <NSObject>

// Called when the user accepts the consent shown by this coordinator.
- (void)consentCoordinatorDidAcceptConsent:
    (ConsentCoordinator*)consentCoordinator;

@end

@interface ConsentCoordinator : NSObject

// Delegate to handle the coordinator.
@property(nonatomic, weak) id<ConsentCoordinatorDelegate> delegate;

// Default initializer. When the coordinator is started it will present on
// |baseViewController|.
- (instancetype)
       initWithBaseViewController:(UIViewController*)baseViewController
                          context:(ASCredentialProviderExtensionContext*)context
          reauthenticationHandler:
              (ReauthenticationHandler*)reauthenticationHandler
    isInitialConfigurationRequest:(BOOL)isInitialConfigurationRequest
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Starts the consent screen.
- (void)start;

// Stops the consent screen.
- (void)stop;

@end

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CONSENT_COORDINATOR_H_
