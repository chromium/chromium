// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/page_classification/features.h"

#import "base/metrics/field_trial_params.h"

BASE_FEATURE(kPageClassification, base::FEATURE_DISABLED_BY_DEFAULT);

const char kPageClassificationModeParam[] = "classification_mode";

constexpr base::FeatureParam<PageClassificationMode>::Option
    kPageClassificationModeOptions[] = {
        {PageClassificationMode::kOnDeviceOnly, "on_device_only"},
        {PageClassificationMode::kVerticalsOnly, "verticals_only"},
        {PageClassificationMode::kOnDeviceWithVerticalsFallback,
         "on_device_with_fallback"}};

constexpr base::FeatureParam<PageClassificationMode> kPageClassificationMode{
    &kPageClassification, kPageClassificationModeParam,
    PageClassificationMode::kOnDeviceOnly, &kPageClassificationModeOptions};

const char kPageClassificationOnDeviceClassifierParam[] =
    "enable_on_device_classifier";

BASE_FEATURE_PARAM(bool,
                   kPageClassificationOnDeviceClassifier,
                   &kPageClassification,
                   kPageClassificationOnDeviceClassifierParam,
                   false);

const char kPageClassificationAllowGpuExecutionParam[] = "allow_gpu_execution";

BASE_FEATURE_PARAM(bool,
                   kPageClassificationAllowGpuExecution,
                   &kPageClassification,
                   kPageClassificationAllowGpuExecutionParam,
                   false);

const char kPageClassificationTitleAndUrlOnlyParam[] = "use_title_and_url_only";

BASE_FEATURE_PARAM(bool,
                   kPageClassificationTitleAndUrlOnly,
                   &kPageClassification,
                   kPageClassificationTitleAndUrlOnlyParam,
                   true);

bool IsPageClassificationEnabled() {
  return base::FeatureList::IsEnabled(kPageClassification);
}

PageClassificationMode GetPageClassificationMode() {
  if (base::FeatureList::IsEnabled(kPageClassification)) {
    return kPageClassificationMode.Get();
  }
  return PageClassificationMode::kOnDeviceOnly;
}

bool IsPageClassificationOnDeviceClassifierEnabled() {
  return IsPageClassificationEnabled() &&
         kPageClassificationOnDeviceClassifier.Get();
}

bool IsPageClassificationAllowGpuExecutionEnabled() {
  return IsPageClassificationEnabled() &&
         kPageClassificationAllowGpuExecution.Get();
}

bool IsPageClassificationTitleAndUrlOnlyEnabled() {
  return kPageClassificationTitleAndUrlOnly.Get();
}
