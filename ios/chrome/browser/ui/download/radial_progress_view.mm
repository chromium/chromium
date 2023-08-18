// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/download/radial_progress_view.h"

#import <QuartzCore/QuartzCore.h>

#import "base/apple/foundation_util.h"

@interface RadialProgressView ()

// CALayer that backs this view up. Serves as progress track.
@property(nonatomic, readonly) CAShapeLayer* trackLayer;

// CALayer added as a sublayer for self.layer. Serves as progress.
@property(nonatomic, readonly) CAShapeLayer* progressLayer;

@end

@implementation RadialProgressView
@synthesize progressLayer = _progressLayer;

#pragma mark - UIView overrides

+ (Class)layerClass {
  return [CAShapeLayer class];
}

- (void)setBounds:(CGRect)bounds {
  [super setBounds:bounds];

  // progressPathWithEndAngle: relies on self.bounds and must be updated here.
  // Track starts at 12 o'clock and ends at 12 o'clock.
  self.trackLayer.path =
      [self progressPathWithEndAngle:M_PI * 2 - M_PI_2].CGPath;
}

- (void)willMoveToSuperview:(UIView*)newSuperview {
  if (newSuperview && !self.progressLayer.superlayer) {
    self.trackLayer.fillColor = UIColor.clearColor.CGColor;
    UIColor* resolvedColor = [self.trackTintColor
        resolvedColorWithTraitCollection:self.traitCollection];
    self.trackLayer.strokeColor = resolvedColor.CGColor;
    self.trackLayer.lineWidth = self.lineWidth;

    [self.trackLayer addSublayer:self.progressLayer];
    [self updateProgressLayer];
  }
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];

  BOOL differentColorAppearance = [self.traitCollection
      hasDifferentColorAppearanceComparedToTraitCollection:
          previousTraitCollection];
  if (differentColorAppearance) {
    self.trackLayer.strokeColor = self.trackTintColor.CGColor;
    self.progressLayer.strokeColor = self.progressTintColor.CGColor;
  }
}

#pragma mark - Public

- (void)setProgress:(float)progress {
  if (_progress != progress) {
    _progress = progress;
    [self updateProgressLayer];
  }
}

#pragma mark - Private

// Creates progressLayer if necessary and updates its path.
- (void)updateProgressLayer {
  if (!self.superview) {
    // view is not ready yet. -updateProgressLayer will be called again from
    // -willMoveToSuperview:.
    return;
  }

  self.progressLayer.path =
      [self progressPathWithEndAngle:M_PI * 2 * self.progress - M_PI_2].CGPath;
}

// Returns Bezier path for drawing radial progress or track. Start angle is
// always 12 o'clock.
- (UIBezierPath*)progressPathWithEndAngle:(CGFloat)endAngleInRadians {
  CGPoint center =
      CGPointMake(CGRectGetMidX(self.bounds), CGRectGetMidY(self.bounds));
  CGFloat radius = CGRectGetWidth(self.bounds) / 2 - self.lineWidth;
  return [UIBezierPath bezierPathWithArcCenter:center
                                        radius:radius
                                    startAngle:-M_PI_2
                                      endAngle:endAngleInRadians
                                     clockwise:YES];
}

- (CAShapeLayer*)trackLayer {
  return base::apple::ObjCCastStrict<CAShapeLayer>(self.layer);
}

- (CAShapeLayer*)progressLayer {
  if (!_progressLayer) {
    _progressLayer = [CAShapeLayer layer];
    _progressLayer.fillColor = UIColor.clearColor.CGColor;
    UIColor* resolvedColor = [self.progressTintColor
        resolvedColorWithTraitCollection:self.traitCollection];
    _progressLayer.strokeColor = resolvedColor.CGColor;
    _progressLayer.lineWidth = self.lineWidth;
  }
  return _progressLayer;
}

@end
