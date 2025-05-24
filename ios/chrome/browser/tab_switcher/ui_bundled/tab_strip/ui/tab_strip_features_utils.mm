// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_strip/ui/tab_strip_features_utils.h"

#import "base/feature_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"

@implementation TabStripFeaturesUtils

+ (BOOL)isModernTabStripNewTabButtonDynamic {
  return YES;
}

+ (BOOL)isModernTabStripWithTabGroups {
  return IsTabGroupInGridEnabled();
}

+ (BOOL)hasCloserNTB {
  return YES;
}

+ (BOOL)hasDarkerBackground {
  return NO;
}

+ (BOOL)hasDarkerBackgroundV3 {
  return YES;
}

+ (BOOL)hasNoNTBBackground {
  return NO;
}

+ (BOOL)hasBlackBackground {
  return NO;
}

+ (BOOL)hasBiggerNTB {
  return YES;
}

+ (BOOL)hasCloseButtonsVisible {
  return NO;
}

+ (BOOL)hasHighContrastInactiveTabs {
  return YES;
}

+ (BOOL)hasHighContrastNTB {
  return NO;
}

+ (BOOL)hasDetachedTabs {
  return NO;
}

@end
