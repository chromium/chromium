// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_PRIVATE_NETWORK_ACCESS_CHECK_RESULT_H_
#define SERVICES_NETWORK_PUBLIC_CPP_PRIVATE_NETWORK_ACCESS_CHECK_RESULT_H_

#include "base/component_export.h"

namespace network {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PrivateNetworkAccessCheckResult {
  // Request is allowed because it is missing a client security state.
  kAllowedMissingClientSecurityState = 0,

  // Not a private network request: the resource address space is no less
  // public than the client's.
  kAllowedNoLessPublic = 1,

  // Private network request: allowed because policy is `kAllow`.
  kAllowedByPolicyAllow = 2,

  // Private network request: allowed because policy is `kWarn`.
  kAllowedByPolicyWarn = 3,

  // URL loader options include `kURLLoadOptionBlockLocalRequest` and the
  // resource address space is not `kPublic`.
  kBlockedByLoadOption = 4,

  // Private network request: blocked because policy is `kBlock`.
  kBlockedByPolicyBlock = 5,

  // Required for UMA histogram logging.
  kMaxValue = kBlockedByPolicyBlock,
};

// Returns whether `result` indicates the request should be allowed.
bool COMPONENT_EXPORT(NETWORK_CPP) PrivateNetworkAccessCheckResultIsAllowed(
    PrivateNetworkAccessCheckResult result);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_PRIVATE_NETWORK_ACCESS_CHECK_RESULT_H_
