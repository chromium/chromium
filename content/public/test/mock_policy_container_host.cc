// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/mock_policy_container_host.h"

namespace content {

MockPolicyContainerHost::MockPolicyContainerHost() = default;
MockPolicyContainerHost::~MockPolicyContainerHost() = default;

mojo::PendingAssociatedRemote<blink::mojom::PolicyContainerHost>
MockPolicyContainerHost::BindNewEndpointAndPassDedicatedRemote() {
  return receiver_.BindNewEndpointAndPassDedicatedRemote();
}

void MockPolicyContainerHost::FlushForTesting() {
  receiver_.FlushForTesting();
}

blink::mojom::PolicyContainerPtr
MockPolicyContainerHost::CreatePolicyContainerForBlink() {
  return blink::mojom::PolicyContainer::New(
      blink::mojom::PolicyContainerPolicies::New(),
      BindNewEndpointAndPassDedicatedRemote());
}

void MockPolicyContainerHost::BindWithNewEndpoint(
    mojo::PendingAssociatedReceiver<blink::mojom::PolicyContainerHost>
        receiver) {
  receiver.EnableUnassociatedUsage();
  receiver_.Bind(std::move(receiver));
}

}  // namespace content
