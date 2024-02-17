// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/expanded_manual_fill_coordinator.h"

#import "ios/chrome/browser/ui/autofill/manual_fill/expanded_manual_fill_view_controller.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_constants.h"

using manual_fill::ManualFillDataType;

@interface ExpandedManualFillCoordinator ()

// Main view controller for this coordinator.
@property(nonatomic, strong)
    ExpandedManualFillViewController* expandedManualFillViewController;

@end

@implementation ExpandedManualFillCoordinator {
  // Initial data type to present in the expanded manual fill view.
  ManualFillDataType _initialDataType;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                               forDataType:(ManualFillDataType)dataType {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _initialDataType = dataType;
  }
  return self;
}

- (void)start {
  self.expandedManualFillViewController =
      [[ExpandedManualFillViewController alloc]
          initForDataType:_initialDataType];

  //  TODO(b/40942168): Show manual filling options.
}

- (void)stop {
  self.expandedManualFillViewController = nil;
}

- (UIViewController*)viewController {
  return self.expandedManualFillViewController;
}

@end
