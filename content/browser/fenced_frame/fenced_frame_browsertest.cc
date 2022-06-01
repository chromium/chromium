// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/fenced_frame/fenced_frame.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/frame.mojom-test-utils.h"
#include "content/public/browser/frame_type.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/mock_web_contents_observer.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

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

class FencedFrameBrowserTest : public ContentBrowserTest {
 protected:
  FencedFrameBrowserTest() = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ContentBrowserTest::SetUpOnMainThread();

    https_server()->AddDefaultHandlers(GetTestDataFilePath());
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    SetupCrossSiteRedirector(https_server());
  }

  WebContentsImpl* web_contents() {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  RenderFrameHostImpl* primary_main_frame_host() {
    return web_contents()->GetMainFrame();
  }

  test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_test_helper_;
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

 private:
  test::FencedFrameTestHelper fenced_frame_test_helper_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

// Tests that the renderer can create a <fencedframe> that results in a
// browser-side content::FencedFrame also being created.
IN_PROC_BROWSER_TEST_F(FencedFrameBrowserTest, CreateFromScriptAndDestroy) {
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

IN_PROC_BROWSER_TEST_F(FencedFrameBrowserTest, CreateFromParser) {
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

IN_PROC_BROWSER_TEST_F(FencedFrameBrowserTest, Navigation) {
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

IN_PROC_BROWSER_TEST_F(FencedFrameBrowserTest, AboutBlankNavigation) {
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

IN_PROC_BROWSER_TEST_F(FencedFrameBrowserTest, FrameIteration) {
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
      base::BindLambdaForTesting([&](FrameTree* frame_tree) {
        if (frame_tree == fenced_frame_rfh->frame_tree()) {
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
    : public mojom::FrameHostInterceptorForTesting {
 public:
  explicit NavigationDelayerInterceptor(RenderFrameHostImpl* render_frame_host,
                                        base::TimeDelta duration)
      : render_frame_host_(render_frame_host),
        duration_(duration),
        impl_(render_frame_host_->frame_host_receiver_for_testing()
                  .SwapImplForTesting(this)) {}

  ~NavigationDelayerInterceptor() override = default;

  mojom::FrameHost* GetForwardingInterface() override { return impl_; }

  void CreateFencedFrame(
      mojo::PendingAssociatedReceiver<blink::mojom::FencedFrameOwnerHost>
          pending_receiver,
      blink::mojom::FencedFrameMode mode,
      CreateFencedFrameCallback callback) override {
    mojo::PendingAssociatedRemote<blink::mojom::FencedFrameOwnerHost>
        original_remote;

    GetForwardingInterface()->CreateFencedFrame(
        original_remote.InitWithNewEndpointAndPassReceiver(), mode,
        std::move(callback));
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
  raw_ptr<mojom::FrameHost> impl_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(FencedFrameBrowserTest, NavigationStartTime) {
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
IN_PROC_BROWSER_TEST_F(FencedFrameBrowserTest, CrossOriginMessagePost) {
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
IN_PROC_BROWSER_TEST_F(FencedFrameBrowserTest,
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
IN_PROC_BROWSER_TEST_F(FencedFrameBrowserTest,
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
IN_PROC_BROWSER_TEST_F(FencedFrameBrowserTest, ViewportSettings) {
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
IN_PROC_BROWSER_TEST_F(FencedFrameBrowserTest, GetPageUkmSourceId) {
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
IN_PROC_BROWSER_TEST_F(FencedFrameBrowserTest, GetPageUkmSourceId_NestedFrame) {
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

IN_PROC_BROWSER_TEST_F(FencedFrameBrowserTest,
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
IN_PROC_BROWSER_TEST_F(FencedFrameBrowserTest, NodesForIsLoading) {
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

  RenderFrameHostImpl* inner_contents_rfh = inner_contents->GetMainFrame();
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

IN_PROC_BROWSER_TEST_F(FencedFrameBrowserTest,
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
// `WebContents::IsLoading`, `FrameTree::IsLoading`, and
// `FrameTreeNode::IsLoading` should return true. Primary `FrameTree::IsLoading`
// value should reflect the loading state of descendant fenced frames.
IN_PROC_BROWSER_TEST_F(FencedFrameBrowserTest, IsLoading) {
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
  FrameTree* fenced_frame_tree = fenced_frame_root_node->frame_tree();

  // All WebContents::IsLoading, FrameTree::IsLoading, and
  // FrameTreeNode::IsLoading should return true when the fenced frame is
  // loading along with primary FrameTree::IsLoading as we check for inner frame
  // trees loading state.
  EXPECT_TRUE(web_contents()->IsLoading());
  EXPECT_TRUE(primary_main_frame_host()->frame_tree()->IsLoading());
  EXPECT_TRUE(fenced_frame_root_node->IsLoading());
  EXPECT_TRUE(fenced_frame_tree->IsLoading());

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
  EXPECT_FALSE(primary_main_frame_host()->frame_tree()->IsLoading());
  EXPECT_FALSE(fenced_frame_root_node->IsLoading());
  EXPECT_FALSE(fenced_frame_tree->IsLoading());
}

// Test that when the documents inside the fenced frame tree are loading,
// WebContentsObserver::DidStartLoading is fired and when document stops loading
// WebContentsObserver::DidStopLoading is fired. In this test primary page
// completed loading before fenced frame starts loading and we test the loading
// state in the end when both primary page and fenced frame completed loading.
IN_PROC_BROWSER_TEST_F(FencedFrameBrowserTest, DidStartAndDidStopLoading) {
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
IN_PROC_BROWSER_TEST_F(FencedFrameBrowserTest, LoadProgressChanged) {
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
IN_PROC_BROWSER_TEST_F(FencedFrameBrowserTest, NavigationHandleFrameType) {
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
IN_PROC_BROWSER_TEST_F(FencedFrameBrowserTest, UnloadHandler) {
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
    console_observer.Wait();
    EXPECT_EQ(1u, console_observer.messages().size());
  }
  {
    WebContentsConsoleObserver console_observer(web_contents());
    console_observer.SetPattern(kConsolePattern);
    EXPECT_TRUE(ExecJs(fenced_frame_rfh.get(),
                       "window.addEventListener('unload', (e) => {});"));
    console_observer.Wait();
    EXPECT_EQ(1u, console_observer.messages().size());
  }
  {
    WebContentsConsoleObserver console_observer(web_contents());
    console_observer.SetPattern(kConsolePattern);
    EXPECT_TRUE(ExecJs(fenced_frame_rfh.get(),
                       "window.onbeforeunload = function(e){};"));
    console_observer.Wait();
    EXPECT_EQ(1u, console_observer.messages().size());
  }
  {
    WebContentsConsoleObserver console_observer(web_contents());
    console_observer.SetPattern(kConsolePattern);
    EXPECT_TRUE(
        ExecJs(fenced_frame_rfh.get(), "window.onunload = function(e){};"));
    console_observer.Wait();
    EXPECT_EQ(1u, console_observer.messages().size());
  }
}

// Tests that an input event targeted to a fenced frame correctly
// triggers a user interaction notification for WebContentsObservers.
IN_PROC_BROWSER_TEST_F(FencedFrameBrowserTest, UserInteractionForFencedFrame) {
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

IN_PROC_BROWSER_TEST_F(FencedFrameBrowserTest,
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

IN_PROC_BROWSER_TEST_F(FencedFrameBrowserTest,
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

class FencedFrameWithSiteIsolationDisabledBrowserTest
    : public FencedFrameBrowserTest {
 public:
  FencedFrameWithSiteIsolationDisabledBrowserTest() = default;
  ~FencedFrameWithSiteIsolationDisabledBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    FencedFrameBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kDisableSiteIsolation);
  }
};

IN_PROC_BROWSER_TEST_F(FencedFrameWithSiteIsolationDisabledBrowserTest,
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

IN_PROC_BROWSER_TEST_F(FencedFrameWithSiteIsolationDisabledBrowserTest,
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
    ::testing::TestParamInfo<std::tuple<std::vector<FrameTypeWithOrigin>,
                                        bool /* shadow_dom_fenced_frame */>>
        param_info) {
  std::string out;
  for (const auto& frame_type : std::get<0>(param_info.param)) {
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
  if (std::get<1>(param_info.param)) {
    out += "ShadowDOM";
  } else {
    out += "MPArch";
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
    : public ContentBrowserTest,
      public testing::WithParamInterface<
          std::tuple<std::vector<FrameTypeWithOrigin>,
                     bool /* shadow_dom_fenced_frame */>> {
 protected:
  FencedFrameNestedFrameBrowserTest() {
    if (std::get<1>(GetParam())) {
      fenced_frame_helper_ = std::make_unique<test::FencedFrameTestHelper>(
          test::FencedFrameTestHelper::FencedFrameType::kShadowDOM);
    } else {
      fenced_frame_helper_ = std::make_unique<test::FencedFrameTestHelper>();
    }
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ContentBrowserTest::SetUpOnMainThread();

    https_server()->AddDefaultHandlers(GetTestDataFilePath());
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    SetupCrossSiteRedirector(https_server());
    ASSERT_TRUE(https_server()->Start());
  }

  WebContentsImpl* web_contents() {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  RenderFrameHostImpl* LoadNestedFrame() {
    const GURL main_url =
        https_server()->GetURL(kSameOriginHostName, "/title1.html");
    EXPECT_TRUE(NavigateToURL(shell(), main_url));
    RenderFrameHostImpl* frame =
        static_cast<RenderFrameHostImpl*>(web_contents()->GetMainFrame());
    int depth = 0;
    for (const auto& type : std::get<0>(GetParam())) {
      ++depth;
      frame = CreateFrame(frame, type, depth);
    }
    return frame;
  }

  bool IsInFencedFrameTest() {
    for (const auto& type : std::get<0>(GetParam())) {
      if (IsFencedFrameType(type))
        return true;
    }
    return false;
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

 private:
  RenderFrameHostImpl* CreateFrame(RenderFrameHostImpl* parent,
                                   FrameTypeWithOrigin type,
                                   int depth) {
    const GURL url = https_server()->GetURL(
        GetHostNameForFrameType(type),
        "/fenced_frames/title1.html?depth=" + base::NumberToString(depth));

    if (IsFencedFrameType(type)) {
      return static_cast<RenderFrameHostImpl*>(
          fenced_frame_helper_->CreateFencedFrame(parent, url));
    }
    EXPECT_TRUE(ExecJs(parent, JsReplace(kAddIframeScript, url)));

    return static_cast<RenderFrameHostImpl*>(ChildFrameAt(parent, 0));
  }

  std::unique_ptr<test::FencedFrameTestHelper> fenced_frame_helper_;
  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_P(FencedFrameNestedFrameBrowserTest,
                       IsNestedWithinFencedFrame) {
  RenderFrameHostImpl* rfh = LoadNestedFrame();
  EXPECT_EQ(IsInFencedFrameTest(), rfh->IsNestedWithinFencedFrame());
}

INSTANTIATE_TEST_SUITE_P(FencedFrameNestedFrameBrowserTest,
                         FencedFrameNestedFrameBrowserTest,
                         testing::Combine(testing::ValuesIn(kTestParameters),
                                          testing::Bool()),
                         TestParamToString);

namespace {

static std::string ModeTestParamToString(
    ::testing::TestParamInfo<std::tuple<blink::mojom::FencedFrameMode,
                                        blink::mojom::FencedFrameMode,
                                        bool /* shadow_dom_fenced_frame */>>
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

  out += "_Arch";

  if (std::get<2>(param_info.param)) {
    out += "ShadowDOM";
  } else {
    out += "MPArch";
  }

  return out;
}

}  // namespace

class FencedFrameNestedModesTest
    : public ContentBrowserTest,
      public testing::WithParamInterface<
          std::tuple<blink::mojom::FencedFrameMode,
                     blink::mojom::FencedFrameMode,
                     bool /*shadow_dom_fenced_frame*/>> {
 protected:
  FencedFrameNestedModesTest() {
    if (std::get<2>(GetParam())) {
      feature_list_.InitWithFeaturesAndParameters(
          {{blink::features::kFencedFrames,
            {{"implementation_type", "shadow_dom"}}},
           {features::kPrivacySandboxAdsAPIsOverride, {}}},
          {/* disabled_features */});
    } else {
      fenced_frame_test_helper_ =
          std::make_unique<test::FencedFrameTestHelper>();
    }
  }

  std::string GetParentMode() { return ModeToString(std::get<0>(GetParam())); }
  std::string GetChildMode() { return ModeToString(std::get<1>(GetParam())); }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ContentBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  WebContentsImpl* web_contents() {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  RenderFrameHostImpl* primary_main_frame_host() {
    return web_contents()->GetMainFrame();
  }

  test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return *fenced_frame_test_helper_.get();
  }

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

  // This is a unique ptr because in some test cases we don't want to use it,
  // and it automatically enables MPArch fenced frames when created.
  std::unique_ptr<test::FencedFrameTestHelper> fenced_frame_test_helper_;
  base::test::ScopedFeatureList feature_list_;
};

// This test runs the following steps:
//   1.) Creates a "parent" fenced frame with a particular mode
//   2.) Creates a nested/child fenced frame with a particular mode
//   3.) Asserts that creation of the child fenced frame either failed or
//       succeeded depending on its mode.
IN_PROC_BROWSER_TEST_P(FencedFrameNestedModesTest, NestedModes) {
  const GURL main_url =
      embedded_test_server()->GetURL("fencedframe.test", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // 1.) Create the parent fenced frame.
  constexpr char kAddFencedFrameScript[] = R"({
      const fenced_frame = document.createElement('fencedframe');
      fenced_frame.mode = $1;
      document.body.appendChild(fenced_frame);
    })";
  EXPECT_TRUE(ExecJs(primary_main_frame_host(),
                     JsReplace(kAddFencedFrameScript, GetParentMode())));

  // Get the fenced frame's RFH.
  ASSERT_EQ(1u, primary_main_frame_host()->child_count());
  RenderFrameHostImpl* parent_fenced_frame_rfh =
      primary_main_frame_host()->child_at(0)->current_frame_host();
  if (blink::features::IsFencedFramesMPArchBased()) {
    int inner_node_id =
        parent_fenced_frame_rfh->inner_tree_main_frame_tree_node_id();
    parent_fenced_frame_rfh =
        FrameTreeNode::GloballyFindByID(inner_node_id)->current_frame_host();
  }
  ASSERT_TRUE(parent_fenced_frame_rfh);

  // 2.) Attempt to create the child fenced frame.
  EXPECT_TRUE(ExecJs(parent_fenced_frame_rfh,
                     JsReplace(kAddFencedFrameScript, GetChildMode())));

  // 3.) Assert that the child fenced frame was created or not created depending
  //     on the test parameters.
  if (GetParentMode() != GetChildMode()) {
    EXPECT_EQ(0u, parent_fenced_frame_rfh->child_count());
    // Child fenced frame creation should have failed based on its mode.
  } else {
    EXPECT_EQ(1u, parent_fenced_frame_rfh->child_count());
    // Child fenced frame creation should have succeeded because its mode is the
    // same as its parent.
  }
}

INSTANTIATE_TEST_SUITE_P(
    FencedFrameNestedModesTest,
    FencedFrameNestedModesTest,
    testing::Combine(/*parent mode=*/testing::Values(
                         blink::mojom::FencedFrameMode::kDefault,
                         blink::mojom::FencedFrameMode::kOpaqueAds),
                     /*child mode=*/
                     testing::Values(blink::mojom::FencedFrameMode::kDefault,
                                     blink::mojom::FencedFrameMode::kOpaqueAds),
                     /*is_shadowdom=*/testing::Bool()),
    ModeTestParamToString);

}  // namespace content
