// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/ui/tab_strip_features_utils.h"

#import "base/feature_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/features_utils.h"

@implementation TabStripFeaturesUtils

+ (BOOL)isModernTabStripNewTabButtonDynamic {
  CHECK(IsModernTabStripOrRaccoonEnabled());
  std::string feature_param = base::GetFieldTrialParamValueByFeature(
      kModernTabStrip, kModernTabStripParameterName);
  return feature_param != kModernTabStripNTBStaticParam;
}

+ (BOOL)isModernTabStripWithTabGroups {
  return IsTabGroupInGridEnabled();
}

@end
