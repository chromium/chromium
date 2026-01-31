// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_START_SURFACE_UI_BUNDLED_START_SURFACE_FEATURES_H_
#define IOS_CHROME_BROWSER_START_SURFACE_UI_BUNDLED_START_SURFACE_FEATURES_H_

#include "base/feature_list.h"
#include "base/time/time.h"


// The feature to enable or disable the showTabGroupInGrid.
BASE_DECLARE_FEATURE(kShowTabGroupInGridOnStart);

// The feature parameter to indicate inactive duration to return to the Start
// Surface in seconds.
extern const char kReturnToStartSurfaceInactiveDurationInSeconds[];

// The feature parameter to indicate inactive duration to return to the tab
// group in grid view in seconds.
extern const char kShowTabGroupInGridInactiveDurationInSeconds[];

// Checks whether the showTabGroupInGrid feature should be enabled.
bool IsShowTabGroupInGridOnStartEnabled();

// Returns the inactive duration to show the tab group in grid view.
base::TimeDelta GetReturnToTabGroupInGridDuration();

#endif  // IOS_CHROME_BROWSER_START_SURFACE_UI_BUNDLED_START_SURFACE_FEATURES_H_
