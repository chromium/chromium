// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/policy_container_navigation_bundle.h"

#include <iosfwd>
#include <utility>

#include "base/files/file_path.h"
#include "base/test/bind.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/navigation_simulator.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_constants.h"

namespace content {
namespace {

using ::testing::Eq;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::Pointee;

// Returns non-default policies for use in tests.
PolicyContainerPolicies MakeTestPolicies() {
  PolicyContainerPolicies policies;
  policies.referrer_policy = network::mojom::ReferrerPolicy::kAlways;
  policies.ip_address_space = network::mojom::IPAddressSpace::kPublic;
  return policies;
}

// Shorthand.
scoped_refptr<PolicyContainerHost> NewHost(
    const PolicyContainerPolicies& policies) {
  return base::MakeRefCounted<PolicyContainerHost>(policies);
}

GURL AboutBlankUrl() {
  return GURL(url::kAboutBlankURL);
}

GURL AboutSrcdocUrl() {
  return GURL(url::kAboutSrcdocURL);
}

// RenderViewHostImplTestHarness allows interacting with RenderFrameHosts in the
// form of TestRenderFrameHosts. This allows us to easily set policies on frames
// for testing. It also instantiates a BrowserTaskEnvironment so that tests are
// executed "on the UI thread".
//
// This test fixture is moderately expensive to set up (~100ms overhead per
// test), but still an order of magnitude faster than browser tests.
class PolicyContainerNavigationBundleTest
    : public RenderViewHostImplTestHarness {
 protected:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    contents()->GetMainFrame()->InitializeRenderFrameIfNeeded();
  }
};

// Verifies that the initial delivered policies are default-constructed.
TEST_F(PolicyContainerNavigationBundleTest, DefaultDeliveredPolicies) {
  EXPECT_EQ(PolicyContainerNavigationBundle(nullptr, nullptr, nullptr)
                .DeliveredPolicies(),
            PolicyContainerPolicies());
}

// Verifies that SetIPAddressSpace sets the address space in the bundle's
// delivered policies.
TEST_F(PolicyContainerNavigationBundleTest, SetIPAddressSpace) {
  PolicyContainerNavigationBundle bundle(nullptr, nullptr, nullptr);
  bundle.SetIPAddressSpace(network::mojom::IPAddressSpace::kPublic);

  PolicyContainerPolicies expected_policies;
  expected_policies.ip_address_space = network::mojom::IPAddressSpace::kPublic;

  EXPECT_EQ(bundle.DeliveredPolicies(), expected_policies);
}

// Verifies that the default final policies of a bundle are default-constructed,
// and are equal to the policies of the bundle's policy container host.
TEST_F(PolicyContainerNavigationBundleTest, DefaultFinalPolicies) {
  PolicyContainerNavigationBundle bundle(nullptr, nullptr, nullptr);
  bundle.FinalizePolicies(GURL());

  PolicyContainerPolicies expected_policies;
  EXPECT_EQ(bundle.FinalPolicies(), expected_policies);

  scoped_refptr<PolicyContainerHost> host =
      std::move(bundle).TakePolicyContainerHost();
  ASSERT_THAT(host, NotNull());
  EXPECT_EQ(host->policies(), expected_policies);
}

// Verifies that when the URL of the document to commit does not have a local
// scheme, then the final policies are copied from the delivered policies.
TEST_F(PolicyContainerNavigationBundleTest, FinalPoliciesNormalUrl) {
  PolicyContainerNavigationBundle bundle(nullptr, nullptr, nullptr);

  bundle.SetIPAddressSpace(network::mojom::IPAddressSpace::kPublic);
  bundle.FinalizePolicies(GURL("https://foo.test"));

  EXPECT_EQ(bundle.FinalPolicies(), bundle.DeliveredPolicies());
}

// Verifies that when the URL of the document to commit is `about:blank` but
// there is no initiator frame, the final policies are copied from the delivered
// policies.
TEST_F(PolicyContainerNavigationBundleTest,
       FinalPoliciesAboutBlankWithoutInitiator) {
  PolicyContainerNavigationBundle bundle(nullptr, nullptr, nullptr);

  bundle.SetIPAddressSpace(network::mojom::IPAddressSpace::kPublic);
  bundle.FinalizePolicies(AboutBlankUrl());

  EXPECT_EQ(bundle.FinalPolicies(), bundle.DeliveredPolicies());
}

// This test verifies the default final policies on error pages.
TEST_F(PolicyContainerNavigationBundleTest, DefaultFinalPoliciesForErrorPage) {
  PolicyContainerNavigationBundle bundle(nullptr, nullptr, nullptr);

  bundle.FinalizePoliciesForError();

  // Error pages commit with default policies, mostly ignoring the delivered
  // policies and the document's URL.
  EXPECT_EQ(bundle.FinalPolicies(), PolicyContainerPolicies());
}

// This test verifies that error pages commit in the same IP address space as
// the underlying page would have, had it not failed to load.
TEST_F(PolicyContainerNavigationBundleTest, ErrorPageIPAddressSpace) {
  PolicyContainerNavigationBundle bundle(nullptr, nullptr, nullptr);

  bundle.SetIPAddressSpace(network::mojom::IPAddressSpace::kPublic);
  bundle.FinalizePoliciesForError();

  PolicyContainerPolicies expected_policies;
  expected_policies.ip_address_space = network::mojom::IPAddressSpace::kPublic;
  EXPECT_EQ(bundle.FinalPolicies(), expected_policies);
}

// Verifies that InitiatorPolicies() returns nullptr in the absence of an
// initiator frame token.
TEST_F(PolicyContainerNavigationBundleTest, InitiatorPoliciesWithoutInitiator) {
  EXPECT_THAT(PolicyContainerNavigationBundle(nullptr, nullptr, nullptr)
                  .InitiatorPolicies(),
              IsNull());
}

// It would be nice to verify that when given a wrong token, the bundle just
// ignores it and InitiatorPolicies() returns nullptr. However that path is
// guarded by a DCHECK() so we cannot test it.

// Verifies that SetInitiator() copies the policies of the policy container host
// associated to the given frame token, or resets those policies when given
// nullptr.
TEST_F(PolicyContainerNavigationBundleTest, InitiatorPoliciesWithInitiator) {
  PolicyContainerPolicies initiator_policies = MakeTestPolicies();

  TestRenderFrameHost* initiator = contents()->GetMainFrame();
  initiator->SetPolicyContainerHost(NewHost(initiator_policies));

  // Force implicit conversion from LocalFrameToken to UnguessableToken.
  const blink::LocalFrameToken& token = initiator->GetFrameToken();
  PolicyContainerNavigationBundle bundle(nullptr, &token, nullptr);

  EXPECT_THAT(bundle.InitiatorPolicies(), Pointee(Eq(initiator_policies)));
}

// Verifies that when the URL of the document to commit is `about:blank`, the
// bundle's final policies are copied from the iniitator.
TEST_F(PolicyContainerNavigationBundleTest,
       FinalPoliciesAboutBlankWithInitiator) {
  PolicyContainerPolicies initiator_policies = MakeTestPolicies();

  TestRenderFrameHost* initiator = contents()->GetMainFrame();
  initiator->SetPolicyContainerHost(NewHost(initiator_policies));

  // Force implicit conversion from LocalFrameToken to UnguessableToken.
  const blink::LocalFrameToken& token = initiator->GetFrameToken();
  PolicyContainerNavigationBundle bundle(nullptr, &token, nullptr);
  bundle.FinalizePolicies(AboutBlankUrl());

  EXPECT_EQ(bundle.FinalPolicies(), initiator_policies);
}

// Verifies that ParentPolicies returns nullptr in the absence of a parent.
TEST_F(PolicyContainerNavigationBundleTest, ParentPoliciesWithoutParent) {
  EXPECT_THAT(PolicyContainerNavigationBundle(nullptr, nullptr, nullptr)
                  .ParentPolicies(),
              IsNull());
}

// Verifies that ParentPolicies returns a pointer to a copy of the parent's
// policies.
TEST_F(PolicyContainerNavigationBundleTest, ParentPoliciesWithParent) {
  PolicyContainerPolicies parent_policies = MakeTestPolicies();

  TestRenderFrameHost* parent = contents()->GetMainFrame();
  parent->SetPolicyContainerHost(NewHost(parent_policies));

  PolicyContainerNavigationBundle bundle(parent, nullptr, nullptr);

  EXPECT_THAT(bundle.ParentPolicies(), Pointee(Eq(parent_policies)));
}

// Verifies that when the the URL of the document to commit is `about:srcdoc`,
// the bundle's final policies are copied from the parent.
TEST_F(PolicyContainerNavigationBundleTest,
       FinalPoliciesAboutSrcdocWithParent) {
  PolicyContainerPolicies parent_policies = MakeTestPolicies();

  TestRenderFrameHost* parent = contents()->GetMainFrame();
  parent->SetPolicyContainerHost(NewHost(parent_policies));

  PolicyContainerNavigationBundle bundle(parent, nullptr, nullptr);
  bundle.FinalizePolicies(AboutSrcdocUrl());

  EXPECT_EQ(bundle.FinalPolicies(), parent_policies);
}

}  // namespace
}  // namespace content
