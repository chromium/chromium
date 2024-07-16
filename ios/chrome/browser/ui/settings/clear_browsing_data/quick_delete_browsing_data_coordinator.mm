// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_browsing_data_coordinator.h"

#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_browsing_data_delegate.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_browsing_data_view_controller.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_browsing_data_view_controller_delegate.h"

@interface QuickDeleteBrowsingDataCoordinator () <
    QuickDeleteBrowsingDataViewControllerDelegate,
    UIAdaptivePresentationControllerDelegate>
@end

@implementation QuickDeleteBrowsingDataCoordinator {
  QuickDeleteBrowsingDataViewController* _viewController;
  UINavigationController* _navigationController;
}

#pragma mark - ChromeCoordinator

- (void)start {
  _viewController = [[QuickDeleteBrowsingDataViewController alloc] init];
  _viewController.delegate = self;
  _navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];
  _navigationController.modalPresentationStyle = UIModalPresentationFormSheet;
  _navigationController.presentationController.delegate = self;

  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [_navigationController dismissViewControllerAnimated:YES completion:nil];
  _viewController.delegate = nil;
  _viewController = nil;
  _navigationController.presentationController.delegate = nil;
  _navigationController = nil;
}

#pragma mark - QuickDeleteBrowsingDataViewControllerDelegate

- (void)dismissBrowsingDataPage {
  [self.delegate stopBrowsingDataPage];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self dismissBrowsingDataPage];
}

@end
