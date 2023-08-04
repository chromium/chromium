// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_popup_coordinator.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/authentication_ui_util.h"
#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_coordinator.h"

@interface HistorySyncPopupCoordinator () <
    HistorySyncCoordinatorDelegate,
    UIAdaptivePresentationControllerDelegate>
@end

@implementation HistorySyncPopupCoordinator {
  // Coordinator to display the tangible sync view.
  HistorySyncCoordinator* _historySyncCoordinator;
  // Navigation controller created for the popup.
  UINavigationController* _navigationController;
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
                              firstRun:NO];
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
  [_historySyncCoordinator stop];
  _historySyncCoordinator = nil;
  _navigationController.presentationController.delegate = nil;
  _navigationController = nil;
  [self viewWasDismissedWithDeclinedByUser:YES];
}

@end
