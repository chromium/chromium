// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_PROTOTYPE_PUBLIC_FEATURES_H_
#define IOS_CHROME_BROWSER_AIM_PROTOTYPE_PUBLIC_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

// Used to enable development tools for the AIM prototype.
BASE_DECLARE_FEATURE(kAimPrototypeDevTools);

// Parameter of `kAimPrototypeDevTools` to delay image loading.
extern const char kImageLoadDelayMsParam[];
// Parameter of `kAimPrototypeDevTools` to delay image upload.
extern const char kUploadDelayMsParam[];
// Parameter of `kAimPrototypeDevTools` to force image upload failure.
extern const char kForceUploadFailureParam[];

// Returns the configured image load delay.
base::TimeDelta GetImageLoadDelay();

// Returns the configured upload delay.
base::TimeDelta GetUploadDelay();

// Returns whether to force the upload to fail.
bool ShouldForceUploadFailure();

#endif  // IOS_CHROME_BROWSER_AIM_PROTOTYPE_PUBLIC_FEATURES_H_
