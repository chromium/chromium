// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/ui/tab_strip_cell.h"

#import <MaterialComponents/MaterialActivityIndicator.h>

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/image/image_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
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

// Content view constants.
const CGFloat kFaviconLeadingMargin = 16;
const CGFloat kCloseButtonMargin = 10;
const CGFloat kTitleInset = 10.0;
const CGFloat kFontSize = 14.0;
const CGFloat kFaviconSize = 16.0;

// Returns the default favicon image.
UIImage* DefaultFavicon() {
  return DefaultSymbolWithPointSize(kGlobeAmericasSymbol, 14);
}

}  // namespace

@implementation TabStripCell {
  // Content view subviews.
  UIButton* _closeButton;
  UILabel* _titleLabel;
  UIImageView* _faviconView;

  // Rounded decoration views, visible when the cell is selected.
  UIView* _leftTailView;
  UIView* _rightTailView;
  UIView* _topLeftCornerView;
  UIView* _topRightCornerView;

  // Wether the decoration layers have been updated.
  BOOL _decorationLayersUpdated;

  // Circular spinner that shows the loading state of the tab.
  MDCActivityIndicator* _activityIndicator;

  // Title label's trailing constraints.
  NSLayoutConstraint* _titleLabelCollapsedTrailingConstraint;
  NSLayoutConstraint* _titleLabelTrailingConstraint;
}

- (instancetype)initWithFrame:(CGRect)frame {
  if ((self = [super initWithFrame:frame])) {
    self.layer.masksToBounds = NO;
    _decorationLayersUpdated = NO;

    UIView* contentView = self.contentView;

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

    _titleLabel = [self createTitleLabel];
    [contentView addSubview:_titleLabel];

    _leftTailView = [self createTailView];
    [self addSubview:_leftTailView];

    _rightTailView = [self createTailView];
    [self addSubview:_rightTailView];

    [self setupConstraints];
    [self setupDecorationLayers];

    self.selected = NO;
  }
  return self;
}

- (void)setTitle:(NSString*)title {
  _titleLabel.text = title;
}

- (void)setFaviconImage:(UIImage*)image {
  if (!image) {
    _faviconView.image = DefaultFavicon();
  } else {
    _faviconView.image = image;
  }
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

#pragma mark - Accessor

- (void)setSelected:(BOOL)selected {
  [super setSelected:selected];

  // Style the contentView background color.
  self.contentView.backgroundColor =
      selected ? [UIColor colorNamed:kPrimaryBackgroundColor]
               : UIColor.clearColor;

  // Style the favicon tint color.
  _faviconView.tintColor = selected ? [UIColor colorNamed:kCloseButtonColor]
                                    : [UIColor colorNamed:kGrey500Color];
  // Style the close button tint color.
  _closeButton.tintColor = selected ? [UIColor colorNamed:kCloseButtonColor]
                                    : [UIColor colorNamed:kGrey500Color];
  // Style the title text color.
  _titleLabel.textColor = selected ? [UIColor colorNamed:kTextPrimaryColor]
                                   : [UIColor colorNamed:kGrey600Color];

  // Update decoration views visibility.
  _leftTailView.hidden = !selected;
  _rightTailView.hidden = !selected;
  _topLeftCornerView.hidden = !selected;
  _topRightCornerView.hidden = !selected;

  [self updateCollapsedState];
}

- (void)applyLayoutAttributes:
    (UICollectionViewLayoutAttributes*)layoutAttributes {
  [super applyLayoutAttributes:layoutAttributes];

  [self updateCollapsedState];
}

#pragma mark - UICollectionViewCell

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
  _rightTailView.layer.transform = CATransform3DMakeScale(-1, 1, 1);

  // Round the top left corner.
  CAShapeLayer* topLeftCornerLayer = [CAShapeLayer layer];
  topLeftCornerLayer.path = cornerPath.CGPath;
  _topLeftCornerView.layer.mask = topLeftCornerLayer;
  _topLeftCornerView.layer.transform = CATransform3DMakeScale(-1, -1, 1);

  // Round the top right corner.
  CAShapeLayer* topRightCornerLayer = [CAShapeLayer layer];
  topRightCornerLayer.path = cornerPath.CGPath;
  _topRightCornerView.layer.mask = topRightCornerLayer;
  _topRightCornerView.layer.transform = CATransform3DMakeScale(1, -1, 1);

  _decorationLayersUpdated = YES;
}

/// Hides the close button view if the cell is collapsed.
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
    _titleLabelTrailingConstraint.active = NO;
    _titleLabelCollapsedTrailingConstraint.active = YES;
  } else {
    _titleLabelCollapsedTrailingConstraint.active = NO;
    _titleLabelTrailingConstraint.active = YES;
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
  _titleLabelTrailingConstraint = [_titleLabel.trailingAnchor
      constraintEqualToAnchor:_closeButton.leadingAnchor
                     constant:-kTitleInset];
  _titleLabelCollapsedTrailingConstraint = [_titleLabel.trailingAnchor
      constraintEqualToAnchor:contentView.trailingAnchor
                     constant:-kTitleInset];
  [NSLayoutConstraint activateConstraints:@[
    [_titleLabel.leadingAnchor
        constraintEqualToAnchor:leadingImageGuide.trailingAnchor
                       constant:kTitleInset],
    [_titleLabel.centerYAnchor
        constraintEqualToAnchor:contentView.centerYAnchor],
  ]];

  /// `_topLeftCornerView` and `_topRightCornerView` constraints.
  [NSLayoutConstraint activateConstraints:@[
    [_topLeftCornerView.leadingAnchor
        constraintEqualToAnchor:contentView.leadingAnchor],
    [_topLeftCornerView.topAnchor
        constraintEqualToAnchor:contentView.topAnchor],
    [_topLeftCornerView.widthAnchor constraintEqualToConstant:kCornerSize],
    [_topLeftCornerView.heightAnchor constraintEqualToConstant:kCornerSize],

    [_topRightCornerView.trailingAnchor
        constraintEqualToAnchor:contentView.trailingAnchor],
    [_topRightCornerView.topAnchor
        constraintEqualToAnchor:contentView.topAnchor],
    [_topRightCornerView.widthAnchor constraintEqualToConstant:kCornerSize],
    [_topRightCornerView.heightAnchor constraintEqualToConstant:kCornerSize],
  ]];

  /// `_leftTailView` and `_rightTailView` constraints.
  [NSLayoutConstraint activateConstraints:@[
    [_leftTailView.trailingAnchor
        constraintEqualToAnchor:contentView.leadingAnchor],
    [_leftTailView.bottomAnchor
        constraintEqualToAnchor:contentView.bottomAnchor],
    [_leftTailView.widthAnchor constraintEqualToConstant:kCornerSize],
    [_leftTailView.heightAnchor constraintEqualToConstant:kCornerSize],

    [_rightTailView.leadingAnchor
        constraintEqualToAnchor:contentView.trailingAnchor],
    [_rightTailView.bottomAnchor
        constraintEqualToAnchor:contentView.bottomAnchor],
    [_rightTailView.widthAnchor constraintEqualToConstant:kCornerSize],
    [_rightTailView.heightAnchor constraintEqualToConstant:kCornerSize],
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
  return titleLabel;
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

@end
