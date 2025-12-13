// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_PRIVATE_NETWORK_ACCESS_CHECK_RESULT_H_
#define SERVICES_NETWORK_PUBLIC_CPP_PRIVATE_NETWORK_ACCESS_CHECK_RESULT_H_

#include <iosfwd>
#include <optional>
#include <string_view>

#include "base/component_export.h"
#include "services/network/public/mojom/cors.mojom-forward.h"

namespace network {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// Please keep in sync with "PrivateNetworkAccessCheckResult" in
// src/tools/metrics/histograms/metadata/security/enums.xml.
//
// LINT.IfChange(PrivateNetworkAccessCheckResult)
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

  // Deleted
  //
  // kAllowedByTargetIpAddressSpace = 6,
  // kBlockedByTargetIpAddressSpace = 7,
  // kBlockedByPolicyPreflightWarn = 8,
  // kBlockedByPolicyPreflightBlock = 9,
  // kAllowedByPolicyPreflightWarn = 10,

  // Request connected to two different IP address spaces for the same response.
  kBlockedByInconsistentIpAddressSpace = 11,

  // Private network request: allowed because same origin.
  kAllowedPotentiallyTrustworthySameOrigin = 12,

  // Local network access request: blocked unless user grants permission.
  kLNAPermissionRequired = 13,

  // Local network access request: allowed with warning in devtools.
  kLNAAllowedByPolicyWarn = 14,

  // Request carries a `target_ip_address_space` that did not match the ip
  // address that served the request.
  kBlockedByRequiredIpAddressSpaceMismatch = 15,

  // Required for UMA histogram logging.
  kMaxValue = kBlockedByRequiredIpAddressSpaceMismatch,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/security/enums.xml:PrivateNetworkAccessCheckResult)

// Returns a human-readable string representing `result`, suitable for logging.
std::string_view COMPONENT_EXPORT(NETWORK_CPP)
    PrivateNetworkAccessCheckResultToStringPiece(
        PrivateNetworkAccessCheckResult result);

// Results are streamable for easier logging and debugging.
//
// `COMPONENT_EXPORT()` must come first to compile correctly on Windows.
COMPONENT_EXPORT(NETWORK_CPP)
std::ostream& operator<<(std::ostream& out,
                         PrivateNetworkAccessCheckResult result);

// If `result` indicates that the request should be blocked, returns the
// corresponding `CorsError` enum value. Otherwise returns `nullopt`.
std::optional<mojom::CorsError> COMPONENT_EXPORT(NETWORK_CPP)
    PrivateNetworkAccessCheckResultToCorsError(
        PrivateNetworkAccessCheckResult result);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_PRIVATE_NETWORK_ACCESS_CHECK_RESULT_H_
