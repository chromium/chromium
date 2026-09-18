// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_CLASSIFICATION_FEATURES_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_CLASSIFICATION_FEATURES_H_

#import "base/feature_list.h"

// Feature flag controlling the page classification framework.
BASE_DECLARE_FEATURE(kPageClassification);

// Returns true if the page classification framework is enabled.
bool IsPageClassificationEnabled();

// Modes for page classification.
enum class PageClassificationMode {
  // Plan A only: On-device ML model.
  kOnDeviceOnly = 0,
  // Plan B only: Vertical heuristics (OptimizationGuide + Shopping + DOM).
  kVerticalsOnly = 1,
  // Plan A with Plan B fallback: runs on-device model first, falls back to
  // verticals only if the model is unavailable, encounters an error, or
  // times out.
  kOnDeviceWithVerticalsFallback = 2,
};

// Feature parameter for the classification mode in page classification.
extern const char kPageClassificationModeParam[];

// Returns the configured classification mode for page classification.
PageClassificationMode GetPageClassificationMode();

// Feature parameter for enabling on-device category classifier in page
// classification.
extern const char kPageClassificationOnDeviceClassifierParam[];

// Returns true if on-device category classifier is enabled for page
// classification.
bool IsPageClassificationOnDeviceClassifierEnabled();

// Feature parameter for allowing GPU / Neural Engine execution in page
// classification.
extern const char kPageClassificationAllowGpuExecutionParam[];

// Returns true if GPU / Neural Engine execution is allowed for page
// classification.
bool IsPageClassificationAllowGpuExecutionEnabled();

// Feature parameter for using Title and URL only for category classification.
extern const char kPageClassificationTitleAndUrlOnlyParam[];

// Returns true if category classification should only use Title and URL
// instead of extracting APC and generating passages.
bool IsPageClassificationTitleAndUrlOnlyEnabled();

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_CLASSIFICATION_FEATURES_H_
