// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/sharing_status_coordinator.h"

#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/sharing_status_coordinator_delegate.h"
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

@end

@implementation SharingStatusCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:viewController browser:browser];
  return self;
}

- (void)start {
  [super start];

  self.viewController = [[SharingStatusViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  self.viewController.delegate = self;

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
