// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COLORS_MDCPALETTE_CRADDITIONS_H_
#define IOS_CHROME_BROWSER_UI_COLORS_MDCPALETTE_CRADDITIONS_H_

#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"

// Access to overridable palettes.
@interface MDCPalette (CrAdditions)

// Red palette.
+ (MDCPalette*)cr_redPalette;

// Blue palette.
+ (MDCPalette*)cr_bluePalette;

// Green palette.
+ (MDCPalette*)cr_greenPalette;

// Yellow palette.
+ (MDCPalette*)cr_yellowPalette;

// Grey palette.
+ (MDCPalette*)cr_greyPalette;

+ (void)cr_setBluePalette:(MDCPalette*)palette;
+ (void)cr_setRedPalette:(MDCPalette*)palette;
+ (void)cr_setGreenPalette:(MDCPalette*)palette;
+ (void)cr_setYellowPalette:(MDCPalette*)palette;
+ (void)cr_setGreyPalette:(MDCPalette*)palette;

@end

#endif  // IOS_CHROME_BROWSER_UI_COLORS_MDCPALETTE_CRADDITIONS_H_
