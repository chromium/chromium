// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMMERCE_MODEL_PRICE_ALERT_UTIL_H_
#define IOS_CHROME_BROWSER_COMMERCE_MODEL_PRICE_ALERT_UTIL_H_

#import <Foundation/Foundation.h>

namespace web {
class BrowserState;
}  // namespace web

// Price alerts should not be available for all users - only
// MSBB and signed in users with a non-incognito Tab.
bool IsPriceAlertsEligible(web::BrowserState* browser_state);

#endif  // IOS_CHROME_BROWSER_COMMERCE_MODEL_PRICE_ALERT_UTIL_H_
