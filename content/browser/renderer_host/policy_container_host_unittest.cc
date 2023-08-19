// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/policy_container_host.h"

#include "base/run_loop.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "services/network/public/mojom/ip_address_space.mojom-shared.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

using ::testing::ByRef;
using ::testing::Eq;
using ::testing::Pointee;

namespace {

struct SameSizeAsPolicyContainerPolicies {
  network::mojom::ReferrerPolicy referrer_policy;
  network::mojom::IPAddressSpace ip_address_space;
  bool is_web_secure_context;
  std::vector<network::mojom::ContentSecurityPolicyPtr>
      content_security_policies;
  network::CrossOriginOpenerPolicy cross_origin_opener_policy;
  network::CrossOriginEmbedderPolicy cross_origin_embedder_policy;
  network::mojom::WebSandboxFlags sandbox_flags;
  bool is_credentialless;
  bool can_navigate_top_without_user_gesture;
};

}  // namespace

// Asserts size of PolicyContainerPolicies, so that whenever a new element is
// added to PolicyContainerPolicies, the assert will fail. When hitting this
// assert failure, please ensure that the attribute is
// - added to the PolicyContainerPolicies constructor
// - copied correctly in PolicyContainerPolicies::Clone()
// - checked correctly in PolicyContainerPolicies::operator==
// - handled correctly in PolicyContainerPolicies::operator<<
// - tested correctly in PolicyContainerHostTest.PolicyContainerPolicies below.
static_assert(sizeof(PolicyContainerPolicies) ==
                  sizeof(SameSizeAsPolicyContainerPolicies),
              "PolicyContainerPolicies have been modified");

TEST(PolicyContainerPoliciesTest, CloneIsEqual) {
  std::vector<network::mojom::ContentSecurityPolicyPtr> csps;
  auto csp = network::mojom::ContentSecurityPolicy::New();
  csp->treat_as_public_address = true;
  csps.push_back(std::move(csp));
  network::CrossOriginOpenerPolicy coop;
  network::mojom::WebSandboxFlags sandbox_flags =
      network::mojom::WebSandboxFlags::kOrientationLock |
      network::mojom::WebSandboxFlags::kPropagatesToAuxiliaryBrowsingContexts;
  coop.value = network::mojom::CrossOriginOpenerPolicyValue::kSameOrigin;
  coop.report_only_value =
      network::mojom::CrossOriginOpenerPolicyValue::kSameOriginAllowPopups;
  coop.reporting_endpoint = "endpoint 1";
  coop.report_only_reporting_endpoint = "endpoint 2";
  network::CrossOriginEmbedderPolicy coep;
  coep.value = network::mojom::CrossOriginEmbedderPolicyValue::kRequireCorp;
  coep.report_only_value =
      network::mojom::CrossOriginEmbedderPolicyValue::kCredentialless;
  coep.reporting_endpoint = "endpoint 1";
  coep.report_only_reporting_endpoint = "endpoint 2";

  PolicyContainerPolicies policies(
      network::mojom::ReferrerPolicy::kAlways,
      network::mojom::IPAddressSpace::kUnknown,
      /*is_web_secure_context=*/true, std::move(csps), coop, coep,
      sandbox_flags,
      /*is_credentialless=*/true,
      /*can_navigate_top_without_user_gesture=*/true,
      /*allow_cross_origin_isolation=*/false);

  EXPECT_THAT(policies.Clone(), Eq(ByRef(policies)));
}

TEST(PolicyContainerHostTest, ReferrerPolicy) {
  scoped_refptr<PolicyContainerHost> policy_container =
      base::MakeRefCounted<PolicyContainerHost>();
  EXPECT_EQ(network::mojom::ReferrerPolicy::kDefault,
            policy_container->referrer_policy());

  static_cast<blink::mojom::PolicyContainerHost*>(policy_container.get())
      ->SetReferrerPolicy(network::mojom::ReferrerPolicy::kAlways);
  EXPECT_EQ(network::mojom::ReferrerPolicy::kAlways,
            policy_container->referrer_policy());
}

TEST(PolicyContainerHostTest, AssociateWithFrameToken) {
  // We need to satisfy DCHECK_CURRENTLY_ON(BrowserThread::UI).
  content::BrowserTaskEnvironment task_environment;

  scoped_refptr<PolicyContainerHost> policy_container_host =
      base::MakeRefCounted<PolicyContainerHost>();
  blink::LocalFrameToken token;
  policy_container_host->AssociateWithFrameToken(token);
  EXPECT_EQ(policy_container_host.get(),
            PolicyContainerHost::FromFrameToken(token));

  // Check that we can associate a new PolicyContainerHost to the same frame
  // token and everything works correctly.
  scoped_refptr<PolicyContainerHost> policy_container_host_2 =
      base::MakeRefCounted<PolicyContainerHost>();
  policy_container_host_2->AssociateWithFrameToken(token);
  EXPECT_EQ(policy_container_host_2.get(),
            PolicyContainerHost::FromFrameToken(token));
}

TEST(PolicyContainerHostTest, KeepAliveThroughBlinkPolicyContainerRemote) {
  // Enable tasks and RunLoop on the main thread and satisfy
  // DCHECK_CURRENTLY_ON(BrowserThread::UI).
  content::BrowserTaskEnvironment task_environment;

  scoped_refptr<PolicyContainerHost> policy_container_host =
      base::MakeRefCounted<PolicyContainerHost>();
  blink::LocalFrameToken token;
  policy_container_host->AssociateWithFrameToken(token);

  blink::mojom::PolicyContainerPtr blink_policy_container =
      policy_container_host->CreatePolicyContainerForBlink();

  PolicyContainerHost* raw_pointer = policy_container_host.get();
  EXPECT_EQ(raw_pointer, PolicyContainerHost::FromFrameToken(token));

  policy_container_host.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(raw_pointer, PolicyContainerHost::FromFrameToken(token));

  blink_policy_container->remote.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(PolicyContainerHost::FromFrameToken(token));
}

// Test that the disconnect handler is called when all keep alive handles
// disconnect.
TEST(PolicyContainerHostTest, KeepAliveThroughKeepAlives) {
  // Enable tasks and RunLoop on the main thread and satisfy
  // DCHECK_CURRENTLY_ON(BrowserThread::UI).
  content::BrowserTaskEnvironment task_environment;

  scoped_refptr<PolicyContainerHost> policy_container_host =
      base::MakeRefCounted<PolicyContainerHost>();
  blink::LocalFrameToken token;
  policy_container_host->AssociateWithFrameToken(token);

  mojo::PendingRemote<blink::mojom::PolicyContainerHostKeepAliveHandle>
      keep_alive;
  policy_container_host->IssueKeepAliveHandle(
      keep_alive.InitWithNewPipeAndPassReceiver());

  PolicyContainerHost* raw_pointer = policy_container_host.get();
  EXPECT_EQ(raw_pointer, PolicyContainerHost::FromFrameToken(token));

  policy_container_host.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(raw_pointer, PolicyContainerHost::FromFrameToken(token));

  keep_alive.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(PolicyContainerHost::FromFrameToken(token));
}

}  // namespace content
