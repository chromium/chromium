// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_MOCK_POLICY_CONTAINER_HOST_H_
#define CONTENT_PUBLIC_TEST_MOCK_POLICY_CONTAINER_HOST_H_

#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "services/network/public/mojom/content_security_policy.mojom-forward.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/frame/policy_container.mojom.h"

namespace content {

class MockPolicyContainerHost final : public blink::mojom::PolicyContainerHost {
 public:
  MOCK_METHOD(void,
              SetReferrerPolicy,
              (network::mojom::ReferrerPolicy),
              (override));
  MOCK_METHOD(void,
              AddContentSecurityPolicies,
              (std::vector<network::mojom::ContentSecurityPolicyPtr>),
              (override));
  MockPolicyContainerHost();
  ~MockPolicyContainerHost() override;

  blink::mojom::PolicyContainerPtr CreatePolicyContainerForBlink();

  // This does the same as BindNewEndpointAndPassDedicatedRemote, but allows the
  // remote to be created first and the receiver to be passed in.
  void BindWithNewEndpoint(
      mojo::PendingAssociatedReceiver<blink::mojom::PolicyContainerHost>
          receiver);

  mojo::PendingAssociatedRemote<blink::mojom::PolicyContainerHost>
  BindNewEndpointAndPassDedicatedRemote();
  void FlushForTesting();

 private:
  mojo::AssociatedReceiver<blink::mojom::PolicyContainerHost> receiver_{this};
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_MOCK_POLICY_CONTAINER_HOST_H_
