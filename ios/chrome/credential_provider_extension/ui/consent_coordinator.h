// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CONSENT_COORDINATOR_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CONSENT_COORDINATOR_H_

#import <Foundation/Foundation.h>

@class ConsentCoordinator;
@protocol CredentialResponseHandler;
@class ReauthenticationHandler;
@class UIViewController;

@interface ConsentCoordinator : NSObject

// Default initializer. When the coordinator is started it will present on
// `baseViewController`.
- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                 credentialResponseHandler:
                     (id<CredentialResponseHandler>)credentialResponseHandler
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Starts the consent screen.
- (void)start;

// Stops the consent screen.
- (void)stop;

@end

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CONSENT_COORDINATOR_H_
