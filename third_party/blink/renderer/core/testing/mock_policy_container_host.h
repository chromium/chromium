// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_MOCK_POLICY_CONTAINER_HOST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_MOCK_POLICY_CONTAINER_HOST_H_

#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/frame/policy_container.mojom-blink.h"

namespace blink {

class MockPolicyContainerHost : public mojom::blink::PolicyContainerHost {
 public:
  MOCK_METHOD(void,
              SetReferrerPolicy,
              (network::mojom::ReferrerPolicy),
              (override));
  MockPolicyContainerHost() = default;

  mojo::PendingAssociatedRemote<mojom::blink::PolicyContainerHost>
  BindNewEndpointAndPassDedicatedRemote();
  void FlushForTesting();

 private:
  mojo::AssociatedReceiver<mojom::blink::PolicyContainerHost> receiver_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_FAKE_POLICY_CONTAINER_HOST_H_
