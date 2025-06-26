// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/coordinator/reader_mode_chip_coordinator.h"

#import "ios/chrome/browser/reader_mode/ui/reader_mode_chip_view_controller.h"

@implementation ReaderModeChipCoordinator

- (void)start {
  _viewController = [[ReaderModeChipViewController alloc] init];
  [self.baseViewController addChildViewController:_viewController];
  [self.viewController didMoveToParentViewController:_viewController];
}

- (void)stop {
  [_viewController willMoveToParentViewController:nil];
  [_viewController removeFromParentViewController];
  _viewController = nil;
}

@end
