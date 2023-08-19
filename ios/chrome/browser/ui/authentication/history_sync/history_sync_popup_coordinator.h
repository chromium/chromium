// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_HISTORY_SYNC_HISTORY_SYNC_POPUP_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_HISTORY_SYNC_HISTORY_SYNC_POPUP_COORDINATOR_H_

#import "ios/chrome/browser/ui/first_run/interruptible_chrome_coordinator.h"

namespace signin_metrics {
enum class AccessPoint : int;
}  // namespace signin_metrics

@class HistorySyncPopupCoordinator;

// Delegate for the history sync coordinator.
@protocol HistorySyncPopupCoordinatorDelegate <NSObject>

// Called once the dialog has been closed.
// `declined` is YES if the user explicitly declined the history sync opt-in
// dialog.
- (void)historySyncPopupCoordinator:(HistorySyncPopupCoordinator*)coordinator
         didCloseWithDeclinedByUser:(BOOL)declined;

@end

// Coordinator to present the History Sync Opt-In screen.
@interface HistorySyncPopupCoordinator : InterruptibleChromeCoordinator

// delegate for HistorySyncCoordinator.
@property(nonatomic, weak) id<HistorySyncPopupCoordinatorDelegate> delegate;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                       dedicatedSignInDone:(BOOL)dedicatedSignInDone
                               accessPoint:
                                   (signin_metrics::AccessPoint)accessPoint
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_HISTORY_SYNC_HISTORY_SYNC_POPUP_COORDINATOR_H_
