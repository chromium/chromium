// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_coordinator.h"

#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/authentication_ui_util.h"
#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_utils.h"
#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_mediator.h"
#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_view_controller.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"

@interface HistorySyncCoordinator () <HistorySyncMediatorDelegate,
                                      PromoStyleViewControllerDelegate>
@end

@implementation HistorySyncCoordinator {
  // History mediator.
  HistorySyncMediator* _mediator;
  // History view controller.
  HistorySyncViewController* _viewController;
  // `YES` if coordinator used during the first run.
  BOOL _firstRun;
  // `YES` if the user's email should be shown in the footer text.
  BOOL _showUserEmail;
  // Delegate for the history sync coordinator.
  __weak id<HistorySyncCoordinatorDelegate> _delegate;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                        delegate:
                                            (id<HistorySyncCoordinatorDelegate>)
                                                delegate
                                        firstRun:(BOOL)firstRun
                                   showUserEmail:(BOOL)showUserEmail {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
    _firstRun = firstRun;
    _showUserEmail = showUserEmail;
    _delegate = delegate;
  }
  return self;
}

- (void)start {
  [super start];
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  CHECK_EQ(browserState, browserState->GetOriginalChromeBrowserState());
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForBrowserState(browserState);
  syncer::SyncService* syncService =
      SyncServiceFactory::GetForBrowserState(browserState);
  if (!CanHistorySyncOptInBePresented(authenticationService, syncService)) {
    [_delegate closeHistorySyncCoordinator:self declinedByUser:NO];
    return;
  }

  _viewController = [[HistorySyncViewController alloc] init];
  _viewController.delegate = self;
  ChromeAccountManagerService* chromeAccountManagerService =
      ChromeAccountManagerServiceFactory::GetForBrowserState(browserState);
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForBrowserState(browserState);
  _mediator = [[HistorySyncMediator alloc]
      initWithAuthenticationService:authenticationService
        chromeAccountManagerService:chromeAccountManagerService
                    identityManager:identityManager
                        syncService:syncService
                      showUserEmail:_showUserEmail];
  _mediator.consumer = _viewController;
  _mediator.delegate = self;
  if (_firstRun) {
    _viewController.modalInPresentation = YES;
  }

  BOOL animated = self.baseNavigationController.topViewController != nil;
  [self.baseNavigationController setViewControllers:@[ _viewController ]
                                           animated:animated];
}

- (void)stop {
  [super stop];
  _delegate = nil;
  [_mediator disconnect];
  _mediator.consumer = nil;
  _mediator.delegate = nil;
  _mediator = nil;
  _viewController.delegate = nil;
  _viewController = nil;
}

- (void)dealloc {
  DUMP_WILL_BE_CHECK(!_viewController);
}

#pragma mark - HistorySyncMediatorDelegate

- (void)historySyncMediatorPrimaryAccountCleared:
    (HistorySyncMediator*)mediator {
  [_delegate closeHistorySyncCoordinator:self declinedByUser:NO];
}

#pragma mark - PromoStyleViewControllerDelegate

- (void)didTapPrimaryActionButton {
  [_mediator enableHistorySyncOptin];
  [_delegate closeHistorySyncCoordinator:self declinedByUser:NO];
}

- (void)didTapSecondaryActionButton {
  [_delegate closeHistorySyncCoordinator:self declinedByUser:YES];
}

@end
