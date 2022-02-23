// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/policy_container_navigation_bundle.h"

#include <iosfwd>
#include <utility>

#include "base/files/file_path.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/navigation_simulator.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_constants.h"

namespace content {
namespace {

using ::testing::ByRef;
using ::testing::Eq;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::Pointee;
using ::testing::SizeIs;

network::mojom::ContentSecurityPolicyPtr MakeTestCSP() {
  auto csp = network::mojom::ContentSecurityPolicy::New();
  csp->header = network::mojom::ContentSecurityPolicyHeader::New();
  csp->header->header_value = "some-directive some-value";
  return csp;
}

// Returns non-default policies for use in tests.
std::unique_ptr<PolicyContainerPolicies> MakeTestPolicies() {
  std::vector<network::mojom::ContentSecurityPolicyPtr> csp_list;
  csp_list.push_back(MakeTestCSP());
  return std::make_unique<PolicyContainerPolicies>(
      network::mojom::ReferrerPolicy::kAlways,
      network::mojom::IPAddressSpace::kPublic,
      /*is_web_secure_context=*/true, std::move(csp_list),
      network::CrossOriginOpenerPolicy(), network::CrossOriginEmbedderPolicy(),
      network::mojom::WebSandboxFlags::kNone);
}

// Shorthand.
scoped_refptr<PolicyContainerHost> NewHost(
    std::unique_ptr<PolicyContainerPolicies> policies) {
  return base::MakeRefCounted<PolicyContainerHost>(std::move(policies));
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
                .DeliveredPoliciesForTesting(),
            PolicyContainerPolicies());
}

// Verifies that SetIPAddressSpace sets the address space in the bundle's
// delivered policies.
TEST_F(PolicyContainerNavigationBundleTest, SetIPAddressSpace) {
  PolicyContainerNavigationBundle bundle(nullptr, nullptr, nullptr);
  bundle.SetIPAddressSpace(network::mojom::IPAddressSpace::kPublic);

  PolicyContainerPolicies expected_policies;
  expected_policies.ip_address_space = network::mojom::IPAddressSpace::kPublic;

  EXPECT_EQ(bundle.DeliveredPoliciesForTesting(), expected_policies);
}

// Verifies that SetIsOriginPotentiallyTrustworthy sets the secure context bit
// in the bundle's delivered policies.
TEST_F(PolicyContainerNavigationBundleTest, SetIsOriginPotentiallyTrustworthy) {
  PolicyContainerNavigationBundle bundle(nullptr, nullptr, nullptr);
  bundle.SetIsOriginPotentiallyTrustworthy(true);

  PolicyContainerPolicies expected_policies;
  expected_policies.is_web_secure_context = true;

  EXPECT_EQ(bundle.DeliveredPoliciesForTesting(), expected_policies);

  bundle.SetIsOriginPotentiallyTrustworthy(false);

  expected_policies.is_web_secure_context = false;
  EXPECT_EQ(bundle.DeliveredPoliciesForTesting(), expected_policies);
}

// Verifies that SetCrossOriginOpenerPolicy sets the cross-origin-opener-policy
// in the bundle's delivered policies.
TEST_F(PolicyContainerNavigationBundleTest, SetCrossOriginOpenerPolicy) {
  PolicyContainerNavigationBundle bundle(nullptr, nullptr, nullptr);

  network::CrossOriginOpenerPolicy coop;
  coop.value = network::mojom::CrossOriginOpenerPolicyValue::kSameOrigin;
  coop.report_only_value =
      network::mojom::CrossOriginOpenerPolicyValue::kSameOriginAllowPopups;
  coop.reporting_endpoint = "A";
  coop.report_only_reporting_endpoint = "B";

  bundle.SetCrossOriginOpenerPolicy(coop);

  PolicyContainerPolicies expected_policies;
  expected_policies.cross_origin_opener_policy = coop;

  EXPECT_EQ(bundle.DeliveredPoliciesForTesting(), expected_policies);
}

// Verifies that the default final policies of a bundle are default-constructed,
// and are equal to the policies of the bundle's policy container host.
TEST_F(PolicyContainerNavigationBundleTest, DefaultFinalPolicies) {
  PolicyContainerNavigationBundle bundle(nullptr, nullptr, nullptr);
  bundle.ComputePolicies(GURL(), false, network::mojom::WebSandboxFlags::kNone);

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
  bundle.AddContentSecurityPolicy(MakeTestCSP());
  std::unique_ptr<PolicyContainerPolicies> delivered_policies =
      bundle.DeliveredPoliciesForTesting().Clone();
  bundle.ComputePolicies(GURL("https://foo.test"), false,
                         network::mojom::WebSandboxFlags::kNone);

  EXPECT_EQ(bundle.FinalPolicies(), *delivered_policies);
}

// Verifies the final policies when the URL of the document to commit is
// `about:blank` but there is no initiator.
TEST_F(PolicyContainerNavigationBundleTest,
       FinalPoliciesAboutBlankWithoutInitiator) {
  PolicyContainerNavigationBundle bundle(nullptr, nullptr, nullptr);
  bundle.SetIPAddressSpace(network::mojom::IPAddressSpace::kPublic);
  std::unique_ptr<PolicyContainerPolicies> delivered_policies =
      bundle.DeliveredPoliciesForTesting().Clone();
  bundle.ComputePolicies(AboutBlankUrl(), false,
                         network::mojom::WebSandboxFlags::kNone);

  EXPECT_EQ(bundle.FinalPolicies(), *delivered_policies);
}

// Verifies the final policies when the URL of the document to commit is
// `about:blank` but there is no initiator, and we have some additional CSPs.
TEST_F(PolicyContainerNavigationBundleTest,
       FinalPoliciesAboutBlankWithoutInitiatorAdditionalCSP) {
  PolicyContainerNavigationBundle bundle(nullptr, nullptr, nullptr);
  bundle.SetIPAddressSpace(network::mojom::IPAddressSpace::kPublic);
  bundle.AddContentSecurityPolicy(MakeTestCSP());
  std::unique_ptr<PolicyContainerPolicies> delivered_policies =
      bundle.DeliveredPoliciesForTesting().Clone();
  bundle.ComputePolicies(AboutBlankUrl(), false,
                         network::mojom::WebSandboxFlags::kNone);

  EXPECT_EQ(bundle.FinalPolicies(), *delivered_policies);
}

// This test verifies the default final policies on error pages.
TEST_F(PolicyContainerNavigationBundleTest, DefaultFinalPoliciesForErrorPage) {
  PolicyContainerNavigationBundle bundle(nullptr, nullptr, nullptr);

  bundle.ComputePoliciesForError(false, network::mojom::WebSandboxFlags::kNone);

  // Error pages commit with default policies, mostly ignoring the delivered
  // policies and the document's URL.
  EXPECT_EQ(bundle.FinalPolicies(), PolicyContainerPolicies());
}

// This test verifies that error pages commit in the same IP address space as
// the underlying page would have, had it not failed to load.
TEST_F(PolicyContainerNavigationBundleTest, ErrorPageIPAddressSpace) {
  PolicyContainerNavigationBundle bundle(nullptr, nullptr, nullptr);

  bundle.SetIPAddressSpace(network::mojom::IPAddressSpace::kPublic);
  bundle.ComputePoliciesForError(false, network::mojom::WebSandboxFlags::kNone);

  PolicyContainerPolicies expected_policies;
  expected_policies.ip_address_space = network::mojom::IPAddressSpace::kPublic;
  EXPECT_EQ(bundle.FinalPolicies(), expected_policies);
}

// Variation of: PolicyContainerNavigationBundleTest.ErrorPageIPAddressSpace
// The decision to commit an error happens after receiving the response.
TEST_F(PolicyContainerNavigationBundleTest,
       ErrorPageIPAddressSpaceAfterResponse) {
  PolicyContainerNavigationBundle bundle(nullptr, nullptr, nullptr);

  bundle.SetIPAddressSpace(network::mojom::IPAddressSpace::kPrivate);
  PolicyContainerPolicies expected_policies;
  expected_policies.ip_address_space = network::mojom::IPAddressSpace::kPrivate;

  bundle.ComputePolicies(GURL("https://foo.test"), false,
                         network::mojom::WebSandboxFlags::kNone);
  EXPECT_EQ(bundle.FinalPolicies(), expected_policies);

  bundle.ComputePoliciesForError(false, network::mojom::WebSandboxFlags::kNone);
  EXPECT_EQ(bundle.FinalPolicies(), expected_policies);
}

// CSP delivered by the HTTP response are ignored for error document.
TEST_F(PolicyContainerNavigationBundleTest,
       DeliveredCSPIgnoredForErrorDocument) {
  PolicyContainerNavigationBundle bundle(nullptr, nullptr, nullptr);
  bundle.AddContentSecurityPolicy(network::mojom::ContentSecurityPolicy::New());

  bundle.ComputePolicies(GURL("https://foo.test"), false,
                         network::mojom::WebSandboxFlags::kNone);
  EXPECT_THAT(bundle.FinalPolicies().content_security_policies, SizeIs(1));

  bundle.ComputePoliciesForError(false, network::mojom::WebSandboxFlags::kNone);
  EXPECT_THAT(bundle.FinalPolicies().content_security_policies, SizeIs(0));
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
  std::unique_ptr<PolicyContainerPolicies> initiator_policies =
      MakeTestPolicies();

  TestRenderFrameHost* initiator = contents()->GetMainFrame();
  initiator->SetPolicyContainerHost(NewHost(initiator_policies->Clone()));

  // Force implicit conversion from LocalFrameToken to UnguessableToken.
  const blink::LocalFrameToken& token = initiator->GetFrameToken();
  PolicyContainerNavigationBundle bundle(nullptr, &token, nullptr);

  EXPECT_THAT(bundle.InitiatorPolicies(),
              Pointee(Eq(ByRef(*initiator_policies))));
}

// Verifies that when the URL of the document to commit is `about:blank`, the
// bundle's final policies are copied from the initiator.
TEST_F(PolicyContainerNavigationBundleTest,
       FinalPoliciesAboutBlankWithInitiator) {
  std::unique_ptr<PolicyContainerPolicies> initiator_policies =
      MakeTestPolicies();

  TestRenderFrameHost* initiator = contents()->GetMainFrame();
  initiator->SetPolicyContainerHost(NewHost(initiator_policies->Clone()));

  // Force implicit conversion from LocalFrameToken to UnguessableToken.
  const blink::LocalFrameToken& token = initiator->GetFrameToken();
  PolicyContainerNavigationBundle bundle(nullptr, &token, nullptr);
  bundle.ComputePolicies(AboutBlankUrl(), false,
                         network::mojom::WebSandboxFlags::kNone);

  EXPECT_EQ(bundle.FinalPolicies(), *initiator_policies);
}

// Verifies that when the URL of the document to commit is `blob:.*`, the
// bundle's final policies are copied from the initiator.
TEST_F(PolicyContainerNavigationBundleTest, FinalPoliciesBlobWithInitiator) {
  std::unique_ptr<PolicyContainerPolicies> initiator_policies =
      MakeTestPolicies();
  TestRenderFrameHost* initiator = contents()->GetMainFrame();
  initiator->SetPolicyContainerHost(NewHost(initiator_policies->Clone()));

  // Force implicit conversion from LocalFrameToken to UnguessableToken.
  const blink::LocalFrameToken& token = initiator->GetFrameToken();
  PolicyContainerNavigationBundle bundle(nullptr, &token, nullptr);

  bundle.ComputePolicies(
      GURL("blob:https://example.com/016ece86-b7f9-4b07-88c2-a0e36b7f1dd6"),
      false, network::mojom::WebSandboxFlags::kNone);

  EXPECT_EQ(bundle.FinalPolicies(), *initiator_policies);
}

// Verifies that when the URL of the document to commit is `about:blank`, the
// bundle's final policies are copied from the initiator, and additional
// delivered policies are merged.
TEST_F(PolicyContainerNavigationBundleTest,
       FinalPoliciesAboutBlankWithInitiatorAndAdditionalCSP) {
  std::unique_ptr<PolicyContainerPolicies> initiator_policies =
      MakeTestPolicies();

  TestRenderFrameHost* initiator = contents()->GetMainFrame();
  initiator->SetPolicyContainerHost(NewHost(initiator_policies->Clone()));

  // Force implicit conversion from LocalFrameToken to UnguessableToken.
  const blink::LocalFrameToken& token = initiator->GetFrameToken();
  PolicyContainerNavigationBundle bundle(nullptr, &token, nullptr);

  // Add some CSP.
  network::mojom::ContentSecurityPolicyPtr test_csp = MakeTestCSP();
  bundle.AddContentSecurityPolicy(test_csp.Clone());
  bundle.ComputePolicies(AboutBlankUrl(), false,
                         network::mojom::WebSandboxFlags::kNone);

  // Append the CPS to the `initiator_policies` just for testing equality
  // later.
  initiator_policies->content_security_policies.push_back(std::move(test_csp));
  EXPECT_EQ(bundle.FinalPolicies(), *initiator_policies);
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
  std::unique_ptr<PolicyContainerPolicies> parent_policies = MakeTestPolicies();

  TestRenderFrameHost* parent = contents()->GetMainFrame();
  parent->SetPolicyContainerHost(NewHost(parent_policies->Clone()));

  PolicyContainerNavigationBundle bundle(parent, nullptr, nullptr);

  EXPECT_THAT(bundle.ParentPolicies(), Pointee(Eq(ByRef(*parent_policies))));
}

// Verifies that when the the URL of the document to commit is `about:srcdoc`,
// the bundle's final policies are copied from the parent.
TEST_F(PolicyContainerNavigationBundleTest,
       FinalPoliciesAboutSrcdocWithParent) {
  std::unique_ptr<PolicyContainerPolicies> parent_policies = MakeTestPolicies();

  TestRenderFrameHost* parent = contents()->GetMainFrame();
  parent->SetPolicyContainerHost(NewHost(parent_policies->Clone()));

  PolicyContainerNavigationBundle bundle(parent, nullptr, nullptr);
  bundle.ComputePolicies(AboutSrcdocUrl(), false,
                         network::mojom::WebSandboxFlags::kNone);

  EXPECT_EQ(bundle.FinalPolicies(), *parent_policies);
}

// Verifies that when a document has a potentially-trustworthy origin and no
// parent, then it is a secure context.
TEST_F(PolicyContainerNavigationBundleTest,
       IsWebSecureContextTrustworthyOriginNoParent) {
  PolicyContainerNavigationBundle bundle(nullptr, nullptr, nullptr);

  bundle.SetIsOriginPotentiallyTrustworthy(true);

  std::unique_ptr<PolicyContainerPolicies> delivered_policies =
      bundle.DeliveredPoliciesForTesting().Clone();
  EXPECT_TRUE(delivered_policies->is_web_secure_context);

  bundle.ComputePolicies(GURL(), false, network::mojom::WebSandboxFlags::kNone);

  EXPECT_EQ(bundle.FinalPolicies(), *delivered_policies);
}

// Verifies that when a document has a non-potentially-trustworthy origin and no
// parent, then it is not a secure context.
TEST_F(PolicyContainerNavigationBundleTest,
       IsWebSecureContextNonTrustworthyOriginNoParent) {
  PolicyContainerNavigationBundle bundle(nullptr, nullptr, nullptr);

  bundle.SetIsOriginPotentiallyTrustworthy(false);

  std::unique_ptr<PolicyContainerPolicies> delivered_policies =
      bundle.DeliveredPoliciesForTesting().Clone();
  EXPECT_FALSE(delivered_policies->is_web_secure_context);

  bundle.ComputePolicies(GURL(), false, network::mojom::WebSandboxFlags::kNone);

  EXPECT_EQ(bundle.FinalPolicies(), *delivered_policies);
}

// Verifies that when a document has a potentially-trustworthy origin and a
// parent that is not a secure context, then it is not a secure context.
TEST_F(PolicyContainerNavigationBundleTest,
       IsWebSecureContextTrustworthyOriginNonSecureParent) {
  std::unique_ptr<PolicyContainerPolicies> parent_policies = MakeTestPolicies();
  parent_policies->is_web_secure_context = false;

  TestRenderFrameHost* parent = contents()->GetMainFrame();
  parent->SetPolicyContainerHost(NewHost(std::move(parent_policies)));

  PolicyContainerNavigationBundle bundle(parent, nullptr, nullptr);

  bundle.SetIsOriginPotentiallyTrustworthy(true);

  bundle.ComputePolicies(GURL("https://foo.test"), false,
                         network::mojom::WebSandboxFlags::kNone);

  EXPECT_FALSE(bundle.FinalPolicies().is_web_secure_context);
}

// Verifies that when a document has a non-potentially-trustworthy origin and a
// parent that is a secure context, then it is not a secure context.
TEST_F(PolicyContainerNavigationBundleTest,
       IsWebSecureContextNonTrustworthyOriginSecureParent) {
  std::unique_ptr<PolicyContainerPolicies> parent_policies = MakeTestPolicies();
  parent_policies->is_web_secure_context = true;

  TestRenderFrameHost* parent = contents()->GetMainFrame();
  parent->SetPolicyContainerHost(NewHost(std::move(parent_policies)));

  PolicyContainerNavigationBundle bundle(parent, nullptr, nullptr);

  bundle.SetIsOriginPotentiallyTrustworthy(false);

  std::unique_ptr<PolicyContainerPolicies> delivered_policies =
      bundle.DeliveredPoliciesForTesting().Clone();
  EXPECT_FALSE(delivered_policies->is_web_secure_context);

  bundle.ComputePolicies(GURL("http://foo.test"), false,
                         network::mojom::WebSandboxFlags::kNone);

  EXPECT_EQ(bundle.FinalPolicies(), *delivered_policies);
}

// Verifies that when a document has a potentially-trustworthy origin and a
// parent that is a secure context, then it is a secure context.
TEST_F(PolicyContainerNavigationBundleTest,
       IsWebSecureContextTrustworthyOriginSecureParent) {
  std::unique_ptr<PolicyContainerPolicies> parent_policies = MakeTestPolicies();
  parent_policies->is_web_secure_context = true;

  TestRenderFrameHost* parent = contents()->GetMainFrame();
  parent->SetPolicyContainerHost(NewHost(std::move(parent_policies)));

  PolicyContainerNavigationBundle bundle(parent, nullptr, nullptr);

  bundle.SetIsOriginPotentiallyTrustworthy(true);

  std::unique_ptr<PolicyContainerPolicies> delivered_policies =
      bundle.DeliveredPoliciesForTesting().Clone();
  EXPECT_TRUE(delivered_policies->is_web_secure_context);

  bundle.ComputePolicies(GURL("https://foo.test"), false,
                         network::mojom::WebSandboxFlags::kNone);

  EXPECT_EQ(bundle.FinalPolicies(), *delivered_policies);
}

// Verifies that when the the URL of the document to commit is `about:srcdoc`,
// the bundle's final policies are copied from the parent, and additional
// delivered policies are merged.
TEST_F(PolicyContainerNavigationBundleTest,
       FinalPoliciesAboutSrcdocWithParentAndAdditionalCSP) {
  std::unique_ptr<PolicyContainerPolicies> parent_policies = MakeTestPolicies();

  TestRenderFrameHost* parent = contents()->GetMainFrame();
  parent->SetPolicyContainerHost(NewHost(parent_policies->Clone()));

  PolicyContainerNavigationBundle bundle(parent, nullptr, nullptr);

  // Add some CSP.
  network::mojom::ContentSecurityPolicyPtr test_csp = MakeTestCSP();
  bundle.AddContentSecurityPolicy(test_csp.Clone());
  bundle.ComputePolicies(AboutSrcdocUrl(), false,
                         network::mojom::WebSandboxFlags::kNone);

  // Append the CPS to the `parent_policies` just for testing equality
  // later.
  parent_policies->content_security_policies.push_back(std::move(test_csp));
  EXPECT_EQ(bundle.FinalPolicies(), *parent_policies);
}

// Calling ComputePolicies() twice triggers a DCHECK.
TEST_F(PolicyContainerNavigationBundleTest, ComputePoliciesTwiceDCHECK) {
  PolicyContainerNavigationBundle bundle(nullptr, nullptr, nullptr);
  GURL url("https://foo.test");
  bundle.ComputePolicies(url, false, network::mojom::WebSandboxFlags::kNone);
  EXPECT_DCHECK_DEATH(bundle.ComputePolicies(
      url, false, network::mojom::WebSandboxFlags::kNone));
}

// Calling ComputePolicies() followed by ComputePoliciesForError() is supported.
TEST_F(PolicyContainerNavigationBundleTest, ComputePoliciesThenError) {
  PolicyContainerNavigationBundle bundle(nullptr, nullptr, nullptr);
  bundle.ComputePolicies(GURL("https://foo.test"), false,
                         network::mojom::WebSandboxFlags::kNone);
  bundle.ComputePoliciesForError(false, network::mojom::WebSandboxFlags::kNone);
}

// After ComputePolicies() or ComputePoliciesForError(), the initiator policies
// are still accessible.
TEST_F(PolicyContainerNavigationBundleTest,
       AccessInitiatorAfterComputingPolicies) {
  std::unique_ptr<PolicyContainerPolicies> initiator_policies =
      MakeTestPolicies();
  TestRenderFrameHost* initiator = contents()->GetMainFrame();
  initiator->SetPolicyContainerHost(NewHost(initiator_policies->Clone()));
  const blink::LocalFrameToken& token = initiator->GetFrameToken();

  PolicyContainerNavigationBundle bundle(nullptr, &token, nullptr);
  EXPECT_THAT(bundle.InitiatorPolicies(),
              Pointee(Eq(ByRef(*initiator_policies))));

  bundle.ComputePolicies(GURL("https://foo.test"), false,
                         network::mojom::WebSandboxFlags::kNone);
  EXPECT_THAT(bundle.InitiatorPolicies(),
              Pointee(Eq(ByRef(*initiator_policies))));

  bundle.ComputePoliciesForError(false, network::mojom::WebSandboxFlags::kNone);
  EXPECT_THAT(bundle.InitiatorPolicies(),
              Pointee(Eq(ByRef(*initiator_policies))));
}

// After ComputePolicies() or ComputePoliciesForError(), the parent
// policies are still accessible.
TEST_F(PolicyContainerNavigationBundleTest,
       AccessParentAfterComputingPolicies) {
  std::unique_ptr<PolicyContainerPolicies> parent_policies = MakeTestPolicies();
  TestRenderFrameHost* parent = contents()->GetMainFrame();
  parent->SetPolicyContainerHost(NewHost(parent_policies->Clone()));

  PolicyContainerNavigationBundle bundle(parent, nullptr, nullptr);
  EXPECT_THAT(bundle.ParentPolicies(), Pointee(Eq(ByRef(*parent_policies))));

  bundle.ComputePolicies(GURL("https://foo.test"), false,
                         network::mojom::WebSandboxFlags::kNone);
  EXPECT_THAT(bundle.ParentPolicies(), Pointee(Eq(ByRef(*parent_policies))));

  bundle.ComputePoliciesForError(false, network::mojom::WebSandboxFlags::kNone);
  EXPECT_THAT(bundle.ParentPolicies(), Pointee(Eq(ByRef(*parent_policies))));
}

// Verifies that the parent policies are preserved on
// ResetForCrossDocumentRestart.
TEST_F(PolicyContainerNavigationBundleTest,
       ResetForCrossDocumentRestartParentPolicies) {
  std::unique_ptr<PolicyContainerPolicies> parent_policies = MakeTestPolicies();

  TestRenderFrameHost* parent = contents()->GetMainFrame();
  parent->SetPolicyContainerHost(NewHost(parent_policies->Clone()));

  PolicyContainerNavigationBundle bundle(parent, nullptr, nullptr);
  bundle.ComputePolicies(GURL("https://foo.test"), false,
                         network::mojom::WebSandboxFlags::kNone);
  EXPECT_EQ(bundle.FinalPolicies(), PolicyContainerPolicies());

  bundle.ResetForCrossDocumentRestart();
  EXPECT_THAT(bundle.ParentPolicies(), Pointee(Eq(ByRef(*parent_policies))));
  bundle.ComputePolicies(AboutSrcdocUrl(), false,
                         network::mojom::WebSandboxFlags::kNone);

  EXPECT_EQ(bundle.FinalPolicies(), *parent_policies);
}

// Verifies that the initiator policies are preserved on
// ResetForCrossDocumentRestart.
TEST_F(PolicyContainerNavigationBundleTest,
       ResetForCrossDocumentRestartInitiatorPolicies) {
  std::unique_ptr<PolicyContainerPolicies> initiator_policies =
      MakeTestPolicies();

  TestRenderFrameHost* initiator = contents()->GetMainFrame();
  initiator->SetPolicyContainerHost(NewHost(initiator_policies->Clone()));

  // Force implicit conversion from LocalFrameToken to UnguessableToken.
  const blink::LocalFrameToken& token = initiator->GetFrameToken();
  PolicyContainerNavigationBundle bundle(nullptr, &token, nullptr);

  bundle.ComputePolicies(GURL("https://foo.test"), false,
                         network::mojom::WebSandboxFlags::kNone);
  EXPECT_EQ(bundle.FinalPolicies(), PolicyContainerPolicies());

  bundle.ResetForCrossDocumentRestart();
  EXPECT_THAT(bundle.InitiatorPolicies(),
              Pointee(Eq(ByRef(*initiator_policies))));
  bundle.ComputePolicies(AboutBlankUrl(), false,
                         network::mojom::WebSandboxFlags::kNone);

  EXPECT_EQ(bundle.FinalPolicies(), *initiator_policies);
}

}  // namespace
}  // namespace content
