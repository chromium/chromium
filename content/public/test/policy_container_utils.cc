// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/policy_container_utils.h"

#include "third_party/blink/public/mojom/frame/policy_container.mojom.h"

namespace content {

blink::mojom::PolicyContainerClientPtr CreateStubPolicyContainerClient() {
  mojo::PendingAssociatedRemote<blink::mojom::PolicyContainerHost>
      stub_policy_container_remote;
  ignore_result(
      stub_policy_container_remote.InitWithNewEndpointAndPassReceiver());
  return blink::mojom::PolicyContainerClient::New(
      blink::mojom::PolicyContainerData::New(),
      std::move(stub_policy_container_remote));
}

}  // namespace content
