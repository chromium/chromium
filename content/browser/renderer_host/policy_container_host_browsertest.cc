// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/policy_container_host.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"

namespace content {

namespace {
class PolicyContainerHostBrowserTest : public content::ContentBrowserTest {
 public:
  PolicyContainerHostBrowserTest() {
    // enable policy container
    feature_list_.InitAndEnableFeature(blink::features::kPolicyContainer);
  }

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  RenderFrameHostImpl* current_frame_host() {
    return web_contents()->GetMainFrame();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};
}  // namespace

IN_PROC_BROWSER_TEST_F(PolicyContainerHostBrowserTest,
                       ReferrerPolicyFromHeader) {
  using ReferrerPolicy = network::mojom::ReferrerPolicy;
  const struct {
    const char* headers;
    ReferrerPolicy expected_referrer;
  } kTestCases[] = {
      {"", ReferrerPolicy::kDefault},
      {"Referrer-Policy: no-referrer", ReferrerPolicy::kNever},
      {"Referrer-Policy: no-referrer-when-downgrade",
       ReferrerPolicy::kNoReferrerWhenDowngrade},
      {"Referrer-Policy: origin", ReferrerPolicy::kOrigin},
      {"Referrer-Policy: origin-when-cross-origin",
       ReferrerPolicy::kOriginWhenCrossOrigin},
      {"Referrer-Policy: same-origin", ReferrerPolicy::kSameOrigin},
      {"Referrer-Policy: strict-origin", ReferrerPolicy::kStrictOrigin},
      {"Referrer-Policy: strict-origin-when-cross-origin",
       ReferrerPolicy::kStrictOriginWhenCrossOrigin},
      {"Referrer-Policy: unsafe-url", ReferrerPolicy::kAlways},
  };
  for (const auto& test_case : kTestCases) {
    GURL url = embedded_test_server()->GetURL(
        "a.com", "/set-header?" + std::string(test_case.headers));
    ASSERT_TRUE(NavigateToURL(shell(), url));
    EXPECT_EQ(test_case.expected_referrer,
              current_frame_host()->policy_container_host()->referrer_policy());
  }
}

IN_PROC_BROWSER_TEST_F(PolicyContainerHostBrowserTest,
                       ReferrerPolicyMetaUpdates) {
  GURL page = embedded_test_server()->GetURL("a.com", "/empty.html");
  ASSERT_TRUE(NavigateToURL(shell(), page));
  EXPECT_EQ(network::mojom::ReferrerPolicy::kDefault,
            current_frame_host()->policy_container_host()->referrer_policy());
  ASSERT_TRUE(ExecJs(current_frame_host(),
                     "var meta = document.createElement('meta');"
                     "meta.name = 'referrer';"
                     "meta.content = 'no-referrer';"
                     "document.head.appendChild(meta);"));
  EXPECT_EQ(network::mojom::ReferrerPolicy::kNever,
            current_frame_host()->policy_container_host()->referrer_policy());
}

IN_PROC_BROWSER_TEST_F(PolicyContainerHostBrowserTest, CopiedFromPopupOpener) {
  GURL no_referrer_page = embedded_test_server()->GetURL(
      "a.com", "/set-header?Referrer-Policy: no-referrer");
  GURL origin_referrer_page = embedded_test_server()->GetURL(
      "a.com", "/set-header?Referrer-Policy: origin");
  ASSERT_TRUE(NavigateToURL(shell(), origin_referrer_page));

  {
    // Open a popup. It stays on the initial empty document. The
    // PolicyContainerHost's referrer policy must have been inherited from the
    // opener.
    ShellAddedObserver shell_observer;
    ASSERT_TRUE(ExecJs(current_frame_host(), "window.open();"));
    WebContentsImpl* popup_webcontents = static_cast<WebContentsImpl*>(
        shell_observer.GetShell()->web_contents());
    RenderFrameHostImpl* popup_frame =
        popup_webcontents->GetFrameTree()->root()->current_frame_host();
    EXPECT_EQ(network::mojom::ReferrerPolicy::kOrigin,
              popup_frame->policy_container_host()->referrer_policy());
  }
  {
    // Open a popup that navigates to another document, the referrer policy
    // of the current frame (referrer-policy: origin) should be applied, which
    // induces on the following document the |document.referrer| value of
    // |origin_referrer_page.GetWithEmptyPath()| as tested below.
    // The document loaded in the popup should have
    // referrer-policy: no-referrer, from the response headers.
    ShellAddedObserver shell_observer;
    ASSERT_TRUE(ExecJs(current_frame_host(),
                       JsReplace("window.open($1);", no_referrer_page)));
    WebContentsImpl* popup_webcontents = static_cast<WebContentsImpl*>(
        shell_observer.GetShell()->web_contents());
    WaitForLoadStop(popup_webcontents);
    RenderFrameHostImpl* popup_frame =
        popup_webcontents->GetFrameTree()->root()->current_frame_host();
    EXPECT_EQ(network::mojom::ReferrerPolicy::kNever,
              popup_frame->policy_container_host()->referrer_policy());
    EXPECT_EQ(origin_referrer_page.GetWithEmptyPath(),
              EvalJs(popup_frame, "document.referrer;"));
  }

  // Taint the RFH PolicyContainerHost with (referrer-policy: same-origin).
  // This is not the same as the one in the renderer
  // (referrer-policy: origin).
  // This is not possible in a normal situation, but could occur if the
  // renderer was compromised.
  // This will enable the following test, which verifies that the copied policy
  // comes from the browser.
  static_cast<blink::mojom::PolicyContainerHost*>(
      current_frame_host()->policy_container_host())
      ->SetReferrerPolicy(network::mojom::ReferrerPolicy::kSameOrigin);
  // Repeat the two previous tests with the tainted Policy ContainerHost:
  {
    // Open a popup on a document that inherits the Policy Container and verify
    // its Policy Container value. The policy container must be copied within
    // the browser process thus the tainted value should be obtained
    // (referrer-policy: same-origin).
    ShellAddedObserver shell_observer;
    ASSERT_TRUE(ExecJs(current_frame_host(), "window.open('about:blank');"));
    WebContentsImpl* popup_webcontents = static_cast<WebContentsImpl*>(
        shell_observer.GetShell()->web_contents());
    RenderFrameHostImpl* popup_frame =
        popup_webcontents->GetFrameTree()->root()->current_frame_host();
    EXPECT_EQ(network::mojom::ReferrerPolicy::kSameOrigin,
              popup_frame->policy_container_host()->referrer_policy());
  }
  {
    // Open a popup that navigates to another document, the initial empty
    // document should inherit the tainted policy container within the browser
    // process (referrer-policy: same-origin), and the blink value
    // (referrer-policy: origin) should be ignored.
    ShellAddedObserver shell_observer;
    ASSERT_TRUE(ExecJs(current_frame_host(),
                       JsReplace("window.open($1);", no_referrer_page)));
    WebContentsImpl* popup_webcontents = static_cast<WebContentsImpl*>(
        shell_observer.GetShell()->web_contents());
    WaitForLoadStop(popup_webcontents);
    RenderFrameHostImpl* popup_frame =
        popup_webcontents->GetFrameTree()->root()->current_frame_host();
    EXPECT_EQ(network::mojom::ReferrerPolicy::kNever,
              popup_frame->policy_container_host()->referrer_policy());

    // The referrer policy used to determine the referrer comes from blink,
    // resulting in the origin referrer policy being applied instead of the
    // SameOrigin one from browser.
    EXPECT_EQ(origin_referrer_page.GetWithEmptyPath(),
              EvalJs(popup_frame, "document.referrer;"));
  }
}

IN_PROC_BROWSER_TEST_F(PolicyContainerHostBrowserTest, CopiedFromParent) {
  GURL no_referrer_page = embedded_test_server()->GetURL(
      "a.com", "/set-header?Referrer-Policy: no-referrer");
  GURL origin_referrer_page = embedded_test_server()->GetURL(
      "a.com", "/set-header?Referrer-Policy: origin");
  ASSERT_TRUE(NavigateToURL(shell(), origin_referrer_page));

  std::string create_srcdoc_iframe_script(
      "var iframe = document.createElement('iframe');"
      "iframe.srcdoc = '<p>hello world!</p>';"
      "document.body.appendChild(iframe);");
  {
    // Add an iframe and verify its Policy Container value.
    ASSERT_TRUE(ExecJs(current_frame_host(), create_srcdoc_iframe_script));
    ASSERT_EQ(1U, current_frame_host()->child_count());
    WaitForLoadStop(web_contents());
    FrameTreeNode* iframe_node =
        current_frame_host()->child_at(current_frame_host()->child_count() - 1);
    EXPECT_TRUE(iframe_node->current_url().IsAboutSrcdoc());
    EXPECT_EQ(network::mojom::ReferrerPolicy::kOrigin,
              iframe_node->current_frame_host()
                  ->policy_container_host()
                  ->referrer_policy());

    // Navigate the document and verify the policy container is updated and the
    // initiator referrer policy (referrer-policy: origin) is used which
    // induces on the following document the |document.referrer| value of
    // |origin_referrer_page.GetWithEmptyPath()| as tested below.
    ASSERT_TRUE(
        ExecJs(iframe_node->current_frame_host(),
               JsReplace("document.location.href = $1", no_referrer_page)));
    WaitForLoadStop(web_contents());
    EXPECT_EQ(network::mojom::ReferrerPolicy::kNever,
              iframe_node->current_frame_host()
                  ->policy_container_host()
                  ->referrer_policy());
    EXPECT_EQ(origin_referrer_page.GetWithEmptyPath(),
              EvalJs(iframe_node->current_frame_host(), "document.referrer;"));
  }

  // Taint the RFH Policy container with a value that will not be the same in
  // the renderer (kOrigin). This is not possible in a normal situation, but
  // could occur if the renderer was compromised.
  // This will enable the following test, which verifies that the copied policy
  // comes from the browser.
  static_cast<blink::mojom::PolicyContainerHost*>(
      current_frame_host()->policy_container_host())
      ->SetReferrerPolicy(network::mojom::ReferrerPolicy::kSameOrigin);
  // Repeat the previous test with the tainted policy container:
  {
    // Add an iframe and verify its Policy Container value.
    ASSERT_TRUE(ExecJs(current_frame_host(), create_srcdoc_iframe_script));
    // Check that the iframe was properly added.
    ASSERT_EQ(2U, current_frame_host()->child_count());
    WaitForLoadStop(web_contents());
    FrameTreeNode* iframe_node =
        current_frame_host()->child_at(current_frame_host()->child_count() - 1);
    EXPECT_TRUE(iframe_node->current_url().IsAboutSrcdoc());
    EXPECT_EQ(network::mojom::ReferrerPolicy::kSameOrigin,
              iframe_node->current_frame_host()
                  ->policy_container_host()
                  ->referrer_policy());

    ASSERT_TRUE(
        ExecJs(iframe_node->current_frame_host(),
               JsReplace("document.location.href = $1", no_referrer_page)));
    WaitForLoadStop(web_contents());
    // The referrer policy is the one of the newly loaded document:
    // no_referrer_page.
    EXPECT_EQ(network::mojom::ReferrerPolicy::kNever,
              iframe_node->current_frame_host()
                  ->policy_container_host()
                  ->referrer_policy());
    // The referrer is determined using the policy container inherited when the
    // frame was created and navigated to the srcdoc. The tainted value, within
    // the browser process, was inherited during the navigation, and is expected
    // to be referrer-policy: same-origin, leading to the full url being used as
    // a referrer.
    EXPECT_EQ(origin_referrer_page,
              EvalJs(iframe_node->current_frame_host(), "document.referrer;"));
  }
}

IN_PROC_BROWSER_TEST_F(PolicyContainerHostBrowserTest,
                       CopiedFromParentCreatedBySibling) {
  GURL origin_referrer_page = embedded_test_server()->GetURL(
      "a.com", "/set-header?Referrer-Policy: origin");
  GURL same_origin_referrer_page = embedded_test_server()->GetURL(
      "a.com", "/set-header?Referrer-Policy: same-origin");
  GURL no_referrer_page = embedded_test_server()->GetURL(
      "a.com", "/set-header?Referrer-Policy: no-referrer");
  ASSERT_TRUE(NavigateToURL(shell(), origin_referrer_page));
  {
    // Add an iframe and verify its Policy Container value.
    ASSERT_TRUE(
        ExecJs(current_frame_host(),
               JsReplace("var iframe = document.createElement('iframe');"
                         "iframe.src = $1;"
                         "iframe.name = 'first';"
                         "document.body.appendChild(iframe);",
                         same_origin_referrer_page)));
    ASSERT_EQ(1U, current_frame_host()->child_count());
    WaitForLoadStop(web_contents());
    FrameTreeNode* first_iframe_node =
        current_frame_host()->child_at(current_frame_host()->child_count() - 1);
    EXPECT_EQ(network::mojom::ReferrerPolicy::kSameOrigin,
              first_iframe_node->current_frame_host()
                  ->policy_container_host()
                  ->referrer_policy());

    // From the iframe, create a sibling.
    ASSERT_TRUE(ExecJs(first_iframe_node->current_frame_host(),
                       "var iframe = document.createElement('iframe');"
                       "iframe.srcdoc = 'hello world';"
                       "iframe.name = 'second';"
                       "parent.document.body.appendChild(iframe);"));
    ASSERT_EQ(2U, current_frame_host()->child_count());
    FrameTreeNode* second_iframe_node =
        current_frame_host()->child_at(current_frame_host()->child_count() - 1);
    // The policies should be inherited from the creator of the iframe, which is
    // the node to which the iframe is attached to, i.e. the parent, here the
    // main document, which has referrer-policy: origin.
    EXPECT_EQ(network::mojom::ReferrerPolicy::kOrigin,
              second_iframe_node->current_frame_host()
                  ->policy_container_host()
                  ->referrer_policy())
        << "Sibling policy container inherited from parent.";

    // The second iframe navigates its sibling, the first iframe, the inherited
    // referrer policy of second (from the main frame) is applied.
    // The document loaded in first is a local scheme, and as such inherits from
    // the initiator, the second iframe.
    ASSERT_TRUE(ExecJs(second_iframe_node->current_frame_host(),
                       "window.open('about:blank', 'first');"));
    WaitForLoadStop(web_contents());
    EXPECT_EQ(network::mojom::ReferrerPolicy::kOrigin,
              first_iframe_node->current_frame_host()
                  ->policy_container_host()
                  ->referrer_policy());
    EXPECT_EQ(
        origin_referrer_page.GetWithEmptyPath(),
        EvalJs(first_iframe_node->current_frame_host(), "document.referrer;"))
        << "Referrer obtained from applying same-origin referrer policy.";

    // Navigate the second iframe, verify the referrer policy is updated from
    // the response. The document.referrer comes from the application of the
    // inherited referrer policy of the main frame
    // (referrer-policy: same-origin).
    ASSERT_TRUE(
        ExecJs(second_iframe_node->current_frame_host(),
               JsReplace("document.location.href = $1", no_referrer_page)));
    WaitForLoadStop(web_contents());
    EXPECT_EQ(network::mojom::ReferrerPolicy::kNever,
              second_iframe_node->current_frame_host()
                  ->policy_container_host()
                  ->referrer_policy());
    EXPECT_EQ(
        origin_referrer_page.GetWithEmptyPath(),
        EvalJs(second_iframe_node->current_frame_host(), "document.referrer;"))
        << "Referrer obtained from applying same-origin referrer policy.";
  }
}

IN_PROC_BROWSER_TEST_F(PolicyContainerHostBrowserTest, HistoryForMainFrame) {
  NavigationControllerImpl& controller = web_contents()->GetController();
  GURL no_referrer_url(embedded_test_server()->GetURL(
      "/set-header?Referrer-Policy: no-referrer"));
  GURL always_referrer_url(embedded_test_server()->GetURL(
      "/set-header?Referrer-Policy: unsafe-url"));

  // Navigate to a page setting Referrer-Policy: no-referrer.
  ASSERT_TRUE(NavigateToURL(shell(), no_referrer_url));
  ASSERT_EQ(1, controller.GetEntryCount());
  ASSERT_EQ(network::mojom::ReferrerPolicy::kNever,
            current_frame_host()->policy_container_host()->referrer_policy());

  // Now navigate to a local scheme.
  ASSERT_TRUE(ExecJs(current_frame_host(), "window.location = 'about:blank'"));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  ASSERT_EQ(network::mojom::ReferrerPolicy::kNever,
            current_frame_host()->policy_container_host()->referrer_policy());

  ASSERT_EQ(2, controller.GetEntryCount());
  NavigationEntryImpl* entry2 = controller.GetEntryAtIndex(1);

  // Check that RendererDidNavigateToNewEntry stored the correct policy
  // container in the FrameNavigationEntry.
  ASSERT_EQ(network::mojom::ReferrerPolicy::kNever,
            entry2->root_node()
                ->frame_entry->policy_container_policies()
                ->referrer_policy);

  // Same document navigation.
  ASSERT_TRUE(ExecJs(current_frame_host(), "window.location.href = '#top'"));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  ASSERT_EQ(network::mojom::ReferrerPolicy::kNever,
            current_frame_host()->policy_container_host()->referrer_policy());

  ASSERT_EQ(3, controller.GetEntryCount());
  NavigationEntryImpl* entry3 = controller.GetEntryAtIndex(2);
  ASSERT_EQ(network::mojom::ReferrerPolicy::kNever,
            entry3->root_node()
                ->frame_entry->policy_container_policies()
                ->referrer_policy);

  // Navigate to a third page.
  ASSERT_TRUE(NavigateToURL(shell(), always_referrer_url));
  ASSERT_EQ(4, controller.GetEntryCount());
  ASSERT_EQ(network::mojom::ReferrerPolicy::kAlways,
            current_frame_host()->policy_container_host()->referrer_policy());

  // Go back to "about:blank#top"
  controller.GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  // The correct referrer policy should be restored from history.
  ASSERT_EQ(network::mojom::ReferrerPolicy::kNever,
            current_frame_host()->policy_container_host()->referrer_policy());

  // The function RendererDidNavigateToExistingEntry should not have changed
  // anything.
  ASSERT_EQ(network::mojom::ReferrerPolicy::kNever,
            entry3->root_node()
                ->frame_entry->policy_container_policies()
                ->referrer_policy);

  // Go back to "about:blank".
  controller.GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  // The correct referrer policy should be restored from history.
  ASSERT_EQ(network::mojom::ReferrerPolicy::kNever,
            current_frame_host()->policy_container_host()->referrer_policy());

  // The function RendererDidNavigateToExistingEntry should not have changed
  // anything.
  ASSERT_EQ(network::mojom::ReferrerPolicy::kNever,
            entry2->root_node()
                ->frame_entry->policy_container_policies()
                ->referrer_policy);

  // Same URL navigation, which gets converted to a reload.
  ASSERT_TRUE(NavigateFrameToURL(current_frame_host()->frame_tree_node(),
                                 GURL("about:blank")));
  ASSERT_EQ(network::mojom::ReferrerPolicy::kNever,
            current_frame_host()->policy_container_host()->referrer_policy());

  ASSERT_EQ(4, controller.GetEntryCount());

  // Check that after RendererDidNavigateToExistingEntry the policy container in
  // the FrameNavigationEntry is still correct.
  ASSERT_EQ(network::mojom::ReferrerPolicy::kNever,
            entry2->root_node()
                ->frame_entry->policy_container_policies()
                ->referrer_policy);
}

IN_PROC_BROWSER_TEST_F(PolicyContainerHostBrowserTest, HistoryForChildFrame) {
  NavigationControllerImpl& controller = web_contents()->GetController();
  GURL always_referrer_url(embedded_test_server()->GetURL(
      "/set-header?Referrer-Policy: unsafe-url"));
  GURL strict_origin_when_cross_origin_referrer_url(
      embedded_test_server()->GetURL(
          "/set-header?Referrer-Policy: strict-origin-when-cross-origin"));

  GURL main_url(embedded_test_server()->GetURL("/page_with_blank_iframe.html"));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  ASSERT_TRUE(NavigateToURL(shell(), main_url));

  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* child = root->child_at(0);
  ASSERT_NE(nullptr, child);
  ASSERT_EQ(1, controller.GetEntryCount());

  // The child has default referrer policy (same as the parent).
  ASSERT_EQ(
      network::mojom::ReferrerPolicy::kDefault,
      child->current_frame_host()->policy_container_host()->referrer_policy());
  NavigationEntryImpl* entry1 = controller.GetEntryAtIndex(0);
  ASSERT_EQ(network::mojom::ReferrerPolicy::kDefault,
            entry1->GetFrameEntry(child)
                ->policy_container_policies()
                ->referrer_policy);

  // Change referrer policy of the main frame.
  ASSERT_TRUE(ExecJs(current_frame_host(),
                     "var meta = document.createElement('meta');"
                     "meta.name = 'referrer';"
                     "meta.content = 'same-origin';"
                     "document.head.appendChild(meta);"));
  ASSERT_EQ(network::mojom::ReferrerPolicy::kSameOrigin,
            current_frame_host()->policy_container_host()->referrer_policy());

  // 1) Navigate the child frame to a local scheme url.
  ASSERT_TRUE(ExecJs(current_frame_host(),
                     "window.open('data:text/html,Hello', 'test_iframe');"));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  // The new document inherits from the navigation initiator.
  ASSERT_EQ(
      network::mojom::ReferrerPolicy::kSameOrigin,
      child->current_frame_host()->policy_container_host()->referrer_policy());

  // The new page replaces the initial about:blank page in the subframe, so no
  // new navigation entry is created.
  ASSERT_EQ(1, controller.GetEntryCount());

  // The policy container of the FrameNavigationEntry should have been
  // updated. Test that the function RendererDidNavigateAutoSubframe updates the
  // FrameNavigationEntry properly.
  ASSERT_EQ(network::mojom::ReferrerPolicy::kSameOrigin,
            entry1->GetFrameEntry(child)
                ->policy_container_policies()
                ->referrer_policy);

  // 2) Same document navigation.
  ASSERT_TRUE(
      ExecJs(child->current_frame_host(), "window.location.href = '#top';"));
  ASSERT_TRUE(WaitForLoadStop(web_contents()));

  // The policy container has not changed.
  EXPECT_EQ(
      network::mojom::ReferrerPolicy::kSameOrigin,
      child->current_frame_host()->policy_container_host()->referrer_policy());
  ASSERT_EQ(2, controller.GetEntryCount());
  NavigationEntryImpl* entry2 = controller.GetEntryAtIndex(1);
  ASSERT_EQ(network::mojom::ReferrerPolicy::kSameOrigin,
            entry2->GetFrameEntry(child)
                ->policy_container_policies()
                ->referrer_policy);

  // 3) Navigate the child frame to a network scheme url.
  ASSERT_TRUE(NavigateFrameToURL(child, always_referrer_url));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  ASSERT_EQ(3, controller.GetEntryCount());
  ASSERT_EQ(
      network::mojom::ReferrerPolicy::kAlways,
      child->current_frame_host()->policy_container_host()->referrer_policy());

  // 4) Navigate the child frame to another local scheme url.
  ASSERT_TRUE(ExecJs(child->current_frame_host(),
                     "window.location = 'data:text/html,Hello2';"));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  // The new document inherits from the navigation initiator.
  ASSERT_EQ(
      network::mojom::ReferrerPolicy::kAlways,
      child->current_frame_host()->policy_container_host()->referrer_policy());

  // Now test that the function RendererDidNavigateNewSubframe properly stored
  // the policy container in the FrameNavigationEntry.
  ASSERT_EQ(4, controller.GetEntryCount());
  NavigationEntryImpl* entry4 = controller.GetEntryAtIndex(3);
  ASSERT_EQ(network::mojom::ReferrerPolicy::kAlways,
            entry4->GetFrameEntry(child)
                ->policy_container_policies()
                ->referrer_policy);

  // 5) Navigate the child frame to another network scheme url.
  ASSERT_TRUE(
      NavigateFrameToURL(child, strict_origin_when_cross_origin_referrer_url));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  ASSERT_EQ(5, controller.GetEntryCount());
  ASSERT_EQ(
      network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin,
      child->current_frame_host()->policy_container_host()->referrer_policy());

  // 6) Navigate the main frame cross-document to destroy the subframes.
  GURL foo_url(embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/simple_page_1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), foo_url));

  // 7) Navigate all the way back and check that we properly reload the policy
  // container from history.
  controller.GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  controller.GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  child = web_contents()->GetFrameTree()->root()->child_at(0);
  ASSERT_NE(nullptr, child);

  // The correct referrer policy should be restored from history.
  ASSERT_EQ(
      network::mojom::ReferrerPolicy::kAlways,
      child->current_frame_host()->policy_container_host()->referrer_policy());

  // The frame entry should not have changed.
  ASSERT_EQ(network::mojom::ReferrerPolicy::kAlways,
            entry4->GetFrameEntry(child)
                ->policy_container_policies()
                ->referrer_policy);

  controller.GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  controller.GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  // The correct referrer policy should be restored from history.
  ASSERT_EQ(
      network::mojom::ReferrerPolicy::kSameOrigin,
      child->current_frame_host()->policy_container_host()->referrer_policy());

  // The frame entry should not have changed.
  ASSERT_EQ(network::mojom::ReferrerPolicy::kSameOrigin,
            entry2->GetFrameEntry(child)
                ->policy_container_policies()
                ->referrer_policy);

  controller.GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  // The correct referrer policy should be restored from history.
  ASSERT_EQ(
      network::mojom::ReferrerPolicy::kSameOrigin,
      child->current_frame_host()->policy_container_host()->referrer_policy());

  // The frame entry should not have changed.
  ASSERT_EQ(network::mojom::ReferrerPolicy::kSameOrigin,
            entry1->GetFrameEntry(child)
                ->policy_container_policies()
                ->referrer_policy);
}

// Check that the FrameNavigationEntry for the initial empty document is
// correctly populated, both for main frames and for subframes.
IN_PROC_BROWSER_TEST_F(PolicyContainerHostBrowserTest,
                       HistoryForInitialEmptyDocument) {
  GURL origin_referrer_page = embedded_test_server()->GetURL(
      "a.com", "/set-header?Referrer-Policy: origin");
  ASSERT_TRUE(NavigateToURL(shell(), origin_referrer_page));

  {
    // Open a subframe
    ASSERT_TRUE(ExecJs(current_frame_host(),
                       "var frame = document.createElement('iframe');"
                       "frame.name = 'test_iframe';"
                       "document.body.appendChild(frame);"));

    FrameTreeNode* root = web_contents()->GetFrameTree()->root();
    ASSERT_EQ(1U, root->child_count());
    FrameTreeNode* child = root->child_at(0);
    ASSERT_NE(nullptr, child);
    // The child inherits from the parent.
    ASSERT_EQ(network::mojom::ReferrerPolicy::kOrigin,
              child->current_frame_host()
                  ->policy_container_host()
                  ->referrer_policy());

    // The right policy is stored in the FrameNavigationEntry.
    NavigationControllerImpl& controller = web_contents()->GetController();
    ASSERT_EQ(1, controller.GetEntryCount());
    NavigationEntryImpl* entry1 = controller.GetEntryAtIndex(0);
    ASSERT_EQ(network::mojom::ReferrerPolicy::kOrigin,
              entry1->GetFrameEntry(child)
                  ->policy_container_policies()
                  ->referrer_policy);
  }

  {
    // Open a popup.
    ShellAddedObserver shell_observer;
    ASSERT_TRUE(ExecJs(current_frame_host(), "window.open('about:blank');"));
    WebContentsImpl* popup_webcontents = static_cast<WebContentsImpl*>(
        shell_observer.GetShell()->web_contents());
    RenderFrameHostImpl* popup_frame =
        popup_webcontents->GetFrameTree()->root()->current_frame_host();

    // The popup inherits from the creator.
    EXPECT_EQ(network::mojom::ReferrerPolicy::kOrigin,
              popup_frame->policy_container_host()->referrer_policy());

    // The right policy is stored in the FrameNavigationEntry.
    NavigationEntryImpl* entry1 =
        popup_webcontents->GetController().GetEntryAtIndex(0);
    ASSERT_EQ(network::mojom::ReferrerPolicy::kOrigin,
              entry1->root_node()
                  ->frame_entry->policy_container_policies()
                  ->referrer_policy);
  }
}

// This test ensures that the document policies what we store in the
// FrameNavigationEntry are a snapshot of the document policies at
// CommitNavigation time, and do not include updates triggered by Blink.
IN_PROC_BROWSER_TEST_F(PolicyContainerHostBrowserTest,
                       BlinkModificationsDoNotAffectPolicyContainer) {
  NavigationControllerImpl& controller = web_contents()->GetController();

  GURL always_referrer_url(embedded_test_server()->GetURL(
      "/set-header?Referrer-Policy: unsafe-url"));
  ASSERT_TRUE(NavigateToURL(shell(), always_referrer_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Create child frame
  ASSERT_TRUE(ExecJs(current_frame_host(), R"(
    let frame = document.createElement('iframe');
    let content = '<head><meta name="referrer" content="no-referrer"></head>';
    frame.src = 'data:text/html,' + content;
    document.body.appendChild(frame);
  )"));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  FrameTreeNode* child = root->child_at(0);
  ASSERT_NE(nullptr, child);

  // Blink should have parsed Referrer-Policy from the meta tag and the
  // information should have propagated to the PolicyContainerHost.
  ASSERT_EQ(
      network::mojom::ReferrerPolicy::kNever,
      child->current_frame_host()->policy_container_host()->referrer_policy());
  ASSERT_EQ(1, controller.GetEntryCount());
  NavigationEntryImpl* entry1 = controller.GetEntryAtIndex(0);

  // Policies should be stored in the FrameNavigationEntry before any
  // modification coming from blink.
  ASSERT_EQ(network::mojom::ReferrerPolicy::kAlways,
            entry1->GetFrameEntry(child)
                ->policy_container_policies()
                ->referrer_policy);
}

// This is a regression test. A document navigates a remote subframe away from
// about:blank. The new FrameNavigationEntry used to wrongly inherit a
// policies from the about:blank document.
IN_PROC_BROWSER_TEST_F(PolicyContainerHostBrowserTest,
                       PoliciesNotInheritedForRemoteNonLocalScheme) {
  NavigationControllerImpl& controller = web_contents()->GetController();

  // Create a main frame with referrer policy 'unsafe-url'.
  GURL always_referrer_url(embedded_test_server()->GetURL(
      "/set-header?Referrer-Policy: unsafe-url"));
  ASSERT_TRUE(NavigateToURL(shell(), always_referrer_url));

  // Create a cross-origin child frame with referrer policy 'no-referrer'.
  GURL cross_origin_no_referrer_url(embedded_test_server()->GetURL(
      "b.com", "/set-header?Referrer-Policy: no-referrer"));
  EXPECT_TRUE(
      ExecJs(current_frame_host(), JsReplace(R"(
    child_frame = document.createElement('iframe');
    child_frame.src = $1;
    document.body.appendChild(child_frame);
  )",
                                             cross_origin_no_referrer_url)));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  FrameTreeNode* child = current_frame_host()->child_at(0);
  EXPECT_NE(nullptr, child);

  // Navigate the child frame to "about:blank", but keep it in a remote frame
  // w.r.t. the main frame.
  EXPECT_TRUE(ExecJs(child->current_frame_host(), R"(
    location.href = "about:blank";
  )"));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  EXPECT_EQ(
      network::mojom::ReferrerPolicy::kNever,
      child->current_frame_host()->policy_container_host()->referrer_policy());
  EXPECT_EQ(2, controller.GetEntryCount());
  NavigationEntryImpl* entry1 = controller.GetEntryAtIndex(1);

  EXPECT_EQ(network::mojom::ReferrerPolicy::kNever,
            entry1->GetFrameEntry(child)
                ->policy_container_policies()
                ->referrer_policy);

  // Now navigate the child from the main frame to another empty url.
  GURL empty_url(embedded_test_server()->GetURL("c.com", "/empty.html"));
  EXPECT_TRUE(ExecJs(current_frame_host(), JsReplace(R"(
    child_frame.src = $1;
  )",
                                                     empty_url)));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  // The child should have default referrer policy.
  EXPECT_EQ(
      network::mojom::ReferrerPolicy::kDefault,
      child->current_frame_host()->policy_container_host()->referrer_policy());
}

// The following tests shows the behavior of the policy container during the
// early commit following a crashed frame: If an embedder pauses the navigation
// that causing the early commit, they can then execute javascript in the
// committed frame and blink's PolicyContainer would not be connected to
// browser's PolicyContainerHost. This has limited impact in practice.
IN_PROC_BROWSER_TEST_F(PolicyContainerHostBrowserTest,
                       CheckRendererPolicyContainerAccessesAfterCrash) {
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());

  std::string add_referrer_script = R"(
    new Promise(async resolve => {
      // Make sure the DOM is ready before modifying <header>.
      if (document.readyState !== "complete")
        await new Promise(r => addEventListener("DOMContentLoaded", r));

      // Add <meta name="referrer" content=$1>
      let meta = document.createElement("meta");
      meta.name = "referrer";
      meta.content= $1;
      document.head.append(meta);
      resolve(true);
    })
  )";

  // Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));

  // The referrer policy is initially default.
  EXPECT_EQ(network::mojom::ReferrerPolicy::kDefault,
            current_frame_host()->policy_container_host()->referrer_policy());

  // Deliver a new referrer policy in the renderer with
  // <meta name="referrer" content="none">.
  EXPECT_EQ(true, EvalJs(current_frame_host(),
                         content::JsReplace(add_referrer_script, "none")));
  // The policy is propagated to the browser by policy container.
  EXPECT_EQ(network::mojom::ReferrerPolicy::kNever,
            current_frame_host()->policy_container_host()->referrer_policy());

  // Crash the renderer.
  RenderProcessHost* renderer_process = current_frame_host()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      renderer_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  renderer_process->Shutdown(0);
  crash_observer.Wait();

  // Start navigation to B, but don't commit yet.
  TestNavigationManager manager(web_contents(), url_b);
  shell()->LoadURL(url_b);
  EXPECT_TRUE(manager.WaitForRequestStart());

  NavigationRequest* navigation_request =
      NavigationRequest::From(manager.GetNavigationHandle());
  if (ShouldSkipEarlyCommitPendingForCrashedFrame()) {
    EXPECT_EQ(navigation_request->associated_site_instance_type(),
              NavigationRequest::AssociatedSiteInstanceType::SPECULATIVE);
  } else {
    // Policy container is properly initialized in the early committed
    // render frame host.
    EXPECT_TRUE(current_frame_host()->policy_container_host());
    EXPECT_EQ(navigation_request->associated_site_instance_type(),
              NavigationRequest::AssociatedSiteInstanceType::CURRENT);

    // The policy is copied from the previous RFH following the crash.
    EXPECT_EQ(network::mojom::ReferrerPolicy::kNever,
              current_frame_host()->policy_container_host()->referrer_policy());

    // Deliver a new referrer policy in the renderer with
    // <meta name="referrer" content="origin">. Using "origin" to differ from
    // previous "none" and "default".

    EXPECT_EQ(true, EvalJs(current_frame_host(),
                           content::JsReplace(add_referrer_script, "origin")));
    // The previous referrer policy is not propagated to the browser since
    // blink's policy container is not linked with browser's policy container
    // host. Never (a.k.a. "None") is copied from the previous RFH.
    EXPECT_EQ(network::mojom::ReferrerPolicy::kNever,
              current_frame_host()->policy_container_host()->referrer_policy());
  }

  // Let the navigation finish.
  manager.WaitForNavigationFinished();

  EXPECT_EQ(url_b, web_contents()->GetMainFrame()->GetLastCommittedURL());

  // The referrer policy is initialized to default during the navigation (no
  // referrer-policy header in the response).
  EXPECT_EQ(network::mojom::ReferrerPolicy::kDefault,
            current_frame_host()->policy_container_host()->referrer_policy());

  // Deliver a new referrer policy in the renderer with
  // <meta name="referrer" content="strict-origin">. Using "strict-origin" to
  // differ again from the previous uses.
  EXPECT_EQ(true,
            EvalJs(current_frame_host(),
                   content::JsReplace(add_referrer_script, "strict-origin")));
  // This time renderer's policy container properly propagates the referrer
  // policy to the browser.
  EXPECT_EQ(network::mojom::ReferrerPolicy::kStrictOrigin,
            current_frame_host()->policy_container_host()->referrer_policy());
}

// Test that chrome error pages are loaded with a default policy container and
// don't inherit policies.
IN_PROC_BROWSER_TEST_F(PolicyContainerHostBrowserTest, FailedNavigation) {
  // Perform a navigation setting referrer policy.
  GURL url = embedded_test_server()->GetURL(
      "/set-header?Referrer-Policy: no-referrer");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_EQ(network::mojom::ReferrerPolicy::kNever,
            current_frame_host()->policy_container_host()->referrer_policy());

  // Now open a popup with an unreachable url.
  GURL error_url = embedded_test_server()->GetURL("/close-socket");
  ShellAddedObserver shell_observer;
  EXPECT_TRUE(
      ExecJs(current_frame_host(), JsReplace("window.open($1)", error_url)));
  WebContentsImpl* popup_webcontents =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());
  WaitForLoadStop(popup_webcontents);

  NavigationEntry* entry =
      popup_webcontents->GetController().GetLastCommittedEntry();
  EXPECT_EQ(PAGE_TYPE_ERROR, entry->GetPageType());
  RenderFrameHostImpl* popup_frame =
      popup_webcontents->GetFrameTree()->root()->current_frame_host();
  EXPECT_EQ(network::mojom::ReferrerPolicy::kDefault,
            popup_frame->policy_container_host()->referrer_policy());
}

}  // namespace content
