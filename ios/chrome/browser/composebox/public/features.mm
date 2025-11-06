// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/public/features.h"

#import "base/metrics/field_trial_params.h"
#import "base/time/time.h"

BASE_FEATURE(kAIMPrototypeDevTools, base::FEATURE_DISABLED_BY_DEFAULT);

const char kImageLoadDelayMsParam[] = "image_load_delay_ms";
const char kUploadDelayMsParam[] = "upload_delay_ms";
const char kForceUploadFailureParam[] = "force_upload_failure";

base::TimeDelta GetImageLoadDelay() {
  if (!base::FeatureList::IsEnabled(kAIMPrototypeDevTools)) {
    return base::TimeDelta();
  }
  static const base::FeatureParam<int> kImageLoadDelayMs{
      &kAIMPrototypeDevTools, kImageLoadDelayMsParam, 0};
  return base::Milliseconds(kImageLoadDelayMs.Get());
}

base::TimeDelta GetUploadDelay() {
  if (!base::FeatureList::IsEnabled(kAIMPrototypeDevTools)) {
    return base::TimeDelta();
  }
  static const base::FeatureParam<int> kUploadDelayMs{&kAIMPrototypeDevTools,
                                                      kUploadDelayMsParam, 0};
  return base::Milliseconds(kUploadDelayMs.Get());
}

bool ShouldForceUploadFailure() {
  if (!base::FeatureList::IsEnabled(kAIMPrototypeDevTools)) {
    return false;
  }
  static const base::FeatureParam<bool> kForceUploadFailure{
      &kAIMPrototypeDevTools, kForceUploadFailureParam, false};
  return kForceUploadFailure.Get();
}
