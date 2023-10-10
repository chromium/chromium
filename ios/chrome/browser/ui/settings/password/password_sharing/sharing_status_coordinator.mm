// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/sharing_status_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/recipient_info.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/sharing_status_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/sharing_status_mediator.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/sharing_status_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/sharing_status_view_controller_presentation_delegate.h"

@interface SharingStatusCoordinator () <
    SharingStatusViewControllerPresentationDelegate>

// The navigation controller displaying the view controller.
// TODO(crbug.com/1463882): Remove.
@property(nonatomic, strong)
    TableViewNavigationController* navigationController;

// Main view controller for this coordinator.
@property(nonatomic, strong) SharingStatusViewController* viewController;

// Main mediator for this coordinator.
@property(nonatomic, strong) SharingStatusMediator* mediator;

@end

@implementation SharingStatusCoordinator {
  // Contains information about the recipients that the user selected to share a
  // password with.
  NSArray<RecipientInfoForIOSDisplay*>* _recipients;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                recipients:
                                    (NSArray<RecipientInfoForIOSDisplay*>*)
                                        recipients {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _recipients = recipients;
  }
  return self;
}

- (void)start {
  [super start];

  self.viewController = [[SharingStatusViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  self.viewController.delegate = self;

  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  self.mediator = [[SharingStatusMediator alloc]
        initWithAuthService:AuthenticationServiceFactory::GetForBrowserState(
                                browserState)
      accountManagerService:ChromeAccountManagerServiceFactory::
                                GetForBrowserState(browserState)
                 recipients:_recipients];
  self.mediator.consumer = self.viewController;

  self.navigationController =
      [[TableViewNavigationController alloc] initWithTable:self.viewController];
  [self.navigationController
      setModalPresentationStyle:UIModalPresentationPageSheet];
  self.navigationController.navigationBar.hidden = YES;

  UISheetPresentationController* sheetPresentationController =
      self.navigationController.sheetPresentationController;
  if (sheetPresentationController) {
    sheetPresentationController.detents =
        @[ [UISheetPresentationControllerDetent mediumDetent] ];
  }

  [self.baseViewController presentViewController:self.navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self.viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.navigationController = nil;
  self.viewController = nil;
}

#pragma mark - SharingStatusViewControllerPresentationDelegate

- (void)sharingStatusWasDismissed:(SharingStatusViewController*)controller {
  [self.delegate sharingStatusCoordinatorWasDismissed:self];
}

- (void)startPasswordSharing {
  [self.delegate startPasswordSharing];
}

@end
