// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/ui/tab_strip_tab_cell.h"

#import <MaterialComponents/MaterialActivityIndicator.h>

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "base/strings/string_number_conversions.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/image/image_util.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/ui/swift_constants_for_objective_c.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/gradient_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/raccoon/raccoon_api.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The size of the close button.
const CGFloat kCloseButtonSize = 16;
// The alpha of the close button background color.
const CGFloat kCloseButtonBackgroundAlpha = 0.2;

// Size of the decoration corner when the cell is selected.
const CGFloat kCornerSize = 16;

// Threshold width for collapsing the cell and hiding the close button.
const CGFloat kCollapsedWidthThreshold = 150;

// Separator constraints.
const CGFloat kSeparatorHorizontalInset = 2;
const CGFloat kSeparatorGradientWidth = 4;

// Content view constants.
const CGFloat kFaviconLeadingMargin = 10;
const CGFloat kCloseButtonMargin = 10;
const CGFloat kTitleInset = 10;
const CGFloat kFaviconSize = 16;
const CGFloat kTitleGradientWidth = 16;
const CGFloat kContentViewBottomInset = 4;

// Selected border background view constants.
const CGFloat kSelectedBorderBackgroundViewWidth = 8;

// Returns the default favicon image.
UIImage* DefaultFavicon() {
  return DefaultSymbolWithPointSize(kGlobeAmericasSymbol, 14);
}

}  // namespace

@implementation TabStripTabCell {
  // Content view subviews.
  UIButton* _closeButton;
  UIView* _titleContainer;
  UILabel* _titleLabel;
  GradientView* _titleGradientView;
  UIImageView* _faviconView;

  // Decoration views, visible when the cell is selected.
  UIView* _leftTailView;
  UIView* _rightTailView;
  UIView* _bottomTailView;

  // Cell separator.
  UIView* _leadingSeparatorView;
  UIView* _trailingSeparatorView;
  UIView* _leadingSeparatorGradientView;
  UIView* _trailingSeparatorGradientView;

  // Background views displayed when the selected cell in on an edge.
  UIView* _leftSelectedBorderBackgroundView;
  UIView* _rightSelectedBorderBackgroundView;

  // Wether the decoration layers have been updated.
  BOOL _decorationLayersUpdated;

  // Circular spinner that shows the loading state of the tab.
  MDCActivityIndicator* _activityIndicator;

  // Title label's trailing constraints.
  NSLayoutConstraint* _titleContainerCollapsedTrailingConstraint;
  NSLayoutConstraint* _titleContainerTrailingConstraint;

  // Gradient view's constraints.
  NSLayoutConstraint* _titleGradientViewLeadingConstraint;
  NSLayoutConstraint* _titleGradientViewTrailingConstraint;

  // Separator height constraints.
  NSArray<NSLayoutConstraint*>* _separatorHeightConstraints;
  CGFloat _separatorHeight;

  // whether the view is hovered.
  BOOL _hovered;

  // View used to provide accessibility labels/values while letting the close
  // button selectable by VoiceOver.
  UIView* _accessibilityContainerView;
}

- (instancetype)initWithFrame:(CGRect)frame {
  if ((self = [super initWithFrame:frame])) {
    self.layer.masksToBounds = NO;
    _decorationLayersUpdated = NO;
    _hovered = NO;
    _separatorHeight = 0;

    [self addInteraction:[[UIPointerInteraction alloc] initWithDelegate:self]];

    if (ios::provider::IsRaccoonEnabled()) {
      if (@available(iOS 17.0, *)) {
        self.hoverStyle = [UIHoverStyle
            styleWithShape:[UIShape rectShapeWithCornerRadius:kCornerSize]];
      }
    }

    UIView* contentView = self.contentView;
    contentView.layer.masksToBounds = YES;
    contentView.layer.cornerRadius = kCornerSize;
    contentView.translatesAutoresizingMaskIntoConstraints = NO;

    _accessibilityContainerView = [[UIView alloc] init];
    _accessibilityContainerView.isAccessibilityElement = YES;
    _accessibilityContainerView.translatesAutoresizingMaskIntoConstraints = NO;
    [contentView addSubview:_accessibilityContainerView];
    AddSameConstraints(contentView, _accessibilityContainerView);

    // Needed for the drop animation.
    self.layer.cornerRadius = kCornerSize;
    self.backgroundColor = [UIColor colorNamed:kGroupedPrimaryBackgroundColor];

    _faviconView = [self createFaviconView];
    [_accessibilityContainerView addSubview:_faviconView];

    _activityIndicator = [self createActivityIndicatior];
    [_accessibilityContainerView addSubview:_activityIndicator];

    _closeButton = [self createCloseButton];
    [contentView addSubview:_closeButton];

    _titleContainer = [self createTitleContainer];
    [_accessibilityContainerView addSubview:_titleContainer];

    _leftSelectedBorderBackgroundView =
        [self createSelectedBorderBackgroundView];
    [self addSubview:_leftSelectedBorderBackgroundView];

    _rightSelectedBorderBackgroundView =
        [self createSelectedBorderBackgroundView];
    [self addSubview:_rightSelectedBorderBackgroundView];

    _leftTailView = [self createDecorationView];
    [self addSubview:_leftTailView];

    _rightTailView = [self createDecorationView];
    [self addSubview:_rightTailView];

    _bottomTailView = [self createDecorationView];
    [self insertSubview:_bottomTailView belowSubview:contentView];

    _leadingSeparatorView = [self createSeparatorView];
    [self addSubview:_leadingSeparatorView];

    _trailingSeparatorView = [self createSeparatorView];
    [self addSubview:_trailingSeparatorView];

    _leadingSeparatorGradientView = [self createGradientView];
    [self addSubview:_leadingSeparatorGradientView];

    _trailingSeparatorGradientView = [self createGradientView];
    [self addSubview:_trailingSeparatorGradientView];

    [self setupConstraints];
    [self setupDecorationLayers];

    self.selected = NO;
  }
  return self;
}

- (void)setFaviconImage:(UIImage*)image {
  if (!image) {
    _faviconView.image = DefaultFavicon();
  } else {
    _faviconView.image = image;
  }
}

- (UIDragPreviewParameters*)dragPreviewParameters {
  UIBezierPath* visiblePath =
      [UIBezierPath bezierPathWithRoundedRect:self.contentView.bounds
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
  [self updateTitleGradientViewConstraints];
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
    _faviconView.image = DefaultFavicon();
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

- (void)setLeftSelectedBorderBackgroundViewHidden:
    (BOOL)leftSelectedBorderBackgroundViewHidden {
  _leftSelectedBorderBackgroundViewHidden =
      leftSelectedBorderBackgroundViewHidden;
  _leftSelectedBorderBackgroundView.hidden =
      leftSelectedBorderBackgroundViewHidden;
}

- (void)setRightSelectedBorderBackgroundViewHidden:
    (BOOL)rightSelectedBorderBackgroundViewHidden {
  _rightSelectedBorderBackgroundViewHidden =
      rightSelectedBorderBackgroundViewHidden;
  _rightSelectedBorderBackgroundView.hidden =
      rightSelectedBorderBackgroundViewHidden;
}

- (void)setSelected:(BOOL)selected {
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
  _bottomTailView.hidden = !selected;
  [self setLeftSelectedBorderBackgroundViewHidden:YES];
  [self setRightSelectedBorderBackgroundViewHidden:YES];

  [self updateCollapsedState];
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
}

- (void)setHighlighted:(BOOL)highlighted {
  [super setHighlighted:highlighted];
  [self updateColors];
}

- (void)dragStateDidChange:(UICollectionViewCellDragState)dragState {
  [super dragStateDidChange:dragState];
  [self updateColors];
}

#pragma mark - UITraitEnvironment

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  [self updateColors];
}

#pragma mark - UIAccessibility

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

  _decorationLayersUpdated = YES;
}

// Updates view colors.
- (void)updateColors {
  UIColor* backgroundColor;
  if (self.isHighlighted || self.configurationState.cellDragState !=
                                UICellConfigurationDragStateNone) {
    // Before a cell is dragged, it is highlighted.
    // The cell's background color must be updated at this moment, otherwise it
    // will not be applied correctly.
    backgroundColor = [UIColor colorNamed:kGroupedSecondaryBackgroundColor];
  } else if (_hovered) {
    backgroundColor = [UIColor colorNamed:kUpdatedTertiaryBackgroundColor];
  } else {
    backgroundColor =
        self.isSelected ? [UIColor colorNamed:kGroupedSecondaryBackgroundColor]
                        : [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
  }

  // Needed to correctly update the `_titleGradientView` colors in incognito.
  backgroundColor =
      [backgroundColor resolvedColorWithTraitCollection:self.traitCollection];

  self.contentView.backgroundColor = backgroundColor;
  _faviconView.tintColor = self.selected
                               ? [UIColor colorNamed:kCloseButtonColor]
                               : [UIColor colorNamed:kGrey500Color];
  [_titleGradientView setStartColor:[backgroundColor colorWithAlphaComponent:0]
                           endColor:backgroundColor];
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
    _titleContainerTrailingConstraint.active = NO;
    _titleContainerCollapsedTrailingConstraint.active = YES;
  } else {
    _titleContainerCollapsedTrailingConstraint.active = NO;
    _titleContainerTrailingConstraint.active = YES;
  }
}

// Updates the `_titleGradientView` horizontal constraints.
- (void)updateTitleGradientViewConstraints {
  NSTextAlignment titleTextAligment = _titleLabel.textAlignment;

  // To avoid breaking the layout, always disable the active constraint first.
  if (UseRTLLayout()) {
    if (titleTextAligment == NSTextAlignmentLeft) {
      [_titleGradientView setTransform:CGAffineTransformMakeScale(1, 1)];
      _titleGradientViewTrailingConstraint.active = NO;
      _titleGradientViewLeadingConstraint.active = YES;
    } else {
      [_titleGradientView setTransform:CGAffineTransformMakeScale(-1, 1)];
      _titleGradientViewLeadingConstraint.active = NO;
      _titleGradientViewTrailingConstraint.active = YES;
    }
  } else {
    if (titleTextAligment == NSTextAlignmentLeft) {
      [_titleGradientView setTransform:CGAffineTransformMakeScale(1, 1)];
      _titleGradientViewLeadingConstraint.active = NO;
      _titleGradientViewTrailingConstraint.active = YES;
    } else {
      [_titleGradientView setTransform:CGAffineTransformMakeScale(-1, 1)];
      _titleGradientViewTrailingConstraint.active = NO;
      _titleGradientViewLeadingConstraint.active = YES;
    }
  }
}

// Sets the cell constraints.
- (void)setupConstraints {
  UILayoutGuide* leadingImageGuide = [[UILayoutGuide alloc] init];
  [self addLayoutGuide:leadingImageGuide];

  UIView* contentView = self.contentView;

  /// `contentView` constraints.
  [NSLayoutConstraint activateConstraints:@[
    [contentView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
    [contentView.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
    [contentView.topAnchor constraintEqualToAnchor:self.topAnchor],
    [contentView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor
                                             constant:-kContentViewBottomInset]
  ]];

  /// `leadingImageGuide` constraints.
  [NSLayoutConstraint activateConstraints:@[
    [leadingImageGuide.leadingAnchor
        constraintEqualToAnchor:contentView.leadingAnchor
                       constant:kFaviconLeadingMargin],
    [leadingImageGuide.centerYAnchor
        constraintEqualToAnchor:contentView.centerYAnchor],
    [leadingImageGuide.widthAnchor constraintEqualToConstant:kFaviconSize],
    [leadingImageGuide.heightAnchor
        constraintEqualToAnchor:leadingImageGuide.widthAnchor],
  ]];
  AddSameConstraints(leadingImageGuide, _faviconView);
  AddSameConstraints(leadingImageGuide, _activityIndicator);

  /// `_closeButton` constraints.
  [NSLayoutConstraint activateConstraints:@[
    [_closeButton.trailingAnchor
        constraintEqualToAnchor:contentView.trailingAnchor
                       constant:-kCloseButtonMargin],
    [_closeButton.widthAnchor constraintEqualToConstant:kCloseButtonSize],
    [_closeButton.heightAnchor constraintEqualToConstant:kCloseButtonSize],
    [_closeButton.centerYAnchor
        constraintEqualToAnchor:contentView.centerYAnchor],
  ]];

  /// `_titleLabel` constraints.
  _titleContainerTrailingConstraint = [_titleContainer.trailingAnchor
      constraintEqualToAnchor:_closeButton.leadingAnchor
                     constant:-kTitleInset];
  _titleContainerTrailingConstraint.priority = UILayoutPriorityDefaultLow;
  _titleContainerCollapsedTrailingConstraint = [_titleContainer.trailingAnchor
      constraintEqualToAnchor:contentView.trailingAnchor
                     constant:-kTitleInset];
  _titleContainerCollapsedTrailingConstraint.priority =
      UILayoutPriorityDefaultLow;
  [NSLayoutConstraint activateConstraints:@[
    [_titleContainer.leadingAnchor
        constraintEqualToAnchor:leadingImageGuide.trailingAnchor
                       constant:kTitleInset],
    _titleContainerTrailingConstraint,
    [_titleContainer.heightAnchor
        constraintEqualToAnchor:contentView.heightAnchor],
    [_titleContainer.centerYAnchor
        constraintEqualToAnchor:contentView.centerYAnchor],

    [_titleLabel.leadingAnchor
        constraintEqualToAnchor:_titleContainer.leadingAnchor],
    [_titleLabel.centerYAnchor
        constraintEqualToAnchor:_titleContainer.centerYAnchor],
  ]];

  /// `_titleGradientView` constraints.
  _titleGradientViewLeadingConstraint = [_titleGradientView.leadingAnchor
      constraintEqualToAnchor:_titleContainer.leadingAnchor];
  _titleGradientViewTrailingConstraint = [_titleGradientView.trailingAnchor
      constraintEqualToAnchor:_titleContainer.trailingAnchor];
  [NSLayoutConstraint activateConstraints:@[
    _titleGradientViewTrailingConstraint,
    [_titleGradientView.widthAnchor
        constraintEqualToConstant:kTitleGradientWidth],
    [_titleGradientView.heightAnchor
        constraintEqualToAnchor:_titleContainer.heightAnchor],
    [_titleGradientView.centerYAnchor
        constraintEqualToAnchor:_titleContainer.centerYAnchor],
  ]];


  /// `_leftSelectedBorderBackgroundView` and
  /// `_rightSelectedBorderBackgroundView constraints.
  [NSLayoutConstraint activateConstraints:@[
    [_leftSelectedBorderBackgroundView.rightAnchor
        constraintEqualToAnchor:contentView.leftAnchor],
    [_leftSelectedBorderBackgroundView.widthAnchor
        constraintEqualToConstant:kSelectedBorderBackgroundViewWidth],
    [_leftSelectedBorderBackgroundView.heightAnchor
        constraintEqualToAnchor:contentView.heightAnchor],
    [_leftSelectedBorderBackgroundView.centerYAnchor
        constraintEqualToAnchor:contentView.centerYAnchor],

    [_rightSelectedBorderBackgroundView.leftAnchor
        constraintEqualToAnchor:contentView.rightAnchor],
    [_rightSelectedBorderBackgroundView.widthAnchor
        constraintEqualToConstant:kSelectedBorderBackgroundViewWidth],
    [_rightSelectedBorderBackgroundView.heightAnchor
        constraintEqualToAnchor:contentView.heightAnchor],
    [_rightSelectedBorderBackgroundView.centerYAnchor
        constraintEqualToAnchor:contentView.centerYAnchor],
  ]];

  /// `_leftTailView`, `_rightTailView` and `_bottomTailView` constraints.
  [NSLayoutConstraint activateConstraints:@[
    [_leftTailView.rightAnchor constraintEqualToAnchor:contentView.leftAnchor],
    [_leftTailView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
    [_leftTailView.widthAnchor constraintEqualToConstant:kCornerSize],
    [_leftTailView.heightAnchor constraintEqualToConstant:kCornerSize],

    [_rightTailView.leftAnchor constraintEqualToAnchor:contentView.rightAnchor],
    [_rightTailView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
    [_rightTailView.widthAnchor constraintEqualToConstant:kCornerSize],
    [_rightTailView.heightAnchor constraintEqualToConstant:kCornerSize],

    [_bottomTailView.leadingAnchor
        constraintEqualToAnchor:contentView.leadingAnchor],
    [_bottomTailView.trailingAnchor
        constraintEqualToAnchor:contentView.trailingAnchor],
    [_bottomTailView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
    [_bottomTailView.heightAnchor constraintEqualToConstant:kCornerSize],
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
        constraintEqualToAnchor:contentView.centerYAnchor],
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
        constraintEqualToAnchor:contentView.centerYAnchor],
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
        constraintEqualToAnchor:contentView.heightAnchor],
    [_leadingSeparatorGradientView.centerYAnchor
        constraintEqualToAnchor:contentView.centerYAnchor],
  ]];

  /// `_trailingSeparatorGradientView` constraints.
  [NSLayoutConstraint activateConstraints:@[
    [_trailingSeparatorGradientView.trailingAnchor
        constraintEqualToAnchor:_trailingSeparatorView.leadingAnchor],
    [_trailingSeparatorGradientView.widthAnchor
        constraintEqualToConstant:kSeparatorGradientWidth],
    [_trailingSeparatorGradientView.heightAnchor
        constraintEqualToAnchor:contentView.heightAnchor],
    [_trailingSeparatorGradientView.centerYAnchor
        constraintEqualToAnchor:contentView.centerYAnchor],
  ]];
}

// Selector registered to the close button.
- (void)closeButtonTapped:(id)sender {
  base::RecordAction(base::UserMetricsAction("MobileTabStripCloseTab"));
  [self.delegate closeButtonTappedForCell:self];
}

// Returns a new favicon view.
- (UIImageView*)createFaviconView {
  UIImageView* faviconView =
      [[UIImageView alloc] initWithImage:DefaultFavicon()];
  faviconView.translatesAutoresizingMaskIntoConstraints = NO;
  faviconView.contentMode = UIViewContentModeScaleAspectFit;
  return faviconView;
}

// Returns a new close button.
- (UIButton*)createCloseButton {
  UIImage* closeSymbol =
      DefaultSymbolWithPointSize(kXMarkSymbol, kCloseButtonSize);
  UIButton* closeButton = [UIButton buttonWithType:UIButtonTypeCustom];
  closeButton.translatesAutoresizingMaskIntoConstraints = NO;
  [closeButton
      setImage:SymbolWithPalette(
                   closeSymbol,
                   @[
                     [UIColor colorNamed:kTextSecondaryColor],
                     [[UIColor colorNamed:kTextQuaternaryColor]
                         colorWithAlphaComponent:kCloseButtonBackgroundAlpha]
                   ])
      forState:UIControlStateNormal];
  [closeButton addTarget:self
                  action:@selector(closeButtonTapped:)
        forControlEvents:UIControlEventTouchUpInside];
  closeButton.pointerInteractionEnabled = YES;
  return closeButton;
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
      initWithStartColor:[[UIColor colorNamed:kGroupedPrimaryBackgroundColor]
                             colorWithAlphaComponent:0]
                endColor:[UIColor colorNamed:kGroupedPrimaryBackgroundColor]
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

  _titleLabel = [self createTitleLabel];
  [titleContainer addSubview:_titleLabel];

  _titleGradientView = [self createGradientView];
  [titleContainer addSubview:_titleGradientView];

  return titleContainer;
}

// Returns a new Activity Indicator.
- (MDCActivityIndicator*)createActivityIndicatior {
  MDCActivityIndicator* activityIndicator = [[MDCActivityIndicator alloc] init];
  activityIndicator.translatesAutoresizingMaskIntoConstraints = NO;
  activityIndicator.hidden = YES;
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
  separatorView.backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
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
  backgroundView.backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
  backgroundView.translatesAutoresizingMaskIntoConstraints = NO;
  backgroundView.hidden = YES;
  return backgroundView;
}

- (void)updateAccessibilityValue {
  // Use the accessibility Value as there is a pause when using the
  // accessibility hint.
  _accessibilityContainerView.accessibilityValue =
      l10n_util::GetNSStringF(IDS_IOS_TAB_STRIP_TAB_CELL_VOICE_OVER_VALUE,
                              base::NumberToString16(self.tabIndex),
                              base::NumberToString16(self.numberOfTabs));
}

@end
