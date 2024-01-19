// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/ui/tab_strip_cell.h"

#import <MaterialComponents/MaterialActivityIndicator.h>

#import "base/notreached.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/image/image_util.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/gradient_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

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
const CGFloat kSeparatorWidth = 2;
const CGFloat kSeparatorCornerRadius = 1;
const CGFloat kSeparatorHeight = 18;
const CGFloat kSeparatorHorizontalInset = 2;
const CGFloat kSeparatorGradientWidth = 8;

// Content view constants.
const CGFloat kFaviconLeadingMargin = 10;
const CGFloat kCloseButtonMargin = 10;
const CGFloat kTitleInset = 10;
const CGFloat kFontSize = 14;
const CGFloat kFaviconSize = 16;
const CGFloat kTitleGradientWidth = 16;

// Z-Index of the selected cell.
const NSInteger kSelectedZIndex = 10;

// Returns the default favicon image.
UIImage* DefaultFavicon() {
  return DefaultSymbolWithPointSize(kGlobeAmericasSymbol, 14);
}

}  // namespace

@implementation TabStripCell {
  // Content view subviews.
  UIButton* _closeButton;
  UIView* _titleContainer;
  UILabel* _titleLabel;
  GradientView* _titleGradientView;
  UIImageView* _faviconView;

  // Rounded decoration views, visible when the cell is selected.
  UIView* _leftTailView;
  UIView* _rightTailView;
  UIView* _topLeftCornerView;
  UIView* _topRightCornerView;

  // Cell separator.
  UIView* _leadingSeparatorView;
  UIView* _trailingSeparatorView;
  UIView* _leadingSeparatorGradientView;
  UIView* _trailingSeparatorGradientView;

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
}

- (instancetype)initWithFrame:(CGRect)frame {
  if ((self = [super initWithFrame:frame])) {
    self.layer.masksToBounds = NO;
    _decorationLayersUpdated = NO;

    UIView* contentView = self.contentView;
    contentView.layer.masksToBounds = YES;

    _topLeftCornerView = [self createTopCornerView];
    [contentView addSubview:_topLeftCornerView];

    _topRightCornerView = [self createTopCornerView];
    [contentView addSubview:_topRightCornerView];

    _faviconView = [self createFaviconView];
    [contentView addSubview:_faviconView];

    _activityIndicator = [self createActivityIndicatior];
    [contentView addSubview:_activityIndicator];

    _closeButton = [self createCloseButton];
    [contentView addSubview:_closeButton];

    _titleContainer = [self createTitleContainer];
    [contentView addSubview:_titleContainer];

    _leftTailView = [self createTailView];
    [self addSubview:_leftTailView];

    _rightTailView = [self createTailView];
    [self addSubview:_rightTailView];

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

- (void)setTitle:(NSString*)title {
  NSTextAlignment titleTextAligment = DetermineBestAlignmentForText(title);

  _titleLabel.text = [title copy];
  _titleLabel.textAlignment = titleTextAligment;
  [self updateTitleGradientViewConstraints];
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
      [UIBezierPath bezierPathWithRoundedRect:self.bounds
                                 cornerRadius:kCornerSize];
  UIDragPreviewParameters* params = [[UIDragPreviewParameters alloc] init];
  params.visiblePath = visiblePath;
  return params;
}

#pragma mark - Setters

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

- (void)setSelected:(BOOL)selected {
  [super setSelected:selected];

  if (selected) {
    /// The cell attributes is updated just after the cell selection.
    /// Hide separtors to avoid an animation glitch when selecting/inserting.
    _leadingSeparatorView.hidden = YES;
    _trailingSeparatorView.hidden = YES;
    _leadingSeparatorGradientView.hidden = YES;
    _trailingSeparatorGradientView.hidden = YES;
  }

  UIColor* backgroundColor = selected
                                 ? [UIColor colorNamed:kPrimaryBackgroundColor]
                                 : [UIColor colorNamed:kGrey200Color];

  // Update colors.
  self.contentView.backgroundColor = backgroundColor;
  _faviconView.tintColor = selected ? [UIColor colorNamed:kCloseButtonColor]
                                    : [UIColor colorNamed:kGrey500Color];
  [_titleGradientView setStartColor:[backgroundColor colorWithAlphaComponent:0]
                           endColor:backgroundColor];

  // Make the selected cell on top of other cells.
  self.layer.zPosition = selected ? kSelectedZIndex : 0;

  // Update decoration views visibility.
  _leftTailView.hidden = !selected;
  _rightTailView.hidden = !selected;
  _topLeftCornerView.hidden = !selected;
  _topRightCornerView.hidden = !selected;

  [self updateCollapsedState];
}

#pragma mark - UICollectionViewCell

- (void)applyLayoutAttributes:
    (UICollectionViewLayoutAttributes*)layoutAttributes {
  [super applyLayoutAttributes:layoutAttributes];

  [self updateCollapsedState];
}

- (void)prepareForReuse {
  [super prepareForReuse];
  _titleLabel.text = nil;
  self.selected = NO;
  [self setFaviconImage:nil];
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

  // Round the top left corner.
  CAShapeLayer* topLeftCornerLayer = [CAShapeLayer layer];
  topLeftCornerLayer.path = cornerPath.CGPath;
  _topLeftCornerView.layer.mask = topLeftCornerLayer;
  _topLeftCornerView.transform = CGAffineTransformMakeScale(-1, -1);

  // Round the top right corner.
  CAShapeLayer* topRightCornerLayer = [CAShapeLayer layer];
  topRightCornerLayer.path = cornerPath.CGPath;
  _topRightCornerView.layer.mask = topRightCornerLayer;
  _topRightCornerView.transform = CGAffineTransformMakeScale(1, -1);

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

  /// `_topLeftCornerView` and `_topRightCornerView` constraints.
  [NSLayoutConstraint activateConstraints:@[
    [_topLeftCornerView.leftAnchor
        constraintEqualToAnchor:contentView.leftAnchor],
    [_topLeftCornerView.topAnchor
        constraintEqualToAnchor:contentView.topAnchor],
    [_topLeftCornerView.widthAnchor constraintEqualToConstant:kCornerSize],
    [_topLeftCornerView.heightAnchor constraintEqualToConstant:kCornerSize],

    [_topRightCornerView.rightAnchor
        constraintEqualToAnchor:contentView.rightAnchor],
    [_topRightCornerView.topAnchor
        constraintEqualToAnchor:contentView.topAnchor],
    [_topRightCornerView.widthAnchor constraintEqualToConstant:kCornerSize],
    [_topRightCornerView.heightAnchor constraintEqualToConstant:kCornerSize],
  ]];

  /// `_leftTailView` and `_rightTailView` constraints.
  [NSLayoutConstraint activateConstraints:@[
    [_leftTailView.rightAnchor constraintEqualToAnchor:contentView.leftAnchor],
    [_leftTailView.bottomAnchor
        constraintEqualToAnchor:contentView.bottomAnchor],
    [_leftTailView.widthAnchor constraintEqualToConstant:kCornerSize],
    [_leftTailView.heightAnchor constraintEqualToConstant:kCornerSize],

    [_rightTailView.leftAnchor constraintEqualToAnchor:contentView.rightAnchor],
    [_rightTailView.bottomAnchor
        constraintEqualToAnchor:contentView.bottomAnchor],
    [_rightTailView.widthAnchor constraintEqualToConstant:kCornerSize],
    [_rightTailView.heightAnchor constraintEqualToConstant:kCornerSize],
  ]];

  /// `_leadingSeparatorView` constraints.
  [NSLayoutConstraint activateConstraints:@[
    [_leadingSeparatorView.trailingAnchor
        constraintEqualToAnchor:contentView.leadingAnchor
                       constant:-kSeparatorHorizontalInset],
    [_leadingSeparatorView.widthAnchor
        constraintEqualToConstant:kSeparatorWidth],
    [_leadingSeparatorView.heightAnchor
        constraintEqualToConstant:kSeparatorHeight],
    [_leadingSeparatorView.centerYAnchor
        constraintEqualToAnchor:contentView.centerYAnchor],
  ]];

  /// `_trailingSeparatorView` constraints.
  [NSLayoutConstraint activateConstraints:@[
    [_trailingSeparatorView.leadingAnchor
        constraintEqualToAnchor:contentView.trailingAnchor
                       constant:kSeparatorHorizontalInset],
    [_trailingSeparatorView.widthAnchor
        constraintEqualToConstant:kSeparatorWidth],
    [_trailingSeparatorView.heightAnchor
        constraintEqualToConstant:kSeparatorHeight],
    [_trailingSeparatorView.centerYAnchor
        constraintEqualToAnchor:contentView.centerYAnchor],
  ]];

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
      DefaultSymbolWithPointSize(kXMarkCircleFillSymbol, kCloseButtonSize);
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
  return closeButton;
}

// Returns a new title label.
- (UILabel*)createTitleLabel {
  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  titleLabel.font = [UIFont systemFontOfSize:kFontSize
                                      weight:UIFontWeightMedium];
  titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  titleLabel.adjustsFontForContentSizeCategory = YES;

  return titleLabel;
}

// Returns a new gradient view.
- (GradientView*)createGradientView {
  GradientView* gradientView = [[GradientView alloc]
      initWithStartColor:[[UIColor colorNamed:kGrey200Color]
                             colorWithAlphaComponent:0]
                endColor:[UIColor colorNamed:kGrey200Color]
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

// Returns a new tail view.
- (UIView*)createTailView {
  UIView* tailView = [[UIView alloc] init];
  tailView.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  tailView.translatesAutoresizingMaskIntoConstraints = NO;
  tailView.hidden = YES;
  return tailView;
}

// Returns a new top corner view.
- (UIView*)createTopCornerView {
  UIView* topCornerView = [[UIView alloc] init];
  topCornerView.backgroundColor = [UIColor colorNamed:kGrey200Color];
  topCornerView.translatesAutoresizingMaskIntoConstraints = NO;
  topCornerView.hidden = YES;
  return topCornerView;
}

// Returns a new separator view.
- (UIView*)createSeparatorView {
  UIView* separatorView = [[UIView alloc] init];
  separatorView.backgroundColor = [UIColor colorNamed:kGrey400Color];
  separatorView.translatesAutoresizingMaskIntoConstraints = NO;
  separatorView.layer.cornerRadius = kSeparatorCornerRadius;
  return separatorView;
}

@end
