// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/feed_top_section/feed_top_section_coordinator.h"

#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/ui/authentication/signin_presenter.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view_mediator.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/ntp/feed_top_section/feed_top_section_mediator.h"
#import "ios/chrome/browser/ui/ntp/feed_top_section/feed_top_section_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FeedTopSectionCoordinator () <SigninPresenter>

@property(nonatomic, strong) FeedTopSectionMediator* feedTopSectionMediator;

@end

@implementation FeedTopSectionCoordinator

@synthesize viewController = _viewController;

- (void)start {
  DCHECK(self.ntpDelegate);
  FeedTopSectionViewController* feedTopSectionViewController =
      [[FeedTopSectionViewController alloc] init];
  _viewController = feedTopSectionViewController;
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  FeedTopSectionMediator* feedTopSectionMediator =
      [[FeedTopSectionMediator alloc]
          initWithConsumer:feedTopSectionViewController
              browserState:browserState];
  SigninPromoViewMediator* signinPromoViewMediator =
      [[SigninPromoViewMediator alloc]
          initWithAccountManagerService:ChromeAccountManagerServiceFactory::
                                            GetForBrowserState(browserState)
                            authService:AuthenticationServiceFactory::
                                            GetForBrowserState(browserState)
                            prefService:browserState->GetPrefs()
                            accessPoint:signin_metrics::AccessPoint::
                                            ACCESS_POINT_NTP_FEED_TOP_PROMO
                              presenter:self];
  signinPromoViewMediator.consumer = feedTopSectionMediator;
  feedTopSectionMediator.signinPromoMediator = signinPromoViewMediator;
  feedTopSectionMediator.ntpDelegate = self.ntpDelegate;
  feedTopSectionViewController.signinPromoDelegate = signinPromoViewMediator;
  feedTopSectionViewController.delegate = feedTopSectionMediator;
  feedTopSectionViewController.ntpDelegate = self.ntpDelegate;
  self.feedTopSectionMediator = feedTopSectionMediator;
  [feedTopSectionMediator setUp];
}

- (void)stop {
  _viewController = nil;
  self.feedTopSectionMediator = nil;
}

#pragma mark - SigninPresenter

- (void)showSignin:(ShowSigninCommand*)command {
  id<ApplicationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  [handler showSignin:command baseViewController:self.baseViewController];
}

@end
