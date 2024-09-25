// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNOUT_ACTION_SHEET_SIGNOUT_ACTION_SHEET_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNOUT_ACTION_SHEET_SIGNOUT_ACTION_SHEET_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/signin/model/constants.h"

namespace signin_metrics {
enum class ProfileSignout;
}  // namespace signin_metrics

class Browser;
@class SignoutActionSheetCoordinator;

// Delegate that handles user interactions with sign-out action sheet.
@protocol SignoutActionSheetCoordinatorDelegate

// Called when the sign-out flow is in progress. The UI needs to be blocked
// until the sign-out flow is done.
- (void)signoutActionSheetCoordinatorPreventUserInteraction:
    (SignoutActionSheetCoordinator*)coordinator;

// Called when the sign-out flow is done. The UI can be unblocked.
- (void)signoutActionSheetCoordinatorAllowUserInteraction:
    (SignoutActionSheetCoordinator*)coordinator;

@end

// Initiate a sign-out action.
// If the sync feature is disabled, directly sign-out, and display a toast.
// If the sync feature is enabled, displays sign-out action sheet with options
// to clear or keep user data on the device.
// The user must be signed-in to use these actions. The owner is responsible to
// block the UI, when the sign-out flow is in progress. The UI needs to be
// blocked and unblocked using methods from
// SignoutActionSheetCoordinatorDelegate.
// TODO(crbug.com/40066949): Update this comment when syncing users no longer
// exist on iOS.
@interface SignoutActionSheetCoordinator : ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Designated initializer.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                      rect:(CGRect)rect
                                      view:(UIView*)view
                  forceSnackbarOverToolbar:(BOOL)forceSnackbarOverToolbar
                                withSource:(signin_metrics::ProfileSignout)
                                               signout_source_metric
    NS_DESIGNATED_INITIALIZER;

// The delegate.
@property(nonatomic, weak) id<SignoutActionSheetCoordinatorDelegate> delegate;

// The title displayed for the sign-out alert.
@property(nonatomic, strong, readonly) NSString* title;

// The message displayed for the sign-out alert.
@property(nonatomic, strong, readonly) NSString* message;

// Required callback to be used after sign-out is completed.
@property(nonatomic, copy)
    signin_ui::SignoutCompletionCallback signoutCompletion;

// Whether to warns feature won’t be available anymore when user is not
// synced.
@property(nonatomic, assign) BOOL showUnavailableFeatureDialogHeader;

// Whether the sign-out action is part of an account switch or not. If YES, no
// signout snackbar will be displayed, and signout dialog strings will indicate
// an account switch. Default is NO.
@property(nonatomic, assign) BOOL accountSwitch;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNOUT_ACTION_SHEET_SIGNOUT_ACTION_SHEET_COORDINATOR_H_
