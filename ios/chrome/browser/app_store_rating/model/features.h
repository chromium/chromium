// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_STORE_RATING_MODEL_FEATURES_H_
#define IOS_CHROME_BROWSER_APP_STORE_RATING_MODEL_FEATURES_H_

#include <vector>

#import "base/feature_list.h"

// Feature controlling the App Store rating prompt. This base::Feature is needed
// even if it's already enabled by default to allow remotely disabling the
// prompt if needed.
BASE_DECLARE_FEATURE(kAppStoreRating);

// Returns true if the App Store rating feature is enabled.
bool IsAppStoreRatingEnabled();

// Returns the list of countries for which the default browser condition
// exclusion applies.
const std::vector<std::string>
GetCountriesExcludedFromDefaultBrowserCondition();

#endif  // IOS_CHROME_BROWSER_APP_STORE_RATING_MODEL_FEATURES_H_
