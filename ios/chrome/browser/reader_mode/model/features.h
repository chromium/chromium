// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_MODEL_FEATURES_H_
#define IOS_CHROME_BROWSER_READER_MODE_MODEL_FEATURES_H_

#import "base/feature_list.h"
#import "base/metrics/field_trial_params.h"

// Feature to enable Reader Mode UI and entry points.
BASE_DECLARE_FEATURE(kEnableReaderMode);

// Feature to enable page eligibility heuristic to determine whether the Tools
// menu Reader Mode entry point should be shown for the web page.
BASE_DECLARE_FEATURE(kEnableReaderModePageEligibilityForToolsMenu);

// Feature to enable debugging information for Reader Mode UI.
BASE_DECLARE_FEATURE(kEnableReaderModeDebugInfo);

// Name to configure the duration string for page load delay. See
// `base::TimeDeltaFromString` for valid duration string configurations.
extern const char kReaderModeDistillerPageLoadDelayDurationStringName[];

// Returns the delay time before triggering Reader Mode on page load.
const base::TimeDelta ReaderModeDistillerPageLoadDelay();

// Returns whether the Reader Mode feature is available.
bool IsReaderModeAvailable();

// Returns whether the Reader Mode snackbar is enabled.
bool IsReaderModeSnackbarEnabled();

#endif  // IOS_CHROME_BROWSER_READER_MODE_MODEL_FEATURES_H_
