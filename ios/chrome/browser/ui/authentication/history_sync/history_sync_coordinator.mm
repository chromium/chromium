// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_mediator.h"
#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_view_controller.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface HistorySyncCoordinator () <PromoStyleViewControllerDelegate,
                                      HistorySyncMediatorDelegate>
@end

@implementation HistorySyncCoordinator {
  // History mediator.
  HistorySyncMediator* _mediator;
  // History view controller.
  HistorySyncViewController* _viewController;
  // `YES` if coordinator used during the first run.
  BOOL _firstRun;
  // Delegate for the history sync coordinator
  __weak id<HistorySyncCoordinatorDelegate> _delegate;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                        delegate:
                                            (id<HistorySyncCoordinatorDelegate>)
                                                delegate
                                        firstRun:(BOOL)firstRun {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
    _firstRun = firstRun;
    _delegate = delegate;
  }
  return self;
}

- (void)start {
  [super start];
  _viewController = [[HistorySyncViewController alloc] init];
  _viewController.delegate = self;
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForBrowserState(browserState);
  ChromeAccountManagerService* chromeAccountManagerService =
      ChromeAccountManagerServiceFactory::GetForBrowserState(browserState);
  _mediator = [[HistorySyncMediator alloc]
      initWithAuthenticationService:authenticationService
        chromeAccountManagerService:chromeAccountManagerService
                    identityManager:IdentityManagerFactory::GetForBrowserState(
                                        browserState)
                           consumer:_viewController
                           delegate:self];
  if (_firstRun) {
    _viewController.modalInPresentation = YES;
  }
  BOOL animated = self.baseNavigationController.topViewController != nil;
  [self.baseNavigationController setViewControllers:@[ _viewController ]
                                           animated:animated];
}

- (void)stop {
  [super stop];
  [_mediator disconnect];
  _delegate = nil;
  _mediator = nil;
  _viewController = nil;
}

- (void)dealloc {
  DUMP_WILL_BE_CHECK(!_viewController);
}

#pragma mark - HistorySyncMediatorDelegate

- (void)historySyncMediatorPrimaryAccountCleared:
    (HistorySyncMediator*)mediator {
  [_delegate closeHistorySyncCoordinator:self];
}

#pragma mark - HistorySyncViewControllerDelegate

- (void)didTapPrimaryActionButton {
  [_delegate closeHistorySyncCoordinator:self];
}

- (void)didTapSecondaryActionButton {
  [_delegate closeHistorySyncCoordinator:self];
}

@end
