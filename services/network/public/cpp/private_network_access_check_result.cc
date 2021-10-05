// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/private_network_access_check_result.h"

#include "services/network/public/mojom/cors.mojom-shared.h"

namespace network {

using mojom::CorsError;

using Result = PrivateNetworkAccessCheckResult;

absl::optional<CorsError> PrivateNetworkAccessCheckResultToCorsError(
    Result result) {
  switch (result) {
    case Result::kAllowedMissingClientSecurityState:
    case Result::kAllowedNoLessPublic:
    case Result::kAllowedByPolicyAllow:
    case Result::kAllowedByPolicyWarn:
    case Result::kAllowedByTargetIpAddressSpace:
      return absl::nullopt;
    case Result::kBlockedByLoadOption:
      // TODO(https:/crbug.com/1254689): Return better error than this, which
      // does not fit.
    case Result::kBlockedByPolicyBlock:
      return CorsError::kInsecurePrivateNetwork;
    case Result::kBlockedByTargetIpAddressSpace:
      return CorsError::kInvalidPrivateNetworkAccess;
    case Result::kBlockedByPolicyPreflightWarn:
    case Result::kBlockedByPolicyPreflightBlock:
      return CorsError::kUnexpectedPrivateNetworkAccess;
  }
}

}  // namespace network
