// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_COMMON_PERMISSIONS_POLICY_PERMISSIONS_POLICY_FEATURES_INTERNAL_H_
#define THIRD_PARTY_BLINK_COMMON_PERMISSIONS_POLICY_PERMISSIONS_POLICY_FEATURES_INTERNAL_H_

#include <unordered_set>
#include "third_party/blink/public/common/common_export.h"
#include "url/origin.h"

namespace blink {

// Constructs a set of hosts names from the `kDeprecateUnloadAllowlist`
// parameter.
// Exported for testing.
BLINK_COMMON_EXPORT const std::unordered_set<std::string>
UnloadDeprecationAllowedHosts();

// Returns `true` if `hosts` is empty or contains `host`.
// Exported for testing.
BLINK_COMMON_EXPORT bool UnloadDeprecationAllowedForHost(
    const std::string& host,
    const std::unordered_set<std::string>& hosts);

// Checks `origin` against all criteria:
// - the hosts listed in `kDeprecateUnloadAllowlist`
// - the gradual rollout percentage
// If `origin` is an opaque origin, the precursor origin is used.
BLINK_COMMON_EXPORT bool UnloadDeprecationAllowedForOrigin(
    const url::Origin& origin);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_COMMON_PERMISSIONS_POLICY_PERMISSIONS_POLICY_FEATURES_INTERNAL_H_
