// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_FEATURES_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_FEATURES_H_

#include <cstddef>

#import "base/feature_list.h"
#import "base/metrics/field_trial_params.h"
#import "base/time/time.h"

namespace contextual_cueing {

// Feature flag controlling Gemini contextual suggestions cues framework.
BASE_DECLARE_FEATURE(kGeminiContextualSuggestionsCues);

// Returns true if Gemini contextual suggestions cues framework is enabled.
bool IsGeminiContextualSuggestionsCuesEnabled();

// Feature parameter that forces on-device page classification and server model
// execution, bypasses all contextual cueing frequency caps, backoff cooldowns,
// and Feature Engagement Tracker limits, and always presents the Message UI.
extern const char kGeminiContextualSuggestionsCuesIgnoreThresholdsParam[];

// Returns true if contextual cueing thresholds/caps should be ignored,
// on-device classification forced, and Message UI always returned.
bool IsIgnoreContextualCueingThresholdsEnabled();

// Feature parameter for enabling on-device category classifier in Gemini
// contextual suggestions cues.
extern const char kGeminiContextualSuggestionsCuesOnDeviceClassifierParam[];

// Returns true if on-device category classifier is enabled for Gemini
// contextual suggestions cues.
bool IsGeminiContextualSuggestionsCuesOnDeviceClassifierEnabled();

// Feature parameter for allowing GPU / Neural Engine execution in Gemini
// contextual suggestions cues.
extern const char kGeminiContextualSuggestionsCuesAllowGpuExecutionParam[];

// Returns true if GPU / Neural Engine execution is allowed for Gemini
// contextual suggestions cues.
bool IsGeminiContextualSuggestionsCuesAllowGpuExecutionEnabled();

// Feature parameter for using Title and URL only (matching Desktop) for
// category classification.
extern const char kGeminiContextualSuggestionsCuesTitleAndUrlOnlyParam[];

// Returns true if category classification should only use Title and URL
// instead of extracting APC and generating passages.
bool IsGeminiContextualSuggestionsCuesTitleAndUrlOnlyEnabled();

// Feature parameter for enabling server model execution in Gemini contextual
// suggestions cues.
extern const char kGeminiContextualSuggestionsCuesServerModelExecutionParam[];

// Returns true if server model execution is enabled for Gemini contextual
// suggestions cues.
bool IsGeminiContextualSuggestionsCuesServerModelExecutionEnabled();

// Category score thresholds (0.0 to 1.0) for on-device page classification.
BASE_DECLARE_FEATURE_PARAM(double, kEducationClassifierThreshold);
BASE_DECLARE_FEATURE_PARAM(double, kShoppingClassifierThreshold);

// Whether search engine results pages and root homepages are filtered out from
// contextual cue eligibility.
BASE_DECLARE_FEATURE_PARAM(bool, kFilterSearchAndHomepages);

// Global cap: maximum cues shown across all origins within
// `kGlobalCapDuration`.
BASE_DECLARE_FEATURE_PARAM(size_t, kGlobalCapCount);
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kGlobalCapDuration);

// Per-origin cap: maximum cues shown per origin within `kOriginCapDuration`.
BASE_DECLARE_FEATURE_PARAM(size_t, kOriginCapCount);
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kOriginCapDuration);

// Limit on how many recently visited origins are tracked in the LRU cache for
// per-origin frequency capping.
BASE_DECLARE_FEATURE_PARAM(size_t, kVisitedOriginsLimit);

// Minimum committed page navigations required between showing cues.
BASE_DECLARE_FEATURE_PARAM(size_t, kMinPageCountBetweenNudges);

// Base backoff cooldown and exponential multiplier applied when a cue is shown
// or ignored without user interaction.
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kMinTimeBetweenNudges);
BASE_DECLARE_FEATURE_PARAM(double, kIgnoreBackoffMultiplierBase);

// Base backoff cooldown and exponential multiplier applied after a user
// explicitly dismisses a cue.
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kBaseDismissBackoffTime);
BASE_DECLARE_FEATURE_PARAM(double, kDismissBackoffMultiplierBase);

// Backoff cooldown applied after a user accepts (clicks) a cue.
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kClickBackoffTime);

// Whether all frequency capping and cooldown backoff logic is disabled.
BASE_DECLARE_FEATURE_PARAM(bool, kDisableFrequencyCappingAndBackoff);

// Maximum number of eligible background tabs to include in contextual cue model
// execution requests.
BASE_DECLARE_FEATURE_PARAM(size_t, kMaxBackgroundTabs);

// Maximum consecutive Message UI impressions ignored without interaction for a
// vertical before permanently switching to Omnibox Chip UI for that vertical.
BASE_DECLARE_FEATURE_PARAM(size_t, kMaxConsecutiveMessageIgnores);

// When true, forces contextual cues to always use Message UI and never switch
// to Omnibox Chip UI.
BASE_DECLARE_FEATURE_PARAM(bool, kForceMessageUiOnly);

// When true, forces contextual cues to always use Omnibox Chip UI and skip
// Message UI.
BASE_DECLARE_FEATURE_PARAM(bool, kForceOmniboxChipUiOnly);

// Whether to use Private AI (`ModelExecutionServiceType::kPrivateAi`) when
// executing contextual cue model requests.
BASE_DECLARE_FEATURE_PARAM(bool, kUsePrivateAi);

}  // namespace contextual_cueing

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_FEATURES_H_
