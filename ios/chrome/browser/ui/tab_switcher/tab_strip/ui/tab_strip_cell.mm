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

// The size of the xmark symbol image.
NSInteger kXmarkSymbolPointSize = 13;

// Corner radius of the top left and right corner of the content view.
const CGFloat kTopCornerRadius = 16;

// Size of the decoration tails when the cell is selected.
const CGFloat kTailSize = 16;

// Content view constants.
const CGFloat kFaviconLeadingMargin = 16;
const CGFloat kCloseButtonMargin = 16;
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

  // Decoration tails, visible when the cell is selected.
  UIView* _leftTailView;
  UIView* _rightTailView;

  // Width of the cell.
  CGFloat _cellWidth;

  // Wether the tail layers have been updated.
  BOOL _tailLayersUpdated;

  // Circular spinner that shows the loading state of the tab.
  MDCActivityIndicator* _activityIndicator;
}

- (instancetype)initWithFrame:(CGRect)frame {
  if ((self = [super initWithFrame:frame])) {
    self.layer.masksToBounds = NO;
    _cellWidth = 0;
    _tailLayersUpdated = NO;

    UIView* contentView = self.contentView;

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

- (void)layoutSubviews {
  [super layoutSubviews];

  CGFloat cellWidth = self.frame.size.width;
  if (cellWidth != _cellWidth) {
    [self updateContentViewLayer];
    _cellWidth = cellWidth;
  }

  if (!_tailLayersUpdated) {
    [self updateTailLayers];
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

  // Update decoration tails visibility.
  _leftTailView.hidden = !selected;
  _rightTailView.hidden = !selected;
}

#pragma mark - UICollectionViewCell

- (void)prepareForReuse {
  [super prepareForReuse];
  _titleLabel.text = nil;
  self.selected = NO;
  [self setFaviconImage:nil];
}

#pragma mark - Private

/// Updates the `contentView` layer for the `selected` state of the cell.
- (void)updateContentViewLayer {
  // Round the top corners of the content view.
  UIBezierPath* path = [UIBezierPath
      bezierPathWithRoundedRect:self.bounds
              byRoundingCorners:UIRectCornerTopLeft | UIRectCornerTopRight
                    cornerRadii:CGSizeMake(kTopCornerRadius, 0.0)];
  CAShapeLayer* maskLayer = [CAShapeLayer layer];
  maskLayer.path = path.CGPath;
  self.contentView.layer.mask = maskLayer;
}

/// Updates the tail layers for the `selected` state of the cell.
- (void)updateTailLayers {
  CGRect leftTailRect = _leftTailView.bounds;
  if (leftTailRect.size.width == 0) {
    return;
  }

  CGFloat radius = kTailSize;

  // Round the left tail.
  UIBezierPath* leftTailPath = [UIBezierPath bezierPath];
  [leftTailPath moveToPoint:CGPointMake(CGRectGetMaxX(leftTailRect),
                                        CGRectGetMaxY(leftTailRect))];
  [leftTailPath
      addLineToPoint:CGPointMake(CGRectGetMaxX(leftTailRect),
                                 CGRectGetMaxY(leftTailRect) - radius)];
  [leftTailPath
      addArcWithCenter:CGPointMake(CGRectGetMaxX(leftTailRect) - radius,
                                   CGRectGetMaxY(leftTailRect) - radius)
                radius:radius
            startAngle:0
              endAngle:M_PI_2
             clockwise:YES];
  [leftTailPath closePath];
  CAShapeLayer* leftTailMaskLayer = [CAShapeLayer layer];
  leftTailMaskLayer.path = leftTailPath.CGPath;
  _leftTailView.layer.mask = leftTailMaskLayer;

  // Round the right tail.
  CGRect rightTailRect = _rightTailView.bounds;
  UIBezierPath* rightTailpath = [UIBezierPath bezierPath];
  [rightTailpath moveToPoint:CGPointMake(CGRectGetMinX(rightTailRect),
                                         CGRectGetMaxY(rightTailRect))];
  [rightTailpath
      addLineToPoint:CGPointMake(CGRectGetMinX(rightTailRect),
                                 CGRectGetMaxY(rightTailRect) - radius)];
  [rightTailpath
      addArcWithCenter:CGPointMake(CGRectGetMinX(rightTailRect) + radius,
                                   CGRectGetMaxY(rightTailRect) - radius)
                radius:radius
            startAngle:M_PI
              endAngle:M_PI_2
             clockwise:NO];
  [rightTailpath closePath];
  CAShapeLayer* rightTailMaskLayer = [CAShapeLayer layer];
  rightTailMaskLayer.path = rightTailpath.CGPath;
  _rightTailView.layer.mask = rightTailMaskLayer;

  _tailLayersUpdated = YES;
}

// Sets the cell constraints.
- (void)setupConstraints {
  UILayoutGuide* leadingImageGuide = [[UILayoutGuide alloc] init];
  [self addLayoutGuide:leadingImageGuide];

  UIView* contentView = self.contentView;

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

  [NSLayoutConstraint activateConstraints:@[
    [_closeButton.trailingAnchor
        constraintEqualToAnchor:contentView.trailingAnchor
                       constant:-kCloseButtonMargin],
    [_closeButton.centerYAnchor
        constraintEqualToAnchor:contentView.centerYAnchor],
  ]];

  [NSLayoutConstraint activateConstraints:@[
    [_titleLabel.leadingAnchor
        constraintEqualToAnchor:leadingImageGuide.trailingAnchor
                       constant:kTitleInset],
    [_titleLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:_closeButton.leadingAnchor
                                 constant:-kTitleInset],
    [_titleLabel.centerYAnchor
        constraintEqualToAnchor:contentView.centerYAnchor],
  ]];

  [NSLayoutConstraint activateConstraints:@[
    [_leftTailView.trailingAnchor
        constraintEqualToAnchor:contentView.leadingAnchor],
    [_leftTailView.bottomAnchor
        constraintEqualToAnchor:contentView.bottomAnchor],
    [_leftTailView.widthAnchor constraintEqualToConstant:kTailSize],
    [_leftTailView.heightAnchor constraintEqualToConstant:kTailSize],

    [_rightTailView.leadingAnchor
        constraintEqualToAnchor:contentView.trailingAnchor],
    [_rightTailView.bottomAnchor
        constraintEqualToAnchor:contentView.bottomAnchor],
    [_rightTailView.widthAnchor constraintEqualToConstant:kTailSize],
    [_rightTailView.heightAnchor constraintEqualToConstant:kTailSize],
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
  UIImage* close =
      DefaultSymbolTemplateWithPointSize(kXMarkSymbol, kXmarkSymbolPointSize);
  UIButton* closeButton = [UIButton buttonWithType:UIButtonTypeCustom];
  closeButton.translatesAutoresizingMaskIntoConstraints = NO;
  [closeButton setImage:close forState:UIControlStateNormal];
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

@end
