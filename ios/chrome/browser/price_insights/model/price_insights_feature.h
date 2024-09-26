// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PRICE_INSIGHTS_MODEL_PRICE_INSIGHTS_FEATURE_H_
#define IOS_CHROME_BROWSER_PRICE_INSIGHTS_MODEL_PRICE_INSIGHTS_FEATURE_H_

#import <string>

#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

// Feature flag parameter in Price Insights to determine the text displayed for
// a low price.
extern const char kLowPriceParam[];

// Parameter value for kLowPriceParam indicating that the price is low.
extern const char kLowPriceParamPriceIsLow[];

// Parameter value for kLowPriceParam indicating that there is a good deal.
extern const char kLowPriceParamGoodDealNow[];

// Parameter value for kLowPriceParam indicating to see price history.
extern const char kLowPriceParamSeePriceHistory[];

// Determine if the price insights and price tracking are enabled.
bool IsPriceInsightsEnabled(ProfileIOS* profile);

// Determine if the price insights high price feature is enabled.
bool IsPriceInsightsHighPriceEnabled();

// Retrieves the flag parameter value that determines the message displayed for
// a low price.
std::string GetLowPriceParamValue();

#endif  // IOS_CHROME_BROWSER_PRICE_INSIGHTS_MODEL_PRICE_INSIGHTS_FEATURE_H_
