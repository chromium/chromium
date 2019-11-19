// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/render_frame_host_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/interface_provider_filtering.h"
#include "content/browser/renderer_host/input/timeout_monitor.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/page_visibility_state.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/data/mojo_web_test_helper_test.mojom.h"
#include "content/test/did_commit_navigation_interceptor.h"
#include "content/test/frame_host_test_interface.mojom.h"
#include "content/test/test_content_browser_client.h"
#include "net/base/features.h"
#include "net/base/net_errors.h"
#include "net/cookies/cookie_constants.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/service_manager/public/mojom/interface_provider.mojom-test-utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif  // defined(OS_ANDROID)

namespace content {

namespace {

// Implementation of ContentBrowserClient that overrides
// OverridePageVisibilityState() and allows consumers to set a value.
class PrerenderTestContentBrowserClient : public TestContentBrowserClient {
 public:
  PrerenderTestContentBrowserClient()
      : override_enabled_(false),
        visibility_override_(PageVisibilityState::kVisible) {}
  ~PrerenderTestContentBrowserClient() override {}

  void EnableVisibilityOverride(PageVisibilityState visibility_override) {
    override_enabled_ = true;
    visibility_override_ = visibility_override;
  }

  void OverridePageVisibilityState(
      RenderFrameHost* render_frame_host,
      PageVisibilityState* visibility_state) override {
    if (override_enabled_)
      *visibility_state = visibility_override_;
  }

 private:
  bool override_enabled_;
  PageVisibilityState visibility_override_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderTestContentBrowserClient);
};

const char kTrustMeUrl[] = "trustme://host/path/";
const char kTrustMeIfEmbeddingSecureUrl[] =
    "trustmeifembeddingsecure://host/path/";

// Configure trustme: as a scheme that should cause cookies to be treated as
// first-party when top-level, and also installs a URLLoaderFactory that
// makes all requests to it via kTrustMeUrl return a particular iframe.
// Same for trustmeifembeddingsecure, which does the same if the embedded origin
// is secure.
class FirstPartySchemeContentBrowserClient : public TestContentBrowserClient {
 public:
  explicit FirstPartySchemeContentBrowserClient(const GURL& iframe_url)
      : iframe_url_(iframe_url) {}

  ~FirstPartySchemeContentBrowserClient() override = default;

  bool ShouldTreatURLSchemeAsFirstPartyWhenTopLevel(
      base::StringPiece scheme,
      bool is_embedded_origin_secure) override {
    if (is_embedded_origin_secure && scheme == "trustmeifembeddingsecure")
      return true;
    return scheme == "trustme";
  }

  void RegisterNonNetworkNavigationURLLoaderFactories(
      int frame_tree_node_id,
      NonNetworkURLLoaderFactoryMap* factories) override {
    auto trustme_factory = std::make_unique<network::TestURLLoaderFactory>();
    auto trustmeifembeddingsecure_factory =
        std::make_unique<network::TestURLLoaderFactory>();
    std::string response_body =
        base::StrCat({"<iframe src=\"", iframe_url_.spec(), "\"></iframe>"});
    trustme_factory->AddResponse(kTrustMeUrl, response_body);
    trustmeifembeddingsecure_factory->AddResponse(kTrustMeIfEmbeddingSecureUrl,
                                                  response_body);
    factories->emplace("trustme", std::move(trustme_factory));
    factories->emplace("trustmeifembeddingsecure",
                       std::move(trustmeifembeddingsecure_factory));
  }

 private:
  GURL iframe_url_;

  DISALLOW_COPY_AND_ASSIGN(FirstPartySchemeContentBrowserClient);
};

}  // anonymous namespace

// TODO(mlamouri): part of these tests were removed because they were dependent
// on an environment were focus is guaranteed. This is only for
// interactive_ui_tests so these bits need to move there.
// See https://crbug.com/491535
class RenderFrameHostImplBrowserTest : public ContentBrowserTest {
 public:
  RenderFrameHostImplBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}
  ~RenderFrameHostImplBrowserTest() override {}

  // Return an URL for loading a local test file.
  GURL GetFileURL(const base::FilePath::CharType* file_path) {
    base::FilePath path;
    CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &path));
    path = path.Append(GetTestDataFilePath());
    path = path.Append(file_path);
    return GURL(FILE_PATH_LITERAL("file:") + path.value());
  }

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }
  net::EmbeddedTestServer* https_server() { return &https_server_; }

 private:
  net::EmbeddedTestServer https_server_;
};

// Test that when creating a new window, the main frame is correctly focused.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest, IsFocused_AtLoad) {
  EXPECT_TRUE(
      NavigateToURL(shell(), GetTestUrl("render_frame_host", "focus.html")));

  // The main frame should be focused.
  WebContents* web_contents = shell()->web_contents();
  EXPECT_EQ(web_contents->GetMainFrame(), web_contents->GetFocusedFrame());
}

// Test that if the content changes the focused frame, it is correctly exposed.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest, IsFocused_Change) {
  EXPECT_TRUE(
      NavigateToURL(shell(), GetTestUrl("render_frame_host", "focus.html")));

  WebContents* web_contents = shell()->web_contents();

  std::string frames[2] = {"frame1", "frame2"};
  for (const std::string& frame : frames) {
    ExecuteScriptAndGetValue(web_contents->GetMainFrame(),
                             "focus" + frame + "()");

    // The main frame is not the focused frame in the frame tree but the main
    // frame is focused per RFHI rules because one of its descendant is focused.
    // TODO(mlamouri): we should check the frame focus state per RFHI, see the
    // general comment at the beginning of this test file.
    EXPECT_NE(web_contents->GetMainFrame(), web_contents->GetFocusedFrame());
    EXPECT_EQ(frame, web_contents->GetFocusedFrame()->GetFrameName());
  }
}

// Tests focus behavior when the focused frame is removed from the frame tree.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest, RemoveFocusedFrame) {
  EXPECT_TRUE(
      NavigateToURL(shell(), GetTestUrl("render_frame_host", "focus.html")));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  ExecuteScriptAndGetValue(web_contents->GetMainFrame(), "focusframe4()");

  EXPECT_NE(web_contents->GetMainFrame(), web_contents->GetFocusedFrame());
  EXPECT_EQ("frame4", web_contents->GetFocusedFrame()->GetFrameName());
  EXPECT_EQ("frame3",
            web_contents->GetFocusedFrame()->GetParent()->GetFrameName());
  EXPECT_NE(-1, web_contents->GetFrameTree()->focused_frame_tree_node_id_);

  ExecuteScriptAndGetValue(web_contents->GetMainFrame(), "detachframe(3)");
  EXPECT_EQ(nullptr, web_contents->GetFocusedFrame());
  EXPECT_EQ(-1, web_contents->GetFrameTree()->focused_frame_tree_node_id_);

  ExecuteScriptAndGetValue(web_contents->GetMainFrame(), "focusframe2()");
  EXPECT_NE(nullptr, web_contents->GetFocusedFrame());
  EXPECT_NE(web_contents->GetMainFrame(), web_contents->GetFocusedFrame());
  EXPECT_NE(-1, web_contents->GetFrameTree()->focused_frame_tree_node_id_);

  ExecuteScriptAndGetValue(web_contents->GetMainFrame(), "detachframe(2)");
  EXPECT_EQ(nullptr, web_contents->GetFocusedFrame());
  EXPECT_EQ(-1, web_contents->GetFrameTree()->focused_frame_tree_node_id_);
}

// Test that a frame is visible/hidden depending on its WebContents visibility
// state.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       GetVisibilityState_Basic) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL("data:text/html,foo")));
  WebContents* web_contents = shell()->web_contents();

  web_contents->WasShown();
  EXPECT_EQ(PageVisibilityState::kVisible,
            web_contents->GetMainFrame()->GetVisibilityState());

  web_contents->WasHidden();
  EXPECT_EQ(PageVisibilityState::kHidden,
            web_contents->GetMainFrame()->GetVisibilityState());
}

// Test that a frame visibility can be overridden by the ContentBrowserClient.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       GetVisibilityState_Override) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL("data:text/html,foo")));
  WebContents* web_contents = shell()->web_contents();

  PrerenderTestContentBrowserClient new_client;
  ContentBrowserClient* old_client = SetBrowserClientForTesting(&new_client);

  web_contents->WasShown();
  EXPECT_EQ(PageVisibilityState::kVisible,
            web_contents->GetMainFrame()->GetVisibilityState());

  new_client.EnableVisibilityOverride(PageVisibilityState::kHiddenButPainting);
  EXPECT_EQ(PageVisibilityState::kHiddenButPainting,
            web_contents->GetMainFrame()->GetVisibilityState());

  SetBrowserClientForTesting(old_client);
}

// Check that the URLLoaderFactories created by RenderFrameHosts for renderers
// are not trusted.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       URLLoaderFactoryNotTrusted) {
  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory;
  shell()->web_contents()->GetMainFrame()->CreateNetworkServiceDefaultFactory(
      url_loader_factory.BindNewPipeAndPassReceiver());

  std::unique_ptr<network::ResourceRequest> request =
      std::make_unique<network::ResourceRequest>();
  request->url = embedded_test_server()->GetURL("/echo");
  request->request_initiator =
      url::Origin::Create(embedded_test_server()->base_url());
  request->trusted_params = network::ResourceRequest::TrustedParams();

  content::SimpleURLLoaderTestHelper simple_loader_helper;
  std::unique_ptr<network::SimpleURLLoader> simple_loader =
      network::SimpleURLLoader::Create(std::move(request),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);
  simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory.get(), simple_loader_helper.GetCallback());
  simple_loader_helper.WaitForCallback();
  EXPECT_FALSE(simple_loader_helper.response_body());
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, simple_loader->NetError());
}

namespace {

class TestJavaScriptDialogManager : public JavaScriptDialogManager,
                                    public WebContentsDelegate {
 public:
  TestJavaScriptDialogManager()
      : message_loop_runner_(new MessageLoopRunner), url_invalidate_count_(0) {}
  ~TestJavaScriptDialogManager() override {}

  // This waits until either WCD::BeforeUnloadFired is called (the unload has
  // been handled) or JSDM::RunJavaScriptDialog/RunBeforeUnloadDialog is called
  // (a request to display a dialog has been received).
  void Wait() {
    message_loop_runner_->Run();
    message_loop_runner_ = new MessageLoopRunner;
  }

  // Runs the dialog callback.
  void Run(bool success, const base::string16& user_input) {
    std::move(callback_).Run(success, user_input);
  }

  int num_beforeunload_dialogs_seen() { return num_beforeunload_dialogs_seen_; }
  int num_beforeunload_fired_seen() { return num_beforeunload_fired_seen_; }
  bool proceed() { return proceed_; }

  // WebContentsDelegate

  JavaScriptDialogManager* GetJavaScriptDialogManager(
      WebContents* source) override {
    return this;
  }

  void BeforeUnloadFired(WebContents* tab,
                         bool proceed,
                         bool* proceed_to_fire_unload) override {
    ++num_beforeunload_fired_seen_;
    proceed_ = proceed;
    message_loop_runner_->Quit();
  }

  // JavaScriptDialogManager

  void RunJavaScriptDialog(WebContents* web_contents,
                           RenderFrameHost* render_frame_host,
                           JavaScriptDialogType dialog_type,
                           const base::string16& message_text,
                           const base::string16& default_prompt_text,
                           DialogClosedCallback callback,
                           bool* did_suppress_message) override {
    callback_ = std::move(callback);
    message_loop_runner_->Quit();
  }

  void RunBeforeUnloadDialog(WebContents* web_contents,
                             RenderFrameHost* render_frame_host,
                             bool is_reload,
                             DialogClosedCallback callback) override {
    ++num_beforeunload_dialogs_seen_;
    callback_ = std::move(callback);
    message_loop_runner_->Quit();
  }

  bool HandleJavaScriptDialog(WebContents* web_contents,
                              bool accept,
                              const base::string16* prompt_override) override {
    return true;
  }

  void CancelDialogs(WebContents* web_contents, bool reset_state) override {}

  // Keep track of whether the tab has notified us of a navigation state change
  // which invalidates the displayed URL.
  void NavigationStateChanged(WebContents* source,
                              InvalidateTypes changed_flags) override {
    if (changed_flags & INVALIDATE_TYPE_URL)
      url_invalidate_count_++;
  }

  int url_invalidate_count() { return url_invalidate_count_; }
  void reset_url_invalidate_count() { url_invalidate_count_ = 0; }

 private:
  DialogClosedCallback callback_;

  // The MessageLoopRunner used to spin the message loop.
  scoped_refptr<MessageLoopRunner> message_loop_runner_;

  // The number of times NavigationStateChanged has been called.
  int url_invalidate_count_;

  // The total number of beforeunload dialogs seen by this dialog manager.
  int num_beforeunload_dialogs_seen_ = 0;

  // The total number of BeforeUnloadFired events witnessed by the
  // WebContentsDelegate.
  int num_beforeunload_fired_seen_ = 0;

  // The |proceed| value returned by the last unload event.
  bool proceed_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestJavaScriptDialogManager);
};

class DropBeforeUnloadACKFilter : public BrowserMessageFilter {
 public:
  DropBeforeUnloadACKFilter() : BrowserMessageFilter(FrameMsgStart) {}

 protected:
  ~DropBeforeUnloadACKFilter() override {}

 private:
  // BrowserMessageFilter:
  bool OnMessageReceived(const IPC::Message& message) override {
    return message.type() == FrameHostMsg_BeforeUnload_ACK::ID;
  }

  DISALLOW_COPY_AND_ASSIGN(DropBeforeUnloadACKFilter);
};

mojo::ScopedMessagePipeHandle CreateDisconnectedMessagePipeHandle() {
  mojo::MessagePipe pipe;
  return std::move(pipe.handle0);
}

}  // namespace

// Tests that a beforeunload dialog in an iframe doesn't stop the beforeunload
// timer of a parent frame.
// TODO(avi): flaky on Linux TSAN: http://crbug.com/795326
#if defined(OS_LINUX) && defined(THREAD_SANITIZER)
#define MAYBE_IframeBeforeUnloadParentHang DISABLED_IframeBeforeUnloadParentHang
#else
#define MAYBE_IframeBeforeUnloadParentHang IframeBeforeUnloadParentHang
#endif
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       MAYBE_IframeBeforeUnloadParentHang) {
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  TestJavaScriptDialogManager dialog_manager;
  wc->SetDelegate(&dialog_manager);

  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
  // Make an iframe with a beforeunload handler.
  std::string script =
      "var iframe = document.createElement('iframe');"
      "document.body.appendChild(iframe);"
      "iframe.contentWindow.onbeforeunload=function(e){return 'x'};";
  EXPECT_TRUE(content::ExecuteScript(wc, script));
  EXPECT_TRUE(WaitForLoadStop(wc));
  // JavaScript onbeforeunload dialogs require a user gesture.
  for (auto* frame : wc->GetAllFrames())
    frame->ExecuteJavaScriptWithUserGestureForTests(base::string16());

  // Force a process switch by going to a privileged page. The beforeunload
  // timer will be started on the top-level frame but will be paused while the
  // beforeunload dialog is shown by the subframe.
  GURL web_ui_page(std::string(kChromeUIScheme) + "://" +
                   std::string(kChromeUIGpuHost));
  shell()->LoadURL(web_ui_page);
  dialog_manager.Wait();

  RenderFrameHostImpl* main_frame =
      static_cast<RenderFrameHostImpl*>(wc->GetMainFrame());
  EXPECT_TRUE(main_frame->is_waiting_for_beforeunload_ack());

  // Set up a filter to make sure that when the dialog is answered below and the
  // renderer sends the beforeunload ACK, it gets... ahem... lost.
  scoped_refptr<DropBeforeUnloadACKFilter> filter =
      new DropBeforeUnloadACKFilter();
  main_frame->GetProcess()->AddFilter(filter.get());

  // Answer the dialog.
  dialog_manager.Run(true, base::string16());

  // There will be no beforeunload ACK, so if the beforeunload ACK timer isn't
  // functioning then the navigation will hang forever and this test will time
  // out. If this waiting for the load stop works, this test won't time out.
  EXPECT_TRUE(WaitForLoadStop(wc));
  EXPECT_EQ(web_ui_page, wc->GetLastCommittedURL());

  wc->SetDelegate(nullptr);
  wc->SetJavaScriptDialogManagerForTesting(nullptr);
}

// Tests that a gesture is required in a frame before it can request a
// beforeunload dialog.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       BeforeUnloadDialogRequiresGesture) {
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  TestJavaScriptDialogManager dialog_manager;
  wc->SetDelegate(&dialog_manager);

  EXPECT_TRUE(NavigateToURL(
      shell(), GetTestUrl("render_frame_host", "beforeunload.html")));
  // Disable the hang monitor, otherwise there will be a race between the
  // beforeunload dialog and the beforeunload hang timer.
  wc->GetMainFrame()->DisableBeforeUnloadHangMonitorForTesting();

  // Reload. There should be no beforeunload dialog because there was no gesture
  // on the page. If there was, this WaitForLoadStop call will hang.
  wc->GetController().Reload(ReloadType::NORMAL, false);
  EXPECT_TRUE(WaitForLoadStop(wc));

  // Give the page a user gesture and try reloading again. This time there
  // should be a dialog. If there is no dialog, the call to Wait will hang.
  wc->GetMainFrame()->ExecuteJavaScriptWithUserGestureForTests(
      base::string16());
  wc->GetController().Reload(ReloadType::NORMAL, false);
  dialog_manager.Wait();

  // Answer the dialog.
  dialog_manager.Run(true, base::string16());
  EXPECT_TRUE(WaitForLoadStop(wc));

  // The reload should have cleared the user gesture bit, so upon leaving again
  // there should be no beforeunload dialog.
  shell()->LoadURL(GURL("about:blank"));
  EXPECT_TRUE(WaitForLoadStop(wc));

  wc->SetDelegate(nullptr);
  wc->SetJavaScriptDialogManagerForTesting(nullptr);
}

// Test for crbug.com/80401.  Canceling a beforeunload dialog should reset
// the URL to the previous page's URL.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       CancelBeforeUnloadResetsURL) {
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  TestJavaScriptDialogManager dialog_manager;
  wc->SetDelegate(&dialog_manager);

  GURL url(GetTestUrl("render_frame_host", "beforeunload.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  PrepContentsForBeforeUnloadTest(wc);

  // Navigate to a page that triggers a cross-site transition.
  GURL url2(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  shell()->LoadURL(url2);
  dialog_manager.Wait();

  // Cancel the dialog.
  dialog_manager.reset_url_invalidate_count();
  dialog_manager.Run(false, base::string16());
  EXPECT_FALSE(wc->IsLoading());

  // Verify there are no pending history items after the dialog is cancelled.
  // (see crbug.com/93858)
  NavigationEntry* entry = wc->GetController().GetPendingEntry();
  EXPECT_EQ(nullptr, entry);
  EXPECT_EQ(url, wc->GetVisibleURL());

  // There should have been at least one NavigationStateChange event for
  // invalidating the URL in the address bar, to avoid leaving the stale URL
  // visible.
  EXPECT_GE(dialog_manager.url_invalidate_count(), 1);

  wc->SetDelegate(nullptr);
  wc->SetJavaScriptDialogManagerForTesting(nullptr);
}

// Helper class for beforunload tests.  Sets up a custom dialog manager for the
// main WebContents and provides helpers to register and test beforeunload
// handlers.
//
// TODO(alexmos): Refactor other beforeunload tests in this file to use this
// class.
class RenderFrameHostImplBeforeUnloadBrowserTest
    : public RenderFrameHostImplBrowserTest {
 public:
  RenderFrameHostImplBeforeUnloadBrowserTest() {}

  WebContentsImpl* web_contents() {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  TestJavaScriptDialogManager* dialog_manager() {
    return dialog_manager_.get();
  }

  void CloseDialogAndProceed() {
    dialog_manager_->Run(true /* navigation should proceed */,
                         base::string16());
  }

  void CloseDialogAndCancel() {
    dialog_manager_->Run(false /* navigation should proceed */,
                         base::string16());
  }

  // Installs a beforeunload handler in the given frame.
  // |before_unload_options| specify whether the handler should send a "ping"
  // message through domAutomationController, and/or whether it should trigger
  // the modal beforeunload confirmation dialog.
  enum BeforeUnloadOptions {
    SHOW_DIALOG = 1,
    SEND_PING = 2,
  };
  void InstallBeforeUnloadHandler(FrameTreeNode* ftn,
                                  int before_unload_options) {
    std::string script = "window.onbeforeunload = () => { ";
    if (before_unload_options & SEND_PING)
      script += "domAutomationController.send('ping'); ";
    if (before_unload_options & SHOW_DIALOG)
      script += "return 'x'; ";
    script += " }";
    EXPECT_TRUE(ExecuteScript(ftn, script));
  }

  int RetrievePingsFromMessageQueue(DOMMessageQueue* msg_queue) {
    int num_pings = 0;
    std::string message;
    while (msg_queue->PopMessage(&message)) {
      base::TrimString(message, "\"", &message);
      // Only count messages from beforeunload.  For example, an ExecuteScript
      // sends its own message to DOMMessageQueue, which we need to ignore.
      if (message == "ping")
        ++num_pings;
    }
    return num_pings;
  }

 protected:
  void SetUpOnMainThread() override {
    RenderFrameHostImplBrowserTest::SetUpOnMainThread();
    dialog_manager_.reset(new TestJavaScriptDialogManager);
    web_contents()->SetDelegate(dialog_manager_.get());
  }

  void TearDownOnMainThread() override {
    web_contents()->SetDelegate(nullptr);
    web_contents()->SetJavaScriptDialogManagerForTesting(nullptr);
    RenderFrameHostImplBrowserTest::TearDownOnMainThread();
  }

 private:
  std::unique_ptr<TestJavaScriptDialogManager> dialog_manager_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameHostImplBeforeUnloadBrowserTest);
};

// Check that when a frame performs a browser-initiated navigation, its
// cross-site subframe is able to execute a beforeunload handler and put up a
// dialog to cancel or allow the navigation. This matters especially in
// --site-per-process mode; see https://crbug.com/853021.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBeforeUnloadBrowserTest,
                       SubframeShowsDialogWhenMainFrameNavigates) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Install a beforeunload handler in the first iframe.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  InstallBeforeUnloadHandler(root->child_at(0), SHOW_DIALOG);

  // Disable beforeunload timer to prevent flakiness.
  PrepContentsForBeforeUnloadTest(web_contents());

  // Navigate cross-site and wait for the beforeunload dialog to be shown from
  // the subframe.
  GURL cross_site_url(embedded_test_server()->GetURL("c.com", "/title1.html"));
  shell()->LoadURL(cross_site_url);
  dialog_manager()->Wait();

  // Only the main frame should be marked as waiting for beforeunload ACK as
  // the frame being navigated.
  RenderFrameHostImpl* main_frame = web_contents()->GetMainFrame();
  RenderFrameHostImpl* child = root->child_at(0)->current_frame_host();
  EXPECT_TRUE(main_frame->is_waiting_for_beforeunload_ack());
  EXPECT_FALSE(child->is_waiting_for_beforeunload_ack());

  // Sanity check that the main frame is waiting for subframe's beforeunload
  // ACK.
  EXPECT_EQ(main_frame, child->GetBeforeUnloadInitiator());
  EXPECT_EQ(main_frame, main_frame->GetBeforeUnloadInitiator());
  EXPECT_EQ(1u, main_frame->beforeunload_pending_replies_.size());

  // In --site-per-process mode, the beforeunload ACK should come back from the
  // child RFH.  Without --site-per-process, it will come from the main frame
  // RFH, which processes beforeunload for both main frame and child frame,
  // since they are in the same process.
  RenderFrameHostImpl* frame_that_sent_beforeunload_ipc =
      AreAllSitesIsolatedForTesting() ? child : main_frame;
  EXPECT_TRUE(main_frame->beforeunload_pending_replies_.count(
      frame_that_sent_beforeunload_ipc));

  // Answer the dialog with "cancel" to stay on current page.
  CloseDialogAndCancel();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  EXPECT_EQ(main_url, web_contents()->GetLastCommittedURL());

  // Verify beforeunload state has been cleared.
  EXPECT_FALSE(main_frame->is_waiting_for_beforeunload_ack());
  EXPECT_FALSE(child->is_waiting_for_beforeunload_ack());
  EXPECT_EQ(nullptr, main_frame->GetBeforeUnloadInitiator());
  EXPECT_EQ(nullptr, child->GetBeforeUnloadInitiator());
  EXPECT_EQ(0u, main_frame->beforeunload_pending_replies_.size());

  // Try navigating again.  The dialog should come up again.
  shell()->LoadURL(cross_site_url);
  dialog_manager()->Wait();
  EXPECT_TRUE(main_frame->is_waiting_for_beforeunload_ack());

  // Now answer the dialog and allow the navigation to proceed.  Disable
  // SwapOut ACK on the old frame so that it sticks around in pending delete
  // state, since the test later verifies that it has received the beforeunload
  // ACK.
  TestFrameNavigationObserver commit_observer(root);
  main_frame->DisableSwapOutTimerForTesting();
  CloseDialogAndProceed();
  commit_observer.WaitForCommit();
  EXPECT_EQ(cross_site_url, web_contents()->GetLastCommittedURL());
  EXPECT_FALSE(
      web_contents()->GetMainFrame()->is_waiting_for_beforeunload_ack());

  // The navigation that succeeded was a browser-initiated, main frame
  // navigation, so it swapped RenderFrameHosts. |main_frame| should now be
  // pending deletion and waiting for swapout ACK, but it should not be waiting
  // for the beforeunload ACK.
  EXPECT_FALSE(main_frame->is_active());
  EXPECT_FALSE(main_frame->is_waiting_for_beforeunload_ack());
  EXPECT_EQ(0u, main_frame->beforeunload_pending_replies_.size());
  EXPECT_EQ(nullptr, main_frame->GetBeforeUnloadInitiator());
}

// Check that when a frame with multiple cross-site subframes navigates, all
// the subframes execute their beforeunload handlers, but at most one
// beforeunload dialog is allowed per navigation.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBeforeUnloadBrowserTest,
                       MultipleSubframes) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c),b,c(d),c,d)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Install a beforeunload handler in five of eight frames to send a ping via
  // domAutomationController and request a beforeunload dialog.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  InstallBeforeUnloadHandler(root, SEND_PING | SHOW_DIALOG);
  InstallBeforeUnloadHandler(root->child_at(0)->child_at(0),
                             SEND_PING | SHOW_DIALOG);
  InstallBeforeUnloadHandler(root->child_at(1), SEND_PING | SHOW_DIALOG);
  InstallBeforeUnloadHandler(root->child_at(2), SEND_PING | SHOW_DIALOG);
  InstallBeforeUnloadHandler(root->child_at(2)->child_at(0),
                             SEND_PING | SHOW_DIALOG);

  // Disable beforeunload timer to prevent flakiness.
  PrepContentsForBeforeUnloadTest(web_contents());

  // Navigate main frame cross-site and wait for the beforeunload dialog to be
  // shown from one of the frames.
  DOMMessageQueue msg_queue;
  GURL cross_site_url(embedded_test_server()->GetURL("e.com", "/title1.html"));
  shell()->LoadURL(cross_site_url);
  dialog_manager()->Wait();

  // Answer the dialog and allow the navigation to proceed.
  CloseDialogAndProceed();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  EXPECT_EQ(cross_site_url, web_contents()->GetLastCommittedURL());

  // We should've received five beforeunload pings.
  EXPECT_EQ(5, RetrievePingsFromMessageQueue(&msg_queue));

  // No more beforeunload dialogs shouldn't been shown, due to a policy of at
  // most one dialog per navigation.
  EXPECT_EQ(1, dialog_manager()->num_beforeunload_dialogs_seen());
}

// Similar to the test above, but test scenarios where the subframes with
// beforeunload handlers aren't local roots.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBeforeUnloadBrowserTest,
                       NonLocalRootSubframes) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a(b),c(c))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Install a beforeunload handler in two of five frames to send a ping via
  // domAutomationController and request a beforeunload dialog.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  InstallBeforeUnloadHandler(root->child_at(0), SEND_PING | SHOW_DIALOG);
  InstallBeforeUnloadHandler(root->child_at(0)->child_at(0),
                             SEND_PING | SHOW_DIALOG);

  // Disable beforeunload timer to prevent flakiness.
  PrepContentsForBeforeUnloadTest(web_contents());

  // Navigate and wait for the beforeunload dialog to be shown from one of the
  // frames.
  DOMMessageQueue msg_queue;
  GURL cross_site_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  shell()->LoadURL(cross_site_url);
  dialog_manager()->Wait();

  // Answer the dialog and allow the navigation to proceed.
  CloseDialogAndProceed();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  EXPECT_EQ(cross_site_url, web_contents()->GetLastCommittedURL());

  // We should've received two beforeunload pings.
  EXPECT_EQ(2, RetrievePingsFromMessageQueue(&msg_queue));

  // No more beforeunload dialogs shouldn't been shown, due to a policy of at
  // most one dialog per navigation.
  EXPECT_EQ(1, dialog_manager()->num_beforeunload_dialogs_seen());
}

// Test that cross-site subframes run the beforeunload handler when the main
// frame performs a renderer-initiated navigation.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBeforeUnloadBrowserTest,
                       RendererInitiatedNavigation) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a,b,c)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Install a beforeunload handler in both a.com frames to send a ping via
  // domAutomationController.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  InstallBeforeUnloadHandler(root, SEND_PING);
  InstallBeforeUnloadHandler(root->child_at(0), SEND_PING);

  // Install a beforeunload handler in the b.com frame to put up a dialog.
  InstallBeforeUnloadHandler(root->child_at(1), SHOW_DIALOG);

  // Disable beforeunload timer to prevent flakiness.
  PrepContentsForBeforeUnloadTest(web_contents());

  // Start a same-site renderer-initiated navigation.  The beforeunload dialog
  // from the b.com frame should be shown.  The other two a.com frames should
  // send pings from their beforeunload handlers.
  DOMMessageQueue msg_queue;
  GURL new_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  TestNavigationManager navigation_manager(web_contents(), new_url);
  // Use ExecuteScriptAsync because a ping may arrive before the script
  // execution completion notification and confuse our expectations.
  ExecuteScriptAsync(root, "location.href = '" + new_url.spec() + "';");
  dialog_manager()->Wait();

  // Answer the dialog and allow the navigation to proceed.  Note that at this
  // point, without site isolation, the navigation hasn't started yet, as the
  // navigating frame is still processing beforeunload for all its descendant
  // local frames.  With site isolation, the a.com frames have finished
  // beforeunload, and the browser process has received OnBeginNavigation, but
  // the navigation is paused until the b.com subframe process finishes running
  // beforeunload.
  CloseDialogAndProceed();

  // Wait for navigation to end.
  navigation_manager.WaitForNavigationFinished();
  EXPECT_EQ(new_url, web_contents()->GetLastCommittedURL());

  // We should have received two pings from two a.com frames.  If we receive
  // more, that probably means we ran beforeunload an extra time in the a.com
  // frames.
  EXPECT_EQ(2, RetrievePingsFromMessageQueue(&msg_queue));
  EXPECT_EQ(1, dialog_manager()->num_beforeunload_dialogs_seen());
}

// Similar to the test above, but check a navigation in a subframe rather than
// the main frame.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBeforeUnloadBrowserTest,
                       RendererInitiatedNavigationInSubframe) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c),c)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Install a beforeunload handler to send a ping in all frames.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  InstallBeforeUnloadHandler(root, SEND_PING);
  InstallBeforeUnloadHandler(root->child_at(0), SEND_PING);
  InstallBeforeUnloadHandler(root->child_at(0)->child_at(0), SEND_PING);
  InstallBeforeUnloadHandler(root->child_at(1), SEND_PING);

  // Disable beforeunload timer to prevent flakiness.
  PrepContentsForBeforeUnloadTest(web_contents());

  // Start a renderer-initiated navigation in the middle frame.
  DOMMessageQueue msg_queue;
  GURL new_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  TestNavigationManager navigation_manager(web_contents(), new_url);
  // Use ExecuteScriptAsync because a ping may arrive before the script
  // execution completion notification and confuse our expectations.
  ExecuteScriptAsync(root->child_at(0),
                     "location.href = '" + new_url.spec() + "';");
  navigation_manager.WaitForNavigationFinished();
  EXPECT_EQ(new_url,
            root->child_at(0)->current_frame_host()->GetLastCommittedURL());

  // We should have received two pings from the b.com frame and its child.
  // Other frames' beforeunload handlers shouldn't have run.
  EXPECT_EQ(2, RetrievePingsFromMessageQueue(&msg_queue));

  // We shouldn't have seen any beforeunload dialogs.
  EXPECT_EQ(0, dialog_manager()->num_beforeunload_dialogs_seen());
}

// Ensure that when a beforeunload handler deletes a subframe which is also
// running beforeunload, the navigation can still proceed.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBeforeUnloadBrowserTest,
                       DetachSubframe) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Install a beforeunload handler in root frame to delete the subframe.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  std::string script =
      "window.onbeforeunload = () => { "
      "  document.body.removeChild(document.querySelector('iframe'));"
      "}";
  EXPECT_TRUE(ExecuteScript(root, script));

  // Install a beforeunload handler which never finishes in subframe.
  EXPECT_TRUE(ExecuteScript(root->child_at(0),
                            "window.onbeforeunload = () => { while (1) ; }"));

  // Disable beforeunload timer to prevent flakiness.
  PrepContentsForBeforeUnloadTest(web_contents());

  // Navigate main frame and ensure that it doesn't time out.  When the main
  // frame detaches the subframe, the RFHI destruction should unblock the
  // navigation from waiting on the subframe's beforeunload ACK.
  GURL new_url(embedded_test_server()->GetURL("c.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), new_url));
}

// Ensure that A(B(A)) cases work sanely with beforeunload handlers.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBeforeUnloadBrowserTest,
                       RendererInitiatedNavigationInABAB) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(a(b)))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Install a beforeunload handler to send a ping in all frames.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  InstallBeforeUnloadHandler(root, SEND_PING);
  InstallBeforeUnloadHandler(root->child_at(0), SEND_PING);
  InstallBeforeUnloadHandler(root->child_at(0)->child_at(0), SEND_PING);
  InstallBeforeUnloadHandler(root->child_at(0)->child_at(0)->child_at(0),
                             SEND_PING);

  // Disable beforeunload timer to prevent flakiness.
  PrepContentsForBeforeUnloadTest(web_contents());

  // Navigate the main frame.
  DOMMessageQueue msg_queue;
  GURL new_url(embedded_test_server()->GetURL("c.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), new_url));

  // We should have received four pings.
  EXPECT_EQ(4, RetrievePingsFromMessageQueue(&msg_queue));

  // We shouldn't have seen any beforeunload dialogs.
  EXPECT_EQ(0, dialog_manager()->num_beforeunload_dialogs_seen());
}

// Ensure that the beforeunload timeout works properly when
// beforeunload handlers from subframes time out.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBeforeUnloadBrowserTest,
                       TimeoutInSubframe) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Install a beforeunload handler to send a ping in main frame.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  InstallBeforeUnloadHandler(root, SEND_PING);

  // Install a beforeunload handler which never finishes in subframe.
  EXPECT_TRUE(ExecuteScript(root->child_at(0),
                            "window.onbeforeunload = () => { while (1) ; }"));

  // Navigate the main frame.  We should eventually time out on the subframe
  // beforeunload handler and complete the navigation.
  GURL new_url(embedded_test_server()->GetURL("c.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), new_url));
}

// Ensure that the beforeunload timeout isn't restarted when a frame attempts
// to show a beforeunload dialog and fails because the dialog is already being
// shown by another frame.  See https://crbug.com/865223.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBeforeUnloadBrowserTest,
                       TimerNotRestartedBySecondDialog) {
  // This test exercises a scenario that's only possible with
  // --site-per-process.
  if (!AreAllSitesIsolatedForTesting())
    return;

  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  RenderFrameHostImpl* main_frame = web_contents()->GetMainFrame();

  // Install a beforeunload handler to show a dialog in both frames.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  InstallBeforeUnloadHandler(root, SHOW_DIALOG);
  InstallBeforeUnloadHandler(root->child_at(0), SHOW_DIALOG);

  // Extend the beforeunload timeout to prevent flakiness.  This test can't use
  // PrepContentsForBeforeUnloadTest(), as that clears the timer altogether,
  // and this test needs the timer to be valid, to see whether it gets paused
  // and not restarted correctly.
  main_frame->SetBeforeUnloadTimeoutDelayForTesting(
      base::TimeDelta::FromSeconds(30));

  // Start a navigation in the main frame.
  GURL new_url(embedded_test_server()->GetURL("c.com", "/title1.html"));
  shell()->LoadURL(new_url);

  // We should have two pending beforeunload ACKs at this point, and the
  // beforeunload timer should be running.
  EXPECT_EQ(2u, main_frame->beforeunload_pending_replies_.size());
  EXPECT_TRUE(main_frame->beforeunload_timeout_->IsRunning());

  // Wait for the dialog from one of the frames.  Note that either frame could
  // be the first to trigger the dialog.
  dialog_manager()->Wait();

  // The dialog should've canceled the timer.
  EXPECT_FALSE(main_frame->beforeunload_timeout_->IsRunning());

  // Don't close the dialog and allow the second beforeunload to come in and
  // attempt to show a dialog.  This should fail due to the intervention of at
  // most one dialog per navigation and respond to the renderer with the
  // confirmation to proceed, which should trigger a beforeunload ACK
  // from the second frame. Wait for that beforeunload ACK.  After it's
  // received, there will be one ACK remaining for the frame that's currently
  // showing the dialog.
  while (main_frame->beforeunload_pending_replies_.size() > 1) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }

  // Ensure that the beforeunload timer hasn't been restarted, since the first
  // beforeunload dialog is still up at this point.
  EXPECT_FALSE(main_frame->beforeunload_timeout_->IsRunning());

  // Cancel the dialog and make sure we stay on the old page.
  CloseDialogAndCancel();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  EXPECT_EQ(main_url, web_contents()->GetLastCommittedURL());
}

namespace {

// A helper to execute some script in a frame just before it is deleted, such
// that no message loops are pumped and no sync IPC messages are processed
// between script execution and the destruction of the RenderFrameHost  .
class ExecuteScriptBeforeRenderFrameDeletedHelper
    : public RenderFrameDeletedObserver {
 public:
  ExecuteScriptBeforeRenderFrameDeletedHelper(RenderFrameHost* observed_frame,
                                              const std::string& script)
      : RenderFrameDeletedObserver(observed_frame), script_(script) {}

 protected:
  // WebContentsObserver:
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override {
    const bool was_deleted = deleted();
    RenderFrameDeletedObserver::RenderFrameDeleted(render_frame_host);
    if (deleted() && !was_deleted)
      ExecuteScriptAsync(render_frame_host, script_);
  }

 private:
  std::string script_;

  DISALLOW_COPY_AND_ASSIGN(ExecuteScriptBeforeRenderFrameDeletedHelper);
};

}  // namespace

// Regression test for https://crbug.com/728171 where the sync IPC channel has a
// connection error but we don't properly check for it. This occurs because we
// send a sync window.open IPC after the RenderFrameHost is destroyed.
//
// The test creates two WebContents rendered in the same process. The first is
// is the window-opener of the second, so the first window can be used to relay
// information collected during the destruction of the RenderFrame in the second
// WebContents back to the browser process.
//
// The issue is then reproduced by asynchronously triggering a call to
// window.open() in the main frame of the second WebContents in response to
// WebContentsObserver::RenderFrameDeleted -- that is, just before the RFHI is
// destroyed on the browser side. The test assumes that between these two
// events, the UI message loop is not pumped, and no sync IPC messages are
// processed on the UI thread.
//
// Note that if the second WebContents scheduled a call to window.close() to
// close itself after it calls window.open(), the CreateNewWindow sync IPC could
// be dispatched *before* WidgetHostMsg_Close in the browser process, provided
// that the browser happened to be in IPC::SyncChannel::WaitForReply on the UI
// thread (most likely after sending GpuCommandBufferMsg_* messages), in which
// case incoming sync IPCs to this thread are dispatched, but the message loop
// is not pumped, so proxied non-sync IPCs are not delivered.
//
// Furthermore, on Android, exercising window.open() must be delayed until after
// content::RemoveShellView returns, as that method calls into JNI to close the
// view corresponding to the WebContents, which will then call back into native
// code and may run nested message loops and send sync IPC messages.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       FrameDetached_WindowOpenIPCFails) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestUrl("", "title1.html")));
  EXPECT_EQ(1u, Shell::windows().size());
  GURL test_url = GetTestUrl("render_frame_host", "window_open.html");
  std::string open_script =
      base::StringPrintf("popup = window.open('%s');", test_url.spec().c_str());

  TestNavigationObserver second_contents_navigation_observer(nullptr, 1);
  second_contents_navigation_observer.StartWatchingNewWebContents();
  EXPECT_TRUE(content::ExecuteScript(shell(), open_script));
  second_contents_navigation_observer.Wait();

  ASSERT_EQ(2u, Shell::windows().size());
  Shell* new_shell = Shell::windows()[1];
  ExecuteScriptBeforeRenderFrameDeletedHelper deleted_observer(
      new_shell->web_contents()->GetMainFrame(), "callWindowOpen();");
  new_shell->Close();
  deleted_observer.WaitUntilDeleted();

  bool did_call_window_open = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(), "domAutomationController.send(!!popup.didCallWindowOpen)",
      &did_call_window_open));
  EXPECT_TRUE(did_call_window_open);

  std::string result_of_window_open;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      shell(), "domAutomationController.send(String(popup.resultOfWindowOpen))",
      &result_of_window_open));
  EXPECT_EQ("null", result_of_window_open);
}

namespace {
void PostRequestMonitor(int* post_counter,
                        const net::test_server::HttpRequest& request) {
  if (request.method != net::test_server::METHOD_POST)
    return;
  (*post_counter)++;
  auto it = request.headers.find("Content-Type");
  CHECK(it != request.headers.end());
  CHECK(!it->second.empty());
}
}  // namespace

// Verifies form submits and resubmits work.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest, POSTNavigation) {
  net::EmbeddedTestServer http_server;
  http_server.AddDefaultHandlers(GetTestDataFilePath());
  int post_counter = 0;
  http_server.RegisterRequestMonitor(
      base::BindRepeating(&PostRequestMonitor, &post_counter));
  ASSERT_TRUE(http_server.Start());

  GURL url(http_server.GetURL("/session_history/form.html"));
  GURL post_url = http_server.GetURL("/echotitle");

  // Navigate to a page with a form.
  TestNavigationObserver observer(shell()->web_contents());
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_EQ(url, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());

  // Submit the form.
  GURL submit_url("javascript:submitForm('isubmit')");
  EXPECT_TRUE(
      NavigateToURL(shell(), submit_url, post_url /* expected_commit_url */));

  // Check that a proper POST navigation was done.
  EXPECT_EQ("text=&select=a",
            base::UTF16ToASCII(shell()->web_contents()->GetTitle()));
  EXPECT_EQ(post_url, shell()->web_contents()->GetLastCommittedURL());
  EXPECT_TRUE(shell()
                  ->web_contents()
                  ->GetController()
                  .GetLastCommittedEntry()
                  ->GetHasPostData());

  // Reload and verify the form was submitted.
  shell()->web_contents()->GetController().Reload(ReloadType::NORMAL, false);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ("text=&select=a",
            base::UTF16ToASCII(shell()->web_contents()->GetTitle()));
  CHECK_EQ(2, post_counter);
}

namespace {

class NavigationHandleGrabber : public WebContentsObserver {
 public:
  explicit NavigationHandleGrabber(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override {
    if (navigation_handle->GetURL().path() != "/title2.html")
      return;
    NavigationRequest::From(navigation_handle)
        ->set_complete_callback_for_testing(
            base::Bind(&NavigationHandleGrabber::SendingNavigationCommitted,
                       base::Unretained(this), navigation_handle));
  }

  void SendingNavigationCommitted(
      NavigationHandle* navigation_handle,
      NavigationThrottle::ThrottleCheckResult result) {
    if (navigation_handle->GetURL().path() != "/title2.html")
      return;
    ExecuteScriptAsync(web_contents(), "document.open();");
  }

  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    if (navigation_handle->GetURL().path() != "/title2.html")
      return;
    if (navigation_handle->HasCommitted())
      committed_title2_ = true;
    run_loop_.Quit();
  }

  void WaitForTitle2() { run_loop_.Run(); }

  bool committed_title2() { return committed_title2_; }

 private:
  bool committed_title2_ = false;
  base::RunLoop run_loop_;
};
}  // namespace

// Verifies that if a frame aborts a navigation right after it starts, it is
// cancelled.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest, FastNavigationAbort) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Now make a navigation.
  NavigationHandleGrabber observer(shell()->web_contents());
  const base::string16 title = base::ASCIIToUTF16("done");
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(),
                            "window.location.href='/title2.html'"));
  observer.WaitForTitle2();
  // Flush IPCs to make sure the renderer didn't tell us to navigate. Need to
  // make two round trips.
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(), ""));
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(), ""));
  EXPECT_FALSE(observer.committed_title2());
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       TerminationDisablersClearedOnRendererCrash) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GetTestUrl("render_frame_host", "beforeunload.html")));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_rfh1 =
      static_cast<RenderFrameHostImpl*>(wc->GetMainFrame());

  EXPECT_TRUE(main_rfh1->GetSuddenTerminationDisablerState(
      blink::kBeforeUnloadHandler));

  // Make the renderer crash.
  RenderProcessHost* renderer_process = main_rfh1->GetProcess();
  RenderProcessHostWatcher crash_observer(
      renderer_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  renderer_process->Shutdown(0);
  crash_observer.Wait();

  EXPECT_FALSE(main_rfh1->GetSuddenTerminationDisablerState(
      blink::kBeforeUnloadHandler));

  // This should not trigger a DCHECK once the renderer sends up the termination
  // disabler flags.
  shell()->web_contents()->GetController().Reload(ReloadType::NORMAL, false);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  RenderFrameHostImpl* main_rfh2 =
      static_cast<RenderFrameHostImpl*>(wc->GetMainFrame());
  EXPECT_TRUE(main_rfh2->GetSuddenTerminationDisablerState(
      blink::kBeforeUnloadHandler));
}

// Aborted renderer-initiated navigations that don't destroy the current
// document (e.g. no error page is displayed) must not cancel pending
// XMLHttpRequests.
// See https://crbug.com/762945.
IN_PROC_BROWSER_TEST_F(
    ContentBrowserTest,
    AbortedRendererInitiatedNavigationDoNotCancelPendingXHR) {
  net::test_server::ControllableHttpResponse xhr_response(
      embedded_test_server(), "/xhr_request");
  EXPECT_TRUE(embedded_test_server()->Start());

  GURL main_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // 1) Send an xhr request, but do not send its response for the moment.
  const char* send_slow_xhr =
      "var request = new XMLHttpRequest();"
      "request.addEventListener('abort', () => document.title = 'xhr aborted');"
      "request.addEventListener('load', () => document.title = 'xhr loaded');"
      "request.open('GET', '%s');"
      "request.send();";
  const GURL slow_url = embedded_test_server()->GetURL("/xhr_request");
  EXPECT_TRUE(content::ExecuteScript(
      shell(), base::StringPrintf(send_slow_xhr, slow_url.spec().c_str())));
  xhr_response.WaitForRequest();

  // 2) In the meantime, create a renderer-initiated navigation. It will be
  // aborted.
  TestNavigationManager observer(shell()->web_contents(),
                                 GURL("customprotocol:aborted"));
  EXPECT_TRUE(content::ExecuteScript(
      shell(), "window.location = 'customprotocol:aborted'"));
  EXPECT_FALSE(observer.WaitForResponse());
  observer.WaitForNavigationFinished();

  // 3) Send the response for the XHR requests.
  xhr_response.Send(
      "HTTP/1.1 200 OK\r\n"
      "Connection: close\r\n"
      "Content-Length: 2\r\n"
      "Content-Type: text/plain; charset=utf-8\r\n"
      "\r\n"
      "OK");
  xhr_response.Done();

  // 4) Wait for the XHR request to complete.
  const base::string16 xhr_aborted_title = base::ASCIIToUTF16("xhr aborted");
  const base::string16 xhr_loaded_title = base::ASCIIToUTF16("xhr loaded");
  TitleWatcher watcher(shell()->web_contents(), xhr_loaded_title);
  watcher.AlsoWaitForTitle(xhr_aborted_title);

  EXPECT_EQ(xhr_loaded_title, watcher.WaitAndGetTitle());
}

// A browser-initiated javascript-url navigation must not prevent the current
// document from loading.
// See https://crbug.com/766149.
IN_PROC_BROWSER_TEST_F(ContentBrowserTest,
                       BrowserInitiatedJavascriptUrlDoNotPreventLoading) {
  net::test_server::ControllableHttpResponse main_document_response(
      embedded_test_server(), "/main_document");
  EXPECT_TRUE(embedded_test_server()->Start());

  GURL main_document_url(embedded_test_server()->GetURL("/main_document"));
  TestNavigationManager main_document_observer(shell()->web_contents(),
                                               main_document_url);

  // 1) Navigate. Send the header but not the body. The navigation commits in
  //    the browser. The renderer is still loading the document.
  {
    shell()->LoadURL(main_document_url);
    EXPECT_TRUE(main_document_observer.WaitForRequestStart());
    main_document_observer.ResumeNavigation();  // Send the request.

    main_document_response.WaitForRequest();
    main_document_response.Send(
        "HTTP/1.1 200 OK\r\n"
        "Connection: close\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "\r\n");

    EXPECT_TRUE(main_document_observer.WaitForResponse());
    main_document_observer.ResumeNavigation();  // Commit the navigation.
  }

  // 2) A browser-initiated javascript-url navigation happens.
  {
    GURL javascript_url(
        "javascript:window.domAutomationController.send('done')");
    shell()->LoadURL(javascript_url);
    DOMMessageQueue dom_message_queue(WebContents::FromRenderFrameHost(
        shell()->web_contents()->GetMainFrame()));
    std::string done;
    EXPECT_TRUE(dom_message_queue.WaitForMessage(&done));
    EXPECT_EQ("\"done\"", done);
  }

  // 3) The end of the response is issued. The renderer must be able to receive
  //    it.
  {
    const base::string16 document_loaded_title =
        base::ASCIIToUTF16("document loaded");
    TitleWatcher watcher(shell()->web_contents(), document_loaded_title);
    main_document_response.Send(
        "<script>"
        "   window.onload = function(){"
        "     document.title = 'document loaded'"
        "   }"
        "</script>");
    main_document_response.Done();
    EXPECT_EQ(document_loaded_title, watcher.WaitAndGetTitle());
  }
}

// Test that a same-document browser-initiated navigation doesn't prevent a
// document from loading. See https://crbug.com/769645.
IN_PROC_BROWSER_TEST_F(
    ContentBrowserTest,
    SameDocumentBrowserInitiatedNavigationWhileDocumentIsLoading) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/main_document");
  EXPECT_TRUE(embedded_test_server()->Start());

  // 1) Load a new document. It reaches the ReadyToCommit stage and then is slow
  //    to load.
  GURL url(embedded_test_server()->GetURL("/main_document"));
  TestNavigationManager observer_new_document(shell()->web_contents(), url);
  shell()->LoadURL(url);

  // The navigation starts
  EXPECT_TRUE(observer_new_document.WaitForRequestStart());
  observer_new_document.ResumeNavigation();

  // The server sends the first part of the response and waits.
  response.WaitForRequest();
  response.Send(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n"
      "<html>"
      "  <body>"
      "    <div id=\"anchor\"></div>"
      "    <script>"
      "      domAutomationController.send('First part received')"
      "    </script>");

  // The browser reaches the ReadyToCommit stage.
  EXPECT_TRUE(observer_new_document.WaitForResponse());
  RenderFrameHostImpl* main_rfh = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetMainFrame());
  DOMMessageQueue dom_message_queue(WebContents::FromRenderFrameHost(main_rfh));
  observer_new_document.ResumeNavigation();

  // Wait for the renderer to load the first part of the response.
  std::string first_part_received;
  EXPECT_TRUE(dom_message_queue.WaitForMessage(&first_part_received));
  EXPECT_EQ("\"First part received\"", first_part_received);

  // 2) In the meantime, a browser-initiated same-document navigation commits.
  GURL anchor_url(url.spec() + "#anchor");
  TestNavigationManager observer_same_document(shell()->web_contents(),
                                               anchor_url);
  shell()->LoadURL(anchor_url);
  observer_same_document.WaitForNavigationFinished();

  // 3) The last part of the response is received.
  response.Send(
      "    <script>"
      "      domAutomationController.send('Second part received')"
      "    </script>"
      "  </body>"
      "</html>");
  response.Done();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // The renderer should be able to load the end of the response.
  std::string second_part_received;
  EXPECT_TRUE(dom_message_queue.WaitForMessage(&second_part_received));
  EXPECT_EQ("\"Second part received\"", second_part_received);
}

namespace {

// Allows injecting a fake, test-provided |interface_provider_request| into
// DidCommitProvisionalLoad messages in a given |web_contents| instead of the
// real one coming from the renderer process.
class ScopedFakeInterfaceProviderRequestInjector
    : public DidCommitNavigationInterceptor {
 public:
  explicit ScopedFakeInterfaceProviderRequestInjector(WebContents* web_contents)
      : DidCommitNavigationInterceptor(web_contents) {}
  ~ScopedFakeInterfaceProviderRequestInjector() override = default;

  // Sets the fake InterfaceProvider |request| to inject into the next incoming
  // DidCommitProvisionalLoad message.
  void set_fake_request_for_next_commit(
      service_manager::mojom::InterfaceProviderRequest request) {
    next_fake_request_ = std::move(request);
  }

  const GURL& url_of_last_commit() const { return url_of_last_commit_; }

  const service_manager::mojom::InterfaceProviderRequest&
  original_request_of_last_commit() const {
    return original_request_of_last_commit_;
  }

 protected:
  bool WillProcessDidCommitNavigation(
      RenderFrameHost* render_frame_host,
      NavigationRequest* navigation_request,
      ::FrameHostMsg_DidCommitProvisionalLoad_Params* params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr* interface_params)
      override {
    url_of_last_commit_ = params->url;
    if (*interface_params) {
      original_request_of_last_commit_ =
          std::move((*interface_params)->interface_provider_request);
      (*interface_params)->interface_provider_request =
          std::move(next_fake_request_);
    }
    return true;
  }

 private:
  service_manager::mojom::InterfaceProviderRequest next_fake_request_;
  service_manager::mojom::InterfaceProviderRequest
      original_request_of_last_commit_;
  GURL url_of_last_commit_;

  DISALLOW_COPY_AND_ASSIGN(ScopedFakeInterfaceProviderRequestInjector);
};

// Monitors the |document_scoped_interface_provider_binding_| of the given
// |render_frame_host| for incoming interface requests for |interface_name|, and
// invokes |callback| synchronously just before such a request would be
// dispatched.
class ScopedInterfaceRequestMonitor
    : public service_manager::mojom::InterfaceProviderInterceptorForTesting {
 public:
  ScopedInterfaceRequestMonitor(RenderFrameHost* render_frame_host,
                                base::StringPiece interface_name,
                                base::RepeatingClosure callback)
      : rfhi_(static_cast<RenderFrameHostImpl*>(render_frame_host)),
        impl_(binding().SwapImplForTesting(this)),
        interface_name_(interface_name),
        request_callback_(callback) {}

  ~ScopedInterfaceRequestMonitor() override {
    auto* old_impl = binding().SwapImplForTesting(impl_);
    DCHECK_EQ(old_impl, this);
  }

 protected:
  // service_manager::mojom::InterfaceProviderInterceptorForTesting:
  service_manager::mojom::InterfaceProvider* GetForwardingInterface() override {
    return impl_;
  }

  void GetInterface(const std::string& interface_name,
                    mojo::ScopedMessagePipeHandle pipe) override {
    if (interface_name == interface_name_)
      request_callback_.Run();
    GetForwardingInterface()->GetInterface(interface_name, std::move(pipe));
  }

 private:
  mojo::Binding<service_manager::mojom::InterfaceProvider>& binding() {
    return rfhi_->document_scoped_interface_provider_binding_for_testing();
  }

  RenderFrameHostImpl* rfhi_;
  service_manager::mojom::InterfaceProvider* impl_;

  std::string interface_name_;
  base::RepeatingClosure request_callback_;

  DISALLOW_COPY_AND_ASSIGN(ScopedInterfaceRequestMonitor);
};

// Calls |callback| whenever a navigation finishes in |render_frame_host|.
class DidFinishNavigationObserver : public WebContentsObserver {
 public:
  DidFinishNavigationObserver(RenderFrameHost* render_frame_host,
                              base::RepeatingClosure callback)
      : WebContentsObserver(
            WebContents::FromRenderFrameHost(render_frame_host)),
        callback_(callback) {}

 protected:
  // WebContentsObserver:
  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    callback_.Run();
  }

 private:
  base::RepeatingClosure callback_;
  DISALLOW_COPY_AND_ASSIGN(DidFinishNavigationObserver);
};

}  // namespace

// For cross-document navigations, the DidCommitProvisionalLoad message from
// the renderer process will have its |interface_provider_request| argument set
// to the request end of a new InterfaceProvider interface connection that will
// be used by the newly committed document to access services exposed by the
// RenderFrameHost.
//
// This test verifies that even if that |interface_provider_request| already has
// pending interface requests, the RenderFrameHost binds the InterfaceProvider
// request in such a way that these pending interface requests are dispatched
// strictly after WebContentsObserver::DidFinishNavigation has fired, so that
// the requests will be served correctly in the security context of the newly
// committed document (i.e. GetLastCommittedURL/Origin will have been updated).
IN_PROC_BROWSER_TEST_F(
    RenderFrameHostImplBrowserTest,
    EarlyInterfaceRequestsFromNewDocumentDispatchedAfterNavigationFinished) {
  const GURL first_url(embedded_test_server()->GetURL("/title1.html"));
  const GURL second_url(embedded_test_server()->GetURL("/title2.html"));

  // Load a URL that maps to the same SiteInstance as the second URL, to make
  // sure the second navigation will not be cross-process.
  ASSERT_TRUE(NavigateToURL(shell(), first_url));

  // Prepare an InterfaceProviderRequest with pending interface requests.
  service_manager::mojom::InterfaceProviderPtr
      interface_provider_with_pending_request;
  service_manager::mojom::InterfaceProviderRequest
      interface_provider_request_with_pending_request =
          mojo::MakeRequest(&interface_provider_with_pending_request);
  mojo::Remote<mojom::FrameHostTestInterface> test_interface;
  interface_provider_with_pending_request->GetInterface(
      mojom::FrameHostTestInterface::Name_,
      test_interface.BindNewPipeAndPassReceiver().PassPipe());

  // Replace the |interface_provider_request| argument in the next
  // DidCommitProvisionalLoad message coming from the renderer with the
  // rigged |interface_provider_with_pending_request| from above.
  ScopedFakeInterfaceProviderRequestInjector injector(shell()->web_contents());
  injector.set_fake_request_for_next_commit(
      std::move(interface_provider_request_with_pending_request));

  // Expect that by the time the interface request for FrameHostTestInterface is
  // dispatched to the RenderFrameHost, WebContentsObserver::DidFinishNavigation
  // will have already been invoked.
  bool did_finish_navigation = false;
  auto* main_rfh = shell()->web_contents()->GetMainFrame();
  DidFinishNavigationObserver navigation_finish_observer(
      main_rfh, base::BindLambdaForTesting([&did_finish_navigation]() {
        did_finish_navigation = true;
      }));

  base::RunLoop wait_until_interface_request_is_dispatched;
  ScopedInterfaceRequestMonitor monitor(
      main_rfh, mojom::FrameHostTestInterface::Name_,
      base::BindLambdaForTesting([&]() {
        EXPECT_TRUE(did_finish_navigation);
        wait_until_interface_request_is_dispatched.Quit();
      }));

  // Start the same-process navigation.
  test::ScopedInterfaceFilterBypass filter_bypass;
  ASSERT_TRUE(NavigateToURL(shell(), second_url));
  EXPECT_EQ(main_rfh, shell()->web_contents()->GetMainFrame());
  EXPECT_EQ(second_url, injector.url_of_last_commit());
  EXPECT_TRUE(injector.original_request_of_last_commit().is_pending());

  // Wait until the interface request for FrameHostTestInterface is dispatched.
  wait_until_interface_request_is_dispatched.Run();
}

// The InterfaceProvider interface, which is used by the RenderFrame to access
// Mojo services exposed by the RenderFrameHost, is not Channel-associated,
// thus not synchronized with navigation IPC messages. As a result, when the
// renderer commits a load, the DidCommitProvisional message might be at race
// with GetInterface messages, for example, an interface request issued by the
// previous document in its unload handler might arrive to the browser process
// just a moment after DidCommitProvisionalLoad.
//
// This test verifies that even if there is such a last-second GetInterface
// message originating from the previous document, it is no longer serviced.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       LateInterfaceRequestsFromOldDocumentNotDispatched) {
  const GURL first_url(embedded_test_server()->GetURL("/title1.html"));
  const GURL second_url(embedded_test_server()->GetURL("/title2.html"));

  // Prepare an InterfaceProviderRequest with no pending requests.
  service_manager::mojom::InterfaceProviderPtr interface_provider;
  service_manager::mojom::InterfaceProviderRequest interface_provider_request =
      mojo::MakeRequest(&interface_provider);

  // Set up a cunning mechnism to replace the |interface_provider_request|
  // argument in next DidCommitProvisionalLoad message with the rigged
  // |interface_provider_request| from above, whose client end is controlled by
  // this test; then trigger a navigation.
  {
    ScopedFakeInterfaceProviderRequestInjector injector(
        shell()->web_contents());
    test::ScopedInterfaceFilterBypass filter_bypass;
    injector.set_fake_request_for_next_commit(
        std::move(interface_provider_request));

    ASSERT_TRUE(NavigateToURL(shell(), first_url));
    ASSERT_EQ(first_url, injector.url_of_last_commit());
    ASSERT_TRUE(injector.original_request_of_last_commit().is_pending());
  }

  // Prepare an interface receiver for FrameHostTestInterface.
  mojo::Remote<mojom::FrameHostTestInterface> test_interface;
  auto test_interface_receiver = test_interface.BindNewPipeAndPassReceiver();

  // Set up |dispatched_interface_request_callback| that would be invoked if the
  // interface receiver for FrameHostTestInterface was ever dispatched to the
  // RenderFrameHostImpl.
  base::MockCallback<base::RepeatingClosure>
      dispatched_interface_request_callback;
  auto* main_rfh = shell()->web_contents()->GetMainFrame();
  ScopedInterfaceRequestMonitor monitor(
      main_rfh, mojom::FrameHostTestInterface::Name_,
      dispatched_interface_request_callback.Get());

  // Set up the |test_interface request| to arrive on the InterfaceProvider
  // connection corresponding to the old document in the middle of the firing of
  // WebContentsObserver::DidFinishNavigation.
  // TODO(engedy): Should we PostTask() this instead just before synchronously
  // invoking DidCommitProvisionalLoad?
  //
  // Also set up |navigation_finished_callback| to be invoked afterwards, as a
  // sanity check to ensure that the request injection is actually executed.
  base::MockCallback<base::RepeatingClosure> navigation_finished_callback;
  DidFinishNavigationObserver navigation_finish_observer(
      main_rfh, base::BindLambdaForTesting([&]() {
        interface_provider->GetInterface(mojom::FrameHostTestInterface::Name_,
                                         test_interface_receiver.PassPipe());
        std::move(navigation_finished_callback).Run();
      }));

  // The InterfaceProvider connection that semantically belongs to the old
  // document, but whose client end is actually controlled by this test, should
  // still be alive and well.
  ASSERT_TRUE(test_interface.is_bound());
  ASSERT_TRUE(test_interface.is_connected());

  base::RunLoop run_loop;
  test_interface.set_disconnect_handler(run_loop.QuitWhenIdleClosure());

  // Expect that the GetInterface message will never be dispatched, but the
  // DidFinishNavigation callback will be invoked.
  EXPECT_CALL(dispatched_interface_request_callback, Run()).Times(0);
  EXPECT_CALL(navigation_finished_callback, Run());

  // Start the same-process navigation.
  ASSERT_TRUE(NavigateToURL(shell(), second_url));

  // Wait for a connection error on the |test_interface| as a signal, after
  // which it can be safely assumed that no GetInterface message will ever be
  // dispatched from that old InterfaceConnection.
  run_loop.Run();

  EXPECT_FALSE(test_interface.is_connected());
}

// Test the edge case where the `window` global object asssociated with the
// initial empty document is re-used for document corresponding to the first
// real committed load. This happens when the security origins of the two
// documents are the same. We do not want to recalculate this in the browser
// process, however, so for the first commit we leave it up to the renderer
// whether it wants to replace the InterfaceProvider connection or not.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       InterfaceProviderRequestIsOptionalForFirstCommit) {
  const GURL main_frame_url(embedded_test_server()->GetURL("/title1.html"));
  const GURL subframe_url(embedded_test_server()->GetURL("/title2.html"));

  service_manager::mojom::InterfaceProviderPtr interface_provider;
  auto stub_interface_provider_request = mojo::MakeRequest(&interface_provider);
  service_manager::mojom::InterfaceProviderRequest
      null_interface_provider_request(nullptr);

  for (auto* interface_provider_request :
       {&stub_interface_provider_request, &null_interface_provider_request}) {
    SCOPED_TRACE(interface_provider_request->is_pending());

    ASSERT_TRUE(NavigateToURL(shell(), main_frame_url));

    ScopedFakeInterfaceProviderRequestInjector injector(
        shell()->web_contents());
    injector.set_fake_request_for_next_commit(
        std::move(*interface_provider_request));

    // Must set 'src` before adding the iframe element to the DOM, otherwise it
    // will load `about:blank` as the first real load instead of |subframe_url|.
    // See: https://crbug.com/778318.
    //
    // Note that the child frame will first cycle through loading the initial
    // empty document regardless of when/how/if the `src` attribute is set.
    const auto script = base::StringPrintf(
        "let f = document.createElement(\"iframe\");"
        "f.src=\"%s\"; "
        "document.body.append(f);",
        subframe_url.spec().c_str());
    ASSERT_TRUE(ExecuteScript(shell(), script));

    WaitForLoadStop(shell()->web_contents());

    FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                              ->GetFrameTree()
                              ->root();
    ASSERT_EQ(1u, root->child_count());
    FrameTreeNode* child = root->child_at(0u);

    EXPECT_FALSE(injector.original_request_of_last_commit().is_pending());
    EXPECT_TRUE(child->has_committed_real_load());
    EXPECT_EQ(subframe_url, child->current_url());
  }
}

// Regression test for https://crbug.com/821022.
//
// Test the edge case of the above, namely, where the following commits take
// place in a subframe embedded into a document at `http://foo.com/`:
//
//  1) the initial empty document (`about:blank`)
//  2) `about:blank#ref`
//  3) `http://foo.com`
//
// Here, (2) should classify as a same-document navigation, and (3) should be
// considered the first real load. Because the first real load is same-origin
// with the initial empty document, the latter's `window` global object
// asssociated with the initial empty document is re-used for document
// corresponding to the first real committed load.
IN_PROC_BROWSER_TEST_F(
    RenderFrameHostImplBrowserTest,
    InterfaceProviderRequestNotPresentForFirstRealLoadAfterAboutBlankWithRef) {
  const GURL kMainFrameURL(embedded_test_server()->GetURL("/title1.html"));
  const GURL kSubframeURLTwo("about:blank#ref");
  const GURL kSubframeURLThree(embedded_test_server()->GetURL("/title2.html"));
  const auto kNavigateToOneThenTwoScript = base::StringPrintf(
      "var f = document.createElement(\"iframe\");"
      "f.src=\"%s\"; "
      "document.body.append(f);",
      kSubframeURLTwo.spec().c_str());
  const auto kNavigateToThreeScript =
      base::StringPrintf("f.src=\"%s\";", kSubframeURLThree.spec().c_str());

  ASSERT_TRUE(NavigateToURL(shell(), kMainFrameURL));

  // Trigger navigation (1) by creating a new subframe, and then trigger
  // navigation (2) by setting it's `src` attribute before adding it to the DOM.
  //
  // We must set 'src` before adding the iframe element to the DOM, otherwise it
  // will load `about:blank` as the first real load instead of
  // |kSubframeURLTwo|. See: https://crbug.com/778318.
  //
  // Note that the child frame will first cycle through loading the initial
  // empty document regardless of when/how/if the `src` attribute is set.

  ASSERT_TRUE(ExecuteScript(shell(), kNavigateToOneThenTwoScript));
  WaitForLoadStop(shell()->web_contents());

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  ASSERT_EQ(1u, root->child_count());
  FrameTreeNode* child = root->child_at(0u);

  EXPECT_FALSE(child->has_committed_real_load());
  EXPECT_EQ(kSubframeURLTwo, child->current_url());
  EXPECT_EQ(url::Origin::Create(kMainFrameURL), child->current_origin());

  // Set the `src` attribute again to trigger navigation (3).

  TestFrameNavigationObserver commit_observer(child->current_frame_host());
  ScopedFakeInterfaceProviderRequestInjector injector(shell()->web_contents());
  injector.set_fake_request_for_next_commit(nullptr);

  ASSERT_TRUE(ExecuteScript(shell(), kNavigateToThreeScript));
  commit_observer.WaitForCommit();

  EXPECT_FALSE(injector.original_request_of_last_commit().is_pending());

  EXPECT_TRUE(child->has_committed_real_load());
  EXPECT_EQ(kSubframeURLThree, child->current_url());
  EXPECT_EQ(url::Origin::Create(kMainFrameURL), child->current_origin());
}

namespace {
void CheckURLOriginAndNetworkIsolationKey(
    FrameTreeNode* node,
    GURL url,
    url::Origin origin,
    net::NetworkIsolationKey network_isolation_key) {
  EXPECT_EQ(url, node->current_url());
  EXPECT_EQ(origin, node->current_origin());
  EXPECT_EQ(network_isolation_key,
            node->current_frame_host()->network_isolation_key());
}
}  // namespace

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       NetworkIsolationKeyInitialEmptyDocumentIframe) {
  GURL main_frame_url(embedded_test_server()->GetURL("/title1.html"));
  url::Origin main_frame_origin = url::Origin::Create(main_frame_url);
  net::NetworkIsolationKey expected_main_frame_key =
      net::NetworkIsolationKey(main_frame_origin, main_frame_origin);

  GURL subframe_url_one("about:blank");
  GURL subframe_url_two("about:blank#foo");
  GURL subframe_url_three(
      embedded_test_server()->GetURL("foo.com", "/title2.html"));
  url::Origin subframe_origin_three = url::Origin::Create(subframe_url_three);
  net::NetworkIsolationKey expected_subframe_key_three =
      net::NetworkIsolationKey(main_frame_origin, subframe_origin_three);

  // Main frame navigation.
  ASSERT_TRUE(NavigateToURL(shell(), main_frame_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  CheckURLOriginAndNetworkIsolationKey(root, main_frame_url, main_frame_origin,
                                       expected_main_frame_key);

  // Create iframe.
  ASSERT_TRUE(ExecuteScript(shell(), R"(
      var f = document.createElement('iframe');
      f.id = 'myiframe';
      document.body.append(f);
  )"));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  ASSERT_EQ(1u, root->child_count());
  FrameTreeNode* child = root->child_at(0u);
  CheckURLOriginAndNetworkIsolationKey(
      child, subframe_url_one, main_frame_origin, expected_main_frame_key);
  EXPECT_EQ(root->current_frame_host()->GetProcess(),
            child->current_frame_host()->GetProcess());

  // Same-document navigation of iframe.
  ASSERT_TRUE(ExecuteScript(shell(), R"(
      let iframe = document.querySelector('#myiframe');
      iframe.contentWindow.location.hash = 'foo';
  )"));

  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  CheckURLOriginAndNetworkIsolationKey(
      child, subframe_url_two, main_frame_origin, expected_main_frame_key);
  EXPECT_EQ(root->current_frame_host()->GetProcess(),
            child->current_frame_host()->GetProcess());

  // Cross-document navigation of iframe.
  TestFrameNavigationObserver commit_observer(child->current_frame_host());
  std::string subframe_script_three = JsReplace(
      "iframe = document.querySelector('#myiframe');"
      "iframe.contentWindow.location.href = $1;",
      subframe_url_three);
  ASSERT_TRUE(ExecuteScript(shell(), subframe_script_three));
  commit_observer.WaitForCommit();

  CheckURLOriginAndNetworkIsolationKey(child, subframe_url_three,
                                       subframe_origin_three,
                                       expected_subframe_key_three);
  if (AreAllSitesIsolatedForTesting()) {
    EXPECT_NE(root->current_frame_host()->GetProcess(),
              child->current_frame_host()->GetProcess());
  }
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       NetworkIsolationKeyInitialEmptyDocumentPopup) {
  GURL main_frame_url(embedded_test_server()->GetURL("/title1.html"));
  url::Origin main_frame_origin = url::Origin::Create(main_frame_url);
  net::NetworkIsolationKey expected_main_frame_key =
      net::NetworkIsolationKey(main_frame_origin, main_frame_origin);

  GURL popup_url_one("about:blank");
  GURL popup_url_two("about:blank#foo");
  GURL popup_url_three(
      embedded_test_server()->GetURL("foo.com", "/title2.html"));
  url::Origin popup_origin_three = url::Origin::Create(popup_url_three);
  net::NetworkIsolationKey expected_popup_key_three =
      net::NetworkIsolationKey(popup_origin_three, popup_origin_three);

  // Main frame navigation.
  ASSERT_TRUE(NavigateToURL(shell(), main_frame_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  CheckURLOriginAndNetworkIsolationKey(root, main_frame_url, main_frame_origin,
                                       expected_main_frame_key);

  // Create popup.
  WebContentsAddedObserver popup_observer;
  ASSERT_TRUE(ExecuteScript(shell(), "var w = window.open();"));
  WebContents* popup = popup_observer.GetWebContents();

  FrameTreeNode* popup_frame =
      static_cast<RenderFrameHostImpl*>(popup->GetMainFrame())
          ->frame_tree_node();
  CheckURLOriginAndNetworkIsolationKey(
      popup_frame, popup_url_one, main_frame_origin, expected_main_frame_key);
  EXPECT_EQ(root->current_frame_host()->GetProcess(),
            popup_frame->current_frame_host()->GetProcess());

  // Same-document navigation of popup.
  ASSERT_TRUE(ExecuteScript(shell(), "w.location.hash = 'foo';"));
  EXPECT_TRUE(WaitForLoadStop(popup));

  CheckURLOriginAndNetworkIsolationKey(
      popup_frame, popup_url_two, main_frame_origin, expected_main_frame_key);
  EXPECT_EQ(root->current_frame_host()->GetProcess(),
            popup_frame->current_frame_host()->GetProcess());

  // Cross-document navigation of popup.
  TestFrameNavigationObserver commit_observer(
      popup_frame->current_frame_host());
  ASSERT_TRUE(ExecuteScript(
      shell(), JsReplace("w.location.href = $1;", popup_url_three)));
  commit_observer.WaitForCommit();

  CheckURLOriginAndNetworkIsolationKey(popup_frame, popup_url_three,
                                       popup_origin_three,
                                       expected_popup_key_three);
  if (AreAllSitesIsolatedForTesting()) {
    EXPECT_NE(root->current_frame_host()->GetProcess(),
              popup_frame->current_frame_host()->GetProcess());
  }
}

// Verify that if the UMA histograms are correctly recording if interface
// provider requests are getting dropped because they racily arrive from the
// previously active document (after the next navigation already committed).
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       DroppedInterfaceRequestCounter) {
  const GURL kUrl1(embedded_test_server()->GetURL("/title1.html"));
  const GURL kUrl2(embedded_test_server()->GetURL("/title2.html"));
  const GURL kUrl3(embedded_test_server()->GetURL("/title3.html"));
  const GURL kUrl4(embedded_test_server()->GetURL("/empty.html"));

  // The 31-bit hash of the string "content.mojom.MojoWebTestHelper".
  const int32_t kHashOfContentMojomMojoWebTestHelper = 0x77b7b3d6;

  // Client ends of the fake interface provider requests injected for the first
  // and second navigations.
  service_manager::mojom::InterfaceProviderPtr interface_provider_1;
  service_manager::mojom::InterfaceProviderPtr interface_provider_2;

  base::RunLoop wait_until_connection_error_loop_1;
  base::RunLoop wait_until_connection_error_loop_2;

  {
    ScopedFakeInterfaceProviderRequestInjector injector(
        shell()->web_contents());
    injector.set_fake_request_for_next_commit(
        mojo::MakeRequest(&interface_provider_1));
    interface_provider_1.set_connection_error_handler(
        wait_until_connection_error_loop_1.QuitClosure());
    ASSERT_TRUE(NavigateToURL(shell(), kUrl1));
  }

  {
    ScopedFakeInterfaceProviderRequestInjector injector(
        shell()->web_contents());
    injector.set_fake_request_for_next_commit(
        mojo::MakeRequest(&interface_provider_2));
    interface_provider_2.set_connection_error_handler(
        wait_until_connection_error_loop_2.QuitClosure());
    ASSERT_TRUE(NavigateToURL(shell(), kUrl2));
  }

  // Simulate two interface requests corresponding to the first navigation
  // arrived after the second navigation was committed, hence were dropped.
  interface_provider_1->GetInterface(mojom::MojoWebTestHelper::Name_,
                                     CreateDisconnectedMessagePipeHandle());
  interface_provider_1->GetInterface(mojom::MojoWebTestHelper::Name_,
                                     CreateDisconnectedMessagePipeHandle());

  // RFHI destroys the DroppedInterfaceRequestLogger from navigation `n` on
  // navigation `n+2`. Histrograms are recorded on destruction, there should
  // be a single sample indicating two requests having been dropped for the
  // first URL.
  {
    base::HistogramTester histogram_tester;
    ASSERT_TRUE(NavigateToURL(shell(), kUrl3));
    histogram_tester.ExpectUniqueSample(
        "RenderFrameHostImpl.DroppedInterfaceRequests", 2, 1);
    histogram_tester.ExpectUniqueSample(
        "RenderFrameHostImpl.DroppedInterfaceRequestName",
        kHashOfContentMojomMojoWebTestHelper, 2);
  }

  // Simulate one interface request dropped for the second URL.
  interface_provider_2->GetInterface(mojom::MojoWebTestHelper::Name_,
                                     CreateDisconnectedMessagePipeHandle());

  // A final navigation should record the sample from the second URL.
  {
    base::HistogramTester histogram_tester;
    ASSERT_TRUE(NavigateToURL(shell(), kUrl4));
    histogram_tester.ExpectUniqueSample(
        "RenderFrameHostImpl.DroppedInterfaceRequests", 1, 1);
    histogram_tester.ExpectUniqueSample(
        "RenderFrameHostImpl.DroppedInterfaceRequestName",
        kHashOfContentMojomMojoWebTestHelper, 1);
  }

  // Both the DroppedInterfaceRequestLogger for the first and second URLs are
  // destroyed -- even more interfacerequests should not cause any crashes.
  interface_provider_1->GetInterface(mojom::MojoWebTestHelper::Name_,
                                     CreateDisconnectedMessagePipeHandle());
  interface_provider_2->GetInterface(mojom::MojoWebTestHelper::Name_,
                                     CreateDisconnectedMessagePipeHandle());

  // The interface connections should be broken.
  wait_until_connection_error_loop_1.Run();
  wait_until_connection_error_loop_2.Run();
}

// Regression test for https://crbug.com/852350
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       GetCanonicalUrlAfterRendererCrash) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GetTestUrl("render_frame_host", "beforeunload.html")));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame =
      static_cast<RenderFrameHostImpl*>(wc->GetMainFrame());

  // Make the renderer crash.
  RenderProcessHost* renderer_process = main_frame->GetProcess();
  RenderProcessHostWatcher crash_observer(
      renderer_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  renderer_process->Shutdown(0);
  crash_observer.Wait();

  main_frame->GetCanonicalUrlForSharing(base::DoNothing());
}

// This test makes sure that when a blocked frame commits with a different URL,
// it doesn't lead to a leaked NavigationHandle. This is a regression test for
// https://crbug.com/872803.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       ErrorPagesShouldntLeakNavigationHandles) {
  GURL main_url(embedded_test_server()->GetURL(
      "foo.com", "/frame_tree/page_with_one_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  GURL blocked_url(embedded_test_server()->GetURL(
      "blocked.com", "/frame-ancestors-none.html"));
  WebContents* web_contents = shell()->web_contents();
  NavigationHandleObserver navigation_observer(web_contents, blocked_url);
  EXPECT_TRUE(NavigateIframeToURL(web_contents, "child0", blocked_url));

  // Verify that the NavigationHandle / NavigationRequest didn't leak.
  RenderFrameHostImpl* frame = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetAllFrames()[1]);
  EXPECT_FALSE(frame->HasPendingCommitNavigation());

  // TODO(lukasza, clamy): https://crbug.com/784904: Verify that
  // WebContentsObserver::DidFinishNavigation was called with the same
  // NavigationHandle as WebContentsObserver::DidStartNavigation. This requires
  // properly matching the commit IPC to the NavigationHandle (ignoring that
  // their URLs do not match - matching instead using navigation id or mojo
  // interface identity).

  // TODO(https://crbug.com/759184): Verify CSP frame-ancestors in the browser
  // process. Currently, this is done by the renderer process, which commits an
  // empty document with success instead.
  //  EXPECT_EQ(blocked_url, navigation_observer.last_committed_url());
  //  EXPECT_EQ(net::ERR_BLOCKED_BY_RESPONSE,
  //            navigation_obsever.net_error_code());
  EXPECT_TRUE(navigation_observer.has_committed());
  EXPECT_FALSE(navigation_observer.is_error());
  EXPECT_EQ(GURL("data:,"), navigation_observer.last_committed_url());
  EXPECT_EQ(net::Error::OK, navigation_observer.net_error_code());
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       BeforeUnloadDialogSuppressedForDiscard) {
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  TestJavaScriptDialogManager dialog_manager;
  wc->SetDelegate(&dialog_manager);

  EXPECT_TRUE(NavigateToURL(
      shell(), GetTestUrl("render_frame_host", "beforeunload.html")));
  // Disable the hang monitor, otherwise there will be a race between the
  // beforeunload dialog and the beforeunload hang timer.
  wc->GetMainFrame()->DisableBeforeUnloadHangMonitorForTesting();

  // Give the page a user gesture so javascript beforeunload works, and then
  // dispatch a before unload with discard as a reason. This should return
  // without any dialog being seen.
  wc->GetMainFrame()->ExecuteJavaScriptWithUserGestureForTests(
      base::string16());
  wc->GetMainFrame()->DispatchBeforeUnload(
      RenderFrameHostImpl::BeforeUnloadType::DISCARD, false);
  dialog_manager.Wait();
  EXPECT_EQ(0, dialog_manager.num_beforeunload_dialogs_seen());
  EXPECT_EQ(1, dialog_manager.num_beforeunload_fired_seen());
  EXPECT_FALSE(dialog_manager.proceed());

  wc->SetDelegate(nullptr);
  wc->SetJavaScriptDialogManagerForTesting(nullptr);
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       PendingDialogMakesDiscardUnloadReturnFalse) {
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  TestJavaScriptDialogManager dialog_manager;
  wc->SetDelegate(&dialog_manager);

  EXPECT_TRUE(NavigateToURL(
      shell(), GetTestUrl("render_frame_host", "beforeunload.html")));
  // Disable the hang monitor, otherwise there will be a race between the
  // beforeunload dialog and the beforeunload hang timer.
  wc->GetMainFrame()->DisableBeforeUnloadHangMonitorForTesting();

  // Give the page a user gesture so javascript beforeunload works, and then
  // dispatch a before unload with discard as a reason. This should return
  // without any dialog being seen.
  wc->GetMainFrame()->ExecuteJavaScriptWithUserGestureForTests(
      base::string16());

  // Launch an alert javascript dialog. This pending dialog should block a
  // subsequent discarding before unload request.
  wc->GetMainFrame()->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16("setTimeout(function(){alert('hello');}, 10);"),
      base::NullCallback());
  dialog_manager.Wait();
  EXPECT_EQ(0, dialog_manager.num_beforeunload_dialogs_seen());
  EXPECT_EQ(0, dialog_manager.num_beforeunload_fired_seen());

  // Dispatch a before unload request while the first is still blocked
  // on the dialog, and expect it to return false immediately (synchronously).
  wc->GetMainFrame()->DispatchBeforeUnload(
      RenderFrameHostImpl::BeforeUnloadType::DISCARD, false);
  dialog_manager.Wait();
  EXPECT_EQ(0, dialog_manager.num_beforeunload_dialogs_seen());
  EXPECT_EQ(1, dialog_manager.num_beforeunload_fired_seen());
  EXPECT_FALSE(dialog_manager.proceed());

  // Clear the existing javascript dialog so that the associated IPC message
  // doesn't leak.
  dialog_manager.Run(true, base::string16());

  wc->SetDelegate(nullptr);
  wc->SetJavaScriptDialogManagerForTesting(nullptr);
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       NotifiesProcessHostOfAudibleAudio) {
  const auto RunPostedTasks = []() {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  run_loop.QuitClosure());
    run_loop.Run();
  };

  // Note: Just using the beforeunload.html test document to spin-up a
  // renderer. Any document will do.
  EXPECT_TRUE(NavigateToURL(
      shell(), GetTestUrl("render_frame_host", "beforeunload.html")));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  auto* frame = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetMainFrame());
  auto* process = static_cast<RenderProcessHostImpl*>(frame->GetProcess());
  ASSERT_EQ(0, process->get_media_stream_count_for_testing());

  // Audible audio output should cause the media stream count to increment.
  frame->OnAudibleStateChanged(true);
  RunPostedTasks();
  EXPECT_EQ(1, process->get_media_stream_count_for_testing());

  // Silence should cause the media stream count to decrement.
  frame->OnAudibleStateChanged(false);
  RunPostedTasks();
  EXPECT_EQ(0, process->get_media_stream_count_for_testing());

  // Start audible audio output again, and then crash the renderer. Expect the
  // media stream count to be zero after the crash.
  frame->OnAudibleStateChanged(true);
  RunPostedTasks();
  EXPECT_EQ(1, process->get_media_stream_count_for_testing());
  RenderProcessHostWatcher crash_observer(
      process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  process->Shutdown(0);
  crash_observer.Wait();
  RunPostedTasks();
  EXPECT_EQ(0, process->get_media_stream_count_for_testing());
}

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
// ChromeOS and Linux failures are tracked in https://crbug.com/954217
#define MAYBE_VisibilityScrolledOutOfView DISABLED_VisibilityScrolledOutOfView
#else
#define MAYBE_VisibilityScrolledOutOfView VisibilityScrolledOutOfView
#endif
// Test that a frame is visible/hidden depending on its WebContents visibility
// state.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       MAYBE_VisibilityScrolledOutOfView) {
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  GURL main_frame(embedded_test_server()->GetURL("/iframe_out_of_view.html"));
  GURL child_url(embedded_test_server()->GetURL("/hello.html"));

  // This will set up the page frame tree as A(A1()).
  ASSERT_TRUE(NavigateToURL(shell(), main_frame));
  FrameTreeNode* root = web_contents->GetFrameTree()->root();
  FrameTreeNode* nested_iframe_node = root->child_at(0);
  NavigateFrameToURL(nested_iframe_node, child_url);

  ASSERT_EQ(blink::mojom::FrameVisibility::kRenderedOutOfViewport,
            nested_iframe_node->current_frame_host()->visibility());
}

// Test that a frame is visible/hidden depending on its WebContents visibility
// state.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest, VisibilityChildInView) {
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  GURL main_frame(embedded_test_server()->GetURL("/iframe_clipped.html"));
  GURL child_url(embedded_test_server()->GetURL("/hello.html"));

  // This will set up the page frame tree as A(A1()).
  ASSERT_TRUE(NavigateToURL(shell(), main_frame));
  FrameTreeNode* root = web_contents->GetFrameTree()->root();
  FrameTreeNode* nested_iframe_node = root->child_at(0);
  NavigateFrameToURL(nested_iframe_node, child_url);

  ASSERT_EQ(blink::mojom::FrameVisibility::kRenderedInViewport,
            nested_iframe_node->current_frame_host()->visibility());
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       OriginOfFreshFrame_Subframe_NavCancelledByDocWrite) {
  WebContents* web_contents = shell()->web_contents();
  NavigationController& controller = web_contents->GetController();
  GURL main_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), main_url));
  EXPECT_EQ(1, controller.GetEntryCount());
  url::Origin main_origin = url::Origin::Create(main_url);

  // document.open should cancel the cross-origin navigation to '/hung' and the
  // subframe should remain on the parent/initiator origin.
  const char kScriptTemplate[] = R"(
      const frame = document.createElement('iframe');
      frame.src = $1;
      document.body.appendChild(frame);

      const html = '<!DOCTYPE html><html><body>Hello world!</body></html>';
      const doc = frame.contentDocument;
      doc.open();
      doc.write(html);
      doc.close();

      frame.contentWindow.origin;
  )";
  GURL cross_site_url(embedded_test_server()->GetURL("bar.com", "/hung"));
  std::string script = JsReplace(kScriptTemplate, cross_site_url);
  EXPECT_EQ(main_origin.Serialize(), EvalJs(web_contents, script));

  // The subframe navigation should be cancelled and therefore shouldn't
  // contribute an extra history entry.
  EXPECT_EQ(1, controller.GetEntryCount());

  // Browser-side origin should match the renderer-side origin.
  // See also https://crbug.com/932067.
  ASSERT_EQ(2u, web_contents->GetAllFrames().size());
  RenderFrameHost* subframe = web_contents->GetAllFrames()[1];
  EXPECT_EQ(main_origin, subframe->GetLastCommittedOrigin());
}

class RenderFrameHostCreatedObserver : public WebContentsObserver {
 public:
  explicit RenderFrameHostCreatedObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  RenderFrameHost* Wait() {
    if (!new_frame_)
      run_loop_.Run();

    return new_frame_;
  }

 private:
  void RenderFrameCreated(RenderFrameHost* render_frame_host) override {
    new_frame_ = render_frame_host;
    run_loop_.Quit();
  }

  base::RunLoop run_loop_;
  RenderFrameHost* new_frame_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameHostCreatedObserver);
};

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       OriginOfFreshFrame_SandboxedSubframe) {
  WebContents* web_contents = shell()->web_contents();
  NavigationController& controller = web_contents->GetController();
  GURL main_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), main_url));
  EXPECT_EQ(1, controller.GetEntryCount());
  url::Origin main_origin = url::Origin::Create(main_url);

  // Navigate a sandboxed frame to a cross-origin '/hung'.
  RenderFrameHostCreatedObserver subframe_observer(web_contents);
  const char kScriptTemplate[] = R"(
      const frame = document.createElement('iframe');
      frame.sandbox = 'allow-scripts';
      frame.src = $1;
      document.body.appendChild(frame);
  )";
  GURL cross_site_url(embedded_test_server()->GetURL("bar.com", "/hung"));
  std::string script = JsReplace(kScriptTemplate, cross_site_url);
  EXPECT_TRUE(ExecJs(web_contents, script));

  // Wait for a new subframe, but ignore the frame returned by
  // |subframe_observer| (it might be the speculative one, not the current one).
  subframe_observer.Wait();
  ASSERT_EQ(2u, web_contents->GetAllFrames().size());
  RenderFrameHost* subframe = web_contents->GetAllFrames()[1];

  // The browser-side origin of the *sandboxed* subframe should be set to an
  // *opaque* origin (with the parent's origin as the precursor origin).
  EXPECT_TRUE(subframe->GetLastCommittedOrigin().opaque());
  EXPECT_EQ(
      main_origin.GetTupleOrPrecursorTupleIfOpaque(),
      subframe->GetLastCommittedOrigin().GetTupleOrPrecursorTupleIfOpaque());

  // Note that the test cannot check the renderer-side origin of the frame:
  // - Scripts cannot be executed before the frame commits,
  // - The parent cannot document.write into the *sandboxed* frame.
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       OriginOfFreshFrame_Subframe_AboutBlankAndThenDocWrite) {
  WebContents* web_contents = shell()->web_contents();
  NavigationController& controller = web_contents->GetController();
  GURL main_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), main_url));
  EXPECT_EQ(1, controller.GetEntryCount());
  url::Origin main_origin = url::Origin::Create(main_url);

  // Create a new about:blank subframe and document.write into it.
  TestNavigationObserver load_observer(web_contents);
  RenderFrameHostCreatedObserver subframe_observer(web_contents);
  const char kScript[] = R"(
      const frame = document.createElement('iframe');
      // Don't set |frame.src| - have the frame commit an initial about:blank.
      document.body.appendChild(frame);

      const html = '<!DOCTYPE html><html><body>Hello world!</body></html>';
      const doc = frame.contentDocument;
      doc.open();
      doc.write(html);
      doc.close();
  )";
  ExecuteScriptAsync(web_contents, kScript);

  // Wait for the new subframe to be created - this will be still before the
  // commit of about:blank.
  RenderFrameHost* subframe = subframe_observer.Wait();
  EXPECT_EQ(main_origin, subframe->GetLastCommittedOrigin());

  // Wait for the about:blank navigation to finish.
  load_observer.Wait();

  // The subframe commit to about:blank should not contribute an extra history
  // entry.
  EXPECT_EQ(1, controller.GetEntryCount());

  // Browser-side origin should match the renderer-side origin.
  // See also https://crbug.com/932067.
  ASSERT_EQ(2u, web_contents->GetAllFrames().size());
  RenderFrameHost* subframe2 = web_contents->GetAllFrames()[1];
  EXPECT_EQ(subframe, subframe2);  // No swaps are expected.
  EXPECT_EQ(main_origin, subframe2->GetLastCommittedOrigin());
  EXPECT_EQ(main_origin.Serialize(), EvalJs(subframe2, "window.origin"));
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       OriginOfFreshFrame_Popup_NavCancelledByDocWrite) {
  WebContents* web_contents = shell()->web_contents();
  GURL main_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), main_url));
  url::Origin main_origin = url::Origin::Create(main_url);

  // document.open should cancel the cross-origin navigation to '/hung' and the
  // popup should remain on the initiator origin.
  WebContentsAddedObserver popup_observer;
  const char kScriptTemplate[] = R"(
      var popup = window.open($1, 'popup');

      const html = '<!DOCTYPE html><html><body>Hello world!</body></html>';
      const doc = popup.document;
      doc.open();
      doc.write(html);
      doc.close();

      popup.origin;
  )";
  GURL cross_site_url(embedded_test_server()->GetURL("bar.com", "/hung"));
  std::string script = JsReplace(kScriptTemplate, cross_site_url);
  EXPECT_EQ(main_origin.Serialize(), EvalJs(web_contents, script));

  // Browser-side origin should match the renderer-side origin.
  // See also https://crbug.com/932067.
  WebContents* popup = popup_observer.GetWebContents();
  EXPECT_EQ(main_origin, popup->GetMainFrame()->GetLastCommittedOrigin());

  // The popup navigation should be cancelled and therefore shouldn't
  // contribute an extra history entry.
  EXPECT_EQ(0, popup->GetController().GetEntryCount());
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       OriginOfFreshFrame_Popup_AboutBlankAndThenDocWrite) {
  WebContents* web_contents = shell()->web_contents();
  GURL main_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), main_url));
  url::Origin main_origin = url::Origin::Create(main_url);

  // Create a new about:blank popup and document.write into it.
  WebContentsAddedObserver popup_observer;
  TestNavigationObserver load_observer(web_contents);
  const char kScript[] = R"(
      // Empty |url| argument means that the popup will commit an initial
      // about:blank.
      var popup = window.open('', 'popup');

      const html = '<!DOCTYPE html><html><body>Hello world!</body></html>';
      const doc = popup.document;
      doc.open();
      doc.write(html);
      doc.close();
  )";
  ExecuteScriptAsync(web_contents, kScript);

  // Wait for the new popup to be created (this will be before the popup commits
  // the initial about:blank page).
  WebContents* popup = popup_observer.GetWebContents();
  EXPECT_EQ(main_origin, popup->GetMainFrame()->GetLastCommittedOrigin());

  // A round-trip to the renderer process is an indirect way to wait for
  // DidCommitProvisionalLoad IPC for the initial about:blank page.
  // WaitForLoadStop cannot be used, because this commit won't raise
  // NOTIFICATION_LOAD_STOP.
  EXPECT_EQ(123, EvalJs(popup, "123"));
  EXPECT_EQ(main_origin, popup->GetMainFrame()->GetLastCommittedOrigin());

  // The about:blank navigation shouldn't contribute an extra history entry.
  EXPECT_EQ(0, popup->GetController().GetEntryCount());
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       AccessibilityIsRootIframe) {
  GURL main_url(
      embedded_test_server()->GetURL("foo.com", "/page_with_iframe.html"));
  ASSERT_TRUE(NavigateToURL(shell(), main_url));

  RenderFrameHostImpl* main_frame = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetMainFrame());
  EXPECT_TRUE(main_frame->AccessibilityIsMainFrame());

  ASSERT_EQ(1u, main_frame->child_count());
  RenderFrameHostImpl* iframe = main_frame->child_at(0)->current_frame_host();
  EXPECT_FALSE(iframe->AccessibilityIsMainFrame());
}

void FileChooserCallback(base::RunLoop* run_loop,
                         blink::mojom::FileChooserResultPtr result) {
  run_loop->Quit();
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       FileChooserAfterRfhDeath) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));
  auto* rfh = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetMainFrame());
  mojo::Remote<blink::mojom::FileChooser> chooser =
      rfh->BindFileChooserForTesting();

  // Kill the renderer process.
  RenderProcessHostWatcher crash_observer(
      rfh->GetProcess(), RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  rfh->GetProcess()->Shutdown(0);
  crash_observer.Wait();

  // Call FileChooser methods.  The browser process should not crash.
  base::RunLoop run_loop1;
  chooser->OpenFileChooser(blink::mojom::FileChooserParams::New(),
                           base::BindOnce(FileChooserCallback, &run_loop1));
  run_loop1.Run();

  base::RunLoop run_loop2;
  chooser->EnumerateChosenDirectory(
      base::FilePath(), base::BindOnce(FileChooserCallback, &run_loop2));
  run_loop2.Run();

  // Pass if this didn't crash.
}

// Verify that adding an <object> tag which resource is blocked by the network
// stack does not result in terminating the renderer process.
// See https://crbug.com/955777.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       ObjectTagBlockedResource) {
  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(
                                         "/page_with_object_fallback.html")));

  GURL object_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  std::unique_ptr<URLLoaderInterceptor> url_interceptor =
      URLLoaderInterceptor::SetupRequestFailForURL(object_url,
                                                   net::ERR_BLOCKED_BY_CLIENT);

  auto* rfh = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetMainFrame());
  TestNavigationObserver observer(shell()->web_contents());
  EXPECT_TRUE(ExecJs(shell()->web_contents(),
                     JsReplace("setUrl($1, true);", object_url)));
  observer.Wait();
  EXPECT_EQ(rfh->GetLastCommittedOrigin().Serialize(),
            EvalJs(shell()->web_contents(), "window.origin"));
}

// Regression test for crbug.com/953934. It shouldn't crash if we quickly remove
// an object element in the middle of its failing navigation.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       NoCrashOnRemoveObjectElementWithInvalidData) {
  GURL url = GetFileURL(
      FILE_PATH_LITERAL("remove_object_element_with_invalid_data.html"));

  RenderProcessHostWatcher crash_observer(
      shell()->web_contents(),
      RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);

  // This navigates to a page with an object element that will fail to load.
  // When document load event hits, it'll attempt to remove that object element.
  // This might happen while the object element's failed commit is underway.
  // To make sure we hit these conditions and that we don't exit the test too
  // soon, let's wait until the document.readyState finalizes. We don't really
  // care if that succeeds since, in the failing case, the renderer is crashing.
  EXPECT_TRUE(NavigateToURL(shell(), url));
  ignore_result(
      WaitForRenderFrameReady(shell()->web_contents()->GetMainFrame()));

  EXPECT_TRUE(crash_observer.did_exit_normally());
}

// Test deduplication of SameSite cookie deprecation messages.
// TODO(crbug.com/976475): This test is flaky.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       DISABLED_DeduplicateSameSiteCookieDeprecationMessages) {
#if defined(OS_ANDROID)
  // TODO(crbug.com/974701): This test is broken on Android that is
  // Marshmallow or older.
  if (base::android::BuildInfo::GetInstance()->sdk_int() <=
      base::android::SDK_VERSION_MARSHMALLOW) {
    return;
  }
#endif  // defined(OS_ANDROID)

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kCookieDeprecationMessages);

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  ConsoleObserverDelegate console_observer(web_contents, "*");
  web_contents->SetDelegate(&console_observer);

  // Test deprecation messages for SameSiteByDefault.
  // Set a cookie without SameSite on b.com, then access it in a cross-site
  // context.
  GURL url =
      embedded_test_server()->GetURL("b.com", "/set-cookie?nosamesite=1");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  ASSERT_EQ(0u, console_observer.messages().size());
  url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(),b())");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  // Only 1 message even though there are 2 cross-site iframes.
  EXPECT_EQ(1u, console_observer.messages().size());

  // Test deprecation messages for CookiesWithoutSameSiteMustBeSecure.
  // Set a cookie with SameSite=None but without Secure.
  url = embedded_test_server()->GetURL(
      "c.com", "/set-cookie?samesitenoneinsecure=1;SameSite=None");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  // The 1 message from before, plus the (different) message for setting the
  // SameSite=None insecure cookie.
  EXPECT_EQ(2u, console_observer.messages().size());
  // Another copy of the message appears because we have navigated.
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_EQ(3u, console_observer.messages().size());
  EXPECT_EQ(console_observer.messages()[1], console_observer.messages()[2]);
}

// Enable SameSiteByDefaultCookies to test deprecation messages for
// Lax-allow-unsafe.
class RenderFrameHostImplSameSiteByDefaultCookiesBrowserTest
    : public RenderFrameHostImplBrowserTest {
 public:
  void SetUp() override {
    feature_list_.InitWithFeatures({features::kCookieDeprecationMessages,
                                    net::features::kSameSiteByDefaultCookies},
                                   {});
    RenderFrameHostImplBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplSameSiteByDefaultCookiesBrowserTest,
                       DisplaySameSiteCookieDeprecationMessages) {
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  ConsoleObserverDelegate console_observer(web_contents, "*");
  web_contents->SetDelegate(&console_observer);

  // Test deprecation messages for SameSiteByDefault.
  // Set a cookie without SameSite on b.com, then access it in a cross-site
  // context.
  base::Time set_cookie_time = base::Time::Now();
  GURL url =
      embedded_test_server()->GetURL("x.com", "/set-cookie?nosamesite=1");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  // Message does not appear in same-site context (main frame is x).
  ASSERT_EQ(0u, console_observer.messages().size());
  url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(x())");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  // Message appears in cross-site context (a framing x).
  EXPECT_EQ(1u, console_observer.messages().size());

  // Test deprecation messages for CookiesWithoutSameSiteMustBeSecure.
  // Set a cookie with SameSite=None but without Secure.
  url = embedded_test_server()->GetURL(
      "c.com", "/set-cookie?samesitenoneinsecure=1;SameSite=None");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  // The 1 message from before, plus the (different) message for setting the
  // SameSite=None insecure cookie.
  EXPECT_EQ(2u, console_observer.messages().size());

  // Test deprecation messages for Lax-allow-unsafe.
  url = embedded_test_server()->GetURL("a.com",
                                       "/form_that_posts_cross_site.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  // Submit the form to make a cross-site POST request to x.com.
  TestNavigationObserver form_post_observer(shell()->web_contents(), 1);
  EXPECT_TRUE(ExecJs(shell(), "document.getElementById('text-form').submit()"));
  form_post_observer.Wait();

  // The test should not take more than 2 minutes.
  ASSERT_LT(base::Time::Now() - set_cookie_time, net::kLaxAllowUnsafeMaxAge);
  EXPECT_EQ(3u, console_observer.messages().size());

  // Check that the messages were all distinct.
  EXPECT_NE(console_observer.messages()[0], console_observer.messages()[1]);
  EXPECT_NE(console_observer.messages()[0], console_observer.messages()[2]);
  EXPECT_NE(console_observer.messages()[1], console_observer.messages()[2]);
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       SchedulerTrackedFeatures) {
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  RenderFrameHostImpl* main_frame = reinterpret_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetMainFrame());
  // Simulate getting 0b1 as a feature vector from the renderer.
  static_cast<blink::mojom::LocalFrameHost*>(main_frame)
      ->DidChangeActiveSchedulerTrackedFeatures(0b1u);
  DCHECK_EQ(main_frame->scheduler_tracked_features(), 0b1u);
  // Simulate the browser side reporting a feature usage.
  main_frame->OnSchedulerTrackedFeatureUsed(
      static_cast<blink::scheduler::WebSchedulerTrackedFeature>(1));
  DCHECK_EQ(main_frame->scheduler_tracked_features(), 0b11u);
  // Simulate a feature vector being updated from the renderer with some
  // features being activated and some being deactivated.
  static_cast<blink::mojom::LocalFrameHost*>(main_frame)
      ->DidChangeActiveSchedulerTrackedFeatures(0b100u);
  DCHECK_EQ(main_frame->scheduler_tracked_features(), 0b110u);

  // Navigate away and expect that no values persist the navigation.
  // Note that we are still simulating the renderer call, otherwise features
  // like "document loaded" will show up here.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));
  main_frame = reinterpret_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetMainFrame());
  static_cast<blink::mojom::LocalFrameHost*>(main_frame)
      ->DidChangeActiveSchedulerTrackedFeatures(0b0u);
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       ComputeSiteForCookiesForNavigation) {
  // Start second server for HTTPS.
  https_server()->ServeFilesFromSourceDirectory(GetTestDataFilePath());
  ASSERT_TRUE(https_server()->Start());

  GURL url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a(b(d)),c())");

  FirstPartySchemeContentBrowserClient new_client(url);
  ContentBrowserClient* old_client = SetBrowserClientForTesting(&new_client);

  GURL b_url = embedded_test_server()->GetURL("b.com", "/");
  GURL c_url = embedded_test_server()->GetURL("c.com", "/");
  GURL secure_url = https_server()->GetURL("/");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  {
    WebContentsImpl* wc =
        static_cast<WebContentsImpl*>(shell()->web_contents());
    RenderFrameHostImpl* main_frame = wc->GetMainFrame();

    EXPECT_EQ("a.com", main_frame->GetLastCommittedURL().host());
    ASSERT_EQ(2u, main_frame->child_count());
    FrameTreeNode* child_a = main_frame->child_at(0);
    FrameTreeNode* child_c = main_frame->child_at(1);
    EXPECT_EQ("a.com", child_a->current_url().host());
    EXPECT_EQ("c.com", child_c->current_url().host());

    ASSERT_EQ(1u, child_a->child_count());
    FrameTreeNode* child_b = child_a->child_at(0);
    EXPECT_EQ("b.com", child_b->current_url().host());
    ASSERT_EQ(1u, child_b->child_count());
    FrameTreeNode* child_d = child_b->child_at(0);
    EXPECT_EQ("d.com", child_d->current_url().host());

    EXPECT_EQ("a.com",
              main_frame->ComputeSiteForCookiesForNavigation(url).host());
    EXPECT_EQ("b.com",
              main_frame->ComputeSiteForCookiesForNavigation(b_url).host());
    EXPECT_EQ("c.com",
              main_frame->ComputeSiteForCookiesForNavigation(c_url).host());

    // a.com -> a.com frame being navigated.
    EXPECT_EQ("a.com", child_a->current_frame_host()
                           ->ComputeSiteForCookiesForNavigation(url)
                           .host());
    EXPECT_EQ("a.com", child_a->current_frame_host()
                           ->ComputeSiteForCookiesForNavigation(b_url)
                           .host());
    EXPECT_EQ("a.com", child_a->current_frame_host()
                           ->ComputeSiteForCookiesForNavigation(c_url)
                           .host());

    // a.com -> a.com -> b.com frame being navigated.

    // The first case here is especially interesting, since we go to
    // a/a/a from a/a/b. We currently treat this as all first-party, but there
    // is a case to be made for doing it differently, due to involvement of b.
    EXPECT_EQ("a.com", child_b->current_frame_host()
                           ->ComputeSiteForCookiesForNavigation(url)
                           .host());
    EXPECT_EQ("a.com", child_b->current_frame_host()
                           ->ComputeSiteForCookiesForNavigation(b_url)
                           .host());
    EXPECT_EQ("a.com", child_b->current_frame_host()
                           ->ComputeSiteForCookiesForNavigation(c_url)
                           .host());

    // a.com -> c.com frame being navigated.
    EXPECT_EQ("a.com", child_c->current_frame_host()
                           ->ComputeSiteForCookiesForNavigation(url)
                           .host());
    EXPECT_EQ("a.com", child_c->current_frame_host()
                           ->ComputeSiteForCookiesForNavigation(b_url)
                           .host());
    EXPECT_EQ("a.com", child_c->current_frame_host()
                           ->ComputeSiteForCookiesForNavigation(c_url)
                           .host());

    // a.com -> a.com -> b.com -> d.com frame being navigated.
    EXPECT_EQ("", child_d->current_frame_host()
                      ->ComputeSiteForCookiesForNavigation(url)
                      .host());
    EXPECT_EQ("", child_d->current_frame_host()
                      ->ComputeSiteForCookiesForNavigation(b_url)
                      .host());
    EXPECT_EQ("", child_d->current_frame_host()
                      ->ComputeSiteForCookiesForNavigation(c_url)
                      .host());
  }

  // Now try with a trusted scheme that gives first-partiness.
  GURL trusty_url(kTrustMeUrl);
  EXPECT_TRUE(NavigateToURL(shell(), trusty_url));
  {
    WebContentsImpl* wc =
        static_cast<WebContentsImpl*>(shell()->web_contents());
    RenderFrameHostImpl* main_frame =
        static_cast<RenderFrameHostImpl*>(wc->GetMainFrame());
    EXPECT_EQ(trusty_url.GetOrigin(),
              main_frame->GetLastCommittedURL().GetOrigin());

    ASSERT_EQ(1u, main_frame->child_count());
    FrameTreeNode* child_a = main_frame->child_at(0);
    EXPECT_EQ("a.com", child_a->current_url().host());

    ASSERT_EQ(2u, child_a->child_count());
    FrameTreeNode* child_aa = child_a->child_at(0);
    EXPECT_EQ("a.com", child_aa->current_url().host());

    ASSERT_EQ(1u, child_aa->child_count());
    FrameTreeNode* child_aab = child_aa->child_at(0);
    EXPECT_EQ("b.com", child_aab->current_url().host());

    ASSERT_EQ(1u, child_aab->child_count());
    FrameTreeNode* child_aabd = child_aab->child_at(0);
    EXPECT_EQ("d.com", child_aabd->current_url().host());

    // Main frame navigations are not affected by the special schema.
    EXPECT_EQ(url.GetOrigin(),
              main_frame->ComputeSiteForCookiesForNavigation(url).GetOrigin());
    EXPECT_EQ(
        b_url.GetOrigin(),
        main_frame->ComputeSiteForCookiesForNavigation(b_url).GetOrigin());
    EXPECT_EQ(
        c_url.GetOrigin(),
        main_frame->ComputeSiteForCookiesForNavigation(c_url).GetOrigin());

    // Child navigation gets the magic scheme.
    EXPECT_EQ(trusty_url.GetOrigin(),
              child_aa->current_frame_host()
                  ->ComputeSiteForCookiesForNavigation(url)
                  .GetOrigin());
    EXPECT_EQ(trusty_url.GetOrigin(),
              child_aa->current_frame_host()
                  ->ComputeSiteForCookiesForNavigation(b_url)
                  .GetOrigin());
    EXPECT_EQ(trusty_url.GetOrigin(),
              child_aa->current_frame_host()
                  ->ComputeSiteForCookiesForNavigation(c_url)
                  .GetOrigin());

    EXPECT_EQ(trusty_url.GetOrigin(),
              child_aabd->current_frame_host()
                  ->ComputeSiteForCookiesForNavigation(url)
                  .GetOrigin());
    EXPECT_EQ(trusty_url.GetOrigin(),
              child_aabd->current_frame_host()
                  ->ComputeSiteForCookiesForNavigation(b_url)
                  .GetOrigin());
    EXPECT_EQ(trusty_url.GetOrigin(),
              child_aabd->current_frame_host()
                  ->ComputeSiteForCookiesForNavigation(c_url)
                  .GetOrigin());
  }

  // Test trusted scheme that gives first-partiness if the url is secure.
  GURL trusty_if_secure_url(kTrustMeIfEmbeddingSecureUrl);
  EXPECT_TRUE(NavigateToURL(shell(), trusty_if_secure_url));
  {
    WebContentsImpl* wc =
        static_cast<WebContentsImpl*>(shell()->web_contents());
    RenderFrameHostImpl* main_frame =
        static_cast<RenderFrameHostImpl*>(wc->GetMainFrame());
    EXPECT_EQ(trusty_if_secure_url.GetOrigin(),
              main_frame->GetLastCommittedURL().GetOrigin());

    ASSERT_EQ(1u, main_frame->child_count());
    FrameTreeNode* child_a = main_frame->child_at(0);
    EXPECT_EQ("a.com", child_a->current_url().host());

    ASSERT_EQ(2u, child_a->child_count());
    FrameTreeNode* child_aa = child_a->child_at(0);
    EXPECT_EQ("a.com", child_aa->current_url().host());

    ASSERT_EQ(1u, child_aa->child_count());
    FrameTreeNode* child_aab = child_aa->child_at(0);
    EXPECT_EQ("b.com", child_aab->current_url().host());

    ASSERT_EQ(1u, child_aab->child_count());
    FrameTreeNode* child_aabd = child_aab->child_at(0);
    EXPECT_EQ("d.com", child_aabd->current_url().host());

    // Main frame navigations are not affected by the special schema.
    EXPECT_EQ(url.GetOrigin(),
              main_frame->ComputeSiteForCookiesForNavigation(url).GetOrigin());
    EXPECT_EQ(
        b_url.GetOrigin(),
        main_frame->ComputeSiteForCookiesForNavigation(b_url).GetOrigin());
    EXPECT_EQ(
        secure_url.GetOrigin(),
        main_frame->ComputeSiteForCookiesForNavigation(secure_url).GetOrigin());

    // Child navigation gets the magic scheme iff secure.
    EXPECT_EQ("", child_aa->current_frame_host()
                      ->ComputeSiteForCookiesForNavigation(url)
                      .GetOrigin());
    EXPECT_EQ("", child_aa->current_frame_host()
                      ->ComputeSiteForCookiesForNavigation(b_url)
                      .GetOrigin());
    EXPECT_EQ(trusty_url.GetOrigin(),
              child_aa->current_frame_host()
                  ->ComputeSiteForCookiesForNavigation(secure_url)
                  .GetOrigin());

    EXPECT_EQ("", child_aabd->current_frame_host()
                      ->ComputeSiteForCookiesForNavigation(url)
                      .GetOrigin());
    EXPECT_EQ("", child_aabd->current_frame_host()
                      ->ComputeSiteForCookiesForNavigation(b_url)
                      .GetOrigin());
    EXPECT_EQ(trusty_url.GetOrigin(),
              child_aabd->current_frame_host()
                  ->ComputeSiteForCookiesForNavigation(secure_url)
                  .GetOrigin());
  }

  SetBrowserClientForTesting(old_client);
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       ComputeSiteForCookiesForNavigationSandbox) {
  // Test sandboxed subframe.
  {
    GURL url = embedded_test_server()->GetURL(
        "a.com",
        "/cross_site_iframe_factory.html?a(a{sandbox-allow-scripts}(a),"
        "a{sandbox-allow-scripts,sandbox-allow-same-origin}(a))");

    EXPECT_TRUE(NavigateToURL(shell(), url));

    WebContentsImpl* wc =
        static_cast<WebContentsImpl*>(shell()->web_contents());
    RenderFrameHostImpl* main_frame = wc->GetMainFrame();

    EXPECT_EQ("a.com", main_frame->GetLastCommittedURL().host());

    ASSERT_EQ(2u, main_frame->child_count());
    FrameTreeNode* child_a = main_frame->child_at(0);
    EXPECT_EQ("a.com", child_a->current_url().host());
    EXPECT_TRUE(
        child_a->current_frame_host()->GetLastCommittedOrigin().opaque());

    ASSERT_EQ(1u, child_a->child_count());
    FrameTreeNode* child_aa = child_a->child_at(0);
    EXPECT_EQ("a.com", child_aa->current_url().host());
    EXPECT_TRUE(
        child_aa->current_frame_host()->GetLastCommittedOrigin().opaque());

    FrameTreeNode* child_a2 = main_frame->child_at(1);
    EXPECT_EQ("a.com", child_a2->current_url().host());
    EXPECT_FALSE(
        child_a2->current_frame_host()->GetLastCommittedOrigin().opaque());

    ASSERT_EQ(1u, child_a2->child_count());
    FrameTreeNode* child_a2a = child_a2->child_at(0);
    EXPECT_EQ("a.com", child_a2a->current_url().host());
    EXPECT_FALSE(
        child_a2a->current_frame_host()->GetLastCommittedOrigin().opaque());

    // |child_aa| frame navigation should be cross-site since its parent is
    // sandboxed without allow-same-origin
    EXPECT_TRUE(child_aa->current_frame_host()
                    ->ComputeSiteForCookiesForNavigation(url)
                    .is_empty());

    // |child_a2a| frame navigation should be same-site since its sandboxed
    // parent is sandbox-same-origin.
    EXPECT_EQ("a.com", child_a2a->current_frame_host()
                           ->ComputeSiteForCookiesForNavigation(url)
                           .host());
  }

  // Test sandboxed main frame.
  {
    GURL url =
        embedded_test_server()->GetURL("a.com", "/csp_sandboxed_frame.html");
    EXPECT_TRUE(NavigateToURL(shell(), url));

    WebContentsImpl* wc =
        static_cast<WebContentsImpl*>(shell()->web_contents());
    RenderFrameHostImpl* main_frame = wc->GetMainFrame();
    EXPECT_EQ(url, main_frame->GetLastCommittedURL());
    EXPECT_TRUE(main_frame->GetLastCommittedOrigin().opaque());

    ASSERT_EQ(2u, main_frame->child_count());
    FrameTreeNode* child_a = main_frame->child_at(0);
    EXPECT_EQ("a.com", child_a->current_url().host());
    EXPECT_TRUE(
        child_a->current_frame_host()->GetLastCommittedOrigin().opaque());

    EXPECT_TRUE(child_a->current_frame_host()
                    ->ComputeSiteForCookiesForNavigation(url)
                    .is_empty());
  }
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       ComputeSiteForCookiesForNavigationAboutBlank) {
  GURL url = embedded_test_server()->GetURL(
      "a.com", "/page_with_blank_iframe_tree.html");

  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = wc->GetMainFrame();

  EXPECT_EQ("a.com", main_frame->GetLastCommittedURL().host());

  ASSERT_EQ(1u, main_frame->child_count());
  FrameTreeNode* child_a = main_frame->child_at(0);
  EXPECT_TRUE(child_a->current_url().IsAboutBlank());
  EXPECT_EQ("a.com",
            child_a->current_frame_host()->GetLastCommittedOrigin().host());

  ASSERT_EQ(1u, child_a->child_count());
  FrameTreeNode* child_aa = child_a->child_at(0);
  EXPECT_TRUE(child_aa->current_url().IsAboutBlank());
  EXPECT_EQ("a.com",
            child_aa->current_frame_host()->GetLastCommittedOrigin().host());

  // navigating the nested about:blank iframe to a.com is fine, since the origin
  // is inherited.
  EXPECT_EQ("a.com", child_aa->current_frame_host()
                         ->ComputeSiteForCookiesForNavigation(url)
                         .host());
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       ComputeSiteForCookiesForNavigationSrcDoc) {
  // srcdoc frames basically don't figure into site_for_cookies computation.
  GURL url = embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_srcdoc_iframe_tree.html");

  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame =
      static_cast<RenderFrameHostImpl*>(wc->GetMainFrame());
  EXPECT_EQ("a.com", main_frame->GetLastCommittedURL().host());

  ASSERT_EQ(1u, main_frame->child_count());
  FrameTreeNode* child_sd = main_frame->child_at(0);
  EXPECT_TRUE(child_sd->current_url().IsAboutSrcdoc());

  ASSERT_EQ(1u, child_sd->child_count());
  FrameTreeNode* child_sd_a = child_sd->child_at(0);
  EXPECT_EQ("a.com", child_sd_a->current_url().host());

  ASSERT_EQ(1u, child_sd_a->child_count());
  FrameTreeNode* child_sd_a_sd = child_sd_a->child_at(0);
  EXPECT_TRUE(child_sd_a_sd->current_url().IsAboutSrcdoc());
  ASSERT_EQ(0u, child_sd_a_sd->child_count());

  EXPECT_EQ("a.com", child_sd->current_frame_host()
                         ->ComputeSiteForCookiesForNavigation(url)
                         .host());
  EXPECT_EQ("a.com", child_sd_a->current_frame_host()
                         ->ComputeSiteForCookiesForNavigation(url)
                         .host());
  EXPECT_EQ("a.com", child_sd_a_sd->current_frame_host()
                         ->ComputeSiteForCookiesForNavigation(url)
                         .host());

  GURL b_url = embedded_test_server()->GetURL("b.com", "/");
  EXPECT_EQ("b.com",
            main_frame->ComputeSiteForCookiesForNavigation(b_url).host());
  EXPECT_EQ("a.com", child_sd->current_frame_host()
                         ->ComputeSiteForCookiesForNavigation(b_url)
                         .host());
  EXPECT_EQ("a.com", child_sd_a->current_frame_host()
                         ->ComputeSiteForCookiesForNavigation(b_url)
                         .host());
  EXPECT_EQ("a.com", child_sd_a_sd->current_frame_host()
                         ->ComputeSiteForCookiesForNavigation(b_url)
                         .host());
}

// Make sure a local file and its subresources can be reloaded after a crash. In
// particular, after https://crbug.com/981339, a different RenderFrameHost will
// be used for reloading the file. File access must be correctly granted.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest, FileReloadAfterCrash) {
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());

  // 1. Navigate a local file with an iframe.
  GURL main_frame_url = GetFileURL(FILE_PATH_LITERAL("page_with_iframe.html"));
  GURL subframe_url = GetFileURL(FILE_PATH_LITERAL("title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  // 2. Crash.
  RenderProcessHost* process = wc->GetMainFrame()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  process->Shutdown(0);
  crash_observer.Wait();

  // 3. Reload.
  wc->GetController().Reload(ReloadType::NORMAL, false);
  EXPECT_TRUE(WaitForLoadStop(wc));

  // Check the document is correctly reloaded.
  RenderFrameHostImpl* main_document = wc->GetMainFrame();
  ASSERT_EQ(1u, main_document->child_count());
  RenderFrameHostImpl* sub_document =
      main_document->child_at(0)->current_frame_host();
  EXPECT_EQ(main_frame_url, main_document->GetLastCommittedURL());
  EXPECT_EQ(subframe_url, sub_document->GetLastCommittedURL());
  EXPECT_EQ("\n  \n  This page has an iframe. Yay for iframes!\n  \n\n",
            EvalJs(main_document, "document.body.textContent"));
  EXPECT_EQ("This page has no title.\n\n",
            EvalJs(sub_document, "document.body.textContent"));
}

// Make sure a webui can be reloaded after a crash.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest, WebUiReloadAfterCrash) {
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());

  // 1. Navigate a local file with an iframe.
  GURL main_frame_url(std::string(kChromeUIScheme) + "://" +
                      std::string(kChromeUIGpuHost));
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  // 2. Crash.
  RenderProcessHost* process = wc->GetMainFrame()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  process->Shutdown(0);
  crash_observer.Wait();

  // 3. Reload.
  wc->GetController().Reload(ReloadType::NORMAL, false);
  EXPECT_TRUE(WaitForLoadStop(wc));

  // Check the document is correctly reloaded.
  RenderFrameHostImpl* main_document = wc->GetMainFrame();
  EXPECT_EQ(main_frame_url, main_document->GetLastCommittedURL());
  EXPECT_EQ("Graphics Feature Status",
            EvalJs(main_document, "document.querySelector('h3').textContent"));
}

namespace {

// Collects the committed IPAddressSpaces, and makes them available for
// evaluation. Nothing about the request is modified; this is a read-only
// interceptor.
class IPAddressSpaceCollector : public DidCommitNavigationInterceptor {
 public:
  using CommitData = std::pair<GURL, network::mojom::IPAddressSpace>;
  using CommitDataVector = std::vector<CommitData>;

  explicit IPAddressSpaceCollector(WebContents* web_contents)
      : DidCommitNavigationInterceptor(web_contents) {}
  ~IPAddressSpaceCollector() override = default;

  network::mojom::IPAddressSpace IPAddressSpaceForUrl(const GURL& url) const {
    for (auto item : commits_) {
      if (item.first == url)
        return item.second;
    }
    return network::mojom::IPAddressSpace::kUnknown;
  }

  network::mojom::IPAddressSpace last_ip_address_space() const {
    return commits_.back().second;
  }

 protected:
  bool WillProcessDidCommitNavigation(
      RenderFrameHost* render_frame_host,
      NavigationRequest* navigation_request,
      ::FrameHostMsg_DidCommitProvisionalLoad_Params* params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr* interface_params)
      override {
    commits_.push_back(
        CommitData(params->url.spec().c_str(),
                   navigation_request
                       ? navigation_request->commit_params().ip_address_space
                       : network::mojom::IPAddressSpace::kUnknown));
    return true;
  }

 private:
  CommitDataVector commits_;

  DISALLOW_COPY_AND_ASSIGN(IPAddressSpaceCollector);
};

}  // namespace

class RenderFrameHostImplBrowserTestWithNonSecureExternalRequestsBlocked
    : public RenderFrameHostImplBrowserTest {
 public:
  RenderFrameHostImplBrowserTestWithNonSecureExternalRequestsBlocked() {
    feature_list_.InitAndEnableFeature(
        network::features::kBlockNonSecureExternalRequests);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// TODO(https://crbug.com/1014325): Flaky on multiple bots.
IN_PROC_BROWSER_TEST_F(
    RenderFrameHostImplBrowserTestWithNonSecureExternalRequestsBlocked,
    DISABLED_ComputeMainFrameIPAddressSpace) {
  // TODO(mkwst): `about:`, `file:`, `data:`, `blob:`, and `filesystem:` URLs
  // are all treated as `kUnknown` today. This is ~incorrect, but safe, as their
  // web-facing behavior will be equivalent to "public".
  struct {
    GURL url;
    network::mojom::IPAddressSpace expected_internal;
    std::string expected_web_facing;
  } test_cases[] = {
      {GURL("about:blank"), network::mojom::IPAddressSpace::kUnknown, "public"},
      {GURL("data:text/html,foo"), network::mojom::IPAddressSpace::kUnknown,
       "public"},
      {GetTestUrl("", "empty.html"), network::mojom::IPAddressSpace::kUnknown,
       "public"},
      {embedded_test_server()->GetURL("/empty.html"),
       network::mojom::IPAddressSpace::kLocal, "local"},
      {embedded_test_server()->GetURL("/empty-treat-as-public-address.html"),
       network::mojom::IPAddressSpace::kPublic, "public"},
  };

  for (auto test : test_cases) {
    SCOPED_TRACE(test.url);
    IPAddressSpaceCollector collector(shell()->web_contents());
    EXPECT_TRUE(NavigateToURL(shell(), test.url));
    RenderFrameHostImpl* rfhi = static_cast<RenderFrameHostImpl*>(
        shell()->web_contents()->GetMainFrame());
    EXPECT_EQ(test.expected_internal, collector.last_ip_address_space());
    EXPECT_EQ(test.expected_web_facing, EvalJs(rfhi, "document.addressSpace"));
  }
}

IN_PROC_BROWSER_TEST_F(
    RenderFrameHostImplBrowserTestWithNonSecureExternalRequestsBlocked,
    ComputeIFrameLoopbackIPAddressSpace) {
  {
    IPAddressSpaceCollector collector(shell()->web_contents());
    base::string16 expected_title(base::UTF8ToUTF16("LOADED"));
    TitleWatcher title_watcher(shell()->web_contents(), expected_title);
    EXPECT_TRUE(
        NavigateToURL(shell(), embedded_test_server()->GetURL(
                                   "/do-not-treat-as-public-address.html")));
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

    std::vector<RenderFrameHost*> frames =
        shell()->web_contents()->GetAllFrames();
    for (auto* frame : frames) {
      SCOPED_TRACE(::testing::Message()
                   << "URL: " << frame->GetLastCommittedURL());
      auto* rfhi = static_cast<RenderFrameHostImpl*>(frame);

      if (frame->GetLastCommittedURL().IsAboutBlank()) {
        // TODO(986744): `about:blank` is not navigated via
        // `RenderFrameHostImpl::CommitNavigation`, but handled in the renderer
        // via `RenderFrameImpl::CommitSyncNavigation`. This means that we don't
        // calculate the value correctly on the browser-side, but do correctly
        // inherit from the initiator on the Blink-side.
        EXPECT_EQ(network::mojom::IPAddressSpace::kUnknown,
                  collector.IPAddressSpaceForUrl(frame->GetLastCommittedURL()));
        EXPECT_EQ("local", EvalJs(rfhi, "document.addressSpace"));
      } else if (frame->GetLastCommittedURL().SchemeIsFileSystem() ||
                 frame->GetLastCommittedURL().SchemeIsBlob() ||
                 frame->GetLastCommittedURL().IsAboutSrcdoc() ||
                 frame->GetLastCommittedURL().SchemeIs(url::kDataScheme)) {
        // TODO(986744): `data:`, `blob:`, `filesystem:`, and `about:srcdoc`
        // should all inherit the IPAddressSpace from the document
        // that initiated a navigation. Right now, we treat them as `kPublic`.
        EXPECT_EQ(network::mojom::IPAddressSpace::kUnknown,
                  collector.IPAddressSpaceForUrl(frame->GetLastCommittedURL()));
        EXPECT_EQ("public", EvalJs(rfhi, "document.addressSpace"));
      } else {
        // TODO(mkwst): Once the above two TODOs are resolved, this branch will
        // be the correct expectation for all the frames in this test.
        EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
                  collector.IPAddressSpaceForUrl(frame->GetLastCommittedURL()));
        EXPECT_EQ("local", EvalJs(rfhi, "document.addressSpace"));
      }
    }
  }

  // Loading from loopback that asserts publicness: `data:`, `blob:`,
  // `filesystem:`, `about:blank`, and `about:srcdoc` all inherit the assertion.
  {
    IPAddressSpaceCollector collector(shell()->web_contents());
    base::string16 expected_title(base::UTF8ToUTF16("LOADED"));
    TitleWatcher title_watcher(shell()->web_contents(), expected_title);
    EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(
                                           "/treat-as-public-address.html")));
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

    std::vector<RenderFrameHost*> frames =
        shell()->web_contents()->GetAllFrames();
    for (auto* frame : frames) {
      auto* rfhi = static_cast<RenderFrameHostImpl*>(frame);
      if (frame->GetLastCommittedURL().IsAboutBlank()) {
        // TODO(986744): `about:blank` is not navigated via `NavigationRequest`,
        // but handled in the renderer via
        // `RenderFrameImpl::CommitSyncNavigation`. This means that we don't
        // calculate the value correctly on the browser-side, but do correctly
        // inherit from the initiator on the Blink-side.
        EXPECT_EQ(network::mojom::IPAddressSpace::kUnknown,
                  collector.IPAddressSpaceForUrl(frame->GetLastCommittedURL()));
        EXPECT_EQ("public", EvalJs(rfhi, "document.addressSpace"));
      } else if (frame->GetLastCommittedURL().SchemeIsFileSystem() ||
                 frame->GetLastCommittedURL().SchemeIsBlob() ||
                 frame->GetLastCommittedURL().IsAboutSrcdoc() ||
                 frame->GetLastCommittedURL().SchemeIs(url::kDataScheme)) {
        // TODO(986744): `data:`, `blob:`, `filesystem:`, and `about:srcdoc`
        // should all inherit the IPAddressSpace from the document
        // that initiated a navigation. Right now, we treat them as `kUnknown`.
        EXPECT_EQ(network::mojom::IPAddressSpace::kUnknown,
                  collector.IPAddressSpaceForUrl(frame->GetLastCommittedURL()));
        EXPECT_EQ("public", EvalJs(rfhi, "document.addressSpace"));
      } else {
        EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
                  collector.IPAddressSpaceForUrl(frame->GetLastCommittedURL()));
        EXPECT_EQ("public", EvalJs(rfhi, "document.addressSpace"));
      }
    }
  }
}

namespace {

// Calls |callback| whenever a DOMContentLoaded is reached in
// |render_frame_host|.
class DOMContentLoadedObserver : public WebContentsObserver {
 public:
  DOMContentLoadedObserver(WebContents* web_contents,
                           base::RepeatingClosure callback)
      : WebContentsObserver(web_contents), callback_(callback) {}

 protected:
  // WebContentsObserver:
  void DOMContentLoaded(RenderFrameHost* render_Frame_host) override {
    callback_.Run();
  }

 private:
  base::RepeatingClosure callback_;
  DISALLOW_COPY_AND_ASSIGN(DOMContentLoadedObserver);
};

// Calls |callback| whenever a DocumentOnLoad is reached in
// |render_frame_host|.
class DocumentOnLoadObserver : public WebContentsObserver {
 public:
  DocumentOnLoadObserver(WebContents* web_contents,
                         base::RepeatingClosure callback)
      : WebContentsObserver(web_contents), callback_(callback) {}

 protected:
  // WebContentsObserver:
  void DocumentOnLoadCompletedInMainFrame() override { callback_.Run(); }

 private:
  base::RepeatingClosure callback_;
  DISALLOW_COPY_AND_ASSIGN(DocumentOnLoadObserver);
};

}  // namespace

IN_PROC_BROWSER_TEST_F(ContentBrowserTest, LoadCallbacks) {
  net::test_server::ControllableHttpResponse main_document_response(
      embedded_test_server(), "/main_document");
  net::test_server::ControllableHttpResponse image_response(
      embedded_test_server(), "/img");

  EXPECT_TRUE(embedded_test_server()->Start());
  GURL main_document_url(embedded_test_server()->GetURL("/main_document"));

  WebContents* web_contents = shell()->web_contents();
  RenderFrameHostImpl* rfhi =
      static_cast<RenderFrameHostImpl*>(web_contents->GetMainFrame());
  TestNavigationObserver load_observer(web_contents);
  base::RunLoop loop_until_dcl;
  DOMContentLoadedObserver dcl_observer(web_contents,
                                        loop_until_dcl.QuitClosure());
  shell()->LoadURL(main_document_url);

  EXPECT_FALSE(rfhi->dom_content_loaded());
  EXPECT_FALSE(web_contents->IsDocumentOnLoadCompletedInMainFrame());

  main_document_response.WaitForRequest();
  main_document_response.Send(
      "HTTP/1.1 200 OK\r\n"
      "Connection: close\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n"
      "<img src='/img'>");

  load_observer.WaitForNavigationFinished();
  EXPECT_FALSE(rfhi->dom_content_loaded());
  EXPECT_FALSE(web_contents->IsDocumentOnLoadCompletedInMainFrame());

  main_document_response.Done();

  // We should reach DOMContentLoaded, but not onload, since the image resource
  // is still loading.
  loop_until_dcl.Run();
  EXPECT_TRUE(rfhi->is_loading());
  EXPECT_TRUE(rfhi->dom_content_loaded());
  EXPECT_FALSE(web_contents->IsDocumentOnLoadCompletedInMainFrame());

  base::RunLoop loop_until_onload;
  DocumentOnLoadObserver onload_observer(web_contents,
                                         loop_until_onload.QuitClosure());

  image_response.WaitForRequest();
  image_response.Done();

  // And now onload() should be reached.
  loop_until_onload.Run();
  EXPECT_TRUE(rfhi->dom_content_loaded());
  EXPECT_TRUE(web_contents->IsDocumentOnLoadCompletedInMainFrame());
}

IN_PROC_BROWSER_TEST_F(ContentBrowserTest, LoadingStateResetOnNavigation) {
  net::test_server::ControllableHttpResponse document2_response(
      embedded_test_server(), "/document2");

  EXPECT_TRUE(embedded_test_server()->Start());
  GURL url1(embedded_test_server()->GetURL("/title1.html"));
  GURL url2(embedded_test_server()->GetURL("/document2"));

  WebContents* web_contents = shell()->web_contents();
  RenderFrameHostImpl* rfhi =
      static_cast<RenderFrameHostImpl*>(web_contents->GetMainFrame());

  base::RunLoop loop_until_onload;
  DocumentOnLoadObserver onload_observer(web_contents,
                                         loop_until_onload.QuitClosure());
  shell()->LoadURL(url1);
  loop_until_onload.Run();

  EXPECT_TRUE(rfhi->dom_content_loaded());
  EXPECT_TRUE(web_contents->IsDocumentOnLoadCompletedInMainFrame());

  // Expect that the loading state will be reset after a navigation.

  TestNavigationObserver navigation_observer(web_contents);
  shell()->LoadURL(url2);

  document2_response.WaitForRequest();
  document2_response.Send(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n");
  navigation_observer.WaitForNavigationFinished();

  EXPECT_FALSE(rfhi->dom_content_loaded());
  EXPECT_FALSE(web_contents->IsDocumentOnLoadCompletedInMainFrame());
}

IN_PROC_BROWSER_TEST_F(ContentBrowserTest,
                       LoadingStateIsNotResetOnFailedNavigation) {
  net::test_server::ControllableHttpResponse document2_response(
      embedded_test_server(), "/document2");

  EXPECT_TRUE(embedded_test_server()->Start());
  GURL url1(embedded_test_server()->GetURL("/title1.html"));
  GURL url2(embedded_test_server()->GetURL("/document2"));

  WebContents* web_contents = shell()->web_contents();
  RenderFrameHostImpl* rfhi =
      static_cast<RenderFrameHostImpl*>(web_contents->GetMainFrame());

  base::RunLoop loop_until_onload;
  DocumentOnLoadObserver onload_observer(web_contents,
                                         loop_until_onload.QuitClosure());
  shell()->LoadURL(url1);
  loop_until_onload.Run();

  EXPECT_TRUE(rfhi->dom_content_loaded());
  EXPECT_TRUE(web_contents->IsDocumentOnLoadCompletedInMainFrame());

  // Expect that the loading state will NOT be reset after a cancelled
  // navigation.

  TestNavigationManager navigation_manager(web_contents, url2);
  shell()->LoadURL(url2);
  EXPECT_TRUE(navigation_manager.WaitForRequestStart());
  navigation_manager.ResumeNavigation();
  document2_response.WaitForRequest();

  document2_response.Send(
      "HTTP/1.1 204 No Content\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n");
  navigation_manager.WaitForNavigationFinished();

  EXPECT_TRUE(rfhi->dom_content_loaded());
  EXPECT_TRUE(web_contents->IsDocumentOnLoadCompletedInMainFrame());
}

}  // namespace content
