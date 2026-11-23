// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/signin/non_modal_promo/coordinator/non_modal_signin_promo_coordinator.h"

#import "base/check.h"
#import "base/check_op.h"
#import "ios/chrome/browser/authentication/signin/non_modal_promo/coordinator/non_modal_signin_promo_mediator.h"
#import "ios/chrome/browser/authentication/ui_bundled/continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/browser_view/model/browser_view_visibility_notifier_browser_agent.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

@interface NonModalSignInPromoCoordinator () <
    NonModalSignInPromoMediatorDelegate>
@end

@implementation NonModalSignInPromoCoordinator {
  // The mediator responsible for the business logic. Set in `-start` and reset
  // in `-stop`.
  NonModalSignInPromoMediator* _mediator;
  // The type of promo to be displayed.
  NonModalSignInPromoType _promoType;
  // The sign-in coordinator if it is opened.
  SigninCoordinator* _signinCoordinator;
  // Whether the mediator ended the promo while `_signinCoordinator` was still
  // running, so this coordinator should end once sign-in finishes.
  BOOL _shouldDismissPending;
  // Whether this coordinator has been stopped.
  BOOL _stopped;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                 promoType:(NonModalSignInPromoType)promoType {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    CHECK(viewController);
    CHECK(browser);
    _promoType = promoType;
  }
  return self;
}

- (void)dealloc {
  CHECK(!_mediator, base::NotFatalUntil::M156);
}

#pragma mark - ChromeCoordinator

- (void)start {
  // Sign-in is for the regular profile, even from an incognito browser.
  ProfileIOS* profile = self.profile->GetOriginalProfile();
  _mediator = [[NonModalSignInPromoMediator alloc]
      initWithAuthenticationService:AuthenticationServiceFactory::GetForProfile(
                                        profile)
                    identityManager:IdentityManagerFactory::GetForProfile(
                                        profile)
           featureEngagementTracker:feature_engagement::TrackerFactory::
                                        GetForProfile(profile)
                       webStateList:self.browser->GetWebStateList()
                   overlayPresenter:OverlayPresenter::FromBrowser(
                                        self.browser,
                                        OverlayModality::kInfobarBanner)
      browserViewVisibilityNotifier:BrowserViewVisibilityNotifierBrowserAgent::
                                        FromBrowser(self.browser)
                          promoType:_promoType];
  _mediator.delegate = self;
  [_mediator startPromoDisplayTimer];
}

- (void)stop {
  if (_stopped) {
    return;
  }
  _stopped = YES;
  [_mediator disconnect];
  _mediator.delegate = nil;
  _mediator = nil;
  [self stopSigninCoordinator];
  [super stop];
}

#pragma mark - NonModalSignInPromoMediatorDelegate

- (BOOL)nonModalSignInPromoMediatorCanDisplayPromo:
    (NonModalSignInPromoMediator*)mediator {
  // The banner can only be seen on the browser view, e.g. not in the tab grid.
  return self.baseViewController.view.window != nil;
}

- (void)nonModalSignInPromoMediatorDidAccept:
    (NonModalSignInPromoMediator*)mediator {
  [self startSignin];
}

- (void)nonModalSignInPromoMediatorShouldDismiss:
    (NonModalSignInPromoMediator*)mediator {
  if (_signinCoordinator) {
    _shouldDismissPending = YES;
    return;
  }
  [self endPromo];
}

#pragma mark - Private

// Asks the delegate to stop this coordinator.
- (void)endPromo {
  if (_stopped) {
    return;
  }
  [self.delegate dismissNonModalSignInPromo:self];
}

// Starts the sign-in flow when the user taps the promo's action button.
- (void)startSignin {
  if (_signinCoordinator) {
    return;
  }
  signin_metrics::AccessPoint accessPoint;
  switch (_promoType) {
    case NonModalSignInPromoType::kPassword:
      accessPoint = signin_metrics::AccessPoint::kNonModalSigninPasswordPromo;
      break;
    case NonModalSignInPromoType::kBookmark:
      accessPoint = signin_metrics::AccessPoint::kNonModalSigninBookmarkPromo;
      break;
  }
  // Sign-in is for the regular profile, even from an incognito browser.
  Browser* regularBrowser = signin::GetRegularBrowser(self.browser);
  SigninContextStyle contextStyle = SigninContextStyle::kDefault;
  signin_metrics::PromoAction promoAction =
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO;
  _signinCoordinator = [SigninCoordinator
      signinAndHistorySyncCoordinatorWithBaseViewController:
          self.baseViewController
                                                    browser:regularBrowser
                                               contextStyle:contextStyle
                                                accessPoint:accessPoint
                                                promoAction:promoAction
                                        optionalHistorySync:YES
                                            fullscreenPromo:NO
                                       continuationProvider:
                                           DoNothingContinuationProvider()];
  __weak __typeof(self) weakSelf = self;
  _signinCoordinator.signinCompletion =
      ^(SigninCoordinator* coordinator,
        SigninCoordinatorResultOrIdentity result) {
        [weakSelf signinCoordinatorDidFinish:coordinator];
      };
  [_signinCoordinator start];
}

// Called when the sign-in flow started from the promo is finished.
- (void)signinCoordinatorDidFinish:(SigninCoordinator*)coordinator {
  CHECK_EQ(_signinCoordinator, coordinator);
  [self stopSigninCoordinator];
  if (_shouldDismissPending) {
    _shouldDismissPending = NO;
    [self endPromo];
  }
}

// Stops and cleans up the sign-in coordinator.
- (void)stopSigninCoordinator {
  [_signinCoordinator stop];
  _signinCoordinator = nil;
}

@end
