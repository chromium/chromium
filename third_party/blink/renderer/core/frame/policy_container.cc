// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/policy_container.h"

#include <tuple>

#include "services/network/public/cpp/web_sandbox_flags.h"
#include "services/network/public/mojom/integrity_policy.mojom-blink.h"
#include "third_party/blink/renderer/platform/loader/fetch/policy_container_utils.h"

namespace blink {

PolicyContainer::PolicyContainer(
    mojo::PendingAssociatedRemote<mojom::blink::PolicyContainerHost> remote,
    mojom::blink::PolicyContainerPoliciesPtr policies)
    : policies_(std::move(policies)),
      policy_container_host_remote_(std::move(remote)) {}

// static
std::unique_ptr<PolicyContainer> PolicyContainer::CreateEmpty() {
  // Create a dummy PolicyContainerHost remote. All the messages will be
  // ignored.
  mojo::AssociatedRemote<mojom::blink::PolicyContainerHost> dummy_host;
  std::ignore = dummy_host.BindNewEndpointAndPassDedicatedReceiver();

  return std::make_unique<PolicyContainer>(
      dummy_host.Unbind(), mojom::blink::PolicyContainerPolicies::New());
}

// static
std::unique_ptr<PolicyContainer> PolicyContainer::CreateFromWebPolicyContainer(
    std::unique_ptr<WebPolicyContainer> container) {
  if (!container)
    return nullptr;

  return std::make_unique<PolicyContainer>(
      std::move(container->remote),
      FromWebPolicyContainerPolicies(container->policies));
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

const mojom::blink::PolicyContainerPolicies& PolicyContainer::GetPolicies()
    const {
  return *policies_;
}

void PolicyContainer::AddContentSecurityPolicies(
    Vector<network::mojom::blink::ContentSecurityPolicyPtr> policies) {
  for (const auto& policy : policies) {
    policies_->content_security_policies.push_back(policy->Clone());
  }
  policy_container_host_remote_->AddContentSecurityPolicies(
      std::move(policies));
}

}  // namespace blink
