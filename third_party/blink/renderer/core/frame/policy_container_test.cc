// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/policy_container.h"

#include "services/network/public/mojom/content_security_policy.mojom-blink-forward.h"
#include "services/network/public/mojom/cross_origin_embedder_policy.mojom-blink-forward.h"
#include "services/network/public/mojom/ip_address_space.mojom-blink-forward.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/testing/mock_policy_container_host.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

TEST(PolicyContainerTest, MembersAreSetDuringConstruction) {
  test::TaskEnvironment task_environment;
  MockPolicyContainerHost host;
  auto policies = mojom::blink::PolicyContainerPolicies::New(
      network::CrossOriginEmbedderPolicy(
          network::mojom::blink::CrossOriginEmbedderPolicyValue::kNone),
      network::mojom::blink::ReferrerPolicy::kNever,
      Vector<network::mojom::blink::ContentSecurityPolicyPtr>(),
      /*anonymous=*/false, network::mojom::WebSandboxFlags::kNone,
      network::mojom::blink::IPAddressSpace::kUnknown,
      /*can_navigate_top_without_user_gesture=*/true,
      /*allow_cross_origin_isolation_under_initial_empty_document=*/false);
  PolicyContainer policy_container(host.BindNewEndpointAndPassDedicatedRemote(),
                                   std::move(policies));

  EXPECT_EQ(network::mojom::blink::ReferrerPolicy::kNever,
            policy_container.GetReferrerPolicy());
}

TEST(PolicyContainerTest, UpdateReferrerPolicyIsPropagated) {
  test::TaskEnvironment task_environment;
  MockPolicyContainerHost host;
  auto policies = mojom::blink::PolicyContainerPolicies::New(
      network::CrossOriginEmbedderPolicy(
          network::mojom::blink::CrossOriginEmbedderPolicyValue::kNone),
      network::mojom::blink::ReferrerPolicy::kAlways,
      Vector<network::mojom::blink::ContentSecurityPolicyPtr>(),
      /*anonymous=*/false, network::mojom::WebSandboxFlags::kNone,
      network::mojom::blink::IPAddressSpace::kUnknown,
      /*can_navigate_top_without_user_gesture=*/true,
      /*allow_cross_origin_isolation_under_initial_empty_document=*/false);
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

TEST(PolicyContainerTest, AddContentSecurityPolicies) {
  test::TaskEnvironment task_environment;
  MockPolicyContainerHost host;
  auto policies = mojom::blink::PolicyContainerPolicies::New();
  PolicyContainer policy_container(host.BindNewEndpointAndPassDedicatedRemote(),
                                   std::move(policies));

  Vector<network::mojom::blink::ContentSecurityPolicyPtr> new_csps =
      ParseContentSecurityPolicies(
          "script-src 'self' https://example.com:8080,\n"
          "default-src 'self'; img-src example.com",
          network::mojom::blink::ContentSecurityPolicyType::kEnforce,
          network::mojom::blink::ContentSecurityPolicySource::kHTTP,
          KURL("https://example.org"));

  EXPECT_CALL(
      host, AddContentSecurityPolicies(testing::Eq(testing::ByRef(new_csps))));

  policy_container.AddContentSecurityPolicies(mojo::Clone(new_csps));
  EXPECT_EQ(new_csps, policy_container.GetPolicies().content_security_policies);

  // Wait for mojo messages to be received.
  host.FlushForTesting();
}

}  // namespace blink
