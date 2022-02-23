// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMMERCE_PRICE_ALERT_UTIL_H_
#define IOS_CHROME_BROWSER_COMMERCE_PRICE_ALERT_UTIL_H_

#import <Foundation/Foundation.h>

namespace web {
class BrowserState;
}  // namespace web

// Price alerts should not be available for all users - only
// MSBB and signed in users with a non-incognito Tab.
BOOL IsPriceAlertsEligible(web::BrowserState* browser_state);

// Returns true if the flag controlling price alerts is enabled.
BOOL IsPriceAlertsEnabled();

// Returns true if the flag controlling price alerts is enabled and
// the user opt out is enabled. This enables us to experiment with
// different experiences.
BOOL IsPriceAlertsWithOptOutEnabled();

#endif  // IOS_CHROME_BROWSER_COMMERCE_PRICE_ALERT_UTIL_H_
