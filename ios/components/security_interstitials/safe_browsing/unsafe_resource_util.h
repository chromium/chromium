// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_SECURITY_INTERSTITIALS_SAFE_BROWSING_UNSAFE_RESOURCE_UTIL_H_
#define IOS_COMPONENTS_SECURITY_INTERSTITIALS_SAFE_BROWSING_UNSAFE_RESOURCE_UTIL_H_

#include <string>

#include "components/security_interstitials/core/base_safe_browsing_error_ui.h"
#include "components/security_interstitials/core/unsafe_resource.h"

class SafeBrowsingUrlAllowList;

// Runs `resource`'s callback on the appropriate thread.
void RunUnsafeResourceCallback(
    const security_interstitials::UnsafeResource& resource,
    bool proceed,
    bool showed_interstitial);

// Returns the interstitial reason for `resource`.
security_interstitials::BaseSafeBrowsingErrorUI::SBInterstitialReason
GetUnsafeResourceInterstitialReason(
    const security_interstitials::UnsafeResource& resource);

// Returns the metric prefix for error pages for `resource`.
std::string GetUnsafeResourceMetricPrefix(
    const security_interstitials::UnsafeResource& resource);

// Returns the SafeBrowsingUrlAllowList for `resource`.
SafeBrowsingUrlAllowList* GetAllowListForResource(
    const security_interstitials::UnsafeResource& resource);

// Retrieves the main frame URL for `resource` in `web_state`.
const GURL GetMainFrameUrl(
    const security_interstitials::UnsafeResource& resource);

#endif  // IOS_COMPONENTS_SECURITY_INTERSTITIALS_SAFE_BROWSING_UNSAFE_RESOURCE_UTIL_H_
