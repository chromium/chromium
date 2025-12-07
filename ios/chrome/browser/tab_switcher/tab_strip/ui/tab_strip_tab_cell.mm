// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/tab_strip/ui/tab_strip_tab_cell.h"

#import <algorithm>

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "base/strings/string_number_conversions.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/image/image_util.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/tab_switcher/tab_strip/ui/swift_constants_for_objective_c.h"
#import "ios/chrome/browser/tab_switcher/tab_strip/ui/tab_strip_group_stroke_view.h"
#import "ios/chrome/browser/tab_switcher/tab_strip/ui/tab_strip_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/gradient_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/raccoon/raccoon_api.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Scale of activity indicator replacing fav icon when active.
const CGFloat kIndicatorScale = 0.75;

// The size of the close button.
constexpr CGFloat kCloseButtonSize = 16;
constexpr CGFloat kCloseButtonMinimumTouchTarget = 36;

// Size of the decoration corner and corner radius when the cell is selected.
constexpr CGFloat kCornerSize = 16;

// Threshold width for collapsing the cell and hiding the close button.
constexpr CGFloat kCollapsedWidthThreshold = 150;

// Separator constraints.
constexpr CGFloat kSeparatorHorizontalInset = 2;
constexpr CGFloat kSeparatorGradientWidth = 4;

// Visibility constants.
constexpr CGFloat kCloseButtonVisibilityThreshold = 0.3;

// Content view constants.
constexpr CGFloat kFaviconLeadingMargin = 10;
constexpr CGFloat kCloseButtonMargin = 10;
constexpr CGFloat kTitleInset = 10;
constexpr CGFloat kTitleOverflowWidth = 20;
constexpr CGFloat kFaviconSize = 16;
constexpr CGFloat kTitleGradientWidth = 16;
constexpr CGFloat kContentViewBottomInset = 4;

// Selected border background view constants.
constexpr CGFloat kSelectedBorderBackgroundViewWidth = 8;

// The storke width around a blue dot view.
constexpr CGFloat kBlueDotStrokeWidth = 2;

// Size of a blue dot on icon view with considering the stroke width.
constexpr CGFloat kBlueDotSize = 6 + kBlueDotStrokeWidth * 2;
constexpr CGFloat kBlueDotInset = 1;

}  // namespace

@implementation TabStripTabCell {
  // Content view subviews.
  UIButton* _closeButton;
  UIImageView* _faviconView;

  // Group stroke views and constraints.
  TabStripGroupStrokeView* _groupStrokeView;
  NSLayoutConstraint* _groupStrokeViewWidthConstraint;

  // Decoration views, visible when the cell is selected.
  UIView* _leftTailView;
  UIView* _rightTailView;
  UIView* _selectedBackground;

  // Cell separator.
  UIView* _leadingSeparatorView;
  UIView* _trailingSeparatorView;
  UIView* _leadingSeparatorGradientView;
  UIView* _trailingSeparatorGradientView;

  // Background views displayed when the selected cell in on an edge. Used to
  // hide the cells passing behind the selected cell.
  UIView* _leadingSelectedBorderBackgroundView;
  UIView* _trailingSelectedBorderBackgroundView;

  // Circular spinner that shows the loading state of the tab.
  UIActivityIndicatorView* _activityIndicator;

  // The cell's title is always displayed between the favicon and the close
  // button (or the trailing end of the cell if there is no close button). The
  // text of the title will follow its language direction. If the text is too
  // long, it is cut using a gradient.
  // To allow for changing the alpha of the cell, the gradient will be done
  // using a mask on the title and not having a view on top. Resizing
  // dynamically a gradient is not visually pleasant. So to achieve that, the
  // `_titleContainer` will have a fixed size (the max size of the view) and a
  // gradient on both sides. Based on the text reading direction, its right/left
  // edge will be positioned at the same side of `_titleLabel`.
  UIView* _titleContainer;
  UILabel* _titleLabel;
  // Title's trailing constraints.
  NSLayoutConstraint* _titleCollapsedTrailingConstraint;
  NSLayoutConstraint* _titleTrailingConstraint;
  // Title's gradient constraints.
  NSLayoutConstraint* _titleContainerLeftConstraint;
  NSLayoutConstraint* _titleContainerRightConstraint;
  // As NSLineBreakByClipping doesn't work with RTL languages, the `_titleLabel`
  // will be longer than its displayed position to have the ellipsis
  // non-visible. `_titlePositioner` has the "correct" right/left bounds. Based
  // on the title's text, the correct constraint will be activated.
  UILayoutGuide* _titlePositioner;
  NSLayoutConstraint* _titleLeftConstraint;
  NSLayoutConstraint* _titleRightConstraint;

  // Stroke view's constraints.
  NSLayoutConstraint* _groupStrokeViewBottomConstraint;
  NSLayoutConstraint* _groupStrokeViewBottomSelectedConstraint;

  // Separator height constraints.
  NSArray<NSLayoutConstraint*>* _separatorHeightConstraints;
  CGFloat _separatorHeight;

  // whether the view is hovered.
  BOOL _hovered;

  // View used to provide accessibility labels/values while letting the close
  // button selectable by VoiceOver.
  UIView* _accessibilityContainerView;

  // View used to display the blue dot at right bottom corner of the favicon.
  UIView* _blueDotView;
}

- (instancetype)initWithFrame:(CGRect)frame {
  if ((self = [super initWithFrame:frame])) {
    self.layer.masksToBounds = NO;
    _hovered = NO;
    _separatorHeight = 0;

    [self addInteraction:[[UIPointerInteraction alloc] initWithDelegate:self]];

    if (ios::provider::IsRaccoonEnabled()) {
      self.hoverStyle = [UIHoverStyle
          styleWithShape:[UIShape rectShapeWithCornerRadius:kCornerSize]];
    }

    UIView* contentView = self.contentView;
    contentView.layer.masksToBounds = YES;

    _accessibilityContainerView = [[UIView alloc] init];
    _accessibilityContainerView.isAccessibilityElement = YES;
    _accessibilityContainerView.translatesAutoresizingMaskIntoConstraints = NO;
    _accessibilityContainerView.layer.cornerRadius = kCornerSize;
    [contentView addSubview:_accessibilityContainerView];

    _faviconView = [self createFaviconView];
    [_accessibilityContainerView addSubview:_faviconView];

    _activityIndicator = [self createActivityIndicatior];
    [_accessibilityContainerView addSubview:_activityIndicator];

    _closeButton = [self createCloseButton];
    [contentView addSubview:_closeButton];

    _titleContainer = [self createTitleContainer];
    [_accessibilityContainerView addSubview:_titleContainer];

    _titlePositioner = [[UILayoutGuide alloc] init];
    [_accessibilityContainerView addLayoutGuide:_titlePositioner];

    _leadingSelectedBorderBackgroundView =
        [self createSelectedBorderBackgroundView];
    [self addSubview:_leadingSelectedBorderBackgroundView];

    _trailingSelectedBorderBackgroundView =
        [self createSelectedBorderBackgroundView];
    [self addSubview:_trailingSelectedBorderBackgroundView];

    _leftTailView = [self createDecorationView];
    [self addSubview:_leftTailView];

    _rightTailView = [self createDecorationView];
    [self addSubview:_rightTailView];

    _selectedBackground = [self createSelectedBackgroundView];
    [self insertSubview:_selectedBackground belowSubview:contentView];

    _leadingSeparatorView = [self createSeparatorView];
    [self addSubview:_leadingSeparatorView];

    _trailingSeparatorView = [self createSeparatorView];
    [self addSubview:_trailingSeparatorView];

    _leadingSeparatorGradientView = [self createGradientView];
    [self addSubview:_leadingSeparatorGradientView];

    _trailingSeparatorGradientView = [self createGradientView];
    [self addSubview:_trailingSeparatorGradientView];

    _groupStrokeView = [[TabStripGroupStrokeView alloc] init];
    [self addSubview:_groupStrokeView];

    [self setupConstraints];
    [self setupDecorationLayers];
    [self updateGroupStroke];

    self.selected = NO;

    NSArray<UITrait>* traits = TraitCollectionSetForTraits(nil);
    [self registerForTraitChanges:traits withAction:@selector(updateColors)];

    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(handleFocusUpdate:)
               name:UIFocusDidUpdateNotification
             object:nil];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:UIFocusDidUpdateNotification
              object:nil];
}

- (void)setFaviconImage:(UIImage*)image {
  _faviconView.image = image;
}

#pragma mark - TabStripCell

- (UIDragPreviewParameters*)dragPreviewParameters {
  UIBezierPath* visiblePath =
      [UIBezierPath bezierPathWithRoundedRect:_accessibilityContainerView.frame
                                 cornerRadius:kCornerSize];
  UIDragPreviewParameters* params = [[UIDragPreviewParameters alloc] init];
  params.visiblePath = visiblePath;
  return params;
}

#pragma mark - Setters

- (void)setTitle:(NSString*)title {
  [super setTitle:title];
  _accessibilityContainerView.accessibilityLabel = title;
  NSTextAlignment titleTextAligment = DetermineBestAlignmentForText(title);
  _titleLabel.text = [title copy];
  _titleLabel.textAlignment = titleTextAligment;
  [self updateTitleConstraints];
}

- (void)setGroupStrokeColor:(UIColor*)color {
  [super setGroupStrokeColor:color];
  if (_groupStrokeView.backgroundColor == color) {
    return;
  }
  _groupStrokeView.backgroundColor = color;
  [self updateGroupStroke];
}

- (void)setIsLastTabInGroup:(BOOL)isLastTabInGroup {
  if (_isLastTabInGroup == isLastTabInGroup) {
    return;
  }
  _isLastTabInGroup = isLastTabInGroup;
  [self updateGroupStroke];
}

- (void)setLoading:(BOOL)loading {
  if (_loading == loading) {
    return;
  }
  _loading = loading;
  if (loading) {
    _activityIndicator.hidden = NO;
    [_activityIndicator startAnimating];
    _faviconView.hidden = YES;
    _faviconView.image = nil;
  } else {
    _activityIndicator.hidden = YES;
    [_activityIndicator stopAnimating];
    _faviconView.hidden = NO;
  }
}

- (void)setLeadingSeparatorHidden:(BOOL)leadingSeparatorHidden {
  _leadingSeparatorHidden = leadingSeparatorHidden;
  _leadingSeparatorView.hidden = leadingSeparatorHidden;
}

- (void)setTrailingSeparatorHidden:(BOOL)trailingSeparatorHidden {
  _trailingSeparatorHidden = trailingSeparatorHidden;
  _trailingSeparatorView.hidden = trailingSeparatorHidden;
}

- (void)setLeadingSeparatorGradientViewHidden:
    (BOOL)leadingSeparatorGradientViewHidden {
  _leadingSeparatorGradientViewHidden = leadingSeparatorGradientViewHidden;
  _leadingSeparatorGradientView.hidden = leadingSeparatorGradientViewHidden;
}

- (void)setTrailingSeparatorGradientViewHidden:
    (BOOL)trailingSeparatorGradientViewHidden {
  _trailingSeparatorGradientViewHidden = trailingSeparatorGradientViewHidden;
  _trailingSeparatorGradientView.hidden = trailingSeparatorGradientViewHidden;
}

- (void)setLeadingSelectedBorderBackgroundViewHidden:
    (BOOL)leadingSelectedBorderBackgroundViewHidden {
  if (_leadingSelectedBorderBackgroundViewHidden ==
      leadingSelectedBorderBackgroundViewHidden) {
    return;
  }
  _leadingSelectedBorderBackgroundViewHidden =
      leadingSelectedBorderBackgroundViewHidden;
  _leadingSelectedBorderBackgroundView.hidden =
      leadingSelectedBorderBackgroundViewHidden;
}

- (void)setTrailingSelectedBorderBackgroundViewHidden:
    (BOOL)trailingSelectedBorderBackgroundViewHidden {
  if (_trailingSelectedBorderBackgroundViewHidden ==
      trailingSelectedBorderBackgroundViewHidden) {
    return;
  }
  _trailingSelectedBorderBackgroundViewHidden =
      trailingSelectedBorderBackgroundViewHidden;
  _trailingSelectedBorderBackgroundView.hidden =
      trailingSelectedBorderBackgroundViewHidden;
}

- (void)setSelected:(BOOL)selected {
  BOOL oldSelected = self.selected;
  [super setSelected:selected];

  if (selected) {
    _accessibilityContainerView.accessibilityTraits |=
        UIAccessibilityTraitSelected;
  } else {
    _accessibilityContainerView.accessibilityTraits &=
        ~UIAccessibilityTraitSelected;
  }

  if (selected) {
    /// The cell attributes is updated just after the cell selection.
    /// Hide separtors to avoid an animation glitch when selecting/inserting.
    _leadingSeparatorView.hidden = YES;
    _trailingSeparatorView.hidden = YES;
    _leadingSeparatorGradientView.hidden = YES;
    _trailingSeparatorGradientView.hidden = YES;
  }

  [self updateColors];

  // Make the selected cell on top of other cells.
  self.layer.zPosition = selected ? TabStripTabItemConstants.selectedZIndex : 0;

  // Update decoration views visibility.
  _leftTailView.hidden = !selected;
  _rightTailView.hidden = !selected;
  [self setLeadingSelectedBorderBackgroundViewHidden:YES];
  [self setTrailingSelectedBorderBackgroundViewHidden:YES];

  [self updateCollapsedState];
  if (oldSelected != self.selected) {
    [self updateGroupStroke];
  }
}

- (void)setSeparatorsHeight:(CGFloat)height {
  if (_separatorHeight == height) {
    return;
  }
  _separatorHeight = height;

  if (_separatorHeightConstraints) {
    [NSLayoutConstraint deactivateConstraints:_separatorHeightConstraints];
  }
  _separatorHeightConstraints = @[
    [_leadingSeparatorView.heightAnchor constraintEqualToConstant:height],
    [_trailingSeparatorView.heightAnchor constraintEqualToConstant:height],
  ];
  [NSLayoutConstraint activateConstraints:_separatorHeightConstraints];
}

- (void)setTabIndex:(NSInteger)tabIndex {
  if (_tabIndex == tabIndex) {
    return;
  }
  _tabIndex = tabIndex;
  [self updateAccessibilityValue];
}

- (void)setNumberOfTabs:(NSInteger)numberOfTabs {
  if (_numberOfTabs == numberOfTabs) {
    return;
  }
  _numberOfTabs = numberOfTabs;
  [self updateAccessibilityValue];
}

- (void)setIntersectsLeftEdge:(BOOL)intersectsLeftEdge {
  if (super.intersectsLeftEdge != intersectsLeftEdge) {
    super.intersectsLeftEdge = intersectsLeftEdge;
    [self updateGroupStroke];
  }
}

- (void)setIntersectsRightEdge:(BOOL)intersectsRightEdge {
  if (super.intersectsRightEdge != intersectsRightEdge) {
    super.intersectsRightEdge = intersectsRightEdge;
    [self updateGroupStroke];
  }
}

- (void)setCloseButtonVisibility:(CGFloat)visibility {
  CGFloat closeButtonAlpha =
      std::clamp<CGFloat>((visibility - kCloseButtonVisibilityThreshold) /
                              (1 - kCloseButtonVisibilityThreshold),
                          0, 1);
  _closeButton.alpha = closeButtonAlpha;
  // Check if the alpha is low and not just 0 to avoid potential rounding
  // errors.
  _closeButton.hidden = closeButtonAlpha < 0.01;
  _titleTrailingConstraint.constant =
      -kTitleInset + (1 - visibility) * (kCloseButtonSize + kCloseButtonMargin);
}

- (void)setCellVisibility:(CGFloat)visibility {
  _accessibilityContainerView.alpha = visibility;
  _selectedBackground.alpha = visibility;
}

- (void)setHasBlueDot:(BOOL)hasBlueDot {
  if (_hasBlueDot == hasBlueDot) {
    return;
  }

  _hasBlueDot = hasBlueDot;

  if (hasBlueDot) {
    [self showBlueDotView];
  } else {
    [self hideBlueDotView];
  }
}

#pragma mark - UICollectionViewCell

- (void)applyLayoutAttributes:
    (UICollectionViewLayoutAttributes*)layoutAttributes {
  [super applyLayoutAttributes:layoutAttributes];

  [self updateCollapsedState];
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.selected = NO;
  [self setFaviconImage:nil];
  self.item = nil;
  self.numberOfTabs = 0;
  self.tabIndex = 0;
  _accessibilityContainerView.accessibilityValue = nil;
  self.loading = NO;
  self.leadingSeparatorHidden = NO;
  self.trailingSeparatorHidden = NO;
  self.leadingSeparatorGradientViewHidden = NO;
  self.trailingSeparatorGradientViewHidden = NO;
  self.leadingSelectedBorderBackgroundViewHidden = NO;
  self.trailingSelectedBorderBackgroundViewHidden = NO;
  self.isFirstTabInGroup = NO;
  self.isLastTabInGroup = NO;
  self.hasBlueDot = NO;
}

- (void)setHighlighted:(BOOL)highlighted {
  [super setHighlighted:highlighted];
  [self updateColors];
}

- (void)dragStateDidChange:(UICollectionViewCellDragState)dragState {
  [super dragStateDidChange:dragState];
  [self updateColors];
}

#pragma mark - UIAccessibilityAction

- (NSArray*)accessibilityCustomActions {
  return @[ [[UIAccessibilityCustomAction alloc]
      initWithName:l10n_util::GetNSString(IDS_IOS_TAB_SWITCHER_CLOSE_TAB)
            target:self
          selector:@selector(closeButtonTapped:)] ];
}

#pragma mark - UIPointerInteractionDelegate

- (UIPointerRegion*)pointerInteraction:(UIPointerInteraction*)interaction
                      regionForRequest:(UIPointerRegionRequest*)request
                         defaultRegion:(UIPointerRegion*)defaultRegion {
  return defaultRegion;
}

- (void)pointerInteraction:(UIPointerInteraction*)interaction
           willEnterRegion:(UIPointerRegion*)region
                  animator:(id<UIPointerInteractionAnimating>)animator {
  _hovered = YES;
  [self updateColors];
}

- (void)pointerInteraction:(UIPointerInteraction*)interaction
            willExitRegion:(UIPointerRegion*)region
                  animator:(id<UIPointerInteractionAnimating>)animator {
  _hovered = NO;
  [self updateColors];
}

#pragma mark - Private

/// Sets the decoration layers for the `selected` state of the cell.
- (void)setupDecorationLayers {
  // Bottom left corner path.
  UIBezierPath* cornerPath = [UIBezierPath bezierPath];
  [cornerPath moveToPoint:CGPointMake(kCornerSize, kCornerSize)];
  [cornerPath addLineToPoint:CGPointMake(kCornerSize, 0)];
  [cornerPath addArcWithCenter:CGPointMake(0, 0)
                        radius:kCornerSize
                    startAngle:0
                      endAngle:M_PI_2
                     clockwise:YES];
  [cornerPath closePath];

  // Round the left tail.
  CAShapeLayer* leftTailMaskLayer = [CAShapeLayer layer];
  leftTailMaskLayer.path = cornerPath.CGPath;
  _leftTailView.layer.mask = leftTailMaskLayer;

  // Round the right tail.
  CAShapeLayer* rightTailMaskLayer = [CAShapeLayer layer];
  rightTailMaskLayer.path = cornerPath.CGPath;
  _rightTailView.layer.mask = rightTailMaskLayer;
  _rightTailView.transform = CGAffineTransformMakeScale(-1, 1);

  // Setup and mirror separator gradient views if needed.
  _leadingSeparatorGradientView.hidden = YES;
  _trailingSeparatorGradientView.hidden = YES;
  if (UseRTLLayout()) {
    _trailingSeparatorGradientView.transform =
        CGAffineTransformMakeScale(-1, 1);

  } else {
    _leadingSeparatorGradientView.transform = CGAffineTransformMakeScale(-1, 1);
  }
}

// Updates view colors.
- (void)updateColors {
  [UIView setAnimationsEnabled:NO];

  BOOL isSelected = self.isSelected;
  if (self.focused) {
    _selectedBackground.backgroundColor = [UIColor clearColor];
    _accessibilityContainerView.backgroundColor = [UIColor clearColor];
  } else if (self.highlighted || self.configurationState.cellDragState !=
                                     UICellConfigurationDragStateNone) {
    // Before a cell is dragged, it is highlighted.
    // The cell's background color must be updated at this moment, otherwise it
    // will not be applied correctly.
    _selectedBackground.backgroundColor =
        [UIColor colorNamed:kGroupedSecondaryBackgroundColor];
    _accessibilityContainerView.backgroundColor =
        _selectedBackground.backgroundColor;
  } else {
    _selectedBackground.backgroundColor =
        isSelected ? [UIColor colorNamed:kGroupedSecondaryBackgroundColor]
                   : TabStripHelper.cellBackgroundColor;
    _accessibilityContainerView.backgroundColor =
        _hovered ? [UIColor colorNamed:kGrey50Color]
                 : _selectedBackground.backgroundColor;
  }

  UIColor* inactiveColor = [UIColor
      colorWithDynamicProvider:^UIColor*(UITraitCollection* traitCollection) {
        if (traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark) {
          return [UIColor colorNamed:kTextSecondaryColor];
        }
        return [UIColor colorNamed:kTextTertiaryColor];
      }];
  _titleLabel.textColor =
      isSelected ? [UIColor colorNamed:kTextPrimaryColor] : inactiveColor;

  _faviconView.tintColor = self.selected
                               ? [UIColor colorNamed:kCloseButtonColor]
                               : [UIColor colorNamed:kGrey500Color];

  [UIView setAnimationsEnabled:YES];
}

// Hides the close button view if the cell is collapsed.
- (void)updateCollapsedState {
  BOOL collapsed = NO;
  if (self.frame.size.width < kCollapsedWidthThreshold) {
    // Don't hide the close button if the cell is selected.
    collapsed = !self.selected;
  }

  if (collapsed == _closeButton.hidden) {
    return;
  }

  _closeButton.hidden = collapsed;

  // To avoid breaking the layout, always disable the active constraint first.
  if (collapsed) {
    _titleTrailingConstraint.active = NO;
    _titleCollapsedTrailingConstraint.active = YES;
  } else {
    _titleCollapsedTrailingConstraint.active = NO;
    _titleTrailingConstraint.active = YES;
  }
}

// Updates the title gradient and text position horizontal constraints.
- (void)updateTitleConstraints {
  if (_titleLabel.textAlignment == NSTextAlignmentLeft) {
    _titleContainerLeftConstraint.active = NO;
    _titleContainerRightConstraint.active = YES;
    _titleRightConstraint.active = NO;
    _titleLeftConstraint.active = YES;
  } else {
    _titleContainerRightConstraint.active = NO;
    _titleContainerLeftConstraint.active = YES;
    _titleLeftConstraint.active = NO;
    _titleRightConstraint.active = YES;
  }
}

// Updates the `_groupStrokeView` horizontal constraints.
- (void)updateGroupStroke {
  if (!_groupStrokeView.backgroundColor) {
    _groupStrokeView.hidden = YES;
    return;
  }
  _groupStrokeView.hidden = NO;

  const CGFloat lineWidth =
      TabStripCollectionViewConstants.groupStrokeLineWidth;
  if (self.selected) {
    _groupStrokeViewBottomConstraint.active = NO;
    _groupStrokeViewBottomSelectedConstraint.active = YES;
    _groupStrokeViewWidthConstraint.constant = -2 * kCornerSize;
  } else {
    _groupStrokeViewBottomSelectedConstraint.active = NO;
    _groupStrokeViewBottomConstraint.active = YES;
    _groupStrokeViewWidthConstraint.constant = -2 * lineWidth;
  }

  UIBezierPath* path = [UIBezierPath bezierPath];
  CGPoint leftPoint = CGPointZero;
  [path moveToPoint:leftPoint];
  if (self.selected) {
    leftPoint.y += kCornerSize + lineWidth / 2;
    [path addArcWithCenter:leftPoint
                    radius:kCornerSize + lineWidth / 2
                startAngle:M_PI + M_PI_2
                  endAngle:M_PI
                 clockwise:NO];
    leftPoint.x -= kCornerSize + lineWidth / 2;
    leftPoint.y += self.frame.size.height - kCornerSize * 2;
    [path addLineToPoint:leftPoint];
    leftPoint.x -= kCornerSize - lineWidth / 2;
    [path addArcWithCenter:leftPoint
                    radius:kCornerSize - lineWidth / 2
                startAngle:0
                  endAngle:M_PI_2
                 clockwise:YES];
    leftPoint.y += kCornerSize - lineWidth / 2;
    leftPoint.x -= lineWidth;
    [path addLineToPoint:leftPoint];
  }

  UIBezierPath* leftPath = [path copy];
  if (!self.selected) {
    leftPoint.x -= lineWidth;
    if (!self.intersectsRightEdge) {
      leftPoint.x -= TabStripTabItemConstants.horizontalSpacing;
      leftPoint.x -= lineWidth;
    }
    [leftPath addLineToPoint:leftPoint];
  }
  if (self.intersectsLeftEdge) {
    leftPoint.x -= TabStripCollectionViewConstants.groupStrokeExtension;
    [leftPath addLineToPoint:leftPoint];
  }
  leftPoint.y += lineWidth / 2;
  [leftPath addArcWithCenter:leftPoint
                      radius:lineWidth / 2
                  startAngle:M_PI + M_PI_2
                    endAngle:M_PI
                   clockwise:NO];
  [_groupStrokeView setLeadingPath:leftPath.CGPath];

  // The right path starts like the left path, but flipped horizontally.
  [path applyTransform:CGAffineTransformMakeScale(-1, 1)];
  CGPoint rightPoint = path.currentPoint;
  if (!self.isLastTabInGroup && !self.selected) {
    rightPoint.x += lineWidth;
    rightPoint.x += TabStripTabItemConstants.horizontalSpacing;
    rightPoint.x += lineWidth;
    [path addLineToPoint:rightPoint];
  }
  if (self.intersectsRightEdge) {
    rightPoint.x += TabStripCollectionViewConstants.groupStrokeExtension;
    [path addLineToPoint:rightPoint];
  }
  rightPoint.y += lineWidth / 2;
  [path addArcWithCenter:rightPoint
                  radius:lineWidth / 2
              startAngle:M_PI + M_PI_2
                endAngle:0
               clockwise:YES];
  [_groupStrokeView setTrailingPath:path.CGPath];
}

// Sets the cell constraints.
- (void)setupConstraints {
  UILayoutGuide* leadingImageGuide = [[UILayoutGuide alloc] init];
  [self addLayoutGuide:leadingImageGuide];

  UIView* contentView = self.contentView;

  /// `contentView` constraints.
  [NSLayoutConstraint activateConstraints:@[
    [_accessibilityContainerView.leadingAnchor
        constraintEqualToAnchor:contentView.leadingAnchor],
    [_accessibilityContainerView.trailingAnchor
        constraintEqualToAnchor:contentView.trailingAnchor],
    [_accessibilityContainerView.topAnchor
        constraintEqualToAnchor:contentView.topAnchor],
    [_accessibilityContainerView.bottomAnchor
        constraintEqualToAnchor:contentView.bottomAnchor
                       constant:-kContentViewBottomInset]
  ]];

  /// `leadingImageGuide` constraints.
  [NSLayoutConstraint activateConstraints:@[
    [leadingImageGuide.leadingAnchor
        constraintEqualToAnchor:_accessibilityContainerView.leadingAnchor
                       constant:kFaviconLeadingMargin],
    [leadingImageGuide.centerYAnchor
        constraintEqualToAnchor:_accessibilityContainerView.centerYAnchor],
    [leadingImageGuide.widthAnchor constraintEqualToConstant:kFaviconSize],
    [leadingImageGuide.heightAnchor
        constraintEqualToAnchor:leadingImageGuide.widthAnchor],
  ]];
  AddSameConstraints(leadingImageGuide, _faviconView);
  AddSameConstraints(leadingImageGuide, _activityIndicator);

  /// `_closeButton` constraints.
  [NSLayoutConstraint activateConstraints:@[
    [_closeButton.trailingAnchor
        constraintEqualToAnchor:_accessibilityContainerView.trailingAnchor
                       constant:-kCloseButtonMargin],
    [_closeButton.widthAnchor constraintEqualToConstant:kCloseButtonSize],
    [_closeButton.heightAnchor constraintEqualToConstant:kCloseButtonSize],
    [_closeButton.centerYAnchor
        constraintEqualToAnchor:_accessibilityContainerView.centerYAnchor],
  ]];

  /// `_titleLabel` constraints.
  _titleTrailingConstraint = [_titlePositioner.trailingAnchor
      constraintEqualToAnchor:_closeButton.leadingAnchor
                     constant:-kTitleInset];
  _titleCollapsedTrailingConstraint = [_titlePositioner.trailingAnchor
      constraintEqualToAnchor:_accessibilityContainerView.trailingAnchor
                     constant:-kTitleInset];
  // The trailing constraints have a lower priority to allow the cell to have a
  // size of 0.
  _titleTrailingConstraint.priority = UILayoutPriorityRequired - 1;
  _titleCollapsedTrailingConstraint.priority = UILayoutPriorityRequired - 1;
  _titleLeftConstraint = [_titleLabel.leftAnchor
      constraintEqualToAnchor:_titlePositioner.leftAnchor];
  _titleRightConstraint = [_titleLabel.rightAnchor
      constraintEqualToAnchor:_titlePositioner.rightAnchor];
  _titleContainerLeftConstraint = [_titleContainer.leftAnchor
      constraintEqualToAnchor:_titlePositioner.leftAnchor];
  _titleContainerRightConstraint = [_titleContainer.rightAnchor
      constraintEqualToAnchor:_titlePositioner.rightAnchor];
  [NSLayoutConstraint activateConstraints:@[
    [_titlePositioner.leadingAnchor
        constraintEqualToAnchor:leadingImageGuide.trailingAnchor
                       constant:kTitleInset],
    _titleTrailingConstraint,
    _titleLeftConstraint,
    [_titleLabel.centerYAnchor
        constraintEqualToAnchor:_accessibilityContainerView.centerYAnchor],
    [_titleLabel.widthAnchor
        constraintEqualToAnchor:_titlePositioner.widthAnchor
                       constant:kTitleOverflowWidth],
    [_titleContainer.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
  ]];

  /// `_leadingSelectedBorderBackgroundView` and
  /// `_trailingSelectedBorderBackgroundView constraints.
  [NSLayoutConstraint activateConstraints:@[
    [_leadingSelectedBorderBackgroundView.trailingAnchor
        constraintEqualToAnchor:_accessibilityContainerView.leadingAnchor],
    [_leadingSelectedBorderBackgroundView.widthAnchor
        constraintEqualToConstant:kSelectedBorderBackgroundViewWidth],
    [_leadingSelectedBorderBackgroundView.heightAnchor
        constraintEqualToAnchor:_accessibilityContainerView.heightAnchor],
    [_leadingSelectedBorderBackgroundView.centerYAnchor
        constraintEqualToAnchor:_accessibilityContainerView.centerYAnchor],

    [_trailingSelectedBorderBackgroundView.leadingAnchor
        constraintEqualToAnchor:_accessibilityContainerView.trailingAnchor],
    [_trailingSelectedBorderBackgroundView.widthAnchor
        constraintEqualToConstant:kSelectedBorderBackgroundViewWidth],
    [_trailingSelectedBorderBackgroundView.heightAnchor
        constraintEqualToAnchor:_accessibilityContainerView.heightAnchor],
    [_trailingSelectedBorderBackgroundView.centerYAnchor
        constraintEqualToAnchor:_accessibilityContainerView.centerYAnchor],
  ]];

  /// `_leftTailView`, `_rightTailView` and `_selectedBackground` constraints.
  [NSLayoutConstraint activateConstraints:@[
    [_leftTailView.rightAnchor
        constraintEqualToAnchor:_selectedBackground.leftAnchor],
    [_leftTailView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
    [_leftTailView.widthAnchor constraintEqualToConstant:kCornerSize],
    [_leftTailView.heightAnchor constraintEqualToConstant:kCornerSize],

    [_rightTailView.leftAnchor
        constraintEqualToAnchor:_selectedBackground.rightAnchor],
    [_rightTailView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
    [_rightTailView.widthAnchor constraintEqualToConstant:kCornerSize],
    [_rightTailView.heightAnchor constraintEqualToConstant:kCornerSize],
  ]];

  [NSLayoutConstraint activateConstraints:@[
    [_selectedBackground.topAnchor
        constraintEqualToAnchor:_accessibilityContainerView.topAnchor],
    [_selectedBackground.trailingAnchor
        constraintEqualToAnchor:_accessibilityContainerView.trailingAnchor],
    [_selectedBackground.leadingAnchor
        constraintEqualToAnchor:_accessibilityContainerView.leadingAnchor],
    [_selectedBackground.bottomAnchor
        constraintEqualToAnchor:self.bottomAnchor],
  ]];

  /// `_leadingSeparatorView` constraints.
  [NSLayoutConstraint activateConstraints:@[
    [_leadingSeparatorView.trailingAnchor
        constraintEqualToAnchor:contentView.leadingAnchor
                       constant:-kSeparatorHorizontalInset],
    [_leadingSeparatorView.widthAnchor
        constraintEqualToConstant:TabStripStaticSeparatorConstants
                                      .separatorWidth],
    [_leadingSeparatorView.centerYAnchor
        constraintEqualToAnchor:_closeButton.centerYAnchor],
  ]];

  /// `_trailingSeparatorView` constraints.
  [NSLayoutConstraint activateConstraints:@[
    [_trailingSeparatorView.leadingAnchor
        constraintEqualToAnchor:contentView.trailingAnchor
                       constant:kSeparatorHorizontalInset],
    [_trailingSeparatorView.widthAnchor
        constraintEqualToConstant:TabStripStaticSeparatorConstants
                                      .separatorWidth],
    [_trailingSeparatorView.centerYAnchor
        constraintEqualToAnchor:_closeButton.centerYAnchor],
  ]];

  [self setSeparatorsHeight:TabStripStaticSeparatorConstants
                                .regularSeparatorHeight];

  /// `_leadingSeparatorGradientView` constraints.
  [NSLayoutConstraint activateConstraints:@[
    [_leadingSeparatorGradientView.leadingAnchor
        constraintEqualToAnchor:_leadingSeparatorView.trailingAnchor],
    [_leadingSeparatorGradientView.widthAnchor
        constraintEqualToConstant:kSeparatorGradientWidth],
    [_leadingSeparatorGradientView.heightAnchor
        constraintEqualToAnchor:_accessibilityContainerView.heightAnchor],
    [_leadingSeparatorGradientView.centerYAnchor
        constraintEqualToAnchor:_accessibilityContainerView.centerYAnchor],
  ]];

  /// `_trailingSeparatorGradientView` constraints.
  [NSLayoutConstraint activateConstraints:@[
    [_trailingSeparatorGradientView.trailingAnchor
        constraintEqualToAnchor:_trailingSeparatorView.leadingAnchor],
    [_trailingSeparatorGradientView.widthAnchor
        constraintEqualToConstant:kSeparatorGradientWidth],
    [_trailingSeparatorGradientView.heightAnchor
        constraintEqualToAnchor:_accessibilityContainerView.heightAnchor],
    [_trailingSeparatorGradientView.centerYAnchor
        constraintEqualToAnchor:_accessibilityContainerView.centerYAnchor],
  ]];

  /// `_groupStrokeView` constraints.
  _groupStrokeViewBottomConstraint =
      [_groupStrokeView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor];
  _groupStrokeViewBottomConstraint.active = YES;
  _groupStrokeViewBottomSelectedConstraint =
      [_groupStrokeView.bottomAnchor constraintEqualToAnchor:self.topAnchor];
  _groupStrokeViewWidthConstraint =
      [_groupStrokeView.widthAnchor constraintEqualToAnchor:self.widthAnchor];
  _groupStrokeViewWidthConstraint.priority = UILayoutPriorityDefaultHigh;
  _groupStrokeViewWidthConstraint.active = YES;
  AddSameCenterXConstraint(_groupStrokeView, self);
}

// Selector registered to the close button.
- (void)closeButtonTapped:(id)sender {
  base::RecordAction(base::UserMetricsAction("MobileTabStripCloseTab"));
  [self.delegate closeButtonTappedForCell:self];
}

// Returns a new favicon view.
- (UIImageView*)createFaviconView {
  UIImageView* faviconView = [[UIImageView alloc] init];
  faviconView.translatesAutoresizingMaskIntoConstraints = NO;
  faviconView.contentMode = UIViewContentModeScaleAspectFit;
  return faviconView;
}

// Returns a new close button.
- (UIButton*)createCloseButton {
  UIImage* closeSymbol =
      DefaultSymbolWithPointSize(kXMarkSymbol, kCloseButtonSize);
  ExtendedTouchTargetButton* button = [[ExtendedTouchTargetButton alloc] init];
  button.minimumDiameter = kCloseButtonMinimumTouchTarget;
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.tintColor = [UIColor colorNamed:kTextSecondaryColor];
  [button setImage:closeSymbol forState:UIControlStateNormal];
  [button addTarget:self
                action:@selector(closeButtonTapped:)
      forControlEvents:UIControlEventTouchUpInside];
  button.pointerInteractionEnabled = YES;
  button.accessibilityIdentifier =
      TabStripTabItemConstants.closeButtonAccessibilityIdentifier;
  return button;
}

// Returns a new title label.
- (UILabel*)createTitleLabel {
  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  titleLabel.font = [UIFont systemFontOfSize:TabStripTabItemConstants.fontSize
                                      weight:UIFontWeightMedium];
  titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  titleLabel.adjustsFontForContentSizeCategory = YES;

  return titleLabel;
}

// Returns a new gradient view.
- (GradientView*)createGradientView {
  GradientView* gradientView = [[GradientView alloc]
      initWithStartColor:[TabStripHelper.cellBackgroundColor
                             colorWithAlphaComponent:0]
                endColor:TabStripHelper.cellBackgroundColor
              startPoint:CGPointMake(0.0f, 0.5f)
                endPoint:CGPointMake(1.0f, 0.5f)];
  gradientView.translatesAutoresizingMaskIntoConstraints = NO;
  return gradientView;
}

// Returns a new title container view.
- (UIView*)createTitleContainer {
  UIView* titleContainer = [[UIView alloc] init];
  titleContainer.translatesAutoresizingMaskIntoConstraints = NO;
  titleContainer.clipsToBounds = YES;

  CGFloat cellMaxWidth = TabStripTabItemConstants.maxWidth;
  // The percentage of width of the cell to have a gradient of
  // `kTitleGradientWidth` width on both sides.
  CGFloat gradientPercentage = kTitleGradientWidth / cellMaxWidth;

  CAGradientLayer* gradientMask = [[CAGradientLayer alloc] init];
  gradientMask.frame =
      CGRectMake(0, 0, cellMaxWidth, TabStripCollectionViewConstants.height);
  [NSLayoutConstraint activateConstraints:@[
    [titleContainer.widthAnchor constraintEqualToConstant:cellMaxWidth],
    [titleContainer.heightAnchor
        constraintEqualToConstant:TabStripCollectionViewConstants.height],
  ]];
  gradientMask.colors = @[
    (id)UIColor.clearColor.CGColor, (id)UIColor.blackColor.CGColor,
    (id)UIColor.blackColor.CGColor, (id)UIColor.clearColor.CGColor
  ];
  gradientMask.startPoint = CGPointMake(0, 0.5);
  gradientMask.endPoint = CGPointMake(1, 0.5);
  gradientMask.locations =
      @[ @(0), @(gradientPercentage), @(1 - gradientPercentage), @(1) ];

  titleContainer.layer.mask = gradientMask;

  _titleLabel = [self createTitleLabel];
  [titleContainer addSubview:_titleLabel];

  return titleContainer;
}

// Returns a new Activity Indicator.
- (UIActivityIndicatorView*)createActivityIndicatior {
  UIActivityIndicatorView* activityIndicator =
      [[UIActivityIndicatorView alloc] init];
  activityIndicator.color = [UIColor colorNamed:kBlueColor];
  activityIndicator.transform = CGAffineTransformScale(
      activityIndicator.transform, kIndicatorScale, kIndicatorScale);
  activityIndicator.translatesAutoresizingMaskIntoConstraints = NO;
  return activityIndicator;
}

// Returns a new decoration view.
- (UIView*)createDecorationView {
  UIView* tailView = [[UIView alloc] init];
  tailView.backgroundColor =
      [UIColor colorNamed:kGroupedSecondaryBackgroundColor];
  tailView.translatesAutoresizingMaskIntoConstraints = NO;
  tailView.clipsToBounds = NO;
  tailView.hidden = YES;
  return tailView;
}

// Returns a new separator view.
- (UIView*)createSeparatorView {
  UIView* separatorView = [[UIView alloc] init];
  separatorView.backgroundColor = TabStripHelper.backgroundColor;
  separatorView.translatesAutoresizingMaskIntoConstraints = NO;
  separatorView.layer.cornerRadius =
      TabStripStaticSeparatorConstants.separatorCornerRadius;
  separatorView.layer.masksToBounds = YES;

  UIView* backgroundView = [[UIView alloc] init];
  backgroundView.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  backgroundView.backgroundColor = [UIColor colorNamed:kTextQuaternaryColor];
  [separatorView addSubview:backgroundView];

  return separatorView;
}

// Returns a new selected border background view.
- (UIView*)createSelectedBorderBackgroundView {
  UIView* backgroundView = [[UIView alloc] init];
  backgroundView.backgroundColor = TabStripHelper.backgroundColor;
  backgroundView.translatesAutoresizingMaskIntoConstraints = NO;
  backgroundView.hidden = YES;
  return backgroundView;
}

// Returns a new background view for the selected cell.
- (UIView*)createSelectedBackgroundView {
  UIView* selectedBackground = [[UIView alloc] init];
  selectedBackground.translatesAutoresizingMaskIntoConstraints = NO;
  selectedBackground.layer.cornerRadius = kCornerSize;
  selectedBackground.layer.maskedCorners =
      kCALayerMaxXMinYCorner | kCALayerMinXMinYCorner;
  return selectedBackground;
}

- (void)updateAccessibilityValue {
  // Use the accessibility Value as there is a pause when using the
  // accessibility hint.
  BOOL grouped = self.groupStrokeColor != nil;
  _accessibilityContainerView.accessibilityValue = l10n_util::GetNSStringF(
      grouped ? IDS_IOS_TAB_STRIP_TAB_CELL_IN_GROUP_VOICE_OVER_VALUE
              : IDS_IOS_TAB_STRIP_TAB_CELL_VOICE_OVER_VALUE,
      base::NumberToString16(self.tabIndex),
      base::NumberToString16(self.numberOfTabs));
}

// Shows the blue dot view. Adds the view to the cell if there is none yet.
- (void)showBlueDotView {
  if (_blueDotView) {
    _blueDotView.hidden = NO;
    return;
  }

  _blueDotView = [[UIView alloc] init];
  _blueDotView.translatesAutoresizingMaskIntoConstraints = NO;
  _blueDotView.layer.cornerRadius = kBlueDotSize / 2;
  _blueDotView.layer.borderWidth = kBlueDotStrokeWidth;
  _blueDotView.layer.borderColor = TabStripHelper.cellBackgroundColor.CGColor;
  _blueDotView.backgroundColor = [UIColor colorNamed:kBlue600Color];
  _blueDotView.accessibilityIdentifier =
      TabStripTabItemConstants.blueDotAccessibilityIdentifier;
  [_accessibilityContainerView addSubview:_blueDotView];

  [NSLayoutConstraint activateConstraints:@[
    [_blueDotView.widthAnchor constraintEqualToConstant:kBlueDotSize],
    [_blueDotView.heightAnchor constraintEqualToConstant:kBlueDotSize],
    // Position the blue dot at right bottom corner of the favicon image.
    [_blueDotView.centerXAnchor
        constraintEqualToAnchor:_faviconView.centerXAnchor
                       constant:kFaviconSize / 2 - kBlueDotInset],
    [_blueDotView.centerYAnchor
        constraintEqualToAnchor:_faviconView.centerYAnchor
                       constant:kFaviconSize / 2 - kBlueDotInset],
  ]];
}

// Hides the blue dot view.
- (void)hideBlueDotView {
  _blueDotView.hidden = YES;
}

- (void)handleFocusUpdate:(NSNotification*)notification {
  UIFocusUpdateContext* context =
      notification.userInfo[UIFocusUpdateContextKey];
  if (context.nextFocusedView == self ||
      context.previouslyFocusedView == self) {
    [self updateColors];
  }
}

@end
