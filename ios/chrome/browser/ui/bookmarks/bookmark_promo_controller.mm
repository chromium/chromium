// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_promo_controller.h"

#import <memory>

#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
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
  self.shouldShowSigninPromo = NO;
  if (_isIncognito) {
    return;
  }

  DCHECK(_browser);
  ChromeBrowserState* browserState =
      _browser->GetBrowserState()->GetOriginalChromeBrowserState();
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForBrowserState(browserState);
  if ([SigninPromoViewMediator
          shouldDisplaySigninPromoViewWithAccessPoint:
              signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER
                                authenticationService:authenticationService
                                          prefService:browserState
                                                          ->GetPrefs()]) {
    signin::IdentityManager* identityManager =
        IdentityManagerFactory::GetForBrowserState(browserState);
    self.shouldShowSigninPromo =
        !identityManager->HasPrimaryAccount(signin::ConsentLevel::kSync);
  }
}

#pragma mark - IdentityManagerObserverBridgeDelegate

// Called when a user changes the syncing state.
- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSync)) {
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

@end
