// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_FENCED_FRAME_PERMISSIONS_POLICIES_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_FENCED_FRAME_PERMISSIONS_POLICIES_H_

namespace blink {

// In fenced frame trees loaded with either Shared Storage or FLEDGE, these
// permission policies are expected to be enabled. If any feature is disallowed
// for the fenced frame's origin, then the fenced frame will not be allowed to
// navigate. If a fenced frame navigates, each of these features will be allowed
// as if its policy was set to "allow: feature(*)".
constexpr blink::mojom::PermissionsPolicyFeature
    kFencedFrameFledgeDefaultRequiredFeatures[] = {
        blink::mojom::PermissionsPolicyFeature::kAttributionReporting,
        blink::mojom::PermissionsPolicyFeature::kPrivateAggregation,
        blink::mojom::PermissionsPolicyFeature::kSharedStorage,
        blink::mojom::PermissionsPolicyFeature::kSharedStorageSelectUrl};

constexpr blink::mojom::PermissionsPolicyFeature
    kFencedFrameSharedStorageDefaultRequiredFeatures[] = {
        blink::mojom::PermissionsPolicyFeature::kAttributionReporting,
        blink::mojom::PermissionsPolicyFeature::kSharedStorage,
        blink::mojom::PermissionsPolicyFeature::kSharedStorageSelectUrl};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_FENCED_FRAME_PERMISSIONS_POLICIES_H_
