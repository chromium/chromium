// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/fenced_frame/fenced_frame.h"

#include <memory>
#include <string>
#include <tuple>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/input/render_widget_host_input_event_router.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/back_forward_cache_browsertest.h"
#include "content/browser/fenced_frame/fenced_frame_reporter.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_entry_restore_context_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/editable_level.h"
#include "content/public/browser/focused_node_details.h"
#include "content/public/browser/frame_type.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/mock_web_contents_observer.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/resource_load_observer.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_devtools_protocol_client.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_monitor.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/fenced_frame_test_utils.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/connection_tracker.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/fenced_frame/fenced_frame_utils.h"
#include "third_party/blink/public/common/frame/fenced_frame_sandbox_flags.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame.mojom.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-test-utils.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

namespace cors = network::cors::header_names;

constexpr char kAddIframeScript[] = R"({
    (()=>{
        return new Promise((resolve) => {
          const frame = document.createElement('iframe');
          frame.addEventListener('load', () => {resolve();});
          frame.src = $1;
          document.body.appendChild(frame);
        });
    })();
  })";

}  // namespace

class FencedFrameBrowserTestBase : public ContentBrowserTest {
 public:
  using ServerType = net::EmbeddedTestServer::Type;
  FencedFrameBrowserTestBase() : https_server_(ServerType::TYPE_HTTPS) {
    fenced_frame_test_helper_ = std::make_unique<test::FencedFrameTestHelper>();
  }

  // Defines the skeleton of set up method.
  void SetUpOnMainThread() final {
    host_resolver()->AddRule("*", "127.0.0.1");

    https_server()->ServeFilesFromSourceDirectory(GetTestDataFilePath());
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    SetupCrossSiteRedirector(https_server());
    net::test_server::RegisterDefaultHandlers(https_server());

    AdditionalSetup();
    AssertServerStart();
  }

  WebContentsImpl* web_contents() {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  RenderFrameHostImpl* primary_main_frame_host() {
    return web_contents()->GetPrimaryMainFrame();
  }

  test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return *fenced_frame_test_helper_.get();
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

 private:
  // Some test cases require server starting after performing other setups, mark
  // this virtual so that concrete test classes can override it with an empty
  // implementation.
  virtual void AssertServerStart() { ASSERT_TRUE(https_server()->Start()); }

  // Concrete test classes can override this to implement custom setups.
  virtual void AdditionalSetup() {}

  // This is a unique ptr because in some test cases we don't want to use it,
  // and it automatically enables MPArch fenced frames when created.
  std::unique_ptr<test::FencedFrameTestHelper> fenced_frame_test_helper_;
  net::EmbeddedTestServer https_server_;
};

class FencedFrameMPArchBrowserTest : public FencedFrameBrowserTestBase {
 protected:
  FencedFrameMPArchBrowserTest() = default;

  // TODO(crbug.com/40285326): This fails with the field trial testing config.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    FencedFrameBrowserTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitch("disable-field-trial-config");
  }

  base::HistogramTester histogram_tester_;

 private:
  // Server must start after ControllableHttpResponse object being constructed.
  void AssertServerStart() override {}
};

// This is a test class for tests that need to use IsolateAllSiteForTesting()
// and that will be testing process assignments. It is important that
// IsolateAllSiteForTesting is enabled early in these cases, otherwise the
// tests can end up with a main frame where
// AreOriginKeyedProcessesEnabledByDefault() was false when the main frame was
// created (and this is stored in the main frame's BrowsingInstance), and then
// AreOriginKeyedProcessesEnabledByDefault() later returns true due to
// IsolateAllSiteForTesting() turning on site-per-process. This sequence can
// lead to inconsistent SiteInfo settings.
class FencedFrameMPArchBrowserTest_IsolateAllSites
    : public FencedFrameMPArchBrowserTest {
 protected:
  FencedFrameMPArchBrowserTest_IsolateAllSites() = default;

  // TODO(crbug.com/40285326): This fails with the field trial testing config.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    FencedFrameMPArchBrowserTest::SetUpCommandLine(command_line);
    IsolateAllSitesForTesting(command_line);
  }
};

// Tests that the renderer can create a <fencedframe> that results in a
// browser-side content::FencedFrame also being created.
IN_PROC_BROWSER_TEST_F(FencedFrameMPArchBrowserTest,
                       CreateFromScriptAndDestroy) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(https_server()->Start());
  const GURL main_url =
      https_server()->GetURL("c.test", "/fenced_frames/title1.html");
  EXPECT_TRUE(
      NavigateToURL(shell(), https_server()->GetURL("c.test", "/title1.html")));
  RenderFrameHostImplWrapper primary_rfh(primary_main_frame_host());
  RenderFrameHostImplWrapper fenced_frame_rfh(
      fenced_frame_test_helper().CreateFencedFrame(primary_rfh.get(),
                                                   main_url));
  FrameTreeNode* fenced_frame_root_node = fenced_frame_rfh->frame_tree_node();
  EXPECT_TRUE(fenced_frame_root_node->render_manager()
                  ->GetProxyToOuterDelegate()
                  ->is_render_frame_proxy_live());

  // Test `RenderFrameHostImpl::IsInPrimaryMainFrame`.
  EXPECT_TRUE(primary_rfh->IsInPrimaryMainFrame());
  EXPECT_FALSE(fenced_frame_rfh->IsInPrimaryMainFrame());

  // Test `FrameTreeNode::IsFencedFrameRoot()`.
  EXPECT_FALSE(
      web_contents()->GetPrimaryFrameTree().root()->IsFencedFrameRoot());
  EXPECT_FALSE(primary_rfh->child_at(0)->IsFencedFrameRoot());
  EXPECT_TRUE(fenced_frame_root_node->IsFencedFrameRoot());

  // Test `FrameTreeNode::IsInFencedFrameTree()`.
  EXPECT_FALSE(
      web_contents()->GetPrimaryFrameTree().root()->IsInFencedFrameTree());
  EXPECT_FALSE(primary_rfh->child_at(0)->IsInFencedFrameTree());
  EXPECT_TRUE(fenced_frame_root_node->IsInFencedFrameTree());

  EXPECT_TRUE(ExecJs(primary_rfh.get(),
                     "const ff = document.querySelector('fencedframe');\
                     ff.remove();"));
  ASSERT_TRUE(fenced_frame_rfh.WaitUntilRenderFrameDeleted());

  EXPECT_TRUE(primary_rfh->GetFencedFrames().empty());
  EXPECT_TRUE(fenced_frame_rfh.IsDestroyed());
  histogram_tester.ExpectTotalCount(
      "Ads.InterestGroup.Auction.AdNavigationStarted", 0);
}

IN_PROC_BROWSER_TEST_F(FencedFrameMPArchBrowserTest, CreateFromParser) {
  ASSERT_TRUE(https_server()->Start());
  const GURL top_level_url =
      https_server()->GetURL("c.test", "/fenced_frames/basic.html");
  EXPECT_TRUE(NavigateToURL(shell(), top_level_url));

  // The fenced frame is set-up synchronously, so it should exist immediately.
  RenderFrameHostImplWrapper dummy_child_frame(
      primary_main_frame_host()->child_at(0)->current_frame_host());
  EXPECT_TRUE(dummy_child_frame->inner_tree_main_frame_tree_node_id());
  FrameTreeNode* inner_frame_tree_node = FrameTreeNode::GloballyFindByID(
      dummy_child_frame->inner_tree_main_frame_tree_node_id());
  EXPECT_TRUE(inner_frame_tree_node);
}

IN_PROC_BROWSER_TEST_F(FencedFrameMPArchBrowserTest, Navigation) {
  ASSERT_TRUE(https_server()->Start());
  const GURL main_url = https_server()->GetURL("c.test", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // WebContentsObservers should not be notified of commits happening
  // in the non-primary navigation controller.
  testing::NiceMock<MockWebContentsObserver> web_contents_observer(
      web_contents());
  EXPECT_CALL(web_contents_observer, NavigationEntryCommitted(testing::_))
      .Times(0);
  EXPECT_CALL(web_contents_observer, NavigationEntryChanged(testing::_))
      .Times(0);

  RenderFrameHostImpl* primary_rfh = primary_main_frame_host();

  const GURL fenced_frame_url =
      https_server()->GetURL("c.test", "/fenced_frames/title1.html");
  RenderFrameHost* fenced_frame_rfh =
      fenced_frame_test_helper().CreateFencedFrame(primary_rfh,
                                                   fenced_frame_url);

  // Test that a fenced frame navigation does not impact the primary main
  // frame...
  EXPECT_EQ(main_url, primary_rfh->GetLastCommittedURL());
  // ... but should target the correct frame.
  EXPECT_EQ(fenced_frame_url, fenced_frame_rfh->GetLastCommittedURL());
  EXPECT_EQ(url::Origin::Create(fenced_frame_url),
            fenced_frame_rfh->GetLastCommittedOrigin());
}


IN_PROC_BROWSER_TEST_F(FencedFrameMPArchBrowserTest,
                       SettingNullConfigNavigatesToAboutBlank) {
  ASSERT_TRUE(https_server()->Start());
  const GURL main_url = https_server()->GetURL("a.test", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  RenderFrameHostImpl* primary_rfh = primary_main_frame_host();

  const GURL fenced_frame_url =
      https_server()->GetURL("c.test", "/fenced_frames/title1.html");
  fenced_frame_test_helper().CreateFencedFrame(primary_rfh, fenced_frame_url);

  std::vector<FencedFrame*> fenced_frames = primary_rfh->GetFencedFrames();
  ASSERT_EQ(1ul, fenced_frames.size());
  FencedFrame* fenced_frame = fenced_frames.back();

  // Expect the origin is correct.
  EXPECT_EQ(url::Origin::Create(fenced_frame_url),
            EvalJs(fenced_frame->GetInnerRoot(), "self.origin;"));

  TestFrameNavigationObserver observer(fenced_frame->GetInnerRoot());
  EXPECT_TRUE(ExecJs(primary_rfh,
                     "document.querySelector('fencedframe').config = null;"));
  observer.Wait();

  EXPECT_FALSE(fenced_frame->GetInnerRoot()->IsErrorDocument());
  EXPECT_EQ("null", EvalJs(fenced_frame->GetInnerRoot(), "self.origin;"));
  EXPECT_EQ("about:blank",
            EvalJs(fenced_frame->GetInnerRoot(), "window.location.href"));
}

IN_PROC_BROWSER_TEST_F(FencedFrameMPArchBrowserTest, FrameIteration) {
  ASSERT_TRUE(https_server()->Start());
  const GURL main_url = https_server()->GetURL("c.test", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  RenderFrameHostImplWrapper primary_rfh(primary_main_frame_host());

  const GURL fenced_frame_url =
      https_server()->GetURL("c.test", "/fenced_frames/title1.html");
  RenderFrameHostImplWrapper fenced_frame_rfh(
      fenced_frame_test_helper().CreateFencedFrame(primary_rfh.get(),
                                                   fenced_frame_url));

  // Test that the outer => inner delegate mechanism works correctly.
  EXPECT_THAT(CollectAllRenderFrameHosts(primary_rfh.get()),
              testing::ElementsAre(primary_rfh.get(), fenced_frame_rfh.get()));

  // Test that the inner => outer delegate mechanism works correctly.
  EXPECT_EQ(nullptr, fenced_frame_rfh->GetParent());
  EXPECT_EQ(fenced_frame_rfh->GetParentOrOuterDocument(), primary_rfh.get());
  EXPECT_EQ(fenced_frame_rfh->GetOutermostMainFrame(), primary_rfh.get());
  EXPECT_EQ(fenced_frame_rfh->GetParentOrOuterDocumentOrEmbedder(),
            primary_rfh.get());
  EXPECT_EQ(fenced_frame_rfh->GetOutermostMainFrameOrEmbedder(),
            primary_rfh.get());

  // WebContentsImpl::ForEachFrameTree should include fenced frames.
  bool visited_fenced_frame_frame_tree = false;
  web_contents()->ForEachFrameTree([&](FrameTree& frame_tree) {
    if (&frame_tree == fenced_frame_rfh->frame_tree()) {
      visited_fenced_frame_frame_tree = true;
    }
  });
  EXPECT_TRUE(visited_fenced_frame_frame_tree);
}

namespace {

// Intercepts calls to RenderFramHostImpl's CreateFencedFrame mojo method, and
// connects a NavigationDelayer which delays the FencedFrameOwnerHost's
// Navigate mojo method.
class NavigationDelayerInterceptor
    : public blink::mojom::LocalFrameHostInterceptorForTesting {
 public:
  explicit NavigationDelayerInterceptor(RenderFrameHostImpl* render_frame_host,
                                        base::TimeDelta duration)
      : render_frame_host_(render_frame_host),
        duration_(duration),
        impl_(render_frame_host_->local_frame_host_receiver_for_testing()
                  .SwapImplForTesting(this)) {}

  ~NavigationDelayerInterceptor() override = default;

  blink::mojom::LocalFrameHost* GetForwardingInterface() override {
    return impl_;
  }

  void CreateFencedFrame(
      mojo::PendingAssociatedReceiver<blink::mojom::FencedFrameOwnerHost>
          pending_receiver,
      blink::mojom::RemoteFrameInterfacesFromRendererPtr
          remote_frame_interfaces,
      const blink::RemoteFrameToken& frame_token,
      const base::UnguessableToken& devtools_frame_token) override {
    mojo::PendingAssociatedRemote<blink::mojom::FencedFrameOwnerHost>
        original_remote;

    GetForwardingInterface()->CreateFencedFrame(
        original_remote.InitWithNewEndpointAndPassReceiver(),
        std::move(remote_frame_interfaces), frame_token, devtools_frame_token);
    std::vector<FencedFrame*> fenced_frames =
        render_frame_host_->GetFencedFrames();
    ASSERT_FALSE(fenced_frames.empty());
    navigate_interceptor_ = std::make_unique<NavigationDelayer>(
        std::move(original_remote), std::move(pending_receiver),
        fenced_frames.back(), duration_);
  }

 private:
  class NavigationDelayer : public blink::mojom::FencedFrameOwnerHost {
   public:
    explicit NavigationDelayer(
        mojo::PendingAssociatedRemote<blink::mojom::FencedFrameOwnerHost>
            original_remote,
        mojo::PendingAssociatedReceiver<blink::mojom::FencedFrameOwnerHost>
            receiver,
        FencedFrame* fenced_frame,
        base::TimeDelta duration)
        : original_remote_(std::move(original_remote)),
          fenced_frame_(fenced_frame),
          duration_(duration) {
      receiver_.Bind(std::move(receiver));
    }

    ~NavigationDelayer() override = default;

    void Navigate(const GURL& url,
                  base::TimeTicks navigation_start_time) override {
      base::PlatformThread::Sleep(duration_);
      fenced_frame_->Navigate(url, navigation_start_time);
    }

    void DidChangeFramePolicy(const blink::FramePolicy& frame_policy) override {
      fenced_frame_->DidChangeFramePolicy(frame_policy);
    }

   private:
    mojo::AssociatedRemote<blink::mojom::FencedFrameOwnerHost> original_remote_;
    mojo::AssociatedReceiver<blink::mojom::FencedFrameOwnerHost> receiver_{
        this};
    raw_ptr<FencedFrame> fenced_frame_;
    const base::TimeDelta duration_;
  };

  raw_ptr<RenderFrameHostImpl> render_frame_host_;
  std::unique_ptr<NavigationDelayer> navigate_interceptor_;
  const base::TimeDelta duration_;
  raw_ptr<blink::mojom::LocalFrameHost> impl_;
};

}  // namespace


// Test that ensures we can post from an cross origin iframe into the
// fenced frame root.
IN_PROC_BROWSER_TEST_F(FencedFrameMPArchBrowserTest, CrossOriginMessagePost) {
  ASSERT_TRUE(https_server()->Start());
  const GURL main_url =
      https_server()->GetURL("c.test", "/fenced_frames/title1.html");
  const GURL cross_origin_iframe_url =
      https_server()->GetURL("b.com", "/fenced_frames/title1.html");
  EXPECT_TRUE(
      NavigateToURL(shell(), https_server()->GetURL("c.test", "/title1.html")));
  RenderFrameHostImplWrapper primary_rfh(primary_main_frame_host());
  RenderFrameHostImplWrapper fenced_frame_rfh(
      fenced_frame_test_helper().CreateFencedFrame(primary_rfh.get(),
                                                   main_url));
  EXPECT_TRUE(ExecJs(fenced_frame_rfh.get(),
                     JsReplace(kAddIframeScript, cross_origin_iframe_url)));

  RenderFrameHostImpl* iframe = static_cast<RenderFrameHostImpl*>(
      ChildFrameAt(fenced_frame_rfh.get(), 0));

  EXPECT_TRUE(
      EvalJs(iframe->GetParent(), R"(window.addEventListener('message', (e) => {
                e.source.postMessage('echo ' + e.data, "*");
              }, false); true)")
          .ExtractBool());
  EXPECT_EQ("echo test", EvalJs(iframe, R"((async() => {
                      let promise = new Promise(function(resolve, reject) {
                        window.addEventListener('message', (e) => {
                          resolve(e.data)
                        }, false);
                        window.parent.postMessage('test', "*");
                      });
                      let result = await promise;
                      return result;
                    })())"));
}

// Test that when the documents inside fenced frame tree are loading,
// WebContentsObserver::DocumentOnLoadCompletedInPrimaryMainFrame is not invoked
// for fenced frames as it is only invoked for primary main frames.
IN_PROC_BROWSER_TEST_F(FencedFrameMPArchBrowserTest,
                       DocumentOnLoadCompletedInPrimaryMainFrame) {
  ASSERT_TRUE(https_server()->Start());
  // Initialize a MockWebContentsObserver to ensure that
  // DocumentOnLoadCompletedInPrimaryMainFrame is only invoked for primary main
  // RenderFrameHosts.
  testing::NiceMock<MockWebContentsObserver> web_contents_observer(
      web_contents());

  // Navigate to an initial primary page. This should result in invoking
  // DocumentOnLoadCompletedInPrimaryMainFrame once.
  EXPECT_CALL(web_contents_observer,
              DocumentOnLoadCompletedInPrimaryMainFrame())
      .Times(1);
  EXPECT_TRUE(
      NavigateToURL(shell(), https_server()->GetURL("c.test", "/title1.html")));
  RenderFrameHostImplWrapper primary_rfh(primary_main_frame_host());

  // Once the fenced frame complets loading, it shouldn't result in
  // invoking DocumentOnLoadCompletedInPrimaryMainFrame.
  EXPECT_CALL(web_contents_observer,
              DocumentOnLoadCompletedInPrimaryMainFrame())
      .Times(0);
  const GURL fenced_frame_url =
      https_server()->GetURL("c.test", "/fenced_frames/title1.html");
  RenderFrameHostImplWrapper inner_fenced_frame_rfh(
      fenced_frame_test_helper().CreateFencedFrame(primary_rfh.get(),
                                                   fenced_frame_url));
  FrameTreeNode* fenced_frame_root_node =
      inner_fenced_frame_rfh->frame_tree_node();
  EXPECT_FALSE(fenced_frame_root_node->IsLoading());
}

// Test that when the documents inside the fenced frame tree are loading,
// WebContentsObserver::PrimaryMainDocumentElementAvailable is not invoked for
// fenced frames as it is only invoked for primary main frames.
IN_PROC_BROWSER_TEST_F(FencedFrameMPArchBrowserTest,
                       PrimaryMainDocumentElementAvailable) {
  ASSERT_TRUE(https_server()->Start());
  // Initialize a MockWebContentsObserver to ensure that
  // PrimaryMainDocumentElementAvailable is only invoked for primary main
  // RenderFrameHosts.
  testing::NiceMock<MockWebContentsObserver> web_contents_observer(
      web_contents());
  testing::InSequence s;

  // Navigate to an initial primary page. This should result in invoking
  // PrimaryMainDocumentElementAvailable once.
  EXPECT_CALL(web_contents_observer, PrimaryMainDocumentElementAvailable())
      .Times(1);
  EXPECT_TRUE(
      NavigateToURL(shell(), https_server()->GetURL("c.test", "/title1.html")));
  RenderFrameHostImplWrapper primary_rfh(primary_main_frame_host());

  // Once the fenced frame completes loading, it shouldn't result in
  // invoking PrimaryMainDocumentElementAvailable.
  EXPECT_CALL(web_contents_observer, PrimaryMainDocumentElementAvailable())
      .Times(0);
  const GURL fenced_frame_url =
      https_server()->GetURL("c.test", "/fenced_frames/title1.html");
  RenderFrameHostImplWrapper inner_fenced_frame_rfh(
      fenced_frame_test_helper().CreateFencedFrame(primary_rfh.get(),
                                                   fenced_frame_url));
  FrameTreeNode* fenced_frame_root_node =
      inner_fenced_frame_rfh->frame_tree_node();
  EXPECT_FALSE(fenced_frame_root_node->IsLoading());
}


// Test that fenced frames use the primary main frame's UKM source id during
// navigation.
IN_PROC_BROWSER_TEST_F(FencedFrameMPArchBrowserTest, GetPageUkmSourceId) {
  ASSERT_TRUE(https_server()->Start());
  const GURL main_url = https_server()->GetURL("c.test", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  const GURL fenced_frame_url =
      https_server()->GetURL("c.test", "/fenced_frames/title1.html");
  NavigationHandleObserver handle_observer(web_contents(), fenced_frame_url);
  RenderFrameHostImplWrapper fenced_frame_rfh(
      fenced_frame_test_helper().CreateFencedFrame(primary_main_frame_host(),
                                                   fenced_frame_url));
  ASSERT_TRUE(fenced_frame_rfh);

  ukm::SourceId nav_request_id = handle_observer.next_page_ukm_source_id();
  // Should have the same page UKM ID in navigation as page post commit, and as
  // the primary main frame.
  EXPECT_EQ(primary_main_frame_host()->GetPageUkmSourceId(), nav_request_id);
  EXPECT_EQ(fenced_frame_rfh->GetPageUkmSourceId(), nav_request_id);
}

// Test that iframes that nested within fenced frames use the primary main
// frame's UKM source id during navigation.
IN_PROC_BROWSER_TEST_F(FencedFrameMPArchBrowserTest,
                       GetPageUkmSourceId_NestedFrame) {
  ASSERT_TRUE(https_server()->Start());
  const GURL main_url = https_server()->GetURL("c.test", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  const GURL fenced_frame_url =
      https_server()->GetURL("c.test", "/fenced_frames/title1.html");
  RenderFrameHostImplWrapper fenced_frame_rfh(
      fenced_frame_test_helper().CreateFencedFrame(primary_main_frame_host(),
                                                   fenced_frame_url));
  ASSERT_TRUE(fenced_frame_rfh);

  const GURL iframe_url =
      https_server()->GetURL("c.test", "/fenced_frames/title1.html");
  NavigationHandleObserver handle_observer(web_contents(), iframe_url);
  EXPECT_TRUE(
      ExecJs(fenced_frame_rfh.get(), JsReplace(kAddIframeScript, iframe_url)));

  RenderFrameHostImpl* iframe_rfh = static_cast<RenderFrameHostImpl*>(
      ChildFrameAt(fenced_frame_rfh.get(), 0));
  ukm::SourceId nav_request_id = handle_observer.next_page_ukm_source_id();
  // Should have the same page UKM ID in navigation as page post commit, and as
  // the primary main frame.
  EXPECT_EQ(primary_main_frame_host()->GetPageUkmSourceId(), nav_request_id);
  EXPECT_EQ(fenced_frame_rfh->GetPageUkmSourceId(), nav_request_id);
  EXPECT_EQ(iframe_rfh->GetPageUkmSourceId(), nav_request_id);
}

IN_PROC_BROWSER_TEST_F(FencedFrameMPArchBrowserTest,
                       DocumentUKMSourceIdShouldNotBeAssociatedWithURL) {
  ukm::TestAutoSetUkmRecorder recorder;

  ASSERT_TRUE(https_server()->Start());
  const GURL main_url = https_server()->GetURL("a.test", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  ukm::SourceId fenced_frame_document_ukm_source_id = ukm::kInvalidSourceId;
  DidFinishNavigationObserver observer(
      web_contents(),
      base::BindLambdaForTesting([&fenced_frame_document_ukm_source_id](
                                     NavigationHandle* navigation_handle) {
        if (navigation_handle->GetNavigatingFrameType() !=
            FrameType::kFencedFrameRoot)
          return;
        NavigationRequest* request = NavigationRequest::From(navigation_handle);
        fenced_frame_document_ukm_source_id =
            request->commit_params().document_ukm_source_id;
      }));
  const GURL fenced_frame_url =
      https_server()->GetURL("b.test", "/fenced_frames/title1.html");
  RenderFrameHostImplWrapper fenced_frame_rfh(
      fenced_frame_test_helper().CreateFencedFrame(primary_main_frame_host(),
                                                   fenced_frame_url));
  ASSERT_TRUE(fenced_frame_rfh);
  ASSERT_NE(ukm::kInvalidSourceId, fenced_frame_document_ukm_source_id);
  EXPECT_EQ(nullptr,
            recorder.GetSourceForSourceId(fenced_frame_document_ukm_source_id));
}

// Test that FrameTree::CollectNodesForIsLoading doesn't include inner
// WebContents nodes.
IN_PROC_BROWSER_TEST_F(FencedFrameMPArchBrowserTest, NodesForIsLoading) {
  ASSERT_TRUE(https_server()->Start());
  GURL url_a(https_server()->GetURL("c.test", "/page_with_iframe.html"));
  GURL url_b(https_server()->GetURL("c.test", "/title1.html"));
  GURL fenced_frame_url(
      https_server()->GetURL("c.test", "/fenced_frames/title1.html"));

  // 1. Navigate to an initial primary page.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper primary_rfh(primary_main_frame_host());
  FrameTree& primary_frame_tree = web_contents()->GetPrimaryFrameTree();

  // 2. Create a fenced frame embedded inside primary page.
  RenderFrameHostImplWrapper outer_fenced_frame_rfh(
      fenced_frame_test_helper().CreateFencedFrame(primary_rfh.get(),
                                                   fenced_frame_url));

  // 3. Create a inner WebContents and attach it to the main contents. Navigate
  // the inner web contents to an initial page.
  WebContentsImpl* inner_contents =
      static_cast<WebContentsImpl*>(CreateAndAttachInnerContents(
          primary_rfh.get()->child_at(0)->current_frame_host()));
  ASSERT_TRUE(NavigateToURLFromRenderer(inner_contents, url_b));

  RenderFrameHostImpl* inner_contents_rfh =
      inner_contents->GetPrimaryMainFrame();
  FrameTree& inner_contents_primary_frame_tree =
      inner_contents->GetPrimaryFrameTree();
  ASSERT_TRUE(inner_contents_rfh);

  // 4. Create a fenced frame embedded inside inner WebContents.
  RenderFrameHostImplWrapper inner_fenced_frame_rfh(
      fenced_frame_test_helper().CreateFencedFrame(inner_contents_rfh,
                                                   fenced_frame_url));

  // 5. FrameTree::CollectNodesForIsLoading should only include primary_rfh and
  // outer_fenced_frame_rfh when checked against outer delegate FrameTree.
  std::vector<RenderFrameHostImpl*> outer_web_contents_frames;
  for (auto* ftn :
       web_contents()->GetPrimaryFrameTree().CollectNodesForIsLoading()) {
    outer_web_contents_frames.push_back(ftn->current_frame_host());
  }
  EXPECT_EQ(outer_web_contents_frames.size(), 2u);
  EXPECT_THAT(outer_web_contents_frames,
              testing::UnorderedElementsAre(primary_rfh.get(),
                                            outer_fenced_frame_rfh.get()));

  // 6. FrameTree::CollectNodesForIsLoading should only include
  // inner_contents_rfh and inner_fenced_frame_rfh when checked against inner
  // delegate FrameTree.
  std::vector<RenderFrameHostImpl*> inner_web_contents_frames;
  for (auto* ftn :
       inner_contents->GetPrimaryFrameTree().CollectNodesForIsLoading()) {
    inner_web_contents_frames.push_back(ftn->current_frame_host());
  }
  EXPECT_EQ(inner_web_contents_frames.size(), 2u);
  EXPECT_THAT(inner_web_contents_frames,
              testing::UnorderedElementsAre(inner_contents_rfh,
                                            inner_fenced_frame_rfh.get()));

  // 7. Check that FrameTree::LoadingTree returns the correct FrameTree for both
  // outer and inner WebContents frame trees.
  EXPECT_NE(primary_frame_tree.LoadingTree(),
            inner_contents_primary_frame_tree.LoadingTree());
  EXPECT_EQ(primary_frame_tree.LoadingTree(), &primary_frame_tree);
  EXPECT_EQ(inner_contents_primary_frame_tree.LoadingTree(),
            &inner_contents_primary_frame_tree);
}

IN_PROC_BROWSER_TEST_F(FencedFrameMPArchBrowserTest,
                       NoErrorPageOnEmptyFrameHttpError) {
  ASSERT_TRUE(https_server()->Start());
  const GURL kInitialUrl(https_server()->GetURL("c.test", "/title1.html"));
  const GURL kEmpty404Url(
      https_server()->GetURL("c.test", "/fenced_frames/empty404.html"));

  // Load an initial page.
  EXPECT_TRUE(NavigateToURL(shell(), kInitialUrl));
  RenderFrameHostImplWrapper initial_rfh(primary_main_frame_host());

  // Add a fenced frame empty page with 404 status.
  RenderFrameHostImplWrapper fenced_frame_rfh(
      fenced_frame_test_helper().CreateFencedFrame(initial_rfh.get(),
                                                   kEmpty404Url));
  ASSERT_TRUE(fenced_frame_rfh);

  // Confirm no error page was generated in its place.
  std::string contents =
      EvalJs(fenced_frame_rfh.get(), "document.body.textContent;")
          .ExtractString();
  EXPECT_EQ(contents, std::string());
}

// Test that when the documents inside the fenced frame tree are loading, then
// `WebContents::IsLoading`, `FrameTree::IsLoadingIncludingInnerFrameTrees`, and
// `FrameTreeNode::IsLoading` should return true. Primary
// `FrameTree::IsLoadingIncludingInnerFrameTrees` value should reflect the
// loading state of descendant fenced frames.
IN_PROC_BROWSER_TEST_F(FencedFrameMPArchBrowserTest, IsLoading) {
  // Create a HTTP response to control fenced frame navigation.
  net::test_server::ControllableHttpResponse fenced_frame_response(
      https_server(), "/fenced_frames/title2.html");
  ASSERT_TRUE(https_server()->Start());

  // Navigate to primary url.
  EXPECT_TRUE(
      NavigateToURL(shell(), https_server()->GetURL("c.test", "/title1.html")));
  RenderFrameHostImpl* fenced_frame_parent_rfh = primary_main_frame_host();

  // Create a fenced frame for fenced_frame_url.
  const GURL fenced_frame_url =
      https_server()->GetURL("c.test", "/fenced_frames/title2.html");
  fenced_frame_test_helper().CreateFencedFrame(
      fenced_frame_parent_rfh, fenced_frame_url, net::OK,
      blink::FencedFrame::DeprecatedFencedFrameMode::kOpaqueAds,
      /*wait_for_load=*/false);

  std::vector<FencedFrame*> fenced_frames =
      fenced_frame_parent_rfh->GetFencedFrames();
  EXPECT_EQ(fenced_frames.size(), 1ul);
  FencedFrame* fenced_frame = fenced_frames.back();

  RenderFrameHostImplWrapper inner_fenced_frame_rfh(
      fenced_frame->GetInnerRoot());
  FrameTreeNode* fenced_frame_root_node =
      inner_fenced_frame_rfh->frame_tree_node();
  FrameTree& fenced_frame_tree = fenced_frame_root_node->frame_tree();

  // All WebContents::IsLoading, FrameTree::IsLoadingIncludingInnerFrameTrees,
  // and FrameTreeNode::IsLoading should return true when the fenced frame is
  // loading along with primary FrameTree::IsLoadingIncludingInnerFrameTrees as
  // we check for inner frame trees loading state.
  EXPECT_TRUE(web_contents()->IsLoading());
  EXPECT_TRUE(primary_main_frame_host()
                  ->frame_tree()
                  ->IsLoadingIncludingInnerFrameTrees());
  EXPECT_TRUE(fenced_frame_root_node->IsLoading());
  EXPECT_TRUE(fenced_frame_tree.IsLoadingIncludingInnerFrameTrees());

  // Complete the fenced frame response and finish fenced frame navigation.
  fenced_frame_response.WaitForRequest();
  fenced_frame_response.Send(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "Supports-Loading-Mode: fenced-frame\r\n"
      "\r\n");
  fenced_frame_response.Done();

  // Check that all the above loading states should return false once the fenced
  // frame stops loading.
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  EXPECT_FALSE(web_contents()->IsLoading());
  EXPECT_FALSE(primary_main_frame_host()
                   ->frame_tree()
                   ->IsLoadingIncludingInnerFrameTrees());
  EXPECT_FALSE(fenced_frame_root_node->IsLoading());
  EXPECT_FALSE(fenced_frame_tree.IsLoadingIncludingInnerFrameTrees());
}

// Test that when the documents inside the fenced frame tree are loading,
// WebContentsObserver::DidStartLoading is fired and when document stops loading
// WebContentsObserver::DidStopLoading is fired. In this test primary page
// completed loading before fenced frame starts loading and we test the loading
// state in the end when both primary page and fenced frame completed loading.
IN_PROC_BROWSER_TEST_F(FencedFrameMPArchBrowserTest,
                       DidStartAndDidStopLoading) {
  // Create a HTTP response to control fenced frame navigation.
  net::test_server::ControllableHttpResponse fenced_frame_response(
      https_server(), "/fenced_frames/title2.html");
  ASSERT_TRUE(https_server()->Start());

  // Navigate to primary url.
  EXPECT_TRUE(
      NavigateToURL(shell(), https_server()->GetURL("c.test", "/title1.html")));
  EXPECT_FALSE(web_contents()->IsLoading());
  RenderFrameHostImpl* fenced_frame_parent_rfh = primary_main_frame_host();
  EXPECT_EQ(fenced_frame_parent_rfh->GetFencedFrames().size(), 0ul);

  // Initialize a MockWebContentsObserver and ensure that
  // DidStartLoading and DidStopLoading are invoked.
  testing::NiceMock<MockWebContentsObserver> web_contents_observer(
      web_contents());
  testing::InSequence s;

  // Create a fenced frame for fenced_frame_url. This will result in invoking
  // DidStartLoading callback once.
  EXPECT_CALL(web_contents_observer, DidStartLoading()).Times(1);
  const GURL fenced_frame_url =
      https_server()->GetURL("c.test", "/fenced_frames/title2.html");
  fenced_frame_test_helper().CreateFencedFrame(
      fenced_frame_parent_rfh, fenced_frame_url, net::OK,
      blink::FencedFrame::DeprecatedFencedFrameMode::kOpaqueAds,
      /*wait_for_load=*/false);
  std::vector<FencedFrame*> fenced_frames =
      fenced_frame_parent_rfh->GetFencedFrames();
  EXPECT_EQ(fenced_frame_parent_rfh->GetFencedFrames().size(), 1ul);
  EXPECT_TRUE(web_contents()->IsLoading());

  // Complete the fenced frame response and finish fenced frame navigation.
  fenced_frame_response.WaitForRequest();
  fenced_frame_response.Send(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "Supports-Loading-Mode: fenced-frame\r\n"
      "\r\n");
  fenced_frame_response.Done();

  // Once the fenced frame stops loading, this should result in invoking
  // the DidStopLoading callback once.
  EXPECT_CALL(web_contents_observer, DidStopLoading()).Times(1);
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
}

// Ensure that WebContentsObserver::LoadProgressChanged is not invoked when
// there is a change in load state of fenced frame as LoadProgressChanged is
// attributed to only primary main frame load progress change.
IN_PROC_BROWSER_TEST_F(FencedFrameMPArchBrowserTest, LoadProgressChanged) {
  ASSERT_TRUE(https_server()->Start());
  const GURL fenced_frame_url =
      https_server()->GetURL("c.test", "/fenced_frames/title1.html");
  const GURL main_url = https_server()->GetURL("c.test", "/title1.html");

  // Navigate to primary url.
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  RenderFrameHostImplWrapper primary_rfh(primary_main_frame_host());

  // Initialize a MockWebContentsObserver and ensure that LoadProgressChanged is
  // not invoked for fenced frames.
  testing::NiceMock<MockWebContentsObserver> web_contents_observer(
      web_contents());

  // Create a fenced frame for fenced_frame_url. This shouldn't call
  // LoadProgressChanged.
  EXPECT_CALL(web_contents_observer, LoadProgressChanged(testing::_)).Times(0);
  fenced_frame_test_helper().CreateFencedFrame(primary_rfh.get(),
                                               fenced_frame_url);
}

// Tests that NavigationHandle::GetNavigatingFrameType() returns the correct
// type.
IN_PROC_BROWSER_TEST_F(FencedFrameMPArchBrowserTest,
                       NavigationHandleFrameType) {
  ASSERT_TRUE(https_server()->Start());
  {
    DidFinishNavigationObserver observer(
        web_contents(),
        base::BindLambdaForTesting([](NavigationHandle* navigation_handle) {
          EXPECT_TRUE(navigation_handle->IsInPrimaryMainFrame());
          DCHECK_EQ(navigation_handle->GetNavigatingFrameType(),
                    FrameType::kPrimaryMainFrame);
        }));
    EXPECT_TRUE(NavigateToURL(
        shell(), https_server()->GetURL("c.test", "/title1.html")));
  }

  {
    DidFinishNavigationObserver observer(
        web_contents(),
        base::BindLambdaForTesting([](NavigationHandle* navigation_handle) {
          EXPECT_FALSE(navigation_handle->IsInMainFrame());
          DCHECK_EQ(navigation_handle->GetNavigatingFrameType(),
                    FrameType::kSubframe);
        }));
    EXPECT_TRUE(
        ExecJs(primary_main_frame_host(),
               JsReplace(kAddIframeScript,
                         https_server()->GetURL("c.test", "/empty.html"))));
  }
  {
    const GURL fenced_frame_url =
        https_server()->GetURL("c.test", "/fenced_frames/title1.html");
    RenderFrameHostImplWrapper fenced_frame_rfh(
        fenced_frame_test_helper().CreateFencedFrame(primary_main_frame_host(),
                                                     fenced_frame_url));
    DidFinishNavigationObserver observer(
        web_contents(),
        base::BindLambdaForTesting([](NavigationHandle* navigation_handle) {
          EXPECT_TRUE(
              navigation_handle->GetRenderFrameHost()->IsFencedFrameRoot());
          DCHECK_EQ(navigation_handle->GetNavigatingFrameType(),
                    FrameType::kFencedFrameRoot);
        }));
    fenced_frame_test_helper().NavigateFrameInFencedFrameTree(
        fenced_frame_rfh.get(), fenced_frame_url);
  }
}

// Tests that an unload/beforeunload event handler won't be set from
// fenced frames.
IN_PROC_BROWSER_TEST_F(FencedFrameMPArchBrowserTest, UnloadHandler) {
  ASSERT_TRUE(https_server()->Start());
  const GURL main_url =
      https_server()->GetURL("c.test", "/fenced_frames/title1.html");
  EXPECT_TRUE(
      NavigateToURL(shell(), https_server()->GetURL("c.test", "/title1.html")));
  RenderFrameHostImplWrapper primary_rfh(primary_main_frame_host());
  RenderFrameHostImplWrapper fenced_frame_rfh(
      fenced_frame_test_helper().CreateFencedFrame(primary_rfh.get(),
                                                   main_url));

  const char kConsolePattern[] =
      "unload/beforeunload handlers are prohibited in fenced frames.";
  {
    WebContentsConsoleObserver console_observer(web_contents());
    console_observer.SetPattern(kConsolePattern);
    EXPECT_TRUE(ExecJs(fenced_frame_rfh.get(),
                       "window.addEventListener('beforeunload', (e) => {});"));
    ASSERT_TRUE(console_observer.Wait());
    EXPECT_EQ(1u, console_observer.messages().size());
  }
  {
    WebContentsConsoleObserver console_observer(web_contents());
    console_observer.SetPattern(kConsolePattern);
    EXPECT_TRUE(ExecJs(fenced_frame_rfh.get(),
                       "window.addEventListener('unload', (e) => {});"));
    ASSERT_TRUE(console_observer.Wait());
    EXPECT_EQ(1u, console_observer.messages().size());
  }
  {
    WebContentsConsoleObserver console_observer(web_contents());
    console_observer.SetPattern(kConsolePattern);
    EXPECT_TRUE(ExecJs(fenced_frame_rfh.get(),
                       "window.onbeforeunload = function(e){};"));
    ASSERT_TRUE(console_observer.Wait());
    EXPECT_EQ(1u, console_observer.messages().size());
  }
  {
    WebContentsConsoleObserver console_observer(web_contents());
    console_observer.SetPattern(kConsolePattern);
    EXPECT_TRUE(
        ExecJs(fenced_frame_rfh.get(), "window.onunload = function(e){};"));
    ASSERT_TRUE(console_observer.Wait());
    EXPECT_EQ(1u, console_observer.messages().size());
  }
}

// Tests that an input event targeted to a fenced frame correctly
// triggers a user interaction notification for WebContentsObservers.
IN_PROC_BROWSER_TEST_F(FencedFrameMPArchBrowserTest,
                       UserInteractionForFencedFrame) {
  ASSERT_TRUE(https_server()->Start());
  const GURL main_url = https_server()->GetURL("c.test", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  RenderFrameHostImplWrapper primary_rfh(primary_main_frame_host());

  const GURL fenced_frame_url =
      https_server()->GetURL("c.test", "/fenced_frames/title1.html");
  RenderFrameHostImplWrapper fenced_frame_rfh(
      fenced_frame_test_helper().CreateFencedFrame(primary_rfh.get(),
                                                   fenced_frame_url));

  ::testing::NiceMock<MockWebContentsObserver> web_contents_observer(
      web_contents());
  EXPECT_CALL(web_contents_observer, DidGetUserInteraction(testing::_))
      .Times(1);

  // Target an event to the fenced frame's RenderWidgetHostView.
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebPointerProperties::Button::kLeft;
  mouse_event.SetPositionInWidget(5, 5);
  fenced_frame_rfh->GetRenderWidgetHost()->ForwardMouseEvent(mouse_event);
}

// Tests that a SetAutoscrollSelectionActiveInMainFrame request from a fenced
// frame's main-frame widget does not affect mouse-up routing to the outer
// root view. This should only be honored for the outermost main frame.
IN_PROC_BROWSER_TEST_F(FencedFrameMPArchBrowserTest,
                       AutoscrollSelectionFromFencedFrameIgnored) {
  ASSERT_TRUE(https_server()->Start());
  const GURL main_url = https_server()->GetURL("c.test", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  RenderFrameHostImplWrapper primary_rfh(primary_main_frame_host());

  const GURL fenced_frame_url =
      https_server()->GetURL("c.test", "/fenced_frames/title1.html");
  RenderFrameHostImplWrapper fenced_frame_rfh(
      fenced_frame_test_helper().CreateFencedFrame(primary_rfh.get(),
                                                   fenced_frame_url));

  RenderWidgetHostImpl* fenced_frame_rwh =
      fenced_frame_rfh->GetRenderWidgetHost();
  RenderWidgetHostImpl* primary_rwh = primary_rfh->GetRenderWidgetHost();

  ASSERT_TRUE(fenced_frame_rwh->owner_delegate());
  ASSERT_TRUE(fenced_frame_rwh->GetView()->IsRenderWidgetHostViewChildFrame());

  input::RenderWidgetHostInputEventRouter* router =
      web_contents()->GetInputEventRouter();

  RenderWidgetHostMouseEventMonitor primary_monitor(primary_rwh);
  RenderWidgetHostMouseEventMonitor fenced_frame_monitor(fenced_frame_rwh);

  // 1. Simulate the request arriving from the fenced frame's renderer.
  // It should be ignored.
  fenced_frame_rwh->SetAutoscrollSelectionActiveInMainFrame(true);

  // Dispatch MouseUp to fenced frame.
  blink::WebMouseEvent mouse_up(
      blink::WebInputEvent::Type::kMouseUp, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_up.button = blink::WebPointerProperties::Button::kLeft;
  mouse_up.SetPositionInWidget(10, 20);
  gfx::PointF target_location(5, 8);

  router->DispatchMouseEvent(primary_rwh->GetView(),
                             fenced_frame_rwh->GetView(), mouse_up,
                             ui::LatencyInfo(), target_location);

  // Fenced frame should receive it at target_location.
  EXPECT_TRUE(fenced_frame_monitor.EventWasReceived());
  EXPECT_EQ(fenced_frame_monitor.event().PositionInWidget().x(), 5);
  EXPECT_EQ(fenced_frame_monitor.event().PositionInWidget().y(), 8);
  // Primary main frame should NOT receive it.
  EXPECT_FALSE(primary_monitor.EventWasReceived());

  // Reset monitors.
  fenced_frame_monitor.ResetEventReceived();
  primary_monitor.ResetEventReceived();

  // 2. The outermost main frame's request should still be honored.
  primary_rwh->SetAutoscrollSelectionActiveInMainFrame(true);

  router->DispatchMouseEvent(primary_rwh->GetView(),
                             fenced_frame_rwh->GetView(), mouse_up,
                             ui::LatencyInfo(), target_location);

  // Fenced frame should receive it at target_location.
  EXPECT_TRUE(fenced_frame_monitor.EventWasReceived());
  EXPECT_EQ(fenced_frame_monitor.event().PositionInWidget().x(), 5);
  EXPECT_EQ(fenced_frame_monitor.event().PositionInWidget().y(), 8);
  // Primary main frame should ALSO receive it, but at original coordinates.
  EXPECT_TRUE(primary_monitor.EventWasReceived());
  EXPECT_EQ(primary_monitor.event().PositionInWidget().x(), 10);
  EXPECT_EQ(primary_monitor.event().PositionInWidget().y(), 20);
}

// Test that WebContents::GetFocusedFrame includes results from a fenced
// frame's frame tree.
IN_PROC_BROWSER_TEST_F(FencedFrameMPArchBrowserTest,
                       FocusedFrameInFencedFrame) {
  ASSERT_TRUE(https_server()->Start());
  const GURL url = https_server()->GetURL("c.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), url));

  const GURL fenced_frame_url =
      https_server()->GetURL("c.test", "/fenced_frames/title1.html");
  RenderFrameHostImplWrapper fenced_frame_rfh(
      fenced_frame_test_helper().CreateFencedFrame(primary_main_frame_host(),
                                                   fenced_frame_url));

  // NavigateToURL sets initial focus, which is why GetFocusedFrame doesn't
  // start as null.
  EXPECT_EQ(web_contents()->GetFocusedFrame(), primary_main_frame_host());

  FrameFocusedObserver focus_observer(fenced_frame_rfh.get());
  // ExecJs runs with a user gesture which is needed for the fenced frame to be
  // allowed to take focus.
  EXPECT_TRUE(ExecJs(fenced_frame_rfh.get(), "window.focus()"));
  focus_observer.Wait();
  EXPECT_EQ(web_contents()->GetFocusedFrame(), fenced_frame_rfh.get());
}

class FocusChangedWatcher : public WebContentsObserver {
 public:
  explicit FocusChangedWatcher(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  void OnFocusChangedInPage(const FocusedNodeDetails& details) override {
    future_.SetValue(details);
  }

  const FocusedNodeDetails& Wait() { return future_.Get(); }
  bool observed() const { return future_.IsReady(); }

 private:
  base::test::TestFuture<FocusedNodeDetails> future_;
};

class FencedFrameMPArchBrowserTestWithEnforceFocusDisabled
    : public FencedFrameMPArchBrowserTest {
 public:
  FencedFrameMPArchBrowserTestWithEnforceFocusDisabled() {
    feature_list_.InitAndDisableFeature(features::kFencedFramesEnforceFocus);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Regression test for crbug.com/514519203.
// Verify that an unfocused fenced frame cannot trigger focused element changed
// notifications on the root view.
IN_PROC_BROWSER_TEST_F(FencedFrameMPArchBrowserTestWithEnforceFocusDisabled,
                       FencedFrameFocusedElementChangedWithoutFocus) {
  ASSERT_TRUE(https_server()->Start());
  const GURL url = https_server()->GetURL("c.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), url));

  // 1. Focus primary main frame input.
  {
    FocusChangedWatcher watcher(web_contents());
    ASSERT_TRUE(ExecJs(primary_main_frame_host(),
                       "const input = document.createElement('input');"
                       "input.id = 'primary_input';"
                       "document.body.appendChild(input);"
                       "input.focus();"));
    const FocusedNodeDetails& details = watcher.Wait();
    EXPECT_NE(details.editable_level, content::EditableLevel::kNotEditable);
  }

  // 2. Create fenced frame and add two inputs.
  const GURL fenced_frame_url =
      https_server()->GetURL("c.test", "/fenced_frames/title1.html");
  RenderFrameHostImplWrapper fenced_frame_rfh(
      fenced_frame_test_helper().CreateFencedFrame(primary_main_frame_host(),
                                                   fenced_frame_url));
  ASSERT_TRUE(ExecJs(fenced_frame_rfh.get(),
                     "const input1 = document.createElement('input');"
                     "input1.id = 'fenced_input1';"
                     "document.body.appendChild(input1);"
                     "const input2 = document.createElement('input');"
                     "input2.id = 'fenced_input2';"
                     "document.body.appendChild(input2);"));

  // 3. Focus fenced_input1 WITH user gesture.
  {
    FocusChangedWatcher watcher(web_contents());
    ASSERT_TRUE(ExecJs(fenced_frame_rfh.get(),
                       "document.getElementById('fenced_input1').focus();"));
    const FocusedNodeDetails& details = watcher.Wait();
    EXPECT_NE(details.editable_level, content::EditableLevel::kNotEditable);
  }

  // 4. Focus primary main frame input WITH user gesture.
  {
    FocusChangedWatcher watcher(web_contents());
    ASSERT_TRUE(ExecJs(primary_main_frame_host(),
                       "const input = document.createElement('input');"
                       "input.id = 'primary_input';"
                       "document.body.appendChild(input);"
                       "input.focus();"));
    const FocusedNodeDetails& details = watcher.Wait();
    EXPECT_NE(details.editable_level, content::EditableLevel::kNotEditable);
  }

  // Clear user activation on the fenced frame to ensure it doesn't have
  // transient user activation from step 3.
  static_cast<RenderFrameHostImpl*>(fenced_frame_rfh.get())
      ->ClearUserActivation();

  // 5. Try to focus fenced_input2 WITHOUT user gesture.
  // Fenced frame is NOT focused now.
  FocusChangedWatcher final_watcher(web_contents());
  ASSERT_TRUE(ExecJs(fenced_frame_rfh.get(),
                     "document.getElementById('fenced_input2').focus();",
                     EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Force a roundtrip to ensure any pending IPCs are processed.
  EXPECT_EQ(true, EvalJs(fenced_frame_rfh.get(), "true"));

  // If the bug is present, the unfocused fenced frame can still trigger
  // FocusedElementChanged, which would notify our observer.
  // We expect it to be ignored (after fix).
  EXPECT_FALSE(final_watcher.observed());
}

// Test that the initial navigation in a fenced frame, which navigates from the
// initial empty document, is not classified as a client redirect.
IN_PROC_BROWSER_TEST_F(FencedFrameMPArchBrowserTest,
                       InitialNavigationIsNotClientRedirect) {
  ASSERT_TRUE(https_server()->Start());
  const GURL url = https_server()->GetURL("c.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), url));

  FrameNavigateParamsCapturer capturer(web_contents());
  const GURL fenced_frame_url =
      https_server()->GetURL("c.test", "/fenced_frames/title1.html");
  fenced_frame_test_helper().CreateFencedFrame(primary_main_frame_host(),
                                               fenced_frame_url);
  capturer.Wait();
  EXPECT_EQ(capturer.urls()[0], fenced_frame_url);

  ASSERT_EQ(1U, capturer.transitions().size());
  // The transition used for the initial navigation in the fenced frame is not
  // classified as a client-side redirect.
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      capturer.transitions()[0],
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_AUTO_SUBFRAME)));
}

IN_PROC_BROWSER_TEST_F(FencedFrameMPArchBrowserTest_IsolateAllSites,
                       ProcessAllocationWithFullSiteIsolation) {
  ASSERT_TRUE(https_server()->Start());
  ASSERT_TRUE(AreAllSitesIsolatedForTesting());

  const GURL main_url = https_server()->GetURL("a.test", "/title1.html");
  const GURL same_site_fenced_frame_url =
      https_server()->GetURL("a.test", "/fenced_frames/title1.html");
  const GURL cross_site_fenced_frame_url =
      https_server()->GetURL("b.test", "/fenced_frames/title1.html");

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Empty fenced frame document should have a different site instance, but
  // should be in the same process as embedder.
  RenderFrameHost* fenced_frame_rfh =
      fenced_frame_test_helper().CreateFencedFrame(primary_main_frame_host(),
                                                   GURL());
  EXPECT_NE(fenced_frame_rfh->GetSiteInstance(),
            primary_main_frame_host()->GetSiteInstance());
  EXPECT_FALSE(fenced_frame_rfh->GetSiteInstance()->IsRelatedSiteInstance(
      primary_main_frame_host()->GetSiteInstance()));
  EXPECT_EQ(fenced_frame_rfh->GetProcess(),
            primary_main_frame_host()->GetProcess());

  // Same-site fenced frame document should be in the same process as embedder.
  fenced_frame_rfh = fenced_frame_test_helper().NavigateFrameInFencedFrameTree(
      fenced_frame_rfh, same_site_fenced_frame_url);
  EXPECT_NE(fenced_frame_rfh->GetSiteInstance(),
            primary_main_frame_host()->GetSiteInstance());
  EXPECT_FALSE(fenced_frame_rfh->GetSiteInstance()->IsRelatedSiteInstance(
      primary_main_frame_host()->GetSiteInstance()));
  EXPECT_EQ(fenced_frame_rfh->GetProcess(),
            primary_main_frame_host()->GetProcess());

  // Cross-site fenced frame document should be in a different process from its
  // embedder.
  fenced_frame_rfh = fenced_frame_test_helper().NavigateFrameInFencedFrameTree(
      fenced_frame_rfh, cross_site_fenced_frame_url);
  EXPECT_NE(fenced_frame_rfh->GetProcess(),
            primary_main_frame_host()->GetProcess());
}

IN_PROC_BROWSER_TEST_F(FencedFrameMPArchBrowserTest_IsolateAllSites,
                       CrossSiteFencedFramesShareProcess) {
  ASSERT_TRUE(https_server()->Start());
  ASSERT_TRUE(AreAllSitesIsolatedForTesting());

  const GURL main_url = https_server()->GetURL("a.test", "/title1.html");
  const GURL same_site_fenced_frame_url =
      https_server()->GetURL("a.test", "/fenced_frames/title1.html");
  const GURL cross_site_fenced_frame_url =
      https_server()->GetURL("b.test", "/fenced_frames/title1.html");

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Two fenced frames that are same-site with each other, and cross-site with
  // the embedder should be in the same process. This happens due to the
  // subframe process reuse policy which also applies to fenced frames (the
  // second fenced frame will try to reuse an existing process that is locked to
  // the same site).
  RenderFrameHost* ff_rfh_1 = fenced_frame_test_helper().CreateFencedFrame(
      primary_main_frame_host(), cross_site_fenced_frame_url);
  RenderFrameHost* ff_rfh_2 = fenced_frame_test_helper().CreateFencedFrame(
      primary_main_frame_host(), cross_site_fenced_frame_url);
  EXPECT_NE(ff_rfh_1->GetSiteInstance(), ff_rfh_2->GetSiteInstance());
  EXPECT_NE(ff_rfh_1->GetProcess(), primary_main_frame_host()->GetProcess());
  EXPECT_EQ(ff_rfh_1->GetProcess(), ff_rfh_2->GetProcess());

  // The cross-site fenced frame should be moved to the same process as embedder
  // when navigated to same-site (similar to before).
  ff_rfh_2 = fenced_frame_test_helper().NavigateFrameInFencedFrameTree(
      ff_rfh_2, same_site_fenced_frame_url);
  EXPECT_EQ(ff_rfh_2->GetProcess(), primary_main_frame_host()->GetProcess());
}

// Tests to ensure that the owner forced sandbox flags are set when a fenced
// frame is created, and are kept after the fenced frame is navigated
// to a page with a CSP sandbox header.
IN_PROC_BROWSER_TEST_F(FencedFrameMPArchBrowserTest,
                       EnsureSandboxFlagsEnforced) {
  ASSERT_TRUE(https_server()->Start());
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  ASSERT_TRUE(AreAllSitesIsolatedForTesting());

  const GURL main_url = https_server()->GetURL("a.test", "/title1.html");
  const GURL fenced_frame_url =
      https_server()->GetURL("a.test", "/fenced_frames/title1.html");

  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  RenderFrameHostImplWrapper primary_rfh(primary_main_frame_host());

  RenderFrameHostImplWrapper ff_rfh(
      fenced_frame_test_helper().CreateFencedFrame(primary_rfh.get(),
                                                   fenced_frame_url));

  EXPECT_TRUE(ff_rfh->IsSandboxed(blink::kFencedFrameForcedSandboxFlags));

  GURL new_fenced_frame_url =
      https_server()->GetURL("a.test", "/fenced_frames/sandbox_flags.html");
  RenderFrameHostImplWrapper new_fenced_frame_rfh(
      fenced_frame_test_helper().NavigateFrameInFencedFrameTree(
          ff_rfh.get(), new_fenced_frame_url));

  EXPECT_TRUE(!new_fenced_frame_rfh->IsErrorDocument());
  EXPECT_TRUE(
      new_fenced_frame_rfh->IsSandboxed(blink::kFencedFrameForcedSandboxFlags));
}

IN_PROC_BROWSER_TEST_F(FencedFrameMPArchBrowserTest,
                       CreateFencedFrameWhileInBackForwardCache) {
  if (!BackForwardCache::IsBackForwardCacheFeatureEnabled()) {
    LOG(ERROR) << "BackForwardCache must be enabled for this test.";
    return;
  }

  ASSERT_TRUE(https_server()->Start());
  ASSERT_TRUE(
      NavigateToURL(shell(), https_server()->GetURL("a.test", "/title1.html")));
  RenderFrameHostWrapper primary_rfh(primary_main_frame_host());
  ASSERT_TRUE(
      ExecJs(primary_rfh.get(),
             JsReplace(kAddIframeScript,
                       https_server()->GetURL("c.test", "/title1.html"))));
  RenderFrameHostWrapper iframe(
      static_cast<RenderFrameHostImpl*>(ChildFrameAt(primary_rfh.get(), 0)));

  ASSERT_TRUE(
      NavigateToURL(shell(), https_server()->GetURL("b.test", "/title1.html")));
  ASSERT_EQ(primary_rfh->GetLifecycleState(),
            RenderFrameHost::LifecycleState::kInBackForwardCache);
  ASSERT_EQ(iframe->GetLifecycleState(),
            RenderFrameHost::LifecycleState::kInBackForwardCache);

  mojo::PendingAssociatedRemote<blink::mojom::FencedFrameOwnerHost> remote;
  mojo::PendingAssociatedReceiver<blink::mojom::FencedFrameOwnerHost> receiver;
  receiver = remote.InitWithNewEndpointAndPassReceiver();

  auto remote_frame_interfaces =
      blink::mojom::RemoteFrameInterfacesFromRenderer::New();
  remote_frame_interfaces->frame_host_receiver =
      mojo::AssociatedRemote<blink::mojom::RemoteFrameHost>()
          .BindNewEndpointAndPassDedicatedReceiver();
  mojo::AssociatedRemote<blink::mojom::RemoteFrame> frame;
  std::ignore = frame.BindNewEndpointAndPassDedicatedReceiver();
  remote_frame_interfaces->frame = frame.Unbind();

  static_cast<RenderFrameHostImpl*>(iframe.get())
      ->CreateFencedFrame(
          std::move(receiver), std::move(remote_frame_interfaces),
          blink::RemoteFrameToken(), base::UnguessableToken::Create());
  EXPECT_TRUE(primary_rfh.WaitUntilRenderFrameDeleted());
  EXPECT_TRUE(iframe.IsRenderFrameDeleted());
}

// Tests that in a fenced frame the frame's isolation info correctly identifies
// its requests as main frame, but not outer most main frame.
IN_PROC_BROWSER_TEST_F(FencedFrameMPArchBrowserTest,
                       FencedFrameIsolationInfoRequests) {
  ASSERT_TRUE(https_server()->Start());

  net::IsolationInfo outer_most_frame_isolation_info;
  DidFinishNavigationObserver primary_frame_observer(
      web_contents(),
      base::BindLambdaForTesting([&](NavigationHandle* navigation_handle) {
        if (navigation_handle->GetNavigatingFrameType() !=
            FrameType::kPrimaryMainFrame) {
          return;
        }
        NavigationRequest* request = NavigationRequest::From(navigation_handle);
        outer_most_frame_isolation_info = request->GetIsolationInfo();
      }));
  ASSERT_TRUE(
      NavigateToURL(shell(), https_server()->GetURL("c.test", "/title1.html")));
  RenderFrameHostImplWrapper primary_rfh(primary_main_frame_host());

  net::IsolationInfo fenced_frame_isolation_info;
  DidFinishNavigationObserver fenced_frame_observer(
      web_contents(),
      base::BindLambdaForTesting([&](NavigationHandle* navigation_handle) {
        if (navigation_handle->GetNavigatingFrameType() !=
            FrameType::kFencedFrameRoot) {
          return;
        }
        NavigationRequest* request = NavigationRequest::From(navigation_handle);
        fenced_frame_isolation_info = request->GetIsolationInfo();
      }));

  const GURL fenced_frame_url =
      https_server()->GetURL("c.test", "/fenced_frames/title1.html");
  RenderFrameHostImplWrapper fenced_frame_rfh(
      fenced_frame_test_helper().CreateFencedFrame(primary_rfh.get(),
                                                   fenced_frame_url));

  ASSERT_FALSE(outer_most_frame_isolation_info.IsEmpty());
  ASSERT_FALSE(fenced_frame_isolation_info.IsEmpty());

  // Both frames' navigation should be considered a main frame request.
  EXPECT_TRUE(outer_most_frame_isolation_info.IsMainFrameRequest());
  EXPECT_TRUE(fenced_frame_isolation_info.IsMainFrameRequest());

  // But only the outer most main frame's navigation should be considered an
  // outer most main frame request.
  EXPECT_TRUE(outer_most_frame_isolation_info.IsOutermostMainFrameRequest());
  EXPECT_FALSE(fenced_frame_isolation_info.IsOutermostMainFrameRequest());
}

class FencedFrameWithSiteIsolationDisabledBrowserTest
    : public FencedFrameMPArchBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  FencedFrameWithSiteIsolationDisabledBrowserTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    if (GetParam()) {
      enabled_features.push_back(features::kIsolateFencedFrames);
    } else {
      disabled_features.push_back(features::kIsolateFencedFrames);
    }

    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  ~FencedFrameWithSiteIsolationDisabledBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    FencedFrameMPArchBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kDisableSiteIsolation);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         FencedFrameWithSiteIsolationDisabledBrowserTest,
                         testing::Bool(),
                         [](auto& info) {
                           return info.param ? "IsolatedFencedFrames"
                                             : "UnisolatedFencedFrames";
                         });

IN_PROC_BROWSER_TEST_P(FencedFrameWithSiteIsolationDisabledBrowserTest,
                       ProcessAllocationWithSiteIsolationDisabled) {
  ASSERT_TRUE(https_server()->Start());
  if (AreAllSitesIsolatedForTesting()) {
    LOG(ERROR) << "Site isolation should be disabled for this test.";
    return;
  }

  const GURL main_url = https_server()->GetURL("a.test", "/title1.html");
  const GURL same_site_fenced_frame_url =
      https_server()->GetURL("a.test", "/fenced_frames/title1.html");
  const GURL cross_site_fenced_frame_url =
      https_server()->GetURL("b.test", "/fenced_frames/title1.html");

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Empty fenced frame document should be in the same process as embedder.
  RenderFrameHost* fenced_frame_rfh =
      fenced_frame_test_helper().CreateFencedFrame(primary_main_frame_host(),
                                                   GURL());
  EXPECT_NE(fenced_frame_rfh->GetSiteInstance(),
            primary_main_frame_host()->GetSiteInstance());
  EXPECT_EQ(fenced_frame_rfh->GetProcess(),
            primary_main_frame_host()->GetProcess());

  // Same-site fenced frame document should be in the same process as embedder.
  fenced_frame_rfh = fenced_frame_test_helper().NavigateFrameInFencedFrameTree(
      fenced_frame_rfh, same_site_fenced_frame_url);
  EXPECT_EQ(fenced_frame_rfh->GetProcess(),
            primary_main_frame_host()->GetProcess());

  // Cross-site fenced frame document should be in the same process as the
  // embedder (with site isolation disabled).
  fenced_frame_rfh = fenced_frame_test_helper().NavigateFrameInFencedFrameTree(
      fenced_frame_rfh, cross_site_fenced_frame_url);
  EXPECT_EQ(fenced_frame_rfh->GetProcess(),
            primary_main_frame_host()->GetProcess());
}

IN_PROC_BROWSER_TEST_P(FencedFrameWithSiteIsolationDisabledBrowserTest,
                       ProcessAllocationWithDynamicIsolatedOrigin) {
  ASSERT_TRUE(https_server()->Start());
  if (AreAllSitesIsolatedForTesting()) {
    LOG(ERROR) << "Site isolation should be disabled for this test.";
    return;
  }
  const GURL main_url = https_server()->GetURL("a.test", "/title1.html");
  const GURL isolated_cross_site_fenced_frame_url =
      https_server()->GetURL("isolated.b.test", "/fenced_frames/title1.html");
  const GURL cross_site_fenced_frame_url =
      https_server()->GetURL("c.test", "/fenced_frames/title1.html");

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Start isolating "isolated.b.test".
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  policy->AddFutureIsolatedOrigins(
      {url::Origin::Create(isolated_cross_site_fenced_frame_url)},
      ChildProcessSecurityPolicy::IsolatedOriginSource::TEST);

  RenderFrameHost* ff_rfh_1 = fenced_frame_test_helper().CreateFencedFrame(
      primary_main_frame_host(), cross_site_fenced_frame_url);
  RenderFrameHost* ff_rfh_2 = fenced_frame_test_helper().CreateFencedFrame(
      primary_main_frame_host(), isolated_cross_site_fenced_frame_url);

  // The c.test fenced frame should share a process with the embedder, but
  // the isolated.b.test fenced frame should be in a different process.
  EXPECT_EQ(ff_rfh_1->GetProcess(), primary_main_frame_host()->GetProcess());
  EXPECT_NE(ff_rfh_2->GetProcess(), ff_rfh_1->GetProcess());

  // When we navigate the second fenced frame to c.test, it should now share
  // its process with the embedder.
  ff_rfh_2 = fenced_frame_test_helper().NavigateFrameInFencedFrameTree(
      ff_rfh_2, cross_site_fenced_frame_url);
  EXPECT_EQ(ff_rfh_2->GetProcess(), primary_main_frame_host()->GetProcess());
}

IN_PROC_BROWSER_TEST_P(FencedFrameWithSiteIsolationDisabledBrowserTest,
                       ProcessAllocationWhenRootIsIsolated) {
  ASSERT_TRUE(https_server()->Start());
  if (AreAllSitesIsolatedForTesting()) {
    LOG(ERROR) << "Site isolation should be disabled for this test.";
    return;
  }

  const GURL isolated_url =
      https_server()->GetURL("isolated.b.test", "/title1.html");
  const GURL fenced_frame_url =
      https_server()->GetURL("a.test", "/fenced_frames/title1.html");
  const GURL iframe_url = https_server()->GetURL("a.test", "/title1.html");

  // Start isolating "isolated.b.test".
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  policy->AddFutureIsolatedOrigins(
      {url::Origin::Create(isolated_url)},
      ChildProcessSecurityPolicy::IsolatedOriginSource::TEST);

  EXPECT_TRUE(NavigateToURL(shell(), isolated_url));
  RenderFrameHost* ff_rfh = fenced_frame_test_helper().CreateFencedFrame(
      primary_main_frame_host(), GURL());
  EXPECT_EQ(ff_rfh->GetProcess(), primary_main_frame_host()->GetProcess());
  ff_rfh = fenced_frame_test_helper().NavigateFrameInFencedFrameTree(
      ff_rfh, fenced_frame_url);
  EXPECT_NE(ff_rfh->GetProcess(), primary_main_frame_host()->GetProcess());

  EXPECT_TRUE(ExecJs(primary_main_frame_host(),
                     JsReplace(kAddIframeScript, iframe_url)));
  RenderFrameHostImpl* iframe = static_cast<RenderFrameHostImpl*>(
      ChildFrameAt(primary_main_frame_host(), 1));
  ASSERT_TRUE(iframe && iframe->GetParent()->IsInPrimaryMainFrame());
  EXPECT_EQ(iframe->GetProcess(), ff_rfh->GetProcess());
}

IN_PROC_BROWSER_TEST_P(FencedFrameWithSiteIsolationDisabledBrowserTest,
                       ProcessAllocationForNestedFencedFrame) {
  ASSERT_TRUE(https_server()->Start());
  if (AreAllSitesIsolatedForTesting()) {
    LOG(ERROR) << "Site isolation should be disabled for this test.";
    return;
  }

  const GURL main_url = https_server()->GetURL("a.test", "/title1.html");
  const GURL fenced_frame_url =
      https_server()->GetURL("b.test", "/fenced_frames/title1.html");

  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  RenderFrameHost* ff_rfh = fenced_frame_test_helper().CreateFencedFrame(
      primary_main_frame_host(), fenced_frame_url);
  EXPECT_EQ(ff_rfh->GetProcess(), primary_main_frame_host()->GetProcess());
  RenderFrameHost* nested_ff_rfh =
      fenced_frame_test_helper().CreateFencedFrame(ff_rfh, fenced_frame_url);
  EXPECT_EQ(ff_rfh->GetProcess(), nested_ff_rfh->GetProcess());
}

IN_PROC_BROWSER_TEST_P(FencedFrameWithSiteIsolationDisabledBrowserTest,
                       ProcessAllocationForFencedFrameInIsolatedPopup) {
  ASSERT_TRUE(https_server()->Start());

  const GURL main_url = https_server()->GetURL("a.test", "/title1.html");
  const GURL fenced_frame_url =
      https_server()->GetURL("b.test", "/fenced_frames/title1.html");
  const GURL popup_url =
      https_server()->GetURL("isolated.c.test", "/title2.html");

  // Start isolating "isolated.c.test".
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  policy->AddFutureIsolatedOrigins(
      {url::Origin::Create(popup_url)},
      ChildProcessSecurityPolicy::IsolatedOriginSource::TEST);

  ASSERT_TRUE(NavigateToURL(shell(), main_url));

  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(
      ExecJs(primary_main_frame_host(), "popup = window.open('about:blank');"));
  Shell* popup = new_shell_observer.GetShell();
  EXPECT_TRUE(NavigateToURLFromRenderer(popup, popup_url));
  RenderFrameHost* popup_rfh = popup->web_contents()->GetPrimaryMainFrame();
  ASSERT_EQ(
      primary_main_frame_host()->GetSiteInstance()->GetBrowsingInstanceId(),
      popup_rfh->GetSiteInstance()->GetBrowsingInstanceId());

  RenderFrameHost* ff_rfh =
      fenced_frame_test_helper().CreateFencedFrame(popup_rfh, fenced_frame_url);
  ASSERT_EQ(ff_rfh->GetProcess(), primary_main_frame_host()->GetProcess());
}

class FencedFrameIsolatedSandboxedIframesBrowserTest
    : public FencedFrameMPArchBrowserTest_IsolateAllSites,
      public ::testing::WithParamInterface<bool> {
 public:
  FencedFrameIsolatedSandboxedIframesBrowserTest() {
    if (GetParam()) {
      // Run test with both isolation features enabled.
      feature_list_.InitWithFeatures({blink::features::kIsolateSandboxedIframes,
                                      features::kIsolateFencedFrames},
                                     {});
    } else {
      // Run test with only isolated sandboxed iframes enabled.
      feature_list_.InitWithFeatures(
          {blink::features::kIsolateSandboxedIframes},
          {features::kIsolateFencedFrames});
    }
  }
  ~FencedFrameIsolatedSandboxedIframesBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// This is a basic test to make sure the kIsolateSandboxedIframes (OOPSIF) load
// properly with FencedFrames, both when FencedFrames isolation mode is off and
// on. The OOPSIF frame is sandboxed due to a CSP sandbox header delivered with
// the page loaded into the FencedFrame. The FencedFrame element doesn't support
// the 'sandbox' attribute directly, nor can it be loaded inside an OOPSIF since
// OOPSIFs by definition disallow same-origin, whereas the FencedFrame element
// will only load inside a sandbox if allow-same-origin is specified on the
// sandbox. See kFencedFrameMandatoryUnsandboxedFlags.
IN_PROC_BROWSER_TEST_P(FencedFrameIsolatedSandboxedIframesBrowserTest,
                       CSP_Mainframe) {
  bool testing_with_isolate_fenced_frames = GetParam();
  ASSERT_TRUE(AreAllSitesIsolatedForTesting());
  ASSERT_TRUE(https_server()->Start());

  const GURL main_url = https_server()->GetURL("a.test", "/title1.html");
  const GURL fenced_frame_url =
      https_server()->GetURL("a.test", "/fenced_frames/sandbox_flags.html");

  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  RenderFrameHostImpl* ff_rfh = static_cast<RenderFrameHostImpl*>(
      fenced_frame_test_helper().CreateFencedFrame(primary_main_frame_host(),
                                                   fenced_frame_url));
  EXPECT_TRUE(ff_rfh->IsSandboxed(blink::kFencedFrameForcedSandboxFlags));

  EXPECT_NE(ff_rfh->GetProcess(), primary_main_frame_host()->GetProcess());
  // This next result may seem weird, but the is_fenced() bit only gets
  // set if SiteIsolationPolicy::IsProcessIsolationForFencedFramesEnabled()
  // returns true in SiteInstanceImpl::CreateForFencedFrame().
  // The bit is picked up from the fenced frame's BrowsingInstance's
  // IsolationContext when the SiteInfo is created.
  EXPECT_EQ(testing_with_isolate_fenced_frames,
            ff_rfh->GetSiteInstance()->GetSiteInfo().is_fenced());
  EXPECT_TRUE(ff_rfh->GetSiteInstance()->GetSecurityPrincipal().IsSandboxed());
  EXPECT_NE(
      primary_main_frame_host()->GetSiteInstance()->GetBrowsingInstanceId(),
      ff_rfh->GetSiteInstance()->GetBrowsingInstanceId());
}

// Similar to CSP_Mainframe, but in this test OOPSIF doesn't isolate the fenced
// frames, while kIsolateFencedFrames does.
IN_PROC_BROWSER_TEST_P(FencedFrameIsolatedSandboxedIframesBrowserTest,
                       Non_CSP_Mainframe) {
  bool testing_with_isolate_fenced_frames = GetParam();
  ASSERT_TRUE(AreAllSitesIsolatedForTesting());
  ASSERT_TRUE(https_server()->Start());

  const GURL main_url = https_server()->GetURL("a.test", "/title1.html");
  const GURL fenced_frame_url =
      https_server()->GetURL("a.test", "/fenced_frames/title1.html");

  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  RenderFrameHostImpl* ff_rfh = static_cast<RenderFrameHostImpl*>(
      fenced_frame_test_helper().CreateFencedFrame(primary_main_frame_host(),
                                                   fenced_frame_url));
  EXPECT_TRUE(ff_rfh->IsSandboxed(blink::kFencedFrameForcedSandboxFlags));

  EXPECT_EQ(testing_with_isolate_fenced_frames,
            ff_rfh->GetSiteInstance()->GetSiteInfo().is_fenced());
  if (testing_with_isolate_fenced_frames) {
    EXPECT_NE(ff_rfh->GetProcess(), primary_main_frame_host()->GetProcess());
  } else {
    EXPECT_EQ(ff_rfh->GetProcess(), primary_main_frame_host()->GetProcess());
  }
  EXPECT_FALSE(ff_rfh->GetSiteInstance()->GetSecurityPrincipal().IsSandboxed());
  EXPECT_NE(
      primary_main_frame_host()->GetSiteInstance()->GetBrowsingInstanceId(),
      ff_rfh->GetSiteInstance()->GetBrowsingInstanceId());
}

// A test to confirm that a FencedFrame fails to create inside a CSP sandbox
// frame without allow-same-origin. This test should fail regardless of the
// state of kIsolateSandboxedIframes or kIsolateFencedFrames.
IN_PROC_BROWSER_TEST_P(FencedFrameIsolatedSandboxedIframesBrowserTest,
                       NoFencedFramesInIsolatedSandboxedIframes) {
  ASSERT_TRUE(AreAllSitesIsolatedForTesting());
  ASSERT_TRUE(https_server()->Start());

  // Load CSP sandboxed frame as mainframe.
  const GURL main_url =
      https_server()->GetURL("a.test", "/fenced_frames/sandbox_flags.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  EXPECT_TRUE(primary_main_frame_host()
                  ->GetSiteInstance()
                  ->GetSecurityPrincipal()
                  .IsSandboxed());

  // Try to load FencedFrame inside the CSP sandboxed frame.
  const GURL fenced_frame_url =
      https_server()->GetURL("a.test", "/fenced_frames/title1.html");
  // Extracted from CreateFencedFrame, which doesn't expect to fail.
  constexpr char kAddFencedFrameScript[] = R"({
    const fenced_frame = document.createElement('fencedframe');
    document.body.appendChild(fenced_frame);
  })";
  size_t previous_fenced_frame_count =
      primary_main_frame_host()->GetFencedFrames().size();
  EXPECT_EQ(0U, previous_fenced_frame_count);
  // The following attempt to create a fenced frame is expected to fail since
  // it would otherwise be contained in a sandbox that doesn't have the
  // allow-same-origin attribute. See kFencedFrameMandatoryUnsandboxedFlags.
  EXPECT_FALSE(
      ExecJs(primary_main_frame_host(), kAddFencedFrameScript,
             EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE |
                 EvalJsOptions::EXECUTE_SCRIPT_HONOR_JS_CONTENT_SETTINGS));
  EXPECT_EQ(previous_fenced_frame_count,
            primary_main_frame_host()->GetFencedFrames().size());
}

class FencedFrameProcessIsolationBrowserTest
    : public FencedFrameMPArchBrowserTest {
 public:
  FencedFrameProcessIsolationBrowserTest() {
    feature_list_.InitWithFeatures({features::kIsolateFencedFrames}, {});
  }
  ~FencedFrameProcessIsolationBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(FencedFrameProcessIsolationBrowserTest, BasicTest) {
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  ASSERT_TRUE(AreAllSitesIsolatedForTesting());
  ASSERT_TRUE(https_server()->Start());
  const GURL main_url = https_server()->GetURL("a.test", "/title1.html");
  const GURL fenced_frame_url =
      https_server()->GetURL("a.test", "/fenced_frames/title1.html");

  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  RenderFrameHostImpl* ff_rfh = static_cast<RenderFrameHostImpl*>(
      fenced_frame_test_helper().CreateFencedFrame(primary_main_frame_host(),
                                                   fenced_frame_url));
  EXPECT_TRUE(ff_rfh->GetSiteInstance()->GetSiteInfo().is_fenced());
  EXPECT_NE(ff_rfh->GetProcess(), primary_main_frame_host()->GetProcess());
  EXPECT_NE(
      primary_main_frame_host()->GetSiteInstance()->GetBrowsingInstanceId(),
      ff_rfh->GetSiteInstance()->GetBrowsingInstanceId());
}

// Tests that fenced frames that are same-origin with each other are put in
// the same process.
IN_PROC_BROWSER_TEST_F(FencedFrameProcessIsolationBrowserTest,
                       SameOriginFencedFramesArePutInTheSameProcess) {
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  ASSERT_TRUE(AreAllSitesIsolatedForTesting());
  ASSERT_TRUE(https_server()->Start());

  const GURL main_url = https_server()->GetURL("a.test", "/title1.html");
  const GURL fenced_frame_url =
      https_server()->GetURL("b.test", "/fenced_frames/title1.html");

  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  RenderFrameHostImpl* ff_rfh_1 = static_cast<RenderFrameHostImpl*>(
      fenced_frame_test_helper().CreateFencedFrame(primary_main_frame_host(),
                                                   fenced_frame_url));
  RenderFrameHostImpl* ff_rfh_2 = static_cast<RenderFrameHostImpl*>(
      fenced_frame_test_helper().CreateFencedFrame(primary_main_frame_host(),
                                                   fenced_frame_url));

  EXPECT_NE(ff_rfh_1->GetProcess(), primary_main_frame_host()->GetProcess());
  EXPECT_NE(ff_rfh_1->GetSiteInstance(), ff_rfh_2->GetSiteInstance());
  EXPECT_EQ(ff_rfh_1->GetProcess(), ff_rfh_2->GetProcess());
}

// Tests that fenced frames that are cross-origin with each other are put in
// different processes.
IN_PROC_BROWSER_TEST_F(FencedFrameProcessIsolationBrowserTest,
                       CrossOriginFencedFramesArePutInDifferentProcesses) {
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  ASSERT_TRUE(AreAllSitesIsolatedForTesting());
  ASSERT_TRUE(https_server()->Start());

  const GURL main_url = https_server()->GetURL("a.test", "/title1.html");
  const GURL ff_url_1 =
      https_server()->GetURL("b.test", "/fenced_frames/title1.html");
  const GURL ff_url_2 =
      https_server()->GetURL("c.test", "/fenced_frames/title1.html");

  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  RenderFrameHostImpl* ff_rfh_1 = static_cast<RenderFrameHostImpl*>(
      fenced_frame_test_helper().CreateFencedFrame(primary_main_frame_host(),
                                                   ff_url_1));
  RenderFrameHostImpl* ff_rfh_2 = static_cast<RenderFrameHostImpl*>(
      fenced_frame_test_helper().CreateFencedFrame(primary_main_frame_host(),
                                                   ff_url_2));

  EXPECT_NE(ff_rfh_1->GetProcess(), primary_main_frame_host()->GetProcess());
  EXPECT_NE(ff_rfh_2->GetProcess(), primary_main_frame_host()->GetProcess());
  EXPECT_NE(ff_rfh_1->GetProcess(), ff_rfh_2->GetProcess());
}

// Tests that a subframe inside a primary page is allocated to a separate
// process from a subframe inside a fenced frame.
IN_PROC_BROWSER_TEST_F(FencedFrameProcessIsolationBrowserTest,
                       SubframeIsolation) {
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  ASSERT_TRUE(AreAllSitesIsolatedForTesting());
  ASSERT_TRUE(https_server()->Start());

  const GURL main_url = https_server()->GetURL("a.test", "/title1.html");
  const GURL ff_url =
      https_server()->GetURL("b.test", "/fenced_frames/title1.html");
  const GURL subframe_url = https_server()->GetURL("c.test", "/title2.html");

  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  RenderFrameHostImpl* ff_rfh = static_cast<RenderFrameHostImpl*>(
      fenced_frame_test_helper().CreateFencedFrame(primary_main_frame_host(),
                                                   ff_url));

  // Add iframe in primary main frame.
  EXPECT_TRUE(ExecJs(primary_main_frame_host(),
                     JsReplace(kAddIframeScript, subframe_url)));
  RenderFrameHost* primary_subframe =
      ChildFrameAt(primary_main_frame_host(), 0);

  // Add iframe in fenced frame.
  EXPECT_TRUE(ExecJs(ff_rfh, JsReplace(kAddIframeScript, subframe_url)));
  RenderFrameHost* ff_subframe = ChildFrameAt(ff_rfh, 0);

  // Both subframes should be in separate processes (despite being same-site).
  EXPECT_NE(primary_subframe->GetProcess(), ff_subframe->GetProcess());
  EXPECT_NE(primary_subframe->GetSiteInstance(),
            ff_subframe->GetSiteInstance());
  EXPECT_FALSE(static_cast<RenderFrameHostImpl*>(primary_subframe)
                   ->GetSiteInstance()
                   ->GetSiteInfo()
                   .is_fenced());
  EXPECT_TRUE(static_cast<RenderFrameHostImpl*>(ff_subframe)
                  ->GetSiteInstance()
                  ->GetSiteInfo()
                  .is_fenced());
}

// Tests process assignment in the following scenario:
// a.com
//   <fencedframe src=a.com>
//     <iframe src=a.com>
//       <fencedframe src=a.com>
IN_PROC_BROWSER_TEST_F(FencedFrameProcessIsolationBrowserTest,
                       NestedFencedFrames) {
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  ASSERT_TRUE(AreAllSitesIsolatedForTesting());
  ASSERT_TRUE(https_server()->Start());

  const GURL main_url = https_server()->GetURL("a.test", "/title1.html");
  const GURL subframe_url =
      https_server()->GetURL("a.test", "/fenced_frames/title0.html");
  const GURL fenced_frame_url =
      https_server()->GetURL("a.test", "/fenced_frames/title1.html");

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Create outer fenced frame and add same-origin subframe.
  RenderFrameHostImpl* outer_ff_rfh = static_cast<RenderFrameHostImpl*>(
      fenced_frame_test_helper().CreateFencedFrame(primary_main_frame_host(),
                                                   fenced_frame_url));
  EXPECT_TRUE(ExecJs(outer_ff_rfh, JsReplace(kAddIframeScript, subframe_url)));
  RenderFrameHost* outer_ff_subframe = ChildFrameAt(outer_ff_rfh, 0);

  // Create nested fenced frame.
  RenderFrameHostImpl* inner_ff_rfh = static_cast<RenderFrameHostImpl*>(
      fenced_frame_test_helper().CreateFencedFrame(outer_ff_subframe,
                                                   fenced_frame_url));

  EXPECT_EQ(outer_ff_rfh->GetSiteInstance(),
            outer_ff_subframe->GetSiteInstance());
  EXPECT_NE(outer_ff_subframe->GetSiteInstance(),
            inner_ff_rfh->GetSiteInstance());

  // All frames will share the same process (except the primary main frame).
  EXPECT_NE(primary_main_frame_host()->GetProcess(),
            outer_ff_rfh->GetProcess());
  EXPECT_EQ(outer_ff_rfh->GetProcess(), outer_ff_subframe->GetProcess());
  EXPECT_EQ(outer_ff_subframe->GetProcess(), inner_ff_rfh->GetProcess());
}

// Tests that error pages inside fenced frames are process-isolated from the
// embedding page.
IN_PROC_BROWSER_TEST_F(FencedFrameProcessIsolationBrowserTest, ErrorPage) {
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  ASSERT_TRUE(AreAllSitesIsolatedForTesting());
  ASSERT_TRUE(https_server()->Start());

  const GURL main_url = https_server()->GetURL("a.test", "/title1.html");
  const GURL fenced_frame_url =
      https_server()->GetURL("a.test", "/title2.html");

  ASSERT_TRUE(NavigateToURL(shell(), main_url));

  // Loading the fenced frame should fail due to the absence of a
  // "Supports-Loading-Mode" header.
  RenderFrameHostImpl* ff_rfh = static_cast<RenderFrameHostImpl*>(
      fenced_frame_test_helper().CreateFencedFrame(
          primary_main_frame_host(), fenced_frame_url, net::ERR_ABORTED));
  ASSERT_NE(ff_rfh, nullptr);
  EXPECT_TRUE(ff_rfh->IsErrorDocument());

  const SiteInfo& ff_site_info = ff_rfh->GetSiteInstance()->GetSiteInfo();
  EXPECT_TRUE(ff_site_info.is_error_page());
  EXPECT_TRUE(ff_site_info.is_fenced());

  EXPECT_NE(ff_rfh->GetProcess(), primary_main_frame_host()->GetProcess());

  // Create another fenced frame that loads an error page.
  RenderFrameHostImpl* ff_rfh_2 = static_cast<RenderFrameHostImpl*>(
      fenced_frame_test_helper().CreateFencedFrame(
          primary_main_frame_host(), fenced_frame_url, net::ERR_ABORTED));
  ASSERT_NE(ff_rfh_2, nullptr);
  // Both fenced frame error pages should share a process.
  EXPECT_EQ(ff_rfh_2->GetProcess(), ff_rfh->GetProcess());
}

namespace {

enum class FrameTypeWithOrigin {
  kSameOriginIframe,
  kCrossOriginIframe,
  kSameOriginFencedFrame,
  kCrossOriginFencedFrame,
};

const std::vector<FrameTypeWithOrigin> kTestParameters[] = {
    {},

    {FrameTypeWithOrigin::kSameOriginIframe},
    {FrameTypeWithOrigin::kCrossOriginIframe},
    {FrameTypeWithOrigin::kSameOriginIframe,
     FrameTypeWithOrigin::kSameOriginIframe},
    {FrameTypeWithOrigin::kSameOriginIframe,
     FrameTypeWithOrigin::kCrossOriginIframe},
    {FrameTypeWithOrigin::kCrossOriginIframe,
     FrameTypeWithOrigin::kSameOriginIframe},
    {FrameTypeWithOrigin::kCrossOriginIframe,
     FrameTypeWithOrigin::kCrossOriginIframe},

    {FrameTypeWithOrigin::kSameOriginFencedFrame},
    {FrameTypeWithOrigin::kCrossOriginFencedFrame},
    {FrameTypeWithOrigin::kSameOriginFencedFrame,
     FrameTypeWithOrigin::kSameOriginIframe},
    {FrameTypeWithOrigin::kSameOriginFencedFrame,
     FrameTypeWithOrigin::kCrossOriginIframe},
    {FrameTypeWithOrigin::kCrossOriginFencedFrame,
     FrameTypeWithOrigin::kSameOriginIframe},
    {FrameTypeWithOrigin::kCrossOriginFencedFrame,
     FrameTypeWithOrigin::kCrossOriginIframe}};

static std::string TestParamToString(
    ::testing::TestParamInfo<std::vector<FrameTypeWithOrigin>> param_info) {
  std::string out = "Top_";
  for (const auto& frame_type : param_info.param) {
    switch (frame_type) {
      case FrameTypeWithOrigin::kSameOriginIframe:
        out += "SameI_";
        break;
      case FrameTypeWithOrigin::kCrossOriginIframe:
        out += "CrossI_";
        break;
      case FrameTypeWithOrigin::kSameOriginFencedFrame:
        out += "SameF_";
        break;
      case FrameTypeWithOrigin::kCrossOriginFencedFrame:
        out += "CrossF_";
        break;
    }
  }
  return out;
}

const char* kSameOriginHostName = "a.test";
const char* kCrossOriginHostName = "b.test";

const char* GetHostNameForFrameType(FrameTypeWithOrigin type) {
  switch (type) {
    case FrameTypeWithOrigin::kSameOriginIframe:
      return kSameOriginHostName;
    case FrameTypeWithOrigin::kCrossOriginIframe:
      return kCrossOriginHostName;
    case FrameTypeWithOrigin::kSameOriginFencedFrame:
      return kSameOriginHostName;
    case FrameTypeWithOrigin::kCrossOriginFencedFrame:
      return kCrossOriginHostName;
  }
}

bool IsFencedFrameType(FrameTypeWithOrigin type) {
  switch (type) {
    case FrameTypeWithOrigin::kSameOriginIframe:
      return false;
    case FrameTypeWithOrigin::kCrossOriginIframe:
      return false;
    case FrameTypeWithOrigin::kSameOriginFencedFrame:
      return true;
    case FrameTypeWithOrigin::kCrossOriginFencedFrame:
      return true;
  }
}

}  // namespace

class FencedFrameNestedFrameBrowserTest
    : public FencedFrameBrowserTestBase,
      public testing::WithParamInterface<std::vector<FrameTypeWithOrigin>> {
 protected:
  FencedFrameNestedFrameBrowserTest() = default;

  RenderFrameHostImpl* LoadNestedFrame() {
    const GURL main_url =
        https_server()->GetURL(kSameOriginHostName, "/title1.html");
    EXPECT_TRUE(NavigateToURL(shell(), main_url));
    RenderFrameHostImpl* frame = static_cast<RenderFrameHostImpl*>(
        web_contents()->GetPrimaryMainFrame());
    int depth = 0;
    for (const auto& type : GetParam()) {
      ++depth;
      frame = CreateFrame(frame, type, depth);
    }
    return frame;
  }

  bool IsInFencedFrameTest() {
    for (const auto& type : GetParam()) {
      if (IsFencedFrameType(type))
        return true;
    }
    return false;
  }

 private:
  RenderFrameHostImpl* CreateFrame(RenderFrameHostImpl* parent,
                                   FrameTypeWithOrigin type,
                                   int depth) {
    const GURL url = https_server()->GetURL(
        GetHostNameForFrameType(type),
        "/fenced_frames/title1.html?depth=" + base::NumberToString(depth));

    if (IsFencedFrameType(type)) {
      return static_cast<RenderFrameHostImpl*>(
          fenced_frame_test_helper().CreateFencedFrame(parent, url));
    }
    EXPECT_TRUE(ExecJs(parent, JsReplace(kAddIframeScript, url)));

    return static_cast<RenderFrameHostImpl*>(ChildFrameAt(parent, 0));
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(FencedFrameNestedFrameBrowserTest,
                       IsNestedWithinFencedFrame) {
  RenderFrameHostImpl* rfh = LoadNestedFrame();
  EXPECT_EQ(IsInFencedFrameTest(), rfh->IsNestedWithinFencedFrame());
}

INSTANTIATE_TEST_SUITE_P(FencedFrameNestedFrameBrowserTest,
                         FencedFrameNestedFrameBrowserTest,
                         testing::ValuesIn(kTestParameters),
                         TestParamToString);

// TODO(domfarolino): Rename this.
class FencedFrameParameterizedBrowserTest : public FencedFrameBrowserTestBase {
 public:
  FencedFrameParameterizedBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kFencedFrames, {}},
         {net::features::kThirdPartyStoragePartitioning, {}},
         {features::kPrivacySandboxAdsAPIsOverride, {}},
         {blink::features::kAllowURNsInIframes, {}},
         {blink::features::kDisplayWarningDeprecateURNIframesUseFencedFrames,
          {}},
         {features::kBackForwardCache, {}},
         // This feature allows `runAdAuction()`'s promise to resolve to a
         // `FencedFrameConfig` object upon developer request.
         {blink::features::kFencedFramesAPIChanges, {}},
         {blink::features::kFencedFramesAutomaticBeaconCredentials, {}},
         {blink::features::kFencedFramesLocalUnpartitionedDataAccess, {}},
         {blink::features::kFencedFramesCrossOriginEventReporting, {}},
         {blink::features::kFencedFramesReportEventHeaderChanges, {}},
         {blink::features::kFencedFramesCrossOriginAutomaticBeaconData, {}}},
        {/* disabled_features */});
  }

  ~FencedFrameParameterizedBrowserTest() override {
    // Shutdown the server explicitly so that there is no race with the
    // destruction of cookie_headers_map_ and invocation of RequestMonitor.
    if (https_server()->Started()) {
      EXPECT_TRUE(https_server()->ShutdownAndWaitUntilComplete());
    }
  }

  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(FencedFrameParameterizedBrowserTest,
                       RTCPeerConnectionDisabled) {
  GURL main_url(https_server()->GetURL("a.test", "/fenced_frames/title1.html"));
  GURL fenced_frame_url =
      https_server()->GetURL("a.test", "/fenced_frames/title1.html");

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  RenderFrameHost* primary_rfh = primary_main_frame_host();
  RenderFrameHost* fenced_frame_host =
      fenced_frame_test_helper().CreateFencedFrame(primary_rfh,
                                                   fenced_frame_url);

  // Copied from https://webrtc.org/getting-started/peer-connections.
  // The contents of the configuration object doesn't matter here,
  // because construction should fail before the information becomes
  // relevant.
  auto result = EvalJs(fenced_frame_host, R"(
    const configuration = {
      'iceServers': [{'urls': 'stun:stun.example.com:19302'}]
    };
    const peerConnection = new RTCPeerConnection(configuration);
  )");

  EXPECT_THAT(result,
              EvalJsResult::ErrorIs(testing::HasSubstr(
                  "Failed to construct 'RTCPeerConnection': "
                  "RTCPeerConnection is not allowed in fenced frames.")));
}

namespace {
class InsecureContentTestContentBrowserClient
    : public ContentBrowserTestContentBrowserClient {
 public:
  void OverrideWebPreferences(WebContents* web_contents,
                              SiteInstance& main_frame_site,
                              blink::web_pref::WebPreferences* prefs) override {
    // Browser will both run and display insecure content.
    prefs->allow_running_insecure_content = true;
  }
};
}  // namespace



namespace {
class TestJavaScriptDialogManager : public JavaScriptDialogManager,
                                    public WebContentsDelegate {
 public:
  TestJavaScriptDialogManager() = default;
  ~TestJavaScriptDialogManager() override = default;
  // WebContentsDelegate overrides
  JavaScriptDialogManager* GetJavaScriptDialogManager(
      WebContents* source) override {
    return this;
  }

  // JavaScriptDialogManager overrides
  void RunJavaScriptDialog(WebContents* web_contents,
                           RenderFrameHost* render_frame_host,
                           JavaScriptDialogType dialog_type,
                           const std::u16string& message_text,
                           const std::u16string& default_prompt_text,
                           DialogClosedCallback callback,
                           bool* did_suppress_message) override {}
  void RunBeforeUnloadDialog(WebContents* web_contents,
                             RenderFrameHost* render_frame_host,
                             bool is_reload,
                             DialogClosedCallback callback) override {}
  void CancelDialogs(WebContents* web_contents, bool reset_state) override {
    cancel_dialogs_called_ = true;
  }

  bool cancel_dialogs_called() { return cancel_dialogs_called_; }

 private:
  bool cancel_dialogs_called_ = false;
};
}  // namespace

// Test that navigation in fenced frame happens regardless of dialogs.
// It should also keep the dialogs as-is.

IN_PROC_BROWSER_TEST_F(FencedFrameParameterizedBrowserTest,
                       NavigateUnfencedTopAndGoBack) {
  GURL main_url(https_server()->GetURL("a.test", "/fenced_frames/title1.html"));
  GURL fenced_frame_url =
      https_server()->GetURL("a.test", "/fenced_frames/title1.html");
  TestNavigationObserver load_observer(web_contents());

  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  load_observer.Wait();

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  RenderFrameHost* primary_rfh = primary_main_frame_host();
  std::ignore = fenced_frame_test_helper().CreateFencedFrame(
      primary_rfh, fenced_frame_url, net::OK,
      blink::FencedFrame::DeprecatedFencedFrameMode::kOpaqueAds);

  auto* fenced_frame = GetFencedFrameRootNode(root->child_at(0));

  GURL new_main_url(https_server()->GetURL("b.test", "/hello.html"));
  // Now let's try to use unfencedTop and come back to the page with the fenced
  // frame.
  TestFrameNavigationObserver observer(root);
  EXPECT_TRUE(ExecJs(fenced_frame, JsReplace("window.open($1, '_unfencedTop');",
                                             new_main_url)));
  observer.Wait();
  EXPECT_EQ(2, root->navigator().controller().GetEntryCount());
  EXPECT_EQ(new_main_url, root->current_frame_host()->GetLastCommittedURL());

  // Go back.
  {
    TestNavigationObserver back_load_observer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(root, "history.back();"));
    back_load_observer.Wait();
  }
  EXPECT_EQ(2, root->navigator().controller().GetEntryCount());
  EXPECT_EQ(main_url, root->current_frame_host()->GetLastCommittedURL());

  if (BackForwardCache::IsBackForwardCacheFeatureEnabled()) {
    // When bfcache is enabled, the fenced frame should still be there after we
    // go back.
    EXPECT_EQ(1U, root->child_count());
    fenced_frame = GetFencedFrameRootNode(root->child_at(0));
    EXPECT_TRUE(fenced_frame->IsFencedFrameRoot());
    EXPECT_TRUE(fenced_frame->IsInFencedFrameTree());
    EXPECT_EQ(fenced_frame_url, fenced_frame->current_url());
  } else {
    // When bfcache is disabled, the fenced frame should no longer exist when we
    // go back, because it was created programmatically.
    EXPECT_EQ(0U, root->child_count());
  }
}

// Simulates the crash in crbug.com/1317642 by disabling BFCache and going back
// to a page with a fenced frame navigation. This is a regression test
// originally for Shadow DOM fenced frames, which no longer exist, but we still
// explicitly test this scenario.

// 1. creates a default mode fenced frame.
// 2. creates an opaque mode urn iframe nested in the fenced frame.
// 3. do an `_unfencedTop` navigation from the urn iframe.
//
// The `_unfencedTop` navigation should succeed. Note: previously this test
// existed to confirm that the URN iframe's fenced frame properties were used in
// the `_unfencedTop` navigation. However, now that we've allowed default mode
// fenced frames to use the `_unfencedTop` navigation target, this test doesn't
// have the same failure mode as before. However, it's still useful to verify
// that the `_unfencedTop` navigation works in a nested URN iframe.

// TODO(crbug.com/40060657): Once navigation support for urn::uuid in iframes is
// deprecated, this test should be removed.

// Parameterized on whether the feature is enabled or not.
class UUIDFrameTreeBrowserTest
    : public FencedFrameBrowserTestBase,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  UUIDFrameTreeBrowserTest() {
    scoped_feature_list_.InitWithFeatureStates(
        {{blink::features::kAllowURNsInIframes, IsAllowURNsInIframesEnabled()},
         {blink::features::kDisplayWarningDeprecateURNIframesUseFencedFrames,
          DisplayWarningDeprecateURNIframesUseFencedFrames()}});
  }

  bool NavigateIframeAndCheckURL(WebContents* web_contents,
                                 const std::string& html_id,
                                 const GURL& url,
                                 const GURL& expected_commit_url) {
    TestNavigationObserver nav_observer(web_contents);
    if (!BeginNavigateIframeToURL(web_contents, html_id, url))
      return false;
    nav_observer.Wait();
    EXPECT_EQ(expected_commit_url, nav_observer.last_navigation_url());
    return nav_observer.last_navigation_succeeded();
  }

  static std::string DescribeParams(
      const ::testing::TestParamInfo<ParamType>& info) {
    return base::StringPrintf(
        "%s_%s",
        std::get<0>(info.param) ? "AllowURNsInIframes"
                                : "DoNotAllowURNsInIframes",
        std::get<1>(info.param)
            ? "DisplayWarningDeprecateURNIframesUseFencedFrames"
            : "DoNotDisplayWarningDeprecateURNIframesUseFencedFrames");
  }

  bool IsAllowURNsInIframesEnabled() { return std::get<0>(GetParam()); }

  bool DisplayWarningDeprecateURNIframesUseFencedFrames() {
    return std::get<1>(GetParam());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(UUIDFrameTreeBrowserTest,
                       CheckIframeNavigationWithUUID) {
  base::HistogramTester histogram_tester;
  GURL main_url = https_server()->GetURL("b.test", "/hello.html");
  GURL initial_frame_url = https_server()->GetURL("a.test", "/hello.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  histogram_tester.ExpectTotalCount(
      "Navigation.BrowserMappedUrnUuidInIframeOrFencedFrame", 0);

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  {
    EXPECT_TRUE(ExecJs(root,
                       "var f = document.createElement('iframe');"
                       "f.id = \"test_iframe\";"
                       "document.body.appendChild(f);"));
  }
  EXPECT_EQ(1U, root->child_count());

  // Initially navigate the iframe to somewhere specific.
  EXPECT_TRUE(NavigateIframeAndCheckURL(web_contents(), "test_iframe",
                                        initial_frame_url, initial_frame_url));
  histogram_tester.ExpectTotalCount(
      "Navigation.BrowserMappedUrnUuidInIframeOrFencedFrame", 0);

  GURL frame_url(
      https_server()->GetURL("a.test", "/fenced_frames/title1.html"));
  FencedFrameURLMapping& url_mapping =
      root->current_frame_host()->GetPage().fenced_frame_urls_map();
  auto urn_uuid = test::AddAndVerifyFencedFrameURL(&url_mapping, frame_url);

  WebContentsConsoleObserver console_observer(web_contents());
  auto filter =
      [](const content::WebContentsConsoleObserver::Message& message) {
        return message.log_level == blink::mojom::ConsoleMessageLevel::kWarning;
      };
  console_observer.SetFilter(base::BindRepeating(filter));
  console_observer.SetPattern(
      "Protected Audience/selectURL will deprecate supporting iframes to "
      "render the winning ad*");

  if (IsAllowURNsInIframesEnabled()) {
    // If the feature is enabled, we should navigate to the mapped page.
    EXPECT_TRUE(NavigateIframeAndCheckURL(web_contents(), "test_iframe",
                                          urn_uuid, frame_url));
    histogram_tester.ExpectBucketCount(
        "Navigation.BrowserMappedUrnUuidInIframeOrFencedFrame", 1, 1);
    // A console warning is emitted during navigation if feature
    // `kDisplayWarningDeprecateURNIframesUseFencedFrames` is enabled. This will
    // be removed once navigation support for urn::uuid in iframes is
    // deprecated.
    // TODO(crbug.com/40060657)

    if (DisplayWarningDeprecateURNIframesUseFencedFrames()) {
      ASSERT_TRUE(console_observer.Wait());
      ASSERT_FALSE(console_observer.messages().empty());
      EXPECT_EQ(
          console_observer.GetMessageAt(0),
          "Protected Audience/selectURL will deprecate supporting iframes to "
          "render the winning ad/selected URL. "
          "Please use fenced frames instead. See "
          "https://developer.chrome.com/en/docs/privacy-sandbox/fenced-frame/"
          "#examples");
    }
  } else {
    // If the feature is disabled, navigation should fail.
    EXPECT_FALSE(NavigateIframeAndCheckURL(web_contents(), "test_iframe",
                                           urn_uuid, GURL()));
    histogram_tester.ExpectBucketCount(
        "Navigation.BrowserMappedUrnUuidInIframeOrFencedFrame", 1, 0);
    // No console warning is emitted if the feature is disabled.
    EXPECT_TRUE(console_observer.messages().empty());
  }

  // The parent will be able to access window.frames[0] as iframes are
  // visible via frames[].
  EXPECT_EQ(1, EvalJs(root, "window.frames.length"));
}

IN_PROC_BROWSER_TEST_P(UUIDFrameTreeBrowserTest,
                       CheckIframeNavigationWithInvalidUUID) {
  GURL main_url = https_server()->GetURL("b.test", "/hello.html");
  GURL initial_frame_url = https_server()->GetURL("a.test", "/hello.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  {
    EXPECT_TRUE(ExecJs(root,
                       "var f = document.createElement('iframe');"
                       "f.id = \"test_iframe\";"
                       "document.body.appendChild(f);"));
  }
  EXPECT_EQ(1U, root->child_count());

  // Initially navigate the iframe to somewhere specific.
  EXPECT_TRUE(NavigateIframeAndCheckURL(web_contents(), "test_iframe",
                                        initial_frame_url, initial_frame_url));

  GURL urn_uuid("urn:uuid:c36973b5-e5d9-de59-e4c4-364f137b3c7a");

  // We expect iframe navigations to invalid URNs to fail, regardless of if the
  // feature is enabled.
  EXPECT_FALSE(NavigateIframeAndCheckURL(web_contents(), "test_iframe",
                                         urn_uuid, GURL()));

  // The parent will be able to access window.frames[0] as iframes are
  // visible via frames[].
  EXPECT_EQ(1, EvalJs(root, "window.frames.length"));
}

IN_PROC_BROWSER_TEST_P(UUIDFrameTreeBrowserTest,
                       CheckMainFrameNavigationWithUUIDFails) {
  GURL main_url = https_server()->GetURL("b.test", "/hello.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  GURL frame_url(
      https_server()->GetURL("a.test", "/fenced_frames/title1.html"));
  FencedFrameURLMapping& url_mapping =
      root->current_frame_host()->GetPage().fenced_frame_urls_map();
  auto urn_uuid = test::AddAndVerifyFencedFrameURL(&url_mapping, frame_url);

  // Top page navigation to a URN should fail regardless of if the feature is
  // enabled.
  EXPECT_FALSE(NavigateToURL(shell(), urn_uuid));
}

INSTANTIATE_TEST_SUITE_P(All,
                         UUIDFrameTreeBrowserTest,
                         ::testing::Combine(testing::Bool(), testing::Bool()),
                         &UUIDFrameTreeBrowserTest::DescribeParams);

INSTANTIATE_TEST_SUITE_P(All,
                         FencedFrameIsolatedSandboxedIframesBrowserTest,
                         ::testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "kIsolateFencedFramesEnabled"
                                             : "kIsolateFencedFramesDisabled";
                         });


}  // namespace content
