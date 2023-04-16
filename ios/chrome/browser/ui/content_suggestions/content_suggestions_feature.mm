// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"

#import "base/metrics/field_trial.h"
#import "base/metrics/field_trial_params.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/application_context/application_context.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Feature disabled by default to keep showing old Zine feed.
BASE_FEATURE(kDiscoverFeedInNtp,
             "DiscoverFeedInNtp",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Feature disabled by default.
BASE_FEATURE(kSingleNtp, "SingleNTP", base::FEATURE_ENABLED_BY_DEFAULT);

// Feature disabled by default.
BASE_FEATURE(kMagicStack, "MagicStack", base::FEATURE_DISABLED_BY_DEFAULT);

// A parameter to indicate whether the native UI is enabled for the discover
// feed.
const char kDiscoverFeedIsNativeUIEnabled[] = "DiscoverFeedIsNativeUIEnabled";

bool IsDiscoverFeedEnabled() {
  return base::FeatureList::IsEnabled(kDiscoverFeedInNtp);
}

bool IsMagicStackEnabled() {
  return base::FeatureList::IsEnabled(kMagicStack);
}
