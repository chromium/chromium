// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_frame_host_impl.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/renderer_host/input/timeout_monitor.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/sms/test/mock_sms_provider.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/page_visibility_state.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/render_frame_host_test_support.h"
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
#include "content/test/test_render_frame_host_factory.h"
#include "net/base/features.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/cookies/cookie_constants.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/browser_interface_broker.mojom-test-utils.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-test-utils.h"
#include "url/gurl.h"
#include "url/origin.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#include "third_party/blink/public/mojom/remote_objects/remote_objects.mojom.h"
#endif  // defined(OS_ANDROID)

namespace content {
namespace {

using ::testing::NotNull;

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
      base::UkmSourceId ukm_source_id,
      NonNetworkURLLoaderFactoryDeprecatedMap* uniquely_owned_factories,
      NonNetworkURLLoaderFactoryMap* factories) override {
    auto trustme_factory = std::make_unique<network::TestURLLoaderFactory>();
    auto trustmeifembeddingsecure_factory =
        std::make_unique<network::TestURLLoaderFactory>();
    std::string response_body =
        base::StrCat({"<iframe src=\"", iframe_url_.spec(), "\"></iframe>"});
    trustme_factory->AddResponse(kTrustMeUrl, response_body);
    trustmeifembeddingsecure_factory->AddResponse(kTrustMeIfEmbeddingSecureUrl,
                                                  response_body);
    uniquely_owned_factories->emplace("trustme", std::move(trustme_factory));
    uniquely_owned_factories->emplace(
        "trustmeifembeddingsecure",
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
  using LifecycleState = RenderFrameHostImpl::LifecycleState;
  RenderFrameHostImplBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    // This makes the tests that check NetworkIsolationKeys make sure both the
    // frame and top frame origins are correct, without significantly affecting
    // other tests.
    feature_list_.InitAndEnableFeature(
        net::features::kAppendFrameOriginToNetworkIsolationKey);
  }
  ~RenderFrameHostImplBrowserTest() override = default;

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

    // TODO(https://crbug.com/794320): Remove this when the new Java Bridge code
    // is integrated into WebView.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kJavaScriptFlags, "--expose_gc");

    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kEnableBlinkFeatures, "SmsReceiver");
  }
  net::EmbeddedTestServer* https_server() { return &https_server_; }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  RenderFrameHostImpl* root_frame_host() const {
    return web_contents()->GetMainFrame();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer https_server_;
};

std::string ExecuteJavaScriptMethodAndGetResult(
    RenderFrameHostImpl* render_frame,
    const std::string& object,
    const std::string& method,
    base::Value arguments) {
  bool executing = true;
  std::string result;
  base::OnceCallback<void(base::Value)> call_back = base::BindOnce(
      [](bool* flag, std::string* reason, base::Value value) {
        *flag = false;
        DCHECK(value.is_string());
        *reason = value.GetString();
      },
      base::Unretained(&executing), base::Unretained(&result));

  render_frame->ExecuteJavaScriptMethod(
      base::UTF8ToUTF16(object), base::UTF8ToUTF16(method),
      std::move(arguments), std::move(call_back));

  while (executing) {
    base::RunLoop loop;
    loop.RunUntilIdle();
  }

  return result;
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       ExecuteJavaScriptMethodWorksWithArguments) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GetTestUrl("render_frame_host", "jsMethodTest.html")));

  RenderFrameHostImpl* render_frame = web_contents()->GetMainFrame();
  render_frame->AllowInjectingJavaScript();

  base::Value empty_arguments(base::Value::Type::LIST);
  std::string result = ExecuteJavaScriptMethodAndGetResult(
      render_frame, "window", "someMethod", std::move(empty_arguments));
  EXPECT_EQ(result, "called someMethod()");

  base::Value single_arguments(base::Value::Type::LIST);
  single_arguments.Append("arg1");
  result = ExecuteJavaScriptMethodAndGetResult(
      render_frame, "window", "someMethod", std::move(single_arguments));
  EXPECT_EQ(result, "called someMethod(arg1)");

  base::Value four_arguments(base::Value::Type::LIST);
  four_arguments.Append("arg1");
  four_arguments.Append("arg2");
  four_arguments.Append("arg3");
  four_arguments.Append("arg4");
  result = ExecuteJavaScriptMethodAndGetResult(
      render_frame, "window", "someMethod", std::move(four_arguments));
  EXPECT_EQ(result, "called someMethod(arg1,arg2,arg3,arg4)");
}

// Test that when creating a new window, the main frame is correctly focused.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest, IsFocused_AtLoad) {
  EXPECT_TRUE(
      NavigateToURL(shell(), GetTestUrl("render_frame_host", "focus.html")));

  // The main frame should be focused.
  EXPECT_EQ(web_contents()->GetMainFrame(), web_contents()->GetFocusedFrame());
}

// Test that if the content changes the focused frame, it is correctly exposed.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest, IsFocused_Change) {
  EXPECT_TRUE(
      NavigateToURL(shell(), GetTestUrl("render_frame_host", "focus.html")));

  std::string frames[2] = {"frame1", "frame2"};
  for (const std::string& frame : frames) {
    ExecuteScriptAndGetValue(web_contents()->GetMainFrame(),
                             "focus" + frame + "()");

    // The main frame is not the focused frame in the frame tree but the main
    // frame is focused per RFHI rules because one of its descendant is focused.
    // TODO(mlamouri): we should check the frame focus state per RFHI, see the
    // general comment at the beginning of this test file.
    EXPECT_NE(web_contents()->GetMainFrame(),
              web_contents()->GetFocusedFrame());
    EXPECT_EQ(frame, web_contents()->GetFocusedFrame()->GetFrameName());
  }
}

// Tests focus behavior when the focused frame is removed from the frame tree.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest, RemoveFocusedFrame) {
  EXPECT_TRUE(
      NavigateToURL(shell(), GetTestUrl("render_frame_host", "focus.html")));

  ExecuteScriptAndGetValue(web_contents()->GetMainFrame(), "focusframe4()");

  EXPECT_NE(web_contents()->GetMainFrame(), web_contents()->GetFocusedFrame());
  EXPECT_EQ("frame4", web_contents()->GetFocusedFrame()->GetFrameName());
  EXPECT_EQ("frame3",
            web_contents()->GetFocusedFrame()->GetParent()->GetFrameName());
  EXPECT_NE(-1, web_contents()->GetFrameTree()->focused_frame_tree_node_id_);

  ExecuteScriptAndGetValue(web_contents()->GetMainFrame(), "detachframe(3)");
  EXPECT_EQ(nullptr, web_contents()->GetFocusedFrame());
  EXPECT_EQ(-1, web_contents()->GetFrameTree()->focused_frame_tree_node_id_);

  ExecuteScriptAndGetValue(web_contents()->GetMainFrame(), "focusframe2()");
  EXPECT_NE(nullptr, web_contents()->GetFocusedFrame());
  EXPECT_NE(web_contents()->GetMainFrame(), web_contents()->GetFocusedFrame());
  EXPECT_NE(-1, web_contents()->GetFrameTree()->focused_frame_tree_node_id_);

  ExecuteScriptAndGetValue(web_contents()->GetMainFrame(), "detachframe(2)");
  EXPECT_EQ(nullptr, web_contents()->GetFocusedFrame());
  EXPECT_EQ(-1, web_contents()->GetFrameTree()->focused_frame_tree_node_id_);
}

// Test that a frame is visible/hidden depending on its WebContents visibility
// state.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       GetVisibilityState_Basic) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL("data:text/html,foo")));

  web_contents()->WasShown();
  EXPECT_EQ(PageVisibilityState::kVisible,
            web_contents()->GetMainFrame()->GetVisibilityState());

  web_contents()->WasHidden();
  EXPECT_EQ(PageVisibilityState::kHidden,
            web_contents()->GetMainFrame()->GetVisibilityState());
}

// Test that a frame visibility can be overridden by the ContentBrowserClient.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       GetVisibilityState_Override) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL("data:text/html,foo")));

  PrerenderTestContentBrowserClient new_client;
  ContentBrowserClient* old_client = SetBrowserClientForTesting(&new_client);

  web_contents()->WasShown();
  EXPECT_EQ(PageVisibilityState::kVisible,
            web_contents()->GetMainFrame()->GetVisibilityState());

  new_client.EnableVisibilityOverride(PageVisibilityState::kHiddenButPainting);
  EXPECT_EQ(PageVisibilityState::kHiddenButPainting,
            web_contents()->GetMainFrame()->GetVisibilityState());

  SetBrowserClientForTesting(old_client);
}

// Check that the URLLoaderFactories created by RenderFrameHosts for renderers
// are not trusted.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       URLLoaderFactoryNotTrusted) {
  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory;
  web_contents()->GetMainFrame()->CreateNetworkServiceDefaultFactory(
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

// A RenderFrameHostImpl that discards callback for BeforeUnload.
class RenderFrameHostImplForBeforeUnloadInterceptor
    : public RenderFrameHostImpl {
 public:
  using RenderFrameHostImpl::RenderFrameHostImpl;

  void SendBeforeUnload(bool is_reload,
                        base::WeakPtr<RenderFrameHostImpl> rfh) override {
    rfh->GetAssociatedLocalFrame()->BeforeUnload(is_reload, base::DoNothing());
  }

 private:
  friend class RenderFrameHostFactoryForBeforeUnloadInterceptor;
};

class RenderFrameHostFactoryForBeforeUnloadInterceptor
    : public TestRenderFrameHostFactory {
 protected:
  std::unique_ptr<RenderFrameHostImpl> CreateRenderFrameHost(
      SiteInstance* site_instance,
      scoped_refptr<RenderViewHostImpl> render_view_host,
      RenderFrameHostDelegate* delegate,
      FrameTree* frame_tree,
      FrameTreeNode* frame_tree_node,
      int32_t routing_id,
      const base::UnguessableToken& frame_token,
      bool renderer_initiated_creation) override {
    return base::WrapUnique(new RenderFrameHostImplForBeforeUnloadInterceptor(
        site_instance, std::move(render_view_host), delegate, frame_tree,
        frame_tree_node, routing_id, frame_token, renderer_initiated_creation,
        RenderFrameHostImpl::LifecycleState::kActive));
  }
};

mojo::ScopedMessagePipeHandle CreateDisconnectedMessagePipeHandle() {
  mojo::MessagePipe pipe;
  return std::move(pipe.handle0);
}

}  // namespace

// Tests that a beforeunload dialog in an iframe doesn't stop the beforeunload
// timer of a parent frame.
// TODO(avi): flaky on Linux TSAN: http://crbug.com/795326
#if (defined(OS_LINUX) || defined(OS_CHROMEOS)) && defined(THREAD_SANITIZER)
#define MAYBE_IframeBeforeUnloadParentHang DISABLED_IframeBeforeUnloadParentHang
#else
#define MAYBE_IframeBeforeUnloadParentHang IframeBeforeUnloadParentHang
#endif
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       MAYBE_IframeBeforeUnloadParentHang) {
  RenderFrameHostFactoryForBeforeUnloadInterceptor interceptor;

  TestJavaScriptDialogManager dialog_manager;
  web_contents()->SetDelegate(&dialog_manager);

  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
  // Make an iframe with a beforeunload handler.
  std::string script =
      "var iframe = document.createElement('iframe');"
      "document.body.appendChild(iframe);"
      "iframe.contentWindow.onbeforeunload=function(e){return 'x'};";
  EXPECT_TRUE(content::ExecuteScript(web_contents(), script));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  // JavaScript onbeforeunload dialogs require a user gesture.
  for (auto* frame : web_contents()->GetAllFrames())
    frame->ExecuteJavaScriptWithUserGestureForTests(base::string16());

  // Force a process switch by going to a privileged page. The beforeunload
  // timer will be started on the top-level frame but will be paused while the
  // beforeunload dialog is shown by the subframe.
  GURL web_ui_page(std::string(kChromeUIScheme) + "://" +
                   std::string(kChromeUIGpuHost));
  shell()->LoadURL(web_ui_page);
  dialog_manager.Wait();

  RenderFrameHostImpl* main_frame = web_contents()->GetMainFrame();
  EXPECT_TRUE(main_frame->is_waiting_for_beforeunload_completion());

  // Answer the dialog.
  dialog_manager.Run(true, base::string16());

  // There will be no beforeunload completion callback invocation, so if the
  // beforeunload completion callback timer isn't functioning then the
  // navigation will hang forever and this test will time out. If this waiting
  // for the load stop works, this test won't time out.
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  EXPECT_EQ(web_ui_page, web_contents()->GetLastCommittedURL());

  web_contents()->SetDelegate(nullptr);
  web_contents()->SetJavaScriptDialogManagerForTesting(nullptr);
}

// Tests that a gesture is required in a frame before it can request a
// beforeunload dialog.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       BeforeUnloadDialogRequiresGesture) {
  TestJavaScriptDialogManager dialog_manager;
  web_contents()->SetDelegate(&dialog_manager);

  EXPECT_TRUE(NavigateToURL(
      shell(), GetTestUrl("render_frame_host", "beforeunload.html")));
  // Disable the hang monitor, otherwise there will be a race between the
  // beforeunload dialog and the beforeunload hang timer.
  web_contents()->GetMainFrame()->DisableBeforeUnloadHangMonitorForTesting();

  // Reload. There should be no beforeunload dialog because there was no gesture
  // on the page. If there was, this WaitForLoadStop call will hang.
  web_contents()->GetController().Reload(ReloadType::NORMAL, false);
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  // Give the page a user gesture and try reloading again. This time there
  // should be a dialog. If there is no dialog, the call to Wait will hang.
  web_contents()->GetMainFrame()->ExecuteJavaScriptWithUserGestureForTests(
      base::string16());
  web_contents()->GetController().Reload(ReloadType::NORMAL, false);
  dialog_manager.Wait();

  // Answer the dialog.
  dialog_manager.Run(true, base::string16());
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  // The reload should have cleared the user gesture bit, so upon leaving again
  // there should be no beforeunload dialog.
  shell()->LoadURL(GURL("about:blank"));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  web_contents()->SetDelegate(nullptr);
  web_contents()->SetJavaScriptDialogManagerForTesting(nullptr);
}

// Test for crbug.com/80401.  Canceling a beforeunload dialog should reset
// the URL to the previous page's URL.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       CancelBeforeUnloadResetsURL) {
  TestJavaScriptDialogManager dialog_manager;
  web_contents()->SetDelegate(&dialog_manager);

  GURL url(GetTestUrl("render_frame_host", "beforeunload.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  PrepContentsForBeforeUnloadTest(web_contents());

  // Navigate to a page that triggers a cross-site transition.
  GURL url2(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  shell()->LoadURL(url2);
  dialog_manager.Wait();

  // Cancel the dialog.
  dialog_manager.reset_url_invalidate_count();
  dialog_manager.Run(false, base::string16());
  EXPECT_FALSE(web_contents()->IsLoading());

  // Verify there are no pending history items after the dialog is cancelled.
  // (see crbug.com/93858)
  NavigationEntry* entry = web_contents()->GetController().GetPendingEntry();
  EXPECT_EQ(nullptr, entry);
  EXPECT_EQ(url, web_contents()->GetVisibleURL());

  // There should have been at least one NavigationStateChange event for
  // invalidating the URL in the address bar, to avoid leaving the stale URL
  // visible.
  EXPECT_GE(dialog_manager.url_invalidate_count(), 1);

  web_contents()->SetDelegate(nullptr);
  web_contents()->SetJavaScriptDialogManagerForTesting(nullptr);
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

  // Install a beforeunload handler in the b.com subframe.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  InstallBeforeUnloadHandler(root->child_at(0), SHOW_DIALOG);

  // Disable beforeunload timer to prevent flakiness.
  PrepContentsForBeforeUnloadTest(web_contents());

  // Navigate cross-site.
  GURL cross_site_url(embedded_test_server()->GetURL("c.com", "/title1.html"));
  shell()->LoadURL(cross_site_url);

  // Only the main frame should be marked as waiting for beforeunload completion
  // callback as the frame being navigated.
  RenderFrameHostImpl* main_frame = web_contents()->GetMainFrame();
  RenderFrameHostImpl* child = root->child_at(0)->current_frame_host();
  EXPECT_TRUE(main_frame->is_waiting_for_beforeunload_completion());
  EXPECT_FALSE(child->is_waiting_for_beforeunload_completion());

  // Sanity check that the main frame is waiting for subframe's beforeunload
  // ACK.
  EXPECT_EQ(main_frame, child->GetBeforeUnloadInitiator());
  EXPECT_EQ(main_frame, main_frame->GetBeforeUnloadInitiator());

  // When in --site-per-process mode, LoadURL() should trigger two beforeunload
  // IPCs for subframe and the main frame: the subframe has a beforeunload
  // handler, and while the main frame does not, we always send the IPC to
  // navigating frames, regardless of whether or not they have a handler.
  //
  // Without --site-per-process, only one beforeunload IPC should be sent to
  // the main frame, which will handle both (same-process) frames.
  EXPECT_EQ(AreAllSitesIsolatedForTesting() ? 2u : 1u,
            main_frame->beforeunload_pending_replies_.size());

  // Wait for the beforeunload dialog to be shown from the subframe.
  dialog_manager()->Wait();

  // The main frame should still be waiting for subframe's beforeunload
  // completion callback.
  EXPECT_EQ(main_frame, child->GetBeforeUnloadInitiator());
  EXPECT_EQ(main_frame, main_frame->GetBeforeUnloadInitiator());
  EXPECT_TRUE(main_frame->is_waiting_for_beforeunload_completion());
  EXPECT_FALSE(child->is_waiting_for_beforeunload_completion());

  // In --site-per-process mode, the beforeunload completion callback should
  // happen on the child RFH.  Without --site-per-process, it will come from the
  // main frame RFH, which processes beforeunload for both main frame and child
  // frame, since they are in the same process.
  RenderFrameHostImpl* frame_that_sent_beforeunload_ipc =
      AreAllSitesIsolatedForTesting() ? child : main_frame;
  EXPECT_TRUE(main_frame->beforeunload_pending_replies_.count(
      frame_that_sent_beforeunload_ipc));

  // Answer the dialog with "cancel" to stay on current page.
  CloseDialogAndCancel();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  EXPECT_EQ(main_url, web_contents()->GetLastCommittedURL());

  // Verify beforeunload state has been cleared.
  EXPECT_FALSE(main_frame->is_waiting_for_beforeunload_completion());
  EXPECT_FALSE(child->is_waiting_for_beforeunload_completion());
  EXPECT_EQ(nullptr, main_frame->GetBeforeUnloadInitiator());
  EXPECT_EQ(nullptr, child->GetBeforeUnloadInitiator());
  EXPECT_EQ(0u, main_frame->beforeunload_pending_replies_.size());

  // Try navigating again.  The dialog should come up again.
  shell()->LoadURL(cross_site_url);
  dialog_manager()->Wait();
  EXPECT_TRUE(main_frame->is_waiting_for_beforeunload_completion());

  // Now answer the dialog and allow the navigation to proceed.  Disable
  // unload ACK on the old frame so that it sticks around in pending delete
  // state, since the test later verifies that it has received the beforeunload
  // ACK.
  TestFrameNavigationObserver commit_observer(root);
  main_frame->DisableUnloadTimerForTesting();
  CloseDialogAndProceed();
  commit_observer.WaitForCommit();
  EXPECT_EQ(cross_site_url, web_contents()->GetLastCommittedURL());
  EXPECT_FALSE(
      web_contents()->GetMainFrame()->is_waiting_for_beforeunload_completion());

  // The navigation that succeeded was a browser-initiated, main frame
  // navigation, so it swapped RenderFrameHosts. |main_frame| should either be
  // in pending deletion and waiting for unload ACK or enter back-forward cache,
  // but it should not be waiting for the beforeunload completion callback.
  EXPECT_THAT(
      main_frame->lifecycle_state(),
      testing::AnyOf(testing::Eq(LifecycleState::kRunningUnloadHandlers),
                     testing::Eq(LifecycleState::kInBackForwardCache)));
  EXPECT_FALSE(main_frame->is_waiting_for_beforeunload_completion());
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
  // navigation from waiting on the subframe's beforeunload completion callback.
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

  // We should have two pending beforeunload completion callbacks at this point,
  // and the beforeunload timer should be running.
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
  // confirmation to proceed, which should trigger a beforeunload completion
  // callback from the second frame. Wait for that beforeunload completion
  // callback. After it's received, there will be one ACK remaining for the
  // frame that's currently showing the dialog.
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
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  EXPECT_EQ(1u, Shell::windows().size());
  GURL test_url =
      embedded_test_server()->GetURL("/render_frame_host/window_open.html");
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
  TestNavigationObserver observer(web_contents());
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_EQ(url, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());

  // Submit the form.
  GURL submit_url("javascript:submitForm('isubmit')");
  EXPECT_TRUE(
      NavigateToURL(shell(), submit_url, post_url /* expected_commit_url */));

  // Check that a proper POST navigation was done.
  EXPECT_EQ("text=&select=a", base::UTF16ToASCII(web_contents()->GetTitle()));
  EXPECT_EQ(post_url, web_contents()->GetLastCommittedURL());
  EXPECT_TRUE(shell()
                  ->web_contents()
                  ->GetController()
                  .GetLastCommittedEntry()
                  ->GetHasPostData());

  // Reload and verify the form was submitted.
  web_contents()->GetController().Reload(ReloadType::NORMAL, false);
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  EXPECT_EQ("text=&select=a", base::UTF16ToASCII(web_contents()->GetTitle()));
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
            base::BindOnce(&NavigationHandleGrabber::SendingNavigationCommitted,
                           base::Unretained(this), navigation_handle));
  }

  bool SendingNavigationCommitted(
      NavigationHandle* navigation_handle,
      NavigationThrottle::ThrottleCheckResult result) {
    if (navigation_handle->GetURL().path() == "/title2.html")
      ExecuteScriptAsync(web_contents(), "document.open();");
    return false;
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

class DocumentUkmSourceIdObserver : public WebContentsObserver {
 public:
  explicit DocumentUkmSourceIdObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  ukm::SourceId GetMainFrameDocumentUkmSourceId() {
    return main_frame_document_ukm_source_id_;
  }
  ukm::SourceId GetSubFrameDocumentUkmSourceId() {
    return sub_frame_document_ukm_source_id_;
  }

 protected:
  // WebContentsObserver:
  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    bool is_main_frame_navigation = navigation_handle->IsInMainFrame();
    // Track the source ids from NavigationRequests for access by browser tests.
    NavigationRequest* request = NavigationRequest::From(navigation_handle);
    ukm::SourceId document_ukm_source_id =
        request->commit_params().document_ukm_source_id;

    if (is_main_frame_navigation)
      main_frame_document_ukm_source_id_ = document_ukm_source_id;
    else
      sub_frame_document_ukm_source_id_ = document_ukm_source_id;
  }

 private:
  ukm::SourceId main_frame_document_ukm_source_id_ = ukm::kInvalidSourceId;
  ukm::SourceId sub_frame_document_ukm_source_id_ = ukm::kInvalidSourceId;
  DISALLOW_COPY_AND_ASSIGN(DocumentUkmSourceIdObserver);
};
}  // namespace

// Verifies that if a frame aborts a navigation right after it starts, it is
// cancelled.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest, FastNavigationAbort) {
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // This test only makes sense for navigations that stay in the same
  // RenderFrame, otherwise the document.open() will run on the previous
  // page's RenderFrame, and the navigation won't get aborted. We need to
  // ensure that we won't trigger a same-site cross-RFH navigation.
  // TODO(crbug.com/1099193): This should also work on cross-RFH same-site
  // navigations.
  DisableProactiveBrowsingInstanceSwapFor(web_contents()->GetMainFrame());

  // Now make a navigation. |observer| will make a document.open() call at
  // ReadyToCommit time - see
  // NavigationHandleGrabber::SendingNavigationCommitted(). The navigation
  // should get aborted because of the document.open() in the navigating RFH.
  NavigationHandleGrabber observer(web_contents());
  const base::string16 title = base::ASCIIToUTF16("done");
  EXPECT_TRUE(
      ExecuteScript(web_contents(), "window.location.href='/title2.html'"));
  observer.WaitForTitle2();
  // Flush IPCs to make sure the renderer didn't tell us to navigate. Need to
  // make two round trips.
  EXPECT_TRUE(ExecuteScript(web_contents(), ""));
  EXPECT_TRUE(ExecuteScript(web_contents(), ""));
  EXPECT_FALSE(observer.committed_title2());
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       TerminationDisablersClearedOnRendererCrash) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GetTestUrl("render_frame_host", "beforeunload.html")));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  RenderFrameHostImpl* main_rfh1 = web_contents()->GetMainFrame();

  EXPECT_TRUE(main_rfh1->GetSuddenTerminationDisablerState(
      blink::mojom::SuddenTerminationDisablerType::kBeforeUnloadHandler));

  // Make the renderer crash.
  RenderProcessHost* renderer_process = main_rfh1->GetProcess();
  RenderProcessHostWatcher crash_observer(
      renderer_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  renderer_process->Shutdown(0);
  crash_observer.Wait();

  EXPECT_FALSE(main_rfh1->GetSuddenTerminationDisablerState(
      blink::mojom::SuddenTerminationDisablerType::kBeforeUnloadHandler));

  // This should not trigger a DCHECK once the renderer sends up the termination
  // disabler flags.
  web_contents()->GetController().Reload(ReloadType::NORMAL, false);
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  RenderFrameHostImpl* main_rfh2 = web_contents()->GetMainFrame();
  EXPECT_TRUE(main_rfh2->GetSuddenTerminationDisablerState(
      blink::mojom::SuddenTerminationDisablerType::kBeforeUnloadHandler));
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

// Allows injecting a fake, test-provided |interface_broker_receiver| into
// DidCommitProvisionalLoad messages in a given |web_contents| instead of the
// real one coming from the renderer process.
class ScopedFakeInterfaceBrokerRequestInjector
    : public DidCommitNavigationInterceptor {
 public:
  explicit ScopedFakeInterfaceBrokerRequestInjector(WebContents* web_contents)
      : DidCommitNavigationInterceptor(web_contents) {}
  ~ScopedFakeInterfaceBrokerRequestInjector() override = default;
  ScopedFakeInterfaceBrokerRequestInjector(
      const ScopedFakeInterfaceBrokerRequestInjector&) = delete;
  ScopedFakeInterfaceBrokerRequestInjector& operator=(
      const ScopedFakeInterfaceBrokerRequestInjector&) = delete;

  // Sets the fake BrowserInterfaceBroker |receiver| to inject into the next
  // incoming DidCommitProvisionalLoad message.
  void set_fake_receiver_for_next_commit(
      mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker> receiver) {
    next_fake_receiver_ = std::move(receiver);
  }

  const GURL& url_of_last_commit() const { return url_of_last_commit_; }

  const mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>&
  original_receiver_of_last_commit() const {
    return original_receiver_of_last_commit_;
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
      original_receiver_of_last_commit_ =
          std::move((*interface_params)->browser_interface_broker_receiver);
      (*interface_params)->browser_interface_broker_receiver =
          std::move(next_fake_receiver_);
    }
    return true;
  }

 private:
  mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
      next_fake_receiver_;
  mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
      original_receiver_of_last_commit_;
  GURL url_of_last_commit_;
};

// Monitors the |broker_receiver_| of the given |render_frame_host| for incoming
// interface requests for |interface_name|, and invokes |callback| synchronously
// just before such a request would be dispatched.
class ScopedInterfaceRequestMonitor
    : public blink::mojom::BrowserInterfaceBrokerInterceptorForTesting {
 public:
  ScopedInterfaceRequestMonitor(RenderFrameHostImpl* render_frame_host,
                                base::StringPiece interface_name,
                                base::RepeatingClosure callback)
      : rfhi_(render_frame_host),
        impl_(receiver().SwapImplForTesting(this)),
        interface_name_(interface_name),
        request_callback_(callback) {}

  ~ScopedInterfaceRequestMonitor() override {
    auto* old_impl = receiver().SwapImplForTesting(impl_);
    DCHECK_EQ(old_impl, this);
  }

 protected:
  // blink::mojom::BrowserInterfaceBrokerInterceptorForTesting:
  blink::mojom::BrowserInterfaceBroker* GetForwardingInterface() override {
    return impl_;
  }

  void GetInterface(mojo::GenericPendingReceiver receiver) override {
    if (receiver.interface_name() == interface_name_)
      request_callback_.Run();
    GetForwardingInterface()->GetInterface(std::move(receiver));
  }

 private:
  mojo::Receiver<blink::mojom::BrowserInterfaceBroker>& receiver() {
    return rfhi_->browser_interface_broker_receiver_for_testing();
  }

  RenderFrameHostImpl* rfhi_;
  blink::mojom::BrowserInterfaceBroker* impl_;

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
// the renderer process will have its |interface_broker_receiver| argument set
// to the receiver end of a new BrowserInterfaceBroker interface connection that
// will be used by the newly committed document to access services exposed by
// the RenderFrameHost.
//
// This test verifies that even if that |interface_broker_receiver| already
// has pending interface receivers, the RenderFrameHost binds the
// BrowserInterfaceBroker receiver in such a way that these pending interface
// receivers are dispatched strictly after
// WebContentsObserver::DidFinishNavigation has fired, so that the receivers
// will be served correctly in the security context of the newly committed
// document (i.e. GetLastCommittedURL/Origin will have been updated).
IN_PROC_BROWSER_TEST_F(
    RenderFrameHostImplBrowserTest,
    EarlyInterfaceRequestsFromNewDocumentDispatchedAfterNavigationFinished) {
  const GURL first_url(embedded_test_server()->GetURL("/title1.html"));
  const GURL second_url(embedded_test_server()->GetURL("/title2.html"));

  // Load a URL that maps to the same SiteInstance as the second URL, to make
  // sure the second navigation will not be cross-process.
  ASSERT_TRUE(NavigateToURL(shell(), first_url));

  // Prepare an PendingReceiver<BrowserInterfaceBroker> with pending interface
  // requests.
  mojo::Remote<blink::mojom::BrowserInterfaceBroker>
      interface_broker_with_pending_requests;
  mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
      interface_broker_receiver_with_pending_receiver =
          interface_broker_with_pending_requests.BindNewPipeAndPassReceiver();
  mojo::Remote<mojom::FrameHostTestInterface> test_interface;
  interface_broker_with_pending_requests->GetInterface(
      test_interface.BindNewPipeAndPassReceiver());

  // Replace the |interface_broker_receiver| argument in the next
  // DidCommitProvisionalLoad message coming from the renderer with the
  // rigged |interface_broker_with_pending_requests| from above.
  ScopedFakeInterfaceBrokerRequestInjector injector(web_contents());
  injector.set_fake_receiver_for_next_commit(
      std::move(interface_broker_receiver_with_pending_receiver));

  // Expect that by the time the interface request for FrameHostTestInterface is
  // dispatched to the RenderFrameHost, WebContentsObserver::DidFinishNavigation
  // will have already been invoked.
  bool did_finish_navigation = false;

  // Start the same-process navigation.
  TestNavigationManager navigation_manager(web_contents(), second_url);
  shell()->LoadURL(second_url);
  EXPECT_TRUE(navigation_manager.WaitForResponse());
  auto* committing_rfh =
      NavigationRequest::From(navigation_manager.GetNavigationHandle())
          ->GetRenderFrameHost();

  DidFinishNavigationObserver navigation_finish_observer(
      committing_rfh, base::BindLambdaForTesting([&did_finish_navigation]() {
        did_finish_navigation = true;
      }));

  base::RunLoop wait_until_interface_request_is_dispatched;
  ScopedInterfaceRequestMonitor monitor(
      committing_rfh, mojom::FrameHostTestInterface::Name_,
      base::BindLambdaForTesting([&]() {
        EXPECT_TRUE(did_finish_navigation);
        wait_until_interface_request_is_dispatched.Quit();
      }));

  // Finish the navigation.
  navigation_manager.WaitForNavigationFinished();
  EXPECT_EQ(second_url, injector.url_of_last_commit());
  EXPECT_TRUE(injector.original_receiver_of_last_commit().is_valid());

  // Wait until the interface request for FrameHostTestInterface is dispatched.
  wait_until_interface_request_is_dispatched.Run();
}

// The BrowserInterfaceBroker interface, which is used by the RenderFrame to
// access Mojo services exposed by the RenderFrameHost, is not
// Channel-associated, thus not synchronized with navigation IPC messages. As a
// result, when the renderer commits a load, the DidCommitProvisional message
// might be at race with GetInterface messages, for example, an interface
// request issued by the previous document in its unload handler might arrive to
// the browser process just a moment after DidCommitProvisionalLoad.
//
// This test verifies that even if there is such a last-second GetInterface
// message originating from the previous document, it is no longer serviced.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       LateInterfaceRequestsFromOldDocumentNotDispatched) {
  const GURL first_url(embedded_test_server()->GetURL("/title1.html"));
  const GURL second_url(embedded_test_server()->GetURL("/title2.html"));

  // Prepare an PendingReceiver<BrowserInterfaceBroker> with no pending
  // requests.
  mojo::Remote<blink::mojom::BrowserInterfaceBroker> interface_broker;
  mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
      interface_broker_receiver = interface_broker.BindNewPipeAndPassReceiver();

  // Set up a cunning mechanism to replace the |interface_broker_receiver|
  // argument in next DidCommitProvisionalLoad message with the rigged
  // |interface_broker_receiver| from above, whose client end is controlled by
  // this test; then trigger a navigation.
  {
    ScopedFakeInterfaceBrokerRequestInjector injector(web_contents());
    injector.set_fake_receiver_for_next_commit(
        std::move(interface_broker_receiver));

    ASSERT_TRUE(NavigateToURL(shell(), first_url));
    ASSERT_EQ(first_url, injector.url_of_last_commit());
    ASSERT_TRUE(injector.original_receiver_of_last_commit().is_valid());
  }

  // The test below only works for same-RFH navigations, so we need to ensure
  // that we won't trigger a same-site cross-RFH navigation.
  DisableProactiveBrowsingInstanceSwapFor(web_contents()->GetMainFrame());

  // Prepare an interface receiver for FrameHostTestInterface.
  mojo::Remote<mojom::FrameHostTestInterface> test_interface;
  auto test_interface_receiver = test_interface.BindNewPipeAndPassReceiver();

  // Set up |dispatched_interface_request_callback| that would be invoked if the
  // interface receiver for FrameHostTestInterface was ever dispatched to the
  // RenderFrameHostImpl.
  base::MockCallback<base::RepeatingClosure>
      dispatched_interface_request_callback;
  auto* main_rfh = web_contents()->GetMainFrame();
  ScopedInterfaceRequestMonitor monitor(
      main_rfh, mojom::FrameHostTestInterface::Name_,
      dispatched_interface_request_callback.Get());

  // Set up the |test_interface request| to arrive on the BrowserInterfaceBroker
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
        interface_broker->GetInterface(std::move(test_interface_receiver));
        std::move(navigation_finished_callback).Run();
      }));

  // The BrowserInterfaceBroker connection that semantically belongs to the old
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
// whether it wants to replace the BrowserInterfaceBroker connection or not.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       InterfaceBrokerRequestIsOptionalForFirstCommit) {
  const GURL main_frame_url(embedded_test_server()->GetURL("/title1.html"));
  const GURL subframe_url(embedded_test_server()->GetURL("/title2.html"));

  mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker> interface_broker;
  auto stub_interface_broker_receiver =
      interface_broker.InitWithNewPipeAndPassReceiver();
  mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
      null_interface_broker_receiver((mojo::NullReceiver()));

  for (auto* interface_broker_receiver :
       {&stub_interface_broker_receiver, &null_interface_broker_receiver}) {
    SCOPED_TRACE(interface_broker_receiver->is_valid());

    ASSERT_TRUE(NavigateToURL(shell(), main_frame_url));

    ScopedFakeInterfaceBrokerRequestInjector injector(web_contents());
    injector.set_fake_receiver_for_next_commit(
        std::move(*interface_broker_receiver));

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

    EXPECT_TRUE(WaitForLoadStop(web_contents()));

    FrameTreeNode* root = web_contents()->GetFrameTree()->root();
    ASSERT_EQ(1u, root->child_count());
    FrameTreeNode* child = root->child_at(0u);

    EXPECT_FALSE(injector.original_receiver_of_last_commit().is_valid());
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
    InterfaceBrokerRequestNotPresentForFirstRealLoadAfterAboutBlankWithRef) {
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
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  ASSERT_EQ(1u, root->child_count());
  FrameTreeNode* child = root->child_at(0u);

  EXPECT_FALSE(child->has_committed_real_load());
  EXPECT_EQ(kSubframeURLTwo, child->current_url());
  EXPECT_EQ(url::Origin::Create(kMainFrameURL), child->current_origin());

  // Set the `src` attribute again to trigger navigation (3).

  TestFrameNavigationObserver commit_observer(child->current_frame_host());
  ScopedFakeInterfaceBrokerRequestInjector injector(web_contents());
  injector.set_fake_receiver_for_next_commit(mojo::NullReceiver());

  ASSERT_TRUE(ExecuteScript(shell(), kNavigateToThreeScript));
  commit_observer.WaitForCommit();
  EXPECT_FALSE(injector.original_receiver_of_last_commit().is_valid());

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
            node->current_frame_host()->GetNetworkIsolationKey());
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

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  CheckURLOriginAndNetworkIsolationKey(root, main_frame_url, main_frame_origin,
                                       expected_main_frame_key);

  // Create iframe.
  ASSERT_TRUE(ExecuteScript(shell(), R"(
      var f = document.createElement('iframe');
      f.id = 'myiframe';
      document.body.append(f);
  )"));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

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

  EXPECT_TRUE(WaitForLoadStop(web_contents()));

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

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  CheckURLOriginAndNetworkIsolationKey(root, main_frame_url, main_frame_origin,
                                       expected_main_frame_key);

  // Create popup.
  WebContentsAddedObserver popup_observer;
  ASSERT_TRUE(ExecuteScript(shell(), "var w = window.open('');"));
  WebContentsImpl* popup =
      static_cast<WebContentsImpl*>(popup_observer.GetWebContents());

  FrameTreeNode* popup_frame = popup->GetMainFrame()->frame_tree_node();
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

// Navigating an iframe to about:blank sets the NetworkIsolationKey differently
// than creating a new frame at about:blank, so needs to be tested.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       NetworkIsolationKeyNavigateIframeToAboutBlank) {
  GURL main_frame_url(embedded_test_server()->GetURL("/page_with_iframe.html"));
  url::Origin origin = url::Origin::Create(main_frame_url);
  net::NetworkIsolationKey expected_network_isolation_key =
      net::NetworkIsolationKey(origin, origin);

  ASSERT_TRUE(NavigateToURL(shell(), main_frame_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  CheckURLOriginAndNetworkIsolationKey(root, main_frame_url, origin,
                                       expected_network_isolation_key);
  ASSERT_EQ(1u, root->child_count());

  CheckURLOriginAndNetworkIsolationKey(
      root->child_at(0), embedded_test_server()->GetURL("/title1.html"), origin,
      expected_network_isolation_key);
  RenderFrameHost* iframe = root->child_at(0)->current_frame_host();

  TestFrameNavigationObserver commit_observer(iframe);
  ASSERT_TRUE(ExecuteScript(iframe, "window.location = 'about:blank'"));
  commit_observer.WaitForCommit();

  ASSERT_EQ(1u, root->child_count());
  CheckURLOriginAndNetworkIsolationKey(root->child_at(0), GURL("about:blank"),
                                       origin, expected_network_isolation_key);
  // Site-for-cookies should be consistent with the NetworkIsolationKey.
  EXPECT_TRUE(
      root->child_at(0)
          ->current_frame_host()
          ->ComputeSiteForCookies()
          .IsFirstParty(
              expected_network_isolation_key.GetTopFrameOrigin()->GetURL()));
}

// An iframe that starts at about:blank and is itself nested in a cross-site
// iframe should have the same NetworkIsolationKey as its parent.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       NetworkIsolationKeyNestedCrossSiteAboutBlankIframe) {
  const char kSiteA[] = "a.test";
  const char kSiteB[] = "b.test";

  // Navigation and creation paths for determining about:blank's
  // NetworkIsolationKey are different. This test is for the NIK-on-creation
  // path, so need a URL that will start with a nested about:blank iframe.
  GURL nested_iframe_url = GURL("about:blank");
  GURL cross_site_iframe_url(embedded_test_server()->GetURL(
      kSiteB, net::test_server::GetFilePathWithReplacements(
                  "/page_with_iframe.html",
                  base::StringPairs{
                      {"title1.html", nested_iframe_url.spec().c_str()}})));
  GURL main_frame_url(embedded_test_server()->GetURL(
      kSiteA, net::test_server::GetFilePathWithReplacements(
                  "/page_with_iframe.html",
                  base::StringPairs{
                      {"title1.html", cross_site_iframe_url.spec().c_str()}})));

  // This should be the origin for both the iframes.
  url::Origin iframe_origin = url::Origin::Create(cross_site_iframe_url);

  url::Origin main_frame_origin = url::Origin::Create(main_frame_url);

  net::NetworkIsolationKey expected_iframe_network_isolation_key(
      main_frame_origin, iframe_origin);
  net::NetworkIsolationKey expected_main_frame_network_isolation_key(
      main_frame_origin, main_frame_origin);

  ASSERT_TRUE(NavigateToURL(shell(), main_frame_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  CheckURLOriginAndNetworkIsolationKey(
      root, main_frame_url, main_frame_origin,
      expected_main_frame_network_isolation_key);

  ASSERT_EQ(1u, root->child_count());
  FrameTreeNode* cross_site_iframe = root->child_at(0);
  CheckURLOriginAndNetworkIsolationKey(cross_site_iframe, cross_site_iframe_url,
                                       iframe_origin,
                                       expected_iframe_network_isolation_key);
  // Cross site iframes should have an empty site-for-cookies.
  EXPECT_TRUE(cross_site_iframe->current_frame_host()
                  ->ComputeSiteForCookies()
                  .IsNull());

  ASSERT_EQ(1u, cross_site_iframe->child_count());
  FrameTreeNode* nested_iframe = cross_site_iframe->child_at(0);
  CheckURLOriginAndNetworkIsolationKey(nested_iframe, nested_iframe_url,
                                       iframe_origin,
                                       expected_iframe_network_isolation_key);
  // Cross site iframes should have an empty site-for-cookies.
  EXPECT_TRUE(
      nested_iframe->current_frame_host()->ComputeSiteForCookies().IsNull());
}

// An iframe that's navigated to about:blank and is itself nested in a
// cross-site iframe should have the same NetworkIsolationKey as its parent. The
// navigation path is a bit different from the creation path in the above path,
// so needs to be tested as well.
IN_PROC_BROWSER_TEST_F(
    RenderFrameHostImplBrowserTest,
    NetworkIsolationKeyNavigateNestedCrossSiteAboutBlankIframe) {
  const char kSiteA[] = "a.test";
  const char kSiteB[] = "b.test";
  const char kSiteC[] = "c.test";

  // Start with a.test iframing b.test iframing c.test.  Innermost iframe should
  // not be on the same site as the middle iframe, so that navigations to/from
  // about:blank initiated by b.test change its origin.
  GURL innermost_iframe_url(
      embedded_test_server()->GetURL(kSiteC, "/title1.html"));
  GURL middle_iframe_url(embedded_test_server()->GetURL(
      kSiteB, net::test_server::GetFilePathWithReplacements(
                  "/page_with_iframe.html",
                  base::StringPairs{
                      {"title1.html", innermost_iframe_url.spec().c_str()}})));
  GURL main_frame_url(embedded_test_server()->GetURL(
      kSiteA, net::test_server::GetFilePathWithReplacements(
                  "/page_with_iframe.html",
                  base::StringPairs{
                      {"title1.html", middle_iframe_url.spec().c_str()}})));

  url::Origin innermost_iframe_origin =
      url::Origin::Create(innermost_iframe_url);
  url::Origin middle_iframe_origin = url::Origin::Create(middle_iframe_url);
  url::Origin main_frame_origin = url::Origin::Create(main_frame_url);

  net::NetworkIsolationKey expected_innermost_iframe_network_isolation_key(
      main_frame_origin, innermost_iframe_origin);
  net::NetworkIsolationKey expected_middle_iframe_network_isolation_key(
      main_frame_origin, middle_iframe_origin);
  net::NetworkIsolationKey expected_main_frame_network_isolation_key(
      main_frame_origin, main_frame_origin);

  ASSERT_TRUE(NavigateToURL(shell(), main_frame_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  CheckURLOriginAndNetworkIsolationKey(
      root, main_frame_url, main_frame_origin,
      expected_main_frame_network_isolation_key);

  ASSERT_EQ(1u, root->child_count());
  FrameTreeNode* middle_iframe = root->child_at(0);
  CheckURLOriginAndNetworkIsolationKey(
      middle_iframe, middle_iframe_url, middle_iframe_origin,
      expected_middle_iframe_network_isolation_key);
  // Cross site iframes should have an empty site-for-cookies.
  EXPECT_TRUE(
      middle_iframe->current_frame_host()->ComputeSiteForCookies().IsNull());

  ASSERT_EQ(1u, middle_iframe->child_count());
  FrameTreeNode* innermost_iframe = middle_iframe->child_at(0);
  CheckURLOriginAndNetworkIsolationKey(
      innermost_iframe, innermost_iframe_url, innermost_iframe_origin,
      expected_innermost_iframe_network_isolation_key);
  // Cross site iframes should have an empty site-for-cookies.
  EXPECT_TRUE(
      innermost_iframe->current_frame_host()->ComputeSiteForCookies().IsNull());

  // The middle iframe navigates the innermost iframe to about:blank. It should
  // then have the same NetworkIsolationKey as the middle iframe.
  TestNavigationObserver nav_observer1(web_contents());
  ASSERT_TRUE(ExecJs(
      middle_iframe->current_frame_host(),
      "var iframe = "
      "document.getElementById('test_iframe');iframe.src='about:blank';"));
  nav_observer1.WaitForNavigationFinished();
  CheckURLOriginAndNetworkIsolationKey(
      innermost_iframe, GURL("about:blank"), middle_iframe_origin,
      expected_middle_iframe_network_isolation_key);
  // Cross site iframes should have an empty site-for-cookies.
  EXPECT_TRUE(
      middle_iframe->current_frame_host()->ComputeSiteForCookies().IsNull());

  // The innermost iframe, now at about:blank, navigates itself back its
  // original location, which should make it use c.test's NIK again.
  TestNavigationObserver nav_observer2(web_contents());
  ASSERT_TRUE(
      ExecJs(innermost_iframe->current_frame_host(), "window.history.back();"));
  nav_observer2.WaitForNavigationFinished();
  CheckURLOriginAndNetworkIsolationKey(
      innermost_iframe, innermost_iframe_url, innermost_iframe_origin,
      expected_innermost_iframe_network_isolation_key);
  // Cross site iframes should have an empty site-for-cookies.
  EXPECT_TRUE(
      innermost_iframe->current_frame_host()->ComputeSiteForCookies().IsNull());

  // The innermost iframe, now at c.test, navigates itself back to about:blank.
  // Despite c.test initiating the navigation, the iframe should be using
  // b.test's NIK, since the navigation entry was created by a navigation
  // initiated by b.test.
  TestNavigationObserver nav_observer3(web_contents());
  ASSERT_TRUE(ExecJs(innermost_iframe->current_frame_host(),
                     "window.history.forward();"));
  nav_observer3.WaitForNavigationFinished();
  CheckURLOriginAndNetworkIsolationKey(
      innermost_iframe, GURL("about:blank"), middle_iframe_origin,
      expected_middle_iframe_network_isolation_key);
  // Cross site iframes should have an empty site-for-cookies.
  EXPECT_TRUE(
      innermost_iframe->current_frame_host()->ComputeSiteForCookies().IsNull());
}

// Verify that if the UMA histograms are correctly recording if interface
// broker requests are getting dropped because they racily arrive from the
// previously active document (after the next navigation already committed).
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       DroppedInterfaceRequestCounter) {
  const GURL kUrl1(embedded_test_server()->GetURL("/title1.html"));
  const GURL kUrl2(embedded_test_server()->GetURL("/title2.html"));
  const GURL kUrl3(embedded_test_server()->GetURL("/title3.html"));
  const GURL kUrl4(embedded_test_server()->GetURL("/empty.html"));

  // The 31-bit hash of the string "content.mojom.MojoWebTestHelper".
  const int32_t kHashOfContentMojomMojoWebTestHelper = 0x77b7b3d6;

  // Client ends of the fake interface broker receivers injected for the first
  // and second navigations.
  mojo::Remote<blink::mojom::BrowserInterfaceBroker> interface_broker_1;
  mojo::Remote<blink::mojom::BrowserInterfaceBroker> interface_broker_2;

  base::RunLoop wait_until_connection_error_loop_1;
  base::RunLoop wait_until_connection_error_loop_2;

  {
    ScopedFakeInterfaceBrokerRequestInjector injector(web_contents());
    injector.set_fake_receiver_for_next_commit(
        interface_broker_1.BindNewPipeAndPassReceiver());
    interface_broker_1.set_disconnect_handler(
        wait_until_connection_error_loop_1.QuitClosure());
    ASSERT_TRUE(NavigateToURL(shell(), kUrl1));
  }

  // The test below only makes sense for same-RFH navigations, so we need to
  // ensure that we won't trigger a same-site cross-RFH navigation.
  DisableProactiveBrowsingInstanceSwapFor(web_contents()->GetMainFrame());

  {
    ScopedFakeInterfaceBrokerRequestInjector injector(web_contents());
    injector.set_fake_receiver_for_next_commit(
        interface_broker_2.BindNewPipeAndPassReceiver());
    interface_broker_2.set_disconnect_handler(
        wait_until_connection_error_loop_2.QuitClosure());
    ASSERT_TRUE(NavigateToURL(shell(), kUrl2));
  }

  // Simulate two interface requests corresponding to the first navigation
  // arrived after the second navigation was committed, hence were dropped.
  interface_broker_1->GetInterface(
      mojo::PendingReceiver<mojom::MojoWebTestHelper>(
          CreateDisconnectedMessagePipeHandle()));
  interface_broker_1->GetInterface(
      mojo::PendingReceiver<mojom::MojoWebTestHelper>(
          CreateDisconnectedMessagePipeHandle()));

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
  interface_broker_2->GetInterface(
      mojo::PendingReceiver<mojom::MojoWebTestHelper>(
          CreateDisconnectedMessagePipeHandle()));

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
  interface_broker_1->GetInterface(
      mojo::PendingReceiver<mojom::MojoWebTestHelper>(
          CreateDisconnectedMessagePipeHandle()));
  interface_broker_2->GetInterface(
      mojo::PendingReceiver<mojom::MojoWebTestHelper>(
          CreateDisconnectedMessagePipeHandle()));

  // The interface connections should be broken.
  wait_until_connection_error_loop_1.Run();
  wait_until_connection_error_loop_2.Run();
}

// Regression test for https://crbug.com/852350
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       GetCanonicalUrlAfterRendererCrash) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GetTestUrl("render_frame_host", "beforeunload.html")));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  RenderFrameHostImpl* main_frame = web_contents()->GetMainFrame();

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
  NavigationHandleObserver navigation_observer(web_contents(), blocked_url);
  EXPECT_TRUE(NavigateIframeToURL(web_contents(), "child0", blocked_url));

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
  EXPECT_TRUE(navigation_observer.has_committed());
  EXPECT_TRUE(navigation_observer.is_error());
  EXPECT_EQ(blocked_url, frame->GetLastCommittedURL());
  EXPECT_EQ(net::ERR_BLOCKED_BY_RESPONSE, navigation_observer.net_error_code());
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       BeforeUnloadDialogSuppressedForDiscard) {
  TestJavaScriptDialogManager dialog_manager;
  web_contents()->SetDelegate(&dialog_manager);

  EXPECT_TRUE(NavigateToURL(
      shell(), GetTestUrl("render_frame_host", "beforeunload.html")));
  // Disable the hang monitor, otherwise there will be a race between the
  // beforeunload dialog and the beforeunload hang timer.
  web_contents()->GetMainFrame()->DisableBeforeUnloadHangMonitorForTesting();

  // Give the page a user gesture so javascript beforeunload works, and then
  // dispatch a before unload with discard as a reason. This should return
  // without any dialog being seen.
  web_contents()->GetMainFrame()->ExecuteJavaScriptWithUserGestureForTests(
      base::string16());
  web_contents()->GetMainFrame()->DispatchBeforeUnload(
      RenderFrameHostImpl::BeforeUnloadType::DISCARD, false);
  dialog_manager.Wait();
  EXPECT_EQ(0, dialog_manager.num_beforeunload_dialogs_seen());
  EXPECT_EQ(1, dialog_manager.num_beforeunload_fired_seen());
  EXPECT_FALSE(dialog_manager.proceed());

  web_contents()->SetDelegate(nullptr);
  web_contents()->SetJavaScriptDialogManagerForTesting(nullptr);
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       PendingDialogMakesDiscardUnloadReturnFalse) {
  TestJavaScriptDialogManager dialog_manager;
  web_contents()->SetDelegate(&dialog_manager);

  EXPECT_TRUE(NavigateToURL(
      shell(), GetTestUrl("render_frame_host", "beforeunload.html")));
  // Disable the hang monitor, otherwise there will be a race between the
  // beforeunload dialog and the beforeunload hang timer.
  web_contents()->GetMainFrame()->DisableBeforeUnloadHangMonitorForTesting();

  // Give the page a user gesture so javascript beforeunload works, and then
  // dispatch a before unload with discard as a reason. This should return
  // without any dialog being seen.
  web_contents()->GetMainFrame()->ExecuteJavaScriptWithUserGestureForTests(
      base::string16());

  // Launch an alert javascript dialog. This pending dialog should block a
  // subsequent discarding before unload request.
  web_contents()->GetMainFrame()->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16("setTimeout(function(){alert('hello');}, 10);"),
      base::NullCallback());
  dialog_manager.Wait();
  EXPECT_EQ(0, dialog_manager.num_beforeunload_dialogs_seen());
  EXPECT_EQ(0, dialog_manager.num_beforeunload_fired_seen());

  // Dispatch a before unload request while the first is still blocked
  // on the dialog, and expect it to return false immediately (synchronously).
  web_contents()->GetMainFrame()->DispatchBeforeUnload(
      RenderFrameHostImpl::BeforeUnloadType::DISCARD, false);
  dialog_manager.Wait();
  EXPECT_EQ(0, dialog_manager.num_beforeunload_dialogs_seen());
  EXPECT_EQ(1, dialog_manager.num_beforeunload_fired_seen());
  EXPECT_FALSE(dialog_manager.proceed());

  // Clear the existing javascript dialog so that the associated IPC message
  // doesn't leak.
  dialog_manager.Run(true, base::string16());

  web_contents()->SetDelegate(nullptr);
  web_contents()->SetJavaScriptDialogManagerForTesting(nullptr);
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
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  auto* frame = web_contents()->GetMainFrame();
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
  GURL main_frame(embedded_test_server()->GetURL("/iframe_out_of_view.html"));
  GURL child_url(embedded_test_server()->GetURL("/hello.html"));

  // This will set up the page frame tree as A(A1()).
  ASSERT_TRUE(NavigateToURL(shell(), main_frame));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* nested_iframe_node = root->child_at(0);
  NavigateFrameToURL(nested_iframe_node, child_url);

  ASSERT_EQ(blink::mojom::FrameVisibility::kRenderedOutOfViewport,
            nested_iframe_node->current_frame_host()->visibility());
}

// Test that a frame is visible/hidden depending on its WebContents visibility
// state.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest, VisibilityChildInView) {
  GURL main_frame(embedded_test_server()->GetURL("/iframe_clipped.html"));
  GURL child_url(embedded_test_server()->GetURL("/hello.html"));

  // This will set up the page frame tree as A(A1()).
  ASSERT_TRUE(NavigateToURL(shell(), main_frame));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* nested_iframe_node = root->child_at(0);
  NavigateFrameToURL(nested_iframe_node, child_url);

  ASSERT_EQ(blink::mojom::FrameVisibility::kRenderedInViewport,
            nested_iframe_node->current_frame_host()->visibility());
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       OriginOfFreshFrame_Subframe_NavCancelledByDocWrite) {
  NavigationController& controller = web_contents()->GetController();
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
  EXPECT_EQ(main_origin.Serialize(), EvalJs(web_contents(), script));

  // The subframe navigation should be cancelled and therefore shouldn't
  // contribute an extra history entry.
  EXPECT_EQ(1, controller.GetEntryCount());

  // Browser-side origin should match the renderer-side origin.
  // See also https://crbug.com/932067.
  ASSERT_EQ(2u, web_contents()->GetAllFrames().size());
  RenderFrameHost* subframe = web_contents()->GetAllFrames()[1];
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
  NavigationController& controller = web_contents()->GetController();
  GURL main_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), main_url));
  EXPECT_EQ(1, controller.GetEntryCount());
  url::Origin main_origin = url::Origin::Create(main_url);

  // Navigate a sandboxed frame to a cross-origin '/hung'.
  RenderFrameHostCreatedObserver subframe_observer(web_contents());
  const char kScriptTemplate[] = R"(
      const frame = document.createElement('iframe');
      frame.sandbox = 'allow-scripts';
      frame.src = $1;
      document.body.appendChild(frame);
  )";
  GURL cross_site_url(embedded_test_server()->GetURL("bar.com", "/hung"));
  std::string script = JsReplace(kScriptTemplate, cross_site_url);
  EXPECT_TRUE(ExecJs(web_contents(), script));

  // Wait for a new subframe, but ignore the frame returned by
  // |subframe_observer| (it might be the speculative one, not the current one).
  subframe_observer.Wait();
  ASSERT_EQ(2u, web_contents()->GetAllFrames().size());
  RenderFrameHost* subframe = web_contents()->GetAllFrames()[1];

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
  NavigationController& controller = web_contents()->GetController();
  GURL main_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), main_url));
  EXPECT_EQ(1, controller.GetEntryCount());
  url::Origin main_origin = url::Origin::Create(main_url);

  // Create a new about:blank subframe and document.write into it.
  TestNavigationObserver load_observer(web_contents());
  RenderFrameHostCreatedObserver subframe_observer(web_contents());
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
  ExecuteScriptAsync(web_contents(), kScript);

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
  ASSERT_EQ(2u, web_contents()->GetAllFrames().size());
  RenderFrameHost* subframe2 = web_contents()->GetAllFrames()[1];
  EXPECT_EQ(subframe, subframe2);  // No swaps are expected.
  EXPECT_EQ(main_origin, subframe2->GetLastCommittedOrigin());
  EXPECT_EQ(main_origin.Serialize(), EvalJs(subframe2, "window.origin"));
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       OriginOfFreshFrame_Popup_NavCancelledByDocWrite) {
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
  EXPECT_EQ(main_origin.Serialize(), EvalJs(web_contents(), script));

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
  GURL main_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), main_url));
  url::Origin main_origin = url::Origin::Create(main_url);

  // Create a new about:blank popup and document.write into it.
  WebContentsAddedObserver popup_observer;
  TestNavigationObserver load_observer(web_contents());
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
  ExecuteScriptAsync(web_contents(), kScript);

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

  RenderFrameHostImpl* main_frame = web_contents()->GetMainFrame();
  EXPECT_TRUE(main_frame->AccessibilityIsMainFrame());

  ASSERT_EQ(1u, main_frame->child_count());
  RenderFrameHostImpl* iframe = main_frame->child_at(0)->current_frame_host();
  EXPECT_FALSE(iframe->AccessibilityIsMainFrame());
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       RequestSnapshotAXTreeAfterRenderProcessHostDeath) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));
  auto* rfh = web_contents()->GetMainFrame();

  // Kill the renderer process.
  RenderProcessHostWatcher crash_observer(
      rfh->GetProcess(), RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  rfh->GetProcess()->Shutdown(0);
  crash_observer.Wait();

  // Call RequestAXSnapshotTree method. The browser process should not crash.
  rfh->RequestAXTreeSnapshot(
      base::BindOnce([](const ui::AXTreeUpdate& snapshot) { NOTREACHED(); }),
      ui::AXMode::kWebContents);

  base::RunLoop().RunUntilIdle();

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

  auto* rfh = web_contents()->GetMainFrame();
  TestNavigationObserver observer(web_contents());
  EXPECT_TRUE(ExecJs(rfh, JsReplace("setUrl($1, true);", object_url)));
  observer.Wait();
  EXPECT_EQ(rfh->GetLastCommittedOrigin().Serialize(),
            EvalJs(web_contents(), "window.origin"));
}

// Regression test for crbug.com/953934. It shouldn't crash if we quickly remove
// an object element in the middle of its failing navigation.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       NoCrashOnRemoveObjectElementWithInvalidData) {
  GURL url = GetFileURL(
      FILE_PATH_LITERAL("remove_object_element_with_invalid_data.html"));

  RenderProcessHostWatcher crash_observer(
      web_contents(), RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);

  // This navigates to a page with an object element that will fail to load.
  // When document load event hits, it'll attempt to remove that object element.
  // This might happen while the object element's failed commit is underway.
  // To make sure we hit these conditions and that we don't exit the test too
  // soon, let's wait until the document.readyState finalizes. We don't really
  // care if that succeeds since, in the failing case, the renderer is crashing.
  EXPECT_TRUE(NavigateToURL(shell(), url));
  ignore_result(WaitForRenderFrameReady(web_contents()->GetMainFrame()));

  EXPECT_TRUE(crash_observer.did_exit_normally());
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       SchedulerTrackedFeatures) {
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  RenderFrameHostImpl* main_frame = web_contents()->GetMainFrame();
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
  main_frame = web_contents()->GetMainFrame();
  main_frame->DidChangeActiveSchedulerTrackedFeatures(0b0u);
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       ComputeIsolationInfoForNavigationSiteForCookies) {
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
    RenderFrameHostImpl* main_frame = web_contents()->GetMainFrame();

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

    EXPECT_EQ("a.com", main_frame->ComputeIsolationInfoForNavigation(url)
                           .site_for_cookies()
                           .registrable_domain());
    EXPECT_EQ("b.com", main_frame->ComputeIsolationInfoForNavigation(b_url)
                           .site_for_cookies()
                           .registrable_domain());
    EXPECT_EQ("c.com", main_frame->ComputeIsolationInfoForNavigation(c_url)
                           .site_for_cookies()
                           .registrable_domain());

    // a.com -> a.com frame being navigated.
    EXPECT_EQ("a.com", child_a->current_frame_host()
                           ->ComputeIsolationInfoForNavigation(url)
                           .site_for_cookies()
                           .registrable_domain());
    EXPECT_EQ("a.com", child_a->current_frame_host()
                           ->ComputeIsolationInfoForNavigation(b_url)
                           .site_for_cookies()
                           .registrable_domain());
    EXPECT_EQ("a.com", child_a->current_frame_host()
                           ->ComputeIsolationInfoForNavigation(c_url)
                           .site_for_cookies()
                           .registrable_domain());

    // a.com -> a.com -> b.com frame being navigated.

    // The first case here is especially interesting, since we go to
    // a/a/a from a/a/b. We currently treat this as all first-party, but there
    // is a case to be made for doing it differently, due to involvement of b.
    EXPECT_EQ("a.com", child_b->current_frame_host()
                           ->ComputeIsolationInfoForNavigation(url)
                           .site_for_cookies()
                           .registrable_domain());
    EXPECT_EQ("a.com", child_b->current_frame_host()
                           ->ComputeIsolationInfoForNavigation(b_url)
                           .site_for_cookies()
                           .registrable_domain());
    EXPECT_EQ("a.com", child_b->current_frame_host()
                           ->ComputeIsolationInfoForNavigation(c_url)
                           .site_for_cookies()
                           .registrable_domain());

    // a.com -> c.com frame being navigated.
    EXPECT_EQ("a.com", child_c->current_frame_host()
                           ->ComputeIsolationInfoForNavigation(url)
                           .site_for_cookies()
                           .registrable_domain());
    EXPECT_EQ("a.com", child_c->current_frame_host()
                           ->ComputeIsolationInfoForNavigation(b_url)
                           .site_for_cookies()
                           .registrable_domain());
    EXPECT_EQ("a.com", child_c->current_frame_host()
                           ->ComputeIsolationInfoForNavigation(c_url)
                           .site_for_cookies()
                           .registrable_domain());

    // a.com -> a.com -> b.com -> d.com frame being navigated.
    EXPECT_EQ("", child_d->current_frame_host()
                      ->ComputeIsolationInfoForNavigation(url)
                      .site_for_cookies()
                      .registrable_domain());
    EXPECT_EQ("", child_d->current_frame_host()
                      ->ComputeIsolationInfoForNavigation(b_url)
                      .site_for_cookies()
                      .registrable_domain());
    EXPECT_EQ("", child_d->current_frame_host()
                      ->ComputeIsolationInfoForNavigation(c_url)
                      .site_for_cookies()
                      .registrable_domain());
  }

  // Now try with a trusted scheme that gives first-partiness.
  GURL trusty_url(kTrustMeUrl);
  EXPECT_TRUE(NavigateToURL(shell(), trusty_url));
  {
    RenderFrameHostImpl* main_frame = web_contents()->GetMainFrame();
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
    EXPECT_TRUE(net::SiteForCookies::FromUrl(url).IsEquivalent(
        main_frame->ComputeIsolationInfoForNavigation(url).site_for_cookies()));
    EXPECT_TRUE(net::SiteForCookies::FromUrl(b_url).IsEquivalent(
        main_frame->ComputeIsolationInfoForNavigation(b_url)
            .site_for_cookies()));
    EXPECT_TRUE(net::SiteForCookies::FromUrl(c_url).IsEquivalent(
        main_frame->ComputeIsolationInfoForNavigation(c_url)
            .site_for_cookies()));

    // Child navigation gets the magic scheme.
    EXPECT_TRUE(net::SiteForCookies::FromUrl(trusty_url)
                    .IsEquivalent(child_aa->current_frame_host()
                                      ->ComputeIsolationInfoForNavigation(url)
                                      .site_for_cookies()));
    EXPECT_TRUE(net::SiteForCookies::FromUrl(trusty_url)
                    .IsEquivalent(child_aa->current_frame_host()
                                      ->ComputeIsolationInfoForNavigation(b_url)
                                      .site_for_cookies()));
    EXPECT_TRUE(net::SiteForCookies::FromUrl(trusty_url)
                    .IsEquivalent(child_aa->current_frame_host()
                                      ->ComputeIsolationInfoForNavigation(c_url)
                                      .site_for_cookies()));

    EXPECT_TRUE(net::SiteForCookies::FromUrl(trusty_url)
                    .IsEquivalent(child_aabd->current_frame_host()
                                      ->ComputeIsolationInfoForNavigation(url)
                                      .site_for_cookies()));
    EXPECT_TRUE(net::SiteForCookies::FromUrl(trusty_url)
                    .IsEquivalent(child_aabd->current_frame_host()
                                      ->ComputeIsolationInfoForNavigation(b_url)
                                      .site_for_cookies()));
    EXPECT_TRUE(net::SiteForCookies::FromUrl(trusty_url)
                    .IsEquivalent(child_aabd->current_frame_host()
                                      ->ComputeIsolationInfoForNavigation(c_url)
                                      .site_for_cookies()));
  }

  // Test trusted scheme that gives first-partiness if the url is secure.
  GURL trusty_if_secure_url(kTrustMeIfEmbeddingSecureUrl);
  EXPECT_TRUE(NavigateToURL(shell(), trusty_if_secure_url));
  {
    RenderFrameHostImpl* main_frame = web_contents()->GetMainFrame();
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
    EXPECT_TRUE(net::SiteForCookies::FromUrl(url).IsEquivalent(
        main_frame->ComputeIsolationInfoForNavigation(url).site_for_cookies()));
    EXPECT_TRUE(net::SiteForCookies::FromUrl(b_url).IsEquivalent(
        main_frame->ComputeIsolationInfoForNavigation(b_url)
            .site_for_cookies()));
    EXPECT_TRUE(
        net::SiteForCookies::FromUrl(secure_url)
            .IsEquivalent(
                main_frame->ComputeIsolationInfoForNavigation(secure_url)
                    .site_for_cookies()));

    // Child navigation gets the magic scheme iff secure.
    EXPECT_TRUE(child_aa->current_frame_host()
                    ->ComputeIsolationInfoForNavigation(url)
                    .site_for_cookies()
                    .IsNull());
    EXPECT_TRUE(child_aa->current_frame_host()
                    ->ComputeIsolationInfoForNavigation(b_url)
                    .site_for_cookies()
                    .IsNull());
    EXPECT_TRUE(
        net::SiteForCookies::FromUrl(trusty_url)
            .IsEquivalent(child_aa->current_frame_host()
                              ->ComputeIsolationInfoForNavigation(secure_url)
                              .site_for_cookies()));

    EXPECT_TRUE(child_aabd->current_frame_host()
                    ->ComputeIsolationInfoForNavigation(url)
                    .site_for_cookies()
                    .IsNull());
    EXPECT_TRUE(child_aabd->current_frame_host()
                    ->ComputeIsolationInfoForNavigation(b_url)
                    .site_for_cookies()
                    .IsNull());
    EXPECT_TRUE(
        net::SiteForCookies::FromUrl(trusty_url)
            .IsEquivalent(child_aabd->current_frame_host()
                              ->ComputeIsolationInfoForNavigation(secure_url)
                              .site_for_cookies()));
  }

  SetBrowserClientForTesting(old_client);
}

// Test that when ancestor iframes differ in scheme that the SiteForCookies
// state is updated accordingly.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       ComputeSiteForCookiesSchemefulIsSameForAncestorFrames) {
  https_server()->ServeFilesFromSourceDirectory(GetTestDataFilePath());
  https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  ASSERT_TRUE(https_server()->Start());

  GURL url = https_server()->GetURL(
      "a.test", "/cross_site_iframe_factory.html?a.test(a.test)");
  GURL insecure_url = embedded_test_server()->GetURL(
      "a.test", "/cross_site_iframe_factory.html?a.test(a.test(a.test))");
  GURL other_url = https_server()->GetURL("c.test", "/");
  EXPECT_TRUE(NavigateToURL(shell(), insecure_url));
  {
    RenderFrameHostImpl* main_frame = web_contents()->GetMainFrame();

    EXPECT_EQ("a.test", main_frame->GetLastCommittedURL().host());
    EXPECT_EQ("http", main_frame->frame_tree_node()->current_origin().scheme());
    ASSERT_EQ(1u, main_frame->child_count());
    FrameTreeNode* child = main_frame->child_at(0);
    EXPECT_EQ("a.test", child->current_url().host());
    EXPECT_EQ("http", child->current_origin().scheme());
    ASSERT_EQ(1u, child->child_count());
    FrameTreeNode* grandchild = child->child_at(0);
    EXPECT_EQ("a.test", grandchild->current_url().host());

    // Both the frames above grandchild are the same scheme, so
    // SiteForCookies::schemefully_same() should indicate that.
    EXPECT_TRUE(child->current_frame_host()
                    ->ComputeIsolationInfoForNavigation(other_url)
                    .site_for_cookies()
                    .schemefully_same());
    EXPECT_EQ("a.test", child->current_frame_host()
                            ->ComputeIsolationInfoForNavigation(other_url)
                            .site_for_cookies()
                            .registrable_domain());

    net::SiteForCookies grandchild_same_scheme =
        grandchild->current_frame_host()->ComputeSiteForCookies();
    EXPECT_TRUE(grandchild_same_scheme.schemefully_same());
    EXPECT_EQ("a.test", grandchild_same_scheme.registrable_domain());

    net::SiteForCookies grandchild_same_scheme_navigation =
        grandchild->current_frame_host()
            ->ComputeIsolationInfoForNavigation(other_url)
            .site_for_cookies();
    EXPECT_TRUE(grandchild_same_scheme_navigation.schemefully_same());
    EXPECT_EQ("a.test", grandchild_same_scheme_navigation.registrable_domain());

    // Navigate the middle child frame to https.
    NavigateFrameToURL(child, url);
    EXPECT_EQ("a.test", child->current_url().host());
    EXPECT_EQ("https", child->current_origin().scheme());
    EXPECT_EQ(1u, child->child_count());

    grandchild = child->child_at(0);

    // Now the frames above grandchild differ only in scheme. SiteForCookies
    // should be the same except that schemefully_same() should be false.
    net::SiteForCookies grandchild_cross_scheme =
        grandchild->current_frame_host()->ComputeSiteForCookies();
    EXPECT_FALSE(grandchild_cross_scheme.schemefully_same());
    EXPECT_EQ("a.test", grandchild_cross_scheme.registrable_domain());

    net::SiteForCookies grandchild_cross_scheme_navigation =
        grandchild->current_frame_host()
            ->ComputeIsolationInfoForNavigation(other_url)
            .site_for_cookies();
    EXPECT_FALSE(grandchild_cross_scheme_navigation.schemefully_same());
    EXPECT_EQ("a.test",
              grandchild_cross_scheme_navigation.registrable_domain());

    // IsEquivalent() doesn't check schemefully_same.
    EXPECT_TRUE(grandchild_cross_scheme.IsEquivalent(grandchild_same_scheme));
    EXPECT_TRUE(grandchild_cross_scheme_navigation.IsEquivalent(
        grandchild_same_scheme_navigation));
  }
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       ComputeIsolationInfoForNavigationSiteForCookiesSandbox) {
  // Test sandboxed subframe.
  {
    GURL url = embedded_test_server()->GetURL(
        "a.com",
        "/cross_site_iframe_factory.html?a(a{sandbox-allow-scripts}(a),"
        "a{sandbox-allow-scripts,sandbox-allow-same-origin}(a))");

    EXPECT_TRUE(NavigateToURL(shell(), url));

    RenderFrameHostImpl* main_frame = web_contents()->GetMainFrame();

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
                    ->ComputeIsolationInfoForNavigation(url)
                    .site_for_cookies()
                    .IsNull());

    // |child_a2a| frame navigation should be same-site since its sandboxed
    // parent is sandbox-same-origin.
    EXPECT_EQ("a.com", child_a2a->current_frame_host()
                           ->ComputeIsolationInfoForNavigation(url)
                           .site_for_cookies()
                           .registrable_domain());
  }

  // Test sandboxed main frame.
  {
    GURL url =
        embedded_test_server()->GetURL("a.com", "/csp_sandboxed_frame.html");
    EXPECT_TRUE(NavigateToURL(shell(), url));

    RenderFrameHostImpl* main_frame = web_contents()->GetMainFrame();
    EXPECT_EQ(url, main_frame->GetLastCommittedURL());
    EXPECT_TRUE(main_frame->GetLastCommittedOrigin().opaque());

    ASSERT_EQ(2u, main_frame->child_count());
    FrameTreeNode* child_a = main_frame->child_at(0);
    EXPECT_EQ("a.com", child_a->current_url().host());
    EXPECT_TRUE(
        child_a->current_frame_host()->GetLastCommittedOrigin().opaque());

    EXPECT_TRUE(child_a->current_frame_host()
                    ->ComputeIsolationInfoForNavigation(url)
                    .site_for_cookies()
                    .IsNull());
  }
}

IN_PROC_BROWSER_TEST_F(
    RenderFrameHostImplBrowserTest,
    ComputeIsolationInfoForNavigationSiteForCookiesAboutBlank) {
  GURL url = embedded_test_server()->GetURL(
      "a.com", "/page_with_blank_iframe_tree.html");

  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderFrameHostImpl* main_frame = web_contents()->GetMainFrame();

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
                         ->ComputeIsolationInfoForNavigation(url)
                         .site_for_cookies()
                         .registrable_domain());
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       ComputeIsolationInfoForNavigationSiteForCookiesSrcDoc) {
  // srcdoc frames basically don't figure into site_for_cookies computation.
  GURL url = embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_srcdoc_iframe_tree.html");

  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderFrameHostImpl* main_frame = web_contents()->GetMainFrame();
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
                         ->ComputeIsolationInfoForNavigation(url)
                         .site_for_cookies()
                         .registrable_domain());
  EXPECT_EQ("a.com", child_sd_a->current_frame_host()
                         ->ComputeIsolationInfoForNavigation(url)
                         .site_for_cookies()
                         .registrable_domain());
  EXPECT_EQ("a.com", child_sd_a_sd->current_frame_host()
                         ->ComputeIsolationInfoForNavigation(url)
                         .site_for_cookies()
                         .registrable_domain());

  GURL b_url = embedded_test_server()->GetURL("b.com", "/");
  EXPECT_EQ("b.com", main_frame->ComputeIsolationInfoForNavigation(b_url)
                         .site_for_cookies()
                         .registrable_domain());
  EXPECT_EQ("a.com", child_sd->current_frame_host()
                         ->ComputeIsolationInfoForNavigation(b_url)
                         .site_for_cookies()
                         .registrable_domain());
  EXPECT_EQ("a.com", child_sd_a->current_frame_host()
                         ->ComputeIsolationInfoForNavigation(b_url)
                         .site_for_cookies()
                         .registrable_domain());
  EXPECT_EQ("a.com", child_sd_a_sd->current_frame_host()
                         ->ComputeIsolationInfoForNavigation(b_url)
                         .site_for_cookies()
                         .registrable_domain());
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       ComputeSiteForCookiesFileURL) {
  GURL main_frame_url = GetFileURL(FILE_PATH_LITERAL("page_with_iframe.html"));
  GURL subframe_url = GetFileURL(FILE_PATH_LITERAL("title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  RenderFrameHostImpl* main_frame = web_contents()->GetMainFrame();
  EXPECT_EQ(main_frame_url, main_frame->GetLastCommittedURL());
  EXPECT_TRUE(net::SiteForCookies::FromUrl(GURL("file:///"))
                  .IsEquivalent(main_frame->ComputeSiteForCookies()));

  ASSERT_EQ(1u, main_frame->child_count());
  RenderFrameHostImpl* child = main_frame->child_at(0)->current_frame_host();
  EXPECT_EQ(subframe_url, child->GetLastCommittedURL());
  EXPECT_TRUE(net::SiteForCookies::FromUrl(GURL("file:///"))
                  .IsEquivalent(child->ComputeSiteForCookies()));
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       ComputeSiteForCookiesParentNavigatedAway) {
  // Navigate to site with same-domain frame, save a RenderFrameHostImpl to
  // the child.
  GURL url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)");

  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderFrameHostImpl* main_frame = web_contents()->GetMainFrame();

  EXPECT_EQ("a.com", main_frame->GetLastCommittedURL().host());

  ASSERT_EQ(1u, main_frame->child_count());
  FrameTreeNode* child_a = main_frame->child_at(0);
  RenderFrameHostImpl* child_rfh = child_a->current_frame_host();
  EXPECT_EQ("a.com", child_rfh->GetLastCommittedOrigin().host());
  GURL kid_url = child_rfh->GetLastCommittedURL();

  // Disable the unload ACK and the unload timer. Also pretend the child frame
  // has an unload handler, so it doesn't get cleaned up synchronously, and
  // block its detach handler.
  auto filter = base::MakeRefCounted<DropMessageFilter>(
      FrameMsgStart, FrameHostMsg_Unload_ACK::ID);
  main_frame->GetProcess()->AddFilter(filter.get());
  main_frame->DisableUnloadTimerForTesting();
  child_rfh->SuddenTerminationDisablerChanged(
      true, blink::mojom::SuddenTerminationDisablerType::kUnloadHandler);
  child_rfh->SetSubframeUnloadTimeoutForTesting(base::TimeDelta::FromDays(7));
  child_rfh->DoNotDeleteForTesting();

  // Open a popup on a.com to keep the process alive.
  OpenPopup(shell(), embedded_test_server()->GetURL("a.com", "/title2.html"),
            "foo");

  // Navigate root to b.com.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title3.html")));

  // The old RFH should be pending deletion, but its site_for_cookies should
  // be unchanged.
  EXPECT_TRUE(child_rfh->IsPendingDeletion());
  EXPECT_EQ(kid_url, child_rfh->GetLastCommittedURL());
  EXPECT_EQ(url, main_frame->GetLastCommittedURL());
  EXPECT_TRUE(main_frame->IsPendingDeletion());
  EXPECT_FALSE(main_frame->IsCurrent());
  net::SiteForCookies computed_for_child = child_rfh->ComputeSiteForCookies();
  EXPECT_TRUE(
      net::SiteForCookies::FromUrl(url).IsEquivalent(computed_for_child))
      << computed_for_child.ToDebugString();
}

// Make sure a local file and its subresources can be reloaded after a crash. In
// particular, after https://crbug.com/981339, a different RenderFrameHost will
// be used for reloading the file. File access must be correctly granted.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest, FileReloadAfterCrash) {
  // 1. Navigate a local file with an iframe.
  GURL main_frame_url = GetFileURL(FILE_PATH_LITERAL("page_with_iframe.html"));
  GURL subframe_url = GetFileURL(FILE_PATH_LITERAL("title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  // 2. Crash.
  RenderProcessHost* process = web_contents()->GetMainFrame()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  process->Shutdown(0);
  crash_observer.Wait();

  // 3. Reload.
  web_contents()->GetController().Reload(ReloadType::NORMAL, false);
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  // Check the document is correctly reloaded.
  RenderFrameHostImpl* main_document = web_contents()->GetMainFrame();
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
  // 1. Navigate a local file with an iframe.
  GURL main_frame_url(std::string(kChromeUIScheme) + "://" +
                      std::string(kChromeUIGpuHost));
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));

  // 2. Crash.
  RenderProcessHost* process = web_contents()->GetMainFrame()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  process->Shutdown(0);
  crash_observer.Wait();

  // 3. Reload.
  web_contents()->GetController().Reload(ReloadType::NORMAL, false);
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  // Check the document is correctly reloaded.
  RenderFrameHostImpl* main_document = web_contents()->GetMainFrame();
  EXPECT_EQ(main_frame_url, main_document->GetLastCommittedURL());
  // Execute script in an isolated world to avoid causing a Trusted Types
  // violation due to eval.
  EXPECT_EQ("Graphics Feature Status",
            EvalJs(main_document, "document.querySelector('h3').textContent",
                   EXECUTE_SCRIPT_DEFAULT_OPTIONS, /*world_id=*/1));
}

// Start with A(B), navigate A to C. By emulating a slow unload handler B, check
// the status of IsCurrent for subframes of A i.e., B before and after
// navigating to C.
// Test is flaky: https://crbug.com/1114149.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       DISABLED_CheckIsCurrentBeforeAndAfterUnload) {
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  GURL url_ab(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));

  // 1) Navigate to a page with an iframe.
  EXPECT_TRUE(NavigateToURL(shell(), url_ab));
  RenderFrameHostImpl* rfh_a = web_contents()->GetMainFrame();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();
  RenderFrameDeletedObserver delete_rfh_b(rfh_b);
  EXPECT_EQ(LifecycleState::kActive, rfh_b->lifecycle_state());

  // 2) Leave rfh_b in pending deletion state.
  LeaveInPendingDeletionState(rfh_b);

  // 3) Check the IsCurrent state of rfh_a, rfh_b before navigating to C.
  EXPECT_TRUE(rfh_a->IsCurrent());
  EXPECT_TRUE(rfh_b->IsCurrent());

  // 4) Navigate rfh_a to C.
  EXPECT_TRUE(NavigateToURL(shell(), url_c));
  RenderFrameHostImpl* rfh_c = web_contents()->GetMainFrame();

  EXPECT_THAT(rfh_a->lifecycle_state(),
              testing::AnyOf(testing::Eq(LifecycleState::kReadyToBeDeleted),
                             testing::Eq(LifecycleState::kInBackForwardCache)));
  EXPECT_THAT(
      rfh_b->lifecycle_state(),
      testing::AnyOf(testing::Eq(LifecycleState::kRunningUnloadHandlers),
                     testing::Eq(LifecycleState::kInBackForwardCache)));

  // 5) Check the IsCurrent state of rfh_a, rfh_b and rfh_c after navigating to
  // C.
  EXPECT_FALSE(rfh_a->IsCurrent());
  EXPECT_FALSE(rfh_b->IsCurrent());
  EXPECT_TRUE(rfh_c->IsCurrent());
}

// Test the LifecycleState is updated correctly for the main frame during
// navigation.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       CheckLifecycleStateTransitionOnMainFrame) {
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = root_frame_host();
  EXPECT_EQ(LifecycleState::kActive, rfh_a->lifecycle_state());

  // 2) Leave rfh_a in pending deletion state to check for rfh_a LifecycleState
  // after navigating to B.
  LeaveInPendingDeletionState(rfh_a);

  // 3) Start navigation to B, but don't commit yet.
  TestNavigationManager manager(web_contents(), url_b);
  shell()->LoadURL(url_b);
  EXPECT_TRUE(manager.WaitForRequestStart());

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  RenderFrameHostImpl* pending_rfh =
      root->render_manager()->speculative_frame_host();
  NavigationRequest* navigation_request = root->navigation_request();
  EXPECT_EQ(navigation_request->associated_site_instance_type(),
            NavigationRequest::AssociatedSiteInstanceType::SPECULATIVE);
  EXPECT_TRUE(pending_rfh);

  // 4) Check the LifecycleState of both rfh_a and pending_rfh before commit.
  EXPECT_EQ(LifecycleState::kSpeculative, pending_rfh->lifecycle_state());
  EXPECT_EQ(LifecycleState::kActive, rfh_a->lifecycle_state());
  EXPECT_EQ(root_frame_host(), rfh_a);

  // 5) Let the navigation finish and make sure it is succeeded.
  manager.WaitForNavigationFinished();
  EXPECT_EQ(url_b, web_contents()->GetMainFrame()->GetLastCommittedURL());
  RenderFrameHostImpl* rfh_b = root_frame_host();

  // 6) Check the LifecycleState of both rfh_a and rfh_b after navigating to B.
  EXPECT_THAT(
      rfh_a->lifecycle_state(),
      testing::AnyOf(testing::Eq(LifecycleState::kRunningUnloadHandlers),
                     testing::Eq(LifecycleState::kInBackForwardCache)));
  EXPECT_EQ(LifecycleState::kActive, rfh_b->lifecycle_state());
}

// Test the LifecycleState is updated correctly for a subframe.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       CheckRFHLifecycleStateTransitionOnSubFrame) {
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  GURL url_ab(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));

  // Lifecycle state of initial (Blank page) RenderFrameHost should be active as
  // we don't update the LifecycleState prior to navigation commits (to new URL
  // i.e., url_ab in this case).
  EXPECT_EQ(LifecycleState::kActive, root_frame_host()->lifecycle_state());

  // 1) Navigate to a page with an iframe.
  EXPECT_TRUE(NavigateToURL(shell(), url_ab));
  RenderFrameHostImpl* rfh_a = web_contents()->GetMainFrame();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();
  EXPECT_EQ(LifecycleState::kActive, rfh_b->lifecycle_state());

  // 2) Navigate B's subframe to a cross-site C.
  NavigateFrameToURL(rfh_b->frame_tree_node(), url_c);

  // 3) Check LifecycleState of sub-frame rfh_c after navigating from subframe
  // rfh_b.
  RenderFrameHostImpl* rfh_c = rfh_a->child_at(0)->current_frame_host();
  EXPECT_EQ(LifecycleState::kActive, rfh_c->lifecycle_state());

  // 4) Add a new child frame.
  RenderFrameHostCreatedObserver subframe_observer(web_contents());
  EXPECT_TRUE(ExecJs(rfh_c,
                     "let iframe = document.createElement('iframe');"
                     "document.body.appendChild(iframe);"));
  subframe_observer.Wait();

  // 5) LifecycleState of newly inserted child frame should be kActive before
  // navigation.
  RenderFrameHostImpl* rfh_d = rfh_c->child_at(0)->current_frame_host();
  EXPECT_EQ(LifecycleState::kActive, rfh_d->lifecycle_state());
}

// Verify that a new RFH gets marked as having committed a navigation after
// both normal navigations and error page navigations.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       HasCommittedAnyNavigation) {
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  EXPECT_TRUE(root_frame_host()->has_committed_any_navigation_);

  GURL error_url(embedded_test_server()->GetURL("b.com", "/empty.html"));
  std::unique_ptr<URLLoaderInterceptor> url_interceptor =
      URLLoaderInterceptor::SetupRequestFailForURL(error_url,
                                                   net::ERR_DNS_TIMED_OUT);
  EXPECT_FALSE(NavigateToURL(shell(), error_url));
  EXPECT_TRUE(root_frame_host()->has_committed_any_navigation_);
}

// Test the LifecycleState when a renderer crashes during navigation.
// When navigating after a crash, the new RenderFrameHost should
// become active immediately, prior to the navigation committing. This is
// an optimization to prevent the user from sitting around on the sad tab
// unnecessarily.
// TODO(https://crbug.com/1072817): This behavior might be revisited in the
// future.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       CheckRFHLifecycleStateWhenRendererCrashes) {
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = root_frame_host();
  EXPECT_EQ(LifecycleState::kActive, rfh_a->lifecycle_state());

  // 2) Renderer crash.
  RenderProcessHost* renderer_process = rfh_a->GetProcess();
  RenderProcessHostWatcher crash_observer(
      renderer_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  renderer_process->Shutdown(0);
  crash_observer.Wait();

  // 3) Start navigation to B, but don't commit yet.
  TestNavigationManager manager(web_contents(), url_b);
  shell()->LoadURL(url_b);
  EXPECT_TRUE(manager.WaitForRequestStart());

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  RenderFrameHostImpl* current_rfh =
      root->render_manager()->current_frame_host();
  NavigationRequest* navigation_request = root->navigation_request();
  EXPECT_EQ(navigation_request->associated_site_instance_type(),
            NavigationRequest::AssociatedSiteInstanceType::CURRENT);

  // 4) Check the LifecycleState of B's RFH.
  EXPECT_EQ(LifecycleState::kActive, current_rfh->lifecycle_state());

  // 5) Let the navigation finish and make sure it is succeeded.
  manager.WaitForNavigationFinished();
  EXPECT_EQ(url_b, web_contents()->GetMainFrame()->GetLastCommittedURL());
  EXPECT_EQ(LifecycleState::kActive, current_rfh->lifecycle_state());
}

// Check that same site navigation correctly resets document_used_web_otp_.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       SameSiteNavigationResetsDocumentUsedWebOTP) {
  const GURL first_url(embedded_test_server()->GetURL("/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), first_url));

  auto provider = std::make_unique<MockSmsProvider>();
  MockSmsProvider* mock_provider_ptr = provider.get();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(std::move(provider));

  std::string script = R"(
    (async () => {
      let cred = await navigator.credentials.get({otp: {transport: ["sms"]}});
      return cred.code;
    }) ();
  )";

  EXPECT_CALL(*mock_provider_ptr, Retrieve(testing::_))
      .WillOnce(testing::Invoke([&]() {
        mock_provider_ptr->NotifyReceive(url::Origin::Create(first_url),
                                         "hello");
      }));

  // EvalJs waits for the promise being resolved. This ensures that the browser
  // has time to see the otp usage, and records it, before we test for it below.
  EXPECT_EQ("hello", EvalJs(shell(), script));

  RenderFrameHostImpl* main_rfh = web_contents()->GetMainFrame();
  EXPECT_TRUE(main_rfh->DocumentUsedWebOTP());

  // Loads a URL that maps to the same SiteInstance as the first URL, to make
  // sure the navigation will not be cross-process.
  const GURL second_url(embedded_test_server()->GetURL("/title2.html"));
  ASSERT_TRUE(NavigateToURL(shell(), second_url));
  EXPECT_FALSE(main_rfh->DocumentUsedWebOTP());
}

// It is hard to test this feature fully at the integration test level. Indeed,
// there is no good way to inject a fake endpoint value into the URLLoader code
// that performs the CORS-RFC1918 checks. The most intrusive injection
// primitive, URLLoaderInterceptor, cannot be made to work as it bypasses the
// network service entirely. Intercepted subresource requests therefore do not
// execute the code under test and are never blocked.
//
// We are able to intercept top-level navigations, which allows us to test that
// the correct address space is committed in the client security state for
// local, private and public IP addresses.
//
// We further test that given a client security state with each IP address
// space, subresource requests served by local IP addresses fail unless
// initiated from the same address space. This provides integration testing
// coverage for both success and failure cases of the code under test.
//
// Finally, we have unit tests that test all possible combinations of source and
// destination IP address spaces in services/network/url_loader_unittest.cc.
// Those cover fetches to other address spaces than local.
class RenderFrameHostImplBrowserTestWithInsecurePrivateNetworkRequestsBlocked
    : public RenderFrameHostImplBrowserTest {
 public:
  RenderFrameHostImplBrowserTestWithInsecurePrivateNetworkRequestsBlocked() {
    feature_list_.InitAndEnableFeature(
        features::kBlockInsecurePrivateNetworkRequests);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// TODO(https://crbug.com/1014325): Flaky on multiple bots.
IN_PROC_BROWSER_TEST_F(
    RenderFrameHostImplBrowserTestWithInsecurePrivateNetworkRequestsBlocked,
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
    EXPECT_TRUE(NavigateToURL(shell(), test.url));
    RenderFrameHostImpl* rfhi = web_contents()->GetMainFrame();
    EXPECT_EQ(test.expected_internal,
              rfhi->last_committed_client_security_state()->ip_address_space);
    EXPECT_EQ(test.expected_web_facing, EvalJs(rfhi, "document.addressSpace"));
  }
}

IN_PROC_BROWSER_TEST_F(
    RenderFrameHostImplBrowserTestWithInsecurePrivateNetworkRequestsBlocked,
    ComputeIFrameLoopbackIPAddressSpace) {
  {
    base::string16 expected_title(base::UTF8ToUTF16("LOADED"));
    TitleWatcher title_watcher(web_contents(), expected_title);
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
        EXPECT_EQ(
            network::mojom::IPAddressSpace::kUnknown,
            rfhi->last_committed_client_security_state()->ip_address_space);
        EXPECT_EQ("local", EvalJs(rfhi, "document.addressSpace"));
      } else if (frame->GetLastCommittedURL().SchemeIsFileSystem() ||
                 frame->GetLastCommittedURL().SchemeIsBlob() ||
                 frame->GetLastCommittedURL().IsAboutSrcdoc() ||
                 frame->GetLastCommittedURL().SchemeIs(url::kDataScheme)) {
        // TODO(986744): `data:`, `blob:`, `filesystem:`, and `about:srcdoc`
        // should all inherit the IPAddressSpace from the document
        // that initiated a navigation. Right now, we treat them as `kPublic`.
        EXPECT_EQ(
            network::mojom::IPAddressSpace::kUnknown,
            rfhi->last_committed_client_security_state()->ip_address_space);
        EXPECT_EQ("public", EvalJs(rfhi, "document.addressSpace"));
      } else {
        // TODO(mkwst): Once the above two TODOs are resolved, this branch will
        // be the correct expectation for all the frames in this test.
        EXPECT_EQ(
            network::mojom::IPAddressSpace::kLocal,
            rfhi->last_committed_client_security_state()->ip_address_space);
        EXPECT_EQ("local", EvalJs(rfhi, "document.addressSpace"));
      }
    }
  }

  // Loading from loopback that asserts publicness: `data:`, `blob:`,
  // `filesystem:`, `about:blank`, and `about:srcdoc` all inherit the assertion.
  {
    base::string16 expected_title(base::UTF8ToUTF16("LOADED"));
    TitleWatcher title_watcher(web_contents(), expected_title);
    EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(
                                           "/treat-as-public-address.html")));
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

    std::vector<RenderFrameHost*> frames = web_contents()->GetAllFrames();
    for (auto* frame : frames) {
      auto* rfhi = static_cast<RenderFrameHostImpl*>(frame);
      if (frame->GetLastCommittedURL().IsAboutBlank()) {
        // TODO(986744): `about:blank` is not navigated via `NavigationRequest`,
        // but handled in the renderer via
        // `RenderFrameImpl::CommitSyncNavigation`. This means that we don't
        // calculate the value correctly on the browser-side, but do correctly
        // inherit from the initiator on the Blink-side.
        EXPECT_EQ(
            network::mojom::IPAddressSpace::kUnknown,
            rfhi->last_committed_client_security_state()->ip_address_space);
        EXPECT_EQ("public", EvalJs(rfhi, "document.addressSpace"));
      } else if (frame->GetLastCommittedURL().SchemeIsFileSystem() ||
                 frame->GetLastCommittedURL().SchemeIsBlob() ||
                 frame->GetLastCommittedURL().IsAboutSrcdoc() ||
                 frame->GetLastCommittedURL().SchemeIs(url::kDataScheme)) {
        // TODO(986744): `data:`, `blob:`, `filesystem:`, and `about:srcdoc`
        // should all inherit the IPAddressSpace from the document
        // that initiated a navigation. Right now, we treat them as `kUnknown`.
        EXPECT_EQ(
            network::mojom::IPAddressSpace::kUnknown,
            rfhi->last_committed_client_security_state()->ip_address_space);
        EXPECT_EQ("public", EvalJs(rfhi, "document.addressSpace"));
      } else {
        EXPECT_EQ(
            network::mojom::IPAddressSpace::kPublic,
            rfhi->last_committed_client_security_state()->ip_address_space);
        EXPECT_EQ("public", EvalJs(rfhi, "document.addressSpace"));
      }
    }
  }
}

// This test verifies that when the right feature is enabled, iframe requests:
//  - from an insecure page with the "treat-as-public-address" CSP directive
//  - to a local IP address
// are blocked.
IN_PROC_BROWSER_TEST_F(
    RenderFrameHostImplBrowserTestWithInsecurePrivateNetworkRequestsBlocked,
    IframeFromInsecureTreatAsPublicToLocalIsBlocked) {
  // Unfortunately for us, http://localhost is considered secure. Fortunately,
  // the host resolver in these tests is set to resolve anything to 127.0.0.1.
  // We use http://foo.test, which is not considered secure.
  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL(
          "foo.test",
          "/set-header?Content-Security-Policy: treat-as-public-address")));

  EXPECT_TRUE(ExecJs(root_frame_host(), R"(
    const iframe = document.createElement("iframe");
    iframe.src = "empty.html";
    document.body.appendChild(iframe);
  )"));

  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  // Check that the child iframe failed to fetch.
  ASSERT_EQ(1ul, root_frame_host()->child_count());
  auto* child_frame = root_frame_host()->child_at(0)->current_frame_host();
  EXPECT_EQ(0, child_frame->last_http_status_code());
  EXPECT_EQ(GURL(), child_frame->last_successful_url());
}

namespace {

// Returns a snippet of Javascript that fetch()es the given URL.
//
// The snippet evaluates to a boolean promise which resolves to true iff the
// fetch was successful. The promise never rejects, as doing so makes it hard
// to assert failure.
std::string FetchSubresourceScript(const std::string& url_spec) {
  return base::ReplaceStringPlaceholders(
      R"(fetch("$1").then(
           response => response.ok,
           error => {
             console.log('Error fetching "$1"', error);
             return false;
           });
      )",
      {url_spec}, nullptr);
}

// Returns an IP address in the private address space.
net::IPAddress PrivateAddress() {
  return net::IPAddress(10, 0, 1, 2);
}

// Returns an IP address in the public address space.
net::IPAddress PublicAddress() {
  return net::IPAddress(40, 0, 1, 2);
}

// Minimal response headers for an intercepted response to be successful.
constexpr base::StringPiece kMinimalResponseHeaders =  // force line break
    R"(HTTP/1.0 200 OK
Content-type: text/html

)";

// Minimal response body containing an HTML document.
constexpr base::StringPiece kMinimalHtmlBody = R"(
<html>
<head></head>
<body></body>
</html>
)";

// Wraps the URLLoaderInterceptor method of the same name, asserts success.
//
// NOTE: ASSERT_* macros can only be used in functions returning void.
void WriteResponseBody(base::StringPiece body,
                       network::mojom::URLLoaderClient* client) {
  ASSERT_EQ(content::URLLoaderInterceptor::WriteResponseBody(body, client),
            MOJO_RESULT_OK);
}

// Helper for InterceptorWithFakeEndpoint.
bool MaybeInterceptWithFakeEndpoint(
    const GURL& intercepted_url,
    const net::IPEndPoint& endpoint,
    content::URLLoaderInterceptor::RequestParams* params) {
  const GURL& request_url = params->url_request.url;
  if (request_url != intercepted_url) {
    LOG(INFO) << "MaybeInterceptWithFakeEndpoint: ignoring request to "
              << request_url;
    return false;
  }

  LOG(INFO) << "MaybeInterceptWithFakeEndpoint: intercepting request to "
            << request_url;

  auto response = network::mojom::URLResponseHead::New();
  response->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(kMinimalResponseHeaders));
  response->headers->GetMimeType(&response->mime_type);
  response->remote_endpoint = endpoint;
  params->client->OnReceiveResponse(std::move(response));

  WriteResponseBody(kMinimalHtmlBody, params->client.get());
  return true;
}

// The returned interceptor intercepts requests to |url|, fakes its network
// endpoint to reflect the value of |endpoint|, and responds OK with a minimal
// HTML body.
std::unique_ptr<content::URLLoaderInterceptor> InterceptorWithFakeEndpoint(
    const GURL& url,
    const net::IPEndPoint& endpoint) {
  LOG(INFO) << "Starting to intercept requests to " << url
            << " with fake endpoint " << endpoint.ToString();
  return std::make_unique<content::URLLoaderInterceptor>(
      base::BindRepeating(&MaybeInterceptWithFakeEndpoint, url, endpoint));
}

}  // namespace

// This test mimics the tests below, with the blocking feature disabled. It
// verifies that by default requests:
//  - from an insecure page with the "treat-as-public-address" CSP directive
//  - to a local IP address
// are not blocked.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       PrivateNetworkRequestIsNotBlockedByDefault) {
  // Unfortunately for us, http://localhost is considered secure. Fortunately,
  // the host resolver in these tests is set to resolve anything to 127.0.0.1.
  // We use http://foo.test, which is not considered secure.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   "foo.test", "/empty-treat-as-public-address.html")));

  const auto& security_state =
      root_frame_host()->last_committed_client_security_state();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);

  // Check that the page can load a local resource.
  EXPECT_EQ(true,
            EvalJs(root_frame_host(), FetchSubresourceScript("image.jpg")));
}

// This test verifies that when the right feature is enabled but the policy
// disables it, requests:
//  - from an insecure page with the "treat-as-public-address" CSP directive
//  - to a local IP address
// are not blocked.
IN_PROC_BROWSER_TEST_F(
    RenderFrameHostImplBrowserTestWithInsecurePrivateNetworkRequestsBlocked,
    FromInsecureTreatAsPublicToLocalWithPolicySetToAllowIsNotBlocked) {
  // Localhost is treated as secure, even when loaded over naked HTTP.
  // This is easier than using the HTTPS test server, since that server cannot
  // lie about its domain name, so we have to use localhost anyway.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   "foo.test", "/empty-treat-as-public-address.html")));

  // TODO(crbug.com/986744): Disable policy and fix test expectation once
  // policies are correctly wired up to the code under test.

  const auto& security_state =
      root_frame_host()->last_committed_client_security_state();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);

  // Check that the page can load a local resource.
  // TODO(crbug.com/986744): Expect true once policy wiring is fixed.
  EXPECT_EQ(false,
            EvalJs(root_frame_host(), FetchSubresourceScript("image.jpg")));
}

// This test verifies that when the right feature is enabled, requests:
//  - from a secure page with the "treat-as-public-address" CSP directive
//  - to a local IP address
// are not blocked.
IN_PROC_BROWSER_TEST_F(
    RenderFrameHostImplBrowserTestWithInsecurePrivateNetworkRequestsBlocked,
    FromSecureTreatAsPublicToLocalIsNotBlocked) {
  // Localhost is treated as secure, even when loaded over naked HTTP.
  // This is easier than using the HTTPS test server, since that server cannot
  // lie about its domain name, so we have to use localhost anyway.
  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL("/empty-treat-as-public-address.html")));

  const auto& security_state =
      root_frame_host()->last_committed_client_security_state();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_TRUE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);

  // Check that the page can load a local resource.
  EXPECT_EQ(true,
            EvalJs(root_frame_host(), FetchSubresourceScript("image.jpg")));
}

// This test verifies that when the right feature is enabled, requests:
//  - from an insecure page with the "treat-as-public-address" CSP directive
//  - to a local IP address
// are blocked.
IN_PROC_BROWSER_TEST_F(
    RenderFrameHostImplBrowserTestWithInsecurePrivateNetworkRequestsBlocked,
    FromInsecureTreatAsPublicToLocalIsBlocked) {
  // Unfortunately for us, http://localhost is considered secure. Fortunately,
  // the host resolver in these tests is set to resolve anything to 127.0.0.1.
  // We use http://foo.test, which is not considered secure.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   "foo.test", "/empty-treat-as-public-address.html")));

  const auto& security_state =
      root_frame_host()->last_committed_client_security_state();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);

  // Check that the page cannot load a local resource.
  EXPECT_EQ(false,
            EvalJs(root_frame_host(), FetchSubresourceScript("image.jpg")));
}

// This test verifies that when the right feature is enabled, requests:
//  - from an insecure page served by a public IP address
//  - to local IP addresses
//  are blocked.
IN_PROC_BROWSER_TEST_F(
    RenderFrameHostImplBrowserTestWithInsecurePrivateNetworkRequestsBlocked,
    FromInsecurePublicToLocalIsBlocked) {
  // Intercept the page load and pretend it came from a public IP.

  const GURL url = embedded_test_server()->GetURL("foo.test", "/index.html");

  // Use the same port as the server, so that the fetch is not cross-origin.
  auto interceptor = InterceptorWithFakeEndpoint(
      url, net::IPEndPoint(PublicAddress(), embedded_test_server()->port()));

  EXPECT_TRUE(NavigateToURL(shell(), url));

  const auto& security_state =
      root_frame_host()->last_committed_client_security_state();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);

  // Check that the page cannot load a local resource.
  EXPECT_EQ(false,
            EvalJs(root_frame_host(), FetchSubresourceScript("image.jpg")));
}

// This test verifies that when the right feature is enabled, requests:
//  - from an insecure page served by a private IP address
//  - to local IP addresses
//  are blocked.
IN_PROC_BROWSER_TEST_F(
    RenderFrameHostImplBrowserTestWithInsecurePrivateNetworkRequestsBlocked,
    FromInsecurePrivateToLocalIsBlocked) {
  // Intercept the page load and pretend it came from a private IP.

  const GURL url = embedded_test_server()->GetURL("foo.test", "/index.html");

  // Use the same port as the server, so that the fetch is not cross-origin.
  auto interceptor = InterceptorWithFakeEndpoint(
      url, net::IPEndPoint(PrivateAddress(), embedded_test_server()->port()));

  EXPECT_TRUE(NavigateToURL(shell(), url));

  const auto& security_state =
      root_frame_host()->last_committed_client_security_state();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kPrivate,
            security_state->ip_address_space);

  // Check that the page cannot load a local resource.
  EXPECT_EQ(false,
            EvalJs(root_frame_host(), FetchSubresourceScript("image.jpg")));
}

// This test verifies that when the right feature is enabled, requests:
//  - from an insecure page served by a local IP address
//  - to local IP addresses
//  are not blocked.
IN_PROC_BROWSER_TEST_F(
    RenderFrameHostImplBrowserTestWithInsecurePrivateNetworkRequestsBlocked,
    FromInsecureLocalToLocalIsNotBlocked) {
  const GURL url = embedded_test_server()->GetURL("foo.test", "/empty.html");

  EXPECT_TRUE(NavigateToURL(shell(), url));

  const auto& security_state =
      root_frame_host()->last_committed_client_security_state();
  ASSERT_FALSE(security_state.is_null());
  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);

  // Check that the page can load a local resource.
  EXPECT_EQ(true,
            EvalJs(root_frame_host(), FetchSubresourceScript("image.jpg")));
}

// This test verifies that when the right feature is enabled, requests:
//  - from a secure page with the "treat-as-public-address" CSP directive
//  - embedded in an insecure page served from a local IP address
//  - to local IP addresses
//  are blocked.
IN_PROC_BROWSER_TEST_F(
    RenderFrameHostImplBrowserTestWithInsecurePrivateNetworkRequestsBlocked,
    FromSecurePublicEmbeddedInInsecureLocalToLocalIsBlocked) {
  // First navigate to an insecure page served by a local IP address.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("foo.test", "/empty.html")));

  auto* security_state =
      root_frame_host()->last_committed_client_security_state().get();
  ASSERT_THAT(security_state, NotNull());
  EXPECT_FALSE(security_state->is_web_secure_context);
  EXPECT_EQ(network::mojom::IPAddressSpace::kLocal,
            security_state->ip_address_space);

  // Then embed a secure public iframe.
  auto iframe_url = embedded_test_server()->GetURL(
      "/set-header?Content-Security-Policy: treat-as-public-address");
  std::string script = base::ReplaceStringPlaceholders(
      R"(
        const iframe = document.createElement("iframe");
        iframe.src = "$1";
        document.body.appendChild(iframe);
      )",
      {iframe_url.spec()}, nullptr);
  EXPECT_TRUE(ExecJs(root_frame_host(), script));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  ASSERT_EQ(1ul, root_frame_host()->child_count());
  auto* child_frame = root_frame_host()->child_at(0)->current_frame_host();

  security_state = child_frame->last_committed_client_security_state().get();
  ASSERT_THAT(security_state, NotNull());

  // Even though the iframe document was loaded from a secure connection, the
  // context is deemed insecure because it was embedded by an insecure context.
  EXPECT_FALSE(security_state->is_web_secure_context);

  // The address space of the document, however, is not influenced by the
  // parent's address space.
  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);

  // Check that the iframe cannot load a local resource.
  EXPECT_EQ(false, EvalJs(child_frame, FetchSubresourceScript("image.jpg")));
}

// This test verifies that when the right feature is enabled, requests:
//  - from a secure page with the "treat-as-public-address" CSP directive
//  - embedded in a frame with no committed navigation
//  - to local IP addresses
//  are blocked.
IN_PROC_BROWSER_TEST_F(
    RenderFrameHostImplBrowserTestWithInsecurePrivateNetworkRequestsBlocked,
    FromSecurePublicEmbeddedInNotCommittedFrameToLocalIsBlocked) {
  // Start a navigation. This forces the RenderFrameHost to initialize its
  // RenderFrame (1). The navigation is then cancelled by a HTTP 204 code (2).
  //
  // (1) Allows Javascript to be executed in the frame, otherwise ExecJs()
  // fails.
  // (2) Thus the RenderFrameHost does not commit a client security state.
  EXPECT_TRUE(NavigateToURLAndExpectNoCommit(
      shell(), embedded_test_server()->GetURL("/nocontent")));

  // This results in the frame not having a security state.
  EXPECT_TRUE(
      root_frame_host()->last_committed_client_security_state().is_null());

  // Then embed a secure public iframe.
  auto iframe_url = embedded_test_server()->GetURL(
      "/set-header?Content-Security-Policy: treat-as-public-address");
  std::string script = base::ReplaceStringPlaceholders(
      R"(
        const iframe = document.createElement("iframe");
        iframe.src = "$1";
        document.body.appendChild(iframe);
      )",
      {iframe_url.spec()}, nullptr);
  EXPECT_TRUE(ExecJs(root_frame_host(), script));
  EXPECT_FALSE(WaitForLoadStop(web_contents()));

  ASSERT_EQ(1ul, root_frame_host()->child_count());
  auto* child_frame = root_frame_host()->child_at(0)->current_frame_host();

  const auto& security_state =
      child_frame->last_committed_client_security_state();
  ASSERT_FALSE(security_state.is_null());

  // The context is deemed secure because the parent frame had no client
  // security state, and the child frame loaded a secure page.
  //
  // TODO(crbug.com/1124346): Determine if this is correct, fix it if not.
  EXPECT_TRUE(security_state->is_web_secure_context);

  // The address space of the document, however, is not influenced by the
  // parent's address space.
  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            security_state->ip_address_space);

  // Check that the iframe can load a local resource.
  //
  // TODO(crbug.com/1124346): Determine if this is correct, fix it if not.
  EXPECT_EQ(true, EvalJs(child_frame, FetchSubresourceScript("image.jpg")));
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

  EXPECT_FALSE(rfhi->IsDOMContentLoaded());
  EXPECT_FALSE(web_contents->IsDocumentOnLoadCompletedInMainFrame());

  main_document_response.WaitForRequest();
  main_document_response.Send(
      "HTTP/1.1 200 OK\r\n"
      "Connection: close\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n"
      "<img src='/img'>");

  load_observer.WaitForNavigationFinished();
  EXPECT_FALSE(rfhi->IsDOMContentLoaded());
  EXPECT_FALSE(web_contents->IsDocumentOnLoadCompletedInMainFrame());

  main_document_response.Done();

  // We should reach DOMContentLoaded, but not onload, since the image resource
  // is still loading.
  loop_until_dcl.Run();
  EXPECT_TRUE(rfhi->is_loading());
  EXPECT_TRUE(rfhi->IsDOMContentLoaded());
  EXPECT_FALSE(web_contents->IsDocumentOnLoadCompletedInMainFrame());

  base::RunLoop loop_until_onload;
  DocumentOnLoadObserver onload_observer(web_contents,
                                         loop_until_onload.QuitClosure());

  image_response.WaitForRequest();
  image_response.Done();

  // And now onload() should be reached.
  loop_until_onload.Run();
  EXPECT_TRUE(rfhi->IsDOMContentLoaded());
  EXPECT_TRUE(web_contents->IsDocumentOnLoadCompletedInMainFrame());
}

IN_PROC_BROWSER_TEST_F(ContentBrowserTest, LoadingStateResetOnNavigation) {
  net::test_server::ControllableHttpResponse document2_response(
      embedded_test_server(), "/document2");

  EXPECT_TRUE(embedded_test_server()->Start());
  GURL url1(embedded_test_server()->GetURL("/title1.html"));
  GURL url2(embedded_test_server()->GetURL("/document2"));

  WebContents* web_contents = shell()->web_contents();

  base::RunLoop loop_until_onload;
  DocumentOnLoadObserver onload_observer(web_contents,
                                         loop_until_onload.QuitClosure());
  shell()->LoadURL(url1);
  loop_until_onload.Run();

  EXPECT_TRUE(static_cast<RenderFrameHostImpl*>(web_contents->GetMainFrame())
                  ->IsDOMContentLoaded());
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
  EXPECT_FALSE(web_contents->GetMainFrame()->IsDOMContentLoaded());
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

  EXPECT_TRUE(rfhi->IsDOMContentLoaded());
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

  EXPECT_TRUE(rfhi->IsDOMContentLoaded());
  EXPECT_TRUE(web_contents->IsDocumentOnLoadCompletedInMainFrame());
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest, GetUkmSourceIds) {
  ukm::TestAutoSetUkmRecorder recorder;
  // This test site has one cross-site iframe.
  GURL main_frame_url(
      embedded_test_server()->GetURL("/frame_tree/page_with_one_frame.html"));
  WebContents* web_contents = shell()->web_contents();
  DocumentUkmSourceIdObserver observer(web_contents);

  ASSERT_TRUE(NavigateToURL(shell(), main_frame_url));
  ASSERT_EQ(2u, web_contents->GetAllFrames().size());

  RenderFrameHostImpl* main_frame_host =
      static_cast<RenderFrameHostImpl*>(web_contents->GetMainFrame());
  ukm::SourceId page_ukm_source_id = main_frame_host->GetPageUkmSourceId();
  ukm::SourceId main_frame_doc_ukm_source_id =
      observer.GetMainFrameDocumentUkmSourceId();

  RenderFrameHostImpl* sub_frame_host = static_cast<RenderFrameHostImpl*>(
      main_frame_host->child_at(0)->current_frame_host());
  ukm::SourceId subframe_doc_ukm_source_id =
      observer.GetSubFrameDocumentUkmSourceId();

  // Navigation-level source id should be the same for all frames on the page.
  ASSERT_EQ(page_ukm_source_id, sub_frame_host->GetPageUkmSourceId());

  // The two document source ids and the navigation source id should be all
  // distinct.
  EXPECT_NE(page_ukm_source_id, main_frame_doc_ukm_source_id);
  EXPECT_NE(page_ukm_source_id, subframe_doc_ukm_source_id);
  EXPECT_NE(main_frame_doc_ukm_source_id, subframe_doc_ukm_source_id);

  const auto& document_created_entries =
      recorder.GetEntriesByName("DocumentCreated");
  // There should one DocumentCreated entry for each of the two frames.
  ASSERT_EQ(2u, document_created_entries.size());

  auto* main_frame_document_created_entry =
      recorder.GetDocumentCreatedEntryForSourceId(main_frame_doc_ukm_source_id);
  auto* sub_frame_document_created_entry =
      recorder.GetDocumentCreatedEntryForSourceId(subframe_doc_ukm_source_id);

  // Verify the recorded values on the DocumentCreated entries.
  EXPECT_EQ(page_ukm_source_id,
            *recorder.GetEntryMetric(main_frame_document_created_entry,
                                     "NavigationSourceId"));
  EXPECT_TRUE(*recorder.GetEntryMetric(main_frame_document_created_entry,
                                       "IsMainFrame"));
  EXPECT_FALSE(*recorder.GetEntryMetric(main_frame_document_created_entry,
                                        "IsCrossOriginFrame"));

  EXPECT_EQ(page_ukm_source_id,
            *recorder.GetEntryMetric(sub_frame_document_created_entry,
                                     "NavigationSourceId"));
  EXPECT_FALSE(*recorder.GetEntryMetric(sub_frame_document_created_entry,
                                        "IsMainFrame"));
  EXPECT_TRUE(*recorder.GetEntryMetric(sub_frame_document_created_entry,
                                       "IsCrossOriginFrame"));

  // Verify source creations. Main frame document source should have the URL;
  // no source should have been created for the sub-frame document.
  recorder.ExpectEntrySourceHasUrl(main_frame_document_created_entry,
                                   main_frame_url);
  EXPECT_EQ(nullptr, recorder.GetSourceForSourceId(subframe_doc_ukm_source_id));

  // Spot-check that an example entry recorded from the renderer uses the
  // correct document source id set by the RFH.
  const auto& blink_entries = recorder.GetEntriesByName("Blink.PageLoad");
  for (const auto* entry : blink_entries) {
    EXPECT_EQ(main_frame_doc_ukm_source_id, entry->source_id);
  }
}

// TODO(https://crbug.com/794320): the code below is temporary and will be
// removed when Java Bridge is mojofied.
#if defined(OS_ANDROID)

struct ObjectData {
  const int32_t id;
  const std::vector<std::string> methods;
};

ObjectData kMainObject{5, {"getId", "getInnerObject", "readArray"}};
ObjectData kInnerObject{10, {"getInnerId"}};

class MockInnerObject : public blink::mojom::RemoteObject {
 public:
  void HasMethod(const std::string& name, HasMethodCallback callback) override {
    bool has_method =
        std::find(kInnerObject.methods.begin(), kInnerObject.methods.end(),
                  name) != kInnerObject.methods.end();
    std::move(callback).Run(has_method);
  }
  void GetMethods(GetMethodsCallback callback) override {
    std::move(callback).Run(kInnerObject.methods);
  }
  void InvokeMethod(
      const std::string& name,
      std::vector<blink::mojom::RemoteInvocationArgumentPtr> arguments,
      InvokeMethodCallback callback) override {
    EXPECT_EQ("getInnerId", name);
    blink::mojom::RemoteInvocationResultPtr result =
        blink::mojom::RemoteInvocationResult::New();
    result->error = blink::mojom::RemoteInvocationError::OK;
    result->value = blink::mojom::RemoteInvocationResultValue::NewNumberValue(
        kInnerObject.id);
    std::move(callback).Run(std::move(result));
  }
};

class MockObject : public blink::mojom::RemoteObject {
 public:
  explicit MockObject(
      mojo::PendingReceiver<blink::mojom::RemoteObject> receiver)
      : receiver_(this, std::move(receiver)) {}
  void HasMethod(const std::string& name, HasMethodCallback callback) override {
    bool has_method =
        std::find(kMainObject.methods.begin(), kMainObject.methods.end(),
                  name) != kMainObject.methods.end();
    std::move(callback).Run(has_method);
  }

  void GetMethods(GetMethodsCallback callback) override {
    std::move(callback).Run(kMainObject.methods);
  }
  void InvokeMethod(
      const std::string& name,
      std::vector<blink::mojom::RemoteInvocationArgumentPtr> arguments,
      InvokeMethodCallback callback) override {
    blink::mojom::RemoteInvocationResultPtr result =
        blink::mojom::RemoteInvocationResult::New();
    result->error = blink::mojom::RemoteInvocationError::OK;
    if (name == "getId") {
      result->value = blink::mojom::RemoteInvocationResultValue::NewNumberValue(
          kMainObject.id);
    } else if (name == "readArray") {
      EXPECT_EQ(1U, arguments.size());
      EXPECT_TRUE(arguments[0]->is_array_value());
      num_elements_received_ = arguments[0]->get_array_value().size();
      result->value =
          blink::mojom::RemoteInvocationResultValue::NewBooleanValue(true);
    } else if (name == "getInnerObject") {
      result->value = blink::mojom::RemoteInvocationResultValue::NewObjectId(
          kInnerObject.id);
    }
    std::move(callback).Run(std::move(result));
  }

  int get_num_elements_received() const { return num_elements_received_; }

 private:
  int num_elements_received_ = 0;
  mojo::Receiver<blink::mojom::RemoteObject> receiver_;
};

class MockObjectHost : public blink::mojom::RemoteObjectHost {
 public:
  void GetObject(
      int32_t object_id,
      mojo::PendingReceiver<blink::mojom::RemoteObject> receiver) override {
    if (object_id == kMainObject.id) {
      mock_object_ = std::make_unique<MockObject>(std::move(receiver));
    } else if (object_id == kInnerObject.id) {
      mojo::MakeSelfOwnedReceiver(std::make_unique<MockInnerObject>(),
                                  std::move(receiver));
    }
  }

  void ReleaseObject(int32_t object_id) override {
    release_object_called_[object_id] = true;
  }

  mojo::PendingRemote<blink::mojom::RemoteObjectHost> GetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  MockObject* GetMockObject() const { return mock_object_.get(); }

  bool release_object_called_for_object(int32_t object_id) const {
    return release_object_called_.at(object_id);
  }

 private:
  mojo::Receiver<blink::mojom::RemoteObjectHost> receiver_{this};
  std::unique_ptr<MockObject> mock_object_;
  std::map<int32_t, bool> release_object_called_{{kMainObject.id, false},
                                                 {kInnerObject.id, false}};
};

class RemoteObjectInjector : public WebContentsObserver {
 public:
  explicit RemoteObjectInjector(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  const MockObjectHost& GetObjectHost() const { return host_; }

 private:
  void RenderFrameCreated(RenderFrameHost* render_frame_host) override {
    mojo::Remote<blink::mojom::RemoteObjectGateway> gateway;
    mojo::Remote<blink::mojom::RemoteObjectGatewayFactory> factory;
    static_cast<RenderFrameHostImpl*>(render_frame_host)
        ->GetRemoteInterfaces()
        ->GetInterface(factory.BindNewPipeAndPassReceiver());
    factory->CreateRemoteObjectGateway(host_.GetRemote(),
                                       gateway.BindNewPipeAndPassReceiver());
    gateway->AddNamedObject("testObject", kMainObject.id);
  }

  MockObjectHost host_;

  DISALLOW_COPY_AND_ASSIGN(RemoteObjectInjector);
};

namespace {
void SetupRemoteObjectInvocation(Shell* shell, const GURL& url) {
  WebContents* web_contents = shell->web_contents();

  // The first load triggers RenderFrameCreated on a RenderFrameHostObserver
  // instance, where the object injection happens.
  shell->LoadURL(url);
  EXPECT_TRUE(WaitForLoadStop(web_contents));
  // Injected objects become visible only after reload.
  web_contents->GetController().Reload(ReloadType::NORMAL, false);
  EXPECT_TRUE(WaitForLoadStop(web_contents));
}
}  // namespace

// TODO(https://crbug.com/794320): Remove this when the new Java Bridge code is
// integrated into WebView.
// This test is a temporary way of verifying that the renderer part
// works as expected.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       RemoteObjectEnumerateProperties) {
  GURL url(embedded_test_server()->GetURL("/empty.html"));

  RemoteObjectInjector injector(web_contents());
  SetupRemoteObjectInvocation(shell(), url);

  std::string kScript = "Object.keys(testObject).join(' ');";
  auto result = EvalJs(web_contents(), kScript);
  EXPECT_EQ(base::JoinString(kMainObject.methods, " "),
            result.value.GetString());
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       RemoteObjectInvokeNonexistentMethod) {
  GURL url(embedded_test_server()->GetURL("/empty.html"));

  RemoteObjectInjector injector(web_contents());
  SetupRemoteObjectInvocation(shell(), url);

  std::string kScript = "testObject.getInnerId();";
  EXPECT_FALSE(EvalJs(web_contents(), kScript).error.empty());
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       RemoteObjectInvokeMethodReturningNumber) {
  GURL url(embedded_test_server()->GetURL("/empty.html"));

  RemoteObjectInjector injector(web_contents());
  SetupRemoteObjectInvocation(shell(), url);

  std::string kScript = "testObject.getId();";
  EXPECT_EQ(kMainObject.id, EvalJs(web_contents(), kScript));
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       RemoteObjectInvokeMethodTakingArray) {
  GURL url(embedded_test_server()->GetURL("/empty.html"));

  RemoteObjectInjector injector(web_contents());
  SetupRemoteObjectInvocation(shell(), url);

  std::string kScript = "testObject.readArray([6, 8, 2]);";
  EXPECT_TRUE(EvalJs(web_contents(), kScript).error.empty());
  EXPECT_EQ(
      3, injector.GetObjectHost().GetMockObject()->get_num_elements_received());
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       RemoteObjectInvokeMethodReturningObject) {
  GURL url(embedded_test_server()->GetURL("/empty.html"));

  RemoteObjectInjector injector(web_contents());
  SetupRemoteObjectInvocation(shell(), url);

  std::string kScript = "testObject.getInnerObject().getInnerId();";
  EXPECT_EQ(kInnerObject.id, EvalJs(web_contents(), kScript));
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       RemoteObjectInvokeMethodException) {
  GURL url(embedded_test_server()->GetURL("/empty.html"));

  RemoteObjectInjector injector(web_contents());
  SetupRemoteObjectInvocation(shell(), url);

  std::string error_message = "hahaha";

  std::string kScript = JsReplace(R"(
      const array = [1, 2, 3];
      Object.defineProperty(array, 0, {
        get() { throw new Error($1); }
      });
      testObject.readArray(array);
    )",
                                  error_message);
  auto error = EvalJs(web_contents(), kScript).error;
  EXPECT_NE(error.find(error_message), std::string::npos);
}

// Based on testReturnedObjectIsGarbageCollected.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest, RemoteObjectRelease) {
  GURL url(embedded_test_server()->GetURL("/empty.html"));

  RemoteObjectInjector injector(web_contents());
  SetupRemoteObjectInvocation(shell(), url);

  EXPECT_EQ(
      "object",
      EvalJs(
          web_contents(),
          "globalInner = testObject.getInnerObject(); typeof globalInner; "));

  EXPECT_FALSE(injector.GetObjectHost().release_object_called_for_object(
      kInnerObject.id));
  EXPECT_EQ("object", EvalJs(web_contents(), "gc(); typeof globalInner;"));
  EXPECT_FALSE(injector.GetObjectHost().release_object_called_for_object(
      kInnerObject.id));
  EXPECT_EQ(
      "undefined",
      EvalJs(web_contents(), "delete globalInner; gc(); typeof globalInner;"));
  EXPECT_TRUE(injector.GetObjectHost().release_object_called_for_object(
      kInnerObject.id));
}

#endif  // OS_ANDROID

// The RenderFrameHost's last HTTP status code shouldn't change after
// same-document navigations.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       HttpStatusCodeAfterSameDocumentNavigation) {
  GURL url_201(embedded_test_server()->GetURL("/echo?status=201"));
  EXPECT_TRUE(NavigateToURL(shell(), url_201));
  EXPECT_EQ(201, root_frame_host()->last_http_status_code());
  EXPECT_TRUE(ExecJs(root_frame_host(), "location.href = '#'"));
  EXPECT_EQ(201, root_frame_host()->last_http_status_code());
}

// The RenderFrameHost's last HTTP method shouldn't change after
// same-document navigations.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       HttpMethodAfterSameDocumentNavigation) {
  GURL url(embedded_test_server()->GetURL("/empty.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_EQ("GET", root_frame_host()->last_http_method());

  TestNavigationObserver observer_post(web_contents());
  ExecuteScriptAsync(root_frame_host(), R"(
    let input = document.createElement("input");
    input.setAttribute("type", "hidden");
    input.setAttribute("name", "value");

    let form = document.createElement('form');
    form.appendChild(input);
    form.setAttribute("method", "POST");
    form.setAttribute("action", "?1");
    document.body.appendChild(form);
    form.submit();
  )");
  observer_post.Wait();
  EXPECT_EQ("POST", root_frame_host()->last_http_method());

  EXPECT_TRUE(ExecJs(root_frame_host(), "location.href = '#'"));
  EXPECT_EQ("POST", root_frame_host()->last_http_method());
}

}  // namespace content
