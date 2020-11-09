// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/policy_container_host.h"

namespace content {

PolicyContainerHost::PolicyContainerHost() = default;
PolicyContainerHost::PolicyContainerHost(
    network::mojom::ReferrerPolicy referrer_policy)
    : referrer_policy_(referrer_policy) {}
PolicyContainerHost::~PolicyContainerHost() = default;

void PolicyContainerHost::SetReferrerPolicy(
    network::mojom::ReferrerPolicy referrer_policy) {
  referrer_policy_ = referrer_policy;
}

blink::mojom::PolicyContainerPtr
PolicyContainerHost::CreatePolicyContainerForBlink() {
  // This function might be called several times, for example if we need to
  // recreate the RenderFrame after the renderer process died. We gracefully
  // handle this by resetting the receiver and creating a new one. It would be
  // good to find a way to check that the previous remote has been deleted or is
  // not needed anymore. Unfortunately, this cannot be done with a disconnect
  // handler, since the mojo disconnect notification is not guaranteed to be
  // received before we try to create a new remote.
  policy_container_host_receiver_.reset();
  return blink::mojom::PolicyContainer::New(
      blink::mojom::PolicyContainerDocumentPolicies::New(referrer_policy_),
      policy_container_host_receiver_.BindNewEndpointAndPassRemote());
}

std::unique_ptr<PolicyContainerHost> PolicyContainerHost::Clone() const {
  std::unique_ptr<PolicyContainerHost> copy =
      std::make_unique<PolicyContainerHost>(referrer_policy_);
  return copy;
}

void PolicyContainerHost::Bind(
    mojo::PendingAssociatedReceiver<blink::mojom::PolicyContainerHost>
        receiver) {
  policy_container_host_receiver_.Bind(std::move(receiver));
}

}  // namespace content
