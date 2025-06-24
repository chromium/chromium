// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/bwg_promo_view_controller.h"

#import "ios/chrome/browser/intelligence/bwg/ui/bwg_consent_mutator.h"
#import "ios/chrome/browser/intelligence/bwg/ui/bwg_constants.h"
#import "ios/chrome/browser/intelligence/bwg/ui/bwg_promo_view_controller_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/ui/bwg_ui_utils.h"
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
const CGFloat kMainStackSpacing = 8.0;

// Horizontal padding for the main title and sub title.
const CGFloat kMainTitleHorizontalPadding = 8.0;

// Icons size.
const CGFloat kIconSize = 16.0;
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
// Spacing after the subTitle.
const CGFloat kSpacingAfterSubTitle = 16.0;

// Size of the outer box containing the icon.
const CGFloat kOuterBoxSize = 64.0;

// Height of the separator line.
const CGFloat kSeparatorHeight = 1.0;

// Spacing between the scrollView and the buttons.
const CGFloat kSpacingScrollViewAndButtons = 16.0;

// Spacing between primary and secondary buttons.
const CGFloat kSpacingPrimarySecondaryButtons = 0.0;

// TODO(crbug.com/414778685): Add strings.
// String constants for UI elements.
NSString* const kBWGPromoMainTitleText = @"Lorem ipsum dolor sit amet.";
NSString* const kBWGPromoSubTitleText =
    @"Lorem ipsum dolor sit amet, consecte tur adipiscing purposes. Sed do.";
NSString* const kBWGPromoFirstBoxTitleText = @"Sed do.";
NSString* const kBWGPromoFirstBoxBodyText =
    @"Lorem ipsum dolor sit amet, consecte tur adipiscing purposes.";
NSString* const kBWGPromoSecondBoxTitleText = @"consecte tur adipiscing";
NSString* const kBWGPromoSecondBoxBodyText =
    @"orem ipsum dolor sit amet. orem ipsum dolor sit amet.";

}  // namespace

@interface BWGPromoViewController ()
@end

@implementation BWGPromoViewController {
  // Main stack view containing all the others views.
  UIStackView* _mainStackView;
  // Stack view for the scrollable content.
  UIScrollView* _contentScrollView;
  // Content stack view.
  UIStackView* _contentStackView;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.navigationItem.hidesBackButton = YES;
  [self setupStackViews];
}

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];
  [self.BWGPromoDelegate promoViewControllerWasDismissed];
}

#pragma mark - Public

- (CGFloat)contentHeight {
  return
      [_mainStackView systemLayoutSizeFittingSize:UILayoutFittingCompressedSize]
          .height +
      [_contentStackView
          systemLayoutSizeFittingSize:UILayoutFittingCompressedSize]
          .height;
}

#pragma mark - Private

// Configures all the stacks.
- (void)setupStackViews {
  [self configureMainStackView];
  [self configureContentStackView];
  [_mainStackView addArrangedSubview:_contentScrollView];
  [_mainStackView setCustomSpacing:kSpacingScrollViewAndButtons
                         afterView:_contentScrollView];
  [self configureButtons];
}

// Creates a tiny horizontal separator.
- (UIView*)createSeparatorView {
  UIView* wrapperContainer = [[UIView alloc] init];
  wrapperContainer.translatesAutoresizingMaskIntoConstraints = NO;
  UIView* separator = [[UIView alloc] init];
  separator.backgroundColor = [UIColor colorNamed:kGrey500Color];
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
  ]];
  return wrapperContainer;
}

// Configures the main stack view.
- (void)configureMainStackView {
  _mainStackView = [[UIStackView alloc] init];
  _mainStackView.axis = UILayoutConstraintAxisVertical;
  _mainStackView.spacing = kMainStackSpacing;

  _mainStackView.translatesAutoresizingMaskIntoConstraints = NO;

  [self.view addSubview:_mainStackView];
  AddSameConstraintsWithInsets(
      _mainStackView, self.view.safeAreaLayoutGuide,
      NSDirectionalEdgeInsetsMake(0, kMainStackHorizontalInset, 0,
                                  kMainStackHorizontalInset));
}

// Configures the content stack view.
- (void)configureContentStackView {
  _contentScrollView = [[UIScrollView alloc] init];
  _contentScrollView.translatesAutoresizingMaskIntoConstraints = NO;

  _contentStackView = [[UIStackView alloc] init];
  _contentStackView.axis = UILayoutConstraintAxisVertical;
  _contentStackView.spacing = kMainStackSpacing;
  _contentStackView.translatesAutoresizingMaskIntoConstraints = NO;

  [_contentScrollView addSubview:_contentStackView];

  AddSameConstraintsWithInsets(_contentStackView, _contentScrollView,
                               NSDirectionalEdgeInsetsMake(0, 0, 0, 0));

  [NSLayoutConstraint activateConstraints:@[
    [_contentStackView.widthAnchor
        constraintEqualToAnchor:_contentScrollView.widthAnchor]
  ]];

  [_contentStackView addArrangedSubview:[self createMainTitle]];
  UIView* subTitle = [self createSubTitle];
  [_contentStackView addArrangedSubview:subTitle];
  [_contentStackView setCustomSpacing:kSpacingAfterSubTitle afterView:subTitle];

  UIImageSymbolConfiguration* config = [UIImageSymbolConfiguration
      configurationWithPointSize:kIconSize
                          weight:UIImageSymbolWeightMedium];

  UIImageView* firstIconImageView = [[UIImageView alloc]
      initWithImage:CustomSymbolWithConfiguration(kTextSearchSymbol, config)];

  UIView* firstIconContainer =
      [self createIconContainerView:firstIconImageView];
  UIStackView* firstTitleBodyStackView =
      [self createContentDescriptionWithTitle:kBWGPromoFirstBoxTitleText
                                         body:kBWGPromoFirstBoxBodyText];
  UIStackView* firstContentHorizontalStackView =
      [self createContentHorizontalStackViewWithIconContainer:firstIconContainer
                                               titleBodyStack:
                                                   firstTitleBodyStackView];
  [_contentStackView addArrangedSubview:firstContentHorizontalStackView];

  UIView* separatorView = [self createSeparatorView];
  [_contentStackView addArrangedSubview:separatorView];

  UIImageView* secondIconImageView = [[UIImageView alloc]
      initWithImage:DefaultSymbolWithConfiguration(kListBulletSymbol, config)];

  UIView* secondIconContainer =
      [self createIconContainerView:secondIconImageView];
  UIStackView* secondTitleBodyStackView =
      [self createContentDescriptionWithTitle:kBWGPromoSecondBoxTitleText
                                         body:kBWGPromoSecondBoxBodyText];
  UIStackView* secondContentHorizontalStackView = [self
      createContentHorizontalStackViewWithIconContainer:secondIconContainer
                                         titleBodyStack:
                                             secondTitleBodyStackView];
  [_contentStackView addArrangedSubview:secondContentHorizontalStackView];
}

// Creates the main title.
- (UIView*)createMainTitle {
  UILabel* mainTitleLabel = [[UILabel alloc] init];
  mainTitleLabel.text = kBWGPromoMainTitleText;
  mainTitleLabel.textAlignment = NSTextAlignmentCenter;
  mainTitleLabel.adjustsFontSizeToFitWidth = YES;
  mainTitleLabel.translatesAutoresizingMaskIntoConstraints = NO;

  mainTitleLabel.font =
      PreferredFontForTextStyle(UIFontTextStyleTitle2, UIFontWeightBold);

  UIView* titleContainerView = [[UIView alloc] init];
  titleContainerView.translatesAutoresizingMaskIntoConstraints = NO;

  [titleContainerView addSubview:mainTitleLabel];

  AddSameConstraintsWithInsets(
      mainTitleLabel, titleContainerView,
      NSDirectionalEdgeInsetsMake(0, kMainTitleHorizontalPadding, 0,
                                  kMainTitleHorizontalPadding));
  return titleContainerView;
}

// Creates the sub title.
- (UIView*)createSubTitle {
  UILabel* subTitleLabel = [[UILabel alloc] init];
  subTitleLabel.text = kBWGPromoSubTitleText;
  subTitleLabel.numberOfLines = 2;
  subTitleLabel.textAlignment = NSTextAlignmentCenter;
  subTitleLabel.adjustsFontSizeToFitWidth = YES;
  subTitleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  subTitleLabel.textColor = [UIColor colorNamed:kGrey800Color];
  subTitleLabel.font =
      PreferredFontForTextStyle(UIFontTextStyleHeadline, UIFontWeightRegular);

  return subTitleLabel;
}

// Creates the iconBox container view with the  inner box and icon.
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
  innerBox.backgroundColor = [UIColor colorNamed:kBackgroundColor];
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

// Creates the primary button.
- (UIButton*)createPrimaryButton {
  UIButton* primaryButton = [BWGUIUtils
      createPrimaryButtonWithTitle:l10n_util::GetNSString(
                                       IDS_IOS_BWG_PROMO_PRIMARY_BUTTON)];
  [primaryButton addTarget:self
                    action:@selector(didTapPrimaryButton:)
          forControlEvents:UIControlEventTouchUpInside];
  // TODO(crbug.com/420643840): Add a11y labels.
  return primaryButton;
}

// Creates the secondary button.
- (UIButton*)createSecondaryButton {
  UIButton* secondaryButton = [BWGUIUtils
      createSecondaryButtonWithTitle:
          l10n_util::GetNSString(IDS_IOS_BWG_FIRST_RUN_SECONDARY_BUTTON)];
  [secondaryButton addTarget:self
                      action:@selector(didTapSecondaryButton:)
            forControlEvents:UIControlEventTouchUpInside];
  secondaryButton.accessibilityLabel = @"Promo Secondary Action";
  return secondaryButton;
}

// Configures primary and secondary buttons.
- (void)configureButtons {
  UIView* primaryButtonView = [self createPrimaryButton];
  [_mainStackView addArrangedSubview:primaryButtonView];
  [_mainStackView setCustomSpacing:kSpacingPrimarySecondaryButtons
                         afterView:primaryButtonView];
  [_mainStackView addArrangedSubview:[self createSecondaryButton]];
}

// Did tap Primary Button.
- (void)didTapPrimaryButton:(UIButton*)sender {
  [self.BWGPromoDelegate didAcceptPromo];
}

// Did tap Secondary Button.
- (void)didTapSecondaryButton:(UIButton*)sender {
  [self.mutator didCloseBWGPromo];
}

@end
