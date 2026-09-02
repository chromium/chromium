// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/ui/actuation_header_view.h"

#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_constants.h"
#import "ios/chrome/browser/shared/ui/buildflags.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

using intelligence::actor::kSpacingLarge;
using intelligence::actor::kSpacingMedium;
using intelligence::actor::kSpacingSmall;
using intelligence::actor::kSpacingTiny;

// Layout dimensions.
const CGFloat kInnerContentSize = 32.0;
const CGFloat kLogoSize = 24.0;
const CGFloat kHeaderMinHeight = 44.0;

// Shadow styling.
const CGFloat kButtonShadowRadius = 6.0;
const CGFloat kButtonShadowOpacity = 0.16f;
const CGFloat kButtonShadowOffset = 2.0;

// TODO(crbug.com/552996431): Centralize Gemini logo in a shared symbol helper.
UIImage* DefaultGeminiLogo() {
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
  return SymbolWithPointSize(SymbolGeminiBrandedLogo, kLogoSize);
#else
  return SymbolWithPointSize(SymbolGeminiNonBrandedLogo, kLogoSize);
#endif
}

}  // namespace

@implementation ActuationHeaderView {
  UIImageView* _imageView;

  UILabel* _titleLabel;
  UILabel* _subtitleLabel;
  UIStackView* _textStackView;

  UIStackView* _accessoryStackView;
  UIStackView* _contentStackView;
}

#pragma mark - Public

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _actuating = NO;

    [self setupSubviews];
    [self setupConstraints];
  }
  return self;
}

- (void)reset {
  self.title = nil;
  self.subtitle = nil;
  self.actuating = NO;
  self.primaryAccessoryButton = nil;
  self.secondaryAccessoryButton = nil;
}

// TODO(crbug.com/552512657): Add helper for textual capsule/pill buttons.
+ (UIButton*)createCircularIconButtonWithIcon:(UIImage*)icon
                                       action:(UIAction*)action {
  UIButtonConfiguration* buttonConfig =
      [UIButtonConfiguration filledButtonConfiguration];
  buttonConfig.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
  buttonConfig.image = icon;
  buttonConfig.baseForegroundColor = [UIColor colorNamed:kTextPrimaryColor];
  buttonConfig.baseBackgroundColor = [UIColor colorNamed:kSolidWhiteColor];

  UIButton* button = [UIButton buttonWithConfiguration:buttonConfig
                                         primaryAction:action];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.layer.shadowColor = [UIColor blackColor].CGColor;
  button.layer.shadowOffset = CGSizeMake(0, kButtonShadowOffset);
  button.layer.shadowOpacity = kButtonShadowOpacity;
  button.layer.shadowRadius = kButtonShadowRadius;
  button.layer.shadowPath =
      [UIBezierPath bezierPathWithOvalInRect:CGRectMake(0, 0, kInnerContentSize,
                                                        kInnerContentSize)]
          .CGPath;
  AddSquareConstraints(button, kInnerContentSize);
  return button;
}

- (void)setTitle:(NSString*)title {
  _title = [title copy];
  _titleLabel.text = _title;
  _titleLabel.hidden = (_title.length == 0);
  // TODO(crbug.com/552512657): Configure accessibility properties and labels.
}

- (void)setSubtitle:(NSString*)subtitle {
  _subtitle = [subtitle copy];
  _subtitleLabel.text = _subtitle;
  _subtitleLabel.hidden = (_subtitle.length == 0);
  // TODO(crbug.com/552512657): Add support for layout progress and
  // interpolation of subtitle visibility during detent changes.
}

- (void)setActuating:(BOOL)actuating {
  if (_actuating == actuating) {
    return;
  }
  _actuating = actuating;
  // TODO(crbug.com/552512657): Add animated spinner layer around the Gemini
  // logo when actuating.
}

- (void)setPrimaryAccessoryButton:(UIButton*)primaryAccessoryButton {
  _primaryAccessoryButton = primaryAccessoryButton;
  [self updateAccessoryStack];
}

- (void)setSecondaryAccessoryButton:(UIButton*)secondaryAccessoryButton {
  _secondaryAccessoryButton = secondaryAccessoryButton;
  [self updateAccessoryStack];
}

#pragma mark - Private

// Creates and configures the subviews including the root horizontal stack.
- (void)setupSubviews {
  _imageView = [[UIImageView alloc] initWithImage:DefaultGeminiLogo()];
  _imageView.contentMode = UIViewContentModeScaleAspectFit;
  [_imageView setContentHuggingPriority:UILayoutPriorityRequired
                                forAxis:UILayoutConstraintAxisHorizontal];
  [_imageView
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];

  _titleLabel = [[UILabel alloc] init];
  _titleLabel.font =
      CreateDynamicFont(UIFontTextStyleSubheadline, UIFontWeightBold);
  _titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];

  _subtitleLabel = [[UILabel alloc] init];
  _subtitleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  _subtitleLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  _subtitleLabel.hidden = YES;

  _textStackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ _titleLabel, _subtitleLabel ]];
  _textStackView.axis = UILayoutConstraintAxisVertical;
  _textStackView.alignment = UIStackViewAlignmentLeading;
  _textStackView.spacing = kSpacingTiny;
  [_textStackView
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisHorizontal];

  _accessoryStackView = [[UIStackView alloc] init];
  _accessoryStackView.alignment = UIStackViewAlignmentCenter;
  _accessoryStackView.spacing = kSpacingSmall;
  [_accessoryStackView
      setContentHuggingPriority:UILayoutPriorityRequired
                        forAxis:UILayoutConstraintAxisHorizontal];
  [_accessoryStackView
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];
  _accessoryStackView.hidden = YES;

  _contentStackView = [[UIStackView alloc] initWithArrangedSubviews:@[
    _imageView, _textStackView, _accessoryStackView
  ]];
  _contentStackView.axis = UILayoutConstraintAxisHorizontal;
  _contentStackView.alignment = UIStackViewAlignmentCenter;
  _contentStackView.spacing = kSpacingMedium;
  [_contentStackView setCustomSpacing:kSpacingSmall afterView:_textStackView];
  _contentStackView.directionalLayoutMargins = NSDirectionalEdgeInsetsMake(
      kSpacingSmall, kSpacingLarge, kSpacingSmall, kSpacingLarge);
  _contentStackView.layoutMarginsRelativeArrangement = YES;
  _contentStackView.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:_contentStackView];
}

// Configures layout constraints.
- (void)setupConstraints {
  AddSameConstraints(_contentStackView, self);
  [self.heightAnchor constraintGreaterThanOrEqualToConstant:kHeaderMinHeight]
      .active = YES;
  AddSquareConstraints(_imageView, kLogoSize);
}

// Rebuilds the accessory buttons stack in deterministic order:
// `[secondaryAccessoryButton (leading), primaryAccessoryButton (trailing)]`.
- (void)updateAccessoryStack {
  for (UIView* view in _accessoryStackView.arrangedSubviews) {
    [view removeFromSuperview];
  }

  if (_secondaryAccessoryButton) {
    [_accessoryStackView addArrangedSubview:_secondaryAccessoryButton];
  }
  if (_primaryAccessoryButton) {
    [_accessoryStackView addArrangedSubview:_primaryAccessoryButton];
  }
  _accessoryStackView.hidden =
      (_accessoryStackView.arrangedSubviews.count == 0);
}

@end
