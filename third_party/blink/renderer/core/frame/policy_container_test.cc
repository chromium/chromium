// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/policy_container.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/testing/mock_policy_container_host.h"

namespace blink {

TEST(PolicyContainerTest, MembersAreSetDuringConstruction) {
  MockPolicyContainerHost host;
  auto policies = mojom::blink::PolicyContainerPolicies::New(
      network::mojom::blink::ReferrerPolicy::kNever,
      network::mojom::blink::IPAddressSpace::kPrivate);
  PolicyContainer policy_container(host.BindNewEndpointAndPassDedicatedRemote(),
                                   std::move(policies));

  EXPECT_EQ(network::mojom::blink::ReferrerPolicy::kNever,
            policy_container.GetReferrerPolicy());
  EXPECT_EQ(network::mojom::blink::IPAddressSpace::kPrivate,
            policy_container.GetIPAddressSpace());
}

TEST(PolicyContainerTest, UpdateReferrerPolicyIsPropagated) {
  MockPolicyContainerHost host;
  auto policies = mojom::blink::PolicyContainerPolicies::New(
      network::mojom::blink::ReferrerPolicy::kAlways,
      network::mojom::blink::IPAddressSpace::kPublic);
  PolicyContainer policy_container(host.BindNewEndpointAndPassDedicatedRemote(),
                                   std::move(policies));

  EXPECT_CALL(host,
              SetReferrerPolicy(network::mojom::blink::ReferrerPolicy::kNever));
  policy_container.UpdateReferrerPolicy(
      network::mojom::blink::ReferrerPolicy::kNever);
  EXPECT_EQ(network::mojom::blink::ReferrerPolicy::kNever,
            policy_container.GetReferrerPolicy());

  // Wait for mojo messages to be received.
  host.FlushForTesting();
}

}  // namespace blink
