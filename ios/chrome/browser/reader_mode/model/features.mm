// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/features.h"

#import "base/feature_list.h"
#import "base/metrics/field_trial_params.h"
#import "ios/chrome/browser/reader_mode/model/constants.h"

BASE_FEATURE(kEnableReaderModeDistillerHeuristic,
             "EnableReaderModeDistillerHeuristic",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableReaderModeDistiller,
             "EnableReaderModeDistiller",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kReaderModeDistillerPageLoadProbabilityName[] =
    "reader-mode-distiller-page-load-probability";

constexpr base::FeatureParam<double> kReaderModeDistillerPageLoadProbability{
    &kEnableReaderModeDistillerHeuristic,
    /*name=*/kReaderModeDistillerPageLoadProbabilityName,
    /*default_value=*/0.001};

const char kReaderModeDistillerPageLoadDelayDurationStringName[] =
    "reader-mode-distiller-page-load-delay-duration-string";

const base::TimeDelta ReaderModeDistillerPageLoadDelay() {
  return base::GetFieldTrialParamByFeatureAsTimeDelta(
      kEnableReaderModeDistillerHeuristic,
      /*name=*/kReaderModeDistillerPageLoadDelayDurationStringName,
      /*default_value=*/kReaderModeDistillerPageLoadDelay);
}
