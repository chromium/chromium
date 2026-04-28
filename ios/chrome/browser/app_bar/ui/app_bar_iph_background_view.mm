// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_bar/ui/app_bar_iph_background_view.h"

#import <QuartzCore/QuartzCore.h>

#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@implementation AppBarIPHBackgroundView

+ (Class)layerClass {
  return [CAGradientLayer class];
}

- (instancetype)init {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    self.userInteractionEnabled = NO;

    CAGradientLayer* gradientLayer = (CAGradientLayer*)self.layer;
    gradientLayer.type = kCAGradientLayerRadial;

    // Define the Colors
    UIColor* centerColor = [UIColor colorNamed:kBlue600Color];
    UIColor* edgeColor = [centerColor colorWithAlphaComponent:0.0];

    gradientLayer.colors = @[ (id)centerColor.CGColor, (id)edgeColor.CGColor ];

    // Center bottom.
    gradientLayer.startPoint = CGPointMake(0.5, 1.0);
    // Increase the x-value (e.g., to 2.0 or 2.5) to stretch the radius
    // horizontally. The y-value stays at 0.0 so the vertical height of the
    // gradient remains the same.
    gradientLayer.endPoint = CGPointMake(1.75, 0);

    // Tweak how quickly the gradient fades (Pushing this closer to 1.0 also
    // makes the core look wider)
    gradientLayer.locations = @[ @0.2, @0.8 ];
  }
  return self;
}

@end
