// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_MODEL_FEATURES_H_
#define IOS_CHROME_BROWSER_READER_MODE_MODEL_FEATURES_H_

#import "base/feature_list.h"
#import "base/metrics/field_trial_params.h"

// Feature to enable Reader Mode page distillation heuristic that tracks
// an approximation of when the Reader Mode UI will be available.
BASE_DECLARE_FEATURE(kEnableReaderModeDistillerHeuristic);

// Feature to enable Reader Mode page distillation.
BASE_DECLARE_FEATURE(kEnableReaderModeDistiller);

// Name to configure the page load probability.
extern const char kReaderModeDistillerPageLoadProbabilityName[];
// Configurable rate from (0, 1] at which to trigger the distiller heuristic.
extern const base::FeatureParam<double> kReaderModeDistillerPageLoadProbability;

// Name to configure the duration string for page load delay. See
// `base::TimeDeltaFromString` for valid duration string configurations.
extern const char kReaderModeDistillerPageLoadDelayDurationStringName[];

// Returns the delay time before triggering Reader Mode on page load.
const base::TimeDelta ReaderModeDistillerPageLoadDelay();

#endif  // IOS_CHROME_BROWSER_READER_MODE_MODEL_FEATURES_H_
