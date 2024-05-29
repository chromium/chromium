// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/account_switching/account_switcher_view_controller.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {
constexpr CGFloat kHalfSheetCornerRadius = 20.0;
}  // namespace

@implementation AccountSwitcherViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
  [self setUpBottomSheetPresentationController];
}

#pragma mark - Private

// Sets up the sheet presentation controller and its properties when using
// UIModalPresentationPageSheet mode.
- (void)setUpBottomSheetPresentationController {
  UISheetPresentationController* presentationController =
      self.sheetPresentationController;
  if (!self.sheetPresentationController) {
    return;
  }
  presentationController.prefersEdgeAttachedInCompactHeight = YES;
  presentationController.widthFollowsPreferredContentSizeWhenEdgeAttached = YES;
  presentationController.preferredCornerRadius = kHalfSheetCornerRadius;
  presentationController.detents = @[
    [UISheetPresentationControllerDetent mediumDetent],
    [UISheetPresentationControllerDetent largeDetent]
  ];
}

@end
