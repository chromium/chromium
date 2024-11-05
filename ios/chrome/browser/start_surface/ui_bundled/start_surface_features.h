// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_START_SURFACE_UI_BUNDLED_START_SURFACE_FEATURES_H_
#define IOS_CHROME_BROWSER_START_SURFACE_UI_BUNDLED_START_SURFACE_FEATURES_H_

#include "base/feature_list.h"
#include "base/time/time.h"

// Enum to represent states of the IOSStartTimeStartupRemediations feature.
enum class StartupRemediationsType {
  kOpenNewNTPTab = 0,
  kSaveNewNTPWebState = 1,
  kDisabled = 2,
};

// The feature to enable or disable the Start Surface.
BASE_DECLARE_FEATURE(kStartSurface);

// Feature to enable Startup time remediations in the Start Surface.
BASE_DECLARE_FEATURE(kIOSStartTimeStartupRemediations);

// The feature parameter to indicate inactive duration to return to the Start
// Surface in seconds.
extern const char kReturnToStartSurfaceInactiveDurationInSeconds[];

// The feature parameter for kIOSStartTimeStartupRemediations to indicate if the
// "SaveNewNTPWebState" remediation will be enabled.
extern const char kIOSStartTimeStartupRemediationsSaveNTPWebState[];

// Checks whether the Start Surface should be enabled.
bool IsStartSurfaceEnabled();

// Returns the inactive duration to show the Start Surface.
base::TimeDelta GetReturnToStartSurfaceDuration();

// Returns the state of the IOSStartTimeStartupRemediations feature.
StartupRemediationsType GetIOSStartTimeStartupRemediationsEnabledType();

#endif  // IOS_CHROME_BROWSER_START_SURFACE_UI_BUNDLED_START_SURFACE_FEATURES_H_
