// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_MODEL_FEATURES_H_
#define IOS_CHROME_BROWSER_READER_MODE_MODEL_FEATURES_H_

#import "base/feature_list.h"
#import "base/time/time.h"

// Feature to enable Reader Mode UI in the US country code.
BASE_DECLARE_FEATURE(kEnableReaderModeInUS);

// Feature to enable Reader Mode omnibox entry point in the US country code.
BASE_DECLARE_FEATURE(kEnableReaderModeOmniboxEntryPointInUS);

// Feature to enable Reader Mode translation with access to the translation
// settings from the infobar framework.
BASE_DECLARE_FEATURE(kEnableReaderModeTranslationWithInfobar);

// Feature to enable Readability heuristic for page triggering eligibility.
BASE_DECLARE_FEATURE(kEnableReadabilityHeuristic);

// Feature to enable optimization guide eligibility check.
BASE_DECLARE_FEATURE(kEnableReaderModeOptimizationGuideEligibility);

// Feature to enable disabling links in Reading Mode from Content Settings.
BASE_DECLARE_FEATURE(kEnableContentSettingsOptionForLinks);

// Returns whether the Reader Mode feature is available.
bool IsReaderModeAvailable();

// Returns whether the omnibox entrypoint is enabled.
bool IsReaderModeOmniboxEntryPointEnabled();

// Returns whether optimization guide eligibility check is enabled.
bool IsReaderModeOptimizationGuideEligibilityAvailable();

// Returns whether option to disable links in Content Settings is enabled.
bool IsReaderModeContentSettingsForLinkEnabled();

#endif  // IOS_CHROME_BROWSER_READER_MODE_MODEL_FEATURES_H_
