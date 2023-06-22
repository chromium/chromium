// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/bottom_sheet/payments_suggestion_bottom_sheet_coordinator.h"

#import "ios/chrome/browser/ui/autofill/bottom_sheet/payments_suggestion_bottom_sheet_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PaymentsSuggestionBottomSheetCoordinator ()

// This view controller is used to display the bottom sheet.
@property(nonatomic, strong)
    PaymentsSuggestionBottomSheetViewController* viewController;

@end

@implementation PaymentsSuggestionBottomSheetCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                    params:(const autofill::FormActivityParams&)
                                               params {
  self = [super initWithBaseViewController:viewController browser:browser];
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  self.viewController =
      [[PaymentsSuggestionBottomSheetViewController alloc] init];
  [self.baseViewController presentViewController:self.viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [super stop];
  [self.viewController dismissViewControllerAnimated:NO completion:nil];
  self.viewController = nil;
}

@end
