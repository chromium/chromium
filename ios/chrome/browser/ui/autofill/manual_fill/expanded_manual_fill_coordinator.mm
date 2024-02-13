// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/expanded_manual_fill_coordinator.h"

#import "ios/chrome/browser/ui/autofill/manual_fill/expanded_manual_fill_view_controller.h"

@interface ExpandedManualFillCoordinator ()

// Main view controller for this coordinator.
@property(nonatomic, strong)
    ExpandedManualFillViewController* expandedManualFillViewController;

@end

@implementation ExpandedManualFillCoordinator

- (void)start {
  self.expandedManualFillViewController =
      [[ExpandedManualFillViewController alloc] init];
}

- (void)stop {
  self.expandedManualFillViewController = nil;
}

- (UIViewController*)viewController {
  return self.expandedManualFillViewController;
}

@end
