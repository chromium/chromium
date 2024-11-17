// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/docking_promo/ui/docking_promo_view_controller.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/instruction_view/instruction_view.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// The name of the animation used for the Docking Promo.
NSString* const kAnimationName = @"docking_promo";

// The name of the animation used for the Docking Promo in dark mode.
NSString* const kAnimationNameDarkMode = @"docking_promo_darkmode";

// The accessibility identifier for the Docking Promo view.
NSString* const kDockingPromoAccessibilityId = @"kDockingPromoAccessibilityId";

// The keypath for the "Edit Home Screen" animation text in the
// `animationTextProvider` dictionary.
NSString* const kEditHomeScreenKeypath = @"edit_home_screen";

}  // namespace

@implementation DockingPromoViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.animationName = kAnimationName;
  self.animationNameDarkMode = kAnimationNameDarkMode;
  self.titleString = l10n_util::GetNSString(IDS_IOS_DOCKING_PROMO_TITLE);
  self.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_DOCKING_PROMO_PRIMARY_BUTTON_TITLE);
  self.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_DOCKING_PROMO_SECONDARY_BUTTON_TITLE);

  // Set the text localization.
  NSString* editHomeScreenTitle = l10n_util::GetNSString(
      IDS_IOS_DOCKING_EDIT_HOME_SCREEN_LOTTIE_INSTRUCTION);
  self.animationTextProvider = @{kEditHomeScreenKeypath : editHomeScreenTitle};

  // Add the Docking Promo instructional steps.
  NSArray* dockingPromoSteps = @[
    l10n_util::GetNSString(IDS_IOS_DOCKING_PROMO_FIRST_INSTRUCTION),
    l10n_util::GetNSString(IDS_IOS_DOCKING_PROMO_SECOND_INSTRUCTION),
  ];

  UIView* instructionView =
      [[InstructionView alloc] initWithList:dockingPromoSteps];

  instructionView.translatesAutoresizingMaskIntoConstraints = NO;

  self.underTitleView = instructionView;

  [super viewDidLoad];

  self.view.accessibilityIdentifier = kDockingPromoAccessibilityId;
}

@end
