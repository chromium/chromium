// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_main_coordinator.h"

@interface PrivacyGuideMainCoordinator () <
    UIAdaptivePresentationControllerDelegate>
@end

@implementation PrivacyGuideMainCoordinator {
  UINavigationController* _navigationController;
}

- (void)start {
  // TODO(crbug.com/1494887): Start the Welcome step before presenting the
  // UINavigationController.

  _navigationController =
      [[UINavigationController alloc] initWithNavigationBarClass:nil
                                                    toolbarClass:nil];
  _navigationController.modalPresentationStyle = UIModalPresentationFormSheet;
  _navigationController.presentationController.delegate = self;
  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [_navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  _navigationController.presentationController.delegate = nil;
  _navigationController = nil;
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self.delegate privacyGuideMainCoordinatorDidRemove:self];
}

@end
