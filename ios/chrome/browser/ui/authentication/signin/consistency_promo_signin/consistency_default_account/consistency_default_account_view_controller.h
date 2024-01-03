// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_DEFAULT_ACCOUNT_CONSISTENCY_DEFAULT_ACCOUNT_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_DEFAULT_ACCOUNT_CONSISTENCY_DEFAULT_ACCOUNT_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_utils.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_default_account/consistency_default_account_consumer.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_sheet/child_consistency_sheet_view_controller.h"

@class ConsistencyDefaultAccountViewController;
@protocol ConsistencyLayoutDelegate;

// Delegate protocol for ConsistencyDefaultAccountViewController.
@protocol ConsistencyDefaultAccountActionDelegate <NSObject>

// Called when the user taps on the skip or cancel button.
- (void)consistencyDefaultAccountViewControllerSkip:
    (ConsistencyDefaultAccountViewController*)viewController;
// Called when the user taps on the identity chooser button.
- (void)consistencyDefaultAccountViewControllerOpenIdentityChooser:
    (ConsistencyDefaultAccountViewController*)viewController;
// Called when the user taps on the continue button.
- (void)consistencyDefaultAccountViewControllerContinueWithSelectedIdentity:
    (ConsistencyDefaultAccountViewController*)viewController;
// Called when the user taps on the "Sign In…" button without an existing
// account.
- (void)consistencyDefaultAccountViewControllerAddAccountAndSignin:
    (ConsistencyDefaultAccountViewController*)viewController;

@end

// View controller for ConsistencyDefaultAccountCoordinator.
@interface ConsistencyDefaultAccountViewController
    : UIViewController <ChildConsistencySheetViewController,
                        ConsistencyDefaultAccountConsumer>

// Delegate for all the user actions.
@property(nonatomic, weak) id<ConsistencyDefaultAccountActionDelegate>
    actionDelegate;
@property(nonatomic, weak) id<ConsistencyLayoutDelegate> layoutDelegate;

// Starts the spinner and disables buttons.
- (void)startSpinner;
// Stops the spinner and enables buttons.
- (void)stopSpinner;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_DEFAULT_ACCOUNT_CONSISTENCY_DEFAULT_ACCOUNT_VIEW_CONTROLLER_H_
