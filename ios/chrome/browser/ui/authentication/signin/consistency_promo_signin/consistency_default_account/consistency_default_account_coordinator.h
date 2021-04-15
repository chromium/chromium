// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_DEFAULT_ACCOUNT_CONSISTENCY_DEFAULT_ACCOUNT_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_DEFAULT_ACCOUNT_CONSISTENCY_DEFAULT_ACCOUNT_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

@class ChromeIdentity;
@class ConsistencyDefaultAccountCoordinator;

@protocol ConsistencyDefaultAccountCoordinatorDelegate <NSObject>

// Called when the user wants to skip the consistency promo.
- (void)consistencyDefaultAccountCoordinatorSkip:
    (ConsistencyDefaultAccountCoordinator*)coordinator;

// Called when the user wants to choose a different identity.
- (void)consistencyDefaultAccountCoordinatorOpenIdentityChooser:
    (ConsistencyDefaultAccountCoordinator*)coordinator;

// Called when the user wants to sign-in with the default identity.
- (void)consistencyDefaultAccountCoordinator:
            (ConsistencyDefaultAccountCoordinator*)coordinator
                            selectedIdentity:(ChromeIdentity*)chromeIdentity;

@end

// This coordinator presents an entry point to the Chrome sign-in flow with the
// default account available on the device.
@interface ConsistencyDefaultAccountCoordinator : ChromeCoordinator

@property(nonatomic, strong, readonly) UIViewController* viewController;
@property(nonatomic, weak) id<ConsistencyDefaultAccountCoordinatorDelegate>
    delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_DEFAULT_ACCOUNT_CONSISTENCY_DEFAULT_ACCOUNT_COORDINATOR_H_
