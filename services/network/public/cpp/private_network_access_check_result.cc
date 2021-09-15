// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/private_network_access_check_result.h"

namespace network {

bool PrivateNetworkAccessCheckResultIsAllowed(
    PrivateNetworkAccessCheckResult result) {
  switch (result) {
    case PrivateNetworkAccessCheckResult::kAllowedMissingClientSecurityState:
    case PrivateNetworkAccessCheckResult::kAllowedNoLessPublic:
    case PrivateNetworkAccessCheckResult::kAllowedByPolicyAllow:
    case PrivateNetworkAccessCheckResult::kAllowedByPolicyWarn:
      return true;
    case PrivateNetworkAccessCheckResult::kBlockedByLoadOption:
    case PrivateNetworkAccessCheckResult::kBlockedByPolicyBlock:
      return false;
  }
}

}  // namespace network
