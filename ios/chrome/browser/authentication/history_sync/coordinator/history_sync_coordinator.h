// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_HISTORY_SYNC_COORDINATOR_HISTORY_SYNC_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_HISTORY_SYNC_COORDINATOR_HISTORY_SYNC_COORDINATOR_H_

#import "base/ios/block_types.h"
#import "ios/chrome/browser/authentication/history_sync/public/history_sync_constants.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class HistorySyncCoordinator;
namespace history_sync {
enum class HistorySyncSkipReason;
}
enum class SigninContextStyle;
namespace signin_metrics {
enum class AccessPoint : int;
}  // namespace signin_metrics

// Delegate for the history sync coordinator.
@protocol HistorySyncCoordinatorDelegate <NSObject>

// Called once `historySyncCoordinator` wants to be stopped.
// `result` returns reason why the history sync opt-in dialog was closed.
// Not called if the coordinator's owner calls stop while the dialog is still
// opened.
- (void)historySyncCoordinator:(HistorySyncCoordinator*)historySyncCoordinator
                    withResult:(HistorySyncResult)result;

@end

// Coordinator for history sync view. The current implementation supports only
// showing the view in a navigation controller.
@interface HistorySyncCoordinator : ChromeCoordinator

// Records metric if the History Sync Opt-In screen is skipped for the given
// reason, for the given access point.
+ (void)recordHistorySyncSkipMetric:(history_sync::HistorySyncSkipReason)reason
                        accessPoint:(signin_metrics::AccessPoint)accessPoint;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Initiates a HistorySyncCoordinator with `navigationController`,
// `browser` and `delegate`.
- (instancetype)
    initWithBaseNavigationController:
        (UINavigationController*)navigationController
                             browser:(Browser*)browser
                            delegate:
                                (id<HistorySyncCoordinatorDelegate>)delegate
                            firstRun:(BOOL)firstRun
                       showUserEmail:(BOOL)showUserEmail
                          isOptional:(BOOL)isOptional
                        contextStyle:(SigninContextStyle)contextStyle
                         accessPoint:(signin_metrics::AccessPoint)accessPoint
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_HISTORY_SYNC_COORDINATOR_HISTORY_SYNC_COORDINATOR_H_
