// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/ui/tab_strip_utils.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/ui/tab_strip_features_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@implementation TabStripHelper

+ (UIColor*)backgroundColor {
  if (TabStripFeaturesUtils.hasBlackBackground) {
    return UIColor.blackColor;
  } else if (TabStripFeaturesUtils.hasDarkerBackground) {
    return [UIColor colorNamed:kTabStripBackgroundColor];
  } else if (TabStripFeaturesUtils.hasDarkerBackgroundV3) {
    return [UIColor colorNamed:kTabStripV3BackgroundColor];
  }
  return [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
}

+ (UIColor*)newTabButtonSymbolColor {
  if (TabStripFeaturesUtils.hasHighContrastNTB) {
    return [UIColor colorNamed:kTextPrimaryColor];
  } else if (TabStripFeaturesUtils.hasBlackBackground) {
    return [UIColor colorNamed:kStaticGrey600Color];
  } else if (TabStripFeaturesUtils.hasBiggerNTB) {
    return [UIColor colorNamed:kTabStripNewTabButtonColor];
  }
  return [UIColor colorNamed:kTextSecondaryColor];
}

@end
