// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui_bundled/tab_groups/tab_group_indicator_features_utils.h"

#import "ios/chrome/browser/shared/public/features/features.h"

const char kTabGroupIndicatorVisible[] = "tab-group-indicator-visible";
const char kTabGroupIndicatorBelowOmnibox[] =
    "tab-group-indicator-below-omnibox";
const char kTabGroupIndicatorButtonsUpdate[] = "tab-group-indicator-buttons";

bool HasTabGroupIndicatorVisible() {
  CHECK(IsTabGroupIndicatorEnabled());
  return GetFieldTrialParamByFeatureAsBool(kTabGroupIndicator,
                                           kTabGroupIndicatorVisible, true);
}

bool HasTabGroupIndicatorBelowOmnibox() {
  CHECK(IsTabGroupIndicatorEnabled());
  return GetFieldTrialParamByFeatureAsBool(
      kTabGroupIndicator, kTabGroupIndicatorBelowOmnibox, false);
}

bool HasTabGroupIndicatorButtonsUpdated() {
  CHECK(IsTabGroupIndicatorEnabled());
  return GetFieldTrialParamByFeatureAsBool(
      kTabGroupIndicator, kTabGroupIndicatorButtonsUpdate, true);
}
