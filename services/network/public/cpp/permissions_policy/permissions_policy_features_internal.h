// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_PERMISSIONS_POLICY_PERMISSIONS_POLICY_FEATURES_INTERNAL_H_
#define SERVICES_NETWORK_PUBLIC_CPP_PERMISSIONS_POLICY_PERMISSIONS_POLICY_FEATURES_INTERNAL_H_

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "url/origin.h"

namespace network {

// Constructs a set of hosts names from the `kDeprecateUnloadAllowlist`
// parameter.
// Exported for testing.
COMPONENT_EXPORT(NETWORK_CPP_WEB_PLATFORM)
const base::flat_set<std::string> UnloadDeprecationAllowedHosts();

// Returns `true` if `hosts` is empty or contains `host`.
// Exported for testing.
COMPONENT_EXPORT(NETWORK_CPP_WEB_PLATFORM)
bool UnloadDeprecationAllowedForHost(const std::string& host,
                                     const base::flat_set<std::string>& hosts);

// Checks `origin` against all criteria enabled by features/params
// - the hosts listed in `kDeprecateUnloadAllowlist`
// - the gradual rollout percentage
// - the bucketing of origins
// If `origin` is an opaque origin, the precursor origin is used.
COMPONENT_EXPORT(NETWORK_CPP_WEB_PLATFORM)
bool UnloadDeprecationAllowedForOrigin(const url::Origin& origin);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_PERMISSIONS_POLICY_PERMISSIONS_POLICY_FEATURES_INTERNAL_H_
