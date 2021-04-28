// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_SIGNIN_ERROR_CONSISTENCY_SIGNIN_ERROR_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_SIGNIN_ERROR_CONSISTENCY_SIGNIN_ERROR_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "google_apis/gaia/google_service_auth_error.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/bottom_sheet/child_bottom_sheet_view_controller.h"

@class ConsistencySigninErrorViewController;

@protocol ConsistencySigninErrorViewControllerDelegate <NSObject>

// Retries the sign-in in the case of an authentication error.
- (void)consistencySigninErrorViewControllerDidTapRetrySignin:
    (ConsistencySigninErrorViewController*)viewController;

@end

// Displays a sign-in error message based on the error state.
@interface ConsistencySigninErrorViewController
    : UIViewController <ChildBottomSheetViewController>

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNAme
                         bundle:(NSBundle*)nibBundle NS_UNAVAILABLE;

// Designated initializer.
- (instancetype)initWithAuthErrorState:
    (const GoogleServiceAuthError::State&)errorState NS_DESIGNATED_INITIALIZER;

@property(nonatomic, weak) id<ConsistencySigninErrorViewControllerDelegate>
    delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_SIGNIN_ERROR_CONSISTENCY_SIGNIN_ERROR_VIEW_CONTROLLER_H_
