// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_store_rating/ui_bundled/features.h"

#import "base/feature_list.h"
#import "ios/chrome/browser/promos_manager/model/features.h"

BASE_FEATURE(kAppStoreRating,
             "AppStoreRating",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsAppStoreRatingEnabled() {
  return base::FeatureList::IsEnabled(kAppStoreRating);
}

const std::vector<std::string>
GetCountriesExcludedFromDefaultBrowserCondition() {
  return {
      "at", "be", "bg", "cy", "cz", "de", "dk", "ee", "es", "fi",
      "fr", "gr", "hr", "hu", "ie", "is", "it", "lt", "lu", "lv",
      "mt", "nl", "no", "pl", "pt", "ro", "se", "si", "sk",
  };
}
