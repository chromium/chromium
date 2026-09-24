// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/contextual_cueing/features.h"

#import "ios/chrome/browser/intelligence/features/features.h"

namespace contextual_cueing {

BASE_FEATURE(kGeminiContextualSuggestionsCues,
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kGeminiContextualSuggestionsCuesOnDeviceClassifierParam[] =
    "enable_on_device_classifier";

BASE_FEATURE_PARAM(bool,
                   kGeminiContextualSuggestionsCuesOnDeviceClassifier,
                   &kGeminiContextualSuggestionsCues,
                   kGeminiContextualSuggestionsCuesOnDeviceClassifierParam,
                   false);

const char kGeminiContextualSuggestionsCuesAllowGpuExecutionParam[] =
    "allow_gpu_execution";

BASE_FEATURE_PARAM(bool,
                   kGeminiContextualSuggestionsCuesAllowGpuExecution,
                   &kGeminiContextualSuggestionsCues,
                   kGeminiContextualSuggestionsCuesAllowGpuExecutionParam,
                   false);

const char kGeminiContextualSuggestionsCuesTitleAndUrlOnlyParam[] =
    "use_title_and_url_only";

BASE_FEATURE_PARAM(bool,
                   kGeminiContextualSuggestionsCuesTitleAndUrlOnly,
                   &kGeminiContextualSuggestionsCues,
                   kGeminiContextualSuggestionsCuesTitleAndUrlOnlyParam,
                   true);

const char kGeminiContextualSuggestionsCuesServerModelExecutionParam[] =
    "enable_server_model_execution";

BASE_FEATURE_PARAM(bool,
                   kGeminiContextualSuggestionsCuesServerModelExecution,
                   &kGeminiContextualSuggestionsCues,
                   kGeminiContextualSuggestionsCuesServerModelExecutionParam,
                   false);

bool IsGeminiContextualSuggestionsCuesEnabled() {
  if (!IsPageActionMenuEnabled()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kGeminiContextualSuggestionsCues);
}

bool IsGeminiContextualSuggestionsCuesOnDeviceClassifierEnabled() {
  return IsGeminiContextualSuggestionsCuesEnabled() &&
         kGeminiContextualSuggestionsCuesOnDeviceClassifier.Get();
}

bool IsGeminiContextualSuggestionsCuesAllowGpuExecutionEnabled() {
  return IsGeminiContextualSuggestionsCuesEnabled() &&
         kGeminiContextualSuggestionsCuesAllowGpuExecution.Get();
}

bool IsGeminiContextualSuggestionsCuesTitleAndUrlOnlyEnabled() {
  return IsGeminiContextualSuggestionsCuesEnabled() &&
         kGeminiContextualSuggestionsCuesTitleAndUrlOnly.Get();
}

bool IsGeminiContextualSuggestionsCuesServerModelExecutionEnabled() {
  return IsGeminiContextualSuggestionsCuesEnabled() &&
         kGeminiContextualSuggestionsCuesServerModelExecution.Get();
}

BASE_FEATURE_PARAM(double,
                   kEducationClassifierThreshold,
                   &kGeminiContextualSuggestionsCues,
                   0.5);

BASE_FEATURE_PARAM(double,
                   kShoppingClassifierThreshold,
                   &kGeminiContextualSuggestionsCues,
                   0.5);

BASE_FEATURE_PARAM(bool,
                   kFilterSearchAndHomepages,
                   &kGeminiContextualSuggestionsCues,
                   true);

BASE_FEATURE_PARAM(size_t,
                   kGlobalCapCount,
                   &kGeminiContextualSuggestionsCues,
                   4);

BASE_FEATURE_PARAM(base::TimeDelta,
                   kGlobalCapDuration,
                   &kGeminiContextualSuggestionsCues,
                   base::Hours(24));

BASE_FEATURE_PARAM(size_t,
                   kOriginCapCount,
                   &kGeminiContextualSuggestionsCues,
                   1);

BASE_FEATURE_PARAM(base::TimeDelta,
                   kOriginCapDuration,
                   &kGeminiContextualSuggestionsCues,
                   base::Hours(1));

BASE_FEATURE_PARAM(size_t,
                   kVisitedOriginsLimit,
                   &kGeminiContextualSuggestionsCues,
                   20);

BASE_FEATURE_PARAM(size_t,
                   kMinPageCountBetweenNudges,
                   &kGeminiContextualSuggestionsCues,
                   10);

BASE_FEATURE_PARAM(base::TimeDelta,
                   kMinTimeBetweenNudges,
                   &kGeminiContextualSuggestionsCues,
                   base::Minutes(60));

BASE_FEATURE_PARAM(double,
                   kIgnoreBackoffMultiplierBase,
                   &kGeminiContextualSuggestionsCues,
                   1.5);

BASE_FEATURE_PARAM(base::TimeDelta,
                   kBaseDismissBackoffTime,
                   &kGeminiContextualSuggestionsCues,
                   base::Hours(24));

BASE_FEATURE_PARAM(double,
                   kDismissBackoffMultiplierBase,
                   &kGeminiContextualSuggestionsCues,
                   2.0);

BASE_FEATURE_PARAM(base::TimeDelta,
                   kClickBackoffTime,
                   &kGeminiContextualSuggestionsCues,
                   base::Minutes(30));

BASE_FEATURE_PARAM(bool,
                   kDisableFrequencyCappingAndBackoff,
                   &kGeminiContextualSuggestionsCues,
                   false);

BASE_FEATURE_PARAM(size_t,
                   kMaxBackgroundTabs,
                   &kGeminiContextualSuggestionsCues,
                   5);

BASE_FEATURE_PARAM(size_t,
                   kMaxConsecutiveMessageIgnores,
                   &kGeminiContextualSuggestionsCues,
                   3);

BASE_FEATURE_PARAM(bool,
                   kForceMessageUiOnly,
                   &kGeminiContextualSuggestionsCues,
                   false);

BASE_FEATURE_PARAM(bool,
                   kForceOmniboxChipUiOnly,
                   &kGeminiContextualSuggestionsCues,
                   false);

}  // namespace contextual_cueing
