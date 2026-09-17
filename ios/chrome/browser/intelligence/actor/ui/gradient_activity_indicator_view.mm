// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/ui/gradient_activity_indicator_view.h"

#import <QuartzCore/QuartzCore.h>

#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {

// Default layout dimensions.
const CGFloat kDefaultLineWidth = 2.0;
const CGFloat kDefaultViewSize = 24.0;
const CGFloat kStrokeEnd = 0.18;

// Animation timing configuration.
const CFTimeInterval kCycleDuration = 4.0;

// Animation keys.
NSString* const kGradientRotationKey = @"gradientRotation";
NSString* const kStrokeGroupKey = @"strokeGroup";

// Default color palette.
NSArray<UIColor*>* DefaultGradientColors() {
  return @[
    [UIColor colorNamed:kBlue500Color],
    [UIColor colorNamed:kRed500Color],
    [UIColor colorNamed:kYellow500Color],
    [UIColor colorNamed:kGreen500Color],
    [UIColor colorNamed:kBlue500Color],
  ];
}

}  // namespace

@implementation GradientActivityIndicatorView {
  CAGradientLayer* _gradientLayer;
  CAShapeLayer* _maskLayer;
  BOOL _animating;
  CABasicAnimation* _gradientRotationAnimation;
  CAAnimationGroup* _strokeGroupAnimation;
}

#pragma mark - Initialization

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _animating = NO;
    _gradientColors = DefaultGradientColors();

    self.userInteractionEnabled = NO;
    self.isAccessibilityElement = YES;
    self.accessibilityTraits = UIAccessibilityTraitUpdatesFrequently;
    self.hidden = YES;

    [self setupLayers];
    [self setupAnimations];

    [self registerForTraitChanges:@[ [UITraitUserInterfaceStyle class] ]
                       withAction:@selector(updateLayerColors)];

    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(handleWillEnterForeground)
               name:UIApplicationWillEnterForegroundNotification
             object:nil];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

#pragma mark - Public

- (void)setGradientColors:(NSArray<UIColor*>*)gradientColors {
  _gradientColors = [gradientColors copy];
  [self updateGradientLayerColors];
}

- (void)startAnimating {
  if (_animating) {
    return;
  }
  _animating = YES;
  self.hidden = NO;
  [_gradientLayer addAnimation:_gradientRotationAnimation
                        forKey:kGradientRotationKey];
  [_maskLayer addAnimation:_strokeGroupAnimation forKey:kStrokeGroupKey];
}

- (void)stopAnimating {
  if (!_animating) {
    return;
  }
  _animating = NO;
  self.hidden = YES;
  [_gradientLayer removeAllAnimations];
  [_maskLayer removeAllAnimations];
  [self resetLayerState];
}

#pragma mark - UIView Overrides

- (void)layoutSubviews {
  [super layoutSubviews];
  [CATransaction begin];
  [CATransaction setDisableActions:YES];
  _gradientLayer.frame = self.bounds;
  _maskLayer.frame = self.bounds;
  [self updateMaskPath];
  [CATransaction commit];
}

- (CGSize)intrinsicContentSize {
  return CGSizeMake(kDefaultViewSize, kDefaultViewSize);
}

#pragma mark - Private

// Configures the conic gradient layer and its shape layer mask.
- (void)setupLayers {
  _gradientLayer = [CAGradientLayer layer];
  _gradientLayer.type = kCAGradientLayerConic;
  _gradientLayer.startPoint = CGPointMake(0.5, 0.5);
  _gradientLayer.endPoint = CGPointMake(0.5, 0.0);

  _maskLayer = [CAShapeLayer layer];
  _maskLayer.fillColor = nil;
  _maskLayer.lineCap = kCALineCapRound;
  _maskLayer.lineWidth = kDefaultLineWidth;

  [self resetLayerState];
  [self updateLayerColors];

  _gradientLayer.mask = _maskLayer;
  [self.layer addSublayer:_gradientLayer];
}

// Resets layer transforms and stroke progress to their initial state.
- (void)resetLayerState {
  [CATransaction begin];
  [CATransaction setDisableActions:YES];
  _gradientLayer.transform = CATransform3DIdentity;
  _maskLayer.transform = CATransform3DIdentity;
  _maskLayer.strokeStart = 0.0;
  _maskLayer.strokeEnd = kStrokeEnd;
  [CATransaction commit];
}

// Updates the gradient and mask stroke colors.
- (void)updateLayerColors {
  [self updateGradientLayerColors];
  _maskLayer.strokeColor = [UIColor colorNamed:kSolidBlackColor].CGColor;
}

// Converts `_gradientColors` to `CGColor` and applies them to `_gradientLayer`.
- (void)updateGradientLayerColors {
  NSMutableArray* cgColors =
      [NSMutableArray arrayWithCapacity:_gradientColors.count];
  for (UIColor* color in _gradientColors) {
    [cgColors addObject:(id)color.CGColor];
  }
  _gradientLayer.colors = cgColors;
}

// Updates the circular arc path on `_maskLayer` to match the view bounds.
- (void)updateMaskPath {
  CGFloat minDimension =
      MIN(CGRectGetWidth(self.bounds), CGRectGetHeight(self.bounds));
  CGFloat radius = (minDimension - kDefaultLineWidth) / 2.0;
  if (radius <= 0.0) {
    _maskLayer.path = nil;
    return;
  }
  CGPoint center =
      CGPointMake(CGRectGetMidX(self.bounds), CGRectGetMidY(self.bounds));
  UIBezierPath* path = [UIBezierPath bezierPathWithArcCenter:center
                                                      radius:radius
                                                  startAngle:-M_PI_2
                                                    endAngle:(3.0 * M_PI_2)
                                                   clockwise:YES];
  _maskLayer.path = path.CGPath;
}

// Configures the gradient rotation and keyframed arc stroke animations.
- (void)setupAnimations {
  // Keyframe specification for the arc stretch and rotation animation.
  struct Keyframe {
    CGFloat key_time;
    CGFloat stroke_end;
    CGFloat rotation;
    // Timing curve applied to the transition from this keyframe to the next.
    // Must be nil for the final keyframe.
    CAMediaTimingFunctionName timing_curve;
  };

  // 2 slow loops of 1 revolution followed by 1 fast loop (2 full revolutions).
  const Keyframe keyframes[] = {
      {0.000, kStrokeEnd, 0.0 * M_PI, kCAMediaTimingFunctionEaseInEaseOut},
      {0.175, 0.32, 1.0 * M_PI, kCAMediaTimingFunctionEaseInEaseOut},
      {0.350, kStrokeEnd, 2.0 * M_PI, kCAMediaTimingFunctionEaseInEaseOut},
      {0.525, 0.32, 3.0 * M_PI, kCAMediaTimingFunctionEaseInEaseOut},
      {0.700, kStrokeEnd, 4.0 * M_PI, kCAMediaTimingFunctionEaseIn},
      {0.820, 0.85, 5.2 * M_PI, kCAMediaTimingFunctionLinear},
      {0.900, 0.85, 6.2 * M_PI, kCAMediaTimingFunctionEaseOut},
      {1.000, kStrokeEnd, 8.0 * M_PI, nil},
  };

  // Continuous rotation of the conic gradient.
  _gradientRotationAnimation =
      [CABasicAnimation animationWithKeyPath:@"transform.rotation.z"];
  _gradientRotationAnimation.fromValue = @(0.0);
  _gradientRotationAnimation.toValue = @(2.0 * M_PI);
  _gradientRotationAnimation.duration = kCycleDuration;
  _gradientRotationAnimation.repeatCount = HUGE_VALF;

  // Variable-speed arc stretch and rotation.
  NSMutableArray<NSNumber*>* keyTimes = [NSMutableArray array];
  NSMutableArray<NSNumber*>* strokeEndValues = [NSMutableArray array];
  NSMutableArray<NSNumber*>* rotationValues = [NSMutableArray array];
  NSMutableArray<CAMediaTimingFunction*>* timingFunctions =
      [NSMutableArray array];

  for (const Keyframe& keyframe : keyframes) {
    [keyTimes addObject:@(keyframe.key_time)];
    [strokeEndValues addObject:@(keyframe.stroke_end)];
    [rotationValues addObject:@(keyframe.rotation)];
    if (keyframe.timing_curve) {
      [timingFunctions addObject:[CAMediaTimingFunction
                                     functionWithName:keyframe.timing_curve]];
    }
  }

  CAKeyframeAnimation* strokeEnd =
      [CAKeyframeAnimation animationWithKeyPath:@"strokeEnd"];
  strokeEnd.values = strokeEndValues;
  strokeEnd.keyTimes = keyTimes;
  strokeEnd.timingFunctions = timingFunctions;

  CAKeyframeAnimation* maskRotation =
      [CAKeyframeAnimation animationWithKeyPath:@"transform.rotation.z"];
  maskRotation.values = rotationValues;
  maskRotation.keyTimes = keyTimes;
  maskRotation.timingFunctions = timingFunctions;

  _strokeGroupAnimation = [CAAnimationGroup animation];
  _strokeGroupAnimation.animations = @[ strokeEnd, maskRotation ];
  _strokeGroupAnimation.duration = kCycleDuration;
  _strokeGroupAnimation.repeatCount = HUGE_VALF;
}

// Restores animations removed upon backgrounding.
- (void)handleWillEnterForeground {
  if (!_animating) {
    return;
  }
  [_gradientLayer addAnimation:_gradientRotationAnimation
                        forKey:kGradientRotationKey];
  [_maskLayer addAnimation:_strokeGroupAnimation forKey:kStrokeGroupKey];
}

@end
