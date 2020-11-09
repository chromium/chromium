// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/policy_container_host.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
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

}  // namespace content
