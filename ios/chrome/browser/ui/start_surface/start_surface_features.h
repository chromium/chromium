// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_START_SURFACE_START_SURFACE_FEATURES_H_
#define IOS_CHROME_BROWSER_UI_START_SURFACE_START_SURFACE_FEATURES_H_

#include "base/feature_list.h"

// Start Surface experiment params.
extern const char kStartSurfaceHideShortcutsParam[];
extern const char kStartSurfaceShrinkLogoParam[];
extern const char kStartSurfaceReturnToRecentTabParam[];

// The feature to enable or disable the Start Surface.
BASE_DECLARE_FEATURE(kStartSurface);

// The feature parameter to indicate inactive duration to return to the Start
// Surface in seconds.
extern const char kReturnToStartSurfaceInactiveDurationInSeconds[];

// Checks whether the Start Surface should be enabled.
bool IsStartSurfaceEnabled();

// Returns the inactive duration to show the Start Surface.
double GetReturnToStartSurfaceDuration();

// Returns true if the shortcuts should be hidden on NTP for the Start Surface
bool ShouldHideShortcutsForStartSurface();

// Returns true if the Google logo should be shrunk on NTP for the Start
// Surface.
bool ShouldShrinkLogoForStartSurface();

// Returns true if the most recent tab tile should be created on NTP for the
// Start Surface.
bool ShouldShowReturnToMostRecentTabForStartSurface();

#endif  // IOS_CHROME_BROWSER_UI_START_SURFACE_START_SURFACE_FEATURES_H_.
