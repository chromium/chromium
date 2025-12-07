// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PRICE_INSIGHTS_MODEL_PRICE_INSIGHTS_FEATURE_H_
#define IOS_CHROME_BROWSER_PRICE_INSIGHTS_MODEL_PRICE_INSIGHTS_FEATURE_H_

#import <string>

class ProfileIOS;

// Determine if the price insights and price tracking are enabled.
bool IsPriceInsightsEnabled(ProfileIOS* profile);

// Determine if Price Insights on iOS is enabled for the current region.
bool IsPriceInsightsRegionEnabled();

#endif  // IOS_CHROME_BROWSER_PRICE_INSIGHTS_MODEL_PRICE_INSIGHTS_FEATURE_H_
