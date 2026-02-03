// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_URL_BLOCKING_MODEL_POLICY_URL_BLOCKING_UTIL_H_
#define IOS_CHROME_BROWSER_POLICY_URL_BLOCKING_MODEL_POLICY_URL_BLOCKING_UTIL_H_

#import "components/policy/core/browser/url_list/policy_blocklist_service.h"

@class NSError;

namespace policy_url_blocking_util {

// Creates an error describing a navigation failure due to the request having
// been blocked by enterprise policy.
// Policy source is used to determine whether the URL was blocked by
// URLBlocklist or by Incognito-specific policy and return the appropriate error
// code.
NSError* CreateBlockedUrlError(
    PolicyBlocklistService::PolicyBlocklistState::PolicySource policy_source);

}  // namespace policy_url_blocking_util

#endif  // IOS_CHROME_BROWSER_POLICY_URL_BLOCKING_MODEL_POLICY_URL_BLOCKING_UTIL_H_
