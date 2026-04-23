// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_promo_view_controller.h"

#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_consent_mutator.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/font/font_api.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Icons size.
const CGFloat kIconSize = 20.0;
const CGFloat kWhiteInnerSize = 32.0;

// Corner radius of icons.
const CGFloat kIconCornerRadius = 16.0;
// Inner box padding and corner radius.
const CGFloat kInnerBoxPadding = 12.0;
const CGFloat kInnerBoxCornerRadius = 12.0;

// Horizontal stack spacing between icon and text.
const CGFloat kContentHorizontalStackSpacing = 8.0;

// Spacing and padding of the body title and box.
const CGFloat kTitleBodyVerticalSpacing = 4.0;
// Padding within the vertical title body stack view.
const CGFloat kTitleBodyBoxContentPadding = 12.0;

// Size of the outer box containing the icon.
const CGFloat kOuterBoxSize = 64.0;

// Height of the separator line.
const CGFloat kSeparatorHeight = 1.0;

// Spacing between the main title and summary.
const CGFloat kSpacingTitleAndSummary = 10.0;

// Constants for gradient Gemini logo.
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
const CGFloat kGeminiLogoFontScale = 2.0;
#endif
const CGFloat kFontCapHeightMultiplier = 1.1;
const CGFloat kImageWidthAdjustment = 10.0;
const CGFloat kBaselineAdjustment = 10.0;

}  // namespace

@interface GeminiPromoViewController ()
@end

@implementation GeminiPromoViewController {
  // Main stack view. This view itself does not scroll.
  UIStackView* _mainStackView;
  // View that contains the main title.
  UIView* _titleContainerView;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  self.navigationItem.hidesBackButton = YES;
  [self configureMainStackView];
}

#pragma mark - GeminiFREViewControllerProtocol

- (CGFloat)contentHeight {
  return
      [_mainStackView systemLayoutSizeFittingSize:UILayoutFittingCompressedSize]
          .height;
}

#pragma mark - Private

// Creates a tiny horizontal separator.
- (UIView*)createSeparatorView {
  UIView* wrapperContainer = [[UIView alloc] init];
  wrapperContainer.translatesAutoresizingMaskIntoConstraints = NO;
  UIView* separator = [[UIView alloc] init];
  separator.backgroundColor = [UIColor colorNamed:kBWGSeparatorColor];
  separator.translatesAutoresizingMaskIntoConstraints = NO;
  [wrapperContainer addSubview:separator];
  LayoutSides sides =
      LayoutSides::kTop | LayoutSides::kBottom | LayoutSides::kTrailing;
  AddSameConstraintsToSides(separator, wrapperContainer, sides);
  [NSLayoutConstraint activateConstraints:@[
    [separator.heightAnchor constraintEqualToConstant:kSeparatorHeight],
    [separator.leadingAnchor
        constraintEqualToAnchor:wrapperContainer.leadingAnchor
                       constant:kOuterBoxSize + kContentHorizontalStackSpacing +
                                kTitleBodyBoxContentPadding]
  ]];
  return wrapperContainer;
}

// Creates a feature row with an icon and localized title/body IDs.
- (UIView*)createFeatureRowWithIcon:(UIImage*)icon
                            titleID:(int)titleID
                             bodyID:(int)bodyID {
  NSString* title = l10n_util::GetNSString(titleID);
  NSString* body = l10n_util::GetNSString(bodyID);

  UIImageView* iconImageView = [[UIImageView alloc] initWithImage:icon];
  UIView* iconContainer = [self createIconContainerView:iconImageView];
  UIStackView* titleBodyStackView =
      [self createContentDescriptionWithTitle:title body:body];
  return [self
      createContentHorizontalStackViewWithIconContainer:iconContainer
                                         titleBodyStack:titleBodyStackView];
}

// Configures the main stack view and contains all the content including the
// buttons.
- (void)configureMainStackView {
  _mainStackView = [[UIStackView alloc] init];
  _mainStackView.axis = UILayoutConstraintAxisVertical;
  _mainStackView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_mainStackView];
  PinToSafeArea(_mainStackView, self.view);

  [_mainStackView addArrangedSubview:[self createMainTitle]];
  [_mainStackView setCustomSpacing:kSpacingTitleAndSummary
                         afterView:_titleContainerView];

  UIImageSymbolConfiguration* config = [UIImageSymbolConfiguration
      configurationWithPointSize:kIconSize
                          weight:UIImageSymbolWeightMedium];

  UIView* askRow =
      [self createFeatureRowWithIcon:CustomSymbolWithConfiguration(
                                         kTextSearchSymbol, config)
                             titleID:IDS_IOS_BWG_PROMO_FIRST_BOX_TITLE
                              bodyID:IDS_IOS_BWG_PROMO_FIRST_BOX_BODY];
  [_mainStackView addArrangedSubview:askRow];
  [_mainStackView addArrangedSubview:[self createSeparatorView]];

  if ([self.mutator shouldShowImageRemixRow]) {
    UIView* remixRow = [self
        createFeatureRowWithIcon:DefaultSymbolWithConfiguration(
                                     kPhotoOnRectangleAngled, config)
                         titleID:IDS_IOS_GEMINI_PROMO_REMIX_IMAGE_BOX_TITLE
                          bodyID:IDS_IOS_GEMINI_PROMO_REMIX_IMAGE_BOX_BODY];
    [_mainStackView addArrangedSubview:remixRow];
    [_mainStackView addArrangedSubview:[self createSeparatorView]];
  }

  UIView* summarizeRow =
      [self createFeatureRowWithIcon:DefaultSymbolWithConfiguration(
                                         kListBulletSymbol, config)
                             titleID:IDS_IOS_BWG_PROMO_SECOND_BOX_TITLE
                              bodyID:IDS_IOS_BWG_PROMO_SECOND_BOX_BODY];
  [_mainStackView addArrangedSubview:summarizeRow];
}

// Creates the main title.
- (UIView*)createMainTitle {
  UILabel* mainTitleLabel = [self createGradientMainTitleLabel];
  _titleContainerView = [[UIView alloc] init];
  _titleContainerView.translatesAutoresizingMaskIntoConstraints = NO;

  [_titleContainerView addSubview:mainTitleLabel];

  AddSameConstraints(mainTitleLabel, _titleContainerView);
  return _titleContainerView;
}

// Create a gradient main title label.
- (UILabel*)createGradientMainTitleLabel {
  UILabel* mainTitleLabel = [[UILabel alloc] init];
  mainTitleLabel.textAlignment = NSTextAlignmentCenter;
  mainTitleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  mainTitleLabel.numberOfLines = 0;
  UIFont* labelFont =
      PreferredFontForTextStyle(UIFontTextStyleTitle1, UIFontWeightSemibold);
  mainTitleLabel.font = labelFont;
  NSString* mainTitleString =
      l10n_util::GetNSString(IDS_IOS_BWG_PROMO_MAIN_TITLE);

#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
  NSString* gradientSubstring =
      l10n_util::GetNSString(IDS_IOS_BWG_PROMO_GRADIENT_TEXT);
  UIImage* geminiIcon = CustomSymbolWithPointSize(
      kGeminiFullSymbol, labelFont.pointSize + kGeminiLogoFontScale);
  UIImage* gradientImage = [self createGradientImageFromSymbol:geminiIcon];

  NSAttributedString* gradientGeminiString =
      [self attributedStringWithText:mainTitleString
                  replacingSubstring:gradientSubstring
                           withImage:gradientImage
                                font:labelFont];

  mainTitleLabel.attributedText = gradientGeminiString;
#else
  NSMutableAttributedString* attributedString =
      [[NSMutableAttributedString alloc] initWithString:mainTitleString];
  mainTitleLabel.attributedText = attributedString;
#endif

  mainTitleLabel.accessibilityLabel = mainTitleString;
  mainTitleLabel.accessibilityTraits |= UIAccessibilityTraitHeader;
  return mainTitleLabel;
}

// Creates a gradient image from a symbol image.
- (UIImage*)createGradientImageFromSymbol:(UIImage*)symbolImage {
  CGSize iconSize = symbolImage.size;

  CAGradientLayer* gradientLayer = [CAGradientLayer layer];
  gradientLayer.colors = [self createGradientColorsArray];
  gradientLayer.startPoint = CGPointMake(0.0, 0.5);
  gradientLayer.endPoint = CGPointMake(1.0, 0.5);
  gradientLayer.frame = CGRectMake(0, 0, iconSize.width, iconSize.height);

  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:iconSize];
  UIImage* gradientImage = [renderer
      imageWithActions:^(UIGraphicsImageRendererContext* rendererContext) {
        // CG layers have inversed coordinates of symbol images. Therefore, we
        // vertically flip the rendered image.
        CGContextTranslateCTM(rendererContext.CGContext, 0, iconSize.height);
        CGContextScaleCTM(rendererContext.CGContext, 1.0, -1.0);
        CGContextClipToMask(rendererContext.CGContext,
                            CGRectMake(0, 0, iconSize.width, iconSize.height),
                            symbolImage.CGImage);
        [gradientLayer renderInContext:rendererContext.CGContext];
      }];
  return gradientImage;
}

// Creates an attributed string and replaces a substring with an image.
- (NSAttributedString*)attributedStringWithText:(NSString*)text
                             replacingSubstring:(NSString*)substring
                                      withImage:(UIImage*)image
                                           font:(UIFont*)font {
  NSMutableAttributedString* attributedString =
      [[NSMutableAttributedString alloc]
          initWithString:text
              attributes:@{NSFontAttributeName : font}];

  NSRange range = [attributedString.string rangeOfString:substring];
  NSTextAttachment* textAttachment = [[NSTextAttachment alloc] init];
  textAttachment.image = image;

  // Adjust bounds so image aligns with the string.
  CGFloat imageHeight = font.capHeight * kFontCapHeightMultiplier;
  CGFloat imageAspectRatio = image.size.width / image.size.height;
  CGFloat imageWidth = imageHeight * imageAspectRatio + kImageWidthAdjustment;
  CGFloat yOrigin = font.descender / kBaselineAdjustment;

  textAttachment.bounds = CGRectMake(0, yOrigin, imageWidth, imageHeight);

  NSAttributedString* attachmentString =
      [NSAttributedString attributedStringWithAttachment:textAttachment];

  [attributedString replaceCharactersInRange:range
                        withAttributedString:attachmentString];

  return attributedString;
}

// Create an array of colors representing a gradient color palette.
- (NSArray*)createGradientColorsArray {
  UITraitCollection* lightTraitCollection = [UITraitCollection
      traitCollectionWithUserInterfaceStyle:UIUserInterfaceStyleLight];
  NSArray<UIColor*>* colors = @[
    [[UIColor colorNamed:kBlue500Color]
        resolvedColorWithTraitCollection:lightTraitCollection],
    [[UIColor colorNamed:kBlue700Color]
        resolvedColorWithTraitCollection:lightTraitCollection],
    [[UIColor colorNamed:kBlue300Color]
        resolvedColorWithTraitCollection:lightTraitCollection]
  ];

  NSMutableArray<id>* gradientColorArray = [[NSMutableArray alloc] init];
  for (UIColor* color in colors) {
    [gradientColorArray addObject:static_cast<id>(color.CGColor)];
  }
  return gradientColorArray;
}

// Creates the iconBox container view with the inner box and icon.
- (UIView*)createIconContainerView:(UIImageView*)iconImageView {
  UIView* iconBox = [[UIView alloc] init];
  iconBox.backgroundColor = [UIColor colorNamed:kFaviconBackgroundColor];
  iconBox.clipsToBounds = YES;
  iconBox.translatesAutoresizingMaskIntoConstraints = NO;
  iconBox.layer.cornerRadius = kIconCornerRadius;

  UIView* innerBox = [[UIView alloc] init];
  innerBox.layer.cornerRadius = kInnerBoxCornerRadius;
  innerBox.clipsToBounds = YES;
  innerBox.translatesAutoresizingMaskIntoConstraints = NO;
  innerBox.backgroundColor = [UIColor colorNamed:kSolidWhiteColor];
  [iconBox addSubview:innerBox];

  AddSameConstraintsWithInsets(
      innerBox, iconBox,
      NSDirectionalEdgeInsetsMake(kInnerBoxPadding, kInnerBoxPadding,
                                  kInnerBoxPadding, kInnerBoxPadding));

  iconImageView.contentMode = UIViewContentModeScaleAspectFit;
  iconImageView.translatesAutoresizingMaskIntoConstraints = NO;
  [innerBox addSubview:iconImageView];

  AddSameCenterConstraints(iconImageView, innerBox);
  AddSquareConstraints(innerBox, kWhiteInnerSize);
  AddSquareConstraints(iconBox, kOuterBoxSize);
  return iconBox;
}

// Creates a vertical stack view containing the title and body labels.
- (UIStackView*)createContentDescriptionWithTitle:(NSString*)titleText
                                             body:(NSString*)bodyText {
  UIStackView* titleBodyVerticalStackView = [[UIStackView alloc] init];
  titleBodyVerticalStackView.axis = UILayoutConstraintAxisVertical;
  titleBodyVerticalStackView.alignment = UIStackViewAlignmentLeading;
  titleBodyVerticalStackView.spacing = kTitleBodyVerticalSpacing;
  titleBodyVerticalStackView.translatesAutoresizingMaskIntoConstraints = NO;

  titleBodyVerticalStackView.layoutMarginsRelativeArrangement = YES;
  titleBodyVerticalStackView.directionalLayoutMargins =
      NSDirectionalEdgeInsetsMake(
          kTitleBodyBoxContentPadding, kTitleBodyBoxContentPadding,
          kTitleBodyBoxContentPadding, kTitleBodyBoxContentPadding);

  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.text = titleText;
  titleLabel.font = PreferredFontForTextStyle(UIFontTextStyleHeadline);
  titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  titleLabel.numberOfLines = 0;
  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  titleLabel.accessibilityTraits |= UIAccessibilityTraitHeader;

  UILabel* bodyLabel = [[UILabel alloc] init];
  bodyLabel.text = bodyText;
  bodyLabel.font = PreferredFontForTextStyle(UIFontTextStyleBody);
  bodyLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  bodyLabel.numberOfLines = 0;
  bodyLabel.translatesAutoresizingMaskIntoConstraints = NO;

  [titleBodyVerticalStackView addArrangedSubview:titleLabel];
  [titleBodyVerticalStackView addArrangedSubview:bodyLabel];

  return titleBodyVerticalStackView;
}

// Creates a horizontal stack view containing the icon and the title body stack
// view.
- (UIStackView*)
    createContentHorizontalStackViewWithIconContainer:(UIView*)iconContainerView
                                       titleBodyStack:
                                           (UIStackView*)
                                               titleBodyVerticalStack {
  UIStackView* contentHorizontalStackView = [[UIStackView alloc] init];
  contentHorizontalStackView.alignment = UIStackViewAlignmentCenter;
  contentHorizontalStackView.spacing = kContentHorizontalStackSpacing;
  contentHorizontalStackView.translatesAutoresizingMaskIntoConstraints = NO;

  [contentHorizontalStackView addArrangedSubview:iconContainerView];
  [contentHorizontalStackView addArrangedSubview:titleBodyVerticalStack];

  return contentHorizontalStackView;
}

@end
