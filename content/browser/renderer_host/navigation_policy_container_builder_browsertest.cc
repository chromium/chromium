// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigation_policy_container_builder.h"

#include "base/strings/stringprintf.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/navigation_state_keep_alive.h"
#include "content/browser/renderer_host/policy_container_host.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/ip_address_space_overrides_test_utils.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_constants.h"

namespace content {
namespace {

using ::testing::ByRef;
using ::testing::Eq;
using ::testing::IsNull;
using ::testing::Pointee;

const PolicyContainerPolicies& GetPolicies(RenderFrameHostImpl* frame) {
  return frame->policy_container_host()->policies();
}

GURL AboutBlankUrl() {
  return GURL(url::kAboutBlankURL);
}

GURL AboutSrcdocUrl() {
  return GURL(url::kAboutSrcdocURL);
}

network::mojom::ContentSecurityPolicyPtr MakeTestCSP() {
  auto csp = network::mojom::ContentSecurityPolicy::New();
  csp->header = network::mojom::ContentSecurityPolicyHeader::New();
  csp->header->header_value = "some-directive some-value";
  return csp;
}

}  // namespace

// See also the unit tests for NavigationPolicyContainerBuilder, which exercise
// simpler parts of the API. We use browser tests to exercise behavior in the
// presence of navigation history in particular.
class NavigationPolicyContainerBuilderBrowserTest : public ContentBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    public_test_server_.AddDefaultHandlers(GetTestDataFilePath());
    ASSERT_TRUE(public_test_server_.Start());
    network::AddPublicIpAddressSpaceOverrideToCommandLine(public_test_server_,
                                                          *command_line);
  }
  explicit NavigationPolicyContainerBuilderBrowserTest() {
    CHECK(embedded_test_server()->Start());
  }

  // Returns a pointer to the current root RenderFrameHostImpl.
  RenderFrameHostImpl* root_frame_host() {
    return static_cast<RenderFrameHostImpl*>(
        shell()->web_contents()->GetPrimaryMainFrame());
  }

  // Helper to access the StoragePartition of the root RenderFrameHostImpl.
  StoragePartitionImpl* root_storage_partition() {
    return root_frame_host()->GetStoragePartition();
  }

  // Returns the URL of a page in the loopback address space.
  GURL LoopbackUrl() const { return embedded_test_server()->GetURL("/echo"); }

  // Returns the URL of a page in the public address space.
  GURL PublicUrl() const {
    return public_test_server_.GetURL(
        "/set-header?Content-Security-Policy: script-src 'self'");
  }

  // Returns the FrameNavigationEntry for the root node in the last committed
  // navigation entry.
  // Returns nullptr if there is no committed navigation entry.
  FrameNavigationEntry* GetLastCommittedFrameNavigationEntry() {
    auto* entry = static_cast<NavigationEntryImpl*>(
        shell()->web_contents()->GetController().GetLastCommittedEntry());
    if (!entry) {
      return nullptr;
    }

    return entry->root_node()->frame_entry.get();
  }

  net::EmbeddedTestServer& public_test_server() { return public_test_server_; }

 private:
  net::EmbeddedTestServer public_test_server_;
};

// Verifies that HistoryPolicies() returns nullptr in the absence of a history
// entry.
//
// Even though this could be a unit test, we define this here so as to keep all
// tests of HistoryPolicies() in the same place.
IN_PROC_BROWSER_TEST_F(NavigationPolicyContainerBuilderBrowserTest,
                       HistoryPoliciesWithoutEntry) {
  EXPECT_THAT(
      NavigationPolicyContainerBuilder(nullptr, nullptr).HistoryPolicies(),
      IsNull());
}

// Verifies that HistoryPolicies() returns non-null during history navigation.
IN_PROC_BROWSER_TEST_F(NavigationPolicyContainerBuilderBrowserTest,
                       HistoryPoliciesForNetworkScheme) {
  // Navigate to a document with a network scheme. Its history entry should have
  // its policies initialized from the network response.
  EXPECT_TRUE(NavigateToURL(shell()->web_contents(), LoopbackUrl()));

  const PolicyContainerPolicies& root_policies = GetPolicies(root_frame_host());
  EXPECT_EQ(root_policies.ip_address_space,
            network::mojom::IPAddressSpace::kLoopback);

  NavigationPolicyContainerBuilder builder(
      nullptr, GetLastCommittedFrameNavigationEntry());

  EXPECT_THAT(builder.HistoryPolicies(), Pointee(Eq(ByRef(root_policies))));
}

// Verifies that SetFrameNavigationEntry() copies the policies during history
// navigation, if any, or resets those policies when given nullptr.
IN_PROC_BROWSER_TEST_F(NavigationPolicyContainerBuilderBrowserTest,
                       HistoryPoliciesForBlankUrl) {
  // First navigate to a local scheme with non-default policies. To do that, we
  // first navigate to a document with a public address space, then have that
  // document navigate itself to `about:blank`. The final blank document
  // inherits its policies from the first document, and stores them in its
  // frame navigation entry for restoring later.
  EXPECT_TRUE(NavigateToURL(shell()->web_contents(), PublicUrl()));
  EXPECT_TRUE(NavigateToURLFromRenderer(root_frame_host(), AboutBlankUrl()));
  RenderFrameHostImpl* root = root_frame_host();

  const PolicyContainerPolicies& root_policies = GetPolicies(root);
  EXPECT_EQ(root_policies.ip_address_space,
            network::mojom::IPAddressSpace::kPublic);

  // Now that we have set up a navigation entry with non-default policies, we
  // can run the test itself.
  NavigationPolicyContainerBuilder builder(
      nullptr, GetLastCommittedFrameNavigationEntry());

  EXPECT_THAT(builder.HistoryPolicies(), Pointee(Eq(ByRef(root_policies))));
}

// Verifies that HistoryPolicies() returns non-null even when associated with
// a non-current FrameNavigationEntry.
IN_PROC_BROWSER_TEST_F(NavigationPolicyContainerBuilderBrowserTest,
                       HistoryPoliciesForNonCurentEntry) {
  // Navigate to a document with a network scheme. Its history entry should have
  // its policies initialized from the network response.
  EXPECT_TRUE(NavigateToURL(shell()->web_contents(), LoopbackUrl()));

  const PolicyContainerPolicies& root_policies = GetPolicies(root_frame_host());
  EXPECT_EQ(root_policies.ip_address_space,
            network::mojom::IPAddressSpace::kLoopback);

  FrameNavigationEntry* entry = GetLastCommittedFrameNavigationEntry();
  NavigationPolicyContainerBuilder builder(nullptr, entry);

  // Verify the state is correct before navigating away.
  EXPECT_THAT(builder.HistoryPolicies(), Pointee(Eq(ByRef(root_policies))));

  EXPECT_TRUE(NavigateToURL(shell()->web_contents(), PublicUrl()));

  // Now that the FrameNavigationEntry is non-current, verify that it still has
  // the builder.
  EXPECT_NE(entry, GetLastCommittedFrameNavigationEntry());
  NavigationPolicyContainerBuilder builder2(nullptr, entry);
  EXPECT_THAT(builder2.HistoryPolicies(), Pointee(Eq(ByRef(root_policies))));
}

// Verifies that CreatePolicyContainerForBlink() returns a policy container
// containing a copy of the builder's final policies.
IN_PROC_BROWSER_TEST_F(NavigationPolicyContainerBuilderBrowserTest,
                       CreatePolicyContainerForBlink) {
  NavigationPolicyContainerBuilder builder(nullptr, nullptr);
  builder.SetIPAddressSpace(network::mojom::IPAddressSpace::kPublic);

  MockNavigationHandle navigation_handle(GURL(), nullptr);
  builder.ComputePolicies(&navigation_handle, /*initiator_policies=*/nullptr,
                          false, network::mojom::WebSandboxFlags::kNone,
                          /*is_credentialless=*/false,
                          /*is_secure_context_root=*/false);

  // This must be called on a task runner, hence the need for this test to be
  // a browser test and not a simple unit test.
  blink::mojom::PolicyContainerPtr container =
      builder.CreatePolicyContainerForBlink();
  ASSERT_FALSE(container.is_null());
  ASSERT_FALSE(container->policies.is_null());

  const blink::mojom::PolicyContainerPolicies& policies = *container->policies;
  EXPECT_EQ(policies.referrer_policy, builder.FinalPolicies().referrer_policy);
}

// Verifies that when the URL of the document to commit is `about:blank`, and
// when a navigation entry with policies is given, then the navigation
// initiator's policies are ignored in favor of the policies from the entry.
IN_PROC_BROWSER_TEST_F(NavigationPolicyContainerBuilderBrowserTest,
                       FinalPoliciesAboutBlankWithInitiatorAndHistory) {

  // First navigate to a local scheme with non-default policies. To do that, we
  // first navigate to a document with a public address space, then have that
  // document navigate itself to `about:blank`. The final blank document
  // inherits its policies from the first document, and stores them in its frame
  // navigation entry for restoring later.
  EXPECT_TRUE(NavigateToURL(shell()->web_contents(), PublicUrl()));
  EXPECT_TRUE(NavigateToURLFromRenderer(root_frame_host(), AboutBlankUrl()));

  PolicyContainerPolicies initiator_policies;
  initiator_policies.ip_address_space =
      network::mojom::IPAddressSpace::kLoopback;

  NavigationPolicyContainerBuilder builder(
      nullptr, GetLastCommittedFrameNavigationEntry());

  PolicyContainerPolicies history_policies = builder.HistoryPolicies()->Clone();

  // Deliver a Content Security Policy via `AddContentSecurityPolicy`. This
  // policy should not be incorporated in the final policies, since the builder
  // is using the history policies.
  builder.AddContentSecurityPolicy(MakeTestCSP());

  MockNavigationHandle navigation_handle(AboutBlankUrl(), nullptr);
  builder.ComputePolicies(&navigation_handle, &initiator_policies, false,
                          network::mojom::WebSandboxFlags::kNone,
                          /*is_credentialless=*/false,
                          /*is_secure_context_root=*/false);

  EXPECT_EQ(builder.FinalPolicies(), history_policies);
}

// Verifies that when the URL of the document to commit is `about:srcdoc`, and
// when a navigation entry with policies is given, then the parent's policies
// are ignored in favor of the policies from the entry.
IN_PROC_BROWSER_TEST_F(NavigationPolicyContainerBuilderBrowserTest,
                       FinalPoliciesAboutSrcDocWithParentAndHistory) {
  // First navigate to a local scheme with non-default policies. To do that, we
  // first navigate to a document with a public address space, then have that
  // document navigate itself to `about:blank`. The final blank document
  // inherits its policies from the first document, and stores them in its
  // frame navigation entry for restoring later.
  EXPECT_TRUE(NavigateToURL(shell()->web_contents(), PublicUrl()));
  EXPECT_TRUE(NavigateToURLFromRenderer(root_frame_host(), AboutBlankUrl()));
  RenderFrameHostImpl* root = root_frame_host();

  // Embed another frame with different policies, to use as the "parent".
  std::string script_template = R"(
    new Promise((resolve) => {
      const iframe = document.createElement("iframe");
      iframe.src = $1;
      iframe.onload = () => { resolve(true); }
      document.body.appendChild(iframe);
    })
  )";
  EXPECT_EQ(true, EvalJs(root, JsReplace(script_template, LoopbackUrl())));

  RenderFrameHostImpl* parent = root->child_at(0)->current_frame_host();
  NavigationPolicyContainerBuilder builder(
      parent, GetLastCommittedFrameNavigationEntry());

  EXPECT_NE(*builder.HistoryPolicies(), *builder.ParentPolicies());

  PolicyContainerPolicies history_policies = builder.HistoryPolicies()->Clone();

  // Deliver a Content Security Policy via `AddContentSecurityPolicy`. This
  // policy should not be incorporated in the final policies, since the builder
  // is using the history policies.
  builder.AddContentSecurityPolicy(MakeTestCSP());

  MockNavigationHandle navigation_handle(AboutSrcdocUrl(), nullptr);
  builder.ComputePolicies(&navigation_handle, /*initiator_policies=*/nullptr,
                          false, network::mojom::WebSandboxFlags::kNone,
                          /*is_credentialless=*/false,
                          /*is_secure_context_root=*/false);

  EXPECT_EQ(builder.FinalPolicies(), history_policies);
}

// Verifies that history policies are ignored in the case of error pages.
IN_PROC_BROWSER_TEST_F(NavigationPolicyContainerBuilderBrowserTest,
                       FinalPoliciesErrorPageWithHistory) {
  // First navigate to a local scheme with non-default policies. To do that, we
  // first navigate to a document with a public address space, then have that
  // document navigate itself to `about:blank`. The final blank document
  // inherits its policies from the first document, and stores them in its
  // frame navigation entry for restoring later.
  EXPECT_TRUE(NavigateToURL(shell()->web_contents(), PublicUrl()));
  EXPECT_TRUE(NavigateToURLFromRenderer(root_frame_host(), AboutBlankUrl()));

  NavigationPolicyContainerBuilder builder(
      nullptr, GetLastCommittedFrameNavigationEntry());

  builder.ComputePoliciesForError();

  // Error pages commit with default policies, ignoring the history policies.
  EXPECT_EQ(builder.FinalPolicies(), PolicyContainerPolicies());
}

// After |ComputePolicies()| or |ComputePoliciesForError()|, the history
// policies are still accessible.
IN_PROC_BROWSER_TEST_F(NavigationPolicyContainerBuilderBrowserTest,
                       AccessHistoryAfterComputingPolicies) {
  // First navigate to a local scheme with non-default policies. To do that, we
  // first navigate to a document with a public address space, then have that
  // document navigate itself to `about:blank`. The final blank document
  // inherits its policies from the first document, and stores them in its
  // frame navigation entry for restoring later.
  EXPECT_TRUE(NavigateToURL(shell()->web_contents(), PublicUrl()));
  EXPECT_TRUE(NavigateToURLFromRenderer(root_frame_host(), AboutBlankUrl()));

  NavigationPolicyContainerBuilder builder(
      nullptr, GetLastCommittedFrameNavigationEntry());

  PolicyContainerPolicies history_policies = builder.HistoryPolicies()->Clone();

  MockNavigationHandle navigation_handle(AboutBlankUrl(), nullptr);
  builder.ComputePolicies(&navigation_handle, /*initiator_policies=*/nullptr,
                          false, network::mojom::WebSandboxFlags::kNone,
                          /*is_credentialless=*/false,
                          /*is_secure_context_root=*/false);
  EXPECT_THAT(builder.HistoryPolicies(), Pointee(Eq(ByRef(history_policies))));

  builder.ComputePoliciesForError();
  EXPECT_THAT(builder.HistoryPolicies(), Pointee(Eq(ByRef(history_policies))));
}

// Verifies that history policies from a reused navigation entry aren't used for
// non-local navigations.
IN_PROC_BROWSER_TEST_F(NavigationPolicyContainerBuilderBrowserTest,
                       NoHistoryPoliciesInheritedForNonLocalUrlsOnReload) {
  // Navigate to some non-local url first.
  WebContents* tab = shell()->web_contents();
  EXPECT_TRUE(NavigateToURL(tab, PublicUrl()));
  EXPECT_EQ(PublicUrl(), tab->GetLastCommittedURL());

  // Navigate by doing a client-redirect (through renderer-initiated
  // replacement) to about:blank to put policies to navigation entry.
  TestNavigationObserver navigation_observer(shell()->web_contents());
  EXPECT_TRUE(
      ExecJs(root_frame_host(), "window.location.replace('about:blank');"));
  navigation_observer.WaitForNavigationFinished();
  EXPECT_EQ(AboutBlankUrl(), tab->GetLastCommittedURL());

  // Now reload to original url and ensure that history entry policies stored
  // earlier aren't applied to non-local URL (no DCHECK triggered).
  TestNavigationObserver observer(tab, /*expected_number_of_navigations=*/1);
  tab->GetController().LoadOriginalRequestURL();
  observer.Wait();  // No DCHECK expected.
  EXPECT_EQ(PublicUrl(), tab->GetLastCommittedURL());
}

// Verifies that history policies from a restored navigation entry are
// overwritten if the policies have changed.
IN_PROC_BROWSER_TEST_F(NavigationPolicyContainerBuilderBrowserTest,
                       NoHistoryPoliciesInheritedForNetworkUrlsOnBack) {
  DisableBackForwardCacheForTesting(shell()->web_contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);

  // Start by navigating to a network URL with one policy.
  WebContents* tab = shell()->web_contents();
  EXPECT_TRUE(NavigateToURL(tab, PublicUrl()));
  EXPECT_EQ(PublicUrl(), tab->GetLastCommittedURL());

  // Use replaceState() to change to a same-origin URL with a different policy
  // (which happens to be no policy for /echo).
  GURL public_url_without_policies = public_test_server().GetURL("/echo");
  TestNavigationObserver navigation_observer(shell()->web_contents());
  EXPECT_TRUE(
      ExecJs(root_frame_host(),
             base::StringPrintf("window.history.replaceState('', null, '%s');",
                                public_url_without_policies.spec().data())));
  navigation_observer.WaitForNavigationFinished();
  EXPECT_EQ(public_url_without_policies, tab->GetLastCommittedURL());

  // Because we changed the url via replaceState rather than actually
  // navigating to public_url_without_policies, it shouldn't have modified any
  // policies.
  EXPECT_FALSE(
      GetPolicies(root_frame_host()).content_security_policies.empty());
  FrameNavigationEntry* entry = GetLastCommittedFrameNavigationEntry();
  EXPECT_FALSE(
      entry->policy_container_policies()->content_security_policies.empty());

  // Navigate away, then back to public_url_without_policies.
  EXPECT_TRUE(NavigateToURL(shell()->web_contents(), AboutBlankUrl()));
  EXPECT_TRUE(HistoryGoBack(shell()->web_contents()));

  // This time we actually loaded public_url_without_policies. We should use
  // its (non-existent) content security policies and updated the policies on
  // the FrameNavigationEntry, rather than restoring the previous set from the
  // FNE.
  EXPECT_EQ(entry, GetLastCommittedFrameNavigationEntry());
  EXPECT_TRUE(
      entry->policy_container_policies()->content_security_policies.empty());
  EXPECT_TRUE(GetPolicies(root_frame_host()).content_security_policies.empty());
}

// Verifies that the history policies are preserved on
// ResetForCrossDocumentRestart.
IN_PROC_BROWSER_TEST_F(NavigationPolicyContainerBuilderBrowserTest,
                       ResetForCrossDocumentRestartHistoryPolicies) {
  RenderFrameHostImpl* root = root_frame_host();

  // First navigate to a local scheme with non-default policies. To do that, we
  // first navigate to a document with a public address space, then have that
  // document navigate itself to `about:blank`. The final blank document
  // inherits its policies from the first document, and stores them in its frame
  // navigation entry for restoring later.
  EXPECT_TRUE(NavigateToURL(shell()->web_contents(), PublicUrl()));
  EXPECT_TRUE(NavigateToURLFromRenderer(root, AboutBlankUrl()));

  NavigationPolicyContainerBuilder builder(
      nullptr, GetLastCommittedFrameNavigationEntry());

  PolicyContainerPolicies history_policies = builder.HistoryPolicies()->Clone();

  MockNavigationHandle navigation_handle(GURL("http://foo.test"), nullptr);
  builder.ComputePolicies(&navigation_handle, /*initiator_policies=*/nullptr,
                          false, network::mojom::WebSandboxFlags::kNone,
                          /*is_credentialless=*/false,
                          /*is_secure_context_root=*/false);

  EXPECT_EQ(builder.FinalPolicies(), PolicyContainerPolicies());

  builder.ResetForCrossDocumentRestart();
  EXPECT_THAT(builder.HistoryPolicies(), Pointee(Eq(ByRef(history_policies))));

  navigation_handle.set_url(AboutBlankUrl());
  builder.ComputePolicies(&navigation_handle, /*initiator_policies=*/nullptr,
                          false, network::mojom::WebSandboxFlags::kNone,
                          /*is_credentialless=*/false,
                          /*is_secure_context_root=*/false);

  EXPECT_EQ(builder.FinalPolicies(), history_policies);
}

// Verifies that when the URL of the document to commit is `about:blank`, the
// builder's final policies are copied from the initiator.
IN_PROC_BROWSER_TEST_F(NavigationPolicyContainerBuilderBrowserTest,
                       FinalPoliciesAboutBlankWithInitiator) {
  RenderFrameHostImpl* initiator = root_frame_host();
  const PolicyContainerPolicies* initiator_policies =
      initiator->policy_container_host()->policies_ptr();

  NavigationPolicyContainerBuilder builder(nullptr, nullptr);
  MockNavigationHandle navigation_handle(AboutBlankUrl(), nullptr);
  builder.ComputePolicies(&navigation_handle, initiator_policies, false,
                          network::mojom::WebSandboxFlags::kNone,
                          /*is_credentialless=*/false,
                          /*is_secure_context_root=*/false);

  EXPECT_EQ(builder.FinalPolicies(), *initiator_policies);
}

// Verifies that when the URL of the document to commit is `blob:.*`, the
// builder's final policies are copied from the initiator.
IN_PROC_BROWSER_TEST_F(NavigationPolicyContainerBuilderBrowserTest,
                       FinalPoliciesBlobWithInitiator) {
  RenderFrameHostImpl* initiator = root_frame_host();
  const PolicyContainerPolicies* initiator_policies =
      initiator->policy_container_host()->policies_ptr();

  NavigationPolicyContainerBuilder builder(nullptr, nullptr);

  MockNavigationHandle navigation_handle(
      GURL("blob:https://example.com/016ece86-b7f9-4b07-88c2-a0e36b7f1dd6"),
      nullptr);
  builder.ComputePolicies(&navigation_handle, initiator_policies, false,
                          network::mojom::WebSandboxFlags::kNone,
                          /*is_credentialless=*/false,
                          /*is_secure_context_root=*/false);

  EXPECT_EQ(builder.FinalPolicies(), *initiator_policies);
}

// Verifies that when the URL of the document to commit is `about:blank`, the
// builder's final policies are copied from the initiator, and additional
// delivered policies are merged.
IN_PROC_BROWSER_TEST_F(NavigationPolicyContainerBuilderBrowserTest,
                       FinalPoliciesAboutBlankWithInitiatorAndAdditionalCSP) {
  RenderFrameHostImpl* initiator = root_frame_host();
  PolicyContainerPolicies initiator_policies =
      initiator->policy_container_host()->policies().Clone();

  NavigationPolicyContainerBuilder builder(nullptr, nullptr);

  // Add some CSP.
  network::mojom::ContentSecurityPolicyPtr test_csp = MakeTestCSP();
  builder.AddContentSecurityPolicy(test_csp.Clone());
  MockNavigationHandle navigation_handle(AboutBlankUrl(), nullptr);
  builder.ComputePolicies(&navigation_handle, &initiator_policies, false,
                          network::mojom::WebSandboxFlags::kNone,
                          /*is_credentialless=*/false,
                          /*is_secure_context_root=*/false);

  initiator_policies.content_security_policies.push_back(std::move(test_csp));
  EXPECT_EQ(builder.FinalPolicies(), initiator_policies);
}

}  // namespace content
