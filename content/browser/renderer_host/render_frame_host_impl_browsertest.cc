// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_frame_host_impl.h"

#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/input/timeout_monitor.h"
#include "components/viz/common/features.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/origin_trial_state_host_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/sms/test/mock_sms_provider.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/common/features.h"
#include "content/common/frame.mojom-forward.h"
#include "content/common/frame.mojom-shared.h"
#include "content/common/frame.mojom-test-utils.h"
#include "content/common/frame_messages.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/cors_origin_pattern_setter.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/origin_trials_controller_delegate.h"
#include "content/public/browser/page_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/page_visibility_state.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/commit_message_delayer.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/render_frame_host_test_support.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_navigation_throttle.h"
#include "content/public/test/test_navigation_throttle_inserter.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/data/mojo_web_test_helper_test.mojom.h"
#include "content/test/did_commit_navigation_interceptor.h"
#include "content/test/frame_host_test_interface.mojom.h"
#include "content/test/test_render_frame_host_factory.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/features.h"
#include "net/base/net_errors.h"
#include "net/base/schemeful_site.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/connection_tracker.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/mojom/browser_interface_broker.mojom-test-utils.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-test-utils.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "third_party/blink/public/mojom/remote_objects/remote_objects.mojom.h"
#include "ui/android/delegated_frame_host_android.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if defined(USE_AURA)
#include "content/browser/renderer_host/delegated_frame_host.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#endif  // defined(USE_AURA)

#if BUILDFLAG(IS_MAC)
#include "content/browser/renderer_host/browser_compositor_view_mac.h"
#include "content/browser/renderer_host/test_render_widget_host_view_mac_factory.h"
#endif

#if BUILDFLAG(IS_IOS)
#include "content/browser/renderer_host/browser_compositor_ios.h"
#include "content/browser/renderer_host/test_render_widget_host_view_ios_factory.h"
#endif

namespace content {
namespace {

const char kTrustMeUrl[] = "trustme://host/path/";
const char kTrustMeIfEmbeddingSecureUrl[] =
    "trustmeifembeddingsecure://host/path/";

// Configure trustme: as a scheme that should cause cookies to be treated as
// first-party when top-level, and also installs a URLLoaderFactory that
// makes all requests to it via kTrustMeUrl return a particular iframe.
// Same for trustmeifembeddingsecure, which does the same if the embedded origin
// is secure.
class FirstPartySchemeContentBrowserClient
    : public ContentBrowserTestContentBrowserClient {
 public:
  explicit FirstPartySchemeContentBrowserClient(const GURL& iframe_url)
      : iframe_url_(iframe_url) {
    trustme_factory_ = std::make_unique<network::TestURLLoaderFactory>();
    trustmeifembeddingsecure_factory_ =
        std::make_unique<network::TestURLLoaderFactory>();
    std::string response_body =
        base::StrCat({"<iframe src=\"", iframe_url_.spec(), "\"></iframe>"});
    trustme_factory_->AddResponse(kTrustMeUrl, response_body);
    trustmeifembeddingsecure_factory_->AddResponse(kTrustMeIfEmbeddingSecureUrl,
                                                   response_body);
  }

  FirstPartySchemeContentBrowserClient(
      const FirstPartySchemeContentBrowserClient&) = delete;
  FirstPartySchemeContentBrowserClient& operator=(
      const FirstPartySchemeContentBrowserClient&) = delete;

  ~FirstPartySchemeContentBrowserClient() override = default;

  bool ShouldTreatURLSchemeAsFirstPartyWhenTopLevel(
      std::string_view scheme,
      bool is_embedded_origin_secure) override {
    if (is_embedded_origin_secure && scheme == "trustmeifembeddingsecure")
      return true;
    return scheme == "trustme";
  }

  mojo::PendingRemote<network::mojom::URLLoaderFactory>
  CreateNonNetworkNavigationURLLoaderFactory(
      const std::string& scheme,
      FrameTreeNodeId frame_tree_node_id) override {
    if (scheme == "trustme") {
      mojo::PendingRemote<network::mojom::URLLoaderFactory> trustme_remote;
      trustme_factory_->Clone(trustme_remote.InitWithNewPipeAndPassReceiver());
      return trustme_remote;
    }

    if (scheme == "trustmeifembeddingsecure") {
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          trustmeifembeddingsecure_remote;
      trustmeifembeddingsecure_factory_->Clone(
          trustmeifembeddingsecure_remote.InitWithNewPipeAndPassReceiver());
      return trustmeifembeddingsecure_remote;
    }
    return {};
  }

 private:
  GURL iframe_url_;
  std::unique_ptr<network::TestURLLoaderFactory> trustme_factory_;
  std::unique_ptr<network::TestURLLoaderFactory>
      trustmeifembeddingsecure_factory_;
};

}  // namespace

// TODO(mlamouri): part of these tests were removed because they were dependent
// on an environment were focus is guaranteed. This is only for
// interactive_ui_tests so these bits need to move there.
// See https://crbug.com/491535
class RenderFrameHostImplBrowserTest : public ContentBrowserTest {
 public:
  using LifecycleStateImpl = RenderFrameHostImpl::LifecycleStateImpl;
  RenderFrameHostImplBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}
  ~RenderFrameHostImplBrowserTest() override = default;

  // Return an URL for loading a local test file.
  GURL GetFileURL(const base::FilePath::CharType* file_path) {
    base::FilePath path;
    CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &path));
    path = path.Append(GetTestDataFilePath());
    path = path.Append(file_path);
    return GURL("file:" + path.AsUTF8Unsafe());
  }

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // TODO(crbug.com/40554401): Remove this when the new Java Bridge code
    // is integrated into WebView.
    command_line->AppendSwitchASCII(blink::switches::kJavaScriptFlags,
                                    "--expose_gc");

    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures, "WebOTP");
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  RenderFrameHostImpl* root_frame_host() const {
    return web_contents()->GetPrimaryMainFrame();
  }

 private:
  net::EmbeddedTestServer https_server_;
};

std::string ExecuteJavaScriptMethodAndGetResult(
    RenderFrameHostImpl* render_frame,
    const std::string& object,
    const std::string& method,
    base::Value::List arguments) {
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

// Navigates to a URL and waits till the navigation is finished. It doesn't wait
// for the load to complete. Use this instead of NavigateToURL in tests that are
// testing navigation related cases and doesn't need the load to finish. Load
// could get blocked on blink::mojom::CodeCacheHostInterface if the browser
// interface is not available.
bool NavigateToURLAndDoNotWaitForLoadStop(Shell* window, const GURL& url) {
  TestNavigationManager observer(window->web_contents(), url);
  window->LoadURL(url);
  EXPECT_TRUE(observer.WaitForNavigationFinished());
  return url ==
         window->web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL();
}

// Navigates an iframe nested within another iframe to the specified URL via
// Javascript executed from the outer document. Also, this waits for the
// navigation to finish.
void NavigateToURLFromGrandparentDocument(Shell* window, const GURL& url) {
  RenderFrameHostImpl* rfh_grandparent =
      static_cast<WebContentsImpl*>(window->web_contents())
          ->GetPrimaryMainFrame();
  RenderFrameHostImpl* rfh_parent =
      rfh_grandparent->child_at(0)->current_frame_host();
  RenderFrameHostImpl* rfh_child =
      rfh_parent->child_at(0)->current_frame_host();

  // If we are navigating from a.com/ to a.com/#x or vice versa, we expect this
  // to be a same-document navigation.
  bool expected_is_same_document =
      rfh_child->GetLastCommittedURL().GetWithoutRef() == url.GetWithoutRef();

  TestNavigationManager navigation_manager(window->web_contents(), url);
  NavigationHandleObserver navigation_observer(window->web_contents(), url);
  ASSERT_TRUE(
      ExecJs(rfh_grandparent,
             JsReplace("frames[0].frames[0].location.href = $1;", url)));

  ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());

  ASSERT_FALSE(navigation_observer.is_error());
  ASSERT_EQ(navigation_observer.last_committed_url(), url);
  rfh_child = rfh_parent->child_at(0)->current_frame_host();
  ASSERT_EQ(navigation_observer.frame_tree_node_id(),
            rfh_child->GetFrameTreeNodeId());

  EXPECT_EQ(navigation_observer.is_same_document(), expected_is_same_document);
}

// Returns the `initiator_origin` member of the last committed frame navigation
// entry corresponding to `rfh`. For more information on the returned value, see
// `FrameNavigationEntry::initiator_origin()`.
std::optional<url::Origin> GetInitiatorOrigin(RenderFrameHostImpl* rfh) {
  FrameTreeNode* frame_tree_node = rfh->frame_tree_node();
  FrameNavigationEntry* frame_navigation_entry =
      frame_tree_node->frame_tree()
          .controller()
          .GetLastCommittedEntry()
          ->GetFrameEntry(frame_tree_node);
  EXPECT_TRUE(frame_navigation_entry);

  return frame_navigation_entry->initiator_origin();
}

using blink::mojom::BlockingDetails;
using BackForwardCacheBlockingDetails =
    RenderFrameHostImpl::BackForwardCacheBlockingDetails;
using BlocklistedFeature = blink::scheduler::WebSchedulerTrackedFeature;
using BlocklistedFeatures = blink::scheduler::WebSchedulerTrackedFeatures;
// Helper function to create a vector which contains the mojom feature
// information.
BackForwardCacheBlockingDetails CreateBlockingDetails(
    std::initializer_list<BlocklistedFeature> features) {
  BackForwardCacheBlockingDetails feature_vector;
  for (auto feature : features) {
    auto feature_info = BlockingDetails::New();
    feature_info->feature = static_cast<uint32_t>(feature);
    feature_vector.push_back(std::move(feature_info));
  }
  return feature_vector;
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       ExecuteJavaScriptMethodWorksWithArguments) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GetTestUrl("render_frame_host", "jsMethodTest.html")));

  RenderFrameHostImpl* render_frame = web_contents()->GetPrimaryMainFrame();
  render_frame->AllowInjectingJavaScript();

  base::Value::List empty_arguments;
  std::string result = ExecuteJavaScriptMethodAndGetResult(
      render_frame, "window", "someMethod", std::move(empty_arguments));
  EXPECT_EQ(result, "called someMethod()");

  base::Value::List single_arguments;
  single_arguments.Append("arg1");
  result = ExecuteJavaScriptMethodAndGetResult(
      render_frame, "window", "someMethod", std::move(single_arguments));
  EXPECT_EQ(result, "called someMethod(arg1)");

  base::Value::List four_arguments;
  four_arguments.Append("arg1");
  four_arguments.Append("arg2");
  four_arguments.Append("arg3");
  four_arguments.Append("arg4");
  result = ExecuteJavaScriptMethodAndGetResult(
      render_frame, "window", "someMethod", std::move(four_arguments));
  EXPECT_EQ(result, "called someMethod(arg1,arg2,arg3,arg4)");
}

// Tests that IPC messages that are dropped (because they are sent before
// RenderFrameCreated) do not prevent later IPC messages from being sent after
// the RenderFrame is created. See https://crbug.com/1154852.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       MessagesBeforeAndAfterRenderFrameCreated) {
  // Start with a WebContents that hasn't created its main RenderFrame.
  WebContents* web_contents = shell()->web_contents();
  ASSERT_FALSE(web_contents->GetPrimaryMainFrame()->IsRenderFrameLive());

  // An attempt to run script via GetAssociatedLocalFrame will do nothing before
  // the RenderFrame is created, since the message sent to the renderer will get
  // dropped. In https://crbug.com/1154852, this causes future messages sent via
  // GetAssociatedLocalFrame to also be dropped.
  web_contents->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
      u"'foo'", base::NullCallback(), ISOLATED_WORLD_ID_GLOBAL);

  // Navigating will create the RenderFrame.
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  ASSERT_TRUE(web_contents->GetPrimaryMainFrame()->IsRenderFrameLive());

  // Future attempts to run script via GetAssociatedLocalFrame should succeed.
  // This timed out before the fix, since the message was dropped and no value
  // was retrieved.
  EXPECT_EQ("foo", EvalJs(web_contents->GetPrimaryMainFrame(), "'foo'"));
}

// Test that when creating a new window, the main frame is correctly focused.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest, IsFocused_AtLoad) {
  EXPECT_TRUE(
      NavigateToURL(shell(), GetTestUrl("render_frame_host", "focus.html")));

  // The main frame should be focused.
  EXPECT_EQ(web_contents()->GetPrimaryMainFrame(),
            web_contents()->GetFocusedFrame());
}

// Test that if the content changes the focused frame, it is correctly exposed.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest, IsFocused_Change) {
  EXPECT_TRUE(
      NavigateToURL(shell(), GetTestUrl("render_frame_host", "focus.html")));

  std::string frames[2] = {"frame1", "frame2"};
  for (const std::string& frame : frames) {
    EXPECT_TRUE(
        ExecJs(web_contents()->GetPrimaryMainFrame(), "focus" + frame + "()"));

    // The main frame is not the focused frame in the frame tree but the main
    // frame is focused per RFHI rules because one of its descendant is focused.
    // TODO(mlamouri): we should check the frame focus state per RFHI, see the
    // general comment at the beginning of this test file.
    EXPECT_NE(web_contents()->GetPrimaryMainFrame(),
              web_contents()->GetFocusedFrame());
    EXPECT_EQ(frame, web_contents()->GetFocusedFrame()->GetFrameName());
  }
}

// Tests focus behavior when the focused frame is removed from the frame tree.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest, RemoveFocusedFrame) {
  EXPECT_TRUE(
      NavigateToURL(shell(), GetTestUrl("render_frame_host", "focus.html")));

  EXPECT_TRUE(ExecJs(web_contents()->GetPrimaryMainFrame(), "focusframe4()"));

  EXPECT_NE(web_contents()->GetPrimaryMainFrame(),
            web_contents()->GetFocusedFrame());
  EXPECT_EQ("frame4", web_contents()->GetFocusedFrame()->GetFrameName());
  EXPECT_EQ("frame3",
            web_contents()->GetFocusedFrame()->GetParent()->GetFrameName());
  EXPECT_TRUE(
      web_contents()->GetPrimaryFrameTree().focused_frame_tree_node_id_);

  EXPECT_TRUE(ExecJs(web_contents()->GetPrimaryMainFrame(), "detachframe(3)"));
  EXPECT_EQ(nullptr, web_contents()->GetFocusedFrame());
  EXPECT_TRUE(web_contents()
                  ->GetPrimaryFrameTree()
                  .focused_frame_tree_node_id_.is_null());

  EXPECT_TRUE(ExecJs(web_contents()->GetPrimaryMainFrame(), "focusframe2()"));
  EXPECT_NE(nullptr, web_contents()->GetFocusedFrame());
  EXPECT_NE(web_contents()->GetPrimaryMainFrame(),
            web_contents()->GetFocusedFrame());
  EXPECT_TRUE(
      web_contents()->GetPrimaryFrameTree().focused_frame_tree_node_id_);

  EXPECT_TRUE(ExecJs(web_contents()->GetPrimaryMainFrame(), "detachframe(2)"));
  EXPECT_EQ(nullptr, web_contents()->GetFocusedFrame());
  EXPECT_TRUE(web_contents()
                  ->GetPrimaryFrameTree()
                  .focused_frame_tree_node_id_.is_null());
}

// Test that a frame is visible/hidden depending on its WebContents visibility
// state.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       GetVisibilityState_Basic) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL("data:text/html,foo")));

  web_contents()->WasShown();
  EXPECT_EQ(PageVisibilityState::kVisible,
            web_contents()->GetPrimaryMainFrame()->GetVisibilityState());

  web_contents()->WasHidden();
  EXPECT_EQ(PageVisibilityState::kHidden,
            web_contents()->GetPrimaryMainFrame()->GetVisibilityState());
}

// Check that the URLLoaderFactories created by RenderFrameHosts for renderers
// are not trusted.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       URLLoaderFactoryNotTrusted) {
  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory;
  web_contents()->GetPrimaryMainFrame()->CreateNetworkServiceDefaultFactory(
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
      url_loader_factory.get(), simple_loader_helper.GetCallbackDeprecated());
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

  TestJavaScriptDialogManager(const TestJavaScriptDialogManager&) = delete;
  TestJavaScriptDialogManager& operator=(const TestJavaScriptDialogManager&) =
      delete;

  ~TestJavaScriptDialogManager() override {}

  // This waits until either WCD::BeforeUnloadFired is called (the unload has
  // been handled) or JSDM::RunJavaScriptDialog/RunBeforeUnloadDialog is called
  // (a request to display a dialog has been received).
  void Wait() {
    message_loop_runner_->Run();
    message_loop_runner_ = new MessageLoopRunner;
  }

  // Runs the dialog callback.
  void Run(bool success, const std::u16string& user_input) {
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
                           const std::u16string& message_text,
                           const std::u16string& default_prompt_text,
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
                              const std::u16string* prompt_override) override {
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
};

// A RenderFrameHostImpl that discards callback for BeforeUnload.
class RenderFrameHostImplForBeforeUnloadInterceptor
    : public RenderFrameHostImpl {
 public:
  using RenderFrameHostImpl::RenderFrameHostImpl;

  void SendBeforeUnload(bool is_reload,
                        base::WeakPtr<RenderFrameHostImpl> rfh,
                        bool for_legacy) override {
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
      mojo::PendingAssociatedRemote<mojom::Frame> frame_remote,
      const blink::LocalFrameToken& frame_token,
      const blink::DocumentToken& document_token,
      base::UnguessableToken devtools_frame_token,
      bool renderer_initiated_creation,
      RenderFrameHostImpl::LifecycleStateImpl lifecycle_state,
      scoped_refptr<BrowsingContextState> browsing_context_state) override {
    return base::WrapUnique(new RenderFrameHostImplForBeforeUnloadInterceptor(
        site_instance, std::move(render_view_host), delegate, frame_tree,
        frame_tree_node, routing_id, std::move(frame_remote), frame_token,
        document_token, devtools_frame_token, renderer_initiated_creation,
        lifecycle_state, std::move(browsing_context_state),
        frame_tree_node->frame_owner_element_type(), frame_tree_node->parent(),
        frame_tree_node->fenced_frame_status()));
  }
};

}  // namespace

// Tests that a beforeunload dialog in an iframe doesn't stop the beforeunload
// timer of a parent frame.
// TODO(avi): flaky on Linux TSAN: http://crbug.com/795326
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && defined(THREAD_SANITIZER)
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
  EXPECT_TRUE(ExecJs(web_contents(), script));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  // JavaScript onbeforeunload dialogs require a user gesture.
  web_contents()->GetPrimaryMainFrame()->ForEachRenderFrameHost(
      [](content::RenderFrameHostImpl* render_frame_host) {
        render_frame_host->ExecuteJavaScriptWithUserGestureForTests(
            std::u16string(), base::NullCallback(), ISOLATED_WORLD_ID_GLOBAL);
      });

  // Force a process switch by going to a privileged page. The beforeunload
  // timer will be started on the top-level frame but will be paused while the
  // beforeunload dialog is shown by the subframe.
  GURL web_ui_page(std::string(kChromeUIScheme) + "://" +
                   std::string(kChromeUIGpuHost));
  shell()->LoadURL(web_ui_page);
  dialog_manager.Wait();

  RenderFrameHostImpl* main_frame = web_contents()->GetPrimaryMainFrame();
  EXPECT_TRUE(main_frame->is_waiting_for_beforeunload_completion());

  // Answer the dialog.
  dialog_manager.Run(true, std::u16string());

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
  web_contents()
      ->GetPrimaryMainFrame()
      ->DisableBeforeUnloadHangMonitorForTesting();

  // Reload. There should be no beforeunload dialog because there was no gesture
  // on the page. If there was, this WaitForLoadStop call will hang.
  web_contents()->GetController().Reload(ReloadType::NORMAL, false);
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  // Give the page a user gesture and try reloading again. This time there
  // should be a dialog. If there is no dialog, the call to Wait will hang.
  web_contents()
      ->GetPrimaryMainFrame()
      ->ExecuteJavaScriptWithUserGestureForTests(
          std::u16string(), base::NullCallback(), ISOLATED_WORLD_ID_GLOBAL);
  web_contents()->GetController().Reload(ReloadType::NORMAL, false);
  dialog_manager.Wait();

  // Answer the dialog.
  dialog_manager.Run(true, std::u16string());
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  // The reload should have cleared the user gesture bit, so upon leaving again
  // there should be no beforeunload dialog.
  shell()->LoadURL(GURL("about:blank"));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  web_contents()->SetDelegate(nullptr);
  web_contents()->SetJavaScriptDialogManagerForTesting(nullptr);
}

// Tests that requesting a before unload confirm dialog on a non-active
// does not show a dialog.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       BeforeUnloadConfirmOnNonActive) {
  TestJavaScriptDialogManager dialog_manager;
  web_contents()->SetDelegate(&dialog_manager);

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = root_frame_host();
  LeaveInPendingDeletionState(rfh_a);

  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  rfh_a->RunBeforeUnloadConfirm(true, base::DoNothing());

  // We should not have seen a dialog because the page isn't active anymore.
  EXPECT_EQ(0, dialog_manager.num_beforeunload_dialogs_seen());

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
  dialog_manager.Run(false, std::u16string());
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

// A separate class needed to test with Origin Trial tokens.
class RenderFrameHostImplWithTokensBrowserTest : public ContentBrowserTest {
 public:
  RenderFrameHostImplWithTokensBrowserTest() {
    test_features_.InitAndEnableFeature(::features::kPersistentOriginTrials);
  }

  ~RenderFrameHostImplWithTokensBrowserTest() override = default;

  // The URL that will be used for Origin Trials.
  static constexpr char kOriginTrialUrl[] = "https://127.0.0.1:44444";
  // The URL that will be used to load third-party scripts.
  static constexpr char kThirdPartyScriptUrl[] = "https://127.0.0.1:44445";
  // The URL that is same-site but cross-origin to the third-party scripts URL.
  static constexpr char kThirdPartyCrossOriginUrl[] = "https://127.0.0.1:44446";
  // URL for empty page responses.
  static constexpr char kEmptyPageUrl[] = "https://127.0.0.1:44440";
  // A cross-site URL used for Origin Trials.
  static constexpr char kCrossSiteOriginTrialUrl[] = "https://a.com";

 protected:
  void SetUpOnMainThread() override {
    // Set up the framework that allows us to intercept and inspect any Origin
    // Trial header requests.
    url_loader_interceptor_ =
        std::make_unique<URLLoaderInterceptor>(base::BindRepeating(
            &RenderFrameHostImplWithTokensBrowserTest::InterceptURLRequest,
            base::Unretained(this)));
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    url_loader_interceptor_.reset();
    GetOriginTrialsDelegate()->ClearPersistedTokens();
    ContentBrowserTest::TearDownOnMainThread();
  }

  void SetOriginTrialToken(const std::string& token) {
    origin_trial_token_ = token;
  }

  OriginTrialsControllerDelegate* GetOriginTrialsDelegate() {
    return shell()
        ->web_contents()
        ->GetPrimaryMainFrame()
        ->GetBrowserContext()
        ->GetOriginTrialsControllerDelegate();
  }

  GURL empty_page_url() const {
    return GURL(base::StrCat({kEmptyPageUrl, "/empty.html"}));
  }

  GURL simple_origin_trial_url() const {
    return GURL(base::StrCat({kOriginTrialUrl, "/title1.html"}));
  }

  GURL meta_tag_origin_trial_url() const {
    return GURL(base::StrCat({kOriginTrialUrl, "/meta.html"}));
  }

  GURL script_meta_tag_origin_trial_url() const {
    return GURL(base::StrCat({kOriginTrialUrl, "/meta_script.html"}));
  }

  GURL meta_tag_injecting_javascript_url() const {
    return GURL(base::StrCat({kThirdPartyScriptUrl, "/meta.js"}));
  }

  GURL cross_site_script_meta_tag_origin_trial_url() const {
    return GURL(base::StrCat({kCrossSiteOriginTrialUrl, "/meta_script.html"}));
  }

  GURL empty_frame_meta_origin_trial_url() const {
    return GURL(base::StrCat({kThirdPartyScriptUrl, "/empty.html"}));
  }

  GURL same_site_cross_origin_url() const {
    return GURL(base::StrCat({kThirdPartyCrossOriginUrl, "/empty.html"}));
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

 private:
  bool RespondForEmptyUrl(URLLoaderInterceptor::RequestParams* params) {
    std::string headers = "HTTP/1.1 200 OK\nContent-Type: text/html\n";
    std::string body = "<html>This page has no title.</html>";
    URLLoaderInterceptor::WriteResponse(headers, body, params->client.get());
    return true;
  }

  bool RespondForSimpleOriginTrialUrl(
      URLLoaderInterceptor::RequestParams* params) {
    if (origin_trial_token_.empty()) {
      return false;
    }
    // Construct the origin trial header response.
    std::string headers = "HTTP/1.1 200 OK\nContent-Type: text/html\n";
    base::StrAppend(&headers, {"Origin-Trial: ", origin_trial_token_, "\n"});
    std::string body = "<html>This page has no title.</html>";
    URLLoaderInterceptor::WriteResponse(headers, body, params->client.get());
    return true;
  }

  bool RespondForMetaTagOriginTrialUrl(
      URLLoaderInterceptor::RequestParams* params) {
    if (origin_trial_token_.empty()) {
      return false;
    }
    // Construct the origin trial header response.
    std::string headers = "HTTP/1.1 200 OK\nContent-Type: text/html\n";
    std::string body = base::StrCat(
        {"<html><head><meta http-equiv=\"origin-trial\" content=\"",
         origin_trial_token_,
         "\"></head><body>"
         "This page has no title.</body></html>"});
    URLLoaderInterceptor::WriteResponse(headers, body, params->client.get());
    return true;
  }

  bool RespondForScriptMetaTagOriginTrialUrl(
      URLLoaderInterceptor::RequestParams* params) {
    if (origin_trial_token_.empty()) {
      return false;
    }
    // Construct the origin trial header response.
    std::string headers = "HTTP/1.1 200 OK\nContent-Type: text/html\n";
    std::string body = base::StrCat(
        {"<html><head><script src=\"",
         meta_tag_injecting_javascript_url().spec(),
         "\"></script></head><body>This page has no title.</body></html>"});
    URLLoaderInterceptor::WriteResponse(headers, body, params->client.get());
    return true;
  }

  bool RespondForMetaTagInjectingScriptUrl(
      URLLoaderInterceptor::RequestParams* params) {
    if (origin_trial_token_.empty()) {
      return false;
    }
    // Construct the origin trial header response.
    std::string headers =
        "HTTP/1.1 200 OK\nContent-Type: application/javascript\n";
    std::string body =
        base::StrCat({"const otMeta = document.createElement('meta'); "
                      "otMeta.httpEquiv = 'origin-trial'; "
                      "otMeta.content = '",
                      origin_trial_token_,
                      "'; "
                      "document.head.append(otMeta); ",
                      "const iframe = document.createElement('iframe'); ",
                      "document.head.appendChild(iframe); "});
    URLLoaderInterceptor::WriteResponse(headers, body, params->client.get());
    return true;
  }

  // Create the framework to intercept origin trial header requests.
  bool InterceptURLRequest(URLLoaderInterceptor::RequestParams* params) {
    if (params->url_request.url == empty_page_url()) {
      return RespondForEmptyUrl(params);
    }
    if (params->url_request.url == simple_origin_trial_url()) {
      return RespondForSimpleOriginTrialUrl(params);
    }
    if (params->url_request.url == meta_tag_origin_trial_url()) {
      return RespondForMetaTagOriginTrialUrl(params);
    }
    if (params->url_request.url == script_meta_tag_origin_trial_url()) {
      return RespondForScriptMetaTagOriginTrialUrl(params);
    }
    if (params->url_request.url == meta_tag_injecting_javascript_url()) {
      return RespondForMetaTagInjectingScriptUrl(params);
    }
    if (params->url_request.url ==
        cross_site_script_meta_tag_origin_trial_url()) {
      return RespondForScriptMetaTagOriginTrialUrl(params);
    }
    if (params->url_request.url == empty_frame_meta_origin_trial_url()) {
      return RespondForEmptyUrl(params);
    }
    if (params->url_request.url == same_site_cross_origin_url()) {
      return RespondForEmptyUrl(params);
    }

    return false;
  }

  base::test::ScopedFeatureList test_features_;
  std::string origin_trial_token_;
  std::unique_ptr<URLLoaderInterceptor> url_loader_interceptor_;
};

// Check that the RuntimeFeatureStateDocumentData is altered when we receive a
// OriginTrialStateHost IPC.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplWithTokensBrowserTest,
                       DocumentDataAltered) {
  // Generated with:
  // tools/origin_trials/generate_token.py https://127.0.0.1:44444
  // DisableThirdPartyStoragePartitioning2
  // --expire-timestamp=2000000000
  const char kValidFirstPartyToken[] =
      "AxMOenT1sG/"
      "yhbr90XJPbHZ9fHn4dUuB9ti4X+Ec1QneGq68WfIZCMJ9w9ZI0AjpyceLwlFpxe/"
      "mdVIf3VJCwwoAAABveyJvcmlnaW4iOiAiaHR0cHM6Ly8xMjcuMC4wLjE6NDQ0NDQiLCAiZmV"
      "hdHVyZSI6ICJEaXNhYmxlVGhpcmRQYXJ0eVN0b3JhZ2VQYXJ0aXRpb25pbmcyIiwgImV4cGl"
      "yeSI6IDIwMDAwMDAwMDB9";

  SetOriginTrialToken(kValidFirstPartyToken);
  EXPECT_TRUE(NavigateToURL(shell(), simple_origin_trial_url()));

  // Create a test remote to initiate the IPC.
  mojo::Remote<blink::mojom::OriginTrialStateHost>
      origin_trial_state_host_remote;
  OriginTrialStateHostImpl::Create(
      web_contents()->GetPrimaryMainFrame(),
      origin_trial_state_host_remote.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(origin_trial_state_host_remote.is_connected());

  // Before ApplyFeatureDiffForOriginTrial() is called, we expect that the
  // feature overrides will be empty.
  auto expected_overrides =
      base::flat_map<blink::mojom::RuntimeFeature, bool>();
  RuntimeFeatureStateDocumentData* actual_document_data =
      RuntimeFeatureStateDocumentData::GetForCurrentDocument(
          web_contents()->GetPrimaryMainFrame());
  EXPECT_EQ(expected_overrides,
            actual_document_data->runtime_feature_state_read_context()
                .GetFeatureOverrides());

  // Simulate receiving a feature diff from the renderer process.
  auto overrides_with_tokens =
      base::flat_map<blink::mojom::RuntimeFeature,
                     blink::mojom::OriginTrialFeatureStatePtr>();
  std::string raw_token(kValidFirstPartyToken);
  std::vector<std::string> raw_tokens_vector{raw_token};
  overrides_with_tokens
      [blink::mojom::RuntimeFeature::kDisableThirdPartyStoragePartitioning2] =
          blink::mojom::OriginTrialFeatureState::New(true, raw_tokens_vector);
  origin_trial_state_host_remote.get()->ApplyFeatureDiffForOriginTrial(
      std::move(overrides_with_tokens));

  // Create the set of expected overrides without the corresponding tokens.
  expected_overrides
      [blink::mojom::RuntimeFeature::kDisableThirdPartyStoragePartitioning2] =
          true;

  // Verify that the document data was altered with the correct overrides.
  origin_trial_state_host_remote.FlushForTesting();
  EXPECT_EQ(expected_overrides,
            actual_document_data->runtime_feature_state_read_context()
                .GetFeatureOverrides());
}

// Tests that the frame's RuntimeFeatureStateDocumentData is in a valid
// state when
// * The frame has crashed
// * It is reloaded
// * The renderer attempts to apply a header OT token before the frame has been
// finished committing. (While header OT tokens aren't supported by
// RuntimeFeatureState, we still shouldn't crash if someone tries to use one.)
//
// Also tests that even if a dummy RuntimeFeatureStateDocumentData is created,
// the NavigationRequest's RuntimeFeatureStateContext will overwrite it.
IN_PROC_BROWSER_TEST_F(
    RenderFrameHostImplWithTokensBrowserTest,
    ReloadedCrashedFrameWithHeaderOriginTrialShouldHaveValidRuntimeFeatureStateDocumentData) {
  // Generated with:
  // tools/origin_trials/generate_token.py https://127.0.0.1:44440
  // DisableThirdPartyStoragePartitioning2
  // --expire-timestamp=2000000000
  const char kValidFirstPartyTokenForEmptyUrl[] =
      "A68fOEp2t0jAR/ewxM8TMwuZRCCCqT5+w/"
      "lmt6pgABRYKg+"
      "3Ix7S3pe5kqXLTdRgCKKQUeXdGL24tSeDb5cFbwUAAABveyJvcmlnaW4iOiAiaHR0cHM6Ly8"
      "xMjcuMC4wLjE6NDQ0NDAiLCAiZmVhdHVyZSI6ICJEaXNhYmxlVGhpcmRQYXJ0eVN0b3JhZ2V"
      "QYXJ0aXRpb25pbmcyIiwgImV4cGlyeSI6IDIwMDAwMDAwMDB9";

  SetOriginTrialToken(kValidFirstPartyTokenForEmptyUrl);
  EXPECT_TRUE(NavigateToURL(shell(), empty_page_url()));

  // Crash the frame.
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* rfh = web_contents->GetPrimaryMainFrame();
  RenderProcessHost* process = rfh->GetProcess();
  {
    RenderProcessHostWatcher crash_observer(
        process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    process->Shutdown(0);
    crash_observer.Wait();
  }
  ASSERT_FALSE(rfh->IsRenderFrameLive());

  // Create an observer that will set the state of
  // DisableThirdPartyStoragePartitioning2 to true once the navigation begins to
  // commit.
  class ReadyToCommitObserver : public WebContentsObserver {
   public:
    explicit ReadyToCommitObserver(WebContentsImpl* web_contents)
        : WebContentsObserver(web_contents) {}

    // WebContentsObserver:
    void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override {
      navigation_handle->GetMutableRuntimeFeatureStateContext()
          .SetDisableThirdPartyStoragePartitioning2Enabled(true);
    }
  };
  ReadyToCommitObserver commit_observer(web_contents);

  // Create a message delayer that will mock a header OT token being applied
  // before the navigation finishes committing.
  auto did_commit_callback =
      base::BindLambdaForTesting([&](RenderFrameHost* rfh) {
        // Create a test remote to initiate the IPC.
        mojo::Remote<blink::mojom::OriginTrialStateHost>
            origin_trial_state_host_remote;
        OriginTrialStateHostImpl::Create(
            web_contents->GetPrimaryMainFrame(),
            origin_trial_state_host_remote.BindNewPipeAndPassReceiver());
        ASSERT_TRUE(origin_trial_state_host_remote.is_connected());

        auto overrides_with_tokens =
            base::flat_map<blink::mojom::RuntimeFeature,
                           blink::mojom::OriginTrialFeatureStatePtr>();
        std::string raw_token(kValidFirstPartyTokenForEmptyUrl);
        std::vector<std::string> raw_tokens_vector{raw_token};
        overrides_with_tokens[blink::mojom::RuntimeFeature::
                                  kDisableThirdPartyStoragePartitioning2] =
            blink::mojom::OriginTrialFeatureState::New(true, raw_tokens_vector);
        origin_trial_state_host_remote.get()->ApplyFeatureDiffForOriginTrial(
            std::move(overrides_with_tokens));

        origin_trial_state_host_remote.FlushForTesting();

        RuntimeFeatureStateDocumentData* document_data =
            RuntimeFeatureStateDocumentData::GetForCurrentDocument(
                web_contents->GetPrimaryMainFrame());

        ASSERT_TRUE(document_data);
        // Confirm that attempting to check if the feature is enabled doesn't
        // result in a crash.
        //
        // The feature should still be disabled. This is because even though we
        // sent an, otherwise, valid token we're unable to verify it against
        // the expected url since the frame has a last committed origin of null
        // at this point in time.
        // Additionally, the RuntimeFeatureStateContext in the navigation
        // request hasn't yet been saved into the DocumentData.
        EXPECT_FALSE(document_data->runtime_feature_state_read_context()
                         .IsDisableThirdPartyStoragePartitioning2Enabled());
      });
  CommitMessageDelayer commit_delayer(web_contents,
                                      empty_page_url() /* deferred_url */,
                                      std::move(did_commit_callback));
  shell()->Reload();
  commit_delayer.Wait();

  RuntimeFeatureStateDocumentData* document_data =
      RuntimeFeatureStateDocumentData::GetForCurrentDocument(
          web_contents->GetPrimaryMainFrame());

  ASSERT_TRUE(document_data);
  // Now that the navigation has finished committing, the DocumentData should
  // contain the "true" set by the observer.
  EXPECT_TRUE(document_data->runtime_feature_state_read_context()
                  .IsDisableThirdPartyStoragePartitioning2Enabled());
}

// Check that the RuntimeFeatureStateDocumentData is not altered when we receive
// a OriginTrialStateHost IPC that contains an invalid token.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplWithTokensBrowserTest,
                       DocumentDataInvalidToken) {
  const char kInvalidToken[] = "invalid";

  SetOriginTrialToken(kInvalidToken);
  EXPECT_TRUE(NavigateToURL(shell(), simple_origin_trial_url()));

  // Create a test remote to initiate the IPC.
  mojo::Remote<blink::mojom::OriginTrialStateHost>
      origin_trial_state_host_remote;
  OriginTrialStateHostImpl::Create(
      web_contents()->GetPrimaryMainFrame(),
      origin_trial_state_host_remote.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(origin_trial_state_host_remote.is_connected());

  // Before ApplyFeatureDiffForOriginTrial() is called, we expect that the
  // feature overrides will be empty.
  auto expected_overrides =
      base::flat_map<blink::mojom::RuntimeFeature, bool>();
  RuntimeFeatureStateDocumentData* actual_document_data =
      RuntimeFeatureStateDocumentData::GetForCurrentDocument(
          web_contents()->GetPrimaryMainFrame());
  EXPECT_EQ(expected_overrides,
            actual_document_data->runtime_feature_state_read_context()
                .GetFeatureOverrides());

  // Simulate receiving a feature diff from the renderer process.
  auto overrides_with_tokens =
      base::flat_map<blink::mojom::RuntimeFeature,
                     blink::mojom::OriginTrialFeatureStatePtr>();
  std::string raw_token(kInvalidToken);
  std::vector<std::string> raw_tokens_vector{raw_token};
  overrides_with_tokens
      [blink::mojom::RuntimeFeature::kDisableThirdPartyStoragePartitioning2] =
          blink::mojom::OriginTrialFeatureState::New(true, raw_tokens_vector);
  origin_trial_state_host_remote.get()->ApplyFeatureDiffForOriginTrial(
      std::move(overrides_with_tokens));

  // Verify that no feature overrides were added.
  origin_trial_state_host_remote.FlushForTesting();
  EXPECT_EQ(expected_overrides,
            actual_document_data->runtime_feature_state_read_context()
                .GetFeatureOverrides());
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplWithTokensBrowserTest,
                       BrowserValidatesTokensFromMetaTags) {
  // Generated with
  // tools/origin_trials/generate_token.py https://127.0.0.1:44444 \
  // FrobulatePersistent --expire-timestamp=2000000000
  const char kValidToken[] =
      "A156ll3LJpuECt+dqMQpOAg3H6ayl6AfL6v3Lf/"
      "pu2RWy4VHhJ6dnNYoaLbdXnhQUmOxjxcoIarc6lrgGvmKBwgAAABdeyJvcmlnaW4iOiAiaHR"
      "0cHM6Ly8xMjcuMC4wLjE6NDQ0NDQiLCAiZmVhdHVyZSI6ICJGcm9idWxhdGVQZXJzaXN0ZW5"
      "0IiwgImV4cGlyeSI6IDIwMDAwMDAwMDB9";
  base::Time validTime = base::Time::FromSecondsSinceUnixEpoch(1000000000);
  SetOriginTrialToken(kValidToken);

  EXPECT_TRUE(NavigateToURL(shell(), meta_tag_origin_trial_url()));
  // Navigate to a different page as a means of waiting, due to flakiness
  // caused by a race between the meta tag being pushed and us checking the
  // origin trial.
  EXPECT_TRUE(NavigateToURL(shell(), empty_page_url()));

  OriginTrialsControllerDelegate* delegate = GetOriginTrialsDelegate();

  url::Origin trial_origin = url::Origin::Create(GURL(kOriginTrialUrl));
  EXPECT_TRUE(delegate->IsFeaturePersistedForOrigin(
      /*origin=*/trial_origin, /*partition_origin=*/trial_origin,
      blink::mojom::OriginTrialFeature::kOriginTrialsSampleAPIPersistentFeature,
      validTime));
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplWithTokensBrowserTest,
                       BrowserRejectsInvalidTokensFromMetaTags) {
  const char kInvalidToken[] = "invalid";
  base::Time validTime = base::Time::FromSecondsSinceUnixEpoch(1000000000);
  SetOriginTrialToken(kInvalidToken);

  EXPECT_TRUE(NavigateToURL(shell(), meta_tag_origin_trial_url()));
  // Navigate to a different page as a means of waiting, due to flakiness
  // caused by a race between the meta tag being pushed and us checking the
  // origin trial.
  EXPECT_TRUE(NavigateToURL(shell(), empty_page_url()));

  OriginTrialsControllerDelegate* delegate = GetOriginTrialsDelegate();

  url::Origin trial_origin = url::Origin::Create(GURL(kOriginTrialUrl));
  EXPECT_TRUE(delegate
                  ->GetPersistedTrialsForOrigin(
                      /*origin=*/trial_origin,
                      /*partition_origin=*/trial_origin, validTime)
                  .empty());
}

IN_PROC_BROWSER_TEST_F(
    RenderFrameHostImplWithTokensBrowserTest,
    BrowserValidatesThirdPartyDeprecationTokensFromMetaTags) {
  // Generated with
  // tools/origin_trials/generate_token.py https://127.0.0.1:44445
  // FrobulatePersistentThirdPartyDeprecation --expire-timestamp=2000000000
  // --is-third-party
  const char kValidToken[] =
      "A8B9KtAVHmLw5hAydE4P2L/"
      "AU3V2CWYHQyFtbm2EJIED3tLM4MTg85xMiXrwj8fzQZbIEvnJjJV+"
      "azzrkWAxUw8AAACIeyJvcmlnaW4iOiAiaHR0cHM6Ly8xMjcuMC4wLjE6NDQ0NDUiLCAiZmVh"
      "dHVyZSI6ICJGcm9idWxhdGVQZXJzaXN0ZW50VGhpcmRQYXJ0eURlcHJlY2F0aW9uIiwgImV4"
      "cGlyeSI6IDIwMDAwMDAwMDAsICJpc1RoaXJkUGFydHkiOiB0cnVlfQ==";
  base::Time validTime = base::Time::FromSecondsSinceUnixEpoch(1000000000);
  SetOriginTrialToken(kValidToken);

  EXPECT_TRUE(NavigateToURL(shell(), script_meta_tag_origin_trial_url()));
  // Navigate to a different page as a means of waiting, due to flakiness
  // caused by a race between the meta tag being pushed and us checking the
  // origin trial.
  EXPECT_TRUE(NavigateToURL(shell(), empty_page_url()));

  OriginTrialsControllerDelegate* delegate = GetOriginTrialsDelegate();

  // The Trial should be enabled in the context where it was set.
  url::Origin main_origin = url::Origin::Create(GURL(kOriginTrialUrl));
  url::Origin trial_origin = url::Origin::Create(GURL(kThirdPartyScriptUrl));
  EXPECT_TRUE(delegate->IsFeaturePersistedForOrigin(
      /*origin=*/trial_origin, /*partition_origin=*/main_origin,
      blink::mojom::OriginTrialFeature::
          kOriginTrialsSampleAPIPersistentThirdPartyDeprecationFeature,
      validTime));
}

IN_PROC_BROWSER_TEST_F(
    RenderFrameHostImplWithTokensBrowserTest,
    ReusedChildFrameNavigatedFromDeprecationTrialIsPartitioned) {
  // Generated with
  // tools/origin_trials/generate_token.py https://127.0.0.1:44445
  // DisableThirdPartyStoragePartitioning2 --expire-timestamp=2000000000
  // --is-third-party
  const char kValidToken[] =
      "A1HN+j5dGwYe307k+"
      "ljKWOpwMh6rXnk3mFDsOs0TG2ibF9tOnChGQCrhjn6oJXxmZxeU91hgMBfI48Cm+"
      "iswgg8AAACFeyJvcmlnaW4iOiAiaHR0cHM6Ly8xMjcuMC4wLjE6NDQ0NDUiLCAiZmVhdHVyZ"
      "SI6ICJEaXNhYmxlVGhpcmRQYXJ0eVN0b3JhZ2VQYXJ0aXRpb25pbmcyIiwgImV4cGlyeSI6I"
      "DIwMDAwMDAwMDAsICJpc1RoaXJkUGFydHkiOiB0cnVlfQ==";
  SetOriginTrialToken(kValidToken);

  // Navigate to "a.com" and load a script from a third-party. In that script,
  // the deprecation trial token above is added via <meta> tag. Then, the script
  // adds an iframe.
  EXPECT_TRUE(
      NavigateToURL(shell(), cross_site_script_meta_tag_origin_trial_url()));
  RenderFrameHostImpl* child_frame =
      static_cast<RenderFrameHostImpl*>(ChildFrameAt(shell(), 0));
  ASSERT_TRUE(child_frame);
  // Navigate the currently empty iframe to a URL that is same-site with the
  // third-party script.
  EXPECT_TRUE(NavigateToURLFromRenderer(child_frame,
                                        empty_frame_meta_origin_trial_url()));
  // Execute a dummy roundtrip to ensure the <meta> tag trial token has time to
  // parse and be applied to the iframe.
  EXPECT_TRUE(ExecJs(shell(), ";"));

  // Re-obtain the iframe after confirming the navigation is complete. If
  // deprecation trial is registered correctly, its StorageKey will be
  // first-party.
  child_frame = static_cast<RenderFrameHostImpl*>(ChildFrameAt(shell(), 0));
  EXPECT_TRUE(child_frame->GetStorageKey().IsFirstPartyContext());

  // Calculate the StorageKey when providing a same-site, cross-origin
  // `new_rfh_origin`, which simulates a navigation where the RenderFrameHost
  // would be reused.
  url::Origin new_rfh_origin =
      url::Origin::Create(same_site_cross_origin_url());
  blink::StorageKey new_storage_key =
      child_frame->CalculateStorageKey(new_rfh_origin, /*nonce=*/nullptr);
  // Ensure that the StorageKey is third-party, even though the
  // RenderFrameHost we "reused" had ThirdPartyStoragePartitioning
  // disabled via deprecation trial.
  EXPECT_TRUE(new_storage_key.IsThirdPartyContext());
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplWithTokensBrowserTest,
                       BrowserRejectsThirdPartyMetaTagsNonDeprecation) {
  // Generated with
  // tools/origin_trials/generate_token.py https://127.0.0.1:44445 \
  // FrobulatePersistent --expire-timestamp=2000000000 --is-third-party
  const char kValidToken[] =
      "A5It5cGJLhpgVjvJ/GFD/hJci1G7qeWdhAdZEJ4/"
      "RQz0ZtVOfRufZVsYjATJe5DNuDjLtH4IjC5tYUQiDq6hsQgAAABzeyJvcmlnaW4iOiAiaHR0"
      "cHM6Ly8xMjcuMC4wLjE6NDQ0NDUiLCAiZmVhdHVyZSI6ICJGcm9idWxhdGVQZXJzaXN0ZW50"
      "IiwgImV4cGlyeSI6IDIwMDAwMDAwMDAsICJpc1RoaXJkUGFydHkiOiB0cnVlfQ==";
  base::Time validTime = base::Time::FromSecondsSinceUnixEpoch(1000000000);
  SetOriginTrialToken(kValidToken);

  EXPECT_TRUE(NavigateToURL(shell(), script_meta_tag_origin_trial_url()));
  // Navigate to a different page as a means of waiting, due to flakiness
  // caused by a race between the meta tag being pushed and us checking the
  // origin trial.
  EXPECT_TRUE(NavigateToURL(shell(), empty_page_url()));

  OriginTrialsControllerDelegate* delegate = GetOriginTrialsDelegate();

  url::Origin main_origin = url::Origin::Create(GURL(kOriginTrialUrl));
  url::Origin trial_origin = url::Origin::Create(GURL(kThirdPartyScriptUrl));
  EXPECT_TRUE(delegate
                  ->GetPersistedTrialsForOrigin(
                      /*origin=*/trial_origin,
                      /*partition_origin=*/main_origin, validTime)
                  .empty());
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

  RenderFrameHostImplBeforeUnloadBrowserTest(
      const RenderFrameHostImplBeforeUnloadBrowserTest&) = delete;
  RenderFrameHostImplBeforeUnloadBrowserTest& operator=(
      const RenderFrameHostImplBeforeUnloadBrowserTest&) = delete;

  TestJavaScriptDialogManager* dialog_manager() {
    return dialog_manager_.get();
  }

  void CloseDialogAndProceed() {
    dialog_manager_->Run(true /* navigation should proceed */,
                         std::u16string());
  }

  void CloseDialogAndCancel() {
    dialog_manager_->Run(false /* navigation should proceed */,
                         std::u16string());
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
    EXPECT_TRUE(ExecJs(ftn, script));
  }

  int RetrievePingsFromMessageQueue(DOMMessageQueue* msg_queue) {
    int num_pings = 0;
    std::string message;
    while (msg_queue->PopMessage(&message)) {
      base::TrimString(message, "\"", &message);
      // Only count messages from beforeunload.  For example, an ExecJs
      // sends its own message to DOMMessageQueue, which we need to ignore.
      if (message == "ping")
        ++num_pings;
    }
    return num_pings;
  }

 protected:
  void SetUpOnMainThread() override {
    RenderFrameHostImplBrowserTest::SetUpOnMainThread();
    dialog_manager_ = std::make_unique<TestJavaScriptDialogManager>();
    web_contents()->SetDelegate(dialog_manager_.get());
  }

  void TearDownOnMainThread() override {
    web_contents()->SetDelegate(nullptr);
    web_contents()->SetJavaScriptDialogManagerForTesting(nullptr);
    RenderFrameHostImplBrowserTest::TearDownOnMainThread();
  }

 private:
  std::unique_ptr<TestJavaScriptDialogManager> dialog_manager_;
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
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  InstallBeforeUnloadHandler(root->child_at(0), SHOW_DIALOG);

  // This test assumes a beforeunload handler is present on the main frame.
  static_cast<RenderFrameHostImpl*>(web_contents()->GetPrimaryMainFrame())
      ->SuddenTerminationDisablerChanged(
          true,
          blink::mojom::SuddenTerminationDisablerType::kBeforeUnloadHandler);

  // Disable beforeunload timer to prevent flakiness.
  PrepContentsForBeforeUnloadTest(web_contents());

  // Navigate cross-site.
  GURL cross_site_url(embedded_test_server()->GetURL("c.com", "/title1.html"));
  shell()->LoadURL(cross_site_url);

  // Only the main frame should be marked as waiting for beforeunload completion
  // callback as the frame being navigated.
  RenderFrameHostImpl* main_frame = web_contents()->GetPrimaryMainFrame();
  RenderFrameHostImpl* child = root->child_at(0)->current_frame_host();
  EXPECT_TRUE(main_frame->is_waiting_for_beforeunload_completion());
  EXPECT_FALSE(child->is_waiting_for_beforeunload_completion());

  // Sanity check that the main frame is waiting for subframe's beforeunload
  // ACK.
  EXPECT_EQ(main_frame, child->GetBeforeUnloadInitiator());
  EXPECT_EQ(main_frame, main_frame->GetBeforeUnloadInitiator());

  // When in a strict SiteInstances mode, LoadURL() should trigger two
  // beforeunload IPCs for subframe and the main frame: the subframe has a
  // beforeunload handler, and while the main frame does not, we always send the
  // IPC to navigating frames, regardless of whether or not they have a handler.
  //
  // Without strict SiteInstances, only one beforeunload IPC should be sent to
  // the main frame, which will handle both (same-process) frames.
  EXPECT_EQ(AreStrictSiteInstancesEnabled() ? 2u : 1u,
            main_frame->beforeunload_pending_replies_.size());

  // Wait for the beforeunload dialog to be shown from the subframe.
  dialog_manager()->Wait();

  // The main frame should still be waiting for subframe's beforeunload
  // completion callback.
  EXPECT_EQ(main_frame, child->GetBeforeUnloadInitiator());
  EXPECT_EQ(main_frame, main_frame->GetBeforeUnloadInitiator());
  EXPECT_TRUE(main_frame->is_waiting_for_beforeunload_completion());
  EXPECT_FALSE(child->is_waiting_for_beforeunload_completion());

  // In a strict SiteInstances mode, the beforeunload completion callback should
  // happen on the child RFH.  Without strict SiteInstances, it will come from
  // the main frame RFH, which processes beforeunload for both main frame and
  // child frame, since they are in the same process and SiteInstance.
  RenderFrameHostImpl* frame_that_sent_beforeunload_ipc =
      AreStrictSiteInstancesEnabled() ? child : main_frame;
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
  EXPECT_FALSE(web_contents()
                   ->GetPrimaryMainFrame()
                   ->is_waiting_for_beforeunload_completion());

  // The navigation that succeeded was a browser-initiated, main frame
  // navigation, so it swapped RenderFrameHosts. |main_frame| should either be
  // in pending deletion and waiting for unload ACK or enter back-forward cache,
  // but it should not be waiting for the beforeunload completion callback.
  EXPECT_THAT(
      main_frame->lifecycle_state(),
      testing::AnyOf(testing::Eq(LifecycleStateImpl::kRunningUnloadHandlers),
                     testing::Eq(LifecycleStateImpl::kInBackForwardCache)));
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
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
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
  DOMMessageQueue msg_queue(web_contents());
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
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  InstallBeforeUnloadHandler(root->child_at(0), SEND_PING | SHOW_DIALOG);
  InstallBeforeUnloadHandler(root->child_at(0)->child_at(0),
                             SEND_PING | SHOW_DIALOG);

  // Disable beforeunload timer to prevent flakiness.
  PrepContentsForBeforeUnloadTest(web_contents());

  // Navigate and wait for the beforeunload dialog to be shown from one of the
  // frames.
  DOMMessageQueue msg_queue(web_contents());
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
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  InstallBeforeUnloadHandler(root, SEND_PING);
  InstallBeforeUnloadHandler(root->child_at(0), SEND_PING);

  // Install a beforeunload handler in the b.com frame to put up a dialog.
  InstallBeforeUnloadHandler(root->child_at(1), SHOW_DIALOG);

  // Disable beforeunload timer to prevent flakiness.
  PrepContentsForBeforeUnloadTest(web_contents());

  // Start a same-site renderer-initiated navigation.  The beforeunload dialog
  // from the b.com frame should be shown.  The other two a.com frames should
  // send pings from their beforeunload handlers.
  DOMMessageQueue msg_queue(web_contents());
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
  ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());
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
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  InstallBeforeUnloadHandler(root, SEND_PING);
  InstallBeforeUnloadHandler(root->child_at(0), SEND_PING);
  InstallBeforeUnloadHandler(root->child_at(0)->child_at(0), SEND_PING);
  InstallBeforeUnloadHandler(root->child_at(1), SEND_PING);

  // Disable beforeunload timer to prevent flakiness.
  PrepContentsForBeforeUnloadTest(web_contents());

  // Start a renderer-initiated navigation in the middle frame.
  DOMMessageQueue msg_queue(web_contents());
  GURL new_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  TestNavigationManager navigation_manager(web_contents(), new_url);
  // Use ExecuteScriptAsync because a ping may arrive before the script
  // execution completion notification and confuse our expectations.
  ExecuteScriptAsync(root->child_at(0),
                     "location.href = '" + new_url.spec() + "';");
  ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());
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
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  std::string script =
      "window.onbeforeunload = () => { "
      "  document.body.removeChild(document.querySelector('iframe'));"
      "}";
  EXPECT_TRUE(ExecJs(root, script));

  // Install a beforeunload handler which never finishes in subframe.
  EXPECT_TRUE(ExecJs(root->child_at(0),
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
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  InstallBeforeUnloadHandler(root, SEND_PING);
  InstallBeforeUnloadHandler(root->child_at(0), SEND_PING);
  InstallBeforeUnloadHandler(root->child_at(0)->child_at(0), SEND_PING);
  InstallBeforeUnloadHandler(root->child_at(0)->child_at(0)->child_at(0),
                             SEND_PING);

  // Disable beforeunload timer to prevent flakiness.
  PrepContentsForBeforeUnloadTest(web_contents());

  // Navigate the main frame.
  DOMMessageQueue msg_queue(web_contents());
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
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  InstallBeforeUnloadHandler(root, SEND_PING);

  // Install a beforeunload handler which never finishes in subframe.
  EXPECT_TRUE(ExecJs(root->child_at(0),
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
  RenderFrameHostImpl* main_frame = web_contents()->GetPrimaryMainFrame();

  // Install a beforeunload handler to show a dialog in both frames.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  InstallBeforeUnloadHandler(root, SHOW_DIALOG);
  InstallBeforeUnloadHandler(root->child_at(0), SHOW_DIALOG);

  // Extend the beforeunload timeout to prevent flakiness.  This test can't use
  // PrepContentsForBeforeUnloadTest(), as that clears the timer altogether,
  // and this test needs the timer to be valid, to see whether it gets paused
  // and not restarted correctly.
  main_frame->SetBeforeUnloadTimeoutDelayForTesting(base::Seconds(30));

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
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return main_frame->beforeunload_pending_replies_.size() <= 1; }));

  // Ensure that the beforeunload timer hasn't been restarted, since the first
  // beforeunload dialog is still up at this point.
  EXPECT_FALSE(main_frame->beforeunload_timeout_->IsRunning());

  // Cancel the dialog and make sure we stay on the old page.
  CloseDialogAndCancel();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  EXPECT_EQ(main_url, web_contents()->GetLastCommittedURL());
}

// During a complex WebContents destruction, test resuming a navigation, due to
// of a beforeunloader. This is a regersion test for: https://crbug.com/1147567.
// - Start from A(B(C))
// - C adds a beforeunload handler.
// - B starts a navigation, waiting for C.
// - The WebContents is closed, which deletes C, then B, then A.
// When deleting C, the navigations in B can begin, but this happen while B was
// destructing itself.
//
// Note: This needs 3 nested documents instead of 2, because deletion of the
// main RenderFrameHost is different from normal RenderFrameHost. This is
// required to reproduce https://crbug.com/1147567.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBeforeUnloadBrowserTest,
                       CloseWebContent) {
  // This test exercises a scenario that's only possible with
  // --site-per-process.
  if (!AreAllSitesIsolatedForTesting())
    return;

  // For unknown reasons, it seems required to start from a "live"
  // RenderFrameHost. Otherwise creating a new Shell below will crash.
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  GURL url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c))");
  Shell* new_shell = Shell::CreateNewWindow(
      web_contents()->GetController().GetBrowserContext(), url, nullptr,
      gfx::Size());
  auto* web_contents = static_cast<WebContentsImpl*>(new_shell->web_contents());
  EXPECT_TRUE(WaitForLoadStop(web_contents));
  RenderFrameHostImpl* rfh_a = web_contents->GetPrimaryMainFrame();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();
  RenderFrameHostImpl* rfh_c = rfh_b->child_at(0)->current_frame_host();

  // C has a beforeunload handler, slow to reply.
  EXPECT_TRUE(ExecJs(rfh_c, "onbeforeunload = () => {while(1);}"));
  // B navigate elsewhere. This triggers C's beforeunload handler.
  EXPECT_TRUE(ExecJs(rfh_b, "location.href = 'about:blank';"));
  // Closing the Shell, this deletes C and causes the navigation above to start.
  new_shell->Close();
  // Test pass if this doesn't reach a CHECK.
}

namespace {

class OnDidStartNavigation : public WebContentsObserver {
 public:
  OnDidStartNavigation(WebContents* web_contents,
                       base::RepeatingClosure callback)
      : WebContentsObserver(web_contents), callback_(callback) {}

  void DidStartNavigation(NavigationHandle* navigation) override {
    callback_.Run();
  }

 private:
  base::RepeatingClosure callback_;
};

}  // namespace

// This test closes beforeunload dialog due to a new navigation starting from
// within WebContentsObserver::DidStartNavigation. This test succeeds if it
// doesn't crash with a UAF while loading the second page.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBeforeUnloadBrowserTest,
                       DidStartNavigationClosesDialog) {
  GURL url1 = embedded_test_server()->GetURL(
      "a.com", "/render_frame_host/beforeunload.html");
  GURL url2 = embedded_test_server()->GetURL("b.com", "/title1.html");

  EXPECT_TRUE(NavigateToURL(shell(), url1));

  auto weak_web_contents = web_contents()->GetWeakPtr();
  // This matches the behaviour of TabModalDialogManager in
  // components/javascript_dialogs.
  OnDidStartNavigation close_dialog(web_contents(),
                                    base::BindLambdaForTesting([&]() {
                                      CloseDialogAndCancel();

                                      // Check that web_contents() were not
                                      // deleted.
                                      ASSERT_TRUE(weak_web_contents);
                                    }));

  web_contents()->GetPrimaryMainFrame()->RunBeforeUnloadConfirm(
      true, base::DoNothing());

  EXPECT_TRUE(NavigateToURL(shell(), url2));
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

  ExecuteScriptBeforeRenderFrameDeletedHelper(
      const ExecuteScriptBeforeRenderFrameDeletedHelper&) = delete;
  ExecuteScriptBeforeRenderFrameDeletedHelper& operator=(
      const ExecuteScriptBeforeRenderFrameDeletedHelper&) = delete;

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
  EXPECT_TRUE(ExecJs(shell(), open_script));
  second_contents_navigation_observer.Wait();

  ASSERT_EQ(2u, Shell::windows().size());
  Shell* new_shell = Shell::windows()[1];
  ExecuteScriptBeforeRenderFrameDeletedHelper deleted_observer(
      new_shell->web_contents()->GetPrimaryMainFrame(), "callWindowOpen();");
  new_shell->Close();
  deleted_observer.WaitUntilDeleted();

  EXPECT_EQ(true, EvalJs(shell(), "!!popup.didCallWindowOpen"));

  EXPECT_EQ("null", EvalJs(shell(), "String(popup.resultOfWindowOpen)"));
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

class DocumentUkmSourceIdObserver : public WebContentsObserver {
 public:
  explicit DocumentUkmSourceIdObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  DocumentUkmSourceIdObserver(const DocumentUkmSourceIdObserver&) = delete;
  DocumentUkmSourceIdObserver& operator=(const DocumentUkmSourceIdObserver&) =
      delete;

  ukm::SourceId GetPrimaryMainFrameDocumentUkmSourceId() {
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
  // TODO(crbug.com/40137364): This should also work on cross-RFH same-site
  // navigations.
  if (ShouldCreateNewHostForAllFrames()) {
    return;
  }
  DisableProactiveBrowsingInstanceSwapFor(
      web_contents()->GetPrimaryMainFrame());

  // Now make a navigation. |observer| will make a document.open() call at
  // ReadyToCommit time - see
  // NavigationHandleGrabber::SendingNavigationCommitted(). The navigation
  // should get aborted because of the document.open() in the navigating RFH.
  NavigationHandleGrabber observer(web_contents());
  const std::u16string title = u"done";
  EXPECT_TRUE(ExecJs(web_contents(), "window.location.href='/title2.html'"));
  observer.WaitForTitle2();
  // Flush IPCs to make sure the renderer didn't tell us to navigate. Need to
  // make two round trips.
  EXPECT_TRUE(ExecJs(web_contents(), ""));
  EXPECT_TRUE(ExecJs(web_contents(), ""));
  EXPECT_FALSE(observer.committed_title2());
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       TerminationDisablersClearedOnRendererCrash) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GetTestUrl("render_frame_host", "beforeunload.html")));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  RenderFrameHostImpl* main_rfh1 = web_contents()->GetPrimaryMainFrame();

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

  RenderFrameHostImpl* main_rfh2 = web_contents()->GetPrimaryMainFrame();
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
  static constexpr char kSendSlowXhr[] =
      "var request = new XMLHttpRequest();"
      "request.addEventListener('abort', () => document.title = 'xhr aborted');"
      "request.addEventListener('load', () => document.title = 'xhr loaded');"
      "request.open('GET', '%s');"
      "request.send();";
  const GURL slow_url = embedded_test_server()->GetURL("/xhr_request");
  EXPECT_TRUE(ExecJs(
      shell(), base::StringPrintf(kSendSlowXhr, slow_url.spec().c_str())));
  xhr_response.WaitForRequest();

  // 2) In the meantime, create a renderer-initiated navigation. It will be
  // aborted.
  TestNavigationManager observer(shell()->web_contents(),
                                 GURL("customprotocol:aborted"));
  EXPECT_TRUE(ExecJs(shell(), "window.location = 'customprotocol:aborted'"));
  EXPECT_FALSE(observer.WaitForResponse());
  ASSERT_TRUE(observer.WaitForNavigationFinished());

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
  const std::u16string xhr_aborted_title = u"xhr aborted";
  const std::u16string xhr_loaded_title = u"xhr loaded";
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
        shell()->web_contents()->GetPrimaryMainFrame()));
    std::string done;
    EXPECT_TRUE(dom_message_queue.WaitForMessage(&done));
    EXPECT_EQ("\"done\"", done);
  }

  // 3) The end of the response is issued. The renderer must be able to receive
  //    it.
  {
    const std::u16string document_loaded_title = u"document loaded";
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
      shell()->web_contents()->GetPrimaryMainFrame());
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
  ASSERT_TRUE(observer_same_document.WaitForNavigationFinished());

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
      mojom::DidCommitProvisionalLoadParamsPtr* params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr* interface_params)
      override {
    url_of_last_commit_ = (**params).url;
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
                                std::string_view interface_name,
                                base::RepeatingClosure callback)
      : rfhi_(render_frame_host),
        impl_(receiver().SwapImplForTesting(this)),
        interface_name_(interface_name),
        request_callback_(callback) {}

  ScopedInterfaceRequestMonitor(const ScopedInterfaceRequestMonitor&) = delete;
  ScopedInterfaceRequestMonitor& operator=(
      const ScopedInterfaceRequestMonitor&) = delete;

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

  raw_ptr<RenderFrameHostImpl> rfhi_;
  raw_ptr<blink::mojom::BrowserInterfaceBroker> impl_;

  std::string interface_name_;
  base::RepeatingClosure request_callback_;
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
      committing_rfh,
      base::BindLambdaForTesting([&did_finish_navigation](NavigationHandle*) {
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
  ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());
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

    ASSERT_TRUE(NavigateToURLAndDoNotWaitForLoadStop(shell(), first_url));
    ASSERT_EQ(first_url, injector.url_of_last_commit());
    ASSERT_TRUE(injector.original_receiver_of_last_commit().is_valid());
  }

  // The test below only works for same-RFH navigations, so we need to ensure
  // that we won't trigger a same-site cross-RFH navigation.
  DisableProactiveBrowsingInstanceSwapFor(
      web_contents()->GetPrimaryMainFrame());
  if (ShouldCreateNewHostForAllFrames()) {
    return;
  }

  // Prepare an interface receiver for FrameHostTestInterface.
  mojo::Remote<mojom::FrameHostTestInterface> test_interface;
  auto test_interface_receiver = test_interface.BindNewPipeAndPassReceiver();

  // Set up |dispatched_interface_request_callback| that would be invoked if the
  // interface receiver for FrameHostTestInterface was ever dispatched to the
  // RenderFrameHostImpl.
  base::MockCallback<base::RepeatingClosure>
      dispatched_interface_request_callback;
  auto* main_rfh = web_contents()->GetPrimaryMainFrame();
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
      main_rfh, base::BindLambdaForTesting([&](NavigationHandle*) {
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
  ASSERT_TRUE(NavigateToURLAndDoNotWaitForLoadStop(shell(), second_url));

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
    ASSERT_TRUE(ExecJs(shell(), script));

    EXPECT_TRUE(WaitForLoadStop(web_contents()));

    FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
    ASSERT_EQ(1u, root->child_count());
    FrameTreeNode* child = root->child_at(0u);

    EXPECT_FALSE(injector.original_receiver_of_last_commit().is_valid());
    EXPECT_FALSE(child->is_on_initial_empty_document());
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

  ASSERT_TRUE(ExecJs(shell(), kNavigateToOneThenTwoScript));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1u, root->child_count());
  FrameTreeNode* child = root->child_at(0u);

  EXPECT_TRUE(child->is_on_initial_empty_document());
  EXPECT_EQ(kSubframeURLTwo, child->current_url());
  EXPECT_EQ(url::Origin::Create(kMainFrameURL), child->current_origin());

  // Set the `src` attribute again to trigger navigation (3).

  TestFrameNavigationObserver commit_observer(child->current_frame_host());
  ScopedFakeInterfaceBrokerRequestInjector injector(web_contents());
  injector.set_fake_receiver_for_next_commit(mojo::NullReceiver());

  ASSERT_TRUE(ExecJs(shell(), kNavigateToThreeScript));
  commit_observer.WaitForCommit();
  EXPECT_FALSE(injector.original_receiver_of_last_commit().is_valid());

  EXPECT_FALSE(child->is_on_initial_empty_document());
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
  net::SchemefulSite main_frame_site = net::SchemefulSite(main_frame_url);
  net::NetworkIsolationKey expected_main_frame_key =
      net::NetworkIsolationKey(main_frame_site, main_frame_site);
  GURL subframe_url_one("about:blank");
  GURL subframe_url_two("about:blank#foo");
  GURL subframe_url_three(
      embedded_test_server()->GetURL("foo.com", "/title2.html"));
  url::Origin subframe_origin_three = url::Origin::Create(subframe_url_three);
  net::NetworkIsolationKey expected_subframe_key_three =
      net::NetworkIsolationKey(main_frame_site,
                               net::SchemefulSite(subframe_origin_three));

  // Main frame navigation.
  ASSERT_TRUE(NavigateToURL(shell(), main_frame_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  CheckURLOriginAndNetworkIsolationKey(root, main_frame_url, main_frame_origin,
                                       expected_main_frame_key);

  // Create iframe.
  ASSERT_TRUE(ExecJs(shell(), R"(
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
  ASSERT_TRUE(ExecJs(shell(), R"(
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
  ASSERT_TRUE(ExecJs(shell(), subframe_script_three));
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
  net::SchemefulSite main_frame_site = net::SchemefulSite(main_frame_url);
  net::NetworkIsolationKey expected_main_frame_key =
      net::NetworkIsolationKey(main_frame_site, main_frame_site);

  GURL popup_url_one("about:blank");
  GURL popup_url_two("about:blank#foo");
  GURL popup_url_three(
      embedded_test_server()->GetURL("foo.com", "/title2.html"));
  url::Origin popup_origin_three = url::Origin::Create(popup_url_three);
  net::SchemefulSite pop_site_three = net::SchemefulSite(popup_url_three);
  net::NetworkIsolationKey expected_popup_key_three =
      net::NetworkIsolationKey(pop_site_three, pop_site_three);

  // Main frame navigation.
  ASSERT_TRUE(NavigateToURL(shell(), main_frame_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  CheckURLOriginAndNetworkIsolationKey(root, main_frame_url, main_frame_origin,
                                       expected_main_frame_key);

  // Create popup.
  WebContentsAddedObserver popup_observer;
  ASSERT_TRUE(ExecJs(shell(), "var w = window.open('');"));
  WebContentsImpl* popup =
      static_cast<WebContentsImpl*>(popup_observer.GetWebContents());

  FrameTreeNode* popup_frame = popup->GetPrimaryMainFrame()->frame_tree_node();
  CheckURLOriginAndNetworkIsolationKey(
      popup_frame, popup_url_one, main_frame_origin, expected_main_frame_key);
  EXPECT_EQ(root->current_frame_host()->GetProcess(),
            popup_frame->current_frame_host()->GetProcess());

  // Same-document navigation of popup.
  ASSERT_TRUE(ExecJs(shell(), "w.location.hash = 'foo';"));
  EXPECT_TRUE(WaitForLoadStop(popup));

  CheckURLOriginAndNetworkIsolationKey(
      popup_frame, popup_url_two, main_frame_origin, expected_main_frame_key);
  EXPECT_EQ(root->current_frame_host()->GetProcess(),
            popup_frame->current_frame_host()->GetProcess());

  // Cross-document navigation of popup.
  TestFrameNavigationObserver commit_observer(
      popup_frame->current_frame_host());
  ASSERT_TRUE(
      ExecJs(shell(), JsReplace("w.location.href = $1;", popup_url_three)));
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
  net::SchemefulSite main_frame_site = net::SchemefulSite(main_frame_url);
  net::NetworkIsolationKey expected_network_isolation_key =
      net::NetworkIsolationKey(main_frame_site, main_frame_site);

  ASSERT_TRUE(NavigateToURL(shell(), main_frame_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  CheckURLOriginAndNetworkIsolationKey(root, main_frame_url, origin,
                                       expected_network_isolation_key);
  ASSERT_EQ(1u, root->child_count());

  CheckURLOriginAndNetworkIsolationKey(
      root->child_at(0), embedded_test_server()->GetURL("/title1.html"), origin,
      expected_network_isolation_key);
  RenderFrameHost* iframe = root->child_at(0)->current_frame_host();

  TestFrameNavigationObserver commit_observer(iframe);
  ASSERT_TRUE(ExecJs(iframe, "window.location = 'about:blank'"));
  commit_observer.WaitForCommit();

  ASSERT_EQ(1u, root->child_count());
  CheckURLOriginAndNetworkIsolationKey(root->child_at(0), GURL("about:blank"),
                                       origin, expected_network_isolation_key);
  // The iframe's SiteForCookies should first party with respect to
  // |main_frame_url|.
  EXPECT_TRUE(root->child_at(0)
                  ->current_frame_host()
                  ->ComputeSiteForCookies()
                  .IsFirstParty(main_frame_url));
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
  net::SchemefulSite main_frame_site = net::SchemefulSite(main_frame_url);

  net::NetworkIsolationKey expected_iframe_network_isolation_key(
      main_frame_site, net::SchemefulSite{iframe_origin});
  net::NetworkIsolationKey expected_main_frame_network_isolation_key(
      main_frame_site, main_frame_site);

  ASSERT_TRUE(NavigateToURL(shell(), main_frame_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
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
  net::SchemefulSite main_frame_site = net::SchemefulSite(main_frame_url);

  net::NetworkIsolationKey expected_innermost_iframe_network_isolation_key(
      main_frame_site, net::SchemefulSite{innermost_iframe_origin});
  net::NetworkIsolationKey expected_middle_iframe_network_isolation_key(
      main_frame_site, net::SchemefulSite{middle_iframe_origin});
  net::NetworkIsolationKey expected_main_frame_network_isolation_key(
      main_frame_site, main_frame_site);

  ASSERT_TRUE(NavigateToURL(shell(), main_frame_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
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

// Helper function to fetch the canonical link URL from the provided frame.
std::optional<GURL> GetCanonicalUrlFromFrame(RenderFrameHostImpl* frame) {
  base::RunLoop loop;
  std::optional<GURL> canon_url;
  frame->GetCanonicalUrl(
      base::BindLambdaForTesting([&](const std::optional<GURL>& url) {
        canon_url = url;
        loop.Quit();
      }));
  loop.Run();
  return canon_url;
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest, GetCanonicalUrl_None) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  base::HistogramTester histogram_tester;
  std::optional<GURL> canon_url =
      GetCanonicalUrlFromFrame(web_contents()->GetPrimaryMainFrame());
  // No canonical link should be returned if the page has none.
  ASSERT_FALSE(canon_url.has_value());
  content::FetchHistogramsFromChildProcesses();

#if BUILDFLAG(IS_ANDROID)
  ASSERT_TRUE(base::TimeTicks::IsHighResolution())
      << "The Blink.Frame.GetCanonicalUrlRendererTime histogram has "
         "microseconds precision and requires a high-resolution clock";
  histogram_tester.ExpectTotalCount("Blink.Frame.GetCanonicalUrlRendererTime",
                                    1);
#endif
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest, GetCanonicalUrl_InBody) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GetTestUrl("render_frame_host", "canonical_link_in_body.html")));

  base::HistogramTester histogram_tester;
  std::optional<GURL> canon_url =
      GetCanonicalUrlFromFrame(web_contents()->GetPrimaryMainFrame());
  // A canonical link in the body should be ignored.
  ASSERT_FALSE(canon_url.has_value());
  content::FetchHistogramsFromChildProcesses();

#if BUILDFLAG(IS_ANDROID)
  ASSERT_TRUE(base::TimeTicks::IsHighResolution())
      << "The Blink.Frame.GetCanonicalUrlRendererTime histogram has "
         "microseconds precision and requires a high-resolution clock";
  histogram_tester.ExpectTotalCount("Blink.Frame.GetCanonicalUrlRendererTime",
                                    1);
#endif
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       GetCanonicalUrl_SingleLink_WithDocumentFragment) {
  GURL::Replacements replace_fragment;
  replace_fragment.SetRefStr("fragment");
  GURL url_with_fragment =
      GetTestUrl("render_frame_host", "canonical_link.html")
          .ReplaceComponents(replace_fragment);
  EXPECT_TRUE(NavigateToURL(shell(), url_with_fragment));

  base::HistogramTester histogram_tester;
  std::optional<GURL> canon_url =
      GetCanonicalUrlFromFrame(web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(canon_url.has_value());
  // The canonical link should be returned appended with the fragment from the
  // loaded document/URL.
  EXPECT_EQ(GURL("https://example.com/canonical.html#fragment"),
            canon_url.value());
  content::FetchHistogramsFromChildProcesses();

#if BUILDFLAG(IS_ANDROID)
  ASSERT_TRUE(base::TimeTicks::IsHighResolution())
      << "The Blink.Frame.GetCanonicalUrlRendererTime histogram has "
         "microseconds precision and requires a high-resolution clock";
  histogram_tester.ExpectTotalCount("Blink.Frame.GetCanonicalUrlRendererTime",
                                    1);
#endif
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       GetCanonicalUrl_MultipleLinks_WithCanonicalFragment) {
  GURL::Replacements replace_fragment;
  replace_fragment.SetRefStr("fragment");
  GURL url_with_fragment =
      GetTestUrl("render_frame_host", "canonical_links_with_fragments.html")
          .ReplaceComponents(replace_fragment);
  EXPECT_TRUE(NavigateToURL(shell(), url_with_fragment));

  base::HistogramTester histogram_tester;
  std::optional<GURL> canon_url =
      GetCanonicalUrlFromFrame(web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(canon_url.has_value());
  // The first canonical link should be returned, and its fragment should
  // take precedence over the one from the loaded document/URL.
  EXPECT_EQ(GURL("https://example.com/canonical1.html#a1"), canon_url.value());
  content::FetchHistogramsFromChildProcesses();

#if BUILDFLAG(IS_ANDROID)
  ASSERT_TRUE(base::TimeTicks::IsHighResolution())
      << "The Blink.Frame.GetCanonicalUrlRendererTime histogram has "
         "microseconds precision and requires a high-resolution clock";
  histogram_tester.ExpectTotalCount("Blink.Frame.GetCanonicalUrlRendererTime",
                                    1);
#endif
}

// Regression test for https://crbug.com/852350
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       GetCanonicalUrlAfterRendererCrash) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GetTestUrl("render_frame_host", "beforeunload.html")));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  RenderFrameHostImpl* main_frame = web_contents()->GetPrimaryMainFrame();

  // Make the renderer crash.
  RenderProcessHost* renderer_process = main_frame->GetProcess();
  RenderProcessHostWatcher crash_observer(
      renderer_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  renderer_process->Shutdown(0);
  crash_observer.Wait();

  main_frame->GetCanonicalUrl(base::DoNothing());
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
  RenderFrameHostImpl* frame =
      static_cast<RenderFrameHostImpl*>(ChildFrameAt(root_frame_host(), 0));

  EXPECT_FALSE(frame->HasPendingCommitNavigation());

  // TODO(lukasza, clamy): https://crbug.com/784904: Verify that
  // WebContentsObserver::DidFinishNavigation was called with the same
  // NavigationHandle as WebContentsObserver::DidStartNavigation. This requires
  // properly matching the commit IPC to the NavigationHandle (ignoring that
  // their URLs do not match - matching instead using navigation id or mojo
  // interface identity).

  // TODO(crbug.com/40537082): Verify CSP frame-ancestors in the browser
  // process. Currently, this is done by the renderer process, which commits an
  // empty document with success instead.
  EXPECT_TRUE(navigation_observer.has_committed());
  EXPECT_TRUE(navigation_observer.is_error());
  EXPECT_EQ(blocked_url, frame->GetLastCommittedURL());
  EXPECT_EQ(net::ERR_BLOCKED_BY_RESPONSE, navigation_observer.net_error_code());
}

// Tests that when a same-document commit is aborted in the renderer (in this
// case, by the navigate event being cancelled), it does not leak its
// NavigationHandle.
IN_PROC_BROWSER_TEST_F(
    RenderFrameHostImplBrowserTest,
    AbortedSameDocumentNavigationsShouldNotLeakNavigationHandles) {
  GURL main_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), main_url));

  // Abort the next navigation via the navigate event.
  EXPECT_TRUE(ExecJs(root_frame_host(),
                     "navigation.onnavigate = e => e.preventDefault()"));
  GURL blocked_url(
      embedded_test_server()->GetURL("foo.com", "/title1.html#blocked"));
  NavigationHandleObserver navigation_observer(web_contents(), blocked_url);
  EXPECT_FALSE(NavigateToURL(shell(), blocked_url));
  EXPECT_EQ(main_url,
            web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL());
  EXPECT_TRUE(navigation_observer.is_same_document());

  // Verify that the NavigationHandle / NavigationRequest didn't leak.
  EXPECT_FALSE(root_frame_host()->HasPendingCommitNavigation());
}

// TODO(japhet): Remove this helper class and use RenderFrameHostImplBrowserTest
// when blink::features::kNavigateEventCommitBehavior is removed.
class RenderFrameHostImplCommitBehaviorBrowserTest
    : public RenderFrameHostImplBrowserTest {
 public:
  RenderFrameHostImplCommitBehaviorBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kNavigateEventCommitBehavior);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that when the navigation API intercepts and defers a commit, then
// a second navigation preempts the deferred commit, no NavigationHandles are
// leaked.
IN_PROC_BROWSER_TEST_F(
    RenderFrameHostImplCommitBehaviorBrowserTest,
    DeferredAndPreemptedSameDocumentNavigationsShouldNotLeakNavigationHandles) {
  GURL main_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), main_url));

  // Add a navigate event listener that will intercept the next navigation and
  // defer it for 100ms, then start a new navigation to preempt it.
  EXPECT_TRUE(ExecJs(root_frame_host(),
                     "navigation.addEventListener('navigate',"
                     "  e => { e.intercept({"
                     "    commit: 'after-transition',"
                     "    handler: () => new Promise(r => setTimeout(r, 100))"
                     "  }); "
                     "  setTimeout(() => navigation.navigate('#allowed'), 0);"
                     "}, { once: true });"));
  GURL blocked_url(
      embedded_test_server()->GetURL("foo.com", "/title1.html#blocked"));
  NavigationHandleObserver navigation_observer(web_contents(), blocked_url);
  EXPECT_FALSE(NavigateToURL(shell(), blocked_url));
  EXPECT_TRUE(navigation_observer.is_same_document());

  GURL allowed_url(
      embedded_test_server()->GetURL("foo.com", "/title1.html#allowed"));
  EXPECT_EQ(allowed_url,
            web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL());

  // Verify that the NavigationHandle / NavigationRequest didn't leak.
  EXPECT_FALSE(root_frame_host()->HasPendingCommitNavigation());
}

// Tests that when the navigation API intercepts and defers a commit, then
// the commit is aborted by script, no NavigationHandles are leaked.
IN_PROC_BROWSER_TEST_F(
    RenderFrameHostImplCommitBehaviorBrowserTest,
    DeferredAndRejectedSameDocumentNavigationsShouldntLeakNavigationHandles) {
  GURL main_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), main_url));

  // Add a navigate event listener that will intercept the next navigation and
  // reject it without committing it, causing the navigation to abort.
  EXPECT_TRUE(ExecJs(root_frame_host(),
                     "navigation.addEventListener('navigate',"
                     "  e => { e.intercept({"
                     "    commit: 'after-transition',"
                     "    handler: () => Promise.reject()"
                     "  }); "
                     "});"));
  GURL blocked_url(
      embedded_test_server()->GetURL("foo.com", "/title1.html#blocked"));
  NavigationHandleObserver navigation_observer(web_contents(), blocked_url);
  EXPECT_FALSE(NavigateToURL(shell(), blocked_url));
  EXPECT_TRUE(navigation_observer.is_same_document());

  // Verify that the NavigationHandle / NavigationRequest didn't leak.
  EXPECT_FALSE(root_frame_host()->HasPendingCommitNavigation());
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       BeforeUnloadDialogSuppressedForDiscard) {
  TestJavaScriptDialogManager dialog_manager;
  web_contents()->SetDelegate(&dialog_manager);

  EXPECT_TRUE(NavigateToURL(
      shell(), GetTestUrl("render_frame_host", "beforeunload.html")));
  // Disable the hang monitor, otherwise there will be a race between the
  // beforeunload dialog and the beforeunload hang timer.
  web_contents()
      ->GetPrimaryMainFrame()
      ->DisableBeforeUnloadHangMonitorForTesting();

  // Give the page a user gesture so javascript beforeunload works, and then
  // dispatch a before unload with discard as a reason. This should return
  // without any dialog being seen.
  web_contents()
      ->GetPrimaryMainFrame()
      ->ExecuteJavaScriptWithUserGestureForTests(
          std::u16string(), base::NullCallback(), ISOLATED_WORLD_ID_GLOBAL);
  web_contents()->GetPrimaryMainFrame()->DispatchBeforeUnload(
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
  web_contents()
      ->GetPrimaryMainFrame()
      ->DisableBeforeUnloadHangMonitorForTesting();

  // Give the page a user gesture so javascript beforeunload works, and then
  // dispatch a before unload with discard as a reason. This should return
  // without any dialog being seen.
  web_contents()
      ->GetPrimaryMainFrame()
      ->ExecuteJavaScriptWithUserGestureForTests(
          std::u16string(), base::NullCallback(), ISOLATED_WORLD_ID_GLOBAL);

  // Launch an alert javascript dialog. This pending dialog should block a
  // subsequent discarding before unload request.
  web_contents()->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
      u"setTimeout(function(){alert('hello');}, 10);", base::NullCallback(),
      ISOLATED_WORLD_ID_GLOBAL);
  dialog_manager.Wait();
  EXPECT_EQ(0, dialog_manager.num_beforeunload_dialogs_seen());
  EXPECT_EQ(0, dialog_manager.num_beforeunload_fired_seen());

  // Dispatch a before unload request while the first is still blocked
  // on the dialog, and expect it to return false immediately (synchronously).
  web_contents()->GetPrimaryMainFrame()->DispatchBeforeUnload(
      RenderFrameHostImpl::BeforeUnloadType::DISCARD, false);
  dialog_manager.Wait();
  EXPECT_EQ(0, dialog_manager.num_beforeunload_dialogs_seen());
  EXPECT_EQ(1, dialog_manager.num_beforeunload_fired_seen());
  EXPECT_FALSE(dialog_manager.proceed());

  // Clear the existing javascript dialog so that the associated IPC message
  // doesn't leak.
  dialog_manager.Run(true, std::u16string());

  web_contents()->SetDelegate(nullptr);
  web_contents()->SetJavaScriptDialogManagerForTesting(nullptr);
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       NotifiesProcessHostOfAudibleAudio) {
  const auto RunPostedTasks = []() {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  };

  // Note: Just using the beforeunload.html test document to spin-up a
  // renderer. Any document will do.
  EXPECT_TRUE(NavigateToURL(
      shell(), GetTestUrl("render_frame_host", "beforeunload.html")));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  auto* frame = web_contents()->GetPrimaryMainFrame();
  auto* process = static_cast<RenderProcessHostImpl*>(frame->GetProcess());
  ASSERT_EQ(0, process->get_media_stream_count_for_testing());

  // Audible audio output should cause the media stream count to increment.
  frame->OnMediaStreamAdded(RenderFrameHostImpl::GetAudibleMediaStreamType());
  RunPostedTasks();
  EXPECT_EQ(1, process->get_media_stream_count_for_testing());

  // Silence should cause the media stream count to decrement.
  frame->OnMediaStreamRemoved(RenderFrameHostImpl::GetAudibleMediaStreamType());
  RunPostedTasks();
  EXPECT_EQ(0, process->get_media_stream_count_for_testing());

  // Start audible audio output again, and then crash the renderer. Expect the
  // media stream count to be zero after the crash.
  frame->OnMediaStreamAdded(RenderFrameHostImpl::GetAudibleMediaStreamType());
  RunPostedTasks();
  EXPECT_EQ(1, process->get_media_stream_count_for_testing());
  RenderProcessHostWatcher crash_observer(
      process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  process->Shutdown(0);
  crash_observer.Wait();
  RunPostedTasks();
  EXPECT_EQ(0, process->get_media_stream_count_for_testing());
}

// Test that a frame is visible/hidden depending on its WebContents visibility
// state.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       DISABLED_VisibilityScrolledOutOfView) {
  GURL main_frame(embedded_test_server()->GetURL("/iframe_out_of_view.html"));
  GURL child_url(embedded_test_server()->GetURL("/hello.html"));

  // This will set up the page frame tree as A(A1()).
  ASSERT_TRUE(NavigateToURL(shell(), main_frame));
  // TODO(crbug.com/41453701): Re-enable this test
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* nested_iframe_node = root->child_at(0);
  EXPECT_TRUE(NavigateToURLFromRenderer(nested_iframe_node, child_url));

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
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* nested_iframe_node = root->child_at(0);
  EXPECT_TRUE(NavigateToURLFromRenderer(nested_iframe_node, child_url));

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
  RenderFrameHostImpl* subframe =
      static_cast<RenderFrameHostImpl*>(ChildFrameAt(root_frame_host(), 0));
  ASSERT_TRUE(subframe);
  EXPECT_EQ(main_origin, subframe->GetLastCommittedOrigin());
  EXPECT_EQ(blink::StorageKey::CreateFirstParty(main_origin),
            subframe->GetStorageKey());
}

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
  RenderFrameHost* subframe = ChildFrameAt(root_frame_host(), 0);
  ASSERT_TRUE(subframe);

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
  RenderFrameHostImpl* subframe =
      static_cast<RenderFrameHostImpl*>(subframe_observer.Wait());
  EXPECT_EQ(main_origin, subframe->GetLastCommittedOrigin());
  EXPECT_EQ(blink::StorageKey::CreateFirstParty(main_origin),
            subframe->GetStorageKey());

  // Wait for the about:blank navigation to finish.
  load_observer.Wait();

  // The subframe commit to about:blank should not contribute an extra history
  // entry.
  EXPECT_EQ(1, controller.GetEntryCount());

  // Browser-side origin should match the renderer-side origin.
  // See also https://crbug.com/932067.
  RenderFrameHostImpl* subframe2 =
      static_cast<RenderFrameHostImpl*>(ChildFrameAt(root_frame_host(), 0));
  ASSERT_TRUE(subframe2);
  EXPECT_EQ(subframe, subframe2);  // No swaps are expected.
  EXPECT_EQ(main_origin, subframe2->GetLastCommittedOrigin());
  EXPECT_EQ(blink::StorageKey::CreateFirstParty(main_origin),
            subframe2->GetStorageKey());
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
  EXPECT_EQ(main_origin,
            popup->GetPrimaryMainFrame()->GetLastCommittedOrigin());
  EXPECT_EQ(blink::StorageKey::CreateFirstParty(main_origin),
            static_cast<RenderFrameHostImpl*>(popup->GetPrimaryMainFrame())
                ->GetStorageKey());

  // The popup navigation should be cancelled and therefore shouldn't
  // contribute an extra history entry.
  EXPECT_EQ(1, popup->GetController().GetEntryCount());
  EXPECT_TRUE(popup->GetController().GetLastCommittedEntry()->IsInitialEntry());
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       OriginOfFreshFrame_Popup_AboutBlankAndThenDocWrite) {
  GURL main_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), main_url));
  url::Origin main_origin = url::Origin::Create(main_url);

  // Create a new about:blank popup and document.write into it.
  WebContentsAddedObserver popup_observer;
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

  // Wait for the new popup to be created (this will be before the popup finish
  // the synchronous about:blank commit in the browser).
  WebContents* popup = popup_observer.GetWebContents();
  content::TestNavigationObserver load_observer(popup);
  EXPECT_EQ(main_origin,
            popup->GetPrimaryMainFrame()->GetLastCommittedOrigin());
  EXPECT_EQ(blink::StorageKey::CreateFirstParty(main_origin),
            static_cast<RenderFrameHostImpl*>(popup->GetPrimaryMainFrame())
                ->GetStorageKey());
  load_observer.WaitForNavigationFinished();

  EXPECT_EQ(main_origin,
            popup->GetPrimaryMainFrame()->GetLastCommittedOrigin());
  EXPECT_EQ(blink::StorageKey::CreateFirstParty(main_origin),
            static_cast<RenderFrameHostImpl*>(popup->GetPrimaryMainFrame())
                ->GetStorageKey());

  // The popup navigation should be cancelled and therefore shouldn't
  // contribute an extra history entry.
  EXPECT_EQ(1, popup->GetController().GetEntryCount());
  EXPECT_TRUE(popup->GetController().GetLastCommittedEntry()->IsInitialEntry());
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       AccessibilityIsRootIframe) {
  GURL main_url(
      embedded_test_server()->GetURL("foo.com", "/page_with_iframe.html"));
  ASSERT_TRUE(NavigateToURL(shell(), main_url));

  RenderFrameHostImpl* main_frame = web_contents()->GetPrimaryMainFrame();
  EXPECT_TRUE(main_frame->AccessibilityIsRootFrame());

  ASSERT_EQ(1u, main_frame->child_count());
  RenderFrameHostImpl* iframe = main_frame->child_at(0)->current_frame_host();
  EXPECT_FALSE(iframe->AccessibilityIsRootFrame());
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       RequestSnapshotAXTreeAfterRenderProcessHostDeath) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));
  auto* rfh = web_contents()->GetPrimaryMainFrame();

  // Kill the renderer process.
  RenderProcessHostWatcher crash_observer(
      rfh->GetProcess(), RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  rfh->GetProcess()->Shutdown(0);
  crash_observer.Wait();

  // Call RequestAXSnapshotTree method. The browser process should not crash.
  auto params = mojom::SnapshotAccessibilityTreeParams::New();
  rfh->RequestAXTreeSnapshot(base::BindOnce([](ui::AXTreeUpdate& snapshot) {
                               NOTREACHED_IN_MIGRATION();
                             }),
                             std::move(params));

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

  auto* rfh = web_contents()->GetPrimaryMainFrame();
  TestNavigationObserver observer(web_contents());
  EXPECT_TRUE(ExecJs(rfh, JsReplace("setUrl($1);", object_url),
                     EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
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
  std::ignore = WaitForRenderFrameReady(web_contents()->GetPrimaryMainFrame());

  EXPECT_TRUE(crash_observer.did_exit_normally());
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       SchedulerTrackedFeatures) {
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  RenderFrameHostImpl* main_frame = web_contents()->GetPrimaryMainFrame();
  // Simulate getting WebSocket in a feature vector from the renderer.
  main_frame->DidChangeBackForwardCacheDisablingFeatures(
      CreateBlockingDetails({BlocklistedFeature::kWebSocket}));
  ASSERT_EQ(main_frame->GetBackForwardCacheDisablingFeatures(),
            BlocklistedFeatures({BlocklistedFeature::kWebSocket}));

  // Simulate the browser side reporting WebRTC usage.
  main_frame->OnBackForwardCacheDisablingStickyFeatureUsed(
      static_cast<BlocklistedFeature>(BlocklistedFeature::kWebRTC));
  ASSERT_EQ(main_frame->GetBackForwardCacheDisablingFeatures(),
            BlocklistedFeatures(
                {BlocklistedFeature::kWebSocket, BlocklistedFeature::kWebRTC}));

  // Simulate a feature vector being updated from the renderer with some
  // features being activated and some being deactivated.
  // [kWebSocket(0), kWebRTC(1)] -> [kWebRTC(1),
  // kMainResourceHasCacheControlNoCache(2)]
  main_frame->DidChangeBackForwardCacheDisablingFeatures(CreateBlockingDetails(
      {BlocklistedFeature::kWebRTC,
       BlocklistedFeature::kMainResourceHasCacheControlNoCache}));
  ASSERT_EQ(main_frame->GetBackForwardCacheDisablingFeatures(),
            BlocklistedFeatures(
                {BlocklistedFeature::kWebRTC,
                 BlocklistedFeature::kMainResourceHasCacheControlNoCache}));

  // Navigate away and expect that no values persist the navigation.
  // Note that we are still simulating the renderer call, otherwise features
  // like "document loaded" will show up here.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));
  main_frame = web_contents()->GetPrimaryMainFrame();
  BackForwardCacheBlockingDetails empty_vector;
  main_frame->DidChangeBackForwardCacheDisablingFeatures(
      CreateBlockingDetails({}));
}

class RenderFrameHostImplSchemefulEnabledBrowserTest
    : public RenderFrameHostImplBrowserTest {
 public:
  RenderFrameHostImplSchemefulEnabledBrowserTest() {
    scope_feature_list_.InitAndEnableFeature(net::features::kSchemefulSameSite);
  }

 protected:
  base::test::ScopedFeatureList scope_feature_list_;
};

class RenderFrameHostImplNoStrictSiteIsolationOnAndroidBrowserTest
    : public RenderFrameHostImplBrowserTest {
 public:
  RenderFrameHostImplNoStrictSiteIsolationOnAndroidBrowserTest() = default;
  ~RenderFrameHostImplNoStrictSiteIsolationOnAndroidBrowserTest() override =
      default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    RenderFrameHostImplBrowserTest::SetUpCommandLine(command_line);

#if BUILDFLAG(IS_ANDROID)
    // On Android, --site-per-process may be passed on some bots to force strict
    // site isolation.  That causes this test too create a lot of processes and
    // time out due to running too slowly, so force this test to run without
    // strict site isolation on Android.  This is ok since this test doesn't
    // actually care about process isolation.
    command_line->RemoveSwitch(switches::kSitePerProcess);
#endif
  }
};

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       ComputeIsolationInfoForNavigationSiteForCookies) {
  // Start second server for HTTPS.
  https_server()->ServeFilesFromSourceDirectory(GetTestDataFilePath());
  ASSERT_TRUE(https_server()->Start());

  GURL url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a(b(d)),c())");

  FirstPartySchemeContentBrowserClient new_client(url);

  GURL b_url = embedded_test_server()->GetURL("b.com", "/");
  GURL c_url = embedded_test_server()->GetURL("c.com", "/");
  GURL secure_url = https_server()->GetURL("/");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  {
    RenderFrameHostImpl* main_frame = web_contents()->GetPrimaryMainFrame();

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
    RenderFrameHostImpl* main_frame = web_contents()->GetPrimaryMainFrame();
    EXPECT_EQ(trusty_url.DeprecatedGetOriginAsURL(),
              main_frame->GetLastCommittedURL().DeprecatedGetOriginAsURL());

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
    RenderFrameHostImpl* main_frame = web_contents()->GetPrimaryMainFrame();
    EXPECT_EQ(trusty_if_secure_url.DeprecatedGetOriginAsURL(),
              main_frame->GetLastCommittedURL().DeprecatedGetOriginAsURL());

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
}

// Verifies that when a document navigates a cross-origin grandchild frame to
// a new origin, the corresponding FrameNavigationEntry initiator origin equals
// the document origin.
IN_PROC_BROWSER_TEST_F(
    RenderFrameHostImplBrowserTest,
    InitiatorOriginForCrossDocumentNavigationInCrossOriginGrandchild) {
  GURL a_url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c))");
  EXPECT_TRUE(NavigateToURL(shell(), a_url));

  RenderFrameHostImpl* rfh_a = web_contents()->GetPrimaryMainFrame();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();
  RenderFrameHostImpl* rfh_c = rfh_b->child_at(0)->current_frame_host();

  // The initiator origin should begin as b.com since it created the iframe.
  EXPECT_EQ(embedded_test_server()->GetOrigin("b.com"),
            GetInitiatorOrigin(rfh_c));

  // Navigate to d.com/title1.html from the outer document.
  GURL d_url = embedded_test_server()->GetURL("d.com", "/title1.html");
  NavigateToURLFromGrandparentDocument(shell(), d_url);

  RenderFrameHostImpl* rfh_d = rfh_b->child_at(0)->current_frame_host();

  // A navigation initiated by the outer document should update the initiator to
  // the origin of that document.
  EXPECT_EQ(url::Origin::Create(a_url), GetInitiatorOrigin(rfh_d));
}

class RenderFrameHostImplSiteIsolationBrowserTest
    : public RenderFrameHostImplBrowserTest,
      public testing::WithParamInterface<bool> {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (disable_site_isolation_) {
      command_line->AppendSwitch(switches::kDisableSiteIsolation);
    } else {
      command_line->AppendSwitch(switches::kSitePerProcess);
    }
    RenderFrameHostImplBrowserTest::SetUpCommandLine(command_line);
  }

 private:
  bool disable_site_isolation_ = GetParam();
};

INSTANTIATE_TEST_SUITE_P(All,
                         RenderFrameHostImplSiteIsolationBrowserTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "SiteIsolationDisabled"
                                             : "SiteIsolationEnabled";
                         });

// Verifies that when a document navigates a cross-origin grandchild frame to
// a fragment URL (i.e. a same-document navigation), the corresponding
// FrameNavigationEntry initiator origin equals the document origin.
IN_PROC_BROWSER_TEST_P(
    RenderFrameHostImplSiteIsolationBrowserTest,
    InitiatorOriginForSameDocumentNavigationInCrossOriginGrandchild) {
  GURL a_url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c))");

  EXPECT_TRUE(NavigateToURL(shell(), a_url));

  RenderFrameHostImpl* rfh_a = web_contents()->GetPrimaryMainFrame();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();
  RenderFrameHostImpl* rfh_c = rfh_b->child_at(0)->current_frame_host();

  // The initiator origin should begin as b.com since it created the iframe.
  EXPECT_EQ(embedded_test_server()->GetOrigin("b.com"),
            GetInitiatorOrigin(rfh_c));

  // Do a same-document navigation from the outer document.
  GURL c_url_with_fragment =
      GURL(base::StrCat({rfh_c->GetLastCommittedURL().spec(), "#x"}));
  NavigateToURLFromGrandparentDocument(shell(), c_url_with_fragment);

  // A navigation initiated by the outer document should update the initiator to
  // the origin of that document.
  if (AreAllSitesIsolatedForTesting()) {
    EXPECT_EQ(url::Origin::Create(a_url), GetInitiatorOrigin(rfh_c));
  } else {
    // TODO(crbug.com/367440964): The origin of the same-document navigation is
    // currently used in place of the actual initiator_origin:
    // https://source.chromium.org/chromium/chromium/src/+/main:content/browser/renderer_host/navigation_request.cc;l=1495-1497;drc=e66b343b5554785a32ad988bfe5c3c524f5e1857
    EXPECT_EQ(url::Origin::Create(c_url_with_fragment),
              GetInitiatorOrigin(rfh_c));
  }
}

// Verifies that when a document navigates a same-origin grandchild frame to
// a new origin, the corresponding FrameNavigationEntry initiator origin equals
// the document origin.
IN_PROC_BROWSER_TEST_F(
    RenderFrameHostImplBrowserTest,
    InitiatorOriginForCrossDocumentNavigationInSameOriginGrandchild) {
  GURL a_url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(a))");

  EXPECT_TRUE(NavigateToURL(shell(), a_url));

  RenderFrameHostImpl* rfh_a = web_contents()->GetPrimaryMainFrame();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();
  RenderFrameHostImpl* rfh_a2 = rfh_b->child_at(0)->current_frame_host();

  // The initiator origin should begin as b.com since it created the iframe.
  EXPECT_EQ(embedded_test_server()->GetOrigin("b.com"),
            GetInitiatorOrigin(rfh_a2));

  // Navigate to d.com/title1.html from the outer document.
  GURL d_url = embedded_test_server()->GetURL("d.com", "/title1.html");
  NavigateToURLFromGrandparentDocument(shell(), d_url);

  RenderFrameHostImpl* rfh_d = rfh_b->child_at(0)->current_frame_host();

  // A navigation initiated by the outer document should update the initiator to
  // the origin of that document.
  EXPECT_EQ(embedded_test_server()->GetOrigin("a.com"),
            GetInitiatorOrigin(rfh_d));
}

// Verifies that when a document navigates a same-origin grandchild frame to a
// fragment URL (i.e. a same-document navigation), the corresponding
// FrameNavigationEntry initiator origin equals the document origin.
IN_PROC_BROWSER_TEST_F(
    RenderFrameHostImplBrowserTest,
    InitiatorOriginForSameDocumentNavigationInSameOriginGrandchild) {
  GURL a_url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(a))");

  EXPECT_TRUE(NavigateToURL(shell(), a_url));

  RenderFrameHostImpl* rfh_a = web_contents()->GetPrimaryMainFrame();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();
  RenderFrameHostImpl* rfh_a2 = rfh_b->child_at(0)->current_frame_host();

  // The initiator origin should begin as b.com since it created the iframe.
  EXPECT_EQ(embedded_test_server()->GetOrigin("b.com"),
            GetInitiatorOrigin(rfh_a2));

  // Do a same-document navigation from the outer document.
  GURL a_url_with_fragment =
      GURL(base::StrCat({rfh_a2->GetLastCommittedURL().spec(), "#x"}));
  NavigateToURLFromGrandparentDocument(shell(), a_url_with_fragment);

  // A navigation initiated by the outer document should update the initiator to
  // the origin of that document.
  EXPECT_EQ(embedded_test_server()->GetOrigin("a.com"),
            GetInitiatorOrigin(rfh_a2));
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
    RenderFrameHostImpl* main_frame = web_contents()->GetPrimaryMainFrame();

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
    EXPECT_TRUE(NavigateToURLFromRenderer(child, url));
    EXPECT_EQ("a.test", child->current_url().host());
    EXPECT_EQ("https", child->current_origin().scheme());
    EXPECT_EQ(1u, child->child_count());

    grandchild = child->child_at(0);

    // Now the frames above grandchild differ only in scheme. This results in
    // null SiteForCookies because of the schemefully_same flag, but site should
    // still not be opaque.
    net::SiteForCookies grandchild_cross_scheme =
        grandchild->current_frame_host()->ComputeSiteForCookies();
    EXPECT_TRUE(grandchild_cross_scheme.IsNull());
    EXPECT_FALSE(grandchild_cross_scheme.site().opaque());

    net::SiteForCookies grandchild_cross_scheme_navigation =
        grandchild->current_frame_host()
            ->ComputeIsolationInfoForNavigation(other_url)
            .site_for_cookies();
    EXPECT_TRUE(grandchild_cross_scheme_navigation.IsNull());
    EXPECT_FALSE(grandchild_cross_scheme_navigation.site().opaque());
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

    RenderFrameHostImpl* main_frame = web_contents()->GetPrimaryMainFrame();

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

    RenderFrameHostImpl* main_frame = web_contents()->GetPrimaryMainFrame();
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

  RenderFrameHostImpl* main_frame = web_contents()->GetPrimaryMainFrame();

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

  RenderFrameHostImpl* main_frame = web_contents()->GetPrimaryMainFrame();
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

  RenderFrameHostImpl* main_frame = web_contents()->GetPrimaryMainFrame();
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

  RenderFrameHostImpl* main_frame = web_contents()->GetPrimaryMainFrame();

  EXPECT_EQ("a.com", main_frame->GetLastCommittedURL().host());

  ASSERT_EQ(1u, main_frame->child_count());
  FrameTreeNode* child_a = main_frame->child_at(0);
  RenderFrameHostImpl* child_rfh = child_a->current_frame_host();
  EXPECT_EQ("a.com", child_rfh->GetLastCommittedOrigin().host());
  GURL kid_url = child_rfh->GetLastCommittedURL();

  // Disable the unload ACK and the unload timer. Also pretend the child frame
  // has an unload handler, so it doesn't get cleaned up synchronously, and
  // block its detach handler.
  auto unload_ack_filter = base::BindRepeating([] { return true; });
  main_frame->SetUnloadACKCallbackForTesting(unload_ack_filter);
  main_frame->DisableUnloadTimerForTesting();
  child_rfh->SuddenTerminationDisablerChanged(
      true, blink::mojom::SuddenTerminationDisablerType::kUnloadHandler);
  child_rfh->SetSubframeUnloadTimeoutForTesting(base::Days(7));
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
  EXPECT_FALSE(main_frame->IsActive());
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
  RenderProcessHost* process =
      web_contents()->GetPrimaryMainFrame()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  process->Shutdown(0);
  crash_observer.Wait();

  // 3. Reload.
  web_contents()->GetController().Reload(ReloadType::NORMAL, false);
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  // Check the document is correctly reloaded.
  RenderFrameHostImpl* main_document = web_contents()->GetPrimaryMainFrame();
  ASSERT_EQ(1u, main_document->child_count());
  RenderFrameHostImpl* sub_document =
      main_document->child_at(0)->current_frame_host();
  EXPECT_EQ(main_frame_url, main_document->GetLastCommittedURL());
  EXPECT_EQ(subframe_url, sub_document->GetLastCommittedURL());
  EXPECT_THAT(
      EvalJs(main_document, "document.body.textContent").ExtractString(),
      ::testing::HasSubstr("This page has an iframe. Yay for iframes!"));
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
  RenderProcessHost* process =
      web_contents()->GetPrimaryMainFrame()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  process->Shutdown(0);
  crash_observer.Wait();

  // 3. Reload.
  web_contents()->GetController().Reload(ReloadType::NORMAL, false);
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  // Check the document is correctly reloaded.
  RenderFrameHostImpl* main_document = web_contents()->GetPrimaryMainFrame();
  EXPECT_EQ(main_frame_url, main_document->GetLastCommittedURL());
  // Execute script in an isolated world to avoid causing a Trusted Types
  // violation due to eval.
  EXPECT_THAT(EvalJs(main_document,
                     "document.querySelector('info-view').shadowRoot"
                     ".querySelector('#used-only-by-test').text",
                     EXECUTE_SCRIPT_DEFAULT_OPTIONS, /*world_id=*/1)
                  .ExtractString(),
              testing::StartsWith("GPU Info"));
}

// Start with A(B), navigate A to C. By emulating a slow unload handler B, check
// the status of IsActive for subframes of A i.e., B before and after
// navigating to C.
// Test is flaky: https://crbug.com/1114149.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       DISABLED_CheckIsActiveBeforeAndAfterUnload) {
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  GURL url_ab(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));

  // 1) Navigate to a page with an iframe.
  EXPECT_TRUE(NavigateToURL(shell(), url_ab));
  RenderFrameHostImpl* rfh_a = web_contents()->GetPrimaryMainFrame();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();
  RenderFrameDeletedObserver delete_rfh_b(rfh_b);
  EXPECT_EQ(LifecycleStateImpl::kActive, rfh_b->lifecycle_state());

  // 2) Leave rfh_b in pending deletion state.
  LeaveInPendingDeletionState(rfh_b);

  // 3) Check the IsActive state of rfh_a, rfh_b before navigating to C.
  EXPECT_TRUE(rfh_a->IsActive());
  EXPECT_TRUE(rfh_b->IsActive());

  // 4) Navigate rfh_a to C.
  EXPECT_TRUE(NavigateToURL(shell(), url_c));
  RenderFrameHostImpl* rfh_c = web_contents()->GetPrimaryMainFrame();

  EXPECT_THAT(
      rfh_a->lifecycle_state(),
      testing::AnyOf(testing::Eq(LifecycleStateImpl::kReadyToBeDeleted),
                     testing::Eq(LifecycleStateImpl::kInBackForwardCache)));
  EXPECT_THAT(
      rfh_b->lifecycle_state(),
      testing::AnyOf(testing::Eq(LifecycleStateImpl::kRunningUnloadHandlers),
                     testing::Eq(LifecycleStateImpl::kInBackForwardCache)));

  // 5) Check the IsActive state of rfh_a, rfh_b and rfh_c after navigating to
  // C.
  EXPECT_FALSE(rfh_a->IsActive());
  EXPECT_FALSE(rfh_b->IsActive());
  EXPECT_TRUE(rfh_c->IsActive());
}

// Test the LifecycleStateImpl is updated correctly for the main frame during
// navigation.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       CheckLifecycleStateTransitionOnMainFrame) {
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = root_frame_host();
  EXPECT_EQ(LifecycleStateImpl::kActive, rfh_a->lifecycle_state());

  // 2) Leave rfh_a in pending deletion state to check for rfh_a
  // LifecycleStateImpl after navigating to B.
  LeaveInPendingDeletionState(rfh_a);

  // 3) Start navigation to B, but don't commit yet.
  TestNavigationManager manager(web_contents(), url_b);
  shell()->LoadURL(url_b);
  manager.WaitForSpeculativeRenderFrameHostCreation();

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  RenderFrameHostImpl* pending_rfh =
      root->render_manager()->speculative_frame_host();
  NavigationRequest* navigation_request = root->navigation_request();
  EXPECT_EQ(navigation_request->GetAssociatedRFHType(),
            NavigationRequest::AssociatedRenderFrameHostType::SPECULATIVE);
  EXPECT_TRUE(pending_rfh);

  // 4) Check the LifecycleStateImpl of both rfh_a and pending_rfh before
  // commit.
  EXPECT_EQ(LifecycleStateImpl::kSpeculative, pending_rfh->lifecycle_state());
  EXPECT_EQ(LifecycleStateImpl::kActive, rfh_a->lifecycle_state());
  EXPECT_EQ(root_frame_host(), rfh_a);
  EXPECT_TRUE(rfh_a->IsInPrimaryMainFrame());
  EXPECT_FALSE(pending_rfh->IsInPrimaryMainFrame());

  // 5) Let the navigation finish and make sure it is succeeded.
  ASSERT_TRUE(manager.WaitForNavigationFinished());
  EXPECT_EQ(url_b,
            web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL());
  RenderFrameHostImpl* rfh_b = root_frame_host();

  // 6) Check the LifecycleStateImpl of both rfh_a and rfh_b after navigating to
  // B.
  EXPECT_THAT(
      rfh_a->lifecycle_state(),
      testing::AnyOf(testing::Eq(LifecycleStateImpl::kRunningUnloadHandlers),
                     testing::Eq(LifecycleStateImpl::kInBackForwardCache)));
  EXPECT_FALSE(rfh_a->GetPage().IsPrimary());
  EXPECT_FALSE(rfh_a->IsInPrimaryMainFrame());
  EXPECT_EQ(LifecycleStateImpl::kActive, rfh_b->lifecycle_state());
  EXPECT_TRUE(rfh_b->GetPage().IsPrimary());
  EXPECT_TRUE(rfh_b->IsInPrimaryMainFrame());
}

// Test the LifecycleStateImpl is updated correctly for a subframe.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       CheckRFHLifecycleStateTransitionOnSubFrame) {
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  GURL url_ab(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));

  // Lifecycle state of initial (Blank page) RenderFrameHost should be active as
  // we don't update the LifecycleStateImpl prior to navigation commits (to new
  // URL i.e., url_ab in this case).
  EXPECT_EQ(LifecycleStateImpl::kActive, root_frame_host()->lifecycle_state());

  // 1) Navigate to a page with an iframe.
  EXPECT_TRUE(NavigateToURL(shell(), url_ab));
  RenderFrameHostImpl* rfh_a = web_contents()->GetPrimaryMainFrame();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();
  EXPECT_EQ(LifecycleStateImpl::kActive, rfh_b->lifecycle_state());
  // `rfh_b` is in the primary page, but since it's a subframe, it's not the
  // primary main frame.
  EXPECT_TRUE(rfh_b->GetPage().IsPrimary());
  EXPECT_FALSE(rfh_b->IsInPrimaryMainFrame());

  // 2) Navigate B's subframe to a cross-site C.
  EXPECT_TRUE(NavigateToURLFromRenderer(rfh_b->frame_tree_node(), url_c));

  // 3) Check LifecycleStateImpl of sub-frame rfh_c after navigating from
  // subframe rfh_b.
  RenderFrameHostImpl* rfh_c = rfh_a->child_at(0)->current_frame_host();
  EXPECT_EQ(LifecycleStateImpl::kActive, rfh_c->lifecycle_state());

  // 4) Add a new child frame.
  RenderFrameHostCreatedObserver subframe_observer(web_contents());
  EXPECT_TRUE(ExecJs(rfh_c,
                     "let iframe = document.createElement('iframe');"
                     "document.body.appendChild(iframe);"));
  subframe_observer.Wait();

  // 5) LifecycleStateImpl of newly inserted child frame should be kActive
  // before navigation.
  RenderFrameHostImpl* rfh_d = rfh_c->child_at(0)->current_frame_host();
  EXPECT_EQ(LifecycleStateImpl::kActive, rfh_d->lifecycle_state());
}

// Test that LifecycleStateImpl is updated correctly during
// cross-RenderFrameHost navigation.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       CheckLifecycleStateTransitionWithPendingCommit) {
  class CheckLifecycleStateImpl : public WebContentsObserver {
   public:
    explicit CheckLifecycleStateImpl(WebContents* web_contents)
        : WebContentsObserver(web_contents) {}

    // WebContentsObserver overrides:
    void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override {
      RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(
          navigation_handle->GetRenderFrameHost());
      EXPECT_EQ(rfh->lifecycle_state(), LifecycleStateImpl::kPendingCommit);
      EXPECT_EQ(rfh->GetLifecycleState(),
                RenderFrameHost::LifecycleState::kPendingCommit);
      EXPECT_FALSE(rfh->GetPage().IsPrimary());
      EXPECT_FALSE(rfh->IsInPrimaryMainFrame());
    }
  };

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = root_frame_host();
  EXPECT_EQ(LifecycleStateImpl::kActive, rfh_a->lifecycle_state());

  // 2) Start navigation to B, but don't commit yet.
  TestNavigationManager manager(web_contents(), url_b);
  shell()->LoadURL(url_b);
  manager.WaitForSpeculativeRenderFrameHostCreation();

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  RenderFrameHostImpl* speculative_rfh =
      root->render_manager()->speculative_frame_host();
  NavigationRequest* navigation_request = root->navigation_request();
  EXPECT_EQ(navigation_request->GetAssociatedRFHType(),
            NavigationRequest::AssociatedRenderFrameHostType::SPECULATIVE);
  EXPECT_TRUE(speculative_rfh);

  // 3) Check the LifecycleStateImpl of both rfh_a and speculative_rfh before
  // commit.
  EXPECT_EQ(LifecycleStateImpl::kSpeculative,
            speculative_rfh->lifecycle_state());
  EXPECT_EQ(LifecycleStateImpl::kActive, rfh_a->lifecycle_state());
  EXPECT_EQ(root_frame_host(), rfh_a);
  EXPECT_TRUE(rfh_a->IsInPrimaryMainFrame());
  EXPECT_FALSE(speculative_rfh->IsInPrimaryMainFrame());

  // 4) Check that LifecycleStateImpl of speculative_rfh transitions to
  // kPendingCommit in ReadyToCommitNavigation.
  CheckLifecycleStateImpl check_pending_commit(web_contents());

  // 5) Let the navigation finish and make sure it is succeeded.
  ASSERT_TRUE(manager.WaitForNavigationFinished());
  EXPECT_EQ(url_b,
            web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL());
  RenderFrameHostImpl* rfh_b = root_frame_host();
  EXPECT_EQ(rfh_b, speculative_rfh);
  EXPECT_EQ(LifecycleStateImpl::kActive, rfh_b->lifecycle_state());
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

// Ensure that calling document.open in an error page does not cause a renderer
// kill when it inherits the unreachable error URL.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       DocumentOpenInErrorPage) {
  GURL error_url(embedded_test_server()->GetURL("error.com", "/empty.html"));
  std::unique_ptr<URLLoaderInterceptor> url_interceptor =
      URLLoaderInterceptor::SetupRequestFailForURL(error_url,
                                                   net::ERR_DNS_TIMED_OUT);
  EXPECT_FALSE(NavigateToURL(shell(), error_url));

  // Calling document.open should not cause CanCommitURL to fail, even though
  // the error page URL is inherited.
  // See https://crbug.com/326250356#comment36.
  EXPECT_TRUE(ExecJs(shell(), "document.open();"));
  EXPECT_EQ(GURL(kUnreachableWebDataURL),
            root_frame_host()->last_document_url_in_renderer());

  // Ensure the renderer process has not crashed.
  ASSERT_TRUE(ExecJs(shell(), "true"));
  ASSERT_TRUE(root_frame_host()->IsRenderFrameLive());
}

// Similar to DocumentOpenInErrorPage, but without error page isolation in the
// main frame, which changes ProcessLock expectations and can thus fail in
// additional ways.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       DocumentOpenInErrorPageWithoutErrorPageIsolation) {
  // Disable error page isolation in main frames, similar to Android WebView.
  class NoErrorPageIsolationContentBrowserClient
      : public ContentBrowserTestContentBrowserClient {
   public:
    bool ShouldIsolateErrorPage(bool in_main_frame) override { return false; }
  } no_error_isolation_client;

  GURL error_url(embedded_test_server()->GetURL("error.com", "/empty.html"));
  std::unique_ptr<URLLoaderInterceptor> url_interceptor =
      URLLoaderInterceptor::SetupRequestFailForURL(error_url,
                                                   net::ERR_DNS_TIMED_OUT);
  EXPECT_FALSE(NavigateToURL(shell(), error_url));

  // Calling document.open should not cause CanCommitURL to fail, even though
  // the error page URL is inherited.
  // See https://crbug.com/326250356#comment36.
  EXPECT_TRUE(ExecJs(shell(), "document.open();"));
  EXPECT_EQ(GURL(kUnreachableWebDataURL),
            root_frame_host()->last_document_url_in_renderer());

  // Ensure the renderer process has not crashed.
  ASSERT_TRUE(ExecJs(shell(), "true"));
  ASSERT_TRUE(root_frame_host()->IsRenderFrameLive());
}

// Similar to DocumentOpenInErrorPage, but when loading the error page in a
// subframe, which lacks error page isolation.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       DocumentOpenInErrorPageSubframe) {
  GURL main_frame_url(
      embedded_test_server()->GetURL("/frame_tree/page_with_one_frame.html"));
  ASSERT_TRUE(NavigateToURL(shell(), main_frame_url));

  GURL error_url(embedded_test_server()->GetURL("error.com", "/empty.html"));
  std::unique_ptr<URLLoaderInterceptor> url_interceptor =
      URLLoaderInterceptor::SetupRequestFailForURL(error_url,
                                                   net::ERR_DNS_TIMED_OUT);
  EXPECT_TRUE(NavigateIframeToURL(web_contents(), "child0", error_url));
  RenderFrameHostImpl* subframe =
      static_cast<RenderFrameHostImpl*>(ChildFrameAt(root_frame_host(), 0));

  // Calling document.open should not cause CanCommitURL to fail, even though
  // the error page URL is inherited.
  // See https://crbug.com/326250356#comment36.
  EXPECT_TRUE(ExecJs(subframe, "document.open();"));
  EXPECT_EQ(GURL(kUnreachableWebDataURL),
            subframe->last_document_url_in_renderer());

  // Ensure the renderer process has not crashed.
  ASSERT_TRUE(ExecJs(subframe, "true"));
  ASSERT_TRUE(subframe->IsRenderFrameLive());
}

// Test the LifecycleStateImpl when a renderer crashes during navigation.
// When navigating after a crash, the new RenderFrameHost should
// become active immediately, prior to the navigation committing. This is
// an optimization to prevent the user from sitting around on the sad tab
// unnecessarily.
// TODO(crbug.com/40052076): This behavior might be revisited in the
// future.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       CheckRFHLifecycleStateWhenRendererCrashes) {
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = root_frame_host();
  EXPECT_EQ(LifecycleStateImpl::kActive, rfh_a->lifecycle_state());

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

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  RenderFrameHostImpl* current_rfh =
      root->render_manager()->current_frame_host();
  NavigationRequest* navigation_request = root->navigation_request();
  if (ShouldSkipEarlyCommitPendingForCrashedFrame()) {
    EXPECT_EQ(navigation_request->GetAssociatedRFHType(),
              NavigationRequest::AssociatedRenderFrameHostType::SPECULATIVE);
  } else {
    EXPECT_EQ(navigation_request->GetAssociatedRFHType(),
              NavigationRequest::AssociatedRenderFrameHostType::CURRENT);
  }

  // 4) Check the LifecycleStateImpl of B's RFH.
  EXPECT_EQ(LifecycleStateImpl::kActive, current_rfh->lifecycle_state());

  // 5) Let the navigation finish and make sure it is succeeded.
  ASSERT_TRUE(manager.WaitForNavigationFinished());
  EXPECT_EQ(url_b,
            web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL());
  // The RenderFrameHost has been replaced after the crash, so get it again.
  current_rfh = root->render_manager()->current_frame_host();
  EXPECT_EQ(LifecycleStateImpl::kActive, current_rfh->lifecycle_state());
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

  EXPECT_CALL(*mock_provider_ptr, Retrieve(testing::_, testing::_))
      .WillOnce(testing::Invoke([&]() {
        mock_provider_ptr->NotifyReceive(
            std::vector<url::Origin>{url::Origin::Create(first_url)}, "hello",
            SmsFetcher::UserConsent::kObtained);
      }));

  // EvalJs waits for the promise being resolved. This ensures that the browser
  // has time to see the otp usage, and records it, before we test for it below.
  EXPECT_EQ("hello", EvalJs(shell(), script));

  EXPECT_TRUE(web_contents()->GetPrimaryMainFrame()->DocumentUsedWebOTP());

  // Loads a URL that maps to the same SiteInstance as the first URL, to make
  // sure the navigation will not be cross-process.
  const GURL second_url(embedded_test_server()->GetURL("/title2.html"));
  ASSERT_TRUE(NavigateToURL(shell(), second_url));
  EXPECT_FALSE(web_contents()->GetPrimaryMainFrame()->DocumentUsedWebOTP());
}

namespace {

// Calls |callback| whenever a DOMContentLoaded is reached in
// |render_frame_host|.
class DOMContentLoadedObserver : public WebContentsObserver {
 public:
  DOMContentLoadedObserver(WebContents* web_contents,
                           base::RepeatingClosure callback)
      : WebContentsObserver(web_contents), callback_(callback) {}

  DOMContentLoadedObserver(const DOMContentLoadedObserver&) = delete;
  DOMContentLoadedObserver& operator=(const DOMContentLoadedObserver&) = delete;

 protected:
  // WebContentsObserver:
  void DOMContentLoaded(RenderFrameHost* render_Frame_host) override {
    callback_.Run();
  }

 private:
  base::RepeatingClosure callback_;
};

// Calls |callback| whenever a DocumentOnLoad is reached in
// |render_frame_host|.
class DocumentOnLoadObserver : public WebContentsObserver {
 public:
  DocumentOnLoadObserver(WebContents* web_contents,
                         base::RepeatingClosure callback)
      : WebContentsObserver(web_contents), callback_(callback) {}

  DocumentOnLoadObserver(const DocumentOnLoadObserver&) = delete;
  DocumentOnLoadObserver& operator=(const DocumentOnLoadObserver&) = delete;

 protected:
  // WebContentsObserver:
  void DocumentOnLoadCompletedInPrimaryMainFrame() override { callback_.Run(); }

 private:
  base::RepeatingClosure callback_;
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
      static_cast<RenderFrameHostImpl*>(web_contents->GetPrimaryMainFrame());
  TestNavigationObserver load_observer(web_contents);
  base::RunLoop loop_until_dcl;
  DOMContentLoadedObserver dcl_observer(web_contents,
                                        loop_until_dcl.QuitClosure());
  shell()->LoadURL(main_document_url);

  EXPECT_FALSE(rfhi->IsDOMContentLoaded());
  EXPECT_FALSE(web_contents->IsDocumentOnLoadCompletedInPrimaryMainFrame());

  main_document_response.WaitForRequest();
  main_document_response.Send(
      "HTTP/1.1 200 OK\r\n"
      "Connection: close\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n"
      "<img src='/img'>");

  load_observer.WaitForNavigationFinished();
  EXPECT_FALSE(rfhi->IsDOMContentLoaded());
  EXPECT_FALSE(web_contents->IsDocumentOnLoadCompletedInPrimaryMainFrame());

  main_document_response.Done();

  // We should reach DOMContentLoaded, but not onload, since the image resource
  // is still loading.
  loop_until_dcl.Run();
  EXPECT_TRUE(rfhi->is_loading());
  EXPECT_TRUE(rfhi->IsDOMContentLoaded());
  EXPECT_FALSE(web_contents->IsDocumentOnLoadCompletedInPrimaryMainFrame());

  base::RunLoop loop_until_onload;
  DocumentOnLoadObserver onload_observer(web_contents,
                                         loop_until_onload.QuitClosure());

  image_response.WaitForRequest();
  image_response.Done();

  // And now onload() should be reached.
  loop_until_onload.Run();
  EXPECT_TRUE(rfhi->IsDOMContentLoaded());
  EXPECT_TRUE(web_contents->IsDocumentOnLoadCompletedInPrimaryMainFrame());
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

  EXPECT_TRUE(
      static_cast<RenderFrameHostImpl*>(web_contents->GetPrimaryMainFrame())
          ->IsDOMContentLoaded());
  EXPECT_TRUE(web_contents->IsDocumentOnLoadCompletedInPrimaryMainFrame());

  // Expect that the loading state will be reset after a navigation.

  TestNavigationObserver navigation_observer(web_contents);
  shell()->LoadURL(url2);

  document2_response.WaitForRequest();
  document2_response.Send(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n");
  navigation_observer.WaitForNavigationFinished();
  EXPECT_FALSE(web_contents->GetPrimaryMainFrame()->IsDOMContentLoaded());
  EXPECT_FALSE(web_contents->IsDocumentOnLoadCompletedInPrimaryMainFrame());
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
      static_cast<RenderFrameHostImpl*>(web_contents->GetPrimaryMainFrame());

  base::RunLoop loop_until_onload;
  DocumentOnLoadObserver onload_observer(web_contents,
                                         loop_until_onload.QuitClosure());
  shell()->LoadURL(url1);
  loop_until_onload.Run();

  EXPECT_TRUE(rfhi->IsDOMContentLoaded());
  EXPECT_TRUE(web_contents->IsDocumentOnLoadCompletedInPrimaryMainFrame());

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
  ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());

  EXPECT_TRUE(rfhi->IsDOMContentLoaded());
  EXPECT_TRUE(web_contents->IsDocumentOnLoadCompletedInPrimaryMainFrame());
}

// Flaky on all platforms. crbug/1336851
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       DISABLED_GetUkmSourceIds) {
  ukm::TestAutoSetUkmRecorder recorder;
  // This test site has one cross-site iframe.
  GURL main_frame_url(
      embedded_test_server()->GetURL("/frame_tree/page_with_one_frame.html"));
  WebContents* web_contents = shell()->web_contents();
  DocumentUkmSourceIdObserver observer(web_contents);

  ASSERT_TRUE(NavigateToURL(shell(), main_frame_url));

  RenderFrameHostImpl* main_frame_host =
      static_cast<RenderFrameHostImpl*>(web_contents->GetPrimaryMainFrame());
  ukm::SourceId page_ukm_source_id = main_frame_host->GetPageUkmSourceId();
  ukm::SourceId main_frame_doc_ukm_source_id =
      observer.GetPrimaryMainFrameDocumentUkmSourceId();

  ASSERT_EQ(1u, main_frame_host->child_count());
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
  // There should be one DocumentCreated entry for each of the two frames.
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
  EXPECT_FALSE(*recorder.GetEntryMetric(main_frame_document_created_entry,
                                        "IsCrossSiteFrame"));

  EXPECT_EQ(page_ukm_source_id,
            *recorder.GetEntryMetric(sub_frame_document_created_entry,
                                     "NavigationSourceId"));
  EXPECT_FALSE(*recorder.GetEntryMetric(sub_frame_document_created_entry,
                                        "IsMainFrame"));
  EXPECT_TRUE(*recorder.GetEntryMetric(sub_frame_document_created_entry,
                                       "IsCrossOriginFrame"));
  EXPECT_TRUE(*recorder.GetEntryMetric(sub_frame_document_created_entry,
                                       "IsCrossSiteFrame"));

  // Verify source creations. Main frame document source should have the URL;
  // no source should have been created for the sub-frame document.
  recorder.ExpectEntrySourceHasUrl(main_frame_document_created_entry,
                                   main_frame_url);
  EXPECT_EQ(nullptr, recorder.GetSourceForSourceId(subframe_doc_ukm_source_id));

  // Spot-check that an example entry recorded from the renderer uses the
  // correct document source id set by the RFH.
  const auto& blink_entries = recorder.GetEntriesByName("Blink.PageLoad");
  for (const ukm::mojom::UkmEntry* entry : blink_entries) {
    EXPECT_EQ(main_frame_doc_ukm_source_id, entry->source_id);
  }
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest, CrossSiteFrame) {
  ukm::TestAutoSetUkmRecorder recorder;
  // This test site has one cross-origin but same-site iframe (b.x.com).
  GURL main_frame_url(embedded_test_server()->GetURL(
      "a.x.com", "/frame_tree/page_with_cross_origin_same_site_iframe.html"));
  WebContents* web_contents = shell()->web_contents();
  DocumentUkmSourceIdObserver observer(web_contents);

  ASSERT_TRUE(NavigateToURL(shell(), main_frame_url));

  auto* sub_frame_document_created_entry =
      recorder.GetDocumentCreatedEntryForSourceId(
          observer.GetSubFrameDocumentUkmSourceId());

  // Verify the recorded values on the sub frame's DocumentCreated entry.
  EXPECT_FALSE(*recorder.GetEntryMetric(sub_frame_document_created_entry,
                                        "IsMainFrame"));
  EXPECT_TRUE(*recorder.GetEntryMetric(sub_frame_document_created_entry,
                                       "IsCrossOriginFrame"));
  EXPECT_FALSE(*recorder.GetEntryMetric(sub_frame_document_created_entry,
                                        "IsCrossSiteFrame"));
}

// TODO(crbug.com/40554401): the code below is temporary and will be
// removed when Java Bridge is mojofied.
#if BUILDFLAG(IS_ANDROID)

struct ObjectData {
  const int32_t id;
  const std::vector<std::string> methods;
};

ObjectData kMainObject{5, {"getId", "getInnerObject", "readArray"}};
ObjectData kInnerObject{10, {"getInnerId"}};

class MockInnerObject : public blink::mojom::RemoteObject {
 public:
  void HasMethod(const std::string& name, HasMethodCallback callback) override {
    std::move(callback).Run(base::Contains(kInnerObject.methods, name));
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
  void NotifyReleasedObject() override {}
};

class MockObject : public blink::mojom::RemoteObject {
 public:
  explicit MockObject(
      mojo::PendingReceiver<blink::mojom::RemoteObject> receiver)
      : receiver_(this, std::move(receiver)) {}
  void HasMethod(const std::string& name, HasMethodCallback callback) override {
    std::move(callback).Run(base::Contains(kMainObject.methods, name));
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

  void NotifyReleasedObject() override {}

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
    reference_count_map_[object_id]++;
  }

  void AcquireObject(int32_t object_id) override {
    reference_count_map_[object_id]++;
  }

  void ReleaseObject(int32_t object_id) override {
    reference_count_map_[object_id]--;
  }

  mojo::PendingRemote<blink::mojom::RemoteObjectHost> GetRemote() {
    if (receiver_.is_bound()) {
      // When a new RenderFrame is created for a navigation we call this
      // function again from `RemoteObjectInjector::RenderFrameCreated()`,
      // so unbind the previous connection with the previous RenderFrame if
      // needed. Note that we might lose some in-flight messages with the
      // previous RenderFrame in this case, so this is not perfect.
      // TODO(https://crbug.com/40615943): Add better support for RemoteObjects
      // on RenderFrame swaps, if needed.
      receiver_.reset();
    }
    return receiver_.BindNewPipeAndPassRemote();
  }

  MockObject* GetMockObject() const { return mock_object_.get(); }

  int ReferenceCount(int32_t object_id) const {
    return !reference_count_map_.at(object_id);
  }

 private:
  mojo::Receiver<blink::mojom::RemoteObjectHost> receiver_{this};
  std::unique_ptr<MockObject> mock_object_;
  std::map<int32_t, int> reference_count_map_{{kMainObject.id, 0},
                                              {kInnerObject.id, 0}};
};

class RemoteObjectInjector : public WebContentsObserver {
 public:
  explicit RemoteObjectInjector(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  RemoteObjectInjector(const RemoteObjectInjector&) = delete;
  RemoteObjectInjector& operator=(const RemoteObjectInjector&) = delete;

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
};

namespace {
void SetupRemoteObjectInvocation(Shell* shell, const GURL& url) {
  WebContents* web_contents = shell->web_contents();

  // The first load triggers RenderFrameCreated on a WebContentsObserver
  // instance, where the object injection happens.
  shell->LoadURL(url);
  EXPECT_TRUE(WaitForLoadStop(web_contents));
  // Injected objects become visible only after reload.
  web_contents->GetController().Reload(ReloadType::NORMAL, false);
  EXPECT_TRUE(WaitForLoadStop(web_contents));
}
}  // namespace

// TODO(crbug.com/40554401): Remove this when the new Java Bridge code is
// integrated into WebView.
// This test is a temporary way of verifying that the renderer part
// works as expected.
// TODO(crbug.com/347691518): This test is flaky.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       DISABLED_RemoteObjectEnumerateProperties) {
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
  EXPECT_THAT(EvalJs(web_contents(), kScript), EvalJsResult::IsError());
}

// TODO(crbug.com/40236762): This test is flaky.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       DISABLED_RemoteObjectInvokeMethodReturningNumber) {
  GURL url(embedded_test_server()->GetURL("/empty.html"));

  RemoteObjectInjector injector(web_contents());
  SetupRemoteObjectInvocation(shell(), url);

  std::string kScript = "testObject.getId();";
  EXPECT_EQ(kMainObject.id, EvalJs(web_contents(), kScript));
}

// TODO(crbug.com/40236899): This test is flaky.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       DISABLED_RemoteObjectInvokeMethodTakingArray) {
  GURL url(embedded_test_server()->GetURL("/empty.html"));

  RemoteObjectInjector injector(web_contents());
  SetupRemoteObjectInvocation(shell(), url);

  std::string kScript = "testObject.readArray([6, 8, 2]);";
  EXPECT_THAT(EvalJs(web_contents(), kScript), EvalJsResult::IsOk());
  EXPECT_EQ(
      3, injector.GetObjectHost().GetMockObject()->get_num_elements_received());
}

// TODO(crbug.com/40274210): This test is flaky.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       DISABLED_RemoteObjectInvokeMethodReturningObject) {
  GURL url(embedded_test_server()->GetURL("/empty.html"));

  RemoteObjectInjector injector(web_contents());
  SetupRemoteObjectInvocation(shell(), url);

  std::string kScript = "testObject.getInnerObject().getInnerId();";
  EXPECT_EQ(kInnerObject.id, EvalJs(web_contents(), kScript));
}

// TODO(crbug.com/340869172): This test is flaky.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       DISABLED_RemoteObjectInvokeMethodException) {
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
// TODO(crbug.com/340928363): This test is flaky.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       DISABLED_RemoteObjectRelease) {
  GURL url(embedded_test_server()->GetURL("/empty.html"));

  RemoteObjectInjector injector(web_contents());
  SetupRemoteObjectInvocation(shell(), url);

  EXPECT_EQ(
      "object",
      EvalJs(
          web_contents(),
          "globalInner = testObject.getInnerObject(); typeof globalInner; "));

  EXPECT_GT(injector.GetObjectHost().ReferenceCount(kInnerObject.id), 0);
  EXPECT_EQ("object", EvalJs(web_contents(), "gc(); typeof globalInner;"));
  EXPECT_GT(injector.GetObjectHost().ReferenceCount(kInnerObject.id), 0);
  EXPECT_EQ(
      "undefined",
      EvalJs(web_contents(), "delete globalInner; gc(); typeof globalInner;"));
  EXPECT_EQ(injector.GetObjectHost().ReferenceCount(kInnerObject.id), 0);
}

#endif  // BUILDFLAG(IS_ANDROID)

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

// Check Chrome won't attempt automatically loading the /favicon.ico if it would
// be blocked by CSP.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       DefaultFaviconVersusCSP) {
  auto navigate = [&](std::string csp) {
    EXPECT_TRUE(NavigateToURL(
        shell(), embedded_test_server()->GetURL(
                     "/set-header?Content-Security-Policy: " + csp)));
    // DidStopLoading() and UpdateFaviconURL() are sent together from the same
    // task. However we have waited only for DidStopLoading(). Make a round trip
    // with the renderer to ensure UpdateFaviconURL() to be received.
    EXPECT_TRUE(ExecJs(root_frame_host(), ""));
  };

  // Blocked by CSP.
  navigate("img-src 'none'");
  EXPECT_EQ(0u, web_contents()->GetFaviconURLs().size());

  // Allowed by CSP.
  navigate("img-src *");
  EXPECT_EQ(1u, web_contents()->GetFaviconURLs().size());
  EXPECT_EQ("/favicon.ico",
            web_contents()->GetFaviconURLs()[0]->icon_url.path());
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       GetWebExposedIsolationLevel) {
  // Not isolated:
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/empty.html")));
  EXPECT_EQ(WebExposedIsolationLevel::kNotIsolated,
            root_frame_host()->GetWebExposedIsolationLevel());

  // Cross-Origin Isolated:
  EXPECT_TRUE(NavigateToURL(shell(),
                            embedded_test_server()->GetURL(
                                "/set-header?"
                                "Cross-Origin-Opener-Policy: same-origin&"
                                "Cross-Origin-Embedder-Policy: require-corp")));
  // Status is kIsolated.
  EXPECT_EQ(WebExposedIsolationLevel::kIsolated,
            root_frame_host()->GetWebExposedIsolationLevel());
}

class IsolatedApplicationContentBrowserClient
    : public ContentBrowserTestContentBrowserClient {
 public:
  explicit IsolatedApplicationContentBrowserClient(
      const std::string& isolated_application_host)
      : isolated_application_host_(isolated_application_host) {}

  bool ShouldUrlUseApplicationIsolationLevel(BrowserContext* browser_context,
                                             const GURL& url) override {
    return url.host() == isolated_application_host_;
  }

 private:
  std::string isolated_application_host_;
};

class RenderFrameHostImplBrowserTestWithRestrictedApis
    : public RenderFrameHostImplBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    RenderFrameHostImplBrowserTest::SetUpCommandLine(command_line);

    mock_cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    RenderFrameHostImplBrowserTest::SetUpInProcessBrowserTestFixture();
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    RenderFrameHostImplBrowserTest::TearDownInProcessBrowserTestFixture();
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

 protected:
  void SetUpOnMainThread() override {
    RenderFrameHostImplBrowserTest::SetUpOnMainThread();

    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
    https_server()->ServeFilesFromSourceDirectory(GetTestDataFilePath());
    net::test_server::RegisterDefaultHandlers(https_server());
    ASSERT_TRUE(https_server()->Start());
  }

 private:
  ContentMockCertVerifier mock_cert_verifier_;
};

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTestWithRestrictedApis,
                       GetWebExposedIsolationLevel) {
  std::string app_host = "app.com";
  IsolatedApplicationContentBrowserClient client(app_host);

  // Not isolated:
  std::string non_app_host = "nonapp.com";
  TestNavigationObserver navigation_observer(web_contents());
  shell()->LoadURL(https_server()->GetURL(non_app_host, "/empty.html"));
  navigation_observer.Wait();

  EXPECT_TRUE(navigation_observer.last_navigation_succeeded());
  EXPECT_EQ(WebExposedIsolationLevel::kNotIsolated,
            root_frame_host()->GetProcess()->GetWebExposedIsolationLevel());
  EXPECT_EQ(WebExposedIsolationLevel::kNotIsolated,
            root_frame_host()->GetWebExposedIsolationLevel());
  EXPECT_EQ(false, EvalJs(root_frame_host(), "self.crossOriginIsolated"));

  // Cross-Origin Isolated:
  GURL non_app_url =
      https_server()->GetURL(non_app_host,
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Cross-Origin-Embedder-Policy: require-corp&"
                             "Cross-Origin-Resource-Policy: cross-origin");
  EXPECT_TRUE(NavigateToURL(shell(), non_app_url));
  EXPECT_EQ(WebExposedIsolationLevel::kIsolated,
            root_frame_host()->GetProcess()->GetWebExposedIsolationLevel());
  EXPECT_EQ(WebExposedIsolationLevel::kIsolated,
            root_frame_host()->GetWebExposedIsolationLevel());
  EXPECT_EQ(true, EvalJs(root_frame_host(), "self.crossOriginIsolated"));

  // Permission delegated Cross-Origin Isolated child frame:
  std::string create_iframe = R"(
    new Promise(resolve => {
      const iframe = document.createElement('iframe');
      iframe.src = $1;
      iframe.allow = $2;
      iframe.addEventListener('load', () => resolve(true));
      document.body.appendChild(iframe);
    });
  )";
  EXPECT_TRUE(ExecJs(shell(), JsReplace(create_iframe, non_app_url, "")));
  RenderFrameHost* non_app_child_frame = ChildFrameAt(root_frame_host(), 0);
  EXPECT_EQ(WebExposedIsolationLevel::kIsolated,
            non_app_child_frame->GetProcess()->GetWebExposedIsolationLevel());
  EXPECT_EQ(WebExposedIsolationLevel::kIsolated,
            non_app_child_frame->GetWebExposedIsolationLevel());
  EXPECT_EQ(true, EvalJs(non_app_child_frame, "self.crossOriginIsolated"));

  // Non permission delegated Cross-Origin Isolated child frame:
  EXPECT_TRUE(ExecJs(shell(), JsReplace(create_iframe, non_app_url,
                                        "cross-origin-isolated 'none'")));
  non_app_child_frame = ChildFrameAt(root_frame_host(), 1);
  EXPECT_EQ(WebExposedIsolationLevel::kIsolated,
            non_app_child_frame->GetProcess()->GetWebExposedIsolationLevel());
  EXPECT_EQ(WebExposedIsolationLevel::kNotIsolated,
            non_app_child_frame->GetWebExposedIsolationLevel());
  EXPECT_EQ(false, EvalJs(non_app_child_frame, "self.crossOriginIsolated"));

  // Isolated Application:
  GURL app_url =
      https_server()->GetURL(app_host,
                             "/set-header?"
                             "Cross-Origin-Opener-Policy: same-origin&"
                             "Cross-Origin-Embedder-Policy: require-corp&"
                             "Cross-Origin-Resource-Policy: same-origin");
  Shell* app_shell = shell()->CreateNewWindow(
      web_contents()->GetController().GetBrowserContext(), GURL(),
      /*site_instance=*/nullptr, gfx::Size());
  EXPECT_TRUE(NavigateToURL(app_shell, app_url));
  RenderFrameHost* app_frame = app_shell->web_contents()->GetPrimaryMainFrame();
  EXPECT_EQ(WebExposedIsolationLevel::kIsolatedApplication,
            app_frame->GetProcess()->GetWebExposedIsolationLevel());
  EXPECT_EQ(WebExposedIsolationLevel::kIsolatedApplication,
            app_frame->GetWebExposedIsolationLevel());
  EXPECT_EQ(true, EvalJs(app_frame, "self.crossOriginIsolated"));

  // Permission delegated same-origin Isolated Application child frame:
  EXPECT_TRUE(ExecJs(
      app_shell, JsReplace(create_iframe, app_url, "cross-origin-isolated")));
  RenderFrameHost* app_child_frame = ChildFrameAt(app_frame, 0);
  EXPECT_EQ(WebExposedIsolationLevel::kIsolatedApplication,
            app_child_frame->GetProcess()->GetWebExposedIsolationLevel());
  EXPECT_EQ(WebExposedIsolationLevel::kIsolatedApplication,
            app_child_frame->GetWebExposedIsolationLevel());
  EXPECT_EQ(true, EvalJs(app_child_frame, "self.crossOriginIsolated"));

  // Non permission delegated same-origin Isolated Application child frame:
  EXPECT_TRUE(ExecJs(app_shell, JsReplace(create_iframe, app_url,
                                          "cross-origin-isolated 'none'")));
  app_child_frame = ChildFrameAt(app_frame, 1);
  EXPECT_EQ(WebExposedIsolationLevel::kIsolatedApplication,
            app_child_frame->GetProcess()->GetWebExposedIsolationLevel());
  EXPECT_EQ(WebExposedIsolationLevel::kNotIsolated,
            app_child_frame->GetWebExposedIsolationLevel());
  EXPECT_EQ(false, EvalJs(app_child_frame, "self.crossOriginIsolated"));

  // Permission delegated cross-origin Isolated Application child frame:
  // The frame's WebExposedIsolationLevel isn't "isolated application" despite
  // being delegated the "cross-origin-isolated" permission because that
  // isolation level can only be delegated to same-origin child frames.
  EXPECT_TRUE(ExecJs(app_shell, JsReplace(create_iframe, non_app_url,
                                          "cross-origin-isolated")));
  non_app_child_frame = ChildFrameAt(app_frame, 2);
  EXPECT_EQ(WebExposedIsolationLevel::kIsolated,
            non_app_child_frame->GetProcess()->GetWebExposedIsolationLevel());
  EXPECT_EQ(WebExposedIsolationLevel::kIsolated,
            non_app_child_frame->GetWebExposedIsolationLevel());
  EXPECT_EQ(true, EvalJs(non_app_child_frame, "self.crossOriginIsolated"));

  // Non permission delegated cross-origin Isolated Application child frame:
  EXPECT_TRUE(ExecJs(app_shell, JsReplace(create_iframe, non_app_url,
                                          "cross-origin-isolated 'none'")));
  non_app_child_frame = ChildFrameAt(app_frame, 3);
  EXPECT_EQ(WebExposedIsolationLevel::kIsolated,
            non_app_child_frame->GetProcess()->GetWebExposedIsolationLevel());
  EXPECT_EQ(WebExposedIsolationLevel::kNotIsolated,
            non_app_child_frame->GetWebExposedIsolationLevel());
  EXPECT_EQ(false, EvalJs(non_app_child_frame, "self.crossOriginIsolated"));
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       CommitNavigationCounter) {
  GURL initial_url = embedded_test_server()->GetURL("a.com", "/title1.html");
  GURL same_document_url =
      embedded_test_server()->GetURL("a.com", "/title1.html#index");
  GURL other_url = embedded_test_server()->GetURL("a.com", "/title2.html");

  GURL blocked_url(embedded_test_server()->GetURL("a.com", "/blocked.html"));
  std::unique_ptr<URLLoaderInterceptor> url_interceptor =
      URLLoaderInterceptor::SetupRequestFailForURL(blocked_url,
                                                   net::ERR_BLOCKED_BY_CLIENT);

  // Regular, initial navigation.
  {
    RenderFrameHostImpl* initial_rfh = static_cast<RenderFrameHostImpl*>(
        web_contents()->GetPrimaryMainFrame());
    int initial_counter = initial_rfh->commit_navigation_sent_counter();

    EXPECT_TRUE(NavigateToURL(shell(), initial_url));

    EXPECT_EQ(initial_rfh, web_contents()->GetPrimaryMainFrame())
        << "No RFH swap expected.";
    EXPECT_GT(
        web_contents()->GetPrimaryMainFrame()->commit_navigation_sent_counter(),
        initial_counter)
        << "The commit_navigation_sent_counter has been increased.";
  }

  // Same document navigation.
  {
    RenderFrameHostImpl* initial_rfh = static_cast<RenderFrameHostImpl*>(
        web_contents()->GetPrimaryMainFrame());
    int initial_counter = initial_rfh->commit_navigation_sent_counter();

    EXPECT_TRUE(NavigateToURL(shell(), same_document_url));

    EXPECT_EQ(initial_rfh, web_contents()->GetPrimaryMainFrame())
        << "No RFH swap expected.";
    EXPECT_EQ(
        initial_counter,
        web_contents()->GetPrimaryMainFrame()->commit_navigation_sent_counter())
        << "The commit_navigation_sent_counter has not been increased.";
  }

  // New document navigation.
  {
    RenderFrameHostImpl* initial_rfh = static_cast<RenderFrameHostImpl*>(
        web_contents()->GetPrimaryMainFrame());
    int initial_counter = initial_rfh->commit_navigation_sent_counter();

    EXPECT_TRUE(NavigateToURL(shell(), other_url));

    EXPECT_TRUE(initial_rfh != web_contents()->GetPrimaryMainFrame() ||
                web_contents()
                        ->GetPrimaryMainFrame()
                        ->commit_navigation_sent_counter() > initial_counter)
        << "Either the RFH has been swapped or the counter has been increased.";
  }

  // Failed navigation.
  {
    RenderFrameHostImpl* initial_rfh = static_cast<RenderFrameHostImpl*>(
        web_contents()->GetPrimaryMainFrame());
    int initial_counter = initial_rfh->commit_navigation_sent_counter();

    EXPECT_FALSE(NavigateToURL(shell(), blocked_url));

    EXPECT_TRUE(initial_rfh != web_contents()->GetPrimaryMainFrame() ||
                web_contents()
                        ->GetPrimaryMainFrame()
                        ->commit_navigation_sent_counter() > initial_counter)
        << "Either the RFH has been swapped or the counter has been increased.";
  }
}

using RenderFrameHostImplSubframeReuseBrowserTest =
    RenderFrameHostImplBrowserTest;

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplSubframeReuseBrowserTest,
                       SubframeShutdownDelay) {
  // This test exercises a scenario that's only possible with
  // --site-per-process.
  if (!AreAllSitesIsolatedForTesting())
    return;

  // Navigate to a site with a subframe.
  GURL url_1(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  RenderFrameHostImpl* rfh_b =
      root_frame_host()->child_at(0)->current_frame_host();
  int subframe_process_id = rfh_b->GetProcess()->GetID();
  RenderFrameDeletedObserver delete_rfh_b(rfh_b);
  TestFrameNavigationObserver commit_observer(
      web_contents()->GetPrimaryFrameTree().root());

  // Navigate to another page on the same site with the same subframe.
  GURL url_2(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  shell()->LoadURL(url_2);

  // Wait for site |url_2| to commit, but not fully load so that its subframe is
  // not yet loaded.
  commit_observer.WaitForCommit();

  // Wait for the subframe RenderFrameHost in |url_1| to shut down.
  delete_rfh_b.WaitUntilDeleted();

  // The process hosting the subframe should have its shutdown delayed and be
  // tracked in the pending-delete tracker.
  auto* subframe_process_host = static_cast<RenderProcessHostImpl*>(
      content::RenderProcessHost::FromID(subframe_process_id));
  if (RenderProcessHostImpl::ShouldDelayProcessShutdown()) {
    ASSERT_TRUE(subframe_process_host->IsProcessShutdownDelayedForTesting());
  } else {
    ASSERT_EQ(nullptr, subframe_process_host);
  }

  // Wait for |url_2| to fully load so that its subframe loads.
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  // The process for the just-deleted subframe should be reused for the new
  // subframe, because they share the same site.
  RenderFrameHostImpl* new_rfh_b =
      root_frame_host()->child_at(0)->current_frame_host();
  ASSERT_EQ(RenderProcessHostImpl::ShouldDelayProcessShutdown(),
            subframe_process_id == new_rfh_b->GetProcess()->GetID());

  // The process should no longer be in the pending-delete tracker, as it has
  // been reused.
  if (RenderProcessHostImpl::ShouldDelayProcessShutdown()) {
    ASSERT_FALSE(static_cast<RenderProcessHostImpl*>(
                     content::RenderProcessHost::FromID(subframe_process_id))
                     ->IsProcessShutdownDelayedForTesting());
  }
}

// Test that multiple subframe-shutdown delays from the same source can be in
// effect, and that cancelling one delay does not cancel the others.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplSubframeReuseBrowserTest,
                       MultipleDelays) {
  // This test exercises a scenario that's only possible with
  // --site-per-process.
  if (!AreAllSitesIsolatedForTesting())
    return;

  // Create a test RenderProcessHostImpl.
  ASSERT_TRUE(NavigateToURL(shell(),
                            embedded_test_server()->GetURL(
                                "a.com", "/cross_site_iframe_factory.html?a")));
  RenderFrameHostImpl* rfh = root_frame_host();
  RenderProcessHostImpl* process =
      static_cast<RenderProcessHostImpl*>(rfh->GetProcess());
  EXPECT_FALSE(process->IsProcessShutdownDelayedForTesting());

  // Delay process shutdown twice from the same site info.
  const SiteInfo site_info = rfh->GetSiteInstance()->GetSiteInfo();
  const base::TimeDelta delay = base::Seconds(5);
  process->DelayProcessShutdown(delay, base::TimeDelta(), site_info);
  EXPECT_EQ(RenderProcessHostImpl::ShouldDelayProcessShutdown(),
            process->IsProcessShutdownDelayedForTesting());
  process->DelayProcessShutdown(delay, base::TimeDelta(), site_info);
  EXPECT_EQ(RenderProcessHostImpl::ShouldDelayProcessShutdown(),
            process->IsProcessShutdownDelayedForTesting());

  // When one delay is cancelled, the other should remain in effect.
  process->CancelProcessShutdownDelay(site_info);
  EXPECT_EQ(RenderProcessHostImpl::ShouldDelayProcessShutdown(),
            process->IsProcessShutdownDelayedForTesting());
  process->CancelProcessShutdownDelay(site_info);
  EXPECT_FALSE(process->IsProcessShutdownDelayedForTesting());
}

// Tests that RenderFrameHost::ForEachRenderFrameHost visits the correct frames
// in the correct order.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest, ForEachRenderFrameHost) {
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c),d)"));

  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = root_frame_host();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();
  RenderFrameHostImpl* rfh_c = rfh_b->child_at(0)->current_frame_host();
  RenderFrameHostImpl* rfh_d = rfh_a->child_at(1)->current_frame_host();

  // When starting iteration from the primary frame, we should see the frame
  // itself and its descendants in breadth first order.
  EXPECT_THAT(CollectAllRenderFrameHosts(rfh_a),
              testing::ElementsAre(rfh_a, rfh_b, rfh_d, rfh_c));

  // When starting iteration from a subframe, only it and its descendants should
  // be seen.
  EXPECT_THAT(CollectAllRenderFrameHosts(rfh_b),
              testing::ElementsAre(rfh_b, rfh_c));

  // Test that iteration stops when requested.
  {
    std::vector<RenderFrameHostImpl*> visited_frames;
    rfh_a->ForEachRenderFrameHostWithAction([&](RenderFrameHostImpl* rfh) {
      visited_frames.push_back(rfh);
      return RenderFrameHost::FrameIterationAction::kStop;
    });
    EXPECT_THAT(visited_frames, testing::ElementsAre(rfh_a));
  }
  {
    std::vector<RenderFrameHostImpl*> visited_frames;
    rfh_a->ForEachRenderFrameHostWithAction([&](RenderFrameHostImpl* rfh) {
      visited_frames.push_back(rfh);
      return RenderFrameHost::FrameIterationAction::kSkipChildren;
    });
    EXPECT_THAT(visited_frames, testing::ElementsAre(rfh_a));
  }

  // Now consider stopping or skipping children at |rfh_b|. If we skip children,
  // we skip |rfh_c|, but not |rfh_d|. If we stop iteration, we skip both
  // |rfh_c| and |rfh_d|.
  {
    std::vector<RenderFrameHostImpl*> visited_frames;
    rfh_a->ForEachRenderFrameHostWithAction([&](RenderFrameHostImpl* rfh) {
      visited_frames.push_back(rfh);
      return rfh == rfh_b ? RenderFrameHost::FrameIterationAction::kStop
                          : RenderFrameHost::FrameIterationAction::kContinue;
    });
    EXPECT_THAT(visited_frames, testing::ElementsAre(rfh_a, rfh_b));
  }
  {
    std::vector<RenderFrameHostImpl*> visited_frames;
    rfh_a->ForEachRenderFrameHostWithAction([&](RenderFrameHostImpl* rfh) {
      visited_frames.push_back(rfh);
      return rfh == rfh_b ? RenderFrameHost::FrameIterationAction::kSkipChildren
                          : RenderFrameHost::FrameIterationAction::kContinue;
    });
    EXPECT_THAT(visited_frames, testing::ElementsAre(rfh_a, rfh_b, rfh_d));
  }

  EXPECT_EQ(nullptr, rfh_a->GetParentOrOuterDocument());
  EXPECT_EQ(rfh_a, rfh_b->GetParentOrOuterDocument());
  EXPECT_EQ(rfh_b, rfh_c->GetParentOrOuterDocument());
  EXPECT_EQ(rfh_a, rfh_d->GetParentOrOuterDocument());
  EXPECT_EQ(rfh_a, rfh_a->GetOutermostMainFrame());
  EXPECT_EQ(rfh_a, rfh_b->GetOutermostMainFrame());
  EXPECT_EQ(rfh_a, rfh_c->GetOutermostMainFrame());
  EXPECT_EQ(rfh_a, rfh_d->GetOutermostMainFrame());
  EXPECT_EQ(rfh_a, rfh_a->GetOutermostMainFrameOrEmbedder());
  EXPECT_EQ(rfh_a, rfh_b->GetOutermostMainFrameOrEmbedder());
  EXPECT_EQ(rfh_a, rfh_c->GetOutermostMainFrameOrEmbedder());
  EXPECT_EQ(rfh_a, rfh_d->GetOutermostMainFrameOrEmbedder());
}

// Tests that RenderFrameHost::ForEachRenderFrameHost does not expose
// speculative RFHs, unless content internal code requests them.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       ForEachRenderFrameHostSpeculative) {
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = root_frame_host();
  EXPECT_EQ(LifecycleStateImpl::kActive, rfh_a->lifecycle_state());

  TestNavigationManager nav_manager(web_contents(), url_b);
  shell()->LoadURL(url_b);
  nav_manager.WaitForSpeculativeRenderFrameHostCreation();

  RenderFrameHostImpl* rfh_b =
      rfh_a->frame_tree_node()->render_manager()->speculative_frame_host();
  ASSERT_TRUE(rfh_b);
  EXPECT_EQ(LifecycleStateImpl::kSpeculative, rfh_b->lifecycle_state());

  // We test that the following properties hold during both the speculative and
  // pending commit lifecycle state of |rfh_b|.
  base::RepeatingClosure test_expectations = base::BindRepeating(
      [](RenderFrameHostImpl* rfh_a, RenderFrameHostImpl* rfh_b) {
        // ForEachRenderFrameHost does not expose the speculative RFH.
        EXPECT_THAT(CollectAllRenderFrameHosts(rfh_a),
                    testing::ElementsAre(rfh_a));

        // When we request the speculative RFH, we visit it.
        EXPECT_THAT(CollectAllRenderFrameHostsIncludingSpeculative(rfh_a),
                    testing::UnorderedElementsAre(rfh_a, rfh_b));

        // If ForEachRenderFrameHost is called on a speculative RFH directly, do
        // nothing.
        rfh_b->ForEachRenderFrameHostWithAction([](RenderFrameHostImpl* rfh) {
          ADD_FAILURE() << "Visited speculative RFH";
          return RenderFrameHost::FrameIterationAction::kStop;
        });

        // If we request speculative RFHs and directly call this on a
        // speculative RFH, just visit the given speculative RFH.
        EXPECT_THAT(CollectAllRenderFrameHostsIncludingSpeculative(rfh_b),
                    testing::ElementsAre(rfh_b));
      },
      rfh_a, rfh_b);

  {
    SCOPED_TRACE("Speculative LifecycleState");
    test_expectations.Run();
  }

  class ReadyToCommitObserver : public WebContentsObserver {
   public:
    explicit ReadyToCommitObserver(WebContentsImpl* web_contents,
                                   base::RepeatingClosure test_expectations)
        : WebContentsObserver(web_contents),
          test_expectations_(test_expectations) {}

    // WebContentsObserver:
    void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override {
      EXPECT_EQ(static_cast<RenderFrameHostImpl*>(
                    navigation_handle->GetRenderFrameHost())
                    ->lifecycle_state(),
                LifecycleStateImpl::kPendingCommit);
      SCOPED_TRACE("PendingCommit LifecycleState");
      test_expectations_.Run();
    }

   private:
    base::RepeatingClosure test_expectations_;
  };

  ReadyToCommitObserver ready_to_commit_observer(web_contents(),
                                                 test_expectations);
  ASSERT_TRUE(nav_manager.WaitForNavigationFinished());
}

// Like ForEachRenderFrameHostSpeculative, but for a speculative RFH for a
// subframe navigation.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       ForEachRenderFrameHostSpeculativeWithSubframes) {
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c))"));
  GURL url_d(embedded_test_server()->GetURL("d.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = root_frame_host();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();
  RenderFrameHostImpl* rfh_c = rfh_b->child_at(0)->current_frame_host();
  EXPECT_EQ(LifecycleStateImpl::kActive, rfh_a->lifecycle_state());
  EXPECT_EQ(LifecycleStateImpl::kActive, rfh_b->lifecycle_state());
  EXPECT_EQ(LifecycleStateImpl::kActive, rfh_c->lifecycle_state());

  TestNavigationManager nav_manager(web_contents(), url_d);
  ASSERT_TRUE(BeginNavigateToURLFromRenderer(rfh_b, url_d));
  nav_manager.WaitForSpeculativeRenderFrameHostCreation();

  RenderFrameHostImpl* rfh_d =
      rfh_b->frame_tree_node()->render_manager()->speculative_frame_host();
  ASSERT_TRUE(rfh_d);
  EXPECT_EQ(LifecycleStateImpl::kSpeculative, rfh_d->lifecycle_state());

  // ForEachRenderFrameHost does not expose the speculative RFH.
  EXPECT_THAT(CollectAllRenderFrameHosts(rfh_a),
              testing::ElementsAre(rfh_a, rfh_b, rfh_c));

  // When we request the speculative RFH, we visit it.
  EXPECT_THAT(CollectAllRenderFrameHostsIncludingSpeculative(rfh_a),
              testing::UnorderedElementsAre(rfh_a, rfh_b, rfh_d, rfh_c));

  // When beginning iteration from the current RFH of the navigating frame, we
  // also visit the speculative RFH.
  EXPECT_THAT(CollectAllRenderFrameHostsIncludingSpeculative(rfh_b),
              testing::UnorderedElementsAre(rfh_b, rfh_d, rfh_c));

  // If ForEachRenderFrameHost is called on a speculative RFH directly, do
  // nothing.
  rfh_d->ForEachRenderFrameHostWithAction([](RenderFrameHostImpl* rfh) {
    ADD_FAILURE() << "Visited speculative RFH";
    return RenderFrameHost::FrameIterationAction::kStop;
  });

  // If we request speculative RFHs and directly call this on a speculative RFH,
  // just visit the given speculative RFH.
  EXPECT_THAT(CollectAllRenderFrameHostsIncludingSpeculative(rfh_d),
              testing::ElementsAre(rfh_d));

  // Test that iteration stops when requested.
  {
    // We don't check the RFHs visited in the interest of not overtesting the
    // ordering of speculative RFHs.
    bool stopped = false;
    rfh_a->ForEachRenderFrameHostIncludingSpeculativeWithAction(
        [&](RenderFrameHostImpl* rfh) {
          EXPECT_FALSE(stopped);
          if (rfh->lifecycle_state() == LifecycleStateImpl::kSpeculative) {
            stopped = true;
            return RenderFrameHost::FrameIterationAction::kStop;
          }
          return RenderFrameHost::FrameIterationAction::kContinue;
        });
  }

  {
    bool stopped = false;
    rfh_b->ForEachRenderFrameHostIncludingSpeculativeWithAction(
        [&](RenderFrameHostImpl* rfh) {
          EXPECT_FALSE(stopped);
          if (rfh->lifecycle_state() == LifecycleStateImpl::kSpeculative) {
            stopped = true;
            return RenderFrameHost::FrameIterationAction::kStop;
          }
          return RenderFrameHost::FrameIterationAction::kContinue;
        });
  }

  // Skipping the children of a current RFH whose FrameTreeNode has a
  // speculative RFH skips the children but still includes the speculative RFH.
  {
    std::vector<RenderFrameHostImpl*> visited_frames;
    rfh_a->ForEachRenderFrameHostIncludingSpeculativeWithAction(
        [&](RenderFrameHostImpl* rfh) {
          visited_frames.push_back(rfh);
          return (rfh == rfh_b)
                     ? RenderFrameHost::FrameIterationAction::kSkipChildren
                     : RenderFrameHost::FrameIterationAction::kContinue;
        });
    EXPECT_THAT(visited_frames,
                testing::UnorderedElementsAre(rfh_a, rfh_b, rfh_d));
  }

  {
    std::vector<RenderFrameHostImpl*> visited_frames;
    rfh_b->ForEachRenderFrameHostIncludingSpeculativeWithAction(
        [&](RenderFrameHostImpl* rfh) {
          visited_frames.push_back(rfh);
          return (rfh == rfh_b)
                     ? RenderFrameHost::FrameIterationAction::kSkipChildren
                     : RenderFrameHost::FrameIterationAction::kContinue;
        });
    EXPECT_THAT(visited_frames, testing::UnorderedElementsAre(rfh_b, rfh_d));
  }

  // Skipping the children of a speculative RFH is not useful, but is included
  // here for completeness of testing.
  {
    std::vector<RenderFrameHostImpl*> visited_frames;
    rfh_a->ForEachRenderFrameHostIncludingSpeculativeWithAction(
        [&](RenderFrameHostImpl* rfh) {
          visited_frames.push_back(rfh);
          return (rfh->lifecycle_state() == LifecycleStateImpl::kSpeculative)
                     ? RenderFrameHost::FrameIterationAction::kSkipChildren
                     : RenderFrameHost::FrameIterationAction::kContinue;
        });
    EXPECT_THAT(visited_frames,
                testing::UnorderedElementsAre(rfh_a, rfh_b, rfh_d, rfh_c));
  }

  {
    std::vector<RenderFrameHostImpl*> visited_frames;
    rfh_b->ForEachRenderFrameHostIncludingSpeculativeWithAction(
        [&](RenderFrameHostImpl* rfh) {
          visited_frames.push_back(rfh);
          return (rfh->lifecycle_state() == LifecycleStateImpl::kSpeculative)
                     ? RenderFrameHost::FrameIterationAction::kSkipChildren
                     : RenderFrameHost::FrameIterationAction::kContinue;
        });
    EXPECT_THAT(visited_frames,
                testing::UnorderedElementsAre(rfh_b, rfh_d, rfh_c));
  }
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       ForEachRenderFrameHostPendingDeletion) {
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c))"));
  GURL url_d(embedded_test_server()->GetURL("d.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = root_frame_host();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();
  RenderFrameHostImpl* rfh_c = rfh_b->child_at(0)->current_frame_host();
  EXPECT_EQ(LifecycleStateImpl::kActive, rfh_a->lifecycle_state());
  EXPECT_EQ(LifecycleStateImpl::kActive, rfh_b->lifecycle_state());
  EXPECT_EQ(LifecycleStateImpl::kActive, rfh_c->lifecycle_state());
  LeaveInPendingDeletionState(rfh_a);
  LeaveInPendingDeletionState(rfh_b);
  LeaveInPendingDeletionState(rfh_c);

  EXPECT_TRUE(NavigateToURL(shell(), url_d));
  RenderFrameHostImpl* rfh_d = root_frame_host();

  // ForEachRenderFrameHost on the primary RFH does not visit the pending delete
  // RFHs.
  EXPECT_THAT(CollectAllRenderFrameHosts(rfh_d), testing::ElementsAre(rfh_d));

  // ForEachRenderFrameHost on the pending delete RFHs only visits the pending
  // delete RFHs.
  EXPECT_THAT(CollectAllRenderFrameHosts(rfh_a),
              testing::ElementsAre(rfh_a, rfh_b, rfh_c));
  EXPECT_THAT(CollectAllRenderFrameHosts(rfh_b),
              testing::ElementsAre(rfh_b, rfh_c));
}

// Tests that RenderFrameHost::ForEachRenderFrameHost visits the frames of an
// inner WebContents.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       ForEachRenderFrameHostInnerContents) {
  GURL url_a(embedded_test_server()->GetURL("a.com", "/page_with_iframe.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = root_frame_host();
  WebContentsImpl* inner_contents = static_cast<WebContentsImpl*>(
      CreateAndAttachInnerContents(rfh_a->child_at(0)->current_frame_host()));
  ASSERT_TRUE(NavigateToURLFromRenderer(inner_contents, url_b));

  RenderFrameHostImpl* rfh_b = inner_contents->GetPrimaryMainFrame();

  EXPECT_THAT(CollectAllRenderFrameHosts(rfh_a),
              testing::ElementsAre(rfh_a, rfh_b));
  EXPECT_EQ(nullptr, rfh_b->GetParent());
  // Note that since this is a generic test inner WebContents, whether it's
  // considered an outer document or embedder is just an implementation detail.
  EXPECT_EQ(nullptr, rfh_b->GetParentOrOuterDocument());
  EXPECT_EQ(rfh_b, rfh_b->GetOutermostMainFrame());
  EXPECT_EQ(rfh_a, rfh_b->GetParentOrOuterDocumentOrEmbedder());
  EXPECT_EQ(rfh_a, rfh_b->GetOutermostMainFrameOrEmbedder());
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       ForEachRenderFrameHostInnerContentsWithSubframes) {
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a(a),a)"));
  GURL url_b(embedded_test_server()->GetURL(
      "b.com", "/cross_site_iframe_factory.html?b(c(d),e)"));

  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a_main = root_frame_host();
  RenderFrameHostImpl* rfh_a_sub1 =
      rfh_a_main->child_at(0)->current_frame_host();
  RenderFrameHostImpl* rfh_a_sub2 =
      rfh_a_main->child_at(1)->current_frame_host();
  WebContentsImpl* inner_contents =
      static_cast<WebContentsImpl*>(CreateAndAttachInnerContents(
          rfh_a_sub1->child_at(0)->current_frame_host()));
  ASSERT_TRUE(NavigateToURLFromRenderer(inner_contents, url_b));

  RenderFrameHostImpl* rfh_b = inner_contents->GetPrimaryMainFrame();
  RenderFrameHostImpl* rfh_c = rfh_b->child_at(0)->current_frame_host();
  RenderFrameHostImpl* rfh_d = rfh_c->child_at(0)->current_frame_host();
  RenderFrameHostImpl* rfh_e = rfh_b->child_at(1)->current_frame_host();

  EXPECT_THAT(CollectAllRenderFrameHosts(rfh_a_main),
              testing::ElementsAre(rfh_a_main, rfh_a_sub1, rfh_a_sub2, rfh_b,
                                   rfh_c, rfh_e, rfh_d));
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       ForEachRenderFrameHostMultipleInnerContents) {
  // After attaching inner contents, this will be A(B(C),D)
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a,a)"));
  GURL url_b(embedded_test_server()->GetURL(
      "b.com", "/cross_site_iframe_factory.html?b(b)"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));
  GURL url_d(embedded_test_server()->GetURL("d.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = root_frame_host();

  WebContentsImpl* contents_b = static_cast<WebContentsImpl*>(
      CreateAndAttachInnerContents(rfh_a->child_at(0)->current_frame_host()));
  ASSERT_TRUE(NavigateToURLFromRenderer(contents_b, url_b));
  RenderFrameHostImpl* rfh_b = contents_b->GetPrimaryMainFrame();

  WebContentsImpl* contents_c = static_cast<WebContentsImpl*>(
      CreateAndAttachInnerContents(rfh_b->child_at(0)->current_frame_host()));
  ASSERT_TRUE(NavigateToURLFromRenderer(contents_c, url_c));
  RenderFrameHostImpl* rfh_c = contents_c->GetPrimaryMainFrame();

  WebContentsImpl* contents_d = static_cast<WebContentsImpl*>(
      CreateAndAttachInnerContents(rfh_a->child_at(1)->current_frame_host()));
  ASSERT_TRUE(NavigateToURLFromRenderer(contents_d, url_d));
  RenderFrameHostImpl* rfh_d = contents_d->GetPrimaryMainFrame();

  EXPECT_THAT(CollectAllRenderFrameHosts(rfh_a),
              testing::ElementsAre(rfh_a, rfh_b, rfh_d, rfh_c));
}

// This test verifies that RFHImpl::ForEachImmediateLocalRoot works as expected.
// The frame tree used in the test is:
//                                A0
//                            /    |    \
//                          A1     B1    A2
//                         /  \    |    /  \
//                        B2   A3  B3  A4   C2
//                       /    /   / \    \
//                      D1   D2  C3  C4  C5
//
// As an example, the expected set of immediate local roots for the root node A0
// should be {B1, B2, C2, D2, C5}. Note that the order is compatible with that
// of a BFS traversal from root node A0.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       FindImmediateLocalRoots) {
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  GURL main_url(embedded_test_server()->GetURL(
      "a.com",
      "/cross_site_iframe_factory.html?a(a(b(d),a(d)),b(b(c,c)),a(a(c),c))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Each entry is of the frame "LABEL:ILR1ILR2..." where ILR stands for
  // immediate local root.
  const auto immediate_local_roots = std::to_array<std::string>(
      {"A0:B1B2C2D2C5", "A1:B2D2", "B1:C3C4", "A2:C2C5", "B2:D1", "A3:D2",
       "B3:C3C4", "A4:C5", "C2:", "D1:", "D2:", "C3:", "C4:", "C5:"});

  std::map<RenderFrameHostImpl*, std::string>
      frame_to_immediate_local_roots_map;
  std::map<RenderFrameHostImpl*, std::string> frame_to_label_map;
  size_t index = 0;
  // Map each RenderFrameHostImpl to its label and set of immediate local roots.
  for (auto* ftn : web_contents()->GetPrimaryFrameTree().Nodes()) {
    std::string roots = immediate_local_roots[index++];
    frame_to_immediate_local_roots_map[ftn->current_frame_host()] = roots;
    frame_to_label_map[ftn->current_frame_host()] = roots.substr(0, 2);
  }

  // For each frame in the tree, verify that ForEachImmediateLocalRoot properly
  // visits each and only each immediate local root in a BFS traversal order.
  for (auto* ftn : web_contents()->GetPrimaryFrameTree().Nodes()) {
    RenderFrameHostImpl* current_frame_host = ftn->current_frame_host();
    std::vector<RenderFrameHostImpl*> frame_list;
    current_frame_host->ForEachImmediateLocalRoot(
        [&frame_list](RenderFrameHostImpl* rfh) { frame_list.push_back(rfh); });

    std::string result = frame_to_label_map[current_frame_host];
    result.append(":");
    for (auto* ilr_ptr : frame_list)
      result.append(frame_to_label_map[ilr_ptr]);
    EXPECT_EQ(frame_to_immediate_local_roots_map[current_frame_host], result);
  }
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest, GetSiblings) {
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  // Use actual FrameTreeNode id values in URL.
  GURL main_url(embedded_test_server()->GetURL(
      "1.com", "/cross_site_iframe_factory.html?1(2,3(5),4)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* ftn1 = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* ftn2 = ftn1->child_at(0);
  FrameTreeNode* ftn3 = ftn1->child_at(1);
  FrameTreeNode* ftn4 = ftn1->child_at(2);
  FrameTreeNode* ftn5 = ftn3->child_at(0);

  // Check root node.
  EXPECT_EQ(ftn1->current_frame_host()->NextSibling(), nullptr);
  EXPECT_EQ(ftn1->current_frame_host()->PreviousSibling(), nullptr);

  // Check first child of root (leaf node).
  EXPECT_EQ(ftn2->current_frame_host()->NextSibling(), ftn3);
  EXPECT_EQ(ftn2->current_frame_host()->PreviousSibling(), nullptr);

  // Check second child of root (has child).
  EXPECT_EQ(ftn3->current_frame_host()->NextSibling(), ftn4);
  EXPECT_EQ(ftn3->current_frame_host()->PreviousSibling(), ftn2);

  // Check third child of root (leaf).
  EXPECT_EQ(ftn4->current_frame_host()->NextSibling(), nullptr);
  EXPECT_EQ(ftn4->current_frame_host()->PreviousSibling(), ftn3);

  // Check deepest node in tree (leaf with no siblings).
  EXPECT_EQ(ftn5->current_frame_host()->NextSibling(), nullptr);
  EXPECT_EQ(ftn5->current_frame_host()->PreviousSibling(), nullptr);
}

// Helpers for the DestructorLifetime test case.
class DestructorLifetimeDocumentService
    // The interface in question doesn't really matter here, so just pick a
    // generic one with an easy interface to stub.
    : public DocumentService<blink::mojom::BrowserInterfaceBroker> {
 public:
  DestructorLifetimeDocumentService(
      RenderFrameHostImpl& render_frame_host,
      mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker> receiver,
      bool& was_destroyed)
      : DocumentService(render_frame_host, std::move(receiver)),
        render_frame_host_(render_frame_host.GetWeakPtr()),
        page_(render_frame_host.GetPage().GetWeakPtr()),
        was_destroyed_(was_destroyed) {}

  ~DestructorLifetimeDocumentService() override {
    *was_destroyed_ = true;
    // The destructor should run before SafeRef<RenderFrameHost> is invalidated.
    EXPECT_TRUE(render_frame_host_);
    EXPECT_TRUE(page_);
  }

  void GetInterface(mojo::GenericPendingReceiver pending_receiver) override {}

 private:
  // This should be a SafeRef but that is not yet exposed publicly.
  const base::WeakPtr<RenderFrameHostImpl> render_frame_host_;
  const base::WeakPtr<Page> page_;
  const raw_ref<bool> was_destroyed_;
};

class DestructorLifetimeDocumentUserData
    : public DocumentUserData<DestructorLifetimeDocumentUserData> {
 public:
  explicit DestructorLifetimeDocumentUserData(
      RenderFrameHost* render_frame_host,
      bool& was_destroyed)
      : DocumentUserData<DestructorLifetimeDocumentUserData>(render_frame_host),
        render_frame_host_(
            static_cast<RenderFrameHostImpl*>(render_frame_host)->GetWeakPtr()),
        page_(render_frame_host->GetPage().GetWeakPtr()),
        was_destroyed_(was_destroyed) {}

  ~DestructorLifetimeDocumentUserData() override {
    *was_destroyed_ = true;
    // The destructor should run before SafeRef<RenderFrameHost> is invalidated.
    EXPECT_TRUE(render_frame_host_);
    EXPECT_TRUE(page_);
  }

 private:
  friend DocumentUserData;
  DOCUMENT_USER_DATA_KEY_DECL();

  // This should be a SafeRef or use render_frame_host().
  const base::WeakPtr<RenderFrameHostImpl> render_frame_host_;
  const base::WeakPtr<Page> page_;
  const raw_ref<bool> was_destroyed_;
};

DOCUMENT_USER_DATA_KEY_IMPL(DestructorLifetimeDocumentUserData);

// Tests that when RenderFrameHostImpl is destroyed, destructors of
// commonly-used extension points (currently DocumentService and
// DocumentUserData) run while RenderFrameHostImpl is still in a
// reasonable state.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       MainFrameSameSiteNavigationDestructorLifetime) {
  // The test assumes that the main frame RFH will be reused when navigating.
  DisableBackForwardCacheForTesting(shell()->web_contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);

  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));

  RenderFrameHostImpl* main_frame = web_contents()->GetPrimaryMainFrame();
  ASSERT_TRUE(main_frame);

  bool document_service_was_destroyed = false;
  mojo::Remote<blink::mojom::BrowserInterfaceBroker> remote;
  // This is self-owned so the bare new is OK.
  new DestructorLifetimeDocumentService(*main_frame,
                                        remote.BindNewPipeAndPassReceiver(),
                                        document_service_was_destroyed);

  bool document_user_data_was_destroyed = false;
  DestructorLifetimeDocumentUserData::CreateForCurrentDocument(
      main_frame, document_user_data_was_destroyed);

  RenderFrameHostWrapper main_frame_wrapper(main_frame);
  ASSERT_FALSE(main_frame_wrapper.IsDestroyed());

  // Perform a same-site navigation in the main frame.
  EXPECT_TRUE(NavigateToURLFromRenderer(
      main_frame, embedded_test_server()->GetURL("a.com", "/title2.html")));

  // The navigation should reuse the same RenderFrameHost, except when
  // RenderDocument is enabled.
  if (ShouldCreateNewHostForAllFrames()) {
    EXPECT_TRUE(main_frame_wrapper.WaitUntilRenderFrameDeleted());
  } else {
    EXPECT_EQ(web_contents()->GetPrimaryMainFrame(), main_frame_wrapper.get());
  }

  // The destructors of DestructorLifetimeDocumentService and
  // DestructorLifetimeDocumentUserData also perform googletest
  // assertions to validate invariants.
  EXPECT_TRUE(document_service_was_destroyed);
  EXPECT_TRUE(document_user_data_was_destroyed);
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       MainFrameCrossSiteNavigationDestructorLifetime) {
  // The test assumes that the main frame RFH will be replaced during
  // navigation.
  DisableBackForwardCacheForTesting(shell()->web_contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);
  // All sites must be isolated in order for the navigatino code to replace the
  // navigated RFH.
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());

  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));

  RenderFrameHostImpl* main_frame = web_contents()->GetPrimaryMainFrame();
  ASSERT_TRUE(main_frame);

  bool document_service_was_destroyed = false;
  mojo::Remote<blink::mojom::BrowserInterfaceBroker> remote;
  // This is self-owned so the bare new is OK.
  new DestructorLifetimeDocumentService(*main_frame,
                                        remote.BindNewPipeAndPassReceiver(),
                                        document_service_was_destroyed);

  bool document_user_data_was_destroyed = false;
  DestructorLifetimeDocumentUserData::CreateForCurrentDocument(
      main_frame, document_user_data_was_destroyed);

  RenderFrameHostWrapper main_frame_wrapper(main_frame);
  ASSERT_FALSE(main_frame_wrapper.IsDestroyed());

  // Perform a cross-site navigation in the main frame.
  EXPECT_TRUE(NavigateToURLFromRenderer(
      main_frame, embedded_test_server()->GetURL("b.com", "/title2.html")));

  ASSERT_TRUE(main_frame_wrapper.WaitUntilRenderFrameDeleted());

  // The destructors of DestructorLifetimeDocumentService and
  // DestructorLifetimeDocumentUserData also perform googletest
  // assertions to validate invariants.
  EXPECT_TRUE(main_frame_wrapper.IsDestroyed());
  EXPECT_TRUE(document_service_was_destroyed);
  EXPECT_TRUE(document_user_data_was_destroyed);
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       ChildFrameSameSiteNavigationDestructorLifetime) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   "a.com", "/cross_site_iframe_factory.html?a(a)")));

  RenderFrameHostImpl* child_frame =
      static_cast<RenderFrameHostImpl*>(ChildFrameAt(shell(), 0));
  ASSERT_TRUE(child_frame);
  bool should_change_rfh =
      child_frame->ShouldChangeRenderFrameHostOnSameSiteNavigation();

  bool document_service_was_destroyed = false;
  mojo::Remote<blink::mojom::BrowserInterfaceBroker> remote;
  // This is self-owned so the bare new is OK.
  new DestructorLifetimeDocumentService(*child_frame,
                                        remote.BindNewPipeAndPassReceiver(),
                                        document_service_was_destroyed);

  bool document_user_data_was_destroyed = false;
  DestructorLifetimeDocumentUserData::CreateForCurrentDocument(
      child_frame, document_user_data_was_destroyed);

  RenderFrameHostWrapper child_frame_wrapper(child_frame);
  ASSERT_FALSE(child_frame_wrapper.IsDestroyed());

  // Perform a same-site navigation in the child frame.
  EXPECT_TRUE(NavigateToURLFromRenderer(
      child_frame, embedded_test_server()->GetURL("a.com", "/title2.html")));

  // The navigation should reuse the same RenderFrameHost, except when
  // RenderDocument is enabled.
  if (should_change_rfh) {
    EXPECT_TRUE(child_frame_wrapper.WaitUntilRenderFrameDeleted());
  } else {
    EXPECT_EQ(ChildFrameAt(shell(), 0), child_frame_wrapper.get());
  }

  // The destructors of DestructorLifetimeDocumentService and
  // DestructorLifetimeDocumentUserData also perform googletest
  // assertions to validate invariants.
  EXPECT_TRUE(document_service_was_destroyed);
  EXPECT_TRUE(document_user_data_was_destroyed);
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       ChildFrameCrossSiteNavigationDestructorLifetime) {
  // All sites must be isolated in order for the navigatino code to replace the
  // navigated RFH.
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());

  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   "a.com", "/cross_site_iframe_factory.html?a(a)")));

  RenderFrameHostImpl* child_frame =
      static_cast<RenderFrameHostImpl*>(ChildFrameAt(shell(), 0));
  ASSERT_TRUE(child_frame);

  bool document_service_was_destroyed = false;
  mojo::Remote<blink::mojom::BrowserInterfaceBroker> remote;
  // This is self-owned so the bare new is OK.
  new DestructorLifetimeDocumentService(*child_frame,
                                        remote.BindNewPipeAndPassReceiver(),
                                        document_service_was_destroyed);

  bool document_user_data_was_destroyed = false;
  DestructorLifetimeDocumentUserData::CreateForCurrentDocument(
      child_frame, document_user_data_was_destroyed);

  RenderFrameHostWrapper child_frame_wrapper(child_frame);
  ASSERT_FALSE(child_frame_wrapper.IsDestroyed());

  // Perform a cross-site navigation in the child frame.
  EXPECT_TRUE(NavigateToURLFromRenderer(
      child_frame, embedded_test_server()->GetURL("b.com", "/title2.html")));

  ASSERT_TRUE(child_frame_wrapper.WaitUntilRenderFrameDeleted());

  // The destructors of DestructorLifetimeDocumentService and
  // DestructorLifetimeDocumentUserData also perform googletest
  // assertions to validate invariants.
  EXPECT_TRUE(child_frame_wrapper.IsDestroyed());
  EXPECT_TRUE(document_service_was_destroyed);
  EXPECT_TRUE(document_user_data_was_destroyed);
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       ChildFrameDetachDestructorLifetime) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   "a.com", "/cross_site_iframe_factory.html?a(a)")));

  RenderFrameHostImpl* child_frame =
      static_cast<RenderFrameHostImpl*>(ChildFrameAt(shell(), 0));
  ASSERT_TRUE(child_frame);

  bool document_service_was_destroyed = false;
  mojo::Remote<blink::mojom::BrowserInterfaceBroker> remote;
  // This is self-owned so the bare new is OK.
  new DestructorLifetimeDocumentService(*child_frame,
                                        remote.BindNewPipeAndPassReceiver(),
                                        document_service_was_destroyed);

  bool document_user_data_was_destroyed = false;
  DestructorLifetimeDocumentUserData::CreateForCurrentDocument(
      child_frame, document_user_data_was_destroyed);

  RenderFrameHostWrapper child_frame_wrapper(child_frame);
  ASSERT_FALSE(child_frame_wrapper.IsDestroyed());

  // Remove the child frame from the DOM, which destroys the RenderFrameHost.
  EXPECT_TRUE(ExecJs(shell(), "document.querySelector('iframe').remove()"));

  // The destructors of DestructorLifetimeDocumentService and
  // DestructorLifetimeDocumentUserData also perform googletest
  // assertions to validate invariants.
  EXPECT_TRUE(child_frame_wrapper.IsDestroyed());
  EXPECT_TRUE(document_service_was_destroyed);
  EXPECT_TRUE(document_user_data_was_destroyed);
}

class DestructorLifetimePageUserData
    : public PageUserData<DestructorLifetimePageUserData> {
 public:
  explicit DestructorLifetimePageUserData(Page& page, bool& was_destroyed)
      : PageUserData<DestructorLifetimePageUserData>(page),
        page_(page.GetWeakPtr()),
        was_destroyed_(was_destroyed) {}

  ~DestructorLifetimePageUserData() override {
    *was_destroyed_ = true;

    // The destructor should run before WeakPtr<Page> is invalidated.
    EXPECT_TRUE(page_);
    if (!page_) {
      return;
    }
    // Check Page is still accessible through RenderFrameHost::GetPage. Such
    // access can happen in PageUserData implementations with a complicated
    // destructor.
    auto& render_frame_host = page_->GetMainDocument();
    auto& page = render_frame_host.GetPage();
    // Check returned Page reference is valid.
    EXPECT_EQ(page_.get(), &page);
    // The page is considered as primary if its RenderFrameHost is active. This
    // will be true unless we changed RenderFrameHosts.
    EXPECT_EQ(page.IsPrimary(), !ShouldCreateNewHostForAllFrames());
    if (ShouldCreateNewHostForAllFrames()) {
      EXPECT_EQ(render_frame_host.GetLifecycleState(),
                RenderFrameHost::LifecycleState::kPendingDeletion);
    }
  }

 private:
  friend PageUserData;
  PAGE_USER_DATA_KEY_DECL();

  const base::WeakPtr<Page> page_;
  const raw_ref<bool> was_destroyed_;
};

PAGE_USER_DATA_KEY_IMPL(DestructorLifetimePageUserData);

// Tests that when DocumentAssociatedData is destroyed, destructors of
// PageUserData run while Page and DocumentAssociatedData are still in a
// reasonable state.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       PageUserDataDestructorLifetime) {
  // The test assumes that the Page will get destructed after navigation.
  DisableBackForwardCacheForTesting(shell()->web_contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);

  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));

  RenderFrameHostImpl* main_frame = web_contents()->GetPrimaryMainFrame();
  ASSERT_TRUE(main_frame);

  bool page_user_data_was_destroyed = false;

  DestructorLifetimePageUserData::CreateForPage(main_frame->GetPage(),
                                                page_user_data_was_destroyed);

  RenderFrameHostWrapper main_frame_wrapper(main_frame);
  ASSERT_FALSE(main_frame_wrapper.IsDestroyed());

  // Perform a same-site navigation in the main frame.
  EXPECT_TRUE(NavigateToURLFromRenderer(
      main_frame, embedded_test_server()->GetURL("a.com", "/title2.html")));

  // The navigation should reuse the same RenderFrameHost, except when
  // RenderDocument is enabled.
  if (ShouldCreateNewHostForAllFrames()) {
    EXPECT_TRUE(main_frame_wrapper.WaitUntilRenderFrameDeleted());
  } else {
    EXPECT_EQ(web_contents()->GetPrimaryMainFrame(), main_frame_wrapper.get());
  }

  // The destructor of DestructorLifetimePageUserData also perform googletest
  // assertions to validate invariants.
  EXPECT_TRUE(page_user_data_was_destroyed);
}

class RenderFrameHostImplCredentiallessIframeBrowserTest
    : public RenderFrameHostImplBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    RenderFrameHostImplBrowserTest::SetUpCommandLine(command_line);

    // Enable parsing the iframe 'credentialless' attribute.
    command_line->AppendSwitch(switches::kEnableBlinkTestFeatures);
  }
};

// This test checks that the initial empty document in a credentialless iframe
// whose parent document is not credentialless is credentialless.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplCredentiallessIframeBrowserTest,
                       InitialEmptyDocumentInCredentiallessIframe) {
  GURL main_url = embedded_test_server()->GetURL("/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  RenderFrameHostImpl* main_rfh = web_contents()->GetPrimaryMainFrame();

  // Create an empty iframe
  EXPECT_TRUE(ExecJs(main_rfh,
                     "let child = document.createElement('iframe');"
                     "child.credentialless = true;"
                     "document.body.appendChild(child);"));
  WaitForLoadStop(web_contents());

  EXPECT_FALSE(main_rfh->IsCredentialless());
  EXPECT_EQ(false, EvalJs(main_rfh, "window.credentialless"));
  EXPECT_FALSE(main_rfh->GetStorageKey().nonce().has_value());

  EXPECT_EQ(1U, main_rfh->child_count());
  EXPECT_TRUE(main_rfh->child_at(0)->Credentialless());
  EXPECT_FALSE(main_rfh->child_at(0)->current_frame_host()->IsCredentialless());
  EXPECT_EQ(true, EvalJs(main_rfh->child_at(0)->current_frame_host(),
                         "window.credentialless"));
  EXPECT_FALSE(main_rfh->child_at(0)
                   ->current_frame_host()
                   ->GetStorageKey()
                   .nonce()
                   .has_value());
}

// Check that a page's credentialless_iframes_nonce is re-initialized after
// navigations.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplCredentiallessIframeBrowserTest,
                       NewCredentiallessNonceOnNavigation) {
  GURL main_url = embedded_test_server()->GetURL("/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  base::UnguessableToken first_nonce =
      web_contents()->GetPrimaryPage().credentialless_iframes_nonce();
  EXPECT_TRUE(first_nonce);

  // Same-document navigation does not change the nonce.
  EXPECT_TRUE(NavigateToURL(shell(), main_url.Resolve("#here")));
  EXPECT_EQ(first_nonce,
            web_contents()->GetPrimaryPage().credentialless_iframes_nonce());

  // Cross-document same-site navigation creates a new nonce.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));
  base::UnguessableToken second_nonce =
      web_contents()->GetPrimaryPage().credentialless_iframes_nonce();
  EXPECT_TRUE(second_nonce);
  EXPECT_NE(first_nonce, second_nonce);

  // Cross-document cross-site navigation creates a new nonce.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title1.html")));
  EXPECT_NE(first_nonce,
            web_contents()->GetPrimaryPage().credentialless_iframes_nonce());
  EXPECT_NE(second_nonce,
            web_contents()->GetPrimaryPage().credentialless_iframes_nonce());
}

class RenderFrameHostImplCredentiallessIframeNikBrowserTest
    : public RenderFrameHostImplCredentiallessIframeBrowserTest {
 public:
  RenderFrameHostImplCredentiallessIframeNikBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        net::features::kPartitionConnectionsByNetworkIsolationKey);
  }

  void SetUpOnMainThread() override {
    alternate_test_server_ =
        std::make_unique<net::test_server::EmbeddedTestServer>();
    connection_tracker_ = std::make_unique<net::test_server::ConnectionTracker>(
        alternate_test_server_.get());
    alternate_test_server_->AddDefaultHandlers(GetTestDataFilePath());
    ASSERT_TRUE(alternate_test_server_->Start());
    RenderFrameHostImplCredentiallessIframeBrowserTest::SetUpOnMainThread();
  }

  void ResetNetworkState() {
    auto* network_context = shell()
                                ->web_contents()
                                ->GetBrowserContext()
                                ->GetDefaultStoragePartition()
                                ->GetNetworkContext();
    base::RunLoop close_all_connections_loop;
    network_context->CloseAllConnections(
        close_all_connections_loop.QuitClosure());
    close_all_connections_loop.Run();

    connection_tracker_->ResetCounts();
  }

 protected:
  std::unique_ptr<net::test_server::ConnectionTracker> connection_tracker_;
  std::unique_ptr<net::EmbeddedTestServer> alternate_test_server_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplCredentiallessIframeNikBrowserTest,
                       CredentiallessIframeHasPartitionedNetworkState) {
  GURL main_url = embedded_test_server()->GetURL("/title1.html");

  for (bool credentialless : {false, true}) {
    SCOPED_TRACE(credentialless ? "credentialless iframe" : "normal iframe");
    EXPECT_TRUE(NavigateToURL(shell(), main_url));

    RenderFrameHostImpl* main_rfh = web_contents()->GetPrimaryMainFrame();

    // Create an iframe.
    EXPECT_TRUE(ExecJs(main_rfh,
                       JsReplace("let child = document.createElement('iframe');"
                                 "child.src = $1;"
                                 "child.credentialless = $2;"
                                 "document.body.appendChild(child);",
                                 main_url, credentialless)));
    WaitForLoadStop(web_contents());
    EXPECT_EQ(1U, main_rfh->child_count());
    RenderFrameHostImpl* iframe = main_rfh->child_at(0)->current_frame_host();
    EXPECT_EQ(credentialless, iframe->IsCredentialless());
    EXPECT_EQ(credentialless, EvalJs(iframe, "window.credentialless"));

    ResetNetworkState();

    std::string main_url_origin = main_url.DeprecatedGetOriginAsURL().spec();
    // Remove trailing '/'.
    main_url_origin.pop_back();

    GURL fetch_url = alternate_test_server_->GetURL(
        "/set-header?"
        "Access-Control-Allow-Credentials: true&"
        "Access-Control-Allow-Origin: " +
        main_url_origin);

    // Preconnect a socket with the NetworkIsolationKey of the main frame.
    shell()
        ->web_contents()
        ->GetBrowserContext()
        ->GetDefaultStoragePartition()
        ->GetNetworkContext()
        ->PreconnectSockets(1, fetch_url.DeprecatedGetOriginAsURL(),
                            network::mojom::CredentialsMode::kInclude,
                            main_rfh->GetIsolationInfoForSubresources()
                                .network_anonymization_key());

    connection_tracker_->WaitForAcceptedConnections(1);
    EXPECT_EQ(1u, connection_tracker_->GetAcceptedSocketCount());
    EXPECT_EQ(0u, connection_tracker_->GetReadSocketCount());

    std::string fetch_resource = JsReplace(
        "(async () => {"
        "  let resp = (await fetch($1, { credentials : 'include'}));"
        "  return resp.status; })();",
        fetch_url);

    EXPECT_EQ(200, EvalJs(iframe, fetch_resource));

    // The normal iframe should reuse the preconnected socket, the
    // credentialless iframe should open a new one.
    if (!credentialless) {
      EXPECT_EQ(1u, connection_tracker_->GetAcceptedSocketCount());
    } else {
      EXPECT_EQ(2u, connection_tracker_->GetAcceptedSocketCount());
    }
    EXPECT_EQ(1u, connection_tracker_->GetReadSocketCount());
  }
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest, ErrorDocuments) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/empty.html"));
  {
    // Block the navigation.
    std::unique_ptr<URLLoaderInterceptor> url_interceptor =
        URLLoaderInterceptor::SetupRequestFailForURL(
            main_url, net::ERR_BLOCKED_BY_CLIENT);
    TestNavigationManager manager(web_contents(), main_url);
    shell()->LoadURL(main_url);
    ASSERT_TRUE(manager.WaitForNavigationFinished());
  }

  EXPECT_TRUE(web_contents()->GetPrimaryMainFrame()->IsErrorDocument());

  // Reload with no blocking.
  shell()->Reload();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  EXPECT_FALSE(web_contents()->GetPrimaryMainFrame()->IsErrorDocument());

  std::string script =
      "let child = document.createElement('iframe');"
      "child.src = $1;"
      "document.body.appendChild(child);";

  // Create an iframe.
  EXPECT_TRUE(ExecJs(web_contents()->GetPrimaryMainFrame(),
                     JsReplace(script, "title1.html")));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));

  {
    // Block the navigation.
    GURL child_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
    std::unique_ptr<URLLoaderInterceptor> url_interceptor =
        URLLoaderInterceptor::SetupRequestFailForURL(
            child_url, net::ERR_BLOCKED_BY_CLIENT);
    // Create an iframe but block the navigation.
    EXPECT_TRUE(ExecJs(web_contents()->GetPrimaryMainFrame(),
                       JsReplace(script, "title1.html")));
    EXPECT_TRUE(WaitForLoadStop(web_contents()));
  }

  RenderFrameHostImpl* main_rfh = web_contents()->GetPrimaryMainFrame();
  EXPECT_EQ(2U, main_rfh->child_count());

  RenderFrameHost* child_a = main_rfh->child_at(0)->current_frame_host();
  RenderFrameHost* child_b = main_rfh->child_at(1)->current_frame_host();
  EXPECT_FALSE(web_contents()->GetPrimaryMainFrame()->IsErrorDocument());
  EXPECT_FALSE(child_a->IsErrorDocument());
  EXPECT_TRUE(child_b->IsErrorDocument());
}

// Tests that a popup that is opened by a subframe inherits the subframe's
// origin, instead of the main frame's origin.
// Regression test for https://crbug.com/1311820 and https://crbug.com/1291764.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       PopupOpenedBySubframeHasCorrectOrigin) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  // Navigate to a page with a cross-site iframe.
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);

  // Verify that the main frame & subframe origin differs.
  url::Origin a_origin = url::Origin::Create(main_url);
  url::Origin b_origin = url::Origin::Create(child->current_url());
  EXPECT_EQ(a_origin, root->current_frame_host()->GetLastCommittedOrigin());
  EXPECT_EQ(b_origin, child->current_frame_host()->GetLastCommittedOrigin());
  EXPECT_NE(a_origin, b_origin);

  {
    // From the subframe, open a popup that stays on the initial empty
    // document.
    WebContentsAddedObserver popup_observer;
    ASSERT_TRUE(ExecJs(child, "var w = window.open('/nocontent');"));
    WebContentsImpl* popup =
        static_cast<WebContentsImpl*>(popup_observer.GetWebContents());
    FrameTreeNode* popup_frame =
        popup->GetPrimaryMainFrame()->frame_tree_node();

    // The popup should inherit the subframe's origin. Before the fix for
    // https://crbug.com/1311820, the popup used to inherit the main frame's
    // origin instead.
    EXPECT_EQ(b_origin,
              popup_frame->current_frame_host()->GetLastCommittedOrigin());
    EXPECT_EQ(b_origin.Serialize(), EvalJs(popup_frame, "self.origin"));

    // Try calling document.open() on the popup from itself.
    // This used to cause a renderer kill as the browser used to notice the
    // current origin & process lock mismatched when the document.open()
    // notification IPC arrives.
    EXPECT_EQ(GURL("about:blank"), EvalJs(popup_frame, "location.href"));
    EXPECT_TRUE(ExecJs(popup_frame, "document.open()"));
    EXPECT_EQ(GURL("about:blank"), EvalJs(popup_frame, "location.href"));

    // Try updating the URL of the popup to the opener subframe's URL by
    // calling document.open() on the popup from the opener subframe.
    // This used to cause a renderer kill as the browser used to expect that
    // the popup frame can only update to URLs under `a_origin`, while the
    // new URL is under `b_origin`. See also https://crbug.com/1291764.
    EXPECT_TRUE(ExecJs(child, "w.document.open()"));
    EXPECT_EQ(child->current_url().spec(),
              EvalJs(popup_frame, "location.href"));
  }

  {
    // From the subframe, open a popup that stays on the initial empty
    // document, and specify 'noopener' to sever the opener relationship.
    WebContentsAddedObserver popup_observer;
    ASSERT_TRUE(
        ExecJs(child, "var w = window.open('/nocontent', '', 'noopener');"));
    WebContentsImpl* popup =
        static_cast<WebContentsImpl*>(popup_observer.GetWebContents());
    FrameTreeNode* popup_frame =
        popup->GetPrimaryMainFrame()->frame_tree_node();
    EXPECT_EQ(nullptr, EvalJs(popup_frame, "window.opener"));

    // The popup should use a new opaque origin, instead of the subframe's
    // origin.
    EXPECT_NE(b_origin,
              popup_frame->current_frame_host()->GetLastCommittedOrigin());
    EXPECT_TRUE(
        popup_frame->current_frame_host()->GetLastCommittedOrigin().opaque());
    EXPECT_EQ("null", EvalJs(popup_frame, "self.origin"));
  }
}

class ShouldShowLoadingUIDelegate : public WebContentsDelegate {
 public:
  void LoadingStateChanged(WebContents* source,
                           bool should_show_loading_ui) final {
    is_loading_values_.push_back(source->IsLoading());
    did_show_loading_ui_values_.push_back(should_show_loading_ui);
  }

  const std::vector<bool>& is_loading_values() { return is_loading_values_; }
  const std::vector<bool>& did_show_loading_ui_values() {
    return did_show_loading_ui_values_;
  }

 private:
  std::vector<bool> is_loading_values_;
  std::vector<bool> did_show_loading_ui_values_;
};

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       NavigationApiInterceptsBrowserInitiatedSameDocument) {
  GURL main_url = embedded_test_server()->GetURL("a.com", "/title1.html");
  GURL same_doc_url = embedded_test_server()->GetURL("a.com", "/title1.html#a");
  ASSERT_TRUE(NavigateToURL(shell(), main_url));

  std::unique_ptr<ShouldShowLoadingUIDelegate> delegate =
      std::make_unique<ShouldShowLoadingUIDelegate>();
  web_contents()->SetDelegate(delegate.get());

  EXPECT_TRUE(
      ExecJs(web_contents(),
             "navigation.onnavigate = e => e.intercept("
             "{ handler: () => new Promise(r => setTimeout(r, 100)) })"));
  ASSERT_TRUE(NavigateToURL(shell(), same_doc_url));

  ASSERT_EQ(delegate->is_loading_values().size(), 3ul);
  ASSERT_EQ(delegate->did_show_loading_ui_values().size(), 3ul);

  // First LoadingStateChanged called: start for a browser-initiated
  // same-document navigation. NavigationApi hasn't intercepted yet, so the
  // default same-document behavior of not showing loading UI applies.
  EXPECT_TRUE(delegate->is_loading_values()[0]);
  EXPECT_FALSE(delegate->did_show_loading_ui_values()[0]);

  // Navigation commits, and it's a NavigationApi intercept. The commit message
  // will not trigger DidStartLoading() (because is_loading() is already
  // true), but when StartLoadingForAsyncNavigationApiCommit() is called, a
  // LoadingStateChanged to enable loading UI is fired.
  EXPECT_TRUE(delegate->is_loading_values()[1]);
  EXPECT_TRUE(delegate->did_show_loading_ui_values()[1]);

  // Navigation completes.
  EXPECT_FALSE(delegate->is_loading_values()[2]);
  EXPECT_FALSE(delegate->did_show_loading_ui_values()[2]);
}

// Like NavigationApiInterceptsBrowserInitiatedSameDocument, but the navigation
// is fast enough that the browser never receives a
// StartLoadingForAsyncNavigationApiCommit message from the renderer.
IN_PROC_BROWSER_TEST_F(
    RenderFrameHostImplBrowserTest,
    NavigationApiInterceptsBrowserInitiatedSameDocumentFastHandler) {
  GURL main_url = embedded_test_server()->GetURL("a.com", "/title1.html");
  GURL same_doc_url = embedded_test_server()->GetURL("a.com", "/title1.html#a");
  ASSERT_TRUE(NavigateToURL(shell(), main_url));

  std::unique_ptr<ShouldShowLoadingUIDelegate> delegate =
      std::make_unique<ShouldShowLoadingUIDelegate>();
  web_contents()->SetDelegate(delegate.get());

  // Intercept, but take only a short time to execute the handler.
  // The renderer waits 50ms before sending a
  // StartLoadingForAsyncNavigationApiCommit, and will not send it at all if,
  // as in this case, the navigation completes before the timer fires.
  EXPECT_TRUE(
      ExecJs(web_contents(),
             "navigation.onnavigate = e => e.intercept("
             "{ handler: () => new Promise(r => setTimeout(r, 10)) })"));
  ASSERT_TRUE(NavigateToURL(shell(), same_doc_url));

  ASSERT_EQ(delegate->is_loading_values().size(), 2ul);
  ASSERT_EQ(delegate->did_show_loading_ui_values().size(), 2ul);

  // First LoadingStateChanged called: start for a browser-initiated
  // same-document navigation. NavigationApi hasn't intercepted yet, so the
  // default same-document behavior of not showing loading UI applies.
  EXPECT_TRUE(delegate->is_loading_values()[0]);
  EXPECT_FALSE(delegate->did_show_loading_ui_values()[0]);

  // Next, the navigation will commit due to the intercept. The commit will not
  // trigger DidStartLoading() (because is_loading() is already
  // true), and the renderer will complete its navigation before it would have
  // called StartLoadingForAsyncNavigationApiCommit(), so we never enable the
  // loading UI.

  // Navigation completes.
  EXPECT_FALSE(delegate->is_loading_values()[1]);
  EXPECT_FALSE(delegate->did_show_loading_ui_values()[1]);
}

// Ensure that navigating with a frame tree of A(B(A)) results in the right
// number of beforeunload messages sent.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBeforeUnloadBrowserTest,
                       RendererInitiatedNavigationInABA) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(a))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Install a beforeunload handler to send a ping from both a's.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  InstallBeforeUnloadHandler(root->child_at(0)->child_at(0), SEND_PING);

  // Disable beforeunload timer to prevent flakiness.
  PrepContentsForBeforeUnloadTest(web_contents());

  // Navigate the main frame.
  DOMMessageQueue msg_queue(web_contents());
  GURL new_url(embedded_test_server()->GetURL("c.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), new_url));

  // We should have received one pings (for the grandchild 'a').
  EXPECT_EQ(1, RetrievePingsFromMessageQueue(&msg_queue));

  // We shouldn't have seen any beforeunload dialogs.
  EXPECT_EQ(0, dialog_manager()->num_beforeunload_dialogs_seen());
}

class RenderFrameHostImplBrowserTestWithStoragePartitioning
    : public RenderFrameHostImplBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  RenderFrameHostImplBrowserTestWithStoragePartitioning() {
    if (ThirdPartyStoragePartitioningEnabled()) {
      scoped_feature_list_.InitAndEnableFeature(
          net::features::kThirdPartyStoragePartitioning);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          net::features::kThirdPartyStoragePartitioning);
    }
  }
  bool ThirdPartyStoragePartitioningEnabled() { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    RenderFrameHostImplBrowserTestWithStoragePartitioning,
    /*third_party_storage_partitioning_enabled*/ testing::Bool());

IN_PROC_BROWSER_TEST_P(RenderFrameHostImplBrowserTestWithStoragePartitioning,
                       RenderIframeWithHostPermissionsToChildFrame) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(a))"));
  GURL child_rfh_url(embedded_test_server()->GetURL(
      "b.com", "/cross_site_iframe_factory.html?b(a())"));
  GURL grandchild_rfh_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a()"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  RenderFrameHostImpl* root_rfh = web_contents()->GetPrimaryMainFrame();
  RenderFrameHostImpl* child_rfh_1 =
      root_rfh->child_at(0)->current_frame_host();
  RenderFrameHostImpl* grandchild_rfh_1 =
      child_rfh_1->child_at(0)->current_frame_host();

  // Check root document setup. The StorageKey at the root should be the same
  // regardless of if `kThirdPartyStoragePartitioning` is enabled.
  EXPECT_EQ(blink::StorageKey::CreateFirstParty(url::Origin::Create(main_url)),
            root_rfh->GetStorageKey());

  // child_rfh_1 should have a StorageKey of a.com + b.com when
  // `kThirdPartyStoragePartitioning` is enabled and there are no host
  // permissions set.
  if (ThirdPartyStoragePartitioningEnabled()) {
    EXPECT_EQ(blink::StorageKey::Create(
                  url::Origin::Create(child_rfh_url),
                  net::SchemefulSite(root_rfh->GetLastCommittedOrigin()),
                  blink::mojom::AncestorChainBit::kCrossSite),
              child_rfh_1->GetStorageKey());

    EXPECT_EQ(blink::StorageKey::Create(
                  url::Origin::Create(grandchild_rfh_url),
                  net::SchemefulSite(root_rfh->GetLastCommittedOrigin()),
                  blink::mojom::AncestorChainBit::kCrossSite),
              grandchild_rfh_1->GetStorageKey());
  } else {
    // When `kThirdPartyStoragePartitioning` is disabled, the child and
    // grandchild document's storage key should depend only on their own origin
    // regardless of host permissions.
    EXPECT_EQ(
        blink::StorageKey::CreateFirstParty(url::Origin::Create(child_rfh_url)),
        child_rfh_1->GetStorageKey());

    EXPECT_EQ(blink::StorageKey::CreateFirstParty(
                  url::Origin::Create(grandchild_rfh_url)),
              grandchild_rfh_1->GetStorageKey());
  }

  // Give host permissions for b.com (child_rfh) to a.com (root_rfh).
  {
    std::vector<network::mojom::CorsOriginPatternPtr> patterns;
    base::RunLoop run_loop;
    patterns.push_back(network::mojom::CorsOriginPattern::New(
        "http", "b.com", 0,
        network::mojom::CorsDomainMatchMode::kAllowSubdomains,
        network::mojom::CorsPortMatchMode::kAllowAnyPort,
        network::mojom::CorsOriginAccessMatchPriority::kDefaultPriority));
    CorsOriginPatternSetter::Set(
        root_rfh->GetBrowserContext(), root_rfh->GetLastCommittedOrigin(),
        std::move(patterns), {}, run_loop.QuitClosure());
    run_loop.Run();
  }
  // Navigate main host to re-calculate StorageKey calculation.
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  root_rfh = web_contents()->GetPrimaryMainFrame();
  RenderFrameHostImpl* child_rfh_2 =
      root_rfh->child_at(0)->current_frame_host();
  RenderFrameHostImpl* grandchild_rfh_2 =
      child_rfh_2->child_at(0)->current_frame_host();

  // root_rfh's storage key should not have changed.
  EXPECT_EQ(blink::StorageKey::CreateFirstParty(url::Origin::Create(main_url)),
            root_rfh->GetStorageKey());

  if (ThirdPartyStoragePartitioningEnabled()) {
    // When `kThirdPartyStoragePartitioning` is enabled, the child rfh should
    // now have a top level StorageKey because it is the direct child of the
    // root document and the root has host permissions to it.
    EXPECT_EQ(blink::StorageKey(blink::StorageKey::Create(
                  url::Origin::Create(child_rfh_url),
                  net::SchemefulSite(url::Origin::Create(child_rfh_url)),
                  blink::mojom::AncestorChainBit::kSameSite)),
              child_rfh_2->GetStorageKey());

    // The grandchild document should create a StorageKey using the child
    // document's origin as the top level site.
    EXPECT_EQ(blink::StorageKey::Create(
                  url::Origin::Create(grandchild_rfh_url),
                  net::SchemefulSite(url::Origin::Create(child_rfh_url)),
                  blink::mojom::AncestorChainBit::kCrossSite),
              grandchild_rfh_2->GetStorageKey());
  } else {
    // When `kThirdPartyStoragePartitioning` is disabled, the child and
    // grandchild document's storage key should depend only on their own origin
    // regardless of host permissions.
    EXPECT_EQ(
        blink::StorageKey::CreateFirstParty(url::Origin::Create(child_rfh_url)),
        child_rfh_2->GetStorageKey());

    EXPECT_EQ(blink::StorageKey::CreateFirstParty(
                  url::Origin::Create(grandchild_rfh_url)),
              grandchild_rfh_2->GetStorageKey());
  }
}

// Tests for clearing window.name on cross-site cross-BrowsingInstance
// navigations, with swapping BrowsingContextState and clearing window.name both
// enabled and disabled.
class RenderFrameHostImplBrowsingContextStateNameTest
    : public RenderFrameHostImplBrowserTest,
      public testing::WithParamInterface<testing::tuple<bool, bool>> {
 public:
  // Provides meaningful param names instead of /0, /1, ...
  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    auto [enable_browsing_context_state_swap, disable_frame_name_update] =
        info.param;
    return base::StringPrintf(
        "%s_%s",
        enable_browsing_context_state_swap
            ? "NewBrowsingContextStateOnBrowsingContextGroupSwap"
            : "LegacyOneToOneWithFrameTreeNode",
        disable_frame_name_update
            ? "DisableFrameNameUpdateOnNonCurrentRenderFrameHost"
            : "EnableFrameNameUpdateOnNonCurrentRenderFrameHost");
  }

 protected:
  void SetUp() override {
    // TODO(crbug.com/40840863): Flaky on Mac and Android.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
    GTEST_SKIP();
#else
    // TODO(crbug.com/40840863): This configuration is flaky, for every tests.
    if (!DisableFrameNameUpdateOnNonCurrentRenderFrameHost()) {
      GTEST_SKIP();
    }

    // TODO(crbug.com/40259517):
    // A RenderViewHostImpl from outside the BackForward take a
    // `main_browsing_context_state` associated with a RenderFrameHost from
    // within the BackForwardCache.
    //
    // During WebContents deletion, this causes the
    // RenderViewHostImpl::main_browsing_context_state SafeRef to become
    // dangling, because the BackForwardCache is cleared first.
    if (NewBrowsingContextStateOnBrowsingContextGroupSwap()) {
      GTEST_SKIP();
    }

    browsing_context_state_feature_list_.InitWithFeatureState(
        features::kNewBrowsingContextStateOnBrowsingContextGroupSwap,
        NewBrowsingContextStateOnBrowsingContextGroupSwap());

    disable_name_update_feature_list_.InitWithFeatureState(
        features::kDisableFrameNameUpdateOnNonCurrentRenderFrameHost,
        DisableFrameNameUpdateOnNonCurrentRenderFrameHost());

    RenderFrameHostImplBrowserTest::SetUp();
#endif
  }

  bool DisableFrameNameUpdateOnNonCurrentRenderFrameHost() {
    return testing::get<0>(GetParam());
  }

  bool NewBrowsingContextStateOnBrowsingContextGroupSwap() {
    return testing::get<1>(GetParam());
  }

 private:
  base::test::ScopedFeatureList browsing_context_state_feature_list_;
  base::test::ScopedFeatureList disable_name_update_feature_list_;
};

// Test that, when the RenderFrameHostImpl is in the BackForwardCache, the
// name update is blocked if kDisableFrameNameUpdateOnNonCurrentRenderFrameHost
// is enabled
IN_PROC_BROWSER_TEST_P(RenderFrameHostImplBrowsingContextStateNameTest,
                       BlockNameUpdateForBackForwardCache) {
  // This test specifically wants to test with BackForwardCache enabled, so skip
  // it if BackForwardCache is disabled.
  if (!IsBackForwardCacheEnabled())
    return;

  // Create the RenderFrameHost and store it in the BackForwardCache.
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* render_frame_host =
      web_contents()->GetPrimaryMainFrame();
  EXPECT_TRUE(ExecJs(render_frame_host, "window.name = 'page_name';"));
  EXPECT_TRUE(ExecJs(render_frame_host, "window.name == 'page_name';"));
  EXPECT_EQ(render_frame_host->browsing_context_state()->frame_name(),
            "page_name");
  EXPECT_EQ(render_frame_host->browsing_context_state()
                ->current_replication_state()
                .unique_name,
            "");

  // Update the name using a pagehide handler to ensure that it occurs while the
  // RenderFrameHost is in the BackForwardCache. This typically shouldn't occur
  // and the name update should therefore be blocked.
  EXPECT_TRUE(ExecJs(
      render_frame_host,
      "window.onpagehide = function() { window.name = 'unused_name'; }"));

  // Navigate so that the current RenderFrameHost is cached.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(render_frame_host->IsInBackForwardCache());

  std::string frame_name =
      render_frame_host->browsing_context_state()->frame_name();
  std::string unique_name = render_frame_host->browsing_context_state()
                                ->current_replication_state()
                                .unique_name;

  if (DisableFrameNameUpdateOnNonCurrentRenderFrameHost()) {
    // Verify that the frame name and unique name haven't been changed, even
    // though a name change was triggered by the Javascript.
    EXPECT_EQ(frame_name, "page_name");
    EXPECT_EQ(unique_name, "");
  } else {
    // Verify that the frame name and unique name have been changed, as we are
    // not disabling the update.
    EXPECT_EQ(frame_name, "unused_name");
    EXPECT_EQ(unique_name, "");
  }
}

// Test that, when the RenderFrameHostImpl is in a pending delete state, the
// name update is blocked if kDisableFrameNameUpdateOnNonCurrentRenderFrameHost
// is enabled
IN_PROC_BROWSER_TEST_P(RenderFrameHostImplBrowsingContextStateNameTest,
                       BlockNameUpdateForPendingDelete) {
  // Disable BackForwardCache so that a pending delete state can be forced.
  web_contents()->GetController().GetBackForwardCache().DisableForTesting(
      content::BackForwardCache::TEST_USES_UNLOAD_EVENT);

  // Create the RenderFrameHost and mark it as a pending delete.
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* render_frame_host =
      web_contents()->GetPrimaryMainFrame();
  LeaveInPendingDeletionState(render_frame_host);
  EXPECT_TRUE(ExecJs(render_frame_host, "window.name = 'page_name';"));
  EXPECT_TRUE(ExecJs(render_frame_host, "window.name == 'page_name';"));
  EXPECT_EQ(render_frame_host->browsing_context_state()->frame_name(),
            "page_name");
  EXPECT_EQ(render_frame_host->browsing_context_state()
                ->current_replication_state()
                .unique_name,
            "");

  // Update the name using an unload handler to ensure that it occurs while the
  // RenderFrameHost is in a pending delete state. This typically shouldn't
  // occur and the name update should therefore be blocked.
  EXPECT_TRUE(
      ExecJs(render_frame_host,
             "window.onunload = function() { window.name = 'unused_name'; }"));

  // Navigate so that the current RenderFrameHost is marked as pending delete.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(render_frame_host->IsPendingDeletion());

  std::string frame_name =
      render_frame_host->browsing_context_state()->frame_name();
  std::string unique_name = render_frame_host->browsing_context_state()
                                ->current_replication_state()
                                .unique_name;

  if (DisableFrameNameUpdateOnNonCurrentRenderFrameHost()) {
    // Verify that the frame name and unique name haven't been changed, even
    // though a name change was triggered by the Javascript.
    EXPECT_EQ(frame_name, "page_name");
    EXPECT_EQ(unique_name, "");
  } else {
    // Verify that the frame name and unique name have been changed, as we are
    // not disabling the update.
    EXPECT_EQ(frame_name, "unused_name");
    EXPECT_EQ(unique_name, "");
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    RenderFrameHostImplBrowsingContextStateNameTest,
    testing::Combine(testing::Bool(), testing::Bool()),
    RenderFrameHostImplBrowsingContextStateNameTest::DescribeParams);

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       ResetOwnerInPendingDeletion) {
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));

  // Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = web_contents()->GetPrimaryMainFrame();

  // With BackForwardCache, swapped out RenderFrameHost won't have a
  // replacement proxy as the document is stored in cache.
  DisableBackForwardCacheForTesting(shell()->web_contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);

  // Set up a slow unload handler to force the RFH to linger in the unloaded
  // but not-yet-deleted state.
  EXPECT_TRUE(ExecJs(rfh_a, "window.onunload = function(){}"));

  // Leave rfh_a in pending deletion state.
  LeaveInPendingDeletionState(rfh_a);

  // Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = web_contents()->GetPrimaryMainFrame();

  EXPECT_NE(rfh_a, rfh_b);

  EXPECT_TRUE(rfh_a->IsPendingDeletion());
  EXPECT_EQ(nullptr, rfh_a->owner_);
  EXPECT_NE(nullptr, rfh_b->owner_);
  EXPECT_EQ(rfh_b->owner_, web_contents()->GetPrimaryFrameTree().root());
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       SetOwnerInSpeculativeRFHOwner) {
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = web_contents()->GetPrimaryMainFrame();

  // 2) Leave rfh_a in pending deletion state to check for rfh_a
  // LifecycleStateImpl after navigating to B.
  LeaveInPendingDeletionState(rfh_a);

  // 3) Start navigation to B, but don't commit yet.
  TestNavigationManager manager(web_contents(), url_b);
  shell()->LoadURL(url_b);
  manager.WaitForSpeculativeRenderFrameHostCreation();

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  RenderFrameHostImpl* pending_rfh =
      root->render_manager()->speculative_frame_host();
  EXPECT_TRUE(pending_rfh);
  EXPECT_NE(nullptr, pending_rfh->owner_);
}

// Tests that the devtools_navigation_token is set correctly after:
// 1) Initialization of the RFH (initial empty document)
// 2) First navigation
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       DevToolsNavigationToken_InitialNavigation) {
  RenderFrameHostImplWrapper rfh(web_contents()->GetPrimaryMainFrame());
  EXPECT_EQ(rfh->GetDevToolsNavigationToken(), std::nullopt);

  GURL url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  TestNavigationManager nav_manager(web_contents(), url);
  shell()->LoadURL(url);
  ASSERT_TRUE(nav_manager.WaitForFirstYieldAfterDidStartNavigation());
  base::UnguessableToken devtools_navigation_token =
      NavigationRequest::From(nav_manager.GetNavigationHandle())
          ->devtools_navigation_token();
  ASSERT_TRUE(nav_manager.WaitForNavigationFinished());
  ASSERT_EQ(rfh.get(), web_contents()->GetPrimaryMainFrame());
  EXPECT_EQ(rfh->GetDevToolsNavigationToken(), devtools_navigation_token);
}

// Tests that the devtools_navigation_token changes after a cross-document
// navigation where the RFH is swapped.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       DevToolsNavigationToken_CrossDocumentNavigation) {
  // This test requires that a same-site main frame navigation changes
  // RenderFrameHosts, so we return early if that isn't possible.
  if (!CanSameSiteMainFrameNavigationsChangeRenderFrameHosts()) {
    LOG(ERROR) << "Skipping test due to precondition not being met.";
    return;
  }
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());

  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(web_contents()->GetPrimaryMainFrame());
  auto dnt_a = rfh_a->GetDevToolsNavigationToken();
  EXPECT_TRUE(dnt_a.has_value());

  ASSERT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImplWrapper rfh_b(web_contents()->GetPrimaryMainFrame());
  ASSERT_NE(rfh_a.get(), rfh_b.get());
  auto dnt_b = rfh_b->GetDevToolsNavigationToken();
  EXPECT_TRUE(dnt_b.has_value());

  // The devtools_navigation_token for |rfh_b| should be different from
  // |rfh_a|'s token.
  EXPECT_NE(dnt_a, dnt_b);
}

// Tests that the devtools_navigation_token changes after a cross-document
// navigation where the RFH stays the same.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       DevToolsNavigationToken_SameRFHCrossDocumentNavigation) {
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("a.com", "/title2.html"));

  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(web_contents()->GetPrimaryMainFrame());

  // This test requires that a same site main frame navigation reuses the
  // current RFH, which will not happen if RenderDocument is enabled.
  if (rfh_a->ShouldChangeRenderFrameHostOnSameSiteNavigation()) {
    LOG(ERROR) << "This test case is supposed to test behaviour when a "
                  "same-site navigation reuses the current RFH, which will not "
                  "happen if RenderDocument is enabled.";
    return;
  }

  auto dnt_a = rfh_a->GetDevToolsNavigationToken();
  EXPECT_TRUE(dnt_a.has_value());

  DisableProactiveBrowsingInstanceSwapFor(rfh_a.get());
  ASSERT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImplWrapper rfh_b(web_contents()->GetPrimaryMainFrame());
  ASSERT_EQ(rfh_a->GetSiteInstance(), rfh_b->GetSiteInstance());
  ASSERT_EQ(rfh_a.get(), rfh_b.get());
  auto dnt_b = rfh_b->GetDevToolsNavigationToken();
  EXPECT_TRUE(dnt_b.has_value());

  // The devtools_navigation_token should change after the navigation commits.
  EXPECT_NE(dnt_a, dnt_b);
}

// Tests that the devtools_navigation_token is unchanged after a same document
// navigation.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       DevToolsNavigationToken_SameDocumentNavigation) {
  GURL url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  RenderFrameHostImplWrapper rfh(web_contents()->GetPrimaryMainFrame());
  auto dnt = rfh->GetDevToolsNavigationToken();

  GURL anchor_url = GURL(url.spec() + "#anchor");
  TestNavigationManager nav_manager(web_contents(), anchor_url);
  shell()->LoadURL(anchor_url);
  ASSERT_TRUE(nav_manager.WaitForNavigationFinished());
  ASSERT_EQ(rfh.get(), web_contents()->GetPrimaryMainFrame());
  auto new_dnt = rfh->GetDevToolsNavigationToken();

  // The devtools_navigation_token should not change after a same-document
  // navigation.
  EXPECT_EQ(dnt, new_dnt);
}

// Tests that a BFCached RFH preserves its devtools_navigation_token
// after being restored.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       DevToolsNavigationToken_BFCacheNavigation) {
  // This test specifically wants to test with BackForwardCache enabled, so skip
  // it if BackForwardCache is disabled.
  if (!IsBackForwardCacheEnabled()) {
    LOG(ERROR) << "Skipping test because BackForwardCache is not enabled.";
    return;
  }
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());

  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(web_contents()->GetPrimaryMainFrame());
  auto dnt = rfh_a->GetDevToolsNavigationToken();
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImplWrapper rfh_b(web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(rfh_a->IsInBackForwardCache());
  ASSERT_NE(rfh_a.get(), rfh_b.get());
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ASSERT_EQ(rfh_a.get(), web_contents()->GetPrimaryMainFrame());

  // |rfh_a|'s devtools_navigation_token should not have changed after being
  // restored from the BFCache.
  EXPECT_EQ(rfh_a->GetDevToolsNavigationToken(), dnt);
}

// Tests that the devtools_navigation_token remains null after a synchronous
// about:blank navigation in an iframe.
IN_PROC_BROWSER_TEST_F(
    RenderFrameHostImplBrowserTest,
    DevToolsNavigationToken_SynchronousAboutBlankNavigation) {
  GURL url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), url));
  RenderFrameHostImplWrapper subframe(
      CreateSubframe(web_contents(), "", GURL(), true));
  ASSERT_TRUE(subframe->is_initial_empty_document());
  // Synchronous about:blank navigations are not strictly considered to be
  // "same-document" navigations; but the synchronously committed about:blank
  // document is still considered to be the initial empty document, and the
  // devtools_navigation_token remains null.
  EXPECT_EQ(subframe->GetDevToolsNavigationToken(), std::nullopt);
}

// Tests that the devtools_navigation_token of an RFH is updated after a
// navigation from a crashed RFH. In particular, this code tries to test the
// early-commit-of-speculative-rfh code path.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       DevToolsNavigationToken_EarlyCommitAfterCrash) {
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));

  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(web_contents()->GetPrimaryMainFrame());
  auto dnt_a = rfh_a->GetDevToolsNavigationToken();
  RenderProcessHost* process = rfh_a->GetProcess();
  RenderProcessHostWatcher process_exit_observer(
      process, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  process->Shutdown(RESULT_CODE_KILLED);
  process_exit_observer.Wait();

  ASSERT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImplWrapper rfh_b(web_contents()->GetPrimaryMainFrame());
  auto dnt_b = rfh_b->GetDevToolsNavigationToken();

  EXPECT_NE(dnt_a, dnt_b);
}

class RenderFrameHostImplNewProcessUsedBrowserTest
    : public RenderFrameHostImplBrowserTest {
 public:
  RenderFrameHostImplNewProcessUsedBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(
        features::kProcessPerSiteUpToMainFrameThreshold);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    RenderFrameHostImplNewProcessUsedBrowserTest,
    RecordNewProcessUsedForNavigationWhenSameSiteProcessExists_SameSite) {
  base::HistogramTester histogram;
  GURL url = embedded_test_server()->GetURL("a.com", "/title1.html");
  GURL second_shell_start_url =
      embedded_test_server()->GetURL("start.test", "/title1.html");

  ASSERT_TRUE(NavigateToURL(shell(), url));

  // Navigation from the initial empty RFH does not count.
  histogram.ExpectTotalCount(
      "SiteIsolation.NewProcessUsedForNavigationWhenSameSiteProcessExists", 0);

  Shell* second_shell =
      Shell::CreateNewWindow(shell()->web_contents()->GetBrowserContext(),
                             second_shell_start_url, nullptr, gfx::Size());
  ASSERT_TRUE(NavigateToURL(second_shell, url));
  ASSERT_NE(shell()->web_contents()->GetPrimaryMainFrame()->GetProcess(),
            second_shell->web_contents()->GetPrimaryMainFrame()->GetProcess());

  // `shell()` and `second_shell` opened the same site.
  EXPECT_THAT(
      histogram.GetAllSamples(
          "SiteIsolation.NewProcessUsedForNavigationWhenSameSiteProcessExists"),
      testing::ElementsAre(base::Bucket(true, 1)));
}

IN_PROC_BROWSER_TEST_F(
    RenderFrameHostImplNewProcessUsedBrowserTest,
    RecordNewProcessUsedForNavigationWhenSameSiteProcessExists_OtherSiteToSameSite) {
  base::HistogramTester histogram;
  GURL url = embedded_test_server()->GetURL("a.com", "/title1.html");
  GURL second_shell_start_url =
      embedded_test_server()->GetURL("start.test", "/title1.html");
  GURL other_url = embedded_test_server()->GetURL("b.com", "/title1.html");

  ASSERT_TRUE(NavigateToURL(shell(), url));

  // Navigation from the initial empty RFH does not count.
  histogram.ExpectTotalCount(
      "SiteIsolation.NewProcessUsedForNavigationWhenSameSiteProcessExists", 0);

  Shell* second_shell =
      Shell::CreateNewWindow(shell()->web_contents()->GetBrowserContext(),
                             second_shell_start_url, nullptr, gfx::Size());
  ASSERT_TRUE(NavigateToURL(second_shell, other_url));
  ASSERT_NE(shell()->web_contents()->GetPrimaryMainFrame()->GetProcess(),
            second_shell->web_contents()->GetPrimaryMainFrame()->GetProcess());

  bool requires_dedicated_process = second_shell->web_contents()
                                        ->GetPrimaryMainFrame()
                                        ->GetSiteInstance()
                                        ->RequiresDedicatedProcess();

  // `shell()` and `second_shell` opened different sites.
  if (requires_dedicated_process) {
    EXPECT_THAT(histogram.GetAllSamples(
                    "SiteIsolation."
                    "NewProcessUsedForNavigationWhenSameSiteProcessExists"),
                testing::ElementsAre(base::Bucket(false, 1)));
  } else {
    EXPECT_THAT(histogram.GetAllSamples(
                    "SiteIsolation."
                    "NewProcessUsedForNavigationWhenSameSiteProcessExists"),
                testing::ElementsAre(base::Bucket(true, 1)));
  }

  ASSERT_TRUE(NavigateToURL(second_shell, url));
  // Now `shell()` and `second_shell` opened the same site.
  if (requires_dedicated_process) {
    EXPECT_THAT(
        histogram.GetAllSamples(
            "SiteIsolation."
            "NewProcessUsedForNavigationWhenSameSiteProcessExists"),
        testing::ElementsAre(base::Bucket(false, 1), base::Bucket(true, 1)));
  } else {
    EXPECT_THAT(histogram.GetAllSamples(
                    "SiteIsolation."
                    "NewProcessUsedForNavigationWhenSameSiteProcessExists"),
                testing::ElementsAre(base::Bucket(true, 2)));
  }
}

// TODO(crbug.com/40264958): Consider enabling this test on Android.
// There is no plan to analyze the histogram on Android for now.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(
    RenderFrameHostImplNewProcessUsedBrowserTest,
    RecordNewProcessUsedForNavigationWhenSameSiteProcessExists_DifferentProfile) {
  base::HistogramTester histogram;
  GURL url = embedded_test_server()->GetURL("a.com", "/title1.html");
  GURL second_shell_start_url =
      embedded_test_server()->GetURL("start.test", "/title1.html");

  ASSERT_TRUE(NavigateToURL(shell(), url));

  // Navigation from the initial empty RFH does not count.
  histogram.ExpectTotalCount(
      "SiteIsolation.NewProcessUsedForNavigationWhenSameSiteProcessExists", 0);

  Shell* second_shell = Shell::CreateNewWindow(
      ShellContentBrowserClient::Get()->off_the_record_browser_context(),
      second_shell_start_url, nullptr, gfx::Size());
  ASSERT_TRUE(NavigateToURL(second_shell, url));
  ASSERT_NE(shell()->web_contents()->GetPrimaryMainFrame()->GetProcess(),
            second_shell->web_contents()->GetPrimaryMainFrame()->GetProcess());

  // `shell()` and `second_shell` opened the same site but use different
  // profiles.
  EXPECT_THAT(
      histogram.GetAllSamples(
          "SiteIsolation.NewProcessUsedForNavigationWhenSameSiteProcessExists"),
      testing::ElementsAre(base::Bucket(false, 1)));
}
#endif

IN_PROC_BROWSER_TEST_F(
    RenderFrameHostImplNewProcessUsedBrowserTest,
    RecordNewProcessUsedForNavigationWhenSameSiteProcessExists_SameSiteNavigateTwice) {
  base::HistogramTester histogram;
  GURL url = embedded_test_server()->GetURL("a.com", "/title1.html");
  GURL url2 = embedded_test_server()->GetURL("a.com", "/title2.html");
  GURL second_shell_start_url =
      embedded_test_server()->GetURL("start.test", "/title1.html");

  ASSERT_TRUE(NavigateToURL(shell(), url));

  // Navigation from the initial empty RFH does not count.
  histogram.ExpectTotalCount(
      "SiteIsolation.NewProcessUsedForNavigationWhenSameSiteProcessExists", 0);

  Shell* second_shell =
      Shell::CreateNewWindow(shell()->web_contents()->GetBrowserContext(),
                             second_shell_start_url, nullptr, gfx::Size());
  ASSERT_TRUE(NavigateToURL(second_shell, url));
  ASSERT_NE(shell()->web_contents()->GetPrimaryMainFrame()->GetProcess(),
            second_shell->web_contents()->GetPrimaryMainFrame()->GetProcess());

  // `shell()` and `second_shell` opened the same site.
  EXPECT_THAT(
      histogram.GetAllSamples(
          "SiteIsolation.NewProcessUsedForNavigationWhenSameSiteProcessExists"),
      testing::ElementsAre(base::Bucket(true, 1)));

  ASSERT_TRUE(NavigateToURL(second_shell, url2));
  // Navigating the different page in the same site shouldn't count up
  // histograms.
  EXPECT_THAT(
      histogram.GetAllSamples(
          "SiteIsolation.NewProcessUsedForNavigationWhenSameSiteProcessExists"),
      testing::ElementsAre(base::Bucket(true, 1)));
}

// Tests that if a shutdown BeforeUnload ACK is received when a navigation has
// picked its final RenderFrameHost, both the RenderFrameHost and navigation
// gets destructed.
// Regression test for crbug.com/349065727.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTest,
                       BeforeUnloadACKWithOngoingNavigation) {
  GURL url = embedded_test_server()->GetURL("a.com", "/title1.html");
  GURL url2 = embedded_test_server()->GetURL("b.com", "/title2.html");
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  // Navigate to an initial page.
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Set up a throttle for the next navigation to simulate a shutdown
  // BeforeUnload ACK from OnWillProcessResponse, after a final RenderFrameHost
  // has been picked.
  TestNavigationThrottleInserter throttle_inserter(
      shell()->web_contents(),
      base::BindLambdaForTesting(
          [&](NavigationHandle* handle) -> std::unique_ptr<NavigationThrottle> {
            auto throttle = std::make_unique<TestNavigationThrottle>(handle);
            throttle->SetCallback(
                TestNavigationThrottle::WILL_PROCESS_RESPONSE,
                base::BindLambdaForTesting([&]() {
                  // Simulate a shutdown BeforeUnload ACK that can happen if
                  // e.g. the tab is being closed. Note that this beforeunload
                  // is not triggered by the navigation itself (a navigation
                  // beforeunload will call `Navigator::BeforeUnloadCompleted()`
                  // instead). The shutdown beforeunload ACK will trigger the
                  // deletion of the speculative RenderFrameHost &
                  // NavigationRequest, so do it asynchronously to not crash the
                  // TestNavigationThrottle.
                  GetUIThreadTaskRunner({})->PostTask(
                      FROM_HERE, base::BindLambdaForTesting([&]() {
                        // Check that the NavigationRequest has picked its final
                        // RenderFrameHost.
                        EXPECT_TRUE(root->navigation_request());
                        EXPECT_TRUE(
                            root->navigation_request()->HasRenderFrameHost());
                        // Simulate the BeforeUnload ACK.
                        root->render_manager()->BeforeUnloadCompleted(
                            /*proceed=*/true);
                        // Ensure that the NavigationRequest and speculative
                        // RenderFrameHost has been cleared.
                        EXPECT_FALSE(
                            root->render_manager()->speculative_frame_host());
                        EXPECT_FALSE(root->navigation_request());
                      }));
                }));
            return throttle;
          }));

  // Navigate to another page, which will be cancelled by the shutdown
  // BeforeUnload ACK above.
  TestNavigationManager navigation_manager(web_contents(), url2);
  shell()->LoadURL(url2);

  // A speculative RFH will be created if needed.
  if (AreAllSitesIsolatedForTesting() || IsBackForwardCacheEnabled() ||
      root->current_frame_host()
          ->ShouldChangeRenderFrameHostOnSameSiteNavigation()) {
    navigation_manager.WaitForSpeculativeRenderFrameHostCreation();
    EXPECT_TRUE(root->render_manager()->speculative_frame_host());
  } else {
    EXPECT_TRUE(navigation_manager.WaitForRequestStart());
    EXPECT_FALSE(root->render_manager()->speculative_frame_host());
    navigation_manager.ResumeNavigation();
  }

  // The NavigationRequest got deleted before commit because of the
  // BeforeUnload ACK, along with the speculative RenderFrameHost (if it was
  // created).
  EXPECT_TRUE(navigation_manager.WaitForNavigationFinished());
  EXPECT_FALSE(navigation_manager.was_committed());
  EXPECT_FALSE(root->render_manager()->speculative_frame_host());
}

// Intercept calls to RenderFramHostImpl's CreateNewWindow mojo method to
// overwrite the target url.
class FrameHostInterceptorForPopins
    : public mojom::FrameHostInterceptorForTesting {
 public:
  explicit FrameHostInterceptorForPopins(RenderFrameHostImpl* render_frame_host,
                                         const GURL& target_url)
      : swapped_impl_(render_frame_host->frame_host_receiver_for_testing(),
                      this),
        target_url_(target_url) {}

  ~FrameHostInterceptorForPopins() override = default;

  FrameHostInterceptorForPopins(const FrameHostInterceptorForPopins&) = delete;
  FrameHostInterceptorForPopins& operator=(
      const FrameHostInterceptorForPopins&) = delete;

  mojom::FrameHost* GetForwardingInterface() override {
    return swapped_impl_.old_impl();
  }

  void CreateNewWindow(mojom::CreateNewWindowParamsPtr params,
                       CreateNewWindowCallback callback) override {
    params->target_url = target_url_;
    static_cast<RenderFrameHostImpl*>(GetForwardingInterface())
        ->CreateNewWindow(std::move(params), std::move(callback));
  }

 private:
  mojo::test::ScopedSwapImplForTesting<mojom::FrameHost> swapped_impl_;
  GURL target_url_;
};

class RenderFrameHostImplPartitionedPopinBrowserTest
    : public RenderFrameHostImplBrowserTest {
 public:
  RenderFrameHostImplPartitionedPopinBrowserTest() {
    feature_list_.InitAndEnableFeature(blink::features::kPartitionedPopins);
  }
  ~RenderFrameHostImplPartitionedPopinBrowserTest() override = default;

 private:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_https_test_server().SetSSLConfig(
        net::EmbeddedTestServer::CERT_TEST_NAMES);
    net::test_server::RegisterDefaultHandlers(&embedded_https_test_server());
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplPartitionedPopinBrowserTest,
                       CreateNewWindow_FailsWithEmptyTargetUrl) {
  FrameHostInterceptorForPopins interceptor(
      static_cast<content::RenderFrameHostImpl*>(
          web_contents()->GetPrimaryMainFrame()),
      GURL());
  GURL url(embedded_https_test_server().GetURL("a.test", "/empty.html"));
  ASSERT_TRUE(NavigateToURL(web_contents(), url));
  ASSERT_EQ(ListValueOf("partitioned"),
            EvalJs(web_contents(), "window.popinContextTypesSupported()"));
  content::RenderProcessHostBadMojoMessageWaiter crash_observer(
      web_contents()->GetPrimaryMainFrame()->GetProcess());
  // ExecJs will sometimes finish before the renderer gets killed, so we must
  // ignore the result.
  std::ignore = ExecJs(web_contents(),
                       "window.open('" + url.spec() + "', '_blank', 'popin');");
  EXPECT_EQ(
      "Received bad user message: "
      "Partitioned popins can only open https URLs.",
      crash_observer.Wait());
  // The test passes if the renderer crashes but not the browser.
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplPartitionedPopinBrowserTest,
                       CreateNewWindow_FailsWithAboutBlankTargetUrl) {
  FrameHostInterceptorForPopins interceptor(
      static_cast<content::RenderFrameHostImpl*>(
          web_contents()->GetPrimaryMainFrame()),
      GURL("about:blank"));
  GURL url(embedded_https_test_server().GetURL("a.test", "/empty.html"));
  ASSERT_TRUE(NavigateToURL(web_contents(), url));
  ASSERT_EQ(ListValueOf("partitioned"),
            EvalJs(web_contents(), "window.popinContextTypesSupported()"));
  content::RenderProcessHostBadMojoMessageWaiter crash_observer(
      web_contents()->GetPrimaryMainFrame()->GetProcess());
  // ExecJs will sometimes finish before the renderer gets killed, so we must
  // ignore the result.
  std::ignore = ExecJs(web_contents(),
                       "window.open('" + url.spec() + "', '_blank', 'popin');");
  EXPECT_EQ(
      "Received bad user message: "
      "Partitioned popins can only open https URLs.",
      crash_observer.Wait());
  // The test passes if the renderer crashes but not the browser.
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplPartitionedPopinBrowserTest,
                       CreateNewWindow_FailsWithHttpTargetUrl) {
  FrameHostInterceptorForPopins interceptor(
      static_cast<content::RenderFrameHostImpl*>(
          web_contents()->GetPrimaryMainFrame()),
      GURL("http://a.test"));
  GURL url(embedded_https_test_server().GetURL("a.test", "/empty.html"));
  ASSERT_TRUE(NavigateToURL(web_contents(), url));
  ASSERT_EQ(ListValueOf("partitioned"),
            EvalJs(web_contents(), "window.popinContextTypesSupported()"));
  content::RenderProcessHostBadMojoMessageWaiter crash_observer(
      web_contents()->GetPrimaryMainFrame()->GetProcess());
  // ExecJs will sometimes finish before the renderer gets killed, so we must
  // ignore the result.
  std::ignore = ExecJs(web_contents(),
                       "window.open('" + url.spec() + "', '_blank', 'popin');");
  EXPECT_EQ(
      "Received bad user message: "
      "Partitioned popins can only open https URLs.",
      crash_observer.Wait());
  // The test passes if the renderer crashes but not the browser.
}

class RenderFrameHostImplBrowserTestWithBFCache
    : public RenderFrameHostImplBrowserTest {
 public:
  RenderFrameHostImplBrowserTestWithBFCache() {
    std::vector<base::test::FeatureRefAndParams> enabled_features =
        GetDefaultEnabledBackForwardCacheFeaturesForTesting(
            /*ignore_outstanding_network_request=*/false);
    scoped_feature_list_.InitWithFeaturesAndParameters(
        enabled_features,
        GetDefaultDisabledBackForwardCacheFeaturesForTesting());
  }
  ~RenderFrameHostImplBrowserTestWithBFCache() override = default;

  viz::SurfaceId GetSurfaceId(const FrameTreeNode* frame_tree_node) const;
  bool ContainsSurfaceIdOrNewer(const std::vector<viz::SurfaceId>& ids,
                                viz::SurfaceId expected_id) const;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

viz::SurfaceId RenderFrameHostImplBrowserTestWithBFCache::GetSurfaceId(
    const FrameTreeNode* frame_tree_node) const {
  viz::SurfaceId id;
  auto* view = static_cast<RenderWidgetHostViewBase*>(
      frame_tree_node->current_frame_host()->GetView());
  if (view) {
    id = view->GetCurrentSurfaceId();
  }
  return id;
}

bool RenderFrameHostImplBrowserTestWithBFCache::ContainsSurfaceIdOrNewer(
    const std::vector<viz::SurfaceId>& ids,
    viz::SurfaceId expected_id) const {
  for (auto id : ids) {
    if (id.IsSameOrNewerThan(expected_id)) {
      return true;
    }
  }
  return false;
}

void RenderFrameHostImplBrowserTestWithBFCache::SetUpCommandLine(
    base::CommandLine* command_line) {
  // The full BackForwardCacheBrowserTest suite ads far more to the CommandLine.
  // While we enable many features in the ctor, we need `--site-per-process` to
  // ensure full testing of OOPIFs in BFCache.
  IsolateAllSitesForTesting(command_line);
  RenderFrameHostImplBrowserTest::SetUpCommandLine(command_line);
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTestWithBFCache,
                       ResetOwnerInBFCache) {
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title2.html"));

  // 1) Navigate to A(B).
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = web_contents()->GetPrimaryMainFrame();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();
  FrameTreeNode* expected_parent_ftn = rfh_a->frame_tree_node();
  FrameTreeNode* expected_child_ftn = rfh_b->frame_tree_node();

  // 2) Navigate to C.
  EXPECT_TRUE(NavigateToURL(shell(), url_c));
  RenderFrameHostImpl* rfh_c = web_contents()->GetPrimaryMainFrame();

  // 3) Ensure A(B) are cached.
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());
  EXPECT_FALSE(rfh_c->IsInBackForwardCache());
  EXPECT_NE(rfh_a, rfh_c);
  EXPECT_EQ(nullptr, rfh_a->owner_);
  EXPECT_EQ(expected_child_ftn, rfh_b->owner_);
  EXPECT_EQ(expected_parent_ftn, rfh_c->owner_);

  // 4) Navigate back to A(B).
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  // 5) Ensure C is cached and A's owner is updated.
  EXPECT_TRUE(rfh_c->IsInBackForwardCache());
  EXPECT_EQ(rfh_a, web_contents()->GetPrimaryMainFrame());
  EXPECT_EQ(expected_parent_ftn, rfh_a->owner_);
  EXPECT_EQ(expected_child_ftn, rfh_b->owner_);
  EXPECT_EQ(nullptr, rfh_c->owner_);
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTestWithBFCache,
                       NewContentTimeoutIsNotSetWhenLeavingBFCacheWithSurface) {
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));

  // Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = web_contents()->GetPrimaryMainFrame();

  // Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  RenderWidgetHostViewBase* rwhvb_a =
      static_cast<RenderWidgetHostViewBase*>(rfh_a->GetView());
  bool timer_should_set =
      !rwhvb_a->GetLocalSurfaceId().is_valid() || rwhvb_a->is_evicted();

  // Ensure there is an activation to allow cross-origin paint holding.
  web_contents()->GetPrimaryMainFrame()->ActivateUserActivation(
      blink::mojom::UserActivationNotificationType::kInteraction,
      /*sticky_only=*/true);

  // Navigate back to A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_EQ(rfh_a, web_contents()->GetPrimaryMainFrame());
  RenderWidgetHostImpl* rwhi_a =
      RenderWidgetHostImpl::From(rfh_a->GetView()->GetRenderWidgetHost());
  EXPECT_EQ(rwhi_a->IsContentRenderingTimeoutRunning(), timer_should_set);
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTestWithBFCache,
                       NewContentTimeoutIsSetWhenLeavingBFCacheWithoutSurface) {
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));

  // Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = web_contents()->GetPrimaryMainFrame();

  // Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  RenderViewHostImpl* rvh_a =
      static_cast<RenderViewHostImpl*>(rfh_a->GetRenderViewHost());
  std::vector<viz::SurfaceId> ids = rvh_a->CollectSurfaceIdsForEviction();

  web_contents()->GetPrimaryMainFrame()->ActivateUserActivation(
      blink::mojom::UserActivationNotificationType::kInteraction,
      /*sticky_only=*/true);

  // Navigate back to A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_EQ(rfh_a, web_contents()->GetPrimaryMainFrame());
  RenderWidgetHostImpl* rwhi_a =
      RenderWidgetHostImpl::From(rfh_a->GetView()->GetRenderWidgetHost());
  EXPECT_TRUE(rwhi_a->IsContentRenderingTimeoutRunning());
}

// Test that the new content timeout only applies when there is a user
// activation, since cross-origin paint holding does not apply otherwise. See
// also NewContentTimeoutIsSetWhenLeavingBFCacheWithoutSurface and
// crbug.com/40942531.
IN_PROC_BROWSER_TEST_F(
    RenderFrameHostImplBrowserTestWithBFCache,
    IsNotSetWhenLeavingBFCacheWithoutSurfaceOrUserActivation) {
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));

  // Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = web_contents()->GetPrimaryMainFrame();

  // Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  RenderViewHostImpl* rvh_a =
      static_cast<RenderViewHostImpl*>(rfh_a->GetRenderViewHost());
  std::vector<viz::SurfaceId> ids = rvh_a->CollectSurfaceIdsForEviction();

  // Navigate back to A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_EQ(rfh_a, web_contents()->GetPrimaryMainFrame());
  RenderWidgetHostImpl* rwhi_a =
      RenderWidgetHostImpl::From(rfh_a->GetView()->GetRenderWidgetHost());
  // Here we verify that the timeout is not running as a proxy to say that there
  // is no paint holding. It would be nice to directly check this instead.
  EXPECT_FALSE(rwhi_a->IsContentRenderingTimeoutRunning());
}

namespace {

class RenderFrameHostImplBrowserTestWithBFCacheAndViewTransition
    : public RenderFrameHostImplBrowserTestWithBFCache {
 public:
  RenderFrameHostImplBrowserTestWithBFCacheAndViewTransition() {
    std::vector<base::test::FeatureRefAndParams> enabled_features =
        GetDefaultEnabledBackForwardCacheFeaturesForTesting(
            /*ignore_outstanding_network_request=*/false);
    enabled_features.push_back(
        {blink::features::kViewTransitionOnNavigation, {{}}});
    enabled_features.push_back({blink::features::kPageSwapEvent, {{}}});
    scoped_feature_list_.InitWithFeaturesAndParameters(
        enabled_features,
        GetDefaultDisabledBackForwardCacheFeaturesForTesting());

    EnablePixelOutput();
  }
  ~RenderFrameHostImplBrowserTestWithBFCacheAndViewTransition() override =
      default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

bool IsChildFrame(RenderWidgetHostView* view) {
  CHECK(view);
  return static_cast<RenderWidgetHostViewBase*>(view)
      ->IsRenderWidgetHostViewChildFrame();
}

#if BUILDFLAG(IS_ANDROID)
ui::DelegatedFrameHostAndroid* GetDelegatedFrameHost(
    RenderWidgetHostView* view) {
  CHECK(!IsChildFrame(view));
  return static_cast<RenderWidgetHostViewAndroid*>(view)
      ->delegated_frame_host_for_testing();
}
#else
DelegatedFrameHost* GetDelegatedFrameHost(RenderWidgetHostView* view) {
  CHECK(!IsChildFrame(view));
  DelegatedFrameHost* dfh = nullptr;
#if BUILDFLAG(IS_MAC)
  auto* compositor = GetBrowserCompositorMacForTesting(view);
  dfh = compositor->GetDelegatedFrameHost();
#elif BUILDFLAG(IS_IOS)
  auto* compositor = GetBrowserCompositorIOSForTesting(view);
  dfh = compositor->GetDelegatedFrameHost();
#elif defined(USE_AURA)
  dfh = static_cast<RenderWidgetHostViewAura*>(view)
            ->GetDelegatedFrameHostForTesting();
#endif  // BUILDFLAG(IS_MAC)
  return dfh;
}
#endif  // BUILDFLAG(IS_ANDROID)

viz::SurfaceId GetCurrentSurfaceIdOnDelegatedFrameHost(
    RenderWidgetHostView* view) {
  auto* dfh = GetDelegatedFrameHost(view);
  CHECK(dfh);
#if BUILDFLAG(IS_ANDROID)
  return dfh->GetCurrentSurfaceIdForTesting();
#else
  return dfh->GetCurrentSurfaceId();
#endif  // BUILDFLAG(IS_ANDROID)
}

viz::SurfaceId GetFirstSurfaceIdAfterNavigation(RenderWidgetHostView* view) {
  auto* dfh = GetDelegatedFrameHost(view);
  CHECK(dfh);
  return dfh->GetFirstSurfaceIdAfterNavigationForTesting();
}

}  // namespace

// https://crbug.com/1415340: For a page with ViewTransition being restored from
// BFCache, we explicitly set its fallback surface to the current View to avoid
// visual glitches.
IN_PROC_BROWSER_TEST_F(
    RenderFrameHostImplBrowserTestWithBFCacheAndViewTransition,
    NewContentTimeoutIsSetWhenLeavingBFCacheWithViewTransition) {
  // "red_jank_second_pageshow.html" janks the renderer on the second pageshow
  // event.
  const GURL url_red(embedded_test_server()->GetURL(
      "a.com", "/view_transitions/red_jank_second_pageshow.html"));
  const GURL url_green(
      embedded_test_server()->GetURL("a.com", "/view_transitions/green.html"));

  // Navigate to Red.
  ASSERT_TRUE(NavigateToURL(shell(), url_red));
  RenderFrameHostWrapper rfh_red(web_contents()->GetPrimaryMainFrame());
  const auto first_surface_id_after_nav_before_bfcache_restore =
      GetFirstSurfaceIdAfterNavigation(rfh_red->GetView());

  // Navigate to Green.
  ASSERT_TRUE(NavigateToURL(shell(), url_green));
  ASSERT_FALSE(rfh_red.IsDestroyed());
  ASSERT_TRUE(
      static_cast<RenderFrameHostImpl*>(rfh_red.get())->IsInBackForwardCache());
  auto* rwhi_red =
      RenderWidgetHostImpl::From(rfh_red->GetView()->GetRenderWidgetHost());
  // The BFCached `RenderWidgetHostImpl` must have a stopped timer.
  ASSERT_FALSE(rwhi_red->IsContentRenderingTimeoutRunning());
  // Set the timeout to a max value, such that we can guarantee to manually
  // force the timer to fire via
  // `RenderWidgetHostImpl::ForceFirstFrameAfterNavigationTimeout()`. Deflake
  // the tests.
  rwhi_red->SetNewContentRenderingTimeoutForTesting(base::TimeDelta::Max());

  // Ensure the first frame in the green page is rendered, so that the
  // transition is not skipped.
  WaitForCopyableViewInWebContents(web_contents());

  // Navigate back to Red.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ASSERT_EQ(rfh_red.get(), web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(rwhi_red->IsContentRenderingTimeoutRunning());

  const auto first_surface_id_after_nav_after_bfcache_restore =
      GetFirstSurfaceIdAfterNavigation(rfh_red->GetView());
  // The `first_surface_id_after_nav_` of the DelegatedFrameHost{Android}, after
  // the BFCache restore, should have a different value than before.
  ASSERT_NE(first_surface_id_after_nav_after_bfcache_restore,
            first_surface_id_after_nav_before_bfcache_restore);
  // The first call to `DelegatedFrameHost{Android}::EmbedSurface` will set
  // `first_surface_id_after_nav_` to the new current `viz::SurfaceId`; if there
  // are subsequent `EmbedSurface` calls (i.e., Android), the current surface id
  // will be newer than `first_surface_id_after_nav_`.
  ASSERT_TRUE(
      GetCurrentSurfaceIdOnDelegatedFrameHost(rfh_red->GetView())
          .IsSameOrNewerThan(first_surface_id_after_nav_after_bfcache_restore));

  // Manually force the timer to fire, since the timeout is infinity.
  ASSERT_TRUE(rwhi_red->IsContentRenderingTimeoutRunning());
  rwhi_red->ForceFirstFrameAfterNavigationTimeout();
  ASSERT_FALSE(rwhi_red->IsContentRenderingTimeoutRunning());
  ASSERT_EQ(first_surface_id_after_nav_after_bfcache_restore,
            GetDelegatedFrameHost(rfh_red->GetView())
                ->GetFallbackSurfaceIdForTesting());

  // TODO(crbug.com/40278487): If the red page's renderer still hasn't
  // submitted a new frame after the ContentRenderingTimeout is up, we should
  // abort the transition. Expand this test to cover that behavior when we have
  // a way to abort the transition.
}

// Tests that when a RenderFrameHost is stored in BFCache, that the visibility
// of its child frames are also updating. Any OOPIF should stop submitting new
// viz::CompositorFrames while in the cache. Conversely upon being removed from
// the BFCache, and re-navigated to, the OOPIF should once again start
// submitting new frames.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTestWithBFCache,
                       ChildFramesHiddenWhileInBFCache) {
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title2.html"));

  // 1) Navigate to A(B).
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(web_contents()->GetPrimaryMainFrame());
  FrameTreeNode* expected_parent_ftn = rfh_a->frame_tree_node();

  // 2) Navigate OOPIF B to a page with a continuous Compositor-thread
  // animation.
  GURL url_d(embedded_test_server()->GetURL(
      "b.com", "/rwhv_compositing_animation.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(
      ChildFrameAt(shell()->web_contents()->GetPrimaryMainFrame(), 0), url_d));
  RenderFrameHostImplWrapper rfh_b(rfh_a->child_at(0)->current_frame_host());
  RenderFrameSubmissionObserver rfso_d(rfh_b.get());
  FrameTreeNode* expected_child_ftn = rfh_b->frame_tree_node();

  // 3) Navigate to C.
  EXPECT_TRUE(NavigateToURL(shell(), url_c));

  // Even though navigation has completed, there may still be a frame from D
  // that is being processed by Viz, or whose RenderFrameMetadata has yet to
  // arrive and be processed on the Browser UI thread.
  //
  // So we wait until the first frame submitted by C has been produced and the
  // metadata processed on the Browser UI thread. From this point on we expect
  // that there are no new frames submitted by D.
  RenderFrameHostImplWrapper rfh_c(web_contents()->GetPrimaryMainFrame());
  RenderFrameSubmissionObserver rfso_c =
      RenderFrameSubmissionObserver(rfh_c.get());
  rfso_c.WaitForAnyFrameSubmission();
  int post_nav_num_frames = rfso_d.render_frame_count();

  // 4) Ensure A(B) are cached.
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());
  EXPECT_FALSE(rfh_c->IsInBackForwardCache());
  EXPECT_NE(rfh_a.get(), rfh_c.get());
  EXPECT_EQ(nullptr, rfh_a->owner_);
  EXPECT_EQ(expected_child_ftn, rfh_b->owner_);
  EXPECT_EQ(expected_parent_ftn, rfh_c->owner_);

  // Since previous content had continuous animation, wait a while and confirm
  // no further frames were submitted. Let's wait for 10 frames at 60 Hz, to
  // give some time for slower builds.
  {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(160));
    run_loop.Run();
  }
  int post_wait_num_frames = rfso_d.render_frame_count();
  EXPECT_EQ(post_wait_num_frames, post_nav_num_frames);

  // 5) Navigate back to A(B).
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  // 6) Ensure C is cached and A's owner is updated.
  EXPECT_TRUE(rfh_c->IsInBackForwardCache());
  EXPECT_EQ(rfh_a.get(), web_contents()->GetPrimaryMainFrame());
  EXPECT_EQ(expected_parent_ftn, rfh_a->owner_);
  EXPECT_EQ(expected_child_ftn, rfh_b->owner_);
  EXPECT_EQ(nullptr, rfh_c->owner_);

  // Ensure that the OOPIF became visible and submitted a frame. If this times
  // out then D failed to become visible again.
  rfso_d.WaitForAnyFrameSubmission();
}

// Tests that when a RenderFrameHost is stored in BFCache, that when it is
// evicted, that both it and its child frames have their viz::SurfaceIds
// collected for eviction.
IN_PROC_BROWSER_TEST_F(RenderFrameHostImplBrowserTestWithBFCache,
                       EvictionInBFCache) {
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title2.html"));

  // 1) Navigate to A(B).
  EXPECT_TRUE(NavigateToURL(shell(), url_a));

  RenderFrameHostImpl* rfh_a = web_contents()->GetPrimaryMainFrame();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();
  FrameTreeNode* expected_parent_ftn = rfh_a->frame_tree_node();
  FrameTreeNode* expected_child_ftn = rfh_b->frame_tree_node();

  viz::SurfaceId id_a = GetSurfaceId(expected_parent_ftn);
  // On slower machines the Main-thread for the child frame may not have
  // completed blink::RemoteFrameView::UpdateViewportIntersectionsForSubtree
  // which can advance the viz::SurfaceId. The final may be newer than this, but
  // from the same viz::FrameSink and viz::LocalSurfaceId::embed_token, so we
  // cache now anyways.
  viz::SurfaceId id_b = GetSurfaceId(expected_child_ftn);
  EXPECT_TRUE(id_a.is_valid());
  EXPECT_TRUE(id_b.is_valid());
  // Ensure that they are separate surfaces and that site-isolation of test
  // setup has not regressed.
  EXPECT_NE(id_a, id_b);

  // 2) Navigate to C.
  EXPECT_TRUE(NavigateToURL(shell(), url_c));
  RenderFrameHostImpl* rfh_c = web_contents()->GetPrimaryMainFrame();

  // 3) Ensure A(B) are cached.
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());
  EXPECT_FALSE(rfh_c->IsInBackForwardCache());
  EXPECT_NE(rfh_a, rfh_c);
  EXPECT_EQ(nullptr, rfh_a->owner_);
  EXPECT_EQ(expected_child_ftn, rfh_b->owner_);
  EXPECT_EQ(expected_parent_ftn, rfh_c->owner_);

  // Collect the viz::SurfaceIds to evict. We should find both `id_a` and
  // `id_b`.
  RenderViewHostImpl* rvh_a =
      static_cast<RenderViewHostImpl*>(rfh_a->GetRenderViewHost());
  std::vector<viz::SurfaceId> ids = rvh_a->CollectSurfaceIdsForEviction();

  EXPECT_EQ(ids.size(), 2u);
  EXPECT_TRUE(ContainsSurfaceIdOrNewer(ids, id_a));
  EXPECT_TRUE(ContainsSurfaceIdOrNewer(ids, id_b));
}

class RenderFrameHostImplPrerenderBrowserTest
    : public RenderFrameHostImplBrowserTest {
 public:
  RenderFrameHostImplPrerenderBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &RenderFrameHostImplPrerenderBrowserTest::GetWebContents,
            base::Unretained(this))) {}

  test::PrerenderTestHelper& prerender_helper() { return prerender_helper_; }

  WebContents* GetWebContents() { return shell()->web_contents(); }

 private:
  test::PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplPrerenderBrowserTest,
                       KeepPrerenderRFHOwnerAfterActivation) {
  GURL url_a = embedded_test_server()->GetURL("/title1.html");
  GURL prerender_url = embedded_test_server()->GetURL("/title2.html");
  EXPECT_TRUE(NavigateToURL(shell(), url_a));

  RenderFrameHostImpl* rfh_a = web_contents()->GetPrimaryMainFrame();
  FrameTreeNode* expected_ftn = rfh_a->frame_tree_node();

  // Load a page in the prerender.
  FrameTreeNodeId host_id = prerender_helper().AddPrerender(prerender_url);
  RenderFrameHostImpl* prerender_frame_host = static_cast<RenderFrameHostImpl*>(
      prerender_helper().GetPrerenderedMainFrameHost(host_id));

  EXPECT_NE(rfh_a->owner_, prerender_frame_host->owner_);

  // Activate the prerendered page.
  prerender_helper().NavigatePrimaryPage(prerender_url);

  RenderFrameHostImpl* activated_rfh = web_contents()->GetPrimaryMainFrame();
  EXPECT_EQ(prerender_frame_host, activated_rfh);
  EXPECT_EQ(expected_ftn, activated_rfh->owner_);
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplPrerenderBrowserTest,
                       NewContentTimeoutIsNotSetInPrerender) {
  GURL url_a = embedded_test_server()->GetURL("/title1.html");
  GURL prerender_url = embedded_test_server()->GetURL("/title2.html");
  EXPECT_TRUE(NavigateToURL(shell(), url_a));

  // Load a page in the prerender.
  FrameTreeNodeId host_id = prerender_helper().AddPrerender(prerender_url);
  RenderFrameHostImpl* prerender_frame_host = static_cast<RenderFrameHostImpl*>(
      prerender_helper().GetPrerenderedMainFrameHost(host_id));
  EXPECT_TRUE(prerender_frame_host);
  RenderWidgetHostImpl* prerender_rwhi = RenderWidgetHostImpl::From(
      prerender_frame_host->GetView()->GetRenderWidgetHost());
  EXPECT_FALSE(prerender_rwhi->IsContentRenderingTimeoutRunning());

  // Activate the prerendered page.
  prerender_helper().NavigatePrimaryPage(prerender_url);

  RenderFrameHostImpl* activated_rfh = web_contents()->GetPrimaryMainFrame();
  RenderWidgetHostImpl* activated_rwhi = RenderWidgetHostImpl::From(
      activated_rfh->GetView()->GetRenderWidgetHost());
  EXPECT_EQ(activated_rwhi, prerender_rwhi);
  EXPECT_TRUE(activated_rwhi->IsContentRenderingTimeoutRunning());
}

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplPrerenderBrowserTest,
                       ActivationSurfaceRangeIncludesFallback) {
  GURL url_a = embedded_test_server()->GetURL("/title1.html");
  GURL prerender_url = embedded_test_server()->GetURL("/title2.html");
  EXPECT_TRUE(NavigateToURL(shell(), url_a));

  // Load a page in the prerender.
  prerender_helper().AddPrerender(prerender_url);

  RenderWidgetHostViewBase* initial_view =
      RenderWidgetHostImpl::From(web_contents()
                                     ->GetPrimaryMainFrame()
                                     ->GetView()
                                     ->GetRenderWidgetHost())
          ->GetView();

  viz::SurfaceId initial_surface_id = initial_view->GetCurrentSurfaceId();
  EXPECT_TRUE(initial_surface_id.is_valid());

  // Activate the prerendered page.
  prerender_helper().NavigatePrimaryPage(prerender_url);

  RenderWidgetHostViewBase* activated_view =
      RenderWidgetHostImpl::From(web_contents()
                                     ->GetPrimaryMainFrame()
                                     ->GetView()
                                     ->GetRenderWidgetHost())
          ->GetView();

  EXPECT_NE(activated_view, initial_view);

#if defined(USE_AURA)
  DelegatedFrameHost* activated_dfh =
      static_cast<RenderWidgetHostViewAura*>(activated_view)
          ->GetDelegatedFrameHostForTesting();
  viz::SurfaceId fallback_surface_id =
      activated_dfh->GetFallbackSurfaceIdForTesting();
  EXPECT_TRUE(initial_surface_id.IsSameOrNewerThan(fallback_surface_id));
#elif BUILDFLAG(IS_ANDROID)
  ui::DelegatedFrameHostAndroid* activated_dfh =
      static_cast<RenderWidgetHostViewAndroid*>(activated_view)
          ->delegated_frame_host_for_testing();
  viz::SurfaceId fallback_surface_id =
      activated_dfh->GetFallbackSurfaceIdForTesting();
  EXPECT_TRUE(initial_surface_id.IsSameOrNewerThan(fallback_surface_id))
      << initial_surface_id.ToString() << " " << fallback_surface_id.ToString();
#endif
}

using RenderFrameHostImplDeathTest = RenderFrameHostImplBrowserTest;

IN_PROC_BROWSER_TEST_F(RenderFrameHostImplDeathTest,
                       ReloadInPendingDeletionOrBFCache) {
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));

  // Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = web_contents()->GetPrimaryMainFrame();

  // Leave rfh_a in pending deletion state.
  LeaveInPendingDeletionState(rfh_a);

  // Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  EXPECT_TRUE(rfh_a->IsPendingDeletion() || rfh_a->IsInBackForwardCache());
  EXPECT_CHECK_DEATH(rfh_a->Reload());
}

class RenderFrameHostImplUrgentNavigationIPCBrowserTest
    : public RenderFrameHostImplBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  RenderFrameHostImplUrgentNavigationIPCBrowserTest() {
    if (RenderDocumentEnabled()) {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          /*enabled_features=*/
          {{features::kRenderDocument, {{"level", "all-frames"}}}},
          /*disabled_features=*/{});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{features::kRenderDocument});
    }
  }

  ~RenderFrameHostImplUrgentNavigationIPCBrowserTest() override = default;

 private:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Override RenderFrameHostImplBrowserTest command line switches, which
    // aren't needed for this test.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kEnableBlinkFeatures, "SchedulerYield");
  }

  bool RenderDocumentEnabled() { return GetParam(); }

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(RenderFrameHostImplUrgentNavigationIPCBrowserTest,
                       UrgentMessageNavigationIPCs) {
  GURL url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::string script =
      "function spin(howLong) {"
      "  const start = performance.now();"
      "  while (performance.now() - start < howLong);"
      "}"
      "window.onbeforeunload = () => spin(5);"
      "async function runTest() {"
      "  while (true) {"
      "    spin(5);"
      "    await scheduler.yield()"
      "  }"
      "}"
      "runTest()";
  EXPECT_TRUE(
      ExecJs(web_contents(), script, EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // Disable the hang monitor to ensure the beforeunload task is prioritized.
  web_contents()
      ->GetPrimaryMainFrame()
      ->DisableBeforeUnloadHangMonitorForTesting();

  // Reload. The loop shouldn't cause WaitForLoadStop to hang.
  web_contents()->GetController().Reload(ReloadType::NORMAL, false);
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
}

INSTANTIATE_TEST_SUITE_P(All,
                         RenderFrameHostImplUrgentNavigationIPCBrowserTest,
                         /*RenderDocumentEnabled()*/ testing::Bool());

}  // namespace content
