// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_FULLSCREEN_SIGNIN_SCREEN_COORDINATOR_FULLSCREEN_SIGNIN_SCREEN_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_FULLSCREEN_SIGNIN_SCREEN_COORDINATOR_FULLSCREEN_SIGNIN_SCREEN_COORDINATOR_H_

#import "ios/chrome/browser/authentication/ui_bundled/change_profile_continuation_provider.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol FirstRunScreenDelegate;
@protocol SystemIdentity;
enum class SigninContextStyle;
namespace signin_metrics {
enum class AccessPoint : int;
enum class PromoAction : int;
}  // namespace signin_metrics

// Coordinator responsible for presenting the fullscreen sign-in UI.
// It is a child coordinator managed by the FullscreenSigninCoordinator.
@interface FullscreenSigninScreenCoordinator : ChromeCoordinator

// `identity` is used to select a specific identity. If there are multiple
// accounts on device this identity will be shown as chosen in account picker
// flow.
@property(nonatomic, weak) id<SystemIdentity> identity;

// If `YES`, will allow switching signed-in account.
@property(nonatomic, assign) BOOL canSwitchAccount;

// Initiates a FullscreenSigninScreenCoordinator with `navigationController`,
// `browser` and `delegate`.
// The `delegate` parameter is for handling the transfer between screens.
// The `contextStyle` is used to customize content on screens.
// The `accessPoint` and `promoAction` parameters are used for logging.
- (instancetype)
     initWithBaseNavigationController:
         (UINavigationController*)navigationController
                              browser:(Browser*)browser
                             delegate:(id<FirstRunScreenDelegate>)delegate
                         contextStyle:(SigninContextStyle)contextStyle
                          accessPoint:(signin_metrics::AccessPoint)accessPoint
                          promoAction:(signin_metrics::PromoAction)promoAction
    changeProfileContinuationProvider:(const ChangeProfileContinuationProvider&)
                                          changeProfileContinuationProvider
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_FULLSCREEN_SIGNIN_SCREEN_COORDINATOR_FULLSCREEN_SIGNIN_SCREEN_COORDINATOR_H_
