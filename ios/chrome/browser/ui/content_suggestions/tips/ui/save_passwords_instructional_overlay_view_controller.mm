// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/tips/ui/save_passwords_instructional_overlay_view_controller.h"

#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Accessibility identifier for the Save Passwords instructions view.
NSString* const kSavePasswordsInstructionalOverlayAXID =
    @"kSavePasswordsInstructionalOverlayAXID";

}  // namespace

@implementation SavePasswordsInstructionalOverlayViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.titleString =
      l10n_util::GetNSString(IDS_IOS_MAGIC_STACK_TIP_TUTORIAL_TITLE);
  self.steps = @[
    l10n_util::GetNSString(
        IDS_IOS_MAGIC_STACK_TIP_SAVE_PASSWORDS_TUTORIAL_STEP_1),
    l10n_util::GetNSString(
        IDS_IOS_MAGIC_STACK_TIP_SAVE_PASSWORDS_TUTORIAL_STEP_2),
    l10n_util::GetNSString(
        IDS_IOS_MAGIC_STACK_TIP_SAVE_PASSWORDS_TUTORIAL_STEP_1),
  ];

  [super viewDidLoad];

  self.view.accessibilityIdentifier = kSavePasswordsInstructionalOverlayAXID;
}

#pragma mark - BottomSheetViewController

- (void)expandBottomSheet {
  UISheetPresentationController* presentationController =
      self.sheetPresentationController;
  // Expand to medium detent.
  [presentationController animateChanges:^{
    presentationController.selectedDetentIdentifier =
        UISheetPresentationControllerDetentIdentifierMedium;
  }];
}

- (void)setUpBottomSheetPresentationController {
  UISheetPresentationController* presentationController =
      self.sheetPresentationController;
  presentationController.prefersEdgeAttachedInCompactHeight = YES;
  presentationController.widthFollowsPreferredContentSizeWhenEdgeAttached = YES;
}

- (void)setUpBottomSheetDetents {
  UISheetPresentationController* presentationController =
      self.sheetPresentationController;
  presentationController.detents = @[
    [UISheetPresentationControllerDetent mediumDetent],
    [UISheetPresentationControllerDetent largeDetent]
  ];
}

@end
