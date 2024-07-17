// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/ui/tab_strip_utils.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/ui/tab_strip_features_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@implementation TabStripHelper

+ (UIColor*)backgroundColor {
  if ([TabStripFeaturesUtils isTabStripDarkerBackgroundEnabled] ||
      [TabStripFeaturesUtils isTabStripCloserNTBDarkerBackgroundEnabled]) {
    return [UIColor colorNamed:kTabStripBackgroundColor];
  } else if ([TabStripFeaturesUtils isTabStripBlackBackgroundEnabled]) {
    return UIColor.blackColor;
  }
  return [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
}

+ (UIColor*)newTabButtonSymbolColor {
  if ([TabStripFeaturesUtils isTabStripBlackBackgroundEnabled]) {
    return [UIColor colorNamed:kStaticGrey600Color];
  } else if ([TabStripFeaturesUtils isTabStripV2]) {
    return [UIColor colorNamed:kTabStripNewTabButtonColor];
  }
  return [UIColor colorNamed:kTextSecondaryColor];
}

@end
