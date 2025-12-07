// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_REFERRER_POLICY_H_
#define NET_URL_REQUEST_REFERRER_POLICY_H_

#include <optional>
#include <string_view>

#include "net/base/net_export.h"

namespace net {

// A ReferrerPolicy controls the contents of the Referer header when URLRequest
// following HTTP redirects. Note that setting a ReferrerPolicy on the request
// has no effect on the Referer header of the initial leg of the request; the
// caller is responsible for setting the initial Referer, and the ReferrerPolicy
// only controls what happens to the Referer while following redirects.
//
// NOTE: This enum is persisted to histograms. Do not change or reorder values.
// TODO(~M89): Once the Net.URLRequest.ReferrerPolicyForRequest metric is
// retired.
enum class ReferrerPolicy {
  // Clear the referrer header if the header value is HTTPS but the request
  // destination is HTTP. This is the default behavior of URLRequest.
  CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE = 0,
  // A slight variant on CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE:
  // If the request destination is HTTP, an HTTPS referrer will be cleared. If
  // the request's destination is cross-origin with the referrer (but does not
  // downgrade), the referrer's granularity will be stripped down to an origin
  // rather than a full URL. Same-origin requests will send the full referrer.
  REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN = 1,
  // Strip the referrer down to an origin when the origin of the referrer is
  // different from the destination's origin.
  ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN = 2,
  // Never change the referrer.
  NEVER_CLEAR = 3,
  // Strip the referrer down to the origin regardless of the redirect
  // location.
  ORIGIN = 4,
  // Clear the referrer when the request's referrer is cross-origin with
  // the request's destination.
  CLEAR_ON_TRANSITION_CROSS_ORIGIN = 5,
  // Strip the referrer down to the origin, but clear it entirely if the
  // referrer value is HTTPS and the destination is HTTP.
  ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE = 6,
  // Always clear the referrer regardless of the request destination.
  NO_REFERRER = 7,
  MAX = NO_REFERRER,
};

// Convert the last known-valid value of a pre-concatenated "Referrer-Policy"
// header to the corresponding ReferrerPolicy. For example, the input "origin,
// strict-origin" would result in output of
// ReferrerPolicy::ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE. If no
// recognized values were found then std::nullopt is returned.

// TODO(crbug.com/40217150): Consider updating
// blink::SecurityPolicy::ReferrerPolicyFromString() to use this.
NET_EXPORT std::optional<ReferrerPolicy> ReferrerPolicyFromHeader(
    std::string_view referrer_policy_header_value);

}  // namespace net

#endif  // NET_URL_REQUEST_REFERRER_POLICY_H_
