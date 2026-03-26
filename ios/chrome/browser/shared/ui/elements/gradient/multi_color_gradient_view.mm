// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/elements/gradient/multi_color_gradient_view.h"

#import "base/apple/foundation_util.h"

@implementation MultiColorGradientView {
  // The colors of the gradient.
  NSArray<UIColor*>* _colors;
}

#pragma mark - Public

- (instancetype)initWithColors:(NSArray<UIColor*>*)colors
                     locations:(NSArray<NSNumber*>*)locations
                    startPoint:(CGPoint)startPoint
                      endPoint:(CGPoint)endPoint {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    self.gradientLayer.startPoint = startPoint;
    self.gradientLayer.endPoint = endPoint;
    self.gradientLayer.locations = locations;

    [self updateColors:colors];

    self.userInteractionEnabled = NO;

    NSArray<UITrait>* traits = @[
      UITraitUserInterfaceIdiom.class, UITraitUserInterfaceStyle.class,
      UITraitDisplayGamut.class, UITraitAccessibilityContrast.class,
      UITraitUserInterfaceLevel.class
    ];
    __weak __typeof(self) weakSelf = self;
    UITraitChangeHandler handler = ^(id<UITraitEnvironment> traitEnvironment,
                                     UITraitCollection* previousCollection) {
      [weakSelf updateColorsOnTraitChange:previousCollection];
    };
    [self registerForTraitChanges:traits withHandler:handler];
  }
  return self;
}

- (void)updateColors:(NSArray<UIColor*>*)colors {
  _colors = colors;
  NSMutableArray* cgColors = [NSMutableArray arrayWithCapacity:colors.count];

  for (UIColor* color in colors) {
    [cgColors addObject:(id)color.CGColor];
  }
  self.gradientLayer.colors = cgColors;
}

- (CAGradientLayer*)gradientLayer {
  return base::apple::ObjCCastStrict<CAGradientLayer>(self.layer);
}

+ (Class)layerClass {
  return [CAGradientLayer class];
}

#pragma mark - Private

// Animates and updates the view's color when its appearance has been modified
// via changes in UITraits.
- (void)updateColorsOnTraitChange:(UITraitCollection*)previousTraitCollection {
  if ([self.traitCollection
          hasDifferentColorAppearanceComparedToTraitCollection:
              previousTraitCollection]) {
    [CATransaction begin];
    // If this isn't set, the changes here are automatically animated. The other
    // color changes for dark mode don't animate, however, so there ends up
    // being visual desyncing.
    [CATransaction setDisableActions:YES];
    [self updateColors:_colors];
    [CATransaction commit];
  }
}

@end
