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
// Line width for the fake omnibox.
constexpr CGFloat kLineWidth = 1.;
// Parameters for the fake omnibox.
constexpr CGFloat kFakeOmniboxWidth = 226.;
constexpr CGFloat kFakeOmniboxHeight = 48.;
constexpr CGFloat kFakeOmniboxCornerRadius = 99.;
// Parameters for empty field in the fake omnibox.
constexpr CGFloat kFakeOmniboxFieldWidth = 102.;
constexpr CGFloat kFakeOmniboxFieldHeight = 12.;
constexpr CGFloat kFakeOmniboxFieldCornerRadius = 12.;
constexpr CGFloat kFakeOmniboxFieldLeadingInset = 52.;
// Magnifying glass size.
constexpr CGFloat kMagnifyingGlassSize = 20.;
constexpr CGFloat kMagnifyingGlassFrameSize = 24.;
constexpr CGFloat kMagnifyingGlassLeadingInset = 16.;
constexpr CGFloat kMagnifyingGlassTopInset = 12.;
// The margin between the text and the arrow on the "More" button.
constexpr CGFloat kMoreArrowMargin = 4;
}  // namespace

UIView* CreateFakeEmptyOmnibox() {
  UIView* fake_omnibox = [[UIView alloc] init];

  fake_omnibox.bounds = CGRectMake(0, 0, kFakeOmniboxWidth, kFakeOmniboxHeight);

  // Create the dashed border line.
  CAShapeLayer* fake_omnibox_border = [CAShapeLayer layer];
  fake_omnibox_border.strokeColor = [UIColor colorNamed:kGrey300Color].CGColor;
  fake_omnibox_border.fillColor = nil;
  fake_omnibox_border.lineDashPattern = @[ @2, @1 ];
  fake_omnibox_border.frame = fake_omnibox.bounds;
  fake_omnibox_border.lineWidth = kLineWidth;
  fake_omnibox_border.path =
      [UIBezierPath bezierPathWithRoundedRect:fake_omnibox.bounds
                                 cornerRadius:kFakeOmniboxCornerRadius]
          .CGPath;
  [fake_omnibox.layer addSublayer:fake_omnibox_border];

  // Add the empty grey field inside.
  CAShapeLayer* fake_omnibox_field = [CAShapeLayer layer];
  fake_omnibox_field.fillColor = [UIColor colorNamed:kGrey100Color].CGColor;
  if (base::i18n::IsRTL()) {
    fake_omnibox_field.frame =
        CGRectMake(kFakeOmniboxWidth - kFakeOmniboxFieldLeadingInset -
                       kFakeOmniboxFieldWidth,
                   (kFakeOmniboxHeight - kFakeOmniboxFieldHeight) / 2.,
                   kFakeOmniboxFieldWidth, kFakeOmniboxFieldHeight);
  } else {
    fake_omnibox_field.frame =
        CGRectMake(kFakeOmniboxFieldLeadingInset,
                   (kFakeOmniboxHeight - kFakeOmniboxFieldHeight) / 2.,
                   kFakeOmniboxFieldWidth, kFakeOmniboxFieldHeight);
  }
  fake_omnibox_field.path =
      [UIBezierPath
          bezierPathWithRoundedRect:CGRectMake(0, 0, kFakeOmniboxFieldWidth,
                                               kFakeOmniboxFieldHeight)
                       cornerRadius:kFakeOmniboxFieldCornerRadius]
          .CGPath;
  [fake_omnibox.layer addSublayer:fake_omnibox_field];

  // Add the search icon to the side.
  UIImageView* searchSymbolIcon = [[UIImageView alloc]
      initWithImage:DefaultSymbolWithPointSize(kMagnifyingglassSymbol,
                                               kMagnifyingGlassSize)];

  [fake_omnibox addSubview:searchSymbolIcon];
  if (base::i18n::IsRTL()) {
    searchSymbolIcon.frame =
        CGRectMake(kFakeOmniboxWidth - kMagnifyingGlassLeadingInset -
                       kMagnifyingGlassFrameSize,
                   kMagnifyingGlassTopInset, kMagnifyingGlassFrameSize,
                   kMagnifyingGlassFrameSize);
  } else {
    searchSymbolIcon.frame =
        CGRectMake(kMagnifyingGlassLeadingInset, kMagnifyingGlassTopInset,
                   kMagnifyingGlassFrameSize, kMagnifyingGlassFrameSize);
  }
  fake_omnibox.translatesAutoresizingMaskIntoConstraints = NO;
  return fake_omnibox;
}

UIView* CreateFakeOmnibox(UIImageView* icon, NSString* searchEngineName) {
  UIView* fake_omnibox = [[UIView alloc] init];

  fake_omnibox.bounds = CGRectMake(0, 0, kFakeOmniboxWidth, kFakeOmniboxHeight);

  // Add the shadow around the omnibox.
  CAShapeLayer* fake_omnibox_shadow = [CAShapeLayer layer];
  fake_omnibox_shadow.frame = fake_omnibox.bounds;
  fake_omnibox_shadow.shadowColor = [UIColor colorNamed:kGrey300Color].CGColor;
  fake_omnibox_shadow.shadowOpacity = 1;
  fake_omnibox_shadow.shadowRadius = 16;
  fake_omnibox_shadow.shadowOffset = CGSizeMake(0, 4);
  fake_omnibox_shadow.shadowPath =
      [UIBezierPath bezierPathWithRoundedRect:fake_omnibox.bounds
                                 cornerRadius:kFakeOmniboxCornerRadius]
          .CGPath;
  [fake_omnibox.layer addSublayer:fake_omnibox_shadow];

  // Create the pill-shaped field.
  CAShapeLayer* fake_omnibox_pill = [CAShapeLayer layer];
  fake_omnibox_pill.fillColor = [UIColor colorNamed:kBackgroundColor].CGColor;
  fake_omnibox_pill.frame = fake_omnibox.bounds;
  fake_omnibox_pill.path =
      [UIBezierPath bezierPathWithRoundedRect:fake_omnibox.bounds
                                 cornerRadius:kFakeOmniboxCornerRadius]
          .CGPath;
  [fake_omnibox.layer addSublayer:fake_omnibox_pill];
  // Add the search engine Label.
  UILabel* searchWithLabel = [[UILabel alloc] init];
  if (base::i18n::IsRTL()) {
    searchWithLabel.frame =
        CGRectMake(0., 0., kFakeOmniboxWidth - kFakeOmniboxFieldLeadingInset,
                   kFakeOmniboxHeight);
  } else {
    searchWithLabel.frame = CGRectMake(
        kFakeOmniboxFieldLeadingInset, 0.,
        kFakeOmniboxWidth - kFakeOmniboxFieldLeadingInset, kFakeOmniboxHeight);
  }

  searchWithLabel.text =
      l10n_util::GetNSStringF(IDS_SEARCH_ENGINE_CHOICE_FAKE_OMNIBOX_TEXT,
                              base::SysNSStringToUTF16(searchEngineName));
  searchWithLabel.font = [UIFont systemFontOfSize:13];
  searchWithLabel.numberOfLines = 0;
  [fake_omnibox addSubview:searchWithLabel];

  // Add the favicon on the side.
  [fake_omnibox addSubview:icon];
  if (base::i18n::IsRTL()) {
    icon.frame = CGRectMake(kFakeOmniboxWidth - kMagnifyingGlassLeadingInset -
                                kMagnifyingGlassFrameSize,
                            kMagnifyingGlassTopInset, kMagnifyingGlassFrameSize,
                            kMagnifyingGlassFrameSize);
  } else {
    icon.frame =
        CGRectMake(kMagnifyingGlassLeadingInset, kMagnifyingGlassTopInset,
                   kMagnifyingGlassFrameSize, kMagnifyingGlassFrameSize);
  }

  fake_omnibox.translatesAutoresizingMaskIntoConstraints = NO;
  return fake_omnibox;
}

UIFont* GetTitleFontWithTraitCollection(UITraitCollection* trait_collection) {
  BOOL dynamic_type_enabled = UIContentSizeCategoryIsAccessibilityCategory(
      trait_collection.preferredContentSizeCategory);

  UIFontTextStyle text_style;
  if (!dynamic_type_enabled) {
    if (IsRegularXRegularSizeClass(trait_collection)) {
      text_style = UIFontTextStyleTitle1;
    } else if (!IsSmallDevice()) {
      text_style = UIFontTextStyleLargeTitle;
    }
  } else {
    text_style = UIFontTextStyleTitle2;
  }

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
