// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/follow/first_follow_coordinator.h"

#import "ios/chrome/browser/ui/follow/first_follow_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Sets a custom radius for the half sheet presentation.
constexpr CGFloat kHalfSheetCornerRadius = 20;

}  // namespace

@implementation FirstFollowCoordinator

#pragma mark - ChromeCoordinator

- (void)start {
  FirstFollowViewController* firstFollowViewController =
      [[FirstFollowViewController alloc] init];
  firstFollowViewController.webChannelTitle = self.webChannelTitle;

  if (@available(iOS 15, *)) {
    firstFollowViewController.modalPresentationStyle =
        UIModalPresentationPageSheet;
    UISheetPresentationController* presentationController =
        firstFollowViewController.sheetPresentationController;
    presentationController.prefersEdgeAttachedInCompactHeight = YES;
    presentationController.widthFollowsPreferredContentSizeWhenEdgeAttached =
        YES;
    presentationController.detents =
        @[ UISheetPresentationControllerDetent.mediumDetent ];
    presentationController.preferredCornerRadius = kHalfSheetCornerRadius;
  } else {
    firstFollowViewController.modalPresentationStyle =
        UIModalPresentationFormSheet;
  }

  [self.baseViewController presentViewController:firstFollowViewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  if (self.baseViewController.presentedViewController) {
    [self.baseViewController dismissViewControllerAnimated:NO completion:nil];
  }
}

@end