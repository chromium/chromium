// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_FENCED_FRAME_PERMISSIONS_POLICIES_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_FENCED_FRAME_PERMISSIONS_POLICIES_H_

namespace blink {

// In fenced frame trees, these permission policies are expected to be enabled.
// If any feature is disallowed for the fenced frame's origin, then the fenced
// frame will not be allowed to navigate. This is a medium-term solution that
// will be replaced by a system where consumer APIs (like FLEDGE) can select
// which features to require in order to navigate a fenced frame successfully.
// If a fenced frame navigates, each of these features will be allowed as if
// its policy was set to "allow: feature(*)".
constexpr blink::mojom::PermissionsPolicyFeature
    kFencedFrameOpaqueAdsDefaultAllowedFeatures[] = {
        blink::mojom::PermissionsPolicyFeature::kAttributionReporting,
        blink::mojom::PermissionsPolicyFeature::kSharedStorage};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_FENCED_FRAME_PERMISSIONS_POLICIES_H_
