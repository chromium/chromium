// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/ui_bundled/home/bookmark_promo_controller.h"

#import <memory>

#import "components/bookmarks/common/bookmark_features.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/sync/service/sync_service.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_utils_ios.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/account_settings_presenter.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_configurator.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_consumer.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view_mediator.h"

@interface BookmarkPromoController () <SigninPromoViewConsumer,
                                       IdentityManagerObserverBridgeDelegate>

@end

@implementation BookmarkPromoController {
  base::WeakPtr<Browser> _browser;
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserverBridge;
  // Mediator to use for the sign-in promo view displayed in the bookmark view.
  SigninPromoViewMediator* _signinPromoViewMediator;
}

- (instancetype)initWithBrowser:(Browser*)browser
                    syncService:(syncer::SyncService*)syncService
                       delegate:(id<BookmarkPromoControllerDelegate>)delegate
                signinPresenter:(id<SigninPresenter>)signinPresenter
       accountSettingsPresenter:
           (id<AccountSettingsPresenter>)accountSettingsPresenter {
  DCHECK(browser);
  self = [super init];
  if (self) {
    _delegate = delegate;
    ProfileIOS* profile = browser->GetProfile()->GetOriginalProfile();
    _browser = browser->AsWeakPtr();
    _identityManagerObserverBridge.reset(
        new signin::IdentityManagerObserverBridge(
            IdentityManagerFactory::GetForProfile(profile), self));
    _signinPromoViewMediator = [[SigninPromoViewMediator alloc]
        initWithAccountManagerService:ChromeAccountManagerServiceFactory::
                                          GetForProfile(profile)
                          authService:AuthenticationServiceFactory::
                                          GetForProfile(profile)
                          prefService:profile->GetPrefs()
                          syncService:syncService
                          accessPoint:signin_metrics::AccessPoint::
                                          ACCESS_POINT_BOOKMARK_MANAGER
                      signinPresenter:signinPresenter
             accountSettingsPresenter:accountSettingsPresenter];
    _signinPromoViewMediator.consumer = self;
    _signinPromoViewMediator.dataTypeToWaitForInitialSync =
        syncer::DataType::BOOKMARKS;
    [self updateShouldShowSigninPromo];
  }
  return self;
}

- (void)shutdown {
  [_signinPromoViewMediator disconnect];
  _signinPromoViewMediator = nil;
  _browser = nullptr;
  _identityManagerObserverBridge.reset();
}

- (void)hidePromoCell {
  DCHECK(_browser);
  self.shouldShowSigninPromo = NO;
}

- (void)setShouldShowSigninPromo:(BOOL)shouldShowSigninPromo {
  if (_shouldShowSigninPromo != shouldShowSigninPromo) {
    _shouldShowSigninPromo = shouldShowSigninPromo;
    [self.delegate promoStateChanged:shouldShowSigninPromo];
  }
}

- (void)updateShouldShowSigninPromo {
  DCHECK(_browser);
  ProfileIOS* profile = _browser->GetProfile()->GetOriginalProfile();
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForProfile(profile);
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(profile);
  syncer::SyncService* syncService = SyncServiceFactory::GetForProfile(profile);

  std::optional<SigninPromoAction> signinPromoAction;
  if (!identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    signinPromoAction = SigninPromoAction::kInstantSignin;
  } else if (identityManager->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    // TODO(crbug.com/40066949): Simplify once kSync becomes unreachable or is
    // deleted from the codebase. See ConsentLevel::kSync documentation for
    // details.
    // If the user is already syncing, the promo should not be visible.
    self.shouldShowSigninPromo = NO;
    return;
  } else if (!bookmark_utils_ios::IsAccountBookmarkStorageOptedIn(
                 syncService)) {
    if (self.shouldShowSigninPromo &&
        _signinPromoViewMediator.signinPromoAction !=
            SigninPromoAction::kReviewAccountSettings) {
      // The promo was visible with another action. `shouldShowSigninPromo`
      // needs to be toggled first to reflect this change.
      self.shouldShowSigninPromo = NO;
    }
    // The user signed in, but not opted into account bookmarks storage - show
    // review account settings promo.
    signinPromoAction = SigninPromoAction::kReviewAccountSettings;
  } else if (self.signinPromoViewMediator.showSpinner) {
    // The user is opted into syncing bookmarks, but the first sync is not
    // finished yet - keep the promo visible with the same action to show the
    // spinner.
    signinPromoAction = SigninPromoAction::kInstantSignin;
  } else {
    // The user is opted into syncing bookmarks and the first sync is done -
    // hide the promo.
    self.shouldShowSigninPromo = NO;
    return;
  }

  CHECK(signinPromoAction.has_value());
  if (![SigninPromoViewMediator
          shouldDisplaySigninPromoViewWithAccessPoint:
              signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER
                                    signinPromoAction:signinPromoAction.value()
                                authenticationService:authenticationService
                                          prefService:profile->GetPrefs()]) {
    self.shouldShowSigninPromo = NO;
    return;
  }

  _signinPromoViewMediator.signinPromoAction = signinPromoAction.value();
  self.shouldShowSigninPromo = YES;
}

#pragma mark - IdentityManagerObserverBridgeDelegate

// Called when a user changes the syncing state.
- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  // The account storage promo is not shown if the user is signed-in, so
  // events with sign-in consent level should be captured and handled.
  [self handlePrimaryAccountChange:event
                      consentLevel:signin::ConsentLevel::kSignin];
}

#pragma mark - SigninPromoViewConsumer

- (void)configureSigninPromoWithConfigurator:
            (SigninPromoViewConfigurator*)configurator
                             identityChanged:(BOOL)identityChanged {
  [self.delegate configureSigninPromoWithConfigurator:configurator
                                      identityChanged:identityChanged];
}

- (void)promoProgressStateDidChange {
  [self updateShouldShowSigninPromo];
}

- (void)signinDidFinish {
  [self updateShouldShowSigninPromo];
}

- (void)signinPromoViewMediatorCloseButtonWasTapped:
    (SigninPromoViewMediator*)mediator {
  [self updateShouldShowSigninPromo];
}

#pragma mark - Private methods

// Handles the given primary account change event for the given consent level.
- (void)handlePrimaryAccountChange:
            (const signin::PrimaryAccountChangeEvent&)event
                      consentLevel:(signin::ConsentLevel)consentLevel {
  switch (event.GetEventTypeFor(consentLevel)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet:
      if (!self.signinPromoViewMediator.showSpinner) {
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

@end
