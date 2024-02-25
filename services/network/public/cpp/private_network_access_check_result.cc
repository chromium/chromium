// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/private_network_access_check_result.h"

#include <ostream>

#include "services/network/public/mojom/cors.mojom-shared.h"

namespace network {

using mojom::CorsError;

using Result = PrivateNetworkAccessCheckResult;

std::string_view PrivateNetworkAccessCheckResultToStringPiece(Result result) {
  switch (result) {
    case Result::kAllowedMissingClientSecurityState:
      return "allowed-missing-client-security-state";
    case Result::kAllowedNoLessPublic:
      return "allowed-no-less-public";
    case Result::kAllowedByPolicyAllow:
      return "allowed-by-policy-allow";
    case Result::kAllowedByPolicyWarn:
      return "allowed-by-policy-warn";
    case Result::kAllowedByTargetIpAddressSpace:
      return "allowed-by-target-ip-address-space";
    case Result::kBlockedByLoadOption:
      return "blocked-by-load-option";
    case Result::kBlockedByPolicyBlock:
      return "insecure-private-network";
    case Result::kBlockedByTargetIpAddressSpace:
      return "blocked-by-target-ip-address-space";
    case Result::kBlockedByPolicyPreflightWarn:
      return "blocked-by-policy-preflight-warn";
    case Result::kBlockedByPolicyPreflightBlock:
      return "blocked-by-policy-preflight-block";
    case Result::kAllowedByPolicyPreflightWarn:
      return "allowed-by-policy-preflight-warn";
    case Result::kBlockedByInconsistentIpAddressSpace:
      return "blocked-by-inconsistent-ip-address-space";
    case Result::kAllowedPotentiallyTrustworthySameOrigin:
      return "allowed-potentially-trustworthy-same-origin";
  }
}

std::ostream& operator<<(std::ostream& out,
                         PrivateNetworkAccessCheckResult result) {
  return out << PrivateNetworkAccessCheckResultToStringPiece(result);
}

std::optional<CorsError> PrivateNetworkAccessCheckResultToCorsError(
    Result result) {
  switch (result) {
    case Result::kAllowedMissingClientSecurityState:
    case Result::kAllowedNoLessPublic:
    case Result::kAllowedByPolicyAllow:
    case Result::kAllowedByPolicyWarn:
    case Result::kAllowedByTargetIpAddressSpace:
    case Result::kAllowedByPolicyPreflightWarn:
    case Result::kAllowedPotentiallyTrustworthySameOrigin:
      return std::nullopt;
    case Result::kBlockedByLoadOption:
      // TODO(https:/crbug.com/1254689): Return better error than this, which
      // does not fit.
    case Result::kBlockedByPolicyBlock:
      return CorsError::kInsecurePrivateNetwork;
    case Result::kBlockedByTargetIpAddressSpace:
    case Result::kBlockedByInconsistentIpAddressSpace:
      return CorsError::kInvalidPrivateNetworkAccess;
    case Result::kBlockedByPolicyPreflightWarn:
    case Result::kBlockedByPolicyPreflightBlock:
      return CorsError::kUnexpectedPrivateNetworkAccess;
  }
}

}  // namespace network
