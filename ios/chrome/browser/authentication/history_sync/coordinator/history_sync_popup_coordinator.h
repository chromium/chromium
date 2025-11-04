// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_HISTORY_SYNC_COORDINATOR_HISTORY_SYNC_POPUP_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_HISTORY_SYNC_COORDINATOR_HISTORY_SYNC_POPUP_COORDINATOR_H_

#import "ios/chrome/browser/authentication/history_sync/public/history_sync_constants.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/animated_coordinator.h"

enum class SigninContextStyle;
namespace signin_metrics {
enum class AccessPoint : int;
}  // namespace signin_metrics

@class HistorySyncPopupCoordinator;
typedef NS_ENUM(NSUInteger, SigninCoordinatorResult);

// Delegate for the history sync coordinator.
@protocol HistorySyncPopupCoordinatorDelegate <NSObject>

// Called once `coordinator` wants to be stopped.
// `result` returns reason why the history sync opt-in dialog was closed.
// Not called if the coordinator's owner calls stop while the dialog is still
// opened.
- (void)historySyncPopupCoordinator:(HistorySyncPopupCoordinator*)coordinator
                didFinishWithResult:(HistorySyncResult)result;

@end

// Coordinator to present the History Sync Opt-In screen.
@interface HistorySyncPopupCoordinator : AnimatedCoordinator

// delegate for HistorySyncCoordinator.
@property(nonatomic, weak) id<HistorySyncPopupCoordinatorDelegate> delegate;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Creates the coordinator with the given parameters. showUserEmail should be
// YES if the user has seen the account email just before the History Sync
// Opt-In screen is shown. (e.g. if a manual sign-in is done)
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                             showUserEmail:(BOOL)showUserEmail
                         signOutIfDeclined:(BOOL)signOutIfDeclined
                                isOptional:(BOOL)isOptional
                              contextStyle:(SigninContextStyle)contextStyle
                               accessPoint:
                                   (signin_metrics::AccessPoint)accessPoint
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_HISTORY_SYNC_COORDINATOR_HISTORY_SYNC_POPUP_COORDINATOR_H_
