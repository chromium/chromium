// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/features.h"

BASE_FEATURE(kUploadBlingAIPrototypingData, base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<std::string>
    kUploadBlingAIPrototypingDataLoggingTag{&kUploadBlingAIPrototypingData,
                                            /*name=*/"logging_tag",
                                            /*default_value=*/""};
constexpr base::FeatureParam<std::string>
    kUploadBlingAIPrototypingDataLoggingDescription{
        &kUploadBlingAIPrototypingData,
        /*name=*/"logging_description",
        /*default_value=*/""};

BASE_FEATURE(kStoreBlingAIPrototypingPageContextLocally,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsUploadBlingAIPrototypingDataEnabled() {
  return base::FeatureList::IsEnabled(kUploadBlingAIPrototypingData);
}

bool IsStoreBlingAIPrototypingPageContextLocallyEnabled() {
  return base::FeatureList::IsEnabled(
      kStoreBlingAIPrototypingPageContextLocally);
}
