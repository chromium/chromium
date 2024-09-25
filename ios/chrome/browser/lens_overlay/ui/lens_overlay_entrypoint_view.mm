// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_entrypoint_view.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"

namespace {

const CGFloat kLensCameraSymbolPointSize = 18.0;

}  // namespace

@implementation LensOverlayEntrypointButton

- (instancetype)init {
  self = [super init];

  if (self) {
    self.pointerInteractionEnabled = YES;
    self.pointerStyleProvider = CreateDefaultEffectCirclePointerStyleProvider();
    self.tintColor = [UIColor colorNamed:kToolbarButtonColor];

    UIImageSymbolConfiguration* symbolConfig = [UIImageSymbolConfiguration
        configurationWithPointSize:kLensCameraSymbolPointSize
                            weight:UIImageSymbolWeightRegular
                             scale:UIImageSymbolScaleMedium];
    [self setPreferredSymbolConfiguration:symbolConfig
                          forImageInState:UIControlStateNormal];

    [self setImage:CustomSymbolWithPointSize(kCameraLensSymbol,
                                             kLensCameraSymbolPointSize)
          forState:UIControlStateNormal];
    self.imageView.contentMode = UIViewContentModeScaleAspectFit;

    [NSLayoutConstraint
        activateConstraints:@[ [self.widthAnchor
                                constraintEqualToAnchor:self.heightAnchor] ]];

    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    [center addObserver:self
               selector:@selector(deviceOrientationDidChange)
                   name:UIDeviceOrientationDidChangeNotification
                 object:nil];
  }

  return self;
}

// Handles device rotation.
- (void)deviceOrientationDidChange {
  const UIDeviceOrientation deviceOrientation =
      [[UIDevice currentDevice] orientation];

  // The entrypoint button must be enabled only on landscape mode.
  self.enabled = deviceOrientation == UIDeviceOrientationPortrait;
}

@end
