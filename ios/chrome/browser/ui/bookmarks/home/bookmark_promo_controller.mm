// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/home/bookmark_promo_controller.h"

#import <memory>

#import "components/bookmarks/common/bookmark_features.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_configurator.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_consumer.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view_mediator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BookmarkPromoController () <SigninPromoViewConsumer,
                                       IdentityManagerObserverBridgeDelegate> {
  bool _isIncognito;
  base::WeakPtr<Browser> _browser;
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserverBridge;
}

// Mediator to use for the sign-in promo view displayed in the bookmark view.
@property(nonatomic, readwrite, strong)
    SigninPromoViewMediator* signinPromoViewMediator;

@end

@implementation BookmarkPromoController

@synthesize delegate = _delegate;
@synthesize shouldShowSigninPromo = _shouldShowSigninPromo;
@synthesize signinPromoViewMediator = _signinPromoViewMediator;

- (instancetype)initWithBrowser:(Browser*)browser
                       delegate:(id<BookmarkPromoControllerDelegate>)delegate
                      presenter:(id<SigninPresenter>)presenter
             baseViewController:(UIViewController*)baseViewController {
  DCHECK(browser);
  self = [super init];
  if (self) {
    _delegate = delegate;
    ChromeBrowserState* browserState =
        browser->GetBrowserState()->GetOriginalChromeBrowserState();
    // TODO(crbug.com/1426262): Decide whether to show the signin promo in
    // incognito mode and revisit this code.
    // Incognito browser can go away before this class is released (once the
    // last incognito winwdow is closed), this code avoids keeping a pointer to
    // it.
    _isIncognito = browserState->IsOffTheRecord();
    if (!_isIncognito) {
      _browser = browser->AsWeakPtr();
      _identityManagerObserverBridge.reset(
          new signin::IdentityManagerObserverBridge(
              IdentityManagerFactory::GetForBrowserState(browserState), self));
      _signinPromoViewMediator = [[SigninPromoViewMediator alloc]
                initWithBrowser:browser
          accountManagerService:ChromeAccountManagerServiceFactory::
                                    GetForBrowserState(browserState)
                    authService:AuthenticationServiceFactory::
                                    GetForBrowserState(browserState)
                    prefService:browserState->GetPrefs()
                    accessPoint:signin_metrics::AccessPoint::
                                    ACCESS_POINT_BOOKMARK_MANAGER
                      presenter:presenter
             baseViewController:baseViewController];
      _signinPromoViewMediator.signInOnly = base::FeatureList::IsEnabled(
          bookmarks::kEnableBookmarksAccountStorage);
      _signinPromoViewMediator.consumer = self;
    }
    [self updateShouldShowSigninPromo];
  }
  return self;
}

- (void)dealloc {
  [self shutdown];
}

- (void)shutdown {
  [_signinPromoViewMediator disconnect];
  _signinPromoViewMediator = nil;
  _browser = nullptr;
  _identityManagerObserverBridge.reset();
}

- (void)hidePromoCell {
  DCHECK(!_isIncognito);
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
  if (_isIncognito) {
    self.shouldShowSigninPromo = NO;
    return;
  }
  DCHECK(_browser);
  ChromeBrowserState* browserState =
      _browser->GetBrowserState()->GetOriginalChromeBrowserState();
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForBrowserState(browserState);
  if (![SigninPromoViewMediator
          shouldDisplaySigninPromoViewWithAccessPoint:
              signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER
                                authenticationService:authenticationService
                                          prefService:browserState
                                                          ->GetPrefs()]) {
    self.shouldShowSigninPromo = NO;
    return;
  }
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForBrowserState(browserState);
  if (!identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    if (base::FeatureList::IsEnabled(
            bookmarks::kEnableBookmarksAccountStorage)) {
      PrefService* prefs = browserState->GetPrefs();
      const std::string lastSignedInGaiaId =
          prefs->GetString(prefs::kGoogleServicesLastGaiaId);
      // If the last signed-in user did not remove data during sign-out, don't
      // show the signin promo.
      self.shouldShowSigninPromo = lastSignedInGaiaId.empty();
    } else {
      // If the user is not signed in, the promo should be visible.
      self.shouldShowSigninPromo = YES;
    }
    return;
  }
  if (identityManager->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    // If the user is already syncing, the promo should not be visible.
    self.shouldShowSigninPromo = NO;
    return;
  }
  if (!base::FeatureList::IsEnabled(
          bookmarks::kEnableBookmarksAccountStorage)) {
    // If the account storage feature is not available, the promo should be
    // visible to show "Turn on Sync promo".
    self.shouldShowSigninPromo = YES;
    return;
  }
  // if the account storage feature is available and the user is signed in only,
  // the promo should be visible only if the first sync is not finished yet.
  // This is show the activity indicator.
  self.shouldShowSigninPromo = [self.delegate isPerformingInitialSync];
}

#pragma mark - IdentityManagerObserverBridgeDelegate

// Called when a user changes the syncing state.
- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  if (base::FeatureList::IsEnabled(bookmarks::kEnableBookmarksAccountStorage)) {
    // The account storage promo is not shown if the user is signed-in, so
    // events with sign-in consent level should be captured and handled.
    [self handlePrimaryAccountChange:event
                        consentLevel:signin::ConsentLevel::kSignin];
  } else {
    [self handlePrimaryAccountChange:event
                        consentLevel:signin::ConsentLevel::kSync];
  }
}

#pragma mark - SigninPromoViewConsumer

- (void)configureSigninPromoWithConfigurator:
            (SigninPromoViewConfigurator*)configurator
                             identityChanged:(BOOL)identityChanged {
  [self.delegate configureSigninPromoWithConfigurator:configurator
                                      identityChanged:identityChanged];
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
      if (!self.signinPromoViewMediator.signinInProgress) {
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
