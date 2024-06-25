// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_browsing_data_coordinator.h"

#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_browsing_data_view_controller.h"

@implementation QuickDeleteBrowsingDataCoordinator {
  QuickDeleteBrowsingDataViewController* _viewController;
}

#pragma mark - ChromeCoordinator

- (void)start {
  _viewController = [[QuickDeleteBrowsingDataViewController alloc] init];

  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [_viewController dismissViewControllerAnimated:YES completion:nil];
  _viewController = nil;
}

@end
