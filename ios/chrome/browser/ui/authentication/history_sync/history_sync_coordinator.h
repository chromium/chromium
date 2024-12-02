// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_HISTORY_SYNC_HISTORY_SYNC_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_HISTORY_SYNC_HISTORY_SYNC_COORDINATOR_H_

#import "base/ios/block_types.h"
#import "ios/chrome/browser/first_run/ui_bundled/interruptible_chrome_coordinator.h"

class AuthenticationService;
@class HistorySyncCoordinator;
class PrefService;

namespace signin_metrics {
enum class AccessPoint : int;
}  // namespace signin_metrics

namespace syncer {
class SyncService;
}  // namespace syncer

// The reasons why the History Sync Opt-In screen should be skipped instead of
// being shown to the user. `kNone` indicates that the screen should not be
// skipped.
enum class HistorySyncSkipReason {
  kNone,
  kNotSignedIn,
  kSyncForbiddenByPolicies,
  kAlreadyOptedIn,
  kDeclinedTooOften,
};

// Delegate for the history sync coordinator.
@protocol HistorySyncCoordinatorDelegate <NSObject>

// Called once the dialog can be closed.
- (void)closeHistorySyncCoordinator:
            (HistorySyncCoordinator*)historySyncCoordinator
                     declinedByUser:(BOOL)declined;

@end

// Coordinator for history sync view. The current implementation supports only
// showing the view in a navigation controller.
@interface HistorySyncCoordinator : InterruptibleChromeCoordinator

// Checks if the History Sync Opt-In screen should be skipped, and returns the
// corresponding reason. HistorySyncSkipReason::kNone means that the screen
// should not be skipped.
+ (HistorySyncSkipReason)
    getHistorySyncOptInSkipReason:(syncer::SyncService*)syncService
            authenticationService:(AuthenticationService*)authenticationService
                      prefService:(PrefService*)prefService
            isHistorySyncOptional:(BOOL)isOptional;

// Records metric if the History Sync Opt-In screen is skipped for the given
// reason, for the given access point.
+ (void)recordHistorySyncSkipMetric:(HistorySyncSkipReason)reason
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
                         accessPoint:(signin_metrics::AccessPoint)accessPoint
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_HISTORY_SYNC_HISTORY_SYNC_COORDINATOR_H_
