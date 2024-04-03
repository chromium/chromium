// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/ui/tab_strip_group_cell.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/ui/swift_constants_for_objective_c.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/ui/tab_strip_group_stroke_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

constexpr CGFloat kTitleContainerVerticalPadding = 4;
constexpr CGFloat kTitleContainerCenterYOffset = -2;
constexpr CGFloat kGroupStrokeViewMinimumWidth = 14;

}  // namespace

@implementation TabStripGroupCell {
  UILabel* _titleLabel;
  UIView* _titleContainer;
  TabStripGroupStrokeView* _groupStrokeView;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.isAccessibilityElement = YES;
    _titleContainer = [self createTitleContainer];
    [self.contentView addSubview:_titleContainer];
    _groupStrokeView = [[TabStripGroupStrokeView alloc] init];
    [self addSubview:_groupStrokeView];
    [self setupConstraints];
    [self updateGroupStroke];
  }
  return self;
}

#pragma mark - TabStripCell

- (UIDragPreviewParameters*)dragPreviewParameters {
  UIBezierPath* visiblePath = [UIBezierPath
      bezierPathWithRoundedRect:_titleContainer.frame
                   cornerRadius:_titleContainer.layer.cornerRadius];
  UIDragPreviewParameters* params = [[UIDragPreviewParameters alloc] init];
  params.visiblePath = visiblePath;
  return params;
}

#pragma mark - UICollectionViewCell

- (void)prepareForReuse {
  [super prepareForReuse];
  self.titleContainerBackgroundColor = nil;
  self.collapsed = NO;
}

#pragma mark - Setters

- (void)setTitle:(NSString*)title {
  [super setTitle:title];
  self.accessibilityLabel = title;
  _titleLabel.text = [title copy];
}

- (void)setTitleContainerBackgroundColor:(UIColor*)color {
  _titleContainerBackgroundColor = color;
  _titleContainer.backgroundColor = color;
}

- (void)setGroupStrokeColor:(UIColor*)color {
  if (_groupStrokeView.backgroundColor == color) {
    return;
  }
  _groupStrokeView.backgroundColor = color;
  [self updateGroupStroke];
}

- (void)setCollapsed:(BOOL)collapsed {
  if (_collapsed == collapsed) {
    return;
  }
  _collapsed = collapsed;
  [self updateGroupStroke];
}

#pragma mark - View creation helpers

// Returns a new title label.
- (UILabel*)createTitleLabel {
  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  titleLabel.font = [UIFont systemFontOfSize:TabStripTabItemConstants.fontSize
                                      weight:UIFontWeightMedium];
  titleLabel.textColor = [UIColor colorNamed:kSolidWhiteColor];
  titleLabel.adjustsFontSizeToFitWidth = YES;
  return titleLabel;
}

// Returns a new title container view.
- (UIView*)createTitleContainer {
  UIView* titleContainer = [[UIView alloc] init];
  titleContainer.translatesAutoresizingMaskIntoConstraints = NO;
  titleContainer.layer.cornerRadius =
      TabStripGroupItemConstants.titleContainerHorizontalPadding;
  _titleLabel = [self createTitleLabel];
  [titleContainer addSubview:_titleLabel];
  return titleContainer;
}

#pragma mark - Private

// Sets up constraints.
- (void)setupConstraints {
  UIView* contentView = self.contentView;
  AddSameConstraintsToSidesWithInsets(
      _titleContainer, contentView,
      LayoutSides::kLeading | LayoutSides::kTrailing,
      NSDirectionalEdgeInsetsMake(
          0, TabStripGroupItemConstants.titleContainerHorizontalMargin, 0,
          TabStripGroupItemConstants.titleContainerHorizontalMargin));
  [_titleContainer.centerYAnchor
      constraintEqualToAnchor:contentView.centerYAnchor
                     constant:kTitleContainerCenterYOffset]
      .active = YES;
  AddSameConstraintsWithInsets(
      _titleLabel, _titleContainer,
      NSDirectionalEdgeInsetsMake(
          kTitleContainerVerticalPadding,
          TabStripGroupItemConstants.titleContainerHorizontalPadding,
          kTitleContainerVerticalPadding,
          TabStripGroupItemConstants.titleContainerHorizontalPadding));
  const CGFloat titleLabelMinimumHeight =
      2 * (TabStripGroupItemConstants.titleContainerHorizontalPadding -
           kTitleContainerVerticalPadding);
  [_titleLabel.heightAnchor
      constraintGreaterThanOrEqualToConstant:titleLabelMinimumHeight]
      .active = YES;
  [NSLayoutConstraint activateConstraints:@[
    [_groupStrokeView.leftAnchor
        constraintLessThanOrEqualToAnchor:_titleLabel.leftAnchor],
    [_groupStrokeView.rightAnchor
        constraintGreaterThanOrEqualToAnchor:_titleLabel.rightAnchor],
    [_groupStrokeView.widthAnchor
        constraintGreaterThanOrEqualToConstant:kGroupStrokeViewMinimumWidth],
    [_groupStrokeView.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],
    [_groupStrokeView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
  ]];
}

// Updates the group stroke path, hides the group stroke if necessary.
- (void)updateGroupStroke {
  if (!_groupStrokeView.backgroundColor) {
    _groupStrokeView.hidden = YES;
    return;
  }
  _groupStrokeView.hidden = NO;

  const CGFloat lineWidth =
      TabStripCollectionViewConstants.groupStrokeLineWidth;

  UIBezierPath* leftPath = [UIBezierPath bezierPath];
  CGPoint leftPoint = CGPointZero;
  [leftPath moveToPoint:leftPoint];
  leftPoint.y += lineWidth / 2;
  [leftPath addArcWithCenter:leftPoint
                      radius:lineWidth / 2
                  startAngle:M_PI + M_PI_2
                    endAngle:M_PI
                   clockwise:NO];
  leftPoint.x -= lineWidth / 2;
  [_groupStrokeView setLeftPath:leftPath.CGPath];

  UIBezierPath* rightPath = [UIBezierPath bezierPath];
  CGPoint rightPoint = CGPointZero;
  [rightPath moveToPoint:rightPoint];
  if (self.collapsed) {
    // If the group is collapsed, then the right end of the stroke should just
    // be a quarter circle.
    rightPoint.y += lineWidth / 2;
    [rightPath addArcWithCenter:rightPoint
                         radius:lineWidth / 2
                     startAngle:M_PI + M_PI_2
                       endAngle:0
                      clockwise:YES];
  } else {
    // If the group is not collapse, the right end of the stroke should extend
    // to reach the left end of the next tab.
    rightPoint.x += TabStripGroupItemConstants.titleContainerHorizontalPadding;
    rightPoint.x += TabStripGroupItemConstants.titleContainerHorizontalMargin;
    rightPoint.x += TabStripTabItemConstants.horizontalSpacing;
    [rightPath addLineToPoint:rightPoint];
  }
  [_groupStrokeView setRightPath:rightPath.CGPath];
}

@end
