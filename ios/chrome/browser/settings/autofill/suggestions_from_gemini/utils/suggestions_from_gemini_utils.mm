// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/utils/suggestions_from_gemini_utils.h"

#import "components/optimization_guide/core/feature_registry/feature_registration.h"
#import "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#import "components/prefs/pref_service.h"

SuggestionsFromGeminiPolicyState GetSuggestionsFromGeminiPolicyState(
    const PrefService* pref_service) {
  if (!pref_service) {
    return SuggestionsFromGeminiPolicyState::kFullyAllowed;
  }

  BOOL allowedWithoutLogging =
      pref_service->IsManagedPreference(
          optimization_guide::prefs::kFindAndFillWithGeminiSettings) &&
      pref_service->GetInteger(
          optimization_guide::prefs::kFindAndFillWithGeminiSettings) ==
          static_cast<int>(
              optimization_guide::model_execution::prefs::
                  ModelExecutionEnterprisePolicyValue::kAllowWithoutLogging);

  BOOL disabledByPolicy =
      pref_service->IsManagedPreference(
          optimization_guide::prefs::kFindAndFillWithGeminiSettings) &&
      pref_service->GetInteger(
          optimization_guide::prefs::kFindAndFillWithGeminiSettings) ==
          static_cast<int>(optimization_guide::model_execution::prefs::
                               ModelExecutionEnterprisePolicyValue::kDisable);

  if (disabledByPolicy) {
    return SuggestionsFromGeminiPolicyState::kFullyDisabled;
  }
  if (allowedWithoutLogging) {
    return SuggestionsFromGeminiPolicyState::kLoggingDisabled;
  }
  return SuggestionsFromGeminiPolicyState::kFullyAllowed;
}
