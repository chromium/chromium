// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/feed_top_section/notifications_promo_view.h"

#import "base/apple/foundation_util.h"
#import "base/ios/ios_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/ntp/feed_top_section/notifications_promo_view_constants.h"
#import "ios/chrome/common/button_configuration_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// Horizontal spacing between stackView and cell contentView.
constexpr CGFloat kStackViewPadding = 16.0;
constexpr CGFloat kStackViewTrailingMargin = 19.0;
constexpr CGFloat kStackViewSubViewSpacing = 12.0;
// Margins for the close button.
constexpr CGFloat kCloseButtonTrailingMargin = -8.0;
constexpr CGFloat kCloseButtonTopMargin = 8.0;
// Size for the close button width and height.
constexpr CGFloat kCloseButtonWidthHeight = 24;
}  // namespace

@interface NotificationsPromoView ()

@property(nonatomic, strong) UILabel* textLabel;
@property(nonatomic, strong, readwrite) UIButton* closeButton;

// Stack View containing all internal views on the promo.
@property(nonatomic, strong) UIStackView* promoStackView;

@end

@implementation NotificationsPromoView {
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _textLabel = [self createTextLabel];
    _promoStackView = [self createPromoStackWithViewsArray:@[ _textLabel ]];
    _closeButton = [self createCloseButton];
    [self addSubview:_promoStackView];
    [self addSubview:_closeButton];
    [self activateConstraints];
  }
  return self;
}

#pragma mark - Private

- (void)activateConstraints {
  [NSLayoutConstraint activateConstraints:@[
    // Stack View Constraints.
    [self.promoStackView.leadingAnchor
        constraintEqualToAnchor:self.leadingAnchor
                       constant:kStackViewPadding],
    [self.promoStackView.topAnchor constraintEqualToAnchor:self.topAnchor
                                                  constant:kStackViewPadding],
    [self.promoStackView.bottomAnchor
        constraintEqualToAnchor:self.bottomAnchor
                       constant:-kStackViewPadding],
    [self.promoStackView.trailingAnchor
        constraintEqualToAnchor:self.trailingAnchor
                       constant:-kStackViewTrailingMargin],
    // Close button size constraints.
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

- (UIStackView*)createPromoStackWithViewsArray:(NSArray*)views {
  UIStackView* stackView = [[UIStackView alloc] initWithArrangedSubviews:views];
  stackView.alignment = UIStackViewAlignmentCenter;
  stackView.axis = UILayoutConstraintAxisVertical;
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  stackView.spacing = kStackViewSubViewSpacing;
  return stackView;
}

- (UILabel*)createTextLabel {
  UILabel* textLabel = [[UILabel alloc] init];
  textLabel.translatesAutoresizingMaskIntoConstraints = NO;
  textLabel.numberOfLines = 0;
  textLabel.textAlignment = NSTextAlignmentCenter;
  textLabel.lineBreakMode = NSLineBreakByWordWrapping;
  textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  textLabel.textColor = [UIColor colorNamed:kGrey800Color];
  textLabel.text =
      l10n_util::GetNSString(IDS_IOS_CONTENT_NOTIFICATIONS_PROMO_TEXT);
  return textLabel;
}

// Creates the close button for the Promo.
- (UIButton*)createCloseButton {
  UIButton* button = [[UIButton alloc] init];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.accessibilityIdentifier = kNotificationsPromoCloseButtonId;
  [button addTarget:self
                action:@selector(onCloseButtonAction:)
      forControlEvents:UIControlEventTouchUpInside];
  [button setImage:[UIImage imageNamed:@"signin_promo_close_gray"]
          forState:UIControlStateNormal];
  button.hidden = NO;
  button.pointerInteractionEnabled = YES;
  return button;
}

- (void)accessibilityCloseAction:(id)unused {
  DCHECK(self.closeButton.enabled);
  [self.closeButton sendActionsForControlEvents:UIControlEventTouchUpInside];
}

// Handles close button action.
- (void)onCloseButtonAction:(id)unused {
}

@end
