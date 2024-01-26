// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/app_store_rating/features.h"

#import "base/feature_list.h"
#import "ios/chrome/browser/promos_manager/features.h"

BASE_FEATURE(kAppStoreRating,
             "AppStoreRating",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAppStoreRatingDBExclusionJan2024,
             "AppStoreRatingDBExclusionJan2024",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAppStoreRatingEnabled() {
  return base::FeatureList::IsEnabled(kAppStoreRating);
}

bool IsDefaultBrowserConditionExclusionInEffect() {
  return base::FeatureList::IsEnabled(kAppStoreRatingDBExclusionJan2024);
}

const std::vector<std::string>
GetCountriesExcludedFromDefaultBrowserCondition() {
  if (!IsDefaultBrowserConditionExclusionInEffect()) {
    return {};
  }
  return {
      "at", "be", "bg", "cy", "cz", "de", "dk", "ee", "es", "fi",
      "fr", "gr", "hr", "hu", "ie", "is", "it", "lt", "lu", "lv",
      "mt", "nl", "no", "pl", "pt", "ro", "se", "si", "sk",
  };
}
