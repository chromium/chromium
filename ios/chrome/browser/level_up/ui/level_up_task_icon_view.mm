// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/ui/level_up_task_icon_view.h"

#import "ios/chrome/browser/level_up/coordinator/level_up_task.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// The size of the outer icon background container.
const CGFloat kIconContainerSize = 40.0;
// Corner radius for the outer icon background container.
const CGFloat kIconContainerCornerRadius = 12.0;
// The opacity alpha component for the category background container.
const CGFloat kCategoryBackgroundAlpha = 0.15;
// The size of the inner white background for multicolor icons.
const CGFloat kMulticolorBackgroundSize = 30.0;
// Corner radius for the inner white background for multicolor icons.
const CGFloat kMulticolorBackgroundCornerRadius = 8.0;
// The point size of a task icon inside container.
const CGFloat kIconSize = 24.0;

// Returns the background color for a given task category.
UIColor* CategoryColor(LevelUpTaskCategory category) {
  switch (category) {
    case LevelUpTaskCategory::kProductivity:
      return [UIColor colorNamed:kOrange500Color];
    case LevelUpTaskCategory::kSafety:
      return [UIColor colorNamed:kGreen500Color];
    case LevelUpTaskCategory::kSearch:
      return [UIColor colorNamed:kPurple500Color];
  }
}

}  // namespace

@implementation LevelUpTaskIconView {
  // Container view for task icon background.
  UIView* _iconContainerView;
  // White background view behind multicolor icons.
  UIView* _multicolorBackgroundView;
  // Icon showing task completion state.
  UIImageView* _iconView;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _iconContainerView = [[UIView alloc] init];
    _iconContainerView.translatesAutoresizingMaskIntoConstraints = NO;
    _iconContainerView.layer.cornerRadius = kIconContainerCornerRadius;
    _iconContainerView.layer.masksToBounds = YES;
    [self addSubview:_iconContainerView];
    AddSquareConstraints(_iconContainerView, kIconContainerSize);
    AddSameConstraints(_iconContainerView, self);

    _multicolorBackgroundView = [[UIView alloc] init];
    _multicolorBackgroundView.translatesAutoresizingMaskIntoConstraints = NO;
    _multicolorBackgroundView.backgroundColor =
        [UIColor colorNamed:kPrimaryBackgroundColor];
    _multicolorBackgroundView.layer.cornerRadius =
        kMulticolorBackgroundCornerRadius;
    _multicolorBackgroundView.layer.masksToBounds = YES;
    _multicolorBackgroundView.hidden = YES;
    AddSquareConstraints(_multicolorBackgroundView, kMulticolorBackgroundSize);

    _iconView = [[UIImageView alloc] init];
    _iconView.translatesAutoresizingMaskIntoConstraints = NO;
    _iconView.contentMode = UIViewContentModeScaleAspectFit;
    AddSquareConstraints(_iconView, kIconSize);

    [_iconContainerView addSubview:_multicolorBackgroundView];
    AddSameCenterConstraints(_multicolorBackgroundView, _iconContainerView);

    [_iconContainerView addSubview:_iconView];
    AddSameCenterConstraints(_iconView, _iconContainerView);
  }
  return self;
}

- (void)configureWithTask:(LevelUpTask*)task {
  if (!task) {
    self.hidden = YES;
    return;
  }

  self.hidden = NO;
  if (task.completed) {
    _iconContainerView.backgroundColor = [UIColor clearColor];
    _multicolorBackgroundView.hidden = YES;
    _iconView.tintColor = [UIColor colorNamed:kGreen600Color];
    _iconView.image = SymbolWithPointSize(SymbolCheckmark, kIconSize);
  } else {
    UIColor* categoryColor = CategoryColor(task.category);
    _iconContainerView.backgroundColor =
        [categoryColor colorWithAlphaComponent:kCategoryBackgroundAlpha];
    UIImage* iconImage = SymbolWithPointSize(task.iconSymbol, kIconSize);
    if (task.multicolorIcon) {
      _multicolorBackgroundView.hidden = NO;
      _iconView.tintColor = nil;
      _iconView.image = MakeSymbolMulticolor(iconImage);
    } else {
      _multicolorBackgroundView.hidden = YES;
      _iconView.tintColor = categoryColor;
      _iconView.image = iconImage;
    }
  }
}

@end
