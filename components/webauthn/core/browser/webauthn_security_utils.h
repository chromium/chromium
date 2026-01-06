// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_WEBAUTHN_SECURITY_UTILS_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_WEBAUTHN_SECURITY_UTILS_H_
#include <string>

#include "url/origin.h"

namespace webauthn {

enum class ValidationStatus {
  kSuccess,
  kOpaqueDomain,
  kInvalidProtocol,
  kInvalidDomain,
  kBadRelyingPartyId,
  kJsonParseError,
  kNoJsonMatchHitLimits,
  kNoJsonMatch,
  kAttemptedFetch,
  kWrongContentType,
};

// Returns ValidationStatus::kSuccess if the caller origin is in principle
// authorized to make WebAuthn requests, and an error if it fails some criteria,
// e.g. an insecure protocol or domain.
//
// Reference https://url.spec.whatwg.org/#valid-domain-string and
// https://html.spec.whatwg.org/multipage/origin.html#concept-origin-effective-domain.
ValidationStatus OriginAllowedToMakeWebAuthnRequests(url::Origin caller_origin);

// Returns whether a caller origin is allowed to claim a given Relying Party ID.
// It's valid for the requested RP ID to be a registrable domain suffix of, or
// be equal to, the origin's effective domain.  Reference:
// https://html.spec.whatwg.org/multipage/origin.html#is-a-registrable-domain-suffix-of-or-is-equal-to.
bool OriginIsAllowedToClaimRelyingPartyId(
    const std::string& claimed_relying_party_id,
    const url::Origin& caller_origin);

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_WEBAUTHN_SECURITY_UTILS_H_
