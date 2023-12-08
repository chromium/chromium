// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/feed_top_section/feed_top_section_mediator.h"

#import "base/feature_list.h"
#import "base/time/time.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/sync/base/features.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/utils.h"
#import "ios/chrome/browser/ui/ntp/feed_top_section/feed_top_section_consumer.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_delegate.h"

@interface FeedTopSectionMediator () <IdentityManagerObserverBridgeDelegate> {
  // Observes changes in identity.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityObserverBridge;
}

@property(nonatomic, assign) AuthenticationService* authenticationService;
@property(nonatomic, assign) signin::IdentityManager* identityManager;
@property(nonatomic, assign) BOOL isIncognito;
@property(nonatomic, assign) PrefService* prefService;

// Consumer for this mediator.
@property(nonatomic, weak) id<FeedTopSectionConsumer> consumer;

@end

@implementation FeedTopSectionMediator

// FeedTopSectionViewControllerDelegate
@synthesize signinPromoConfigurator = _signinPromoConfigurator;

- (instancetype)initWithConsumer:(id<FeedTopSectionConsumer>)consumer
                 identityManager:(signin::IdentityManager*)identityManager
                     authService:(AuthenticationService*)authenticationService
                     isIncognito:(BOOL)isIncognito
                     prefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    _authenticationService = authenticationService;
    _identityManager = identityManager;
    _identityObserverBridge.reset(
        new signin::IdentityManagerObserverBridge(_identityManager, self));
    _isIncognito = isIncognito;
    _prefService = prefService;
    _consumer = consumer;
  }
  return self;
}

- (void)setUp {
  [self updateShouldShowPromo];
}

- (void)dealloc {
  [self shutdown];
}

- (void)shutdown {
  _identityObserverBridge.reset();
  self.authenticationService = nullptr;
  self.identityManager = nullptr;
  self.prefService = nullptr;
}

#pragma mark - FeedTopSectionViewControllerDelegate

- (SigninPromoViewConfigurator*)signinPromoConfigurator {
  if (!_signinPromoConfigurator) {
    _signinPromoConfigurator = [_signinPromoMediator createConfigurator];
  }
  return _signinPromoConfigurator;
}

#pragma mark - IdentityManagerObserverBridgeDelegate

// Called when a user changes the syncing state.
- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  auto consent =
      base::FeatureList::IsEnabled(syncer::kReplaceSyncPromosWithSignInPromos)
          ? signin::ConsentLevel::kSignin
          : signin::ConsentLevel::kSync;
  switch (event.GetEventTypeFor(consent)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet:
      if (!self.signinPromoMediator.showSpinner) {
        // User has signed in, stop showing the promo.
        [self updateShouldShowPromo];
      }
      break;
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      [self updateShouldShowPromo];
      break;
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
  }
}

#pragma mark - SigninPromoViewConsumer

- (void)configureSigninPromoWithConfigurator:
            (SigninPromoViewConfigurator*)configurator
                             identityChanged:(BOOL)identityChanged {
  // No-op: The NTP is always recreated when the identity changes, so this is
  // not needed.
}

- (void)signinPromoViewMediatorCloseButtonWasTapped:
    (SigninPromoViewMediator*)mediator {
  [self.NTPDelegate handleFeedTopSectionClosed];
  [self.consumer hidePromo];
  [self.NTPDelegate updateFeedLayout];
}

#pragma mark - Private

- (BOOL)isUserSignedIn {
  auto consent =
      base::FeatureList::IsEnabled(syncer::kReplaceSyncPromosWithSignInPromos)
          ? signin::ConsentLevel::kSignin
          : signin::ConsentLevel::kSync;
  return self.identityManager->HasPrimaryAccount(consent);
}

// TODO(b/315161586): Disable notifications promo if DSE changes.
- (BOOL)shouldShowNotificationsPromo {
  // Check feature flag.
  if (!IsContentPushNotificationsPromoEnabled()) {
    return false;
  }

  // Check if user is signed in.
  if (![self isUserSignedIn]) {
    return false;
  }

  // Check if override is active. Override only works if the user is signed in.
  if (experimental_flags::ShouldForceContentNotificationsPromo()) {
    return true;
  }

  int notificationsPromoTimesShown =
      self.prefService->GetInteger(prefs::kNotificationsPromoTimesShown);
  int notificationsPromoTimesDismissed =
      self.prefService->GetInteger(prefs::kNotificationsPromoTimesDismissed);

  base::Time now = base::Time::Now();
  // Check if promo has been displayed `kNotificationsPromoMaxShownCount`.
  if (notificationsPromoTimesShown >= kNotificationsPromoMaxShownCount) {
    return false;
  }

  // Check if promo is in cooldown from dismissal.
  if (notificationsPromoTimesDismissed >=
      kNotificationsPromoMaxDismissedCount) {
    return false;
  }
  // Check if the pref has been initialized before (base::Time() returns the
  // null value for a base::Time type.
  if (self.prefService->GetTime(prefs::kNotificationsPromoLastDismissed) !=
      base::Time()) {
    if (now -
            self.prefService->GetTime(prefs::kNotificationsPromoLastDismissed) <
        kNotificationsPromoDismissedCooldownTime) {
      return false;
    }
  }
  // Check if it has been less than `kNotificationsPromoShownCooldownTime`.
  if (now - self.prefService->GetTime(prefs::kNotificationsPromoLastShown) <
      kNotificationsPromoShownCooldownTime) {
    return false;
  }
  // If all the conditions pass above, update prefs and return true.
  self.prefService->SetTime(prefs::kNotificationsPromoLastShown, now);
  notificationsPromoTimesShown += 1;
  self.prefService->SetTime(prefs::kNotificationsPromoLastShown, now);
  self.prefService->SetInteger(prefs::kNotificationsPromoTimesShown,
                               notificationsPromoTimesShown);
  return true;
}

- (BOOL)shouldShowSigninPromo {
  // Don't show the promo if the account is not eligible for a SigninPromo.
  BOOL isAccountEligibleForSignInPromo = NO;
  if ([SigninPromoViewMediator
          shouldDisplaySigninPromoViewWithAccessPoint:
              signin_metrics::AccessPoint::ACCESS_POINT_NTP_FEED_TOP_PROMO
                                authenticationService:self.authenticationService
                                          prefService:self.prefService]) {
    isAccountEligibleForSignInPromo = ![self isUserSignedIn];
  }
  // Don't show the promo for incognito or start surface or if account is not
  // eligible.
  BOOL isStartSurfaceOrIncognito = self.isIncognito ||
                                   [self.NTPDelegate isStartSurface] ||
                                   !self.isSignInPromoEnabled;
  if (!isStartSurfaceOrIncognito && isAccountEligibleForSignInPromo) {
    return true;
  }
  return false;
}

- (void)updateShouldShowPromo {
  // Don't show any promo if Set Up List is Enabled.
  PrefService* localState = GetApplicationContext()->GetLocalState();
  if (set_up_list_utils::IsSetUpListActive(localState)) {
    // Hide promo as a safeguard in case it is being shown.
    [self.consumer hidePromo];
    return;
  }

  if ([self shouldShowSigninPromo]) {
    self.consumer.visiblePromoViewType = PromoViewTypeSignin;
    [self.consumer showPromo];
    return;
  }

  if ([self shouldShowNotificationsPromo]) {
    self.consumer.visiblePromoViewType = PromoViewTypeNotifications;
    [self.consumer showPromo];
    return;
  }
}

@end
