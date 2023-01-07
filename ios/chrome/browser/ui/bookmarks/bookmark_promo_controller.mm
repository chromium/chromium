// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_promo_controller.h"

#import <memory>

#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_configurator.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_consumer.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view_mediator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BookmarkPromoController ()<SigninPromoViewConsumer,
                                      IdentityManagerObserverBridgeDelegate> {
  bool _isIncognito;
  ChromeBrowserState* _browserState;
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

- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState
                            delegate:
                                (id<BookmarkPromoControllerDelegate>)delegate
                           presenter:(id<SigninPresenter>)presenter {
  self = [super init];
  if (self) {
    _delegate = delegate;
    // Incognito browserState can go away before this class is released, this
    // code avoids keeping a pointer to it.
    _isIncognito = browserState->IsOffTheRecord();
    if (!_isIncognito) {
      _browserState = browserState;
      _identityManagerObserverBridge.reset(
          new signin::IdentityManagerObserverBridge(
              IdentityManagerFactory::GetForBrowserState(_browserState), self));
      _signinPromoViewMediator = [[SigninPromoViewMediator alloc]
          initWithAccountManagerService:ChromeAccountManagerServiceFactory::
                                            GetForBrowserState(_browserState)
                            authService:AuthenticationServiceFactory::
                                            GetForBrowserState(_browserState)
                            prefService:_browserState->GetPrefs()
                            accessPoint:signin_metrics::AccessPoint::
                                            ACCESS_POINT_BOOKMARK_MANAGER
                              presenter:presenter];
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

  _identityManagerObserverBridge.reset();
}

- (void)hidePromoCell {
  DCHECK(!_isIncognito);
  DCHECK(_browserState);
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
  if (_isIncognito)
    return;

  DCHECK(_browserState);
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForBrowserState(_browserState);
  if ([SigninPromoViewMediator
          shouldDisplaySigninPromoViewWithAccessPoint:
              signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER
                                authenticationService:authenticationService
                                          prefService:_browserState
                                                          ->GetPrefs()]) {
    signin::IdentityManager* identityManager =
        IdentityManagerFactory::GetForBrowserState(_browserState);
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
