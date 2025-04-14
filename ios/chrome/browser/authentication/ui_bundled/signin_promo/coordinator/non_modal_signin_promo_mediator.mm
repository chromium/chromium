// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#import "ios/chrome/browser/authentication/ui_bundled/signin_promo/coordinator/non_modal_signin_promo_mediator.h"

#import "base/timer/timer.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"

namespace {
// Delay before showing the promo after the timer starts.
constexpr base::TimeDelta kPromoDisplayDelay = base::Seconds(3);
// Timeout period after which the promo will be automatically dismissed if not
// interacted with.
constexpr base::TimeDelta kPromoTimeout = base::Seconds(8);
}  // namespace

@interface NonModalSignInPromoMediator () <
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
}

- (instancetype)
    initWithAuthenticationService:(AuthenticationService*)authService
                  identityManager:(signin::IdentityManager*)identityManager {
  self = [super init];
  if (self) {
    _authService = authService;
    _identityManagerObserver =
        std::make_unique<signin::IdentityManagerObserverBridge>(identityManager,
                                                                self);
  }
  return self;
}

#pragma mark - Public

- (void)startPromoDisplayTimer {
  _displayTimer = nil;
  // Check if the user is already signed in
  if (_authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin)) {
    // Inform coordinator to dismiss UI.
    [self.delegate nonModalSignInPromoMediatorShouldDismiss:self];
    return;
  }

  // TODO(crbug.com/405135960): query tracker.

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
  _authService = nil;
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
  // Call the delegate to show the promo.
  [self.delegate nonModalSignInPromoMediatorTimerExpired:self];

  [self startPromoTimeoutTimer];
}

// Called when the timeout timer fires.
- (void)timeoutTimerFired {
  [self stopTimeOutTimers];
  // Inform coordinator to dismiss UI.
  [self.delegate nonModalSignInPromoMediatorShouldDismiss:self];
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  // If user signs in, stop showing the promo.
  if (_authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin)) {
    _displayTimer = nullptr;
    [self stopTimeOutTimers];
    // Inform coordinator to dismiss UI.
    [self.delegate nonModalSignInPromoMediatorShouldDismiss:self];
  }
}

@end
