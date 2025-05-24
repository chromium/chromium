// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/activity_label_view.h"

#import "base/check.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {

// Minimum height of the new activity label.
const CGFloat kNewActivityLabelMinHeight = 30;
// Maximum height of the new activity label.
const CGFloat kNewActivityLabelMaxHeight = 59;
// Minimum width of the new activity label.
const CGFloat kNewActivityLabelMinWidth = 76;
// Maximum width of the new activity label.
const CGFloat kNewActivityLabelMaxWidth = 170;
// Shadow opacity for the new activity label.
const CGFloat kShadowOpacity = 0.2;
// Shadow offset for the new activity label.
const CGSize kShadowOffset = {0, 6};
// Radius of the shadow of the new activity label.
const CGFloat kShadowRadius = 2;
// Horizontal padding of the new activity label.
const CGFloat kLabelHorizontalPadding = 10;
// Space between items in the new activity label.
const CGFloat kSpaceBetweenItem = 6;

}  // namespace

// TODO(crbug.com/371113934): Add smooth animation to hide this view during
// the layout transition.
@implementation ActivityLabelView {
  // Effect view for the background with the material regular dark color.
  UIVisualEffectView* _backgroundView;
  // Title label in the new activity label.
  UILabel* _activityTitleLabel;
  // Icon view in the new activity label. The view is hidden by default.
  UIView* _activityIconView;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    [self setupActivityLabel];
  }
  return self;
}

- (void)setLabelText:(NSString*)text {
  CHECK(_activityTitleLabel);
  _activityTitleLabel.text = text;
}

- (void)setUserIcon:(UIView*)iconView {
  CHECK(_activityIconView);
  if (_activityIconView.subviews.firstObject == iconView) {
    return;
  }

  for (UIView* subview in _activityIconView.subviews) {
    [subview removeFromSuperview];
  }

  // Hide the view if `iconView` is nil.
  if (!iconView) {
    _activityIconView.hidden = YES;
    return;
  }

  [_activityIconView addSubview:iconView];
  [NSLayoutConstraint activateConstraints:@[
    [iconView.centerXAnchor
        constraintEqualToAnchor:_activityIconView.centerXAnchor],
    [iconView.centerYAnchor
        constraintEqualToAnchor:_activityIconView.centerYAnchor],
  ]];
  _activityIconView.hidden = NO;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  // Update the corner radius based on the view's size.
  auto radius = self.bounds.size.height / 2.0;
  self.layer.cornerRadius = radius;
  _backgroundView.layer.cornerRadius = radius;
}

#pragma mark - Private

// Sets up the new activity label.
- (void)setupActivityLabel {
  self.translatesAutoresizingMaskIntoConstraints = NO;
  self.layer.shadowOpacity = kShadowOpacity;
  self.layer.shadowOffset = kShadowOffset;
  self.layer.shadowRadius = kShadowRadius;
  _backgroundView = [[UIVisualEffectView alloc]
      initWithEffect:[UIBlurEffect
                         effectWithStyle:UIBlurEffectStyleSystemMaterialDark]];
  _backgroundView.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  _backgroundView.clipsToBounds = YES;
  _backgroundView.frame = self.frame;
  [self addSubview:_backgroundView];

  _activityIconView = [[UIView alloc] init];
  _activityIconView.hidden = YES;

  _activityTitleLabel = [[UILabel alloc] init];
  _activityTitleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleCaption2];
  _activityTitleLabel.textAlignment = NSTextAlignmentCenter;
  _activityTitleLabel.adjustsFontSizeToFitWidth = YES;
  _activityTitleLabel.adjustsFontForContentSizeCategory = YES;
  _activityTitleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _activityTitleLabel.textColor = UIColor.whiteColor;
  _activityTitleLabel.numberOfLines = 1;

  UIStackView* stackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ _activityIconView, _activityTitleLabel ]];
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  stackView.axis = UILayoutConstraintAxisHorizontal;
  stackView.distribution = UIStackViewDistributionFill;
  stackView.alignment = UIStackViewAlignmentCenter;
  [stackView setCustomSpacing:kSpaceBetweenItem afterView:_activityIconView];

  [self addSubview:stackView];

  // Set `zPosition` to greater than 1 to put this activity label view in the
  // foreground of the cell.
  self.layer.zPosition = 1;

  [NSLayoutConstraint activateConstraints:@[
    [self.heightAnchor
        constraintGreaterThanOrEqualToConstant:kNewActivityLabelMinHeight],
    [self.heightAnchor
        constraintLessThanOrEqualToConstant:kNewActivityLabelMaxHeight],
    [self.widthAnchor
        constraintGreaterThanOrEqualToConstant:kNewActivityLabelMinWidth],
    [self.widthAnchor
        constraintLessThanOrEqualToConstant:kNewActivityLabelMaxWidth],

    [stackView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor
                                            constant:kLabelHorizontalPadding],
    [stackView.trailingAnchor constraintEqualToAnchor:self.trailingAnchor
                                             constant:-kLabelHorizontalPadding],
    [stackView.topAnchor constraintEqualToAnchor:self.topAnchor],
    [stackView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
  ]];
}

@end
