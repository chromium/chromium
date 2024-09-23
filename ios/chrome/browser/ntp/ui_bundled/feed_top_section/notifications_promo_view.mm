// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/feed_top_section/notifications_promo_view.h"

#import "base/apple/foundation_util.h"
#import "base/ios/ios_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ntp/ui_bundled/feed_top_section/feed_top_section_mutator.h"
#import "ios/chrome/browser/ntp/ui_bundled/feed_top_section/notifications_promo_view_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// Horizontal spacing between stackView and cell contentView.
constexpr CGFloat kStackViewPadding = 16.0;
constexpr CGFloat kStackViewTopPadding = 16.0;
constexpr CGFloat kStackViewTrailingMargin = 19.5;
constexpr CGFloat kStackViewSubViewSpacing = 16.0;
// Margins for the close button.
constexpr CGFloat kCloseButtonTrailingMargin = -14.0;
constexpr CGFloat kCloseButtonTopMargin = 14.0;
// Size for the close button width and height.
constexpr CGFloat kCloseButtonWidthHeight = 16;
// Main button corner radius.
constexpr CGFloat kButtonCornerRadius = 8.0;
constexpr CGFloat kButtonTitleHorizontalContentInset = 42.0;
constexpr CGFloat kButtonTitleVerticalContentInset = 9.0;
// Spacing between the buttons.
constexpr CGFloat kButtonStackViewSubViewSpacing = 8.0;
// Buttons and text stack view spacing.
constexpr CGFloat kTextAndButtonStackViewSubViewSpacing = 12.0;
// Main image size.
constexpr CGSize kMainImageSize = {56.0, 56.0};
}  // namespace

@interface NotificationsPromoView ()

@property(nonatomic, strong) UIImageView* imageView;
@property(nonatomic, strong) UILabel* textLabel;
@property(nonatomic, strong) UIButton* primaryButton;
@property(nonatomic, strong) UIButton* secondaryButton;
@property(nonatomic, strong) UIButton* closeButton;

// Stack View containing all internal views on the promo.
@property(nonatomic, strong) UIStackView* buttonStackView;
@property(nonatomic, strong) UIStackView* textAndButtonStackView;
@property(nonatomic, strong) UIStackView* promoStackView;

@end

@implementation NotificationsPromoView {
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _textLabel = [self createTextLabel];
    _primaryButton = [self
        createButtonOfType:NotificationsPromoButtonTypePrimary
                  withText:
                      l10n_util::GetNSString(
                          IDS_IOS_CONTENT_NOTIFICATIONS_PROMO_PRIMARY_BUTTON)];
    _secondaryButton = [self
        createButtonOfType:NotificationsPromoButtonTypeSecondary
                  withText:
                      l10n_util::GetNSString(
                          IDS_IOS_CONTENT_NOTIFICATIONS_PROMO_SECONDARY_BUTTON)];
    _buttonStackView =
        [self createStackViewFromViewArray:@[ _primaryButton, _secondaryButton ]
                               withSpacing:kButtonStackViewSubViewSpacing];
    _textAndButtonStackView = [self
        createStackViewFromViewArray:@[ _textLabel, _buttonStackView ]
                         withSpacing:kTextAndButtonStackViewSubViewSpacing];
    _imageView = [self
        createImageViewWithImage:[UIImage
                                     imageNamed:@"notifications_promo_icon"]
                          ofSize:kMainImageSize];
    _promoStackView = [self
        createStackViewFromViewArray:@[ _imageView, _textAndButtonStackView ]
                         withSpacing:kStackViewSubViewSpacing];
    _closeButton = [self createButtonOfType:NotificationsPromoButtonTypeClose
                                   withText:nil];
    [self addSubview:_promoStackView];
    [self addSubview:_closeButton];
    [self activateMainStackViewConstraints];
  }
  return self;
}

#pragma mark - Private

- (void)activateMainStackViewConstraints {
  [NSLayoutConstraint activateConstraints:@[
    // Stack View Constraints.
    [self.promoStackView.leadingAnchor
        constraintEqualToAnchor:self.leadingAnchor
                       constant:kStackViewTrailingMargin],
    [self.promoStackView.topAnchor
        constraintEqualToAnchor:self.topAnchor
                       constant:kStackViewTopPadding],
    [self.promoStackView.bottomAnchor
        constraintEqualToAnchor:self.bottomAnchor
                       constant:-kStackViewPadding],
    [self.promoStackView.trailingAnchor
        constraintEqualToAnchor:self.trailingAnchor
                       constant:-kStackViewTrailingMargin],
    [self.closeButton.heightAnchor
        constraintEqualToConstant:kCloseButtonWidthHeight],
    [self.closeButton.widthAnchor
        constraintEqualToConstant:kCloseButtonWidthHeight],
    [self.closeButton.trailingAnchor
        constraintEqualToAnchor:self.trailingAnchor
                       constant:kCloseButtonTrailingMargin],
    [self.closeButton.topAnchor constraintEqualToAnchor:self.topAnchor
                                               constant:kCloseButtonTopMargin]
  ]];
}

- (UIImageView*)createImageViewWithImage:(UIImage*)image ofSize:(CGSize)size {
  UIImageView* imageView = [[UIImageView alloc] init];
  imageView.image = image;
  [NSLayoutConstraint activateConstraints:@[
    [imageView.heightAnchor constraintEqualToConstant:size.height],
    [imageView.widthAnchor constraintEqualToConstant:size.width]
  ]];
  imageView.translatesAutoresizingMaskIntoConstraints = NO;
  imageView.contentMode = UIViewContentModeScaleAspectFit;
  return imageView;
}

- (UIStackView*)createStackViewFromViewArray:(NSArray*)views
                                 withSpacing:(CGFloat)spacing {
  UIStackView* stackView = [[UIStackView alloc] initWithArrangedSubviews:views];
  stackView.alignment = UIStackViewAlignmentCenter;
  stackView.axis = UILayoutConstraintAxisVertical;
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  stackView.spacing = spacing;
  return stackView;
}

- (UIButton*)createButtonOfType:(NotificationsPromoButtonType)type
                       withText:(NSString*)text {
  UIButton* button = [[UIButton alloc] init];
  UIFont* font;
  button.pointerInteractionEnabled = YES;
  button.translatesAutoresizingMaskIntoConstraints = NO;
  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];

  switch (type) {
    case NotificationsPromoButtonTypePrimary: {
      buttonConfiguration.titleLineBreakMode = NSLineBreakByTruncatingTail;
      button.accessibilityIdentifier = kNotificationsPromoPrimaryButtonId;
      [button addTarget:self
                    action:@selector(onPrimaryButtonAction:)
          forControlEvents:UIControlEventTouchUpInside];
      button.pointerStyleProvider = CreateOpaqueButtonPointerStyleProvider();

      button.backgroundColor = [UIColor colorNamed:kBackgroundColor];
      // Button layout and constraints.
      button.layer.cornerRadius = kButtonCornerRadius;
      button.clipsToBounds = YES;
      buttonConfiguration.baseForegroundColor = [UIColor colorNamed:kBlueColor];
      buttonConfiguration.contentInsets = NSDirectionalEdgeInsetsMake(
          kButtonTitleVerticalContentInset, kButtonTitleHorizontalContentInset,
          kButtonTitleVerticalContentInset, kButtonTitleHorizontalContentInset);
      // Button text.
      font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
      NSAttributedString* attributedTitle = [[NSAttributedString alloc]
          initWithString:text
              attributes:@{NSFontAttributeName : font}];
      buttonConfiguration.attributedTitle = attributedTitle;
      button.configuration = buttonConfiguration;
      break;
    }
    case NotificationsPromoButtonTypeSecondary: {
      [button setTitleColor:[UIColor colorNamed:kBlueColor]
                   forState:UIControlStateNormal];
      button.accessibilityIdentifier = kNotificationsPromoSecondaryButtonId;
      [button addTarget:self
                    action:@selector(onSecondaryButtonAction:)
          forControlEvents:UIControlEventTouchUpInside];
      font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
      NSAttributedString* attributedTitle = [[NSAttributedString alloc]
          initWithString:text
              attributes:@{NSFontAttributeName : font}];
      buttonConfiguration.attributedTitle = attributedTitle;
      button.configuration = buttonConfiguration;
      break;
    }
    case NotificationsPromoButtonTypeClose: {
      button.accessibilityIdentifier = kNotificationsPromoCloseButtonId;
      [button addTarget:self
                    action:@selector(onCloseButtonAction:)
          forControlEvents:UIControlEventTouchUpInside];
      UIImageSymbolConfiguration* config = [UIImageSymbolConfiguration
          configurationWithPointSize:kCloseButtonWidthHeight
                              weight:UIImageSymbolWeightSemibold];
      UIImage* closeButtonImage =
          DefaultSymbolWithConfiguration(@"xmark", config);
      [button setImage:closeButtonImage forState:UIControlStateNormal];
      button.tintColor = [UIColor colorNamed:kTextTertiaryColor];
      break;
    }
  }
  return button;
}

- (UILabel*)createTextLabel {
  UILabel* textLabel = [[UILabel alloc] init];
  textLabel.translatesAutoresizingMaskIntoConstraints = NO;
  textLabel.numberOfLines = 0;
  textLabel.textAlignment = NSTextAlignmentCenter;
  textLabel.lineBreakMode = NSLineBreakByWordWrapping;
  textLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  textLabel.textColor = [UIColor colorNamed:kGrey800Color];
  textLabel.text =
      l10n_util::GetNSString(IDS_IOS_CONTENT_NOTIFICATIONS_PROMO_TEXT);
  return textLabel;
}

- (void)accessibilityCloseAction:(id)unused {
  DCHECK(self.closeButton.enabled);
  [self.closeButton sendActionsForControlEvents:UIControlEventTouchUpInside];
}

// Handles the primary button action.
- (void)onPrimaryButtonAction:(id)unused {
  [self.mutator notificationsPromoViewMainButtonWasTapped];
}

// Handles the secondary button action.
- (void)onSecondaryButtonAction:(id)unused {
  [self.mutator notificationsPromoViewDismissedFromButton:
                    NotificationsPromoButtonTypeSecondary];
}

// Handles close button action.
- (void)onCloseButtonAction:(id)unused {
  [self.mutator notificationsPromoViewDismissedFromButton:
                    NotificationsPromoButtonTypeClose];
}

@end
