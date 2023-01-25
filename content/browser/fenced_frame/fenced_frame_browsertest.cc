// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/back_forward_cache_browsertest.h"
#include "content/browser/fenced_frame/fenced_frame.h"
#include "content/browser/fenced_frame/fenced_frame_reporter.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_entry_restore_context_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/frame_type.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/mock_web_contents_observer.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/fenced_frame_test_utils.h"
#include "content/test/resource_load_observer.h"
#include "content/test/test_content_browser_client.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/cors/cors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/fenced_frame/fenced_frame_utils.h"
#include "third_party/blink/public/common/frame/fenced_frame_sandbox_flags.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame.mojom.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-test-utils.h"
#include "url/gurl.h"

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

constexpr char kReportingURL[] = "/_report_event_server.html";

GURL AddAndVerifyFencedFrameURL(
    FencedFrameURLMapping* fenced_frame_url_mapping,
    const GURL& https_url,
    scoped_refptr<FencedFrameReporter> fenced_frame_reporter = nullptr) {
  absl::optional<GURL> urn_uuid =
      fenced_frame_url_mapping->AddFencedFrameURLForTesting(
          https_url, std::move(fenced_frame_reporter));
  EXPECT_TRUE(urn_uuid.has_value());
  EXPECT_TRUE(urn_uuid->is_valid());
  return urn_uuid.value();
}

GURL GenerateAndVerifyPendingMappedURN(
    FencedFrameURLMapping* fenced_frame_url_mapping) {
  absl::optional<GURL> pending_urn =
      fenced_frame_url_mapping->GeneratePendingMappedURN();
  EXPECT_TRUE(pending_urn.has_value());
  EXPECT_TRUE(pending_urn->is_valid());

  return pending_urn.value();
}

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

  base::HistogramTester histogram_tester_;

 private:
  // Server must start after ControllableHttpResponse object being constructed.
  void AssertServerStart() override {}
};

// Tests that the renderer can create a <fencedframe> that results in a
// browser-side content::FencedFrame also being created.
IN_PROC_BROWSER_TEST_F(FencedFrameMPArchBrowserTest,
                       CreateFromScriptAndDestroy) {
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
}

IN_PROC_BROWSER_TEST_F(FencedFrameMPArchBrowserTest, CreateFromParser) {
  ASSERT_TRUE(https_server()->Start());
  const GURL top_level_url =
      https_server()->GetURL("c.test", "/fenced_frames/basic.html");
  EXPECT_TRUE(NavigateToURL(shell(), top_level_url));

  // The fenced frame is set-up synchronously, so it should exist immediately.
  RenderFrameHostImplWrapper dummy_child_frame(
      primary_main_frame_host()->child_at(0)->current_frame_host());
  EXPECT_NE(dummy_child_frame->inner_tree_main_frame_tree_node_id(),
            FrameTreeNode::kFrameTreeNodeInvalidId);
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

IN_PROC_BROWSER_TEST_F(FencedFrameMPArchBrowserTest, AboutBlankNavigation) {
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

  // Exepct the origin is correct.
  EXPECT_EQ(url::Origin::Create(fenced_frame_url),
            EvalJs(fenced_frame->GetInnerRoot(), "self.origin;"));

  // Assigning the location from the parent cause the SiteInstance
  // to be calculated incorrectly and crash. see https://crbug.com/1268238.
  // We can't use `NavigateFrameInFencedFrameTree` because that navigates
  // from the inner frame tree and we want the navigation to occur from
  // the outer frame tree.
  TestFrameNavigationObserver observer(fenced_frame->GetInnerRoot());
  EXPECT_TRUE(
      ExecJs(primary_rfh,
             "document.querySelector('fencedframe').src = 'about:blank';"));
  observer.Wait();

  EXPECT_TRUE(!fenced_frame->GetInnerRoot()->IsErrorDocument());
  EXPECT_EQ("null", EvalJs(fenced_frame->GetInnerRoot(), "self.origin;"));
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
  web_contents()->ForEachFrameTree(
      base::BindLambdaForTesting([&](FrameTree& frame_tree) {
        if (&frame_tree == fenced_frame_rfh->frame_tree()) {
          visited_fenced_frame_frame_tree = true;
        }
      }));
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
      blink::mojom::FencedFrameMode mode,
      blink::mojom::RemoteFrameInterfacesFromRendererPtr
          remote_frame_interfaces,
      const blink::RemoteFrameToken& frame_token,
      const base::UnguessableToken& devtools_frame_token) override {
    mojo::PendingAssociatedRemote<blink::mojom::FencedFrameOwnerHost>
        original_remote;

    GetForwardingInterface()->CreateFencedFrame(
        original_remote.InitWithNewEndpointAndPassReceiver(), mode,
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

IN_PROC_BROWSER_TEST_F(FencedFrameMPArchBrowserTest, NavigationStartTime) {
  ASSERT_TRUE(https_server()->Start());
  const GURL main_url = https_server()->GetURL("a.test", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  RenderFrameHostImpl* primary_rfh = primary_main_frame_host();

  const GURL fenced_frame_url =
      https_server()->GetURL("a.test", "/fenced_frames/title1.html");
  constexpr char kAddFencedFrameScript[] = R"({
    const fenced_frame = document.createElement('fencedframe');
    fenced_frame.src = $1;
    document.body.appendChild(fenced_frame);
  })";

  const int delay_in_milliseconds = 1000;

  // The UI thread of the browser process will sleep |delay_in_milliseconds|
  // just before handing FencedFrameOwnerHost's Navigate mojo method.
  NavigationDelayerInterceptor interceptor(
      primary_rfh, base::Milliseconds(delay_in_milliseconds));

  EXPECT_TRUE(
      ExecJs(primary_rfh, JsReplace(kAddFencedFrameScript, fenced_frame_url)));
  std::vector<FencedFrame*> fenced_frames = primary_rfh->GetFencedFrames();
  ASSERT_EQ(1U, fenced_frames.size());
  FencedFrame* fenced_frame = fenced_frames[0];
  WaitForLoadStop(web_contents());

  // The duration between navigationStart (measured in the renderer process) and
  // requestStart (measured in the browser process due to PlzNavigate) must be
  // greater than or equal to |delay_in_milliseconds|.
  EXPECT_GE(EvalJs(fenced_frame->GetInnerRoot(),
                   "performance.timing.requestStart - "
                   "performance.timing.navigationStart")
                .ExtractInt(),
            delay_in_milliseconds);
}

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

// Test that a fenced-frame does not perform any of the Android main-frame
// viewport behaviors like zoom-out-to-fit-content or parsing the viewport
// <meta>.
// Flaky on Mac https://crbug.com/1349900
#if BUILDFLAG(IS_MAC)
#define MAYBE_ViewportSettings DISABLED_ViewportSettings
#else
#define MAYBE_ViewportSettings ViewportSettings
#endif
IN_PROC_BROWSER_TEST_F(FencedFrameMPArchBrowserTest, MAYBE_ViewportSettings) {
  ASSERT_TRUE(https_server()->Start());
  const GURL top_level_url =
      https_server()->GetURL("c.test", "/fenced_frames/viewport.html");
  EXPECT_TRUE(NavigateToURL(shell(), top_level_url));

  RenderFrameHostImplWrapper primary_rfh(primary_main_frame_host());
  std::vector<FencedFrame*> fenced_frames = primary_rfh->GetFencedFrames();
  ASSERT_EQ(1ul, fenced_frames.size());
  FencedFrame* fenced_frame = fenced_frames.back();

  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  // Ensure various dimensions and properties in the fenced frame
  // match the dimensions of the <fencedframe> in the parent and do
  // not take into account the <meta name="viewport"> in the
  // fenced-frame page.
  EXPECT_EQ(
      EvalJs(fenced_frame->GetInnerRoot(), "window.innerWidth").ExtractInt(),
      314);
  EXPECT_EQ(
      EvalJs(fenced_frame->GetInnerRoot(), "window.innerHeight").ExtractInt(),
      271);
  EXPECT_EQ(EvalJs(fenced_frame->GetInnerRoot(),
                   "document.documentElement.clientWidth")
                .ExtractInt(),
            314);
  EXPECT_EQ(EvalJs(fenced_frame->GetInnerRoot(), "window.visualViewport.scale")
                .ExtractDouble(),
            1.0);
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
  fenced_frame_test_helper().CreateFencedFrameAsync(fenced_frame_parent_rfh,
                                                    fenced_frame_url);

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
  fenced_frame_test_helper().CreateFencedFrameAsync(fenced_frame_parent_rfh,
                                                    fenced_frame_url);
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

IN_PROC_BROWSER_TEST_F(FencedFrameMPArchBrowserTest,
                       ProcessAllocationWithFullSiteIsolation) {
  ASSERT_TRUE(https_server()->Start());
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
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

IN_PROC_BROWSER_TEST_F(FencedFrameMPArchBrowserTest,
                       CrossSiteFencedFramesShareProcess) {
  ASSERT_TRUE(https_server()->Start());
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
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

  TestFrameNavigationObserver observer(ff_rfh.get());
  GURL new_fenced_frame_url =
      https_server()->GetURL("a.test", "/fenced_frames/sandbox_flags.html");
  EXPECT_TRUE(
      ExecJs(primary_rfh.get(),
             JsReplace("document.querySelector('fencedframe').src = $1;",
                       new_fenced_frame_url)));
  observer.Wait();

  EXPECT_TRUE(!ff_rfh->IsErrorDocument());
  EXPECT_TRUE(ff_rfh->IsSandboxed(blink::kFencedFrameForcedSandboxFlags));
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
          std::move(receiver), blink::mojom::FencedFrameMode::kDefault,
          std::move(remote_frame_interfaces), blink::RemoteFrameToken(),
          base::UnguessableToken::Create());
  EXPECT_TRUE(primary_rfh.WaitUntilRenderFrameDeleted());
  EXPECT_TRUE(iframe.IsRenderFrameDeleted());
}

class FencedFrameWithSiteIsolationDisabledBrowserTest
    : public FencedFrameMPArchBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  FencedFrameWithSiteIsolationDisabledBrowserTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    if (std::get<0>(GetParam())) {
      enabled_features.push_back(
          features::kProcessSharingWithDefaultSiteInstances);
      disabled_features.push_back(
          features::kProcessSharingWithStrictSiteInstances);
    } else {
      enabled_features.push_back(
          features::kProcessSharingWithStrictSiteInstances);
      disabled_features.push_back(
          features::kProcessSharingWithDefaultSiteInstances);
    }

    if (std::get<1>(GetParam())) {
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

INSTANTIATE_TEST_SUITE_P(
    All,
    FencedFrameWithSiteIsolationDisabledBrowserTest,
    testing::Combine(testing::Bool(), testing::Bool()),
    [](const testing::TestParamInfo<std::tuple<bool, bool>>& info) {
      return base::StringPrintf("%s_%s",
                                std::get<0>(info.param) ? "DefaultSiteInstances"
                                                        : "StrictSiteInstances",
                                std::get<1>(info.param)
                                    ? "IsolatedFencedFrames"
                                    : "UnisolatedFencedFrames");
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
  const GURL subframe_url = https_server()->GetURL("a.test", "/title2.html");
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

namespace {

static std::string ModeTestParamToString(
    ::testing::TestParamInfo<std::tuple<blink::mojom::FencedFrameMode,
                                        blink::mojom::FencedFrameMode>>
        param_info) {
  std::string out = "ParentMode";
  switch (std::get<0>(param_info.param)) {
    case blink::mojom::FencedFrameMode::kDefault:
      out += "Default";
      break;
    case blink::mojom::FencedFrameMode::kOpaqueAds:
      out += "OpaqueAds";
      break;
  }

  out += "_ChildMode";

  switch (std::get<1>(param_info.param)) {
    case blink::mojom::FencedFrameMode::kDefault:
      out += "Default";
      break;
    case blink::mojom::FencedFrameMode::kOpaqueAds:
      out += "OpaqueAds";
      break;
  }

  return out;
}

}  // namespace

class FencedFrameNestedModesTest
    : public FencedFrameBrowserTestBase,
      public testing::WithParamInterface<
          std::tuple<blink::mojom::FencedFrameMode,
                     blink::mojom::FencedFrameMode>> {
 protected:
  FencedFrameNestedModesTest() {
    // TODO(domfarolino): Maybe we don't need this?
    feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kFencedFrames, {}},
         {features::kPrivacySandboxAdsAPIsOverride, {}}},
        {/* disabled_features */});
  }

  std::string GetParentMode() { return ModeToString(std::get<0>(GetParam())); }
  std::string GetChildMode() { return ModeToString(std::get<1>(GetParam())); }

  base::HistogramTester histogram_tester_;

 private:
  std::string ModeToString(blink::mojom::FencedFrameMode mode) {
    switch (mode) {
      case blink::mojom::FencedFrameMode::kDefault:
        return "default";
      case blink::mojom::FencedFrameMode::kOpaqueAds:
        return "opaque-ads";
    }

    NOTREACHED();
    return "";
  }

  base::test::ScopedFeatureList feature_list_;
};

// This test runs the following steps:
//   1.) Creates a "parent" fenced frame with a particular mode
//   2.) Creates a nested/child fenced frame with a particular mode
//   3.) Asserts that creation of the child fenced frame either failed or
//       succeeded depending on its mode.
IN_PROC_BROWSER_TEST_P(FencedFrameNestedModesTest, NestedModes) {
  const GURL main_url = https_server()->GetURL("a.test", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // 1.) Create the parent fenced frame.
  const GURL fenced_frame_url =
      https_server()->GetURL("c.test", "/fenced_frames/title1.html");
  constexpr char kAddFencedFrameScript[] = R"({
      const fenced_frame = document.createElement('fencedframe');
      fenced_frame.src = $2;
      fenced_frame.mode = $1;
      document.body.appendChild(fenced_frame);
    })";
  EXPECT_TRUE(ExecJs(
      primary_main_frame_host(),
      JsReplace(kAddFencedFrameScript, GetParentMode(), fenced_frame_url)));

  // Wait for page to load in order to have it know it's in a secure context.
  WaitForLoadStop(web_contents());

  // Get the fenced frame's RFH.
  ASSERT_EQ(1u, primary_main_frame_host()->child_count());
  RenderFrameHostImpl* parent_fenced_frame_rfh =
      primary_main_frame_host()->child_at(0)->current_frame_host();
  int inner_node_id =
      parent_fenced_frame_rfh->inner_tree_main_frame_tree_node_id();
  parent_fenced_frame_rfh =
      FrameTreeNode::GloballyFindByID(inner_node_id)->current_frame_host();
  ASSERT_TRUE(parent_fenced_frame_rfh);

  // 2.) Attempt to create the child fenced frame.
  EXPECT_TRUE(ExecJs(
      parent_fenced_frame_rfh,
      JsReplace(kAddFencedFrameScript, GetChildMode(), fenced_frame_url)));

  // 3.) Assert that the child fenced frame was created or not created depending
  //     on the test parameters.
  content::FetchHistogramsFromChildProcesses();
  if (GetParentMode() != GetChildMode()) {
    // Child fenced frame creation should have failed based on its mode.
    EXPECT_EQ(0u, parent_fenced_frame_rfh->child_count());
    histogram_tester_.ExpectTotalCount(
        blink::kFencedFrameCreationOrNavigationOutcomeHistogram, 2);
    histogram_tester_.ExpectBucketCount(
        blink::kFencedFrameCreationOrNavigationOutcomeHistogram,
        blink::FencedFrameCreationOutcome::kIncompatibleMode, 1);
  } else {
    // Child fenced frame creation should have succeeded because its mode is the
    // same as its parent.
    EXPECT_EQ(1u, parent_fenced_frame_rfh->child_count());
    histogram_tester_.ExpectTotalCount(
        blink::kFencedFrameCreationOrNavigationOutcomeHistogram, 2);
    if (GetChildMode() == "default") {
      histogram_tester_.ExpectBucketCount(
          blink::kFencedFrameCreationOrNavigationOutcomeHistogram,
          blink::FencedFrameCreationOutcome::kSuccessDefault, 2);
    } else {
      histogram_tester_.ExpectBucketCount(
          blink::kFencedFrameCreationOrNavigationOutcomeHistogram,
          blink::FencedFrameCreationOutcome::kSuccessOpaque, 2);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    FencedFrameNestedModesTest,
    FencedFrameNestedModesTest,
    testing::Combine(
        /*parent mode=*/testing::Values(
            blink::mojom::FencedFrameMode::kDefault,
            blink::mojom::FencedFrameMode::kOpaqueAds),
        /*child mode=*/
        testing::Values(blink::mojom::FencedFrameMode::kDefault,
                        blink::mojom::FencedFrameMode::kOpaqueAds)),
    ModeTestParamToString);

// TODO(domfarolino): Rename this.
class FencedFrameParameterizedBrowserTest : public FencedFrameBrowserTestBase {
 public:
  FencedFrameParameterizedBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kFencedFrames, {}},
         {net::features::kThirdPartyStoragePartitioning, {}},
         {net::features::kNoncedPartitionedCookies, {}},
         {features::kPrivacySandboxAdsAPIsOverride, {}}},
        {/* disabled_features */});
  }

  // This is needed because `TestFrameNavigationObserver` doesn't work properly
  // from within the context of a fenced frame's FrameTree. See the comments
  // below.
  void NavigateFrameInsideFencedFrameTreeAndWaitForFinishedLoad(
      const ToRenderFrameHost& adapter,
      GURL url,
      const std::string& navigate_script,
      net::Error expected_net_error_code = net::OK) {
    RenderFrameHostImpl* rfh =
        static_cast<RenderFrameHostImpl*>(adapter.render_frame_host());
    EXPECT_TRUE(rfh->IsNestedWithinFencedFrame());
    RenderFrameHostImpl* target_rfh = rfh->GetParentOrOuterDocument();
    ExecuteNavigationOrHistoryScriptInFencedFrameTree(
        target_rfh, rfh, navigate_script, expected_net_error_code);
  }

  void UpdateHistoryOrReloadFromFencedFrameTreeAndWaitForFinishedLoad(
      const ToRenderFrameHost& adapter,
      const std::string& history_script) {
    RenderFrameHostImpl* rfh =
        static_cast<RenderFrameHostImpl*>(adapter.render_frame_host());
    EXPECT_TRUE(rfh->IsNestedWithinFencedFrame());

    ExecuteNavigationOrHistoryScriptInFencedFrameTree(rfh, rfh, history_script);
  }

  void ExecuteNavigationOrHistoryScriptInFencedFrameTree(
      RenderFrameHostImpl* target_rfh,
      RenderFrameHostImpl* fenced_frame_rfh,
      const std::string& script,
      net::Error expected_net_error_code = net::OK) {
    TestFrameNavigationObserver observer(fenced_frame_rfh);
    EXPECT_TRUE(ExecJs(target_rfh, script));
    observer.Wait();
    EXPECT_EQ(observer.last_net_error_code(), expected_net_error_code);
  }

  FrameTreeNode* AddIframeInFencedFrame(FrameTreeNode* fenced_frame,
                                        unsigned int child_index) {
    EXPECT_TRUE(
        ExecJs(fenced_frame,
               "var iframe_within_ff = document.createElement('iframe');"
               "document.body.appendChild(iframe_within_ff);"));
    EXPECT_EQ(child_index + 1, fenced_frame->child_count());
    auto* iframe = fenced_frame->child_at(child_index);
    EXPECT_FALSE(iframe->IsFencedFrameRoot());
    EXPECT_TRUE(iframe->IsInFencedFrameTree());
    return iframe;
  }

  // Navigates the element created in AddIframeInFencedFrame.
  void NavigateIframeInFencedFrame(
      FrameTreeNode* iframe,
      const GURL& url,
      net::Error expected_net_error_code = net::OK) {
    EXPECT_FALSE(iframe->IsFencedFrameRoot());
    EXPECT_TRUE(iframe->IsInFencedFrameTree());

    // Navigate the iframe.
    std::string navigate_script =
        JsReplace("iframe_within_ff.src = $1;", url.spec());
    NavigateFrameInsideFencedFrameTreeAndWaitForFinishedLoad(
        iframe, url, navigate_script, expected_net_error_code);
  }

  FrameTreeNode* AddNestedFencedFrame(FrameTreeNode* fenced_frame,
                                      unsigned int child_index) {
    EXPECT_TRUE(ExecJs(
        fenced_frame,
        "var nested_fenced_frame = document.createElement('fencedframe');"
        "document.body.appendChild(nested_fenced_frame);"));
    EXPECT_EQ(child_index + 1, fenced_frame->child_count());
    auto* nested_fenced_frame =
        GetFencedFrameRootNode(fenced_frame->child_at(child_index));
    EXPECT_TRUE(nested_fenced_frame->IsFencedFrameRoot());
    EXPECT_TRUE(nested_fenced_frame->IsInFencedFrameTree());
    return nested_fenced_frame;
  }

  // Navigates the element created in AddNestedFencedFrame.
  void NavigateNestedFencedFrame(FrameTreeNode* nested_fenced_frame,
                                 const GURL& url) {
    EXPECT_TRUE(nested_fenced_frame->IsFencedFrameRoot());
    EXPECT_TRUE(nested_fenced_frame->IsInFencedFrameTree());
    std::string navigate_script =
        JsReplace("nested_fenced_frame.src = $1;", url.spec());
    NavigateFrameInsideFencedFrameTreeAndWaitForFinishedLoad(
        nested_fenced_frame, url, navigate_script);
  }

  // Invoked on "EmbeddedTestServer IO Thread".
  void ObserveRequestHeaders(const net::test_server::HttpRequest& request) {
    base::AutoLock auto_lock(requests_lock_);
    std::string val = request.headers.find("Cookie") != request.headers.end()
                          ? request.headers.at("Cookie").c_str()
                          : "";
    cookie_headers_map_.insert(std::make_pair(request.GetURL().path(), val));

    val = request.headers.find("Sec-Fetch-Dest") != request.headers.end()
              ? request.headers.at("Sec-Fetch-Dest").c_str()
              : "";
    sec_fetch_dest_headers_map_.insert(
        std::make_pair(request.GetURL().path(), val));
  }

  // Returns true if the cookie header was present in the last request received
  // by the server with the same `url.path()`. Also asserts that the cookie
  // header value matches that given in `expected_value`, if it exists. Also
  // clears the value that was just checked by the method invocation.
  bool CheckAndClearCookieHeader(
      const GURL& url,
      const std::string& expected_value = "",
      const base::Location& from_here = base::Location::Current()) {
    base::AutoLock auto_lock(requests_lock_);
    SCOPED_TRACE(from_here.ToString());
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    std::string file_name = url.path();
    CHECK(cookie_headers_map_.find(file_name) != cookie_headers_map_.end());
    std::string header = cookie_headers_map_[file_name];
    EXPECT_EQ(expected_value, header);
    cookie_headers_map_.erase(file_name);
    return !header.empty();
  }

  bool CheckAndClearSecFetchDestHeader(
      const GURL& url,
      const std::string& expected_value = "",
      const base::Location& from_here = base::Location::Current()) {
    base::AutoLock auto_lock(requests_lock_);
    SCOPED_TRACE(from_here.ToString());
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    std::string file_name = url.path();
    CHECK(sec_fetch_dest_headers_map_.find(file_name) !=
          sec_fetch_dest_headers_map_.end());
    std::string header = sec_fetch_dest_headers_map_[file_name];
    EXPECT_EQ(expected_value, header);
    sec_fetch_dest_headers_map_.erase(file_name);
    return !header.empty();
  }

  ~FencedFrameParameterizedBrowserTest() override {
    // Shutdown the server explicitly so that there is no race with the
    // destruction of cookie_headers_map_ and invocation of RequestMonitor.
    if (https_server()->Started()) {
      EXPECT_TRUE(https_server()->ShutdownAndWaitUntilComplete());
    }
  }

  // Checking the count of NavigationEntries for fenced frame.
  void CheckNavigationEntryCount(FrameTreeNode* root,
                                 FrameTreeNode* fenced_frame,
                                 int shadowdom_cnt,
                                 int mparch_cnt) const {
    // Wait for web content to stop loading.
    EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
    // MPArch fenced frame has its own NavigationController so checking
    // `fenced_frame->navigator().controller()`.
    EXPECT_EQ(mparch_cnt,
              fenced_frame->navigator().controller().GetEntryCount());
  }

 private:
  void AdditionalSetup() override {
    https_server()->RegisterRequestMonitor(base::BindRepeating(
        &FencedFrameParameterizedBrowserTest::ObserveRequestHeaders,
        base::Unretained(this)));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::Lock requests_lock_;
  std::map<std::string, std::string> cookie_headers_map_
      GUARDED_BY(requests_lock_);
  std::map<std::string, std::string> sec_fetch_dest_headers_map_
      GUARDED_BY(requests_lock_);
};

// Tests that the fenced frame gets navigated to an actual url given a urn:uuid.
IN_PROC_BROWSER_TEST_F(FencedFrameParameterizedBrowserTest,
                       CheckFencedFrameNavigationWithUUID) {
  base::HistogramTester histogram_tester;
  GURL main_url = https_server()->GetURL("b.test", "/hello.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  histogram_tester.ExpectTotalCount(
      "Navigation.BrowserMappedUrnUuidInIframeOrFencedFrame", 0);

  {
    EXPECT_TRUE(ExecJs(root,
                       "var f = document.createElement('fencedframe');"
                       "f.mode = 'opaque-ads';"
                       "document.body.appendChild(f);"));
  }
  EXPECT_EQ(1U, root->child_count());
  FrameTreeNode* fenced_frame_root_node =
      GetFencedFrameRootNode(root->child_at(0));

  EXPECT_TRUE(fenced_frame_root_node->IsFencedFrameRoot());
  EXPECT_TRUE(fenced_frame_root_node->IsInFencedFrameTree());

  GURL https_url(
      https_server()->GetURL("a.test", "/fenced_frames/title1.html"));
  FencedFrameURLMapping& url_mapping =
      root->current_frame_host()->GetPage().fenced_frame_urls_map();
  auto urn_uuid = AddAndVerifyFencedFrameURL(&url_mapping, https_url);

  std::string navigate_urn_script = JsReplace("f.src = $1;", urn_uuid);

  WebContentsConsoleObserver console_error_observer(shell()->web_contents());
  auto error_filter =
      [](const content::WebContentsConsoleObserver::Message& message) {
        return message.log_level == blink::mojom::ConsoleMessageLevel::kError;
      };
  console_error_observer.SetFilter(base::BindRepeating(error_filter));
  console_error_observer.SetPattern("Supports-Loading-Mode*");

  {
    TestFrameNavigationObserver navigation_observer(fenced_frame_root_node);
    WebContentsConsoleObserver console_observer(web_contents());
    auto filter =
        [](const content::WebContentsConsoleObserver::Message& message) {
          return message.log_level ==
                 blink::mojom::ConsoleMessageLevel::kWarning;
        };
    console_observer.SetFilter(base::BindRepeating(filter));
    console_observer.SetPattern(
        "FLEDGE will deprecate supporting iframes to render the winning ad*");
    EXPECT_EQ(urn_uuid.spec(), EvalJs(root, navigate_urn_script));
    navigation_observer.WaitForCommit();
    // No console warning is emitted for urn::uuid navigation in fenced frames.
    EXPECT_TRUE(console_observer.messages().empty());
  }

  content::FetchHistogramsFromChildProcesses();
  histogram_tester.ExpectTotalCount(
      blink::kFencedFrameCreationOrNavigationOutcomeHistogram, 1);
  // Fenced frame creation succeeded (opaque ads mode)
  histogram_tester.ExpectBucketCount(
      blink::kFencedFrameCreationOrNavigationOutcomeHistogram,
      blink::FencedFrameCreationOutcome::kSuccessOpaque, 1);
  // Fenced frame navigation succeeded, no response header opted-in error
  histogram_tester.ExpectBucketCount(
      blink::kFencedFrameCreationOrNavigationOutcomeHistogram,
      blink::FencedFrameCreationOutcome::kResponseHeaderNotOptIn, 0);
  histogram_tester.ExpectBucketCount(
      "Navigation.BrowserMappedUrnUuidInIframeOrFencedFrame", 0, 1);
  EXPECT_EQ(
      https_url,
      fenced_frame_root_node->current_frame_host()->GetLastCommittedURL());
  EXPECT_EQ(
      url::Origin::Create(https_url),
      fenced_frame_root_node->current_frame_host()->GetLastCommittedOrigin());
  // Fenced frame navigation with opt-in 'Supports-Loading-Mode: fenced-frame'
  // should not emit console errors.
  EXPECT_TRUE(console_error_observer.messages().empty());

  // Parent will still see the src as the urn_uuid and not the mapped url.
  EXPECT_EQ(urn_uuid.spec(), EvalJs(root, "f.src"));

  // The parent will not be able to access window.frames[0] as fenced frames are
  // not visible via frames[].
  EXPECT_FALSE(ExecJs(root, "window.frames[0].location"));
  EXPECT_EQ(0, EvalJs(root, "window.frames.length"));
}

IN_PROC_BROWSER_TEST_F(FencedFrameParameterizedBrowserTest,
                       SharedStorageMetadataInNestedFencedFrame) {
  GURL main_url = https_server()->GetURL("a.test", "/hello.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  FencedFrameURLMapping& url_mapping1 =
      root->current_frame_host()->GetPage().fenced_frame_urls_map();
  auto urn_uuid1 = GenerateAndVerifyPendingMappedURN(&url_mapping1);
  const GURL mapped_url1 =
      https_server()->GetURL("b.test", "/fenced_frames/title1.html");
  SimulateSharedStorageURNMappingComplete(
      url_mapping1, urn_uuid1, mapped_url1,
      /*shared_storage_origin=*/url::Origin::Create(GURL("https://foo.com")),
      /*budget_to_charge=*/2.0);

  EXPECT_TRUE(ExecJs(root,
                     "var f1 = document.createElement('fencedframe');"
                     "f1.mode = 'opaque-ads';"
                     "document.body.appendChild(f1);"));

  EXPECT_EQ(1U, root->child_count());
  FrameTreeNode* fenced_frame_root_node1 =
      GetFencedFrameRootNode(root->child_at(0));

  TestFrameNavigationObserver observer1(
      fenced_frame_root_node1->current_frame_host());
  std::string navigate_urn_script1 = JsReplace("f1.src = $1;", urn_uuid1);
  EXPECT_EQ(urn_uuid1.spec(), EvalJs(root, navigate_urn_script1));
  observer1.Wait();

  FencedFrameURLMapping& url_mapping2 =
      fenced_frame_root_node1->current_frame_host()
          ->GetPage()
          .fenced_frame_urls_map();
  auto urn_uuid2 = GenerateAndVerifyPendingMappedURN(&url_mapping2);
  const GURL mapped_url2 =
      https_server()->GetURL("c.test", "/fenced_frames/title1.html");
  SimulateSharedStorageURNMappingComplete(
      url_mapping2, urn_uuid2, mapped_url2,
      /*shared_storage_origin=*/url::Origin::Create(GURL("https://bar.com")),
      /*budget_to_charge=*/3.0);

  EXPECT_TRUE(ExecJs(fenced_frame_root_node1,
                     "var f2 = document.createElement('fencedframe');"
                     "f2.mode = 'opaque-ads';"
                     "document.body.appendChild(f2);"));

  EXPECT_EQ(1U, fenced_frame_root_node1->child_count());
  FrameTreeNode* fenced_frame_root_node2 =
      GetFencedFrameRootNode(fenced_frame_root_node1->child_at(0));

  TestFrameNavigationObserver observer2(
      fenced_frame_root_node2->current_frame_host());
  std::string navigate_urn_script2 = JsReplace("f2.src = $1;", urn_uuid2);
  EXPECT_EQ(urn_uuid2.spec(),
            EvalJs(fenced_frame_root_node1, navigate_urn_script2));
  observer2.Wait();

  auto metadata = fenced_frame_root_node2->FindSharedStorageBudgetMetadata();

  EXPECT_EQ(metadata.size(), 2u);

  EXPECT_EQ(metadata[0]->origin, url::Origin::Create(GURL("https://bar.com")));
  EXPECT_DOUBLE_EQ(metadata[0]->budget_to_charge, 3.0);

  EXPECT_EQ(metadata[1]->origin, url::Origin::Create(GURL("https://foo.com")));
  EXPECT_DOUBLE_EQ(metadata[1]->budget_to_charge, 2.0);
}

IN_PROC_BROWSER_TEST_F(
    FencedFrameParameterizedBrowserTest,
    TwoFencedFrameNavigationToSameSharedStorageOriginatedUUID_SameMetadata) {
  GURL main_url = https_server()->GetURL("b.test", "/hello.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  {
    EXPECT_TRUE(ExecJs(root,
                       "var f1 = document.createElement('fencedframe');"
                       "f1.mode = 'opaque-ads';"
                       "document.body.appendChild(f1);"));

    EXPECT_TRUE(ExecJs(root,
                       "var f2 = document.createElement('fencedframe');"
                       "f2.mode = 'opaque-ads';"
                       "document.body.appendChild(f2);"));
  }

  EXPECT_EQ(2U, root->child_count());
  FrameTreeNode* fenced_frame_root_node1 =
      GetFencedFrameRootNode(root->child_at(0));

  FrameTreeNode* fenced_frame_root_node2 =
      GetFencedFrameRootNode(root->child_at(1));

  FencedFrameURLMapping& url_mapping =
      root->current_frame_host()->GetPage().fenced_frame_urls_map();
  auto urn_uuid = GenerateAndVerifyPendingMappedURN(&url_mapping);
  const GURL mapped_url =
      https_server()->GetURL("a.test", "/fenced_frames/title1.html");
  SimulateSharedStorageURNMappingComplete(
      url_mapping, urn_uuid, mapped_url,
      /*shared_storage_origin=*/url::Origin::Create(GURL("https://bar.com")),
      /*budget_to_charge=*/2.0);

  {
    TestFrameNavigationObserver observer(
        fenced_frame_root_node1->current_frame_host());
    std::string navigate_urn_script = JsReplace("f1.src = $1;", urn_uuid);
    EXPECT_EQ(urn_uuid.spec(), EvalJs(root, navigate_urn_script));
    observer.Wait();
  }

  {
    TestFrameNavigationObserver observer(
        fenced_frame_root_node2->current_frame_host());
    std::string navigate_urn_script = JsReplace("f2.src = $1;", urn_uuid);
    EXPECT_EQ(urn_uuid.spec(), EvalJs(root, navigate_urn_script));
    observer.Wait();
  }

  EXPECT_EQ(fenced_frame_root_node1->FindSharedStorageBudgetMetadata().size(),
            1u);

  EXPECT_EQ(fenced_frame_root_node1->FindSharedStorageBudgetMetadata(),
            fenced_frame_root_node2->FindSharedStorageBudgetMetadata());
}

// Test the scenario where the FF navigation is deferred and then resumed, and
// the mapped url is a valid one. The navigation is expected to succeed.
IN_PROC_BROWSER_TEST_F(
    FencedFrameParameterizedBrowserTest,
    FencedFrameNavigationWithPendingMappedUUID_MappingSuccess_ValidURL) {
  GURL main_url = https_server()->GetURL("b.test", "/hello.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  {
    EXPECT_TRUE(ExecJs(root,
                       "var f = document.createElement('fencedframe');"
                       "f.mode = 'opaque-ads';"
                       "document.body.appendChild(f);"));
  }
  EXPECT_EQ(1U, root->child_count());
  FrameTreeNode* fenced_frame_root_node =
      GetFencedFrameRootNode(root->child_at(0));

  EXPECT_TRUE(fenced_frame_root_node->IsFencedFrameRoot());
  EXPECT_TRUE(fenced_frame_root_node->IsInFencedFrameTree());

  FencedFrameURLMapping& url_mapping =
      root->current_frame_host()->GetPage().fenced_frame_urls_map();
  FencedFrameURLMappingTestPeer url_mapping_test_peer(&url_mapping);

  auto urn_uuid = GenerateAndVerifyPendingMappedURN(&url_mapping);
  const GURL mapped_url =
      https_server()->GetURL("a.test", "/fenced_frames/title1.html");
  std::string navigate_urn_script = JsReplace("f.src = $1;", urn_uuid);

  TestFrameNavigationObserver observer(
      fenced_frame_root_node->current_frame_host());

  EXPECT_EQ(urn_uuid.spec(), EvalJs(root, navigate_urn_script));

  // After the previous EvalJs, the NavigationRequest should have been created,
  // but may not have begun. Wait for BeginNavigation() and expect it to be
  // deferred on fenced frame url mapping.
  NavigationRequest* request = fenced_frame_root_node->navigation_request();
  if (!request->is_deferred_on_fenced_frame_url_mapping_for_testing()) {
    base::RunLoop run_loop;
    request->set_begin_navigation_callback_for_testing(
        run_loop.QuitWhenIdleClosure());
    run_loop.Run();

    EXPECT_TRUE(request->is_deferred_on_fenced_frame_url_mapping_for_testing());
  }

  EXPECT_TRUE(url_mapping_test_peer.HasObserver(urn_uuid, request));

  auto budget_metadata =
      fenced_frame_root_node->FindSharedStorageBudgetMetadata();
  EXPECT_EQ(budget_metadata.size(), 0u);

  // Trigger the mapping to resume the deferred navigation.
  SimulateSharedStorageURNMappingComplete(
      url_mapping, urn_uuid, mapped_url,
      /*shared_storage_origin=*/url::Origin::Create(GURL("https://bar.com")),
      /*budget_to_charge=*/2.0);

  EXPECT_FALSE(url_mapping_test_peer.HasObserver(urn_uuid, request));

  observer.Wait();

  EXPECT_EQ(
      mapped_url,
      fenced_frame_root_node->current_frame_host()->GetLastCommittedURL());

  budget_metadata = fenced_frame_root_node->FindSharedStorageBudgetMetadata();
  EXPECT_EQ(budget_metadata.size(), 1u);
  EXPECT_EQ(budget_metadata[0]->origin,
            url::Origin::Create(GURL("https://bar.com")));
  EXPECT_DOUBLE_EQ(budget_metadata[0]->budget_to_charge, 2.0);
}

// Test the scenario where the FF navigation is deferred and then resumed, and
// the mapped url is invalid. The navigation is expected to fail.
IN_PROC_BROWSER_TEST_F(
    FencedFrameParameterizedBrowserTest,
    FencedFrameNavigationWithPendingMappedUUID_MappingSuccess_InvalidURL) {
  GURL main_url = https_server()->GetURL("b.test", "/hello.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  {
    EXPECT_TRUE(ExecJs(root,
                       "var f = document.createElement('fencedframe');"
                       "f.mode = 'opaque-ads';"
                       "document.body.appendChild(f);"));
  }
  EXPECT_EQ(1U, root->child_count());
  FrameTreeNode* fenced_frame_root_node =
      GetFencedFrameRootNode(root->child_at(0));

  EXPECT_TRUE(fenced_frame_root_node->IsFencedFrameRoot());
  EXPECT_TRUE(fenced_frame_root_node->IsInFencedFrameTree());

  FencedFrameURLMapping& url_mapping =
      root->current_frame_host()->GetPage().fenced_frame_urls_map();
  FencedFrameURLMappingTestPeer url_mapping_test_peer(&url_mapping);

  auto urn_uuid = GenerateAndVerifyPendingMappedURN(&url_mapping);
  const GURL mapped_url =
      https_server()->GetURL("a.test", "/fenced_frames/nonexistent-url.html");
  std::string navigate_urn_script = JsReplace("f.src = $1;", urn_uuid);

  TestFrameNavigationObserver observer(
      fenced_frame_root_node->current_frame_host());

  EXPECT_EQ(urn_uuid.spec(), EvalJs(root, navigate_urn_script));

  // After the previous EvalJs, the NavigationRequest should have been created,
  // but may not have begun. Wait for BeginNavigation() and expect it to be
  // deferred on fenced frame url mapping.
  NavigationRequest* request = fenced_frame_root_node->navigation_request();
  if (!request->is_deferred_on_fenced_frame_url_mapping_for_testing()) {
    base::RunLoop run_loop;
    request->set_begin_navigation_callback_for_testing(
        run_loop.QuitWhenIdleClosure());
    run_loop.Run();

    EXPECT_TRUE(request->is_deferred_on_fenced_frame_url_mapping_for_testing());
  }

  EXPECT_TRUE(url_mapping_test_peer.HasObserver(urn_uuid, request));

  // Trigger the mapping to resume the deferred navigation.
  SimulateSharedStorageURNMappingComplete(
      url_mapping, urn_uuid, mapped_url,
      /*shared_storage_origin=*/url::Origin::Create(GURL("https://bar.com")),
      /*budget_to_charge=*/2.0);

  EXPECT_FALSE(url_mapping_test_peer.HasObserver(urn_uuid, request));

  // In NavigationRequest::OnResponseStarted(), for fenced frame, it manually
  // fails the navigation with net::ERR_BLOCKED_BY_RESPONSE.
  observer.Wait();
  EXPECT_EQ(observer.last_net_error_code(), net::ERR_BLOCKED_BY_RESPONSE);

  // Despite the error, the budget metadata should be valid.
  auto metadata = fenced_frame_root_node->FindSharedStorageBudgetMetadata();
  EXPECT_EQ(metadata.size(), 1u);
}

IN_PROC_BROWSER_TEST_F(
    FencedFrameParameterizedBrowserTest,
    FencedFrameNavigationWithPendingMappedUUID_NavigationCanceledDuringDeferring) {
  GURL main_url = https_server()->GetURL("b.test", "/hello.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  {
    EXPECT_TRUE(ExecJs(root,
                       "var f = document.createElement('fencedframe');"
                       "f.mode = 'opaque-ads';"
                       "document.body.appendChild(f);"));
  }
  EXPECT_EQ(1U, root->child_count());
  FrameTreeNode* fenced_frame_root_node =
      GetFencedFrameRootNode(root->child_at(0));

  EXPECT_TRUE(fenced_frame_root_node->IsFencedFrameRoot());
  EXPECT_TRUE(fenced_frame_root_node->IsInFencedFrameTree());

  FencedFrameURLMapping& url_mapping =
      root->current_frame_host()->GetPage().fenced_frame_urls_map();
  FencedFrameURLMappingTestPeer url_mapping_test_peer(&url_mapping);

  auto urn_uuid = GenerateAndVerifyPendingMappedURN(&url_mapping);
  const GURL mapped_url =
      https_server()->GetURL("a.test", "/fenced_frames/title1.html");
  std::string navigate_urn_script = JsReplace("f.src = $1;", urn_uuid);

  TestFrameNavigationObserver observer(
      fenced_frame_root_node->current_frame_host());

  EXPECT_EQ(urn_uuid.spec(), EvalJs(root, navigate_urn_script));

  // After the previous EvalJs, the NavigationRequest should have been created,
  // but may not have begun. Wait for BeginNavigation() and expect it to be
  // deferred on fenced frame url mapping.
  NavigationRequest* request = fenced_frame_root_node->navigation_request();
  if (!request->is_deferred_on_fenced_frame_url_mapping_for_testing()) {
    base::RunLoop run_loop;
    request->set_begin_navigation_callback_for_testing(
        run_loop.QuitWhenIdleClosure());
    run_loop.Run();

    EXPECT_TRUE(request->is_deferred_on_fenced_frame_url_mapping_for_testing());
  }

  EXPECT_TRUE(url_mapping_test_peer.HasObserver(urn_uuid, request));

  // Navigate to a new URL. The previous navigation should have been canceled.
  // And `request` should have been removed from `url_mapping`.
  const GURL new_url =
      https_server()->GetURL("a.test", "/fenced_frames/empty.html");
  EXPECT_EQ(new_url.spec(),
            EvalJs(root, JsReplace("f.src = $1;", new_url.spec())));

  EXPECT_FALSE(url_mapping_test_peer.HasObserver(urn_uuid, request));

  observer.Wait();

  EXPECT_EQ(
      new_url,
      fenced_frame_root_node->current_frame_host()->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(FencedFrameParameterizedBrowserTest,
                       CheckFencedFrameCookiesNavigation) {
  // Create an a.test main page and set cookies. Then create a same-origin
  // fenced frame. Its request should not carry the cookies that were set.
  GURL main_url = https_server()->GetURL("a.test", "/hello.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  RenderFrameHostImpl* root_rfh =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetPrimaryFrameTree()
          .root()
          ->current_frame_host();

  // Set SameSite=Lax and SameSite=None cookies and retrieve them.
  EXPECT_TRUE(ExecJs(root_rfh,
                     "document.cookie = 'B=2; SameSite=Lax';"
                     "document.cookie = 'C=2; SameSite=None; Secure';"));
  EXPECT_EQ("B=2; C=2", EvalJs(root_rfh, "document.cookie;"));

  // Test the fenced frame.
  EXPECT_TRUE(ExecJs(root_rfh,
                     "var f = document.createElement('fencedframe');"
                     "f.mode = 'opaque-ads';"
                     "document.body.appendChild(f);"));
  EXPECT_EQ(1U, root_rfh->child_count());

  FrameTreeNode* fenced_frame_root_node =
      GetFencedFrameRootNode(root_rfh->child_at(0));

  EXPECT_TRUE(fenced_frame_root_node->IsFencedFrameRoot());
  EXPECT_TRUE(fenced_frame_root_node->IsInFencedFrameTree());

  GURL https_url(
      https_server()->GetURL("a.test", "/fenced_frames/title1.html"));
  FencedFrameURLMapping& url_mapping =
      root_rfh->GetPage().fenced_frame_urls_map();
  auto urn_uuid = AddAndVerifyFencedFrameURL(&url_mapping, https_url);

  std::string navigate_urn_script = JsReplace("f.src = $1;", urn_uuid);
  NavigateFrameInsideFencedFrameTreeAndWaitForFinishedLoad(
      fenced_frame_root_node, urn_uuid, navigate_urn_script);
  EXPECT_EQ(
      https_url,
      fenced_frame_root_node->current_frame_host()->GetLastCommittedURL());
  EXPECT_EQ(
      url::Origin::Create(https_url),
      fenced_frame_root_node->current_frame_host()->GetLastCommittedOrigin());

  EXPECT_FALSE(CheckAndClearCookieHeader(https_url));

  // Run the same test for an iframe inside the fenced frame. It shouldn't be
  // able to send cookies either.
  // Add a nested iframe inside the fenced frame which needs to be a URL that
  // also opts in to be allowed to load inside of a fenced frame.
  GURL iframe_url(
      https_server()->GetURL("a.test", "/fenced_frames/nested.html"));
  EXPECT_EQ(0U, fenced_frame_root_node->child_count());
  AddIframeInFencedFrame(fenced_frame_root_node, 0);
  NavigateIframeInFencedFrame(fenced_frame_root_node->child_at(0), iframe_url);

  EXPECT_EQ(iframe_url, fenced_frame_root_node->child_at(0)
                            ->current_frame_host()
                            ->GetLastCommittedURL());
  EXPECT_EQ(url::Origin::Create(iframe_url), fenced_frame_root_node->child_at(0)
                                                 ->current_frame_host()
                                                 ->GetLastCommittedOrigin());
  EXPECT_FALSE(CheckAndClearCookieHeader(iframe_url));

  // Check that a subresource request from the main document should have the
  // cookies since that is outside the fenced frame tree.
  ResourceLoadObserver observer(shell());
  GURL image_url = https_server()->GetURL("a.test", "/image.jpg");
  EXPECT_TRUE(
      ExecJs(root_rfh, JsReplace("var img = document.createElement('img');"
                                 "document.body.appendChild(img);",
                                 image_url)));
  std::string load_script = JsReplace("img.src = $1;", image_url.spec());
  EXPECT_EQ(image_url.spec(), EvalJs(root_rfh, load_script));
  observer.WaitForResourceCompletion(image_url);
  EXPECT_TRUE(CheckAndClearCookieHeader(image_url, "B=2; C=2"));
}

IN_PROC_BROWSER_TEST_F(FencedFrameParameterizedBrowserTest,
                       CheckPartitionedCookiesWithNonce) {
  // Create an a.test main page and set cookies. Then create a same-origin
  // fenced frame.
  GURL main_url = https_server()->GetURL("a.test", "/hello.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  RenderFrameHostImpl* root_rfh =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetPrimaryFrameTree()
          .root()
          ->current_frame_host();

  // Set SameSite=Lax and SameSite=None cookies and retrieve them.
  EXPECT_TRUE(ExecJs(root_rfh,
                     "document.cookie = 'B=2; SameSite=Lax';"
                     "document.cookie = 'C=2; SameSite=None; Secure';"));
  EXPECT_EQ("B=2; C=2", EvalJs(root_rfh, "document.cookie;"));

  // Add and navigate a fenced frame.
  EXPECT_TRUE(ExecJs(root_rfh,
                     "var f = document.createElement('fencedframe');"
                     "f.mode = 'opaque-ads';"
                     "document.body.appendChild(f);"));
  EXPECT_EQ(1U, root_rfh->child_count());
  FrameTreeNode* fenced_frame_root_node =
      GetFencedFrameRootNode(root_rfh->child_at(0));
  EXPECT_TRUE(fenced_frame_root_node->IsFencedFrameRoot());
  EXPECT_TRUE(fenced_frame_root_node->IsInFencedFrameTree());

  GURL https_url(
      https_server()->GetURL("a.test", "/fenced_frames/title1.html"));
  FencedFrameURLMapping& url_mapping =
      root_rfh->GetPage().fenced_frame_urls_map();
  auto urn_uuid = AddAndVerifyFencedFrameURL(&url_mapping, https_url);

  std::string navigate_urn_script = JsReplace("f.src = $1;", urn_uuid);
  NavigateFrameInsideFencedFrameTreeAndWaitForFinishedLoad(
      fenced_frame_root_node, urn_uuid, navigate_urn_script);
  EXPECT_EQ(
      https_url,
      fenced_frame_root_node->current_frame_host()->GetLastCommittedURL());
  EXPECT_EQ(
      url::Origin::Create(https_url),
      fenced_frame_root_node->current_frame_host()->GetLastCommittedOrigin());

  // Create cookies in the Fenced Frame.
  EXPECT_TRUE(ExecJs(fenced_frame_root_node->current_frame_host(),
                     "document.cookie = 'B=3; SameSite=Lax';"
                     "document.cookie = 'C=3; SameSite=None; Secure';"));

  const net::IsolationInfo& isolation_info =
      fenced_frame_root_node->current_frame_host()
          ->GetIsolationInfoForSubresources();
  EXPECT_TRUE(isolation_info.nonce());
  absl::optional<net::CookiePartitionKey> partition_key =
      net::CookiePartitionKey::FromNetworkIsolationKey(
          isolation_info.network_isolation_key());
  EXPECT_TRUE(partition_key && partition_key->nonce());
  net::CookiePartitionKeyCollection cookie_partition_key_collection =
      net::CookiePartitionKeyCollection::FromOptional(partition_key);

  std::vector<net::CanonicalCookie> cookies =
      GetCanonicalCookies(shell()->web_contents()->GetBrowserContext(),
                          https_url, cookie_partition_key_collection);
  EXPECT_EQ(2u, cookies.size());
  for (auto cookie : cookies) {
    EXPECT_TRUE(cookie.IsPartitioned());
    EXPECT_TRUE(cookie.PartitionKey() && cookie.PartitionKey()->nonce());
    EXPECT_EQ(cookie.PartitionKey()->nonce(), partition_key->nonce());
    EXPECT_EQ("3", cookie.Value());
  }

  // Run the same test for an iframe inside the fenced frame. It should be
  // able to access the same cookies.
  // Add a nested iframe inside the fenced frame which needs to be a URL that
  // also opts in to be allowed to load inside of a fenced frame.
  GURL iframe_url(
      https_server()->GetURL("a.test", "/fenced_frames/nested.html"));
  EXPECT_EQ(0U, fenced_frame_root_node->child_count());
  AddIframeInFencedFrame(fenced_frame_root_node, 0);
  NavigateIframeInFencedFrame(fenced_frame_root_node->child_at(0), iframe_url);

  EXPECT_EQ(iframe_url, fenced_frame_root_node->child_at(0)
                            ->current_frame_host()
                            ->GetLastCommittedURL());
  EXPECT_EQ(url::Origin::Create(iframe_url), fenced_frame_root_node->child_at(0)
                                                 ->current_frame_host()
                                                 ->GetLastCommittedOrigin());
  EXPECT_EQ("B=3; C=3",
            EvalJs(fenced_frame_root_node->child_at(0)->current_frame_host(),
                   "document.cookie;"));
}

// Tests when a frame is considered a fenced frame or being inside a fenced
// frame tree.
IN_PROC_BROWSER_TEST_F(FencedFrameParameterizedBrowserTest,
                       CheckIsFencedFrame) {
  GURL main_url(https_server()->GetURL("a.test", "/hello.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_TRUE(ExecJs(root,
                     "var fenced_frame = document.createElement('fencedframe');"
                     "document.body.appendChild(fenced_frame);"));

  EXPECT_EQ(1U, root->child_count());

  FrameTreeNode* fenced_frame_root_node =
      GetFencedFrameRootNode(root->child_at(0));
  EXPECT_TRUE(fenced_frame_root_node->IsFencedFrameRoot());
  EXPECT_TRUE(fenced_frame_root_node->IsInFencedFrameTree());

  // Add an iframe.
  EXPECT_TRUE(ExecJs(root,
                     "var iframe = document.createElement('iframe');"
                     "document.body.appendChild(iframe);"));
  EXPECT_EQ(2U, root->child_count());
  EXPECT_FALSE(root->child_at(1)->IsFencedFrameRoot());
  EXPECT_FALSE(root->child_at(1)->IsInFencedFrameTree());

  // Add a nested iframe inside the fenced frame.
  // Before we execute script on the fenced frame, we must navigate it once.
  // This is because the root of a FrameTree does not call
  // RenderFrameHostImpl::RenderFrameCreated() on its owned RFHI, meaning there
  // is nothing to execute script in.
  {
    // Navigate the fenced frame.
    GURL fenced_frame_url(
        https_server()->GetURL("a.test", "/fenced_frames/title1.html"));
    std::string navigate_script =
        JsReplace("fenced_frame.src = $1;", fenced_frame_url.spec());
    NavigateFrameInsideFencedFrameTreeAndWaitForFinishedLoad(
        fenced_frame_root_node, fenced_frame_url, navigate_script);
  }

  AddIframeInFencedFrame(fenced_frame_root_node, 0);
  AddNestedFencedFrame(fenced_frame_root_node, 1);
  EXPECT_EQ(2U, fenced_frame_root_node->child_count());
}

// Tests a nonce is correctly set in the isolation info for a fenced frame tree.
IN_PROC_BROWSER_TEST_F(FencedFrameParameterizedBrowserTest,
                       CheckIsolationInfoAndStorageKeyNonce) {
  GURL main_url(https_server()->GetURL("a.test", "/hello.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_TRUE(ExecJs(root,
                     "var f = document.createElement('fencedframe');"
                     "document.body.appendChild(f);"));
  EXPECT_EQ(1U, root->child_count());

  auto* fenced_frame = GetFencedFrameRootNode(root->child_at(0));
  EXPECT_TRUE(fenced_frame->IsFencedFrameRoot());
  EXPECT_TRUE(fenced_frame->IsInFencedFrameTree());

  // Before we check the IsolationInfo/StorageKey on the fenced frame, we must
  // navigate it once. This is because the root of a FrameTree does not call
  // RenderFrameHostImpl::RenderFrameCreated() on its owned RFHI.
  {
    // Navigate the fenced frame.
    GURL fenced_frame_url(
        https_server()->GetURL("a.test", "/fenced_frames/title1.html"));
    std::string navigate_script =
        JsReplace("f.src = $1;", fenced_frame_url.spec());
    NavigateFrameInsideFencedFrameTreeAndWaitForFinishedLoad(
        fenced_frame, fenced_frame_url, navigate_script);
  }

  // There should be a nonce in the IsolationInfo.
  const net::IsolationInfo& isolation_info =
      fenced_frame->current_frame_host()->GetIsolationInfoForSubresources();
  EXPECT_TRUE(isolation_info.nonce().has_value());
  absl::optional<base::UnguessableToken> fenced_frame_nonce =
      fenced_frame->GetFencedFrameNonce();
  EXPECT_TRUE(fenced_frame_nonce.has_value());
  EXPECT_EQ(fenced_frame_nonce.value(), isolation_info.nonce().value());

  // There should be a nonce in the StorageKey.
  EXPECT_TRUE(
      fenced_frame->current_frame_host()->storage_key().nonce().has_value());
  EXPECT_EQ(fenced_frame_nonce.value(),
            fenced_frame->current_frame_host()->storage_key().nonce().value());

  // Add an iframe. It should not have a nonce.
  EXPECT_TRUE(ExecJs(root,
                     "var subframe = document.createElement('iframe');"
                     "document.body.appendChild(subframe);"));
  EXPECT_EQ(2U, root->child_count());
  auto* iframe = root->child_at(1);
  EXPECT_FALSE(iframe->IsFencedFrameRoot());
  EXPECT_FALSE(iframe->IsInFencedFrameTree());
  const net::IsolationInfo& iframe_isolation_info =
      iframe->current_frame_host()->GetIsolationInfoForSubresources();
  EXPECT_FALSE(iframe_isolation_info.nonce().has_value());
  EXPECT_FALSE(iframe->current_frame_host()->storage_key().nonce().has_value());

  // Navigate the iframe. It should still not have a nonce.
  EXPECT_TRUE(NavigateToURLFromRenderer(
      iframe, https_server()->GetURL("b.test", "/title1.html")));
  const net::IsolationInfo& iframe_new_isolation_info =
      iframe->current_frame_host()->GetIsolationInfoForSubresources();

  EXPECT_FALSE(iframe_new_isolation_info.nonce().has_value());
  EXPECT_FALSE(iframe->current_frame_host()->storage_key().nonce().has_value());

  // Add a nested iframe inside the fenced frame which needs to be a URL that
  // also opts in to be allowed to load inside of a fenced frame.
  AddIframeInFencedFrame(fenced_frame, 0);
  const net::IsolationInfo& nested_iframe_isolation_info =
      fenced_frame->child_at(0)
          ->current_frame_host()
          ->GetIsolationInfoForSubresources();
  EXPECT_TRUE(nested_iframe_isolation_info.nonce().has_value());

  // Check that a nested iframe in the fenced frame tree has the same nonce
  // value as its parent.
  EXPECT_EQ(fenced_frame_nonce.value(),
            nested_iframe_isolation_info.nonce().value());
  absl::optional<base::UnguessableToken> nested_iframe_nonce =
      fenced_frame->child_at(0)->GetFencedFrameNonce();
  EXPECT_EQ(nested_iframe_isolation_info.nonce().value(),
            nested_iframe_nonce.value());
  EXPECT_EQ(fenced_frame_nonce.value(), fenced_frame->child_at(0)
                                            ->current_frame_host()
                                            ->storage_key()
                                            .nonce()
                                            .value());

  // Navigate the iframe. It should still have the same nonce.
  NavigateIframeInFencedFrame(
      fenced_frame->child_at(0),
      https_server()->GetURL("b.test", "/fenced_frames/title1.html"));
  const net::IsolationInfo& nested_iframe_new_isolation_info =
      fenced_frame->child_at(0)
          ->current_frame_host()
          ->GetIsolationInfoForSubresources();
  EXPECT_EQ(nested_iframe_new_isolation_info.nonce().value(),
            nested_iframe_nonce.value());
  EXPECT_EQ(fenced_frame_nonce.value(), fenced_frame->child_at(0)
                                            ->current_frame_host()
                                            ->storage_key()
                                            .nonce()
                                            .value());

  // Add a nested fenced frame.
  auto* nested_fenced_frame = AddNestedFencedFrame(fenced_frame, 1);
  GetFencedFrameRootNode(fenced_frame->child_at(1));
  absl::optional<base::UnguessableToken> nested_fframe_nonce =
      nested_fenced_frame->GetFencedFrameNonce();
  EXPECT_TRUE(nested_fframe_nonce.has_value());

  // Check that a nested fenced frame has a different value than its parent
  // fenced frame.
  EXPECT_NE(fenced_frame_nonce.value(), nested_fframe_nonce.value());

  // Check that the nonce does not change when there is a cross-document
  // navigation.
  NavigateNestedFencedFrame(
      nested_fenced_frame,
      https_server()->GetURL("b.test", "/fenced_frames/title1.html"));
  absl::optional<base::UnguessableToken> new_fenced_frame_nonce =
      fenced_frame->GetFencedFrameNonce();
  EXPECT_NE(absl::nullopt, new_fenced_frame_nonce);
  EXPECT_EQ(new_fenced_frame_nonce.value(), fenced_frame_nonce.value());
}

// Tests that a fenced frame and a same-origin iframe at the same level do not
// share the same storage partition.
IN_PROC_BROWSER_TEST_F(FencedFrameParameterizedBrowserTest,
                       CheckUniqueStorage) {
  GURL main_url(https_server()->GetURL("a.test", "/hello.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_TRUE(ExecJs(root,
                     "var f = document.createElement('fencedframe');"
                     "document.body.appendChild(f);"));
  EXPECT_EQ(1U, root->child_count());

  auto* fenced_frame = GetFencedFrameRootNode(root->child_at(0));
  EXPECT_TRUE(fenced_frame->IsFencedFrameRoot());
  EXPECT_TRUE(fenced_frame->IsInFencedFrameTree());

  // Before we check the storage key on the fenced frame, we must navigate it
  // once. This is because the root of a FrameTree does not call
  // RenderFrameHostImpl::RenderFrameCreated() on its owned RFHI.
  {
    // Navigate the fenced frame.
    GURL fenced_frame_url(
        https_server()->GetURL("a.test", "/fenced_frames/title1.html"));
    std::string navigate_script =
        JsReplace("f.src = $1;", fenced_frame_url.spec());
    NavigateFrameInsideFencedFrameTreeAndWaitForFinishedLoad(
        fenced_frame, fenced_frame_url, navigate_script);
  }

  // There should be a nonce in the StorageKey.
  EXPECT_TRUE(
      fenced_frame->current_frame_host()->storage_key().nonce().has_value());

  absl::optional<base::UnguessableToken> fenced_frame_nonce =
      fenced_frame->GetFencedFrameNonce();
  EXPECT_TRUE(fenced_frame_nonce.has_value());
  EXPECT_EQ(fenced_frame_nonce.value(),
            fenced_frame->current_frame_host()->storage_key().nonce().value());

  // Add an iframe.
  EXPECT_TRUE(ExecJs(root,
                     "var subframe = document.createElement('iframe');"
                     "document.body.appendChild(subframe);"));
  EXPECT_EQ(2U, root->child_count());
  auto* iframe = root->child_at(1);
  EXPECT_FALSE(iframe->IsFencedFrameRoot());
  EXPECT_FALSE(iframe->IsInFencedFrameTree());
  EXPECT_FALSE(iframe->current_frame_host()->storage_key().nonce().has_value());

  // Navigate the iframe. It should still not have a nonce.
  EXPECT_TRUE(NavigateToURLFromRenderer(
      iframe, https_server()->GetURL("a.test", "/title1.html")));

  EXPECT_FALSE(iframe->current_frame_host()->storage_key().nonce().has_value());

  // Set and read a value in the fenced frame's local storage.
  EXPECT_TRUE(ExecJs(fenced_frame, "localStorage[\"foo\"] = \"a\""));
  EXPECT_EQ("a", EvalJs(fenced_frame, "localStorage[\"foo\"]"));

  // Set and read a value in the iframe's local storage.
  EXPECT_TRUE(ExecJs(iframe, "localStorage[\"foo\"] = \"b\""));
  EXPECT_EQ("b", EvalJs(iframe, "localStorage[\"foo\"]"));

  // Set and read a value in the top-frame's local storage.
  EXPECT_TRUE(ExecJs(root, "localStorage[\"foo\"] = \"c\""));
  EXPECT_EQ("c", EvalJs(root, "localStorage[\"foo\"]"));

  // This shouldn't impact the fenced frame's local storage:
  EXPECT_EQ("a", EvalJs(fenced_frame, "localStorage[\"foo\"]"));
}

IN_PROC_BROWSER_TEST_F(FencedFrameParameterizedBrowserTest,
                       CheckFencedFrameNotNavigatedWithoutOptIn) {
  base::HistogramTester histogram_tester;
  GURL main_url = https_server()->GetURL("b.test", "/hello.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  {
    EXPECT_TRUE(ExecJs(root,
                       "var f = document.createElement('fencedframe');"
                       "f.mode = 'opaque-ads';"
                       "document.body.appendChild(f);"));
  }
  EXPECT_EQ(1U, root->child_count());
  FrameTreeNode* fenced_frame_root_node =
      GetFencedFrameRootNode(root->child_at(0));

  GURL https_url(https_server()->GetURL("a.test", "/title1.html"));
  FencedFrameURLMapping& url_mapping =
      root->current_frame_host()->GetPage().fenced_frame_urls_map();
  auto urn_uuid = AddAndVerifyFencedFrameURL(&url_mapping, https_url);

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  auto filter =
      [](const content::WebContentsConsoleObserver::Message& message) {
        return message.log_level == blink::mojom::ConsoleMessageLevel::kError;
      };
  console_observer.SetFilter(base::BindRepeating(filter));
  console_observer.SetPattern("Supports-Loading-Mode*");

  std::string navigate_urn_script = JsReplace("f.src = $1;", urn_uuid);
  NavigateFrameInsideFencedFrameTreeAndWaitForFinishedLoad(
      fenced_frame_root_node, urn_uuid, navigate_urn_script,
      net::ERR_BLOCKED_BY_RESPONSE);

  EXPECT_FALSE(console_observer.messages().empty());
  EXPECT_EQ(
      console_observer.GetMessageAt(0),
      "Supports-Loading-Mode HTTP response header 'fenced-frame' is required "
      "to load the fenced frame root and its nested iframes.");
  content::FetchHistogramsFromChildProcesses();
  histogram_tester.ExpectTotalCount(
      blink::kFencedFrameCreationOrNavigationOutcomeHistogram, 2);
  // Fenced frame creation succeeded (opaque ads mode)
  histogram_tester.ExpectBucketCount(
      blink::kFencedFrameCreationOrNavigationOutcomeHistogram,
      blink::FencedFrameCreationOutcome::kSuccessOpaque, 1);
  // Fenced frame navigation failed (Supports-Loading-Mode response header
  // 'fenced-frame' not opted-in)
  histogram_tester.ExpectBucketCount(
      blink::kFencedFrameCreationOrNavigationOutcomeHistogram,
      blink::FencedFrameCreationOutcome::kResponseHeaderNotOptIn, 1);
}

IN_PROC_BROWSER_TEST_F(FencedFrameParameterizedBrowserTest,
                       CheckNestedIframeNotNavigatedWithoutOptIn) {
  base::HistogramTester histogram_tester;
  GURL main_url = https_server()->GetURL("b.test", "/hello.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  {
    EXPECT_TRUE(ExecJs(root,
                       "var f = document.createElement('fencedframe');"
                       "document.body.appendChild(f);"));
  }
  EXPECT_EQ(1U, root->child_count());
  FrameTreeNode* fenced_frame_root_node =
      GetFencedFrameRootNode(root->child_at(0));

  {
    // Navigate the fenced frame.
    GURL fenced_frame_url(
        https_server()->GetURL("a.test", "/fenced_frames/title1.html"));
    std::string navigate_script =
        JsReplace("f.src = $1;", fenced_frame_url.spec());
    NavigateFrameInsideFencedFrameTreeAndWaitForFinishedLoad(
        fenced_frame_root_node, fenced_frame_url, navigate_script);
  }

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  auto filter =
      [](const content::WebContentsConsoleObserver::Message& message) {
        return message.log_level == blink::mojom::ConsoleMessageLevel::kError;
      };
  console_observer.SetFilter(base::BindRepeating(filter));
  console_observer.SetPattern("Supports-Loading-Mode*");

  // Add a nested iframe inside the fenced frame and navigate.
  AddIframeInFencedFrame(fenced_frame_root_node, 0);
  GURL iframe_url(https_server()->GetURL("a.test", "/title1.html"));
  NavigateIframeInFencedFrame(fenced_frame_root_node->child_at(0), iframe_url,
                              net::ERR_BLOCKED_BY_RESPONSE);

  EXPECT_FALSE(console_observer.messages().empty());
  EXPECT_EQ(
      console_observer.GetMessageAt(0),
      "Supports-Loading-Mode HTTP response header 'fenced-frame' is required "
      "to load the fenced frame root and its nested iframes.");
  content::FetchHistogramsFromChildProcesses();
  histogram_tester.ExpectTotalCount(
      blink::kFencedFrameCreationOrNavigationOutcomeHistogram, 2);
  // Fenced frame creation succeeded (default mode)
  histogram_tester.ExpectBucketCount(
      blink::kFencedFrameCreationOrNavigationOutcomeHistogram,
      blink::FencedFrameCreationOutcome::kSuccessDefault, 1);
  // Fenced frame navigation failed (Supports-Loading-Mode response header
  // 'fenced-frame' not opted-in)
  histogram_tester.ExpectBucketCount(
      blink::kFencedFrameCreationOrNavigationOutcomeHistogram,
      blink::FencedFrameCreationOutcome::kResponseHeaderNotOptIn, 1);
}

IN_PROC_BROWSER_TEST_F(FencedFrameParameterizedBrowserTest,
                       CheckSecFetchDestHeader) {
  GURL main_url(https_server()->GetURL("a.test", "/hello.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_TRUE(ExecJs(root,
                     "var fenced_frame = document.createElement('fencedframe');"
                     "document.body.appendChild(fenced_frame);"));

  EXPECT_EQ(1U, root->child_count());

  FrameTreeNode* fenced_frame_root_node =
      GetFencedFrameRootNode(root->child_at(0));

  {
    // Navigate the fenced frame.
    GURL fenced_frame_url(
        https_server()->GetURL("a.test", "/fenced_frames/title1.html"));
    std::string navigate_script =
        JsReplace("fenced_frame.src = $1;", fenced_frame_url.spec());
    NavigateFrameInsideFencedFrameTreeAndWaitForFinishedLoad(
        fenced_frame_root_node, fenced_frame_url, navigate_script);
    EXPECT_TRUE(
        CheckAndClearSecFetchDestHeader(fenced_frame_url, "fencedframe"));
  }

  // Add a nested iframe inside the fenced frame and navigate.
  AddIframeInFencedFrame(fenced_frame_root_node, 0);
  GURL iframe_url(
      https_server()->GetURL("a.test", "/fenced_frames/title1.html"));
  NavigateIframeInFencedFrame(fenced_frame_root_node->child_at(0), iframe_url);
  EXPECT_TRUE(CheckAndClearSecFetchDestHeader(iframe_url, "fencedframe"));
}

IN_PROC_BROWSER_TEST_F(FencedFrameParameterizedBrowserTest,
                       CheckOpaqueUrlFlag) {
  GURL main_url(https_server()->GetURL("a.test", "/hello.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // Create a fenced frame.
  EXPECT_TRUE(ExecJs(root,
                     "var f = document.createElement('fencedframe');"
                     "f.mode = 'opaque-ads';"
                     "document.body.appendChild(f);"));

  EXPECT_EQ(1U, root->child_count());

  FrameTreeNode* fenced_frame_root_node =
      GetFencedFrameRootNode(root->child_at(0));

  // Navigate the fenced frame from the initial empty document toward a URL
  // with a client side redirect.
  //
  // With MPArch, since this was a navigation toward an opaque URL, in the
  // 'opaque-ads' mode, initiated from the embedder, the navigation must use
  // and commit a document with
  // `is_fenced_frame_root_originating_from_opaque_url` to true.
  GURL fenced_frame_url(
      https_server()->GetURL("a.test", "/fenced_frames/redirect.html"));
  FencedFrameURLMapping& url_mapping =
      root->current_frame_host()->GetPage().fenced_frame_urls_map();
  auto urn_uuid = AddAndVerifyFencedFrameURL(&url_mapping, fenced_frame_url);

  std::string navigate_script = JsReplace("f.src = $1;", urn_uuid.spec());
  NavigateFrameInsideFencedFrameTreeAndWaitForFinishedLoad(
      fenced_frame_root_node, fenced_frame_url, navigate_script);

  EXPECT_TRUE(fenced_frame_root_node->current_frame_host()
                  ->is_fenced_frame_root_originating_from_opaque_url());

  // Navigate the fenced frame again, but toward a non-opaque URL. Since this
  // is initiated from the embedder, the new document must commit with
  // `is_fenced_frame_root_originating_from_opaque_url` to false.
  GURL second_url(
      https_server()->GetURL("a.test", "/fenced_frames/title0.html"));
  std::string second_navigate_script =
      JsReplace("f.src = $1;", second_url.spec());
  NavigateFrameInsideFencedFrameTreeAndWaitForFinishedLoad(
      fenced_frame_root_node, fenced_frame_url, second_navigate_script);
  EXPECT_FALSE(fenced_frame_root_node->current_frame_host()
                   ->is_fenced_frame_root_originating_from_opaque_url());
}

IN_PROC_BROWSER_TEST_F(FencedFrameParameterizedBrowserTest,
                       CancelledNavigationCheckOpaqueUrlFlag) {
  GURL main_url(https_server()->GetURL("a.test", "/hello.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // Create a fenced frame.
  EXPECT_TRUE(ExecJs(root,
                     "var f = document.createElement('fencedframe');"
                     "f.mode = 'opaque-ads';"
                     "document.body.appendChild(f);"));

  EXPECT_EQ(1U, root->child_count());

  FrameTreeNode* fenced_frame_root_node =
      GetFencedFrameRootNode(root->child_at(0));

  // Navigate the fenced frame from the initial empty document toward an opaque
  // URL. With MPArch, since this was in the 'opaque-ads' mode, initiated from
  // the embedder, the navigation must use and commit a document with
  // `is_fenced_frame_root_originating_from_opaque_url` to true.
  GURL fenced_frame_url(
      https_server()->GetURL("a.test", "/fenced_frames/title1.html"));
  FencedFrameURLMapping& url_mapping =
      root->current_frame_host()->GetPage().fenced_frame_urls_map();
  auto urn_uuid = AddAndVerifyFencedFrameURL(&url_mapping, fenced_frame_url);

  std::string navigate_script = JsReplace("f.src = $1;", urn_uuid.spec());
  NavigateFrameInsideFencedFrameTreeAndWaitForFinishedLoad(
      fenced_frame_root_node, fenced_frame_url, navigate_script);

  EXPECT_TRUE(fenced_frame_root_node->current_frame_host()
                  ->is_fenced_frame_root_originating_from_opaque_url());

  // Navigate the fenced frame again, but toward a non-opaque URL and the
  // navigation is cancelled. The navigation is not committed and therefore
  // `is_fenced_frame_root_originating_from_opaque_url` of the document doesn't
  // change.
  GURL second_url(https_server()->GetURL("a.test", "/nocontent"));
  std::string second_navigate_script =
      JsReplace("f.src = $1;", second_url.spec());
  NavigateFrameInsideFencedFrameTreeAndWaitForFinishedLoad(
      fenced_frame_root_node, fenced_frame_url, second_navigate_script,
      net::ERR_ABORTED);

  EXPECT_EQ(fenced_frame_root_node->current_frame_host()->GetLastCommittedURL(),
            fenced_frame_url);

  // The fenced frame's document initiates a navigation. The previous cancelled
  // navigation from the embedder shouldn't have made any side effects. The next
  // committed document must continue to have
  // `is_fenced_frame_root_originating_from_opaque_url` true.
  GURL redirect_url(
      https_server()->GetURL("a.test", "/fenced_frames/title0.html"));
  EXPECT_TRUE(ExecJs(fenced_frame_root_node->current_frame_host(),
                     JsReplace("location.href = $1;", redirect_url.spec())));
  EXPECT_TRUE(content::WaitForLoadStop(shell()->web_contents()));

  EXPECT_EQ(fenced_frame_root_node->current_frame_host()->GetLastCommittedURL(),
            redirect_url);

  EXPECT_TRUE(fenced_frame_root_node->current_frame_host()
                  ->is_fenced_frame_root_originating_from_opaque_url());
}

namespace {
class ScopedInsecureContentTestContentBrowserClient
    : public TestContentBrowserClient {
 public:
  ScopedInsecureContentTestContentBrowserClient()
      : old_client(SetBrowserClientForTesting(this)) {}
  ~ScopedInsecureContentTestContentBrowserClient() override {
    SetBrowserClientForTesting(old_client);
  }

  void OverrideWebkitPrefs(WebContents* web_contents,
                           blink::web_pref::WebPreferences* prefs) override {
    // Browser will both run and display insecure content.
    prefs->allow_running_insecure_content = true;
  }

 private:
  raw_ptr<ContentBrowserClient> old_client;
};
}  // namespace

class FencedFrameIgnoreCertErrors : public FencedFrameParameterizedBrowserTest {
 public:
  FencedFrameIgnoreCertErrors()
      : https_server_mismatched_(ServerType::TYPE_HTTPS) {}

 protected:
  // Tests should call CreateWebContents() to use web_contents() in the test.
  void CreateWebContents() {
    ASSERT_FALSE(web_contents_.get());
    web_contents_ =
        WebContents::Create(WebContents::CreateParams(browser_context_.get()));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    FencedFrameParameterizedBrowserTest::SetUpCommandLine(command_line);
    // Browser will ignore certificate errors.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void TearDownOnMainThread() override {
    web_contents_.reset();
    FencedFrameParameterizedBrowserTest::TearDownOnMainThread();
  }

  void TearDown() override {
    GetUIThreadTaskRunner({})->DeleteSoon(FROM_HERE,
                                          browser_context_.release());
    FencedFrameParameterizedBrowserTest::TearDown();
  }

  net::EmbeddedTestServer* https_server_mismatched() {
    return &https_server_mismatched_;
  }

  WebContents* web_contents() {
    // web_contents_ should be initialized before calling this method.
    EXPECT_TRUE(web_contents_.get());
    return web_contents_.get();
  }

 private:
  void AdditionalSetup() override {
    https_server_mismatched_.ServeFilesFromSourceDirectory(
        GetTestDataFilePath());
    https_server_mismatched_.SetSSLConfig(
        net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
    ASSERT_TRUE(https_server_mismatched_.Start());

    // We need to have a dedicated browser context for the tests.
    // Or, SSLManager::UpdateEntry() doesn't update the entry if
    // |ssl_host_state_delegate_| is nullptr.
    browser_context_ = std::make_unique<TestBrowserContext>();

    https_server()->RegisterRequestMonitor(base::BindRepeating(
        &FencedFrameParameterizedBrowserTest::ObserveRequestHeaders,
        base::Unretained(this)));
  }

  net::EmbeddedTestServer https_server_mismatched_;
  std::unique_ptr<BrowserContext> browser_context_;
  std::unique_ptr<WebContents> web_contents_;
};

IN_PROC_BROWSER_TEST_F(FencedFrameIgnoreCertErrors, FencedframeHasCertError) {
  CreateWebContents();
  // Allow insecure content.
  ScopedInsecureContentTestContentBrowserClient scoped_content_browser_client;

  GURL main_frame_url =
      https_server_mismatched()->GetURL("a.test", "/hello.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), main_frame_url));
  EXPECT_FALSE(web_contents()
                   ->GetController()
                   .GetLastCommittedEntry()
                   ->GetSSL()
                   .content_status &
               SSLStatus::RAN_CONTENT_WITH_CERT_ERRORS);

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // Create a fenced frame element.
  EXPECT_TRUE(ExecJs(root,
                     "var f = document.createElement('fencedframe');"
                     "document.body.appendChild(f);"));
  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* fenced_frame_root_node =
      GetFencedFrameRootNode(root->child_at(0));
  EXPECT_TRUE(fenced_frame_root_node->IsInFencedFrameTree());

  // Navigate the fenced frame.
  GURL fenced_frame_url(https_server_mismatched()->GetURL(
      "b.test", "/fenced_frames/title1.html"));
  TestFrameNavigationObserver observer(fenced_frame_root_node);
  EXPECT_TRUE(ExecJs(root, JsReplace("f.src = $1;", fenced_frame_url.spec())));
  observer.WaitForCommit();
  EXPECT_EQ(
      fenced_frame_url,
      fenced_frame_root_node->current_frame_host()->GetLastCommittedURL());

  EXPECT_TRUE(web_contents()
                  ->GetController()
                  .GetLastCommittedEntry()
                  ->GetSSL()
                  .content_status &
              SSLStatus::RAN_CONTENT_WITH_CERT_ERRORS);
}

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
                       ShouldIgnoreJsDialog) {
  GURL main_url(https_server()->GetURL("a.test", "/hello.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_TRUE(ExecJs(root,
                     "var fenced_frame = document.createElement('fencedframe');"
                     "document.body.appendChild(fenced_frame);"));

  EXPECT_EQ(1U, root->child_count());

  FrameTreeNode* fenced_frame_root_node =
      GetFencedFrameRootNode(root->child_at(0));

  {
    // Navigate the fenced frame.
    GURL fenced_frame_url(
        https_server()->GetURL("a.test", "/fenced_frames/title1.html"));
    std::string navigate_script =
        JsReplace("fenced_frame.src = $1;", fenced_frame_url.spec());
    NavigateFrameInsideFencedFrameTreeAndWaitForFinishedLoad(
        fenced_frame_root_node, fenced_frame_url, navigate_script);
    EXPECT_TRUE(
        CheckAndClearSecFetchDestHeader(fenced_frame_url, "fencedframe"));
  }

  // Setup test dialog manager and create dialog.
  TestJavaScriptDialogManager dialog_manager;
  web_contents()->SetDelegate(&dialog_manager);
  web_contents()->RunJavaScriptDialog(web_contents()->GetPrimaryMainFrame(),
                                      u"", u"", JAVASCRIPT_DIALOG_TYPE_ALERT,
                                      false, base::NullCallback());

  {
    // Navigate fenced frame.
    const GURL new_url =
        https_server()->GetURL("a.test", "/fenced_frames/empty.html");
    std::string navigate_script =
        JsReplace("fenced_frame.src = $1;", new_url.spec());
    NavigateFrameInsideFencedFrameTreeAndWaitForFinishedLoad(
        fenced_frame_root_node, new_url, navigate_script);
  }

  // We should not dismiss dialogs when the fenced frame's subframe navigates
  // and swaps its RFH.
  EXPECT_FALSE(dialog_manager.cancel_dialogs_called());

  // Clean up test dialog manager.
  web_contents()->SetDelegate(nullptr);
  web_contents()->SetJavaScriptDialogManagerForTesting(nullptr);
}

// An observer class that asserts the page transition always is
// `ui::PageTransition::PAGE_TRANSITION_AUTO_SUBFRAME`.
class AlwaysAutoSubframeNavigationObserver : public WebContentsObserver {
 public:
  explicit AlwaysAutoSubframeNavigationObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
        navigation_handle->GetPageTransition(),
        ui::PageTransition::PAGE_TRANSITION_AUTO_SUBFRAME));
  }
};

// Tests that any navigation or history API calls always replace the current
// entry and do not increase the back/forward entries.
IN_PROC_BROWSER_TEST_F(FencedFrameParameterizedBrowserTest,
                       NavigationAndHistoryShouldBeReplaceOnly) {
  GURL main_url(https_server()->GetURL("a.test", "/hello.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // Add the fenced frame element.
  EXPECT_TRUE(ExecJs(root,
                     "var f = document.createElement('fencedframe');"
                     "document.body.appendChild(f);"));
  EXPECT_EQ(1U, root->child_count());

  auto* fenced_frame = GetFencedFrameRootNode(root->child_at(0));
  EXPECT_TRUE(fenced_frame->IsFencedFrameRoot());
  EXPECT_TRUE(fenced_frame->IsInFencedFrameTree());

  // Instantiate a navigation observer to assert from here on the navigations
  // are always `ui::PageTransition::PAGE_TRANSITION_AUTO_SUBFRAME`.
  AlwaysAutoSubframeNavigationObserver auto_subframe_observer(
      shell()->web_contents());

  EXPECT_EQ(1, fenced_frame->navigator().controller().GetEntryCount());

  // 1. Navigate the fenced frame: both cross-document and fragment navigation.
  GURL fenced_frame_url(
      https_server()->GetURL("a.test", "/fenced_frames/title1.html"));
  std::string navigate_script =
      JsReplace("f.src = $1;", fenced_frame_url.spec());
  NavigateFrameInsideFencedFrameTreeAndWaitForFinishedLoad(
      fenced_frame, fenced_frame_url, navigate_script);

  GURL fragment_url(
      https_server()->GetURL("a.test", "/fenced_frames/title1.html#123"));
  navigate_script = JsReplace("f.src = $1;", fragment_url.spec());
  NavigateFrameInsideFencedFrameTreeAndWaitForFinishedLoad(
      fenced_frame, fragment_url, navigate_script);
  EXPECT_EQ(1, fenced_frame->navigator().controller().GetEntryCount());
  EXPECT_EQ(1, root->navigator().controller().GetEntryCount());

  // Do a cross-site navigation to exercise RemoteFrame::Navigate path in the
  // navigation after this one.
  GURL cross_site_url =
      https_server()->GetURL("d.test", "/fenced_frames/title1.html");
  std::string navigate_script_2 =
      JsReplace("f.src = $1;", cross_site_url.spec());
  NavigateFrameInsideFencedFrameTreeAndWaitForFinishedLoad(
      fenced_frame, cross_site_url, navigate_script_2);
  NavigateFrameInsideFencedFrameTreeAndWaitForFinishedLoad(
      fenced_frame, fenced_frame_url, navigate_script);
  EXPECT_EQ(1, fenced_frame->navigator().controller().GetEntryCount());
  EXPECT_EQ(1, root->navigator().controller().GetEntryCount());

  // 2. Do a pushState in the fenced frame which would've normally added a new
  // history entry. The entry count should stay at 1. Also test a replaceState,
  // reload and location.replace.
  UpdateHistoryOrReloadFromFencedFrameTreeAndWaitForFinishedLoad(
      fenced_frame, "window.history.pushState({}, null);");
  UpdateHistoryOrReloadFromFencedFrameTreeAndWaitForFinishedLoad(
      fenced_frame, "window.history.replaceState({}, null);");
  UpdateHistoryOrReloadFromFencedFrameTreeAndWaitForFinishedLoad(
      fenced_frame, "window.location.reload()");
  GURL replace_url(
      https_server()->GetURL("c.test", "/fenced_frames/title1.html"));
  std::string script = JsReplace("location.replace($1);", replace_url);
  UpdateHistoryOrReloadFromFencedFrameTreeAndWaitForFinishedLoad(fenced_frame,
                                                                 script);
  EXPECT_EQ(1, fenced_frame->navigator().controller().GetEntryCount());
  EXPECT_EQ(1, root->navigator().controller().GetEntryCount());

  // 3. Add an iframe to the fenced frame and navigate it. The entry count
  // should stay at 1.
  AddIframeInFencedFrame(fenced_frame, 0 /* child_index */);
  NavigateIframeInFencedFrame(
      fenced_frame->child_at(0),
      https_server()->GetURL("b.test", "/fenced_frames/title1.html"));
  EXPECT_EQ(
      1, fenced_frame->child_at(0)->navigator().controller().GetEntryCount());
  EXPECT_EQ(1, root->navigator().controller().GetEntryCount());

  // 4. Do history changes from the iframe. The entry count should
  // stay at 1.
  UpdateHistoryOrReloadFromFencedFrameTreeAndWaitForFinishedLoad(
      fenced_frame->child_at(0), "window.history.pushState({}, null);");
  UpdateHistoryOrReloadFromFencedFrameTreeAndWaitForFinishedLoad(
      fenced_frame->child_at(0), "window.history.replaceState({}, null);");
  UpdateHistoryOrReloadFromFencedFrameTreeAndWaitForFinishedLoad(
      fenced_frame->child_at(0), "window.location.reload()");
  UpdateHistoryOrReloadFromFencedFrameTreeAndWaitForFinishedLoad(
      fenced_frame->child_at(0), script);
  EXPECT_EQ(
      1, fenced_frame->child_at(0)->navigator().controller().GetEntryCount());
  EXPECT_EQ(1, root->navigator().controller().GetEntryCount());

  // 5. Add a nested fenced frame and navigate it. The entry count should stay
  // at 1.
  FrameTreeNode* nested_fenced_frame =
      AddNestedFencedFrame(fenced_frame, 1 /* child_index */);
  NavigateNestedFencedFrame(
      nested_fenced_frame,
      https_server()->GetURL("b.test", "/fenced_frames/title1.html"));
  EXPECT_EQ(1, nested_fenced_frame->navigator().controller().GetEntryCount());
  EXPECT_EQ(1, root->navigator().controller().GetEntryCount());
  EXPECT_EQ(1, fenced_frame->navigator().controller().GetEntryCount());

  // 6. Do history changes from the nested fenced frame. The entry
  // count should stay at 1.
  UpdateHistoryOrReloadFromFencedFrameTreeAndWaitForFinishedLoad(
      nested_fenced_frame, "window.history.pushState({}, null);");
  UpdateHistoryOrReloadFromFencedFrameTreeAndWaitForFinishedLoad(
      nested_fenced_frame, "window.history.replaceState({}, null);");
  UpdateHistoryOrReloadFromFencedFrameTreeAndWaitForFinishedLoad(
      nested_fenced_frame, "window.location.reload()");
  UpdateHistoryOrReloadFromFencedFrameTreeAndWaitForFinishedLoad(
      nested_fenced_frame, script);
  EXPECT_EQ(1, nested_fenced_frame->navigator().controller().GetEntryCount());
  EXPECT_EQ(1, root->navigator().controller().GetEntryCount());
  EXPECT_EQ(1, fenced_frame->navigator().controller().GetEntryCount());
}

// Tests successfully going back to a page with a fenced frame.
IN_PROC_BROWSER_TEST_F(FencedFrameParameterizedBrowserTest,
                       GoBackToPageWithFencedFrame) {
  GURL main_url(https_server()->GetURL(
      "a.test", "/fenced_frames/basic_fenced_frame_src.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_EQ(1U, root->child_count());
  auto* fenced_frame = GetFencedFrameRootNode(root->child_at(0));
  EXPECT_TRUE(fenced_frame->IsFencedFrameRoot());
  EXPECT_TRUE(fenced_frame->IsInFencedFrameTree());

  GURL fenced_frame_url_1 =
      https_server()->GetURL("a.test", "/fenced_frames/title1.html");

  CheckNavigationEntryCount(root, fenced_frame, /*shadowdom_cnt=*/1,
                            /*mparch_cnt=*/1);
  EXPECT_EQ(fenced_frame_url_1,
            fenced_frame->current_frame_host()->GetLastCommittedURL());

  // Navigate the fenced frame. It should do a replace navigation and therefore
  // the `controller().GetEntryCount()` stays at 1.
  GURL fenced_frame_url_2(
      https_server()->GetURL("c.test", "/fenced_frames/title1.html"));
  std::string script = JsReplace("location.assign($1);", fenced_frame_url_2);
  UpdateHistoryOrReloadFromFencedFrameTreeAndWaitForFinishedLoad(fenced_frame,
                                                                 script);
  EXPECT_EQ(1, fenced_frame->navigator().controller().GetEntryCount());
  EXPECT_EQ(1, root->navigator().controller().GetEntryCount());
  EXPECT_EQ(fenced_frame_url_2,
            fenced_frame->current_frame_host()->GetLastCommittedURL());
  EXPECT_TRUE(WaitForDOMContentLoaded(fenced_frame->current_frame_host()));

  // Navigate the top-level page to another document.
  GURL new_main_url(https_server()->GetURL("b.test", "/hello.html"));
  EXPECT_TRUE(NavigateToURL(shell(), new_main_url));
  EXPECT_EQ(2, root->navigator().controller().GetEntryCount());
  EXPECT_EQ(new_main_url, root->current_frame_host()->GetLastCommittedURL());

  // Go back.
  TestNavigationObserver back_load_observer(shell()->web_contents());
  root->navigator().controller().GoBack();
  back_load_observer.Wait();
  EXPECT_EQ(2, root->navigator().controller().GetEntryCount());

  EXPECT_EQ(1U, root->child_count());
  fenced_frame = GetFencedFrameRootNode(root->child_at(0));
  EXPECT_TRUE(fenced_frame->IsFencedFrameRoot());
  EXPECT_TRUE(fenced_frame->IsInFencedFrameTree());

  // ShadowDOM fenced frames have the same NavigationController as the top-level
  // frame, therefore the count here is 2 because of the navigation of the
  // top-level frame.
  // Note the last committed url is the latest one (`fenced_frame_url_2`) when
  // back/forward cache is enabled. However, when back/forward cache is
  // disabled, it will navigate to `fenced_frame_url_1`. MPArch fenced frame has
  // its own NavigationController which is not retained when the top-level page
  // navigates. Therefore going back lands on the initial navigation in the
  // Fenced Frame.
  // TODO(domfarolino): Before merge remove stuff about shadowdom.
  CheckNavigationEntryCount(root, fenced_frame, /*shadowdom_cnt=*/2,
                            /*mparch_cnt=*/1);

  if (BackForwardCache::IsBackForwardCacheFeatureEnabled()) {
    EXPECT_EQ(fenced_frame_url_2,
              fenced_frame->current_frame_host()->GetLastCommittedURL());
  } else {
    EXPECT_EQ(fenced_frame_url_1,
              fenced_frame->current_frame_host()->GetLastCommittedURL());
  }
}

IN_PROC_BROWSER_TEST_F(FencedFrameParameterizedBrowserTest,
                       ReloadPageWithFencedFrame) {
  GURL main_url(
      https_server()->GetURL("a.test", "/fenced_frames/opaque_ads.html"));
  GURL fenced_frame_url =
      https_server()->GetURL("a.test", "/fenced_frames/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  TestNavigationObserver reload_observer(web_contents());

  EXPECT_TRUE(ExecJs(root, "window.location.reload();"));
  reload_observer.Wait();

  EXPECT_EQ(1, root->navigator().controller().GetEntryCount());
  EXPECT_TRUE(reload_observer.last_navigation_succeeded());
  auto* fenced_frame = GetFencedFrameRootNode(root->child_at(0));
  EXPECT_EQ(fenced_frame_url, fenced_frame->current_url());
}

IN_PROC_BROWSER_TEST_F(FencedFrameParameterizedBrowserTest,
                       NavigateUnfencedTopAndGoBack) {
  GURL main_url(
      https_server()->GetURL("a.test", "/fenced_frames/opaque_ads.html"));
  GURL fenced_frame_url =
      https_server()->GetURL("a.test", "/fenced_frames/title1.html");
  TestNavigationObserver load_observer(web_contents());

  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  load_observer.Wait();

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
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
  EXPECT_EQ(1U, root->child_count());
  fenced_frame = GetFencedFrameRootNode(root->child_at(0));
  EXPECT_TRUE(fenced_frame->IsFencedFrameRoot());
  EXPECT_TRUE(fenced_frame->IsInFencedFrameTree());
  EXPECT_EQ(fenced_frame_url, fenced_frame->current_url());
}

// Simulates the crash in crbug.com/1317642 by disabling BFCache and going back
// to a page with a fenced frame navigation, which in shadowDOM FFs will lead to
// a AUTO_SUBFRAME navigation initiated in the browser.
IN_PROC_BROWSER_TEST_F(FencedFrameParameterizedBrowserTest,
                       GoBackToPageWithFencedFrameNavigationNoBFCache) {
  GURL main_url(https_server()->GetURL(
      "a.test",
      "/fenced_frames/basic_fenced_frame_src_navigate_on_click.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_EQ(1U, root->child_count());
  auto* fenced_frame = GetFencedFrameRootNode(root->child_at(0));
  EXPECT_TRUE(fenced_frame->IsFencedFrameRoot());
  EXPECT_TRUE(fenced_frame->IsInFencedFrameTree());

  // Since the fenced frame is not yet navigated, it's specific controller
  // should have no entries, or should be on the initial NavigationEntry.
  EXPECT_TRUE(!fenced_frame->navigator().controller().GetLastCommittedEntry() ||
              fenced_frame->navigator()
                  .controller()
                  .GetLastCommittedEntry()
                  ->IsInitialEntry());

  TestFrameNavigationObserver observer(fenced_frame);
  EXPECT_TRUE(
      ExecJs(root, "document.getElementsByTagName('button')[0].click();"));
  observer.WaitForCommit();
  GURL fenced_frame_url_1 =
      https_server()->GetURL("a.test", "/fenced_frames/title1.html");

  CheckNavigationEntryCount(root, fenced_frame, /*shadowdom_cnt=*/1,
                            /*mparch_cnt=*/1);
  EXPECT_EQ(fenced_frame_url_1,
            fenced_frame->current_frame_host()->GetLastCommittedURL());
  DisableBackForwardCacheForTesting(shell()->web_contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);

  // Navigate the top-level page to another document.
  GURL new_main_url(https_server()->GetURL("b.test", "/hello.html"));
  EXPECT_TRUE(NavigateToURL(shell(), new_main_url));
  EXPECT_EQ(2, root->navigator().controller().GetEntryCount());
  EXPECT_EQ(new_main_url, root->current_frame_host()->GetLastCommittedURL());

  // Go back.
  TestNavigationObserver back_load_observer(shell()->web_contents());
  root->navigator().controller().GoBack();
  back_load_observer.Wait();
  EXPECT_EQ(2, root->navigator().controller().GetEntryCount());

  EXPECT_EQ(1U, root->child_count());
  fenced_frame = GetFencedFrameRootNode(root->child_at(0));
  EXPECT_TRUE(fenced_frame->IsFencedFrameRoot());
  EXPECT_TRUE(fenced_frame->IsInFencedFrameTree());

  // ShadowDOM fenced frames have the same NavigationController as the top-level
  // frame, therefore the count here is 2 because of the navigation of the
  // top-level frame.
  // Note the last committed url is the latest one in shadowDOM due to the joint
  // history maintained in the single navigation controller and going back can
  // therefore get the latest navigation in the frame which is
  // `fenced_frame_url_1`.
  // MPArch fenced frame has its own NavigationController which is not retained
  // when the top-level page navigates. Therefore going back lands on the
  // initial fenced frame without any navigation.
  CheckNavigationEntryCount(root, fenced_frame, /*shadowdom_cnt=*/2,
                            /*mparch_cnt=*/1);
  EXPECT_TRUE(!fenced_frame->navigator().controller().GetLastCommittedEntry() ||
              fenced_frame->navigator()
                  .controller()
                  .GetLastCommittedEntry()
                  ->IsInitialEntry());
}

IN_PROC_BROWSER_TEST_F(FencedFrameParameterizedBrowserTest,
                       RestorePageWithFencedFrameNavigation) {
  GURL main_url(https_server()->GetURL(
      "a.test",
      "/fenced_frames/basic_fenced_frame_src_navigate_on_click.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_EQ(1U, root->child_count());
  auto* fenced_frame = GetFencedFrameRootNode(root->child_at(0));
  EXPECT_TRUE(fenced_frame->IsFencedFrameRoot());
  EXPECT_TRUE(fenced_frame->IsInFencedFrameTree());

  TestFrameNavigationObserver observer(fenced_frame);
  EXPECT_TRUE(
      ExecJs(root, "document.getElementsByTagName('button')[0].click();"));
  observer.WaitForCommit();
  GURL fenced_frame_url_1 =
      https_server()->GetURL("a.test", "/fenced_frames/title1.html");

  CheckNavigationEntryCount(root, fenced_frame, /*shadowdom_cnt=*/1,
                            /*mparch_cnt=*/1);
  EXPECT_EQ(fenced_frame_url_1,
            fenced_frame->current_frame_host()->GetLastCommittedURL());

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  std::unique_ptr<NavigationEntryImpl> restored_entry =
      NavigationEntryImpl::FromNavigationEntry(
          NavigationController::CreateNavigationEntry(
              main_url, Referrer(), /* initiator_origin= */ absl::nullopt,
              /* initiator_base_url= */ absl::nullopt,
              ui::PAGE_TRANSITION_RELOAD, false, std::string(),
              controller.GetBrowserContext(),
              nullptr /* blob_url_loader_factory */));
  std::unique_ptr<NavigationEntryRestoreContextImpl> context =
      std::make_unique<NavigationEntryRestoreContextImpl>();
  restored_entry->SetPageState(blink::PageState::CreateFromURL(main_url),
                               context.get());
  EXPECT_EQ(0U, restored_entry->root_node()->children.size());

  // Restore the new entry in a new tab and verify the fenced frame loads.
  std::vector<std::unique_ptr<NavigationEntry>> entries;
  entries.push_back(std::move(restored_entry));

  Shell* new_shell = Shell::CreateNewWindow(
      controller.GetBrowserContext(), GURL::EmptyGURL(), nullptr, gfx::Size());
  FrameTreeNode* new_root =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetPrimaryFrameTree()
          .root();
  NavigationControllerImpl& new_controller =
      static_cast<NavigationControllerImpl&>(
          new_shell->web_contents()->GetController());
  new_controller.Restore(entries.size() - 1, RestoreType::kRestored, &entries);

  ASSERT_EQ(0u, entries.size());
  {
    TestNavigationObserver restore_observer(new_shell->web_contents());
    new_controller.LoadIfNecessary();
    restore_observer.Wait();
  }
  ASSERT_EQ(1U, new_root->child_count());
  EXPECT_EQ(main_url, new_root->current_url());

  auto* restored_fenced_frame = GetFencedFrameRootNode(new_root->child_at(0));
  EXPECT_TRUE(restored_fenced_frame->IsFencedFrameRoot());
  EXPECT_TRUE(restored_fenced_frame->IsInFencedFrameTree());

  EXPECT_EQ(1, new_controller.GetEntryCount());
  EXPECT_EQ(0, new_controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* new_entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(main_url, new_entry->root_node()->frame_entry->url());

  // MPArch navigation controller wouldn't have any entry since it's not
  // restored. Therefore we will only have the initial fenced frame without
  // any navigation.
  ASSERT_EQ(0U, new_entry->root_node()->children.size());
  EXPECT_TRUE(!restored_fenced_frame->navigator()
                   .controller()
                   .GetLastCommittedEntry() ||
              restored_fenced_frame->navigator()
                  .controller()
                  .GetLastCommittedEntry()
                  ->IsInitialEntry());
}

IN_PROC_BROWSER_TEST_F(FencedFrameParameterizedBrowserTest,
                       CheckInvalidUrnError) {
  GURL main_url = https_server()->GetURL("b.test", "/hello.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  {
    EXPECT_TRUE(ExecJs(root,
                       "var f = document.createElement('fencedframe');"
                       "f.mode = 'opaque-ads';"
                       "document.body.appendChild(f);"));
  }
  EXPECT_EQ(1U, root->child_count());
  FrameTreeNode* fenced_frame_root_node =
      GetFencedFrameRootNode(root->child_at(0));

  GURL urn_uuid = GURL("urn:uuid:12345678-9abc-def0-1234-56789abcdef0");
  EXPECT_TRUE(urn_uuid.is_valid());

  std::string navigate_urn_script = JsReplace("f.src = $1;", urn_uuid);
  NavigateFrameInsideFencedFrameTreeAndWaitForFinishedLoad(
      fenced_frame_root_node, urn_uuid, navigate_urn_script,
      net::ERR_INVALID_URL);
}

IN_PROC_BROWSER_TEST_F(FencedFrameParameterizedBrowserTest,
                       CheckCSPFencedFrameSrcOpaqueURL) {
  const struct {
    const char* csp;
    bool expect_allowed;
  } kTestCases[]{
      {"fenced-frame-src 'none'", false},
      {"fenced-frame-src 'self'", false},
      {"fenced-frame-src *", true},
      {"fenced-frame-src data:", false},
      {"fenced-frame-src https:", true},
      {"fenced-frame-src https://*:*", true},
      {"fenced-frame-src https://*", false},
      {"fenced-frame-src https://b.test:*", false},
  };

  for (const auto& test_case : kTestCases) {
    GURL main_url = https_server()->GetURL("a.test", "/title1.html");
    EXPECT_TRUE(NavigateToURL(shell(), main_url));

    // It is safe to obtain the root frame tree node here, as it doesn't change.
    FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                              ->GetPrimaryFrameTree()
                              .root();

    EXPECT_TRUE(ExecJs(root, JsReplace(R"(
      var violation = new Promise(resolve => {
        document.addEventListener("securitypolicyviolation", (e) => {
          resolve(e.violatedDirective + ";" + e.blockedURI);
        });
      });

      var meta = document.createElement('meta');
      meta.httpEquiv = 'Content-Security-Policy';
      meta.content = $1;
      document.head.appendChild(meta);
    )",
                                       test_case.csp)));

    EXPECT_TRUE(ExecJs(root,
                       "var f = document.createElement('fencedframe');"
                       "f.mode = 'opaque-ads';"
                       "document.body.appendChild(f);"));

    EXPECT_EQ(1U, root->child_count());

    FrameTreeNode* fenced_frame_root_node =
        GetFencedFrameRootNode(root->child_at(0));

    GURL https_url(
        https_server()->GetURL("b.test", "/fenced_frames/title1.html"));
    FencedFrameURLMapping& url_mapping =
        root->current_frame_host()->GetPage().fenced_frame_urls_map();
    auto urn_uuid = AddAndVerifyFencedFrameURL(&url_mapping, https_url);

    std::string navigate_urn_script = JsReplace("f.src = $1;", urn_uuid);

    net::Error expected_net_error_code =
        test_case.expect_allowed ? net::OK : net::ERR_BLOCKED_BY_CSP;
    NavigateFrameInsideFencedFrameTreeAndWaitForFinishedLoad(
        fenced_frame_root_node, urn_uuid, navigate_urn_script,
        expected_net_error_code);

    if (!test_case.expect_allowed)
      EXPECT_EQ("fenced-frame-src;", EvalJs(root, "violation"));

    absl::optional<blink::mojom::FencedFrameMode> fenced_frame_mode =
        fenced_frame_root_node->GetFencedFrameMode();
    EXPECT_TRUE(fenced_frame_mode.has_value());
    EXPECT_EQ(fenced_frame_mode.value(),
              blink::mojom::FencedFrameMode::kOpaqueAds);
  }
}

IN_PROC_BROWSER_TEST_F(FencedFrameParameterizedBrowserTest,
                       FenceUserActivation) {
  // This test exercises browser-side user activation in the following layout:
  // A: Top-level page    (origin 1)
  //   B: fencedframe     (origin 1)
  //     C1: iframe       (origin 1)
  //       D: fencedframe (origin 1)
  //         E1: iframe   (origin 1)
  //         E2: iframe   (origin 2)
  //     C2: iframe       (origin 2)
  //   F: fencedframe     (origin 1)
  //     G: iframe        (origin 1)
  //
  // See the design document for more details on intended semantics:
  // https://docs.google.com/document/d/1WnIhXOFycoje_sEoZR3Mo0YNSR2Ki7LABIC_HEWFaog/

  // Chrome disallows navigation to a URL in a frame that has more than one
  // ancestor with that URL, so I have to circumvent it with query params.
  const GURL kOrigin1Url =
      https_server()->GetURL("a.test", "/fenced_frames/empty.html");
  const GURL kOrigin1Url2 =
      https_server()->GetURL("a.test", "/fenced_frames/empty.html?");
  const GURL kOrigin1Url3 =
      https_server()->GetURL("a.test", "/fenced_frames/empty.html??");
  const GURL kOrigin2Url =
      https_server()->GetURL("b.test", "/fenced_frames/empty.html");

  // Navigate the top-level page.
  EXPECT_TRUE(NavigateToURL(shell(), kOrigin1Url));
  // It is safe to obtain the root frame tree node here, as it doesn't change.
  auto* nodeA = static_cast<WebContentsImpl*>(shell()->web_contents())
                    ->GetPrimaryFrameTree()
                    .root();
  ASSERT_NE(nullptr, nodeA);

  // Construct the children described above.
  auto* nodeB = AddNestedFencedFrame(nodeA, 0);
  ASSERT_NE(nullptr, nodeB);
  NavigateNestedFencedFrame(nodeB, kOrigin1Url);

  auto* nodeC1 = AddIframeInFencedFrame(nodeB, 0);
  ASSERT_NE(nullptr, nodeC1);
  NavigateIframeInFencedFrame(nodeC1, kOrigin1Url2);

  auto* nodeD = AddNestedFencedFrame(nodeC1, 0);
  ASSERT_NE(nullptr, nodeD);
  NavigateNestedFencedFrame(nodeD, kOrigin1Url2);

  auto* nodeE1 = AddIframeInFencedFrame(nodeD, 0);
  ASSERT_NE(nullptr, nodeE1);
  NavigateIframeInFencedFrame(nodeE1, kOrigin1Url3);

  auto* nodeE2 = AddIframeInFencedFrame(nodeD, 1);
  ASSERT_NE(nullptr, nodeE2);
  NavigateIframeInFencedFrame(nodeE2, kOrigin2Url);

  auto* nodeC2 = AddIframeInFencedFrame(nodeB, 1);
  ASSERT_NE(nullptr, nodeC2);
  NavigateIframeInFencedFrame(nodeC2, kOrigin2Url);

  auto* nodeF = AddNestedFencedFrame(nodeA, 1);
  ASSERT_NE(nullptr, nodeF);
  NavigateNestedFencedFrame(nodeF, kOrigin1Url);

  auto* nodeG = AddIframeInFencedFrame(nodeF, 0);
  ASSERT_NE(nullptr, nodeG);
  NavigateIframeInFencedFrame(nodeG, kOrigin1Url2);

  // Now that the layout is set up, perform the actual user activation tests.
  std::vector<FrameTreeNode*> nodes = {nodeA,  nodeB,  nodeC1, nodeD, nodeE1,
                                       nodeE2, nodeC2, nodeF,  nodeG};

  // Create some helper functions so we can express the user activation
  // notification test cases more concisely.
  auto ClearAll = [&nodes]() {
    // User activation can only be cleared per frame tree in MPArch, so we'll
    // do it from every node just to be safe.
    for (auto* node : nodes) {
      node->current_frame_host()->UpdateUserActivationState(
          blink::mojom::UserActivationUpdateType::kClearActivation,
          blink::mojom::UserActivationNotificationType::kNone);
    }
    for (auto* node : nodes) {
      EXPECT_FALSE(node->HasStickyUserActivation());
      EXPECT_FALSE(node->HasTransientUserActivation());
    }
  };

  auto Activate = [](FrameTreeNode* node) {
    node->UpdateUserActivationState(
        blink::mojom::UserActivationUpdateType::kNotifyActivation,
        blink::mojom::UserActivationNotificationType::kTest);
  };

  auto EXPECT_STICKY = [&nodes](std::vector<bool> should_be_activated) {
    ASSERT_EQ(nodes.size(), should_be_activated.size());
    for (size_t i = 0; i < nodes.size(); ++i) {
      if (should_be_activated[i]) {
        EXPECT_TRUE(nodes[i]->HasStickyUserActivation());
        EXPECT_TRUE(nodes[i]->HasTransientUserActivation());
      } else {
        EXPECT_FALSE(nodes[i]->HasStickyUserActivation());
        EXPECT_FALSE(nodes[i]->HasTransientUserActivation());
      }
    }
  };

  // Activate A, and check that no other frames are activated.
  ClearAll();  // Clear all user activations before we start.
  Activate(nodeA);
  EXPECT_STICKY({true /*A*/, false /*B*/, false /*C1*/, false /*D*/,
                 false /*E1*/, false /*E2*/, false /*C2*/, false /*F*/,
                 false /*G*/});

  // Activate B, and check that only B and C1 are activated.
  ClearAll();
  Activate(nodeB);
  EXPECT_STICKY({false /*A*/, true /*B*/, true /*C1*/, false /*D*/,
                 false /*E1*/, false /*E2*/, false /*C2*/, false /*F*/,
                 false /*G*/});

  // Activate C1, and check that only B and C1 are activated.
  ClearAll();
  Activate(nodeC1);
  EXPECT_STICKY({false /*A*/, true /*B*/, true /*C1*/, false /*D*/,
                 false /*E1*/, false /*E2*/, false /*C2*/, false /*F*/,
                 false /*G*/});

  // Activate C2, and check that only B and C2 are activated.
  ClearAll();
  Activate(nodeC2);
  EXPECT_STICKY({false /*A*/, true /*B*/, false /*C1*/, false /*D*/,
                 false /*E1*/, false /*E2*/, true /*C2*/, false /*F*/,
                 false /*G*/});

  // Activate D, and check that only D and E1 are activated.
  ClearAll();
  Activate(nodeD);
  EXPECT_STICKY({false /*A*/, false /*B*/, false /*C1*/, true /*D*/,
                 true /*E1*/, false /*E2*/, false /*C2*/, false /*F*/,
                 false /*G*/});

  // Activate E1, and check that only D and E1 are activated.
  ClearAll();
  Activate(nodeE1);
  EXPECT_STICKY({false /*A*/, false /*B*/, false /*C1*/, true /*D*/,
                 true /*E1*/, false /*E2*/, false /*C2*/, false /*F*/,
                 false /*G*/});

  // Activate E2, and check that only D and E2 are activated.
  ClearAll();
  Activate(nodeE2);
  EXPECT_STICKY({false /*A*/, false /*B*/, false /*C1*/, true /*D*/,
                 false /*E1*/, true /*E2*/, false /*C2*/, false /*F*/,
                 false /*G*/});

  // Activating F and G is equivalent to activating B and C1, so we omit them.

  // Create some helper functions so we can express the user activation
  // consumption test cases more concisely.
  auto ActivateAll = [&nodes]() {
    // Activate every individual frame just to be safe.
    for (auto* node : nodes) {
      node->current_frame_host()->UpdateUserActivationState(
          blink::mojom::UserActivationUpdateType::kNotifyActivation,
          blink::mojom::UserActivationNotificationType::kTest);
    }
    for (auto* node : nodes) {
      EXPECT_TRUE(node->HasStickyUserActivation());
      EXPECT_TRUE(node->HasTransientUserActivation());
    }
  };

  auto Consume = [](FrameTreeNode* node) {
    node->UpdateUserActivationState(
        blink::mojom::UserActivationUpdateType::kConsumeTransientActivation,
        blink::mojom::UserActivationNotificationType::kTest);
  };

  auto EXPECT_TRANSIENT = [&nodes](std::vector<bool> should_be_activated) {
    ASSERT_EQ(nodes.size(), should_be_activated.size());
    for (size_t i = 0; i < nodes.size(); ++i) {
      EXPECT_TRUE(nodes[i]->HasStickyUserActivation());
      if (should_be_activated[i]) {
        EXPECT_TRUE(nodes[i]->HasTransientUserActivation());
      } else {
        EXPECT_FALSE(nodes[i]->HasTransientUserActivation());
      }
    }
  };

  // These tests are the opposites of the ones above.
  // Consume A, and check that no other frames are consumed.
  ActivateAll();  // Activate all frames before we start.
  Consume(nodeA);
  EXPECT_TRANSIENT({false /*A*/, true /*B*/, true /*C1*/, true /*D*/,
                    true /*E1*/, true /*E2*/, true /*C2*/, true /*F*/,
                    true /*G*/});

  // Consume B, and check that only B, C1, and C2 are consumed.
  ActivateAll();
  Consume(nodeB);
  EXPECT_TRANSIENT({true /*A*/, false /*B*/, false /*C1*/, true /*D*/,
                    true /*E1*/, true /*E2*/, false /*C2*/, true /*F*/,
                    true /*G*/});

  // Consume C2, and check that only B, C1, and C2 are consumed.
  ActivateAll();
  Consume(nodeC2);
  EXPECT_TRANSIENT({true /*A*/, false /*B*/, false /*C1*/, true /*D*/,
                    true /*E1*/, true /*E2*/, false /*C2*/, true /*F*/,
                    true /*G*/});

  // Consume D, and check that only D, E1, and E2 are consumed.
  ActivateAll();
  Consume(nodeD);
  EXPECT_TRANSIENT({true /*A*/, true /*B*/, true /*C1*/, false /*D*/,
                    false /*E1*/, false /*E2*/, true /*C2*/, true /*F*/,
                    true /*G*/});

  // Consume E1, and check that only D, E1, and E2 are consumed.
  ActivateAll();
  Consume(nodeE1);
  EXPECT_TRANSIENT({true /*A*/, true /*B*/, true /*C1*/, false /*D*/,
                    false /*E1*/, false /*E2*/, true /*C2*/, true /*F*/,
                    true /*G*/});
}

// TODO(https://crbug.com/1335512): Flaky.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_FencedAdSizes DISABLED_FencedAdSizes
#else
#define MAYBE_FencedAdSizes FencedAdSizes
#endif
IN_PROC_BROWSER_TEST_F(FencedFrameParameterizedBrowserTest,
                       MAYBE_FencedAdSizes) {
  // This test exercises restrictions on fenced frame sizes in opaque-ads mode.
  // See the design document for more details on intended semantics:
  // https://docs.google.com/document/d/1MVqxc2nzde3cJYIRC8vnXH-a4A6J4GQE-1vBuXhQsPE/edit#

  enum class TestType {
    kFixed,
    kScaleWidthConstantHeightExact,
    kScaleWidthConstantHeightApproximate,
    kScaleWidthConstantAspectRatioExact,
    kScaleWidthConstantAspectRatioApproximate,
  };

  // Test that an opaque-ads mode fenced frame created with size
  // `input_width` by `input_height` gets snapped to size
  // `output_width` by `output_height` on desktop.
  auto TestAdSize = [&](int input_width, int input_height, TestType test_type,
                        int output_width, int output_height) {
    // Navigate the top-level page.
    const GURL kUrl =
        https_server()->GetURL("a.test", "/fenced_frames/empty.html");
    EXPECT_TRUE(NavigateToURL(shell(), kUrl));
    // It is safe to obtain the root frame tree node here, as it doesn't change.
    auto* nodeA = static_cast<WebContentsImpl*>(shell()->web_contents())
                      ->GetPrimaryFrameTree()
                      .root();
    ASSERT_NE(nullptr, nodeA);

    if (test_type != TestType::kFixed) {
#if !BUILDFLAG(IS_ANDROID)
      // Ignore mobile-only tests on platforms other than Android.
      return;
#else
      // Set up tests that scale with screen width.
      int screen_width = EvalJs(nodeA, "screen.width").ExtractInt();

      // Scale the height to match the aspect ratio, if relevant.
      if (test_type == TestType::kScaleWidthConstantAspectRatioExact ||
          test_type == TestType::kScaleWidthConstantAspectRatioApproximate) {
        output_height = (input_height * screen_width) / input_width;
        input_height = output_height;
      }

      // Make the width match the screen width.
      input_width = screen_width;
      output_width = screen_width;

      // If we want to test coercion to sizes that scale with constant height,
      // make the requested width a little wrong.
      if (test_type == TestType::kScaleWidthConstantHeightApproximate ||
          test_type == TestType::kScaleWidthConstantAspectRatioApproximate) {
        input_width++;
      }
#endif
    }

    // Create an opaque-ads fenced frame nodeB with size
    // `input_width` by `input_height`.
    EXPECT_TRUE(ExecJs(
        nodeA,
        JsReplace(
            "var nested_fenced_frame = document.createElement('fencedframe');"
            "nested_fenced_frame.mode = 'opaque-ads';"
            "nested_fenced_frame.width = $1;"
            "nested_fenced_frame.height = $2;"
            "document.body.appendChild(nested_fenced_frame);",
            input_width, input_height)));
    EXPECT_EQ(1UL, nodeA->child_count());
    auto* nodeB = GetFencedFrameRootNode(nodeA->child_at(0));
    EXPECT_TRUE(nodeB->IsFencedFrameRoot());
    EXPECT_TRUE(nodeB->IsInFencedFrameTree());
    ASSERT_NE(nullptr, nodeB);

    // Check the size of the frame before navigating.
    auto frame_width =
        EvalJs(nodeA, "getComputedStyle(nested_fenced_frame).width")
            .ExtractString();
    auto frame_height =
        EvalJs(nodeA, "getComputedStyle(nested_fenced_frame).height")
            .ExtractString();

    // Wait for 2 rAFs to make things deterministic.
    ASSERT_TRUE(EvalJsAfterLifecycleUpdate(nodeA, "", "").error.empty());
    ASSERT_TRUE(EvalJsAfterLifecycleUpdate(nodeA, "", "").error.empty());

    // Navigate the fenced frame, which should force its inner size to the
    // nearest allowed one.
    NavigateNestedFencedFrame(nodeB, kUrl);

    // Check that the outer container size hasn't changed.
    EXPECT_EQ(EvalJs(nodeA, "getComputedStyle(nested_fenced_frame).width")
                  .ExtractString(),
              frame_width);
    EXPECT_EQ(EvalJs(nodeA, "getComputedStyle(nested_fenced_frame).height")
                  .ExtractString(),
              frame_height);

    // Wait for 2 rAFs to make things deterministic.
    ASSERT_TRUE(EvalJsAfterLifecycleUpdate(nodeA, "", "").error.empty());
    ASSERT_TRUE(EvalJsAfterLifecycleUpdate(nodeA, "", "").error.empty());

    // Check that the inner size is what we expect.
    int inner_width = EvalJs(nodeB, "innerWidth").ExtractInt();
    int inner_height = EvalJs(nodeB, "innerHeight").ExtractInt();
    EXPECT_EQ(inner_width, output_width);
    EXPECT_EQ(inner_height, output_height);

    // Attempt to change the size of the fenced frame from the embedder.
    const int new_width = 970;
    const int new_height = 90;
    EXPECT_TRUE(ExecJs(nodeA, JsReplace("nested_fenced_frame.width = $1;"
                                        "nested_fenced_frame.height = $2;",
                                        new_width, new_height)));
    NavigateNestedFencedFrame(nodeB, kUrl);

    // Force a style recomputation.
    ASSERT_TRUE(EvalJs(nodeA, "getComputedStyle(nested_fenced_frame).width")
                    .error.empty());

    // Wait for 2 rAFs to make things deterministic.
    ASSERT_TRUE(EvalJsAfterLifecycleUpdate(nodeA, "", "").error.empty());
    ASSERT_TRUE(EvalJsAfterLifecycleUpdate(nodeA, "", "").error.empty());

    // Check that the inner size hasn't changed.
    inner_width = EvalJs(nodeB, "innerWidth").ExtractInt();
    inner_height = EvalJs(nodeB, "innerHeight").ExtractInt();
    EXPECT_EQ(inner_width, output_width);
    EXPECT_EQ(inner_height, output_height);
  };

  // Run all the individual test cases we want.
  // {input_width, input_height, test_type, output_width, output_height}
  std::vector<std::tuple<int, int, TestType, int, int>> test_cases = {

      // Exact match between requested size and fixed allowed size.
      {320, 50, TestType::kFixed, 320, 50},
      {728, 90, TestType::kFixed, 728, 90},
      {970, 90, TestType::kFixed, 970, 90},
      {320, 100, TestType::kFixed, 320, 100},
      {160, 600, TestType::kFixed, 160, 600},
      {300, 250, TestType::kFixed, 300, 250},
      {970, 250, TestType::kFixed, 970, 250},
      {336, 280, TestType::kFixed, 336, 280},
      {320, 480, TestType::kFixed, 320, 480},
      {300, 600, TestType::kFixed, 300, 600},
      {300, 1050, TestType::kFixed, 300, 1050},

      // Approximate match between requested size and fixed allowed size.
      {320, 49, TestType::kFixed, 320, 50},
      {319, 50, TestType::kFixed, 320, 50},

      // Edge cases for requested size.
      {0, 0, TestType::kFixed, 320, 50},
      {0, 100, TestType::kFixed, 320, 50},
      {100, 0, TestType::kFixed, 320, 50},

      // Exact match between requested size and allowed size that scales with
      // constant height.
      {0, 50, TestType::kScaleWidthConstantHeightExact, 0, 50},
      {0, 100, TestType::kScaleWidthConstantHeightExact, 0, 100},
      {0, 250, TestType::kScaleWidthConstantHeightExact, 0, 250},

      // Approximate match between requested size and allowed size that scales
      // with constant height.
      {0, 50, TestType::kScaleWidthConstantHeightApproximate, 0, 50},
      {0, 100, TestType::kScaleWidthConstantHeightApproximate, 0, 100},
      {0, 250, TestType::kScaleWidthConstantHeightApproximate, 0, 250},

      // Constant height scaling is only supported on sizes where it is
      // declared (e.g. not for height 99).
      {0, 99, TestType::kScaleWidthConstantHeightExact, 0, 100},

      // Exact match between requested size and allowed size that scales with
      // constant aspect ratio.
      {32, 5, TestType::kScaleWidthConstantAspectRatioExact, 0, 0},
      {16, 5, TestType::kScaleWidthConstantAspectRatioExact, 0, 0},
      {6, 5, TestType::kScaleWidthConstantAspectRatioExact, 0, 0},
      {2, 3, TestType::kScaleWidthConstantAspectRatioExact, 0, 0},
      {1, 2, TestType::kScaleWidthConstantAspectRatioExact, 0, 0},

      // Approximate match between requested size and allowed size that scales
      // with constant aspect ratio.
      {32, 5, TestType::kScaleWidthConstantAspectRatioApproximate, 0, 0},
      {16, 5, TestType::kScaleWidthConstantAspectRatioApproximate, 0, 0},
      {6, 5, TestType::kScaleWidthConstantAspectRatioApproximate, 0, 0},
      {2, 3, TestType::kScaleWidthConstantAspectRatioApproximate, 0, 0},
      {1, 2, TestType::kScaleWidthConstantAspectRatioApproximate, 0, 0},
  };

  for (auto& test_case : test_cases) {
    TestAdSize(std::get<0>(test_case), std::get<1>(test_case),
               std::get<2>(test_case), std::get<3>(test_case),
               std::get<4>(test_case));
  }
}

class FencedFrameReportEventBrowserTest
    : public FencedFrameParameterizedBrowserTest {
 public:
  // TODO(crbug.com/1123606): Disable window.fence.reportEvent in iframes.
  // Remove this constructor and `scoped_feature_list_` once FLEDGE stops
  // supporting iframes.
  FencedFrameReportEventBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kAllowURNsInIframes, {}}},
        {/* disabled_features */});
  }

  // An object representing a single step of a reportEvent test.
  // First, we navigate the fenced frame to a new URL.
  // Second, we call reportEvent and validate the results.
  struct Step {
    // Whether the navigation should target a nested iframe rather than the
    // fenced frame root.
    bool is_target_nested_iframe = false;
    // Whether the navigation should be embedder-initiated or fenced-frame
    // initiated.
    bool is_embedder_initiated = false;
    // Whether the navigation should be via a urn:uuid or a normal URL.
    // (This should always be false when `!is_embedder_initiated`.
    bool is_opaque = false;

    struct Event {
      std::string type;
      std::string reporting_destination;
    };
    struct Destination {
      // The origin for the navigation.
      std::string origin;
      // The path for the resource to load.
      std::string path;
    };

    // Specifies the reporting destination and event type for reportEvent.
    Event event{"click", "buyer"};

    // The initial navigation destination (may be redirected).
    Destination destination;
    // A list of redirects that the navigation should take. The last redirect
    // destination will be the ultimate destination of the navigation.
    std::vector<Destination> redirects;

    // Specify the outcome of reportEvent.
    enum class Result {
      kSuccess,
      kModeNotOpaque,
      kCrossOrigin,
      kNoMeta,
      kNoDestination,
      kNoReportingURL,
      kInvalidReportingURL
    };

    // Outcome of reportEvent.
    Result report_event_result = Result::kSuccess;
  };

  std::string GetConsoleWarningPattern(Step::Result result) {
    switch (result) {
      case Step::Result::kModeNotOpaque:
        return "fence.reportEvent is only available in the 'opaque-ads' mode.";
      case Step::Result::kCrossOrigin:
        return "fence.reportEvent is only available in same-origin subframes.";
      case Step::Result::kNoMeta:
        return "This frame did not register reporting metadata.";
      case Step::Result::kNoDestination:
        return "This frame did not register reporting metadata for "
               "destination *";
      case Step::Result::kNoReportingURL:
        return "This frame did not register reporting url for destination * "
               "and event_type *";
      case Step::Result::kInvalidReportingURL:
        return "This frame registered invalid reporting url for destination * "
               "and event_type *";
      default:
        return "";
    }
  }

  std::unique_ptr<net::test_server::BasicHttpResponse>
  GetResponseWithAccessAllowHeaders(
      const net::test_server::HttpRequest* request) {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();

    response->set_code(net::HTTP_OK);
    response->AddCustomHeader(cors::kAccessControlAllowMethods,
                              request->method_string);
    if (base::Contains(request->headers, "Origin")) {
      response->AddCustomHeader(cors::kAccessControlAllowOrigin, "*");
    }
    if (base::Contains(request->headers, cors::kAccessControlRequestHeaders)) {
      response->AddCustomHeader(
          cors::kAccessControlAllowHeaders,
          request->headers.at(cors::kAccessControlRequestHeaders));
    }

    return response;
  }

  // For cross-origin reporting destination, a preflight request is sent before
  // the actual request.
  void VerifyPreflightRequest(const net::test_server::HttpRequest* request) {
    EXPECT_EQ(request->method, net::test_server::HttpMethod::METHOD_OPTIONS);
    EXPECT_TRUE(base::Contains(request->headers, "Origin"));
    EXPECT_TRUE(
        base::Contains(request->headers.at(cors::kAccessControlRequestHeaders),
                       "attribution-reporting-eligible"));
  }

  scoped_refptr<FencedFrameReporter> CreateFencedFrameReporter() {
    return FencedFrameReporter::CreateForFledge(
        web_contents()
            ->GetPrimaryMainFrame()
            ->GetStoragePartition()
            ->GetURLLoaderFactoryForBrowserProcess());
  }

  // A helper function for specifying reportEvent tests. Each step consists of a
  // series of `Step`s specified above.
  void RunTest(std::vector<Step>& steps) {
    // In order to check events reported over the network, we register an HTTP
    // response interceptor for each successful reportEvent request we expect.
    // We register an additional one so that we can check for spurious requests
    // at the end of the test.
    EXPECT_TRUE(steps.size() > 0);
    std::vector<std::unique_ptr<net::test_server::ControllableHttpResponse>>
        responses;
    std::vector<std::unique_ptr<net::test_server::ControllableHttpResponse>>
        redirects;

    std::string reporting_origin = "c.test";
    // We also register interceptors for redirections that we want to perform.
    // Each redirect must be from a unique path so that messages aren't
    // unintentionally intercepted and blocked.
    {
      std::set<std::string> paths;
      for (auto& step : steps) {
        responses.emplace_back(
            std::make_unique<net::test_server::ControllableHttpResponse>(
                https_server(), kReportingURL));
        std::string final_destination_origin =
            step.redirects.empty() ? step.destination.origin
                                   : step.redirects.back().origin;
        if (final_destination_origin != reporting_origin) {
          // The reporting beacon is cross-origin. Two requests will be sent.
          // First is the preflight request, the second is the actual request.
          responses.emplace_back(
              std::make_unique<net::test_server::ControllableHttpResponse>(
                  https_server(), kReportingURL));
        }
        if (step.is_target_nested_iframe) {
          ASSERT_FALSE(step.is_embedder_initiated);
          ASSERT_FALSE(step.is_opaque);
        }
        ASSERT_FALSE(step.destination.origin.empty());
        ASSERT_FALSE(step.destination.path.empty());
        int redirect_index = 0;
        for (auto& redirect_destination : step.redirects) {
          ASSERT_TRUE(paths.find(redirect_destination.path) == paths.end());
          ASSERT_FALSE(redirect_destination.origin.empty());
          ASSERT_FALSE(redirect_destination.path.empty());
          paths.insert(redirect_destination.path);

          // Intercept the previous navigation destination in the chain.
          std::string previous_path =
              redirect_index ? step.redirects[redirect_index - 1].path
                             : step.destination.path;
          redirects.emplace_back(
              std::make_unique<net::test_server::ControllableHttpResponse>(
                  https_server(), previous_path));
          redirect_index++;
        }
      }
    }
    // An additional response is used to check any spurious waiting reported
    // events.
    responses.emplace_back(
        std::make_unique<net::test_server::ControllableHttpResponse>(
            https_server(), kReportingURL));
    ASSERT_TRUE(https_server()->Start());

    // Set up the embedder and a fenced frame.
    GURL main_url = https_server()->GetURL("a.test", "/hello.html");
    EXPECT_TRUE(NavigateToURL(shell(), main_url));
    FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                              ->GetPrimaryFrameTree()
                              .root();
    EXPECT_TRUE(ExecJs(root,
                       "var f = document.createElement('fencedframe');"
                       "f.mode = 'opaque-ads';"
                       "document.body.appendChild(f);"));
    EXPECT_EQ(1U, root->child_count());
    FrameTreeNode* fenced_frame_root_node =
        GetFencedFrameRootNode(root->child_at(0));
    EXPECT_TRUE(fenced_frame_root_node->IsFencedFrameRoot());
    EXPECT_TRUE(fenced_frame_root_node->IsInFencedFrameTree());

    // Create a FencedFrameReporter and pass it reporting metadata.
    scoped_refptr<FencedFrameReporter> fenced_frame_reporter =
        CreateFencedFrameReporter();
    GURL reporting_url(
        https_server()->GetURL("c.test", "/_report_event_server.html"));
    // Set valid reporting metadata for buyer.
    fenced_frame_reporter->OnUrlMappingReady(
        blink::FencedFrame::ReportingDestination::kBuyer,
        {{"click", reporting_url}});
    // Set empty reporting url for seller.
    fenced_frame_reporter->OnUrlMappingReady(
        blink::FencedFrame::ReportingDestination::kSeller, {{"click", GURL()}});
    // Set no reporting urls for component seller.
    fenced_frame_reporter->OnUrlMappingReady(
        blink::FencedFrame::ReportingDestination::kComponentSeller, {});

    // Get the urn mapping object.
    FencedFrameURLMapping& url_mapping =
        root->current_frame_host()->GetPage().fenced_frame_urls_map();

    // Create a holder for a nested iframe.
    absl::optional<FrameTreeNode*> nested_iframe_node = absl::nullopt;

    int navigation_index = 0;
    int response_index = 0;
    int redirect_index = 0;
    for (auto& step : steps) {
      // Configure the navigation.
      GURL navigate_url = https_server()->GetURL(step.destination.origin,
                                                 step.destination.path);
      GURL expect_url = navigate_url;
      if (step.is_opaque) {
        auto urn_uuid = AddAndVerifyFencedFrameURL(&url_mapping, navigate_url,
                                                   fenced_frame_reporter);
        navigate_url = urn_uuid;
      }
      FrameTreeNode* navigation_target_node = fenced_frame_root_node;

      // Add a nested iframe inside the fenced frame if necessary (or clear the
      // handle to it, if the navigation will remove it).
      if (step.is_target_nested_iframe) {
        if (!nested_iframe_node) {
          EXPECT_TRUE(
              ExecJs(fenced_frame_root_node,
                     "var iframe_within_ff = document.createElement('iframe');"
                     "document.body.appendChild(iframe_within_ff);"));
          EXPECT_EQ(1U, fenced_frame_root_node->child_count());
          nested_iframe_node = fenced_frame_root_node->child_at(0);
        }
        navigation_target_node = *nested_iframe_node;
      } else {
        nested_iframe_node = absl::nullopt;
      }

      // Initiate the navigation.
      TestFrameNavigationObserver observer(navigation_target_node);
      if (step.is_target_nested_iframe) {
        EXPECT_TRUE(
            ExecJs(fenced_frame_root_node,
                   JsReplace("iframe_within_ff.src = $1", navigate_url)));
      } else if (step.is_embedder_initiated) {
        EXPECT_TRUE(ExecJs(root, JsReplace("f.src = $1", navigate_url)));
      } else {
        EXPECT_TRUE(ExecJs(fenced_frame_root_node,
                           JsReplace("location.href = $1", navigate_url)));
      }

      // Redirect the navigation if relevant.
      for (auto& redirect_destination : step.redirects) {
        GURL redirect_url = https_server()->GetURL(redirect_destination.origin,
                                                   redirect_destination.path);
        expect_url = redirect_url;
        auto& redirect = *redirects[redirect_index];
        redirect.WaitForRequest();
        std::string redirect_response =
            std::string("HTTP/1.1 302 Moved Temporarily\r\nLocation: ") +
            redirect_url.spec() + std::string("\r\n\r\n");
        redirect.Send(redirect_response);
        redirect.Done();
        redirect_index++;
      }

      // Check that the navigation worked as intended.
      observer.WaitForCommit();
      EXPECT_EQ(
          expect_url,
          navigation_target_node->current_frame_host()->GetLastCommittedURL());
      EXPECT_EQ(url::Origin::Create(expect_url),
                navigation_target_node->current_frame_host()
                    ->GetLastCommittedOrigin());
      navigation_index++;

      // Monitor the console warnings.
      WebContentsConsoleObserver console_observer(web_contents());
      auto filter =
          [](const content::WebContentsConsoleObserver::Message& message) {
            return message.log_level ==
                   blink::mojom::ConsoleMessageLevel::kError;
          };
      console_observer.SetFilter(base::BindRepeating(filter));
      if (step.report_event_result != Step::Result::kSuccess) {
        console_observer.SetPattern(
            GetConsoleWarningPattern(step.report_event_result));
      }

      // Perform the reportEvent call, with a unique body.
      const char report_event_script[] = R"(
        window.fence.reportEvent({
          eventType: $2,
          eventData: $2 + ' $1',
          destination: [$3],
        });
      )";
      EXPECT_TRUE(
          ExecJs(navigation_target_node,
                 JsReplace(report_event_script, navigation_index,
                           step.event.type, step.event.reporting_destination)));

      // If relevant, check that the event report succeeded.
      if (step.report_event_result == Step::Result::kSuccess) {
        std::string final_destination_origin =
            step.redirects.empty() ? step.destination.origin
                                   : step.redirects.back().origin;
        if (final_destination_origin != reporting_origin) {
          auto& preflight_response = *responses[response_index];
          // Verify the preflight request contains the eligibility header under
          // "Access-Control-Request-Headers".
          preflight_response.WaitForRequest();
          VerifyPreflightRequest(preflight_response.http_request());

          // Send response with extra headers.
          std::unique_ptr<net::test_server::BasicHttpResponse>
              access_allow_response = GetResponseWithAccessAllowHeaders(
                  preflight_response.http_request());
          access_allow_response->AddCustomHeader("Content-Type",
                                                 "text/plain;charset=UTF-8");
          preflight_response.Send(access_allow_response->ToResponseString());
          preflight_response.Done();
          ++response_index;
        }
        auto& response = *responses[response_index];
        response.WaitForRequest();
        // Verify the request has the correct content.
        EXPECT_EQ(
            response.http_request()->content,
            step.event.type + " " + base::NumberToString(navigation_index));
        // Verify the request contains the eligibility header.
        EXPECT_EQ(response.http_request()->headers.at(
                      "Attribution-Reporting-Eligible"),
                  "event-source");
        EXPECT_FALSE(base::Contains(response.http_request()->headers,
                                    "Attribution-Reporting-Support"));
        response.Done();
        ++response_index;
      } else {
        ASSERT_TRUE(console_observer.Wait());
        EXPECT_FALSE(console_observer.messages().empty());
        EXPECT_EQ(console_observer.messages().size(), 1u);
      }
    }

    // Check for any spurious waiting reported events.
    EXPECT_TRUE(ExecJs(root, JsReplace("f.src = $1", reporting_url)));
    auto& response = *responses[response_index];
    response.WaitForRequest();
    EXPECT_EQ(response.http_request()->content, "");
    response.Done();
  }

 private:
  // Server must start after ControllableHttpResponse object being constructed.
  void AssertServerStart() override {}

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Fenced frame not in opaque-ads mode should fail reportEvent().
IN_PROC_BROWSER_TEST_F(FencedFrameReportEventBrowserTest,
                       FencedFrameReportEventNonOpaqueAdsMode) {
  net::test_server::ControllableHttpResponse response(https_server(),
                                                      kReportingURL);
  ASSERT_TRUE(https_server()->Start());

  // Set up the embedder and a default mode fenced frame.
  GURL main_url = https_server()->GetURL("a.test", "/hello.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  EXPECT_TRUE(ExecJs(root,
                     "var f = document.createElement('fencedframe');"
                     "f.mode = 'default';"
                     "document.body.appendChild(f);"));

  EXPECT_EQ(1U, root->child_count());
  FrameTreeNode* fenced_frame_root_node =
      GetFencedFrameRootNode(root->child_at(0));
  EXPECT_TRUE(fenced_frame_root_node->IsFencedFrameRoot());
  EXPECT_TRUE(fenced_frame_root_node->IsInFencedFrameTree());

  WebContentsConsoleObserver console_observer(web_contents());
  auto filter =
      [](const content::WebContentsConsoleObserver::Message& message) {
        return message.log_level == blink::mojom::ConsoleMessageLevel::kError;
      };
  console_observer.SetFilter(base::BindRepeating(filter));
  console_observer.SetPattern(
      GetConsoleWarningPattern(Step::Result::kModeNotOpaque));

  // Perform the reportEvent call, with a unique body.
  const char report_event_script[] = R"(
        window.fence.reportEvent({
          eventType: 'click',
          eventData: 'click 0',
          destination: ['buyer'],
        });
      )";
  EXPECT_TRUE(ExecJs(fenced_frame_root_node, report_event_script));

  // Check console warning.
  ASSERT_TRUE(console_observer.Wait());
  EXPECT_FALSE(console_observer.messages().empty());
  EXPECT_EQ(console_observer.messages().size(), 1u);

  // Check that the reporting beacon is not sent.
  EXPECT_FALSE(response.has_received_request());
}

// The simplest test case: URN navigation into reportEvent.
IN_PROC_BROWSER_TEST_F(FencedFrameReportEventBrowserTest,
                       FencedFrameReportEventEmbedderURNNavigation) {
  std::vector<Step> config = {
      {
          .is_embedder_initiated = true,
          .is_opaque = true,
          .destination = {"a.test", "/fenced_frames/title1.html"},
          .report_event_result = Step::Result::kSuccess,
      },
  };
  RunTest(config);
}

// reportEvent shouldn't work if there is no associated reporting metadata with
// the reporting destination.
IN_PROC_BROWSER_TEST_F(
    FencedFrameReportEventBrowserTest,
    FencedFrameReportEventNoMetadataForReportingDestination) {
  std::vector<Step> config = {
      {
          .is_embedder_initiated = true,
          .is_opaque = true,
          .event = {"click", "component-seller"},
          .destination = {"a.test", "/fenced_frames/title1.html"},
          .report_event_result = Step::Result::kNoDestination,
      },
  };
  RunTest(config);
}

// reportEvent shouldn't work if there is no associated reporting url with
// the event type and the reporting destination.
IN_PROC_BROWSER_TEST_F(FencedFrameReportEventBrowserTest,
                       FencedFrameReportEventNoReportingURLForEventType) {
  std::vector<Step> config = {
      {
          .is_embedder_initiated = true,
          .is_opaque = true,
          .event = {"invalid-event", "buyer"},
          .destination = {"a.test", "/fenced_frames/title1.html"},
          .report_event_result = Step::Result::kNoReportingURL,
      },
  };
  RunTest(config);
}

// reportEvent shouldn't work if the reporting url is invalid.
IN_PROC_BROWSER_TEST_F(FencedFrameReportEventBrowserTest,
                       FencedFrameReportEventInvalidReportingURL) {
  std::vector<Step> config = {
      {
          .is_embedder_initiated = true,
          .is_opaque = true,
          .event = {"click", "seller"},
          .destination = {"a.test", "/fenced_frames/title1.html"},
          .report_event_result = Step::Result::kInvalidReportingURL,
      },
  };
  RunTest(config);
}

// reportEvent should work in same-origin subframes.
IN_PROC_BROWSER_TEST_F(FencedFrameReportEventBrowserTest,
                       FencedFrameReportEventNestedIframeSameOriginNavigation) {
  std::vector<Step> config = {
      {
          .is_embedder_initiated = true,
          .is_opaque = true,
          .destination = {"a.test", "/fenced_frames/title1.html"},
          .report_event_result = Step::Result::kSuccess,
      },
      {
          .is_target_nested_iframe = true,
          .destination = {"a.test", "/fenced_frames/title1.html"},
          .report_event_result = Step::Result::kSuccess,
      },
  };
  RunTest(config);
}

// reportEvent shouldn't work in cross-origin subframes.
IN_PROC_BROWSER_TEST_F(
    FencedFrameReportEventBrowserTest,
    FencedFrameReportEventNestedIframeCrossOriginNavigation) {
  std::vector<Step> config = {
      {
          .is_embedder_initiated = true,
          .is_opaque = true,
          .destination = {"a.test", "/fenced_frames/title1.html"},
          .report_event_result = Step::Result::kSuccess,
      },
      {
          .is_target_nested_iframe = true,
          .destination = {"b.test", "/fenced_frames/title1.html"},
          .report_event_result = Step::Result::kCrossOrigin,
      },
  };
  RunTest(config);
}

// Reporting metadata should persist across FF-initiated same-origin
// navigations.
IN_PROC_BROWSER_TEST_F(FencedFrameReportEventBrowserTest,
                       FencedFrameReportEventFFSameOriginNavigation) {
  std::vector<Step> config = {
      {
          .is_embedder_initiated = true,
          .is_opaque = true,
          .destination = {"a.test", "/fenced_frames/title1.html"},
          .report_event_result = Step::Result::kSuccess,
      },
      {
          .destination = {"a.test", "/fenced_frames/title1.html?foo"},
          .report_event_result = Step::Result::kSuccess,
      },
  };
  RunTest(config);
}

// Reporting metadata should be dropped upon cross-origin navigations,
// but come back upon new URN navigations.
IN_PROC_BROWSER_TEST_F(FencedFrameReportEventBrowserTest,
                       FencedFrameReportEventFFCrossOriginNavigation) {
  std::vector<Step> config = {
      {
          .is_embedder_initiated = true,
          .is_opaque = true,
          .destination = {"a.test", "/fenced_frames/title1.html"},
          .report_event_result = Step::Result::kSuccess,
      },
      {
          .destination = {"b.test", "/fenced_frames/title1.html"},
          .report_event_result = Step::Result::kNoMeta,
      },
      {
          .is_embedder_initiated = true,
          .is_opaque = true,
          .destination = {"a.test", "/fenced_frames/title1.html"},
          .report_event_result = Step::Result::kSuccess,
      },
  };
  RunTest(config);
}

// Embedder-initiated URL navigations should always be considered cross-origin.
IN_PROC_BROWSER_TEST_F(FencedFrameReportEventBrowserTest,
                       FencedFrameReportEventEmbedderURLNavigation) {
  std::vector<Step> config = {
      {
          .is_embedder_initiated = true,
          .is_opaque = true,
          .destination = {"a.test", "/fenced_frames/title1.html"},
          .report_event_result = Step::Result::kSuccess,
      },
      {
          .is_embedder_initiated = true,
          .is_opaque = false,
          .destination = {"a.test", "/fenced_frames/title1.html"},
          .report_event_result = Step::Result::kNoMeta,
      },
  };
  RunTest(config);
}

// Same-origin redirects in the initial URN navigation shouldn't affect
// reporting metadata.
IN_PROC_BROWSER_TEST_F(FencedFrameReportEventBrowserTest,
                       FencedFrameReportEventEmbedderSameOriginRedirect) {
  std::vector<Step> config = {
      {
          .is_embedder_initiated = true,
          .is_opaque = true,
          .destination = {"a.test", "/fenced_frames/redirect1.html"},
          .redirects =
              {
                  {"a.test", "/fenced_frames/redirect2.html"},
                  {"a.test", "/fenced_frames/title1.html"},
              },
          .report_event_result = Step::Result::kSuccess,
      },
  };
  RunTest(config);
}

// Cross-origin redirects in the initial URN navigation shouldn't affect
// reporting metadata either.
IN_PROC_BROWSER_TEST_F(FencedFrameReportEventBrowserTest,
                       FencedFrameReportEventEmbedderCrossOriginRedirect) {
  std::vector<Step> config = {
      {
          .is_embedder_initiated = true,
          .is_opaque = true,
          .destination = {"a.test", "/fenced_frames/redirect1.html"},
          .redirects =
              {
                  {"b.test", "/fenced_frames/redirect2.html"},
                  {"c.test", "/fenced_frames/title1.html"},
              },
          .report_event_result = Step::Result::kSuccess,
      },
  };
  RunTest(config);
}

// Metadata should be preserved if all URLs in an FF-initiated redirect chain
// are same-origin.
IN_PROC_BROWSER_TEST_F(FencedFrameReportEventBrowserTest,
                       FencedFrameReportEventFFSameOriginRedirect) {
  std::vector<Step> config = {
      {
          .is_embedder_initiated = true,
          .is_opaque = true,
          .destination = {"a.test", "/fenced_frames/title1.html"},
          .report_event_result = Step::Result::kSuccess,
      },
      {
          .destination = {"a.test", "/fenced_frames/redirect1.html"},
          .redirects =
              {
                  {"a.test", "/fenced_frames/redirect2.html"},
                  {"a.test", "/fenced_frames/title1.html?foo"},
              },
          .report_event_result = Step::Result::kSuccess,
      },
  };
  RunTest(config);
}

// Metadata should be dropped if any URLs in an FF-initiated redirect chain
// are cross-origin.
IN_PROC_BROWSER_TEST_F(FencedFrameReportEventBrowserTest,
                       FencedFrameReportEventFFCrossOriginRedirect) {
  std::vector<Step> config = {
      {
          .is_embedder_initiated = true,
          .is_opaque = true,
          .destination = {"a.test", "/fenced_frames/title1.html"},
          .report_event_result = Step::Result::kSuccess,
      },
      {
          .destination = {"a.test", "/fenced_frames/redirect1.html"},
          .redirects =
              {
                  {"b.test", "/fenced_frames/redirect2.html"},
                  {"a.test", "/fenced_frames/title1.html"},
              },
          .report_event_result = Step::Result::kNoMeta,
      },
  };
  RunTest(config);
}

// (Temporary test for FLEDGE iframe OT.)
// Tests that an iframe with a urn:uuid commits the navigation with the
// associated reporting metadata and `fence.reportEvent` sends the beacon to
// the registered reporting url.
// TODO(crbug.com/1123606): Disable window.fence.reportEvent in iframes.
// Remove this test once the FLEDGE origin trial stops supporting iframes.
IN_PROC_BROWSER_TEST_F(FencedFrameReportEventBrowserTest,
                       IframeReportingMetadata) {
  net::test_server::ControllableHttpResponse preflight_response(https_server(),
                                                                kReportingURL);
  net::test_server::ControllableHttpResponse reporting_response(https_server(),
                                                                kReportingURL);
  ASSERT_TRUE(https_server()->Start());

  GURL main_url = https_server()->GetURL("b.test", "/hello.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  EXPECT_TRUE(ExecJs(root,
                     "var f = document.createElement('iframe');"
                     "document.body.appendChild(f);"));
  EXPECT_EQ(1U, root->child_count());
  FrameTreeNode* iframe_node = root->child_at(0);

  // Create a FencedFrameReporter and pass it reporting metadata.
  scoped_refptr<FencedFrameReporter> fenced_frame_reporter =
      CreateFencedFrameReporter();
  GURL reporting_url(https_server()->GetURL("c.test", kReportingURL));
  // Set valid reporting metadata for buyer.
  fenced_frame_reporter->OnUrlMappingReady(
      blink::FencedFrame::ReportingDestination::kBuyer,
      {{"mouse interaction", reporting_url},
       {"click", https_server()->GetURL("c.test", "/title1.html")}});
  // Set empty reporting url for seller and component seller.
  fenced_frame_reporter->OnUrlMappingReady(
      blink::FencedFrame::ReportingDestination::kSeller, {});
  fenced_frame_reporter->OnUrlMappingReady(
      blink::FencedFrame::ReportingDestination::kComponentSeller, {});

  GURL https_url(
      https_server()->GetURL("a.test", "/fenced_frames/title1.html"));
  FencedFrameURLMapping& url_mapping =
      root->current_frame_host()->GetPage().fenced_frame_urls_map();
  auto urn_uuid = AddAndVerifyFencedFrameURL(&url_mapping, https_url,
                                             fenced_frame_reporter);

  TestFencedFrameURLMappingResultObserver mapping_observer;
  url_mapping.ConvertFencedFrameURNToURL(urn_uuid, &mapping_observer);
  TestFrameNavigationObserver observer(iframe_node);

  EXPECT_EQ(urn_uuid.spec(), EvalJs(root, JsReplace("f.src = $1;", urn_uuid)));

  observer.WaitForCommit();
  EXPECT_TRUE(mapping_observer.mapping_complete_observed());
  EXPECT_EQ(fenced_frame_reporter, mapping_observer.fenced_frame_reporter());

  EXPECT_EQ(https_url,
            iframe_node->current_frame_host()->GetLastCommittedURL());
  EXPECT_EQ(url::Origin::Create(https_url),
            iframe_node->current_frame_host()->GetLastCommittedOrigin());

  std::string event_data = "this is a click";
  EXPECT_TRUE(ExecJs(iframe_node, JsReplace("window.fence.reportEvent({"
                                            "  eventType: 'mouse interaction',"
                                            "  eventData: $1,"
                                            "  destination: ['buyer']});",
                                            event_data)));

  // Verify the preflight request contains the eligibility header under
  // "Access-Control-Request-Headers".
  preflight_response.WaitForRequest();
  VerifyPreflightRequest(preflight_response.http_request());

  // Send response with extra headers.
  std::unique_ptr<net::test_server::BasicHttpResponse> access_allow_response =
      GetResponseWithAccessAllowHeaders(preflight_response.http_request());
  access_allow_response->AddCustomHeader("Content-Type",
                                         "text/plain;charset=UTF-8");
  preflight_response.Send(access_allow_response->ToResponseString());
  preflight_response.Done();

  reporting_response.WaitForRequest();
  // Verify the request has the correct content.
  EXPECT_EQ(reporting_response.http_request()->content, event_data);
  // Verify the request contains the eligibility header.
  EXPECT_EQ(reporting_response.http_request()->headers.at(
                "Attribution-Reporting-Eligible"),
            "event-source");
  EXPECT_FALSE(base::Contains(reporting_response.http_request()->headers,
                              "Attribution-Reporting-Support"));
}

// The reportEvent beacon is a POST request. Upon receiving a 302 redirect
// response, the request is changed to a GET request. In this test case, the
// reporting url is same-origin. There is no preflight requests.
// 1. A POST request is sent to the reporting destination.
// 2. A response with 302 redirect is sent back to the requester.
// 3. A GET request is sent to the redirected destination.
IN_PROC_BROWSER_TEST_F(FencedFrameReportEventBrowserTest,
                       SameOriginReportEventPost302RedirectGet) {
  net::test_server::ControllableHttpResponse response(https_server(),
                                                      kReportingURL);
  net::test_server::ControllableHttpResponse redirect_response(
      https_server(), "/redirect.html");
  ASSERT_TRUE(https_server()->Start());

  // Set up the embedder and a default mode fenced frame.
  GURL main_url = https_server()->GetURL("a.test", "/hello.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  EXPECT_TRUE(ExecJs(root,
                     "var f = document.createElement('fencedframe');"
                     "f.mode = 'opaque-ads';"
                     "document.body.appendChild(f);"));

  EXPECT_EQ(1U, root->child_count());
  FrameTreeNode* fenced_frame_root_node =
      GetFencedFrameRootNode(root->child_at(0));
  EXPECT_TRUE(fenced_frame_root_node->IsFencedFrameRoot());
  EXPECT_TRUE(fenced_frame_root_node->IsInFencedFrameTree());

  GURL https_url(
      https_server()->GetURL("a.test", "/fenced_frames/title1.html"));

  // Create a FencedFrameReporter and pass it reporting metadata.
  scoped_refptr<FencedFrameReporter> fenced_frame_reporter =
      CreateFencedFrameReporter();
  GURL reporting_url(https_server()->GetURL("a.test", kReportingURL));
  // Set valid reporting metadata for buyer.
  fenced_frame_reporter->OnUrlMappingReady(
      blink::FencedFrame::ReportingDestination::kBuyer,
      {{"click", reporting_url}});

  // Get the urn mapping object.
  FencedFrameURLMapping& url_mapping =
      root->current_frame_host()->GetPage().fenced_frame_urls_map();

  // Add url and its reporting metadata to fenced frame url mapping.
  auto urn_uuid = AddAndVerifyFencedFrameURL(&url_mapping, https_url,
                                             fenced_frame_reporter);

  TestFencedFrameURLMappingResultObserver mapping_observer;
  url_mapping.ConvertFencedFrameURNToURL(urn_uuid, &mapping_observer);
  TestFrameNavigationObserver observer(
      fenced_frame_root_node->current_frame_host());

  // Navigate the fenced frame.
  EXPECT_TRUE(ExecJs(root, JsReplace("f.src = $1;", urn_uuid)));

  observer.WaitForCommit();
  EXPECT_TRUE(mapping_observer.mapping_complete_observed());
  EXPECT_EQ(fenced_frame_reporter, mapping_observer.fenced_frame_reporter());

  // Perform the reportEvent call, with a unique body.
  std::string event_data = "this is a click";
  std::string report_event_script = JsReplace(R"(
        window.fence.reportEvent({
          eventType: 'click',
          eventData: $1,
          destination: ['buyer'],
        });
      )",
                                              event_data);
  EXPECT_TRUE(ExecJs(fenced_frame_root_node, report_event_script));

  {
    // Verify the reporting request.
    response.WaitForRequest();
    EXPECT_EQ(response.http_request()->content, event_data);
    EXPECT_EQ(response.http_request()->method,
              net::test_server::HttpMethod::METHOD_POST);
    EXPECT_EQ(
        response.http_request()->headers.at("Attribution-Reporting-Eligible"),
        "event-source");
    EXPECT_TRUE(
        base::Contains(response.http_request()->headers, "Content-Length"));
    EXPECT_TRUE(
        base::Contains(response.http_request()->headers, "Content-Type"));
    EXPECT_TRUE(base::Contains(response.http_request()->headers, "Origin"));
    EXPECT_FALSE(base::Contains(response.http_request()->headers,
                                "Attribution-Reporting-Support"));

    // Send 302 redirect response.
    GURL redirect_url = https_server()->GetURL("a.test", "/redirect.html");
    response.Send(
        /*http_status=*/net::HTTP_FOUND,
        /*content_type=*/"text/plain;charset=UTF-8",
        /*content=*/{}, /*cookies=*/{}, /*extra_headers=*/
        {base::StrCat({"Location: ", redirect_url.spec()})});

    response.Done();
  }

  {
    // Verify the redirect request is a GET request.
    redirect_response.WaitForRequest();
    EXPECT_EQ(redirect_response.http_request()->method,
              net::test_server::HttpMethod::METHOD_GET);
    // Check that POST-specific headers were stripped.
    EXPECT_FALSE(base::Contains(redirect_response.http_request()->headers,
                                "Content-Length"));
    EXPECT_FALSE(base::Contains(redirect_response.http_request()->headers,
                                "Content-Type"));
    EXPECT_FALSE(
        base::Contains(redirect_response.http_request()->headers, "Origin"));
    // Check that the content body was stripped.
    EXPECT_TRUE(redirect_response.http_request()->content.empty());
    // These extra request headers were not stripped.
    EXPECT_EQ(redirect_response.http_request()->headers.at(
                  "Attribution-Reporting-Eligible"),
              "event-source");
    EXPECT_FALSE(base::Contains(response.http_request()->headers,
                                "Attribution-Reporting-Support"));
  }
}

// The reportEvent beacon is a POST request. Upon receiving a 302 redirect
// response, the request is changed to a GET request. In this test case, the
// reporting url is cross-origin.  There are preflight requests.
// 1. A preflight request is sent to the reporting destination.
// 2. A response with 302 redirect is sent back to the requester.
// 3. A preflight request is sent to the redirected destination.
// 4. A response with 200 OK is sent back to the requester.
// 5. A GET request is sent to the redirected destination.
IN_PROC_BROWSER_TEST_F(FencedFrameReportEventBrowserTest,
                       CrossOriginReportEventPost302RedirectGet) {
  net::test_server::ControllableHttpResponse preflight_response(https_server(),
                                                                kReportingURL);
  net::test_server::ControllableHttpResponse reporting_response(https_server(),
                                                                kReportingURL);
  net::test_server::ControllableHttpResponse redirect_preflight_response(
      https_server(), "/redirect.html");
  net::test_server::ControllableHttpResponse redirect_response(
      https_server(), "/redirect.html");
  ASSERT_TRUE(https_server()->Start());

  // Set up the embedder and a default mode fenced frame.
  GURL main_url = https_server()->GetURL("a.test", "/hello.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  EXPECT_TRUE(ExecJs(root,
                     "var f = document.createElement('fencedframe');"
                     "f.mode = 'opaque-ads';"
                     "document.body.appendChild(f);"));

  EXPECT_EQ(1U, root->child_count());
  FrameTreeNode* fenced_frame_root_node =
      GetFencedFrameRootNode(root->child_at(0));
  EXPECT_TRUE(fenced_frame_root_node->IsFencedFrameRoot());
  EXPECT_TRUE(fenced_frame_root_node->IsInFencedFrameTree());

  GURL https_url(
      https_server()->GetURL("a.test", "/fenced_frames/title1.html"));

  // Create a FencedFrameReporter and pass it reporting metadata.
  scoped_refptr<FencedFrameReporter> fenced_frame_reporter =
      CreateFencedFrameReporter();
  GURL reporting_url(https_server()->GetURL("c.test", kReportingURL));
  // Set valid reporting metadata for buyer.
  fenced_frame_reporter->OnUrlMappingReady(
      blink::FencedFrame::ReportingDestination::kBuyer,
      {{"click", reporting_url}});

  // Get the urn mapping object.
  FencedFrameURLMapping& url_mapping =
      root->current_frame_host()->GetPage().fenced_frame_urls_map();

  // Add url and its reporting metadata to fenced frame url mapping.
  auto urn_uuid = AddAndVerifyFencedFrameURL(&url_mapping, https_url,
                                             fenced_frame_reporter);

  TestFencedFrameURLMappingResultObserver mapping_observer;
  url_mapping.ConvertFencedFrameURNToURL(urn_uuid, &mapping_observer);
  TestFrameNavigationObserver observer(
      fenced_frame_root_node->current_frame_host());

  // Navigate the fenced frame.
  EXPECT_TRUE(ExecJs(root, JsReplace("f.src = $1;", urn_uuid)));

  observer.WaitForCommit();
  EXPECT_TRUE(mapping_observer.mapping_complete_observed());
  EXPECT_EQ(fenced_frame_reporter, mapping_observer.fenced_frame_reporter());

  // Perform the reportEvent call, with a unique body.
  std::string event_data = "this is a click";
  std::string report_event_script = JsReplace(R"(
        window.fence.reportEvent({
          eventType: 'click',
          eventData: $1,
          destination: ['buyer'],
        });
      )",
                                              event_data);
  EXPECT_TRUE(ExecJs(fenced_frame_root_node, report_event_script));

  {
    // Verify the preflight request.
    preflight_response.WaitForRequest();
    VerifyPreflightRequest(preflight_response.http_request());

    // Send response with extra headers.
    std::unique_ptr<net::test_server::BasicHttpResponse> access_allow_response =
        GetResponseWithAccessAllowHeaders(preflight_response.http_request());
    access_allow_response->AddCustomHeader("Content-Type",
                                           "text/plain;charset=UTF-8");
    preflight_response.Send(access_allow_response->ToResponseString());

    preflight_response.Done();
  }

  {
    reporting_response.WaitForRequest();
    EXPECT_EQ(reporting_response.http_request()->method,
              net::test_server::HttpMethod::METHOD_POST);
    EXPECT_EQ(reporting_response.http_request()->content, event_data);
    EXPECT_TRUE(base::Contains(reporting_response.http_request()->headers,
                               "Content-Length"));
    EXPECT_TRUE(base::Contains(reporting_response.http_request()->headers,
                               "Content-Type"));
    EXPECT_TRUE(
        base::Contains(reporting_response.http_request()->headers, "Origin"));
    EXPECT_EQ(reporting_response.http_request()->headers.at(
                  "Attribution-Reporting-Eligible"),
              "event-source");
    EXPECT_FALSE(base::Contains(reporting_response.http_request()->headers,
                                "Attribution-Reporting-Support"));

    // Send 302 redirect response, with "Access-Control-Allow-Origin" header.
    // This header is needed to get the redirect through.
    GURL redirect_url = https_server()->GetURL("d.test", "/redirect.html");
    reporting_response.Send(
        /*http_status=*/net::HTTP_FOUND,
        /*content_type=*/"text/plain;charset=UTF-8",
        /*content=*/{}, /*cookies=*/{}, /*extra_headers=*/
        {base::StrCat({cors::kAccessControlAllowOrigin, ": *"}),
         base::StrCat({"Location: ", redirect_url.spec()})});
    reporting_response.Done();
  }

  {
    // Verify the preflight request of the redirect.
    redirect_preflight_response.WaitForRequest();
    VerifyPreflightRequest(preflight_response.http_request());
    EXPECT_EQ(redirect_preflight_response.http_request()->headers.at("Origin"),
              "null");
    // Check that POST-specific headers were stripped.
    EXPECT_FALSE(base::Contains(
        redirect_preflight_response.http_request()->headers, "Content-Length"));
    EXPECT_FALSE(base::Contains(
        redirect_preflight_response.http_request()->headers, "Content-Type"));
    // Check that the content body was stripped.
    EXPECT_TRUE(redirect_preflight_response.http_request()->content.empty());
    // These extra request headers were not stripped.
    EXPECT_EQ(redirect_preflight_response.http_request()->headers.at(
                  cors::kAccessControlRequestHeaders),
              "attribution-reporting-eligible");

    // Send response with extra headers.
    std::unique_ptr<net::test_server::BasicHttpResponse>
        redirect_access_allow_response = GetResponseWithAccessAllowHeaders(
            redirect_preflight_response.http_request());
    redirect_access_allow_response->AddCustomHeader("Content-Type",
                                                    "text/plain;charset=UTF-8");
    redirect_preflight_response.Send(
        redirect_access_allow_response->ToResponseString());

    redirect_preflight_response.Done();
  }

  {
    // Verify the redirect request is a GET request.
    redirect_response.WaitForRequest();
    EXPECT_EQ(redirect_response.http_request()->method,
              net::test_server::HttpMethod::METHOD_GET);
    EXPECT_EQ(redirect_response.http_request()->headers.at("Origin"), "null");
    EXPECT_FALSE(base::Contains(redirect_response.http_request()->headers,
                                "Content-Length"));
    EXPECT_EQ(redirect_response.http_request()->headers.at("Content-Type"),
              "text/plain;charset=UTF-8");
    // Check that the content body was stripped.
    EXPECT_TRUE(redirect_response.http_request()->content.empty());
    // These extra request headers were not stripped.
    EXPECT_EQ(redirect_response.http_request()->headers.at(
                  "Attribution-Reporting-Eligible"),
              "event-source");
    EXPECT_FALSE(base::Contains(reporting_response.http_request()->headers,
                                "Attribution-Reporting-Support"));
  }
}

class FencedFrameReportEventAttributionCrossAppWebEnabledBrowserTest
    : public FencedFrameReportEventBrowserTest {
 public:
  FencedFrameReportEventAttributionCrossAppWebEnabledBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kAttributionReportingCrossAppWeb);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    FencedFrameReportEventAttributionCrossAppWebEnabledBrowserTest,
    ReportEventSameOriginSetsSupportHeader) {
  net::test_server::ControllableHttpResponse response(https_server(),
                                                      kReportingURL);
  ASSERT_TRUE(https_server()->Start());

  // Set up the embedder and a default mode fenced frame.
  GURL main_url = https_server()->GetURL("a.test", "/hello.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  EXPECT_TRUE(ExecJs(root,
                     "var f = document.createElement('fencedframe');"
                     "f.mode = 'opaque-ads';"
                     "document.body.appendChild(f);"));

  EXPECT_EQ(1U, root->child_count());
  FrameTreeNode* fenced_frame_root_node =
      GetFencedFrameRootNode(root->child_at(0));
  EXPECT_TRUE(fenced_frame_root_node->IsFencedFrameRoot());
  EXPECT_TRUE(fenced_frame_root_node->IsInFencedFrameTree());

  GURL https_url(
      https_server()->GetURL("a.test", "/fenced_frames/title1.html"));

  // Create a FencedFrameReporter and pass it reporting metadata.
  scoped_refptr<FencedFrameReporter> fenced_frame_reporter =
      CreateFencedFrameReporter();
  GURL reporting_url(https_server()->GetURL("a.test", kReportingURL));
  // Set valid reporting metadata for buyer.
  fenced_frame_reporter->OnUrlMappingReady(
      blink::FencedFrame::ReportingDestination::kBuyer,
      {{"click", reporting_url}});

  // Get the urn mapping object.
  FencedFrameURLMapping& url_mapping =
      root->current_frame_host()->GetPage().fenced_frame_urls_map();

  // Add url and its reporting metadata to fenced frame url mapping.
  auto urn_uuid = AddAndVerifyFencedFrameURL(&url_mapping, https_url,
                                             fenced_frame_reporter);

  TestFencedFrameURLMappingResultObserver mapping_observer;
  url_mapping.ConvertFencedFrameURNToURL(urn_uuid, &mapping_observer);
  TestFrameNavigationObserver observer(
      fenced_frame_root_node->current_frame_host());

  // Navigate the fenced frame.
  EXPECT_TRUE(ExecJs(root, JsReplace("f.src = $1;", urn_uuid)));

  observer.WaitForCommit();
  EXPECT_TRUE(mapping_observer.mapping_complete_observed());
  EXPECT_EQ(fenced_frame_reporter, mapping_observer.fenced_frame_reporter());

  // Perform the reportEvent call, with a unique body.
  std::string event_data = "this is a click";
  std::string report_event_script = JsReplace(R"(
        window.fence.reportEvent({
          eventType: 'click',
          eventData: $1,
          destination: ['buyer'],
        });
      )",
                                              event_data);
  EXPECT_TRUE(ExecJs(fenced_frame_root_node, report_event_script));

  // Verify the request contains the eligibility header.
  response.WaitForRequest();
  EXPECT_EQ(response.http_request()->content, event_data);
  EXPECT_EQ(
      response.http_request()->headers.at("Attribution-Reporting-Eligible"),
      "event-source");
  EXPECT_EQ(
      response.http_request()->headers.at("Attribution-Reporting-Support"),
      "web");
}

IN_PROC_BROWSER_TEST_F(
    FencedFrameReportEventAttributionCrossAppWebEnabledBrowserTest,
    ReportEventCrossOriginSetsSupportHeader) {
  // For cross-origin reportEvent beacon, a preflight request is sent before the
  // actual request. `preflight_response` is used to handle the preflight
  // request. `reporting_response` is used to handle the actual request. Note
  // ControllableHttpResponse is used in the order they are created.
  net::test_server::ControllableHttpResponse preflight_response(https_server(),
                                                                kReportingURL);
  net::test_server::ControllableHttpResponse reporting_response(https_server(),
                                                                kReportingURL);
  ASSERT_TRUE(https_server()->Start());

  // Set up the embedder and a default mode fenced frame.
  GURL main_url = https_server()->GetURL("a.test", "/hello.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  EXPECT_TRUE(ExecJs(root,
                     "var f = document.createElement('fencedframe');"
                     "f.mode = 'opaque-ads';"
                     "document.body.appendChild(f);"));

  EXPECT_EQ(1U, root->child_count());
  FrameTreeNode* fenced_frame_root_node =
      GetFencedFrameRootNode(root->child_at(0));
  EXPECT_TRUE(fenced_frame_root_node->IsFencedFrameRoot());
  EXPECT_TRUE(fenced_frame_root_node->IsInFencedFrameTree());

  GURL https_url(
      https_server()->GetURL("a.test", "/fenced_frames/title1.html"));

  // Create a FencedFrameReporter and pass it reporting metadata.
  scoped_refptr<FencedFrameReporter> fenced_frame_reporter =
      CreateFencedFrameReporter();
  GURL reporting_url(https_server()->GetURL("c.test", kReportingURL));
  // Set valid reporting metadata for buyer.
  fenced_frame_reporter->OnUrlMappingReady(
      blink::FencedFrame::ReportingDestination::kBuyer,
      {{"click", reporting_url}});

  // Get the urn mapping object.
  FencedFrameURLMapping& url_mapping =
      root->current_frame_host()->GetPage().fenced_frame_urls_map();

  // Add url and its reporting metadata to fenced frame url mapping.
  auto urn_uuid = AddAndVerifyFencedFrameURL(&url_mapping, https_url,
                                             fenced_frame_reporter);

  TestFencedFrameURLMappingResultObserver mapping_observer;
  url_mapping.ConvertFencedFrameURNToURL(urn_uuid, &mapping_observer);
  TestFrameNavigationObserver observer(
      fenced_frame_root_node->current_frame_host());

  // Navigate the fenced frame.
  EXPECT_TRUE(ExecJs(root, JsReplace("f.src = $1;", urn_uuid)));

  observer.WaitForCommit();
  EXPECT_TRUE(mapping_observer.mapping_complete_observed());
  EXPECT_EQ(fenced_frame_reporter, mapping_observer.fenced_frame_reporter());

  // Perform the reportEvent call, with a unique body.
  std::string event_data = "this is a click";
  std::string report_event_script = JsReplace(R"(
        window.fence.reportEvent({
          eventType: 'click',
          eventData: $1,
          destination: ['buyer'],
        });
      )",
                                              event_data);
  EXPECT_TRUE(ExecJs(fenced_frame_root_node, report_event_script));

  {
    // Verify the preflight request contains the eligibility header under
    // "Access-Control-Request-Headers".
    preflight_response.WaitForRequest();
    VerifyPreflightRequest(preflight_response.http_request());

    // Send response with extra headers.
    std::unique_ptr<net::test_server::BasicHttpResponse> access_allow_response =
        GetResponseWithAccessAllowHeaders(preflight_response.http_request());
    access_allow_response->AddCustomHeader("Content-Type",
                                           "text/plain;charset=UTF-8");
    preflight_response.Send(access_allow_response->ToResponseString());

    preflight_response.Done();
  }

  // Verify the request contains the eligibility header.
  {
    reporting_response.WaitForRequest();
    EXPECT_EQ(reporting_response.http_request()->content, event_data);
    EXPECT_EQ(reporting_response.http_request()->headers.at(
                  "Attribution-Reporting-Eligible"),
              "event-source");
    EXPECT_EQ(reporting_response.http_request()->headers.at(
                  "Attribution-Reporting-Support"),
              "web");
  }
}

// Parameterized on whether the feature is enabled or not.
class UUIDFrameTreeBrowserTest : public FencedFrameBrowserTestBase,
                                 public ::testing::WithParamInterface<bool> {
 public:
  UUIDFrameTreeBrowserTest() {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          blink::features::kAllowURNsInIframes);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          blink::features::kAllowURNsInIframes);
    }
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
    return info.param ? "AllowURNsInIframes" : "DoNotAllowURNsInIframes";
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
  auto urn_uuid = AddAndVerifyFencedFrameURL(&url_mapping, frame_url);

  WebContentsConsoleObserver console_observer(web_contents());
  auto filter =
      [](const content::WebContentsConsoleObserver::Message& message) {
        return message.log_level == blink::mojom::ConsoleMessageLevel::kWarning;
      };
  console_observer.SetFilter(base::BindRepeating(filter));
  console_observer.SetPattern(
      "FLEDGE will deprecate supporting iframes to render the winning ad*");

  if (GetParam()) {
    // If the feature is enabled, we should navigate to the mapped page.
    EXPECT_TRUE(NavigateIframeAndCheckURL(web_contents(), "test_iframe",
                                          urn_uuid, frame_url));
    histogram_tester.ExpectBucketCount(
        "Navigation.BrowserMappedUrnUuidInIframeOrFencedFrame", 1, 1);
    // A console warning is emitted during navigation. This will be removed
    // once navigation support for urn::uuid in iframes is deprecated.
    // TODO(crbug.com/1355857)
    EXPECT_FALSE(console_observer.messages().empty());
    EXPECT_EQ(
        console_observer.GetMessageAt(0),
        "FLEDGE will deprecate supporting iframes to render the winning ad. "
        "Please use fenced frames instead. See "
        "https://developer.chrome.com/en/docs/privacy-sandbox/fenced-frame/"
        "#examples");
  } else {
    // If the feature is disabled, navigation should fail.
    EXPECT_FALSE(NavigateIframeAndCheckURL(web_contents(), "test_iframe",
                                           urn_uuid, GURL()));
    histogram_tester.ExpectBucketCount(
        "Navigation.BrowserMappedUrnUuidInIframeOrFencedFrame", 1, 0);
    // No console warning is emitted if the feature is disabled.
    EXPECT_TRUE(console_observer.messages().empty());
  }

  // Parent will still see the src as the urn_uuid and not the mapped url.
  EXPECT_EQ(urn_uuid.spec(), EvalJs(root, "f.src"));

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

  // Parent still sees the src as the urn_uuid.
  EXPECT_EQ(urn_uuid.spec(), EvalJs(root, "f.src"));

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
  auto urn_uuid = AddAndVerifyFencedFrameURL(&url_mapping, frame_url);

  // Top page navigation to a URN should fail regardless of if the feature is
  // enabled.
  EXPECT_FALSE(NavigateToURL(shell(), urn_uuid));
}

INSTANTIATE_TEST_SUITE_P(All,
                         UUIDFrameTreeBrowserTest,
                         ::testing::Values(true, false),
                         &UUIDFrameTreeBrowserTest::DescribeParams);

}  // namespace content
