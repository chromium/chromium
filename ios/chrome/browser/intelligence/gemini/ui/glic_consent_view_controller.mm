// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/gemini/ui/glic_consent_view_controller.h"

#import "ios/chrome/browser/intelligence/gemini/ui/glic_consent_mutator.h"
#import "ios/chrome/browser/intelligence/gemini/ui/glic_constants.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller_delegate.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Main Stack view insets and spacing.
const CGFloat kMainStackHorizontalInset = 20.0;
const CGFloat kMainStackTopInset = 24.0;
const CGFloat kMainStackSpacing = 16.0;

// Icons size and names.
const CGFloat kIconSize = 16.0;
// TODO(crbug.com/414777888): Change info circle fill icon to page spark icon.
constexpr NSString* const kInfoIconName = @"info.circle.fill";
constexpr NSString* const kClockIconName =
    @"clock.arrow.trianglehead.counterclockwise.rotate.90";
const CGFloat kIconImageViewTopPadding = 18.0;
const CGFloat kIconImageViewWidth = 32.0;

// Boxes stack view traits.
const CGFloat kBoxesStackViewSpacing = 2.0;
const CGFloat kBoxesStackViewCornerRadius = 16.0;

// Inner stack view spacing and padding.
const CGFloat kInnerStackViewSpacing = 6.0;
const CGFloat kInnerStackViewPadding = 12.0;

// Line height multiple.
const CGFloat kLineHeightMultiple = 18.0 / 14.0;

}  // namespace

@interface GLICConsentViewController () <PromoStyleViewControllerDelegate>
@end

@implementation GLICConsentViewController {
  // Main stack view containing all the others views.
  UIStackView* _mainStackView;
}

#pragma mark - UIViewController

// TODO(crbug.com/414777915): Implement a basic UI.
- (void)viewDidLoad {
  self.navigationItem.hidesBackButton = YES;
  [self configurePromoStyleProperties];
  [self setupStackView];
  [super viewDidLoad];
}

#pragma mark - Private

// Configure all the stacks.
- (void)setupStackView {
  [self configureMainStackView];
  [_mainStackView addArrangedSubview:[self createBoxesStackView]];
  [_mainStackView addArrangedSubview:[self createFootNoteLabel]];
}

// Configure promo style properties to add buttons. Ignores header image type.
- (void)configurePromoStyleProperties {
  self.layoutBehindNavigationBar = YES;
  self.shouldHideBanner = YES;
  self.headerImageType = PromoStyleImageType::kNone;

  self.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_GLIC_CONSENT_PRIMARY_BUTTON);
  self.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_GLIC_FIRST_RUN_SECONDARY_BUTTON);
}

// Configure the main stack view.
- (void)configureMainStackView {
  _mainStackView = [[UIStackView alloc] init];
  _mainStackView.axis = UILayoutConstraintAxisVertical;
  _mainStackView.distribution = UIStackViewDistributionFill;
  _mainStackView.alignment = UIStackViewAlignmentFill;
  _mainStackView.spacing = kMainStackSpacing;

  _mainStackView.translatesAutoresizingMaskIntoConstraints = NO;

  [self.view addSubview:_mainStackView];

  [NSLayoutConstraint activateConstraints:@[
    [_mainStackView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor
                       constant:kMainStackHorizontalInset],
    [_mainStackView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor
                       constant:-kMainStackHorizontalInset],
    [_mainStackView.topAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor
                       constant:kMainStackTopInset],
    [_mainStackView.bottomAnchor
        constraintLessThanOrEqualToAnchor:self.view.safeAreaLayoutGuide
                                              .bottomAnchor]
  ]];
}

// Create the 2 horizontal boxes stack view.
- (UIStackView*)createBoxesStackView {
  UIStackView* boxesStackView = [[UIStackView alloc] init];
  boxesStackView.axis = UILayoutConstraintAxisVertical;
  boxesStackView.distribution = UIStackViewDistributionFill;
  boxesStackView.alignment = UIStackViewAlignmentFill;
  boxesStackView.spacing = kBoxesStackViewSpacing;
  boxesStackView.layer.cornerRadius = kBoxesStackViewCornerRadius;
  boxesStackView.clipsToBounds = YES;
  boxesStackView.translatesAutoresizingMaskIntoConstraints = NO;

  UIView* box1 = [self
      createHorizontalBoxWithIcon:kInfoIconName
                          boxView:
                              [self
                                  createBoxWithTitle:
                                      kGLICConsentFirstBoxTitleText
                                            bodyText:
                                                kGLICConsentFirstBoxBodyText]];
  [boxesStackView addArrangedSubview:box1];

  UIView* box2 = [self
      createHorizontalBoxWithIcon:kClockIconName
                          boxView:
                              [self
                                  createBoxWithTitle:
                                      kGLICConsentSecondBoxTitleText
                                            bodyText:
                                                kGLICConsentSecondBoxBodyText]];
  [boxesStackView addArrangedSubview:box2];
  return boxesStackView;
}

// Create horizontal stack view with icon and box view.
- (UIView*)createHorizontalBoxWithIcon:(NSString*)iconName
                               boxView:(UIView*)boxView {
  UIStackView* horizontalStackView = [[UIStackView alloc] init];
  horizontalStackView.axis = UILayoutConstraintAxisHorizontal;
  horizontalStackView.distribution = UIStackViewDistributionFillProportionally;
  horizontalStackView.alignment = UIStackViewAlignmentTop;
  horizontalStackView.translatesAutoresizingMaskIntoConstraints = NO;
  horizontalStackView.backgroundColor = [UIColor colorNamed:kGrey100Color];

  UIImageSymbolConfiguration* config = [UIImageSymbolConfiguration
      configurationWithPointSize:kIconSize
                          weight:UIImageSymbolWeightRegular];

  UIImageView* iconImageView = [[UIImageView alloc]
      initWithImage:DefaultSymbolWithConfiguration(iconName, config)];
  iconImageView.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [iconImageView.widthAnchor constraintEqualToConstant:kIconSize],
    [iconImageView.heightAnchor constraintEqualToConstant:kIconSize]
  ]];

  UIView* iconContainerView = [[UIView alloc] init];
  iconContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  [iconContainerView addSubview:iconImageView];
  [horizontalStackView addArrangedSubview:iconContainerView];

  [NSLayoutConstraint activateConstraints:@[
    [iconImageView.centerXAnchor
        constraintEqualToAnchor:iconContainerView.centerXAnchor],
    [iconImageView.topAnchor constraintEqualToAnchor:iconContainerView.topAnchor
                                            constant:kIconImageViewTopPadding],
    [iconContainerView.widthAnchor
        constraintEqualToAnchor:iconImageView.widthAnchor
                       constant:kIconImageViewWidth],
  ]];

  [horizontalStackView addArrangedSubview:boxView];

  return horizontalStackView;
}

// Create the bow view containing the text and the title.
- (UIView*)createBoxWithTitle:(NSString*)titleText
                     bodyText:(NSString*)bodyText {
  UIView* boxView = [[UIView alloc] init];
  boxView.translatesAutoresizingMaskIntoConstraints = NO;

  UIStackView* innerStackView = [[UIStackView alloc] init];
  innerStackView.axis = UILayoutConstraintAxisVertical;
  innerStackView.distribution = UIStackViewDistributionFill;
  innerStackView.alignment = UIStackViewAlignmentLeading;
  innerStackView.spacing = kInnerStackViewSpacing;

  innerStackView.translatesAutoresizingMaskIntoConstraints = NO;
  [boxView addSubview:innerStackView];

  CGFloat innerPadding = kInnerStackViewPadding;
  AddSameConstraintsWithInsets(
      innerStackView, boxView,
      NSDirectionalEdgeInsetsMake(innerPadding, innerPadding, innerPadding,
                                  innerPadding));

  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.text = titleText;
  titleLabel.font = PreferredFontForTextStyle(UIFontTextStyleTitle3,
                                              UIFontWeightSemibold, 14);

  NSMutableAttributedString* attributedText =
      [[NSMutableAttributedString alloc] initWithString:titleText];
  NSMutableParagraphStyle* paragraphStyle =
      [[NSMutableParagraphStyle alloc] init];
  paragraphStyle.lineHeightMultiple = kLineHeightMultiple;
  [attributedText addAttribute:NSParagraphStyleAttributeName
                         value:paragraphStyle
                         range:NSMakeRange(0, [titleText length])];
  titleLabel.attributedText = attributedText;
  titleLabel.adjustsFontForContentSizeCategory = YES;
  titleLabel.numberOfLines = 0;
  [innerStackView addArrangedSubview:titleLabel];

  UILabel* bodyLabel = [[UILabel alloc] init];
  bodyLabel.text = bodyText;
  bodyLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  bodyLabel.adjustsFontForContentSizeCategory = YES;
  bodyLabel.numberOfLines = 0;
  bodyLabel.textColor = [UIColor colorNamed:kGrey700Color];
  [innerStackView addArrangedSubview:bodyLabel];

  return boxView;
}

// Create the foot note label.
- (UILabel*)createFootNoteLabel {
  UILabel* footNoteLabel = [[UILabel alloc] init];
  footNoteLabel.text = kGLICConsentFootNoteText;
  footNoteLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleCaption2];
  footNoteLabel.numberOfLines = 2;
  footNoteLabel.textAlignment = NSTextAlignmentCenter;
  return footNoteLabel;
}

#pragma mark - PromoStyleViewControllerDelegate

- (void)didTapPrimaryActionButton {
  [self.mutator didConsentGLIC];
}

- (void)didTapSecondaryActionButton {
  [self.mutator didRefuseGLICConsent];
}

@end
