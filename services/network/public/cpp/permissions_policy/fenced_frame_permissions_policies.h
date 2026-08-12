// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_PERMISSIONS_POLICY_FENCED_FRAME_PERMISSIONS_POLICIES_H_
#define SERVICES_NETWORK_PUBLIC_CPP_PERMISSIONS_POLICY_FENCED_FRAME_PERMISSIONS_POLICIES_H_

#include "base/containers/span.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"

namespace network {

// In fenced frames loaded with a URL, only these permissions policies are
// allowed to be enabled or inherited. All other permissions policies will be
// turned off.
static inline constexpr base::span<
    const network::mojom::PermissionsPolicyFeature>
    kFencedFrameAllowedFeatures;

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_PERMISSIONS_POLICY_FENCED_FRAME_PERMISSIONS_POLICIES_H_
