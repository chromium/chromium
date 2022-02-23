// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_banner/infobar_banner_features.h"

#include "base/metrics/field_trial_params.h"
#import "ios/chrome/browser/ui/infobars/infobar_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const base::Feature kEnableLongMessageDuration{
    "EnableLongMessageDuration", base::FEATURE_DISABLED_BY_DEFAULT};

const char kLongPresentationMessagesDurationFeatureParam[] =
    "LongPresentationMessagesDurationFeatureParam";

const char kDefaultPresentationMessagesDurationFeatureParam[] =
    "DefaultPresentationMessagesDurationFeatureParam";

bool IsLongMessageDurationEnabled() {
  return base::FeatureList::IsEnabled(kEnableLongMessageDuration);
}

double GetDefaultPresentationMessageDuration() {
  return base::GetFieldTrialParamByFeatureAsDouble(
      kEnableLongMessageDuration,
      kDefaultPresentationMessagesDurationFeatureParam,
      kInfobarBannerDefaultPresentationDurationInSeconds
      /*default to banner's default duration*/);
}

double GetLongPresentationMessageDuration() {
  return base::GetFieldTrialParamByFeatureAsDouble(
      kEnableLongMessageDuration, kLongPresentationMessagesDurationFeatureParam,
      kInfobarBannerLongPresentationDurationInSeconds
      /*default to banner's long duration*/);
}
