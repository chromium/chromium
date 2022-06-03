// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_banner/infobar_banner_features.h"

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
      15 /*default to 15 second*/);
}

double GetLongPresentationMessageDuration() {
  return base::GetFieldTrialParamByFeatureAsDouble(
      kEnableLongMessageDuration, kLongPresentationMessagesDurationFeatureParam,
      20 /*default to 20 second*/);
}
