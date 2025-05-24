// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INCOGNITO_REAUTH_UI_BUNDLED_FEATURES_H_
#define IOS_CHROME_BROWSER_INCOGNITO_REAUTH_UI_BUNDLED_FEATURES_H_

#import "base/feature_list.h"
#import "base/metrics/field_trial_params.h"
#import "base/time/time.h"

// Feature for the Soft Lock.
BASE_DECLARE_FEATURE(kIOSSoftLock);
// Parameter for the kIOSSoftLock feature. The time delay needed
// for Soft Lock to trigger.
extern const char kIOSSoftLockBackgroundThresholdParam[];
extern const base::FeatureParam<base::TimeDelta>
    kIOSSoftLockBackgroundThreshold;

// Whether the Soft Lock feature is enabled.
bool IsIOSSoftLockEnabled();

#endif  // IOS_CHROME_BROWSER_INCOGNITO_REAUTH_UI_BUNDLED_FEATURES_H_
