// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_PERMISSIONS_POLICY_FENCED_FRAME_PERMISSIONS_POLICIES_H_
#define SERVICES_NETWORK_PUBLIC_CPP_PERMISSIONS_POLICY_FENCED_FRAME_PERMISSIONS_POLICIES_H_

#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"

namespace network {

// In fenced frames loaded with a URL, only these permissions policies are
// allowed to be enabled or inherited. All other permissions policies will be
// turned off.
static inline constexpr network::mojom::PermissionsPolicyFeature
    kFencedFrameAllowedFeatures[] = {
        network::mojom::PermissionsPolicyFeature::
            kFencedUnpartitionedStorageRead,
        network::mojom::PermissionsPolicyFeature::kPrivateAggregation,
        network::mojom::PermissionsPolicyFeature::kSharedStorage,
        network::mojom::PermissionsPolicyFeature::kSharedStorageSelectUrl};

// In fenced frame trees loaded with either Shared Storage or Protected
// Audience, these permission policies are expected to be enabled. If any
// feature is disallowed for the fenced frame's origin, then the fenced frame
// will not be allowed to navigate. If a fenced frame navigates, each of these
// features will be allowed as if its policy was set to "allow: feature(*)".
static inline constexpr network::mojom::PermissionsPolicyFeature
    kFencedFrameFledgeDefaultRequiredFeatures[] = {
        network::mojom::PermissionsPolicyFeature::kAttributionReporting,
        network::mojom::PermissionsPolicyFeature::kPrivateAggregation,
        network::mojom::PermissionsPolicyFeature::kSharedStorage,
        network::mojom::PermissionsPolicyFeature::kSharedStorageSelectUrl};

static inline constexpr network::mojom::PermissionsPolicyFeature
    kFencedFrameSharedStorageDefaultRequiredFeatures[] = {
        network::mojom::PermissionsPolicyFeature::kAttributionReporting,
        network::mojom::PermissionsPolicyFeature::kSharedStorage,
        network::mojom::PermissionsPolicyFeature::kSharedStorageSelectUrl};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_PERMISSIONS_POLICY_FENCED_FRAME_PERMISSIONS_POLICIES_H_
