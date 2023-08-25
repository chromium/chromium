// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_popup_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/metrics/user_metrics.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_coordinator.h"

@interface HistorySyncPopupCoordinator () <
    HistorySyncCoordinatorDelegate,
    UIAdaptivePresentationControllerDelegate>
@end

@implementation HistorySyncPopupCoordinator {
  // Authentication service.
  AuthenticationService* _authenticationService;
  // Coordinator to display the tangible sync view.
  HistorySyncCoordinator* _historySyncCoordinator;
  // Navigation controller created for the popup.
  UINavigationController* _navigationController;
  // `YES` if the user has selected an account to sign-in especifically to be
  // able to enabled history sync (eg. using recent tabs history sync promo).
  BOOL _dedicatedSignInDone;
  // Access point associated with the history opt-in screen.
  signin_metrics::AccessPoint _accessPoint;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                       dedicatedSignInDone:(BOOL)dedicatedSignInDone
                               accessPoint:
                                   (signin_metrics::AccessPoint)accessPoint {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _dedicatedSignInDone = dedicatedSignInDone;
    _accessPoint = accessPoint;
  }
  return self;
}

- (void)start {
  [super start];
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  CHECK_EQ(browserState, browserState->GetOriginalChromeBrowserState());
  _authenticationService =
      AuthenticationServiceFactory::GetForBrowserState(browserState);
  syncer::SyncService* syncService =
      SyncServiceFactory::GetForBrowserState(browserState);
  // Check if History Sync Opt-In should be skipped.
  HistorySyncSkipReason skipReason = [HistorySyncCoordinator
      getHistorySyncOptInSkipReason:syncService
              authenticationService:_authenticationService];
  if (skipReason != HistorySyncSkipReason::kNone) {
    [HistorySyncCoordinator recordHistorySyncSkipMetric:skipReason
                                            accessPoint:_accessPoint];
    [self.delegate historySyncPopupCoordinator:self
                    didCloseWithDeclinedByUser:NO];
    return;
  }

  _navigationController =
      [[UINavigationController alloc] initWithNavigationBarClass:nil
                                                    toolbarClass:nil];
  _navigationController.modalPresentationStyle = UIModalPresentationFormSheet;
  _navigationController.presentationController.delegate = self;
  _historySyncCoordinator = [[HistorySyncCoordinator alloc]
      initWithBaseNavigationController:_navigationController
                               browser:self.browser
                              delegate:self
                              firstRun:NO
                         showUserEmail:!_dedicatedSignInDone
                           accessPoint:_accessPoint];
  [_historySyncCoordinator start];
  [_navigationController setNavigationBarHidden:YES animated:NO];
  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [_historySyncCoordinator stop];
  _historySyncCoordinator = nil;
  _navigationController.presentationController.delegate = nil;
  [_navigationController dismissViewControllerAnimated:NO completion:nil];
  _navigationController = nil;
  [super stop];
}

- (void)dealloc {
  // TODO(crbug.com/1454777)
  DUMP_WILL_BE_CHECK(!_historySyncCoordinator);
}

#pragma mark - Private

- (void)viewWasDismissedWithDeclinedByUser:(BOOL)declined {
  _navigationController.presentationController.delegate = nil;
  _navigationController = nil;

  if (declined && _dedicatedSignInDone) {
    _authenticationService->SignOut(
        signin_metrics::ProfileSignout::
            kUserDeclinedHistorySyncAfterDedicatedSignIn,
        /*force_clear_browsing_data=*/false, nil);
  }
  [self.delegate historySyncPopupCoordinator:self
                  didCloseWithDeclinedByUser:declined];
}

#pragma mark - HistorySyncCoordinatorDelegate

- (void)closeHistorySyncCoordinator:
            (HistorySyncCoordinator*)historySyncCoordinator
                     declinedByUser:(BOOL)declined {
  CHECK(_navigationController);
  [_historySyncCoordinator stop];
  _historySyncCoordinator = nil;
  __weak __typeof(self) weakSelf = self;
  [_navigationController
      dismissViewControllerAnimated:YES
                         completion:^() {
                           [weakSelf
                               viewWasDismissedWithDeclinedByUser:declined];
                         }];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  // This should be triggered only if user dismisses the screen manually.
  base::RecordAction(base::UserMetricsAction("Signin_HistorySync_SwipedDown"));
  [_historySyncCoordinator stop];
  _historySyncCoordinator = nil;
  _navigationController.presentationController.delegate = nil;
  _navigationController = nil;
  [self viewWasDismissedWithDeclinedByUser:YES];
}

@end
