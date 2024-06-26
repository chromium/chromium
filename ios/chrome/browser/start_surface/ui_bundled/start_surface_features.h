// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_START_SURFACE_UI_BUNDLED_START_SURFACE_FEATURES_H_
#define IOS_CHROME_BROWSER_START_SURFACE_UI_BUNDLED_START_SURFACE_FEATURES_H_

#include "base/feature_list.h"
#include "base/time/time.h"

// The feature to enable or disable the Start Surface.
BASE_DECLARE_FEATURE(kStartSurface);

// The feature parameter to indicate inactive duration to return to the Start
// Surface in seconds.
extern const char kReturnToStartSurfaceInactiveDurationInSeconds[];

// Checks whether the Start Surface should be enabled.
bool IsStartSurfaceEnabled();

// Returns the inactive duration to show the Start Surface.
base::TimeDelta GetReturnToStartSurfaceDuration();

#endif  // IOS_CHROME_BROWSER_START_SURFACE_UI_BUNDLED_START_SURFACE_FEATURES_H_
