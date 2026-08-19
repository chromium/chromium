// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/autofill_and_passwords_signin_promo_mediator.h"

#import "base/check_op.h"
#import "base/feature_list.h"
#import "base/memory/raw_ptr.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/sync/service/sync_service.h"
#import "ios/chrome/browser/authentication/ui_bundled/change_profile/change_profile_autofill_and_passwords_continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_promo_view_mediator.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/autofill_and_passwords_signin_promo_consumer.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_observer_bridge.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/sync/model/sync_observer_bridge.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface AutofillAndPasswordsSigninPromoMediator () <
    AuthenticationServiceObserving,
    IdentityManagerObserving,
    SyncObserverModelBridge>

// Whether the sign-in promo is shown or not.
@property(nonatomic, assign) BOOL shouldShowSignInPromo;

@end

@implementation AutofillAndPasswordsSigninPromoMediator {
  SigninPromoViewMediator* _signinPromoViewMediator;
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserverBridge;
  std::unique_ptr<AuthenticationServiceObserverBridge>
      _authServiceObserverBridge;
  std::unique_ptr<SyncObserverBridge> _syncObserverBridge;

  raw_ptr<ChromeAccountManagerService> _accountManagerService;
  raw_ptr<AuthenticationService> _authService;
  raw_ptr<PrefService> _prefService;
  raw_ptr<signin::IdentityManager> _identityManager;
  raw_ptr<syncer::SyncService> _syncService;

  // Whether the consumer's view is loaded.
  BOOL _consumerLoaded;
}

- (instancetype)
    initWithAccountManagerService:
        (ChromeAccountManagerService*)accountManagerService
                      authService:(AuthenticationService*)authService
                  identityManager:(signin::IdentityManager*)identityManager
                      prefService:(PrefService*)prefService
                      syncService:(syncer::SyncService*)syncService {
  self = [super init];
  if (self) {
    _accountManagerService = accountManagerService;
    _authService = authService;
    _identityManager = identityManager;
    _prefService = prefService;
    _syncService = syncService;

    _identityManagerObserverBridge =
        std::make_unique<signin::IdentityManagerObserverBridge>(
            _identityManager, self);
    _authServiceObserverBridge =
        std::make_unique<AuthenticationServiceObserverBridge>(_authService,
                                                              self);
    _syncObserverBridge =
        std::make_unique<SyncObserverBridge>(self, _syncService);
  }
  return self;
}

- (void)disconnect {
  [_consumer setSigninPromoDelegate:nil];

  [_signinPromoViewMediator disconnect];
  _signinPromoViewMediator = nil;

  _identityManagerObserverBridge.reset();
  _authServiceObserverBridge.reset();
  _syncObserverBridge.reset();
  _accountManagerService = nullptr;
  _authService = nullptr;
  _prefService = nullptr;
  _identityManager = nullptr;
  _syncService = nullptr;
}

- (void)contentDidLoad {
  _consumerLoaded = YES;
  if (!self.shouldShowSignInPromo) {
    return;
  }

  [self configureSigninPromoWithShouldShow:YES];
  [_signinPromoViewMediator signinPromoViewIsVisible];
}

- (void)updateSignInPromoVisibility {
  // TODO(crbug.com/542168449): Remove the sign-in promo code.
  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillAndPasswordsRemoveSignInPromo)) {
    self.shouldShowSignInPromo = NO;
    return;
  }

  if (_signinPromoViewMediator.showSpinner) {
    SigninPromoViewConfigurator* promoConfigurator =
        [_signinPromoViewMediator createConfigurator];
    [_consumer configureSigninPromoWithConfigurator:promoConfigurator];
    return;
  }

  const BOOL hasPrimaryAccount =
      _identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin);

  if (hasPrimaryAccount) {
    self.shouldShowSignInPromo = NO;
    return;
  }

  if (![SigninPromoViewMediator
          shouldDisplaySigninPromoViewWithAccessPoint:
              signin_metrics::AccessPoint::kSettingsAutofillAndPasswords
                                    signinPromoAction:SigninPromoAction::
                                                          kInstantSignin
                                authenticationService:_authService
                                          prefService:_prefService]) {
    self.shouldShowSignInPromo = NO;
    return;
  }

  self.shouldShowSignInPromo = YES;
}

- (void)signinDidCompleteWithResult:(SigninCoordinatorResult)result {
  [_signinPromoViewMediator signinDidCompleteWithResult:result];
  [self updateSignInPromoVisibility];
}

#pragma mark - Properties

- (void)setConsumer:(id<AutofillAndPasswordsSigninPromoConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;

  [self maybeInitializeSigninPromoViewMediator];
}

- (void)setDelegate:(id<SigninPromoViewMediatorDelegate>)delegate {
  if (_delegate == delegate) {
    return;
  }
  _delegate = delegate;

  [self maybeInitializeSigninPromoViewMediator];
}

#pragma mark - IdentityManagerObserving

- (void)primaryAccountDidChange:
    (const signin::PrimaryAccountChangeEvent&)event {
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet:
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      [self updateSignInPromoVisibility];
      break;
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
  }
}

#pragma mark - AuthenticationServiceObserving

- (void)onServiceStatusChanged {
  [self updateSignInPromoVisibility];
}

#pragma mark - SyncObserverModelBridge

- (void)onSyncStateChanged {
  [self updateSignInPromoVisibility];
}

#pragma mark - Private

// Initializes `_signinPromoViewMediator` if all required dependencies
// (consumer, delegate, account manager service) are available.
- (void)maybeInitializeSigninPromoViewMediator {
  if (_signinPromoViewMediator || !self.consumer || !self.delegate ||
      !_accountManagerService) {
    return;
  }

  _signinPromoViewMediator = [[SigninPromoViewMediator alloc]
                initWithIdentityManager:_identityManager
                  accountManagerService:_accountManagerService
                            authService:_authService
                            prefService:_prefService
                            syncService:_syncService
                            accessPoint:signin_metrics::AccessPoint::
                                            kSettingsAutofillAndPasswords
                               delegate:self.delegate
               accountSettingsPresenter:nil
      changeProfileContinuationProvider:base::BindRepeating([] {
        return CreateChangeProfileAutofillAndPasswordsContinuation();
      })];
  _signinPromoViewMediator.signinPromoAction =
      SigninPromoAction::kInstantSignin;
  _signinPromoViewMediator.consumer = self.consumer;
  [self.consumer setSigninPromoDelegate:_signinPromoViewMediator];

  [self updateSignInPromoVisibility];
}

// Custom setter for `shouldShowSignInPromo` that updates the consumer and
// the sign-in promo view mediator when the visibility changes.
- (void)setShouldShowSignInPromo:(BOOL)shouldShowSignInPromo {
  if (_shouldShowSignInPromo == shouldShowSignInPromo) {
    return;
  }

  _shouldShowSignInPromo = shouldShowSignInPromo;

  if (!_consumerLoaded) {
    return;
  }

  [self configureSigninPromoWithShouldShow:shouldShowSignInPromo];
  if (shouldShowSignInPromo) {
    [_signinPromoViewMediator signinPromoViewIsVisible];
  } else {
    if (_signinPromoViewMediator.isUsable) {
      [_signinPromoViewMediator signinPromoViewIsHidden];
    }
  }
}

// Creates a configurator for the sign-in promo and notifies the consumer
// to update its state.
- (void)configureSigninPromoWithShouldShow:(BOOL)shouldShow {
  SigninPromoViewConfigurator* promoConfigurator =
      [_signinPromoViewMediator createConfigurator];
  [_consumer
      promoStateChanged:shouldShow
      promoConfigurator:promoConfigurator
              promoText:l10n_util::GetNSString(
                            IDS_IOS_SIGNIN_PROMO_AUTOFILL_AND_PASSWORDS)];
}

@end
