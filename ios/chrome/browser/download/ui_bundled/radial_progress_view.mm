// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/ui_bundled/radial_progress_view.h"

#import <QuartzCore/QuartzCore.h>

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"

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
  self.progressLayer.path = [self progressPath].CGPath;
  self.trackLayer.path = [self progressPath].CGPath;
  self.progressLayer.strokeStart = 0;
  self.progressLayer.strokeEnd = self.progress;
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

    if (@available(iOS 17, *)) {
      NSArray<UITrait>* traits = TraitCollectionSetForTraits(@[
        UITraitUserInterfaceIdiom.self, UITraitUserInterfaceStyle.self,
        UITraitDisplayGamut.self, UITraitAccessibilityContrast.self,
        UITraitUserInterfaceLevel.self
      ]);
      __weak __typeof(self) weakSelf = self;
      UITraitChangeHandler handler = ^(id<UITraitEnvironment> traitEnvironment,
                                       UITraitCollection* previousCollection) {
        [weakSelf updateStrokeColorOnTraitChange:previousCollection];
      };
      [self registerForTraitChanges:traits withHandler:handler];
    }
  }
}

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }

  [self updateStrokeColorOnTraitChange:previousTraitCollection];
}
#endif

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

  self.progressLayer.strokeEnd = self.progress;
}

// Returns Bezier path for drawing radial progress or track. Start angle is
// always 12 o'clock.
- (UIBezierPath*)progressPath {
  CGPoint center =
      CGPointMake(CGRectGetMidX(self.bounds), CGRectGetMidY(self.bounds));
  CGFloat radius = CGRectGetWidth(self.bounds) / 2 - self.lineWidth;
  return [UIBezierPath bezierPathWithArcCenter:center
                                        radius:radius
                                    startAngle:-M_PI_2
                                      endAngle:3 * M_PI_2
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

// Updates the `trackLayer` & `progressLayer` property's stroke color if the
// change in the view's traits caused its appearance to change colors.
- (void)updateStrokeColorOnTraitChange:(UITraitCollection*)previousCollection {
  BOOL differentColorAppearance = [self.traitCollection
      hasDifferentColorAppearanceComparedToTraitCollection:previousCollection];
  if (differentColorAppearance) {
    self.trackLayer.strokeColor = self.trackTintColor.CGColor;
    self.progressLayer.strokeColor = self.progressTintColor.CGColor;
  }
}

@end
