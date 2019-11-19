// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_ORIGIN_POLICY_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_ORIGIN_POLICY_H_

#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"

namespace blink {

// Represents a blink-understandable origin policy. This struct needs to be kept
// in sync with the OriginPolicyContents struct in
// //services/network/public/cpp/origin_policy.h.

// Origin Policy spec: https://wicg.github.io/origin-policy/
struct BLINK_EXPORT WebOriginPolicy {
  // The feature policy that is dictated by the origin policy. Each feature
  // is one member of the array.
  // https://w3c.github.io/webappsec-feature-policy/
  WebVector<WebString> features;

  // These two fields together represent the CSP that should be applied to the
  // origin, based on the origin policy.
  // https://w3c.github.io/webappsec-csp/

  // The "enforced" portion of the CSP. This CSP is to be treated as having
  // an "enforced" disposition.
  // https://w3c.github.io/webappsec-csp/#policy-disposition
  WebVector<WebString> content_security_policies;

  // The "report-only" portion of the CSP. This CSP is to be treated as having
  // a "report" disposition.
  // https://w3c.github.io/webappsec-csp/#policy-disposition
  WebVector<WebString> content_security_policies_report_only;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_ORIGIN_POLICY_H_
