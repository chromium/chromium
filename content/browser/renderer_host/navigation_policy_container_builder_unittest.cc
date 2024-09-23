// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigation_policy_container_builder.h"

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
PolicyContainerPolicies MakeTestPolicies() {
  std::vector<network::mojom::ContentSecurityPolicyPtr> csp_list;
  csp_list.push_back(MakeTestCSP());
  return PolicyContainerPolicies(
      network::mojom::ReferrerPolicy::kAlways,
      network::mojom::IPAddressSpace::kPublic,
      /*is_web_secure_context=*/true, std::move(csp_list),
      network::CrossOriginOpenerPolicy(), network::CrossOriginEmbedderPolicy(),
      network::DocumentIsolationPolicy(),
      network::mojom::WebSandboxFlags::kNone,
      /*is_credentialless=*/false,
      /*can_navigate_top_without_user_gesture=*/true,
      /*allow_cross_origin_isolation=*/false);
}

// Shorthand.
scoped_refptr<PolicyContainerHost> NewHost(PolicyContainerPolicies policies) {
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
class NavigationPolicyContainerBuilderTest
    : public RenderViewHostImplTestHarness {
 protected:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    contents()->GetPrimaryMainFrame()->InitializeRenderFrameIfNeeded();
  }
};

// Verifies that the initial delivered policies are default-constructed.
TEST_F(NavigationPolicyContainerBuilderTest, DefaultDeliveredPolicies) {
  EXPECT_EQ(
      NavigationPolicyContainerBuilder(
          nullptr, nullptr, kInvalidChildProcessUniqueId, nullptr, nullptr)
          .DeliveredPoliciesForTesting(),
      PolicyContainerPolicies());
}

// Verifies that SetIPAddressSpace sets the address space in the builder's
// delivered policies.
TEST_F(NavigationPolicyContainerBuilderTest, SetIPAddressSpace) {
  NavigationPolicyContainerBuilder builder(
      nullptr, nullptr, kInvalidChildProcessUniqueId, nullptr, nullptr);
  builder.SetIPAddressSpace(network::mojom::IPAddressSpace::kPublic);

  PolicyContainerPolicies expected_policies;
  expected_policies.ip_address_space = network::mojom::IPAddressSpace::kPublic;

  EXPECT_EQ(builder.DeliveredPoliciesForTesting(), expected_policies);
}

// Verifies that SetIsOriginPotentiallyTrustworthy sets the secure context bit
// in the builder's delivered policies.
TEST_F(NavigationPolicyContainerBuilderTest,
       SetIsOriginPotentiallyTrustworthy) {
  NavigationPolicyContainerBuilder builder(
      nullptr, nullptr, kInvalidChildProcessUniqueId, nullptr, nullptr);
  builder.SetIsOriginPotentiallyTrustworthy(true);

  PolicyContainerPolicies expected_policies;
  expected_policies.is_web_secure_context = true;

  EXPECT_EQ(builder.DeliveredPoliciesForTesting(), expected_policies);

  builder.SetIsOriginPotentiallyTrustworthy(false);

  expected_policies.is_web_secure_context = false;
  EXPECT_EQ(builder.DeliveredPoliciesForTesting(), expected_policies);
}

// Verifies that SetCrossOriginOpenerPolicy sets the cross-origin-opener-policy
// in the builder's delivered policies.
TEST_F(NavigationPolicyContainerBuilderTest, SetCrossOriginOpenerPolicy) {
  NavigationPolicyContainerBuilder builder(
      nullptr, nullptr, kInvalidChildProcessUniqueId, nullptr, nullptr);

  network::CrossOriginOpenerPolicy coop;
  coop.value = network::mojom::CrossOriginOpenerPolicyValue::kSameOrigin;
  coop.report_only_value =
      network::mojom::CrossOriginOpenerPolicyValue::kSameOriginAllowPopups;
  coop.reporting_endpoint = "A";
  coop.report_only_reporting_endpoint = "B";

  builder.SetCrossOriginOpenerPolicy(coop);

  PolicyContainerPolicies expected_policies;
  expected_policies.cross_origin_opener_policy = coop;

  EXPECT_EQ(builder.DeliveredPoliciesForTesting(), expected_policies);
}

// Verifies that SetDocumentIsolationPolicy sets the document-isolation-policy
// in the builder's delivered policies.
TEST_F(NavigationPolicyContainerBuilderTest, SetDocumentIsolationPolicy) {
  NavigationPolicyContainerBuilder builder(
      nullptr, nullptr, kInvalidChildProcessUniqueId, nullptr, nullptr);

  network::DocumentIsolationPolicy dip;
  dip.value =
      network::mojom::DocumentIsolationPolicyValue::kIsolateAndRequireCorp;
  dip.report_only_value =
      network::mojom::DocumentIsolationPolicyValue::kIsolateAndCredentialless;
  dip.reporting_endpoint = "A";
  dip.report_only_reporting_endpoint = "B";

  builder.SetDocumentIsolationPolicy(dip);

  PolicyContainerPolicies expected_policies;
  expected_policies.document_isolation_policy = dip;

  EXPECT_EQ(builder.DeliveredPoliciesForTesting(), expected_policies);
}

// Verifies that the default final policies of a builder are
// default-constructed, and are equal to the policies of the builder's policy
// container host.
TEST_F(NavigationPolicyContainerBuilderTest, DefaultFinalPolicies) {
  NavigationPolicyContainerBuilder builder(
      nullptr, nullptr, kInvalidChildProcessUniqueId, nullptr, nullptr);
  builder.ComputePolicies(GURL(), false, network::mojom::WebSandboxFlags::kNone,
                          /*is_credentialless=*/false);

  PolicyContainerPolicies expected_policies;
  EXPECT_EQ(builder.FinalPolicies(), expected_policies);

  scoped_refptr<PolicyContainerHost> cloned_host =
      builder.GetPolicyContainerHost();
  ASSERT_THAT(cloned_host, NotNull());
  EXPECT_EQ(cloned_host->policies(), expected_policies);

  scoped_refptr<PolicyContainerHost> host =
      std::move(builder).TakePolicyContainerHost();
  ASSERT_THAT(host, NotNull());
  EXPECT_EQ(host->policies(), expected_policies);
  ASSERT_THAT(cloned_host, NotNull());
}

// Verifies that when the URL of the document to commit does not have a local
// scheme, then the final policies are copied from the delivered policies.
TEST_F(NavigationPolicyContainerBuilderTest, FinalPoliciesNormalUrl) {
  NavigationPolicyContainerBuilder builder(
      nullptr, nullptr, kInvalidChildProcessUniqueId, nullptr, nullptr);

  builder.SetIPAddressSpace(network::mojom::IPAddressSpace::kPublic);
  builder.AddContentSecurityPolicy(MakeTestCSP());
  PolicyContainerPolicies delivered_policies =
      builder.DeliveredPoliciesForTesting().Clone();
  builder.ComputePolicies(GURL("https://foo.test"), false,
                          network::mojom::WebSandboxFlags::kNone,
                          /*is_credentialless=*/false);

  EXPECT_EQ(builder.FinalPolicies(), delivered_policies);
}

// Verifies the final policies when the URL of the document to commit is
// `about:blank` but there is no initiator.
TEST_F(NavigationPolicyContainerBuilderTest,
       FinalPoliciesAboutBlankWithoutInitiator) {
  NavigationPolicyContainerBuilder builder(
      nullptr, nullptr, kInvalidChildProcessUniqueId, nullptr, nullptr);
  builder.SetIPAddressSpace(network::mojom::IPAddressSpace::kPublic);
  PolicyContainerPolicies delivered_policies =
      builder.DeliveredPoliciesForTesting().Clone();
  builder.ComputePolicies(AboutBlankUrl(), false,
                          network::mojom::WebSandboxFlags::kNone,
                          /*is_credentialless=*/false);

  EXPECT_EQ(builder.FinalPolicies(), delivered_policies);
}

// Verifies the final policies when the URL of the document to commit is
// `about:blank` but there is no initiator, and we have some additional CSPs.
TEST_F(NavigationPolicyContainerBuilderTest,
       FinalPoliciesAboutBlankWithoutInitiatorAdditionalCSP) {
  NavigationPolicyContainerBuilder builder(
      nullptr, nullptr, kInvalidChildProcessUniqueId, nullptr, nullptr);
  builder.SetIPAddressSpace(network::mojom::IPAddressSpace::kPublic);
  builder.AddContentSecurityPolicy(MakeTestCSP());
  PolicyContainerPolicies delivered_policies =
      builder.DeliveredPoliciesForTesting().Clone();
  builder.ComputePolicies(AboutBlankUrl(), false,
                          network::mojom::WebSandboxFlags::kNone,
                          /*is_credentialless=*/false);

  EXPECT_EQ(builder.FinalPolicies(), delivered_policies);
}

// This test verifies the default final policies on error pages.
TEST_F(NavigationPolicyContainerBuilderTest, DefaultFinalPoliciesForErrorPage) {
  NavigationPolicyContainerBuilder builder(
      nullptr, nullptr, kInvalidChildProcessUniqueId, nullptr, nullptr);

  builder.ComputePoliciesForError();

  // Error pages commit with default policies, mostly ignoring the delivered
  // policies and the document's URL.
  EXPECT_EQ(builder.FinalPolicies(), PolicyContainerPolicies());
}

// This test verifies that error pages commit in the same IP address space as
// the underlying page would have, had it not failed to load.
TEST_F(NavigationPolicyContainerBuilderTest, ErrorPageIPAddressSpace) {
  NavigationPolicyContainerBuilder builder(
      nullptr, nullptr, kInvalidChildProcessUniqueId, nullptr, nullptr);

  builder.SetIPAddressSpace(network::mojom::IPAddressSpace::kPublic);
  builder.ComputePoliciesForError();

  PolicyContainerPolicies expected_policies;
  expected_policies.ip_address_space = network::mojom::IPAddressSpace::kPublic;
  EXPECT_EQ(builder.FinalPolicies(), expected_policies);
}

// Variation of: NavigationPolicyContainerBuilderTest.ErrorPageIPAddressSpace
// The decision to commit an error happens after receiving the response.
TEST_F(NavigationPolicyContainerBuilderTest,
       ErrorPageIPAddressSpaceAfterResponse) {
  NavigationPolicyContainerBuilder builder(
      nullptr, nullptr, kInvalidChildProcessUniqueId, nullptr, nullptr);

  builder.SetIPAddressSpace(network::mojom::IPAddressSpace::kPrivate);
  PolicyContainerPolicies expected_policies;
  expected_policies.ip_address_space = network::mojom::IPAddressSpace::kPrivate;

  builder.ComputePolicies(GURL("https://foo.test"), false,
                          network::mojom::WebSandboxFlags::kNone,
                          /*is_credentialless=*/false);
  EXPECT_EQ(builder.FinalPolicies(), expected_policies);

  builder.ComputePoliciesForError();
  EXPECT_EQ(builder.FinalPolicies(), expected_policies);
}

// CSP delivered by the HTTP response are ignored for error document.
TEST_F(NavigationPolicyContainerBuilderTest,
       DeliveredCSPIgnoredForErrorDocument) {
  NavigationPolicyContainerBuilder builder(
      nullptr, nullptr, kInvalidChildProcessUniqueId, nullptr, nullptr);
  builder.AddContentSecurityPolicy(
      network::mojom::ContentSecurityPolicy::New());

  builder.ComputePolicies(GURL("https://foo.test"), false,
                          network::mojom::WebSandboxFlags::kNone,
                          /*is_credentialless=*/false);
  EXPECT_THAT(builder.FinalPolicies().content_security_policies, SizeIs(1));

  builder.ComputePoliciesForError();
  EXPECT_THAT(builder.FinalPolicies().content_security_policies, SizeIs(0));
}

// Verifies that InitiatorPolicies() returns nullptr in the absence of an
// initiator frame token.
TEST_F(NavigationPolicyContainerBuilderTest,
       InitiatorPoliciesWithoutInitiator) {
  EXPECT_THAT(
      NavigationPolicyContainerBuilder(
          nullptr, nullptr, kInvalidChildProcessUniqueId, nullptr, nullptr)
          .InitiatorPolicies(),
      IsNull());
}

// Verifies that ParentPolicies returns nullptr in the absence of a parent.
TEST_F(NavigationPolicyContainerBuilderTest, ParentPoliciesWithoutParent) {
  EXPECT_THAT(
      NavigationPolicyContainerBuilder(
          nullptr, nullptr, kInvalidChildProcessUniqueId, nullptr, nullptr)
          .ParentPolicies(),
      IsNull());
}

// Verifies that ParentPolicies returns a pointer to a copy of the parent's
// policies.
TEST_F(NavigationPolicyContainerBuilderTest, ParentPoliciesWithParent) {
  PolicyContainerPolicies parent_policies = MakeTestPolicies();

  TestRenderFrameHost* parent = contents()->GetPrimaryMainFrame();
  parent->SetPolicyContainerHost(NewHost(parent_policies.Clone()));

  NavigationPolicyContainerBuilder builder(
      parent, nullptr, kInvalidChildProcessUniqueId, nullptr, nullptr);

  EXPECT_THAT(builder.ParentPolicies(), Pointee(Eq(ByRef(parent_policies))));
}

// Verifies that when the the URL of the document to commit is `about:srcdoc`,
// the builder's final policies are copied from the parent.
TEST_F(NavigationPolicyContainerBuilderTest,
       FinalPoliciesAboutSrcdocWithParent) {
  PolicyContainerPolicies parent_policies = MakeTestPolicies();

  TestRenderFrameHost* parent = contents()->GetPrimaryMainFrame();
  parent->SetPolicyContainerHost(NewHost(parent_policies.Clone()));

  NavigationPolicyContainerBuilder builder(
      parent, nullptr, kInvalidChildProcessUniqueId, nullptr, nullptr);
  builder.ComputePolicies(AboutSrcdocUrl(), false,
                          network::mojom::WebSandboxFlags::kNone,
                          /*is_credentialless=*/false);

  EXPECT_EQ(builder.FinalPolicies(), parent_policies);
}

// Verifies that when a document has a potentially-trustworthy origin and no
// parent, then it is a secure context.
TEST_F(NavigationPolicyContainerBuilderTest,
       IsWebSecureContextTrustworthyOriginNoParent) {
  NavigationPolicyContainerBuilder builder(
      nullptr, nullptr, kInvalidChildProcessUniqueId, nullptr, nullptr);

  builder.SetIsOriginPotentiallyTrustworthy(true);

  PolicyContainerPolicies delivered_policies =
      builder.DeliveredPoliciesForTesting().Clone();
  EXPECT_TRUE(delivered_policies.is_web_secure_context);

  builder.ComputePolicies(GURL(), false, network::mojom::WebSandboxFlags::kNone,
                          /*is_credentialless=*/false);

  EXPECT_EQ(builder.FinalPolicies(), delivered_policies);
}

// Verifies that when a document has a non-potentially-trustworthy origin and no
// parent, then it is not a secure context.
TEST_F(NavigationPolicyContainerBuilderTest,
       IsWebSecureContextNonTrustworthyOriginNoParent) {
  NavigationPolicyContainerBuilder builder(
      nullptr, nullptr, kInvalidChildProcessUniqueId, nullptr, nullptr);

  builder.SetIsOriginPotentiallyTrustworthy(false);

  PolicyContainerPolicies delivered_policies =
      builder.DeliveredPoliciesForTesting().Clone();
  EXPECT_FALSE(delivered_policies.is_web_secure_context);

  builder.ComputePolicies(GURL(), false, network::mojom::WebSandboxFlags::kNone,
                          /*is_credentialless=*/false);

  EXPECT_EQ(builder.FinalPolicies(), delivered_policies);
}

// Verifies that when a document has a potentially-trustworthy origin and a
// parent that is not a secure context, then it is not a secure context.
TEST_F(NavigationPolicyContainerBuilderTest,
       IsWebSecureContextTrustworthyOriginNonSecureParent) {
  PolicyContainerPolicies parent_policies = MakeTestPolicies();
  parent_policies.is_web_secure_context = false;

  TestRenderFrameHost* parent = contents()->GetPrimaryMainFrame();
  parent->SetPolicyContainerHost(NewHost(std::move(parent_policies)));

  NavigationPolicyContainerBuilder builder(
      parent, nullptr, kInvalidChildProcessUniqueId, nullptr, nullptr);

  builder.SetIsOriginPotentiallyTrustworthy(true);

  builder.ComputePolicies(GURL("https://foo.test"), false,
                          network::mojom::WebSandboxFlags::kNone,
                          /*is_credentialless=*/false);

  EXPECT_FALSE(builder.FinalPolicies().is_web_secure_context);
}

// Verifies that when a document has a non-potentially-trustworthy origin and a
// parent that is a secure context, then it is not a secure context.
TEST_F(NavigationPolicyContainerBuilderTest,
       IsWebSecureContextNonTrustworthyOriginSecureParent) {
  PolicyContainerPolicies parent_policies = MakeTestPolicies();
  parent_policies.is_web_secure_context = true;

  TestRenderFrameHost* parent = contents()->GetPrimaryMainFrame();
  parent->SetPolicyContainerHost(NewHost(std::move(parent_policies)));

  NavigationPolicyContainerBuilder builder(
      parent, nullptr, kInvalidChildProcessUniqueId, nullptr, nullptr);

  builder.SetIsOriginPotentiallyTrustworthy(false);

  PolicyContainerPolicies delivered_policies =
      builder.DeliveredPoliciesForTesting().Clone();
  EXPECT_FALSE(delivered_policies.is_web_secure_context);

  builder.ComputePolicies(GURL("http://foo.test"), false,
                          network::mojom::WebSandboxFlags::kNone,
                          /*is_credentialless=*/false);

  EXPECT_EQ(builder.FinalPolicies(), delivered_policies);
}

// Verifies that when a document has a potentially-trustworthy origin and a
// parent that is a secure context, then it is a secure context.
TEST_F(NavigationPolicyContainerBuilderTest,
       IsWebSecureContextTrustworthyOriginSecureParent) {
  PolicyContainerPolicies parent_policies = MakeTestPolicies();
  parent_policies.is_web_secure_context = true;

  TestRenderFrameHost* parent = contents()->GetPrimaryMainFrame();
  parent->SetPolicyContainerHost(NewHost(std::move(parent_policies)));

  NavigationPolicyContainerBuilder builder(
      parent, nullptr, kInvalidChildProcessUniqueId, nullptr, nullptr);

  builder.SetIsOriginPotentiallyTrustworthy(true);

  PolicyContainerPolicies delivered_policies =
      builder.DeliveredPoliciesForTesting().Clone();
  EXPECT_TRUE(delivered_policies.is_web_secure_context);

  builder.ComputePolicies(GURL("https://foo.test"), false,
                          network::mojom::WebSandboxFlags::kNone,
                          /*is_credentialless=*/false);

  EXPECT_EQ(builder.FinalPolicies(), delivered_policies);
}

// Verifies that when the the URL of the document to commit is `about:srcdoc`,
// the builder's final policies are copied from the parent, and additional
// delivered policies are merged.
TEST_F(NavigationPolicyContainerBuilderTest,
       FinalPoliciesAboutSrcdocWithParentAndAdditionalCSP) {
  PolicyContainerPolicies parent_policies = MakeTestPolicies();

  TestRenderFrameHost* parent = contents()->GetPrimaryMainFrame();
  parent->SetPolicyContainerHost(NewHost(parent_policies.Clone()));

  NavigationPolicyContainerBuilder builder(
      parent, nullptr, kInvalidChildProcessUniqueId, nullptr, nullptr);

  // Add some CSP.
  network::mojom::ContentSecurityPolicyPtr test_csp = MakeTestCSP();
  builder.AddContentSecurityPolicy(test_csp.Clone());
  builder.ComputePolicies(AboutSrcdocUrl(), false,
                          network::mojom::WebSandboxFlags::kNone,
                          /*is_credentialless=*/false);

  parent_policies.content_security_policies.push_back(std::move(test_csp));
  EXPECT_EQ(builder.FinalPolicies(), parent_policies);
}

// Calling ComputePolicies() twice triggers a DCHECK.
TEST_F(NavigationPolicyContainerBuilderTest, ComputePoliciesTwiceDCHECK) {
  NavigationPolicyContainerBuilder builder(
      nullptr, nullptr, kInvalidChildProcessUniqueId, nullptr, nullptr);
  GURL url("https://foo.test");
  builder.ComputePolicies(url, false, network::mojom::WebSandboxFlags::kNone,
                          /*is_credentialless=*/false);
  EXPECT_DCHECK_DEATH(builder.ComputePolicies(
      url, false, network::mojom::WebSandboxFlags::kNone,
      /*is_credentialless=*/false));
}

// Calling ComputePolicies() followed by ComputePoliciesForError() is supported.
TEST_F(NavigationPolicyContainerBuilderTest, ComputePoliciesThenError) {
  NavigationPolicyContainerBuilder builder(
      nullptr, nullptr, kInvalidChildProcessUniqueId, nullptr, nullptr);
  builder.ComputePolicies(GURL("https://foo.test"), false,
                          network::mojom::WebSandboxFlags::kNone,
                          /*is_credentialless=*/false);
  builder.ComputePoliciesForError();
}

// After ComputePolicies() or ComputePoliciesForError(), the parent
// policies are still accessible.
TEST_F(NavigationPolicyContainerBuilderTest,
       AccessParentAfterComputingPolicies) {
  PolicyContainerPolicies parent_policies = MakeTestPolicies();
  TestRenderFrameHost* parent = contents()->GetPrimaryMainFrame();
  parent->SetPolicyContainerHost(NewHost(parent_policies.Clone()));

  NavigationPolicyContainerBuilder builder(
      parent, nullptr, kInvalidChildProcessUniqueId, nullptr, nullptr);
  EXPECT_THAT(builder.ParentPolicies(), Pointee(Eq(ByRef(parent_policies))));

  builder.ComputePolicies(GURL("https://foo.test"), false,
                          network::mojom::WebSandboxFlags::kNone,
                          /*is_credentialless=*/false);
  EXPECT_THAT(builder.ParentPolicies(), Pointee(Eq(ByRef(parent_policies))));

  builder.ComputePoliciesForError();
  EXPECT_THAT(builder.ParentPolicies(), Pointee(Eq(ByRef(parent_policies))));
}

// Verifies that the parent policies are preserved on
// ResetForCrossDocumentRestart.
TEST_F(NavigationPolicyContainerBuilderTest,
       ResetForCrossDocumentRestartParentPolicies) {
  PolicyContainerPolicies parent_policies = MakeTestPolicies();

  TestRenderFrameHost* parent = contents()->GetPrimaryMainFrame();
  parent->SetPolicyContainerHost(NewHost(parent_policies.Clone()));

  NavigationPolicyContainerBuilder builder(
      parent, nullptr, kInvalidChildProcessUniqueId, nullptr, nullptr);
  builder.ComputePolicies(GURL("https://foo.test"), false,
                          network::mojom::WebSandboxFlags::kNone,
                          /*is_credentialless=*/false);
  EXPECT_EQ(builder.FinalPolicies(), PolicyContainerPolicies());

  builder.ResetForCrossDocumentRestart();
  EXPECT_THAT(builder.ParentPolicies(), Pointee(Eq(ByRef(parent_policies))));

  builder.ComputePolicies(AboutSrcdocUrl(), false,
                          network::mojom::WebSandboxFlags::kNone,
                          /*is_credentialless=*/false);
  EXPECT_EQ(builder.FinalPolicies(), parent_policies);
}

}  // namespace
}  // namespace content
