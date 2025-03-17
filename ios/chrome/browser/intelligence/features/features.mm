// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/features/features.h"

BASE_FEATURE(kEnhancedCalendar,
             "EnhancedCalendar",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsEnhancedCalendarEnabled() {
  return base::FeatureList::IsEnabled(kEnhancedCalendar);
}
