// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui_bundled/tab_groups/tab_group_indicator_features_utils.h"

#import "ios/chrome/browser/shared/public/features/features.h"

bool HasTabGroupIndicatorVisible() {
  return true;
}

bool HasTabGroupIndicatorBelowOmnibox() {
  return false;
}

bool HasTabGroupIndicatorButtonsUpdated() {
  return true;
}
