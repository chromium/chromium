// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_strip/ui/tab_strip_group_cell.h"

#import "base/task/sequenced_task_runner.h"
#import "ios/chrome/browser/shared/ui/elements/fade_truncating_label.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_strip/ui/swift_constants_for_objective_c.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_strip/ui/tab_strip_group_stroke_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

constexpr CGFloat kTitleContainerVerticalPadding = 4;
constexpr CGFloat kTitleContainerCenterYOffset = -2;
constexpr CGFloat kGroupStrokeViewMinimumWidth = 14;
constexpr double kCollapseUpdateGroupStrokeDelaySeconds = 0.25;
constexpr double kTitleContainerFadeAnimationSeconds = 0.25;

}  // namespace

@implementation TabStripGroupCell {
  FadeTruncatingLabel* _titleLabel;
  UIView* _titleContainer;
  TabStripGroupStrokeView* _groupStrokeView;
  UIView* _notificationDotView;
  NSLayoutConstraint* _titleContainerHeightConstraint;
  NSLayoutConstraint* _titleLabelTrailingConstraint;
  NSLayoutConstraint* _notificationDotViewTrailingConstraint;
  // `_collapsed` state of the cell before starting a drag action.
  BOOL _collapsedBeforeDrag;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _titleContainer = [self createTitleContainer];
    [self.contentView addSubview:_titleContainer];
    _groupStrokeView = [[TabStripGroupStrokeView alloc] init];
    [self addSubview:_groupStrokeView];
    [self setupConstraints];
    [self updateGroupStroke];
    [self updateAccessibilityValue];
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
  _titleContainer.accessibilityValue = nil;
  _titleContainer.accessibilityLabel = nil;
  _titleLabel.text = nil;
  self.delegate = nil;
  self.titleContainerBackgroundColor = nil;
  self.collapsed = NO;
  self.hasNotificationDot = NO;
}

- (void)applyLayoutAttributes:
    (UICollectionViewLayoutAttributes*)layoutAttributes {
  [super applyLayoutAttributes:layoutAttributes];
  // Update the transition state asynchronously to ensure bounds of subviews
  // have been updated accordingly.
  __weak __typeof(self) weakSelf = self;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(^{
        [weakSelf updateTransitionState];
      }));
}

- (void)layoutSubviews {
  [super layoutSubviews];
  [self updateTransitionState];
}

- (void)dragStateDidChange:(UICollectionViewCellDragState)dragState {
  switch (dragState) {
    case UICollectionViewCellDragStateNone:
      [self setCollapsed:_collapsedBeforeDrag];
      break;
    case UICollectionViewCellDragStateLifting: {
      _collapsedBeforeDrag = _collapsed;
      [self setCollapsed:YES];
      break;
    }
    case UICollectionViewCellDragStateDragging:
      break;
  }
}

#pragma mark - Setters

- (void)setTitle:(NSString*)title {
  [super setTitle:title];
  _titleContainer.accessibilityLabel = title;
  _titleLabel.text = [title copy];
}

- (void)setTitleContainerBackgroundColor:(UIColor*)color {
  _titleContainerBackgroundColor = color;
  _titleContainer.backgroundColor = color;
}

- (void)setTitleTextColor:(UIColor*)titleTextColor {
  _titleTextColor = titleTextColor;
  _titleLabel.textColor = titleTextColor;
  _notificationDotView.backgroundColor = _titleTextColor;
}

- (void)setGroupStrokeColor:(UIColor*)color {
  [super setGroupStrokeColor:color];
  if ([_groupStrokeView.backgroundColor isEqual:color]) {
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
  if (!collapsed) {
    [self updateGroupStroke];
  } else {
    __weak __typeof(self) weakSelf = self;
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, base::BindOnce(^{
          [weakSelf updateGroupStroke];
        }),
        base::Seconds(kCollapseUpdateGroupStrokeDelaySeconds));
  }
  [self updateAccessibilityValue];
}

- (void)setIntersectsLeftEdge:(BOOL)intersectsLeftEdge {
  if (super.intersectsLeftEdge != intersectsLeftEdge) {
    super.intersectsLeftEdge = intersectsLeftEdge;
    [self updateTransitionState];
  }
}

- (void)setIntersectsRightEdge:(BOOL)intersectsRightEdge {
  if (super.intersectsRightEdge != intersectsRightEdge) {
    super.intersectsRightEdge = intersectsRightEdge;
    [self updateTransitionState];
  }
}

- (void)setHasNotificationDot:(BOOL)hasNotificationDot {
  if (_hasNotificationDot == hasNotificationDot) {
    return;
  }

  _hasNotificationDot = hasNotificationDot;

  if (hasNotificationDot) {
    [self showNotificationDotView];
  } else {
    [self hideNotificationDotView];
  }
}

#pragma mark - View creation helpers

// Returns a new title label.
- (FadeTruncatingLabel*)createTitleLabel {
  FadeTruncatingLabel* titleLabel = [[FadeTruncatingLabel alloc] init];
  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  titleLabel.font = [UIFont systemFontOfSize:TabStripTabItemConstants.fontSize
                                      weight:UIFontWeightMedium];
  titleLabel.textColor = [UIColor colorNamed:kSolidWhiteColor];
  titleLabel.textAlignment = NSTextAlignmentCenter;
  [titleLabel
      setContentCompressionResistancePriority:UILayoutPriorityRequired - 1
                                      forAxis:UILayoutConstraintAxisHorizontal];
  return titleLabel;
}

// Returns a new title container view.
- (UIView*)createTitleContainer {
  UIView* titleContainer = [[UIView alloc] init];
  titleContainer.translatesAutoresizingMaskIntoConstraints = NO;
  titleContainer.layer.masksToBounds = YES;
  titleContainer.isAccessibilityElement = YES;
  titleContainer.layer.cornerRadius =
      TabStripGroupItemConstants.titleContainerHorizontalPadding;
  _titleLabel = [self createTitleLabel];
  [titleContainer addSubview:_titleLabel];
  return titleContainer;
}

#pragma mark - UIAccessibility

- (NSArray*)accessibilityCustomActions {
  int stringID = self.collapsed ? IDS_IOS_TAB_STRIP_TAB_GROUP_EXPAND
                                : IDS_IOS_TAB_STRIP_TAB_GROUP_COLLAPSE;
  return @[ [[UIAccessibilityCustomAction alloc]
      initWithName:l10n_util::GetNSString(stringID)
            target:self
          selector:@selector(collapseOrExpandTapped:)] ];
}

// Selector registered to expand or collapse tab group.
- (void)collapseOrExpandTapped:(id)sender {
  [self.delegate collapseOrExpandTappedForCell:self];
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

  // The width of each cell is calculated in TabStripLayout.
  // The margin and padding at both edges and the title of a group is taken
  // account into the width of each cell.
  _titleLabelTrailingConstraint = [_titleLabel.trailingAnchor
      constraintEqualToAnchor:_titleContainer.trailingAnchor
                     constant:-TabStripGroupItemConstants
                                   .titleContainerHorizontalPadding];

  NSLayoutConstraint* titleLabelMaxWidthConstraint = [_titleLabel.widthAnchor
      constraintLessThanOrEqualToConstant:TabStripGroupItemConstants
                                              .maxTitleWidth];
  titleLabelMaxWidthConstraint.priority = UILayoutPriorityRequired;
  titleLabelMaxWidthConstraint.active = YES;
  _titleContainerHeightConstraint =
      [_titleContainer.heightAnchor constraintEqualToConstant:0];
  _titleContainerHeightConstraint.active = YES;
  NSLayoutConstraint* groupStrokeViewTitleLabelConstraint =
      [_groupStrokeView.widthAnchor
          constraintEqualToAnchor:_titleLabel.widthAnchor];
  groupStrokeViewTitleLabelConstraint.priority = UILayoutPriorityRequired - 3;
  NSLayoutConstraint* groupStrokeViewTitleContainerConstraint =
      [_groupStrokeView.widthAnchor
          constraintLessThanOrEqualToAnchor:_titleContainer.widthAnchor
                                   constant:
                                       -2 *
                                           TabStripGroupItemConstants
                                               .titleContainerHorizontalPadding -
                                       kGroupStrokeViewMinimumWidth];
  groupStrokeViewTitleContainerConstraint.priority =
      UILayoutPriorityRequired - 2;
  [NSLayoutConstraint activateConstraints:@[
    groupStrokeViewTitleLabelConstraint,
    groupStrokeViewTitleContainerConstraint,
    [_groupStrokeView.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],
    [_groupStrokeView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],

    [_titleContainer.centerYAnchor
        constraintEqualToAnchor:contentView.centerYAnchor
                       constant:kTitleContainerCenterYOffset],
    [_titleLabel.centerYAnchor
        constraintEqualToAnchor:_titleContainer.centerYAnchor],
    [_titleLabel.leadingAnchor
        constraintEqualToAnchor:_titleContainer.leadingAnchor
                       constant:TabStripGroupItemConstants
                                    .titleContainerHorizontalPadding],
    _titleLabelTrailingConstraint,
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
  leftPoint.x -= kGroupStrokeViewMinimumWidth / 2;
  [leftPath addLineToPoint:leftPoint];
  leftPoint.y += lineWidth / 2;
  [leftPath addArcWithCenter:leftPoint
                      radius:lineWidth / 2
                  startAngle:M_PI + M_PI_2
                    endAngle:M_PI
                   clockwise:NO];
  leftPoint.x -= lineWidth / 2;
  [_groupStrokeView setLeadingPath:leftPath.CGPath];

  UIBezierPath* rightPath = [UIBezierPath bezierPath];
  CGPoint rightPoint = CGPointZero;
  [rightPath moveToPoint:rightPoint];
  rightPoint.x += kGroupStrokeViewMinimumWidth / 2;
  [rightPath addLineToPoint:rightPoint];
  if (!self.collapsed) {
    // If the group is not collapse, the right end of the stroke should extend
    // to reach the left end of the next tab.
    rightPoint.x += TabStripGroupItemConstants.titleContainerHorizontalMargin;
    rightPoint.x += TabStripTabItemConstants.horizontalSpacing;
    rightPoint.x += lineWidth;
    rightPoint.x += TabStripCollectionViewConstants.groupStrokeExtension;
    [rightPath addLineToPoint:rightPoint];
  }
  rightPoint.y += lineWidth / 2;
  [rightPath addArcWithCenter:rightPoint
                       radius:lineWidth / 2
                   startAngle:M_PI + M_PI_2
                     endAngle:0
                    clockwise:YES];
  [_groupStrokeView setTrailingPath:rightPath.CGPath];
}

// Updates the title alpha value and title container height according to the
// difference between the size of the title and the size of its container.
- (void)updateTransitionState {
  CGFloat horizontalTitlePadding =
      TabStripGroupItemConstants.titleContainerHorizontalPadding;
  CGFloat verticalTitlePadding = kTitleContainerVerticalPadding;
  CGFloat titleContainerWidth = _titleContainer.bounds.size.width;
  CGFloat maxTitleContainerWidth =
      _titleLabel.frame.size.width + 2 * horizontalTitlePadding;
  CGFloat minTitleContainerHeight = 2 * _titleContainer.layer.cornerRadius;
  CGFloat maxTitleContainerHeight =
      _titleLabel.frame.size.height + 2 * verticalTitlePadding;
  CGFloat factor = 0;
  if (maxTitleContainerWidth - 2 * horizontalTitlePadding > 0) {
    factor = (titleContainerWidth - 2 * horizontalTitlePadding) /
             (maxTitleContainerWidth - 2 * horizontalTitlePadding);
  }
  _titleLabel.alpha = factor;
  _titleContainerHeightConstraint.constant =
      (1 - factor) * minTitleContainerHeight + factor * maxTitleContainerHeight;

  // At the end of the group shrinking animation (factor is 0), if the group
  // intersects with the leading or trailing edge, then animate the title
  // container alpha to 0.
  CGFloat titleContainerAlpha = 1;
  if (factor == 0 && (self.intersectsLeftEdge || self.intersectsRightEdge)) {
    titleContainerAlpha = 0;
  }
  UIView* titleContainer = _titleContainer;
  [UIView animateWithDuration:kTitleContainerFadeAnimationSeconds
                   animations:^{
                     titleContainer.alpha = titleContainerAlpha;
                   }];
}

- (void)updateAccessibilityValue {
  // Use the accessibility Value as there is a pause when using the
  // accessibility hint.
  _titleContainer.accessibilityValue = l10n_util::GetNSString(
      self.collapsed ? IDS_IOS_TAB_STRIP_GROUP_CELL_COLLAPSED_VOICE_OVER_VALUE
                     : IDS_IOS_TAB_STRIP_GROUP_CELL_EXPANDED_VOICE_OVER_VALUE);
}

// Shows the notification dow view. Adds the view to the cell if there is none
// yet.
- (void)showNotificationDotView {
  // Disable the center position of title label to avoid the conflict with the
  // constraint for the notification dot view.
  _titleLabelTrailingConstraint.active = NO;

  if (_notificationDotView) {
    // Enable the constraint for the notification dot view.
    CHECK(_notificationDotViewTrailingConstraint);
    _notificationDotViewTrailingConstraint.active = YES;

    _notificationDotView.hidden = NO;
    return;
  }

  _notificationDotView = [[UIView alloc] init];
  _notificationDotView.backgroundColor = _titleTextColor;
  _notificationDotView.translatesAutoresizingMaskIntoConstraints = NO;
  _notificationDotView.layer.cornerRadius =
      TabStripGroupItemConstants.notificationDotSize / 2;
  _notificationDotView.accessibilityIdentifier =
      TabStripGroupItemConstants.notificationDotAccessibilityIdentifier;
  [_titleContainer addSubview:_notificationDotView];

  _notificationDotViewTrailingConstraint = [_notificationDotView.trailingAnchor
      constraintEqualToAnchor:_titleContainer.trailingAnchor
                     constant:-TabStripGroupItemConstants
                                   .titleContainerHorizontalPadding];

  [NSLayoutConstraint activateConstraints:@[
    [_notificationDotView.widthAnchor
        constraintEqualToConstant:TabStripGroupItemConstants
                                      .notificationDotSize],
    [_notificationDotView.heightAnchor
        constraintEqualToAnchor:_notificationDotView.widthAnchor],
    // Position the notification dot at right end of the cell.
    [_notificationDotView.centerYAnchor
        constraintEqualToAnchor:_titleContainer.centerYAnchor],
    [_notificationDotView.leadingAnchor
        constraintEqualToAnchor:_titleLabel.trailingAnchor
                       constant:TabStripGroupItemConstants
                                    .titleContainerHorizontalMargin],
    _notificationDotViewTrailingConstraint,
  ]];
}

// Hides the notification dot view in the cell and adjusts the constraints.
- (void)hideNotificationDotView {
  _notificationDotView.hidden = YES;

  // Disable the constraint for the notification dot view to avoid the conflict
  // with the constraint for the title label.
  _notificationDotViewTrailingConstraint.active = NO;

  // Enable the constraint for the title label to place it in the center of the
  // container view.
  _titleLabelTrailingConstraint.active = YES;
}

@end
