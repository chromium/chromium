// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_TRUST_TOKEN_OPERATION_AUTHORIZATION_H_
#define SERVICES_NETWORK_PUBLIC_CPP_TRUST_TOKEN_OPERATION_AUTHORIZATION_H_

#include "services/network/public/mojom/trust_tokens.mojom-shared.h"

namespace network {

// A Trust Tokens operation requires the 'trust-token-redemption' Permissions
// Policy feature if it's of type "redemption" or "signing" (as opposed to
// "issuance").
//
// Needing the top-level frame's authorization to execute a redemption operation
// has two motivations: security and privacy.
//  - Security: Executing a redemption operation counts against rate limits,
//  currently indelible short of a user-initiated browsing data clear, that are
//  scoped to the redemption-time top frame origin. Using Permissions Policy
//  stops subframes from denying service to ancestor frames by exhausting these
//  rate limits.
//  - Privacy: The results of a redemption operation ("signed redemption
//  records") are persistent first-party identifiers in the context of the top
//  frame at redemption time. Since embedded third-party frames are on the other
//  side of a privacy boundary, we’d like to prohibit these third-party frames
//  from having access to the redeeming-context RRs without the top-level
//  frame's explicit consent.
constexpr bool DoesTrustTokenOperationRequirePermissionsPolicy(
    mojom::TrustTokenOperationType type) {
  switch (type) {
    case mojom::TrustTokenOperationType::kRedemption:
    case mojom::TrustTokenOperationType::kSigning:
      return true;
    case mojom::TrustTokenOperationType::kIssuance:
      return false;
  }
}

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_TRUST_TOKEN_OPERATION_AUTHORIZATION_H_
