// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_SIGNIN_HISTORY_SYNC_SIGNIN_AND_HISTORY_SYNC_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_SIGNIN_HISTORY_SYNC_SIGNIN_AND_HISTORY_SYNC_COORDINATOR_H_

#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator.h"

namespace signin_metrics {
enum class AccessPoint : int;
enum class PromoAction : int;
}  // namespace signin_metrics

// Coordinator to present the Sign-In then the History Sync Opt-In screen.
// If there is no identity on the device, the SSO add account is displayed to
// sign-in and then the history sync is displayed.
@interface SignInAndHistorySyncCoordinator : SigninCoordinator

// Init the coordinator with its base `viewController`, the `browser`, from
// which `accessPoint` the sign in flow was initialized, using which
// `promoAction` (when relevant) and whether an `optionalHistorySync` (even if
// it is NO, it might still be skipped if the user previously approved it).
// `fullscreenPromo`: whether the promo should be displayed in a fullscreen
// modal.
// The `contextStyle` is used to customize content on screens.
- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                  contextStyle:(SigninContextStyle)contextStyle
                   accessPoint:(signin_metrics::AccessPoint)accessPoint
                   promoAction:(signin_metrics::PromoAction)promoAction
           optionalHistorySync:(BOOL)optionalHistorySync
               fullscreenPromo:(BOOL)fullscreenPromo
          continuationProvider:
              (const ChangeProfileContinuationProvider&)continuationProvider
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                              contextStyle:(SigninContextStyle)contextStyle
                               accessPoint:
                                   (signin_metrics::AccessPoint)accessPoint
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_SIGNIN_HISTORY_SYNC_SIGNIN_AND_HISTORY_SYNC_COORDINATOR_H_
