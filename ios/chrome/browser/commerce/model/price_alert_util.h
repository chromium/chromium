// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMMERCE_MODEL_PRICE_ALERT_UTIL_H_
#define IOS_CHROME_BROWSER_COMMERCE_MODEL_PRICE_ALERT_UTIL_H_

class ProfileIOS;
namespace web {
class WebState;
}  // namespace web

// Price alerts should not be available for all users - only
// MSBB and signed in users with a non-incognito Tab.
bool IsPriceAlertsEligible(ProfileIOS* profile);

// Helper that calls IsPriceAlertsEligible(ProfileIOS*) with
// profile owning `web_state`.
bool IsPriceAlertsEligibleForWebState(web::WebState* web_state);

#endif  // IOS_CHROME_BROWSER_COMMERCE_MODEL_PRICE_ALERT_UTIL_H_
