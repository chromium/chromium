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
    case Result::kBlockedByLoadOption:
      return "blocked-by-load-option";
    case Result::kBlockedByPolicyBlock:
      return "insecure-private-network";
    case Result::kBlockedByInconsistentIpAddressSpace:
      return "blocked-by-inconsistent-ip-address-space";
    case Result::kAllowedPotentiallyTrustworthySameOrigin:
      return "allowed-potentially-trustworthy-same-origin";
    case Result::kLNAPermissionRequired:
      return "lna-permission-required";
    case Result::kLNAAllowedByPolicyWarn:
      return "lna-allowed-by-policy-warn";
    case Result::kBlockedByRequiredIpAddressSpaceMismatch:
      return "blocked-by-required-ip-address-space-mismatch";
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
    case Result::kAllowedPotentiallyTrustworthySameOrigin:
    case Result::kLNAAllowedByPolicyWarn:
      return std::nullopt;
    case Result::kBlockedByLoadOption:
      // TODO(https:/crbug.com/1254689): Return better error than this, which
      // does not fit.
    case Result::kBlockedByPolicyBlock:
      return CorsError::kInsecurePrivateNetwork;
    case Result::kBlockedByInconsistentIpAddressSpace:
    case Result::kBlockedByRequiredIpAddressSpaceMismatch:
      return CorsError::kInvalidPrivateNetworkAccess;
    case Result::kLNAPermissionRequired:
      return CorsError::kLocalNetworkAccessPermissionDenied;
  }
}

}  // namespace network
