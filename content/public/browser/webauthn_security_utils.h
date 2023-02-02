// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEBAUTHN_SECURITY_UTILS_H_
#define CONTENT_PUBLIC_BROWSER_WEBAUTHN_SECURITY_UTILS_H_

#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"
#include "url/origin.h"

namespace content {

// Returns AuthenticatorStatus::SUCCESS if the caller origin is in principle
// authorized to make WebAuthn requests, and an error if it fails some criteria,
// e.g. an insecure protocol or domain.
//
// Reference https://url.spec.whatwg.org/#valid-domain-string and
// https://html.spec.whatwg.org/multipage/origin.html#concept-origin-effective-domain.
CONTENT_EXPORT blink::mojom::AuthenticatorStatus
OriginAllowedToMakeWebAuthnRequests(url::Origin caller_origin);

// Returns whether a caller origin is allowed to claim a given Relying Party ID.
// It's valid for the requested RP ID to be a registrable domain suffix of, or
// be equal to, the origin's effective domain.  Reference:
// https://html.spec.whatwg.org/multipage/origin.html#is-a-registrable-domain-suffix-of-or-is-equal-to.
CONTENT_EXPORT bool OriginIsAllowedToClaimRelyingPartyId(
    const std::string& claimed_relying_party_id,
    const url::Origin& caller_origin);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEBAUTHN_SECURITY_UTILS_H_
