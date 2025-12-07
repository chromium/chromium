// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/bwg_promo_view_controller.h"

#import "ios/chrome/browser/intelligence/bwg/metrics/bwg_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/ui/bwg_consent_mutator.h"
#import "ios/chrome/browser/intelligence/bwg/ui/bwg_promo_view_controller_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/ui/bwg_ui_utils.h"
#import "ios/chrome/browser/intelligence/bwg/utils/bwg_constants.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/chrome_button.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/font/font_api.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Main Stack view insets and spacing.
const CGFloat kMainStackHorizontalInset = 24.0;

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

// Spacing for primary and secondary buttons.
const CGFloat kSpacingPrimarySecondaryButtonsIOS26 = 4.0;
const CGFloat kSpacingPrimarySecondaryButtonsIOS18 = 0;

// Spacing between the scrollView and the buttons.
const CGFloat kSpacingScrollViewAndButtons = 24.0;

// Spacing between the main title and summary.
const CGFloat kSpacingTitleAndSummary = 10.0;

// Constants for gradient Gemini logo.
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
const CGFloat kGeminiLogoFontScale = 2.0;
#endif
const CGFloat kFontCapHeightMultiplier = 1.1;
const CGFloat kImageWidthAdjustment = 10.0;
const CGFloat kBaselineAdjustment = 10.0;

}  // namespace

@interface BWGPromoViewController ()
@end

@implementation BWGPromoViewController {
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

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];
}

#pragma mark - BWGFREViewControllerProtocol

- (CGFloat)contentHeight {
  [self.view layoutIfNeeded];
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
  [NSLayoutConstraint activateConstraints:@[
    [separator.heightAnchor constraintEqualToConstant:kSeparatorHeight],
    [separator.leadingAnchor
        constraintEqualToAnchor:wrapperContainer.leadingAnchor
                       constant:kOuterBoxSize + kContentHorizontalStackSpacing +
                                kTitleBodyBoxContentPadding],
    [separator.trailingAnchor
        constraintEqualToAnchor:wrapperContainer.trailingAnchor],
    [separator.topAnchor constraintEqualToAnchor:wrapperContainer.topAnchor],
    [separator.bottomAnchor
        constraintEqualToAnchor:wrapperContainer.bottomAnchor]
  ]];
  return wrapperContainer;
}

// Configures the main stack view and contains all the content including the
// buttons.
- (void)configureMainStackView {
  _mainStackView = [[UIStackView alloc] init];
  _mainStackView.axis = UILayoutConstraintAxisVertical;
  _mainStackView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_mainStackView];

  UILayoutGuide* safeArea = self.view.safeAreaLayoutGuide;
  [NSLayoutConstraint activateConstraints:@[
    [_mainStackView.topAnchor constraintEqualToAnchor:safeArea.topAnchor],
    [_mainStackView.leadingAnchor
        constraintEqualToAnchor:safeArea.leadingAnchor
                       constant:kMainStackHorizontalInset],
    [_mainStackView.trailingAnchor
        constraintEqualToAnchor:safeArea.trailingAnchor
                       constant:-kMainStackHorizontalInset],
  ]];

  [_mainStackView addArrangedSubview:[self createMainTitle]];
  [_mainStackView setCustomSpacing:kSpacingTitleAndSummary
                         afterView:_titleContainerView];

  UIImageSymbolConfiguration* config = [UIImageSymbolConfiguration
      configurationWithPointSize:kIconSize
                          weight:UIImageSymbolWeightMedium];

  UIImageView* firstIconImageView = [[UIImageView alloc]
      initWithImage:CustomSymbolWithConfiguration(kTextSearchSymbol, config)];

  UIView* firstIconContainer =
      [self createIconContainerView:firstIconImageView];
  UIStackView* firstTitleBodyStackView = [self
      createContentDescriptionWithTitle:l10n_util::GetNSString(
                                            IDS_IOS_BWG_PROMO_FIRST_BOX_TITLE)

                                   body:l10n_util::GetNSString(
                                            IDS_IOS_BWG_PROMO_FIRST_BOX_BODY)];
  UIStackView* firstContentHorizontalStackView =
      [self createContentHorizontalStackViewWithIconContainer:firstIconContainer
                                               titleBodyStack:
                                                   firstTitleBodyStackView];
  [_mainStackView addArrangedSubview:firstContentHorizontalStackView];

  UIView* separatorView = [self createSeparatorView];
  [_mainStackView addArrangedSubview:separatorView];

  UIImageView* secondIconImageView = [[UIImageView alloc]
      initWithImage:DefaultSymbolWithConfiguration(kListBulletSymbol, config)];

  UIView* secondIconContainer =
      [self createIconContainerView:secondIconImageView];
  UIStackView* secondTitleBodyStackView = [self
      createContentDescriptionWithTitle:l10n_util::GetNSString(
                                            IDS_IOS_BWG_PROMO_SECOND_BOX_TITLE)

                                   body:l10n_util::GetNSString(
                                            IDS_IOS_BWG_PROMO_SECOND_BOX_BODY)];
  UIStackView* secondContentHorizontalStackView = [self
      createContentHorizontalStackViewWithIconContainer:secondIconContainer
                                         titleBodyStack:
                                             secondTitleBodyStackView];
  [_mainStackView addArrangedSubview:secondContentHorizontalStackView];
  [_mainStackView setCustomSpacing:kSpacingScrollViewAndButtons
                         afterView:secondContentHorizontalStackView];
  [self configureButtons];
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

#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
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

  [NSLayoutConstraint activateConstraints:@[
    [iconImageView.centerXAnchor
        constraintEqualToAnchor:innerBox.centerXAnchor],
    [iconImageView.centerYAnchor
        constraintEqualToAnchor:innerBox.centerYAnchor],
    [innerBox.widthAnchor constraintEqualToConstant:kWhiteInnerSize],
    [innerBox.heightAnchor constraintEqualToConstant:kWhiteInnerSize],
    [iconBox.widthAnchor constraintEqualToConstant:kOuterBoxSize],
    [iconBox.heightAnchor constraintEqualToConstant:kOuterBoxSize],
  ]];
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

// Creates the Primary Button.
- (UIButton*)createPrimaryButton {
  ChromeButton* primaryButton =
      [[ChromeButton alloc] initWithStyle:ChromeButtonStylePrimary];
  primaryButton.title =
      l10n_util::GetNSString(IDS_IOS_BWG_PROMO_PRIMARY_BUTTON);
  [primaryButton addTarget:self
                    action:@selector(didTapPrimaryButton:)
          forControlEvents:UIControlEventTouchUpInside];
  primaryButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_BWG_PROMO_PRIMARY_BUTTON);
  return primaryButton;
}

// Creates the Secondary Button.
- (UIButton*)createSecondaryButton {
  ChromeButton* secondaryButton =
      [[ChromeButton alloc] initWithStyle:ChromeButtonStyleSecondary];
  secondaryButton.title =
      l10n_util::GetNSString(IDS_IOS_BWG_PROMO_SECONDARY_BUTTON);
  [secondaryButton addTarget:self
                      action:@selector(didTapSecondaryButton:)
            forControlEvents:UIControlEventTouchUpInside];
  secondaryButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_BWG_PROMO_SECONDARY_BUTTON);
  return secondaryButton;
}

// Configures Primary and Secondary Buttons.
- (void)configureButtons {
  UIView* primaryButtonView = [self createPrimaryButton];
  [_mainStackView addArrangedSubview:primaryButtonView];
  if (@available(iOS 26, *)) {
    [_mainStackView setCustomSpacing:kSpacingPrimarySecondaryButtonsIOS26
                           afterView:primaryButtonView];
  } else {
    [_mainStackView setCustomSpacing:kSpacingPrimarySecondaryButtonsIOS18
                           afterView:primaryButtonView];
  }
  [_mainStackView addArrangedSubview:[self createSecondaryButton]];
}

// Did tap Primary Button.
- (void)didTapPrimaryButton:(UIButton*)sender {
  RecordFREPromoAction(IOSGeminiFREAction::kAccept);
  [self.BWGPromoDelegate didAcceptPromo];
}

// Did tap Secondary Button.
- (void)didTapSecondaryButton:(UIButton*)sender {
  RecordFREPromoAction(IOSGeminiFREAction::kDismiss);
  [self.mutator didCloseBWGPromo];
}

@end
