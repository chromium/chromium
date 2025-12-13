// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/tab_strip/ui/tab_strip_group_cell.h"

#import "base/task/sequenced_task_runner.h"
#import "base/time/time.h"
#import "ios/chrome/browser/saved_tab_groups/ui/face_pile_providing.h"
#import "ios/chrome/browser/shared/ui/elements/fade_truncating_label.h"
#import "ios/chrome/browser/tab_switcher/tab_strip/ui/swift_constants_for_objective_c.h"
#import "ios/chrome/browser/tab_switcher/tab_strip/ui/tab_strip_group_stroke_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Content container constraints.
constexpr CGFloat kcontentContainerVerticalPadding = 4;
constexpr CGFloat kcontentContainerCenterYOffset = -2;
constexpr CGFloat kcontentContainerTransitionThreshold = 70;

// Title label constraints.
constexpr CGFloat kTitleLabelCompressionResistance = 998;

// Face pile constraints.
constexpr CGFloat kFacePileLeadingPadding = 4;
constexpr CGFloat kFacePileTrailingPadding = 2;

// Group stroke constraints.
constexpr CGFloat kGroupStrokeViewMinimumWidth = 14;

// Notification dot constraints.
constexpr CGFloat kNotificationDotSize = 6;
constexpr CGFloat kNotificationDotTrailingPadding = 10;

// Animation delays.
constexpr double kCollapseUpdateGroupStrokeDelaySeconds = 0.25;
constexpr double kcontentContainerFadeAnimationSeconds = 0.25;

// Returns the font used by the title.
UIFont* TitleFont() {
  return [UIFont systemFontOfSize:TabStripTabItemConstants.fontSize
                           weight:UIFontWeightMedium];
}

// Calculates the approximative length of the given title. An additional point
// is added to prevent text fading when the title is fully displayed.
CGFloat CalculateTitleLength(NSString* title) {
  // Add an extra point to avoid having the fading when the text is fully
  // displayed.
  return 1 +
         [title sizeWithAttributes:@{NSFontAttributeName : TitleFont()}].width;
}

}  // namespace

@interface TabStripGroupCell ()

// The face pile view.
@property(nonatomic, strong) UIView* facePile;

@end

@implementation TabStripGroupCell {
  FadeTruncatingLabel* _titleLabel;
  UIView* _contentContainer;
  TabStripGroupStrokeView* _groupStrokeView;
  UIView* _notificationDotView;
  NSLayoutConstraint* _contentContainerHeightConstraint;
  NSLayoutConstraint* _titleLabelTrailingConstraint;
  NSLayoutConstraint* _titleLabelLeadingConstraint;
  NSLayoutConstraint* _notificationDotViewTrailingConstraint;
  NSLayoutConstraint* _groupStrokeViewTitleLabelConstraint;
  // `_collapsed` state of the cell before starting a drag action.
  BOOL _collapsedBeforeDrag;
  CGFloat _titleLength;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _contentContainer = [self createContentContainer];
    [self.contentView addSubview:_contentContainer];
    _groupStrokeView = [[TabStripGroupStrokeView alloc] init];
    [self addSubview:_groupStrokeView];
    [self setupConstraints];
    [self updateGroupStroke];
    [self updateAccessibilityValue];
  }
  return self;
}

#pragma mark - Public

+ (CGFloat)approximativeNonSharedWidthWithTitle:(NSString*)title {
  return CalculateTitleLength(title) +
         2 * TabStripGroupItemConstants.contentContainerHorizontalPadding;
}

#pragma mark - TabStripCell

- (UIDragPreviewParameters*)dragPreviewParameters {
  UIBezierPath* visiblePath = [UIBezierPath
      bezierPathWithRoundedRect:_contentContainer.frame
                   cornerRadius:_contentContainer.layer.cornerRadius];
  UIDragPreviewParameters* params = [[UIDragPreviewParameters alloc] init];
  params.visiblePath = visiblePath;
  return params;
}

#pragma mark - UICollectionViewCell

- (void)prepareForReuse {
  [super prepareForReuse];
  _contentContainer.accessibilityValue = nil;
  _contentContainer.accessibilityLabel = nil;
  _facePileProvider = nil;
  _titleLabel.text = nil;
  self.delegate = nil;
  self.contentContainerBackgroundColor = nil;
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

#pragma mark - Getters

- (CGFloat)optimalWidth {
  return _titleLength + [self widthOfNonTitleElementsIncludingPadding];
}

#pragma mark - Setters

- (void)setTitle:(NSString*)title {
  [super setTitle:title];
  _contentContainer.accessibilityLabel = title;
  _titleLabel.text = [title copy];
  _titleLength = CalculateTitleLength(title);
}

- (void)setContentContainerBackgroundColor:(UIColor*)color {
  _contentContainerBackgroundColor = color;
  _contentContainer.backgroundColor = color;
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

- (void)setFacePileProvider:(id<FacePileProviding>)facePileProvider {
  if ([_facePileProvider isEqualFacePileProviding:facePileProvider]) {
    return;
  }
  _facePileProvider = facePileProvider;

  self.facePile = [_facePileProvider facePileView];
}

- (void)setFacePile:(UIView*)facePile {
  if ([_facePile isDescendantOfView:self]) {
    [_facePile removeFromSuperview];
  }

  _facePile = facePile;
  _titleLabelLeadingConstraint.active = !_facePile;

  if (!_facePile) {
    _groupStrokeViewTitleLabelConstraint.constant = 0;
    return;
  }

  _groupStrokeViewTitleLabelConstraint.constant =
      [_facePileProvider facePileWidth];
  facePile.translatesAutoresizingMaskIntoConstraints = NO;
  [facePile setContentHuggingPriority:UILayoutPriorityRequired
                              forAxis:UILayoutConstraintAxisHorizontal];
  [facePile
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [_contentContainer addSubview:facePile];
  [NSLayoutConstraint activateConstraints:@[
    [facePile.leadingAnchor
        constraintEqualToAnchor:_contentContainer.leadingAnchor
                       constant:kFacePileLeadingPadding],
    [facePile.centerYAnchor
        constraintEqualToAnchor:_contentContainer.centerYAnchor],
    [_titleLabel.leadingAnchor constraintEqualToAnchor:facePile.trailingAnchor
                                              constant:kFacePileTrailingPadding]
  ]];
}

#pragma mark - View creation helpers

// Returns a new title label.
- (FadeTruncatingLabel*)createTitleLabel {
  FadeTruncatingLabel* titleLabel = [[FadeTruncatingLabel alloc] init];
  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  titleLabel.font = TitleFont();
  titleLabel.textColor = [UIColor colorNamed:kSolidWhiteColor];
  titleLabel.textAlignment = NSTextAlignmentCenter;
  [titleLabel
      setContentCompressionResistancePriority:kTitleLabelCompressionResistance
                                      forAxis:UILayoutConstraintAxisHorizontal];
  return titleLabel;
}

// Returns a new content container view.
- (UIView*)createContentContainer {
  UIView* contentContainer = [[UIView alloc] init];
  contentContainer.translatesAutoresizingMaskIntoConstraints = NO;
  contentContainer.layer.masksToBounds = YES;
  contentContainer.isAccessibilityElement = YES;
  contentContainer.layer.cornerRadius =
      TabStripGroupItemConstants.contentContainerHorizontalPadding;
  _titleLabel = [self createTitleLabel];
  [contentContainer addSubview:_titleLabel];
  return contentContainer;
}

#pragma mark - UIAccessibilityAction

- (NSArray*)accessibilityCustomActions {
  int stringID = self.collapsed ? IDS_IOS_TAB_STRIP_TAB_GROUP_EXPAND
                                : IDS_IOS_TAB_STRIP_TAB_GROUP_COLLAPSE;
  return @[ [[UIAccessibilityCustomAction alloc]
      initWithName:l10n_util::GetNSString(stringID)
            target:self
          selector:@selector(collapseOrExpandTapped:)] ];
}

#pragma mark - Private

// Selector registered to expand or collapse tab group.
- (void)collapseOrExpandTapped:(id)sender {
  [self.delegate collapseOrExpandTappedForCell:self];
}

// Sets up constraints.
- (void)setupConstraints {
  UIView* contentView = self.contentView;
  AddSameConstraintsToSidesWithInsets(
      _contentContainer, contentView,
      LayoutSides::kLeading | LayoutSides::kTrailing,
      NSDirectionalEdgeInsetsMake(
          0, TabStripGroupItemConstants.contentContainerHorizontalMargin, 0,
          TabStripGroupItemConstants.contentContainerHorizontalMargin));

  // The width of each cell is calculated in TabStripLayout.
  // The margin and padding at both edges and the title of a group is taken
  // account into the width of each cell.
  _titleLabelLeadingConstraint = [_titleLabel.leadingAnchor
      constraintEqualToAnchor:_contentContainer.leadingAnchor
                     constant:TabStripGroupItemConstants
                                  .contentContainerHorizontalPadding];
  _titleLabelTrailingConstraint = [_titleLabel.trailingAnchor
      constraintEqualToAnchor:_contentContainer.trailingAnchor
                     constant:-TabStripGroupItemConstants
                                   .contentContainerHorizontalPadding];
  _titleLabelTrailingConstraint.priority = kTitleLabelCompressionResistance + 1;

  NSLayoutConstraint* titleLabelMaxWidthConstraint = [_titleLabel.widthAnchor
      constraintLessThanOrEqualToConstant:TabStripGroupItemConstants
                                              .maxTitleWidth];
  titleLabelMaxWidthConstraint.priority = UILayoutPriorityRequired;
  titleLabelMaxWidthConstraint.active = YES;
  _contentContainerHeightConstraint =
      [_contentContainer.heightAnchor constraintEqualToConstant:0];
  _contentContainerHeightConstraint.active = YES;
  _groupStrokeViewTitleLabelConstraint = [_groupStrokeView.widthAnchor
      constraintEqualToAnchor:_titleLabel.widthAnchor];
  _groupStrokeViewTitleLabelConstraint.priority =
      kTitleLabelCompressionResistance - 2;
  NSLayoutConstraint* groupStrokeViewcontentContainerConstraint =
      [_groupStrokeView.widthAnchor
          constraintLessThanOrEqualToAnchor:_contentContainer.widthAnchor
                                   constant:
                                       -2 *
                                           TabStripGroupItemConstants
                                               .contentContainerHorizontalPadding -
                                       kGroupStrokeViewMinimumWidth];
  groupStrokeViewcontentContainerConstraint.priority =
      kTitleLabelCompressionResistance - 1;
  [NSLayoutConstraint activateConstraints:@[
    _groupStrokeViewTitleLabelConstraint,
    groupStrokeViewcontentContainerConstraint,
    [_groupStrokeView.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],
    [_groupStrokeView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],

    [_contentContainer.centerYAnchor
        constraintEqualToAnchor:contentView.centerYAnchor
                       constant:kcontentContainerCenterYOffset],
    [_titleLabel.centerYAnchor
        constraintEqualToAnchor:_contentContainer.centerYAnchor],
    _titleLabelLeadingConstraint,
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
    rightPoint.x += TabStripGroupItemConstants.contentContainerHorizontalMargin;
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

// Returns the combined width of all cell elements, excluding the title.
- (CGFloat)widthOfNonTitleElementsIncludingPadding {
  CGFloat width =
      2 * TabStripGroupItemConstants.contentContainerHorizontalPadding;
  if (_facePile) {
    width += [_facePileProvider facePileWidth];
    width += kFacePileLeadingPadding + kFacePileTrailingPadding;
    width -= TabStripGroupItemConstants.contentContainerHorizontalPadding;
  }
  if (_hasNotificationDot) {
    width += kNotificationDotSize;
    width += kNotificationDotTrailingPadding;
  }
  return width;
}

// Updates the alpha of the title and face pile, and adjusts the content
// container's height, based on its current width relative to its min/max
// dimensions.
- (void)updateTransitionState {
  CGFloat verticalTitlePadding = kcontentContainerVerticalPadding;

  CGFloat threshold =
      kcontentContainerTransitionThreshold + _facePile.bounds.size.width;

  CGFloat contentContainerMargin =
      2 * TabStripGroupItemConstants.contentContainerHorizontalMargin;
  CGFloat maxContentContainerWidth =
      fmin(fmin(self.optimalWidth, TabStripGroupItemConstants.maxCellWidth -
                                       contentContainerMargin),
           threshold);

  CGFloat currentcontentContainerWidth = _contentContainer.bounds.size.width;

  CGFloat mincontentContainerDimension =
      2 * _contentContainer.layer.cornerRadius;

  CGFloat factor = 0;
  if (maxContentContainerWidth - mincontentContainerDimension != 0) {
    factor = (currentcontentContainerWidth - mincontentContainerDimension) /
             (maxContentContainerWidth - mincontentContainerDimension);
  }
  factor = fmax(fmin(factor, 1), 0);

  _facePile.alpha = factor;

  CGFloat maxcontentContainerHeight =
      _titleLabel.frame.size.height + 2 * verticalTitlePadding;
  _titleLabel.alpha = factor;
  _contentContainerHeightConstraint.constant =
      (1 - factor) * mincontentContainerDimension +
      factor * maxcontentContainerHeight;

  // At the end of the group shrinking animation (factor is 0), if the group
  // intersects with the leading or trailing edge, then animate the title
  // container alpha to 0.
  CGFloat contentContainerAlpha = 1;
  if (factor == 0 && (self.intersectsLeftEdge || self.intersectsRightEdge)) {
    contentContainerAlpha = 0;
  }
  UIView* contentContainer = _contentContainer;
  [UIView animateWithDuration:kcontentContainerFadeAnimationSeconds
                   animations:^{
                     contentContainer.alpha = contentContainerAlpha;
                   }];
}

- (void)updateAccessibilityValue {
  // Use the accessibility Value as there is a pause when using the
  // accessibility hint.
  _contentContainer.accessibilityValue = l10n_util::GetNSString(
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
  _notificationDotView.layer.cornerRadius = kNotificationDotSize / 2;
  _notificationDotView.accessibilityIdentifier =
      TabStripGroupItemConstants.notificationDotAccessibilityIdentifier;
  [_contentContainer addSubview:_notificationDotView];

  _notificationDotViewTrailingConstraint = [_notificationDotView.trailingAnchor
      constraintEqualToAnchor:_contentContainer.trailingAnchor
                     constant:-kNotificationDotTrailingPadding];
  _notificationDotViewTrailingConstraint.priority =
      kTitleLabelCompressionResistance + 1;

  [NSLayoutConstraint activateConstraints:@[
    [_notificationDotView.widthAnchor
        constraintEqualToConstant:kNotificationDotSize],
    [_notificationDotView.heightAnchor
        constraintEqualToAnchor:_notificationDotView.widthAnchor],
    // Position the notification dot at right end of the cell.
    [_notificationDotView.centerYAnchor
        constraintEqualToAnchor:_contentContainer.centerYAnchor],
    [_notificationDotView.leadingAnchor
        constraintEqualToAnchor:_titleLabel.trailingAnchor
                       constant:TabStripGroupItemConstants
                                    .contentContainerHorizontalMargin],
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
