// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/password_picker_coordinator.h"

#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_picker_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_picker_mediator.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_picker_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_picker_view_controller_presentation_delegate.h"

@interface PasswordPickerCoordinator () <
    PasswordPickerViewControllerPresentationDelegate> {
  std::vector<password_manager::CredentialUIEntry> _credentials;
}

// The navigation controller displaying the view controller.
@property(nonatomic, strong)
    TableViewNavigationController* navigationController;

// Main view controller for this coordinator.
@property(nonatomic, strong) PasswordPickerViewController* viewController;

// Main mediator for this coordinator.
@property(nonatomic, strong) PasswordPickerMediator* mediator;

@end

@implementation PasswordPickerCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)
    initWithBaseNavigationController:
        (UINavigationController*)navigationController
                             browser:(Browser*)browser
                         credentials:(const std::vector<
                                         password_manager::CredentialUIEntry>&)
                                         credentials {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
    _credentials = credentials;
  }
  return self;
}

- (void)start {
  [super start];

  self.viewController = [[PasswordPickerViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  self.viewController.delegate = self;
  self.mediator = [[PasswordPickerMediator alloc]
      initWithCredentials:_credentials
            faviconLoader:IOSChromeFaviconLoaderFactory::GetForBrowserState(
                              self.browser->GetBrowserState())];
  self.viewController.imageDataSource = self.mediator;
  self.mediator.consumer = self.viewController;
  self.navigationController =
      [[TableViewNavigationController alloc] initWithTable:self.viewController];
  [self.navigationController
      setModalPresentationStyle:UIModalPresentationFormSheet];
  self.navigationController.navigationBar.prefersLargeTitles = NO;

  UISheetPresentationController* sheetPresentationController =
      self.navigationController.sheetPresentationController;
  if (sheetPresentationController) {
    sheetPresentationController.detents = @[
      [UISheetPresentationControllerDetent mediumDetent],
      [UISheetPresentationControllerDetent largeDetent]
    ];
  }

  // Disable animation so that it looks as if the loaded password data replaces
  // the spinner view displayed from the parent coordinator.
  [self.baseNavigationController pushViewController:self.viewController
                                           animated:NO];
}

- (void)stop {
  self.navigationController = nil;
  self.viewController = nil;
  self.mediator = nil;
}

#pragma mark - PasswordPickerViewControllerPresentationDelegate

- (void)passwordPickerWasDismissed:(PasswordPickerViewController*)controller {
  [self.baseNavigationController popViewControllerAnimated:NO];
  [self.delegate passwordPickerCoordinatorWasDismissed:self];
}

- (void)passwordPickerClosed:(PasswordPickerViewController*)controller
      withSelectedCredential:
          (const password_manager::CredentialUIEntry&)credential {
  [self.delegate passwordPickerCoordinator:self didSelectCredential:credential];
}

@end
