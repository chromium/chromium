// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_HISTORY_SYNC_HISTORY_SYNC_POPUP_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_HISTORY_SYNC_HISTORY_SYNC_POPUP_COORDINATOR_H_

#import "ios/chrome/browser/first_run/ui_bundled/interruptible_chrome_coordinator.h"

namespace signin_metrics {
enum class AccessPoint : int;
}  // namespace signin_metrics

@class HistorySyncPopupCoordinator;

// Delegate for the history sync coordinator.
@protocol HistorySyncPopupCoordinatorDelegate <NSObject>

// Called once the coordinator is done.
// `result` returns reason why the history sync opt-in dialog was closed.
- (void)historySyncPopupCoordinator:(HistorySyncPopupCoordinator*)coordinator
                didFinishWithResult:(SigninCoordinatorResult)result;

@end

// Coordinator to present the History Sync Opt-In screen.
@interface HistorySyncPopupCoordinator : InterruptibleChromeCoordinator

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
                               accessPoint:
                                   (signin_metrics::AccessPoint)accessPoint
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_HISTORY_SYNC_HISTORY_SYNC_POPUP_COORDINATOR_H_
