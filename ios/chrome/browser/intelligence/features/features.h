// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_FEATURES_FEATURES_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_FEATURES_FEATURES_H_

#import "base/feature_list.h"

// Feature flag controlling whether enhanced calendar is enabled.
BASE_DECLARE_FEATURE(kEnhancedCalendar);

// Returns true if enhanced calendar is enabled.
bool IsEnhancedCalendarEnabled();

// Feature flag controlling the page action menu.
BASE_DECLARE_FEATURE(kPageActionMenu);

// Returns true if the page action menu is enabled.
bool IsPageActionMenuEnabled();

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_FEATURES_FEATURES_H_
