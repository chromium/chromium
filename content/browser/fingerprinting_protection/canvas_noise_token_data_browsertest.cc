// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/fingerprinting_protection/canvas_noise_token_data.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "components/fingerprinting_protection_filter/interventions/common/interventions_features.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_client.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/common/render_frame_test_helper.mojom.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_response_headers.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

using ::testing::Optional;

class CanvasNoiseTestContentBrowserClient
    : public ContentBrowserTestContentBrowserClient {
 public:
  explicit CanvasNoiseTestContentBrowserClient(bool should_enable)
      : should_enable_(should_enable) {}
  ~CanvasNoiseTestContentBrowserClient() override = default;

 private:
  bool ShouldEnableCanvasNoise(content::BrowserContext* browser_context,
                               const GURL& origin) override {
    return should_enable_;
  }

  bool should_enable_;
};

class CanvasNoiseTokenDataDisabledBrowserTest
    : public content::ContentBrowserTest {
 public:
  CanvasNoiseTokenDataDisabledBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(
        fingerprinting_protection_interventions::features::kCanvasNoise);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    content_browser_client_ =
        std::make_unique<CanvasNoiseTestContentBrowserClient>(false);
  }
  void TearDown() override { scoped_feature_list_.Reset(); }

 private:
  std::unique_ptr<CanvasNoiseTestContentBrowserClient> content_browser_client_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(CanvasNoiseTokenDataDisabledBrowserTest,
                       DisabledCanvasNoiseNullOptCanvasNoiseToken) {
  GURL frame_url = embedded_test_server()->GetURL("/defaultresponse");
  ASSERT_TRUE(NavigateToURL(shell(), frame_url));
  std::optional<blink::NoiseToken> committed_token =
      GetCanvasNoiseTokenForPage(shell()->web_contents()->GetPrimaryPage());
  EXPECT_FALSE(committed_token.has_value());
}

class CanvasNoiseTokenDataBrowserTest : public content::ContentBrowserTest {
 public:
  CanvasNoiseTokenDataBrowserTest() = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
    content_browser_client_ =
        std::make_unique<CanvasNoiseTestContentBrowserClient>(true);
  }
  void TearDown() override { scoped_feature_list_.Reset(); }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  // Returns the canvas noise token from the RenderFrameHost's corresponding
  // blink::WebView in the renderer process.
  std::optional<blink::NoiseToken> GetRendererToken(ToRenderFrameHost adapter) {
    mojo::Remote<mojom::RenderFrameTestHelper> remote;
    adapter.render_frame_host()->GetRemoteInterfaces()->GetInterface(
        remote.BindNewPipeAndPassReceiver());
    std::optional<blink::NoiseToken> token_from_renderer = std::nullopt;
    base::RunLoop run_loop;
    remote->GetCanvasNoiseToken(base::BindLambdaForTesting(
        [&](const std::optional<blink::NoiseToken> token) {
          token_from_renderer = token;
          run_loop.Quit();
        }));
    run_loop.Run();
    return token_from_renderer;
  }

 private:
  std::unique_ptr<CanvasNoiseTestContentBrowserClient> content_browser_client_;
  base::test::ScopedFeatureList scoped_feature_list_{
      fingerprinting_protection_interventions::features::kCanvasNoise};
};

// TODO(https://crbug.com/436909582): Add a test to ensure the RenderView gets
// the expected token as soon as the CreateView gets called, prior to the
// PageBroadcast call.

IN_PROC_BROWSER_TEST_F(CanvasNoiseTokenDataBrowserTest,
                       DifferentBrowserContextDifferCanvasNoiseTokens) {
  blink::NoiseToken normal_token = CanvasNoiseTokenData::GetToken(
      CreateBrowser()->web_contents()->GetBrowserContext(),
      url::Origin::Create(GURL("https://example.test")));
  blink::NoiseToken incognito_token = CanvasNoiseTokenData::GetToken(
      CreateOffTheRecordBrowser()->web_contents()->GetBrowserContext(),
      url::Origin::Create(GURL("https://example.test")));

  EXPECT_NE(normal_token.Value(), 0UL);
  EXPECT_NE(incognito_token.Value(), 0UL);
  EXPECT_NE(normal_token, incognito_token);
}

IN_PROC_BROWSER_TEST_F(CanvasNoiseTokenDataBrowserTest,
                       PageTokenPropagatesSameTokenOnSameOrigins) {
  GURL same_url(embedded_test_server()->GetURL("a.com", "/defaultresponse"));

  EXPECT_TRUE(NavigateToURL(shell(), same_url));

  RenderFrameHostImpl* main_frame = web_contents()->GetPrimaryMainFrame();
  ASSERT_TRUE(main_frame);

  std::optional<blink::NoiseToken> first_committed_token =
      GetCanvasNoiseTokenForPage(main_frame->GetPage());
  url::Origin first_origin = main_frame->GetLastCommittedOrigin();
  EXPECT_EQ(first_committed_token, GetRendererToken(main_frame));

  EXPECT_TRUE(NavigateToURLFromRenderer(main_frame, same_url));
  main_frame = web_contents()->GetPrimaryMainFrame();

  std::optional<blink::NoiseToken> second_committed_token =
      GetCanvasNoiseTokenForPage(main_frame->GetPage());
  url::Origin second_origin = main_frame->GetLastCommittedOrigin();
  EXPECT_EQ(second_committed_token, GetRendererToken(main_frame));

  EXPECT_EQ(first_origin, second_origin);
  EXPECT_THAT(first_committed_token, Optional(second_committed_token.value()));
  EXPECT_THAT(first_committed_token,
              Optional(CanvasNoiseTokenData::GetToken(
                  web_contents()->GetBrowserContext(), first_origin)));
}

IN_PROC_BROWSER_TEST_F(
    CanvasNoiseTokenDataBrowserTest,
    PerPageTokensPropagateOnMainFrameDifferOnDifferentOrigins) {
  GURL first_url(embedded_test_server()->GetURL("a.com", "/defaultresponse"));
  GURL second_url(embedded_test_server()->GetURL("b.com", "/defaultresponse"));

  EXPECT_TRUE(NavigateToURL(shell(), first_url));

  RenderFrameHostImpl* main_frame = web_contents()->GetPrimaryMainFrame();
  ASSERT_TRUE(main_frame);

  std::optional<blink::NoiseToken> first_committed_token =
      GetCanvasNoiseTokenForPage(main_frame->GetPage());
  url::Origin first_origin = main_frame->GetLastCommittedOrigin();
  EXPECT_EQ(first_committed_token, GetRendererToken(main_frame));

  EXPECT_TRUE(NavigateToURLFromRenderer(main_frame, second_url));
  main_frame = web_contents()->GetPrimaryMainFrame();

  std::optional<blink::NoiseToken> second_committed_token =
      GetCanvasNoiseTokenForPage(main_frame->GetPage());
  url::Origin second_origin = main_frame->GetLastCommittedOrigin();
  EXPECT_EQ(second_committed_token, GetRendererToken(main_frame));

  EXPECT_NE(first_origin, second_origin);
  EXPECT_NE(first_committed_token, second_committed_token);
  EXPECT_THAT(first_committed_token,
              Optional(CanvasNoiseTokenData::GetToken(
                  web_contents()->GetBrowserContext(), first_origin)));
  EXPECT_THAT(second_committed_token,
              Optional(CanvasNoiseTokenData::GetToken(
                  web_contents()->GetBrowserContext(), second_origin)));
}

IN_PROC_BROWSER_TEST_F(CanvasNoiseTokenDataBrowserTest,
                       OpaqueOriginsCreateUniqueCanvasNoiseTokens) {
  GURL about_blank(url::kAboutBlankURL);

  ASSERT_TRUE(NavigateToURL(shell(), about_blank));

  RenderFrameHostImpl* main_frame = web_contents()->GetPrimaryMainFrame();
  url::Origin opaque_origin = main_frame->GetLastCommittedOrigin();

  std::optional<blink::NoiseToken> first_committed_token =
      GetCanvasNoiseTokenForPage(main_frame->GetPage());
  EXPECT_EQ(first_committed_token, GetRendererToken(main_frame));

  EXPECT_TRUE(first_committed_token.has_value());
  EXPECT_TRUE(opaque_origin.opaque());

  // Token should be regenerated even if the same opaque origin is used again.
  EXPECT_NE(first_committed_token,
            CanvasNoiseTokenData::GetToken(web_contents()->GetBrowserContext(),
                                           opaque_origin));

  ASSERT_TRUE(NavigateToURL(shell(), about_blank));

  main_frame = web_contents()->GetPrimaryMainFrame();
  url::Origin opaque_origin_second = main_frame->GetLastCommittedOrigin();
  std::optional<blink::NoiseToken> second_committed_token =
      GetCanvasNoiseTokenForPage(main_frame->GetPage());
  EXPECT_EQ(second_committed_token, GetRendererToken(main_frame));

  EXPECT_TRUE(opaque_origin_second.opaque());
  EXPECT_TRUE(second_committed_token.has_value());

  // Different opaque origins will generate different tokens.
  EXPECT_NE(opaque_origin, opaque_origin_second);
  EXPECT_NE(second_committed_token, first_committed_token);
}

IN_PROC_BROWSER_TEST_F(CanvasNoiseTokenDataBrowserTest,
                       CanvasNoiseToken_PagePropagationWithSubframeNavigation) {
  GURL url_ab(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());

  EXPECT_TRUE(NavigateToURL(shell(), url_ab));

  RenderFrameHostImpl* rfh_a = web_contents()->GetPrimaryMainFrame();
  EXPECT_NE(GetCanvasNoiseTokenForPage(rfh_a->GetPage()), std::nullopt);

  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();
  EXPECT_NE(GetCanvasNoiseTokenForPage(rfh_b->GetPage()), std::nullopt);

  EXPECT_TRUE(rfh_b->GetPage().IsPrimary());
  EXPECT_TRUE(rfh_a->IsInPrimaryMainFrame());
  EXPECT_FALSE(rfh_b->IsInPrimaryMainFrame());
  EXPECT_EQ(&rfh_a->GetPage(), &rfh_b->GetPage());

  std::optional<blink::NoiseToken> token_a =
      GetCanvasNoiseTokenForPage(rfh_a->GetPage());
  std::optional<blink::NoiseToken> token_b =
      GetCanvasNoiseTokenForPage(rfh_b->GetPage());

  EXPECT_EQ(token_a, token_b);
  EXPECT_EQ(token_a, GetRendererToken(rfh_a));
  EXPECT_EQ(token_b, GetRendererToken(rfh_b));
}

IN_PROC_BROWSER_TEST_F(
    CanvasNoiseTokenDataBrowserTest,
    CanvasNoiseToken_CrossSiteNavigationDifferentOriginDiffersToken) {
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  DisableBackForwardCacheForTesting(shell()->web_contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);

  GURL url_c(embedded_test_server()->GetURL("c.com", "/defaultresponse"));
  GURL url_d(embedded_test_server()->GetURL("d.com", "/defaultresponse"));

  EXPECT_TRUE(NavigateToURL(shell(), url_c));

  RenderFrameHostImpl* main_frame = web_contents()->GetPrimaryMainFrame();
  ASSERT_TRUE(main_frame);

  std::optional<blink::NoiseToken> first_nav_token =
      GetCanvasNoiseTokenForPage(main_frame->GetPage());

  EXPECT_NE(first_nav_token, std::nullopt);

  RenderFrameHostWrapper main_frame_wrapper(main_frame);
  EXPECT_FALSE(main_frame_wrapper.IsDestroyed());

  // Perform a cross-site navigation in the main frame.
  EXPECT_TRUE(NavigateToURLFromRenderer(main_frame, url_d));
  EXPECT_TRUE(main_frame_wrapper.WaitUntilRenderFrameDeleted());
  EXPECT_TRUE(main_frame_wrapper.IsDestroyed());

  // Use the next main frame from WebContents.
  std::optional<blink::NoiseToken> second_nav_token =
      GetCanvasNoiseTokenForPage(
          web_contents()->GetPrimaryMainFrame()->GetPage());

  EXPECT_NE(second_nav_token, std::nullopt);
  EXPECT_NE(first_nav_token, second_nav_token);
}

IN_PROC_BROWSER_TEST_F(CanvasNoiseTokenDataBrowserTest,
                       ChildFrameCrossSiteNavigationDifferentOriginSameToken) {
  GURL url_a_with_child(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/defaultresponse"));
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());

  EXPECT_TRUE(NavigateToURL(shell(), url_a_with_child));

  RenderFrameHostImpl* main_frame = web_contents()->GetPrimaryMainFrame();
  std::optional<blink::NoiseToken> main_frame_nav_token =
      GetCanvasNoiseTokenForPage(main_frame->GetPage());
  EXPECT_NE(main_frame_nav_token, std::nullopt);
  EXPECT_EQ(main_frame_nav_token, GetRendererToken(main_frame));

  RenderFrameHostImpl* child_frame =
      static_cast<RenderFrameHostImpl*>(ChildFrameAt(shell(), 0));
  ASSERT_TRUE(child_frame);

  // Noise token post-cross site navigation.
  std::optional<blink::NoiseToken> child_frame_nav_token_a =
      GetCanvasNoiseTokenForPage(child_frame->GetPage());
  EXPECT_NE(child_frame_nav_token_a, std::nullopt);
  EXPECT_EQ(child_frame_nav_token_a, GetRendererToken(child_frame));
  EXPECT_EQ(main_frame_nav_token, child_frame_nav_token_a);

  RenderFrameHostWrapper child_frame_wrapper(child_frame);
  ASSERT_FALSE(child_frame_wrapper.IsDestroyed());

  // Perform a cross-site navigation in the child frame.
  EXPECT_TRUE(NavigateToURLFromRenderer(child_frame, url_b));

  ASSERT_TRUE(child_frame_wrapper.WaitUntilRenderFrameDeleted());
  EXPECT_TRUE(child_frame_wrapper.IsDestroyed());

  child_frame = static_cast<RenderFrameHostImpl*>(ChildFrameAt(shell(), 0));
  // Noise token post-cross site navigation.
  std::optional<blink::NoiseToken> child_frame_nav_token_b =
      GetCanvasNoiseTokenForPage(child_frame->GetPage());
  EXPECT_NE(child_frame_nav_token_b, std::nullopt);
  EXPECT_EQ(child_frame_nav_token_b, GetRendererToken(child_frame));

  EXPECT_EQ(main_frame_nav_token, child_frame_nav_token_b);
  EXPECT_EQ(child_frame_nav_token_a, child_frame_nav_token_b);
}

// Given that A = a.com and B = b.com and the following frame tree structure:
//
// A1 -> B(A2) where A1 opens B via popup, and A2 is iframed inside B.
//
// This test ensures that A2 actually receives B's canvas noise token instead of
// A1's. Upon navigating to B from A1, A2's remote frame will be created prior
// to B's commit, as such, it's important that A2 receives B's token via the
// UpdateCanvasNoiseToken PageBroadcast method instead of inheriting A1's token.
IN_PROC_BROWSER_TEST_F(CanvasNoiseTokenDataBrowserTest,
                       PopupWithIframeInOpenerOriginUsesMainFrameToken) {
  GURL url_a(embedded_test_server()->GetURL("a.com", "/defaultresponse"));
  GURL url_b_with_a_child(embedded_test_server()->GetURL(
      "b.com", "/cross_site_iframe_factory.html?b(a())"));
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());

  EXPECT_TRUE(NavigateToURL(shell(), url_a));

  RenderFrameHostImpl* main_frame_a = web_contents()->GetPrimaryMainFrame();
  std::optional<blink::NoiseToken> main_frame_a_nav_token =
      GetCanvasNoiseTokenForPage(main_frame_a->GetPage());
  EXPECT_NE(main_frame_a_nav_token, std::nullopt);
  EXPECT_EQ(main_frame_a_nav_token, GetRendererToken(main_frame_a));

  content::CreateAndLoadWebContentsObserver windowed_observer;
  // Now open a popup to b.com with a.com as an iframe.
  EXPECT_TRUE(
      ExecJs(shell(), JsReplace("window.open($1)", url_b_with_a_child)));
  content::WebContents* newtab = windowed_observer.Wait();
  ASSERT_TRUE(newtab);

  RenderFrameHostImpl* main_frame_b =
      static_cast<RenderFrameHostImpl*>(newtab->GetPrimaryMainFrame());
  EXPECT_NE(main_frame_a, main_frame_b);

  // Check b.com's token.
  std::optional<blink::NoiseToken> main_frame_b_nav_token =
      GetCanvasNoiseTokenForPage(main_frame_b->GetPage());
  EXPECT_NE(main_frame_b_nav_token, std::nullopt);
  EXPECT_NE(main_frame_b_nav_token, main_frame_a_nav_token);
  EXPECT_EQ(main_frame_b_nav_token, GetRendererToken(main_frame_b));

  RenderFrameHostImpl* child_frame_a =
      static_cast<RenderFrameHostImpl*>(ChildFrameAt(newtab, 0));
  ASSERT_TRUE(child_frame_a);

  // Check a.com's token under b.com's iframe, which should the be the same as
  // b.com's token.
  std::optional<blink::NoiseToken> child_frame_a_nav_token =
      GetCanvasNoiseTokenForPage(child_frame_a->GetPage());
  EXPECT_NE(child_frame_a_nav_token, std::nullopt);
  EXPECT_EQ(child_frame_a_nav_token, GetRendererToken(child_frame_a));
  EXPECT_EQ(main_frame_b_nav_token, child_frame_a_nav_token);
}

}  // namespace
}  // namespace content
