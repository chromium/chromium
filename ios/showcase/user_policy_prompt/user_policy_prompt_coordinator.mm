// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/user_policy_prompt/user_policy_prompt_coordinator.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/policy/user_policy/user_policy_prompt_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
constexpr CGFloat kHalfSheetCornerRadius = 20;
}  // namespace

@interface SCUserPolicyPromptCoordinator ()

@property(nonatomic, strong) UserPolicyPromptViewController* viewController;

@end

@implementation SCUserPolicyPromptCoordinator

@synthesize baseViewController = _baseViewController;

- (void)start {
  self.viewController = [[UserPolicyPromptViewController alloc]
      initWithManagedDomain:@"showcase.com"];
  self.viewController.actionHandler = nil;
  self.viewController.presentationController.delegate = nil;

  if (@available(iOS 15, *)) {
    self.viewController.modalPresentationStyle = UIModalPresentationPageSheet;
    UISheetPresentationController* presentationController =
        self.viewController.sheetPresentationController;
    presentationController.prefersEdgeAttachedInCompactHeight = YES;
    presentationController.detents = @[
      UISheetPresentationControllerDetent.mediumDetent,
      UISheetPresentationControllerDetent.largeDetent
    ];
    presentationController.preferredCornerRadius = kHalfSheetCornerRadius;
  } else {
    self.viewController.modalPresentationStyle = UIModalPresentationFormSheet;
  }

  [self.baseViewController presentViewController:self.viewController
                                        animated:YES
                                      completion:nil];
}

@end
