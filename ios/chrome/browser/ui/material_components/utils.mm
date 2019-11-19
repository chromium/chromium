// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/material_components/utils.h"

#import <UIKit/UIKit.h>

#include "base/mac/foundation_util.h"
#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"
#import "ios/third_party/material_components_ios/src/components/AppBar/src/MDCAppBarContainerViewController.h"
#import "ios/third_party/material_components_ios/src/components/AppBar/src/MaterialAppBar.h"
#import "ios/third_party/material_components_ios/src/components/FlexibleHeader/src/MaterialFlexibleHeader.h"
#import "ios/third_party/material_components_ios/src/components/NavigationBar/src/MaterialNavigationBar.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_components_ios/src/components/ShadowLayer/src/MaterialShadowLayer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Customize the length and opacity of AppBar's shadow.
void CustomizeAppBarShadow(
    MDCFlexibleHeaderViewController* headerViewController) {
  // Adjust the length of the shadow using a customized intensityBlock.
  MDCFlexibleHeaderShadowIntensityChangeBlock intensityBlock = ^(
      CALayer* _Nonnull shadowLayer, CGFloat intensity) {
    // elevation = 10.26 makes the shadow 12pt tall like the bottom toolbar's.
    CGFloat elevation = 10.26f * intensity;
    [base::mac::ObjCCast<MDCShadowLayer>(shadowLayer) setElevation:elevation];
  };
  // Adjust the opacity of the shadow on the customized shadow layer.
  MDCShadowLayer* shadowLayer = [MDCShadowLayer layer];
  shadowLayer.opacity = 0.4f;
  // Apply the customized shadow layer on the headerView of appBar.
  [headerViewController.headerView setShadowLayer:shadowLayer
                          intensityDidChangeBlock:intensityBlock];
}

void ConfigureAppBarViewControllerWithCardStyle(
    MDCAppBarViewController* viewController) {
  viewController.headerView.canOverExtend = NO;
  viewController.headerView.shiftBehavior =
      MDCFlexibleHeaderShiftBehaviorDisabled;
  viewController.headerView.backgroundColor =
      UIColor.cr_secondarySystemBackgroundColor;
  viewController.navigationBar.tintColor = UIColor.cr_labelColor;
  viewController.navigationBar.titleAlignment =
      MDCNavigationBarTitleAlignmentLeading;

  CustomizeAppBarShadow(viewController);
}
