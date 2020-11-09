// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/policy_container.h"

namespace blink {

PolicyContainer::PolicyContainer(
    mojo::PendingAssociatedRemote<mojom::blink::PolicyContainerHost> remote,
    mojom::blink::PolicyContainerDocumentPoliciesPtr policies)
    : policies_(std::move(policies)),
      policy_container_host_remote_(std::move(remote)) {}

// static
std::unique_ptr<PolicyContainer> PolicyContainer::CreateFromWebPolicyContainer(
    std::unique_ptr<WebPolicyContainer> container) {
  if (!container)
    return nullptr;
  mojom::blink::PolicyContainerDocumentPoliciesPtr policies =
      mojom::blink::PolicyContainerDocumentPolicies::New(
          container->policies.referrer_policy);
  return std::make_unique<PolicyContainer>(std::move(container->remote),
                                           std::move(policies));
}

network::mojom::blink::ReferrerPolicy PolicyContainer::GetReferrerPolicy()
    const {
  return policies_->referrer_policy;
}

void PolicyContainer::UpdateReferrerPolicy(
    network::mojom::blink::ReferrerPolicy policy) {
  policies_->referrer_policy = policy;
  policy_container_host_remote_->SetReferrerPolicy(policy);
}

const mojom::blink::PolicyContainerDocumentPoliciesPtr&
PolicyContainer::GetPolicies() const {
  return policies_;
}

}  // namespace blink
