// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNOUT_ACTION_SHEET_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNOUT_ACTION_SHEET_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/signin/constants.h"
#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"

class Browser;

// Delegate that handles user interactions with sign-out action sheet.
@protocol SignoutActionSheetCoordinatorDelegate

// Called when the user has selected a data retention strategy, either to
// clear or keep their data, before the strategy is implemented on signout.
- (void)didSelectSignoutDataRetentionStrategy;

@end

// Displays sign-out action sheet with options to clear or keep user data
// on the device. The user must be signed-in to use these actions.
@interface SignoutActionSheetCoordinator : ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Designated initializer.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                      rect:(CGRect)rect
                                      view:(UIView*)view
    NS_DESIGNATED_INITIALIZER;

// The delegate.
@property(nonatomic, weak) id<SignoutActionSheetCoordinatorDelegate> delegate;

// The title displayed for the sign-out alert.
@property(nonatomic, strong, readonly) NSString* title;

// Required callback to be used after sign-out is completed.
@property(nonatomic, copy) signin_ui::CompletionCallback completion;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNOUT_ACTION_SHEET_COORDINATOR_H_
