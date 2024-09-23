// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_entrypoint_view.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"

namespace LensOverlay {

const CGFloat kLensCameraSymbolPointSize = 18.0;

UIButton* NewEntrypointButton() {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeSystem];

  button.pointerInteractionEnabled = YES;
  button.pointerStyleProvider = CreateDefaultEffectCirclePointerStyleProvider();
  button.tintColor = [UIColor colorNamed:kToolbarButtonColor];

  UIImageSymbolConfiguration* symbolConfig = [UIImageSymbolConfiguration
      configurationWithPointSize:kLensCameraSymbolPointSize
                          weight:UIImageSymbolWeightRegular
                           scale:UIImageSymbolScaleMedium];
  [button setPreferredSymbolConfiguration:symbolConfig
                          forImageInState:UIControlStateNormal];

  [button setImage:CustomSymbolWithPointSize(kCameraLensSymbol,
                                             kLensCameraSymbolPointSize)
          forState:UIControlStateNormal];
  button.imageView.contentMode = UIViewContentModeScaleAspectFit;

  [NSLayoutConstraint
      activateConstraints:@[ [button.widthAnchor
                              constraintEqualToAnchor:button.heightAnchor] ]];

  return button;
}

}  // namespace LensOverlay
