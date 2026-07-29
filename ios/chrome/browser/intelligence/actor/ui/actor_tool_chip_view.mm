// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/ui/actor_tool_chip_view.h"

#import <QuartzCore/QuartzCore.h>

#import "base/check.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

using intelligence::actor::kSpacingMedium;
using intelligence::actor::kSpacingSmall;

// Chip-specific custom layout overrides.
const CGFloat kChipIconSize = 18.0;

}  // namespace

@implementation ActorToolChipView {
  UIImageView* _iconView;
  UILabel* _label;
  UIStackView* _stackView;
}

- (instancetype)initWithText:(NSString*)text icon:(UIImage*)icon {
  DCHECK(text);
  self = [super initWithFrame:CGRectZero];
  if (self) {
    self.backgroundColor = [UIColor colorNamed:kGrey200Color];
    self.clipsToBounds = YES;
    self.layer.cornerCurve = kCACornerCurveContinuous;
    self.tintColor = [UIColor colorNamed:kTextSecondaryColor];

    _iconView = [[UIImageView alloc] init];
    _iconView.contentMode = UIViewContentModeScaleAspectFit;
    _iconView.translatesAutoresizingMaskIntoConstraints = NO;
    _iconView.image =
        icon ? [icon imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate]
             : nil;
    _iconView.hidden = icon == nil;

    _label = [[UILabel alloc] init];
    _label.adjustsFontForContentSizeCategory = YES;
    _label.textAlignment = NSTextAlignmentCenter;
    _label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _label.translatesAutoresizingMaskIntoConstraints = NO;
    _label.text = [text copy];
    _label.textColor = self.tintColor;

    _stackView =
        [[UIStackView alloc] initWithArrangedSubviews:@[ _iconView, _label ]];
    _stackView.alignment = UIStackViewAlignmentCenter;
    _stackView.spacing = kSpacingSmall;
    _stackView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_stackView];

    [self setupConstraints];
  }
  return self;
}

- (void)updateText:(NSString*)text icon:(UIImage*)icon {
  DCHECK(text);
  _label.text = [text copy];
  _iconView.image =
      icon ? [icon imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate]
           : nil;
  _iconView.hidden = icon == nil;
}

#pragma mark - UIView

- (void)layoutSubviews {
  [super layoutSubviews];
  self.layer.cornerRadius = CGRectGetHeight(self.bounds) / 2.0;
}

- (void)tintColorDidChange {
  [super tintColorDidChange];
  _label.textColor = self.tintColor;
}

#pragma mark - Private

// Intialize the constraints for the subviews
- (void)setupConstraints {
  NSDirectionalEdgeInsets insets = NSDirectionalEdgeInsetsMake(
      kSpacingSmall, kSpacingMedium, kSpacingSmall, kSpacingMedium);
  AddSameConstraintsWithInsets(_stackView, self, insets);
  AddSquareConstraints(_iconView, kChipIconSize);
}

@end
