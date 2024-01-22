// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_APP_STORE_RATING_FEATURES_H_
#define IOS_CHROME_BROWSER_UI_APP_STORE_RATING_FEATURES_H_

#include <vector>

#import "base/feature_list.h"

// Feature controlling the App Store rating prompt.
BASE_DECLARE_FEATURE(kAppStoreRating);

// Feature controlling whether the default browser condition is used when
// determining eligibility for the App Store rating in specific countries.
BASE_DECLARE_FEATURE(kAppStoreRatingDBExclusionJan2024);

// Returns true if the App Store rating feature is enabled.
bool IsAppStoreRatingEnabled();

// Returns true if the App Store default browser country exclusion feature is
// enabled.
bool IsDefaultBrowserConditionExclusionInEffect();

// Returns the list of countries for which the default browser condition
// exclusion applies. Returns an empty vector if the exclusion feature is not
// enabled.
const std::vector<std::string>
GetCountriesExcludedFromDefaultBrowserCondition();

#endif  // IOS_CHROME_BROWSER_UI_APP_STORE_RATING_FEATURES_H_
