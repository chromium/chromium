// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/coordinator/age_mismatch_signout_coordinator.h"

#import "ios/chrome/browser/signin/ui/age_mismatch_signout_view_controller.h"

@implementation AgeMismatchSignoutCoordinator {
  // View controller for the Age Mismatch Prompt.
  AgeMismatchSignoutViewController* _viewController;
}

- (void)start {
  [super start];
  _viewController = [[AgeMismatchSignoutViewController alloc] init];
  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [super stop];
  [_viewController.presentingViewController dismissViewControllerAnimated:YES
                                                               completion:nil];
  _viewController = nil;
}

@end
