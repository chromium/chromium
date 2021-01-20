// Copyright 2020 The Chromium Authors. All rights reserved.
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
  // The code is adapted from
  // mojo::AssociatedReceiver::BindWithNewEndpointAndPassDedicatedRemote
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
      mojo::PendingAssociatedReceiver<mojom::blink::PolicyContainerHost>(
          router0->CreateLocalEndpointHandle(id)),
      nullptr);
}

}  // namespace blink
