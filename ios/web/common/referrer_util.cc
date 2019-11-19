// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/common/referrer_util.h"

#include "base/logging.h"
#include "ios/web/public/navigation/referrer.h"
#include "url/gurl.h"

namespace web {

std::string ReferrerHeaderValueForNavigation(const GURL& destination,
                                             const web::Referrer& referrer) {
  bool is_downgrade = referrer.url.SchemeIsCryptographic() &&
                      !destination.SchemeIsCryptographic();
  switch (referrer.policy) {
    case ReferrerPolicyAlways:
      return referrer.url.GetAsReferrer().spec();
    case ReferrerPolicyNever:
      return std::string();
    case ReferrerPolicyOrigin:
      return referrer.url.GetOrigin().spec();
    case ReferrerPolicyDefault:
    case ReferrerPolicyNoReferrerWhenDowngrade:
      if (is_downgrade)
        return std::string();
      return referrer.url.GetAsReferrer().spec();
    case ReferrerPolicyOriginWhenCrossOrigin:
      if (referrer.url.GetOrigin() != destination.GetOrigin())
        return referrer.url.GetOrigin().spec();
      return referrer.url.GetAsReferrer().spec();
    case ReferrerPolicySameOrigin:
      if (referrer.url.GetOrigin() != destination.GetOrigin())
        return std::string();
      return referrer.url.GetAsReferrer().spec();
    case ReferrerPolicyStrictOrigin:
      if (is_downgrade)
        return std::string();
      return referrer.url.GetOrigin().spec();
    case ReferrerPolicyStrictOriginWhenCrossOrigin:
      if (is_downgrade)
        return std::string();
      if (referrer.url.GetOrigin() != destination.GetOrigin())
        return referrer.url.GetOrigin().spec();
      return referrer.url.GetAsReferrer().spec();
  }
  NOTREACHED();
  return std::string();
}

net::URLRequest::ReferrerPolicy PolicyForNavigation(
    const GURL& destination,
    const web::Referrer& referrer) {
  // Based on the matching logic in content's
  // resource_dispatcher_host_impl.cc
  switch (referrer.policy) {
    case ReferrerPolicyAlways:
      return net::URLRequest::NEVER_CLEAR_REFERRER;
    case ReferrerPolicyNever:
      return net::URLRequest::NO_REFERRER;
    case ReferrerPolicyOrigin:
      return net::URLRequest::ORIGIN;
    case ReferrerPolicyNoReferrerWhenDowngrade:
    case ReferrerPolicyDefault:
      return net::URLRequest::
          CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
    case ReferrerPolicyOriginWhenCrossOrigin:
      return net::URLRequest::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN;
    case ReferrerPolicySameOrigin:
      return net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_CROSS_ORIGIN;
    case ReferrerPolicyStrictOrigin:
      return net::URLRequest::
          ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
    case ReferrerPolicyStrictOriginWhenCrossOrigin:
      return net::URLRequest::
          REDUCE_REFERRER_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN;
  }
  NOTREACHED();
  return net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
}

ReferrerPolicy ReferrerPolicyFromString(const std::string& policy) {
  // https://w3c.github.io/webappsec-referrer-policy/#determine-policy-for-token
  if (policy == "never" || policy == "no-referrer")
    return ReferrerPolicyNever;
  if (policy == "origin")
    return ReferrerPolicyOrigin;
  if (policy == "default" || policy == "no-referrer-when-downgrade")
    return ReferrerPolicyNoReferrerWhenDowngrade;
  if (policy == "origin-when-cross-origin")
    return ReferrerPolicyOriginWhenCrossOrigin;
  if (policy == "always" || policy == "unsafe-url")
    return ReferrerPolicyAlways;
  if (policy == "same-origin")
    return ReferrerPolicySameOrigin;
  if (policy == "strict-origin")
    return ReferrerPolicyStrictOrigin;
  if (policy == "strict-origin-when-cross-origin")
    return ReferrerPolicyStrictOriginWhenCrossOrigin;
  return ReferrerPolicyDefault;
}

}  // namespace web
