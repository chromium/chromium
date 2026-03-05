// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/policy_container_util.h"

#include <utility>

#include "base/containers/to_vector.h"
#include "content/renderer/content_security_policy_util.h"
#include "third_party/blink/public/mojom/frame/policy_container.mojom.h"

namespace content {

std::unique_ptr<blink::WebPolicyContainer> ToWebPolicyContainer(
    blink::mojom::PolicyContainerPtr in) {
  if (!in) {
    return nullptr;
  }

  return std::make_unique<blink::WebPolicyContainer>(
      ToWebPolicyContainerPolicies(in->policies), std::move(in->remote));
}

blink::WebPolicyContainerPolicies ToWebPolicyContainerPolicies(
    const blink::mojom::PolicyContainerPoliciesPtr& policies) {
  return blink::WebPolicyContainerPolicies{
      policies->connection_allowlists,
      policies->cross_origin_embedder_policy.value,
      policies->integrity_policy,
      policies->integrity_policy_report_only,
      policies->referrer_policy,
      ToWebContentSecurityPolicies(policies->content_security_policies),
      policies->is_credentialless,
      policies->sandbox_flags,
      policies->ip_address_space,
      policies->can_navigate_top_without_user_gesture,
      policies->cross_origin_isolation_enabled_by_dip,
  };
}

blink::mojom::PolicyContainerPoliciesPtr FromWebPolicyContainerPolicies(
    const blink::WebPolicyContainerPolicies& policies) {
  network::CrossOriginEmbedderPolicy cross_origin_embedder_policy;
  cross_origin_embedder_policy.value = policies.cross_origin_embedder_policy;
  return blink::mojom::PolicyContainerPolicies::New(
      policies.connection_allowlists, cross_origin_embedder_policy,
      policies.integrity_policy, policies.integrity_policy_report_only,
      policies.referrer_policy,
      base::ToVector(policies.content_security_policies,
                     BuildContentSecurityPolicy),
      policies.is_credentialless, policies.sandbox_flags,
      policies.ip_address_space, policies.can_navigate_top_without_user_gesture,
      policies.cross_origin_isolation_enabled_by_dip);
}

}  // namespace content
