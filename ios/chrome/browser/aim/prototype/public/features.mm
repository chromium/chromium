// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/prototype/public/features.h"

#import "base/metrics/field_trial_params.h"
#import "base/time/time.h"

BASE_FEATURE(kAimPrototypeDevTools, base::FEATURE_DISABLED_BY_DEFAULT);

const char kImageLoadDelayMsParam[] = "image_load_delay_ms";
const char kUploadDelayMsParam[] = "upload_delay_ms";
const char kForceUploadFailureParam[] = "force_upload_failure";

base::TimeDelta GetImageLoadDelay() {
  if (!base::FeatureList::IsEnabled(kAimPrototypeDevTools)) {
    return base::TimeDelta();
  }
  static const base::FeatureParam<int> kImageLoadDelayMs{
      &kAimPrototypeDevTools, kImageLoadDelayMsParam, 0};
  return base::Milliseconds(kImageLoadDelayMs.Get());
}

base::TimeDelta GetUploadDelay() {
  if (!base::FeatureList::IsEnabled(kAimPrototypeDevTools)) {
    return base::TimeDelta();
  }
  static const base::FeatureParam<int> kUploadDelayMs{&kAimPrototypeDevTools,
                                                      kUploadDelayMsParam, 0};
  return base::Milliseconds(kUploadDelayMs.Get());
}

bool ShouldForceUploadFailure() {
  if (!base::FeatureList::IsEnabled(kAimPrototypeDevTools)) {
    return false;
  }
  static const base::FeatureParam<bool> kForceUploadFailure{
      &kAimPrototypeDevTools, kForceUploadFailureParam, false};
  return kForceUploadFailure.Get();
}
