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

// Returns `true` if `hosts` is empty or contains `origin`.
// Exported for testing.
BLINK_COMMON_EXPORT bool UnloadDeprecationAllowedForOrigin(
    const url::Origin& origin,
    const std::unordered_set<std::string>& hosts);

// Checks `origin` against the hosts listed in the `kDeprecateUnloadAllowlist`
// parameter.
bool UnloadDeprecationAllowedForOrigin(const url::Origin& origin);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_COMMON_PERMISSIONS_POLICY_PERMISSIONS_POLICY_FEATURES_INTERNAL_H_
