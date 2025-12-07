// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/ui/info_message/lens_translate_error_view_controller.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/font/font_api.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The image to be displayed on the leading edge of the UI.
NSString* const kLensTranslateErrorImage = @"lens_translate_error";

// The size of the translate error image.
const CGFloat kLensTranslateErrorImageSize = 56.0;

// The leading padding for the translate error view.
const CGFloat kLeadingPadding = 24.0;

// The trailing padding for the translate error view.
const CGFloat kTrailingPadding = 52.0;

// Top padding for the view content.
const CGFloat kViewTopPadding = 36.0;

// Ammount of spacing from the trailing edge of the image, in points.
const CGFloat kImageHorizontalSpacing = 24.0;

// The height of the translate error message.
const CGFloat kPreferredContentHeight = 120.0;

// The font size of the title.
const CGFloat kTitleFontSize = 18;

// The font size of the subtitle.
const CGFloat kSubtitleFontSize = 12;

// The extra space between the title label and the subtitle.
const CGFloat kVerticalSpacing = 2;

}  // namespace

@implementation LensTranslateErrorViewController

#pragma mark - UIViewController

- (CGSize)preferredContentSize {
  CGFloat fittingWidth = self.view.safeAreaLayoutGuide.layoutFrame.size.width;
  return CGSizeMake(fittingWidth, kPreferredContentHeight);
}

- (void)loadView {
  [super loadView];
  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];

  UILabel* titleLabel = [self createTitleLabel];
  UILabel* subtitleLabel = [self createSubtitleLabel];
  UIImageView* imageView = [self createImageView];

  UIStackView* textStackView = [[UIStackView alloc] initWithArrangedSubviews:@[
    titleLabel,
    subtitleLabel,
  ]];
  textStackView.translatesAutoresizingMaskIntoConstraints = NO;
  textStackView.axis = UILayoutConstraintAxisVertical;
  textStackView.spacing = 4;

  UIStackView* mainStackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ imageView, textStackView ]];
  mainStackView.translatesAutoresizingMaskIntoConstraints = NO;
  mainStackView.distribution = UIStackViewDistributionFill;
  mainStackView.spacing = kImageHorizontalSpacing;
  mainStackView.alignment = UIStackViewAlignmentCenter;
  mainStackView.axis = UILayoutConstraintAxisHorizontal;

  [self.view addSubview:mainStackView];

  AddSizeConstraints(imageView, CGSizeMake(kLensTranslateErrorImageSize,
                                           kLensTranslateErrorImageSize));
  LayoutSides sides =
      LayoutSides::kTop | LayoutSides::kTrailing | LayoutSides::kLeading;
  NSDirectionalEdgeInsets insets = NSDirectionalEdgeInsetsMake(
      kViewTopPadding, kLeadingPadding, 0, kTrailingPadding);
  AddSameConstraintsToSidesWithInsets(mainStackView, self.view, sides, insets);
}

#pragma mark - Private Methods

- (UILabel*)createTitleLabel {
  UILabel* label = [[UILabel alloc] init];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.adjustsFontForContentSizeCategory = YES;
  label.lineBreakMode = NSLineBreakByWordWrapping;
  label.numberOfLines = 0;
  label.font = ios::provider::GetBrandedProductRegularFont(kTitleFontSize);
  label.text =
      l10n_util::GetNSString(IDS_IOS_LENS_OVERLAY_TRANSLATE_ERROR_TITLE);
  label.textColor = [UIColor colorNamed:kTextPrimaryColor];
  return label;
}

- (UILabel*)createSubtitleLabel {
  UILabel* label = [[UILabel alloc] init];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.adjustsFontForContentSizeCategory = YES;
  label.lineBreakMode = NSLineBreakByWordWrapping;
  label.numberOfLines = 0;

  NSString* labelText =
      l10n_util::GetNSString(IDS_IOS_LENS_OVERLAY_TRANSLATE_ERROR_SUBTITLE);
  NSMutableAttributedString* attributedString =
      [[NSMutableAttributedString alloc] initWithString:labelText];

  NSMutableParagraphStyle* paragraphStyle =
      [[NSMutableParagraphStyle alloc] init];
  [paragraphStyle setLineSpacing:kVerticalSpacing];

  NSDictionary* attributes = @{
    NSFontAttributeName :
        ios::provider::GetBrandedProductRegularFont(kSubtitleFontSize),
    NSParagraphStyleAttributeName : paragraphStyle,
  };

  [attributedString addAttributes:attributes
                            range:NSMakeRange(0, labelText.length)];

  label.attributedText = attributedString;
  label.textColor =
      [UIColor colorNamed:kLensOverlayConsentDialogDescriptionColor];
  return label;
}

- (UIImageView*)createImageView {
  UIImageView* imageView = [[UIImageView alloc] init];
  imageView.translatesAutoresizingMaskIntoConstraints = NO;
  imageView.contentMode = UIViewContentModeScaleAspectFit;
  imageView.image = [UIImage imageNamed:kLensTranslateErrorImage];
  return imageView;
}

@end
