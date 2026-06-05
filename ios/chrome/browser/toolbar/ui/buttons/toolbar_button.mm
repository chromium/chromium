// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button.h"

#import "ios/chrome/browser/location_bar/ui_bundled/highlight_utils.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button_constants.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_buttons_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"

namespace {

constexpr CGFloat kDisabledOpacity = 0.4;
constexpr CGFloat kBlueDotRadius = 3;
constexpr CGFloat kBlueDotMargin = 1;
constexpr CGFloat kBlueDotWhiteBorderThickness = 2;

// Returns the tint color to be used in the normal mode.
UIColor* NormalTintColor() {
  return [UIColor colorNamed:kSolidBlackColor];
}

}  // namespace

@interface ToolbarButton ()

// The image from the imageLoader, if it has been loaded.
@property(nonatomic, strong, readonly) UIImage* image;

@end

@implementation ToolbarButton {
  ToolbarButtonImageLoader _imageLoader;
  UIView* _backgroundView;
  UIView* _blueDotView;
  UIView* _gradientView;
}

@synthesize image = _image;

- (instancetype)initWithImageLoader:(ToolbarButtonImageLoader)imageLoader
                          incognito:(BOOL)incognito {
  if ((self = [super initWithFrame:CGRectMake(0, 0, kToolbarButtonSize,
                                              kToolbarButtonSize)])) {
    _imageLoader = [imageLoader copy];

    [NSLayoutConstraint activateConstraints:@[
      [self.widthAnchor constraintEqualToConstant:kToolbarButtonSize],
      [self.heightAnchor constraintEqualToConstant:kToolbarButtonSize],
    ]];

    _backgroundView = [[UIView alloc] initWithFrame:CGRectZero];
    _backgroundView.translatesAutoresizingMaskIntoConstraints = NO;
    _backgroundView.backgroundColor = ToolbarElementBackgroundColor(incognito);
    _backgroundView.userInteractionEnabled = NO;
    _backgroundView.clipsToBounds = YES;
    [self insertSubview:_backgroundView belowSubview:self.imageView];
    AddSameConstraints(self, _backgroundView);

    ConfigureCornerRadiusForToolbarButtonContainer(_backgroundView,
                                                   self.traitCollection);

    ConfigureShadowForToolbarElement(self);

    self.tintColor = NormalTintColor();

    [self registerForTraitChanges:@[
      UITraitVerticalSizeClass.class, UITraitHorizontalSizeClass.class
    ]
                       withAction:@selector(updateAppearance)];
  }
  return self;
}

#pragma mark - HighlightButton

- (NSArray<UIView*>*)highlightableViews {
  return @[ self.imageView ];
}

#pragma mark - UIView

- (void)layoutSubviews {
  [super layoutSubviews];
  [self updateMask];
}

- (void)setHidden:(BOOL)hidden {
  [super setHidden:hidden];
  if (self.forceHidden) {
    return;
  }
  self.alpha = hidden ? 0.0 : 1.0;
  self.transform = hidden ? CGAffineTransformMakeScale(0.01, 0.01)
                          : CGAffineTransformIdentity;
}

#pragma mark - UIControl

- (void)setEnabled:(BOOL)enabled {
  [super setEnabled:enabled];
  if (enabled) {
    self.imageView.tintColor = NormalTintColor();
  } else {
    self.imageView.tintColor =
        [NormalTintColor() colorWithAlphaComponent:kDisabledOpacity];
  }
  [self updateAppearance];
}

#pragma mark - Properties

- (UIImage*)image {
  if (!_image) {
    _image = _imageLoader();
  }
  return _image;
}

- (void)setForceHidden:(BOOL)forceHidden {
  _forceHidden = forceHidden;
  [self updateAppearance];
}

- (void)setVisibilityMask:(ToolbarButtonVisibility)visibilityMask {
  _visibilityMask = visibilityMask;
  [self updateAppearance];
}

- (void)setIphHighlighted:(BOOL)iphHighlighted {
  if (_iphHighlighted == iphHighlighted) {
    return;
  }
  _iphHighlighted = iphHighlighted;
  [self updateHighlight];
}

- (void)setHasBlueDot:(BOOL)hasBlueDot {
  if (_hasBlueDot == hasBlueDot) {
    return;
  }
  _hasBlueDot = hasBlueDot;
  if (hasBlueDot && !_blueDotView) {
    _blueDotView = [[UIView alloc] initWithFrame:CGRectZero];
    _blueDotView.translatesAutoresizingMaskIntoConstraints = NO;
    _blueDotView.isAccessibilityElement = NO;
    _blueDotView.backgroundColor = [UIColor colorNamed:kBlueColor];
    _blueDotView.layer.cornerRadius = kBlueDotRadius;
    // Do not add the blue dot to the background as the background will be
    // masked.
    [self insertSubview:_blueDotView belowSubview:self.imageView];

    [NSLayoutConstraint activateConstraints:@[
      [_blueDotView.widthAnchor constraintEqualToConstant:2 * kBlueDotRadius],
      [_blueDotView.heightAnchor
          constraintEqualToAnchor:_blueDotView.widthAnchor],
      [_blueDotView.topAnchor constraintEqualToAnchor:self.topAnchor
                                             constant:kBlueDotMargin],
      [_blueDotView.trailingAnchor constraintEqualToAnchor:self.trailingAnchor
                                                  constant:-kBlueDotMargin],
    ]];
  }
  _blueDotView.hidden = !hasBlueDot;
  if (hasBlueDot) {
    self.accessibilityValue = self.blueDotAccessibilityLabel;
  } else {
    self.accessibilityValue = nil;
  }
  [self updateMask];
  [self updateHighlight];
}

#pragma mark - Private

// Updates the highlight visibility.
- (void)updateHighlight {
  if (_iphHighlighted && !_hasBlueDot) {
    if (!_gradientView) {
      _gradientView = CreateIPHGradientView();
      [_backgroundView addSubview:_gradientView];
      AddSameConstraints(_backgroundView, _gradientView);
    }
    _gradientView.hidden = NO;
    ConfigureIPHImageStyleForImageView(self.imageView);
  } else {
    _gradientView.hidden = YES;
    RemoveIPHImageStyleFromImageView(self.imageView);
    self.imageView.tintColor = NormalTintColor();
  }
}

// Updates the mask on the background for the blue dot.
- (void)updateMask {
  if (_hasBlueDot) {
    CAShapeLayer* maskLayer = [CAShapeLayer layer];
    UIBezierPath* path =
        [UIBezierPath bezierPathWithRect:_backgroundView.bounds];
    CGFloat centerX =
        _backgroundView.bounds.size.width - (kBlueDotMargin + kBlueDotRadius);
    CGFloat centerY = kBlueDotMargin + kBlueDotRadius;
    UIBezierPath* holePath = [UIBezierPath
        bezierPathWithArcCenter:CGPointMake(centerX, centerY)
                         radius:(kBlueDotWhiteBorderThickness + kBlueDotRadius)
                     startAngle:0
                       endAngle:2 * M_PI
                      clockwise:YES];
    [path appendPath:holePath];
    maskLayer.path = path.CGPath;
    maskLayer.fillRule = kCAFillRuleEvenOdd;
    _backgroundView.layer.mask = maskLayer;
  } else {
    _backgroundView.layer.mask = nil;
  }
}

// Updates the image visibility based on the visibility of the button.
- (void)checkImageVisibility {
  if (!self.hidden && !self.currentImage) {
    [self setImage:self.image forState:UIControlStateNormal];
    [self updateHighlight];
  }
}

// Updates the appearance of this ToolbarButton.
- (void)updateAppearance {
  [self updateVisibility];
  [self updateShape];
}

// Helper for `-updateAppearance`. Updates the visibility of this button based
// on the current state and the visibility mask.
- (void)updateVisibility {
  if (self.forceHidden) {
    self.hidden = YES;
    return;
  }
  BOOL isCurrentRegularRegular = IsRegularXRegularSizeClass(self);
  BOOL isCurrentCompactHeight =
      self.traitCollection.verticalSizeClass == UIUserInterfaceSizeClassCompact;
  BOOL isCurrentWideLayout = isCurrentRegularRegular || isCurrentCompactHeight;

  switch (self.visibilityMask) {
    case ToolbarButtonVisibility::kAlways:
      break;
    case ToolbarButtonVisibility::kRegularRegular:
      self.hidden = !isCurrentRegularRegular;
      break;
    case ToolbarButtonVisibility::kWideLayout:
      self.hidden = !isCurrentWideLayout;
      break;
    case ToolbarButtonVisibility::kCompactHeight:
      self.hidden = !isCurrentCompactHeight;
      break;
    case ToolbarButtonVisibility::kWhenEnabled:
      self.hidden = !self.enabled;
      break;
  }
  [self checkImageVisibility];
}

// Helper for `-updateAppearance`. Updates the shape of this button based on the
// current size class of the UI. In windows with compact width, the
// ToolbarButton should be square. Otherwise, they should be circular.
- (void)updateShape {
  ConfigureCornerRadiusForToolbarButtonContainer(_backgroundView,
                                                 self.traitCollection);
  [self updateMask];
}

@end
