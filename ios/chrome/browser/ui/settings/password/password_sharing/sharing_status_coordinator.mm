// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/sharing_status_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
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
    SharingStatusViewControllerPresentationDelegate,
    UIAdaptivePresentationControllerDelegate>

// Main view controller for this coordinator.
@property(nonatomic, strong) SharingStatusViewController* viewController;

// Main mediator for this coordinator.
@property(nonatomic, strong) SharingStatusMediator* mediator;

@end

@implementation SharingStatusCoordinator {
  // Contains information about the recipients that the user selected to share a
  // password with.
  NSArray<RecipientInfoForIOSDisplay*>* _recipients;

  // Website for which the password is being shared.
  NSString* _website;
}

- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                    recipients:(NSArray<RecipientInfoForIOSDisplay*>*)recipients
                       website:(NSString*)website {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _recipients = recipients;
    _website = website;
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
                 recipients:_recipients
                    website:_website];
  self.mediator.consumer = self.viewController;

  self.viewController.sheetPresentationController.detents =
      @[ [UISheetPresentationControllerDetent mediumDetent] ];
  self.viewController.presentationController.delegate = self;

  [self.baseViewController presentViewController:self.viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self.viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.viewController = nil;
  self.mediator = nil;
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self.delegate sharingStatusCoordinatorWasDismissed:self];
}

#pragma mark - SharingStatusViewControllerPresentationDelegate

- (void)sharingStatusWasDismissed:(SharingStatusViewController*)controller {
  [self.delegate sharingStatusCoordinatorWasDismissed:self];
}

- (void)startPasswordSharing {
  [self.delegate startPasswordSharing];
}

@end
