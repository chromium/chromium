// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_PROTOTYPE_PUBLIC_FEATURES_H_
#define IOS_CHROME_BROWSER_AIM_PROTOTYPE_PUBLIC_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

// Used to enable development tools for the AIM prototype.
BASE_DECLARE_FEATURE(kAimPrototypeDevTools);

// The delay in milliseconds to simulate loading the full image.
extern const base::FeatureParam<int> kImageLoadDelayMs;

// The delay in milliseconds to simulate the upload time.
extern const base::FeatureParam<int> kUploadDelayMs;

// If true, the upload will be forced to fail.
extern const base::FeatureParam<bool> kForceUploadFailure;

// Returns the configured image load delay.
base::TimeDelta GetImageLoadDelay();

// Returns the configured upload delay.
base::TimeDelta GetUploadDelay();

// Returns whether to force the upload to fail.
bool ShouldForceUploadFailure();

#endif  // IOS_CHROME_BROWSER_AIM_PROTOTYPE_PUBLIC_FEATURES_H_
