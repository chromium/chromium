// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_NAVIGATION_REFERRER_H_
#define IOS_WEB_PUBLIC_NAVIGATION_REFERRER_H_

#include <ostream>

#include "url/gurl.h"

namespace web {

enum ReferrerPolicy {
  ReferrerPolicyAlways,
  ReferrerPolicyDefault,
  ReferrerPolicyNoReferrerWhenDowngrade,
  ReferrerPolicyNever,
  ReferrerPolicyOrigin,
  ReferrerPolicyOriginWhenCrossOrigin,
  ReferrerPolicySameOrigin,
  ReferrerPolicyStrictOrigin,
  ReferrerPolicyStrictOriginWhenCrossOrigin,
  ReferrerPolicyLast = ReferrerPolicyStrictOriginWhenCrossOrigin
};

// For logging use only.
std::ostream& operator<<(std::ostream& stream, ReferrerPolicy policy);

// This struct holds a referrer URL, as well as the referrer policy to be
// applied to this URL. When passing around referrers that will eventually end
// up being used for URL requests, always use this struct.
struct Referrer {
  Referrer(const GURL& url, ReferrerPolicy policy) : url(url), policy(policy) {}
  Referrer() : policy(ReferrerPolicyDefault) {}

  GURL url;
  ReferrerPolicy policy;
};

// Comparison operators for Referrer.
bool operator==(const Referrer& lhs, const Referrer& rhs);
bool operator!=(const Referrer& lhs, const Referrer& rhs);

// For logging use only.
std::ostream& operator<<(std::ostream& stream, const Referrer& referrer);

}  // namespace web

#endif  // IOS_WEB_PUBLIC_NAVIGATION_REFERRER_H_
