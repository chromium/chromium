// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PRICE_INSIGHTS_MODEL_PRICE_INSIGHTS_FEATURE_H_
#define IOS_CHROME_BROWSER_PRICE_INSIGHTS_MODEL_PRICE_INSIGHTS_FEATURE_H_

class ChromeBrowserState;

// Determine if the price insights and price tracking are enabled.
bool IsPriceInsightsEnabled(ChromeBrowserState* browser_state);

#endif  // IOS_CHROME_BROWSER_PRICE_INSIGHTS_MODEL_PRICE_INSIGHTS_FEATURE_H_
