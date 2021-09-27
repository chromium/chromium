// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/private_network_access_check_result.h"

#include "services/network/public/mojom/cors.mojom-shared.h"

namespace network {

absl::optional<mojom::CorsError> PrivateNetworkAccessCheckResultToCorsError(
    PrivateNetworkAccessCheckResult result) {
  switch (result) {
    case PrivateNetworkAccessCheckResult::kAllowedMissingClientSecurityState:
    case PrivateNetworkAccessCheckResult::kAllowedNoLessPublic:
    case PrivateNetworkAccessCheckResult::kAllowedByPolicyAllow:
    case PrivateNetworkAccessCheckResult::kAllowedByPolicyWarn:
    case PrivateNetworkAccessCheckResult::kAllowedByTargetIpAddressSpace:
      return absl::nullopt;
    case PrivateNetworkAccessCheckResult::kBlockedByLoadOption:
    case PrivateNetworkAccessCheckResult::kBlockedByPolicyBlock:
      return mojom::CorsError::kInsecurePrivateNetwork;
    case PrivateNetworkAccessCheckResult::kBlockedByTargetIpAddressSpace:
      return mojom::CorsError::kInvalidPrivateNetworkAccess;
  }
}

}  // namespace network
