// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/material_components/utils.h"

#import <UIKit/UIKit.h>

#import <MaterialComponents/MDCAppBarContainerViewController.h>
#import <MaterialComponents/MaterialAppBar.h>
#import <MaterialComponents/MaterialCollections.h>
#import <MaterialComponents/MaterialFlexibleHeader.h>
#import <MaterialComponents/MaterialPalettes.h>
#import <MaterialComponents/MaterialShadowLayer.h>

#include "base/mac/foundation_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

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
      [UIColor colorNamed:kSecondaryBackgroundColor];
  viewController.navigationBar.tintColor =
      [UIColor colorNamed:kTextPrimaryColor];
  viewController.navigationBar.titleAlignment =
      MDCNavigationBarTitleAlignmentLeading;

  CustomizeAppBarShadow(viewController);
}
