// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_ui_util.h"

#import "base/i18n/rtl.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/common/ui/util/device_util.h"
#import "ios/chrome/common/ui/util/sdk_forward_declares.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// The margin between the text and the arrow on the "More" button.
constexpr CGFloat kMoreArrowMargin = 4;

}  // namespace

UIFont* GetTitleFontWithTraitCollection(UITraitCollection* trait_collection) {
  BOOL dynamic_type_enabled = UIContentSizeCategoryIsAccessibilityCategory(
      trait_collection.preferredContentSizeCategory);

  UIFontTextStyle text_style = nil;
  if (!dynamic_type_enabled && IsRegularXRegularSizeClass(trait_collection)) {
    text_style = UIFontTextStyleTitle1;
  } else if (!dynamic_type_enabled && !IsSmallDevice()) {
    text_style = UIFontTextStyleLargeTitle;
  } else {
    text_style = UIFontTextStyleTitle2;
  }

  DCHECK(text_style);
  UIFontDescriptor* descriptor =
      [UIFontDescriptor preferredFontDescriptorWithTextStyle:text_style];
  UIFont* font = [UIFont systemFontOfSize:descriptor.pointSize
                                   weight:UIFontWeightBold];
  UIFontMetrics* font_metrics = [UIFontMetrics metricsForTextStyle:text_style];
  return [font_metrics scaledFontForFont:font];
}

UIButton* CreateDisabledPrimaryButton() {
  UIButton* button = PrimaryActionButton(/*pointer_interaction_enabled=*/YES);
  SetConfigurationFont(
      button, [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline]);
  UpdatePrimaryButton(button, /*didScrollToBottom=*/YES, /*didSelectARow=*/NO);
  button.translatesAutoresizingMaskIntoConstraints = NO;
  return button;
}

UIButton* CreateMorePrimaryButton() {
  UIButton* button = PrimaryActionButton(/*pointer_interaction_enabled=*/YES);
  UIButtonConfiguration* buttonConfiguration = button.configuration;

  NSDictionary* textAttributes = @{
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline]
  };
  NSMutableAttributedString* attributedString =
      [[NSMutableAttributedString alloc]
          initWithString:l10n_util::GetNSString(
                             IDS_SEARCH_ENGINE_CHOICE_MORE_BUTTON)
              attributes:textAttributes];
  // Use `ceilf()` when calculating the icon's bounds to ensure the
  // button's content height does not shrink by fractional points, as the
  // attributed string's actual height is slightly smaller than the
  // assigned height.
  NSTextAttachment* attachment = [[NSTextAttachment alloc] init];
  attachment.image = [[UIImage imageNamed:@"read_more_arrow"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  CGFloat height = ceilf(attributedString.size.height);
  CGFloat capHeight = ceilf(
      [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline].capHeight);
  CGFloat horizontalOffset =
      base::i18n::IsRTL() ? -1.f * kMoreArrowMargin : kMoreArrowMargin;
  CGFloat verticalOffset = (capHeight - height) / 2.f;
  attachment.bounds =
      CGRectMake(horizontalOffset, verticalOffset, height, height);
  [attributedString
      appendAttributedString:[NSAttributedString
                                 attributedStringWithAttachment:attachment]];

  buttonConfiguration.attributedTitle = attributedString;
  button.configuration = buttonConfiguration;
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.enabled = YES;
  button.accessibilityIdentifier = kSearchEngineMoreButtonIdentifier;

  return button;
}

void UpdatePrimaryButton(UIButton* button,
                         BOOL isConfirmButton,
                         BOOL isEnabled) {
  if (!isConfirmButton) {
    return;
  }

  SetConfigurationTitle(
      button, l10n_util::GetNSString(IDS_SEARCH_ENGINE_CHOICE_BUTTON_TITLE));
  UIButtonConfiguration* buttonConfiguration = button.configuration;
  if (isEnabled) {
    buttonConfiguration.background.backgroundColor =
        [UIColor colorNamed:kBlueColor];
    buttonConfiguration.baseForegroundColor =
        [UIColor colorNamed:kSolidButtonTextColor];
  } else {
    buttonConfiguration.background.backgroundColor =
        [UIColor colorNamed:kTertiaryBackgroundColor];
    buttonConfiguration.baseForegroundColor =
        [UIColor colorNamed:kDisabledTintColor];
  }
  button.configuration = buttonConfiguration;
  button.enabled = isEnabled;
  button.accessibilityIdentifier = kSetAsDefaultSearchEngineIdentifier;
}
