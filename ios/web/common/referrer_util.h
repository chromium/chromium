// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_COMMON_REFERRER_UTIL_H_
#define IOS_WEB_COMMON_REFERRER_UTIL_H_

#include <string>

#include "ios/web/public/navigation/referrer.h"
#include "net/url_request/referrer_policy.h"

class GURL;

namespace web {

// Returns the string that should be sent as the Referer header value for
// navigating to `destination` from the given referrer, taking the referrer
// policy into account. Returns an empty string if no Referer should be sent.
std::string ReferrerHeaderValueForNavigation(const GURL& destination,
                                             const web::Referrer& referrer);

// Returns the policy that should be used to process subsequent forwards, if
// any.
// TODO(stuartmorgan): Replace this with ReferrerForNavigation, since it should
// always be used with ReferrerHeaderValueForNavigation anyway.
net::ReferrerPolicy PolicyForNavigation(const GURL& destination,
                                        const web::Referrer& referrer);

// Returns the WebReferrerPolicy corresponding to the given policy string
// (e.g., 'always', 'never', 'origin', 'default'). The string is assumed to
// be lowercase already. Unrecognized values will be treated as Default.
ReferrerPolicy ReferrerPolicyFromString(const std::string& policy);

}  // namespace web

#endif  // IOS_WEB_COMMON_REFERRER_UTIL_H_
