// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/password_picker_coordinator.h"

#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_picker_mediator.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_picker_view_controller.h"

@interface PasswordPickerCoordinator () {
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

- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                   credentials:
                       (const std::vector<password_manager::CredentialUIEntry>&)
                           credentials {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _credentials = credentials;
  }
  return self;
}

- (void)start {
  [super start];

  self.viewController = [[PasswordPickerViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  self.mediator =
      [[PasswordPickerMediator alloc] initWithCredentials:_credentials];
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
  self.mediator = nil;
}

@end
