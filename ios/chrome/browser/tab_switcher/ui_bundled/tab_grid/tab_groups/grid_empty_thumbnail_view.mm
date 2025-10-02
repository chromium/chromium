// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/grid_empty_thumbnail_view.h"

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ui/base/device_form_factor.h"

namespace {

// Bar corner radius.
const CGFloat kBarCornerRadius = 4;

// Number of bars in this view.
const int kNumberOfBars = 3;

// Fractional width of a short bar.
const CGFloat kShortBarFraction = 2.0f / 3.0f;

// Margin constants.
const CGFloat kStackViewHorizonalMargin = 8;
const CGFloat kStackViewCenteredPortraitAndLandscapeLeadingMargin = 12;
const CGFloat kStackViewLandscapeTopMargin = 14;

// Group cell constants.
const CGFloat kSmallVerticalSpacing = 6;
const CGFloat kGroupViewBarHeight = 12;

// Grid cell constants.
const CGFloat kLargeSpacingGroup = 12;
const CGFloat kGridCellBarHeight = 22;

}  // namespace

@implementation GridEmptyThumbnailView {
  // The configuration type of this view. Currently used just to define the size
  // of the bars.
  EmptyThumbnailType _type;
  UIStackView* _stackView;
  // Constraints for portrait mode.
  NSMutableArray<NSLayoutConstraint*>* _portraitConstraints;
  // Constraints for portrait centered mode.
  NSMutableArray<NSLayoutConstraint*>* _portraitCenteredConstraints;
  // Constraints for landscape mode.
  NSMutableArray<NSLayoutConstraint*>* _landscapeConstraints;
  NSMutableArray<NSLayoutConstraint*>* _landscapeLeadingConstraints;
}

- (instancetype)initWithType:(EmptyThumbnailType)type {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _type = type;
    self.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
    [self setupPlaceholderViews];
  }
  return self;
}

#pragma mark - Setters

- (void)setLayoutType:(EmptyThumbnailLayoutType)layoutType {
  if (_layoutType == layoutType) {
    return;
  }
  _layoutType = layoutType;
  [NSLayoutConstraint deactivateConstraints:_portraitConstraints];
  [NSLayoutConstraint deactivateConstraints:_portraitCenteredConstraints];
  [NSLayoutConstraint deactivateConstraints:_landscapeConstraints];
  [NSLayoutConstraint deactivateConstraints:_landscapeLeadingConstraints];
  switch (_layoutType) {
    case EmptyThumbnailLayoutTypePortrait:
      [NSLayoutConstraint activateConstraints:_portraitConstraints];
      _stackView.alignment = UIStackViewAlignmentLeading;
      _stackView.spacing = kSmallVerticalSpacing;
      break;
    case EmptyThumbnailLayoutTypeCenteredPortrait:
      [NSLayoutConstraint activateConstraints:_portraitCenteredConstraints];
      _stackView.alignment = UIStackViewAlignmentLeading;
      _stackView.spacing = _type == EmptyThumbnailTypeGridCell
                               ? kLargeSpacingGroup
                               : kSmallVerticalSpacing;
      break;
    case EmptyThumbnailLayoutTypeLandscape:
      [NSLayoutConstraint activateConstraints:_landscapeConstraints];
      _stackView.alignment = UIStackViewAlignmentTrailing;
      _stackView.spacing = kSmallVerticalSpacing;
      break;
    case EmptyThumbnailLayoutTypeLandscapeLeading:
      [NSLayoutConstraint activateConstraints:_landscapeLeadingConstraints];
      _stackView.alignment = UIStackViewAlignmentLeading;
      _stackView.spacing = kSmallVerticalSpacing;
      break;
  }
}

#pragma mark - Private

// Sets up the subviews and and constraints for all layout configurations.
- (void)setupPlaceholderViews {
  _stackView = [[UIStackView alloc] init];
  _stackView.translatesAutoresizingMaskIntoConstraints = NO;
  _stackView.axis = UILayoutConstraintAxisVertical;
  _stackView.distribution = UIStackViewDistributionEqualSpacing;
  [self addSubview:_stackView];

  _portraitConstraints = [NSMutableArray array];
  _portraitCenteredConstraints = [NSMutableArray array];
  _landscapeConstraints = [NSMutableArray array];
  _landscapeLeadingConstraints = [NSMutableArray array];
  for (int i = 0; i < kNumberOfBars; i++) {
    UIView* barView = [self createBar];
    [_stackView addArrangedSubview:barView];
    if (i == kNumberOfBars - 1) {
      // Constraints for the bottom bar.
      // 2/3 width for portrait and centered portrait.
      [_portraitConstraints
          addObject:[barView.widthAnchor
                        constraintEqualToAnchor:_stackView.widthAnchor
                                     multiplier:kShortBarFraction]];
      [_portraitCenteredConstraints
          addObject:[barView.widthAnchor
                        constraintEqualToAnchor:_stackView.widthAnchor
                                     multiplier:kShortBarFraction]];
      [_landscapeLeadingConstraints
          addObject:[barView.widthAnchor
                        constraintEqualToAnchor:_stackView.widthAnchor
                                     multiplier:kShortBarFraction]];
      // Full width for landscape.
      [_landscapeConstraints
          addObject:[barView.widthAnchor
                        constraintEqualToAnchor:_stackView.widthAnchor]];
    } else if (i == 0) {
      // Constraints for top bar.
      // Full width for portrait and centered portrait.
      [_portraitConstraints
          addObject:[barView.widthAnchor
                        constraintEqualToAnchor:_stackView.widthAnchor]];
      [_portraitCenteredConstraints
          addObject:[barView.widthAnchor
                        constraintEqualToAnchor:_stackView.widthAnchor]];
      [_landscapeLeadingConstraints
          addObject:[barView.widthAnchor
                        constraintEqualToAnchor:_stackView.widthAnchor]];
      // 2/3 width for landscape.
      [_landscapeConstraints
          addObject:[barView.widthAnchor
                        constraintEqualToAnchor:_stackView.widthAnchor
                                     multiplier:kShortBarFraction]];
    } else {
      // Center bar always full width.
      [barView.widthAnchor constraintEqualToAnchor:_stackView.widthAnchor]
          .active = YES;
    }
  }

  [_landscapeConstraints addObjectsFromArray:@[
    [_stackView.topAnchor constraintEqualToAnchor:self.topAnchor
                                         constant:kStackViewLandscapeTopMargin],
    [_stackView.leadingAnchor
        constraintEqualToAnchor:self.leadingAnchor
                       constant:kStackViewHorizonalMargin],
    [_stackView.trailingAnchor
        constraintEqualToAnchor:self.trailingAnchor
                       constant:-kStackViewHorizonalMargin],
  ]];

  [_landscapeLeadingConstraints addObjectsFromArray:@[
    [_stackView.topAnchor
        constraintEqualToAnchor:self.topAnchor
                       constant:
                           kStackViewCenteredPortraitAndLandscapeLeadingMargin],
    [_stackView.leadingAnchor
        constraintEqualToAnchor:self.leadingAnchor
                       constant:
                           kStackViewCenteredPortraitAndLandscapeLeadingMargin],
    [_stackView.trailingAnchor
        constraintEqualToAnchor:self.trailingAnchor
                       constant:
                           -
                           kStackViewCenteredPortraitAndLandscapeLeadingMargin],
  ]];

  [_portraitCenteredConstraints addObjectsFromArray:@[
    [_stackView.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
    [_stackView.leadingAnchor
        constraintEqualToAnchor:self.leadingAnchor
                       constant:
                           kStackViewCenteredPortraitAndLandscapeLeadingMargin],
    [_stackView.trailingAnchor
        constraintEqualToAnchor:self.trailingAnchor
                       constant:
                           -
                           kStackViewCenteredPortraitAndLandscapeLeadingMargin],
  ]];
  [_portraitConstraints addObjectsFromArray:@[
    [_stackView.bottomAnchor
        constraintEqualToAnchor:self.bottomAnchor
                       constant:-kStackViewHorizonalMargin],
    [_stackView.leadingAnchor
        constraintEqualToAnchor:self.leadingAnchor
                       constant:kStackViewHorizonalMargin],
    [_stackView.trailingAnchor
        constraintEqualToAnchor:self.trailingAnchor
                       constant:-kStackViewHorizonalMargin],
  ]];
}

- (UIView*)createBar {
  UIView* barView = [[UIView alloc] init];
  barView.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
  barView.layer.cornerRadius = kBarCornerRadius;
  CGFloat barHeight = _type == EmptyThumbnailTypeGridCell ? kGridCellBarHeight
                                                          : kGroupViewBarHeight;
  [barView.heightAnchor constraintEqualToConstant:barHeight].active = YES;
  return barView;
}

@end
