// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_ACCOUNT_SWITCH_ACCOUNT_SWITCH_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_ACCOUNT_SWITCH_ACCOUNT_SWITCH_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator.h"

class Browser;
@protocol SystemIdentity;

// Main class for managed account switch coordinator.
@interface AccountSwitchCoordinator : SigninCoordinator

// Main initializer. `baseViewController` is used to present the sign-out and
// sign-in dialogs, in case it gets dismissed after sign-out, the
// `mainViewController` will be used to complete the sign-in flow. `newIdentity`
// is the identity to switch to. `rect` is the position of the account switch
// row and `rectAnchorView` is the anchor view of it.
- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                               newIdentity:(id<SystemIdentity>)newIdentity
                        mainViewController:(UIViewController*)mainViewController
                                      rect:(CGRect)rect
                    userDecisionCompletion:(void (^)())userDecisionCompletion
                            rectAnchorView:(UIView*)rectAnchorView
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                               accessPoint:
                                   (signin_metrics::AccessPoint)accessPoint
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_ACCOUNT_SWITCH_ACCOUNT_SWITCH_COORDINATOR_H_
