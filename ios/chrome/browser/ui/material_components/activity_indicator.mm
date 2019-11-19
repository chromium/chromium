// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/material_components/activity_indicator.h"

#include "build/branding_buildflags.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSArray* ActivityIndicatorBrandedCycleColors() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return @[
    [[MDCPalette cr_bluePalette] tint500], [[MDCPalette cr_redPalette] tint500],
    [[MDCPalette cr_yellowPalette] tint500],
    [[MDCPalette cr_greenPalette] tint500]
  ];
#else
  return @[
    [MDCPalette bluePalette].tint800,
    [MDCPalette bluePalette].tint500,
    [MDCPalette bluePalette].tint200,
  ];
#endif
}
