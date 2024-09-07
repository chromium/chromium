// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tips_notifications/ui/lens_promo_instructions_view_controller.h"

#import "ios/chrome/browser/shared/ui/elements/instruction_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// AccessibilityIdentifier for the Lens Promo Instructions view.
NSString* const kLensPromoInstructionsAXID = @"kLensPromoInstructionsAXID";

}  // namespace

@implementation LensPromoInstructionsViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.topAlignedLayout = YES;
  self.scrollEnabled = NO;
  self.dismissBarButtonSystemItem = UIBarButtonSystemItemClose;
  self.titleView = [self createTitleLabel];
  self.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_LENS_PROMO_PRIMARY_ACTION);
  self.underTitleView = [self createInstructionView];
  [super viewDidLoad];

  self.view.accessibilityIdentifier = kLensPromoInstructionsAXID;
  [self setUpBottomSheetDetents];
  [self expandBottomSheet];
}

#pragma mark - Private

// Returns a label to use as the title of the instructions view.
- (UILabel*)createTitleLabel {
  UILabel* label = [[UILabel alloc] init];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.numberOfLines = 0;
  label.text = l10n_util::GetNSString(IDS_IOS_LENS_PROMO_INSTRUCTIONS_TITLE);
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  label.adjustsFontForContentSizeCategory = YES;
  label.textAlignment = NSTextAlignmentCenter;
  label.accessibilityTraits |= UIAccessibilityTraitHeader;

  return label;
}

// Returns a view containing instructions to use Lens.
- (InstructionView*)createInstructionView {
  NSArray* steps = @[
    l10n_util::GetNSString(IDS_IOS_LENS_PROMO_INSTRUCTIONS_STEP1),
    l10n_util::GetNSString(IDS_IOS_LENS_PROMO_INSTRUCTIONS_STEP2),
    l10n_util::GetNSString(IDS_IOS_LENS_PROMO_INSTRUCTIONS_STEP3),
  ];
  InstructionView* view = [[InstructionView alloc] initWithList:steps];
  view.translatesAutoresizingMaskIntoConstraints = NO;
  return view;
}

@end
