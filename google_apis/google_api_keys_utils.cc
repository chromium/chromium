// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/google_api_keys_utils.h"

#include "base/feature_list.h"
#include "base/features.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"

namespace google_apis {

namespace {

BASE_FEATURE(kOverrideAPIKeyFeature,
             "OverrideAPIKey",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kOverrideAPIKeyFeatureParam{
    &kOverrideAPIKeyFeature, /*name=*/"api_key", /*default_value=*/""};

}  // namespace

std::string GetAPIKeyOverrideViaFeature() {
  if (base::FeatureList::IsEnabled(kOverrideAPIKeyFeature)) {
    std::string override_api_key = kOverrideAPIKeyFeatureParam.Get();
    if (!override_api_key.empty()) {
      return override_api_key;
    }
  }
  return std::string();
}

void LogAPIKeysMatchHistogram(bool api_keys_match) {
  base::UmaHistogramBoolean("Signin.APIKeyMatchesFeatureOnStartup",
                            api_keys_match);
}

}  // namespace google_apis
