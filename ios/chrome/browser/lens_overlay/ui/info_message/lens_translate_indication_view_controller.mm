// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/ui/info_message/lens_translate_indication_view_controller.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/font/font_api.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The lateral padding for the translate error view.
const CGFloat kLateralPadding = 22.0;

// Top padding for the view content.
const CGFloat kViewTopPadding = 36.0;

// The height of the translate error message.
const CGFloat kPreferredContentHeight = 80.0;

// The font size of the title.
const CGFloat kTitleFontSize = 18;

}  // namespace

@implementation LensTranslateIndicationViewController

#pragma mark - UIViewController

- (CGSize)preferredContentSize {
  CGFloat fittingWidth = self.view.safeAreaLayoutGuide.layoutFrame.size.width;
  return CGSizeMake(fittingWidth, kPreferredContentHeight);
}

- (void)loadView {
  [super loadView];
  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];

  UILabel* label = [[UILabel alloc] init];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.adjustsFontForContentSizeCategory = YES;
  label.lineBreakMode = NSLineBreakByWordWrapping;
  label.numberOfLines = 0;
  label.font = ios::provider::GetBrandedProductRegularFont(kTitleFontSize);
  label.text =
      l10n_util::GetNSString(IDS_IOS_LENS_OVERLAY_TRANSLATE_INDICATION_TITLE);
  label.textColor = [UIColor colorNamed:kTextPrimaryColor];

  [self.view addSubview:label];

  LayoutSides sides =
      LayoutSides::kTop | LayoutSides::kTrailing | LayoutSides::kLeading;
  NSDirectionalEdgeInsets insets = NSDirectionalEdgeInsetsMake(
      kViewTopPadding, kLateralPadding, 0, kLateralPadding);
  AddSameConstraintsToSidesWithInsets(label, self.view, sides, insets);
}

@end
