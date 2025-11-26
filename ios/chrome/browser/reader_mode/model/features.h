// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_MODEL_FEATURES_H_
#define IOS_CHROME_BROWSER_READER_MODE_MODEL_FEATURES_H_

#import "base/feature_list.h"
#import "base/time/time.h"

// Feature to enable Reader Mode UI and entry points.
BASE_DECLARE_FEATURE(kEnableReaderMode);

// Feature to enable Reader Mode UI in the US country code.
BASE_DECLARE_FEATURE(kEnableReaderModeInUS);

// Feature to enable Reader Mode omnibox entry point.
BASE_DECLARE_FEATURE(kEnableReaderModeOmniboxEntryPoint);

// Feature to enable Reader Mode translation.
BASE_DECLARE_FEATURE(kEnableReaderModeTranslation);

// Feature to enable Reader Mode translation with access to the translation
// settings from the infobar framework.
BASE_DECLARE_FEATURE(kEnableReaderModeTranslationWithInfobar);

// Feature to enable page eligibility heuristic to determine whether the Tools
// menu Reader Mode entry point should be shown for the web page.
BASE_DECLARE_FEATURE(kEnableReaderModePageEligibilityForToolsMenu);

// Feature to enable Readability heuristic for page triggering eligibility.
BASE_DECLARE_FEATURE(kEnableReadabilityHeuristic);

// Feature to enable optimization guide eligibility check.
BASE_DECLARE_FEATURE(kEnableReaderModeOptimizationGuideEligibility);

// Name to configure the duration string for heuristic page load delay. See
// `base::TimeDeltaFromString` for valid duration string configurations.
extern const char kReaderModeHeuristicPageLoadDelayDurationStringName[];

// Name to configure the duration string for distillation timeout. See
// `base::TimeDeltaFromString` for valid duration string configurations.
extern const char kReaderModeDistillationTimeoutDurationStringName[];

// Returns the timeout for distilling Reader Mode.
const base::TimeDelta ReaderModeDistillationTimeout();

// Returns the delay time before triggering Reader Mode heuristic on page load.
const base::TimeDelta ReaderModeHeuristicPageLoadDelay();

// Returns whether the Reader Mode feature is available.
bool IsReaderModeAvailable();

// Returns whether the omnibox entrypoint is enabled.
bool IsReaderModeOmniboxEntryPointEnabled();

// Returns whether translation is enabled while in Reading Mode.
bool IsReaderModeTranslationAvailable();

// Returns whether optimization guide eligibility check is enabled.
bool IsReaderModeOptimizationGuideEligibilityAvailable();

#endif  // IOS_CHROME_BROWSER_READER_MODE_MODEL_FEATURES_H_
