// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_POLICY_CONTAINER_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_POLICY_CONTAINER_UTILS_H_

#include "services/network/public/mojom/content_security_policy.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/frame/policy_container.mojom-blink-forward.h"
#include "third_party/blink/public/platform/web_content_security_policy_struct.h"
#include "third_party/blink/public/platform/web_policy_container.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// Converts a WebContentSecurityPolicy into a ContentSecurityPolicy. These two
// classes represent the exact same thing, but one is in public, the other is
// private.
PLATFORM_EXPORT
network::mojom::blink::ContentSecurityPolicyPtr FromWebContentSecurityPolicy(
    const WebContentSecurityPolicy& policy);

// Converts a WebPolicyContainerPolicies into a PolicyContainerPolicies. These
// two classes represent the exact same thing, but one is in public, the other
// is private.
PLATFORM_EXPORT mojom::blink::PolicyContainerPoliciesPtr
FromWebPolicyContainerPolicies(const WebPolicyContainerPolicies& policies);

// Converts a ContentSecurityPolicy into a WebContentSecurityPolicy. These two
// classes represent the exact same thing, but one is in public, the other is
// private.
PLATFORM_EXPORT WebContentSecurityPolicy ToWebContentSecurityPolicy(
    const network::mojom::blink::ContentSecurityPolicy& policy);

// Converts a PolicyContainerPolicies into a WebPolicyContainerPolicies. These
// two classes represent the exact same thing, but one is in public, the other
// is private.
PLATFORM_EXPORT WebPolicyContainerPolicies ToWebPolicyContainerPolicies(
    const mojom::blink::PolicyContainerPolicies& policies);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_POLICY_CONTAINER_UTILS_H_
