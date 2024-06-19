// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_view_controller.h"

#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {
constexpr CGFloat kHalfSheetCornerRadius = 20.0;
}  // namespace

@implementation AccountMenuViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier = kAccountMenuTableViewId;
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
