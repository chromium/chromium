// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#import "ios/chrome/browser/authentication/ui_bundled/signin_promo/coordinator/non_modal_signin_promo_mediator.h"

#import "base/timer/timer.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_promo/coordinator/non_modal_signin_promo_metrics_util.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_promo/signin_promo_types.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_observer_bridge.h"

namespace {
// Delay before showing the promo after the timer starts.
constexpr base::TimeDelta kPromoDisplayDelay = base::Seconds(1);
// Timeout period after which the promo will be automatically dismissed if not
// interacted with.
constexpr base::TimeDelta kPromoTimeout = base::Seconds(8);
}  // namespace

namespace signin {
// Returns whether it is possible to start a sign-in.
bool SigninIsPossible(AuthenticationService* auth_service) {
  return !auth_service->HasPrimaryIdentity(signin::ConsentLevel::kSignin) &&
         auth_service->SigninEnabled();
}

}  // namespace signin

@interface NonModalSignInPromoMediator () <
    AuthenticationServiceObserving,
    IdentityManagerObserverBridgeDelegate>
@end

@implementation NonModalSignInPromoMediator {
  // Bridge to observe changes in the identity manager.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;

  // Timer to delay displaying the promo.
  std::unique_ptr<base::OneShotTimer> _displayTimer;
  // Timer for dismissing the promo after it is shown.
  std::unique_ptr<base::OneShotTimer> _timeoutTimer;

  // The AuthenticationService used by the mediator to monitor sign-in status.
  raw_ptr<AuthenticationService> _authService;

  // Feature engagement tracker for determining if the promo can be shown.
  raw_ptr<feature_engagement::Tracker> _tracker;

  // The type of promo (password or bookmark) being shown.
  SignInPromoType _promoType;

  // Observer for auth service status changes.
  std::unique_ptr<AuthenticationServiceObserverBridge>
      _authServiceObserverBridge;
}

- (instancetype)
    initWithAuthenticationService:(AuthenticationService*)authService
                  identityManager:(signin::IdentityManager*)identityManager
         featureEngagementTracker:(feature_engagement::Tracker*)tracker
                        promoType:(SignInPromoType)promoType {
  self = [super init];
  if (self) {
    CHECK(authService, base::NotFatalUntil::M145);
    CHECK(identityManager, base::NotFatalUntil::M145);
    CHECK(tracker, base::NotFatalUntil::M145);
    _authService = authService;
    _tracker = tracker;
    _promoType = promoType;
    _identityManagerObserver =
        std::make_unique<signin::IdentityManagerObserverBridge>(identityManager,
                                                                self);
    _authServiceObserverBridge =
        std::make_unique<AuthenticationServiceObserverBridge>(authService,
                                                              self);
  }
  return self;
}

- (void)dealloc {
  CHECK(!_authServiceObserverBridge, base::NotFatalUntil::M145);
}

#pragma mark - Public

- (void)startPromoDisplayTimer {
  _displayTimer = nil;
  // Check if the user is already signed in or if sign-in is disabled
  if (!signin::SigninIsPossible(_authService)) {
    // Inform coordinator to dismiss UI.
    [self.delegate nonModalSignInPromoMediatorShouldDismiss:self];
    return;
  }

  // Check with the feature engagement tracker if promo should be shown.
  bool wouldShowPromo = false;

  if (_promoType == SignInPromoType::kPassword) {
    wouldShowPromo = _tracker->WouldTriggerHelpUI(
        feature_engagement::kIPHiOSPromoNonModalSigninPasswordFeature);
  } else if (_promoType == SignInPromoType::kBookmark) {
    wouldShowPromo = _tracker->WouldTriggerHelpUI(
        feature_engagement::kIPHiOSPromoNonModalSigninBookmarkFeature);
  }

  if (!wouldShowPromo) {
    [self.delegate nonModalSignInPromoMediatorShouldDismiss:self];
    return;
  }

  __weak __typeof(self) weakSelf = self;
  _displayTimer = std::make_unique<base::OneShotTimer>();
  _displayTimer->Start(FROM_HERE, kPromoDisplayDelay, base::BindOnce(^{
                         [weakSelf displayTimerFired];
                       }));
}

- (void)stopTimeOutTimers {
  _timeoutTimer = nullptr;
}

- (void)disconnect {
  _identityManagerObserver.reset();
  _authServiceObserverBridge.reset();
  _authService = nil;
  _tracker = nil;
}

#pragma mark - Private

// Starts the timeout timer to automatically dismiss the promo after the
// specified timeout period.
- (void)startPromoTimeoutTimer {
  [self stopTimeOutTimers];

  __weak __typeof(self) weakSelf = self;
  _timeoutTimer = std::make_unique<base::OneShotTimer>();
  _timeoutTimer->Start(FROM_HERE, kPromoTimeout, base::BindOnce(^{
                         [weakSelf timeoutTimerFired];
                       }));
}

// Called when the display timer fires.
- (void)displayTimerFired {
  _displayTimer = nullptr;

  bool shouldShowPromo = false;

  if (_promoType == SignInPromoType::kPassword) {
    shouldShowPromo = _tracker->ShouldTriggerHelpUI(
        feature_engagement::kIPHiOSPromoNonModalSigninPasswordFeature);
  } else if (_promoType == SignInPromoType::kBookmark) {
    shouldShowPromo = _tracker->ShouldTriggerHelpUI(
        feature_engagement::kIPHiOSPromoNonModalSigninBookmarkFeature);
  }

  if (!shouldShowPromo) {
    // Inform coordinator to dismiss UI and clean up.
    [self.delegate nonModalSignInPromoMediatorShouldDismiss:self];
    return;
  }

  // Call the delegate to show the promo.
  [self.delegate nonModalSignInPromoMediatorTimerExpired:self];

  [self startPromoTimeoutTimer];
}

// Called when the timeout timer fires.
- (void)timeoutTimerFired {
  [self stopTimeOutTimers];

  // Inform coordinator to dismiss UI.
  [self.delegate nonModalSignInPromoMediatorShouldDismiss:self];

  // Log the timeout event to metrics.
  LogNonModalSignInPromoAction(NonModalSignInPromoAction::kTimeout, _promoType);
}

- (void)maybeCancelPromo {
  // Cancels the current promo if it should not be displayed.
  if (signin::SigninIsPossible(_authService)) {
    return;
  }
  _displayTimer = nullptr;
  [self stopTimeOutTimers];

  // Inform coordinator to dismiss UI.
  [self.delegate nonModalSignInPromoMediatorShouldDismiss:self];
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  [self maybeCancelPromo];
}

#pragma mark - AuthenticationServiceObserving

- (void)onServiceStatusChanged {
  [self maybeCancelPromo];
}

@end
