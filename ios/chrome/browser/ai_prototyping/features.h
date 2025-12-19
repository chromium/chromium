// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_FEATURES_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_FEATURES_H_

#import <string>

#import "base/feature_list.h"
#import "base/metrics/field_trial_params.h"

// Test-only: Feature flag to enable uploading of BlingAIPrototyping logs during
// manual test.
BASE_DECLARE_FEATURE(kUploadBlingAIPrototypingData);

// The logging tag included in BlingPrototypingLoggingData to use for querying
// the logs.
extern const base::FeatureParam<std::string>
    kUploadBlingAIPrototypingDataLoggingTag;

// The optional logging description included in BlingPrototypingLoggingData for
// additional detail.
extern const base::FeatureParam<std::string>
    kUploadBlingAIPrototypingDataLoggingDescription;

// Test-only: Feature flag to enable storing of PageContext during manual test.
BASE_DECLARE_FEATURE(kStoreBlingAIPrototypingPageContextLocally);

bool IsUploadBlingAIPrototypingDataEnabled();

bool IsStoreBlingAIPrototypingPageContextLocallyEnabled();

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_FEATURES_H_
