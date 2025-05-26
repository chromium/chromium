// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/features.h"

#import "base/feature_list.h"
#import "base/metrics/field_trial_params.h"
#import "ios/chrome/browser/reader_mode/model/constants.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"

BASE_FEATURE(kEnableReaderMode,
             "EnableReaderMode",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableReaderModePageEligibilityForToolsMenu,
             "EnableReaderModePageEligibilityForToolsMenu",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableReaderModeDebugInfo,
             "EnableReaderModeDebugInfo",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kReaderModeDistillerPageLoadDelayDurationStringName[] =
    "reader-mode-distiller-page-load-delay-duration-string";

const base::TimeDelta ReaderModeDistillerPageLoadDelay() {
  return base::GetFieldTrialParamByFeatureAsTimeDelta(
      kEnableReaderMode,
      /*name=*/kReaderModeDistillerPageLoadDelayDurationStringName,
      /*default_value=*/kReaderModeDistillerPageLoadDelay);
}

bool IsReaderModeAvailable() {
  return base::FeatureList::IsEnabled(kEnableReaderMode);
}

bool IsReaderModeSnackbarEnabled() {
  return base::FeatureList::IsEnabled(kEnableReaderModeDebugInfo);
}
