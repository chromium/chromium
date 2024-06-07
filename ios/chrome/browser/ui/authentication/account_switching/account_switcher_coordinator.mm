// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/account_switching/account_switcher_coordinator.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/authentication/account_switching/account_switcher_view_controller.h"

@implementation AccountSwitcherCoordinator {
  AccountSwitcherViewController* _viewController;
}

- (void)start {
  [super start];
  _viewController = [[AccountSwitcherViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  UINavigationController* navController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];

  UIUserInterfaceIdiom idiom = [[UIDevice currentDevice] userInterfaceIdiom];
  if (idiom == UIUserInterfaceIdiomPad) {
    navController.modalPresentationStyle = UIModalPresentationPopover;
    navController.popoverPresentationController.sourceView = self.anchorView;
    navController.popoverPresentationController.permittedArrowDirections =
        UIPopoverArrowDirectionUp;

  } else {
    navController.modalPresentationStyle = UIModalPresentationPageSheet;
    UIBarButtonItem* closeButton = [[UIBarButtonItem alloc]
        initWithBarButtonSystemItem:UIBarButtonSystemItemClose
                             target:self
                             action:@selector(didTapClose)];
    _viewController.navigationItem.rightBarButtonItem = closeButton;
  }

  [self.baseViewController presentViewController:navController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  DCHECK(_viewController);
  [_viewController dismissViewControllerAnimated:YES completion:nil];
  _viewController = nil;
  [super stop];
}

#pragma mark - Private

- (void)didTapClose {
  [self stop];
}

@end
