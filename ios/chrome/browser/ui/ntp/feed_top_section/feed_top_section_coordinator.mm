// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/feed_top_section/feed_top_section_coordinator.h"

#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/signin_presenter.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view_mediator.h"
#import "ios/chrome/browser/ui/ntp/feed_top_section/feed_top_section_mediator.h"
#import "ios/chrome/browser/ui/ntp/feed_top_section/feed_top_section_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FeedTopSectionCoordinator () <SigninPresenter>

@property(nonatomic, strong) FeedTopSectionMediator* feedTopSectionMediator;
@property(nonatomic, strong)
    FeedTopSectionViewController* feedTopSectionViewController;
@property(nonatomic, strong) SigninPromoViewMediator* signinPromoMediator;

// Returns |YES| if the promo is visible in the NTP at the current scroll point.
@property(nonatomic, assign) BOOL isPromoVisible;

@end

@implementation FeedTopSectionCoordinator

// Synthesized from ChromeCoordinator.
@synthesize viewController = _viewController;

- (void)start {
  DCHECK(self.ntpDelegate);
  self.feedTopSectionViewController =
      [[FeedTopSectionViewController alloc] init];
  _viewController = self.feedTopSectionViewController;

  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForBrowserState(browserState);
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForBrowserState(browserState);
  syncer::SyncService* syncService =
      SyncServiceFactory::GetForBrowserState(browserState);
  self.feedTopSectionMediator = [[FeedTopSectionMediator alloc]
      initWithConsumer:self.feedTopSectionViewController
       identityManager:identityManager
           authService:authenticationService
           isIncognito:browserState->IsOffTheRecord()
           prefService:browserState->GetPrefs()];

  self.signinPromoMediator = [[SigninPromoViewMediator alloc]
            initWithBrowser:self.browser
      accountManagerService:ChromeAccountManagerServiceFactory::
                                GetForBrowserState(browserState)
                authService:AuthenticationServiceFactory::GetForBrowserState(
                                browserState)
                prefService:browserState->GetPrefs()
                syncService:syncService
                accessPoint:signin_metrics::AccessPoint::
                                ACCESS_POINT_NTP_FEED_TOP_PROMO
                  presenter:self
         baseViewController:self.feedTopSectionViewController];
  self.signinPromoMediator.consumer = self.feedTopSectionMediator;
  self.feedTopSectionMediator.signinPromoMediator = self.signinPromoMediator;
  self.feedTopSectionMediator.ntpDelegate = self.ntpDelegate;
  self.feedTopSectionViewController.signinPromoDelegate =
      self.signinPromoMediator;
  self.feedTopSectionViewController.delegate = self.feedTopSectionMediator;
  self.feedTopSectionViewController.ntpDelegate = self.ntpDelegate;
  [self.feedTopSectionMediator setUp];
}

- (void)stop {
  _viewController = nil;
  [self.feedTopSectionMediator shutdown];
  [self.signinPromoMediator disconnect];
  self.signinPromoMediator.consumer = nil;
  self.signinPromoMediator = nil;
  self.feedTopSectionMediator = nil;
  self.feedTopSectionViewController = nil;
}

#pragma mark - Public

- (void)signinPromoHasChangedVisibility:(BOOL)visible {
  if (self.isPromoVisible == visible ||
      !self.feedTopSectionViewController.shouldShowSigninPromo) {
    return;
  }
  if (visible) {
    [self.signinPromoMediator signinPromoViewIsVisible];
    self.isPromoVisible = visible;
  } else {
    [self.signinPromoMediator signinPromoViewIsHidden];
    self.isPromoVisible = visible;
  }
}

#pragma mark - SigninPresenter

- (void)showSignin:(ShowSigninCommand*)command {
  id<ApplicationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  [handler showSignin:command baseViewController:self.baseViewController];
}

@end
