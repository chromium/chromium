// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/promo/ui/default_browser_passive_promo_card_view.h"

#import "ios/chrome/browser/default_browser/promo/ui/default_browser_passive_card_view_delegate.h"
#import "ios/chrome/browser/shared/ui/buildflags.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/chrome_button.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// Constants for the layout of the view.
const CGFloat kContainerInnerPadding = 15.0;
const CGFloat kStackViewSpacing = 8.0;
const CGFloat kImageMaxWidth = 130.0;
const CGFloat kIllustratedImageToTitleSpacing = 8.0;

// Constants for the close button.
const CGFloat kCloseButtonTopMargin = 16.0;
const CGFloat kCloseButtonTrailingMargin = 15.0;
const CGFloat kCloseButtonSize = 44.0;
const CGFloat kCloseButtonSymbolPointSize = 14.0;

// Constants for the description to action button padding.
const CGFloat kDescriptionToActionButtonSpacing = 16.0;

#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
NSString* const kPassivePromoImageName =
    @"default_browser_passive_promo_settings";
#else
NSString* const kPassivePromoImageName =
    @"default_browser_passive_promo_settings_chromium";
#endif
}  // namespace

@interface DefaultBrowserPassivePromoCardView ()
@end

@implementation DefaultBrowserPassivePromoCardView {
  UIButton* _closeButton;
  ChromeButton* _actionButton;
  UIImageView* _illustratedImageView;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.translatesAutoresizingMaskIntoConstraints = NO;
    self.backgroundColor = [UIColor clearColor];

    UIStackView* stackView = [self createStackView];
    [self addSubview:stackView];

    _closeButton = [self createCloseButton];
    [_closeButton addTarget:self
                     action:@selector(closeButtonTapped)
           forControlEvents:UIControlEventTouchUpInside];
    [self addSubview:_closeButton];

    _illustratedImageView = [self createImageView];
    [stackView addArrangedSubview:_illustratedImageView];
    [stackView setCustomSpacing:kIllustratedImageToTitleSpacing
                      afterView:_illustratedImageView];

    UILabel* titleLabel = [self createLabelWithTextStyle:UIFontTextStyleBody
                                           textColorName:kTextPrimaryColor];
    titleLabel.text = l10n_util::GetNSString(
        IDS_IOS_SETTINGS_DEFAULT_BROWSER_PASSIVE_CARD_TITLE);
    [stackView addArrangedSubview:titleLabel];

    UILabel* descriptionLabel =
        [self createLabelWithTextStyle:UIFontTextStyleFootnote
                         textColorName:kTextSecondaryColor];
    descriptionLabel.text = l10n_util::GetNSString(
        IDS_IOS_SETTINGS_DEFAULT_BROWSER_PASSIVE_CARD_SUBTITLE);
    [stackView addArrangedSubview:descriptionLabel];
    [stackView setCustomSpacing:kDescriptionToActionButtonSpacing
                      afterView:descriptionLabel];

    _actionButton = [self createActionButton];
    [_actionButton addTarget:self
                      action:@selector(actionButtonTapped)
            forControlEvents:UIControlEventTouchUpInside];
    [stackView addArrangedSubview:_actionButton];

    [self setupConstraintsWithStackView:stackView
                              imageView:_illustratedImageView];
  }
  return self;
}

#pragma mark - Private

// Handles close button tap.
- (void)closeButtonTapped {
  [self.delegate didTapCloseInDefaultBrowserPassivePromoCardView:self];
}

// Handles primary action button tap.
- (void)actionButtonTapped {
  [self.delegate didTapActionInDefaultBrowserPassivePromoCardView:self];
}

// Creates the stack view for the card.
- (UIStackView*)createStackView {
  UIStackView* stackView = [[UIStackView alloc] init];
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  stackView.axis = UILayoutConstraintAxisVertical;
  stackView.alignment = UIStackViewAlignmentCenter;
  stackView.spacing = kStackViewSpacing;
  return stackView;
}

// Creates a label with the given text style and text color name.
- (UILabel*)createLabelWithTextStyle:(UIFontTextStyle)textStyle
                       textColorName:(NSString*)textColorName {
  UILabel* label = [[UILabel alloc] init];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.font = [UIFont preferredFontForTextStyle:textStyle];
  label.textColor = [UIColor colorNamed:textColorName];
  label.textAlignment = NSTextAlignmentCenter;
  label.numberOfLines = 0;
  label.adjustsFontForContentSizeCategory = YES;
  return label;
}

// Creates a close button for the card.
- (UIButton*)createCloseButton {
  UIButton* closeButton = [UIButton buttonWithType:UIButtonTypeSystem];
  closeButton.translatesAutoresizingMaskIntoConstraints = NO;
  closeButton.tintColor = [UIColor colorNamed:kTextSecondaryColor];
  closeButton.accessibilityLabel = l10n_util::GetNSString(
      IDS_IOS_SETTINGS_CLOSE_DEFAULT_BROWSER_PROMO_ACCESSIBILITY_LABEL);
  closeButton.accessibilityTraits = UIAccessibilityTraitButton;
  closeButton.isAccessibilityElement = YES;
  UIImageSymbolConfiguration* symbolConfig = [UIImageSymbolConfiguration
      configurationWithPointSize:kCloseButtonSymbolPointSize
                          weight:UIImageSymbolWeightMedium];
  [closeButton setImage:SymbolWithConfiguration(SymbolXMark, symbolConfig)
               forState:UIControlStateNormal];
  return closeButton;
}

// Creates an image view for the card.
- (UIImageView*)createImageView {
  UIImageView* imageView = [[UIImageView alloc] init];
  imageView.translatesAutoresizingMaskIntoConstraints = NO;
  imageView.contentMode = UIViewContentModeScaleAspectFit;
  imageView.image = [UIImage imageNamed:kPassivePromoImageName];
  return imageView;
}

// Creates the primary action button for the card.
- (ChromeButton*)createActionButton {
  ChromeButton* button =
      [[ChromeButton alloc] initWithStyle:ChromeButtonStylePrimary];
  button.buttonSize = ChromeButtonSizeSmall;
  button.title = l10n_util::GetNSString(
      IDS_IOS_SETTINGS_DEFAULT_BROWSER_PASSIVE_CARD_BUTTON_TITLE);
  return button;
}

// Sets up the constraints for the card's subviews.
- (void)setupConstraintsWithStackView:(UIStackView*)stackView
                            imageView:(UIImageView*)imageView {
  AddSameConstraintsToSidesWithInsets(
      stackView, self,
      LayoutSides::kTop | LayoutSides::kLeading | LayoutSides::kBottom |
          LayoutSides::kTrailing,
      NSDirectionalEdgeInsets{kContainerInnerPadding, kContainerInnerPadding,
                              kContainerInnerPadding, kContainerInnerPadding});

  AddSizeConstraints(_closeButton,
                     CGSizeMake(kCloseButtonSize, kCloseButtonSize));
  AddSameConstraintsToSidesWithInsets(
      _closeButton, self, LayoutSides::kTop | LayoutSides::kTrailing,
      NSDirectionalEdgeInsets{kCloseButtonTopMargin, 0.0, 0.0,
                              kCloseButtonTrailingMargin});

  NSLayoutConstraint* relativeWidthConstraint =
      [imageView.widthAnchor constraintEqualToAnchor:stackView.widthAnchor
                                          multiplier:0.4];
  relativeWidthConstraint.priority = UILayoutPriorityDefaultHigh;
  relativeWidthConstraint.active = YES;

  [imageView.widthAnchor constraintLessThanOrEqualToConstant:kImageMaxWidth]
      .active = YES;
  [imageView.heightAnchor constraintEqualToAnchor:imageView.widthAnchor]
      .active = YES;
}

@end
