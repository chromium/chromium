// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/feed_top_section/feed_top_section_mediator.h"

#import "base/feature_list.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/sync/base/features.h"
#import "ios/chrome/browser/ntp/features.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
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

// Whether the signin promo should be shown. When the promo state changes, it
// will call `promoStateChanges:` on the delegate.
@property(nonatomic, assign) BOOL shouldShowSigninPromo;

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
  [self updateShouldShowSigninPromo];
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

#pragma mark - Setters

- (void)setShouldShowSigninPromo:(BOOL)shouldShowSigninPromo {
  if (_shouldShowSigninPromo == shouldShowSigninPromo) {
    return;
  }
  _shouldShowSigninPromo = shouldShowSigninPromo;

  // Update the consumer.
  self.consumer.shouldShowSigninPromo = _shouldShowSigninPromo;
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
        self.shouldShowSigninPromo = NO;
      }
      break;
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      [self updateShouldShowSigninPromo];
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
  [self.ntpDelegate handleFeedTopSectionClosed];
  self.shouldShowSigninPromo = NO;
}

#pragma mark - Private

- (void)updateShouldShowSigninPromo {
  self.shouldShowSigninPromo = NO;
  // Don't show the promo for incognito or start surface.
  if (self.isIncognito || [self.ntpDelegate isStartSurface] ||
      !self.isSignInPromoEnabled) {
    return;
  }

  // Don't show the promo if SetUpList might be displayed.
  PrefService* localState = GetApplicationContext()->GetLocalState();
  if (IsIOSSetUpListEnabled() &&
      set_up_list_utils::IsSetUpListActive(localState)) {
    return;
  }

  if ([SigninPromoViewMediator
          shouldDisplaySigninPromoViewWithAccessPoint:
              signin_metrics::AccessPoint::ACCESS_POINT_NTP_FEED_TOP_PROMO
                                authenticationService:self.authenticationService
                                          prefService:self.prefService]) {
    auto consent =
        base::FeatureList::IsEnabled(syncer::kReplaceSyncPromosWithSignInPromos)
            ? signin::ConsentLevel::kSignin
            : signin::ConsentLevel::kSync;
    self.shouldShowSigninPromo =
        !self.identityManager->HasPrimaryAccount(consent);
  }
}

@end
