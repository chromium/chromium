// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/app_store_rating/features.h"

#import "base/feature_list.h"
#import "ios/chrome/browser/promos_manager/features.h"

BASE_FEATURE(kAppStoreRating,
             "AppStoreRating",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAppStoreRatingLoosenedTriggers,
             "AppStoreRatingLoosenedTriggers",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAppStoreRatingEnabled() {
  return base::FeatureList::IsEnabled(kAppStoreRating);
}

bool IsAppStoreRatingLoosenedTriggersEnabled() {
  return base::FeatureList::IsEnabled(kAppStoreRatingLoosenedTriggers);
}
