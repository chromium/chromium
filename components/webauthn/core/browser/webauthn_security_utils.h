// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_WEBAUTHN_SECURITY_UTILS_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_WEBAUTHN_SECURITY_UTILS_H_

#include <optional>
#include <string>

#include "url/gurl.h"
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
// authorized to make WebAuthn requests (i.e., it is an https origin or
// localhost), and an error otherwise.
ValidationStatus OriginAllowedToMakeWebAuthnRequests(url::Origin caller_origin);

// Returns whether a caller origin is allowed to claim a given Relying Party ID.
//
// This method returns false if the caller origin that isn't authorized to make
// WebAuthn requests (i.e., not an https origin or localhost).
bool OriginIsAllowedToClaimRelyingPartyId(
    const std::string& claimed_relying_party_id,
    const url::Origin& caller_origin);

// Returns the URL used for remote validation.
//
// This method returns a nullopt if the remote validation URL resembles an IP
// address.
std::optional<GURL> GetRemoteValidationUrl(const std::string& relying_party_id);

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_WEBAUTHN_SECURITY_UTILS_H_
