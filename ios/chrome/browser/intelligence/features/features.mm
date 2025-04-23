// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/features/features.h"

#import "base/metrics/field_trial_params.h"

BASE_FEATURE(kEnhancedCalendar,
             "EnhancedCalendar",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsEnhancedCalendarEnabled() {
  return base::FeatureList::IsEnabled(kEnhancedCalendar);
}

BASE_FEATURE(kPageActionMenu,
             "PageActionMenu",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsPageActionMenuEnabled() {
  return base::FeatureList::IsEnabled(kPageActionMenu);
}

const char kExplainGeminiEditMenuParams[] = "PositionForExplainGeminiEditMenu";

PositionForExplainGeminiEditMenu ExplainGeminiEditMenuPosition() {
  int param = base::GetFieldTrialParamByFeatureAsInt(
      kExplainGeminiEditMenu, kExplainGeminiEditMenuParams, 0);
  if (param == 1) {
    return PositionForExplainGeminiEditMenu::kAfterEdit;
  }
  if (param == 2) {
    return PositionForExplainGeminiEditMenu::kAfterSearch;
  }
  return PositionForExplainGeminiEditMenu::kDisabled;
}

BASE_FEATURE(kExplainGeminiEditMenu,
             "ExplainGeminiEditMenu",
             base::FEATURE_DISABLED_BY_DEFAULT);
