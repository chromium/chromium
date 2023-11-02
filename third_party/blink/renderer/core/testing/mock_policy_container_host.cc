// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/mock_policy_container_host.h"

namespace blink {

mojo::PendingAssociatedRemote<mojom::blink::PolicyContainerHost>
MockPolicyContainerHost::BindNewEndpointAndPassDedicatedRemote() {
  return receiver_.BindNewEndpointAndPassDedicatedRemote();
}

void MockPolicyContainerHost::FlushForTesting() {
  receiver_.FlushForTesting();
}

void MockPolicyContainerHost::BindWithNewEndpoint(
    mojo::PendingAssociatedReceiver<mojom::blink::PolicyContainerHost>
        receiver) {
  receiver.EnableUnassociatedUsage();
  receiver_.Bind(std::move(receiver));
}

}  // namespace blink
