// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/prototype/public/features.h"

#import "base/metrics/field_trial_params.h"
#import "base/time/time.h"

BASE_FEATURE(kAimPrototypeDevTools,
             "AimPrototypeDevTools",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kImageLoadDelayMs{&kAimPrototypeDevTools,
                                                "image_load_delay_ms", 0};
const base::FeatureParam<int> kUploadDelayMs{&kAimPrototypeDevTools,
                                             "upload_delay_ms", 0};
const base::FeatureParam<bool> kForceUploadFailure{
    &kAimPrototypeDevTools, "force_upload_failure", false};

base::TimeDelta GetImageLoadDelay() {
  if (!base::FeatureList::IsEnabled(kAimPrototypeDevTools)) {
    return base::TimeDelta();
  }
  return base::Milliseconds(kImageLoadDelayMs.Get());
}

base::TimeDelta GetUploadDelay() {
  if (!base::FeatureList::IsEnabled(kAimPrototypeDevTools)) {
    return base::TimeDelta();
  }
  return base::Milliseconds(kUploadDelayMs.Get());
}

bool ShouldForceUploadFailure() {
  if (!base::FeatureList::IsEnabled(kAimPrototypeDevTools)) {
    return false;
  }
  return kForceUploadFailure.Get();
}
