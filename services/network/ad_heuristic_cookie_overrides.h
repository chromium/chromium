// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_AD_HEURISTIC_COOKIE_OVERRIDES_H_
#define SERVICES_NETWORK_AD_HEURISTIC_COOKIE_OVERRIDES_H_

#include "net/cookies/cookie_setting_override.h"

namespace network {

// Adds cookie setting overrides for cookie accesses determined to be for
// advertising purposes.
void AddAdsHeuristicCookieSettingOverrides(
    bool is_ad_tagged,
    net::CookieSettingOverrides& overrides);
}  // namespace network

#endif  // SERVICES_NETWORK_AD_HEURISTIC_COOKIE_OVERRIDES_H_
