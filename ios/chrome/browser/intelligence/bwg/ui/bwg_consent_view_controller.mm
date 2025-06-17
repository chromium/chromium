// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/bwg_consent_view_controller.h"

#import "ios/chrome/browser/intelligence/bwg/ui/bwg_consent_mutator.h"
#import "ios/chrome/browser/intelligence/bwg/ui/bwg_constants.h"
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
const CGFloat kMainStackSpacing = 16.0;

// Icons size and names.
const CGFloat kIconSize = 16.0;
// TODO(crbug.com/414777888): Change info circle fill icon to page spark icon.
constexpr NSString* const kInfoIconName = @"info.circle.fill";
const CGFloat kIconImageViewTopPadding = 18.0;
const CGFloat kIconImageViewWidth = 32.0;

// Boxes stack view traits.
const CGFloat kBoxesStackViewSpacing = 2.0;
const CGFloat kBoxesStackViewCornerRadius = 16.0;

// Inner stack view spacing and padding.
const CGFloat kInnerStackViewSpacing = 6.0;
const CGFloat kInnerStackViewPadding = 12.0;

// TODO(crbug.com/414778685): Add strings.
// String constants for UI elements.
NSString* const kBWGConsentFirstBoxTitleText =
    @"Lorem ipsum dolor sit amet, consecte tur adipiscing elit.";
NSString* const kBWGConsentFirstBoxBodyText =
    @"Sed do eiusmod tempor incididunt. Sed do eiusmod tempor incididunt. Sed "
    @"do eiusmod tempor incididunt";
NSString* const kBWGConsentSecondBoxTitleText = @"Lorem ipsum dolor sit amet";
NSString* const kBWGConsentSecondBoxBodyText =
    @"Lorem ipsum dolor sit amet, consecte tur adipiscing purposes. Sed do "
    @"eiusmod tempor incididunt ut labore et dolore magna ali. eiusmod tempor ";
NSString* const kBWGConsentFootnoteText =
    @"Google terms dolor sit amet, Apps Privacy Notice tur adipiscing "
    @"purposes.";

// Action identifier on a tap on links in the footnote.
NSString* const kFootnoteLinkAction = @"footnoteLinkAction";

}  // namespace

@interface BWGConsentViewController () <UITextViewDelegate>
@end

@implementation BWGConsentViewController {
  // Main stack view containing all the others views.
  UIStackView* _mainStackView;
}

#pragma mark - UIViewController

// TODO(crbug.com/414777915): Implement a basic UI.
- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.navigationItem.hidesBackButton = YES;
  [self configureMainStackView];
}

#pragma mark - Public

- (CGFloat)contentHeight {
  return
      [_mainStackView systemLayoutSizeFittingSize:UILayoutFittingCompressedSize]
          .height;
}

#pragma mark - Private

// Creates an attributed string for the footnote with hyperlinks.
- (NSAttributedString*)createFootnoteAttributedText {
  NSString* text = kBWGConsentFootnoteText;

  NSMutableParagraphStyle* centeredTextStyle =
      [[NSMutableParagraphStyle alloc] init];
  centeredTextStyle.alignment = NSTextAlignmentCenter;
  NSDictionary* textAttributes = @{
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleCaption2],
    NSParagraphStyleAttributeName : centeredTextStyle,
  };

  NSMutableAttributedString* attributedText =
      [[NSMutableAttributedString alloc] initWithString:text
                                             attributes:textAttributes];

  NSDictionary* linkAttributes = @{
    NSLinkAttributeName : kFootnoteLinkAction,
  };

  // TODO(crbug.com/414778685): Add strings.
  NSRange firstLinkRange = [text rangeOfString:@"Google terms"];
  [attributedText addAttributes:linkAttributes range:firstLinkRange];
  NSRange secondLinkRange = [text rangeOfString:@"Apps Privacy Notice"];
  [attributedText addAttributes:linkAttributes range:secondLinkRange];
  return attributedText;
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
  [_mainStackView addArrangedSubview:[self createBoxesStackView]];
  [_mainStackView addArrangedSubview:[self createFootnoteView]];
  UIView* primaryButtonView = [self createPrimaryButton];
  [_mainStackView addArrangedSubview:primaryButtonView];
  [_mainStackView setCustomSpacing:0.0 afterView:primaryButtonView];
  [_mainStackView addArrangedSubview:[self createSecondaryButton]];
}

// Creates the 2 horizontal boxes stack view.
- (UIStackView*)createBoxesStackView {
  UIStackView* boxesStackView = [[UIStackView alloc] init];
  boxesStackView.axis = UILayoutConstraintAxisVertical;
  boxesStackView.distribution = UIStackViewDistributionFillProportionally;
  boxesStackView.spacing = kBoxesStackViewSpacing;
  boxesStackView.layer.cornerRadius = kBoxesStackViewCornerRadius;
  boxesStackView.clipsToBounds = YES;
  boxesStackView.translatesAutoresizingMaskIntoConstraints = NO;

  NSString* firstTitle = kBWGConsentFirstBoxTitleText;
  NSString* firstBody = kBWGConsentFirstBoxBodyText;
  UIView* firstBox =
      [self createHorizontalBoxWithIcon:kInfoIconName
                                boxView:[self createBoxWithTitle:firstTitle
                                                        bodyText:firstBody]];
  [boxesStackView addArrangedSubview:firstBox];

  NSString* secondTitle = kBWGConsentSecondBoxTitleText;
  NSString* secondBody = kBWGConsentSecondBoxBodyText;
  UIView* secondBox =
      [self createHorizontalBoxWithIcon:kCounterCloseWiseSymbol
                                boxView:[self createBoxWithTitle:secondTitle
                                                        bodyText:secondBody]];
  [boxesStackView addArrangedSubview:secondBox];
  return boxesStackView;
}

// Creates horizontal stack view with icon and box view.
- (UIView*)createHorizontalBoxWithIcon:(NSString*)iconName
                               boxView:(UIView*)boxView {
  UIStackView* horizontalStackView = [[UIStackView alloc] init];
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

// Creates the bow view containing the text and the title.
- (UIView*)createBoxWithTitle:(NSString*)titleText
                     bodyText:(NSString*)bodyText {
  UIView* boxView = [[UIView alloc] init];
  boxView.translatesAutoresizingMaskIntoConstraints = NO;

  UIStackView* innerStackView = [[UIStackView alloc] init];
  innerStackView.axis = UILayoutConstraintAxisVertical;
  innerStackView.alignment = UIStackViewAlignmentLeading;
  innerStackView.spacing = kInnerStackViewSpacing;

  innerStackView.translatesAutoresizingMaskIntoConstraints = NO;
  [boxView addSubview:innerStackView];

  CGFloat innerPadding = kInnerStackViewPadding;
  AddSameConstraintsWithInsets(
      innerStackView, boxView,
      NSDirectionalEdgeInsetsMake(innerPadding, 0, innerPadding, innerPadding));

  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.text = titleText;
  titleLabel.font =
      PreferredFontForTextStyle(UIFontTextStyleHeadline, UIFontWeightSemibold);

  titleLabel.adjustsFontForContentSizeCategory = YES;
  titleLabel.numberOfLines = 0;
  [innerStackView addArrangedSubview:titleLabel];

  UILabel* bodyLabel = [[UILabel alloc] init];
  bodyLabel.text = bodyText;
  bodyLabel.font = PreferredFontForTextStyle(UIFontTextStyleBody);
  bodyLabel.adjustsFontForContentSizeCategory = YES;
  bodyLabel.numberOfLines = 0;
  bodyLabel.textColor = [UIColor colorNamed:kGrey700Color];
  [innerStackView addArrangedSubview:bodyLabel];

  return boxView;
}

// Creates the foot note view.
- (UITextView*)createFootnoteView {
  UITextView* footNoteTextView = [[UITextView alloc] init];
  footNoteTextView.scrollEnabled = NO;
  footNoteTextView.editable = NO;

  footNoteTextView.delegate = self;
  footNoteTextView.textContainerInset = UIEdgeInsetsZero;
  footNoteTextView.linkTextAttributes =
      @{NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor]};
  footNoteTextView.attributedText = [self createFootnoteAttributedText];
  return footNoteTextView;
}

// Creates the primary button.
- (UIButton*)createPrimaryButton {
  UIButton* primaryButton = [BWGUIUtils
      createPrimaryButtonWithTitle:l10n_util::GetNSString(
                                       IDS_IOS_BWG_CONSENT_PRIMARY_BUTTON)];
  [primaryButton addTarget:self
                    action:@selector(didTapPrimaryButton:)
          forControlEvents:UIControlEventTouchUpInside];
  primaryButton.accessibilityLabel = @"Consent Primary Action";
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
  // TODO(crbug.com/420643840): Add a11y labels.
  return secondaryButton;
}

// Did tap the primary button.
- (void)didTapPrimaryButton:(UIButton*)sender {
  [self.mutator didConsentBWG];
}

// Did tap the secondary button.
- (void)didTapSecondaryButton:(UIButton*)sender {
  [self.mutator didConsentBWG];
}

#pragma mark - UITextViewDelegate

// Handles tap on UITextView.
- (UIAction*)textView:(UITextView*)textView
    primaryActionForTextItem:(UITextItem*)textItem
               defaultAction:(UIAction*)defaultAction {
  if (textItem.link &&
      [textItem.link.absoluteString isEqualToString:kFootnoteLinkAction]) {
    __weak __typeof(self) weakSelf = self;
    return [UIAction actionWithHandler:^(UIAction* action) {
      [weakSelf.mutator handleLearnAboutYourChoicesTapped];
    }];
  }

  return defaultAction;
}

@end
