// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/policy_container.h"

namespace content {

PolicyContainer::PolicyContainer() = default;
PolicyContainer::PolicyContainer(network::mojom::ReferrerPolicy referrer_policy)
    : referrer_policy_(referrer_policy) {}
PolicyContainer::~PolicyContainer() = default;

void PolicyContainer::SetReferrerPolicy(
    network::mojom::ReferrerPolicy referrer_policy) {
  referrer_policy_ = referrer_policy;
}

blink::mojom::PolicyContainerClientPtr PolicyContainer::CreateClientForBlink() {
  // This function might be called several times, for example if we need to
  // recreate the RenderFrame after the renderer process died. We gracefully
  // handle this by resetting the receiver and creating a new one. It would be
  // good to find a way to check that the previous remote has been deleted or is
  // not needed anymore. Unfortunately, this cannot be done with a disconnect
  // handler, since the mojo disconnect notification is not guaranteed to be
  // received before we try to create a new remote.
  policy_container_host_receiver_.reset();
  return blink::mojom::PolicyContainerClient::New(
      blink::mojom::PolicyContainerData::New(referrer_policy_),
      policy_container_host_receiver_.BindNewEndpointAndPassRemote());
}

std::unique_ptr<PolicyContainer> PolicyContainer::Clone() const {
  std::unique_ptr<PolicyContainer> copy =
      std::make_unique<PolicyContainer>(referrer_policy_);
  return copy;
}

}  // namespace content
