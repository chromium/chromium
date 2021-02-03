// Copyright 2021 The Chromium Authors. All rights reserved.
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
  // The code is adapted from
  // mojo::AssociatedReceiver::BindNewEndpointAndPassDedicatedRemote
  //
  // TODO(https://crbug.com/1173504): We should avoid using mojo::internal
  // here. Revisit this code once mojo implements a helper that does this.
  mojo::MessagePipe pipe;
  scoped_refptr<mojo::internal::MultiplexRouter> router0 =
      new mojo::internal::MultiplexRouter(
          std::move(pipe.handle0),
          mojo::internal::MultiplexRouter::MULTI_INTERFACE, false,
          base::SequencedTaskRunnerHandle::Get());
  scoped_refptr<mojo::internal::MultiplexRouter> router1 =
      new mojo::internal::MultiplexRouter(
          std::move(pipe.handle1),
          mojo::internal::MultiplexRouter::MULTI_INTERFACE, true,
          base::SequencedTaskRunnerHandle::Get());

  mojo::InterfaceId id = router1->AssociateInterface(receiver.PassHandle());

  receiver_.Bind(
      mojo::PendingAssociatedReceiver<blink::mojom::PolicyContainerHost>(
          router0->CreateLocalEndpointHandle(id)),
      nullptr);
}

}  // namespace content
