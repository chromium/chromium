// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/ui/assistant_aim_header_view.h"

#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// The point size of the close button.
const CGFloat kCloseButtonSymbolPointSize = 17.0;

// The leading and trailing padding of the header view.
const UIEdgeInsets kHorizontalPadding = {.left = 22.0, .right = 16.0};
const CGFloat kTitleLeadingPadding = 18.0;

// The logo point size.
const CGFloat kSymbolsPointSize = 24.0;

}  // namespace

@implementation AssistantAIMHeaderView {
  // The label representing the title of the header.
  UILabel* _titleLabel;

  // The close button.
  UIButton* _closeButton;

  // The logo.
  UIImageView* _logoView;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    [self setupLogoView];
    [self setUpTitleLabel];
    [self setUpCloseButton];
  }

  return self;
}

- (void)setTitle:(NSString*)title {
  _titleLabel.text = title;
}

- (void)adjustForPercentage:(CGFloat)percentage {
  _titleLabel.alpha = 1 - percentage;
}

#pragma mark - Private

- (void)setUpTitleLabel {
  _titleLabel = [[UILabel alloc] init];
  _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _titleLabel.adjustsFontForContentSizeCategory = YES;
  _titleLabel.lineBreakMode = NSLineBreakByTruncatingTail;
  _titleLabel.font =
      PreferredFontForTextStyle(UIFontTextStyleHeadline, UIFontWeightSemibold);
  _titleLabel.isAccessibilityElement = YES;
  _titleLabel.adjustsFontSizeToFitWidth = YES;
  [self addSubview:_titleLabel];

  [NSLayoutConstraint activateConstraints:@[
    [_titleLabel.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
    [_titleLabel.leadingAnchor constraintEqualToAnchor:_logoView.trailingAnchor
                                              constant:kTitleLeadingPadding],
  ]];
}

- (void)setUpCloseButton {
  _closeButton = [[UIButton alloc] init];
  _closeButton.translatesAutoresizingMaskIntoConstraints = NO;

  UIButtonConfiguration* buttonConfiguration;
  if (@available(iOS 26, *)) {
    if ([UIButtonConfiguration
            respondsToSelector:@selector(prominentGlassButtonConfiguration)]) {
      buttonConfiguration =
          [UIButtonConfiguration prominentGlassButtonConfiguration];
    } else {
      buttonConfiguration = [UIButtonConfiguration glassButtonConfiguration];
    }
  } else {
    buttonConfiguration = [UIButtonConfiguration plainButtonConfiguration];
  }

  buttonConfiguration.image = DefaultSymbolTemplateWithPointSize(
      kXMarkSymbol, kCloseButtonSymbolPointSize);
  buttonConfiguration.baseForegroundColor =
      [UIColor colorNamed:kTextPrimaryColor];
  buttonConfiguration.background.backgroundColor =
      [UIColor colorNamed:kPrimaryBackgroundColor];
  buttonConfiguration.cornerStyle = UIButtonConfigurationCornerStyleCapsule;

  _closeButton =
      [ExtendedTouchTargetButton buttonWithConfiguration:buttonConfiguration
                                           primaryAction:nil];
  [_closeButton addTarget:self
                   action:@selector(didTapCloseButton)
         forControlEvents:UIControlEventTouchUpInside];
  _closeButton.translatesAutoresizingMaskIntoConstraints = NO;

  [self addSubview:_closeButton];

  [NSLayoutConstraint activateConstraints:@[
    [_closeButton.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
    [_closeButton.trailingAnchor
        constraintEqualToAnchor:self.trailingAnchor
                       constant:-kHorizontalPadding.right],
  ]];
}

- (void)setupLogoView {
  _logoView = [[UIImageView alloc] initWithImage:[self iconImage]];
  _logoView.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:_logoView];
  [NSLayoutConstraint activateConstraints:@[
    [_logoView.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
    [_logoView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor
                                            constant:kHorizontalPadding.left],
  ]];
}

- (UIImage*)iconImage {
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
  return MakeSymbolMulticolor(
      CustomSymbolWithPointSize(kGoogleIconSymbol, kSymbolsPointSize));
#else
  return MakeSymbolMulticolor(
      DefaultSymbolWithPointSize(kGearshape2Symbol, kSymbolsPointSize));
#endif
}

#pragma mark - Actions

- (void)didTapCloseButton {
  [self.delegate assistantAIMHeaderViewDidPressClose:self];
}

@end
