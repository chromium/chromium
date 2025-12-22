// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/public/features.h"

#import "base/metrics/field_trial_params.h"
#import "base/time/time.h"

BASE_FEATURE(kComposeboxDevTools, base::FEATURE_DISABLED_BY_DEFAULT);

const char kImageLoadDelayMsParam[] = "image_load_delay_ms";
const char kUploadDelayMsParam[] = "upload_delay_ms";
const char kForceUploadFailureParam[] = "force_upload_failure";

base::TimeDelta GetImageLoadDelay() {
  if (!base::FeatureList::IsEnabled(kComposeboxDevTools)) {
    return base::TimeDelta();
  }
  static const base::FeatureParam<int> kImageLoadDelayMs{
      &kComposeboxDevTools, kImageLoadDelayMsParam, 0};
  return base::Milliseconds(kImageLoadDelayMs.Get());
}

base::TimeDelta GetUploadDelay() {
  if (!base::FeatureList::IsEnabled(kComposeboxDevTools)) {
    return base::TimeDelta();
  }
  static const base::FeatureParam<int> kUploadDelayMs{&kComposeboxDevTools,
                                                      kUploadDelayMsParam, 0};
  return base::Milliseconds(kUploadDelayMs.Get());
}

bool ShouldForceUploadFailure() {
  if (!base::FeatureList::IsEnabled(kComposeboxDevTools)) {
    return false;
  }
  static const base::FeatureParam<bool> kForceUploadFailure{
      &kComposeboxDevTools, kForceUploadFailureParam, false};
  return kForceUploadFailure.Get();
}

BASE_FEATURE(kComposeboxCloseButtonTopAlign, base::FEATURE_DISABLED_BY_DEFAULT);

bool AlignComposeboxCloseButtonToInputPlateTop() {
  return base::FeatureList::IsEnabled(kComposeboxCloseButtonTopAlign);
}

BASE_FEATURE(kComposeboxCompactMode, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsComposeboxCompactModeEnabled() {
  return base::FeatureList::IsEnabled(kComposeboxCompactMode);
}

BASE_FEATURE(kComposeboxForceTop, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsComposeboxForceTopEnabled() {
  return base::FeatureList::IsEnabled(kComposeboxForceTop);
}

BASE_FEATURE(kComposeboxAIMNudge, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsComposeboxAIMNudgeEnabled() {
  // Reminder to also check AIM availability.
  return base::FeatureList::IsEnabled(kComposeboxAIMNudge);
}

BASE_FEATURE(kComposeboxMenuTitle, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsComposeboxMenuTitleEnabled() {
  return base::FeatureList::IsEnabled(kComposeboxMenuTitle);
}

BASE_FEATURE(kComposeboxFetchContextualSuggestionsForImage,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsComposeboxFetchContextualSuggestionsForImageEnabled() {
  return base::FeatureList::IsEnabled(
      kComposeboxFetchContextualSuggestionsForImage);
}

BASE_FEATURE(kComposeboxFetchContextualSuggestionsForMultipleAttachments,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsComposeboxFetchContextualSuggestionsForMultiAttachmentsEnabled() {
  return base::FeatureList::IsEnabled(
      kComposeboxFetchContextualSuggestionsForMultipleAttachments);
}
