// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/ui/actuation_task_button.h"

#import "base/check.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_constants.h"
#import "ios/chrome/browser/shared/ui/elements/gradient/gradient_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

using intelligence::actor::kSpacingLarge;
using intelligence::actor::kSpacingMedium;
using intelligence::actor::kSpacingSmall;
using intelligence::actor::kSpacingTiny;

// Size for the left favicon/logo container.
const CGFloat kIconContainerSize = 32.0;

// Size for the icons (both left favicon and right action symbol).
const CGFloat kIconSize = 20.0;

// Overall button corner radius.
const CGFloat kCornerRadius = 16.0;

// Returns the default gradient start color (soft translucent blue).
UIColor* DefaultGradientStartColor() {
  return [[UIColor colorNamed:kBlueColor] colorWithAlphaComponent:0.15];
}

// Returns the default gradient end color (very light translucent blue).
UIColor* DefaultGradientEndColor() {
  return [[UIColor colorNamed:kBlueColor] colorWithAlphaComponent:0.02];
}

// The duration of the touch highlight fade animation.
const NSTimeInterval kHighlightAnimationDuration = 0.1;

}  // namespace

@implementation ActuationTaskButton {
  GradientView* _gradientView;
  UIView* _iconContainerView;
  UIImageView* _leftImageView;
  UILabel* _titleLabel;
  UILabel* _subtitleLabel;
  UIImageView* _rightImageView;
  UIStackView* _mainStack;
}

- (instancetype)initWithTitle:(NSString*)title
                     subtitle:(NSString*)subtitle
                         icon:(UIImage*)icon {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    self.isAccessibilityElement = YES;
    self.accessibilityTraits = UIAccessibilityTraitButton;

    [self setupSubviews];
    [self setupConstraints];

    self.title = title;
    self.subtitle = subtitle;
    self.icon = icon;
  }
  return self;
}

- (instancetype)init {
  return [self initWithTitle:nil subtitle:nil icon:nil];
}

- (void)setTitle:(NSString*)title {
  _titleLabel.text = title;
  [self updateAccessibilityLabel];
}

- (NSString*)title {
  return _titleLabel.text;
}

- (void)setSubtitle:(NSString*)subtitle {
  _subtitleLabel.text = subtitle;
  [self updateAccessibilityLabel];
}

- (NSString*)subtitle {
  return _subtitleLabel.text;
}

- (void)setIcon:(UIImage*)icon {
  _leftImageView.image = icon;
  _iconContainerView.hidden = (icon == nil);
}

- (UIImage*)icon {
  return _leftImageView.image;
}

- (void)setBackgroundGradientStartColor:(UIColor*)startColor
                               endColor:(UIColor*)endColor {
  UIColor* start = startColor ?: DefaultGradientStartColor();
  UIColor* end = endColor ?: DefaultGradientEndColor();
  [_gradientView setStartColor:start endColor:end];
}

#pragma mark - UIControl

- (void)setHighlighted:(BOOL)highlighted {
  [super setHighlighted:highlighted];
  [UIView animateWithDuration:kHighlightAnimationDuration
                   animations:^{
                     self.alpha = highlighted ? 0.6 : 1.0;
                   }];
}

- (void)setEnabled:(BOOL)enabled {
  [super setEnabled:enabled];
  self.alpha = enabled ? 1.0 : 0.5;
}

#pragma mark - Private

// Creates and adds all internal subviews.
- (void)setupSubviews {
  _gradientView =
      [[GradientView alloc] initWithStartColor:DefaultGradientStartColor()
                                      endColor:DefaultGradientEndColor()
                                    startPoint:CGPointMake(0.0, 0.5)
                                      endPoint:CGPointMake(1.0, 0.5)
                                  gradientType:GradientLayerType::kLinear];
  _gradientView.translatesAutoresizingMaskIntoConstraints = NO;
  _gradientView.layer.cornerRadius = kCornerRadius;
  _gradientView.clipsToBounds = YES;
  _gradientView.userInteractionEnabled = NO;
  [self addSubview:_gradientView];

  _iconContainerView = [[UIView alloc] init];
  _iconContainerView.backgroundColor = [UIColor colorNamed:kSolidWhiteColor];
  _iconContainerView.layer.cornerRadius = kIconContainerSize / 2.0;
  _iconContainerView.clipsToBounds = YES;
  _iconContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  _iconContainerView.hidden = YES;

  _leftImageView = [[UIImageView alloc] init];
  _leftImageView.contentMode = UIViewContentModeScaleAspectFit;
  _leftImageView.translatesAutoresizingMaskIntoConstraints = NO;
  _leftImageView.clipsToBounds = YES;
  [_iconContainerView addSubview:_leftImageView];

  _titleLabel = [[UILabel alloc] init];
  _titleLabel.numberOfLines = 1;
  _titleLabel.font =
      CreateDynamicFont(UIFontTextStyleSubheadline, UIFontWeightBold);
  _titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;

  _subtitleLabel = [[UILabel alloc] init];
  _subtitleLabel.numberOfLines = 1;
  _subtitleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  _subtitleLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  _subtitleLabel.translatesAutoresizingMaskIntoConstraints = NO;

  UIStackView* textStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ _titleLabel, _subtitleLabel ]];
  textStack.axis = UILayoutConstraintAxisVertical;
  textStack.alignment = UIStackViewAlignmentLeading;
  textStack.spacing = kSpacingTiny;
  textStack.translatesAutoresizingMaskIntoConstraints = NO;

  _rightImageView = [[UIImageView alloc] init];
  _rightImageView.contentMode = UIViewContentModeScaleAspectFit;
  _rightImageView.tintColor = [UIColor colorNamed:kTextSecondaryColor];
  _rightImageView.image =
      SymbolTemplateWithPointSize(SymbolExternalLink, kIconSize);
  _rightImageView.translatesAutoresizingMaskIntoConstraints = NO;

  _mainStack = [[UIStackView alloc] initWithArrangedSubviews:@[
    _iconContainerView, textStack, _rightImageView
  ]];
  _mainStack.alignment = UIStackViewAlignmentCenter;
  _mainStack.spacing = kSpacingMedium;
  _mainStack.userInteractionEnabled = NO;
  _mainStack.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:_mainStack];
}

// Configures internal layout constraints.
- (void)setupConstraints {
  AddSameConstraints(_gradientView, self);

  NSDirectionalEdgeInsets insets = NSDirectionalEdgeInsetsMake(
      kSpacingSmall, kSpacingLarge, kSpacingSmall, kSpacingLarge);
  AddSameConstraintsWithInsets(_mainStack, self, insets);

  AddSquareConstraints(_iconContainerView, kIconContainerSize);
  AddSameCenterConstraints(_leftImageView, _iconContainerView);
  AddSquareConstraints(_leftImageView, kIconSize);
  AddSquareConstraints(_rightImageView, kIconSize);
}

- (void)updateAccessibilityLabel {
  NSMutableArray<NSString*>* parts = [NSMutableArray array];
  if (self.title.length > 0) {
    [parts addObject:self.title];
  }
  if (self.subtitle.length > 0) {
    [parts addObject:self.subtitle];
  }
  self.accessibilityLabel = [parts componentsJoinedByString:@", "];
}

@end
