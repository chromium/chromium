// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/policy_container_util.h"

#include "content/renderer/content_security_policy_util.h"
#include "third_party/blink/public/mojom/frame/policy_container.mojom.h"

namespace content {

std::unique_ptr<blink::WebPolicyContainer> ToWebPolicyContainer(
    blink::mojom::PolicyContainerPtr in) {
  if (!in)
    return nullptr;

  return std::make_unique<blink::WebPolicyContainer>(
      blink::WebPolicyContainerPolicies{
          in->policies->cross_origin_embedder_policy.value,
          in->policies->referrer_policy,
          ToWebContentSecurityPolicies(
              std::move(in->policies->content_security_policies)),
          in->policies->is_credentialless,
          in->policies->sandbox_flags,
          in->policies->ip_address_space,
          in->policies->can_navigate_top_without_user_gesture,
          in->policies->allow_cross_origin_isolation,
      },
      std::move(in->remote));
}

}  // namespace content
