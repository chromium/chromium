// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/incognito_reauth/ui_bundled/features.h"

#import "base/feature_list.h"
#import "base/metrics/field_trial_params.h"
#import "base/time/time.h"

BASE_FEATURE(kIOSSoftLock, "IOSSoftLock", base::FEATURE_DISABLED_BY_DEFAULT);

bool IsIOSSoftLockEnabled() {
  return base::FeatureList::IsEnabled(kIOSSoftLock);
}

const char kIOSSoftLockBackgroundThresholdParam[] =
    "soft-lock-background-threshold-minutes";

const base::FeatureParam<base::TimeDelta> kIOSSoftLockBackgroundThreshold{
    &kIOSSoftLock, kIOSSoftLockBackgroundThresholdParam, base::Minutes(10)};
