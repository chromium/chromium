// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/site_per_process_browsertest.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cc/base/math_util.h"
#include "cc/input/touch_action.h"
#include "components/input/input_router.h"
#include "components/input/render_widget_host_input_event_router.h"
#include "components/input/switches.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/process_lock.h"
#include "content/browser/process_reuse_policy.h"
#include "content/browser/renderer_host/agent_scheduling_group_host.h"
#include "content/browser/renderer_host/cross_process_frame_connector.h"
#include "content/browser/renderer_host/frame_navigation_entry.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/navigation_entry_restore_context_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/site_info.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/common/frame.mojom-test-utils.h"
#include "content/common/input/actions_parser.h"
#include "content/common/input/synthetic_gesture.h"
#include "content/common/input/synthetic_gesture_target.h"
#include "content/common/input/synthetic_pinch_gesture_params.h"
#include "content/common/input/synthetic_pointer_action.h"
#include "content/common/input/synthetic_tap_gesture.h"
#include "content/common/input/synthetic_touchscreen_pinch_gesture.h"
#include "content/common/renderer.mojom.h"
#include "content/common/renderer_host.mojom-test-utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host_priority_client.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/policy_container_utils.h"
#include "content/public/test/render_frame_host_test_support.h"
#include "content/public/test/test_devtools_protocol_client.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_navigation_throttle.h"
#include "content/public/test/test_navigation_throttle_inserter.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/shell/common/main_frame_counter_test_impl.h"
#include "content/shell/common/shell_switches.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/did_commit_navigation_interceptor.h"
#include "content/test/render_document_feature.h"
#include "ipc/constants.mojom.h"
#include "ipc/ipc_security_test_util.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/url_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/mock_http_cache.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/web_sandbox_flags.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/permissions_policy/origin_with_possible_wildcards.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/common/permissions_policy/policy_value.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-test-utils.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "third_party/blink/public/mojom/frame/frame_replication_state.mojom.h"
#include "third_party/blink/public/mojom/leak_detector/leak_detector.mojom-test-utils.h"
#include "third_party/blink/public/mojom/leak_detector/leak_detector.mojom.h"
#include "third_party/blink/public/mojom/page/widget.mojom-test-utils.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom.h"
#include "ui/display/display_switches.h"
#include "ui/display/screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/blink/blink_features.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/latency/latency_info.h"
#include "ui/native_theme/native_theme_features.h"

#if defined(USE_AURA)
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "ui/aura/window.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "content/browser/android/gesture_listener_manager.h"
#include "content/browser/android/ime_adapter_android.h"
#include "content/browser/renderer_host/input/touch_selection_controller_client_manager_android.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/browser/web_contents/web_contents_view_android.h"
#include "content/public/browser/android/child_process_importance.h"
#include "content/test/mock_overscroll_refresh_handler_android.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/events/android/event_handler_android.h"
#include "ui/events/android/motion_event_android_java.h"
#include "ui/gfx/geometry/point_f.h"
#endif

using ::testing::SizeIs;
using ::testing::WhenSorted;
using ::testing::ElementsAre;

namespace content {

namespace {

void VerifyChildProcessHasMainFrame(
    mojo::Remote<mojom::MainFrameCounterTest>& main_frame_counter,
    bool expected_state) {
  main_frame_counter.FlushForTesting();
  base::test::TestFuture<bool> has_main_frame_future;
  main_frame_counter->HasMainFrame(has_main_frame_future.GetCallback());
  EXPECT_EQ(expected_state, has_main_frame_future.Get());
}

using CrashVisibility = CrossProcessFrameConnector::CrashVisibility;

// Helper function to send a postMessage and wait for a reply message.  The
// |post_message_script| is executed on the |sender_ftn| frame, and the sender
// frame is expected to post |reply_status| from the DOMAutomationController
// when it receives a reply.
void PostMessageAndWaitForReply(FrameTreeNode* sender_ftn,
                                const std::string& post_message_script,
                                const std::string& reply_status) {
  // Subtle: msg_queue needs to be declared before the EvalJs below, or
  // else it might miss the message of interest.  See https://crbug.com/518729.
  DOMMessageQueue msg_queue(sender_ftn->current_frame_host());

  EXPECT_EQ(true, EvalJs(sender_ftn, "(" + post_message_script + ");"));

  std::string status;
  while (msg_queue.WaitForMessage(&status)) {
    if (status == reply_status)
      break;
  }
}

// Helper function to extract and return "window.receivedMessages" from the
// |sender_ftn| frame.  This variable is used in post_message.html to count the
// number of messages received via postMessage by the current window.
int GetReceivedMessages(FrameTreeNode* ftn) {
  return EvalJs(ftn, "window.receivedMessages;").ExtractInt();
}

// Helper function to perform a window.open from the |caller_frame| targeting a
// frame with the specified name.
void NavigateNamedFrame(const ToRenderFrameHost& caller_frame,
                        const GURL& url,
                        const std::string& name) {
  EXPECT_EQ(true, EvalJs(caller_frame,
                         JsReplace("!!window.open($1, $2)", url, name)));
}

// Helper function to generate a click on the given RenderWidgetHost.  The
// mouse event is forwarded directly to the RenderWidgetHost without any
// hit-testing.
void SimulateMouseClick(RenderWidgetHost* rwh, int x, int y) {
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebPointerProperties::Button::kLeft;
  mouse_event.SetPositionInWidget(x, y);
  rwh->ForwardMouseEvent(mouse_event);
}

// Retrieve self.origin for the frame |ftn|.
EvalJsResult GetOriginFromRenderer(FrameTreeNode* ftn) {
  return EvalJs(ftn, "self.origin;");
}

// This observer detects when WebContents receives notification of a user
// gesture having occurred, following a user input event targeted to
// a RenderWidgetHost under that WebContents.
class UserInteractionObserver : public WebContentsObserver {
 public:
  explicit UserInteractionObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents), user_interaction_received_(false) {}

  UserInteractionObserver(const UserInteractionObserver&) = delete;
  UserInteractionObserver& operator=(const UserInteractionObserver&) = delete;

  ~UserInteractionObserver() override {}

  // Retrieve the flag. There is no need to wait on a loop since
  // DidGetUserInteraction() should be called synchronously with the input
  // event processing in the browser process.
  bool WasUserInteractionReceived() { return user_interaction_received_; }

  void Reset() { user_interaction_received_ = false; }

 private:
  // WebContentsObserver
  void DidGetUserInteraction(const blink::WebInputEvent& event) override {
    user_interaction_received_ = true;
  }

  bool user_interaction_received_;
};

// Supports waiting until a WebContents notifies its observers that the visible
// security state changed, and a test-specific condition is true at that time.
class VisibleSecurityStateObserver : public WebContentsObserver {
 public:
  // Invoked at Wait() start and when the visible security state changes.
  // If the callback returns true, stops waiting.
  using ConditionCallback = base::RepeatingCallback<bool(WebContents*)>;

  // Creates a VisibleSecurityStateObserver which will wait until
  // a visible security state change is announced by |web_contents| and
  // |condition_callback| returns true (unless |condition_callback| returns true
  // in Wait() already, when it will not wait at all).
  VisibleSecurityStateObserver(WebContents* web_contents,
                               ConditionCallback condition_callback)
      : WebContentsObserver(web_contents),
        condition_callback_(condition_callback) {}
  ~VisibleSecurityStateObserver() override = default;

  VisibleSecurityStateObserver(const VisibleSecurityStateObserver& other) =
      delete;
  VisibleSecurityStateObserver& operator=(
      const VisibleSecurityStateObserver& other) = delete;

  // If the |condition_callback| passed to the constructor returns true, this
  // returns immediately. Otherwise, blocks until the |web_contents| passed to
  // the constructor notifies about a visible security state change and the
  // |condition_callback| evaluates to true.
  void Wait() {
    if (condition_callback_.Run(web_contents()))
      return;
    run_loop_.Run();
  }

  void DidChangeVisibleSecurityState() override {
    if (condition_callback_.Run(web_contents()))
      run_loop_.Quit();
  }

 private:
  ConditionCallback condition_callback_;
  base::RunLoop run_loop_;
};

// Helper function to focus a frame by sending it a mouse click and then
// waiting for it to become focused.
void FocusFrame(FrameTreeNode* frame) {
  FrameFocusedObserver focus_observer(frame->current_frame_host());
  SimulateMouseClick(frame->current_frame_host()->GetRenderWidgetHost(), 1, 1);
  focus_observer.Wait();
}

bool ConvertJSONToPoint(const std::string& str, gfx::PointF* point) {
  std::optional<base::Value> value = base::JSONReader::Read(str);
  if (!value.has_value())
    return false;
  if (!value->is_dict())
    return false;
  std::optional<double> x = value->GetDict().FindDouble("x");
  std::optional<double> y = value->GetDict().FindDouble("y");
  if (!x.has_value())
    return false;
  if (!y.has_value())
    return false;
  point->set_x(x.value());
  point->set_y(y.value());
  return true;
}

// Helper function to generate a permissions policy for a single feature and a
// list of origins. (Equivalent to the declared policy "feature origin1 origin2
// ...".) If the origins list is empty, it's treated as matches all origins
// (Equivalent to the declared policy "feature *")
blink::ParsedPermissionsPolicyDeclaration
CreateParsedPermissionsPolicyDeclaration(
    blink::mojom::PermissionsPolicyFeature feature,
    const std::vector<GURL>& origins,
    bool match_all_origins = false,
    const std::optional<GURL> self_if_matches = std::nullopt) {
  blink::ParsedPermissionsPolicyDeclaration declaration;

  declaration.feature = feature;
  if (self_if_matches.has_value()) {
    declaration.self_if_matches = url::Origin::Create(*self_if_matches);
  }
  declaration.matches_all_origins = match_all_origins;
  declaration.matches_opaque_src = match_all_origins;

  for (const auto& origin : origins)
    declaration.allowed_origins.emplace_back(
        *blink::OriginWithPossibleWildcards::FromOrigin(
            url::Origin::Create(origin)));

  std::sort(declaration.allowed_origins.begin(),
            declaration.allowed_origins.end());

  return declaration;
}

blink::ParsedPermissionsPolicy CreateParsedPermissionsPolicy(
    const std::vector<blink::mojom::PermissionsPolicyFeature>& features,
    const std::vector<GURL>& origins,
    bool match_all_origins = false,
    const std::optional<GURL> self_if_matches = std::nullopt) {
  blink::ParsedPermissionsPolicy result;
  result.reserve(features.size());
  for (const auto& feature : features)
    result.push_back(CreateParsedPermissionsPolicyDeclaration(
        feature, origins, match_all_origins, self_if_matches));
  return result;
}

blink::ParsedPermissionsPolicy CreateParsedPermissionsPolicyMatchesSelf(
    const std::vector<blink::mojom::PermissionsPolicyFeature>& features,
    const GURL& self_if_matches) {
  return CreateParsedPermissionsPolicy(features, {}, false, self_if_matches);
}

blink::ParsedPermissionsPolicy CreateParsedPermissionsPolicyMatchesAll(
    const std::vector<blink::mojom::PermissionsPolicyFeature>& features) {
  return CreateParsedPermissionsPolicy(features, {}, true);
}

blink::ParsedPermissionsPolicy CreateParsedPermissionsPolicyMatchesNone(
    const std::vector<blink::mojom::PermissionsPolicyFeature>& features) {
  return CreateParsedPermissionsPolicy(features, {});
}

// Check frame depth on node, widget, and process all match expected depth.
void CheckFrameDepth(unsigned int expected_depth, FrameTreeNode* node) {
  EXPECT_EQ(expected_depth, node->current_frame_host()->GetFrameDepth());
  RenderProcessHostPriorityClient::Priority priority =
      node->current_frame_host()->GetRenderWidgetHost()->GetPriority();
  EXPECT_EQ(expected_depth, priority.frame_depth);
  EXPECT_EQ(expected_depth,
            node->current_frame_host()->GetProcess()->GetFrameDepth());
}

void GenerateTapDownGesture(RenderWidgetHost* rwh) {
  blink::WebGestureEvent gesture_tap_down(
      blink::WebGestureEvent::Type::kGestureTapDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests(),
      blink::WebGestureDevice::kTouchscreen);
  gesture_tap_down.is_source_touch_event_set_blocking = true;
  rwh->ForwardGestureEvent(gesture_tap_down);
}

}  // namespace

//
// SitePerProcessBrowserTestBase
//

SitePerProcessBrowserTestBase::SitePerProcessBrowserTestBase() {
#if !BUILDFLAG(IS_ANDROID)
  // TODO(bokan): Needed for scrollability check in
  // FrameOwnerPropertiesPropagationScrolling. crbug.com/662196.
  feature_list_.InitAndDisableFeature(features::kOverlayScrollbar);
#endif
}

std::string SitePerProcessBrowserTestBase::DepictFrameTree(
    FrameTreeNode* node) {
  return visualizer_.DepictFrameTree(node);
}

std::string SitePerProcessBrowserTestBase::WaitForMessageScript(
    const std::string& result_expression) {
  return base::StringPrintf(
      "var onMessagePromise = new Promise(resolve => {"
      "  window.addEventListener('message', function(event) {"
      "    resolve(%s);"
      "  });"
      "});",
      result_expression.c_str());
}

void SitePerProcessBrowserTestBase::SetUpCommandLine(
    base::CommandLine* command_line) {
  IsolateAllSitesForTesting(command_line);

  command_line->AppendSwitch(input::switches::kValidateInputEventStream);
  // Without this, FocusFrame can be flaky. It depends on dispatching input
  // events which can inadventently get dropped.
  command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
}

void SitePerProcessBrowserTestBase::SetUpOnMainThread() {
  host_resolver()->AddRule("*", "127.0.0.1");
  SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
}

void SitePerProcessBrowserTestBase::ForceUpdateViewportIntersection(
    FrameTreeNode* frame_tree_node,
    const blink::mojom::ViewportIntersectionState& intersection_state) {
  frame_tree_node->render_manager()
      ->GetProxyToParent()
      ->cross_process_frame_connector()
      ->UpdateViewportIntersectionInternal(intersection_state, false);
}

void SitePerProcessBrowserTestBase::RunPostedTasks() {
  base::RunLoop loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, loop.QuitClosure());
  loop.Run();
}

// SitePerProcessBrowserTest

SitePerProcessBrowserTest::SitePerProcessBrowserTest() {
  InitAndEnableRenderDocumentFeature(&feature_list_, GetParam());
}

std::string SitePerProcessBrowserTest::GetExpectedOrigin(
    const std::string& host) {
  GURL url = embedded_test_server()->GetURL(host, "/");
  return url::Origin::Create(url).Serialize();
}

// SitePerProcessIgnoreCertErrorsBrowserTest

void SitePerProcessIgnoreCertErrorsBrowserTest::SetUpOnMainThread() {
  SitePerProcessBrowserTest::SetUpOnMainThread();
  mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
}

void SitePerProcessIgnoreCertErrorsBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  SitePerProcessBrowserTest::SetUpCommandLine(command_line);
  mock_cert_verifier_.SetUpCommandLine(command_line);
}

void SitePerProcessIgnoreCertErrorsBrowserTest::
    SetUpInProcessBrowserTestFixture() {
  SitePerProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
}

void SitePerProcessIgnoreCertErrorsBrowserTest::
    TearDownInProcessBrowserTestFixture() {
  SitePerProcessBrowserTest::TearDownInProcessBrowserTestFixture();
  mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
}

// SitePerProcessAutoplayBrowserTest

class SitePerProcessAutoplayBrowserTest : public SitePerProcessBrowserTest {
 public:
  SitePerProcessAutoplayBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SitePerProcessBrowserTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kAutoplayPolicy,
        switches::autoplay::kDocumentUserActivationRequiredPolicy);
  }

  bool AutoplayAllowed(const ToRenderFrameHost& adapter,
                       bool with_user_gesture) {
    return EvalJs(adapter, "attemptPlay();",
                  with_user_gesture ? EXECUTE_SCRIPT_DEFAULT_OPTIONS
                                    : EXECUTE_SCRIPT_NO_USER_GESTURE)
        .ExtractBool();
  }
};

// Certain tests require the speculative RFH to be created before the browser
// receives any data from the server. The delay of creating the RFH is set to 0
// in these tests so that the speculative RFH is created when the request is
// sent.
class SitePerProcessBrowserTestWithoutSpeculativeRFHDelay
    : public SitePerProcessBrowserTest {
 public:
  SitePerProcessBrowserTestWithoutSpeculativeRFHDelay() {
    feature_list_for_defer_speculative_rfh_.InitAndEnableFeatureWithParameters(
        features::kDeferSpeculativeRFHCreation,
        {{"create_speculative_rfh_delay_ms", "0"}});
  }

 private:
  base::test::ScopedFeatureList feature_list_for_defer_speculative_rfh_;
};

// Ensure that navigating subframes in --site-per-process mode works and the
// correct documents are committed.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, CrossSiteIframe) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a,a(a,a(a)))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  TestNavigationObserver observer(shell()->web_contents());

  // Load same-site page into iframe.
  FrameTreeNode* child = root->child_at(0);
  GURL http_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(child, http_url));
  EXPECT_EQ(http_url, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());
  {
    // There should be only one RenderWidgetHost when there are no
    // cross-process iframes.
    std::set<RenderWidgetHostViewBase*> views_set =
        web_contents()->GetRenderWidgetHostViewsInWebContentsTree();
    EXPECT_EQ(1U, views_set.size());
  }

  EXPECT_EQ(
      " Site A\n"
      "   |--Site A\n"
      "   +--Site A\n"
      "        |--Site A\n"
      "        +--Site A\n"
      "             +--Site A\n"
      "Where A = http://a.com/",
      DepictFrameTree(root));

  // Load cross-site page into iframe.
  GURL url = embedded_test_server()->GetURL("foo.com", "/title2.html");
  {
    RenderFrameDeletedObserver deleted_observer(child->current_frame_host());
    EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), url));
    deleted_observer.WaitUntilDeleted();
  }
  // Verify that the navigation succeeded and the expected URL was loaded.
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(url, observer.last_navigation_url());

  // Ensure that we have created a new process for the subframe.
  ASSERT_EQ(2U, root->child_count());
  SiteInstance* site_instance = child->current_frame_host()->GetSiteInstance();
  RenderViewHost* rvh = child->current_frame_host()->render_view_host();
  RenderProcessHost* rph = child->current_frame_host()->GetProcess();
  EXPECT_NE(shell()->web_contents()->GetPrimaryMainFrame()->GetRenderViewHost(),
            rvh);
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(), site_instance);
  EXPECT_NE(shell()->web_contents()->GetPrimaryMainFrame()->GetProcess(), rph);
  {
    // There should be now two RenderWidgetHosts, one for each process
    // rendering a frame.
    std::set<RenderWidgetHostViewBase*> views_set =
        web_contents()->GetRenderWidgetHostViewsInWebContentsTree();
    EXPECT_EQ(2U, views_set.size());
  }
  mojo::Remote<mojom::MainFrameCounterTest> main_frame_counter;
  shell()->web_contents()->GetPrimaryMainFrame()->GetProcess()->BindReceiver(
      main_frame_counter.BindNewPipeAndPassReceiver());

  VerifyChildProcessHasMainFrame(main_frame_counter, true);

  mojo::Remote<mojom::MainFrameCounterTest> main_frame_counter_child;
  rph->BindReceiver(main_frame_counter_child.BindNewPipeAndPassReceiver());

  VerifyChildProcessHasMainFrame(main_frame_counter_child, false);

  RenderFrameProxyHost* proxy_to_parent =
      child->render_manager()->GetProxyToParent();
  EXPECT_TRUE(proxy_to_parent);
  EXPECT_TRUE(proxy_to_parent->cross_process_frame_connector());
  // The out-of-process iframe should have its own RenderWidgetHost,
  // independent of any RenderViewHost.
  EXPECT_NE(
      rvh->GetWidget()->GetView(),
      proxy_to_parent->cross_process_frame_connector()->get_view_for_testing());
  EXPECT_TRUE(child->current_frame_host()->GetRenderWidgetHost());

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   +--Site A ------- proxies for B\n"
      "        |--Site A -- proxies for B\n"
      "        +--Site A -- proxies for B\n"
      "             +--Site A -- proxies for B\n"
      "Where A = http://a.com/\n"
      "      B = http://foo.com/",
      DepictFrameTree(root));

  // Load another cross-site page into the same iframe.
  url = embedded_test_server()->GetURL("bar.com", "/title3.html");
  {
    RenderFrameDeletedObserver deleted_observer(child->current_frame_host());
    EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), url));
    deleted_observer.WaitUntilDeleted();
  }
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(url, observer.last_navigation_url());

  // Check again that a new process is created and is different from the
  // top level one and the previous one.
  ASSERT_EQ(2U, root->child_count());
  child = root->child_at(0);
  EXPECT_NE(shell()->web_contents()->GetPrimaryMainFrame()->GetRenderViewHost(),
            child->current_frame_host()->render_view_host());
  EXPECT_NE(rvh, child->current_frame_host()->render_view_host());
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_NE(site_instance,
            child->current_frame_host()->GetSiteInstance());
  EXPECT_NE(shell()->web_contents()->GetPrimaryMainFrame()->GetProcess(),
            child->current_frame_host()->GetProcess());
  EXPECT_NE(rph, child->current_frame_host()->GetProcess());
  VerifyChildProcessHasMainFrame(main_frame_counter, true);
  {
    std::set<RenderWidgetHostViewBase*> views_set =
        web_contents()->GetRenderWidgetHostViewsInWebContentsTree();
    EXPECT_EQ(2U, views_set.size());
  }
  EXPECT_EQ(proxy_to_parent, child->render_manager()->GetProxyToParent());
  EXPECT_TRUE(proxy_to_parent->cross_process_frame_connector());
  EXPECT_NE(
      child->current_frame_host()->render_view_host()->GetWidget()->GetView(),
      proxy_to_parent->cross_process_frame_connector()->get_view_for_testing());
  EXPECT_TRUE(child->current_frame_host()->GetRenderWidgetHost());

  EXPECT_EQ(
      " Site A ------------ proxies for C\n"
      "   |--Site C ------- proxies for A\n"
      "   +--Site A ------- proxies for C\n"
      "        |--Site A -- proxies for C\n"
      "        +--Site A -- proxies for C\n"
      "             +--Site A -- proxies for C\n"
      "Where A = http://a.com/\n"
      "      C = http://bar.com/",
      DepictFrameTree(root));
}

// Ensure that processes for iframes correctly track whether or not they have a
// local main frame.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       CrossSiteIframeMainFrameCount) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a,a,a(a,a))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  TestNavigationObserver observer(shell()->web_contents());

  EXPECT_EQ(
      " Site A\n"
      "   |--Site A\n"
      "   |--Site A\n"
      "   +--Site A\n"
      "        |--Site A\n"
      "        +--Site A\n"
      "Where A = http://a.com/",
      DepictFrameTree(root));

  mojo::Remote<mojom::MainFrameCounterTest> main_frame_counter;
  shell()->web_contents()->GetPrimaryMainFrame()->GetProcess()->BindReceiver(
      main_frame_counter.BindNewPipeAndPassReceiver());
  VerifyChildProcessHasMainFrame(main_frame_counter, true);

  GURL url = embedded_test_server()->GetURL(
      "b.com", "/cross_site_iframe_factory.html?b(a,a)");
  {
    RenderFrameDeletedObserver deleted_observer(
        root->child_at(2)->current_frame_host());
    EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(2), url));
    deleted_observer.WaitUntilDeleted();
  }

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site A ------- proxies for B\n"
      "   |--Site A ------- proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "        |--Site A -- proxies for B\n"
      "        +--Site A -- proxies for B\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  VerifyChildProcessHasMainFrame(main_frame_counter, true);

  mojo::Remote<mojom::MainFrameCounterTest> main_frame_counter_child;
  root->child_at(2)->current_frame_host()->GetProcess()->BindReceiver(
      main_frame_counter_child.BindNewPipeAndPassReceiver());
  VerifyChildProcessHasMainFrame(main_frame_counter_child, false);
}

// Ensure that title updates affect the correct NavigationEntry after a new
// subframe navigation with an out-of-process iframe.  https://crbug.com/616609.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, TitleAfterCrossSiteIframe) {
  // Start at an initial page.
  GURL initial_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), initial_url));

  // Navigate to a same-site page with a same-site iframe.
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  // Make the main frame update its title after the subframe loads.
  EXPECT_TRUE(ExecJs(shell()->web_contents(),
                     "document.querySelector('iframe').onload = "
                     "    function() { document.title = 'loaded'; };"));
  EXPECT_TRUE(
      ExecJs(shell()->web_contents(), "document.title = 'not loaded';"));
  std::u16string expected_title(u"loaded");
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);

  // Navigate the iframe cross-site.
  TestNavigationObserver load_observer(shell()->web_contents());
  GURL frame_url = embedded_test_server()->GetURL("b.com", "/title2.html");
  EXPECT_TRUE(ExecJs(root->child_at(0)->current_frame_host(),
                     JsReplace("window.location.href = $1", frame_url)));
  load_observer.Wait();

  // Wait for the title to update and ensure it affects the right NavEntry.
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  NavigationEntry* entry =
      shell()->web_contents()->GetController().GetLastCommittedEntry();
  EXPECT_EQ(expected_title, entry->GetTitle());
}

// This test verifies that scroll bubbling from an OOPIF properly forwards
// GestureFlingStart events from the child frame to the parent frame. This
// test times out on failure.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       GestureFlingStartEventsBubble) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child_iframe_node = root->child_at(0);

  RenderWidgetHost* child_rwh =
      child_iframe_node->current_frame_host()->GetRenderWidgetHost();

  // The fling start won't bubble since its corresponding GSB hasn't bubbled.
  InputEventAckWaiter gesture_fling_start_ack_observer(
      child_rwh, blink::WebInputEvent::Type::kGestureFlingStart);

  WaitForHitTestData(child_iframe_node->current_frame_host());

  gesture_fling_start_ack_observer.Reset();

  GenerateTapDownGesture(child_rwh);

  // Send a GSB, GSU, GFS sequence and verify that the GFS bubbles.
  blink::WebGestureEvent gesture_scroll_begin(
      blink::WebGestureEvent::Type::kGestureScrollBegin,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests(),
      blink::WebGestureDevice::kTouchscreen);
  gesture_scroll_begin.data.scroll_begin.delta_hint_units =
      ui::ScrollGranularity::kScrollByPrecisePixel;
  gesture_scroll_begin.data.scroll_begin.delta_x_hint = 0.f;
  gesture_scroll_begin.data.scroll_begin.delta_y_hint = 5.f;

  child_rwh->ForwardGestureEvent(gesture_scroll_begin);

  blink::WebGestureEvent gesture_scroll_update(
      blink::WebGestureEvent::Type::kGestureScrollUpdate,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests(),
      blink::WebGestureDevice::kTouchscreen);
  gesture_scroll_update.data.scroll_update.delta_units =
      ui::ScrollGranularity::kScrollByPrecisePixel;
  gesture_scroll_update.data.scroll_update.delta_x = 0.f;
  gesture_scroll_update.data.scroll_update.delta_y = 5.f;

  child_rwh->ForwardGestureEvent(gesture_scroll_update);

  blink::WebGestureEvent gesture_fling_start(
      blink::WebGestureEvent::Type::kGestureFlingStart,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests(),
      blink::WebGestureDevice::kTouchscreen);
  gesture_fling_start.data.fling_start.velocity_x = 0.f;
  gesture_fling_start.data.fling_start.velocity_y = 5.f;

  child_rwh->ForwardGestureEvent(gesture_fling_start);

  // We now wait for the fling start event to be acked by the parent
  // frame. If the test fails, then the test times out.
  gesture_fling_start_ack_observer.Wait();
}

// Test that fling on an out-of-process iframe progresses properly.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       TouchscreenGestureFlingStart) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child_iframe_node = root->child_at(0);

  RenderWidgetHost* child_rwh =
      child_iframe_node->current_frame_host()->GetRenderWidgetHost();
  WaitForHitTestData(child_iframe_node->current_frame_host());

  GenerateTapDownGesture(child_rwh);
  // Send a GSB to start scrolling sequence.
  blink::WebGestureEvent gesture_scroll_begin(
      blink::WebGestureEvent::Type::kGestureScrollBegin,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  gesture_scroll_begin.SetSourceDevice(blink::WebGestureDevice::kTouchscreen);
  gesture_scroll_begin.data.scroll_begin.delta_hint_units =
      ui::ScrollGranularity::kScrollByPrecisePixel;
  gesture_scroll_begin.data.scroll_begin.delta_x_hint = 0.f;
  gesture_scroll_begin.data.scroll_begin.delta_y_hint = 5.f;
  child_rwh->ForwardGestureEvent(gesture_scroll_begin);

  // Send a GFS and wait for the ack of the first GSU generated from progressing
  // the fling on the browser.
  InputEventAckWaiter gesture_scroll_update_ack_observer(
      child_rwh, blink::WebInputEvent::Type::kGestureScrollUpdate);
  gesture_scroll_update_ack_observer.Reset();
  blink::WebGestureEvent gesture_fling_start(
      blink::WebGestureEvent::Type::kGestureFlingStart,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  gesture_fling_start.SetSourceDevice(blink::WebGestureDevice::kTouchscreen);
  gesture_fling_start.data.fling_start.velocity_x = 0.f;
  gesture_fling_start.data.fling_start.velocity_y = 50.f;
  child_rwh->ForwardGestureEvent(gesture_fling_start);
  gesture_scroll_update_ack_observer.Wait();
}

// Test that fling on an out-of-process iframe progresses properly.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, TouchpadGestureFlingStart) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child_iframe_node = root->child_at(0);

  RenderWidgetHost* child_rwh =
      child_iframe_node->current_frame_host()->GetRenderWidgetHost();

  // Send a wheel event with phaseBegan to start scrolling sequence.
  InputEventAckWaiter gesture_scroll_begin_ack_observer(
      child_rwh, blink::WebInputEvent::Type::kGestureScrollBegin);
  blink::WebMouseWheelEvent scroll_event(
      blink::WebInputEvent::Type::kMouseWheel,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  scroll_event.delta_units = ui::ScrollGranularity::kScrollByPrecisePixel;
  scroll_event.delta_x = 0.0f;
  scroll_event.delta_y = 5.0f;
  scroll_event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
  child_rwh->ForwardWheelEvent(scroll_event);
  gesture_scroll_begin_ack_observer.Wait();

  // Send a GFS and wait for the ack of the first GSU generated from progressing
  // the fling on the browser.
  InputEventAckWaiter gesture_scroll_update_ack_observer(
      child_rwh, blink::WebInputEvent::Type::kGestureScrollUpdate);
  gesture_scroll_update_ack_observer.Reset();
  blink::WebGestureEvent gesture_fling_start(
      blink::WebGestureEvent::Type::kGestureFlingStart,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  gesture_fling_start.SetSourceDevice(blink::WebGestureDevice::kTouchpad);
  gesture_fling_start.data.fling_start.velocity_x = 0.f;
  gesture_fling_start.data.fling_start.velocity_y = 50.f;
  child_rwh->ForwardGestureEvent(gesture_fling_start);
  // The test will pass when the GSU ack arrives, since it shows that the fling
  // controller has properly generated a GSU event from progressing the fling.
  gesture_scroll_update_ack_observer.Wait();
}

// Tests OOPIF rendering by checking that the RWH of the iframe generates
// OnSwapCompositorFrame message.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, CompositorFrameSwapped) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(baz)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child_node = root->child_at(0);
  GURL site_url(embedded_test_server()->GetURL(
      "baz.com", "/cross_site_iframe_factory.html?baz()"));
  EXPECT_EQ(site_url, child_node->current_url());
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            child_node->current_frame_host()->GetSiteInstance());
  // Wait for CompositorFrame submission.
  RenderFrameSubmissionObserver observer(
      child_node->current_frame_host()
          ->GetRenderWidgetHost()
          ->render_frame_metadata_provider());
  observer.WaitForAnyFrameSubmission();
}

// Ensure that OOPIFs are deleted after navigating to a new main frame.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, CleanupCrossSiteIframe) {
  // The test assumes the previous page gets deleted after navigation. Disable
  // back-forward cache to ensure that it doesn't get preserved in the cache.
  DisableBackForwardCacheForTesting(
      web_contents(), content::BackForwardCache::TEST_REQUIRES_NO_CACHING);
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a,a(a,a(a)))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  TestNavigationObserver observer(shell()->web_contents());

  // Load a cross-site page into both iframes.
  GURL foo_url = embedded_test_server()->GetURL("foo.com", "/title2.html");
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), foo_url));
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(foo_url, observer.last_navigation_url());
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(1), foo_url));
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(foo_url, observer.last_navigation_url());

  // Ensure that we have created a new process for the subframes.
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://foo.com/",
      DepictFrameTree(root));

  int subframe_process_id = root->child_at(0)
                                ->current_frame_host()
                                ->GetSiteInstance()
                                ->GetProcess()
                                ->GetID();
  int subframe_rvh_id = root->child_at(0)
                            ->current_frame_host()
                            ->render_view_host()
                            ->GetRoutingID();
  EXPECT_TRUE(RenderViewHost::FromID(subframe_process_id, subframe_rvh_id));

  // Use Javascript in the parent to remove one of the frames and ensure that
  // the subframe goes away.
  EXPECT_TRUE(ExecJs(shell(),
                     "document.body.removeChild("
                     "document.querySelectorAll('iframe')[0])"));
  ASSERT_EQ(1U, root->child_count());

  // Load a new same-site page in the top-level frame and ensure the other
  // subframe goes away.
  GURL new_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), new_url));
  ASSERT_EQ(0U, root->child_count());

  // Ensure the RVH for the subframe gets cleaned up when the frame goes away.
  EXPECT_FALSE(RenderViewHost::FromID(subframe_process_id, subframe_rvh_id));
}

IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, NavigateRemoteFrame) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a,a(a,a(a)))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  TestNavigationObserver observer(shell()->web_contents());

  // Load same-site page into iframe.
  FrameTreeNode* child = root->child_at(0);
  GURL http_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(child, http_url));
  EXPECT_EQ(http_url, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());

  // Load cross-site page into iframe.
  GURL url = embedded_test_server()->GetURL("foo.com", "/title2.html");
  {
    RenderFrameDeletedObserver deleted_observer(child->current_frame_host());
    EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), url));
    deleted_observer.WaitUntilDeleted();
  }
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(url, observer.last_navigation_url());

  // Ensure that we have created a new process for the subframe.
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   +--Site A ------- proxies for B\n"
      "        |--Site A -- proxies for B\n"
      "        +--Site A -- proxies for B\n"
      "             +--Site A -- proxies for B\n"
      "Where A = http://a.com/\n"
      "      B = http://foo.com/",
      DepictFrameTree(root));
  SiteInstance* site_instance = child->current_frame_host()->GetSiteInstance();
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(), site_instance);

  // Emulate the main frame changing the src of the iframe such that it
  // navigates cross-site.
  url = embedded_test_server()->GetURL("bar.com", "/title3.html");
  {
    RenderFrameDeletedObserver deleted_observer(child->current_frame_host());
    NavigateIframeToURL(shell()->web_contents(), "child-0", url);
    deleted_observer.WaitUntilDeleted();
  }
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(url, observer.last_navigation_url());

  // Check again that a new process is created and is different from the
  // top level one and the previous one.
  EXPECT_EQ(
      " Site A ------------ proxies for C\n"
      "   |--Site C ------- proxies for A\n"
      "   +--Site A ------- proxies for C\n"
      "        |--Site A -- proxies for C\n"
      "        +--Site A -- proxies for C\n"
      "             +--Site A -- proxies for C\n"
      "Where A = http://a.com/\n"
      "      C = http://bar.com/",
      DepictFrameTree(root));

  // Navigate back to the parent's origin and ensure we return to the
  // parent's process.
  {
    RenderFrameDeletedObserver deleted_observer(child->current_frame_host());
    EXPECT_TRUE(NavigateToURLFromRenderer(child, http_url));
    deleted_observer.WaitUntilDeleted();
  }
  EXPECT_EQ(http_url, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(shell()->web_contents()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
}

IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       NavigateRemoteFrameToBlankAndDataURLs) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a,a(a))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  TestNavigationObserver observer(shell()->web_contents());

  // Load same-site page into iframe.
  FrameTreeNode* child = root->child_at(0);
  GURL http_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(child, http_url));
  EXPECT_EQ(http_url, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(
      " Site A\n"
      "   |--Site A\n"
      "   +--Site A\n"
      "        +--Site A\n"
      "Where A = http://a.com/",
      DepictFrameTree(root));

  // Load cross-site page into iframe.
  GURL url = embedded_test_server()->GetURL("foo.com", "/title2.html");
  EXPECT_TRUE(NavigateToURLFromRenderer(child, url));
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(url, observer.last_navigation_url());
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   +--Site A ------- proxies for B\n"
      "        +--Site A -- proxies for B\n"
      "Where A = http://a.com/\n"
      "      B = http://foo.com/",
      DepictFrameTree(root));

  // Navigate iframe to a data URL. The navigation happens from a script in the
  // parent frame, so the data URL should be committed in the same SiteInstance
  // as the parent frame.
  RenderFrameDeletedObserver deleted_observer1(
      root->child_at(0)->current_frame_host());
  GURL data_url("data:text/html,dataurl");
  NavigateIframeToURL(shell()->web_contents(), "child-0", data_url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(data_url, observer.last_navigation_url());

  // Wait for the old process to exit, to verify that the proxies go away.
  deleted_observer1.WaitUntilDeleted();

  // Ensure that we have navigated using the top level process.
  EXPECT_EQ(
      " Site A\n"
      "   |--Site A\n"
      "   +--Site A\n"
      "        +--Site A\n"
      "Where A = http://a.com/",
      DepictFrameTree(root));

  // Load cross-site page into iframe.
  url = embedded_test_server()->GetURL("bar.com", "/title2.html");
  EXPECT_TRUE(NavigateToURLFromRenderer(child, url));
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(url, observer.last_navigation_url());
  EXPECT_EQ(
      " Site A ------------ proxies for C\n"
      "   |--Site C ------- proxies for A\n"
      "   +--Site A ------- proxies for C\n"
      "        +--Site A -- proxies for C\n"
      "Where A = http://a.com/\n"
      "      C = http://bar.com/",
      DepictFrameTree(root));

  // Navigate iframe to about:blank. The navigation happens from a script in the
  // parent frame, so it should be committed in the same SiteInstance as the
  // parent frame.
  RenderFrameDeletedObserver deleted_observer2(
      root->child_at(0)->current_frame_host());
  GURL about_blank_url("about:blank#foo");
  NavigateIframeToURL(shell()->web_contents(), "child-0", about_blank_url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(about_blank_url, observer.last_navigation_url());

  // Wait for the old process to exit, to verify that the proxies go away.
  deleted_observer2.WaitUntilDeleted();

  // Ensure that we have navigated using the top level process.
  EXPECT_EQ(
      " Site A\n"
      "   |--Site A\n"
      "   +--Site A\n"
      "        +--Site A\n"
      "Where A = http://a.com/",
      DepictFrameTree(root));

  // Load cross-site page into iframe again.
  url = embedded_test_server()->GetURL("f00.com", "/title3.html");
  EXPECT_TRUE(NavigateToURLFromRenderer(child, url));
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(url, observer.last_navigation_url());
  EXPECT_EQ(
      " Site A ------------ proxies for D\n"
      "   |--Site D ------- proxies for A\n"
      "   +--Site A ------- proxies for D\n"
      "        +--Site A -- proxies for D\n"
      "Where A = http://a.com/\n"
      "      D = http://f00.com/",
      DepictFrameTree(root));

  // Navigate the iframe itself to about:blank using a script executing in its
  // own context. It should stay in the same SiteInstance as before, not the
  // parent one.
  TestFrameNavigationObserver frame_observer(child);
  EXPECT_TRUE(ExecJs(child, "window.location.href = 'about:blank#foo';"));
  frame_observer.Wait();
  EXPECT_EQ(about_blank_url, child->current_url());

  // Ensure that we have navigated using the top level process.
  EXPECT_EQ(
      " Site A ------------ proxies for D\n"
      "   |--Site D ------- proxies for A\n"
      "   +--Site A ------- proxies for D\n"
      "        +--Site A -- proxies for D\n"
      "Where A = http://a.com/\n"
      "      D = http://f00.com/",
      DepictFrameTree(root));
}

// This test checks that killing a renderer process of a remote frame
// and then navigating some other frame to the same SiteInstance of the killed
// process works properly.
// This can be illustrated as follows,
// where 1/2/3 are FrameTreeNode-s and A/B are processes and B* is the killed
// B process:
//
//     1        A                  A                           A
//    / \  ->  / \  -> Kill B ->  / \  -> Navigate 3 to B ->  / \  .
//   2   3    B   A              B*  A                       B*  B
//
// Initially, node1.proxy_hosts_ = {B}
// After we kill B, we make sure B stays in node1.proxy_hosts_, then we navigate
// 3 to B and we expect that to complete normally.
// See http://crbug.com/432107.
//
// Note that due to http://crbug.com/450681, node2 cannot be re-navigated to
// site B and stays in not rendered state.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       NavigateRemoteFrameToKilledProcess) {
  GURL main_url(embedded_test_server()->GetURL(
      "foo.com", "/cross_site_iframe_factory.html?foo.com(bar.com, foo.com)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  TestNavigationObserver observer(shell()->web_contents());
  ASSERT_EQ(2U, root->child_count());

  // Make sure node2 points to the correct cross-site page.
  GURL site_b_url = embedded_test_server()->GetURL(
      "bar.com", "/cross_site_iframe_factory.html?bar.com()");
  FrameTreeNode* node2 = root->child_at(0);
  EXPECT_EQ(site_b_url, node2->current_url());

  // Kill that cross-site renderer.
  RenderProcessHost* child_process = node2->current_frame_host()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      child_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  child_process->Shutdown(0);
  crash_observer.Wait();

  // Now navigate the second iframe (node3) to the same site as the node2.
  FrameTreeNode* node3 = root->child_at(1);
  EXPECT_TRUE(NavigateToURLFromRenderer(node3, site_b_url));
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(site_b_url, observer.last_navigation_url());
}

// This test ensures that WebContentsImpl::FocusOwningWebContents does not crash
// the browser if the currently focused frame's renderer has disappeared.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, RemoveFocusFromKilledFrame) {
  GURL main_url(embedded_test_server()->GetURL(
      "foo.com", "/cross_site_iframe_factory.html?foo.com(bar.com)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  TestNavigationObserver observer(shell()->web_contents());
  ASSERT_EQ(1U, root->child_count());

  // Make sure node2 points to the correct cross-site page.
  GURL site_b_url = embedded_test_server()->GetURL(
      "bar.com", "/cross_site_iframe_factory.html?bar.com()");
  FrameTreeNode* node2 = root->child_at(0);
  EXPECT_EQ(site_b_url, node2->current_url());

  web_contents()->SetFocusedFrame(
      node2, node2->current_frame_host()->GetSiteInstance()->group());

  // Kill that cross-site renderer.
  RenderProcessHost* child_process = node2->current_frame_host()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      child_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  child_process->Shutdown(0);
  crash_observer.Wait();

  // Try to focus the root's owning WebContents.
  web_contents()->FocusOwningWebContents(
      root->current_frame_host()->GetRenderWidgetHost());
}

// This test is similar to
// SitePerProcessBrowserTest.NavigateRemoteFrameToKilledProcess with
// addition that node2 also has a cross-origin frame to site C.
//
//     1          A                  A                       A
//    / \        / \                / \                     / \  .
//   2   3 ->   B   A -> Kill B -> B*   A -> Navigate 3 -> B*  B
//  /          /
// 4          C
//
// Initially, node1.proxy_hosts_ = {B, C}
// After we kill B, we make sure B stays in node1.proxy_hosts_, but
// C gets cleared from node1.proxy_hosts_.
//
// Note that due to http://crbug.com/450681, node2 cannot be re-navigated to
// site B and stays in not rendered state.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       NavigateRemoteFrameToKilledProcessWithSubtree) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(bar(baz), a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  TestNavigationObserver observer(shell()->web_contents());

  ASSERT_EQ(2U, root->child_count());

  GURL site_b_url(embedded_test_server()->GetURL(
      "bar.com", "/cross_site_iframe_factory.html?bar(baz())"));
  // We can't use a TestNavigationObserver to verify the URL here,
  // since the frame has children that may have clobbered it in the observer.
  EXPECT_EQ(site_b_url, root->child_at(0)->current_url());

  // Ensure that a new process is created for node2.
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            root->child_at(0)->current_frame_host()->GetSiteInstance());
  // Ensure that a new process is *not* created for node3.
  EXPECT_EQ(shell()->web_contents()->GetSiteInstance(),
            root->child_at(1)->current_frame_host()->GetSiteInstance());

  ASSERT_EQ(1U, root->child_at(0)->child_count());

  // Make sure node4 points to the correct cross-site page.
  FrameTreeNode* node4 = root->child_at(0)->child_at(0);
  GURL site_c_url(embedded_test_server()->GetURL(
      "baz.com", "/cross_site_iframe_factory.html?baz()"));
  EXPECT_EQ(site_c_url, node4->current_url());

  // |site_instance_c| is expected to go away once we kill |child_process_b|
  // below, so create a local scope so we can extend the lifetime of
  // |site_instance_c| with a refptr.
  {
    // Initially each frame has proxies for the other sites.
    EXPECT_EQ(
        " Site A ------------ proxies for B C\n"
        "   |--Site B ------- proxies for A C\n"
        "   |    +--Site C -- proxies for A B\n"
        "   +--Site A ------- proxies for B C\n"
        "Where A = http://a.com/\n"
        "      B = http://bar.com/\n"
        "      C = http://baz.com/",
        DepictFrameTree(root));

    // Kill the render process for Site B.
    RenderProcessHost* child_process_b =
        root->child_at(0)->current_frame_host()->GetProcess();
    RenderProcessHostWatcher crash_observer(
        child_process_b, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    child_process_b->Shutdown(0);
    crash_observer.Wait();

    // The Site C frame (a child of the crashed Site B frame) should go away,
    // and there should be no remaining proxies for site C anywhere.
    EXPECT_EQ(
        " Site A ------------ proxies for B\n"
        "   |--Site B ------- proxies for A\n"
        "   +--Site A ------- proxies for B\n"
        "Where A = http://a.com/\n"
        "      B = http://bar.com/ (no process)",
        DepictFrameTree(root));
  }

  // Now navigate the second iframe (node3) to Site B also.
  FrameTreeNode* node3 = root->child_at(1);
  GURL url = embedded_test_server()->GetURL("bar.com", "/title1.html");
  EXPECT_TRUE(NavigateToURLFromRenderer(node3, url));
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(url, observer.last_navigation_url());

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://bar.com/",
      DepictFrameTree(root));
}

// Ensure that the renderer process doesn't crash when the main frame navigates
// a remote child to a page that results in a network error.
// See https://crbug.com/558016.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, NavigateRemoteAfterError) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  // Load same-site page into iframe.
  {
    TestNavigationObserver observer(shell()->web_contents());
    FrameTreeNode* child = root->child_at(0);
    GURL http_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
    EXPECT_TRUE(NavigateToURLFromRenderer(child, http_url));
    EXPECT_EQ(http_url, observer.last_navigation_url());
    EXPECT_TRUE(observer.last_navigation_succeeded());
    observer.Wait();
  }

  // Load cross-site page into iframe.
  {
    TestNavigationObserver observer(shell()->web_contents());
    FrameTreeNode* child = root->child_at(0);
    GURL url = embedded_test_server()->GetURL("foo.com", "/title2.html");
    EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), url));
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_EQ(url, observer.last_navigation_url());
    observer.Wait();

    // Ensure that we have created a new process for the subframe.
    EXPECT_EQ(
        " Site A ------------ proxies for B\n"
        "   +--Site B ------- proxies for A\n"
        "Where A = http://a.com/\n"
        "      B = http://foo.com/",
        DepictFrameTree(root));
    SiteInstance* site_instance =
        child->current_frame_host()->GetSiteInstance();
    EXPECT_NE(shell()->web_contents()->GetSiteInstance(), site_instance);
  }

  // Stop the test server and try to navigate the remote frame.
  {
    GURL url = embedded_test_server()->GetURL("bar.com", "/title3.html");
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    NavigateIframeToURL(shell()->web_contents(), "child-0", url);
  }
}

// Ensure that a cross-site page ends up in the correct process when it
// successfully loads after earlier encountering a network error for it.
// See https://crbug.com/560511.
// TODO(creis): Make the net error page show in the correct process as well,
// per https://crbug.com/588314.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, ProcessTransferAfterError) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);
  GURL url_a = child->current_url();

  // Disable host resolution in the test server and try to navigate the subframe
  // cross-site, which will lead to a committed net error.
  GURL url_b = embedded_test_server()->GetURL("b.com", "/title3.html");
  auto url_loader_interceptor = std::make_unique<URLLoaderInterceptor>(
      base::BindRepeating([](URLLoaderInterceptor::RequestParams* params) {
        network::URLLoaderCompletionStatus status;
        status.error_code = net::ERR_NOT_IMPLEMENTED;
        params->client->OnComplete(status);
        return true;
      }));

  TestNavigationObserver observer(shell()->web_contents());
  NavigateIframeToURL(shell()->web_contents(), "child-0", url_b);
  EXPECT_FALSE(observer.last_navigation_succeeded());
  EXPECT_EQ(url_b, observer.last_navigation_url());
  EXPECT_EQ(2, shell()->web_contents()->GetController().GetEntryCount());

  // Ensure that we have created a new process for the subframe.
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());

  // We have switched RenderFrameHosts for the subframe, so the last successful
  // url should be empty (since the frame only loaded an error page).
  EXPECT_EQ(GURL(), child->current_frame_host()->last_successful_url());
  EXPECT_EQ(url_b, child->current_url());
  EXPECT_EQ("null", child->current_origin().Serialize());

  // Try again after re-enabling host resolution.
  url_loader_interceptor.reset();

  // Activate the root frame by executing a dummy script.
  //
  // TODO(mustaq): Why does the |back_load_observer.Wait()| below time out
  // without the user activation?
  EXPECT_TRUE(ExecJs(root, "// No-op script"));
  NavigateIframeToURL(shell()->web_contents(), "child-0", url_b);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(url_b, observer.last_navigation_url());

  // The FrameTreeNode should have updated its URL and origin.
  EXPECT_EQ(url_b, child->current_frame_host()->last_successful_url());
  EXPECT_EQ(url_b, child->current_url());
  EXPECT_EQ(url_b.DeprecatedGetOriginAsURL().spec(),
            child->current_origin().Serialize() + '/');

  // Ensure that we have created a new process for the subframe.
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());

  // Make sure that the navigation replaced the error page and that going back
  // ends up on the original site.
  EXPECT_EQ(2, shell()->web_contents()->GetController().GetEntryCount());
  {
    RenderFrameDeletedObserver deleted_observer(child->current_frame_host());
    TestNavigationObserver back_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_load_observer.Wait();

    // Wait for the old process to exit, to verify that the proxies go away.
    deleted_observer.WaitUntilDeleted();
  }
  EXPECT_EQ(
      " Site A\n"
      "   +--Site A\n"
      "Where A = http://a.com/",
      DepictFrameTree(root));
  EXPECT_EQ(shell()->web_contents()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_EQ(url_a, child->current_frame_host()->last_successful_url());
  EXPECT_EQ(url_a, child->current_url());
  EXPECT_EQ(url_a.DeprecatedGetOriginAsURL().spec(),
            child->current_origin().Serialize() + '/');
}

// Verify that killing a cross-site frame's process B and then navigating a
// frame to B correctly recreates all proxies in B.
//
//      1           A                    A          A
//    / | \       / | \                / | \      / | \  .
//   2  3  4 ->  B  A  A -> Kill B -> B* A  A -> B* B  A
//
// After the last step, the test sends a postMessage from node 3 to node 4,
// verifying that a proxy for node 4 has been recreated in process B.  This
// verifies the fix for https://crbug.com/478892.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       NavigatingToKilledProcessRestoresAllProxies) {
  // Navigate to a page with three frames: one cross-site and two same-site.
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_three_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  TestNavigationObserver observer(shell()->web_contents());

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   |--Site A ------- proxies for B\n"
      "   +--Site A ------- proxies for B\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  // Kill the first subframe's b.com renderer.
  RenderProcessHost* child_process =
      root->child_at(0)->current_frame_host()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      child_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  child_process->Shutdown(0);
  crash_observer.Wait();

  // Navigate the second subframe to b.com to recreate the b.com process.
  GURL b_url = embedded_test_server()->GetURL("b.com", "/post_message.html");
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(1), b_url));
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(b_url, observer.last_navigation_url());
  EXPECT_TRUE(root->child_at(1)->current_frame_host()->IsRenderFrameLive());

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   |--Site B ------- proxies for A\n"
      "   +--Site A ------- proxies for B\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  // Check that third subframe's proxy is available in the b.com process by
  // sending it a postMessage from second subframe, and waiting for a reply.
  PostMessageAndWaitForReply(root->child_at(1),
                             "postToSibling('subframe-msg','frame3')",
                             "\"done-frame2\"");
}

// Verify that proxy creation doesn't recreate a crashed process if no frame
// will be created in it.
//
//      1           A                    A          A
//    / | \       / | \                / | \      / | \    .
//   2  3  4 ->  B  A  A -> Kill B -> B* A  A -> B* A  A
//                                                      \  .
//                                                       A
//
// The test kills process B (node 2), creates a child frame of node 4 in
// process A, and then checks that process B isn't resurrected to create a
// proxy for the new child frame.  See https://crbug.com/476846.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       CreateChildFrameAfterKillingProcess) {
  // Navigate to a page with three frames: one cross-site and two same-site.
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_three_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   |--Site A ------- proxies for B\n"
      "   +--Site A ------- proxies for B\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));
  SiteInstanceImpl* b_site_instance =
      root->child_at(0)->current_frame_host()->GetSiteInstance();

  // Kill the first subframe's renderer (B).
  RenderProcessHost* child_process =
      root->child_at(0)->current_frame_host()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      child_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  child_process->Shutdown(0);
  crash_observer.Wait();

  // Add a new child frame to the third subframe.
  RenderFrameHostCreatedObserver frame_observer(shell()->web_contents(), 1);
  EXPECT_TRUE(
      ExecJs(root->child_at(2),
             "document.body.appendChild(document.createElement('iframe'));"));
  frame_observer.Wait();

  // The new frame should have a RenderFrameProxyHost for B, but it should not
  // be alive, and B should still not have a process (verified by last line of
  // expected DepictFrameTree output).
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   |--Site A ------- proxies for B\n"
      "   +--Site A ------- proxies for B\n"
      "        +--Site A -- proxies for B\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/ (no process)",
      DepictFrameTree(root));
  FrameTreeNode* grandchild = root->child_at(2)->child_at(0);
  RenderFrameProxyHost* grandchild_rfph =
      grandchild->current_frame_host()
          ->browsing_context_state()
          ->GetRenderFrameProxyHost(b_site_instance->group());
  EXPECT_FALSE(grandchild_rfph->is_render_frame_proxy_live());

  // Navigate the second subframe to b.com to recreate process B.
  TestNavigationObserver observer(shell()->web_contents());
  GURL b_url = embedded_test_server()->GetURL("b.com", "/title1.html");
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(1), b_url));
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(b_url, observer.last_navigation_url());

  // Ensure that the grandchild `blink::RemoteFrame` in B was created when
  // process B was restored.
  EXPECT_TRUE(grandchild_rfph->is_render_frame_proxy_live());
}

// Verify that creating a child frame after killing and reloading an opener
// process doesn't crash. See https://crbug.com/501152.
//   1. Navigate to site A.
//   2. Open a popup with window.open and navigate it cross-process to site B.
//   3. Kill process A for the original tab.
//   4. Reload the original tab to resurrect process A.
//   5. Add a child frame to the top-level frame in the popup tab B.
// In step 5, we try to create proxies for the child frame in all SiteInstances
// for which its parent has proxies.  This includes A.  However, even though
// process A is live (step 4), the parent proxy in A is not live (which was
// incorrectly assumed previously).  This is because step 4 does not resurrect
// proxies for popups opened before the crash.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       CreateChildFrameAfterKillingOpener) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  SiteInstanceImpl* site_instance_a =
      root->current_frame_host()->GetSiteInstance();

  // Open a popup and navigate it cross-process to b.com.
  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(ExecJs(root, "popup = window.open('about:blank');"));
  Shell* popup = new_shell_observer.GetShell();
  GURL popup_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(popup, popup_url));

  // Verify that each top-level frame has proxies in the other's SiteInstance.
  FrameTreeNode* popup_root =
      static_cast<WebContentsImpl*>(popup->web_contents())
          ->GetPrimaryFrameTree()
          .root();
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));
  EXPECT_EQ(
      " Site B ------------ proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(popup_root));

  // Kill the first window's renderer (a.com).
  RenderProcessHost* child_process = root->current_frame_host()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      child_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  child_process->Shutdown(0);
  crash_observer.Wait();
  EXPECT_FALSE(root->current_frame_host()->IsRenderFrameLive());

  // The proxy for the popup in a.com should've died.
  RenderFrameProxyHost* rfph =
      popup_root->current_frame_host()
          ->browsing_context_state()
          ->GetRenderFrameProxyHost(site_instance_a->group());
  EXPECT_FALSE(rfph->is_render_frame_proxy_live());

  // Recreate the a.com renderer.
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());

  // The popup's proxy in a.com should still not be live. Re-navigating the
  // main window to a.com doesn't reinitialize a.com proxies for popups
  // previously opened from the main window.
  EXPECT_FALSE(rfph->is_render_frame_proxy_live());

  // Add a new child frame on the popup.
  RenderFrameHostCreatedObserver frame_observer(popup->web_contents(), 1);
  EXPECT_TRUE(ExecJs(
      popup, "document.body.appendChild(document.createElement('iframe'));"));
  frame_observer.Wait();

  // Both the child frame's and its parent's proxies should still not be live.
  // The main page can't reach them since it lost reference to the popup after
  // it crashed, so there is no need to create them.
  EXPECT_FALSE(rfph->is_render_frame_proxy_live());
  RenderFrameProxyHost* child_rfph =
      popup_root->child_at(0)
          ->current_frame_host()
          ->browsing_context_state()
          ->GetRenderFrameProxyHost(site_instance_a->group());
  EXPECT_TRUE(child_rfph);
  EXPECT_FALSE(child_rfph->is_render_frame_proxy_live());
}

// In A-embed-B-embed-C scenario, verify that killing process B clears proxies
// of C from the tree.
//
//     1          A                  A
//    / \        / \                / \    .
//   2   3 ->   B   A -> Kill B -> B*  A
//  /          /
// 4          C
//
// node1 is the root.
// Initially, both node1.proxy_hosts_ and node3.proxy_hosts_ contain C.
// After we kill B, make sure proxies for C are cleared.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       KillingRendererClearsDescendantProxies) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_two_frames_nested.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(2U, root->child_count());

  GURL site_b_url(embedded_test_server()->GetURL(
      "bar.com", "/frame_tree/page_with_one_frame.html"));
  // We can't use a TestNavigationObserver to verify the URL here,
  // since the frame has children that may have clobbered it in the observer.
  EXPECT_EQ(site_b_url, root->child_at(0)->current_url());

  // Ensure that a new process is created for node2.
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            root->child_at(0)->current_frame_host()->GetSiteInstance());
  // Ensure that a new process is *not* created for node3.
  EXPECT_EQ(shell()->web_contents()->GetSiteInstance(),
            root->child_at(1)->current_frame_host()->GetSiteInstance());

  ASSERT_EQ(1U, root->child_at(0)->child_count());

  // Make sure node4 points to the correct cross-site-page.
  FrameTreeNode* node4 = root->child_at(0)->child_at(0);
  GURL site_c_url(embedded_test_server()->GetURL("baz.com", "/title1.html"));
  EXPECT_EQ(site_c_url, node4->current_url());

  // |site_instance_c_group|'s frames and proxies are expected to go away once
  // we kill |child_process_b| below.
  scoped_refptr<SiteInstanceGroup> site_instance_c_group =
      node4->current_frame_host()->GetSiteInstance()->group();

  // Initially proxies for both B and C will be present in the root.
  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   |--Site B ------- proxies for A C\n"
      "   |    +--Site C -- proxies for A B\n"
      "   +--Site A ------- proxies for B C\n"
      "Where A = http://a.com/\n"
      "      B = http://bar.com/\n"
      "      C = http://baz.com/",
      DepictFrameTree(root));

  EXPECT_GT(site_instance_c_group->active_frame_count(), 0U);

  // Kill process B.
  RenderProcessHost* child_process_b =
      root->child_at(0)->current_frame_host()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      child_process_b, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  child_process_b->Shutdown(0);
  crash_observer.Wait();

  // Make sure proxy C has gone from root.
  // Make sure proxy C has gone from node3 as well.
  // Make sure proxy B stays around in root and node3.
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   +--Site A ------- proxies for B\n"
      "Where A = http://a.com/\n"
      "      B = http://bar.com/ (no process)",
      DepictFrameTree(root));

  EXPECT_EQ(0U, site_instance_c_group->active_frame_count());
}

// Crash a subframe and ensures its children are cleared from the FrameTree.
// See http://crbug.com/338508.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, CrashSubframe) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Check the subframe process.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));
  FrameTreeNode* child = root->child_at(0);
  EXPECT_TRUE(
      child->current_frame_host()->render_view_host()->IsRenderViewLive());
  EXPECT_TRUE(child->current_frame_host()->IsRenderFrameLive());

  // Crash the subframe process.
  RenderProcessHost* root_process = root->current_frame_host()->GetProcess();
  RenderProcessHost* child_process = child->current_frame_host()->GetProcess();
  {
    RenderProcessHostWatcher crash_observer(
        child_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    child_process->Shutdown(0);
    crash_observer.Wait();
  }

  // Ensure that the child frame still exists but has been cleared.
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/ (no process)",
      DepictFrameTree(root));
  EXPECT_EQ(1U, root->child_count());
  EXPECT_EQ(main_url, root->current_url());
  EXPECT_EQ(GURL(), child->current_url());

  EXPECT_FALSE(
      child->current_frame_host()->render_view_host()->IsRenderViewLive());
  EXPECT_FALSE(child->current_frame_host()->IsRenderFrameLive());

  // Now crash the top-level page to clear the child frame.
  {
    RenderProcessHostWatcher crash_observer(
        root_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    root_process->Shutdown(0);
    crash_observer.Wait();
  }
  EXPECT_EQ(0U, root->child_count());
  EXPECT_EQ(GURL(), root->current_url());
}

// When a new subframe is added, related SiteInstances that can reach the
// subframe should create proxies for it (https://crbug.com/423587).  This test
// checks that if A embeds B and later adds a new subframe A2, A2 gets a proxy
// in B's process.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, CreateProxiesForNewFrames) {
  GURL main_url(embedded_test_server()->GetURL(
      "b.com", "/frame_tree/page_with_one_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());

  // Make sure the frame starts out at the correct cross-site URL.
  EXPECT_EQ(embedded_test_server()->GetURL("baz.com", "/title1.html"),
            root->child_at(0)->current_url());

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://b.com/\n"
      "      B = http://baz.com/",
      DepictFrameTree(root));

  // Add a new child frame to the top-level frame.
  RenderFrameHostCreatedObserver frame_observer(shell()->web_contents(), 1);
  EXPECT_TRUE(ExecJs(shell(), "addFrame('data:text/html,foo');"));
  frame_observer.Wait();

  // The new frame should have a proxy in Site B, for use by the old frame.
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   +--Site A ------- proxies for B\n"
      "Where A = http://b.com/\n"
      "      B = http://baz.com/",
      DepictFrameTree(root));
}

// TODO(nasko): Disable this test until out-of-process iframes is ready and the
// security checks are back in place.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       DISABLED_CrossSiteIframeRedirectOnce) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  ASSERT_TRUE(https_server.Start());

  GURL main_url(embedded_test_server()->GetURL("/site_per_process_main.html"));
  GURL http_url(embedded_test_server()->GetURL("/title1.html"));
  GURL https_url(https_server.GetURL("/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  TestNavigationObserver observer(shell()->web_contents());
  {
    // Load cross-site client-redirect page into Iframe.
    // Should be blocked.
    GURL client_redirect_https_url(
        https_server.GetURL("/client-redirect?/title1.html"));
    EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(), "test",
                                    client_redirect_https_url));
    // DidFailProvisionalLoad when navigating to client_redirect_https_url.
    EXPECT_EQ(observer.last_navigation_url(), client_redirect_https_url);
    EXPECT_FALSE(observer.last_navigation_succeeded());
  }

  {
    // Load cross-site server-redirect page into Iframe,
    // which redirects to same-site page.
    GURL server_redirect_http_url(
        https_server.GetURL("/server-redirect?" + http_url.spec()));
    EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(), "test",
                                    server_redirect_http_url));
    EXPECT_EQ(observer.last_navigation_url(), http_url);
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  {
    // Load cross-site server-redirect page into Iframe,
    // which redirects to cross-site page.
    GURL server_redirect_http_url(
        https_server.GetURL("/server-redirect?/title1.html"));
    EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(), "test",
                                    server_redirect_http_url));
    // DidFailProvisionalLoad when navigating to https_url.
    EXPECT_EQ(observer.last_navigation_url(), https_url);
    EXPECT_FALSE(observer.last_navigation_succeeded());
  }

  {
    // Load same-site server-redirect page into Iframe,
    // which redirects to cross-site page.
    GURL server_redirect_http_url(
        embedded_test_server()->GetURL("/server-redirect?" + https_url.spec()));
    EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(), "test",
                                    server_redirect_http_url));

    EXPECT_EQ(observer.last_navigation_url(), https_url);
    EXPECT_FALSE(observer.last_navigation_succeeded());
  }

  {
    // Load same-site client-redirect page into Iframe,
    // which redirects to cross-site page.
    GURL client_redirect_http_url(
        embedded_test_server()->GetURL("/client-redirect?" + https_url.spec()));

    LoadStopObserver load_observer2(shell()->web_contents());

    EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(), "test",
                                    client_redirect_http_url));

    // Same-site Client-Redirect Page should be loaded successfully.
    EXPECT_EQ(observer.last_navigation_url(), client_redirect_http_url);
    EXPECT_TRUE(observer.last_navigation_succeeded());

    // Redirecting to Cross-site Page should be blocked.
    load_observer2.Wait();
    EXPECT_EQ(observer.last_navigation_url(), https_url);
    EXPECT_FALSE(observer.last_navigation_succeeded());
  }

  {
    // Load same-site server-redirect page into Iframe,
    // which redirects to same-site page.
    GURL server_redirect_http_url(
        embedded_test_server()->GetURL("/server-redirect?/title1.html"));
    EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(), "test",
                                    server_redirect_http_url));
    EXPECT_EQ(observer.last_navigation_url(), http_url);
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  {
    // Load same-site client-redirect page into Iframe,
    // which redirects to same-site page.
    GURL client_redirect_http_url(
        embedded_test_server()->GetURL("/client-redirect?" + http_url.spec()));
    LoadStopObserver load_observer2(shell()->web_contents());

    EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(), "test",
                                    client_redirect_http_url));

    // Same-site Client-Redirect Page should be loaded successfully.
    EXPECT_EQ(observer.last_navigation_url(), client_redirect_http_url);
    EXPECT_TRUE(observer.last_navigation_succeeded());

    // Redirecting to Same-site Page should be loaded successfully.
    load_observer2.Wait();
    EXPECT_EQ(observer.last_navigation_url(), http_url);
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }
}

// TODO(nasko): Disable this test until out-of-process iframes is ready and the
// security checks are back in place.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       DISABLED_CrossSiteIframeRedirectTwice) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  ASSERT_TRUE(https_server.Start());

  GURL main_url(embedded_test_server()->GetURL("/site_per_process_main.html"));
  GURL http_url(embedded_test_server()->GetURL("/title1.html"));
  GURL https_url(https_server.GetURL("/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  TestNavigationObserver observer(shell()->web_contents());
  {
    // Load client-redirect page pointing to a cross-site client-redirect page,
    // which eventually redirects back to same-site page.
    GURL client_redirect_https_url(
        https_server.GetURL("/client-redirect?" + http_url.spec()));
    GURL client_redirect_http_url(embedded_test_server()->GetURL(
        "/client-redirect?" + client_redirect_https_url.spec()));

    // We should wait until second client redirect get cancelled.
    LoadStopObserver load_observer2(shell()->web_contents());

    EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(), "test",
                                    client_redirect_http_url));

    // DidFailProvisionalLoad when navigating to client_redirect_https_url.
    load_observer2.Wait();
    EXPECT_EQ(observer.last_navigation_url(), client_redirect_https_url);
    EXPECT_FALSE(observer.last_navigation_succeeded());
  }

  {
    // Load server-redirect page pointing to a cross-site server-redirect page,
    // which eventually redirect back to same-site page.
    GURL server_redirect_https_url(
        https_server.GetURL("/server-redirect?" + http_url.spec()));
    GURL server_redirect_http_url(embedded_test_server()->GetURL(
        "/server-redirect?" + server_redirect_https_url.spec()));
    EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(), "test",
                                    server_redirect_http_url));
    EXPECT_EQ(observer.last_navigation_url(), http_url);
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  {
    // Load server-redirect page pointing to a cross-site server-redirect page,
    // which eventually redirects back to cross-site page.
    GURL server_redirect_https_url(
        https_server.GetURL("/server-redirect?" + https_url.spec()));
    GURL server_redirect_http_url(embedded_test_server()->GetURL(
        "/server-redirect?" + server_redirect_https_url.spec()));
    EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(), "test",
                                    server_redirect_http_url));

    // DidFailProvisionalLoad when navigating to https_url.
    EXPECT_EQ(observer.last_navigation_url(), https_url);
    EXPECT_FALSE(observer.last_navigation_succeeded());
  }

  {
    // Load server-redirect page pointing to a cross-site client-redirect page,
    // which eventually redirects back to same-site page.
    GURL client_redirect_http_url(
        https_server.GetURL("/client-redirect?" + http_url.spec()));
    GURL server_redirect_http_url(embedded_test_server()->GetURL(
        "/server-redirect?" + client_redirect_http_url.spec()));
    EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(), "test",
                                    server_redirect_http_url));

    // DidFailProvisionalLoad when navigating to client_redirect_http_url.
    EXPECT_EQ(observer.last_navigation_url(), client_redirect_http_url);
    EXPECT_FALSE(observer.last_navigation_succeeded());
  }
}

// Ensure that when navigating a frame cross-process RenderFrameProxyHosts are
// created in the FrameTree skipping the subtree of the navigating frame (but
// not the navigating frame itself).
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, ProxyCreationSkipsSubtree) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a,a(a,a(a)))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  EXPECT_TRUE(root->child_at(1) != nullptr);
  EXPECT_EQ(2U, root->child_at(1)->child_count());

  {
    // Load same-site page into iframe.
    TestNavigationObserver observer(shell()->web_contents());
    GURL http_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
    EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), http_url));
    EXPECT_EQ(http_url, observer.last_navigation_url());
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_EQ(
        " Site A\n"
        "   |--Site A\n"
        "   +--Site A\n"
        "        |--Site A\n"
        "        +--Site A\n"
        "             +--Site A\n"
        "Where A = http://a.com/",
        DepictFrameTree(root));
  }

  // Create the cross-site URL to navigate to.
  GURL cross_site_url =
      embedded_test_server()->GetURL("foo.com", "/frame_tree/title2.html");

  // Load cross-site page into the second iframe without waiting for the
  // navigation to complete. Once LoadURLWithParams returns, we would expect
  // proxies to have been created in the frame tree, but children of the
  // navigating frame to still be present. The reason is that we don't run the
  // message loop, so no IPCs that alter the frame tree can be processed.
  FrameTreeNode* child = root->child_at(1);
  SiteInstance* site = nullptr;
  std::string cross_site_rfh_type = "speculative";
  {
    TestNavigationObserver observer(shell()->web_contents());
    TestNavigationManager navigation_manager(shell()->web_contents(),
                                             cross_site_url);
    NavigationController::LoadURLParams params(cross_site_url);
    params.transition_type = PageTransitionFromInt(ui::PAGE_TRANSITION_LINK);
    params.frame_tree_node_id = child->frame_tree_node_id();
    child->navigator().controller().LoadURLWithParams(params);
    navigation_manager.WaitForSpeculativeRenderFrameHostCreation();

    site = child->render_manager()->speculative_frame_host()->GetSiteInstance();
    EXPECT_NE(shell()->web_contents()->GetSiteInstance(), site);

    std::string tree = base::StringPrintf(
        " Site A ------------ proxies for B\n"
        "   |--Site A ------- proxies for B\n"
        "   +--Site A (B %s) -- proxies for B\n"
        "        |--Site A\n"
        "        +--Site A\n"
        "             +--Site A\n"
        "Where A = http://a.com/\n"
        "      B = http://foo.com/",
        cross_site_rfh_type.c_str());
    EXPECT_EQ(tree, DepictFrameTree(root));

    // Now that the verification is done, run the message loop and wait for the
    // navigation to complete.
    ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_EQ(cross_site_url, observer.last_navigation_url());

    EXPECT_EQ(
        " Site A ------------ proxies for B\n"
        "   |--Site A ------- proxies for B\n"
        "   +--Site B ------- proxies for A\n"
        "Where A = http://a.com/\n"
        "      B = http://foo.com/",
        DepictFrameTree(root));
  }

  // Load another cross-site page into the same iframe.
  cross_site_url = embedded_test_server()->GetURL("bar.com", "/title3.html");
  {
    // Perform the same checks as the first cross-site navigation, since
    // there have been issues in subsequent cross-site navigations. Also ensure
    // that the SiteInstance has properly changed.
    // TODO(nasko): Once we have proper cleanup of resources, add code to
    // verify that the intermediate SiteInstance/RenderFrameHost have been
    // properly cleaned up.
    TestNavigationObserver observer(shell()->web_contents());
    TestNavigationManager navigation_manager(shell()->web_contents(),
                                             cross_site_url);
    NavigationController::LoadURLParams params(cross_site_url);
    params.transition_type = PageTransitionFromInt(ui::PAGE_TRANSITION_LINK);
    params.frame_tree_node_id = child->frame_tree_node_id();
    child->navigator().controller().LoadURLWithParams(params);
    navigation_manager.WaitForSpeculativeRenderFrameHostCreation();

    SiteInstance* site2 =
        child->render_manager()->speculative_frame_host()->GetSiteInstance();
    EXPECT_NE(shell()->web_contents()->GetSiteInstance(), site2);
    EXPECT_NE(site, site2);

    std::string tree = base::StringPrintf(
        " Site A ------------ proxies for B C\n"
        "   |--Site A ------- proxies for B C\n"
        "   +--Site B (C %s) -- proxies for A C\n"
        "Where A = http://a.com/\n"
        "      B = http://foo.com/\n"
        "      C = http://bar.com/",
        cross_site_rfh_type.c_str());
    EXPECT_EQ(tree, DepictFrameTree(root));

    ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_EQ(cross_site_url, observer.last_navigation_url());
    EXPECT_EQ(0U, child->child_count());
  }
}

// Verify origin replication with an A-embed-B-embed-C-embed-A hierarchy.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, OriginReplication) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c(a),b), a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   |--Site B ------- proxies for A C\n"       // tiptop_child
      "   |    |--Site C -- proxies for A B\n"       // middle_child
      "   |    |    +--Site A -- proxies for B C\n"  // lowest_child
      "   |    +--Site B -- proxies for A C\n"
      "   +--Site A ------- proxies for B C\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/\n"
      "      C = http://c.com/",
      DepictFrameTree(root));

  url::Origin a_origin =
      url::Origin::Create(embedded_test_server()->GetURL("a.com", "/"));
  url::Origin b_origin =
      url::Origin::Create(embedded_test_server()->GetURL("b.com", "/"));
  url::Origin c_origin =
      url::Origin::Create(embedded_test_server()->GetURL("c.com", "/"));
  FrameTreeNode* tiptop_child = root->child_at(0);
  FrameTreeNode* middle_child = root->child_at(0)->child_at(0);
  FrameTreeNode* lowest_child = root->child_at(0)->child_at(0)->child_at(0);

  // Check that b.com frame's location.ancestorOrigins contains the correct
  // origin for the parent.  The origin should have been replicated as part of
  // the mojom::Renderer::CreateView message that created the parent's
  // `blink::RemoteFrame` in b.com's process.
  EXPECT_EQ(ListValueOf(a_origin),
            EvalJs(tiptop_child, "Array.from(location.ancestorOrigins);"));

  // Check that c.com frame's location.ancestorOrigins contains the correct
  // origin for its two ancestors. The topmost parent origin should be
  // replicated as part of mojom::Renderer::CreateView, and the middle frame
  // (b.com's) origin should be replicated as part of
  // blink::mojom::RemoteFrame::CreateRemoteChild sent for b.com's frame in
  // c.com's process.
  EXPECT_EQ(ListValueOf(b_origin, a_origin),
            EvalJs(middle_child, "Array.from(location.ancestorOrigins);"));

  // Check that the nested a.com frame's location.ancestorOrigins contains the
  // correct origin for its three ancestors.
  EXPECT_EQ(ListValueOf(c_origin, b_origin, a_origin),
            EvalJs(lowest_child, "Array.from(location.ancestorOrigins);"));
}

// Test that HasReceivedUserGesture and HasReceivedUserGestureBeforeNavigation
// are propagated correctly across origins.
// TODO(crbug.com/40653035): This test is flaky.
IN_PROC_BROWSER_TEST_P(SitePerProcessAutoplayBrowserTest,
                       DISABLED_PropagateUserGestureFlag) {
  GURL main_url(embedded_test_server()->GetURL(
      "example.com", "/media/autoplay/autoplay-enabled.html"));
  GURL foo_url(embedded_test_server()->GetURL(
      "foo.com", "/media/autoplay/autoplay-enabled.html"));
  GURL bar_url(embedded_test_server()->GetURL(
      "bar.com", "/media/autoplay/autoplay-enabled.html"));
  GURL secondary_url(embedded_test_server()->GetURL(
      "test.example.com", "/media/autoplay/autoplay-enabled.html"));
  GURL disabled_url(embedded_test_server()->GetURL(
      "test.example.com", "/media/autoplay/autoplay-disabled.html"));

  // Load a page with an iframe that has autoplay.
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  // Navigate the subframes to cross-origin pages.
  EXPECT_TRUE(NavigateFrameToURL(root->child_at(0), foo_url));
  EXPECT_TRUE(NavigateFrameToURL(root->child_at(0)->child_at(0), bar_url));

  // Test that all frames can autoplay if there has been a gesture in the top
  // frame.
  EXPECT_TRUE(AutoplayAllowed(shell(), true));
  EXPECT_TRUE(AutoplayAllowed(root->child_at(0), false));
  EXPECT_TRUE(AutoplayAllowed(root->child_at(0)->child_at(0), false));

  // Navigate to a new page on the same origin.
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), secondary_url));
  root = web_contents()->GetPrimaryFrameTree().root();

  // Navigate the subframes to cross-origin pages.
  EXPECT_TRUE(NavigateFrameToURL(root->child_at(0), foo_url));
  EXPECT_TRUE(NavigateFrameToURL(root->child_at(0)->child_at(0), bar_url));

  // Test that all frames can autoplay because the gesture bit has been passed
  // through the navigation.
  EXPECT_TRUE(AutoplayAllowed(shell(), false));
  EXPECT_TRUE(AutoplayAllowed(root->child_at(0), false));
  EXPECT_TRUE(AutoplayAllowed(root->child_at(0)->child_at(0), false));

  // Navigate to a page with autoplay disabled.
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), disabled_url));
  EXPECT_TRUE(NavigateFrameToURL(root->child_at(0), foo_url));

  // Test that autoplay is no longer allowed.
  EXPECT_TRUE(AutoplayAllowed(shell(), false));
  EXPECT_FALSE(AutoplayAllowed(root->child_at(0), false));

  // Navigate to another origin and make sure autoplay is disabled.
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), foo_url));
  EXPECT_TRUE(NavigateFrameToURL(root->child_at(0), bar_url));
  EXPECT_FALSE(AutoplayAllowed(shell(), false));
  EXPECT_FALSE(AutoplayAllowed(shell(), false));
}

// Check that iframe sandbox flags are replicated correctly.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, SandboxFlagsReplication) {
  GURL main_url(embedded_test_server()->GetURL("/sandboxed_frames.html"));
  const url::Origin main_origin = url::Origin::Create(main_url);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  TestNavigationObserver observer(shell()->web_contents());

  // Navigate the second (sandboxed) subframe to a cross-site page with a
  // subframe.
  GURL foo_url(
      embedded_test_server()->GetURL("foo.com", "/frame_tree/1-1.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(1), foo_url));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // We can't use a TestNavigationObserver to verify the URL here,
  // since the frame has children that may have clobbered it in the observer.
  EXPECT_EQ(foo_url, root->child_at(1)->current_url());

  // Load cross-site page into subframe's subframe.
  ASSERT_EQ(2U, root->child_at(1)->child_count());
  GURL bar_url(embedded_test_server()->GetURL("bar.com", "/title1.html"));
  EXPECT_TRUE(
      NavigateToURLFromRenderer(root->child_at(1)->child_at(0), bar_url));
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(bar_url, observer.last_navigation_url());

  // Opening a popup in the sandboxed foo.com iframe should fail.
  EXPECT_EQ(false, EvalJs(root->child_at(1),
                          "!!window.open('data:text/html,dataurl');"));
  EXPECT_EQ(1u, Shell::windows().size());

  // Opening a popup in a frame whose parent is sandboxed should also fail.
  // Here, bar.com frame's sandboxed parent frame is a remote frame in
  // bar.com's process.
  EXPECT_EQ(false, EvalJs(root->child_at(1)->child_at(0),
                          "!!window.open('data:text/html,dataurl');"));
  EXPECT_EQ(1u, Shell::windows().size());

  // Same, but now try the case where bar.com frame's sandboxed parent is a
  // local frame in bar.com's process.
  EXPECT_EQ(false, EvalJs(root->child_at(2)->child_at(0),
                          "!!window.open('data:text/html,dataurl');"));
  EXPECT_EQ(1u, Shell::windows().size());

  // Check that foo.com frame's location.ancestorOrigins contains the correct
  // origin for the parent, which should be unaffected by sandboxing.
  EXPECT_EQ(ListValueOf(main_origin),
            EvalJs(root->child_at(1), "Array.from(location.ancestorOrigins);"));

  // Now check location.ancestorOrigins for the bar.com frame. The middle frame
  // (foo.com's) origin should be unique, since that frame is sandboxed, and
  // the top frame should match |main_url|.
  EXPECT_EQ(ListValueOf("null", main_origin),
            EvalJs(root->child_at(1)->child_at(0),
                   "Array.from(location.ancestorOrigins);"));
}

// Check that dynamic updates to iframe sandbox flags are propagated correctly.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, DynamicSandboxFlags) {
  bool sandboxed_iframes_are_isolated =
      SiteIsolationPolicy::AreIsolatedSandboxedIframesEnabled();
  GURL main_url(
      embedded_test_server()->GetURL("/frame_tree/page_with_two_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  TestNavigationObserver observer(shell()->web_contents());
  ASSERT_EQ(2U, root->child_count());

  // Make sure first frame starts out at the correct cross-site page.
  EXPECT_EQ(embedded_test_server()->GetURL("bar.com", "/title1.html"),
            root->child_at(0)->current_url());

  // Navigate second frame to another cross-site page.
  GURL baz_url(embedded_test_server()->GetURL("baz.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(1), baz_url));
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(baz_url, observer.last_navigation_url());

  // Both frames should not be sandboxed to start with.
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            root->child_at(0)->pending_frame_policy().sandbox_flags);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            root->child_at(0)->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            root->child_at(1)->pending_frame_policy().sandbox_flags);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            root->child_at(1)->effective_frame_policy().sandbox_flags);

  // Dynamically update sandbox flags for the first frame.
  EXPECT_TRUE(ExecJs(
      shell(), "document.querySelector('iframe').sandbox='allow-scripts';"));

  // Check that updated sandbox flags are propagated to browser process.
  // The new flags should be reflected in pending_frame_policy().sandbox_flags,
  // while effective_frame_policy().sandbox_flags should still reflect the old
  // flags, because sandbox flag updates take place only after navigations.
  // "allow-scripts" resets both SandboxFlags::Scripts and
  // SandboxFlags::AutomaticFeatures bits per blink::parseSandboxPolicy().
  network::mojom::WebSandboxFlags expected_flags =
      network::mojom::WebSandboxFlags::kAll &
      ~network::mojom::WebSandboxFlags::kScripts &
      ~network::mojom::WebSandboxFlags::kAutomaticFeatures;
  EXPECT_EQ(expected_flags,
            root->child_at(0)->pending_frame_policy().sandbox_flags);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            root->child_at(0)->effective_frame_policy().sandbox_flags);

  // Navigate the first frame to a page on the same site.  The new sandbox
  // flags should take effect.
  GURL bar_url(
      embedded_test_server()->GetURL("bar.com", "/frame_tree/2-4.html"));
  {
    RenderFrameDeletedObserver deleted_observer(
        root->child_at(0)->current_frame_host());
    EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), bar_url));
    if (sandboxed_iframes_are_isolated) {
      deleted_observer.WaitUntilDeleted();
    }
  }
  // (The new page has a subframe; wait for it to load as well.)
  ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(bar_url, root->child_at(0)->current_url());
  ASSERT_EQ(1U, root->child_at(0)->child_count());

  EXPECT_EQ(
      base::StringPrintf(" Site A ------------ proxies for B C\n"
                         "   |--Site B ------- proxies for A C\n"
                         "   |    +--Site B -- proxies for A C\n"
                         "   +--Site C ------- proxies for A B\n"
                         "Where A = http://127.0.0.1/\n"
                         "      B = http://bar.com/%s\n"
                         "      C = http://baz.com/",
                         sandboxed_iframes_are_isolated ? " (sandboxed)" : ""),
      DepictFrameTree(root));

  // Confirm that the browser process has updated the frame's current sandbox
  // flags.
  EXPECT_EQ(expected_flags,
            root->child_at(0)->pending_frame_policy().sandbox_flags);
  EXPECT_EQ(expected_flags,
            root->child_at(0)->effective_frame_policy().sandbox_flags);

  // Opening a popup in the now-sandboxed frame should fail.
  EXPECT_EQ(false, EvalJs(root->child_at(0),
                          "!!window.open('data:text/html,dataurl');"));
  EXPECT_EQ(1u, Shell::windows().size());

  // Navigate the child of the now-sandboxed frame to a page on baz.com.  The
  // child should inherit the latest sandbox flags from its parent frame, which
  // is currently a proxy in baz.com's renderer process.  This checks that the
  // proxies of |root->child_at(0)| were also updated with the latest sandbox
  // flags.
  // TODO(crbug.com/40943240): When IsolateSandboxedIframes is enabled,
  // this test no longer uses the proxy inheritance mentioned above, because
  // sandboxed and unsandboxed baz.com pages will be in different SiteInstances.
  // Restructure the test so it still provides coverage for proxy inheritance
  // when IsolateSandboxedIframes is enabled.
  GURL baz_child_url(embedded_test_server()->GetURL("baz.com", "/title2.html"));
  {
    RenderFrameDeletedObserver deleted_observer(
        root->child_at(0)->child_at(0)->current_frame_host());
    EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0)->child_at(0),
                                          baz_child_url));
    deleted_observer.WaitUntilDeleted();
  }
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(baz_child_url, observer.last_navigation_url());

  if (sandboxed_iframes_are_isolated) {
    switch (blink::features::kIsolateSandboxedIframesGroupingParam.Get()) {
      case blink::features::IsolateSandboxedIframesGrouping::kPerSite:
      case blink::features::IsolateSandboxedIframesGrouping::kPerOrigin:
        EXPECT_EQ(
            " Site A ------------ proxies for B C D\n"
            "   |--Site B ------- proxies for A C D\n"
            "   |    +--Site D -- proxies for A B C\n"
            "   +--Site C ------- proxies for A B D\n"
            "Where A = http://127.0.0.1/\n"
            "      B = http://bar.com/ (sandboxed)\n"
            "      C = http://baz.com/\n"
            "      D = http://baz.com/ (sandboxed)",
            DepictFrameTree(root));
        break;
      case blink::features::IsolateSandboxedIframesGrouping::kPerDocument:
        // TODO(crbug.com/40941714): Add output for the PerDocument
        // case, and parameterize this test to run all variants (none, per-site,
        // per-origin, per-document).
        break;
    }
  } else {
    EXPECT_EQ(
        " Site A ------------ proxies for B C\n"
        "   |--Site B ------- proxies for A C\n"
        "   |    +--Site C -- proxies for A B\n"
        "   +--Site C ------- proxies for A B\n"
        "Where A = http://127.0.0.1/\n"
        "      B = http://bar.com/\n"
        "      C = http://baz.com/",
        DepictFrameTree(root));
  }

  // Opening a popup in the child of a sandboxed frame should fail.
  EXPECT_EQ(false, EvalJs(root->child_at(0)->child_at(0),
                          "!!window.open('data:text/html,dataurl');"));
  EXPECT_EQ(1u, Shell::windows().size());

  // Child of a sandboxed frame should also be sandboxed on the browser side.
  EXPECT_EQ(
      expected_flags,
      root->child_at(0)->child_at(0)->effective_frame_policy().sandbox_flags);
}

// Check that dynamic updates to iframe sandbox flags are propagated correctly.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       DynamicSandboxFlagsRemoteToLocal) {
  GURL main_url(
      embedded_test_server()->GetURL("/frame_tree/page_with_two_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  TestNavigationObserver observer(shell()->web_contents());
  ASSERT_EQ(2U, root->child_count());

  // Make sure the two frames starts out at correct URLs.
  EXPECT_EQ(embedded_test_server()->GetURL("bar.com", "/title1.html"),
            root->child_at(0)->current_url());
  EXPECT_EQ(embedded_test_server()->GetURL("/title1.html"),
            root->child_at(1)->current_url());

  // Update the second frame's sandbox flags.
  EXPECT_TRUE(
      ExecJs(shell(),
             "document.querySelectorAll('iframe')[1].sandbox='allow-scripts'"));

  // Check that the current sandbox flags are updated but the effective
  // sandbox flags are not.
  network::mojom::WebSandboxFlags expected_flags =
      network::mojom::WebSandboxFlags::kAll &
      ~network::mojom::WebSandboxFlags::kScripts &
      ~network::mojom::WebSandboxFlags::kAutomaticFeatures;
  EXPECT_EQ(expected_flags,
            root->child_at(1)->pending_frame_policy().sandbox_flags);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            root->child_at(1)->effective_frame_policy().sandbox_flags);

  // Navigate the second subframe to a page on bar.com.  This will trigger a
  // remote-to-local frame swap in bar.com's process.
  GURL bar_url(embedded_test_server()->GetURL(
      "bar.com", "/frame_tree/page_with_one_frame.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(1), bar_url));
  EXPECT_EQ(bar_url, root->child_at(1)->current_url());
  ASSERT_EQ(1U, root->child_at(1)->child_count());

  // Confirm that the browser process has updated the current sandbox flags.
  EXPECT_EQ(expected_flags,
            root->child_at(1)->pending_frame_policy().sandbox_flags);
  EXPECT_EQ(expected_flags,
            root->child_at(1)->effective_frame_policy().sandbox_flags);

  // Opening a popup in the sandboxed second frame should fail.
  EXPECT_EQ(false, EvalJs(root->child_at(1),
                          "!!window.open('data:text/html,dataurl');"));
  EXPECT_EQ(1u, Shell::windows().size());

  // Make sure that the child frame inherits the sandbox flags of its
  // now-sandboxed parent frame.
  EXPECT_EQ(false, EvalJs(root->child_at(1)->child_at(0),
                          "!!window.open('data:text/html,dataurl');"));
  EXPECT_EQ(1u, Shell::windows().size());
}

// Check that dynamic updates to iframe sandbox flags are propagated correctly.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       DynamicSandboxFlagsRendererInitiatedNavigation) {
  GURL main_url(
      embedded_test_server()->GetURL("/frame_tree/page_with_one_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  TestNavigationObserver observer(shell()->web_contents());
  ASSERT_EQ(1U, root->child_count());

  // Make sure the frame starts out at the correct cross-site page.
  EXPECT_EQ(embedded_test_server()->GetURL("baz.com", "/title1.html"),
            root->child_at(0)->current_url());

  // The frame should not be sandboxed to start with.
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            root->child_at(0)->pending_frame_policy().sandbox_flags);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            root->child_at(0)->effective_frame_policy().sandbox_flags);

  // Dynamically update the frame's sandbox flags.
  EXPECT_TRUE(ExecJs(
      shell(), "document.querySelector('iframe').sandbox='allow-scripts';"));

  // Check that updated sandbox flags are propagated to browser process.
  // The new flags should be set in pending_frame_policy().sandbox_flags, while
  // effective_frame_policy().sandbox_flags should still reflect the old flags,
  // because sandbox flag updates take place only after navigations.
  // "allow-scripts" resets both SandboxFlags::Scripts and
  // SandboxFlags::AutomaticFeatures bits per blink::parseSandboxPolicy().
  network::mojom::WebSandboxFlags expected_flags =
      network::mojom::WebSandboxFlags::kAll &
      ~network::mojom::WebSandboxFlags::kScripts &
      ~network::mojom::WebSandboxFlags::kAutomaticFeatures;
  EXPECT_EQ(expected_flags,
            root->child_at(0)->pending_frame_policy().sandbox_flags);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            root->child_at(0)->effective_frame_policy().sandbox_flags);

  // Perform a renderer-initiated same-site navigation in the first frame. The
  // new sandbox flags should take effect.
  TestFrameNavigationObserver frame_observer(root->child_at(0));
  ASSERT_TRUE(ExecJs(root->child_at(0), "window.location.href='/title2.html'"));
  frame_observer.Wait();
  EXPECT_EQ(embedded_test_server()->GetURL("baz.com", "/title2.html"),
            root->child_at(0)->current_url());

  // Confirm that the browser process has updated the frame's current sandbox
  // flags.
  EXPECT_EQ(expected_flags,
            root->child_at(0)->pending_frame_policy().sandbox_flags);
  EXPECT_EQ(expected_flags,
            root->child_at(0)->effective_frame_policy().sandbox_flags);

  // Opening a popup in the now-sandboxed frame should fail.
  EXPECT_EQ(false, EvalJs(root->child_at(0),
                          "!!window.open('data:text/html,dataurl');"));
  EXPECT_EQ(1u, Shell::windows().size());
}

// Verify that when a new child frame is added, the proxies created for it in
// other SiteInstances have correct sandbox flags and origin.
//
//     A         A           A
//    /         / \         / \    .
//   B    ->   B   A   ->  B   A
//                              \  .
//                               B
//
// The test checks sandbox flags and origin for the proxy added in step 2, by
// checking whether the grandchild frame added in step 3 sees proper sandbox
// flags and origin for its (remote) parent.  This wasn't addressed when
// https://crbug.com/423587 was fixed.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       ProxiesForNewChildFramesHaveCorrectReplicationState) {
  GURL main_url(
      embedded_test_server()->GetURL("/frame_tree/page_with_one_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://baz.com/",
      DepictFrameTree(root));

  // In the root frame, add a new sandboxed local frame, which itself has a
  // child frame on baz.com.  Wait for three RenderFrameHosts to be created:
  // the new sandboxed local frame, its child (while it's still local), and a
  // speculative RFH when starting the cross-site navigation to baz.com.
  RenderFrameHostCreatedObserver frame_observer(shell()->web_contents(), 3);
  EXPECT_TRUE(ExecJs(root,
                     "addFrame('/frame_tree/page_with_one_frame.html',"
                     "         'allow-scripts allow-same-origin')"));
  frame_observer.Wait();

  // Wait for the cross-site navigation to baz.com in the grandchild to finish.
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  FrameTreeNode* bottom_child = root->child_at(1)->child_at(0);
  EXPECT_EQ(embedded_test_server()->GetURL("baz.com", "/title1.html"),
            bottom_child->current_url());

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   +--Site A ------- proxies for B\n"
      "        +--Site B -- proxies for A\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://baz.com/",
      DepictFrameTree(root));

  // Use location.ancestorOrigins to check that the grandchild on baz.com sees
  // correct origin for its parent and grandparent, which are at the same URL
  // and origin (namely, page_with_one_frame.html on the server's default
  // origin).
  EXPECT_EQ(
      ListValueOf(url::Origin::Create(main_url), url::Origin::Create(main_url)),
      EvalJs(bottom_child, "Array.from(location.ancestorOrigins);"));

  // Check that the sandbox flags in the browser process are correct.
  // "allow-scripts" resets both network::mojom::WebSandboxFlags::Scripts and
  // network::mojom::WebSandboxFlags::AutomaticFeatures bits per
  // blink::parseSandboxPolicy().
  network::mojom::WebSandboxFlags expected_flags =
      network::mojom::WebSandboxFlags::kAll &
      ~network::mojom::WebSandboxFlags::kScripts &
      ~network::mojom::WebSandboxFlags::kAutomaticFeatures &
      ~network::mojom::WebSandboxFlags::kOrigin;
  EXPECT_EQ(expected_flags,
            root->child_at(1)->effective_frame_policy().sandbox_flags);

  // The child of the sandboxed frame should've inherited sandbox flags, so it
  // should not be able to create popups.
  EXPECT_EQ(expected_flags,
            bottom_child->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(false,
            EvalJs(bottom_child, "!!window.open('data:text/html,dataurl')"));
  EXPECT_EQ(1u, Shell::windows().size());
}

// Verify that a child frame can retrieve the name property set by its parent.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, WindowNameReplication) {
  GURL main_url(embedded_test_server()->GetURL("/frame_tree/2-4.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  TestNavigationObserver observer(shell()->web_contents());

  // Load cross-site page into iframe.
  GURL frame_url =
      embedded_test_server()->GetURL("foo.com", "/frame_tree/3-1.html");
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), frame_url));
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(frame_url, observer.last_navigation_url());

  // Ensure that a new process is created for the subframe.
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            root->child_at(0)->current_frame_host()->GetSiteInstance());

  // Check that the window.name seen by the frame matches the name attribute
  // specified by its parent in the iframe tag.
  EXPECT_EQ("3-1-name", EvalJs(root->child_at(0), "window.name;"));
}

// Verify that dynamic updates to a frame's window.name propagate to the
// frame's proxies, so that the latest frame names can be used in navigations.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, DynamicWindowName) {
  GURL main_url(embedded_test_server()->GetURL("/frame_tree/2-4.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  TestNavigationObserver observer(shell()->web_contents());

  // Load cross-site page into iframe.
  GURL frame_url =
      embedded_test_server()->GetURL("foo.com", "/frame_tree/3-1.html");
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), frame_url));
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(frame_url, observer.last_navigation_url());

  // Browser process should know the child frame's original window.name
  // specified in the iframe element.
  EXPECT_EQ(root->child_at(0)->frame_name(), "3-1-name");

  // Update the child frame's window.name.
  EXPECT_TRUE(ExecJs(root->child_at(0), "window.name = 'updated-name';"));

  // The change should propagate to the browser process.
  EXPECT_EQ(root->child_at(0)->frame_name(), "updated-name");

  // The proxy in the parent process should also receive the updated name.
  // Now iframe's name and the content window's name differ, so it shouldn't
  // be possible to access to the content window with the updated name.
  EXPECT_EQ(true, EvalJs(shell(), "frames['updated-name'] === undefined;"));
  // Change iframe's name to match the content window's name so that it can
  // reference the child frame by its new name in case of cross origin.
  EXPECT_TRUE(ExecJs(root, "window['3-1-id'].name = 'updated-name';"));
  EXPECT_EQ(true, EvalJs(shell(), "frames['updated-name'] == frames[0];"));

  // Issue a renderer-initiated navigation from the root frame to the child
  // frame using the frame's name. Make sure correct frame is navigated.
  //
  // TODO(alexmos): When blink::createWindow is refactored to handle
  // RemoteFrames, this should also be tested via window.open(url, frame_name)
  // and a more complicated frame hierarchy (https://crbug.com/463742)
  TestFrameNavigationObserver frame_observer(root->child_at(0));
  GURL foo_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  EXPECT_TRUE(
      ExecJs(shell(),
             JsReplace("frames['updated-name'].location.href = $1", foo_url)));
  frame_observer.Wait();
  EXPECT_EQ(foo_url, root->child_at(0)->current_url());
}

// Verify that when a frame is navigated to a new origin, the origin update
// propagates to the frame's proxies.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, OriginUpdatesReachProxies) {
  GURL main_url(
      embedded_test_server()->GetURL("/frame_tree/page_with_two_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  TestNavigationObserver observer(shell()->web_contents());

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   +--Site A ------- proxies for B\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://bar.com/",
      DepictFrameTree(root));

  // Navigate second subframe to a baz.com.  This should send an origin update
  // to the frame's proxy in the bar.com (first frame's) process.
  GURL frame_url = embedded_test_server()->GetURL("baz.com", "/title2.html");
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(1), frame_url));
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(frame_url, observer.last_navigation_url());

  // The first frame can't directly observe the second frame's origin with
  // JavaScript.  Instead, try to navigate the second frame from the first
  // frame.  This should fail with a console error message, which should
  // contain the second frame's updated origin (see blink::Frame::canNavigate).
  WebContentsConsoleObserver console_observer(shell()->web_contents());
  console_observer.SetPattern("Unsafe attempt to initiate navigation*");

  // frames[1] can't be used due to a bug where RemoteFrames are created out of
  // order (https://crbug.com/478792).  Instead, target second frame by name.
  EXPECT_TRUE(ExecJs(root->child_at(0),
                     "try { parent.frames['frame2'].location.href = "
                     "'data:text/html,foo'; } catch (e) {}"));
  ASSERT_TRUE(console_observer.Wait());

  std::string frame_origin = root->child_at(1)->current_origin().Serialize();
  EXPECT_EQ(frame_origin + "/", frame_url.DeprecatedGetOriginAsURL().spec());
  EXPECT_TRUE(base::MatchPattern(console_observer.GetMessageAt(0u),
                                 "*" + frame_origin + "*"))
      << "Error message does not contain the frame's latest origin ("
      << frame_origin << ")";
}

// Ensure that navigating subframes in --site-per-process mode properly fires
// the DidStopLoading event on WebContentsObserver.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, CrossSiteDidStopLoading) {
  GURL main_url(embedded_test_server()->GetURL("/site_per_process_main.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  TestNavigationObserver observer(shell()->web_contents());

  // Load same-site page into iframe.
  FrameTreeNode* child = root->child_at(0);
  GURL http_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(child, http_url));
  EXPECT_EQ(http_url, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());

  // Load cross-site page into iframe.
  TestNavigationObserver nav_observer(shell()->web_contents(), 1);
  GURL url = embedded_test_server()->GetURL("foo.com", "/title2.html");
  NavigationController::LoadURLParams params(url);
  params.transition_type = ui::PAGE_TRANSITION_LINK;
  params.frame_tree_node_id = child->frame_tree_node_id();
  child->navigator().controller().LoadURLWithParams(params);
  nav_observer.Wait();

  // Verify that the navigation succeeded and the expected URL was loaded.
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(url, observer.last_navigation_url());
}

// Ensure that the renderer does not crash when navigating a frame that has a
// sibling RemoteFrame.  See https://crbug.com/426953.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       NavigateWithSiblingRemoteFrame) {
  GURL main_url(
      embedded_test_server()->GetURL("/frame_tree/page_with_two_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  TestNavigationObserver observer(shell()->web_contents());

  // Make sure the first frame is out of process.
  ASSERT_EQ(2U, root->child_count());
  FrameTreeNode* node2 = root->child_at(0);
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            node2->current_frame_host()->GetSiteInstance());

  // Make sure the second frame is in the parent's process.
  FrameTreeNode* node3 = root->child_at(1);
  EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
            node3->current_frame_host()->GetSiteInstance());

  // Navigate the second iframe (node3) to a URL in its own process.
  GURL title_url = embedded_test_server()->GetURL("/title2.html");
  EXPECT_TRUE(NavigateToURLFromRenderer(node3, title_url));
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(title_url, observer.last_navigation_url());
  EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
            node3->current_frame_host()->GetSiteInstance());
  EXPECT_TRUE(node3->current_frame_host()->IsRenderFrameLive());
}

// Ensure that the renderer does not crash when a local frame with a remote
// parent frame is swapped from local to remote, then back to local again.
// See https://crbug.com/585654.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       NavigateSiblingsToSameProcess) {
  GURL main_url(
      embedded_test_server()->GetURL("/frame_tree/page_with_two_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  FrameTreeNode* node2 = root->child_at(0);
  FrameTreeNode* node3 = root->child_at(1);

  // Navigate the second iframe to the same process as the first.
  GURL frame_url = embedded_test_server()->GetURL("bar.com", "/title1.html");
  EXPECT_TRUE(NavigateToURLFromRenderer(node3, frame_url));

  // Verify that they are in the same process.
  EXPECT_EQ(node2->current_frame_host()->GetSiteInstance(),
            node3->current_frame_host()->GetSiteInstance());
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            node3->current_frame_host()->GetSiteInstance());

  // Navigate the first iframe into its parent's process.
  GURL title_url = embedded_test_server()->GetURL("/title2.html");
  EXPECT_TRUE(NavigateToURLFromRenderer(node2, title_url));
  EXPECT_NE(node2->current_frame_host()->GetSiteInstance(),
            node3->current_frame_host()->GetSiteInstance());

  // Return the first iframe to the same process as its sibling, and ensure
  // that it does not crash.
  EXPECT_TRUE(NavigateToURLFromRenderer(node2, frame_url));
  EXPECT_EQ(node2->current_frame_host()->GetSiteInstance(),
            node3->current_frame_host()->GetSiteInstance());
  EXPECT_TRUE(node2->current_frame_host()->IsRenderFrameLive());
}

// Verify that load events for iframe elements work when the child frame is
// out-of-process.  In such cases, the load event is forwarded from the child
// frame to the parent frame via the browser process.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, LoadEventForwarding) {
  // Load a page with a cross-site frame.  The parent page has an onload
  // handler in the iframe element that appends "LOADED" to the document title.
  {
    GURL main_url(
        embedded_test_server()->GetURL("/frame_with_load_event.html"));
    std::u16string expected_title(u"LOADED");
    TitleWatcher title_watcher(shell()->web_contents(), expected_title);
    EXPECT_TRUE(NavigateToURL(shell(), main_url));
    EXPECT_EQ(title_watcher.WaitAndGetTitle(), expected_title);
  }

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  // Load another cross-site page into the iframe and check that the load event
  // is fired.
  {
    GURL foo_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
    std::u16string expected_title(u"LOADEDLOADED");
    TitleWatcher title_watcher(shell()->web_contents(), expected_title);
    TestNavigationObserver observer(shell()->web_contents());
    EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), foo_url));
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_EQ(foo_url, observer.last_navigation_url());
    EXPECT_EQ(title_watcher.WaitAndGetTitle(), expected_title);
  }
}

// Check that postMessage can be routed between cross-site iframes.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, SubframePostMessage) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_post_message_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  ASSERT_EQ(2U, root->child_count());

  // Verify the frames start at correct URLs.  First frame should be
  // same-site; second frame should be cross-site.
  GURL same_site_url(embedded_test_server()->GetURL("/post_message.html"));
  EXPECT_EQ(same_site_url, root->child_at(0)->current_url());
  GURL foo_url(embedded_test_server()->GetURL("foo.com", "/post_message.html"));
  EXPECT_EQ(foo_url, root->child_at(1)->current_url());
  EXPECT_NE(root->child_at(0)->current_frame_host()->GetSiteInstance(),
            root->child_at(1)->current_frame_host()->GetSiteInstance());

  // Send a message from first, same-site frame to second, cross-site frame.
  // Expect the second frame to reply back to the first frame.
  PostMessageAndWaitForReply(root->child_at(0),
                             "postToSibling('subframe-msg','subframe2')",
                             "\"done-subframe1\"");

  // Send a postMessage from second, cross-site frame to its parent.  Expect
  // parent to send a reply to the frame.
  std::u16string expected_title(u"subframe-msg");
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);
  PostMessageAndWaitForReply(root->child_at(1), "postToParent('subframe-msg')",
                             "\"done-subframe2\"");
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  // Verify the total number of received messages for each subframe.  First
  // frame should have one message (reply from second frame).  Second frame
  // should have two messages (message from first frame and reply from parent).
  // Parent should have one message (from second frame).
  EXPECT_EQ(1, GetReceivedMessages(root->child_at(0)));
  EXPECT_EQ(2, GetReceivedMessages(root->child_at(1)));
  EXPECT_EQ(1, GetReceivedMessages(root));
}

// Check that postMessage can be sent from a subframe on a cross-process opener
// tab, and that its event.source points to a valid proxy.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       PostMessageWithSubframeOnOpenerChain) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_post_message_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  ASSERT_EQ(2U, root->child_count());

  // Verify the initial state of the world.  First frame should be same-site;
  // second frame should be cross-site.
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site A ------- proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://foo.com/",
      DepictFrameTree(root));

  // Open a popup from the first subframe (so that popup's window.opener points
  // to the subframe) and navigate it to bar.com.
  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(ExecJs(root->child_at(0), "openPopup('about:blank');"));
  Shell* popup = new_shell_observer.GetShell();
  GURL popup_url(
      embedded_test_server()->GetURL("bar.com", "/post_message.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(popup, popup_url));

  // From the popup, open another popup for baz.com.  This will be used to
  // check that the whole opener chain is processed when creating proxies and
  // not just an immediate opener.
  ShellAddedObserver new_shell_observer2;
  EXPECT_TRUE(ExecJs(popup, "openPopup('about:blank');"));
  Shell* popup2 = new_shell_observer2.GetShell();
  GURL popup2_url(
      embedded_test_server()->GetURL("baz.com", "/post_message.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(popup2, popup2_url));

  // Ensure that we've created proxies for SiteInstances of both popups (C, D)
  // in the main window's frame tree.
  EXPECT_EQ(
      " Site A ------------ proxies for B C D\n"
      "   |--Site A ------- proxies for B C D\n"
      "   +--Site B ------- proxies for A C D\n"
      "Where A = http://a.com/\n"
      "      B = http://foo.com/\n"
      "      C = http://bar.com/\n"
      "      D = http://baz.com/",
      DepictFrameTree(root));

  // Check the first popup's frame tree as well.  Note that it doesn't have a
  // proxy for foo.com, since foo.com can't reach the popup.  It does have a
  // proxy for its opener a.com (which can reach it via the window.open
  // reference) and second popup (which can reach it via window.opener).
  FrameTreeNode* popup_root =
      static_cast<WebContentsImpl*>(popup->web_contents())
          ->GetPrimaryFrameTree()
          .root();
  EXPECT_EQ(
      " Site C ------------ proxies for A D\n"
      "Where A = http://a.com/\n"
      "      C = http://bar.com/\n"
      "      D = http://baz.com/",
      DepictFrameTree(popup_root));

  // Send a message from first subframe on main page to the first popup and
  // wait for a reply back. The reply verifies that the proxy for the opener
  // tab's subframe is targeted properly.
  PostMessageAndWaitForReply(root->child_at(0), "postToPopup('subframe-msg')",
                             "\"done-subframe1\"");

  // Send a postMessage from the popup to window.opener and ensure that it
  // reaches subframe1.  This verifies that the subframe opener information
  // propagated to the popup's RenderFrame.  Wait for subframe1 to send a reply
  // message to the popup.
  EXPECT_TRUE(ExecJs(popup, "window.name = 'popup';"));
  PostMessageAndWaitForReply(popup_root, "postToOpener('subframe-msg', '*')",
                             "\"done-popup\"");

  // Second a postMessage from popup2 to window.opener.opener, which should
  // resolve to subframe1.  This tests opener chains of length greater than 1.
  // As before, subframe1 will send a reply to popup2.
  FrameTreeNode* popup2_root =
      static_cast<WebContentsImpl*>(popup2->web_contents())
          ->GetPrimaryFrameTree()
          .root();
  EXPECT_TRUE(ExecJs(popup2, "window.name = 'popup2';"));
  PostMessageAndWaitForReply(popup2_root,
                             "postToOpenerOfOpener('subframe-msg', '*')",
                             "\"done-popup2\"");

  // Verify the total number of received messages for each subframe:
  //  - 3 for first subframe (two from first popup, one from second popup)
  //  - 2 for popup (both from first subframe)
  //  - 1 for popup2 (reply from first subframe)
  //  - 0 for other frames
  EXPECT_EQ(0, GetReceivedMessages(root));
  EXPECT_EQ(3, GetReceivedMessages(root->child_at(0)));
  EXPECT_EQ(0, GetReceivedMessages(root->child_at(1)));
  EXPECT_EQ(2, GetReceivedMessages(popup_root));
  EXPECT_EQ(1, GetReceivedMessages(popup2_root));
}

// Check that parent.frames[num] references correct sibling frames when the
// parent is remote.  See https://crbug.com/478792.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, IndexedFrameAccess) {
  // Start on a page with three same-site subframes.
  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/frame_tree/top.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(3U, root->child_count());
  FrameTreeNode* child0 = root->child_at(0);
  FrameTreeNode* child1 = root->child_at(1);
  FrameTreeNode* child2 = root->child_at(2);

  // Send each of the frames to a different site.  Each new renderer will first
  // create proxies for the parent and two sibling subframes and then create
  // and insert the new RenderFrame into the frame tree.
  GURL b_url(embedded_test_server()->GetURL("b.com", "/post_message.html"));
  GURL c_url(embedded_test_server()->GetURL("c.com", "/post_message.html"));
  GURL d_url(embedded_test_server()->GetURL("d.com", "/post_message.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(child0, b_url));
  EXPECT_TRUE(NavigateToURLFromRenderer(child1, c_url));
  EXPECT_TRUE(NavigateToURLFromRenderer(child2, d_url));

  EXPECT_EQ(
      " Site A ------------ proxies for B C D\n"
      "   |--Site B ------- proxies for A C D\n"
      "   |--Site C ------- proxies for A B D\n"
      "   +--Site D ------- proxies for A B C\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/\n"
      "      C = http://c.com/\n"
      "      D = http://d.com/",
      DepictFrameTree(root));

  // Check that each subframe sees itself at correct index in parent.frames.
  EXPECT_EQ(true, EvalJs(child0, "window === parent.frames[0];"));
  EXPECT_EQ(true, EvalJs(child1, "window === parent.frames[1];"));
  EXPECT_EQ(true, EvalJs(child2, "window === parent.frames[2];"));

  // Send a postMessage from B to parent.frames[1], which should go to C, and
  // wait for reply.
  PostMessageAndWaitForReply(child0, "postToSibling('subframe-msg', 1)",
                             "\"done-1-1-name\"");

  // Send a postMessage from C to parent.frames[2], which should go to D, and
  // wait for reply.
  PostMessageAndWaitForReply(child1, "postToSibling('subframe-msg', 2)",
                             "\"done-1-2-name\"");

  // Verify the total number of received messages for each subframe.
  EXPECT_EQ(1, GetReceivedMessages(child0));
  EXPECT_EQ(2, GetReceivedMessages(child1));
  EXPECT_EQ(1, GetReceivedMessages(child2));
}

IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, RFPHDestruction) {
  GURL main_url(embedded_test_server()->GetURL("/site_per_process_main.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  TestNavigationObserver observer(shell()->web_contents());

  // Load cross-site page into iframe.
  FrameTreeNode* child = root->child_at(0);
  GURL url = embedded_test_server()->GetURL("foo.com", "/title2.html");
  {
    RenderFrameDeletedObserver deleted_observer(child->current_frame_host());
    EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), url));
    deleted_observer.WaitUntilDeleted();
  }
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(url, observer.last_navigation_url());
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   +--Site A ------- proxies for B\n"
      "        |--Site A -- proxies for B\n"
      "        +--Site A -- proxies for B\n"
      "             +--Site A -- proxies for B\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://foo.com/",
      DepictFrameTree(root));

  // Load another cross-site page.
  url = embedded_test_server()->GetURL("bar.com", "/title3.html");
  {
    RenderFrameDeletedObserver deleted_observer(child->current_frame_host());
    NavigateIframeToURL(shell()->web_contents(), "test", url);
    deleted_observer.WaitUntilDeleted();
  }
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(url, observer.last_navigation_url());
  EXPECT_EQ(
      " Site A ------------ proxies for C\n"
      "   |--Site C ------- proxies for A\n"
      "   +--Site A ------- proxies for C\n"
      "        |--Site A -- proxies for C\n"
      "        +--Site A -- proxies for C\n"
      "             +--Site A -- proxies for C\n"
      "Where A = http://127.0.0.1/\n"
      "      C = http://bar.com/",
      DepictFrameTree(root));

  // Navigate back to the parent's origin.
  {
    RenderFrameDeletedObserver deleted_observer(child->current_frame_host());
    url = embedded_test_server()->GetURL("/title1.html");
    EXPECT_TRUE(NavigateToURLFromRenderer(child, url));
    // Wait for the old process to exit, to verify that the proxies go away.
    deleted_observer.WaitUntilDeleted();
  }
  EXPECT_EQ(url, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());

  EXPECT_EQ(
      " Site A\n"
      "   |--Site A\n"
      "   +--Site A\n"
      "        |--Site A\n"
      "        +--Site A\n"
      "             +--Site A\n"
      "Where A = http://127.0.0.1/",
      DepictFrameTree(root));
}

IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, OpenPopupWithRemoteParent) {
  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/site_per_process_main.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  // Navigate first child cross-site.
  GURL frame_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), frame_url));

  // Open a popup from the first child.
  Shell* new_shell =
      OpenPopup(root->child_at(0), GURL(url::kAboutBlankURL), "");
  EXPECT_TRUE(new_shell);

  // Check that the popup's opener is correct on both the browser and renderer
  // sides.
  FrameTreeNode* popup_root =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetPrimaryFrameTree()
          .root();
  EXPECT_EQ(root->child_at(0), popup_root->opener());

  EXPECT_EQ(frame_url.spec(),
            EvalJs(popup_root, "window.opener.location.href;"));

  // Now try the same with a cross-site popup and make sure it ends up in a new
  // process and with a correct opener.
  GURL popup_url(embedded_test_server()->GetURL("c.com", "/title2.html"));
  Shell* cross_site_popup = OpenPopup(root->child_at(0), popup_url, "");
  EXPECT_TRUE(cross_site_popup);

  FrameTreeNode* cross_site_popup_root =
      static_cast<WebContentsImpl*>(cross_site_popup->web_contents())
          ->GetPrimaryFrameTree()
          .root();
  EXPECT_EQ(cross_site_popup_root->current_url(), popup_url);

  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            cross_site_popup->web_contents()->GetSiteInstance());
  EXPECT_NE(root->child_at(0)->current_frame_host()->GetSiteInstance(),
            cross_site_popup->web_contents()->GetSiteInstance());

  EXPECT_EQ(root->child_at(0), cross_site_popup_root->opener());

  // Ensure the popup's window.opener points to the right subframe.  Note that
  // we can't check the opener's location as above since it's cross-origin.
  EXPECT_EQ(true, EvalJs(cross_site_popup_root,
                         "window.opener === window.opener.top.frames[0];"));
}

// Test that cross-process popups can't be navigated to disallowed URLs by
// their opener.  This ensures that proper URL validation is performed when
// RenderFrameProxyHosts are navigated.  See https://crbug.com/595339.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, NavigatePopupToIllegalURL) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Open a cross-site popup.
  GURL popup_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  Shell* popup = OpenPopup(shell(), popup_url, "foo");
  EXPECT_TRUE(popup);
  EXPECT_NE(popup->web_contents()->GetSiteInstance(),
            shell()->web_contents()->GetSiteInstance());

  WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern("Not allowed to load local resource:*");

  // From the opener, navigate the popup to a file:/// URL.  This should result
  // in a console error and stay on the old page.
  GURL file_url("file:///");
  NavigateNamedFrame(shell(), file_url, "foo");
  EXPECT_TRUE(WaitForLoadStop(popup->web_contents()));
  EXPECT_EQ(popup_url, popup->web_contents()->GetLastCommittedURL());
  EXPECT_TRUE(base::MatchPattern(console_observer.GetMessageAt(0u),
                                 "Not allowed to load local resource: file:*"));

  // Now try the same test with a chrome:// URL.
  GURL chrome_url(std::string(kChromeUIScheme) + "://" +
                  std::string(kChromeUIGpuHost));
  NavigateNamedFrame(shell(), chrome_url, "foo");
  EXPECT_TRUE(WaitForLoadStop(popup->web_contents()));
  EXPECT_EQ(popup_url, popup->web_contents()->GetLastCommittedURL());
  EXPECT_TRUE(
      base::MatchPattern(console_observer.GetMessageAt(1u),
                         std::string("Not allowed to load local resource: ") +
                             kChromeUIScheme + ":*"));
}

// Verify that named frames are discoverable from their opener's ancestors.
// See https://crbug.com/511474.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       DiscoverNamedFrameFromAncestorOfOpener) {
  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/site_per_process_main.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  // Navigate first child cross-site.
  GURL frame_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), frame_url));

  // Open a popup named "foo" from the first child.
  Shell* foo_shell =
      OpenPopup(root->child_at(0), GURL(url::kAboutBlankURL), "foo");
  EXPECT_TRUE(foo_shell);

  // Check that a proxy was created for the "foo" popup in a.com.
  FrameTreeNode* foo_root =
      static_cast<WebContentsImpl*>(foo_shell->web_contents())
          ->GetPrimaryFrameTree()
          .root();
  SiteInstanceImpl* site_instance_a =
      root->current_frame_host()->GetSiteInstance();
  RenderFrameProxyHost* popup_rfph_for_a =
      foo_root->current_frame_host()
          ->browsing_context_state()
          ->GetRenderFrameProxyHost(site_instance_a->group());
  EXPECT_TRUE(popup_rfph_for_a);

  // Verify that the main frame can find the "foo" popup by name.  If
  // window.open targets the correct frame, the "foo" popup's current URL
  // should be updated to |named_frame_url|.
  GURL named_frame_url(embedded_test_server()->GetURL("c.com", "/title2.html"));
  NavigateNamedFrame(shell(), named_frame_url, "foo");
  EXPECT_TRUE(WaitForLoadStop(foo_shell->web_contents()));
  EXPECT_EQ(named_frame_url, foo_root->current_url());

  // Navigate the popup cross-site and ensure it's still reachable via
  // window.open from the main frame.
  GURL d_url(embedded_test_server()->GetURL("d.com", "/title3.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(foo_shell, d_url));
  EXPECT_EQ(d_url, foo_root->current_url());
  NavigateNamedFrame(shell(), named_frame_url, "foo");
  EXPECT_TRUE(WaitForLoadStop(foo_shell->web_contents()));
  EXPECT_EQ(named_frame_url, foo_root->current_url());
}

class SitePerProcessFencedFrameTest : public SitePerProcessBrowserTestBase {
 public:
  SitePerProcessFencedFrameTest() {
    fenced_frame_helper_ =
        std::make_unique<content::test::FencedFrameTestHelper>();
  }

  void SetUpOnMainThread() override {
    SitePerProcessBrowserTestBase::SetUpOnMainThread();
    https_server_.ServeFilesFromSourceDirectory(GetTestDataFilePath());
    ASSERT_TRUE(https_server_.Start());
  }

 protected:
  net::EmbeddedTestServer& https_server() { return https_server_; }

  content::RenderFrameHost* CreateFencedFrame(content::RenderFrameHost* parent,
                                              const GURL& url) {
    if (fenced_frame_helper_) {
      return fenced_frame_helper_->CreateFencedFrame(parent, url);
    }

    // FencedFrameTestHelper only supports the MPArch version of fenced frames.
    // So need to maually create a fenced frame for the ShadowDOM version.
    content::TestNavigationManager navigation(web_contents(), url);

    constexpr char kAddFencedFrameScript[] = R"({
        const fenced_frame = document.createElement('fencedframe');
        fenced_frame.src = $1;
        document.body.appendChild(fenced_frame);
    })";
    EXPECT_TRUE(ExecJs(parent, content::JsReplace(kAddFencedFrameScript, url)));
    EXPECT_TRUE(navigation.WaitForNavigationFinished());

    return ChildFrameAt(parent, 0);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<content::test::FencedFrameTestHelper> fenced_frame_helper_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_F(SitePerProcessFencedFrameTest,
                       PopupFromFencedFrameDoesNotCreateProxy) {
  GURL main_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  // Create a fenced frame.
  GURL fenced_frame_url(https_server().GetURL("/fenced_frames/title1.html"));
  RenderFrameHost* fenced_frame_host = CreateFencedFrame(
      web_contents()->GetPrimaryMainFrame(), fenced_frame_url);
  EXPECT_NE(nullptr, fenced_frame_host);

  // Open a popup named "foo" from the fenced frame.
  Shell* popup_shell =
      OpenPopup(fenced_frame_host, GURL(url::kAboutBlankURL), "foo", "", false);
  EXPECT_TRUE(popup_shell);

  // Check that the popup from the fenced frame didn't create a proxy.
  // Opening popups from fenced frames forces noopener, which makes named
  // frames not discoverable.
  FrameTreeNode* popup_root =
      static_cast<WebContentsImpl*>(popup_shell->web_contents())
          ->GetPrimaryFrameTree()
          .root();
  EXPECT_EQ(nullptr, popup_root->opener());

  SiteInstanceImpl* site_instance =
      root->current_frame_host()->GetSiteInstance();
  EXPECT_FALSE(popup_root->current_frame_host()
                   ->browsing_context_state()
                   ->GetRenderFrameProxyHost(site_instance->group()));

  SiteInstanceImpl* embedder_site_instance =
      static_cast<RenderFrameHostImpl*>(fenced_frame_host)->GetSiteInstance();
  EXPECT_FALSE(popup_root->current_frame_host()
                   ->browsing_context_state()
                   ->GetRenderFrameProxyHost(embedder_site_instance->group()));
}

// Similar to DiscoverNamedFrameFromAncestorOfOpener, but check that if a
// window is created without a name and acquires window.name later, it will
// still be discoverable from its opener's ancestors.  Also, instead of using
// an opener's ancestor, this test uses a popup with same origin as that
// ancestor. See https://crbug.com/511474.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       DiscoverFrameAfterSettingWindowName) {
  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/site_per_process_main.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  // Open a same-site popup from the main frame.
  GURL a_com_url(embedded_test_server()->GetURL("a.com", "/title3.html"));
  Shell* a_com_shell = OpenPopup(root->child_at(0), a_com_url, "");
  EXPECT_TRUE(a_com_shell);

  // Navigate first child on main frame cross-site.
  GURL frame_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), frame_url));

  // Open an unnamed popup from the first child frame.
  Shell* foo_shell =
      OpenPopup(root->child_at(0), GURL(url::kAboutBlankURL), "");
  EXPECT_TRUE(foo_shell);

  // There should be no proxy created for the "foo" popup in a.com, since
  // there's no way for the two a.com frames to access it yet.
  FrameTreeNode* foo_root =
      static_cast<WebContentsImpl*>(foo_shell->web_contents())
          ->GetPrimaryFrameTree()
          .root();
  SiteInstanceImpl* site_instance_a =
      root->current_frame_host()->GetSiteInstance();
  EXPECT_FALSE(foo_root->current_frame_host()
                   ->browsing_context_state()
                   ->GetRenderFrameProxyHost(site_instance_a->group()));

  // Set window.name in the popup's frame.
  EXPECT_TRUE(ExecJs(foo_shell, "window.name = 'foo'"));

  // A proxy for the popup should now exist in a.com.
  EXPECT_TRUE(foo_root->current_frame_host()
                  ->browsing_context_state()
                  ->GetRenderFrameProxyHost(site_instance_a->group()));

  // Verify that the a.com popup can now find the "foo" popup by name.
  GURL named_frame_url(embedded_test_server()->GetURL("c.com", "/title2.html"));
  NavigateNamedFrame(a_com_shell, named_frame_url, "foo");
  EXPECT_TRUE(WaitForLoadStop(foo_shell->web_contents()));
  EXPECT_EQ(named_frame_url, foo_root->current_url());
}

// Check that frame opener updates work with subframes.  Set up a window with a
// popup and update openers for the popup's main frame and subframe to
// subframes on first window, as follows:
//
//    foo      +---- bar
//    / \      |     / \      .
// bar   foo <-+  bar   foo
//  ^                    |
//  +--------------------+
//
// The sites are carefully set up so that both opener updates are cross-process
// but still allowed by Blink's navigation checks.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, UpdateSubframeOpener) {
  GURL main_url = embedded_test_server()->GetURL(
      "foo.com", "/frame_tree/page_with_two_frames.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  EXPECT_EQ(2U, root->child_count());

  // From the top frame, open a popup and navigate it to a cross-site page with
  // two subframes.
  Shell* popup_shell = OpenPopup(shell(), GURL(url::kAboutBlankURL), "popup");
  EXPECT_TRUE(popup_shell);
  GURL popup_url(embedded_test_server()->GetURL(
      "bar.com", "/frame_tree/page_with_post_message_frames.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(popup_shell, popup_url));

  FrameTreeNode* popup_root =
      static_cast<WebContentsImpl*>(popup_shell->web_contents())
          ->GetPrimaryFrameTree()
          .root();
  EXPECT_EQ(2U, popup_root->child_count());

  // Popup's opener should point to main frame to start with.
  EXPECT_EQ(root, popup_root->opener());

  // Update the popup's opener to the second subframe on the main page (which
  // is same-origin with the top frame, i.e., foo.com).
  EXPECT_EQ(true, EvalJs(root->child_at(1), "!!window.open('','popup');"));

  // Check that updated opener propagated to the browser process and the
  // popup's bar.com process.
  EXPECT_EQ(root->child_at(1), popup_root->opener());

  EXPECT_EQ(true,
            EvalJs(popup_shell,
                   "window.opener === window.opener.parent.frames['frame2'];"));

  // Now update opener on the popup's second subframe (foo.com) to the main
  // page's first subframe (bar.com).
  EXPECT_EQ(true, EvalJs(root->child_at(0), "!!window.open('','subframe2');"));

  // Check that updated opener propagated to the browser process and the
  // foo.com process.
  EXPECT_EQ(root->child_at(0), popup_root->child_at(1)->opener());

  EXPECT_EQ(true,
            EvalJs(popup_root->child_at(1),
                   "window.opener === window.opener.parent.frames['frame1'];"));
}

// Check that when a subframe navigates to a new SiteInstance, the new
// SiteInstance will get a proxy for the opener of subframe's parent.  I.e.,
// accessing parent.opener from the subframe should still work after a
// cross-process navigation.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       NavigatingSubframePreservesOpenerInParent) {
  GURL main_url = embedded_test_server()->GetURL("a.com", "/post_message.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  // Open a popup with a cross-site page that has a subframe.
  GURL popup_url(embedded_test_server()->GetURL(
      "b.com", "/cross_site_iframe_factory.html?b(b)"));
  Shell* popup_shell = OpenPopup(shell(), popup_url, "popup");
  EXPECT_TRUE(popup_shell);
  FrameTreeNode* popup_root =
      static_cast<WebContentsImpl*>(popup_shell->web_contents())
          ->GetPrimaryFrameTree()
          .root();
  EXPECT_EQ(1U, popup_root->child_count());

  // Check that the popup's opener is correct in the browser process.
  EXPECT_EQ(root, popup_root->opener());

  // Navigate popup's subframe to another site.
  GURL frame_url(embedded_test_server()->GetURL("c.com", "/post_message.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(popup_root->child_at(0), frame_url));

  // Check that the new subframe process still sees correct opener for its
  // parent by sending a postMessage to subframe's parent.opener.
  EXPECT_EQ(true, EvalJs(popup_root->child_at(0), "!!parent.opener;"));

  std::u16string expected_title = u"msg";
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);
  EXPECT_EQ(true, EvalJs(popup_root->child_at(0),
                         "postToOpenerOfParent('msg','*');"));
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

// Check that if a subframe has an opener, that opener is preserved when the
// subframe navigates cross-site.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, NavigateSubframeWithOpener) {
  GURL main_url(embedded_test_server()->GetURL(
      "foo.com", "/frame_tree/page_with_two_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   +--Site A ------- proxies for B\n"
      "Where A = http://foo.com/\n"
      "      B = http://bar.com/",
      DepictFrameTree(root));

  // Update the first (cross-site) subframe's opener to root frame.
  EXPECT_EQ(true, EvalJs(root, "!!window.open('','frame1');"));

  // Check that updated opener propagated to the browser process and subframe's
  // process.
  EXPECT_EQ(root, root->child_at(0)->opener());

  EXPECT_EQ(true,
            EvalJs(root->child_at(0), "window.opener === window.parent;"));

  // Navigate the subframe with opener to another site.
  GURL frame_url(embedded_test_server()->GetURL("baz.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), frame_url));

  // Check that the subframe still sees correct opener in its new process.
  EXPECT_EQ(true,
            EvalJs(root->child_at(0), "window.opener === window.parent;"));

  // Navigate second subframe to a new site.  Check that the proxy that's
  // created for the first subframe in the new SiteInstance has correct opener.
  GURL frame2_url(embedded_test_server()->GetURL("qux.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(1), frame2_url));

  EXPECT_EQ(true, EvalJs(root->child_at(1),
                         "parent.frames['frame1'].opener === parent;"));
}

// Check that if a subframe has an opener, that opener is preserved when a new
// `blink::RemoteFrame` is created for that subframe in another renderer
// process. Similar to NavigateSubframeWithOpener, but this test verifies the
// subframe opener plumbing for blink::mojom::RemoteFrame::CreateRemoteChild(),
// whereas NavigateSubframeWithOpener targets mojom::Renderer::CreateFrame().
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       NewRenderFrameProxyPreservesOpener) {
  GURL main_url(
      embedded_test_server()->GetURL("foo.com", "/post_message.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  // Open a popup with a cross-site page that has two subframes.
  GURL popup_url(embedded_test_server()->GetURL(
      "bar.com", "/frame_tree/page_with_post_message_frames.html"));
  Shell* popup_shell = OpenPopup(shell(), popup_url, "popup");
  EXPECT_TRUE(popup_shell);
  FrameTreeNode* popup_root =
      static_cast<WebContentsImpl*>(popup_shell->web_contents())
          ->GetPrimaryFrameTree()
          .root();
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site A ------- proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://bar.com/\n"
      "      B = http://foo.com/",
      DepictFrameTree(popup_root));

  // Update the popup's second subframe's opener to root frame.  This is
  // allowed because that subframe is in the same foo.com SiteInstance as the
  // root frame.
  EXPECT_EQ(true, EvalJs(root, "!!window.open('','subframe2');"));

  // Check that the opener update propagated to the browser process and bar.com
  // process.
  EXPECT_EQ(root, popup_root->child_at(1)->opener());
  EXPECT_EQ(true,
            EvalJs(popup_root->child_at(0),
                   "parent.frames['subframe2'].opener && "
                   "    parent.frames['subframe2'].opener === parent.opener;"));

  // Navigate the popup's first subframe to another site.
  GURL frame_url(
      embedded_test_server()->GetURL("baz.com", "/post_message.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(popup_root->child_at(0), frame_url));

  // Check that the second subframe's opener is still correct in the first
  // subframe's new process.  Verify it both in JS and with a postMessage.
  EXPECT_EQ(true,
            EvalJs(popup_root->child_at(0),
                   "parent.frames['subframe2'].opener && "
                   "    parent.frames['subframe2'].opener === parent.opener;"));

  std::u16string expected_title = u"msg";
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);
  EXPECT_EQ(true, EvalJs(popup_root->child_at(0),
                         "postToOpenerOfSibling('subframe2', 'msg', '*');"));
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

// Test for https://crbug.com/515302. Perform two navigations, A1 -> B2 -> A3,
// and drop the mojo::AgentSchedulingGroupHost::DidUnloadRenderFrame from the A1
// -> B2 navigation, so that the second B2 -> A3 navigation is initiated before
// the first page receives the
// mojo::AgentSchedulingGroupHost::DidUnloadRenderFrame. Ensure that this
// doesn't crash and that the RVH(A1) is not reused in that case.
#if BUILDFLAG(IS_MAC)
#define MAYBE_RenderViewHostIsNotReusedAfterDelayedUnloadACK \
  DISABLED_RenderViewHostIsNotReusedAfterDelayedUnloadACK
#else
#define MAYBE_RenderViewHostIsNotReusedAfterDelayedUnloadACK \
  RenderViewHostIsNotReusedAfterDelayedUnloadACK
#endif
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       MAYBE_RenderViewHostIsNotReusedAfterDelayedUnloadACK) {
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), a_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  RenderFrameHostImpl* rfh = root->current_frame_host();
  RenderViewHostImpl* rvh = rfh->render_view_host();
  int rvh_routing_id = rvh->GetRoutingID();
  int rvh_process_id = rvh->GetProcess()->GetID();
  SiteInstanceImpl* site_instance = rfh->GetSiteInstance();
  RenderFrameDeletedObserver deleted_observer(rfh);

  // Install a BrowserMessageFilter to drop
  // mojo::AgentSchedulingGroupHost::DidUnloadRenderFrame messages in A's
  // process.
  auto unload_ack_filter = base::BindRepeating([] { return true; });
  rfh->SetUnloadACKCallbackForTesting(unload_ack_filter);
  rfh->DisableUnloadTimerForTesting();

  // Navigate to B.  This must wait for DidCommitProvisionalLoad and not
  // DidStopLoading, so that the Unload timer doesn't call OnUnloaded and
  // destroy |rfh| and |rvh| before they are checked in the test.
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  TestFrameNavigationObserver commit_observer(root);
  EXPECT_TRUE(ExecJs(shell(), JsReplace("location = $1", b_url)));
  commit_observer.WaitForCommit();
  EXPECT_FALSE(deleted_observer.deleted());

  // The previous RFH should be either:
  // 1) In the BackForwardCache, if back-forward cache is enabled.
  // 2) Pending deletion otherwise, since the
  // mojo::AgentSchedulingGroupHost::DidUnloadRenderFrame for A->B is dropped.
  EXPECT_THAT(
      rfh->lifecycle_state(),
      testing::AnyOf(
          testing::Eq(
              RenderFrameHostImpl::LifecycleStateImpl::kRunningUnloadHandlers),
          testing::Eq(
              RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache)));

  // Without the mojo::AgentSchedulingGroupHost::DidUnloadRenderFrame and timer,
  // the process A will never shutdown. Simulate the process being killed now.
  content::RenderProcessHostWatcher crash_observer(
      rvh->GetProcess(),
      content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  EXPECT_TRUE(rvh->GetProcess()->Shutdown(0));
  crash_observer.Wait();

  // Verify that the RVH and RFH for A were cleaned up.
  EXPECT_FALSE(root->frame_tree().GetRenderViewHost(site_instance->group()));
  EXPECT_TRUE(deleted_observer.deleted());

  // Start a navigation back to A, being careful to stay in the same
  // BrowsingInstance, and check that the RenderViewHost wasn't reused.
  TestNavigationManager navigation_manager(shell()->web_contents(), a_url);
  shell()->LoadURLForFrame(a_url, std::string(),
                           ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK));
  navigation_manager.WaitForSpeculativeRenderFrameHostCreation();
  RenderFrameHostImpl* pending_rfh =
      root->render_manager()->speculative_frame_host();
  RenderViewHostImpl* pending_rvh = pending_rfh->render_view_host();

  // When ProactivelySwapBrowsingInstance A1 and A3 aren't using the same
  // BrowsingInstance.
  if (CanCrossSiteNavigationsProactivelySwapBrowsingInstances())
    EXPECT_NE(site_instance, pending_rfh->GetSiteInstance());
  else
    EXPECT_EQ(site_instance, pending_rfh->GetSiteInstance());

  EXPECT_FALSE(rvh_routing_id == pending_rvh->GetRoutingID() &&
               rvh_process_id == pending_rvh->GetProcess()->GetID());

  // Make sure the last navigation finishes without crashing.
  ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());
}

// Test for https://crbug.com/591478, where navigating to a cross-site page with
// a subframe on the old site caused a crash while trying to reuse the old
// RenderViewHost.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       ReusePendingDeleteRenderViewHostForSubframe) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  std::string script =
      "window.onunload = function() { "
      "  var start = Date.now();"
      "  while (Date.now() - start < 1000);"
      "}";
  EXPECT_TRUE(ExecJs(shell(), script));

  // Navigating cross-site with an iframe to the original site shouldn't crash.
  GURL second_url(embedded_test_server()->GetURL(
      "b.com", "/cross_site_iframe_factory.html?b(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), second_url));

  // If the subframe is created while the main frame is pending deletion, then
  // the RVH will be reused.  The main frame should've been swapped with a
  // proxy despite being the last active frame in the progress (see
  // https://crbug.com/568836), and this proxy should also be reused by the new
  // page.
  //
  // TODO(creis, alexmos): Find a way to assert this that isn't flaky. For now,
  // the test is just likely (not certain) to catch regressions by crashing.
}

// Check that when a cross-process frame acquires focus, the old focused frame
// loses focus and fires blur events.  Starting on a page with a cross-site
// subframe, simulate mouse clicks to switch focus from root frame to subframe
// and then back to root frame.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       CrossProcessFocusChangeFiresBlurEvents) {
  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/page_with_input_field.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  // Focus the main frame's text field.  The return value "input-focus"
  // indicates that the focus event was fired correctly.
  EXPECT_EQ("input-focus", EvalJs(shell(), "focusInputField()"));

  // The main frame should be focused.
  EXPECT_EQ(root, root->frame_tree().GetFocusedFrame());

  DOMMessageQueue msg_queue(web_contents());

  // Click on the cross-process subframe.
  SimulateMouseClick(
      root->child_at(0)->current_frame_host()->GetRenderWidgetHost(), 1, 1);

  // Check that the main frame lost focus and fired blur event on the input
  // text field.
  EXPECT_EQ(true, EvalJs(shell(), "waitForBlur()"));

  // The subframe should now be focused.
  EXPECT_EQ(root->child_at(0), root->frame_tree().GetFocusedFrame());

  // Click on the root frame.
  SimulateMouseClick(shell()
                         ->web_contents()
                         ->GetPrimaryMainFrame()
                         ->GetRenderViewHost()
                         ->GetWidget(),
                     1, 1);

  // Check that the subframe lost focus and fired blur event on its
  // document's body.
  std::string status;
  while (msg_queue.WaitForMessage(&status)) {
    if (status == "\"document-blur\"")
      break;
  }

  // The root frame should be focused again.
  EXPECT_EQ(root, root->frame_tree().GetFocusedFrame());
}

// Check that when a cross-process subframe is focused, its parent's
// document.activeElement correctly returns the corresponding <iframe> element.
// The test sets up an A-embed-B-embed-C page and shifts focus A->B->A->C,
// checking document.activeElement after each change.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, DocumentActiveElement) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   +--Site B ------- proxies for A C\n"
      "        +--Site C -- proxies for A B\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/\n"
      "      C = http://c.com/",
      DepictFrameTree(root));

  FrameTreeNode* child = root->child_at(0);
  FrameTreeNode* grandchild = root->child_at(0)->child_at(0);

  // The main frame should be focused to start with.
  EXPECT_EQ(root, root->frame_tree().GetFocusedFrame());

  // Focus the b.com frame.
  FocusFrame(child);
  EXPECT_EQ(child, root->frame_tree().GetFocusedFrame());

  // Helper function to check a property of document.activeElement in the
  // specified frame.
  auto verify_active_element_property = [](RenderFrameHost* rfh,
                                           const std::string& property,
                                           const std::string& expected_value) {
    std::string script = base::StringPrintf(
        "document.activeElement.%s.toLowerCase();", property.c_str());
    EXPECT_EQ(expected_value, EvalJs(rfh, script));
  };

  // Verify that document.activeElement on main frame points to the <iframe>
  // element for the b.com frame.
  RenderFrameHost* root_rfh = root->current_frame_host();
  verify_active_element_property(root_rfh, "tagName", "iframe");
  verify_active_element_property(root_rfh, "src", child->current_url().spec());

  // Focus the a.com main frame again.
  FocusFrame(root);
  EXPECT_EQ(root, root->frame_tree().GetFocusedFrame());

  // Main frame document's <body> should now be the active element.
  verify_active_element_property(root_rfh, "tagName", "body");

  // Now shift focus from main frame to c.com frame.
  FocusFrame(grandchild);

  // Check document.activeElement in main frame.  It should still point to
  // <iframe> for the b.com frame, since Blink computes the focused iframe
  // element by walking the parent chain of the focused frame until it hits the
  // current frame.  This logic should still work with remote frames.
  verify_active_element_property(root_rfh, "tagName", "iframe");
  verify_active_element_property(root_rfh, "src", child->current_url().spec());

  // Check document.activeElement in b.com subframe.  It should point to
  // <iframe> for the c.com frame.  This is a tricky case where B needs to find
  // out that focus changed from one remote frame to another (A to C).
  RenderFrameHost* child_rfh = child->current_frame_host();
  verify_active_element_property(child_rfh, "tagName", "iframe");
  verify_active_element_property(child_rfh, "src",
                                 grandchild->current_url().spec());
}

// Check that window.focus works for cross-process subframes.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, SubframeWindowFocus) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,c)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   |--Site B ------- proxies for A C\n"
      "   +--Site C ------- proxies for A B\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/\n"
      "      C = http://c.com/",
      DepictFrameTree(root));

  FrameTreeNode* child1 = root->child_at(0);
  FrameTreeNode* child2 = root->child_at(1);

  // The main frame should be focused to start with.
  EXPECT_EQ(root, root->frame_tree().GetFocusedFrame());

  // Register focus and blur events that will send messages when each frame's
  // window gets or loses focus, and configure some utility functions useful for
  // waiting for these messages.
  const char kSetupFocusEvents[] = R"(
      window.addEventListener('focus', function() {
        window.top.postMessage('%s-got-focus', '*');
      });
      window.addEventListener('blur', function() {
        window.top.postMessage('%s-lost-focus', '*');
      });
      function onEvent(target, eventName, property, value) {
        return new Promise((resolve, reject) => {
          function listener(event) {
            if (event[property] == value) {
              resolve();
              target.removeEventListener(eventName, listener);
            }
          };
          target.addEventListener(eventName, listener);
        });
      }
      function expectMessages(messageList) {
        var promiseList = messageList.map(
            (dataValue) => onEvent(window, 'message', 'data', dataValue));
        return Promise.all(promiseList);
      }
  )";
  std::string script = base::StringPrintf(kSetupFocusEvents, "main", "main");
  ExecuteScriptAsync(shell(), script);
  script = base::StringPrintf(kSetupFocusEvents, "child1", "child1");
  ExecuteScriptAsync(child1, script);
  script = base::StringPrintf(kSetupFocusEvents, "child2", "child2");
  ExecuteScriptAsync(child2, script);

  // Execute window.focus on the B subframe from the A main frame.
  // Process A should fire a blur event, and process B should fire a focus
  // event.  Wait for both events.
  EXPECT_EQ(true, EvalJs(root, R"((async function() {
      allMessages = [];
      window.addEventListener('message', (event) => {
        allMessages.push(event.data);
      });

      var messages = expectMessages(['main-lost-focus', 'child1-got-focus']);
      frames[0].focus();
      await messages;

      return allMessages.length == 2 || allMessages;
  })())"));

  EXPECT_EQ(child1, root->frame_tree().GetFocusedFrame());

  // Now, execute window.focus on the C subframe from A main frame.  This
  // checks that we can shift focus from one remote frame to another.
  //
  // Wait for the two subframes (B and C) to fire blur and focus events.
  EXPECT_EQ(true, EvalJs(root, R"((async function() {
      var messages = expectMessages(['child1-lost-focus', 'child2-got-focus']);
      frames[1].focus();
      await messages;
      return allMessages.length == 4 || allMessages;
  })())"));

  // The C subframe should now be focused.
  EXPECT_EQ(child2, root->frame_tree().GetFocusedFrame());

  // Install event listeners in the A main frame, expecting the main frame to
  // obtain focus.
  EXPECT_TRUE(
      ExecJs(root,
             "var messages = "
             "    expectMessages(['child2-lost-focus', 'main-got-focus']);"));

  // window.focus the main frame from the C subframe.
  ExecuteScriptAsync(child2, "parent.focus()");

  // Wait for the messages to arrive in the A main frame.
  EXPECT_EQ(true, EvalJs(root, R"((async function() {
      await messages;
      return allMessages.length == 6 || allMessages;
  })())"));

  // The main frame should now be focused.
  EXPECT_EQ(root, root->frame_tree().GetFocusedFrame());
}

// Check that when a subframe has focus, and another subframe navigates
// cross-site to a new renderer process, this doesn't reset the focused frame
// to the main frame.  See https://crbug.com/802156.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       SubframeFocusNotLostWhenAnotherFrameNavigatesCrossSite) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a,a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child1 = root->child_at(0);
  FrameTreeNode* child2 = root->child_at(1);

  // The main frame should be focused to start with.
  EXPECT_EQ(root, root->frame_tree().GetFocusedFrame());

  // Add an <input> element to the first subframe.
  ExecuteScriptAsync(
      child1, "document.body.appendChild(document.createElement('input'))");

  // Focus the first subframe using window.focus().
  FrameFocusedObserver focus_observer(child1->current_frame_host());
  ExecuteScriptAsync(root, "frames[0].focus()");
  focus_observer.Wait();
  EXPECT_EQ(child1, root->frame_tree().GetFocusedFrame());

  // Give focus to the <input> element in the first subframe.
  ExecuteScriptAsync(child1, "document.querySelector('input').focus()");

  // Now, navigate second subframe cross-site.  Ensure that this won't change
  // the focused frame.
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(child2, b_url));
  // This is needed because the incorrect focused frame change as in
  // https://crbug.com/802156 requires an additional post-commit IPC roundtrip.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(child1, root->frame_tree().GetFocusedFrame());

  // The <input> in first subframe should still be the activeElement.
  EXPECT_EQ(
      "input",
      base::ToLowerASCII(
          EvalJs(child1, "document.activeElement.tagName").ExtractString()));
}

// Tests that we are using the correct `blink::RemoteFrame` when navigating an
// opener window.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, OpenerSetLocation) {
  // Navigate the main window.
  GURL main_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), main_url);

  // Load cross-site page into a new window.
  GURL cross_url = embedded_test_server()->GetURL("foo.com", "/title1.html");
  Shell* popup = OpenPopup(shell(), cross_url, "");
  EXPECT_EQ(popup->web_contents()->GetLastCommittedURL(), cross_url);

  // Use new window to navigate main window.
  EXPECT_TRUE(
      ExecJs(popup, JsReplace("window.opener.location.href = $1", cross_url)));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), cross_url);
}

// crbug.com/1281755
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define MAYBE_NavigateProxyAndDetachBeforeProvisionalFrameCreation \
  DISABLED_NavigateProxyAndDetachBeforeProvisionalFrameCreation
#else
#define MAYBE_NavigateProxyAndDetachBeforeProvisionalFrameCreation \
  NavigateProxyAndDetachBeforeProvisionalFrameCreation
#endif
// Test for https://crbug.com/526304, where a parent frame executes a
// remote-to-local navigation on a child frame and immediately removes the same
// child frame.  This test exercises the path where the detach happens before
// the provisional local frame is created.
IN_PROC_BROWSER_TEST_P(
    SitePerProcessBrowserTest,
    MAYBE_NavigateProxyAndDetachBeforeProvisionalFrameCreation) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContents* contents = shell()->web_contents();
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(contents)->GetPrimaryFrameTree().root();
  EXPECT_EQ(2U, root->child_count());

  // Navigate the first child frame to 'about:blank' (which is a
  // remote-to-local transition), and then detach it.
  FrameDeletedObserver observer(root->child_at(0)->current_frame_host());
  std::string script =
      "var f = document.querySelector('iframe');"
      "f.contentWindow.location.href = 'about:blank';"
      "setTimeout(function() { document.body.removeChild(f); }, 0);";
  EXPECT_TRUE(ExecJs(root, script));
  observer.Wait();
  EXPECT_EQ(1U, root->child_count());

  // Make sure the main frame renderer does not crash and ignores the
  // navigation to the frame that's already been deleted.
  EXPECT_EQ(1, EvalJs(root, "frames.length"));
}

// Test for a variation of https://crbug.com/526304, where a child frame does a
// remote-to-local navigation, and the parent frame removes that child frame
// after the provisional local frame is created and starts to navigate, but
// before it commits.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       NavigateProxyAndDetachBeforeCommit) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContents* contents = shell()->web_contents();
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(contents)->GetPrimaryFrameTree().root();
  EXPECT_EQ(2U, root->child_count());
  FrameTreeNode* child = root->child_at(0);

  // Start a remote-to-local navigation for the child, but don't wait for
  // commit.
  GURL same_site_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  NavigationController::LoadURLParams params(same_site_url);
  params.transition_type = ui::PAGE_TRANSITION_LINK;
  params.frame_tree_node_id = child->frame_tree_node_id();
  child->navigator().controller().LoadURLWithParams(params);

  // Tell parent to remove the first child.  This should happen after the
  // previous navigation starts but before it commits.
  FrameDeletedObserver observer(child->current_frame_host());
  EXPECT_TRUE(ExecJs(
      root, "document.body.removeChild(document.querySelector('iframe'));"));
  observer.Wait();
  EXPECT_EQ(1U, root->child_count());

  // Make sure the a.com renderer does not crash.
  EXPECT_EQ(1, EvalJs(root, "frames.length;"));
}

// Similar to NavigateProxyAndDetachBeforeCommit, but uses a synchronous
// navigation to about:blank and the parent removes the child frame in a load
// event handler for the subframe.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, NavigateAboutBlankAndDetach) {
  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/remove_frame_on_load.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContents* contents = shell()->web_contents();
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(contents)->GetPrimaryFrameTree().root();
  EXPECT_EQ(1U, root->child_count());
  FrameTreeNode* child = root->child_at(0);
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());

  // Navigate the child frame to "about:blank" from the parent document and
  // wait for it to be removed.
  FrameDeletedObserver observer(child->current_frame_host());
  EXPECT_TRUE(
      ExecJs(root, base::StringPrintf("f.src = '%s'", url::kAboutBlankURL)));
  observer.Wait();

  // Make sure the a.com renderer does not crash and the frame is removed.
  EXPECT_EQ(0, EvalJs(root, "frames.length;"));
}

// This test ensures that the RenderFrame isn't leaked in the renderer process
// if a pending cross-process navigation is cancelled. The test works by trying
// to create a new RenderFrame with the same routing id. If there is an
// entry with the same routing ID, a CHECK is hit and the process crashes.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       SubframePendingAndBackToSameSiteInstance) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Capture the FrameTreeNode this test will be navigating.
  FrameTreeNode* node =
      web_contents()->GetPrimaryFrameTree().root()->child_at(0);
  EXPECT_TRUE(node);
  EXPECT_NE(node->current_frame_host()->GetSiteInstance(),
            node->parent()->GetSiteInstance());

  // Navigate to the site of the parent, but to a page that will not commit.
  GURL same_site_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  TestNavigationManager stalled_navigation(web_contents(), same_site_url);
  {
    NavigationController::LoadURLParams params(same_site_url);
    params.transition_type = ui::PAGE_TRANSITION_LINK;
    params.frame_tree_node_id = node->frame_tree_node_id();
    node->navigator().controller().LoadURLWithParams(params);
    EXPECT_TRUE(stalled_navigation.WaitForResponse());
  }

  // Grab the routing id of the pending RenderFrameHost and set up a process
  // observer to ensure there is no crash when a new RenderFrame creation is
  // attempted.
  RenderProcessHost* process =
      node->render_manager()->speculative_frame_host()->GetProcess();
  AgentSchedulingGroupHost* agent_scheduling_group =
      AgentSchedulingGroupHost::GetOrCreate(*node->render_manager()
                                                 ->speculative_frame_host()
                                                 ->GetSiteInstance()
                                                 ->group(),
                                            *process);
  RenderProcessHostWatcher watcher(
      process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  int frame_routing_id =
      node->render_manager()->speculative_frame_host()->GetRoutingID();
  blink::LocalFrameToken frame_token =
      node->render_manager()->speculative_frame_host()->GetFrameToken();
  blink::RemoteFrameToken previous_frame_token =
      node->render_manager()->GetProxyToParent()->GetFrameToken();

  // Now go to c.com so the navigation to a.com is cancelled and send an IPC
  // to create a new RenderFrame with the routing id of the previously pending
  // one.
  EXPECT_TRUE(NavigateToURLFromRenderer(
      node, embedded_test_server()->GetURL("c.com", "/title2.html")));
  {
    mojo::PendingAssociatedRemote<mojom::Frame> pending_frame;

    mojom::CreateFrameParamsPtr params = mojom::CreateFrameParams::New();
    params->routing_id = frame_routing_id;
    params->frame = pending_frame.InitWithNewEndpointAndPassReceiver();
    std::ignore = params->interface_broker.InitWithNewPipeAndPassReceiver();
    std::ignore = params->associated_interface_provider_remote
                      .InitWithNewEndpointAndPassReceiver();
    params->previous_frame_token = previous_frame_token;
    params->opener_frame_token = std::nullopt;
    params->parent_frame_token =
        shell()->web_contents()->GetPrimaryMainFrame()->GetFrameToken();
    params->frame_owner_properties = blink::mojom::FrameOwnerProperties::New();
    params->frame_token = frame_token;
    params->devtools_frame_token = base::UnguessableToken::Create();
    params->document_token = blink::DocumentToken();
    params->policy_container = CreateStubPolicyContainer();
    params->replication_state = blink::mojom::FrameReplicationState::New();
    agent_scheduling_group->CreateFrame(std::move(params));
  }

  // Disable the BackForwardCache to ensure the old process is going to be
  // released.
  DisableBackForwardCacheForTesting(web_contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);

  // The test must wait for the process to exit, but if there is no leak, the
  // RenderFrame will be properly created and there will be no crash.
  // Therefore, navigate the main frame to completely different site, which
  // will cause the original process to exit cleanly.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("d.com", "/title3.html")));
  watcher.Wait();
  EXPECT_TRUE(watcher.did_exit_normally());
}

// This test ensures that the RenderFrame isn't leaked in the renderer process
// when a remote parent detaches a child frame. The test works by trying
// to create a new RenderFrame with the same routing id. If there is an
// entry with the same routing ID, a CHECK is hit and the process crashes.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, ParentDetachRemoteChild) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsImpl* contents = web_contents();
  EXPECT_EQ(2U, contents->GetPrimaryFrameTree().root()->child_count());

  // Capture the FrameTreeNode this test will be navigating.
  FrameTreeNode* node = contents->GetPrimaryFrameTree().root()->child_at(0);
  EXPECT_TRUE(node);
  EXPECT_NE(node->current_frame_host()->GetSiteInstance(),
            node->parent()->GetSiteInstance());

  // Grab the routing id of the first child RenderFrameHost and set up a process
  // observer to ensure there is no crash when a new RenderFrame creation is
  // attempted.
  RenderProcessHost* process = node->current_frame_host()->GetProcess();
  AgentSchedulingGroupHost* agent_scheduling_group =
      AgentSchedulingGroupHost::GetOrCreate(
          *node->current_frame_host()->GetSiteInstance()->group(), *process);
  RenderProcessHostWatcher watcher(
      process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  int frame_routing_id = node->current_frame_host()->GetRoutingID();
  blink::LocalFrameToken frame_token =
      node->current_frame_host()->GetFrameToken();
  int widget_routing_id =
      node->current_frame_host()->GetRenderWidgetHost()->GetRoutingID();
  std::optional<blink::FrameToken> parent_frame_token =
      node->parent()
          ->frame_tree_node()
          ->render_manager()
          ->GetFrameTokenForSiteInstanceGroup(
              node->current_frame_host()->GetSiteInstance()->group());

  // Have the parent frame remove the child frame from its DOM. This should
  // result in the child RenderFrame being deleted in the remote process.
  EXPECT_TRUE(ExecJs(contents,
                     "document.body.removeChild("
                     "document.querySelectorAll('iframe')[0])"));
  EXPECT_EQ(1U, contents->GetPrimaryFrameTree().root()->child_count());

  {
    mojo::PendingAssociatedRemote<mojom::Frame> pending_frame;
    mojo::PendingAssociatedRemote<blink::mojom::FrameWidget> blink_frame_widget;
    mojo::PendingAssociatedRemote<blink::mojom::Widget> blink_widget;

    mojom::CreateFrameParamsPtr params = mojom::CreateFrameParams::New();
    params->routing_id = frame_routing_id;
    params->frame = pending_frame.InitWithNewEndpointAndPassReceiver();
    std::ignore = params->interface_broker.InitWithNewPipeAndPassReceiver();
    std::ignore = params->associated_interface_provider_remote
                      .InitWithNewEndpointAndPassReceiver();
    params->previous_frame_token = std::nullopt;
    params->opener_frame_token = std::nullopt;
    params->parent_frame_token = parent_frame_token;
    params->previous_sibling_frame_token = std::nullopt;
    params->frame_owner_properties = blink::mojom::FrameOwnerProperties::New();
    params->widget_params = mojom::CreateFrameWidgetParams::New();
    params->widget_params->routing_id = widget_routing_id;
    params->widget_params->frame_widget =
        blink_frame_widget.InitWithNewEndpointAndPassReceiver();
    params->widget_params->widget =
        blink_widget.InitWithNewEndpointAndPassReceiver();
    std::ignore = params->widget_params->frame_widget_host
                      .InitWithNewEndpointAndPassReceiver();
    std::ignore =
        params->widget_params->widget_host.InitWithNewEndpointAndPassReceiver();
    params->widget_params->visual_properties.screen_infos =
        display::ScreenInfos(display::ScreenInfo());
    params->replication_state = blink::mojom::FrameReplicationState::New();
    params->replication_state->name = "name";
    params->replication_state->unique_name = "name";
    params->frame_token = frame_token;
    params->devtools_frame_token = base::UnguessableToken::Create();
    params->document_token = blink::DocumentToken();
    params->policy_container = CreateStubPolicyContainer();
    agent_scheduling_group->CreateFrame(std::move(params));
  }

  // The test must wait for the process to exit, but if there is no leak, the
  // RenderFrame will be properly created and there will be no crash.
  // Therefore, navigate the remaining subframe to completely different site,
  // which will cause the original process to exit cleanly.
  EXPECT_TRUE(NavigateToURLFromRenderer(
      contents->GetPrimaryFrameTree().root()->child_at(0),
      embedded_test_server()->GetURL("d.com", "/title3.html")));
  watcher.Wait();
  EXPECT_TRUE(watcher.did_exit_normally());
}

// Verify that sandbox flags inheritance works across multiple levels of
// frames.  See https://crbug.com/576845.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, SandboxFlagsInheritance) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  // Set sandbox flags for child frame.
  EXPECT_TRUE(ExecJs(
      root, "document.querySelector('iframe').sandbox = 'allow-scripts';"));

  // Calculate expected flags.  Note that "allow-scripts" resets both
  // network::mojom::WebSandboxFlags::Scripts and
  // network::mojom::WebSandboxFlags::AutomaticFeatures bits per
  // blink::parseSandboxPolicy().
  network::mojom::WebSandboxFlags expected_flags =
      network::mojom::WebSandboxFlags::kAll &
      ~network::mojom::WebSandboxFlags::kScripts &
      ~network::mojom::WebSandboxFlags::kAutomaticFeatures;
  EXPECT_EQ(expected_flags,
            root->child_at(0)->pending_frame_policy().sandbox_flags);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            root->child_at(0)->effective_frame_policy().sandbox_flags);

  // Navigate child frame so that the sandbox flags take effect.  Use a page
  // with three levels of frames and make sure all frames properly inherit
  // sandbox flags.
  GURL frame_url(embedded_test_server()->GetURL(
      "b.com", "/cross_site_iframe_factory.html?b(c(d))"));
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), frame_url));

  // Wait for subframes to load as well.
  ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // Check each new frame's sandbox flags on the browser process side.
  FrameTreeNode* b_child = root->child_at(0);
  FrameTreeNode* c_child = b_child->child_at(0);
  FrameTreeNode* d_child = c_child->child_at(0);
  EXPECT_EQ(expected_flags, b_child->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(expected_flags, c_child->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(expected_flags, d_child->effective_frame_policy().sandbox_flags);

  // Check whether each frame is sandboxed on the renderer side, by seeing if
  // each frame's origin is unique ("null").
  EXPECT_EQ("null", GetOriginFromRenderer(b_child));
  EXPECT_EQ("null", GetOriginFromRenderer(c_child));
  EXPECT_EQ("null", GetOriginFromRenderer(d_child));
}

// Check that sandbox flags are not inherited before they take effect.  Create
// a child frame, update its sandbox flags but don't navigate the frame, and
// ensure that a new cross-site grandchild frame doesn't inherit the new flags
// (which shouldn't have taken effect).
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       SandboxFlagsNotInheritedBeforeNavigation) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  // Set sandbox flags for child frame.
  EXPECT_TRUE(ExecJs(
      root, "document.querySelector('iframe').sandbox = 'allow-scripts';"));

  // These flags should be pending but not take effect, since there's been no
  // navigation.
  network::mojom::WebSandboxFlags expected_flags =
      network::mojom::WebSandboxFlags::kAll &
      ~network::mojom::WebSandboxFlags::kScripts &
      ~network::mojom::WebSandboxFlags::kAutomaticFeatures;
  FrameTreeNode* child = root->child_at(0);
  EXPECT_EQ(expected_flags, child->pending_frame_policy().sandbox_flags);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            child->effective_frame_policy().sandbox_flags);

  // Add a new grandchild frame and navigate it cross-site.
  RenderFrameHostCreatedObserver frame_observer(shell()->web_contents(), 1);
  EXPECT_TRUE(ExecJs(
      child, "document.body.appendChild(document.createElement('iframe'));"));
  frame_observer.Wait();

  FrameTreeNode* grandchild = child->child_at(0);
  GURL frame_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  TestFrameNavigationObserver navigation_observer(grandchild);
  EXPECT_TRUE(NavigateToURLFromRenderer(grandchild, frame_url));
  navigation_observer.Wait();

  // Since the update flags haven't yet taken effect in its parent, this
  // grandchild frame should not be sandboxed.
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            grandchild->pending_frame_policy().sandbox_flags);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            grandchild->effective_frame_policy().sandbox_flags);

  // Check that the grandchild frame isn't sandboxed on the renderer side.  If
  // sandboxed, its origin would be unique ("null").
  EXPECT_EQ(GetExpectedOrigin("b.com"), GetOriginFromRenderer(grandchild));
}

// Verify that popups opened from sandboxed frames inherit sandbox flags from
// their opener, and that they keep these inherited flags after being navigated
// cross-site.  See https://crbug.com/483584.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       NewPopupInheritsSandboxFlagsFromOpener) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  // Set sandbox flags for child frame.
  EXPECT_TRUE(ExecJs(root,
                     "document.querySelector('iframe').sandbox = "
                     "    'allow-scripts allow-popups';"));

  // Calculate expected flags.  Note that "allow-scripts" resets both
  // network::mojom::WebSandboxFlags::Scripts and
  // network::mojom::WebSandboxFlags::AutomaticFeatures bits per
  // blink::parseSandboxPolicy().
  network::mojom::WebSandboxFlags expected_flags =
      network::mojom::WebSandboxFlags::kAll &
      ~network::mojom::WebSandboxFlags::kAutomaticFeatures &
      ~network::mojom::WebSandboxFlags::kPopups &
      ~network::mojom::WebSandboxFlags::kScripts &
      ~network::mojom::WebSandboxFlags::kTopNavigationToCustomProtocols;
  EXPECT_EQ(expected_flags,
            root->child_at(0)->pending_frame_policy().sandbox_flags);

  // Navigate child frame cross-site.  The sandbox flags should take effect.
  GURL frame_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  TestFrameNavigationObserver frame_observer(root->child_at(0));
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), frame_url));
  frame_observer.Wait();
  EXPECT_EQ(expected_flags,
            root->child_at(0)->effective_frame_policy().sandbox_flags);

  // Verify that they've also taken effect on the renderer side.  The sandboxed
  // frame's origin should be opaque.
  EXPECT_EQ("null", GetOriginFromRenderer(root->child_at(0)));
  const url::SchemeHostPort tuple_b(frame_url);
  const url::Origin sandbox_origin_b = root->child_at(0)->current_origin();
  EXPECT_TRUE(sandbox_origin_b.opaque());
  EXPECT_EQ(tuple_b, sandbox_origin_b.GetTupleOrPrecursorTupleIfOpaque());

  // Open a popup named "foo" from the sandboxed child frame.
  Shell* foo_shell =
      OpenPopup(root->child_at(0), GURL(url::kAboutBlankURL), "foo");
  EXPECT_TRUE(foo_shell);

  FrameTreeNode* foo_root =
      static_cast<WebContentsImpl*>(foo_shell->web_contents())
          ->GetPrimaryFrameTree()
          .root();

  // Check that the sandbox flags for new popup are correct in the browser
  // process.
  EXPECT_EQ(expected_flags, foo_root->effective_frame_policy().sandbox_flags);

  // The popup's origin should be opaque, since it's sandboxed, but cross-origin
  // from its opener.
  EXPECT_EQ("null", GetOriginFromRenderer(foo_root));
  url::Origin sandbox_origin_b2 = foo_root->current_origin();
  EXPECT_NE(sandbox_origin_b2, sandbox_origin_b);
  EXPECT_TRUE(sandbox_origin_b2.opaque());
  EXPECT_EQ(tuple_b, sandbox_origin_b2.GetTupleOrPrecursorTupleIfOpaque());

  // Navigate the popup cross-site.  This should be placed in an opaque origin
  // derived from c.com, and retain the inherited sandbox flags.
  GURL c_url(embedded_test_server()->GetURL("c.com", "/title1.html"));
  const url::SchemeHostPort tuple_c(c_url);
  {
    TestFrameNavigationObserver popup_observer(foo_root);
    EXPECT_TRUE(ExecJs(foo_root, JsReplace("location.href = $1", c_url)));
    popup_observer.Wait();
    EXPECT_EQ(c_url, foo_shell->web_contents()->GetLastCommittedURL());
  }

  // Confirm that the popup is still sandboxed, both on browser and renderer
  // sides.
  EXPECT_EQ(expected_flags, foo_root->effective_frame_policy().sandbox_flags);
  EXPECT_EQ("null", GetOriginFromRenderer(foo_root));
  const url::Origin sandbox_origin_c = foo_root->current_origin();
  EXPECT_NE(sandbox_origin_b, sandbox_origin_c);
  EXPECT_TRUE(sandbox_origin_c.opaque());
  EXPECT_EQ(tuple_c, sandbox_origin_c.GetTupleOrPrecursorTupleIfOpaque());

  // Navigate the popup back to b.com.  The popup should perform a
  // remote-to-local navigation in the b.com process, and keep an opaque
  // origin and the inherited sandbox flags.
  {
    TestFrameNavigationObserver popup_observer(foo_root);
    EXPECT_TRUE(ExecJs(foo_root, JsReplace("location.href = $1", frame_url)));
    popup_observer.Wait();
    EXPECT_EQ(frame_url, foo_shell->web_contents()->GetLastCommittedURL());
  }

  // Confirm that the popup is still sandboxed, both on browser and renderer
  // sides. This navigation should result in a new opaque origin derived
  // from b.com.
  EXPECT_EQ(expected_flags, foo_root->effective_frame_policy().sandbox_flags);
  EXPECT_EQ("null", GetOriginFromRenderer(foo_root));
  url::Origin sandbox_origin_b3 = foo_root->current_origin();
  EXPECT_TRUE(sandbox_origin_b3.opaque());
  EXPECT_EQ(tuple_b, sandbox_origin_b3.GetTupleOrPrecursorTupleIfOpaque());
  EXPECT_NE(sandbox_origin_b, sandbox_origin_b3);
  EXPECT_NE(sandbox_origin_b2, sandbox_origin_b3);
}

// Verify that popups opened from frames sandboxed with the
// "allow-popups-to-escape-sandbox" directive do *not* inherit sandbox flags
// from their opener.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       OpenUnsandboxedPopupFromSandboxedFrame) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  // Set sandbox flags for child frame, specifying that popups opened from it
  // should not be sandboxed.
  EXPECT_TRUE(ExecJs(
      root,
      "document.querySelector('iframe').sandbox = "
      "    'allow-scripts allow-popups allow-popups-to-escape-sandbox';"));

  // Set expected flags for the child frame.  Note that "allow-scripts" resets
  // both network::mojom::WebSandboxFlags::Scripts and
  // network::mojom::WebSandboxFlags::AutomaticFeatures bits per
  // blink::parseSandboxPolicy().
  network::mojom::WebSandboxFlags expected_flags =
      network::mojom::WebSandboxFlags::kAll &
      ~network::mojom::WebSandboxFlags::kScripts &
      ~network::mojom::WebSandboxFlags::kAutomaticFeatures &
      ~network::mojom::WebSandboxFlags::kPopups &
      ~network::mojom::WebSandboxFlags::kTopNavigationToCustomProtocols &
      ~network::mojom::WebSandboxFlags::kPropagatesToAuxiliaryBrowsingContexts;
  EXPECT_EQ(expected_flags,
            root->child_at(0)->pending_frame_policy().sandbox_flags);

  // Navigate child frame cross-site.  The sandbox flags should take effect.
  GURL frame_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  TestFrameNavigationObserver frame_observer(root->child_at(0));
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), frame_url));
  frame_observer.Wait();
  EXPECT_EQ(expected_flags,
            root->child_at(0)->effective_frame_policy().sandbox_flags);

  // Open a cross-site popup named "foo" from the child frame.
  GURL b_url(embedded_test_server()->GetURL("c.com", "/title1.html"));
  Shell* foo_shell = OpenPopup(root->child_at(0), b_url, "foo");
  EXPECT_TRUE(foo_shell);

  FrameTreeNode* foo_root =
      static_cast<WebContentsImpl*>(foo_shell->web_contents())
          ->GetPrimaryFrameTree()
          .root();

  // Check that the sandbox flags for new popup are correct in the browser
  // process.  They should not have been inherited.
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            foo_root->effective_frame_policy().sandbox_flags);
  // Check that the sandbox flags for the popup document are correct in the
  // browser process: None are set from the frame, none are set from the
  // navigation.
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            foo_root->current_frame_host()->active_sandbox_flags());

  // The popup's origin should match |b_url|, since it's not sandboxed.
  EXPECT_EQ(url::Origin::Create(b_url).Serialize(),
            EvalJs(foo_root, "self.origin;"));
}

// Verify that popup frames opened from sandboxed documents with the
// "allow-popups-to-escape-sandbox" directive do *not* inherit sandbox flags AND
// that local scheme documents do *not* inherit flags from the opener/initiator.
IN_PROC_BROWSER_TEST_P(
    SitePerProcessBrowserTest,
    OpenSandboxedDocumentInUnsandboxedPopupFromSandboxedFrame) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  // Set sandbox flags for child frame, specifying that popups opened from it
  // should not be sandboxed.
  EXPECT_TRUE(ExecJs(
      root,
      "document.querySelector('iframe').sandbox = "
      "    'allow-scripts allow-popups allow-popups-to-escape-sandbox';"));

  // Set expected flags for the child frame.  Note that "allow-scripts" resets
  // both network::mojom::WebSandboxFlags::Scripts and
  // network::mojom::WebSandboxFlags::AutomaticFeatures bits per
  // blink::parseSandboxPolicy().
  network::mojom::WebSandboxFlags expected_flags =
      network::mojom::WebSandboxFlags::kAll &
      ~network::mojom::WebSandboxFlags::kScripts &
      ~network::mojom::WebSandboxFlags::kAutomaticFeatures &
      ~network::mojom::WebSandboxFlags::kPopups &
      ~network::mojom::WebSandboxFlags::kTopNavigationToCustomProtocols &
      ~network::mojom::WebSandboxFlags::kPropagatesToAuxiliaryBrowsingContexts;
  EXPECT_EQ(expected_flags,
            root->child_at(0)->pending_frame_policy().sandbox_flags);

  // Navigate child frame cross-site.  The sandbox flags should take effect.
  GURL frame_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  TestFrameNavigationObserver frame_observer(root->child_at(0));
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), frame_url));
  frame_observer.Wait();
  EXPECT_EQ(expected_flags,
            root->child_at(0)->effective_frame_policy().sandbox_flags);

  // Open a popup named "foo" from the child frame on about:blank.
  GURL foo_url("about:blank");
  Shell* foo_shell = OpenPopup(root->child_at(0), foo_url, "foo");
  EXPECT_TRUE(foo_shell);

  FrameTreeNode* foo_root =
      static_cast<WebContentsImpl*>(foo_shell->web_contents())
          ->GetPrimaryFrameTree()
          .root();

  // Check that the sandbox flags for new popup frame are correct in the browser
  // process. They should not have been inherited.
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            foo_root->effective_frame_policy().sandbox_flags);
  // Check that the sandbox flags for the popup document are correct in the
  // browser process. They should not have been inherited (for about:blank).
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            foo_root->current_frame_host()->active_sandbox_flags());
}

// Verify that popup frames opened from sandboxed documents with the
// "allow-popups-to-escape-sandbox" directive do *not* inherit sandbox flags AND
// that local scheme documents do inherit CSP sandbox flags from the
// opener/initiator.
IN_PROC_BROWSER_TEST_P(
    SitePerProcessBrowserTest,
    OpenSandboxedDocumentInUnsandboxedPopupFromCSPSandboxedDocument) {
  GURL main_url = embedded_test_server()->GetURL(
      "a.test",
      "/set-header?"
      "Content-Security-Policy: sandbox "
      "allow-scripts allow-popups allow-popups-to-escape-sandbox");

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  // Set expected flags for the child frame.  Note that "allow-scripts" resets
  // both network::mojom::WebSandboxFlags::Scripts and
  // network::mojom::WebSandboxFlags::AutomaticFeatures bits per
  // blink::parseSandboxPolicy().
  network::mojom::WebSandboxFlags expected_flags =
      network::mojom::WebSandboxFlags::kAll &
      ~network::mojom::WebSandboxFlags::kScripts &
      ~network::mojom::WebSandboxFlags::kAutomaticFeatures &
      ~network::mojom::WebSandboxFlags::kPopups &
      ~network::mojom::WebSandboxFlags::kTopNavigationToCustomProtocols &
      ~network::mojom::WebSandboxFlags::kPropagatesToAuxiliaryBrowsingContexts;

  EXPECT_EQ(expected_flags, root->current_frame_host()->active_sandbox_flags());

  // Open a popup named "foo" from the child frame on about:blank.
  GURL foo_url("about:blank");
  Shell* foo_shell = OpenPopup(root, foo_url, "foo");
  EXPECT_TRUE(foo_shell);

  FrameTreeNode* foo_root =
      static_cast<WebContentsImpl*>(foo_shell->web_contents())
          ->GetPrimaryFrameTree()
          .root();

  // Check that the sandbox flags for new popup frame are correct in the browser
  // process. They should not have been inherited.
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            foo_root->effective_frame_policy().sandbox_flags);
  // Check that the sandbox flags for the popup document are correct in the
  // browser process. They should have been inherited.
  EXPECT_EQ(expected_flags,
            foo_root->current_frame_host()->active_sandbox_flags());
}

// Test that subresources with certificate errors get reported to the
// browser. That is, if https://example.test frames https://a.com which
// loads an image with certificate errors, the browser should be
// notified about the subresource with certificate errors and downgrade
// the UI appropriately.
// TODO(crbug.com/40705650): Flaky.
IN_PROC_BROWSER_TEST_P(SitePerProcessIgnoreCertErrorsBrowserTest,
                       DISABLED_SubresourceWithCertificateErrors) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  SetupCrossSiteRedirector(&https_server);
  ASSERT_TRUE(https_server.Start());

  GURL url(https_server.GetURL(
      "example.test",
      "/mixed-content/non-redundant-cert-error-in-iframe.html"));

  // The update of the security state can happen asynchronously after the
  // navigation finished, see https://crbug.com/1105145.
  VisibleSecurityStateObserver displayed_content_with_cert_errors_observer(
      shell()->web_contents(),
      base::BindRepeating([](WebContents* web_contents) {
        NavigationEntry* entry =
            web_contents->GetController().GetLastCommittedEntry();
        // The image that the iframe loaded had certificate errors also, so
        // the page should be marked as having displayed subresources with
        // cert errors.
        return entry && (entry->GetSSL().content_status &
                         SSLStatus::DISPLAYED_CONTENT_WITH_CERT_ERRORS) != 0;
      }));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  displayed_content_with_cert_errors_observer.Wait();

  NavigationEntry* entry =
      shell()->web_contents()->GetController().GetLastCommittedEntry();
  ASSERT_TRUE(entry);

  // The main page was loaded with certificate errors.
  EXPECT_TRUE(net::IsCertStatusError(entry->GetSSL().cert_status));
}

// Test setting a cross-origin iframe to display: none.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, CrossSiteIframeDisplayNone) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  RenderWidgetHost* root_render_widget_host =
      root->current_frame_host()->GetRenderWidgetHost();

  // Set the iframe to display: none.
  EXPECT_TRUE(ExecJs(
      shell(), "document.querySelector('iframe').style.display = 'none'"));

  // Waits until pending frames are done.
  std::unique_ptr<MainThreadFrameObserver> observer(
      new MainThreadFrameObserver(root_render_widget_host));
  observer->Wait();

  // Force the renderer to generate a new frame.
  EXPECT_TRUE(ExecJs(shell(), "document.body.style.background = 'black'"));

  // Waits for the next frame.
  observer->Wait();
}

// Test that a cross-origin iframe can be blocked by X-Frame-Options and CSP
// frame-ancestors.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       CrossSiteIframeBlockedByXFrameOptionsOrCSP) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  // Add a load event handler for the iframe element.
  EXPECT_TRUE(ExecJs(shell(),
                     "document.querySelector('iframe').onload = "
                     "    function() { document.title = 'loaded'; };"));

  // The blocked url reported in the console message should only contain the
  // origin, in order to avoid sensitive data being leaked to the parent frame.
  //
  // TODO(crbug.com/40053800): We should not leak any information at all
  // to the parent frame. Instead, we should send a message directly to Devtools
  // (without passing through a renderer): that can also contain more
  // information (like the full blocked url).
  GURL reported_blocked_url = embedded_test_server()->GetURL("b.com", "/");
  const struct {
    const char* url;
    bool use_error_page;
    std::string expected_console_message;
  } kTestCases[] = {
      {"/frame-ancestors-none.html", false,
       "Refused to frame '" + reported_blocked_url.spec() +
           "' because an ancestor violates the following Content Security "
           "Policy directive: \"frame-ancestors 'none'\".\n"},
      {"/x-frame-options-deny.html", true,
       "Refused to display '" + reported_blocked_url.spec() +
           "' in a frame because it set 'X-Frame-Options' to 'deny'."},
  };

  for (const auto& test : kTestCases) {
    GURL blocked_url = embedded_test_server()->GetURL("b.com", test.url);
    EXPECT_TRUE(ExecJs(shell(), "document.title = 'not loaded';"));
    std::u16string expected_title(u"loaded");
    TitleWatcher title_watcher(shell()->web_contents(), expected_title);

    WebContentsConsoleObserver console_observer(shell()->web_contents());
    console_observer.SetPattern("Refused to*");

    // Navigate the subframe to a blocked URL.
    TestNavigationObserver load_observer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(shell(),
                       JsReplace("frames[0].location.href = $1", blocked_url)));
    load_observer.Wait();

    // The blocked frame's origin should become unique.
    const url::Origin child_origin =
        root->child_at(0)->current_frame_host()->GetLastCommittedOrigin();
    EXPECT_TRUE(child_origin.opaque());
    EXPECT_EQ(url::Origin::Create(blocked_url.DeprecatedGetOriginAsURL())
                  .GetTupleOrPrecursorTupleIfOpaque(),
              child_origin.GetTupleOrPrecursorTupleIfOpaque());

    // X-Frame-Options and CSP frame-ancestors behave differently. XFO commits
    // an error page, while CSP commits a "data:," URL.
    // TODO(crbug.com/41405925): Use an error page for both.
    EXPECT_FALSE(load_observer.last_navigation_succeeded());
    EXPECT_EQ(net::ERR_BLOCKED_BY_RESPONSE,
              load_observer.last_net_error_code());
    EXPECT_EQ(root->child_at(0)->current_frame_host()->GetLastCommittedURL(),
              blocked_url);
    EXPECT_EQ("Error", EvalJs(root->child_at(0), "document.title"));

    // The blocked frame should still fire a load event in its parent's process.
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

    EXPECT_EQ(console_observer.GetMessageAt(0u), test.expected_console_message);

    // Check that the current RenderFrameHost has stopped loading.
    EXPECT_FALSE(root->child_at(0)->current_frame_host()->is_loading());

    // Navigate the subframe to another cross-origin page and ensure that this
    // navigation succeeds.  Use a renderer-initiated navigation to test the
    // transfer logic, which used to have some issues with this.
    GURL c_url(embedded_test_server()->GetURL("c.com", "/title1.html"));
    EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(), "child-0", c_url));
    EXPECT_EQ(c_url, root->child_at(0)->current_url());

    // When a page gets blocked due to XFO or CSP, it is sandboxed with the
    // SandboxOrigin flag (i.e., its origin is set to be unique) to ensure that
    // the blocked page is seen as cross-origin. However, those flags shouldn't
    // affect future navigations for a frame. Verify this for the above
    // navigation.
    EXPECT_EQ(c_url.DeprecatedGetOriginAsURL().spec(),
              root->child_at(0)->current_origin().Serialize() + "/");
    EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
              root->child_at(0)->effective_frame_policy().sandbox_flags);
  }
}

// Test that a cross-origin frame's navigation can be blocked by CSP frame-src.
// In this version of a test, CSP comes from HTTP headers.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       CrossSiteIframeBlockedByParentCSPFromHeaders) {
  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/frame-src-self-and-b.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  // Sanity-check that the test page has the expected shape for testing.
  GURL old_subframe_url(
      embedded_test_server()->GetURL("b.com", "/title2.html"));
  EXPECT_FALSE(root->child_at(0)->HasSameOrigin(*root));
  EXPECT_EQ(old_subframe_url, root->child_at(0)->current_url());
  const std::vector<network::mojom::ContentSecurityPolicyPtr>& root_csp =
      root->current_frame_host()
          ->policy_container_host()
          ->policies()
          .content_security_policies;
  EXPECT_EQ(1u, root_csp.size());
  EXPECT_EQ("frame-src 'self' http://b.com:*",
            root_csp[0]->header->header_value);

  // Monitor subframe's load events via main frame's title.
  EXPECT_TRUE(ExecJs(shell(),
                     "document.querySelector('iframe').onload = "
                     "    function() { document.title = 'loaded'; };"));
  EXPECT_TRUE(ExecJs(shell(), "document.title = 'not loaded';"));
  std::u16string expected_title(u"loaded");
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);

  // Try to navigate the subframe to a blocked URL.
  TestNavigationObserver load_observer(shell()->web_contents());
  GURL blocked_url = embedded_test_server()->GetURL("c.com", "/title3.html");
  EXPECT_TRUE(ExecJs(root->child_at(0),
                     JsReplace("window.location.href = $1", blocked_url)));

  // The blocked frame should still fire a load event in its parent's process.
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  // Check that the current RenderFrameHost has stopped loading.
  if (root->child_at(0)->current_frame_host()->is_loading())
    load_observer.Wait();

  // The last successful url shouldn't be the blocked url.
  EXPECT_NE(blocked_url,
            root->child_at(0)->current_frame_host()->last_successful_url());

  // The blocked frame should go to an error page. Errors currently commit
  // with the URL of the blocked page.
  EXPECT_EQ(blocked_url, root->child_at(0)->current_url());

  // The page should get the title of an error page (i.e "Error") and not the
  // title of the blocked page.
  EXPECT_EQ("Error", EvalJs(root->child_at(0), "document.title"));

  // Navigate to a URL without CSP.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));
}

// Test that a cross-origin frame's navigation can be blocked by CSP frame-src.
// In this version of a test, CSP comes from a <meta> element added after the
// page has already loaded.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       CrossSiteIframeBlockedByParentCSPFromMeta) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  // Navigate the subframe to a location we will disallow in the future.
  GURL old_subframe_url(
      embedded_test_server()->GetURL("b.com", "/title2.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), old_subframe_url));

  // Add frame-src CSP via a new <meta> element.
  EXPECT_TRUE(
      ExecJs(shell(),
             "var meta = document.createElement('meta');"
             "meta.httpEquiv = 'Content-Security-Policy';"
             "meta.content = 'frame-src https://a.com:*';"
             "document.getElementsByTagName('head')[0].appendChild(meta);"));

  // Sanity-check that the test page has the expected shape for testing.
  // (the CSP should not have an effect on the already loaded frames).
  EXPECT_FALSE(root->child_at(0)->HasSameOrigin(*root));
  EXPECT_EQ(old_subframe_url, root->child_at(0)->current_url());
  const std::vector<network::mojom::ContentSecurityPolicyPtr>& root_csp =
      root->current_frame_host()
          ->policy_container_host()
          ->policies()
          .content_security_policies;
  EXPECT_EQ(1u, root_csp.size());
  EXPECT_EQ("frame-src https://a.com:*", root_csp[0]->header->header_value);

  // Monitor subframe's load events via main frame's title.
  EXPECT_TRUE(ExecJs(shell(),
                     "document.querySelector('iframe').onload = "
                     "    function() { document.title = 'loaded'; };"));
  EXPECT_TRUE(ExecJs(shell(), "document.title = 'not loaded';"));
  std::u16string expected_title(u"loaded");
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);

  // Try to navigate the subframe to a blocked URL.
  TestNavigationObserver load_observer2(shell()->web_contents());
  GURL blocked_url = embedded_test_server()->GetURL("c.com", "/title3.html");
  EXPECT_TRUE(ExecJs(root->child_at(0),
                     JsReplace("window.location.href = $1;", blocked_url)));

  // The blocked frame should still fire a load event in its parent's process.
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  // Check that the current RenderFrameHost has stopped loading.
  if (root->child_at(0)->current_frame_host()->is_loading())
    load_observer2.Wait();

  // The last successful url shouldn't be the blocked url.
  EXPECT_NE(blocked_url,
            root->child_at(0)->current_frame_host()->last_successful_url());

  // The blocked frame should go to an error page. Errors currently commit
  // with the URL of the blocked page.
  EXPECT_EQ(blocked_url, root->child_at(0)->current_url());

  // The page should get the title of an error page (i.e "Error") and not the
  // title of the blocked page.
  EXPECT_EQ("Error", EvalJs(root->child_at(0), "document.title"));
}

// Test that a cross-origin frame's navigation can be blocked by CSP frame-src.
// In this version of a test, CSP is inherited by srcdoc iframe from a parent
// that declared CSP via HTTP headers.  Cross-origin frame navigating to a
// blocked location is a child of the srcdoc iframe.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       CrossSiteIframeBlockedByCSPInheritedBySrcDocParent) {
  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/frame-src-self-and-b.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* srcdoc_frame = root->child_at(1);
  EXPECT_TRUE(srcdoc_frame != nullptr);
  FrameTreeNode* navigating_frame = srcdoc_frame->child_at(0);
  EXPECT_TRUE(navigating_frame != nullptr);

  // Sanity-check that the test page has the expected shape for testing.
  // (the CSP should not have an effect on the already loaded frames).
  GURL old_subframe_url(
      embedded_test_server()->GetURL("b.com", "/title2.html"));
  EXPECT_TRUE(srcdoc_frame->HasSameOrigin(*root));
  EXPECT_FALSE(srcdoc_frame->HasSameOrigin(*navigating_frame));
  EXPECT_EQ(old_subframe_url, navigating_frame->current_url());
  const std::vector<network::mojom::ContentSecurityPolicyPtr>& srcdoc_csp =
      srcdoc_frame->current_frame_host()
          ->policy_container_host()
          ->policies()
          .content_security_policies;
  EXPECT_EQ(1u, srcdoc_csp.size());
  EXPECT_EQ("frame-src 'self' http://b.com:*",
            srcdoc_csp[0]->header->header_value);

  // Monitor navigating_frame's load events via srcdoc_frame posting
  // a message to the parent frame.
  EXPECT_TRUE(ExecJs(root,
                     "window.addEventListener('message', function(event) {"
                     "  document.title = event.data;"
                     "});"));
  EXPECT_TRUE(
      ExecJs(srcdoc_frame,
             "document.querySelector('iframe').onload = "
             "    function() { window.top.postMessage('loaded', '*'); };"));
  EXPECT_TRUE(ExecJs(shell(), "document.title = 'not loaded';"));
  std::u16string expected_title(u"loaded");
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);

  // Try to navigate the subframe to a blocked URL.
  TestNavigationObserver load_observer2(shell()->web_contents());
  GURL blocked_url = embedded_test_server()->GetURL("c.com", "/title3.html");
  EXPECT_TRUE(ExecJs(navigating_frame,
                     JsReplace("window.location.href = $1;", blocked_url)));

  // The blocked frame should still fire a load event in its parent's process.
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  // Check that the current RenderFrameHost has stopped loading.
  if (navigating_frame->current_frame_host()->is_loading())
    load_observer2.Wait();

  // The last successful url shouldn't be the blocked url.
  EXPECT_NE(blocked_url,
            navigating_frame->current_frame_host()->last_successful_url());

  // The blocked frame should go to an error page. Errors currently commit
  // with the URL of the blocked page.
  EXPECT_EQ(blocked_url, navigating_frame->current_url());

  // The page should get the title of an error page (i.e "Error") and not the
  // title of the blocked page.
  EXPECT_EQ("Error", EvalJs(navigating_frame, "document.title"));

  // Navigate the subframe to a URL without CSP.
  EXPECT_TRUE(NavigateToURLFromRenderer(
      srcdoc_frame, embedded_test_server()->GetURL("a.com", "/title1.html")));

  // Verify that the frame's CSP got correctly reset to an empty set.
  EXPECT_EQ(0u, srcdoc_frame->current_frame_host()
                    ->policy_container_host()
                    ->policies()
                    .content_security_policies.size());
}

// Tests that the state of the RenderViewHost is properly reset when the main
// frame is navigated to the same SiteInstance as one of its child frames.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       NavigateMainFrameToChildSite) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsImpl* contents = web_contents();
  FrameTreeNode* root = contents->GetPrimaryFrameTree().root();
  EXPECT_EQ(1U, root->child_count());

  // The test expect the BrowsingInstance to be kept across cross-site main
  // frame navigations. ProactivelySwapBrowsingInstance will provide a new one.
  // To prevent this, a popup is opened.
  if (CanCrossSiteNavigationsProactivelySwapBrowsingInstances()) {
    GURL popup_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
    EXPECT_TRUE(OpenPopup(root, popup_url, "foo"));
  }

  // Ensure the RenderViewHost for the SiteInstance of the child is considered
  // inactive.
  RenderViewHostImpl* rvh = contents->GetPrimaryFrameTree()
                                .GetRenderViewHost(root->child_at(0)
                                                       ->current_frame_host()
                                                       ->GetSiteInstance()
                                                       ->group())
                                .get();
  EXPECT_FALSE(rvh->is_active());

  // Have the child frame navigate its parent to its SiteInstance.
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  auto script = JsReplace("parent.location = $1", b_url);

  // Ensure the child has received a user gesture, so that it has permission
  // to framebust.
  SimulateMouseClick(
      root->child_at(0)->current_frame_host()->GetRenderWidgetHost(), 1, 1);
  TestFrameNavigationObserver frame_observer(root);
  EXPECT_TRUE(ExecJs(root->child_at(0), script));
  frame_observer.Wait();
  EXPECT_EQ(b_url, root->current_url());

  // Verify that the same RenderViewHost is preserved and that it is now active.
  EXPECT_EQ(rvh, contents->GetPrimaryFrameTree().GetRenderViewHost(
                     root->current_frame_host()->GetSiteInstance()->group()));
  EXPECT_TRUE(rvh->is_active());
}

// Test for https://crbug.com/568836.  From an A-embed-B page, navigate the
// subframe from B to A.  This cleans up the process for B, but the test delays
// the browser side from killing the B process right away.  This allows the
// B process to process the subframe's detached event and the disconnect
// of the blink::WebView's blink::mojom::PageBroadcast mojo channel. In the bug,
// the latter crashed while detaching the subframe's LocalFrame (triggered as
// part of closing the `blink::WebView`), because this tried to access the
// subframe's WebFrameWidget (from RenderFrameImpl::didChangeSelection), which
// had already been cleared by the former.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       CloseSubframeWidgetAndViewOnProcessExit) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // "Select all" in the subframe.  The bug only happens if there's a selection
  // change, which triggers the path through didChangeSelection.
  root->child_at(0)
      ->current_frame_host()
      ->GetRenderWidgetHost()
      ->GetFrameWidgetInputHandler()
      ->SelectAll();

  // Prevent b.com process from terminating right away once the subframe
  // navigates away from b.com below.  This is necessary so that the renderer
  // process has time to process the closings of RenderWidget and
  // `blink::WebView`, which is where the original bug was triggered.
  // Incrementing the keep alive ref count will cause
  // RenderProcessHostImpl::Cleanup to forego process termination.
  RenderProcessHostImpl* subframe_process = static_cast<RenderProcessHostImpl*>(
      root->child_at(0)->current_frame_host()->GetProcess());
  subframe_process->IncrementKeepAliveRefCount(0);

  // Navigate the subframe away from b.com.  Since this is the last active
  // frame in the b.com process, this causes the RenderWidget and
  // `blink::WebView` to be closed.
  EXPECT_TRUE(NavigateToURLFromRenderer(
      root->child_at(0),
      embedded_test_server()->GetURL("a.com", "/title1.html")));

  // Release the process.
  RenderProcessHostWatcher process_shutdown_observer(
      subframe_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  subframe_process->DecrementKeepAliveRefCount(0);
  process_shutdown_observer.Wait();
}

// Tests that an input event targeted to a out-of-process iframe correctly
// triggers a user interaction notification for WebContentsObservers.
// This is used for browser features such as download request limiting and
// launching multiple external protocol handlers, which can block repeated
// actions from a page when a user is not interacting with the page.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       UserInteractionForChildFrameTest) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  UserInteractionObserver observer(web_contents());

  // Target an event to the child frame's RenderWidgetHostView.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  SimulateMouseClick(
      root->child_at(0)->current_frame_host()->GetRenderWidgetHost(), 5, 5);

  EXPECT_TRUE(observer.WasUserInteractionReceived());

  // Target an event to the main frame.
  observer.Reset();
  SimulateMouseClick(root->current_frame_host()->GetRenderWidgetHost(), 1, 1);

  EXPECT_TRUE(observer.WasUserInteractionReceived());
}

// Ensures that navigating to data: URLs present in session history will
// correctly commit the navigation in the same process as the one used for the
// original navigation. See https://crbug.com/606996.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       NavigateSubframeToDataUrlInSessionHistory) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  EXPECT_EQ(2U, root->child_count());
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  TestNavigationObserver observer(shell()->web_contents());
  FrameTreeNode* child = root->child_at(0);

  // Navigate iframe to a data URL, which will commit in a new SiteInstance.
  GURL data_url("data:text/html,dataurl");
  EXPECT_TRUE(NavigateToURLFromRenderer(child, data_url));
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(data_url, observer.last_navigation_url());
  scoped_refptr<SiteInstanceImpl> orig_site_instance =
      child->current_frame_host()->GetSiteInstance();
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(), orig_site_instance);

  // Navigate it to another cross-site url.
  GURL cross_site_url(embedded_test_server()->GetURL("c.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(child, cross_site_url));
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(cross_site_url, observer.last_navigation_url());
  EXPECT_EQ(3, web_contents()->GetController().GetEntryCount());
  EXPECT_NE(orig_site_instance, child->current_frame_host()->GetSiteInstance());

  // Go back and ensure the data: URL committed in the same SiteInstance as the
  // original navigation.
  EXPECT_TRUE(web_contents()->GetController().CanGoBack());
  TestFrameNavigationObserver frame_observer(child);
  web_contents()->GetController().GoBack();
  frame_observer.WaitForCommit();
  EXPECT_EQ(orig_site_instance, child->current_frame_host()->GetSiteInstance());
}

// The site URL for a data: URL is the scheme + the serialized nonce from the
// origin. This means that two data: URLs with the same body will have different
// site URLs.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, DataUrlsHaveUniqueSiteURLs) {
  // Force process reuse for same-site URLs, to test whether identical data:
  // URLs share a process with each other.
  RenderProcessHost::SetMaxRendererProcessCount(1);

  // Load a main frame data: URL.
  GURL data_url("data:text/html,dataurl");
  EXPECT_TRUE(NavigateToURL(shell(), data_url));

  // Open another tab, then load the same data: URL in that tab. We need to
  // first navigate the new tab to a different page, a_url.
  // Shell::CreateNewWindow opens a new tab to about:blank, then loads the URL
  // passed in. Since the about:blank is in a new tab, it gets a new process,
  // and the passed-in URL keeps using that about:blank process. By navigating
  // from a_url to the data: URL, we exercise the flow that will reuse the
  // existing data: URL process, if possible.
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  ShellAddedObserver new_shell_observer;
  Shell* new_shell =
      Shell::CreateNewWindow(static_cast<NavigationControllerImpl&>(
                                 shell()->web_contents()->GetController())
                                 .GetBrowserContext(),
                             a_url, nullptr, gfx::Size());
  auto* new_contents = static_cast<WebContentsImpl*>(new_shell->web_contents());
  EXPECT_TRUE(WaitForLoadStop(new_contents));
  EXPECT_TRUE(NavigateToURL(new_shell, data_url));

  auto* main_frame = shell()->web_contents()->GetPrimaryMainFrame();
  auto* new_frame = new_shell->web_contents()->GetPrimaryMainFrame();
  GURL main_url = main_frame->GetSiteInstance()->GetSiteURL();
  GURL new_url = new_frame->GetSiteInstance()->GetSiteURL();
  EXPECT_NE(new_frame->GetSiteInstance(), main_frame->GetSiteInstance());

  // The site URL is the data scheme followed by a serialized nonce, which is
  // unique for every data: URL instance.
  EXPECT_NE(main_url, new_url);
  EXPECT_TRUE(main_url.SchemeIs(url::kDataScheme));
  EXPECT_EQ(new_url.GetContent().length(),
            base::UnguessableToken::Create().ToString().length());
  EXPECT_NE(new_frame->GetProcess(), main_frame->GetProcess());
}

// Ensures that subframes navigated to data: URLs start in a process based on
// their creator, but end up in unique processes after a restore (since
// SiteInstance relationships are not preserved on restore, until
// https://crbug.com/14987 is fixed).  This is better than restoring into the
// parent process, per https://crbug.com/863069.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       SubframeDataUrlsAfterRestore) {
  // We must use a page that has iframes in the HTML here, unlike
  // cross_site_iframe_factory.html which loads them dynamically.  In the latter
  // case, Chrome will not restore subframe URLs from history, which is needed
  // for this test.
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_two_iframes.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  EXPECT_EQ(2U, root->child_count());
  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   |--Site B ------- proxies for A C\n"
      "   +--Site C ------- proxies for A B\n"
      "Where A = http://a.com/\n"
      "      B = http://bar.com/\n"
      "      C = http://baz.com/",
      DepictFrameTree(root));

  FrameTreeNode* child_0 = root->child_at(0);
  FrameTreeNode* child_1 = root->child_at(1);
  scoped_refptr<SiteInstanceImpl> child_site_instance_0 =
      child_0->current_frame_host()->GetSiteInstance();
  scoped_refptr<SiteInstanceImpl> child_site_instance_1 =
      child_1->current_frame_host()->GetSiteInstance();

  // Navigate the iframes to data URLs via renderer initiated navigations, which
  // will commit in the existing SiteInstances.
  TestNavigationObserver observer(shell()->web_contents());
  GURL data_url_0("data:text/html,dataurl_0");
  {
    TestFrameNavigationObserver commit_observer(child_0);
    EXPECT_TRUE(ExecJs(child_0, JsReplace("location.href = $1", data_url_0)));
    commit_observer.WaitForCommit();
  }
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(data_url_0, observer.last_navigation_url());
  EXPECT_EQ(child_site_instance_0,
            child_0->current_frame_host()->GetSiteInstance());

  GURL data_url_1("data:text/html,dataurl_1");
  {
    TestFrameNavigationObserver commit_observer(child_1);
    EXPECT_TRUE(ExecJs(child_1, JsReplace("location.href = $1", data_url_1)));
    commit_observer.WaitForCommit();
  }
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(data_url_1, observer.last_navigation_url());
  EXPECT_EQ(child_site_instance_1,
            child_1->current_frame_host()->GetSiteInstance());

  // Grab the NavigationEntry and clone its PageState into a new entry for
  // restoring into a new tab.
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
  std::unique_ptr<NavigationEntryImpl> restored_entry =
      NavigationEntryImpl::FromNavigationEntry(
          NavigationController::CreateNavigationEntry(
              main_url, Referrer(), /* initiator_origin= */ std::nullopt,
              /* initiator_base_url= */ std::nullopt,
              ui::PAGE_TRANSITION_RELOAD, false, std::string(),
              controller.GetBrowserContext(),
              nullptr /* blob_url_loader_factory */));
  EXPECT_EQ(0U, restored_entry->root_node()->children.size());
  NavigationEntryRestoreContextImpl context;
  restored_entry->SetPageState(entry->GetPageState(), &context);
  ASSERT_EQ(2U, restored_entry->root_node()->children.size());

  // Restore the NavigationEntry into a new tab and check that the data URLs are
  // not loaded into the parent's SiteInstance.
  std::vector<std::unique_ptr<NavigationEntry>> entries;
  entries.push_back(std::move(restored_entry));
  Shell* new_shell = Shell::CreateNewWindow(controller.GetBrowserContext(),
                                            GURL(), nullptr, gfx::Size());
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
  ASSERT_EQ(2U, new_root->child_count());
  EXPECT_EQ(main_url, new_root->current_url());
  EXPECT_EQ("data", new_root->child_at(0)->current_url().scheme());
  EXPECT_EQ("data", new_root->child_at(1)->current_url().scheme());

  EXPECT_NE(new_root->current_frame_host()->GetSiteInstance(),
            new_root->child_at(0)->current_frame_host()->GetSiteInstance());
  EXPECT_NE(new_root->current_frame_host()->GetSiteInstance(),
            new_root->child_at(1)->current_frame_host()->GetSiteInstance());
  EXPECT_NE(new_root->child_at(0)->current_frame_host()->GetSiteInstance(),
            new_root->child_at(1)->current_frame_host()->GetSiteInstance());
}

// Similar to SubframeDataUrlsAfterRestore. Ensures that about:blank frames
// are not put into their parent process after restore if their initiator origin
// is different from the parent.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       SubframeBlankUrlsAfterRestore) {
  // We must use a page that has iframes in the HTML here, unlike
  // cross_site_iframe_factory.html which loads them dynamically.  In the latter
  // case, Chrome will not restore subframe URLs from history, which is needed
  // for this test.
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_two_iframes.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  EXPECT_EQ(2U, root->child_count());
  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   |--Site B ------- proxies for A C\n"
      "   +--Site C ------- proxies for A B\n"
      "Where A = http://a.com/\n"
      "      B = http://bar.com/\n"
      "      C = http://baz.com/",
      DepictFrameTree(root));

  FrameTreeNode* child_0 = root->child_at(0);
  FrameTreeNode* child_1 = root->child_at(1);
  scoped_refptr<SiteInstanceImpl> child_site_instance_0 =
      child_0->current_frame_host()->GetSiteInstance();
  scoped_refptr<SiteInstanceImpl> child_site_instance_1 =
      child_1->current_frame_host()->GetSiteInstance();

  // Navigate the iframes to about:blank URLs via renderer initiated
  // navigations, which will commit in the existing SiteInstances.
  TestNavigationObserver observer(shell()->web_contents());
  GURL blank_url("about:blank");
  {
    TestFrameNavigationObserver commit_observer(child_0);
    EXPECT_TRUE(ExecJs(child_0, JsReplace("location.href = $1", blank_url)));
    commit_observer.WaitForCommit();
  }
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(blank_url, observer.last_navigation_url());
  EXPECT_EQ(child_site_instance_0,
            child_0->current_frame_host()->GetSiteInstance());

  GURL blank_url_ref("about:blank#1");
  {
    TestFrameNavigationObserver commit_observer(child_1);
    EXPECT_TRUE(
        ExecJs(child_1, JsReplace("location.href = $1", blank_url_ref)));
    commit_observer.WaitForCommit();
  }
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(blank_url_ref, observer.last_navigation_url());
  EXPECT_EQ(child_site_instance_1,
            child_1->current_frame_host()->GetSiteInstance());

  // Grab the NavigationEntry and clone its PageState into a new entry for
  // restoring into a new tab.
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
  std::unique_ptr<NavigationEntryImpl> restored_entry =
      NavigationEntryImpl::FromNavigationEntry(
          NavigationController::CreateNavigationEntry(
              main_url, Referrer(), /* initiator_origin= */ std::nullopt,
              /* initiator_base_url= */ std::nullopt,
              ui::PAGE_TRANSITION_RELOAD, false, std::string(),
              controller.GetBrowserContext(),
              nullptr /* blob_url_loader_factory */));
  EXPECT_EQ(0U, restored_entry->root_node()->children.size());
  NavigationEntryRestoreContextImpl context;
  restored_entry->SetPageState(entry->GetPageState(), &context);
  ASSERT_EQ(2U, restored_entry->root_node()->children.size());

  // Restore the NavigationEntry into a new tab and check that the about:blank
  // URLs are not loaded into the parent's SiteInstance.
  std::vector<std::unique_ptr<NavigationEntry>> entries;
  entries.push_back(std::move(restored_entry));
  Shell* new_shell = Shell::CreateNewWindow(controller.GetBrowserContext(),
                                            GURL(), nullptr, gfx::Size());
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
  ASSERT_EQ(2U, new_root->child_count());
  EXPECT_EQ(main_url, new_root->current_url());
  auto* new_child_0 = new_root->child_at(0);
  auto* new_child_1 = new_root->child_at(1);
  EXPECT_TRUE(new_child_0->current_url().IsAboutBlank());
  EXPECT_TRUE(new_child_1->current_url().IsAboutBlank());

  // Restored frames should retain the origin from before restoring.
  EXPECT_EQ(new_root->current_frame_host()->GetLastCommittedOrigin(),
            root->current_frame_host()->GetLastCommittedOrigin());
  EXPECT_EQ(new_child_0->current_frame_host()
                ->GetLastCommittedOrigin()
                .GetTupleOrPrecursorTupleIfOpaque(),
            child_0->current_frame_host()
                ->GetLastCommittedOrigin()
                .GetTupleOrPrecursorTupleIfOpaque());
  EXPECT_EQ(new_child_1->current_frame_host()
                ->GetLastCommittedOrigin()
                .GetTupleOrPrecursorTupleIfOpaque(),
            child_1->current_frame_host()
                ->GetLastCommittedOrigin()
                .GetTupleOrPrecursorTupleIfOpaque());
  EXPECT_NE(child_0->current_frame_host()
                ->GetLastCommittedOrigin()
                .GetTupleOrPrecursorTupleIfOpaque(),
            child_1->current_frame_host()
                ->GetLastCommittedOrigin()
                .GetTupleOrPrecursorTupleIfOpaque());

  // Origin for child frames should match the navigation initiators.
  EXPECT_EQ(
      new_root->current_frame_host()->GetLastCommittedOrigin().Serialize(),
      GetOriginFromRenderer(new_root));
  EXPECT_EQ(GetExpectedOrigin("bar.com"), GetOriginFromRenderer(new_child_0));
  EXPECT_EQ(GetExpectedOrigin("baz.com"), GetOriginFromRenderer(new_child_1));

  // Since the origin for the frames are different, they all end up in different
  // SiteInstances.
  EXPECT_NE(new_root->current_frame_host()->GetSiteInstance(),
            new_child_0->current_frame_host()->GetSiteInstance());
  EXPECT_NE(new_root->current_frame_host()->GetSiteInstance(),
            new_child_1->current_frame_host()->GetSiteInstance());
  EXPECT_NE(new_child_0->current_frame_host()->GetSiteInstance(),
            new_child_1->current_frame_host()->GetSiteInstance());
}

// Similar to SubframeBlankUrlsAfterRestore, but ensures that about:srcdoc ends
// up in its parent's process after restore, since that's where its content
// comes from.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       SubframeSrcdocUrlAfterRestore) {
  // Load a page that uses iframe srcdoc.
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_srcdoc_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  EXPECT_EQ(1U, root->child_count());
  FrameTreeNode* child = root->child_at(0);
  scoped_refptr<SiteInstanceImpl> child_site_instance =
      child->current_frame_host()->GetSiteInstance();
  EXPECT_EQ(child_site_instance, root->current_frame_host()->GetSiteInstance());

  // Grab the NavigationEntry and clone its PageState into a new entry for
  // restoring into a new tab.
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
  std::unique_ptr<NavigationEntryImpl> restored_entry =
      NavigationEntryImpl::FromNavigationEntry(
          NavigationController::CreateNavigationEntry(
              main_url, Referrer(), /* initiator_origin= */ std::nullopt,
              /* initiator_base_url= */ std::nullopt,
              ui::PAGE_TRANSITION_RELOAD, false, std::string(),
              controller.GetBrowserContext(),
              nullptr /* blob_url_loader_factory */));
  EXPECT_EQ(0U, restored_entry->root_node()->children.size());
  NavigationEntryRestoreContextImpl context;
  restored_entry->SetPageState(entry->GetPageState(), &context);
  ASSERT_EQ(1U, restored_entry->root_node()->children.size());

  // Restore the NavigationEntry into a new tab and check that the srcdoc URLs
  // are still loaded into the parent's SiteInstance.
  std::vector<std::unique_ptr<NavigationEntry>> entries;
  entries.push_back(std::move(restored_entry));
  Shell* new_shell = Shell::CreateNewWindow(controller.GetBrowserContext(),
                                            GURL(), nullptr, gfx::Size());
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
  EXPECT_TRUE(new_root->child_at(0)->current_url().IsAboutSrcdoc());
  // Not only should the srcdoc inherit its base url from its initiator, but it
  // should also be properly restored from the session history.
  EXPECT_EQ(
      main_url,
      GURL(EvalJs(new_root->child_at(0), "document.baseURI").ExtractString()));

  EXPECT_EQ(new_root->current_frame_host()->GetSiteInstance(),
            new_root->child_at(0)->current_frame_host()->GetSiteInstance());
}

// Ensures that navigating to about:blank URLs present in session history will
// correctly commit the navigation in the same process as the one used for
// the original navigation.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       NavigateSubframeToAboutBlankInSessionHistory) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  EXPECT_EQ(2U, root->child_count());
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  TestNavigationObserver observer(shell()->web_contents());
  FrameTreeNode* child = root->child_at(0);

  // Navigate iframe to about:blank, which will commit in a new SiteInstance.
  GURL about_blank_url("about:blank");
  EXPECT_TRUE(NavigateToURLFromRenderer(child, about_blank_url));
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(about_blank_url, observer.last_navigation_url());
  scoped_refptr<SiteInstanceImpl> orig_site_instance =
    child->current_frame_host()->GetSiteInstance();
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(), orig_site_instance);

  // Navigate it to another cross-site url.
  GURL cross_site_url(embedded_test_server()->GetURL("c.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(child, cross_site_url));
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(cross_site_url, observer.last_navigation_url());
  EXPECT_EQ(3, web_contents()->GetController().GetEntryCount());
  EXPECT_NE(orig_site_instance, child->current_frame_host()->GetSiteInstance());

  // Go back and ensure the about:blank URL committed in the same SiteInstance
  // as the original navigation.
  EXPECT_TRUE(web_contents()->GetController().CanGoBack());
  TestFrameNavigationObserver frame_observer(child);
  web_contents()->GetController().GoBack();
  frame_observer.WaitForCommit();
  EXPECT_EQ(orig_site_instance, child->current_frame_host()->GetSiteInstance());
}

// Intercepts calls to LocalMainFrame's ShowCreatedWindow mojo method, and
// invokes the provided callback.
class ShowCreatedWindowInterceptor
    : public blink::mojom::LocalMainFrameHostInterceptorForTesting {
 public:
  // The caller has to guarantee that `render_frame_host` lives at least as long
  // as ShowCreatedWindowInterceptor.
  ShowCreatedWindowInterceptor(
      RenderFrameHostImpl* render_frame_host,
      base::OnceCallback<void(int32_t pending_widget_routing_id)> test_callback)
      : render_frame_host_(render_frame_host),
        test_callback_(std::move(test_callback)),
        swapped_impl_(
            render_frame_host_->local_main_frame_host_receiver_for_testing(),
            this) {}

  ~ShowCreatedWindowInterceptor() override = default;

  blink::mojom::LocalMainFrameHost* GetForwardingInterface() override {
    return swapped_impl_.old_impl();
  }

  void ShowCreatedWindow(const blink::LocalFrameToken& opener_frame_token,
                         WindowOpenDisposition disposition,
                         blink::mojom::WindowFeaturesPtr window_features,
                         bool user_gesture,
                         ShowCreatedWindowCallback callback) override {
    show_callback_ = std::move(callback);
    opener_frame_token_ = opener_frame_token;
    user_gesture_ = user_gesture;
    window_features_ = std::move(window_features);
    disposition_ = disposition;
    std::move(test_callback_)
        .Run(render_frame_host_->GetRenderWidgetHost()->GetRoutingID());
  }

  void ResumeShowCreatedWindow() {
    GetForwardingInterface()->ShowCreatedWindow(
        opener_frame_token_, disposition_, std::move(window_features_),
        user_gesture_, std::move(show_callback_));
  }

 private:
  raw_ptr<RenderFrameHostImpl> render_frame_host_;
  base::OnceCallback<void(int32_t pending_widget_routing_id)> test_callback_;
  ShowCreatedWindowCallback show_callback_;
  blink::LocalFrameToken opener_frame_token_;
  blink::mojom::WindowFeaturesPtr window_features_;
  bool user_gesture_ = false;
  WindowOpenDisposition disposition_;
  mojo::test::ScopedSwapImplForTesting<blink::mojom::LocalMainFrameHost>
      swapped_impl_;
};

// Listens for the source WebContents opening the new WebContents then attaches
// a show listener to the widget.
class NewWindowCreatedObserver : public WebContentsObserver {
 public:
  NewWindowCreatedObserver(
      WebContents* web_contents,
      base::OnceCallback<void(int32_t pending_widget_routing_id)> test_callback)
      : WebContentsObserver(web_contents),
        test_callback_(std::move(test_callback)) {}

  // WebContentsObserver overrides.
  void DidOpenRequestedURL(WebContents* new_contents,
                           RenderFrameHost* source_render_frame_host,
                           const GURL& url,
                           const Referrer& referrer,
                           WindowOpenDisposition disposition,
                           ui::PageTransition transition,
                           bool started_from_context_menu,
                           bool renderer_initiated) override {
    show_interceptor_ = std::make_unique<ShowCreatedWindowInterceptor>(
        static_cast<RenderFrameHostImpl*>(new_contents->GetPrimaryMainFrame()),
        std::move(test_callback_));

    // Stop observing now.
    Observe(nullptr);
  }

  void ResumeShowCreatedWindow() {
    show_interceptor_->ResumeShowCreatedWindow();
  }

 private:
  std::unique_ptr<ShowCreatedWindowInterceptor> show_interceptor_;
  base::OnceCallback<void(int32_t pending_widget_routing_id)> test_callback_;
};

// Test for https://crbug.com/612276.  Simultaneously open two new windows from
// two subframes in different processes, where each subframe process's next
// routing ID is the same.  Make sure that both windows are created properly.
//
// Each new window requires two IPCs to first create it (handled by
// CreateNewWindow) and then show it (ShowCreatedWindow).  In the bug, both
// CreateNewWindow calls arrived before the ShowCreatedWindow calls, resulting
// in the two pending windows colliding in the pending WebContents map, which
// used to be keyed only by routing_id.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       TwoSubframesCreatePopupsSimultaneously) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,c)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child1 = root->child_at(0);
  FrameTreeNode* child2 = root->child_at(1);
  RenderFrameHostImpl* frame1 = child1->current_frame_host();
  RenderFrameHostImpl* frame2 = child2->current_frame_host();
  RenderProcessHost* process1 = frame1->GetProcess();
  RenderProcessHost* process2 = frame2->GetProcess();

  // Call window.open simultaneously in both subframes to create two popups.
  // Wait for and then drop both ShowCreatedWindow messages.  This will ensure
  // that both CreateNewWindow calls happen before either ShowCreatedWindow
  // call.
  base::RunLoop run_loop1;
  int32_t routing_id1;
  NewWindowCreatedObserver interceptor1(
      web_contents(),
      base::BindLambdaForTesting([&](int32_t pending_widget_routing_id) {
        routing_id1 = pending_widget_routing_id;
        run_loop1.Quit();
      }));
  EXPECT_TRUE(ExecJs(child1, "window.open();"));
  run_loop1.Run();

  base::RunLoop run_loop2;
  int32_t routing_id2;
  NewWindowCreatedObserver interceptor2(
      web_contents(),
      base::BindLambdaForTesting([&](int32_t pending_widget_routing_id) {
        routing_id2 = pending_widget_routing_id;
        run_loop2.Quit();
      }));

  EXPECT_TRUE(ExecJs(child2, "window.open();"));
  run_loop2.Run();

  // At this point, we should have two pending WebContents.
  EXPECT_TRUE(base::Contains(web_contents()->pending_contents_,
                             GlobalRoutingID(process1->GetID(), routing_id1)));
  EXPECT_TRUE(base::Contains(web_contents()->pending_contents_,
                             GlobalRoutingID(process2->GetID(), routing_id2)));

  // Both subframes were set up in the same way, so the next routing ID for the
  // new popup windows should match up (this led to the collision in the
  // pending contents map in the original bug).
  EXPECT_EQ(routing_id1, routing_id2);

  // Now, resuming processing the show messages.
  interceptor1.ResumeShowCreatedWindow();
  interceptor2.ResumeShowCreatedWindow();

  // Verify that both shells were properly created.
  EXPECT_EQ(3u, Shell::windows().size());
}

// Intercepts calls to PopupWidgetHost's RequestClosePopup mojo method, and
// discards it. The caller has to guarantee that `render_widget_host` lives at
// least as long as RequestCloseWidgetInterceptor.
class RequestCloseWidgetInterceptor
    : public blink::mojom::PopupWidgetHostInterceptorForTesting {
 public:
  explicit RequestCloseWidgetInterceptor(
      RenderWidgetHostImpl* render_widget_host)
      : swapped_impl_(
            render_widget_host->popup_widget_host_receiver_for_testing(),
            this) {}

  ~RequestCloseWidgetInterceptor() override = default;

  blink::mojom::PopupWidgetHost* GetForwardingInterface() override {
    return swapped_impl_.old_impl();
  }

  void RequestClosePopup() override {}

 private:
  mojo::test::ScopedSwapImplForTesting<blink::mojom::PopupWidgetHost>
      swapped_impl_;
};

// Intercepts calls to PopupWidgetHost's ShowPopup mojo method, and
// invokes the provided callback. The caller has to guarantee that
// `render_widget_host` lives at least as long as
// ShowCreatedPopupWidgetInterceptor.
class ShowCreatedPopupWidgetInterceptor
    : public blink::mojom::PopupWidgetHostInterceptorForTesting {
 public:
  ShowCreatedPopupWidgetInterceptor(
      RenderWidgetHostImpl* render_widget_host,
      base::OnceCallback<void(int32_t pending_widget_routing_id)> test_callback)
      : render_widget_host_(render_widget_host),
        test_callback_(std::move(test_callback)),
        swapped_impl_(
            render_widget_host_->popup_widget_host_receiver_for_testing(),
            this) {}

  ~ShowCreatedPopupWidgetInterceptor() override = default;

  blink::mojom::PopupWidgetHost* GetForwardingInterface() override {
    return swapped_impl_.old_impl();
  }

  void ShowPopup(const gfx::Rect& initial_rect,
                 const gfx::Rect& initial_anchor_rect,
                 ShowPopupCallback callback) override {
    show_callback_ = std::move(callback);
    initial_rect_ = initial_rect;
    std::move(test_callback_).Run(render_widget_host_->GetRoutingID());
  }

  void ResumeShowPopupWidget() {
    // Let anchor have same origin as bounds, but its width and height should be
    // 1,1 as RenderWidgetHostViewAura sets OwnedWindowAnchorPosition as
    // kBottomLeft. Otherwise, the bottom left point of the |initial_rect|'s
    // size is going to be used as the origin of a popup.
    gfx::Rect anchor = initial_rect_;
    anchor.set_size({1, 1});
    GetForwardingInterface()->ShowPopup(initial_rect_, anchor,
                                        std::move(show_callback_));
  }

 private:
  raw_ptr<RenderWidgetHostImpl> render_widget_host_;
  base::OnceCallback<void(int32_t pending_widget_routing_id)> test_callback_;
  ShowPopupCallback show_callback_;
  gfx::Rect initial_rect_;
  mojo::test::ScopedSwapImplForTesting<blink::mojom::PopupWidgetHost>
      swapped_impl_;
};

// Listens for the source RenderFrameHost opening the new popup widget then
// attaches a show listener to the widget.
class NewPopupWidgetCreatedObserver {
 public:
  NewPopupWidgetCreatedObserver(
      RenderFrameHostImpl* frame_host,
      base::OnceCallback<void(int32_t pending_widget_routing_id)> test_callback)
      : create_new_popup_widget_interceptor_(
            frame_host,
            base::BindOnce(&NewPopupWidgetCreatedObserver::DidCreatePopupWidget,
                           base::Unretained(this))),
        test_callback_(std::move(test_callback)) {}

  void ResumeShowPopupWidget() { show_interceptor_->ResumeShowPopupWidget(); }

 private:
  void DidCreatePopupWidget(RenderWidgetHostImpl* widget) {
    show_interceptor_ = std::make_unique<ShowCreatedPopupWidgetInterceptor>(
        widget, std::move(test_callback_));
  }

  CreateNewPopupWidgetInterceptor create_new_popup_widget_interceptor_;
  std::unique_ptr<ShowCreatedPopupWidgetInterceptor> show_interceptor_;
  base::OnceCallback<void(int32_t pending_widget_routing_id)> test_callback_;
};

// Test for https://crbug.com/612276.  Similar to
// TwoSubframesOpenWindowsSimultaneously, but use popup menu widgets instead of
// windows.
//
// The plumbing that this test is verifying is not utilized on Mac/Android,
// where popup menus don't create a popup RenderWidget, but rather they trigger
// a FrameHostMsg_ShowPopup to ask the browser to build and display the actual
// popup using native controls.
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_ANDROID)
// Disable the test due to flaky: https://crbug.com/1126165
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_TwoSubframesCreatePopupMenuWidgetsSimultaneously \
  DISABLED_TwoSubframesCreatePopupMenuWidgetsSimultaneously
#else
#define MAYBE_TwoSubframesCreatePopupMenuWidgetsSimultaneously \
  TwoSubframesCreatePopupMenuWidgetsSimultaneously
#endif
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       MAYBE_TwoSubframesCreatePopupMenuWidgetsSimultaneously) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,c)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child1 = root->child_at(0);
  FrameTreeNode* child2 = root->child_at(1);
  RenderProcessHost* process1 = child1->current_frame_host()->GetProcess();
  RenderProcessHost* process2 = child2->current_frame_host()->GetProcess();

  // Navigate both subframes to a page with a <select> element.
  EXPECT_TRUE(NavigateToURLFromRenderer(
      child1, embedded_test_server()->GetURL(
                  "b.com", "/site_isolation/page-with-select.html")));
  EXPECT_TRUE(NavigateToURLFromRenderer(
      child2, embedded_test_server()->GetURL(
                  "c.com", "/site_isolation/page-with-select.html")));

  // Open both <select> menus by focusing each item and sending a space key
  // at the focused node. This creates a popup widget in both processes.
  // Wait for and then drop the ViewHostMsg_ShowWidget messages, so that both
  // widgets are left in pending-but-not-shown state.
  input::NativeWebKeyboardEvent event(
      blink::WebKeyboardEvent::Type::kChar, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  event.text[0] = ' ';

  base::RunLoop run_loop1;
  int32_t routing_id1;
  NewPopupWidgetCreatedObserver interceptor1(
      child1->current_frame_host(),
      base::BindLambdaForTesting([&](int32_t pending_widget_routing_id) {
        routing_id1 = pending_widget_routing_id;
        run_loop1.Quit();
      }));
  EXPECT_TRUE(ExecJs(child1, "focusSelectMenu();"));
  child1->current_frame_host()->GetRenderWidgetHost()->ForwardKeyboardEvent(
      event);
  run_loop1.Run();

  auto first_popup_global_id = GlobalRoutingID(process1->GetID(), routing_id1);
  // Add an interceptor for first popup widget so it doesn't get closed
  // immediately while the other one is being opened.
  EXPECT_TRUE(
      base::Contains(web_contents()->pending_widgets_, first_popup_global_id));

  RequestCloseWidgetInterceptor child1_popup_widget_interceptor(
      static_cast<RenderWidgetHostImpl*>(
          web_contents()->pending_widgets_[first_popup_global_id]));

  base::RunLoop run_loop2;
  int32_t routing_id2;
  NewPopupWidgetCreatedObserver interceptor2(
      child2->current_frame_host(),
      base::BindLambdaForTesting([&](int32_t pending_widget_routing_id) {
        routing_id2 = pending_widget_routing_id;
        run_loop2.Quit();
      }));
  EXPECT_TRUE(ExecJs(child2, "focusSelectMenu();"));
  child2->current_frame_host()->GetRenderWidgetHost()->ForwardKeyboardEvent(
      event);
  run_loop2.Run();

  // At this point, we should have two pending widgets.
  EXPECT_TRUE(
      base::Contains(web_contents()->pending_widgets_, first_popup_global_id));
  EXPECT_TRUE(base::Contains(web_contents()->pending_widgets_,
                             GlobalRoutingID(process2->GetID(), routing_id2)));

  // Both subframes were set up in the same way, so the next routing ID for the
  // new popup widgets should match up (this led to the collision in the
  // pending widgets map in the original bug).
  EXPECT_EQ(routing_id1, routing_id2);

  // Now simulate both widgets being shown.
  interceptor1.ResumeShowPopupWidget();
  interceptor2.ResumeShowPopupWidget();
  EXPECT_FALSE(base::Contains(web_contents()->pending_widgets_,
                              GlobalRoutingID(process1->GetID(), routing_id1)));
  EXPECT_FALSE(base::Contains(web_contents()->pending_widgets_,
                              GlobalRoutingID(process2->GetID(), routing_id2)));

  // There are posted tasks that must be run before the test shuts down, lest
  // they access deleted state.
  RunPostedTasks();
}
#endif

// Test for https://crbug.com/615575. It ensures that file chooser triggered
// by a document in an out-of-process subframe works properly.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, FileChooserInSubframe) {
  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)")));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  GURL url(embedded_test_server()->GetURL("b.com", "/file_input.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), url));

  // Use FileChooserDelegate to avoid showing the actual dialog and to respond
  // back to the renderer process with predefined file.
  base::RunLoop run_loop;
  base::FilePath file;
  EXPECT_TRUE(base::PathService::Get(base::DIR_TEMP, &file));
  file = file.AppendASCII("bar");
  std::unique_ptr<FileChooserDelegate> delegate(
      new FileChooserDelegate(file, run_loop.QuitClosure()));
  shell()->web_contents()->SetDelegate(delegate.get());
  EXPECT_TRUE(ExecJs(root->child_at(0),
                     "document.getElementById('fileinput').click();"));
  run_loop.Run();

  // Also, extract the file from the renderer process to ensure that the
  // response made it over successfully and the proper filename is set.
  EXPECT_EQ("bar",
            EvalJs(root->child_at(0),
                   "document.getElementById('fileinput').files[0].name;"));
}

// Test that the pending RenderFrameHost is canceled and destroyed when its
// process dies. Previously, reusing a top-level pending RFH which
// is not live was hitting a CHECK in CreateRenderView due to having neither a
// main frame routing ID nor a proxy routing ID.  See https://crbug.com/627400
// for more details.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       PendingRFHIsCanceledWhenItsProcessDies) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  // Open a popup at b.com.
  GURL popup_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  Shell* popup_shell = OpenPopup(root, popup_url, "foo");
  EXPECT_TRUE(popup_shell);

  // The RenderViewHost for b.com in the main tab should not be active.
  SiteInstanceGroup* b_group =
      static_cast<SiteInstanceImpl*>(
          popup_shell->web_contents()->GetSiteInstance())
          ->group();
  RenderViewHostImpl* rvh =
      web_contents()->GetPrimaryFrameTree().GetRenderViewHost(b_group).get();
  EXPECT_FALSE(rvh->is_active());

  // Navigate main tab to a b.com URL that will not commit.
  GURL stall_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  TestNavigationManager delayer(shell()->web_contents(), stall_url);
  EXPECT_TRUE(ExecJs(shell(), JsReplace("location = $1", stall_url)));
  delayer.WaitForSpeculativeRenderFrameHostCreation();

  // The pending RFH should be in the same process as the popup.
  RenderFrameHostImpl* pending_rfh =
      root->render_manager()->speculative_frame_host();
  RenderProcessHost* pending_process = pending_rfh->GetProcess();
  EXPECT_EQ(pending_process,
            popup_shell->web_contents()->GetPrimaryMainFrame()->GetProcess());

  // Kill the b.com process, currently in use by the pending RenderFrameHost
  // and the popup.
  RenderProcessHostWatcher crash_observer(
      pending_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  EXPECT_TRUE(pending_process->Shutdown(0));
  crash_observer.Wait();

  // The pending RFH should have been canceled and destroyed, so that it won't
  // be reused while it's not live in the next navigation.
  EXPECT_FALSE(root->render_manager()->speculative_frame_host());

  // Navigate main tab to b.com again.  This should not crash.
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title3.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), b_url));

  // The b.com RVH in the main tab should become active.
  EXPECT_TRUE(rvh->is_active());
}

// Test that killing a pending RenderFrameHost's process doesn't leave its
// RenderViewHost confused whether it's active or not for future navigations
// that try to reuse it.  See https://crbug.com/627893 for more details.
// Similar to the test above for https://crbug.com/627400, except the popup is
// navigated after pending RFH's process is killed, rather than the main tab.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       RenderViewHostKeepsSwappedOutStateIfPendingRFHDies) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  // Open a popup at b.com.
  GURL popup_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  Shell* popup_shell = OpenPopup(root, popup_url, "foo");
  EXPECT_TRUE(popup_shell);

  // The RenderViewHost for b.com in the main tab should not be active.
  SiteInstanceGroup* b_group =
      static_cast<SiteInstanceImpl*>(
          popup_shell->web_contents()->GetSiteInstance())
          ->group();
  RenderViewHostImpl* rvh =
      web_contents()->GetPrimaryFrameTree().GetRenderViewHost(b_group).get();
  EXPECT_FALSE(rvh->is_active());

  // Navigate main tab to a b.com URL that will not commit.
  GURL stall_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  NavigationHandleObserver handle_observer(shell()->web_contents(), stall_url);
  TestNavigationManager delayer(shell()->web_contents(), stall_url);
  EXPECT_TRUE(ExecJs(shell(), JsReplace("location = $1", stall_url)));
  delayer.WaitForSpeculativeRenderFrameHostCreation();

  // Kill the b.com process, currently in use by the pending RenderFrameHost
  // and the popup.
  RenderProcessHost* pending_process =
      popup_shell->web_contents()->GetPrimaryMainFrame()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      pending_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  EXPECT_TRUE(pending_process->Shutdown(0));
  crash_observer.Wait();

  // Since the navigation above didn't commit, the b.com RenderViewHost in the
  // main tab should still not be active.
  EXPECT_FALSE(rvh->is_active());
  EXPECT_EQ(net::ERR_ABORTED, handle_observer.net_error_code());

  // Navigate popup to b.com to recreate the b.com process.  When creating
  // opener proxies, |rvh| should be reused as a swapped out RVH.  In
  // https://crbug.com/627893, recreating the opener `blink::WebView` was
  // hitting a CHECK(params.swapped_out) in the renderer process, since its
  // RenderViewHost was brought into an active state by the navigation to
  // |stall_url| above, even though it never committed.
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title3.html"));
  EXPECT_TRUE(NavigateToURLInSameBrowsingInstance(popup_shell, b_url));
  EXPECT_FALSE(rvh->is_active());
}

// Test that a crashed subframe can be successfully navigated to the site it
// was on before crashing.  See https://crbug.com/634368.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       NavigateCrashedSubframeToSameSite) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);

  // Set up a postMessage handler in the main frame for later use.
  EXPECT_TRUE(ExecJs(
      root->current_frame_host(),
      "window.addEventListener('message',"
      "                        function(e) { document.title = e.data; });"));

  // Crash the subframe process.
  RenderProcessHost* child_process = child->current_frame_host()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      child_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  child_process->Shutdown(0);
  crash_observer.Wait();
  EXPECT_FALSE(child->current_frame_host()->IsRenderFrameLive());

  // When the subframe dies, its RenderWidgetHostView should be cleared and
  // reset in the CrossProcessFrameConnector.
  EXPECT_FALSE(child->current_frame_host()->GetView());
  RenderFrameProxyHost* proxy_to_parent =
      child->render_manager()->GetProxyToParent();
  EXPECT_FALSE(
      proxy_to_parent->cross_process_frame_connector()->get_view_for_testing());

  // Navigate the subframe to the same site it was on before crashing.  This
  // should reuse the subframe's current RenderFrameHost and reinitialize the
  // RenderFrame in a new process.
  NavigateFrameToURL(child,
                     embedded_test_server()->GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(child->current_frame_host()->IsRenderFrameLive());

  // The RenderWidgetHostView for the child should be recreated and set to be
  // used in the CrossProcessFrameConnector.  Without this, the frame won't be
  // rendered properly.
  EXPECT_TRUE(child->current_frame_host()->GetView());
  EXPECT_EQ(
      child->current_frame_host()->GetView(),
      proxy_to_parent->cross_process_frame_connector()->get_view_for_testing());

  // Make sure that the child frame has submitted a compositor frame
  RenderFrameSubmissionObserver frame_observer(child);
  frame_observer.WaitForMetadataChange();

  // Send a postMessage from the child to its parent.  This verifies that the
  // parent's proxy in the child's SiteInstance was also restored.
  std::u16string expected_title(u"I am alive!");
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);
  EXPECT_TRUE(ExecJs(child->current_frame_host(),
                     "parent.postMessage('I am alive!', '*');"));
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

// Test that session history length and offset are replicated to all renderer
// processes in a FrameTree.  This allows each renderer to see correct values
// for history.length, and to check the offset validity properly for
// navigations initiated via history.go(). See https:/crbug.com/501116.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, SessionHistoryReplication) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a,a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child1 = root->child_at(0);
  FrameTreeNode* child2 = root->child_at(1);
  GURL child_first_url(child1->current_url());
  EXPECT_EQ(child1->current_url(), child2->current_url());

  // Helper to retrieve the history length from a given frame.
  auto history_length = [](FrameTreeNode* ftn) {
    return EvalJs(ftn->current_frame_host(), "history.length;");
  };

  // All frames should see a history length of 1 to start with.
  EXPECT_EQ(1, history_length(root));
  EXPECT_EQ(1, history_length(child1));
  EXPECT_EQ(1, history_length(child2));

  // Navigate first child cross-site.  This increases history length to 2.
  EXPECT_TRUE(NavigateToURLFromRenderer(
      child1, embedded_test_server()->GetURL("b.com", "/title1.html")));
  EXPECT_EQ(2, history_length(root));
  EXPECT_EQ(2, history_length(child1));
  EXPECT_EQ(2, history_length(child2));

  // Navigate second child same-site.
  GURL child2_last_url(embedded_test_server()->GetURL("a.com", "/title2.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(child2, child2_last_url));
  EXPECT_EQ(3, history_length(root));
  EXPECT_EQ(3, history_length(child1));
  EXPECT_EQ(3, history_length(child2));

  // Navigate first child same-site to another b.com URL.
  GURL child1_last_url(embedded_test_server()->GetURL("b.com", "/title3.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(child1, child1_last_url));
  EXPECT_EQ(4, history_length(root));
  EXPECT_EQ(4, history_length(child1));
  EXPECT_EQ(4, history_length(child2));

  // Go back three entries using the history API from the main frame. This
  // checks that both history length and offset are not stale in a.com, as
  // otherwise this navigation might be dropped by Blink.
  EXPECT_TRUE(ExecJs(root, "history.go(-3);"));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(main_url, root->current_url());
  EXPECT_EQ(child_first_url, child1->current_url());
  EXPECT_EQ(child_first_url, child2->current_url());

  // Now go forward three entries from the child1 frame and check that the
  // history length and offset are not stale in b.com.
  EXPECT_TRUE(ExecJs(child1, "history.go(3);"));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(main_url, root->current_url());
  EXPECT_EQ(child1_last_url, child1->current_url());
  EXPECT_EQ(child2_last_url, child2->current_url());
}

// Intercepts calls to LocalFrameHost::DispatchLoad method(), and discards them.
class DispatchLoadInterceptor
    : public blink::mojom::LocalFrameHostInterceptorForTesting {
 public:
  explicit DispatchLoadInterceptor(RenderFrameHostImpl* render_frame_host)
      : swapped_impl_(
            render_frame_host->local_frame_host_receiver_for_testing(),
            this) {}

  ~DispatchLoadInterceptor() override = default;

  LocalFrameHost* GetForwardingInterface() override {
    return swapped_impl_.old_impl();
  }

  // Discard incoming calls to LocalFrameHost::DispatchLoad().
  void DispatchLoad() override {}

 private:
  mojo::test::ScopedSwapImplForTesting<blink::mojom::LocalFrameHost>
      swapped_impl_;
};

// Test that the renderer isn't killed when a frame generates a load event just
// after becoming pending deletion.  See https://crbug.com/636513.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       LoadEventForwardingWhilePendingDeletion) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);

  // Open a popup in the b.com process for later use.
  GURL popup_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  Shell* popup_shell = OpenPopup(root, popup_url, "foo");
  EXPECT_TRUE(popup_shell);

  // Navigate subframe to b.com.  Wait for commit but not full load.
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  {
    TestFrameNavigationObserver commit_observer(child);
    EXPECT_TRUE(ExecJs(child, JsReplace("location.href = $1", b_url)));
    commit_observer.WaitForCommit();
  }
  RenderFrameHostImpl* child_rfh = child->current_frame_host();
  child_rfh->DisableUnloadTimerForTesting();

  // At this point, the subframe should have a proxy in its parent's
  // SiteInstance, a.com.
  EXPECT_TRUE(child->render_manager()->GetProxyToParent());

  {
    // Intercept calls to the LocalFrameHost::DispatchLoad() method.
    DispatchLoadInterceptor interceptor(child_rfh);

    // Now, go back to a.com in the subframe and wait for commit.
    {
      TestFrameNavigationObserver commit_observer(child);
      web_contents()->GetController().GoBack();
      commit_observer.WaitForCommit();
    }

    // At this point, the subframe's old RFH for b.com should be pending
    // deletion, and the subframe's proxy in a.com should've been cleared.
    EXPECT_TRUE(child_rfh->IsPendingDeletion());
    EXPECT_FALSE(child->render_manager()->GetProxyToParent());

    // Simulate that the load event is dispatched from |child_rfh| just after
    // it's become pending deletion.
    child_rfh->DispatchLoad();
  }

  // In the bug, DispatchLoad killed the b.com renderer.  Ensure that this is
  // not the case. Note that the process kill doesn't happen immediately, so
  // IsRenderFrameLive() can't be checked here (yet).  Instead, check that
  // JavaScript can still execute in b.com using the popup.
  EXPECT_TRUE(ExecJs(popup_shell->web_contents(), "true"));
}

IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       RFHTransfersWhilePendingDeletion) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // Start a cross-process navigation and wait until the response is received.
  GURL cross_site_url_1 =
      embedded_test_server()->GetURL("b.com", "/title1.html");
  TestNavigationManager cross_site_manager(shell()->web_contents(),
                                           cross_site_url_1);
  shell()->web_contents()->GetController().LoadURL(
      cross_site_url_1, Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
  EXPECT_TRUE(cross_site_manager.WaitForResponse());

  // Start a renderer-initiated navigation to a cross-process url and make sure
  // the navigation will be blocked before being transferred.
  GURL cross_site_url_2 =
      embedded_test_server()->GetURL("c.com", "/title1.html");
  TestNavigationManager transfer_manager(shell()->web_contents(),
                                         cross_site_url_2);
  EXPECT_TRUE(ExecJs(root, JsReplace("location.href = $1", cross_site_url_2)));
  EXPECT_TRUE(transfer_manager.WaitForResponse());

  // Now have the cross-process navigation commit and mark the current RFH as
  // pending deletion.
  ASSERT_TRUE(cross_site_manager.WaitForNavigationFinished());

  // Resume the navigation in the previous RFH that has just been marked as
  // pending deletion. We should not crash.
  ASSERT_TRUE(transfer_manager.WaitForNavigationFinished());
}

class NavigationHandleWatcher : public WebContentsObserver {
 public:
  explicit NavigationHandleWatcher(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}
  void DidStartNavigation(NavigationHandle* navigation_handle) override {
    DCHECK_EQ(GURL("http://b.com/"),
              navigation_handle->GetStartingSiteInstance()->GetSiteURL());
  }
};

// Verifies that the SiteInstance of a NavigationHandle correctly identifies the
// RenderFrameHost that started the navigation (and not the destination RFH).
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       NavigationHandleSiteInstance) {
  // Navigate to a page with a cross-site iframe.
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Navigate the iframe cross-site.
  NavigationHandleWatcher watcher(shell()->web_contents());
  TestNavigationObserver load_observer(shell()->web_contents());
  GURL frame_url = embedded_test_server()->GetURL("c.com", "/title1.html");
  EXPECT_TRUE(ExecJs(shell()->web_contents(),
                     JsReplace("window.frames[0].location = $1", frame_url)));
  load_observer.Wait();
}

// Test that when canceling a pending RenderFrameHost in the middle of a
// redirect, and then killing the corresponding `blink::WebView`'s renderer
// process, the RenderViewHost isn't reused in an improper state later.
// Previously this led to a crash in CreateRenderView when recreating the
// `blink::WebView` due to a stale main frame routing ID.  See
// https://crbug.com/627400.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       ReuseNonLiveRenderViewHostAfterCancelPending) {
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  GURL c_url(embedded_test_server()->GetURL("c.com", "/title3.html"));

  EXPECT_TRUE(NavigateToURL(shell(), a_url));

  // Open a popup and navigate it to b.com.
  Shell* popup = OpenPopup(shell(), a_url, "popup");
  EXPECT_TRUE(NavigateToURLFromRenderer(popup, b_url));

  // Open a second popup and navigate it to b.com, which redirects to c.com.
  // The navigation to b.com will create a pending RenderFrameHost, which will
  // be canceled during the redirect to c.com.  Note that
  // NavigateToURLFromRenderer will return false because the committed URL
  // won't match the requested URL due to the redirect.
  Shell* popup2 = OpenPopup(shell(), a_url, "popup2");
  TestNavigationObserver observer(popup2->web_contents());
  GURL redirect_url(embedded_test_server()->GetURL(
      "b.com", "/server-redirect?" + c_url.spec()));
  EXPECT_FALSE(NavigateToURLFromRenderer(popup2, redirect_url));
  EXPECT_EQ(c_url, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());

  // Kill the b.com process (which currently hosts a `blink::RemoteFrame` that
  // replaced the pending RenderFrame in |popup2|, as well as the RenderFrame
  // for |popup|).
  RenderProcessHost* b_process =
      popup->web_contents()->GetPrimaryMainFrame()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      b_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  b_process->Shutdown(0);
  crash_observer.Wait();

  // Navigate the second popup to b.com.  This used to crash when creating the
  // `blink::WebView`, because it reused the RenderViewHost created by the
  // canceled navigation to b.com, and that RenderViewHost had a stale main
  // frame routing ID and active state.
  EXPECT_TRUE(NavigateToURLInSameBrowsingInstance(popup2, b_url));
}

// Check that after a pending RFH is canceled and replaced with a proxy (which
// reuses the canceled RFH's RenderViewHost), navigating to a main frame in the
// same site as the canceled RFH doesn't lead to a renderer crash.  The steps
// here are similar to ReuseNonLiveRenderViewHostAfterCancelPending, but don't
// involve crashing the renderer. See https://crbug.com/651980.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       RecreateMainFrameAfterCancelPending) {
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  GURL c_url(embedded_test_server()->GetURL("c.com", "/title3.html"));

  EXPECT_TRUE(NavigateToURL(shell(), a_url));

  // Open a popup and navigate it to b.com.
  Shell* popup = OpenPopup(shell(), a_url, "popup");
  EXPECT_TRUE(NavigateToURLFromRenderer(popup, b_url));

  // Open a second popup and navigate it to b.com, which redirects to c.com.
  // The navigation to b.com will create a pending RenderFrameHost, which will
  // be canceled during the redirect to c.com.  Note that NavigateToURL will
  // return false because the committed URL won't match the requested URL due
  // to the redirect.
  Shell* popup2 = OpenPopup(shell(), a_url, "popup2");
  TestNavigationObserver observer(popup2->web_contents());
  GURL redirect_url(embedded_test_server()->GetURL(
      "b.com", "/server-redirect?" + c_url.spec()));
  EXPECT_FALSE(NavigateToURLFromRenderer(popup2, redirect_url));
  EXPECT_EQ(c_url, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());

  // Navigate the second popup to b.com.  This used to crash the b.com renderer
  // because it failed to delete the canceled RFH's RenderFrame, so this caused
  // it to try to create a frame widget which already existed.
  EXPECT_TRUE(NavigateToURLFromRenderer(popup2, b_url));
}

// Check that when a pending RFH is canceled and a proxy needs to be created in
// its place, the proxy is properly initialized on the renderer side.  See
// https://crbug.com/653746.
// The test disables the delay of creating the speculative RFH since it requires
// the created RFH to be cancelld because of the cross-origin redirect.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTestWithoutSpeculativeRFHDelay,
                       CommunicateWithProxyAfterCancelPending) {
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  GURL c_url(embedded_test_server()->GetURL("c.com", "/title3.html"));

  EXPECT_TRUE(NavigateToURL(shell(), a_url));

  // Open a popup and navigate it to b.com.
  Shell* popup = OpenPopup(shell(), a_url, "popup");
  EXPECT_TRUE(NavigateToURLFromRenderer(popup, b_url));

  // Open a second popup and navigate it to b.com, which redirects to c.com.
  // The navigation to b.com will create a pending RenderFrameHost, which will
  // be canceled during the redirect to c.com.  Note that NavigateToURL will
  // return false because the committed URL won't match the requested URL due
  // to the redirect.
  Shell* popup2 = OpenPopup(shell(), a_url, "popup2");
  TestNavigationObserver observer(popup2->web_contents());
  GURL redirect_url(embedded_test_server()->GetURL(
      "b.com", "/server-redirect?" + c_url.spec()));
  EXPECT_FALSE(NavigateToURLFromRenderer(popup2, redirect_url));
  EXPECT_EQ(c_url, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());

  // Because b.com has other active frames (namely, the frame in |popup|),
  // there should be a proxy created for the canceled RFH, and it should be
  // live.
  SiteInstance* b_instance = popup->web_contents()->GetSiteInstance();
  FrameTreeNode* popup2_root =
      static_cast<WebContentsImpl*>(popup2->web_contents())
          ->GetPrimaryFrameTree()
          .root();
  RenderFrameProxyHost* proxy =
      popup2_root->current_frame_host()
          ->browsing_context_state()
          ->GetRenderFrameProxyHost(
              static_cast<SiteInstanceImpl*>(b_instance)->group());
  EXPECT_TRUE(proxy);
  EXPECT_TRUE(proxy->is_render_frame_proxy_live());

  // Add a postMessage listener in |popup2| (currently at a c.com URL).
  EXPECT_TRUE(ExecJs(popup2,
                     "window.addEventListener('message', function(event) {\n"
                     "  document.title=event.data;\n"
                     "});"));

  // Check that a postMessage can be sent via |proxy| above.  This needs to be
  // done from the b.com process.  |popup| is currently in b.com, but it can't
  // reach the window reference for |popup2| due to a security restriction in
  // Blink. So, navigate the main tab to b.com and then send a postMessage to
  // |popup2|. This is allowed since the main tab is |popup2|'s opener.
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), b_url));

  std::u16string expected_title(u"foo");
  TitleWatcher title_watcher(popup2->web_contents(), expected_title);
  EXPECT_TRUE(
      ExecJs(shell(), "window.open('','popup2').postMessage('foo', '*');"));
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       HeaderPolicyOnXSLTNavigation) {
  GURL url(embedded_test_server()->GetURL("a.com", "/permissions-policy.xml"));

  EXPECT_TRUE(NavigateToURL(shell(), url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  EXPECT_EQ(CreateParsedPermissionsPolicyMatchesSelf(
                {blink::mojom::PermissionsPolicyFeature::kGeolocation},
                url.DeprecatedGetOriginAsURL()),
            root->current_replication_state().permissions_policy_header);
}

IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       TestPolicyReplicationOnSameOriginNavigation) {
  GURL start_url(
      embedded_test_server()->GetURL("a.com", "/permissions-policy1.html"));
  GURL first_nav_url(
      embedded_test_server()->GetURL("a.com", "/permissions-policy2.html"));
  GURL second_nav_url(embedded_test_server()->GetURL("a.com", "/title2.html"));

  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  EXPECT_EQ(CreateParsedPermissionsPolicyMatchesSelf(
                {blink::mojom::PermissionsPolicyFeature::kGeolocation,
                 blink::mojom::PermissionsPolicyFeature::kPayment},
                start_url.DeprecatedGetOriginAsURL()),
            root->current_replication_state().permissions_policy_header);

  // When the main frame navigates to a page with a new policy, it should
  // overwrite the old one.
  EXPECT_TRUE(NavigateToURL(shell(), first_nav_url));
  EXPECT_EQ(CreateParsedPermissionsPolicyMatchesAll(
                {blink::mojom::PermissionsPolicyFeature::kGeolocation,
                 blink::mojom::PermissionsPolicyFeature::kPayment}),
            root->current_replication_state().permissions_policy_header);

  // When the main frame navigates to a page without a policy, the replicated
  // policy header should be cleared.
  EXPECT_TRUE(NavigateToURL(shell(), second_nav_url));
  EXPECT_TRUE(
      root->current_replication_state().permissions_policy_header.empty());
}

IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       TestPolicyReplicationOnCrossOriginNavigation) {
  GURL start_url(
      embedded_test_server()->GetURL("a.com", "/permissions-policy1.html"));
  GURL first_nav_url(
      embedded_test_server()->GetURL("b.com", "/permissions-policy2.html"));
  GURL second_nav_url(embedded_test_server()->GetURL("c.com", "/title2.html"));

  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  EXPECT_EQ(CreateParsedPermissionsPolicyMatchesSelf(
                {blink::mojom::PermissionsPolicyFeature::kGeolocation,
                 blink::mojom::PermissionsPolicyFeature::kPayment},
                start_url.DeprecatedGetOriginAsURL()),
            root->current_replication_state().permissions_policy_header);

  // When the main frame navigates to a page with a new policy, it should
  // overwrite the old one.
  EXPECT_TRUE(NavigateToURL(shell(), first_nav_url));
  EXPECT_EQ(CreateParsedPermissionsPolicyMatchesAll(
                {blink::mojom::PermissionsPolicyFeature::kGeolocation,
                 blink::mojom::PermissionsPolicyFeature::kPayment}),
            root->current_replication_state().permissions_policy_header);

  // When the main frame navigates to a page without a policy, the replicated
  // policy header should be cleared.
  EXPECT_TRUE(NavigateToURL(shell(), second_nav_url));
  EXPECT_TRUE(
      root->current_replication_state().permissions_policy_header.empty());
}

// Test that the replicated permissions policy header is correct in subframes as
// they navigate.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       TestPolicyReplicationFromRemoteFrames) {
  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/permissions-policy-main.html"));
  GURL first_nav_url(
      embedded_test_server()->GetURL("b.com", "/permissions-policy2.html"));
  GURL second_nav_url(embedded_test_server()->GetURL("c.com", "/title2.html"));

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  EXPECT_EQ(CreateParsedPermissionsPolicy(
                {blink::mojom::PermissionsPolicyFeature::kGeolocation,
                 blink::mojom::PermissionsPolicyFeature::kPayment},
                {GURL("http://example.com/")}, /*match_all_origins=*/false,
                main_url.DeprecatedGetOriginAsURL()),
            root->current_replication_state().permissions_policy_header);
  EXPECT_EQ(1UL, root->child_count());
  EXPECT_EQ(
      CreateParsedPermissionsPolicyMatchesSelf(
          {blink::mojom::PermissionsPolicyFeature::kGeolocation,
           blink::mojom::PermissionsPolicyFeature::kPayment},
          main_url.DeprecatedGetOriginAsURL()),
      root->child_at(0)->current_replication_state().permissions_policy_header);

  // Navigate the iframe cross-site.
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), first_nav_url));
  EXPECT_EQ(
      CreateParsedPermissionsPolicyMatchesAll(
          {blink::mojom::PermissionsPolicyFeature::kGeolocation,
           blink::mojom::PermissionsPolicyFeature::kPayment}),
      root->child_at(0)->current_replication_state().permissions_policy_header);

  // Navigate the iframe to another location, this one with no policy header
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), second_nav_url));
  EXPECT_TRUE(root->child_at(0)
                  ->current_replication_state()
                  .permissions_policy_header.empty());

  // Navigate the iframe back to a page with a policy
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), first_nav_url));
  EXPECT_EQ(
      CreateParsedPermissionsPolicyMatchesAll(
          {blink::mojom::PermissionsPolicyFeature::kGeolocation,
           blink::mojom::PermissionsPolicyFeature::kPayment}),
      root->child_at(0)->current_replication_state().permissions_policy_header);
}

// Test that the replicated permissions policy header is correct in remote
// proxies after the local frame has navigated.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       TestPermissionsPolicyReplicationToProxyOnNavigation) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_two_frames.html"));
  GURL first_nav_url(
      embedded_test_server()->GetURL("a.com", "/permissions-policy3.html"));
  GURL second_nav_url(
      embedded_test_server()->GetURL("a.com", "/permissions-policy4.html"));

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  EXPECT_TRUE(
      root->current_replication_state().permissions_policy_header.empty());
  EXPECT_EQ(2UL, root->child_count());
  EXPECT_TRUE(root->child_at(1)
                  ->current_replication_state()
                  .permissions_policy_header.empty());

  // Navigate the iframe to a page with a policy, and a nested cross-site iframe
  // (to the same site as a root->child_at(1) so that the render process already
  // exists.)
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(1), first_nav_url));
  EXPECT_EQ(
      CreateParsedPermissionsPolicyMatchesNone(
          {blink::mojom::PermissionsPolicyFeature::kGeolocation,
           blink::mojom::PermissionsPolicyFeature::kPayment}),
      root->child_at(1)->current_replication_state().permissions_policy_header);

  EXPECT_EQ(1UL, root->child_at(1)->child_count());

  // Ask the deepest iframe to report the enabled state of the geolocation
  // feature. If its parent frame's policy was replicated correctly to the
  // proxy, then this will be disabled. Otherwise, it will be enabled by the
  // "allow" attribute on the parent frame.
  EXPECT_EQ(false,
            EvalJs(root->child_at(1)->child_at(0),
                   "document.featurePolicy.allowsFeature('geolocation')"));

  // Now navigate the iframe to a page with no header policy, and the same
  // nested cross-site iframe. The header policy should be cleared in the proxy.
  // In this case, the frame policy from the parent will allow geolocation to be
  // delegated.
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(1), second_nav_url));
  EXPECT_TRUE(root->child_at(1)
                  ->current_replication_state()
                  .permissions_policy_header.empty());
  EXPECT_EQ(1UL, root->child_at(1)->child_count());

  // Ask the deepest iframe to report the enabled state of the geolocation
  // feature. If its parent frame's policy was replicated correctly to the
  // proxy, then this will now be allowed.
  EXPECT_EQ(true,
            EvalJs(root->child_at(1)->child_at(0),
                   "document.featurePolicy.allowsFeature('geolocation')"));
}

// Test that the constructed permissions policy is correct in sandboxed
// frames. Sandboxed frames have an opaque origin, and if the frame policy,
// which is constructed in the parent frame, cannot send that origin through
// the browser process to the sandboxed frame, then the sandboxed frame's
// policy will be incorrect.
//
// This is a regression test for https://crbug.com/690520
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       TestAllowAttributeInSandboxedFrame) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com",
      "/cross_site_iframe_factory.html?"
      "a(b{allow-geolocation,sandbox-allow-scripts})"));
  GURL nav_url(embedded_test_server()->GetURL("c.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  EXPECT_TRUE(
      root->current_replication_state().permissions_policy_header.empty());
  EXPECT_EQ(1UL, root->child_count());
  // Verify that the child frame is sandboxed with an opaque origin.
  EXPECT_TRUE(root->child_at(0)
                  ->current_frame_host()
                  ->GetLastCommittedOrigin()
                  .opaque());
  // And verify that the origin in the replication state is also opaque.
  EXPECT_TRUE(root->child_at(0)->current_origin().opaque());

  // Ask the sandboxed iframe to report the enabled state of the geolocation
  // feature. If the declared policy was correctly flagged as referring to the
  // opaque origin, then the policy in the sandboxed renderer will be
  // constructed correctly, and geolocation will be enabled in the sandbox.
  // Otherwise, it will be disabled, as geolocation is disabled by default in
  // cross-origin frames.
  EXPECT_EQ(true,
            EvalJs(root->child_at(0),
                   "document.featurePolicy.allowsFeature('geolocation');"));

  TestNavigationObserver load_observer(shell()->web_contents());
  EXPECT_TRUE(ExecJs(root->child_at(0),
                     JsReplace("document.location.href=$1", nav_url)));
  load_observer.Wait();

  // Verify that the child frame is sandboxed with an opaque origin.
  EXPECT_TRUE(root->child_at(0)
                  ->current_frame_host()
                  ->GetLastCommittedOrigin()
                  .opaque());
  // And verify that the origin in the replication state is also opaque.
  EXPECT_TRUE(root->child_at(0)->current_origin().opaque());

  EXPECT_EQ(true,
            EvalJs(root->child_at(0),
                   "document.featurePolicy.allowsFeature('geolocation');"));
}

// Test that the constructed permissions policy is correct in sandboxed
// frames. Sandboxed frames have an opaque origin, and if the frame policy,
// which is constructed in the parent frame, cannot send that origin through
// the browser process to the sandboxed frame, then the sandboxed frame's
// policy will be incorrect.
//
// This is a regression test for https://crbug.com/690520
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       TestAllowAttributeInOpaqueOriginAfterNavigation) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/page_with_data_iframe_and_allow.html"));
  GURL nav_url(embedded_test_server()->GetURL("c.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  EXPECT_TRUE(
      root->current_replication_state().permissions_policy_header.empty());
  EXPECT_EQ(1UL, root->child_count());
  // Verify that the child frame has an opaque origin.
  EXPECT_TRUE(root->child_at(0)
                  ->current_frame_host()
                  ->GetLastCommittedOrigin()
                  .opaque());
  // And verify that the origin in the replication state is also opaque.
  EXPECT_TRUE(root->child_at(0)->current_origin().opaque());

  // Verify that geolocation is enabled in the document.
  EXPECT_EQ(true,
            EvalJs(root->child_at(0),
                   "document.featurePolicy.allowsFeature('geolocation');"));

  TestNavigationObserver load_observer(shell()->web_contents());
  EXPECT_TRUE(ExecJs(root->child_at(0),
                     JsReplace("document.location.href=$1", nav_url)));
  load_observer.Wait();

  // Verify that the child frame no longer has an opaque origin.
  EXPECT_FALSE(root->child_at(0)
                   ->current_frame_host()
                   ->GetLastCommittedOrigin()
                   .opaque());
  // Verify that the origin in the replication state is also no longer opaque.
  EXPECT_FALSE(root->child_at(0)->current_origin().opaque());

  // Verify that the new document does not have geolocation enabled.
  EXPECT_EQ(false,
            EvalJs(root->child_at(0),
                   "document.featurePolicy.allowsFeature('geolocation');"));
}

// Ensure that an iframe that navigates cross-site doesn't use the same process
// as its parent. Then when its parent navigates it via the "srcdoc" attribute,
// it must reuse its parent's process.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       IframeSrcdocAfterCrossSiteNavigation) {
  GURL parent_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL child_url(embedded_test_server()->GetURL(
      "b.com", "/cross_site_iframe_factory.html?b()"));

  // #1 Navigate to a page with a cross-site iframe.
  EXPECT_TRUE(NavigateToURL(shell(), parent_url));

  // Ensure that the iframe uses its own process.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1u, root->child_count());
  FrameTreeNode* child = root->child_at(0);
  EXPECT_EQ(parent_url, root->current_url());
  EXPECT_EQ(child_url, child->current_url());
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_NE(root->current_frame_host()->GetProcess(),
            child->current_frame_host()->GetProcess());

  // #2 Navigate the iframe to its srcdoc attribute.
  TestNavigationObserver load_observer(shell()->web_contents());
  EXPECT_TRUE(ExecJs(
      root, "document.getElementById('child-0').srcdoc = 'srcdoc content';"));
  load_observer.Wait();

  // Ensure that the iframe reuses its parent's process.
  EXPECT_TRUE(child->current_url().IsAboutSrcdoc());
  EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_EQ(root->current_frame_host()->GetProcess(),
            child->current_frame_host()->GetProcess());
}

// Verify that a remote-to-local navigation in a crashed subframe works.  See
// https://crbug.com/487872.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       RemoteToLocalNavigationInCrashedSubframe) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);

  // Crash the subframe process.
  RenderProcessHost* child_process = child->current_frame_host()->GetProcess();
  {
    RenderProcessHostWatcher crash_observer(
        child_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    child_process->Shutdown(0);
    crash_observer.Wait();
  }
  EXPECT_FALSE(child->current_frame_host()->IsRenderFrameLive());

  // Do a remote-to-local navigation of the child frame from the parent frame.
  TestFrameNavigationObserver frame_observer(child);
  GURL frame_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(ExecJs(
      root, JsReplace("document.querySelector('iframe').src = $1", frame_url)));
  frame_observer.Wait();

  EXPECT_TRUE(child->current_frame_host()->IsRenderFrameLive());
  EXPECT_FALSE(child->IsLoading());
  EXPECT_EQ(child->current_frame_host()->GetSiteInstance(),
            root->current_frame_host()->GetSiteInstance());

  // Ensure the subframe is correctly attached in the frame tree, and that it
  // has correct content.
  EXPECT_EQ(1, EvalJs(root, "frames.length;"));

  EXPECT_EQ("This page has no title.",
            EvalJs(root, "frames[0].document.body.innerText;"));
}

// Tests that trying to open a context menu in the old RFH after commiting a
// navigation doesn't crash the browser. https://crbug.com/677266.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       ContextMenuAfterCrossProcessNavigation) {
  // Navigate to a.com.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));

  // Disable the unload ACK and the unload timer.
  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());
  auto unload_ack_filter = base::BindRepeating([] { return true; });
  rfh->SetUnloadACKCallbackForTesting(unload_ack_filter);
  rfh->DisableUnloadTimerForTesting();

  // Open a popup on a.com to keep the process alive.
  OpenPopup(shell(), embedded_test_server()->GetURL("a.com", "/title2.html"),
            "foo");

  // Cross-process navigation to b.com.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title3.html")));

  // Pretend that a.com just requested a context menu. This used to cause a
  // because the RenderWidgetHostView is destroyed when the frame is unloaded
  // and added to pending delete list.
  rfh->ShowContextMenu(mojo::NullAssociatedRemote(), ContextMenuParams());
}

// Test iframe container policy is replicated properly to the browser.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, ContainerPolicy) {
  GURL url(embedded_test_server()->GetURL("/allowed_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  EXPECT_EQ(0UL, root->effective_frame_policy().container_policy.size());
  EXPECT_EQ(
      0UL, root->child_at(0)->effective_frame_policy().container_policy.size());
  EXPECT_EQ(
      0UL, root->child_at(1)->effective_frame_policy().container_policy.size());
  EXPECT_EQ(
      2UL, root->child_at(2)->effective_frame_policy().container_policy.size());
  EXPECT_EQ(
      2UL, root->child_at(3)->effective_frame_policy().container_policy.size());
}

// Test dynamic updates to iframe "allow" attribute are propagated correctly.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, ContainerPolicyDynamic) {
  GURL main_url(embedded_test_server()->GetURL("/allowed_frames.html"));
  GURL nav_url(
      embedded_test_server()->GetURL("b.com", "/permissions-policy2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  EXPECT_EQ(
      2UL, root->child_at(2)->effective_frame_policy().container_policy.size());

  // Removing the "allow" attribute; pending policy should update, but effective
  // policy remains unchanged.
  EXPECT_TRUE(ExecJs(
      root, "document.getElementById('child-2').setAttribute('allow','')"));
  EXPECT_EQ(
      2UL, root->child_at(2)->effective_frame_policy().container_policy.size());
  EXPECT_EQ(0UL,
            root->child_at(2)->pending_frame_policy().container_policy.size());

  // Navigate the frame; pending policy should be committed.
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(2), nav_url));
  EXPECT_EQ(
      0UL, root->child_at(2)->effective_frame_policy().container_policy.size());
}

// Check that out-of-process frames correctly calculate the container policy in
// the renderer when navigating cross-origin. The policy should be unchanged
// when modified dynamically in the parent frame. When the frame is navigated,
// the new renderer should have the correct container policy.
//
// TODO(iclelland): Once there is a proper JS inspection API from the renderer,
// use that to check the policy. Until then, we test webkitFullscreenEnabled,
// which conveniently just returns the result of calling isFeatureEnabled on
// the fullscreen feature. Since there are no HTTP header policies involved,
// this verifies the presence of the container policy in the iframe.
// https://crbug.com/703703
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       ContainerPolicyCrossOriginNavigation) {
  WebContentsImpl* contents = web_contents();
  FrameTreeNode* root = contents->GetPrimaryFrameTree().root();

  // Helper to check if a frame is allowed to go fullscreen on the renderer
  // side.
  auto is_fullscreen_allowed = [](FrameTreeNode* ftn) {
    return EvalJs(ftn, "document.webkitFullscreenEnabled;");
  };

  // Load a page with an <iframe> without allowFullscreen.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   "a.com", "/cross_site_iframe_factory.html?a(b)")));

  // Dynamically enable fullscreen for the subframe and check that the
  // fullscreen property was updated on the FrameTreeNode.
  EXPECT_TRUE(ExecJs(
      root, "document.getElementById('child-0').allowFullscreen='true'"));

  // No change is expected to the container policy for dynamic modification of
  // a loaded frame.
  EXPECT_EQ(false, is_fullscreen_allowed(root->child_at(0)));

  // Cross-site navigation should update the container policy in the new render
  // frame.
  EXPECT_TRUE(NavigateToURLFromRenderer(
      root->child_at(0),
      embedded_test_server()->GetURL("c.com", "/title1.html")));
  EXPECT_EQ(true, is_fullscreen_allowed(root->child_at(0)));
}

// Test that dynamic updates to iframe sandbox attribute correctly set the
// replicated container policy.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       ContainerPolicySandboxDynamic) {
  GURL main_url(embedded_test_server()->GetURL("/allowed_frames.html"));
  GURL nav_url(
      embedded_test_server()->GetURL("b.com", "/permissions-policy2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  // Validate that the effective container policy contains a single non-unique
  // origin.
  const blink::ParsedPermissionsPolicy initial_effective_policy =
      root->child_at(2)->effective_frame_policy().container_policy;
  EXPECT_EQ(1UL, initial_effective_policy[0].allowed_origins.size());

  // Set the "sandbox" attribute; pending policy should update, and should now
  // be flagged as matching the opaque origin of the frame (without containing
  // an actual opaque origin, since the parent frame doesn't actually have that
  // origin yet) but the effective policy should remain unchanged.
  EXPECT_TRUE(ExecJs(
      root, "document.getElementById('child-2').setAttribute('sandbox','')"));
  const blink::ParsedPermissionsPolicy updated_effective_policy =
      root->child_at(2)->effective_frame_policy().container_policy;
  const blink::ParsedPermissionsPolicy updated_pending_policy =
      root->child_at(2)->pending_frame_policy().container_policy;
  EXPECT_EQ(1UL, updated_effective_policy[0].allowed_origins.size());
  EXPECT_TRUE(updated_pending_policy[0].matches_opaque_src);
  EXPECT_EQ(0UL, updated_pending_policy[0].allowed_origins.size());

  // Navigate the frame; pending policy should now be committed.
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(2), nav_url));
  const blink::ParsedPermissionsPolicy final_effective_policy =
      root->child_at(2)->effective_frame_policy().container_policy;
  EXPECT_TRUE(final_effective_policy[0].matches_opaque_src);
  EXPECT_EQ(0UL, final_effective_policy[0].allowed_origins.size());
}

// Test that creating a new remote frame at the same origin as its parent
// results in the correct permissions policy in the RemoteSecurityContext.
// https://crbug.com/852102
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       PermissionsPolicyConstructionInExistingProxy) {
  WebContentsImpl* contents = web_contents();
  FrameTreeNode* root = contents->GetPrimaryFrameTree().root();

  // Navigate to a page (1) with a cross-origin iframe (2). After load, the
  // frame tree should look like:
  //
  //    a.com(1)
  //   /
  // b.com(2)
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   "a.com", "/cross_site_iframe_factory.html?a(b)")));

  // Programmatically create a new same-origin frame (3) under the root, with a
  // cross-origin child (4). Since two SiteInstances already exist at this
  // point, a proxy for frame 3 will be created in the renderer for frames 2 and
  // 4. The frame tree should look like:
  //
  //    a.com(1)
  //   /      \
  // b.com(2) a.com(3)
  //                \
  //                b.com(4)
  auto create_subframe_script = JsReplace(
      "var f = document.createElement('iframe'); f.src=$1; "
      "document.body.appendChild(f);",
      embedded_test_server()->GetURL(
          "a.com", "/cross_site_iframe_factory.html?a(b{allow-autoplay})"));
  EXPECT_TRUE(ExecJs(root, create_subframe_script));
  EXPECT_TRUE(WaitForLoadStop(contents));

  // Verify the shape of the frame tree
  EXPECT_EQ(2UL, root->child_count());
  EXPECT_EQ(1UL, root->child_at(1)->child_count());

  // Ask frame 4 to report the enabled state of the autoplay feature. Frame 3's
  // policy should allow autoplay if created correctly, as it is same-origin
  // with the root, where the feature is enabled by default, and therefore
  // should be able to delegate it to frame 4.
  // This indirectly tests the replicated policy in frame 3: Because frame 4 is
  // cross-origin to frame 3, it will use the proxy's replicated policy as the
  // parent policy; otherwise we would just ask frame 3 to report its own state.
  EXPECT_EQ(true, EvalJs(root->child_at(1)->child_at(0),
                         "document.featurePolicy.allowsFeature('autoplay');"));
}

// Test harness that allows for "barrier" style delaying of requests matching
// certain paths. Call SetDelayedRequestsForPath to delay requests, then
// SetUpEmbeddedTestServer to register handlers and start the server.
class RequestDelayingSitePerProcessBrowserTest
    : public SitePerProcessBrowserTest {
 public:
  RequestDelayingSitePerProcessBrowserTest()
      : test_server_(std::make_unique<net::EmbeddedTestServer>()) {}

  // Must be called after any calls to SetDelayedRequestsForPath.
  void SetUpEmbeddedTestServer() {
    SetupCrossSiteRedirector(test_server_.get());
    test_server_->RegisterRequestHandler(base::BindRepeating(
        &RequestDelayingSitePerProcessBrowserTest::HandleMockResource,
        base::Unretained(this)));
    ASSERT_TRUE(test_server_->Start());
  }

  // Delays |num_delayed| requests with URLs whose path parts match |path|. When
  // the |num_delayed| + 1 request matching the path comes in, the rest are
  // unblocked.
  // Note: must be called on the UI thread before |test_server_| is started.
  void SetDelayedRequestsForPath(const std::string& path, int num_delayed) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(!test_server_->Started());
    num_remaining_requests_to_delay_for_path_[path] = num_delayed;
  }

 private:
  // Called on the test server's thread.
  void AddDelayedResponse(
      base::WeakPtr<net::test_server::HttpResponseDelegate> delegate) {
    response_closures_.push_back(base::BindOnce(
        &net::test_server::HttpResponseDelegate::SendHeadersContentAndFinish,
        delegate, net::HTTP_OK, "OK", base::StringPairs(), ""));
  }

  // Custom embedded test server handler. Looks for requests matching
  // num_remaining_requests_to_delay_for_path_, and delays them if necessary. As
  // soon as a single request comes in and:
  // 1) It matches a delayed path
  // 2) No path has any more requests to delay
  // Then we release the barrier and finish all delayed requests.
  std::unique_ptr<net::test_server::HttpResponse> HandleMockResource(
      const net::test_server::HttpRequest& request) {
    auto it =
        num_remaining_requests_to_delay_for_path_.find(request.GetURL().path());
    if (it == num_remaining_requests_to_delay_for_path_.end())
      return nullptr;

    // If there are requests to delay for this path, make a delayed request
    // which will be finished later. Otherwise fall through to the bottom and
    // send an empty response.
    if (it->second > 0) {
      --it->second;
      return std::make_unique<DelayedResponse>(this);
    }
    MaybeStartRequests();
    return nullptr;
  }

  // If there are no more requests to delay, post a series of tasks finishing
  // all the delayed tasks. This will be called on the test server's thread.
  void MaybeStartRequests() {
    for (auto it : num_remaining_requests_to_delay_for_path_) {
      if (it.second > 0)
        return;
    }
    for (auto& it : response_closures_)
      std::move(it).Run();
  }

  // This class passes the delegates needed to respond to a request to the
  // underlying test fixture.
  class DelayedResponse : public net::test_server::BasicHttpResponse {
   public:
    explicit DelayedResponse(
        RequestDelayingSitePerProcessBrowserTest* test_harness)
        : test_harness_(test_harness) {}

    DelayedResponse(const DelayedResponse&) = delete;
    DelayedResponse& operator=(const DelayedResponse&) = delete;

    void SendResponse(base::WeakPtr<net::test_server::HttpResponseDelegate>
                          delegate) override {
      test_harness_->AddDelayedResponse(delegate);
    }

   private:
    raw_ptr<RequestDelayingSitePerProcessBrowserTest> test_harness_;
  };

  // Set of delegates to call which will complete delayed requests. May only be
  // modified on the test_server_'s thread.
  std::vector<base::OnceClosure> response_closures_;

  // Map from URL paths to the number of requests to delay for that particular
  // path. Initialized on the UI thread but modified and read on the test
  // server's thread after the |test_server_| is started.
  std::map<std::string, int> num_remaining_requests_to_delay_for_path_;

  // Don't use embedded_test_server() because this one requires custom
  // initialization.
  std::unique_ptr<net::EmbeddedTestServer> test_server_;
};

// Regression tests for https://crbug.com/678206, where the request throttling
// in ResourceScheduler was not updated for OOPIFs. This resulted in a single
// hung delayable request (e.g. video) starving all other delayable requests.
// The tests work by delaying n requests in a cross-domain iframe. Once the n +
// 1st request goes through to the network stack (ensuring it was not starved),
// the delayed request completed.
//
// If the logic is not correct, these tests will time out, as the n + 1st
// request will never start.
IN_PROC_BROWSER_TEST_P(RequestDelayingSitePerProcessBrowserTest,
                       DelayableSubframeRequestsOneFrame) {
  std::string path = "/mock-video.mp4";
  SetDelayedRequestsForPath(path, 2);
  SetUpEmbeddedTestServer();
  GURL url(embedded_test_server()->GetURL(
      "a.com", base::StringPrintf("/site_isolation/"
                                  "subframes_with_resources.html?urls=%s&"
                                  "numSubresources=3",
                                  path.c_str())));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_EQ(true, EvalJs(shell(), "createFrames()"));
}

IN_PROC_BROWSER_TEST_P(RequestDelayingSitePerProcessBrowserTest,
                       DelayableSubframeRequestsTwoFrames) {
  std::string path0 = "/mock-video0.mp4";
  std::string path1 = "/mock-video1.mp4";
  SetDelayedRequestsForPath(path0, 2);
  SetDelayedRequestsForPath(path1, 2);
  SetUpEmbeddedTestServer();
  GURL url(embedded_test_server()->GetURL(
      "a.com", base::StringPrintf("/site_isolation/"
                                  "subframes_with_resources.html?urls=%s,%s&"
                                  "numSubresources=3",
                                  path0.c_str(), path1.c_str())));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_EQ(true, EvalJs(shell(), "createFrames()"));
}

#if BUILDFLAG(IS_ANDROID)
class TextSelectionObserver : public TextInputManager::Observer {
 public:
  explicit TextSelectionObserver(TextInputManager* text_input_manager)
      : text_input_manager_(text_input_manager) {
    text_input_manager->AddObserver(this);
  }

  TextSelectionObserver(const TextSelectionObserver&) = delete;
  TextSelectionObserver& operator=(const TextSelectionObserver&) = delete;

  ~TextSelectionObserver() { text_input_manager_->RemoveObserver(this); }

  void WaitForSelectedText(const std::string& expected_text) {
    if (last_selected_text_ == expected_text)
      return;
    expected_text_ = expected_text;
    loop_runner_ = new MessageLoopRunner();
    loop_runner_->Run();
  }

 private:
  void OnTextSelectionChanged(TextInputManager* text_input_manager,
                              RenderWidgetHostViewBase* updated_view) override {
    last_selected_text_ = base::UTF16ToUTF8(
        text_input_manager->GetTextSelection(updated_view)->selected_text());
    if (last_selected_text_ == expected_text_ && loop_runner_)
      loop_runner_->Quit();
  }

  const raw_ptr<TextInputManager> text_input_manager_;
  std::string last_selected_text_;
  std::string expected_text_;
  scoped_refptr<MessageLoopRunner> loop_runner_;
};

class SitePerProcessAndroidImeTest : public SitePerProcessBrowserTest {
 public:
  SitePerProcessAndroidImeTest() : SitePerProcessBrowserTest() {}

  SitePerProcessAndroidImeTest(const SitePerProcessAndroidImeTest&) = delete;
  SitePerProcessAndroidImeTest& operator=(const SitePerProcessAndroidImeTest&) =
      delete;

  ~SitePerProcessAndroidImeTest() override {}

 protected:
  ImeAdapterAndroid* ime_adapter() {
    return static_cast<RenderWidgetHostViewAndroid*>(
               web_contents()->GetRenderWidgetHostView())
        ->ime_adapter_for_testing();
  }

  void FocusInputInFrame(RenderFrameHostImpl* frame) {
    ASSERT_TRUE(ExecJs(frame, "window.focus(); input.focus();"));
  }

  // Creates a page with multiple (nested) OOPIFs and populates all of them
  // with an <input> element along with the required handlers for the test.
  void LoadPage() {
    ASSERT_TRUE(NavigateToURL(
        shell(),
        GURL(embedded_test_server()->GetURL(
            "a.com", "/cross_site_iframe_factory.html?a(b,c(a(b)))"))));
    FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
    frames_.push_back(root->current_frame_host());
    frames_.push_back(root->child_at(0)->current_frame_host());
    frames_.push_back(root->child_at(1)->current_frame_host());
    frames_.push_back(root->child_at(1)->child_at(0)->current_frame_host());
    frames_.push_back(
        root->child_at(1)->child_at(0)->child_at(0)->current_frame_host());

    // Adds an <input> to frame and sets up a handler for |window.oninput|. When
    // the input event is fired (by changing the value of <input> element), the
    // handler will select all the text so that the corresponding text selection
    // update on the browser side notifies the test about input insertion.
    std::string add_input_script =
        "var input = document.createElement('input');"
        "document.body.appendChild(input);"
        "window.oninput = function() {"
        "  input.select();"
        "};";

    for (content::RenderFrameHostImpl* frame : frames_) {
      ASSERT_TRUE(ExecJs(frame, add_input_script));
    }
  }

  // This methods tries to commit |text| by simulating a native call from Java.
  void CommitText(const char* text) {
    JNIEnv* env = base::android::AttachCurrentThread();

    // A valid caller is needed for ImeAdapterAndroid::GetUnderlinesFromSpans.
    base::android::ScopedJavaLocalRef<jobject> caller =
        ime_adapter()->java_ime_adapter_for_testing(env);

    // Input string from Java side.
    base::android::ScopedJavaLocalRef<jstring> jtext =
        base::android::ConvertUTF8ToJavaString(env, text);

    // Simulating a native call from Java side.
    ime_adapter()->CommitText(
        env, base::android::JavaParamRef<jobject>(env, caller.obj()),
        base::android::JavaParamRef<jobject>(env, jtext.obj()),
        base::android::JavaParamRef<jstring>(env, jtext.obj()), 0);
  }

  std::vector<raw_ptr<RenderFrameHostImpl, VectorExperimental>> frames_;
};

// This test verifies that committing text will be applied on the focused
// RenderWidgetHost.
IN_PROC_BROWSER_TEST_P(SitePerProcessAndroidImeTest,
                       CommitTextForFocusedWidget) {
  LoadPage();
  TextSelectionObserver selection_observer(
      web_contents()->GetTextInputManager());
  for (size_t index = 0; index < frames_.size(); ++index) {
    std::string text = base::StringPrintf("text%zu", index);
    FocusInputInFrame(frames_[index]);
    CommitText(text.c_str());
    selection_observer.WaitForSelectedText(text);
  }
}
#endif  // BUILDFLAG(IS_ANDROID)

// Test that an OOPIF at b.com can navigate to a cross-site a.com URL that
// transfers back to b.com.  See https://crbug.com/681077#c10 and
// https://crbug.com/660407.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       SubframeTransfersToCurrentRFH) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  ASSERT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  scoped_refptr<SiteInstanceImpl> b_site_instance =
      root->child_at(0)->current_frame_host()->GetSiteInstance();

  // Navigate subframe to a URL that will redirect from a.com back to b.com.
  // This navigation shouldn't time out.  Also ensure that the pending RFH
  // that was created for a.com is destroyed.
  GURL frame_url(
      embedded_test_server()->GetURL("a.com", "/cross-site/b.com/title2.html"));
  NavigateIframeToURL(shell()->web_contents(), "child-0", frame_url);
  EXPECT_FALSE(root->child_at(0)->render_manager()->speculative_frame_host());
  GURL redirected_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  EXPECT_EQ(root->child_at(0)->current_url(), redirected_url);
  EXPECT_EQ(b_site_instance,
            root->child_at(0)->current_frame_host()->GetSiteInstance());

  // Try the same navigation, but use the browser-initiated path.
  NavigateFrameToURL(root->child_at(0), frame_url);
  EXPECT_FALSE(root->child_at(0)->render_manager()->speculative_frame_host());
  EXPECT_EQ(root->child_at(0)->current_url(), redirected_url);
  EXPECT_EQ(b_site_instance,
            root->child_at(0)->current_frame_host()->GetSiteInstance());
}

IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       FrameSwapPreservesUniqueName) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  ASSERT_TRUE(NavigateToURL(shell(), main_url));

  // Navigate the subframe cross-site…
  {
    GURL url(embedded_test_server()->GetURL("b.com", "/title1.html"));
    EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(), "child-0", url));
  }
  // and then same-site…
  {
    GURL url(embedded_test_server()->GetURL("a.com", "/title1.html"));
    EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(), "child-0", url));
  }
  // and cross-site once more.
  {
    GURL url(embedded_test_server()->GetURL("b.com", "/title1.html"));
    EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(), "child-0", url));
  }

  // Inspect the navigation entries and make sure that the navigation target
  // remained constant across frame swaps.
  auto& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  EXPECT_EQ(4, controller.GetEntryCount());

  std::set<std::string> names;
  for (int i = 0; i < controller.GetEntryCount(); ++i) {
    NavigationEntryImpl::TreeNode* root =
        controller.GetEntryAtIndex(i)->root_node();
    ASSERT_EQ(1U, root->children.size());
    names.insert(root->children[0]->frame_entry->frame_unique_name());
  }

  // More than one entry in the set means that the subframe frame navigation
  // entries didn't have a consistent unique name. This will break history
  // navigations =(
  EXPECT_THAT(names, SizeIs(1)) << "Mismatched names for subframe!";
}

// Tests that POST body is not lost when it targets a OOPIF.
// See https://crbug.com/710937.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, PostTargetSubFrame) {
  // Navigate to a page with an OOPIF.
  GURL main_url(
      embedded_test_server()->GetURL("/frame_tree/page_with_one_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // The main frame and the subframe live on different processes.
  EXPECT_EQ(1u, root->child_count());
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            root->child_at(0)->current_frame_host()->GetSiteInstance());

  // Make a form submission from the main frame and target the OOPIF.
  GURL form_url(embedded_test_server()->GetURL("/echoall"));
  TestNavigationObserver form_post_observer(shell()->web_contents(), 1);
  EXPECT_TRUE(ExecJs(shell()->web_contents(), JsReplace(R"(
    var form = document.createElement('form');

    // POST form submission to /echoall.
    form.setAttribute("method", "POST");
    form.setAttribute("action", $1);

    // Target the OOPIF.
    form.setAttribute("target", "child-name-0");

    // Add some POST data: "my_token=my_value";
    var input = document.createElement("input");
    input.setAttribute("type", "hidden");
    input.setAttribute("name", "my_token");
    input.setAttribute("value", "my_value");
    form.appendChild(input);

    // Submit the form.
    document.body.appendChild(form);
    form.submit();
  )",
                                                        form_url)));
  form_post_observer.Wait();

  NavigationEntryImpl* entry = static_cast<NavigationEntryImpl*>(
      shell()->web_contents()->GetController().GetLastCommittedEntry());
  // TODO(arthursonzogni): This is wrong. The last committed entry was
  // renderer-initiated. See https://crbug.com/722251.
  EXPECT_FALSE(entry->is_renderer_initiated());

  // Verify that POST body was correctly passed to the server and ended up in
  // the body of the page.
  EXPECT_EQ("my_token=my_value\n",
            EvalJs(root->child_at(0),
                   "document.getElementsByTagName('pre')[0].innerText;"));
}

// Tests that POST method and body is not lost when an OOPIF submits a form
// that targets the main frame.  See https://crbug.com/806215.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       PostTargetsMainFrameFromOOPIF) {
  // Navigate to a page with an OOPIF.
  GURL main_url(
      embedded_test_server()->GetURL("/frame_tree/page_with_one_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  // The main frame and the subframe live on different processes.
  EXPECT_EQ(1u, root->child_count());
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            root->child_at(0)->current_frame_host()->GetSiteInstance());

  // Make a form submission from the subframe and target its parent frame.
  GURL form_url(embedded_test_server()->GetURL("/echoall"));
  TestNavigationObserver form_post_observer(web_contents());
  EXPECT_TRUE(
      ExecJs(root->child_at(0)->current_frame_host(), JsReplace(R"(
    var form = document.createElement('form');

    // POST form submission to /echoall.
    form.setAttribute("method", "POST");
    form.setAttribute("action", $1);

    // Target the parent.
    form.setAttribute("target", "_parent");

    // Add some POST data: "my_token=my_value";
    var input = document.createElement("input");
    input.setAttribute("type", "hidden");
    input.setAttribute("name", "my_token");
    input.setAttribute("value", "my_value");
    form.appendChild(input);

    // Submit the form.
    document.body.appendChild(form);
    form.submit();
  )",
                                                                form_url)));
  form_post_observer.Wait();

  // Verify that the FrameNavigationEntry's method is POST.
  NavigationEntryImpl* entry = static_cast<NavigationEntryImpl*>(
      web_contents()->GetController().GetLastCommittedEntry());
  EXPECT_EQ("POST", entry->root_node()->frame_entry->method());

  // Verify that POST body was correctly passed to the server and ended up in
  // the body of the page.
  EXPECT_EQ("my_token=my_value\n",
            EvalJs(root, "document.getElementsByTagName('pre')[0].innerText"));

  // Reload the main frame and ensure the POST body is preserved.  This checks
  // that the POST body was saved in the FrameNavigationEntry.
  web_contents()->GetController().Reload(ReloadType::NORMAL,
                                         false /* check_for_repost */);
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  EXPECT_EQ("my_token=my_value\n",
            EvalJs(root, "document.getElementsByTagName('pre')[0].innerText"));
}

// Verify that a remote-to-local main frame navigation doesn't overwrite
// the previous history entry.  See https://crbug.com/725716.
IN_PROC_BROWSER_TEST_P(
    SitePerProcessBrowserTest,
    DISABLED_CrossProcessMainFrameNavigationDoesNotOverwriteHistory) {
  GURL foo_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  GURL bar_url(embedded_test_server()->GetURL("bar.com", "/title2.html"));

  EXPECT_TRUE(NavigateToURL(shell(), foo_url));

  // Open a same-site popup to keep the www.foo.com process alive.
  OpenPopup(shell(), GURL(url::kAboutBlankURL), "foo");

  // Navigate foo -> bar -> foo.
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), bar_url));
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), foo_url));

  // There should be three history entries.
  EXPECT_EQ(3, web_contents()->GetController().GetEntryCount());

  // Go back: this should go to bar.com.
  {
    TestNavigationObserver back_observer(web_contents());
    web_contents()->GetController().GoBack();
    back_observer.Wait();
  }
  EXPECT_EQ(bar_url,
            web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL());

  // Go back again.  This should go to foo.com.
  {
    TestNavigationObserver back_observer(web_contents());
    web_contents()->GetController().GoBack();
    back_observer.Wait();
  }
  EXPECT_EQ(foo_url,
            web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL());
}

// The test is flaky on Linux, Chrome OS, etc; cf https://crbug.com/1170583.
#if BUILDFLAG(IS_POSIX)
#define MAYBE_CrossProcessInertSubframe DISABLED_CrossProcessInertSubframe
#else
#define MAYBE_CrossProcessInertSubframe CrossProcessInertSubframe
#endif
// Tests that when an out-of-process iframe becomes inert due to a modal
// <dialog> element, the contents of the iframe can still take focus.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       MAYBE_CrossProcessInertSubframe) {
  // This uses a(b,b) instead of a(b) to preserve the b.com process even when
  // the first subframe is navigated away from it.
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  ASSERT_EQ(2U, root->child_count());

  FrameTreeNode* iframe_node = root->child_at(0);

  EXPECT_TRUE(ExecJs(
      iframe_node,
      "document.head.innerHTML = '';"
      "document.body.innerHTML = '<input id=\"text1\"> <input id=\"text2\">';"
      "text1.focus();"));

  // Add a <dialog> to the root frame and call showModal on it.
  EXPECT_TRUE(ExecJs(root,
                     "let dialog = "
                     "document.body.appendChild(document.createElement('"
                     "dialog'));"
                     "dialog.innerHTML = 'Modal dialog <input>';"
                     "dialog.showModal();"));

  // Yield the UI thread to ensure that the real SetIsInert message
  // handler runs, in order to guarantee that the update arrives at the
  // renderer process before the script below.
  base::RunLoop().RunUntilIdle();

  RenderFrameProxyHost* root_proxy =
      iframe_node->render_manager()->GetProxyToParent();
  EXPECT_TRUE(root_proxy->IsInertForTesting());

  std::string focused_element;

  // Attempt to change focus in the inert subframe. This should work.
  // The setTimeout ensures that the inert bit can propagate before the
  // test JS code runs.
  EXPECT_EQ("text2", EvalJs(iframe_node,
                            "new Promise(resolve => {"
                            "  window.setTimeout(() => {"
                            "    text2.focus();"
                            "    resolve(document.activeElement.id);"
                            "  }, 0);"
                            "});"));

  // Navigate the child frame to another site, so that it moves into a new
  // process.
  GURL site_url(embedded_test_server()->GetURL("c.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(iframe_node, site_url));

  // NavigateToURLFromRenderer returns when the navigation commits, at which
  // point frame state has to be re-sent to the new frame. Yield the thread to
  // prevent races with the inertness update.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(ExecJs(
      iframe_node,
      "document.head.innerHTML = '';"
      "document.body.innerHTML = '<input id=\"text1\"> <input id=\"text2\">';"
      "text1.focus();"));

  // Verify we can still set focus after the navigation.
  EXPECT_EQ("text2", EvalJs(iframe_node,
                            "text2.focus();"
                            "document.activeElement.id;"));

  // Navigate the subframe back into its parent process to verify that the
  // new local frame remains non-inert.
  GURL same_site_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(iframe_node, same_site_url));

  EXPECT_TRUE(ExecJs(
      iframe_node,
      "document.head.innerHTML = '';"
      "document.body.innerHTML = '<input id=\"text1\"> <input id=\"text2\">';"
      "text1.focus();"));

  // Verify we can still set focus after the navigation.
  EXPECT_EQ("text2", EvalJs(iframe_node,
                            "text2.focus();"
                            "document.activeElement.id;"));
}

// Tests that IsInert frame flag is correctly updated and propagated.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       CrossProcessIsInertPropagation) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* frame_a =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetPrimaryFrameTree()
          .root();
  ASSERT_EQ(1U, frame_a->child_count());
  FrameTreeNode* frame_b = frame_a->child_at(0);
  ASSERT_EQ(1U, frame_b->child_count());
  FrameTreeNode* frame_c = frame_b->child_at(0);
  RenderFrameProxyHost* proxy_b = frame_b->render_manager()->GetProxyToParent();
  RenderFrameProxyHost* proxy_c = frame_c->render_manager()->GetProxyToParent();

  auto waitForInertPropagated = [&]() {
    // Force layout. This recomputes the element styles so that the <iframe>
    // gets the updated ComputedStyle::IsInert() flag. This triggers an update
    // of the associated RenderFrameProxyHost::IsInertForTesting().
    for (FrameTreeNode* frame : {frame_a, frame_b, frame_c})
      ExecuteScriptAsync(frame, "document.body.offsetLeft");

    // Propagating the inert flag requires sending messages in between the
    // browser and the renderers. Since they are using the same mojo interfaces
    // as ExecJs, waiting for an browser<->renderer roundtrip using ExecJs
    // should be enough to guarantee it has been propagate.
    for (FrameTreeNode* frame : {frame_a, frame_b, frame_c})
      EXPECT_TRUE(ExecJs(frame, "'Done'"));
  };

  waitForInertPropagated();
  EXPECT_FALSE(proxy_b->IsInertForTesting());
  EXPECT_FALSE(proxy_c->IsInertForTesting());

  // Make b inert, this should also make c inert.
  EXPECT_TRUE(ExecJs(frame_a, "document.body.inert = true;"));
  waitForInertPropagated();
  EXPECT_TRUE(proxy_b->IsInertForTesting());
  EXPECT_TRUE(proxy_c->IsInertForTesting());

  // Make b non-inert, this should also make c non-inert.
  EXPECT_TRUE(ExecJs(frame_a, "document.body.inert = false;"));
  waitForInertPropagated();
  EXPECT_FALSE(proxy_b->IsInertForTesting());
  EXPECT_FALSE(proxy_c->IsInertForTesting());

  // Make c inert.
  EXPECT_TRUE(ExecJs(frame_b, "document.body.inert = true;"));
  waitForInertPropagated();
  EXPECT_FALSE(proxy_b->IsInertForTesting());
  EXPECT_TRUE(proxy_c->IsInertForTesting());

  // Make b inert, c should continue being inert.
  EXPECT_TRUE(ExecJs(frame_a, "document.body.inert = true;"));
  waitForInertPropagated();
  EXPECT_TRUE(proxy_b->IsInertForTesting());
  EXPECT_TRUE(proxy_c->IsInertForTesting());

  // Try to make c non-inert, it should still be inert due to b.
  EXPECT_TRUE(ExecJs(frame_b, "document.body.inert = false;"));
  waitForInertPropagated();
  EXPECT_TRUE(proxy_b->IsInertForTesting());
  EXPECT_TRUE(proxy_c->IsInertForTesting());

  // Make b non-inert, this should also make c non-inert.
  EXPECT_TRUE(ExecJs(frame_a, "document.body.inert = false;"));
  waitForInertPropagated();
  EXPECT_FALSE(proxy_b->IsInertForTesting());
  EXPECT_FALSE(proxy_c->IsInertForTesting());

  // Make b anc inert.
  EXPECT_TRUE(ExecJs(frame_a, "document.body.inert = true;"));
  EXPECT_TRUE(ExecJs(frame_b, "document.body.inert = true;"));
  waitForInertPropagated();
  EXPECT_TRUE(proxy_b->IsInertForTesting());
  EXPECT_TRUE(proxy_c->IsInertForTesting());

  // Make b non-inert, c should continue being inert.
  EXPECT_TRUE(ExecJs(frame_a, "document.body.inert = false;"));
  waitForInertPropagated();
  EXPECT_FALSE(proxy_b->IsInertForTesting());
  EXPECT_TRUE(proxy_c->IsInertForTesting());
}

// Check that main frames for the same site rendering in unrelated tabs start
// sharing processes that are already dedicated to that site when over process
// limit. See https://crbug.com/513036.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       MainFrameProcessReuseWhenOverLimit) {
  // Set the process limit to 1.
  RenderProcessHost::SetMaxRendererProcessCount(1);

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), url_a));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  // Create an unrelated shell window.
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));
  Shell* new_shell = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(new_shell, url_b));

  FrameTreeNode* new_shell_root =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetPrimaryFrameTree()
          .root();

  // The new window's b.com root should not reuse the a.com process.
  EXPECT_NE(root->current_frame_host()->GetProcess(),
            new_shell_root->current_frame_host()->GetProcess());

  // Navigating the new window to a.com should reuse the first window's
  // process.
  EXPECT_TRUE(NavigateToURL(new_shell, url_a));
  EXPECT_EQ(root->current_frame_host()->GetProcess(),
            new_shell_root->current_frame_host()->GetProcess());
}

// Check that subframes for the same site rendering in unrelated tabs start
// sharing processes that are already dedicated to that site when over process
// limit. See https://crbug.com/513036.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       SubframeProcessReuseWhenOverLimit) {
  // Set the process limit to 1.
  RenderProcessHost::SetMaxRendererProcessCount(1);

  GURL first_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,b(c))"));
  ASSERT_TRUE(NavigateToURL(shell(), first_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  // Processes for dedicated sites should never be reused.
  EXPECT_NE(root->current_frame_host()->GetProcess(),
            root->child_at(0)->current_frame_host()->GetProcess());
  EXPECT_NE(root->current_frame_host()->GetProcess(),
            root->child_at(1)->current_frame_host()->GetProcess());
  EXPECT_NE(root->current_frame_host()->GetProcess(),
            root->child_at(1)->child_at(0)->current_frame_host()->GetProcess());
  EXPECT_NE(root->child_at(1)->current_frame_host()->GetProcess(),
            root->child_at(1)->child_at(0)->current_frame_host()->GetProcess());
  EXPECT_EQ(root->child_at(0)->current_frame_host()->GetProcess(),
            root->child_at(1)->current_frame_host()->GetProcess());

  // Create an unrelated shell window.
  Shell* new_shell = CreateBrowser();

  GURL new_shell_url(embedded_test_server()->GetURL(
      "d.com", "/cross_site_iframe_factory.html?d(a(b))"));
  ASSERT_TRUE(NavigateToURL(new_shell, new_shell_url));

  FrameTreeNode* new_shell_root =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetPrimaryFrameTree()
          .root();

  // New tab's root (d.com) should go into a separate process.
  EXPECT_NE(root->current_frame_host()->GetProcess(),
            new_shell_root->current_frame_host()->GetProcess());
  EXPECT_NE(root->child_at(0)->current_frame_host()->GetProcess(),
            new_shell_root->current_frame_host()->GetProcess());
  EXPECT_NE(root->child_at(1)->child_at(0)->current_frame_host()->GetProcess(),
            new_shell_root->current_frame_host()->GetProcess());

  // The new tab's subframe should reuse the a.com process.
  EXPECT_EQ(root->current_frame_host()->GetProcess(),
            new_shell_root->child_at(0)->current_frame_host()->GetProcess());

  // The new tab's grandchild frame should reuse the b.com process.
  EXPECT_EQ(root->child_at(0)->current_frame_host()->GetProcess(),
            new_shell_root->child_at(0)
                ->child_at(0)
                ->current_frame_host()
                ->GetProcess());
}

// Check that when a main frame and a subframe start navigating to the same
// cross-site URL at the same time, the new RenderFrame for the subframe is
// created successfully without crashing, and the navigations complete
// successfully.  This test checks the scenario where the main frame ends up
// committing before the subframe, and the test below checks the case where the
// subframe commits first.
//
// This used to be problematic in that the main frame navigation created an
// active RenderViewHost with a RenderFrame already swapped into the tree, and
// then while that navigation was still pending, the subframe navigation
// created its RenderFrame, which crashed when referencing its parent by a
// proxy which didn't exist.
//
// All cross-process navigations now require creating a `blink::RemoteFrame`
// before creating a RenderFrame, which makes such navigations follow the
// provisional frame (remote-to-local navigation) paths, where such a scenario
// is no longer possible.  See https://crbug.com/756790.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       TwoCrossSitePendingNavigationsAndMainFrameWins) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);

  // Navigate both frames cross-site to b.com simultaneously.
  GURL new_url_1(embedded_test_server()->GetURL("b.com", "/title1.html"));
  GURL new_url_2(embedded_test_server()->GetURL("b.com", "/title2.html"));
  TestNavigationManager manager1(web_contents(), new_url_1);
  TestNavigationManager manager2(web_contents(), new_url_2);
  auto script = JsReplace("location = $1; frames[0].location = $2;", new_url_1,
                          new_url_2);
  EXPECT_TRUE(ExecJs(web_contents(), script));

  // Wait for main frame request, but don't commit it yet.  This should create
  // a speculative RenderFrameHost.
  manager1.WaitForSpeculativeRenderFrameHostCreation();
  RenderFrameHostImpl* root_speculative_rfh =
      root->render_manager()->speculative_frame_host();
  EXPECT_TRUE(root_speculative_rfh);
  scoped_refptr<SiteInstanceImpl> b_root_site_instance(
      root_speculative_rfh->GetSiteInstance());

  // There should now be a live b.com proxy for the root, since it is doing a
  // cross-process navigation.
  RenderFrameProxyHost* root_proxy =
      root->current_frame_host()
          ->browsing_context_state()
          ->GetRenderFrameProxyHost(b_root_site_instance->group());
  EXPECT_TRUE(root_proxy);
  EXPECT_TRUE(root_proxy->is_render_frame_proxy_live());

  // Wait for subframe request, but don't commit it yet.
  manager2.WaitForSpeculativeRenderFrameHostCreation();
  RenderFrameHostImpl* subframe_speculative_rfh =
      child->render_manager()->speculative_frame_host();
  EXPECT_TRUE(child->render_manager()->speculative_frame_host());
  scoped_refptr<SiteInstanceImpl> b_subframe_site_instance(
      subframe_speculative_rfh->GetSiteInstance());

  // Similarly, the subframe should also have a b.com proxy (unused in this
  // test), since it is also doing a cross-process navigation.
  RenderFrameProxyHost* child_proxy =
      child->current_frame_host()
          ->browsing_context_state()
          ->GetRenderFrameProxyHost(b_subframe_site_instance->group());
  EXPECT_TRUE(child_proxy);
  EXPECT_TRUE(child_proxy->is_render_frame_proxy_live());

  // Now let the main frame commit.
  ASSERT_TRUE(manager1.WaitForNavigationFinished());

  // Make sure the process is live and at the new URL.
  EXPECT_TRUE(b_root_site_instance->GetProcess()->IsInitializedAndNotDead());
  EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());
  EXPECT_EQ(root_speculative_rfh, root->current_frame_host());
  EXPECT_EQ(new_url_1, root->current_frame_host()->GetLastCommittedURL());

  // The subframe should be gone, so the second navigation should have no
  // effect.
  ASSERT_TRUE(manager2.WaitForNavigationFinished());

  // The new commit should have detached the old child frame.
  EXPECT_EQ(0U, root->child_count());
  EXPECT_EQ(0, EvalJs(web_contents(), "frames.length;"));

  // The root proxy should be gone.
  if (b_subframe_site_instance->group()) {
    EXPECT_FALSE(
        root->current_frame_host()
            ->browsing_context_state()
            ->GetRenderFrameProxyHost(b_subframe_site_instance->group()));
  }
}

// Similar to TwoCrossSitePendingNavigationsAndMainFrameWins, but checks the
// case where the subframe navigation commits before the main frame.  See
// https://crbug.com/756790.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       TwoCrossSitePendingNavigationsAndSubframeWins) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a,a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);
  FrameTreeNode* child2 = root->child_at(1);

  // Install postMessage handlers in main frame and second subframe for later
  // use.
  EXPECT_TRUE(ExecJs(root->current_frame_host(),
                     "window.addEventListener('message', function(event) {\n"
                     "  event.source.postMessage(event.data + '-reply', '*');\n"
                     "});"));
  EXPECT_TRUE(ExecJs(
      child2->current_frame_host(),
      "window.addEventListener('message', function(event) {\n"
      "  event.source.postMessage(event.data + '-subframe-reply', '*');\n"
      "});"));

  // Start a main frame navigation to b.com.
  GURL new_url_1(embedded_test_server()->GetURL("b.com", "/title1.html"));
  TestNavigationManager manager1(web_contents(), new_url_1);
  EXPECT_TRUE(ExecJs(web_contents(), JsReplace("location = $1", new_url_1)));

  // Wait for main frame request and check the frame tree.  There should be a
  // proxy for b.com at the root, but nowhere else at this point.
  manager1.WaitForSpeculativeRenderFrameHostCreation();
  EXPECT_EQ(
      " Site A (B speculative) -- proxies for B\n"
      "   |--Site A\n"
      "   +--Site A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  // Now start navigating the first subframe to b.com.
  GURL new_url_2(embedded_test_server()->GetURL("b.com", "/title2.html"));
  TestNavigationManager manager2(web_contents(), new_url_2);
  EXPECT_TRUE(
      ExecJs(web_contents(), JsReplace("frames[0].location = $1", new_url_2)));

  // Wait for subframe request.
  manager2.WaitForSpeculativeRenderFrameHostCreation();
  RenderFrameHostImpl* child_speculative_rfh =
      child->render_manager()->speculative_frame_host();
  EXPECT_TRUE(child_speculative_rfh);
  scoped_refptr<SiteInstanceImpl> b_site_instance(
      child_speculative_rfh->GetSiteInstance());

  // Check that all frames have proxies for b.com at this point. The proxy for
  // |child2| is important to create since |child| has to use it to communicate
  // with |child2| if |child| commits first.
  if (CanCrossSiteNavigationsProactivelySwapBrowsingInstances()) {
    // With ProactivelySwapBrowsingInstance, the new main document and the new
    // iframe don't have the same SiteInstance, because they belong to two
    // unrelated pages. The two page use different BrowsingInstances.
    EXPECT_EQ(
        " Site A (B speculative) -- proxies for B C\n"
        "   |--Site A (C speculative) -- proxies for C\n"
        "   +--Site A ------- proxies for C\n"
        "Where A = http://a.com/\n"
        "      B = http://b.com/\n"
        "      C = http://b.com/",
        DepictFrameTree(root));
  } else {
    EXPECT_EQ(
        " Site A (B speculative) -- proxies for B\n"
        "   |--Site A (B speculative) -- proxies for B\n"
        "   +--Site A ------- proxies for B\n"
        "Where A = http://a.com/\n"
        "      B = http://b.com/",
        DepictFrameTree(root));
  }

  // Now let the subframe commit.
  ASSERT_TRUE(manager2.WaitForNavigationFinished());

  // Make sure the process is live and at the new URL.
  EXPECT_TRUE(b_site_instance->GetProcess()->IsInitializedAndNotDead());
  ASSERT_EQ(2U, root->child_count());
  EXPECT_TRUE(child->current_frame_host()->IsRenderFrameLive());
  EXPECT_EQ(child_speculative_rfh, child->current_frame_host());
  EXPECT_EQ(new_url_2, child->current_frame_host()->GetLastCommittedURL());

  // Recheck the proxies.  Main frame should still be pending.
  if (CanCrossSiteNavigationsProactivelySwapBrowsingInstances()) {
    EXPECT_EQ(
        " Site A (B speculative) -- proxies for B C\n"
        "   |--Site C ------- proxies for A\n"
        "   +--Site A ------- proxies for C\n"
        "Where A = http://a.com/\n"
        "      B = http://b.com/\n"
        "      C = http://b.com/",
        DepictFrameTree(root));
  } else {
    EXPECT_EQ(
        " Site A (B speculative) -- proxies for B\n"
        "   |--Site B ------- proxies for A\n"
        "   +--Site A ------- proxies for B\n"
        "Where A = http://a.com/\n"
        "      B = http://b.com/",
        DepictFrameTree(root));
  }

  // Make sure the subframe can communicate to both the root remote frame
  // (where the postMessage should go to the current RenderFrameHost rather
  // than the pending one) and its sibling remote frame in the a.com process.
  EXPECT_TRUE(
      ExecJs(child->current_frame_host(), WaitForMessageScript("event.data")));
  EXPECT_TRUE(ExecJs(child, "parent.postMessage('root-ping', '*')"));
  EXPECT_EQ("root-ping-reply",
            EvalJs(child->current_frame_host(), "onMessagePromise"));

  EXPECT_TRUE(
      ExecJs(child->current_frame_host(), WaitForMessageScript("event.data")));
  EXPECT_TRUE(
      ExecJs(child, "parent.frames[1].postMessage('sibling-ping', '*')"));
  EXPECT_EQ("sibling-ping-subframe-reply",
            EvalJs(child->current_frame_host(), "onMessagePromise"));

  // Cancel the pending main frame navigation, and verify that the subframe can
  // still communicate with the (old) main frame.
  root->navigator().CancelNavigation(
      root, NavigationDiscardReason::kExplicitCancellation);
  EXPECT_FALSE(root->render_manager()->speculative_frame_host());

  EXPECT_TRUE(
      ExecJs(child->current_frame_host(), WaitForMessageScript("event.data")));
  EXPECT_TRUE(ExecJs(child, "parent.postMessage('root-ping', '*')"));
  EXPECT_EQ("root-ping-reply",
            EvalJs(child->current_frame_host(), "onMessagePromise"));
}

// Similar to TwoCrossSitePendingNavigations* tests above, but checks the case
// where the current window and its opener navigate simultaneously.
// See https://crbug.com/756790.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       TwoCrossSitePendingNavigationsWithOpener) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  // Install a postMessage handler in main frame for later use.
  EXPECT_TRUE(ExecJs(web_contents(),
                     "window.addEventListener('message', function(event) {\n"
                     "  event.source.postMessage(event.data + '-reply', '*');\n"
                     "});"));

  Shell* popup_shell =
      OpenPopup(shell()->web_contents(), GURL(url::kAboutBlankURL), "popup");

  // Start a navigation to b.com in the first (opener) tab.
  GURL new_url_1(embedded_test_server()->GetURL("b.com", "/title1.html"));
  TestNavigationManager manager(web_contents(), new_url_1);
  EXPECT_TRUE(ExecJs(web_contents(), JsReplace("location = $1", new_url_1)));
  manager.WaitForSpeculativeRenderFrameHostCreation();

  // Before it commits, start and commit a navigation to b.com in the second
  // tab.
  GURL new_url_2(embedded_test_server()->GetURL("b.com", "/title2.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(popup_shell, new_url_2));

  // Check that the opener still has a speculative RenderFrameHost and a
  // corresponding proxy for b.com.
  RenderFrameHostImpl* speculative_rfh =
      root->render_manager()->speculative_frame_host();
  EXPECT_TRUE(speculative_rfh);
  scoped_refptr<SiteInstanceImpl> b_site_instance(
      speculative_rfh->GetSiteInstance());
  RenderFrameProxyHost* proxy =
      root->current_frame_host()
          ->browsing_context_state()
          ->GetRenderFrameProxyHost(b_site_instance->group());
  EXPECT_TRUE(proxy);
  EXPECT_TRUE(proxy->is_render_frame_proxy_live());

  // Make sure the second tab can communicate to its (old) opener remote frame.
  // The postMessage should go to the current RenderFrameHost rather than the
  // pending one in the first tab's main frame.
  EXPECT_TRUE(
      ExecJs(popup_shell->web_contents(), WaitForMessageScript("event.data")));

  EXPECT_TRUE(ExecJs(popup_shell->web_contents(),
                     "opener.postMessage('opener-ping', '*');"));
  EXPECT_EQ("opener-ping-reply",
            EvalJs(popup_shell->web_contents(), "onMessagePromise"));

  // Cancel the pending main frame navigation, and verify that the subframe can
  // still communicate with the (old) main frame.
  root->navigator().CancelNavigation(
      root, NavigationDiscardReason::kExplicitCancellation);
  EXPECT_FALSE(root->render_manager()->speculative_frame_host());

  EXPECT_TRUE(
      ExecJs(popup_shell->web_contents(), WaitForMessageScript("event.data")));
  EXPECT_TRUE(ExecJs(popup_shell->web_contents(),
                     "opener.postMessage('opener-ping', '*')"));
  EXPECT_EQ("opener-ping-reply",
            EvalJs(popup_shell->web_contents(), "onMessagePromise"));
}

IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       DetachSpeculativeRenderFrameHost) {
  // Commit a page with one iframe.
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Start a cross-site navigation.
  GURL cross_site_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  TestNavigationManager nav_manager(shell()->web_contents(), cross_site_url);
  BeginNavigateIframeToURL(web_contents(), "child-0", cross_site_url);

  // Wait for the request, but don't commit it yet. This should create a
  // speculative RenderFrameHost.
  nav_manager.WaitForSpeculativeRenderFrameHostCreation();
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  RenderFrameHostImpl* speculative_rfh = root->current_frame_host()
                                             ->child_at(0)
                                             ->render_manager()
                                             ->speculative_frame_host();
  EXPECT_TRUE(speculative_rfh);

  // Currently, the browser process never handles an explicit Detach() for a
  // speculative RFH, since the speculative RFH or the entire FTN is always
  // destroyed before the renderer sends this IPC.
  speculative_rfh->Detach();

  // Passes if there is no crash.
}

// Tests what happens if the renderer attempts to cancel a navigation after the
// NavigationRequest has already reached READY_TO_COMMIT.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       CancelNavigationAfterReadyToCommit) {
  class NavigationCanceller : public WebContentsObserver {
   public:
    NavigationCanceller(WebContents* web_contents,
                        RenderFrameHost& requesting_rfh)
        : WebContentsObserver(web_contents), requesting_rfh_(requesting_rfh) {}

    // WebContentsObserver overrides:
    void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override {
      // Cancel the navigation in the renderer, but don't wait for the
      // reply. This is to ensure the browser process does not process any
      // incoming messages and learn about the renderer's cancellation
      // before the browser process dispatches a CommitNavigation() to the
      // renderer.
      ExecuteScriptAsync(&*requesting_rfh_, "window.stop()");
    }

   private:
    const raw_ref<RenderFrameHost, AcrossTasksDanglingUntriaged>
        requesting_rfh_;
  };

  // Set up a test page with a same-site child frame.
  // TODO(dcheng): In the future, it might be useful to also have a test where
  // the child frame is same-site but cross-origin, and have the parent
  // initiate the navigation in the child frame.
  GURL url1(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(web_contents(), url1));

  // Now navigate the first child to another same-site page. Note that with
  // subframe RenderDocument, this will create a speculative RFH.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  GURL url2(embedded_test_server()->GetURL("a.com", "/title1.html"));
  TestNavigationManager nav_manager(web_contents(), url2);
  FrameTreeNode* first_child = root->child_at(0);
  EXPECT_TRUE(BeginNavigateToURLFromRenderer(
      first_child->render_manager()->current_frame_host(), url2));

  EXPECT_TRUE(nav_manager.WaitForResponse());

  bool using_speculative_rfh =
      !!first_child->render_manager()->speculative_frame_host();

  NavigationCanceller canceller(
      web_contents(), *first_child->render_manager()->current_frame_host());

  ASSERT_TRUE(nav_manager.WaitForNavigationFinished());
  // The navigation should be committed if and only if it committed in a new
  // RFH (i.e. if the navigation used a speculative RFH).
  EXPECT_EQ(using_speculative_rfh, nav_manager.was_committed());
}

namespace {

// Helper for various <object> navigation test cases that trigger fallback
// handling. Fallback handling should never reach ready-to-commit navigation, so
// this helper forces test failure if a ReadyToCommitNavigation() is received.
class AssertNoReadyToCommitNavigationCalls : public WebContentsObserver {
 public:
  explicit AssertNoReadyToCommitNavigationCalls(WebContents* contents)
      : WebContentsObserver(contents) {}

 private:
  // WebContentsObserver overrides:
  void ReadyToCommitNavigation(NavigationHandle* handle) override {
    ASSERT_TRUE(false);
  }
};

}  // namespace

// Test that a same-site navigation in <object> that fails with an HTTP error
// directly triggers fallback handling, rather than triggering fallback handling
// in the renderer after it receives a `CommitNavigation()` IPC.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       ObjectTagSameSiteNavigationWithHTTPError) {
  // Set up a test page with a same-site child frame hosted in an <object> tag.
  // TODO(dcheng): In the future, it might be useful to also have a test where
  // the child frame is same-site but cross-origin, and have the parent
  // initiate the navigation in the child frame.
  GURL url1(embedded_test_server()->GetURL("a.com", "/object-frame.html"));
  EXPECT_TRUE(NavigateToURL(web_contents(), url1));

  // There should be one nested browsing context.
  EXPECT_EQ(1, EvalJs(web_contents(), "window.length"));
  // And there should be no fallback content displayed.
  EXPECT_EQ("", EvalJs(web_contents(), "document.body.innerText"));

  // <object> fallback handling should never reach ReadyToCommitNavigation.
  AssertNoReadyToCommitNavigationCalls asserter(web_contents());

  // Now navigate the first child to a same-site page that will result in a 404.
  // Note that with subframe RenderDocument, this will create a speculative RFH.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  GURL url2(embedded_test_server()->GetURL("a.com", "/page404.html"));
  TestNavigationManager nav_manager(web_contents(), url2);
  FrameTreeNode* first_child = root->child_at(0);
  EXPECT_TRUE(BeginNavigateToURLFromRenderer(
      first_child->render_manager()->current_frame_host(), url2));

  const bool using_speculative_rfh =
      !!first_child->render_manager()->speculative_frame_host();
  // Speculative RFH will not be created at this point if we enable deferring.
  EXPECT_EQ(using_speculative_rfh,
            GetRenderDocumentLevel() >= RenderDocumentLevel::kSubframe &&
                !base::FeatureList::IsEnabled(
                    features::kDeferSpeculativeRFHCreation));

  ASSERT_TRUE(nav_manager.WaitForNavigationFinished());
  // There should be no commit...
  EXPECT_FALSE(nav_manager.was_committed());
  // .. and the navigation should have been aborted.
  EXPECT_FALSE(nav_manager.was_successful());
  // Fallback handling should discard the child browsing context and render the
  // fallback contents.
  // TODO(dcheng): Chrome is not compliant with the spec. An HTTP error triggers
  // fallback content, which is supposed to discard the nested browsing
  // context...
  EXPECT_EQ(1, EvalJs(web_contents(), "window.length"));
  EXPECT_EQ("fallback", EvalJs(web_contents(), "document.body.innerText"));
}

// Test that a cross-site navigation in <object> that fails with an HTTP error
// directly triggers fallback handling, rather than triggering fallback handling
// in the renderer after it receives a `CommitNavigation()` IPC.
// The test disables the delay of creating the speculative RFH since it
// will check the created speculative RFH for a failing request. The speculative
// RFH will not be created after receiving the 404 response.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTestWithoutSpeculativeRFHDelay,
                       ObjectTagCrossSiteNavigationWithHTTPError) {
  // Set up a test page with a same-site child frame hosted in an <object> tag.
  // TODO(dcheng): In the future, it might be useful to also have a test where
  // the child frame is same-site but cross-origin, and have the parent
  // initiate the navigation in the child frame.
  GURL url1(embedded_test_server()->GetURL("a.com", "/object-frame.html"));
  EXPECT_TRUE(NavigateToURL(web_contents(), url1));

  // There should be one nested browsing context.
  EXPECT_EQ(1, EvalJs(web_contents(), "window.length"));
  // And there should be no fallback content displayed.
  EXPECT_EQ("", EvalJs(web_contents(), "document.body.innerText"));

  // <object> fallback handling should never reach ReadyToCommitNavigation.
  AssertNoReadyToCommitNavigationCalls asserter(web_contents());

  // Now navigate the first child to a cross-site page that will result in a
  // 404.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  GURL url2(embedded_test_server()->GetURL("b.com", "/page404.html"));
  TestNavigationManager nav_manager(web_contents(), url2);
  FrameTreeNode* first_child = root->child_at(0);
  EXPECT_TRUE(BeginNavigateToURLFromRenderer(
      first_child->render_manager()->current_frame_host(), url2));
  nav_manager.WaitForSpeculativeRenderFrameHostCreation();
  // Cross-site navigations always force a speculative RFH to be created.
  EXPECT_TRUE(first_child->render_manager()->speculative_frame_host());

  ASSERT_TRUE(nav_manager.WaitForNavigationFinished());
  // There should be no commit...
  EXPECT_FALSE(nav_manager.was_committed());
  // .. and the navigation should have been aborted.
  EXPECT_FALSE(nav_manager.was_successful());
  // Fallback handling should discard the child browsing context and render the
  // fallback contents.
  // TODO(dcheng): Chrome is not compliant with the spec. An HTTP error triggers
  // fallback content, which is supposed to discard the nested browsing
  // context...
  EXPECT_EQ(1, EvalJs(web_contents(), "window.length"));
  EXPECT_EQ("fallback", EvalJs(web_contents(), "document.body.innerText"));
}

// Test that a same-site navigation in <object> that fails with an HTTP error
// and also subsequently fails to load the body still directly triggers fallback
// handling, rather than triggering fallback handling in the renderer after it
// receives a `CommitNavigation()` IPC.
IN_PROC_BROWSER_TEST_P(
    SitePerProcessBrowserTest,
    ObjectTagSameSiteNavigationWithHTTPErrorAndFailedBodyLoad) {
  // Set up a test page with a same-site child frame hosted in an <object> tag.
  // TODO(dcheng): In the future, it might be useful to also have a test where
  // the child frame is same-site but cross-origin, and have the parent
  // initiate the navigation in the child frame.
  GURL url1(embedded_test_server()->GetURL("a.com", "/object-frame.html"));
  EXPECT_TRUE(NavigateToURL(web_contents(), url1));

  // There should be one nested browsing context.
  EXPECT_EQ(1, EvalJs(web_contents(), "window.length"));
  // And there should be no fallback content displayed.
  EXPECT_EQ("", EvalJs(web_contents(), "document.body.innerText"));

  // This test differs from CommitNavigationWithHTTPErrorInObjectTag by
  // triggering a body load failure. `ObjectNavigationFallbackBodyLoader`
  // detects this by setting a disconnect handler on the `mojo::Receiver` for
  // `network:;mojom::URLLoaderClient`. Exercise this code path by:
  // 1. inserting a test `NavigationThrottle`
  // 2. replacing the `network::mojom::URLLoaderClient` endpoint with one where
  //    the corresponding `mojo::Remote` is simply closed at
  //    `WILL_PROCESS_RESPONSE` time.
  TestNavigationThrottleInserter navigation_throttle_inserter(
      web_contents(),
      base::BindRepeating(
          [](NavigationHandle* handle) -> std::unique_ptr<NavigationThrottle> {
            auto throttle = std::make_unique<TestNavigationThrottle>(handle);
            throttle->SetCallback(
                TestNavigationThrottle::WILL_PROCESS_RESPONSE,
                base::BindLambdaForTesting([handle]() {
                  // Swap out the URL loader client endpoint and just drop the
                  // mojo::Remote. This will trigger the mojo::Receiver to be
                  // disconnected, which should still trigger fallback handling
                  // despite body loading failing.
                  mojo::Remote<network::mojom::URLLoaderClient>
                      remote_to_be_dropped;
                  auto* request = static_cast<NavigationRequest*>(handle);
                  request->mutable_url_loader_client_endpoints_for_testing()
                      ->url_loader_client =
                      remote_to_be_dropped.BindNewPipeAndPassReceiver();
                }));
            return throttle;
          }));

  // <object> fallback handling should never reach ReadyToCommitNavigation.
  AssertNoReadyToCommitNavigationCalls asserter(web_contents());

  // Now navigate the first child to a same-site page that will result in a 404,
  // though the body loading will fail. Note that with subframe RenderDocument,
  // this will create a speculative RFH.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  GURL url2(embedded_test_server()->GetURL("a.com", "/page404.html"));
  TestNavigationManager nav_manager(web_contents(), url2);
  FrameTreeNode* first_child = root->child_at(0);
  EXPECT_TRUE(BeginNavigateToURLFromRenderer(
      first_child->render_manager()->current_frame_host(), url2));

  const bool using_speculative_rfh =
      !!first_child->render_manager()->speculative_frame_host();
  // Speculative RFH will not be created at this point if we enable deferring.
  EXPECT_EQ(using_speculative_rfh,
            GetRenderDocumentLevel() >= RenderDocumentLevel::kSubframe &&
                !base::FeatureList::IsEnabled(
                    features::kDeferSpeculativeRFHCreation));

  ASSERT_TRUE(nav_manager.WaitForNavigationFinished());
  // There should be no commit...
  EXPECT_FALSE(nav_manager.was_committed());
  // .. and the navigation should have been aborted.
  EXPECT_FALSE(nav_manager.was_successful());
  // Fallback handling should discard the child browsing context and render the
  // fallback contents.
  // TODO(dcheng): Chrome is not compliant with the spec. An HTTP error triggers
  // fallback content, which is supposed to discard the nested browsing
  // context...
  EXPECT_EQ(1, EvalJs(web_contents(), "window.length"));
  EXPECT_EQ("fallback", EvalJs(web_contents(), "document.body.innerText"));

  // `WaitForNavigationFinished()` should imply the `NavigationRequest` has been
  // cleaned up as well, but check to be sure.
  EXPECT_FALSE(first_child->navigation_request());
}

// Test that a cross-site navigation in <object> that fails with an HTTP error
// and also subsequently fails to load the body still directly triggers fallback
// handling, rather than triggering fallback handling in the renderer after it
// receives a `CommitNavigation()` IPC.
// The test disables the delay of creating the speculative RFH since it
// will check the created speculative RFH for a failing request. The speculative
// RFH will not be created after receiving the 404 response.
IN_PROC_BROWSER_TEST_P(
    SitePerProcessBrowserTestWithoutSpeculativeRFHDelay,
    ObjectTagCrossSiteNavigationWithHTTPErrorAndFailedBodyLoad) {
  // Set up a test page with a same-site child frame hosted in an <object> tag.
  // TODO(dcheng): In the future, it might be useful to also have a test where
  // the child frame is same-site but cross-origin, and have the parent
  // initiate the navigation in the child frame.
  GURL url1(embedded_test_server()->GetURL("a.com", "/object-frame.html"));
  EXPECT_TRUE(NavigateToURL(web_contents(), url1));

  // There should be one nested browsing context.
  EXPECT_EQ(1, EvalJs(web_contents(), "window.length"));
  // And there should be no fallback content displayed.
  EXPECT_EQ("", EvalJs(web_contents(), "document.body.innerText"));

  // This test differs from CommitNavigationWithHTTPErrorInObjectTag by
  // triggering a body load failure. `ObjectNavigationFallbackBodyLoader`
  // detects this by setting a disconnect handler on the `mojo::Receiver` for
  // `network:;mojom::URLLoaderClient`. Exercise this code path by:
  // 1. inserting a test `NavigationThrottle`
  // 2. replacing the `network::mojom::URLLoaderClient` endpoint with one where
  //    the corresponding `mojo::Remote` is simply closed at
  //    `WILL_PROCESS_RESPONSE` time.
  TestNavigationThrottleInserter navigation_throttle_inserter(
      web_contents(),
      base::BindRepeating(
          [](NavigationHandle* handle) -> std::unique_ptr<NavigationThrottle> {
            auto throttle = std::make_unique<TestNavigationThrottle>(handle);
            throttle->SetCallback(
                TestNavigationThrottle::WILL_PROCESS_RESPONSE,
                base::BindLambdaForTesting([handle]() {
                  // Swap out the URL loader client endpoint and just drop the
                  // mojo::Remote. This will trigger the mojo::Receiver to be
                  // disconnected, which should still trigger fallback handling
                  // despite body loading failing.
                  mojo::Remote<network::mojom::URLLoaderClient>
                      remote_to_be_dropped;
                  auto* request = static_cast<NavigationRequest*>(handle);
                  request->mutable_url_loader_client_endpoints_for_testing()
                      ->url_loader_client =
                      remote_to_be_dropped.BindNewPipeAndPassReceiver();
                }));
            return throttle;
          }));

  // <object> fallback handling should never reach ReadyToCommitNavigation.
  AssertNoReadyToCommitNavigationCalls asserter(web_contents());

  // Now navigate the first child to a cross-site page that will result in a
  // 404, though the body loading will fail.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  GURL url2(embedded_test_server()->GetURL("b.com", "/page404.html"));
  TestNavigationManager nav_manager(web_contents(), url2);
  FrameTreeNode* first_child = root->child_at(0);
  EXPECT_TRUE(BeginNavigateToURLFromRenderer(
      first_child->render_manager()->current_frame_host(), url2));
  nav_manager.WaitForSpeculativeRenderFrameHostCreation();
  // Cross-site navigations always force a speculative RFH to be created.
  EXPECT_TRUE(first_child->render_manager()->speculative_frame_host());

  ASSERT_TRUE(nav_manager.WaitForNavigationFinished());
  // There should be no commit...
  EXPECT_FALSE(nav_manager.was_committed());
  // .. and the navigation should have been aborted.
  EXPECT_FALSE(nav_manager.was_successful());
  // Fallback handling should discard the child browsing context and render the
  // fallback contents.
  // TODO(dcheng): Chrome is not compliant with the spec. An HTTP error triggers
  // fallback content, which is supposed to discard the nested browsing
  // context...
  EXPECT_EQ(1, EvalJs(web_contents(), "window.length"));
  EXPECT_EQ("fallback", EvalJs(web_contents(), "document.body.innerText"));

  // `WaitForNavigationFinished()` should imply the `NavigationRequest` has been
  // cleaned up as well, but check to be sure.
  EXPECT_FALSE(first_child->navigation_request());
}

// Test that a same-site navigation in <object> that fails with a network error
// directly triggers fallback handling, rather than triggering fallback handling
// in the renderer after it receives a `CommitFailedNavigation()` IPC.
// The test disables the delay of creating the speculative RFH since it
// will check the created speculative RFH for a failing request. The speculative
// RFH will not be created after the network error.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTestWithoutSpeculativeRFHDelay,
                       ObjectTagSameSiteNavigationWithNetworkError) {
  // Set up a test page with a same-site child frame hosted in an <object> tag.
  GURL url1(embedded_test_server()->GetURL("a.com", "/object-frame.html"));
  EXPECT_TRUE(NavigateToURL(web_contents(), url1));

  // <object> fallback handling should never reach ReadyToCommitNavigation.
  AssertNoReadyToCommitNavigationCalls asserter(web_contents());

  // Now navigate the first child to a same-site page that will result in a
  // network error. Note that with subframe RenderDocument, this will create a
  // speculative RFH.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  GURL error_url(embedded_test_server()->GetURL("a.com", "/empty.html"));
  std::unique_ptr<URLLoaderInterceptor> interceptor =
      URLLoaderInterceptor::SetupRequestFailForURL(error_url,
                                                   net::ERR_CONNECTION_REFUSED);
  TestNavigationManager nav_manager(web_contents(), error_url);
  FrameTreeNode* first_child = root->child_at(0);
  EXPECT_TRUE(BeginNavigateToURLFromRenderer(
      first_child->render_manager()->current_frame_host(), error_url));
  if (GetRenderDocumentLevel() >= RenderDocumentLevel::kSubframe) {
    nav_manager.WaitForSpeculativeRenderFrameHostCreation();
  }

  const bool using_speculative_rfh =
      !!first_child->render_manager()->speculative_frame_host();
  EXPECT_EQ(using_speculative_rfh,
            GetRenderDocumentLevel() >= RenderDocumentLevel::kSubframe);

  // `WaitForResponse()` should signal failure by returning `false` false since
  // the URLLoaderInterceptor forces a network error.
  EXPECT_FALSE(nav_manager.WaitForResponse());

  ASSERT_TRUE(nav_manager.WaitForNavigationFinished());
  EXPECT_FALSE(nav_manager.was_committed());

  // Make sure that the speculative RFH has been cleaned up, if needed.
  EXPECT_EQ(nullptr, first_child->render_manager()->speculative_frame_host());
}

// Test that a cross-site navigation in <object> that fails with a network error
// directly triggers fallback handling, rather than triggering fallback handling
// in the renderer after it receives a `CommitFailedNavigation()` IPC.
// The test disables the delay of creating the speculative RFH since it
// will check the created speculative RFH for a failing request. The speculative
// RFH will not be created after the network error.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTestWithoutSpeculativeRFHDelay,
                       ObjectTagCrossSiteNavigationWithNetworkError) {
  // Set up a test page with a same-site child frame hosted in an <object> tag.
  GURL url1(embedded_test_server()->GetURL("a.com", "/object-frame.html"));
  EXPECT_TRUE(NavigateToURL(web_contents(), url1));

  // <object> fallback handling should never reach ReadyToCommitNavigation.
  AssertNoReadyToCommitNavigationCalls asserter(web_contents());

  // Now navigate the first child to a cross-site page that will result in a
  // network error.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  GURL error_url(embedded_test_server()->GetURL("b.com", "/empty.html"));
  std::unique_ptr<URLLoaderInterceptor> interceptor =
      URLLoaderInterceptor::SetupRequestFailForURL(error_url,
                                                   net::ERR_CONNECTION_REFUSED);
  TestNavigationManager nav_manager(web_contents(), error_url);
  FrameTreeNode* first_child = root->child_at(0);
  EXPECT_TRUE(BeginNavigateToURLFromRenderer(
      first_child->render_manager()->current_frame_host(), error_url));
  nav_manager.WaitForSpeculativeRenderFrameHostCreation();
  // Cross-site navigations always force a speculative RFH to be created.
  EXPECT_TRUE(first_child->render_manager()->speculative_frame_host());

  // `WaitForResponse()` should signal failure by returning `false` false since
  // the URLLoaderInterceptor forces a network error.
  EXPECT_FALSE(nav_manager.WaitForResponse());

  ASSERT_TRUE(nav_manager.WaitForNavigationFinished());
  EXPECT_FALSE(nav_manager.was_committed());

  // Make sure that the speculative RFH has been cleaned up, if needed.
  EXPECT_EQ(nullptr, first_child->render_manager()->speculative_frame_host());
}

class SitePerProcessBrowserTestWithLeakDetector
    : public SitePerProcessBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SitePerProcessBrowserTest::SetUpCommandLine(command_line);
    // Using the LeakDetector requires exposing GC.
    command_line->AppendSwitchASCII(blink::switches::kJavaScriptFlags,
                                    "--expose-gc");
  }
};

IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTestWithLeakDetector,
                       CloseWebContentsWithSpeculativeRenderFrameHost) {
  const GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(web_contents(), url1));

  // Open a popup in B. This is to prevent any fast shutdown shenanigans that
  // might otherwise happen when the speculative RFH is discarded later.
  Shell* new_shell =
      OpenPopup(web_contents(),
                embedded_test_server()->GetURL("b.com", "/title1.html"), "");
  ASSERT_TRUE(new_shell);

  mojo::Remote<blink::mojom::LeakDetector> leak_detector_remote;
  new_shell->web_contents()->GetPrimaryMainFrame()->GetProcess()->BindReceiver(
      leak_detector_remote.BindNewPipeAndPassReceiver());

  // One live document is expected from the newly opened popup.
  {
    base::test::TestFuture<blink::mojom::LeakDetectionResultPtr> result_future;
    leak_detector_remote->PerformLeakDetection(result_future.GetCallback());
    auto result = result_future.Take();
    EXPECT_EQ(1u, result->number_of_live_documents);
    // Note: the number of live frames includes remote frames.
    EXPECT_EQ(2u, result->number_of_live_frames);
  }

  // Start a navigation to B, but don't let it commit. This should associate a
  // speculative RFH with the main frame.
  const GURL url2(embedded_test_server()->GetURL("b.com", "/title1.html"));
  TestNavigationManager nav_manager(web_contents(), url2);
  ASSERT_TRUE(BeginNavigateToURLFromRenderer(web_contents(), url2));
  ASSERT_TRUE(nav_manager.WaitForResponse());

  // Speculative RFH should be created in B, increasing the number of live
  // documents and frames.
  {
    base::test::TestFuture<blink::mojom::LeakDetectionResultPtr> result_future;
    leak_detector_remote->PerformLeakDetection(result_future.GetCallback());
    auto result = result_future.Take();
    EXPECT_EQ(2u, result->number_of_live_documents);
    // Note: the number of live frames includes remote frames.
    EXPECT_EQ(3u, result->number_of_live_frames);
  }

  // Close the WebContents associated with the speculative RFH.
  shell()->Close();
  // Synchronize with the renderer.
  EXPECT_TRUE(ExecJs(new_shell, ""));

  // The resources associated with the speculative RFH should be freed now, as
  // well as the original frame from the now closed shell.
  {
    base::test::TestFuture<blink::mojom::LeakDetectionResultPtr> result_future;
    leak_detector_remote->PerformLeakDetection(result_future.GetCallback());
    auto result = result_future.Take();
    EXPECT_EQ(1u, result->number_of_live_documents);
    // Note: the number of live frames includes remote frames.
    EXPECT_EQ(1u, result->number_of_live_frames);
  }
}

IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTestWithLeakDetector,
                       DetachFrameWithSpeculativeRenderFrameHost) {
  const GURL url1(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  ASSERT_TRUE(NavigateToURL(web_contents(), url1));

  // Open a popup in B. This is to prevent any fast shutdown shenanigans that
  // might otherwise happen when the speculative RFH is discarded later.
  Shell* new_shell =
      OpenPopup(web_contents(),
                embedded_test_server()->GetURL("b.com", "/title1.html"), "");
  ASSERT_TRUE(new_shell);

  mojo::Remote<blink::mojom::LeakDetector> leak_detector_remote;
  new_shell->web_contents()->GetPrimaryMainFrame()->GetProcess()->BindReceiver(
      leak_detector_remote.BindNewPipeAndPassReceiver());

  // One live document is expected from the newly opened popup.
  {
    base::test::TestFuture<blink::mojom::LeakDetectionResultPtr> result_future;
    leak_detector_remote->PerformLeakDetection(result_future.GetCallback());
    auto result = result_future.Take();
    EXPECT_EQ(1u, result->number_of_live_documents);
    // Note: the number of live frames includes remote frames.
    EXPECT_EQ(3u, result->number_of_live_frames);
  }

  // Start a navigation to B in the iframe, but don't let it commit. This should
  // associate a speculative RFH with the child frame.
  const GURL url2(embedded_test_server()->GetURL("b.com", "/title1.html"));
  TestNavigationManager nav_manager(web_contents(), url2);
  ASSERT_TRUE(BeginNavigateToURLFromRenderer(web_contents()
                                                 ->GetPrimaryFrameTree()
                                                 .root()
                                                 ->current_frame_host()
                                                 ->child_at(0),
                                             url2));
  ASSERT_TRUE(nav_manager.WaitForResponse());

  // Speculative RFH should be created in B, increasing the number of live
  // documents and frames.
  {
    base::test::TestFuture<blink::mojom::LeakDetectionResultPtr> result_future;
    leak_detector_remote->PerformLeakDetection(result_future.GetCallback());
    auto result = result_future.Take();
    EXPECT_EQ(2u, result->number_of_live_documents);
    // Note: the number of live frames includes remote frames.
    EXPECT_EQ(4u, result->number_of_live_frames);
  }

  // Detach the <iframe> associated with the speculative RFH.
  EXPECT_TRUE(
      ExecJs(web_contents(), "document.querySelector('iframe').remove()"));
  // Synchronize with the renderer.
  EXPECT_TRUE(ExecJs(new_shell, ""));

  // The resources associated with the speculative RFH should be freed now.
  {
    base::test::TestFuture<blink::mojom::LeakDetectionResultPtr> result_future;
    leak_detector_remote->PerformLeakDetection(result_future.GetCallback());
    auto result = result_future.Take();
    EXPECT_EQ(1u, result->number_of_live_documents);
    // Note: the number of live frames includes remote frames.
    EXPECT_EQ(2u, result->number_of_live_frames);
  }
}

#if BUILDFLAG(IS_ANDROID)

namespace {

class MockEventHandlerAndroid : public ui::EventHandlerAndroid {
 public:
  bool OnTouchEvent(const ui::MotionEventAndroid& event) override {
    did_receive_event_ = true;
    return true;
  }

  bool did_receive_event() { return did_receive_event_; }

 private:
  bool did_receive_event_ = false;
};

}  // namespace

IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       SpeculativeRenderFrameHostDoesNotReceiveInput) {
  GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url1));

  RenderWidgetHostViewAndroid* rwhva =
      static_cast<RenderWidgetHostViewAndroid*>(
          shell()->web_contents()->GetRenderWidgetHostView());
  ui::ViewAndroid* rwhva_native_view = rwhva->GetNativeView();
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  // Start a cross-site navigation.
  GURL url2(embedded_test_server()->GetURL("b.com", "/title2.html"));
  TestNavigationManager nav_manager(web_contents(), url2);
  shell()->LoadURL(url2);

  // Wait for the request, but don't commit it yet. This should create a
  // speculative RenderFrameHost.
  nav_manager.WaitForSpeculativeRenderFrameHostCreation();
  RenderFrameHostImpl* root_speculative_rfh =
      root->render_manager()->speculative_frame_host();
  EXPECT_TRUE(root_speculative_rfh);
  RenderWidgetHostViewAndroid* rwhv_speculative =
      static_cast<RenderWidgetHostViewAndroid*>(
          root_speculative_rfh->GetView());
  ui::ViewAndroid* rwhv_speculative_native_view =
      rwhv_speculative->GetNativeView();

  ui::ViewAndroid* root_view = web_contents()->GetView()->GetNativeView();
  EXPECT_TRUE(root_view);

  MockEventHandlerAndroid mock_handler;
  rwhva_native_view->set_event_handler(&mock_handler);
  MockEventHandlerAndroid mock_handler_speculative;
  rwhv_speculative_native_view->set_event_handler(&mock_handler_speculative);
  // Avoid having the root try to handle the following event.
  root_view->set_event_handler(nullptr);

  auto size = root_view->GetSize();
  float x = size.width() / 2;
  float y = size.height() / 2;
  ui::MotionEventAndroid::Pointer pointer0(0, x, y, 0, 0, 0, 0, 0);
  ui::MotionEventAndroid::Pointer pointer1(0, 0, 0, 0, 0, 0, 0, 0);
  ui::MotionEventAndroidJava event(nullptr, nullptr,
                                   1.f / root_view->GetDipScale(), 0.f, 0.f,
                                   0.f, base::TimeTicks(), 0, 1, 0, 0, 0, 0, 0,
                                   0, 0, 0, 0, false, &pointer0, &pointer1);
  root_view->OnTouchEventForTesting(event);

  EXPECT_TRUE(mock_handler.did_receive_event());
  EXPECT_FALSE(mock_handler_speculative.did_receive_event());
}

IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, TestChildProcessImportance) {
  web_contents()->SetPrimaryMainFrameImportance(
      ChildProcessImportance::MODERATE);

  // Construct root page with one child in different domain.
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1u, root->child_count());
  FrameTreeNode* child = root->child_at(0);

  // Importance should survive initial navigation. Note importance only affect
  // main frame, so sub frame process should remain NORMAL throughout.
  EXPECT_EQ(ChildProcessImportance::MODERATE,
            root->current_frame_host()->GetProcess()->GetEffectiveImportance());
  EXPECT_EQ(
      ChildProcessImportance::NORMAL,
      child->current_frame_host()->GetProcess()->GetEffectiveImportance());

  // Check setting importance.
  web_contents()->SetPrimaryMainFrameImportance(ChildProcessImportance::NORMAL);
  EXPECT_EQ(ChildProcessImportance::NORMAL,
            root->current_frame_host()->GetProcess()->GetEffectiveImportance());
  EXPECT_EQ(
      ChildProcessImportance::NORMAL,
      child->current_frame_host()->GetProcess()->GetEffectiveImportance());
  web_contents()->SetPrimaryMainFrameImportance(
      ChildProcessImportance::IMPORTANT);
  EXPECT_EQ(ChildProcessImportance::IMPORTANT,
            root->current_frame_host()->GetProcess()->GetEffectiveImportance());
  EXPECT_EQ(
      ChildProcessImportance::NORMAL,
      child->current_frame_host()->GetProcess()->GetEffectiveImportance());

  // Check importance is maintained if child navigates to new domain.
  int old_child_process_id = child->current_frame_host()->GetProcess()->GetID();
  GURL url = embedded_test_server()->GetURL("foo.com", "/title2.html");
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), url));
  int new_child_process_id = child->current_frame_host()->GetProcess()->GetID();
  EXPECT_NE(old_child_process_id, new_child_process_id);
  EXPECT_EQ(
      ChildProcessImportance::NORMAL,
      child->current_frame_host()->GetProcess()->GetEffectiveImportance());
  EXPECT_EQ(ChildProcessImportance::IMPORTANT,
            root->current_frame_host()->GetProcess()->GetEffectiveImportance());

  // Check importance is maintained if root navigates to new domain.
  int old_root_process_id = root->current_frame_host()->GetProcess()->GetID();
  child = nullptr;  // Going to navigate root to page without any child.
  EXPECT_TRUE(NavigateToURLFromRenderer(root, url));
  EXPECT_EQ(0u, root->child_count());
  int new_root_process_id = root->current_frame_host()->GetProcess()->GetID();
  EXPECT_NE(old_root_process_id, new_root_process_id);
  EXPECT_EQ(ChildProcessImportance::IMPORTANT,
            root->current_frame_host()->GetProcess()->GetEffectiveImportance());
}

class TouchSelectionControllerClientTestWrapper
    : public ui::TouchSelectionControllerClient {
 public:
  explicit TouchSelectionControllerClientTestWrapper(
      ui::TouchSelectionControllerClient* client)
      : expected_event_(ui::SELECTION_HANDLES_SHOWN), client_(client) {}

  TouchSelectionControllerClientTestWrapper(
      const TouchSelectionControllerClientTestWrapper&) = delete;
  TouchSelectionControllerClientTestWrapper& operator=(
      const TouchSelectionControllerClientTestWrapper&) = delete;

  ~TouchSelectionControllerClientTestWrapper() override {}

  void InitWaitForSelectionEvent(ui::SelectionEventType expected_event) {
    DCHECK(!run_loop_);
    expected_event_ = expected_event;
    run_loop_ = std::make_unique<base::RunLoop>();
  }

  void Wait() {
    DCHECK(run_loop_);
    run_loop_->Run();
    run_loop_.reset();
  }

 private:
  // TouchSelectionControllerClient:
  void OnSelectionEvent(ui::SelectionEventType event) override {
    client_->OnSelectionEvent(event);
    if (run_loop_ && event == expected_event_)
      run_loop_->Quit();
  }

  bool SupportsAnimation() const override {
    return client_->SupportsAnimation();
  }

  void SetNeedsAnimate() override { client_->SetNeedsAnimate(); }

  void MoveCaret(const gfx::PointF& position) override {
    client_->MoveCaret(position);
  }

  void MoveRangeSelectionExtent(const gfx::PointF& extent) override {
    client_->MoveRangeSelectionExtent(extent);
  }

  void SelectBetweenCoordinates(const gfx::PointF& base,
                                const gfx::PointF& extent) override {
    client_->SelectBetweenCoordinates(base, extent);
  }

  std::unique_ptr<ui::TouchHandleDrawable> CreateDrawable() override {
    return client_->CreateDrawable();
  }

  void DidScroll() override {}

  void OnDragUpdate(const ui::TouchSelectionDraggable::Type type,
                    const gfx::PointF& position) override {}

  ui::SelectionEventType expected_event_;
  std::unique_ptr<base::RunLoop> run_loop_;
  // Not owned.
  raw_ptr<ui::TouchSelectionControllerClient, DanglingUntriaged> client_;
};

class TouchSelectionControllerClientAndroidSiteIsolationTest
    : public SitePerProcessBrowserTest {
 public:
  TouchSelectionControllerClientAndroidSiteIsolationTest()
      : root_rwhv_(nullptr),
        child_rwhv_(nullptr),
        child_frame_tree_node_(nullptr),
        selection_controller_client_(nullptr) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SitePerProcessBrowserTestBase::SetUpCommandLine(command_line);
    IsolateAllSitesForTesting(command_line);
  }

  RenderWidgetHostViewAndroid* GetRenderWidgetHostViewAndroid() {
    return static_cast<RenderWidgetHostViewAndroid*>(
        shell()->web_contents()->GetRenderWidgetHostView());
  }

  void SelectWithLongPress(gfx::Point point) {
    // Get main frame view for event insertion.
    RenderWidgetHostViewAndroid* main_view = GetRenderWidgetHostViewAndroid();

    SendTouch(main_view, ui::MotionEvent::Action::DOWN, point);
    // action_timeout() is far longer than needed for a LongPress, so we use
    // a custom timeout here.
    DelayBy(base::Milliseconds(2000));
    SendTouch(main_view, ui::MotionEvent::Action::UP, point);
  }

  void SimpleTap(gfx::Point point) {
    // Get main frame view for event insertion.
    RenderWidgetHostViewAndroid* main_view = GetRenderWidgetHostViewAndroid();

    SendTouch(main_view, ui::MotionEvent::Action::DOWN, point);
    // tiny_timeout() is way shorter than a reasonable user-created tap gesture,
    // so we use a custom timeout here.
    DelayBy(base::Milliseconds(300));
    SendTouch(main_view, ui::MotionEvent::Action::UP, point);
  }

  void SetupTest() {
    GURL test_url(embedded_test_server()->GetURL(
        "a.com", "/cross_site_iframe_factory.html?a(a)"));
    EXPECT_TRUE(NavigateToURL(shell(), test_url));
    frame_observer_ = std::make_unique<RenderFrameSubmissionObserver>(
        shell()->web_contents());
    FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                              ->GetPrimaryFrameTree()
                              .root();
    EXPECT_EQ(
        " Site A\n"
        "   +--Site A\n"
        "Where A = http://a.com/",
        FrameTreeVisualizer().DepictFrameTree(root));
    TestNavigationObserver observer(shell()->web_contents());
    EXPECT_EQ(1u, root->child_count());
    child_frame_tree_node_ = root->child_at(0);

    root_rwhv_ = static_cast<RenderWidgetHostViewAndroid*>(
        root->current_frame_host()->GetRenderWidgetHost()->GetView());
    selection_controller_client_ =
        new TouchSelectionControllerClientTestWrapper(
            root_rwhv_->GetSelectionControllerClientManagerForTesting());
    root_rwhv_->SetSelectionControllerClientForTesting(
        base::WrapUnique(selection_controller_client_.get()));

    // We need to load the desired subframe and then wait until it's stable,
    // i.e. generates no new compositor frames for some reasonable time period:
    // a stray frame between touch selection's pre-handling of GestureLongPress
    // and the expected frame containing the selected region can confuse the
    // TouchSelectionController, causing it to fail to show selection handles.
    // Note this is an issue with the TouchSelectionController in general, and
    // not a property of this test.
    GURL child_url(
        embedded_test_server()->GetURL("b.com", "/touch_selection.html"));
    EXPECT_TRUE(
        NavigateToURLFromRenderer(child_frame_tree_node_.get(), child_url));
    EXPECT_EQ(
        " Site A ------------ proxies for B\n"
        "   +--Site B ------- proxies for A\n"
        "Where A = http://a.com/\n"
        "      B = http://b.com/",
        FrameTreeVisualizer().DepictFrameTree(root));
    // The child will change with the cross-site navigation. It shouldn't change
    // after this.
    child_frame_tree_node_ = root->child_at(0);
    WaitForHitTestData(child_frame_tree_node_->current_frame_host());

    child_rwhv_ = static_cast<RenderWidgetHostViewChildFrame*>(
        child_frame_tree_node_->current_frame_host()
            ->GetRenderWidgetHost()
            ->GetView());

    EXPECT_EQ(child_url, observer.last_navigation_url());
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  // This must be called before the main-frame's RenderWidgetHostView is freed,
  // else we'll have a nullptr dereference on shutdown.
  void ShutdownTest() {
    ASSERT_TRUE(frame_observer_);
    frame_observer_.reset();
  }

  gfx::PointF GetPointInChild() {
    gfx::PointF point_f;
    std::string str = EvalJs(child_frame_tree_node_->current_frame_host(),
                             "get_top_left_of_text()")
                          .ExtractString();
    ConvertJSONToPoint(str, &point_f);
    // Offset the point so that it is within the text. Character dimensions are
    // based on the font size in `touch_selection.html`.
    constexpr int kCharacterWidth = 15;
    constexpr int kCharacterHeight = 15;
    point_f.Offset(2 * kCharacterWidth, 0.5f * kCharacterHeight);
    point_f = child_rwhv()->TransformPointToRootCoordSpaceF(point_f);
    return point_f;
  }

  void VerifyHandlePosition() {
    // Check that selection handles are close to the selection range.
    // The test will timeout if this never happens.
    ui::TouchSelectionController* touch_selection_controller =
        root_rwhv()->touch_selection_controller();

    bool handles_in_place = false;
    while (!handles_in_place) {
      gfx::PointF selection_start =
          touch_selection_controller->GetStartPosition();
      gfx::PointF selection_end = touch_selection_controller->GetEndPosition();
      gfx::RectF handle_start =
          touch_selection_controller->GetStartHandleRect();
      gfx::RectF handle_end = touch_selection_controller->GetEndHandleRect();

      // Not all Android bots seem to actually show the handle, so check first.
      if (handle_start.IsEmpty()) {
        handles_in_place = true;
      } else {
        bool has_end_handle =
            !touch_selection_controller->GetEndHandleRect().IsEmpty();
        // handle_start.y() defined the top of the handle's rect, and x() is
        // left.
        bool start_near_y =
            std::abs(selection_start.y() - handle_start.y()) <= 3.f;
        bool start_in_x_range = selection_start.x() >= handle_start.x() &&
                                selection_start.x() <= handle_start.right();
        bool end_near_y = std::abs(selection_end.y() - handle_end.y()) <= 3.f;
        bool end_in_x_range = selection_end.x() >= handle_end.x() &&
                              selection_end.x() <= handle_end.right();
        handles_in_place = start_near_y && start_in_x_range && end_near_y &&
                           end_in_x_range && has_end_handle;
      }
      if (!handles_in_place)
        DelayBy(base::Milliseconds(100));
    }
  }

  RenderWidgetHostViewAndroid* root_rwhv() { return root_rwhv_; }

  RenderWidgetHostViewChildFrame* child_rwhv() { return child_rwhv_; }

  float PageScaleFactor() {
    return frame_observer_->LastRenderFrameMetadata().page_scale_factor;
  }

  TouchSelectionControllerClientTestWrapper* selection_controller_client() {
    return selection_controller_client_;
  }

  void OnSyntheticGestureSent() {
    gesture_run_loop_ = std::make_unique<base::RunLoop>();
    gesture_run_loop_->Run();
  }

  void OnSyntheticGestureCompleted(SyntheticGesture::Result result) {
    EXPECT_EQ(SyntheticGesture::GESTURE_FINISHED, result);
    gesture_run_loop_->Quit();
  }

 protected:
  void DelayBy(base::TimeDelta delta) {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), delta);
    run_loop.Run();
  }

 private:
  void SendTouch(RenderWidgetHostViewAndroid* view,
                 ui::MotionEvent::Action action,
                 gfx::Point point) {
    DCHECK(action >= ui::MotionEvent::Action::DOWN &&
           action < ui::MotionEvent::Action::CANCEL);

    ui::MotionEventAndroid::Pointer p(0, point.x(), point.y(), 10, 0, 0, 0, 0);
    JNIEnv* env = base::android::AttachCurrentThread();
    auto time_ns = (ui::EventTimeForNow() - base::TimeTicks()).InNanoseconds();
    ui::MotionEventAndroidJava touch(
        env, nullptr, 1.f, 0, 0, 0, base::TimeTicks::FromJavaNanoTime(time_ns),
        ui::MotionEventAndroid::GetAndroidAction(action), 1, 0, 0, 0, 0, 0, 0,
        0, 0, 0, false, &p, nullptr);
    view->OnTouchEvent(touch);
  }

  raw_ptr<RenderWidgetHostViewAndroid, DanglingUntriaged> root_rwhv_;
  raw_ptr<RenderWidgetHostViewChildFrame, DanglingUntriaged> child_rwhv_;
  raw_ptr<FrameTreeNode, DanglingUntriaged> child_frame_tree_node_;
  std::unique_ptr<RenderFrameSubmissionObserver> frame_observer_;
  raw_ptr<TouchSelectionControllerClientTestWrapper, DanglingUntriaged>
      selection_controller_client_;

  std::unique_ptr<base::RunLoop> gesture_run_loop_;
};

IN_PROC_BROWSER_TEST_P(TouchSelectionControllerClientAndroidSiteIsolationTest,
                       BasicSelectionIsolatedIframe) {
  // Load test URL with cross-process child.
  SetupTest();

  EXPECT_EQ(ui::TouchSelectionController::INACTIVE,
            root_rwhv()->touch_selection_controller()->active_status());
  // Find the location of some text to select.
  gfx::PointF point_f = GetPointInChild();

  // Initiate selection with a sequence of events that go through the targeting
  // system.
  selection_controller_client()->InitWaitForSelectionEvent(
      ui::SELECTION_HANDLES_SHOWN);

  SelectWithLongPress(gfx::Point(point_f.x(), point_f.y()));

  selection_controller_client()->Wait();

  // Check that selection is active and the quick menu is showing.
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            root_rwhv()->touch_selection_controller()->active_status());

  // Make sure handles are correctly positioned.
  VerifyHandlePosition();

  // Tap inside/outside the iframe and make sure the selection handles go away.
  selection_controller_client()->InitWaitForSelectionEvent(
      ui::SELECTION_HANDLES_CLEARED);
  // Since Android tests may run with page_scale_factor < 1, use an offset a
  // bigger than +/-1 for doing the inside/outside taps to cancel the selection
  // handles.
  gfx::PointF point_inside_iframe =
      child_rwhv()->TransformPointToRootCoordSpaceF(gfx::PointF(+5.f, +5.f));
  SimpleTap(gfx::Point(point_inside_iframe.x(), point_inside_iframe.y()));
  selection_controller_client()->Wait();

  EXPECT_EQ(ui::TouchSelectionController::INACTIVE,
            root_rwhv()->touch_selection_controller()->active_status());

  // Let's wait for the previous events to clear the round-trip to the renders
  // and back.
  DelayBy(base::Milliseconds(2000));

  // Initiate selection with a sequence of events that go through the targeting
  // system. Repeat of above but this time we'l cancel the selection by
  // tapping outside of the OOPIF.
  selection_controller_client()->InitWaitForSelectionEvent(
      ui::SELECTION_HANDLES_SHOWN);

  SelectWithLongPress(gfx::Point(point_f.x(), point_f.y()));

  selection_controller_client()->Wait();

  // Check that selection is active and the quick menu is showing.
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            root_rwhv()->touch_selection_controller()->active_status());

  // Tap inside/outside the iframe and make sure the selection handles go away.
  selection_controller_client()->InitWaitForSelectionEvent(
      ui::SELECTION_HANDLES_CLEARED);
  // Since Android tests may run with page_scale_factor < 1, use an offset a
  // bigger than +/-1 for doing the inside/outside taps to cancel the selection
  // handles.
  gfx::PointF point_outside_iframe =
      child_rwhv()->TransformPointToRootCoordSpaceF(gfx::PointF(-5.f, -5.f));
  SimpleTap(gfx::Point(point_outside_iframe.x(), point_outside_iframe.y()));
  selection_controller_client()->Wait();

  EXPECT_EQ(ui::TouchSelectionController::INACTIVE,
            root_rwhv()->touch_selection_controller()->active_status());

  // Cleanup before shutting down.
  ShutdownTest();
}

// This test verifies that the handles associated with an active touch selection
// are still correctly positioned after a pinch-zoom operation.
#if BUILDFLAG(IS_ANDROID)  // Flaky on Android.  See https://crbug.com/906204.
#define MAYBE_SelectionThenPinchInOOPIF DISABLED_SelectionThenPinchInOOPIF
#else
#define MAYBE_SelectionThenPinchInOOPIF SelectionThenPinchInOOPIF
#endif
IN_PROC_BROWSER_TEST_P(TouchSelectionControllerClientAndroidSiteIsolationTest,
                       MAYBE_SelectionThenPinchInOOPIF) {
  // Load test URL with cross-process child.
  SetupTest();

  EXPECT_EQ(ui::TouchSelectionController::INACTIVE,
            root_rwhv()->touch_selection_controller()->active_status());
  // Find the location of some text to select.
  gfx::PointF point_f = GetPointInChild();

  // Initiate selection with a sequence of events that go through the targeting
  // system.
  selection_controller_client()->InitWaitForSelectionEvent(
      ui::SELECTION_HANDLES_SHOWN);

  SelectWithLongPress(gfx::Point(point_f.x(), point_f.y()));

  selection_controller_client()->Wait();

  // Check that selection is active and the quick menu is showing.
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            root_rwhv()->touch_selection_controller()->active_status());

  // Make sure handles are correctly positioned.
  VerifyHandlePosition();

  // Generate a pinch sequence, then re-verify handles are in the correct
  // location.
  float page_scale_delta = 2.f;
  float current_page_scale = PageScaleFactor();
  float target_page_scale = current_page_scale * page_scale_delta;

  SyntheticPinchGestureParams params;
  // We'll use the selection point for the pinch center to minimize the
  // likelihood of the selection getting zoomed offscreen.
  params.anchor = point_f;
  // Note: the |scale_factor| in |params| is actually treated as a delta, not
  // absolute, page scale.
  params.scale_factor = page_scale_delta;
  auto synthetic_pinch_gesture =
      std::make_unique<SyntheticTouchscreenPinchGesture>(params);

  auto* host =
      static_cast<RenderWidgetHostImpl*>(root_rwhv()->GetRenderWidgetHost());
  InputEventAckWaiter gesture_pinch_end_waiter(
      host, blink::WebInputEvent::Type::kGesturePinchEnd);
  host->QueueSyntheticGesture(
      std::move(synthetic_pinch_gesture),
      base::BindOnce(&TouchSelectionControllerClientAndroidSiteIsolationTest::
                         OnSyntheticGestureCompleted,
                     base::Unretained(this)));
  OnSyntheticGestureSent();
  // Make sure the gesture is complete from the renderer's point of view.
  gesture_pinch_end_waiter.Wait();

  VerifyHandlePosition();
  // TODO(wjmaclean): Investigate why SyntheticTouchscreenPinchGesture final
  // scales are so imprecise.
  // https://crbug.com/897173
  const float kScaleFactorTolerance = 0.05f;
  EXPECT_NEAR(target_page_scale, PageScaleFactor(), kScaleFactorTolerance);

  // Cleanup before shutting down.
  ShutdownTest();
}
#endif  // BUILDFLAG(IS_ANDROID)

class TouchEventObserver : public RenderWidgetHost::InputEventObserver {
 public:
  TouchEventObserver(std::vector<uint32_t>* outgoing_touch_event_ids,
                     std::vector<uint32_t>* acked_touch_event_ids)
      : outgoing_touch_event_ids_(outgoing_touch_event_ids),
        acked_touch_event_ids_(acked_touch_event_ids) {}

  TouchEventObserver(const TouchEventObserver&) = delete;
  TouchEventObserver& operator=(const TouchEventObserver&) = delete;

  void OnInputEvent(const blink::WebInputEvent& event) override {
    if (!blink::WebInputEvent::IsTouchEventType(event.GetType()))
      return;

    const auto& touch_event = static_cast<const blink::WebTouchEvent&>(event);
    outgoing_touch_event_ids_->push_back(touch_event.unique_touch_event_id);
  }

  void OnInputEventAck(blink::mojom::InputEventResultSource source,
                       blink::mojom::InputEventResultState state,
                       const blink::WebInputEvent& event) override {
    if (!blink::WebInputEvent::IsTouchEventType(event.GetType()))
      return;

    const auto& touch_event = static_cast<const blink::WebTouchEvent&>(event);
    acked_touch_event_ids_->push_back(touch_event.unique_touch_event_id);
  }

 private:
  raw_ptr<std::vector<uint32_t>> outgoing_touch_event_ids_;
  raw_ptr<std::vector<uint32_t>> acked_touch_event_ids_;
};

// This test verifies the ability of the TouchEventAckQueue to send TouchEvent
// acks to the root view in the correct order in the event of a slow renderer.
// This test uses a main-frame which acks instantly (no touch handler), and a
// child frame which acks very slowly. A synthetic gesture tap is sent to the
// child first, then the main frame. In this scenario, we expect the touch
// events sent to the main-frame to ack first, which will be problematic if
// the events are acked to the GestureRecognizer out of order.
//
// This test is disabled due to flakiness on all platforms, but especially on
// Android.  See https://crbug.com/945025.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       DISABLED_TouchEventAckQueueOrdering) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1u, root->child_count());
  FrameTreeNode* child_node = root->child_at(0);

  // Add a *slow* & non-passive touch event handler in the child. It needs to
  // be non-passive to ensure TouchStart doesn't get acked until after the
  // touch handler completes.
  EXPECT_TRUE(ExecJs(child_node,
                     "touch_event_count = 0;\
       function touch_handler(ev) {\
         var start = Date.now();\
         while (Date.now() < start + 1000) {}\
         touch_event_count++;\
       }\
       document.body.addEventListener('touchstart', touch_handler,\
                                      { passive : false });\
       document.body.addEventListener('touchend', touch_handler,\
                                      { passive : false });"));

  WaitForHitTestData(child_node->current_frame_host());

  auto* root_host = static_cast<RenderWidgetHostImpl*>(
      root->current_frame_host()->GetRenderWidgetHost());
  auto* child_host = static_cast<RenderWidgetHostImpl*>(
      child_node->current_frame_host()->GetRenderWidgetHost());

  // Create InputEventObserver for both, with access to common queue for
  // logging.
  std::vector<uint32_t> outgoing_touch_event_ids;
  std::vector<uint32_t> acked_touch_event_ids;

  TouchEventObserver parent_touch_event_observer(&outgoing_touch_event_ids,
                                                 &acked_touch_event_ids);
  TouchEventObserver child_touch_event_observer(&outgoing_touch_event_ids,
                                                &acked_touch_event_ids);

  root_host->AddInputEventObserver(&parent_touch_event_observer);
  child_host->AddInputEventObserver(&child_touch_event_observer);

  InputEventAckWaiter root_ack_waiter(root_host,
                                      blink::WebInputEvent::Type::kTouchEnd);
  InputEventAckWaiter child_ack_waiter(child_host,
                                       blink::WebInputEvent::Type::kTouchEnd);
  InputEventAckWaiter child_gesture_tap_ack_waiter(
      child_host, blink::WebInputEvent::Type::kGestureTap);

  // Create GestureTap for child.
  gfx::PointF child_tap_point;
  {
    // We need to know the center of the child's body, but in root view
    // coordinates.
    std::string str = EvalJs(child_node,
                             "var rect = document.body.getBoundingClientRect();\
         var point = {\
           x: rect.left + rect.width / 2,\
           y: rect.top + rect.height / 2\
         };\
         JSON.stringify(point);")
                          .ExtractString();
    ConvertJSONToPoint(str, &child_tap_point);
    child_tap_point = child_node->current_frame_host()
                          ->GetView()
                          ->TransformPointToRootCoordSpaceF(child_tap_point);
  }
  SyntheticTapGestureParams child_tap_params;
  child_tap_params.position = child_tap_point;
  child_tap_params.gesture_source_type =
      content::mojom::GestureSourceType::kTouchInput;
  child_tap_params.duration_ms = 300.f;
  auto child_tap_gesture =
      std::make_unique<SyntheticTapGesture>(child_tap_params);

  // Create GestureTap for root.
  SyntheticTapGestureParams root_tap_params;
  root_tap_params.position = gfx::PointF(5.f, 5.f);
  root_tap_params.duration_ms = 300.f;
  root_tap_params.gesture_source_type =
      content::mojom::GestureSourceType::kTouchInput;
  auto root_tap_gesture =
      std::make_unique<SyntheticTapGesture>(root_tap_params);

  // Queue both GestureTaps, child first.
  // Note that we want the SyntheticGestureController to start sending the
  // root tap gesture as soon as it's finished sending the events for the
  // child tap gesture, otherwise it would wait for the acks from the child
  // before starting the root gesture which defeats the purpose of this test.
  root_host->QueueSyntheticGestureCompleteImmediately(
      std::move(child_tap_gesture));
  root_host->QueueSyntheticGesture(
      std::move(root_tap_gesture),
      base::BindOnce([](SyntheticGesture::Result result) {
        EXPECT_EQ(SyntheticGesture::GESTURE_FINISHED, result);
      }));

  root_ack_waiter.Wait();
  child_ack_waiter.Wait();

  // Verify the child did receive two touch events.
  EXPECT_EQ(2, EvalJs(child_node, "touch_event_count;"));

  // Verify Acks from parent arrive first.
  EXPECT_EQ(4u, outgoing_touch_event_ids.size());
  EXPECT_EQ(4u, acked_touch_event_ids.size());
  EXPECT_EQ(outgoing_touch_event_ids[2], acked_touch_event_ids[0]);
  EXPECT_EQ(outgoing_touch_event_ids[3], acked_touch_event_ids[1]);

  // Verify no DCHECKs from GestureRecognizer, indicating acks happened in
  // order.
  child_gesture_tap_ack_waiter.Wait();
}

// Verify that sandbox flags specified by a CSP header are properly inherited by
// child frames, but are removed when the frame navigates.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       ActiveSandboxFlagsMaintainedAcrossNavigation) {
  bool sandboxed_iframes_are_isolated =
      SiteIsolationPolicy::AreIsolatedSandboxedIframesEnabled();
  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/sandbox_main_frame_csp.html"));
  RenderFrameDeletedObserver deleted_observer(
      web_contents()->GetPrimaryFrameTree().root()->current_frame_host());
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  if (sandboxed_iframes_are_isolated) {
    // The initial navigation is away from an initial un-sandboxed mainframe to
    // a sandboxed mainframe, so before we call DepictFrameTree below we need to
    // wait for the RenderFrameHost from the initial mainframe to be deleted and
    // its proxies removed.
    // TODO(crbug.com/40282613): See if we can reuse the initial RFH for
    // a navigation to a sandboxed frame instead?
    deleted_observer.WaitUntilDeleted();
  }

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1u, root->child_count());

  EXPECT_EQ(
      base::StringPrintf(" Site A\n"
                         "   +--Site A\n"
                         "Where A = http://a.com/%s",
                         sandboxed_iframes_are_isolated ? " (sandboxed)" : ""),
      DepictFrameTree(root));
  if (sandboxed_iframes_are_isolated &&
      blink::features::kIsolateSandboxedIframesGroupingParam.Get() ==
          blink::features::IsolateSandboxedIframesGrouping::kPerOrigin) {
    // In per-origin IsolatedSandboxedIframes mode, the server port is retained
    // in the site URL.
    GURL main_site(embedded_test_server()->GetURL("a.com", "/"));
    EXPECT_EQ(main_site,
              root->current_frame_host()->GetSiteInstance()->GetSiteURL());
  }

  FrameTreeNode* child_node = root->child_at(0);

  EXPECT_EQ(shell()->web_contents()->GetSiteInstance(),
            child_node->current_frame_host()->GetSiteInstance());

  // Main page is served with a CSP header applying sandbox flags allow-popups,
  // allow-pointer-lock and allow-scripts.
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            root->pending_frame_policy().sandbox_flags);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            root->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(
      network::mojom::WebSandboxFlags::kAll &
          ~network::mojom::WebSandboxFlags::kAutomaticFeatures &
          ~network::mojom::WebSandboxFlags::kPointerLock &
          ~network::mojom::WebSandboxFlags::kPopups &
          ~network::mojom::WebSandboxFlags::kScripts &
          ~network::mojom::WebSandboxFlags::kTopNavigationToCustomProtocols,
      root->active_sandbox_flags());

  // Child frame has iframe sandbox flags allow-popups, allow-scripts, and
  // allow-orientation-lock. It should receive the intersection of those with
  // the parent sandbox flags: allow-popups and allow-scripts.
  EXPECT_EQ(
      network::mojom::WebSandboxFlags::kAll &
          ~network::mojom::WebSandboxFlags::kAutomaticFeatures &
          ~network::mojom::WebSandboxFlags::kPopups &
          ~network::mojom::WebSandboxFlags::kScripts &
          ~network::mojom::WebSandboxFlags::kTopNavigationToCustomProtocols,
      root->child_at(0)->pending_frame_policy().sandbox_flags);
  EXPECT_EQ(
      network::mojom::WebSandboxFlags::kAll &
          ~network::mojom::WebSandboxFlags::kAutomaticFeatures &
          ~network::mojom::WebSandboxFlags::kPopups &
          ~network::mojom::WebSandboxFlags::kScripts &
          ~network::mojom::WebSandboxFlags::kTopNavigationToCustomProtocols,
      root->child_at(0)->effective_frame_policy().sandbox_flags);

  // Document in child frame is served with a CSP header giving sandbox flags
  // allow-scripts, allow-popups and allow-pointer-lock. The final effective
  // flags should only include allow-scripts and allow-popups.
  EXPECT_EQ(
      network::mojom::WebSandboxFlags::kAll &
          ~network::mojom::WebSandboxFlags::kAutomaticFeatures &
          ~network::mojom::WebSandboxFlags::kPopups &
          ~network::mojom::WebSandboxFlags::kScripts &
          ~network::mojom::WebSandboxFlags::kTopNavigationToCustomProtocols,
      root->child_at(0)->active_sandbox_flags());

  // Navigate the child frame to a new page. This should clear any CSP-applied
  // sandbox flags.
  GURL frame_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), frame_url));

  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            child_node->current_frame_host()->GetSiteInstance());

  // Navigating should reset the sandbox flags to the frame owner flags:
  // allow-popups and allow-scripts.
  EXPECT_EQ(
      network::mojom::WebSandboxFlags::kAll &
          ~network::mojom::WebSandboxFlags::kAutomaticFeatures &
          ~network::mojom::WebSandboxFlags::kPopups &
          ~network::mojom::WebSandboxFlags::kScripts &
          ~network::mojom::WebSandboxFlags::kTopNavigationToCustomProtocols,
      root->child_at(0)->active_sandbox_flags());
  EXPECT_EQ(
      network::mojom::WebSandboxFlags::kAll &
          ~network::mojom::WebSandboxFlags::kAutomaticFeatures &
          ~network::mojom::WebSandboxFlags::kPopups &
          ~network::mojom::WebSandboxFlags::kScripts &
          ~network::mojom::WebSandboxFlags::kTopNavigationToCustomProtocols,
      root->child_at(0)->pending_frame_policy().sandbox_flags);
  EXPECT_EQ(
      network::mojom::WebSandboxFlags::kAll &
          ~network::mojom::WebSandboxFlags::kAutomaticFeatures &
          ~network::mojom::WebSandboxFlags::kPopups &
          ~network::mojom::WebSandboxFlags::kScripts &
          ~network::mojom::WebSandboxFlags::kTopNavigationToCustomProtocols,
      root->child_at(0)->effective_frame_policy().sandbox_flags);
}

// Test that after an RFH is unloaded, its old sandbox flags remain active.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       ActiveSandboxFlagsRetainedAfterUnload) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/sandboxed_main_frame_script.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  RenderFrameHostImpl* rfh =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetPrimaryMainFrame();

  // Check sandbox flags on RFH before navigating away.
  EXPECT_EQ(
      network::mojom::WebSandboxFlags::kAll &
          ~network::mojom::WebSandboxFlags::kAutomaticFeatures &
          ~network::mojom::WebSandboxFlags::kPointerLock &
          ~network::mojom::WebSandboxFlags::kPopups &
          ~network::mojom::WebSandboxFlags::kScripts &
          ~network::mojom::WebSandboxFlags::kTopNavigationToCustomProtocols,
      rfh->active_sandbox_flags());

  // Set up a slow unload handler to force the RFH to linger in the unloaded but
  // not-yet-deleted state.
  EXPECT_TRUE(ExecJs(rfh, "window.onunload=function(e){ while(1); };\n"));

  rfh->DisableUnloadTimerForTesting();
  RenderFrameDeletedObserver rfh_observer(rfh);

  // Navigate to a page with no sandbox, but wait for commit, not for the actual
  // load to finish.
  TestFrameNavigationObserver commit_observer(root);
  shell()->LoadURL(
      GURL(embedded_test_server()->GetURL("b.com", "/title1.html")));
  commit_observer.WaitForCommit();

  // The previous RFH should be either:
  // 1) In the BackForwardCache, or
  // 2) Pending deletion, waiting for the
  // mojo::AgentSchedulingGroupHost::DidUnloadRenderFrame. As a result, it must
  // still be alive.
  ASSERT_TRUE(rfh->IsRenderFrameLive());
  EXPECT_THAT(
      rfh->lifecycle_state(),
      testing::AnyOf(
          testing::Eq(
              RenderFrameHostImpl::LifecycleStateImpl::kRunningUnloadHandlers),
          testing::Eq(
              RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache)));

  ASSERT_FALSE(rfh_observer.deleted());

  // Check sandbox flags on old RFH -- they should be unchanged.
  EXPECT_EQ(
      network::mojom::WebSandboxFlags::kAll &
          ~network::mojom::WebSandboxFlags::kAutomaticFeatures &
          ~network::mojom::WebSandboxFlags::kPointerLock &
          ~network::mojom::WebSandboxFlags::kPopups &
          ~network::mojom::WebSandboxFlags::kScripts &
          ~network::mojom::WebSandboxFlags::kTopNavigationToCustomProtocols,
      rfh->active_sandbox_flags());

  // The FrameTreeNode should have flags which represent the new state.
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            root->effective_frame_policy().sandbox_flags);
}

// Verify that when CSP-set sandbox flags on a page change due to navigation,
// the new flags are propagated to proxies in other SiteInstances.
//
//   A        A         A         A
//    \        \         \         \     .
//     B  ->    B*   ->   B*   ->   B*
//             /  \      /  \      /  \  .
//            B    B    A    B    C    B
//
// (B* has CSP-set sandbox flags)
// The test checks sandbox flags for the proxy added in step 2, by checking
// whether the grandchild frames navigated to in step 3 and 4 see the correct
// sandbox flags.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       ActiveSandboxFlagsCorrectInProxies) {
  bool sandboxed_iframes_are_isolated =
      SiteIsolationPolicy::AreIsolatedSandboxedIframesEnabled();
  GURL main_url(embedded_test_server()->GetURL(
      "foo.com", "/cross_site_iframe_factory.html?foo(bar)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  TestNavigationObserver observer(shell()->web_contents());

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://foo.com/\n"
      "      B = http://bar.com/",
      DepictFrameTree(root));

  // Navigate the child to a CSP-sandboxed page on the same origin as it is
  // currently. This should update the flags in its proxies as well.
  auto* child = root->child_at(0);
  RenderFrameDeletedObserver deleted_observer_child(
      child->current_frame_host());
  EXPECT_TRUE(NavigateToURLFromRenderer(
      root->child_at(0),
      embedded_test_server()->GetURL("bar.com", "/csp_sandboxed_frame.html")));
  // DepictFrameTree remembers all the sites it has seen in the test, so the
  // expected output changes depending on whether we have additional sites from
  // process-isolated sandboxed frames. How many additional sites we have
  // depends on the grouping mode.
  if (sandboxed_iframes_are_isolated) {
    // Sandboxed iframes force the RFH to change; wait for the old one to go
    // away so that proxies in its SiteInstance don't affect DepictFrameTree
    // output.
    deleted_observer_child.WaitUntilDeleted();
    switch (blink::features::kIsolateSandboxedIframesGroupingParam.Get()) {
      case blink::features::IsolateSandboxedIframesGrouping::kPerSite:
      case blink::features::IsolateSandboxedIframesGrouping::kPerOrigin:
        EXPECT_EQ(
            " Site A ------------ proxies for C\n"
            "   +--Site C ------- proxies for A\n"
            "        |--Site C -- proxies for A\n"
            "        +--Site C -- proxies for A\n"
            "Where A = http://foo.com/\n"
            "      C = http://bar.com/ (sandboxed)",
            DepictFrameTree(root));
        break;
      case blink::features::IsolateSandboxedIframesGrouping::kPerDocument:
        // TODO(crbug.com/40941714): Add output for the PerDocument
        // case, and parameterize this test to run all variants (none, per-site,
        // per-origin, per-document).
        break;
    }
  } else {
    EXPECT_EQ(
        " Site A ------------ proxies for B\n"
        "   +--Site B ------- proxies for A\n"
        "        |--Site B -- proxies for A\n"
        "        +--Site B -- proxies for A\n"
        "Where A = http://foo.com/\n"
        "      B = http://bar.com/",
        DepictFrameTree(root));
  }

  // Now navigate the first grandchild to a page on the same origin as the main
  // frame. It should still be sandboxed, as it should get its flags from its
  // (remote) parent.
  // TODO(crbug.com/40943240): When IsolateSandboxedIframes is enabled,
  // this test no longer uses proxy inheritance; the grandchild and the main
  // frame won't be in the same SiteInstance anymore, so this test will no
  // longer exercise sandbox flags inheritance from an existing remote frame.
  // Restructure the test so it still provides coverage for proxy inheritance
  // when IsolateSandboxedIframes is enabled.
  EXPECT_TRUE(NavigateToURLFromRenderer(
      root->child_at(0)->child_at(0),
      embedded_test_server()->GetURL("foo.com", "/title1.html")));

  if (sandboxed_iframes_are_isolated) {
    switch (blink::features::kIsolateSandboxedIframesGroupingParam.Get()) {
      case blink::features::IsolateSandboxedIframesGrouping::kPerSite:
      case blink::features::IsolateSandboxedIframesGrouping::kPerOrigin:
        EXPECT_EQ(
            " Site A ------------ proxies for C D\n"
            "   +--Site C ------- proxies for A D\n"
            "        |--Site D -- proxies for A C\n"
            "        +--Site C -- proxies for A D\n"
            "Where A = http://foo.com/\n"
            "      C = http://bar.com/ (sandboxed)\n"
            "      D = http://foo.com/ (sandboxed)",
            DepictFrameTree(root));
        break;
      case blink::features::IsolateSandboxedIframesGrouping::kPerDocument:
        // TODO(crbug.com/40941714): Add output for the PerDocument
        // case, and parameterize this test to run all variants (none, per-site,
        // per-origin, per-document).
        break;
    }
  } else {
    EXPECT_EQ(
        " Site A ------------ proxies for B\n"
        "   +--Site B ------- proxies for A\n"
        "        |--Site A -- proxies for B\n"
        "        +--Site B -- proxies for A\n"
        "Where A = http://foo.com/\n"
        "      B = http://bar.com/",
        DepictFrameTree(root));
  }

  // The child of the sandboxed frame should've inherited sandbox flags, so it
  // should not be able to create popups.
  EXPECT_EQ(
      network::mojom::WebSandboxFlags::kAll &
          ~network::mojom::WebSandboxFlags::kScripts &
          ~network::mojom::WebSandboxFlags::kAutomaticFeatures,
      root->child_at(0)->child_at(0)->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(
      root->child_at(0)->child_at(0)->active_sandbox_flags(),
      root->child_at(0)->child_at(0)->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(true, EvalJs(root->child_at(0)->child_at(0),
                         "!window.open('data:text/html,dataurl');"));
  EXPECT_EQ(1u, Shell::windows().size());

  // Finally, navigate the grandchild frame to a new origin, creating a new site
  // instance. Again, the new document should be sandboxed, as it should get its
  // flags from its (remote) parent in B.
  RenderFrameDeletedObserver deleted_observer_grandchild(
      root->child_at(0)->child_at(0)->current_frame_host());
  EXPECT_TRUE(NavigateToURLFromRenderer(
      root->child_at(0)->child_at(0),
      embedded_test_server()->GetURL("baz.com", "/title1.html")));

  deleted_observer_grandchild.WaitUntilDeleted();
  if (sandboxed_iframes_are_isolated) {
    switch (blink::features::kIsolateSandboxedIframesGroupingParam.Get()) {
      case blink::features::IsolateSandboxedIframesGrouping::kPerSite:
      case blink::features::IsolateSandboxedIframesGrouping::kPerOrigin:
        EXPECT_EQ(
            " Site A ------------ proxies for C E\n"
            "   +--Site C ------- proxies for A E\n"
            "        |--Site E -- proxies for A C\n"
            "        +--Site C -- proxies for A E\n"
            "Where A = http://foo.com/\n"
            "      C = http://bar.com/ (sandboxed)\n"
            "      E = http://baz.com/ (sandboxed)",
            DepictFrameTree(root));
        break;
      case blink::features::IsolateSandboxedIframesGrouping::kPerDocument:
        // TODO(crbug.com/40941714): Add output for the PerDocument
        // case, and parameterize this test to run all variants (none, per-site,
        // per-origin, per-document).
        break;
    }
  } else {
    EXPECT_EQ(
        " Site A ------------ proxies for B C\n"
        "   +--Site B ------- proxies for A C\n"
        "        |--Site C -- proxies for A B\n"
        "        +--Site B -- proxies for A C\n"
        "Where A = http://foo.com/\n"
        "      B = http://bar.com/\n"
        "      C = http://baz.com/",
        DepictFrameTree(root));
  }

  // The child of the sandboxed frame should've inherited sandbox flags, so it
  // should not be able to create popups.
  EXPECT_EQ(
      network::mojom::WebSandboxFlags::kAll &
          ~network::mojom::WebSandboxFlags::kScripts &
          ~network::mojom::WebSandboxFlags::kAutomaticFeatures,
      root->child_at(0)->child_at(0)->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(
      root->child_at(0)->child_at(0)->active_sandbox_flags(),
      root->child_at(0)->child_at(0)->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(true, EvalJs(root->child_at(0)->child_at(0),
                         "!window.open('data:text/html,dataurl');"));
  EXPECT_EQ(1u, Shell::windows().size());
}

// Verify that when the sandbox iframe attribute changes on a page which also
// has CSP-set sandbox flags, that the correct combination of flags is set in
// the sandboxed page after navigation.
//
//   A        A         A                                  A
//    \        \         \                                  \     .
//     B  ->    B*   ->   B*   -> (change sandbox attr) ->   B*
//             /  \      /  \                               /  \  .
//            B    B    A    B                             A'   B
//
// (B* has CSP-set sandbox flags)
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       ActiveSandboxFlagsCorrectAfterUpdate) {
  GURL main_url(embedded_test_server()->GetURL(
      "foo.com", "/cross_site_iframe_factory.html?foo(bar)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  TestNavigationObserver observer(shell()->web_contents());

  // Navigate the child to a CSP-sandboxed page on the same origin as it is
  // currently. This should update the flags in its proxies as well.
  EXPECT_TRUE(NavigateToURLFromRenderer(
      root->child_at(0),
      embedded_test_server()->GetURL("bar.com", "/csp_sandboxed_frame.html")));

  // Now navigate the first grandchild to a page on the same origin as the main
  // frame. It should still be sandboxed, as it should get its flags from its
  // (remote) parent.
  EXPECT_TRUE(NavigateToURLFromRenderer(
      root->child_at(0)->child_at(0),
      embedded_test_server()->GetURL("foo.com", "/title1.html")));

  // The child of the sandboxed frame should've inherited sandbox flags, so it
  // should not be able to create popups.
  EXPECT_EQ(
      network::mojom::WebSandboxFlags::kAll &
          ~network::mojom::WebSandboxFlags::kScripts &
          ~network::mojom::WebSandboxFlags::kAutomaticFeatures,
      root->child_at(0)->child_at(0)->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(
      root->child_at(0)->child_at(0)->active_sandbox_flags(),
      root->child_at(0)->child_at(0)->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(true, EvalJs(root->child_at(0)->child_at(0),
                         "!window.open('data:text/html,dataurl');"));
  EXPECT_EQ(1u, Shell::windows().size());

  // Update the sandbox attribute in the child frame. This should be overridden
  // by the CSP-set sandbox on this frame: The grandchild should *not* receive
  // an allowance for popups after it is navigated.
  EXPECT_TRUE(ExecJs(root->child_at(0),
                     "document.querySelector('iframe').sandbox = "
                     "    'allow-scripts allow-popups';"));
  // Finally, navigate the grandchild frame to another page on the top-level
  // origin; the active sandbox flags should still come from the it's parent's
  // CSP and the frame owner attributes.
  EXPECT_TRUE(NavigateToURLFromRenderer(
      root->child_at(0)->child_at(0),
      embedded_test_server()->GetURL("foo.com", "/title2.html")));
  EXPECT_EQ(
      network::mojom::WebSandboxFlags::kAll &
          ~network::mojom::WebSandboxFlags::kScripts &
          ~network::mojom::WebSandboxFlags::kAutomaticFeatures,
      root->child_at(0)->child_at(0)->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(
      root->child_at(0)->child_at(0)->active_sandbox_flags(),
      root->child_at(0)->child_at(0)->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(true, EvalJs(root->child_at(0)->child_at(0),
                         "!window.open('data:text/html,dataurl');"));
  EXPECT_EQ(1u, Shell::windows().size());
}

// Verify that when the sandbox iframe attribute is removed from a page which
// also has CSP-set sandbox flags, that the flags are cleared in the browser
// and renderers (including proxies) after navigation to a page without CSP-set
// flags.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       ActiveSandboxFlagsCorrectWhenCleared) {
  GURL main_url(
      embedded_test_server()->GetURL("foo.com", "/sandboxed_frames_csp.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  TestNavigationObserver observer(shell()->web_contents());

  // The second child has both iframe-attribute sandbox flags and CSP-set flags.
  // Verify that it the flags are combined correctly in the frame tree.
  EXPECT_EQ(network::mojom::WebSandboxFlags::kAll &
                ~network::mojom::WebSandboxFlags::kPointerLock &
                ~network::mojom::WebSandboxFlags::kOrientationLock &
                ~network::mojom::WebSandboxFlags::kScripts &
                ~network::mojom::WebSandboxFlags::kAutomaticFeatures,
            root->child_at(1)->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kAll &
                ~network::mojom::WebSandboxFlags::kPointerLock &
                ~network::mojom::WebSandboxFlags::kScripts &
                ~network::mojom::WebSandboxFlags::kAutomaticFeatures,
            root->child_at(1)->active_sandbox_flags());

  EXPECT_TRUE(NavigateToURLFromRenderer(
      root->child_at(1), embedded_test_server()->GetURL(
                             "bar.com", "/sandboxed_child_frame.html")));
  EXPECT_EQ(network::mojom::WebSandboxFlags::kAll &
                ~network::mojom::WebSandboxFlags::kPointerLock &
                ~network::mojom::WebSandboxFlags::kOrientationLock &
                ~network::mojom::WebSandboxFlags::kScripts &
                ~network::mojom::WebSandboxFlags::kAutomaticFeatures,
            root->child_at(1)->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kAll &
                ~network::mojom::WebSandboxFlags::kPointerLock &
                ~network::mojom::WebSandboxFlags::kScripts &
                ~network::mojom::WebSandboxFlags::kAutomaticFeatures,
            root->child_at(1)->active_sandbox_flags());

  // Remove the sandbox attribute from the child frame.
  EXPECT_TRUE(ExecJs(root,
                     "document.querySelectorAll('iframe')[1]"
                     ".removeAttribute('sandbox');"));
  // Finally, navigate that child frame to another page on the same origin with
  // no CSP-set sandbox. Its sandbox flags should be completely cleared, and
  // should be cleared in the proxy in the main frame's renderer as well.
  // We can check that the flags were properly cleared by nesting another frame
  // under the child, and ensuring that *it* saw no sandbox flags in the
  // browser, or in the RemoteSecurityContext in the main frame's renderer.
  EXPECT_TRUE(NavigateToURLFromRenderer(
      root->child_at(1),
      embedded_test_server()->GetURL(
          "bar.com", "/cross_site_iframe_factory.html?bar(foo)")));

  // Check the sandbox flags on the child frame in the browser process.
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            root->child_at(1)->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            root->child_at(1)->active_sandbox_flags());

  // Check the sandbox flags on the grandchid frame in the browser process.
  EXPECT_EQ(
      network::mojom::WebSandboxFlags::kNone,
      root->child_at(1)->child_at(0)->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(
      root->child_at(1)->child_at(0)->active_sandbox_flags(),
      root->child_at(1)->child_at(0)->effective_frame_policy().sandbox_flags);

  // Check the sandbox flags in the grandchild frame's renderer by attempting
  // to open a popup. This should succeed.
  EXPECT_EQ(true, EvalJs(root->child_at(1)->child_at(0),
                         "!!window.open('data:text/html,dataurl');"));
  EXPECT_EQ(2u, Shell::windows().size());
}

// Check that a subframe that requires a dedicated process will attempt to
// reuse an existing process for the same site, even across BrowsingInstances.
// This helps consolidate processes when running under --site-per-process.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       SubframeReusesExistingProcess) {
  GURL foo_url(
      embedded_test_server()->GetURL("foo.com", "/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);

  // Open an unrelated tab in a separate BrowsingInstance, and navigate it to
  // to bar.com.  This SiteInstance should have a default process reuse
  // policy - only subframes attempt process reuse.
  GURL bar_url(
      embedded_test_server()->GetURL("bar.com", "/page_with_iframe.html"));
  Shell* second_shell = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(second_shell, bar_url));
  scoped_refptr<SiteInstanceImpl> second_shell_instance =
      static_cast<SiteInstanceImpl*>(second_shell->web_contents()
                                         ->GetPrimaryMainFrame()
                                         ->GetSiteInstance());
  EXPECT_FALSE(second_shell_instance->IsRelatedSiteInstance(
      root->current_frame_host()->GetSiteInstance()));
  RenderProcessHost* bar_process = second_shell_instance->GetProcess();
  EXPECT_EQ(ProcessReusePolicy::DEFAULT,
            second_shell_instance->process_reuse_policy());

  // Now navigate the first tab's subframe to bar.com.  Confirm that it reuses
  // |bar_process|.
  NavigateIframeToURL(web_contents(), "test_iframe", bar_url);
  EXPECT_EQ(bar_url, child->current_url());
  EXPECT_EQ(bar_process, child->current_frame_host()->GetProcess());
  EXPECT_EQ(
      ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE_SUBFRAME,
      child->current_frame_host()->GetSiteInstance()->process_reuse_policy());

  EXPECT_TRUE(child->current_frame_host()->IsCrossProcessSubframe());
  EXPECT_EQ(
      bar_url.host(),
      child->current_frame_host()->GetSiteInstance()->GetSiteURL().host());

  // The subframe's SiteInstance should still be different from second_shell's
  // SiteInstance, and they should be in separate BrowsingInstances.
  EXPECT_NE(second_shell_instance,
            child->current_frame_host()->GetSiteInstance());
  EXPECT_FALSE(second_shell_instance->IsRelatedSiteInstance(
      child->current_frame_host()->GetSiteInstance()));

  // Navigate the second tab to a foo.com URL with a same-site subframe.  This
  // leaves only the first tab's subframe in the bar.com process.
  EXPECT_TRUE(NavigateToURL(second_shell, foo_url));
  EXPECT_NE(bar_process,
            second_shell->web_contents()->GetPrimaryMainFrame()->GetProcess());

  // Navigate the second tab's subframe to bar.com, and check that this
  // new subframe reuses the process of the subframe in the first tab, even
  // though the two are in separate BrowsingInstances.
  NavigateIframeToURL(second_shell->web_contents(), "test_iframe", bar_url);
  FrameTreeNode* second_subframe =
      static_cast<WebContentsImpl*>(second_shell->web_contents())
          ->GetPrimaryFrameTree()
          .root()
          ->child_at(0);
  EXPECT_EQ(bar_process, second_subframe->current_frame_host()->GetProcess());
  EXPECT_NE(child->current_frame_host()->GetSiteInstance(),
            second_subframe->current_frame_host()->GetSiteInstance());

  // Open a third, unrelated tab, navigate it to bar.com, and check that
  // its main frame doesn't share a process with the existing bar.com
  // subframes.
  Shell* third_shell = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(third_shell, bar_url));
  SiteInstanceImpl* third_shell_instance = static_cast<SiteInstanceImpl*>(
      third_shell->web_contents()->GetPrimaryMainFrame()->GetSiteInstance());
  EXPECT_NE(third_shell_instance,
            second_subframe->current_frame_host()->GetSiteInstance());
  EXPECT_NE(third_shell_instance,
            child->current_frame_host()->GetSiteInstance());
  EXPECT_NE(third_shell_instance->GetProcess(), bar_process);
}

class SitePerProcessNoSharingBrowserTest : public SitePerProcessBrowserTest {
 public:
  SitePerProcessNoSharingBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(
        features::kProcessPerSiteUpToMainFrameThreshold);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Check that when a subframe reuses an existing process for the same site
// across BrowsingInstances, a browser-initiated navigation in that subframe's
// tab doesn't unnecessarily share the reused process.  See
// https://crbug.com/803367.
IN_PROC_BROWSER_TEST_P(SitePerProcessNoSharingBrowserTest,
                       NoProcessSharingAfterSubframeReusesExistingProcess) {
  GURL foo_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  SiteInstanceImpl* foo_instance =
      root->current_frame_host()->GetSiteInstance();

  // Open an unrelated tab in a separate BrowsingInstance, and navigate it to
  // to bar.com.
  GURL bar_url(
      embedded_test_server()->GetURL("bar.com", "/page_with_iframe.html"));
  Shell* second_shell = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(second_shell, bar_url));
  FrameTreeNode* second_root =
      static_cast<WebContentsImpl*>(second_shell->web_contents())
          ->GetPrimaryFrameTree()
          .root();
  FrameTreeNode* second_child = second_root->child_at(0);
  scoped_refptr<SiteInstanceImpl> bar_instance =
      second_root->current_frame_host()->GetSiteInstance();
  EXPECT_FALSE(bar_instance->IsRelatedSiteInstance(foo_instance));

  // Navigate the second tab's subframe to foo.com.  Confirm that it reuses
  // first tab's process.
  NavigateIframeToURL(second_shell->web_contents(), "test_iframe", foo_url);
  EXPECT_EQ(foo_url, second_child->current_url());
  scoped_refptr<SiteInstanceImpl> second_child_foo_instance =
      second_child->current_frame_host()->GetSiteInstance();
  EXPECT_EQ(ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE_SUBFRAME,
            second_child_foo_instance->process_reuse_policy());
  EXPECT_NE(foo_instance, second_child_foo_instance);
  EXPECT_EQ(foo_instance->GetProcess(),
            second_child_foo_instance->GetProcess());

  // Perform a browser-initiated address bar navigation in the second tab to
  // foo.com.  This should swap BrowsingInstances and end up in a separate
  // process from the first tab.
  EXPECT_TRUE(NavigateToURL(second_shell, foo_url));
  SiteInstanceImpl* new_instance =
      second_root->current_frame_host()->GetSiteInstance();
  EXPECT_NE(second_child_foo_instance, new_instance);
  EXPECT_FALSE(second_child_foo_instance->IsRelatedSiteInstance(new_instance));
  EXPECT_FALSE(bar_instance->IsRelatedSiteInstance(new_instance));
  EXPECT_FALSE(foo_instance->IsRelatedSiteInstance(new_instance));
  EXPECT_NE(new_instance->GetProcess(), foo_instance->GetProcess());
  EXPECT_NE(new_instance->GetProcess(), bar_instance->GetProcess());
}

namespace {

// Intercepts the next DidCommitProvisionalLoad message for |deferred_url| in
// any frame of the |web_contents|, and holds off on dispatching it until
// *after* the DidCommitProvisionalLoad message for the next navigation in the
// |web_contents| has been dispatched.
//
// Reversing the order in which the commit messages are dispatched simulates a
// busy renderer that takes a very long time to actually commit the navigation
// to |deferred_url| after receiving FrameNavigationControl::CommitNavigation;
// whereas there is a fast cross-site navigation taking place in the same
// frame which starts second but finishes first.
class CommitMessageOrderReverser : public DidCommitNavigationInterceptor {
 public:
  using DidStartDeferringCommitCallback =
      base::OnceCallback<void(RenderFrameHost*)>;

  CommitMessageOrderReverser(
      WebContents* web_contents,
      const GURL& deferred_url,
      DidStartDeferringCommitCallback deferred_url_triggered_action)
      : DidCommitNavigationInterceptor(web_contents),
        deferred_url_(deferred_url),
        deferred_url_triggered_action_(
            std::move(deferred_url_triggered_action)) {}

  CommitMessageOrderReverser(const CommitMessageOrderReverser&) = delete;
  CommitMessageOrderReverser& operator=(const CommitMessageOrderReverser&) =
      delete;

  ~CommitMessageOrderReverser() override = default;

  void WaitForBothCommits() { outer_run_loop.Run(); }

 protected:
  bool WillProcessDidCommitNavigation(
      RenderFrameHost* render_frame_host,
      NavigationRequest* navigation_request,
      mojom::DidCommitProvisionalLoadParamsPtr* params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr* interface_params)
      override {
    // The DidCommitProvisionalLoad message is dispatched once this method
    // returns, so to defer committing the the navigation to |deferred_url_|,
    // run a nested message loop until the subsequent other commit message is
    // dispatched.
    if ((**params).url == deferred_url_) {
      std::move(deferred_url_triggered_action_).Run(render_frame_host);

      base::RunLoop nested_run_loop(base::RunLoop::Type::kNestableTasksAllowed);
      nested_loop_quit_ = nested_run_loop.QuitClosure();
      nested_run_loop.Run();
      outer_run_loop.Quit();
    } else if (nested_loop_quit_) {
      std::move(nested_loop_quit_).Run();
    }
    return true;
  }

 private:
  base::RunLoop outer_run_loop;
  base::OnceClosure nested_loop_quit_;

  const GURL deferred_url_;
  DidStartDeferringCommitCallback deferred_url_triggered_action_;
};

}  // namespace

// Create an out-of-process iframe that causes itself to be detached during
// its layout/animate phase. See https://crbug.com/802932.
//
// TODO(crbug.com/40561636): Disabled on Android, Mac, and ChromeOS due to
// flakiness.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_OOPIFDetachDuringAnimation DISABLED_OOPIFDetachDuringAnimation
#else
#define MAYBE_OOPIFDetachDuringAnimation OOPIFDetachDuringAnimation
#endif
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       MAYBE_OOPIFDetachDuringAnimation) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/frame-detached-in-animationstart-event.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "        +--Site A -- proxies for B\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  FrameTreeNode* nested_child = root->child_at(0)->child_at(0);
  WaitForHitTestData(nested_child->current_frame_host());

  EXPECT_TRUE(ExecJs(nested_child->current_frame_host(), "startTest();"));

  // Test passes if the main renderer doesn't crash. Ping to verify.
  EXPECT_EQ(true, EvalJs(root->current_frame_host(), "true;"));
}

// Tests that a cross-process iframe asked to navigate to the same URL will
// successfully commit the navigation.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       IFrameSameDocumentNavigation) {
  GURL main_url(embedded_test_server()->GetURL(
      "foo.com", "/cross_site_iframe_factory.html?foo(bar)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* iframe = root->child_at(0);

  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            iframe->current_frame_host()->GetSiteInstance());

  // The iframe navigates same-document to a fragment.
  GURL iframe_fragment_url = GURL(iframe->current_url().spec() + "#foo");
  {
    TestNavigationObserver observer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(iframe->current_frame_host(),
                       JsReplace("location.href=$1", iframe_fragment_url)));
    observer.Wait();
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_EQ(iframe_fragment_url, iframe->current_url());
  }

  // The parent frame wants the iframe do a navigation to the same URL. Because
  // the URL has a fragment, this will be treated as a same-document navigation,
  // and not as a normal load of the same URL. This should succeed.
  {
    TestNavigationObserver observer(shell()->web_contents());
    EXPECT_TRUE(ExecJs(root->current_frame_host(),
                       JsReplace("document.getElementById('child-0').src=$1",
                                 iframe_fragment_url)));
    observer.Wait();
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_EQ(iframe_fragment_url, iframe->current_url());
  }
}

// Verifies the the renderer has the size of the frame after commit.
// https://crbug/804046, https://crbug.com/801091
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, SizeAvailableAfterCommit) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);

  GURL b_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  TestFrameNavigationObserver commit_observer(child);
  NavigationController::LoadURLParams params(b_url);
  params.transition_type = PageTransitionFromInt(ui::PAGE_TRANSITION_LINK);
  params.frame_tree_node_id = child->frame_tree_node_id();
  child->navigator().controller().LoadURLWithParams(params);
  commit_observer.WaitForCommit();

  EXPECT_GT(EvalJs(child, "window.innerHeight;").ExtractDouble(), 0);
}

// Test that a late mojo::AgentSchedulingGroupHost::DidUnloadRenderFrame won't
// incorrectly mark RenderViewHost as inactive if it's already been reused and
// switched to active by another navigation.  See https://crbug.com/823567.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       RenderViewHostStaysActiveWithLateUnloadACK) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));

  // Open a popup and navigate it to a.com.
  Shell* popup = OpenPopup(
      shell(), embedded_test_server()->GetURL("a.com", "/title2.html"), "foo");
  WebContentsImpl* popup_contents =
      static_cast<WebContentsImpl*>(popup->web_contents());
  RenderFrameHostImpl* rfh = popup_contents->GetPrimaryMainFrame();
  RenderViewHostImpl* rvh = rfh->render_view_host();

  // Disable the unload ACK and the unload timer.
  auto unload_ack_filter = base::BindRepeating([] { return true; });
  rfh->SetUnloadACKCallbackForTesting(unload_ack_filter);
  rfh->DisableUnloadTimerForTesting();

  // Navigate popup to b.com.  Because there's an opener, the RVH for a.com
  // stays around in swapped-out state.
  EXPECT_TRUE(NavigateToURLInSameBrowsingInstance(
      popup, embedded_test_server()->GetURL("b.com", "/title3.html")));
  EXPECT_FALSE(rvh->is_active());

  // The old RenderFrameHost is now pending deletion.
  ASSERT_TRUE(rfh->IsRenderFrameLive());
  ASSERT_TRUE(rfh->IsPendingDeletion());

  // Kill the b.com process.
  RenderProcessHost* b_process =
      popup_contents->GetPrimaryMainFrame()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      b_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  b_process->Shutdown(0);
  crash_observer.Wait();

  // Go back in the popup from b.com to a.com/title2.html.  Because the current
  // b.com RFH is dead, the new RFH is committed right away (without waiting
  // for renderer to commit), so that users don't need to look at the sad tab.
  TestNavigationObserver back_observer(popup_contents);
  popup_contents->GetController().GoBack();

  // Pretend that the original RFH in a.com now finishes running its unload
  // handler and sends the mojo::AgentSchedulingGroupHost::DidUnloadRenderFrame.
  rfh->OnUnloaded();

  // Wait for the new a.com navigation to finish.
  back_observer.Wait();

  // The RVH for a.com should've been reused, and it should be active.  Its
  // main frame should've been updated to the RFH from the back navigation.
  EXPECT_EQ(popup_contents->GetPrimaryMainFrame()->render_view_host(), rvh);
  EXPECT_TRUE(rvh->is_active());
  EXPECT_EQ(rvh->GetMainRenderFrameHost(),
            popup_contents->GetPrimaryMainFrame());
}

// Check that when A opens a new window with B which embeds an A subframe, the
// subframe is visible and generates paint events.  See
// https://crbug.com/638375.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       SubframeVisibleAfterRenderViewBecomesSwappedOut) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  GURL popup_url(embedded_test_server()->GetURL(
      "b.com", "/cross_site_iframe_factory.html?b(b)"));
  Shell* popup_shell = OpenPopup(shell()->web_contents(), popup_url, "popup");
  FrameTreeNode* popup_child =
      static_cast<WebContentsImpl*>(popup_shell->web_contents())
          ->GetPrimaryFrameTree()
          .root()
          ->child_at(0);

  // Navigate popup's subframe to a page on a.com, which will generate
  // continuous compositor frames by incrementing a counter on the page.
  EXPECT_TRUE(NavigateToURLFromRenderer(
      popup_child, embedded_test_server()->GetURL("a.com", "/counter.html")));

  RenderWidgetHostViewChildFrame* child_view =
      static_cast<RenderWidgetHostViewChildFrame*>(
          popup_child->current_frame_host()->GetView());

  // Make sure the child frame keeps generating compositor frames.
  RenderFrameSubmissionObserver frame_counter(
      child_view->host_->render_frame_metadata_provider());
  while (frame_counter.render_frame_count() < 10)
    frame_counter.WaitForAnyFrameSubmission();
}

IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, FrameDepthSimple) {
  // Five nodes, from depth 0 to 4.
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c(d(e))))"));
  const size_t number_of_nodes = 5;
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* node = web_contents()->GetPrimaryFrameTree().root();
  for (unsigned int expected_depth = 0; expected_depth < number_of_nodes;
       ++expected_depth) {
    CheckFrameDepth(expected_depth, node);

    if (expected_depth + 1 < number_of_nodes)
      node = node->child_at(0);
  }
}

IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, FrameDepthTest) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a,b(a))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  CheckFrameDepth(0u, root);

  FrameTreeNode* child0 = root->child_at(0);
  {
    EXPECT_EQ(1u, child0->current_frame_host()->GetFrameDepth());
    RenderProcessHostPriorityClient::Priority priority =
        child0->current_frame_host()->GetRenderWidgetHost()->GetPriority();
    // Same site instance as root.
    EXPECT_EQ(0u, priority.frame_depth);
    EXPECT_EQ(0u, child0->current_frame_host()->GetProcess()->GetFrameDepth());
  }

  FrameTreeNode* child1 = root->child_at(1);
  CheckFrameDepth(1u, child1);
  // In addition, site b's inactive Widget should not contribute priority.
  RenderViewHostImpl* child1_rvh =
      child1->current_frame_host()->render_view_host();
  EXPECT_FALSE(child1_rvh->is_active());
  EXPECT_EQ(RenderProcessHostImpl::kMaxFrameDepthForPriority,
            child1_rvh->GetWidget()->GetPriority().frame_depth);
  EXPECT_FALSE(static_cast<RenderWidgetHostOwnerDelegate*>(child1_rvh)
                   ->ShouldContributePriorityToProcess());

  FrameTreeNode* grand_child = root->child_at(1)->child_at(0);
  {
    EXPECT_EQ(2u, grand_child->current_frame_host()->GetFrameDepth());
    RenderProcessHostPriorityClient::Priority priority =
        grand_child->current_frame_host()->GetRenderWidgetHost()->GetPriority();
    EXPECT_EQ(2u, priority.frame_depth);
    // Same process as root
    EXPECT_EQ(0u,
              grand_child->current_frame_host()->GetProcess()->GetFrameDepth());
  }
}

// Disabled due to flakiness. crbug.com/1146083
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define MAYBE_VisibilityFrameDepthTest DISABLED_VisibilityFrameDepthTest
#else
#define MAYBE_VisibilityFrameDepthTest VisibilityFrameDepthTest
#endif
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       MAYBE_VisibilityFrameDepthTest) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL popup_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  Shell* new_shell = OpenPopup(root->child_at(0), popup_url, "");
  FrameTreeNode* popup_root =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetPrimaryFrameTree()
          .root();

  // Subframe and popup share the same process. Both are visible, so depth
  // should be 0.
  RenderProcessHost* subframe_process =
      root->child_at(0)->current_frame_host()->GetProcess();
  RenderProcessHost* popup_process =
      popup_root->current_frame_host()->GetProcess();
  EXPECT_EQ(subframe_process, popup_process);
  EXPECT_EQ(2, popup_process->VisibleClientCount());
  EXPECT_EQ(0u, popup_process->GetFrameDepth());

  // Hide popup. Process should have one visible client and depth should be 1,
  // since depth 0 popup is hidden.
  new_shell->web_contents()->WasHidden();
  EXPECT_EQ(1, popup_process->VisibleClientCount());
  EXPECT_EQ(1u, popup_process->GetFrameDepth());

  // Navigate main page to same origin as popup in same BrowsingInstance,
  // s main page should run in the same process as the popup. The depth on the
  // process should be 0, from the main frame of main page.
  EXPECT_TRUE(NavigateToURLInSameBrowsingInstance(shell(), popup_url));
  // Performing a Load causes aura window to be focused (see
  // Shell::LoadURLForFrame) which recomputes window occlusion for all windows
  // (on chromeos) which unhides the popup. Hide popup again.
  new_shell->web_contents()->WasHidden();
  RenderProcessHost* new_root_process =
      root->current_frame_host()->GetProcess();
  EXPECT_EQ(new_root_process, popup_process);
  EXPECT_EQ(1, popup_process->VisibleClientCount());
  EXPECT_EQ(0u, popup_process->GetFrameDepth());

  // Go back on main page. Should go back to same state as before navigation.
  TestNavigationObserver back_load_observer(shell()->web_contents());
  shell()->web_contents()->GetController().GoBack();
  back_load_observer.Wait();
  new_shell->web_contents()->WasHidden();
  EXPECT_EQ(1, popup_process->VisibleClientCount());
  EXPECT_EQ(1u, popup_process->GetFrameDepth());

  // Unhide popup. Should go back to same state as before hide.
  new_shell->web_contents()->WasShown();
  EXPECT_EQ(2, popup_process->VisibleClientCount());
  EXPECT_EQ(0u, popup_process->GetFrameDepth());
}

// Check that when a postMessage is called on a remote frame, it waits for the
// current script block to finish executing before forwarding the postMessage,
// so that if the script causes any other IPCs to be sent in the same event
// loop iteration, those IPCs are processed, and their side effects are
// observed by the target frame before it receives the forwarded postMessage.
// See https://crbug.com/828529.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       CrossProcessPostMessageWaitsForCurrentScriptToFinish) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  EXPECT_EQ(root, root->frame_tree().GetFocusedFrame());

  // Add an onmessage handler to the subframe to send back a bool of whether
  // the subframe has focus.
  EXPECT_TRUE(
      ExecJs(root->child_at(0), WaitForMessageScript("document.hasFocus()")));

  // Now, send a postMessage from main frame to subframe, and then focus the
  // subframe in the same script.  postMessage should be scheduled after the
  // focus() call, so the IPC to focus the subframe should arrive before the
  // postMessage IPC, and the subframe should already know that it's focused in
  // the onmessage handler.
  EXPECT_EQ(true, ExecJs(root,
                         "frames[0].postMessage('','*');\n"
                         "frames[0].focus();\n"));
  EXPECT_EQ(true, EvalJs(root->child_at(0), "onMessagePromise"));
}

// Ensure that if a cross-process postMessage is scheduled, and then the target
// frame is detached before the postMessage is forwarded, the source frame's
// renderer does not crash.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       CrossProcessPostMessageAndDetachTarget) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  // Send a postMessage to the subframe and then immediately detach the
  // subframe.
  EXPECT_TRUE(ExecJs(root,
                     "frames[0].postMessage('','*');\n"
                     "document.body.removeChild(\n"
                     "    document.querySelector('iframe'));\n"));

  // Test passes if the main renderer doesn't crash.  Use setTimeout to ensure
  // this ping is evaluated after the (scheduled) postMessage is processed.
  EXPECT_EQ(
      true,
      EvalJs(
          root,
          "new Promise(resolve => setTimeout(() => { resolve(true); }, 0))"));
}

// Tests that the last committed URL is preserved on an RFH even after the RFH
// goes into the pending deletion state.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       LastCommittedURLRetainedAfterUnload) {
  // Navigate to a.com.
  GURL start_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));
  RenderFrameHostImpl* rfh = web_contents()->GetPrimaryMainFrame();
  EXPECT_EQ(start_url, rfh->GetLastCommittedURL());

  // Disable the unload ACK and the unload timer.
  auto unload_ack_filter = base::BindRepeating([] { return true; });
  rfh->SetUnloadACKCallbackForTesting(unload_ack_filter);
  rfh->DisableUnloadTimerForTesting();

  // Open a popup on a.com to keep the process alive.
  OpenPopup(shell(), embedded_test_server()->GetURL("a.com", "/title2.html"),
            "foo");

  // Navigate cross-process to b.com.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title3.html")));

  // The old RFH should be pending deletion.
  EXPECT_TRUE(rfh->IsPendingDeletion());
  EXPECT_FALSE(rfh->IsActive());
  EXPECT_NE(rfh, web_contents()->GetPrimaryMainFrame());

  // Check that it still has a valid last committed URL.
  EXPECT_EQ(start_url, rfh->GetLastCommittedURL());
}

#if BUILDFLAG(IS_ANDROID)

// This test ensures that gestures from child frames notify the gesture manager
// which exists only on the root frame. i.e. the gesture manager knows we're in
// a scroll gesture when it's happening in a cross-process child frame. This is
// important in cases like hiding the text selection popup during a scroll.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       GestureManagerListensToChildFrames) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);
  GURL b_url(embedded_test_server()->GetURL("b.com", "/scrollable_page.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(child, b_url));

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  RenderWidgetHost* rwh = root->current_frame_host()->GetRenderWidgetHost();
  RenderWidgetHost* child_rwh =
      child->current_frame_host()->GetRenderWidgetHost();

  RunUntilInputProcessed(rwh);
  RunUntilInputProcessed(child_rwh);

  RenderWidgetHostViewAndroid* rwhv_root =
      static_cast<RenderWidgetHostViewAndroid*>(
          root->current_frame_host()->GetRenderWidgetHost()->GetView());

  ASSERT_FALSE(
      rwhv_root->gesture_listener_manager_->IsScrollInProgressForTesting());

  // Start a scroll gesture in the child frame, ensure the main frame's gesture
  // listener manager records that its in a scroll.
  {
    blink::WebGestureEvent gesture_scroll_begin(
        blink::WebGestureEvent::Type::kGestureScrollBegin,
        blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests(),
        blink::WebGestureDevice::kTouchscreen);
    gesture_scroll_begin.data.scroll_begin.delta_hint_units =
        ui::ScrollGranularity::kScrollByPrecisePixel;
    gesture_scroll_begin.data.scroll_begin.delta_x_hint = 0.f;
    // Note: Negative y-delta in a gesture event results in scrolling down on a
    // page (i.e. causes positive window.scrollY).
    gesture_scroll_begin.data.scroll_begin.delta_y_hint = -5.f;

    blink::WebMouseEvent mouse_move(
        blink::WebInputEvent::Type::kMouseMove,
        blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests());

    // We wait for the dummy mouse move event since the GestureScrollEnd ACK is
    // used change the gesture manager scrolling state but InputEventAckWaiter
    // is the first-in-line so the state won't yet be changed when it returns.
    // Thus we send a second event and when it's ACK'd we know the first has
    // already been processed (we do the same thing above but with a
    // ScrollUpdate).
    InputEventAckWaiter mouse_move_waiter(
        child_rwh, blink::WebInputEvent::Type::kMouseMove);

    child_rwh->ForwardGestureEvent(gesture_scroll_begin);
    child_rwh->ForwardMouseEvent(mouse_move);
    mouse_move_waiter.Wait();

    EXPECT_TRUE(
        rwhv_root->gesture_listener_manager_->IsScrollInProgressForTesting());
  }

  // Finish the scroll, ensure the gesture manager sees the scroll end.
  {
    blink::WebGestureEvent gesture_scroll_end(
        blink::WebGestureEvent::Type::kGestureScrollEnd,
        blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests(),
        blink::WebGestureDevice::kTouchscreen);

    // See comment above for why this is sent.
    blink::WebMouseEvent mouse_move(
        blink::WebInputEvent::Type::kMouseMove,
        blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests());

    InputEventAckWaiter mouse_move_waiter(
        child_rwh, blink::WebInputEvent::Type::kMouseMove);

    child_rwh->ForwardGestureEvent(gesture_scroll_end);
    child_rwh->ForwardMouseEvent(mouse_move);
    mouse_move_waiter.Wait();

    EXPECT_FALSE(
        rwhv_root->gesture_listener_manager_->IsScrollInProgressForTesting());
  }
}
#endif  // BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, DisplayLockThrottlesOOPIF) {
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  FrameTreeNode* a_frame = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* b_frame = a_frame->child_at(0);

  // Force a lifecycle update in both frames to get to steady state.
  ASSERT_TRUE(EvalJsAfterLifecycleUpdate(a_frame->current_frame_host(), "", "")
                  .error.empty());
  ASSERT_TRUE(EvalJsAfterLifecycleUpdate(b_frame->current_frame_host(), "", "")
                  .error.empty());

  // Display lock an ancestor of the <iframe> element in a_frame. The display
  // lock status will be propagated to the OOPIF during lifecycle update.
  ASSERT_TRUE(EvalJsAfterLifecycleUpdate(
                  a_frame->current_frame_host(),
                  "document.body.style = 'content-visibility: hidden'", "")
                  .error.empty());

  // At this point, a_frame should have already sent an IPC to b_frame causing
  // b_frame to become throttled. Create an IntersectionObserver and observe a
  // visible element in b_frame. The display lock status should cause the
  // visible element to be reported as "not intersecting".
  static const char kObserverScript[] = R"(
      new Promise((resolve, reject) => {
        new IntersectionObserver((entries, observer) => {
          observer.unobserve(entries[0].target);
          resolve(String(entries[0].isIntersecting))
        }).observe(document.getElementById('siteNameHeading'))
      })
  )";
  EvalJsResult result1 = EvalJs(b_frame->current_frame_host(), kObserverScript);
  ASSERT_TRUE(result1.error.empty());
  EXPECT_EQ(result1.ExtractString(), "false");

  // Unlock the element in a_frame, run through the same steps, and look for an
  // "is intersecting" notification.
  ASSERT_TRUE(EvalJsAfterLifecycleUpdate(a_frame->current_frame_host(),
                                         "document.body.style = ''", "")
                  .error.empty());
  EvalJsResult result2 = EvalJs(b_frame->current_frame_host(), kObserverScript);
  ASSERT_EQ(result2.error, "");
  EXPECT_EQ(result2.ExtractString(), "true");
}

namespace {

// Helper class to intercept DidCommitProvisionalLoad messages and inject a
// call to close the current tab right before them.
class ClosePageBeforeCommitHelper : public DidCommitNavigationInterceptor {
 public:
  explicit ClosePageBeforeCommitHelper(WebContents* web_contents)
      : DidCommitNavigationInterceptor(web_contents) {}

  ClosePageBeforeCommitHelper(const ClosePageBeforeCommitHelper&) = delete;
  ClosePageBeforeCommitHelper& operator=(const ClosePageBeforeCommitHelper&) =
      delete;

  void Wait() {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    run_loop_.reset();
  }

 private:
  // DidCommitNavigationInterceptor:
  bool WillProcessDidCommitNavigation(
      RenderFrameHost* render_frame_host,
      NavigationRequest* navigation_request,
      mojom::DidCommitProvisionalLoadParamsPtr* params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr* interface_params)
      override {
    RenderFrameHostImpl* rfh =
        static_cast<RenderFrameHostImpl*>(render_frame_host);
    EXPECT_TRUE(rfh->render_view_host()->is_active());
    rfh->GetMainFrame()->ClosePage(
        RenderFrameHostImpl::ClosePageSource::kBrowser);
    if (run_loop_)
      run_loop_->Quit();
    return true;
  }

  std::unique_ptr<base::RunLoop> run_loop_;
};

}  // namespace

// Verify that when a tab is closed just before a commit IPC arrives for a
// subframe in the tab, a subsequent resource timing IPC from the subframe RFH
// won't generate a renderer kill.  See https://crbug.com/805705.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       CloseTabBeforeSubframeCommits) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  // Open a popup in a.com to keep that process alive.
  GURL same_site_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  Shell* new_shell = OpenPopup(root, same_site_url, "");

  // Add a blank grandchild frame.
  RenderFrameHostCreatedObserver frame_observer(shell()->web_contents(), 1);
  EXPECT_TRUE(
      ExecJs(root->child_at(0),
             "document.body.appendChild(document.createElement('iframe'));"));
  frame_observer.Wait();
  FrameTreeNode* grandchild = root->child_at(0)->child_at(0);

  // Navigate grandchild to an a.com URL.  Note that only a frame's initial
  // navigation forwards resource timing info to parent, so it's important that
  // this iframe was initially blank.
  //
  // Just before this URL commits, close the page.
  ClosePageBeforeCommitHelper close_page_helper(web_contents());
  EXPECT_TRUE(ExecJs(grandchild, JsReplace("location = $1", same_site_url)));
  close_page_helper.Wait();

  // Test passes if the a.com renderer doesn't crash. Ping to verify.
  EXPECT_EQ(true, EvalJs(new_shell, "true;"));
}

class SitePerProcessBrowserTouchActionTest : public SitePerProcessBrowserTest {
 public:
  SitePerProcessBrowserTouchActionTest() = default;

  bool GetTouchActionForceEnableZoom(RenderWidgetHost* rwh) {
    input::InputRouterImpl* input_router = static_cast<input::InputRouterImpl*>(
        static_cast<RenderWidgetHostImpl*>(rwh)->input_router());
    return input_router->touch_action_filter_.force_enable_zoom_;
  }

  // Computes the effective and allowed touch action for |rwhv_child| by
  // dispatching a touch to it through |rwhv_root|. |rwhv_root| is the root
  // frame containing |rwhv_child|. |rwhv_child| is the child (or indirect
  // descendent) of |rwhv_root| to get the touch action of. |event_position|
  // should be within |rwhv_child| in |rwhv_root|'s coordinate space.
  void GetTouchActionsForChild(
      input::RenderWidgetHostInputEventRouter* router,
      RenderWidgetHostViewBase* rwhv_root,
      RenderWidgetHostViewBase* rwhv_child,
      const gfx::Point& event_position,
      std::optional<cc::TouchAction>& effective_touch_action,
      std::optional<cc::TouchAction>& allowed_touch_action) {
    InputEventAckWaiter ack_observer(
        rwhv_child->GetRenderWidgetHost(),
        base::BindRepeating([](blink::mojom::InputEventResultSource source,
                               blink::mojom::InputEventResultState state,
                               const blink::WebInputEvent& event) {
          return event.GetType() == blink::WebGestureEvent::Type::kTouchStart ||
                 event.GetType() == blink::WebGestureEvent::Type::kTouchMove ||
                 event.GetType() == blink::WebGestureEvent::Type::kTouchEnd;
        }));

    input::InputRouterImpl* input_router = static_cast<input::InputRouterImpl*>(
        static_cast<RenderWidgetHostImpl*>(rwhv_child->GetRenderWidgetHost())
            ->input_router());
    // Clear the touch actions that were set by previous touches.
    input_router->touch_action_filter_.allowed_touch_action_.reset();
    // Send a touch start event to child to get the TAF filled with child
    // frame's touch action.
    ack_observer.Reset();
    blink::SyntheticWebTouchEvent touch_event;
    int index = touch_event.PressPoint(event_position.x(), event_position.y());
    router->RouteTouchEvent(rwhv_root, &touch_event, ui::LatencyInfo());
    ack_observer.Wait();
    // Reset them to get the new value.
    effective_touch_action.reset();
    allowed_touch_action.reset();
    effective_touch_action =
        input_router->touch_action_filter_.allowed_touch_action_;
    // Effective touch action are sent from a separate IPC
    // channel, so it is not guaranteed to have value when the ACK for the
    // touch start arrived because the ACK is from the main thread.
    allowed_touch_action =
        input_router->touch_action_filter_.compositor_allowed_touch_action_;

    // Send a touch move and touch end to complete the sequence, this also
    // avoids triggering DCHECKs when sending followup events.
    ack_observer.Reset();
    touch_event.MovePoint(index, 1, 1);
    router->RouteTouchEvent(rwhv_root, &touch_event, ui::LatencyInfo());
    ack_observer.Wait();

    ack_observer.Reset();
    touch_event.ReleasePoint(index);
    router->RouteTouchEvent(rwhv_root, &touch_event, ui::LatencyInfo());
    ack_observer.Wait();
  }

  void GiveItSomeTime(const base::TimeDelta& t) {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), t);
    run_loop.Run();
  }

  // Waits until the parent frame has had enough time to propagate the effective
  // touch action to the child frame and the child frame has had enough time to
  // process it.
  void WaitForTouchActionUpdated(
      MainThreadFrameObserver* root_thread_observer,
      MainThreadFrameObserver* child_thread_observer) {
    // Sends an event to the root frame's renderer main thread, upon return the
    // root frame should have calculated the new effective touch action for the
    // child frame.
    root_thread_observer->Wait();
    // Sends an event to the child frame's renderer main thread, upon return the
    // child frame should have received the effective touch action from parent
    // and propagated it.
    child_thread_observer->Wait();
    // The child's handling of the touch action may lead to further propagation
    // back to the parent. This sends an event to the root frame's renderer main
    // thread, upon return it should have handled any touch action update.
    root_thread_observer->Wait();
  }
};

#if BUILDFLAG(IS_ANDROID)
// Class to set |force_enable_zoom| to true in WebkitPrefs.
class EnableForceZoomContentClient
    : public ContentBrowserTestContentBrowserClient {
 public:
  EnableForceZoomContentClient() = default;

  EnableForceZoomContentClient(const EnableForceZoomContentClient&) = delete;
  EnableForceZoomContentClient& operator=(const EnableForceZoomContentClient&) =
      delete;

  void OverrideWebkitPrefs(WebContents* web_contents,
                           blink::web_pref::WebPreferences* prefs) override {
    prefs->force_enable_zoom = true;
  }
};

IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTouchActionTest,
                       ForceEnableZoomPropagatesToChild) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1U, root->child_count());
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  FrameTreeNode* child = root->child_at(0);
  EXPECT_TRUE(NavigateToURLFromRenderer(child, b_url));
  WaitForHitTestData(child->current_frame_host());

  // Get access to child's TouchActionFilter.
  RenderWidgetHost* child_rwh =
      child->current_frame_host()->GetRenderWidgetHost();
  EXPECT_FALSE(GetTouchActionForceEnableZoom(child_rwh));

  EnableForceZoomContentClient new_client;

  web_contents()->OnWebPreferencesChanged();

  EXPECT_TRUE(GetTouchActionForceEnableZoom(child_rwh));

  // Add a new oopif child frame, and make sure it initializes with the correct
  // value of ForceEnableZoom.
  GURL c_url = embedded_test_server()->GetURL("c.com", "/title1.html");
  std::string create_frame_script = base::StringPrintf(
      "var new_iframe = document.createElement('iframe');"
      "new_iframe.src = '%s';"
      "document.body.appendChild(new_iframe);",
      c_url.spec().c_str());
  EXPECT_TRUE(ExecJs(root, create_frame_script));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  ASSERT_EQ(2U, root->child_count());

  FrameTreeNode* new_child = root->child_at(1);
  EXPECT_NE(root->current_frame_host()->GetRenderWidgetHost(),
            new_child->current_frame_host()->GetRenderWidgetHost());
  EXPECT_TRUE(GetTouchActionForceEnableZoom(
      new_child->current_frame_host()->GetRenderWidgetHost()));
}

IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTouchActionTest,
                       CheckForceEnableZoomValue) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("foo.com", "/title1.html")));
  EXPECT_FALSE(GetTouchActionForceEnableZoom(
      web_contents()->GetPrimaryMainFrame()->GetRenderViewHost()->GetWidget()));

  EnableForceZoomContentClient new_client;

  web_contents()->OnWebPreferencesChanged();

  EXPECT_TRUE(GetTouchActionForceEnableZoom(
      web_contents()->GetPrimaryMainFrame()->GetRenderViewHost()->GetWidget()));

  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("bar.com", "/title2.html")));

  EXPECT_TRUE(GetTouchActionForceEnableZoom(
      web_contents()->GetPrimaryMainFrame()->GetRenderViewHost()->GetWidget()));
}

#endif  // BUILDFLAG(IS_ANDROID)

// Flaky on every platform, failing most of the time on Android.
// See https://crbug.com/945734
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTouchActionTest,
                       DISABLED_EffectiveTouchActionPropagatesAcrossFrames) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);
  RenderWidgetHostViewBase* rwhv_root = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostViewBase* rwhv_child = static_cast<RenderWidgetHostViewBase*>(
      child->current_frame_host()->GetRenderWidgetHost()->GetView());
  std::unique_ptr<MainThreadFrameObserver> root_thread_observer(
      new MainThreadFrameObserver(
          root->current_frame_host()->GetRenderWidgetHost()));
  root_thread_observer->Wait();

  GURL b_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(child, b_url));

  // Force the renderer to generate a new frame.
  EXPECT_TRUE(ExecJs(shell(), "document.body.style.touchAction = 'none'"));
  // Waits for the next frame.
  WaitForHitTestData(child->current_frame_host());
  std::unique_ptr<MainThreadFrameObserver> child_thread_observer(
      new MainThreadFrameObserver(
          child->current_frame_host()->GetRenderWidgetHost()));

  RenderWidgetHostViewChildFrame* child_view =
      static_cast<RenderWidgetHostViewChildFrame*>(
          child->current_frame_host()->GetRenderWidgetHost()->GetView());
  gfx::Point point_inside_child = ToFlooredPoint(
      child_view->TransformPointToRootCoordSpaceF(gfx::PointF(+5.f, +5.f)));

  input::RenderWidgetHostInputEventRouter* router =
      static_cast<WebContentsImpl*>(web_contents())->GetInputEventRouter();

  WaitForTouchActionUpdated(root_thread_observer.get(),
                            child_thread_observer.get());
  std::optional<cc::TouchAction> effective_touch_action;
  std::optional<cc::TouchAction> allowed_touch_action;
  cc::TouchAction expected_touch_action = cc::TouchAction::kPan;
  // Gestures are filtered by the intersection of touch-action values of the
  // touched element and all its ancestors up to the one that implements the
  // gesture. Since iframe allows scrolling, touch action pan restrictions will
  // not affect iframe's descendants, so we expect TouchAction::kPan instead of
  // TouchAction::kAuto in iframe's child.
  GetTouchActionsForChild(router, rwhv_root, rwhv_child, point_inside_child,
                          effective_touch_action, allowed_touch_action);
  if (allowed_touch_action.has_value())
    EXPECT_EQ(expected_touch_action, allowed_touch_action.value());

  EXPECT_TRUE(ExecJs(shell(), "document.body.style.touchAction = 'auto'"));
  WaitForTouchActionUpdated(root_thread_observer.get(),
                            child_thread_observer.get());
  expected_touch_action = cc::TouchAction::kAuto;
  GetTouchActionsForChild(router, rwhv_root, rwhv_child, point_inside_child,
                          effective_touch_action, allowed_touch_action);
  EXPECT_EQ(expected_touch_action, effective_touch_action.has_value()
                                       ? effective_touch_action.value()
                                       : cc::TouchAction::kAuto);
  if (allowed_touch_action.has_value())
    EXPECT_EQ(expected_touch_action, allowed_touch_action.value());
}

// Flaky on all platform. http://crbug.com/9515270
IN_PROC_BROWSER_TEST_F(
    SitePerProcessBrowserTouchActionTest,
    DISABLED_EffectiveTouchActionPropagatesAcrossNestedFrames) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* parent = root->child_at(0);
  GURL b_url(embedded_test_server()->GetURL(
      "b.com", "/frame_tree/page_with_iframe_in_div.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(parent, b_url));

  ASSERT_EQ(1U, parent->child_count());
  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   +--Site B ------- proxies for A C\n"
      "        +--Site C -- proxies for A B\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/\n"
      "      C = http://bar.com/",
      DepictFrameTree(root));

  FrameTreeNode* child = root->child_at(0)->child_at(0);
  RenderWidgetHostViewBase* rwhv_root = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostViewBase* rwhv_child = static_cast<RenderWidgetHostViewBase*>(
      child->current_frame_host()->GetRenderWidgetHost()->GetView());
  std::unique_ptr<MainThreadFrameObserver> root_thread_observer(
      new MainThreadFrameObserver(
          root->current_frame_host()->GetRenderWidgetHost()));
  root_thread_observer->Wait();

  EXPECT_TRUE(ExecJs(shell(), "document.body.style.touchAction = 'none'"));

  // Wait for child frame ready in order to get the correct point inside child.
  WaitForHitTestData(child->current_frame_host());
  std::unique_ptr<MainThreadFrameObserver> child_thread_observer(
      new MainThreadFrameObserver(
          child->current_frame_host()->GetRenderWidgetHost()));
  RenderWidgetHostViewChildFrame* child_view =
      static_cast<RenderWidgetHostViewChildFrame*>(
          child->current_frame_host()->GetRenderWidgetHost()->GetView());
  gfx::Point point_inside_child = ToFlooredPoint(
      child_view->TransformPointToRootCoordSpaceF(gfx::PointF(+5.f, +5.f)));

  input::RenderWidgetHostInputEventRouter* router =
      static_cast<WebContentsImpl*>(web_contents())->GetInputEventRouter();

  // Child should inherit effective touch action none from root.
  WaitForTouchActionUpdated(root_thread_observer.get(),
                            child_thread_observer.get());
  std::optional<cc::TouchAction> effective_touch_action;
  std::optional<cc::TouchAction> allowed_touch_action;
  cc::TouchAction expected_touch_action = cc::TouchAction::kPan;
  GetTouchActionsForChild(router, rwhv_root, rwhv_child, point_inside_child,
                          effective_touch_action, allowed_touch_action);
  if (allowed_touch_action.has_value())
    EXPECT_EQ(expected_touch_action, allowed_touch_action.value());

  // Child should inherit effective touch action none from parent.
  EXPECT_TRUE(ExecJs(shell(), "document.body.style.touchAction = 'auto'"));
  EXPECT_TRUE(ExecJs(
      parent,
      "document.getElementById('parent-div').style.touchAction = 'none';"));
  WaitForTouchActionUpdated(root_thread_observer.get(),
                            child_thread_observer.get());
  GetTouchActionsForChild(router, rwhv_root, rwhv_child, point_inside_child,
                          effective_touch_action, allowed_touch_action);
  if (allowed_touch_action.has_value())
    EXPECT_EQ(expected_touch_action, allowed_touch_action.value());

  // Child should inherit effective touch action auto from root and parent.
  EXPECT_TRUE(ExecJs(
      parent,
      "document.getElementById('parent-div').style.touchAction = 'auto'"));
  WaitForTouchActionUpdated(root_thread_observer.get(),
                            child_thread_observer.get());
  expected_touch_action = cc::TouchAction::kAuto;
  GetTouchActionsForChild(router, rwhv_root, rwhv_child, point_inside_child,
                          effective_touch_action, allowed_touch_action);
  if (allowed_touch_action.has_value())
    EXPECT_EQ(expected_touch_action, allowed_touch_action.value());
}

IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTouchActionTest,
                       EffectiveTouchActionPropagatesWhenChildFrameNavigates) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(child, b_url));

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  RenderWidgetHostViewBase* rwhv_root = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostViewBase* rwhv_child = static_cast<RenderWidgetHostViewBase*>(
      child->current_frame_host()->GetRenderWidgetHost()->GetView());
  std::unique_ptr<MainThreadFrameObserver> root_thread_observer(
      new MainThreadFrameObserver(
          root->current_frame_host()->GetRenderWidgetHost()));
  root_thread_observer->Wait();

  EXPECT_TRUE(ExecJs(shell(), "document.body.style.touchAction = 'none'"));

  // Wait for child frame ready in order to get the correct point inside child.
  WaitForHitTestData(child->current_frame_host());
  std::unique_ptr<MainThreadFrameObserver> child_thread_observer(
      new MainThreadFrameObserver(
          child->current_frame_host()->GetRenderWidgetHost()));
  RenderWidgetHostViewChildFrame* child_view =
      static_cast<RenderWidgetHostViewChildFrame*>(
          child->current_frame_host()->GetRenderWidgetHost()->GetView());
  gfx::Point point_inside_child = gfx::ToFlooredPoint(
      child_view->TransformPointToRootCoordSpaceF(gfx::PointF(+5.f, +5.f)));

  input::RenderWidgetHostInputEventRouter* router =
      static_cast<WebContentsImpl*>(web_contents())->GetInputEventRouter();
  // Child should inherit effective touch action none from root.
  WaitForTouchActionUpdated(root_thread_observer.get(),
                            child_thread_observer.get());
  std::optional<cc::TouchAction> effective_touch_action;
  std::optional<cc::TouchAction> allowed_touch_action;
  cc::TouchAction expected_touch_action =
      cc::TouchAction::kPan | cc::TouchAction::kInternalPanXScrolls |
      cc::TouchAction::kInternalNotWritable;
  GetTouchActionsForChild(router, rwhv_root, rwhv_child, point_inside_child,
                          effective_touch_action, allowed_touch_action);
  if (allowed_touch_action.has_value())
    EXPECT_EQ(expected_touch_action, allowed_touch_action.value());

  // After navigation, child should still inherit effective touch action none
  // from parent.
  GURL new_url(embedded_test_server()->GetURL("c.com", "/title2.html"));
  // Reset before navigation, as navigation destroys the underlying
  // RenderWidgetHost being observed.
  child_thread_observer.reset();
  EXPECT_TRUE(NavigateToURLFromRenderer(child, new_url));
  WaitForHitTestData(child->current_frame_host());
  // Navigation destroys the previous RenderWidgetHost, so we need to begin
  // observing the new renderer main thread associated with the child frame.
  child_thread_observer = std::make_unique<MainThreadFrameObserver>(
      child->current_frame_host()->GetRenderWidgetHost());

  rwhv_child = static_cast<RenderWidgetHostViewBase*>(
      child->current_frame_host()->GetRenderWidgetHost()->GetView());

  WaitForTouchActionUpdated(root_thread_observer.get(),
                            child_thread_observer.get());
  GetTouchActionsForChild(router, rwhv_root, rwhv_child, point_inside_child,
                          effective_touch_action, allowed_touch_action);
  if (allowed_touch_action.has_value())
    EXPECT_EQ(expected_touch_action, allowed_touch_action.value());
}

IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       ChildFrameCrashMetrics_KilledMainFrame) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a(b(b,c)))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  // Kill the main frame.
  base::HistogramTester histograms;
  RenderProcessHost* child_process = root->current_frame_host()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      child_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  child_process->Shutdown(0);
  crash_observer.Wait();

  // Verify that no child frame metrics got logged.
  histograms.ExpectTotalCount("Stability.ChildFrameCrash.Visibility", 0);
}

IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       ChildFrameCrashMetrics_NeverShown) {
  // Set-up a frame tree that helps verify what the metrics tracks:
  // 1) frames (12 frames are affected if B process gets killed) or
  // 2) widgets (10 b widgets and 1 c widget are affected if B is killed) or
  // 3) crashes (1 crash if B process gets killed)?
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(b,c),b,b,b,b,b,b,b,b,b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  // Hide the web contents (UpdateWebContentsVisibility is called twice to avoid
  // hitting the |!did_first_set_visible_| case).
  web_contents()->UpdateWebContentsVisibility(Visibility::VISIBLE);
  web_contents()->UpdateWebContentsVisibility(Visibility::HIDDEN);

  // Kill the subframe.
  base::HistogramTester histograms;
  RenderProcessHost* child_process =
      root->child_at(0)->current_frame_host()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      child_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  child_process->Shutdown(0);
  crash_observer.Wait();

  // Navigate away - this will trigger logging of the UMA.
  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));

  // Wait until the page with the crashed frame gets unloaded (triggering its
  // evicton if it got into the back/forward cache), so that the histogram will
  // be recorded when the renderer process is gone.
  // TODO(crbug.com/40175240): Ensure pages with crashed subframes won't
  // get into back/forward cache.
  InactiveRenderFrameHostDeletionObserver inactive_rfh_deletion_observer(
      web_contents());
  inactive_rfh_deletion_observer.Wait();

  histograms.ExpectUniqueSample("Stability.ChildFrameCrash.Visibility",
                                CrashVisibility::kNeverVisibleAfterCrash, 10);
}

IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       ChildFrameCrashMetrics_ScrolledIntoView) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  // Fill the main frame so that the subframe is pushed below the fold (is
  // scrolled outside of the current view) and wait until the main frame redraws
  // itself (i.e. making sure CPFC::OnUpdateViewportIntersection has arrived).
  std::string filling_script = R"(
    var frame = document.body.querySelectorAll("iframe")[0];
    for (var i = 0; i < 100; i++) {
      var p = document.createElement("p");
      p.innerText = "blah";
      document.body.insertBefore(p, frame);
    }
  )";
  EXPECT_TRUE(ExecJs(root, filling_script));
  // This will ensure that browser has received the
  // FrameHostMsg_UpdateViewportIntersection IPC message from the renderer main
  // thread.
  EXPECT_EQ(true,
            EvalJsAfterLifecycleUpdate(root->current_frame_host(), "", "true"));

  // Kill the child frame.
  base::HistogramTester histograms;
  RenderProcessHost* child_process =
      root->child_at(0)->current_frame_host()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      child_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  child_process->Shutdown(0);
  crash_observer.Wait();

  // Verify that no child frame metrics got logged (yet - while the subframe is
  // below the fold / is not scrolled into view).
  histograms.ExpectTotalCount("Stability.ChildFrameCrash.Visibility", 0);
  histograms.ExpectTotalCount(
      "Stability.ChildFrameCrash.ShownAfterCrashingReason", 0);

  // Scroll the subframe into view and wait until the scrolled frame draws
  // itself.
  std::string scrolling_script = R"(
    var frame = document.body.querySelectorAll("iframe")[0];
    frame.scrollIntoView();
  )";
  EXPECT_TRUE(ExecJs(root, scrolling_script));
  // Wait for FrameHostMsg_UpdateViewportIntersection again.
  EXPECT_EQ(true,
            EvalJsAfterLifecycleUpdate(root->current_frame_host(), "", "true"));

  // Verify that the expected metrics got logged.
  histograms.ExpectUniqueSample(
      "Stability.ChildFrameCrash.Visibility",
      CrossProcessFrameConnector::CrashVisibility::kShownAfterCrashing, 1);
  histograms.ExpectUniqueSample(
      "Stability.ChildFrameCrash.ShownAfterCrashingReason",
      CrossProcessFrameConnector::ShownAfterCrashingReason::
          kViewportIntersection,
      1);
}

class SitePerProcessAndProcessPerSiteBrowserTest
    : public SitePerProcessBrowserTest {
 public:
  SitePerProcessAndProcessPerSiteBrowserTest() {}

  SitePerProcessAndProcessPerSiteBrowserTest(
      const SitePerProcessAndProcessPerSiteBrowserTest&) = delete;
  SitePerProcessAndProcessPerSiteBrowserTest& operator=(
      const SitePerProcessAndProcessPerSiteBrowserTest&) = delete;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SitePerProcessBrowserTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kProcessPerSite);
  }
};

// Verify that when --site-per-process is combined with --process-per-site, a
// cross-site, browser-initiated navigation with a generated page transition
// does not stay in the old SiteInstance.  See https://crbug.com/825411.
IN_PROC_BROWSER_TEST_P(SitePerProcessAndProcessPerSiteBrowserTest,
                       GeneratedTransitionsSwapProcesses) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("foo.com", "/title1.html")));
  scoped_refptr<SiteInstance> foo_site_instance(
      web_contents()->GetSiteInstance());

  // Navigate cross-site via a generated transition.  This would normally
  // happen for search queries.
  TestNavigationObserver observer(web_contents());
  NavigationController::LoadURLParams params(
      embedded_test_server()->GetURL("bar.com", "/title2.html"));
  params.transition_type = ui::PAGE_TRANSITION_GENERATED;
  web_contents()->GetController().LoadURLWithParams(params);
  observer.Wait();

  // Ensure the original SiteInstance wasn't reused.
  EXPECT_NE(foo_site_instance, web_contents()->GetSiteInstance());

  // Ensure the new page can access cookies without getting killed.
  EXPECT_TRUE(ExecJs(web_contents(), "document.cookie = 'foo=bar';"));
  EXPECT_EQ("foo=bar", EvalJs(web_contents(), "document.cookie;"));
}

namespace {

// Helper for waiting until next same-document navigation commits in
// |web_contents|.
class SameDocumentCommitObserver : public WebContentsObserver {
 public:
  explicit SameDocumentCommitObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {
    EXPECT_TRUE(web_contents);
  }

  SameDocumentCommitObserver(const SameDocumentCommitObserver&) = delete;
  SameDocumentCommitObserver& operator=(const SameDocumentCommitObserver&) =
      delete;

  void Wait() { run_loop_.Run(); }

  const GURL& last_committed_url() { return last_committed_url_; }

 private:
  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    if (navigation_handle->IsSameDocument()) {
      last_committed_url_ = navigation_handle->GetURL();
      run_loop_.Quit();
    }
  }

  GURL last_committed_url_;
  base::RunLoop run_loop_;
};

}  // namespace

// Ensure that a same-document navigation does not cancel an ongoing
// cross-process navigation.  See https://crbug.com/825677.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       ReplaceStateDoesNotCancelCrossSiteNavigation) {
  GURL url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  // Give the page a beforeunload handler that does a replaceState.  Do this
  // from setTimeout so that the navigation that triggers beforeunload is
  // already started when the replaceState happens.
  EXPECT_TRUE(ExecJs(root,
                     "window.onbeforeunload = function (e) {"
                     "  setTimeout(() => {"
                     "    history.replaceState({}, 'footitle', 'foo');"
                     "  }, 0);"
                     "};\n"));

  GURL url2 = embedded_test_server()->GetURL("b.com", "/title1.html");
  TestNavigationManager cross_site_navigation(web_contents(), url2);
  SameDocumentCommitObserver replace_state_observer(web_contents());

  // Start a cross-site navigation.  Using a renderer-initiated navigation
  // rather than a browser-initiated one is important here, since
  // https://crbug.com/825677 was triggered only when replaceState ran while
  // having a user gesture, which will be the case here since ExecJs
  // runs with a user gesture.
  EXPECT_TRUE(ExecJs(root, JsReplace("location.href = $1", url2)));
  EXPECT_TRUE(cross_site_navigation.WaitForRequestStart());

  // Now wait for the replaceState to commit while the cross-process navigation
  // is paused.
  replace_state_observer.Wait();
  GURL replace_state_url = embedded_test_server()->GetURL("a.com", "/foo");
  EXPECT_EQ(replace_state_url, replace_state_observer.last_committed_url());

  // The cross-process navigation should not be canceled after the
  // replaceState.
  ASSERT_TRUE(root->IsLoading());
  ASSERT_TRUE(root->navigation_request());

  // Resume and finish the cross-process navigation.
  cross_site_navigation.ResumeNavigation();
  ASSERT_TRUE(cross_site_navigation.WaitForNavigationFinished());
  EXPECT_TRUE(cross_site_navigation.was_successful());
  EXPECT_EQ(url2, web_contents()->GetLastCommittedURL());
}

// Test that a pending frame policy, such as an updated sandbox attribute, does
// not take effect after a same-document navigation.  See
// https://crbug.com/849311.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       SameDocumentNavigationDoesNotCommitPendingFramePolicy) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* subframe = root->child_at(0);

  // The subframe should not be sandboxed.
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            subframe->pending_frame_policy().sandbox_flags);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            subframe->effective_frame_policy().sandbox_flags);

  // Set the "sandbox" attribute on the subframe; pending policy should update.
  EXPECT_TRUE(ExecJs(
      root, "document.querySelector('iframe').sandbox = 'allow-scripts';"));
  // "allow-scripts" resets both SandboxFlags::Scripts and
  // SandboxFlags::AutomaticFeatures bits per blink::ParseSandboxPolicy().
  network::mojom::WebSandboxFlags expected_flags =
      network::mojom::WebSandboxFlags::kAll &
      ~network::mojom::WebSandboxFlags::kScripts &
      ~network::mojom::WebSandboxFlags::kAutomaticFeatures;
  EXPECT_EQ(expected_flags, subframe->pending_frame_policy().sandbox_flags);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            subframe->effective_frame_policy().sandbox_flags);

  // Commit a same-document navigation with replaceState.  The new sandbox
  // flags should still be pending but not effective.
  SameDocumentCommitObserver replace_state_observer(web_contents());
  EXPECT_TRUE(ExecJs(subframe, "history.replaceState({}, 'footitle', 'foo');"));
  replace_state_observer.Wait();

  EXPECT_EQ(expected_flags, subframe->pending_frame_policy().sandbox_flags);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            subframe->effective_frame_policy().sandbox_flags);

  // Also try a same-document navigation to a fragment, which also shouldn't
  // commit the pending sandbox flags.
  GURL fragment_url = GURL(subframe->current_url().spec() + "#foo");
  {
    SameDocumentCommitObserver fragment_observer(web_contents());
    EXPECT_TRUE(ExecJs(subframe, JsReplace("location.href=$1", fragment_url)));
    fragment_observer.Wait();
    EXPECT_EQ(fragment_url, subframe->current_url());
  }

  EXPECT_EQ(expected_flags, subframe->pending_frame_policy().sandbox_flags);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            subframe->effective_frame_policy().sandbox_flags);
}

// Ensure that when two cross-site frames have subframes with unique origins,
// and those subframes create blob URLs and navigate to them, the blob URLs end
// up in different processes. See https://crbug.com/863623.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       TwoBlobURLsWithNullOriginDontShareProcess) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/navigation_controller/page_with_data_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* subframe = root->child_at(0);

  // Create a blob URL in the subframe, and navigate to it.
  TestNavigationObserver observer(shell()->web_contents());
  std::string blob_script =
      "var blob = new Blob(['foo'], {type : 'text/html'});"
      "var url = URL.createObjectURL(blob);"
      "location = url;";
  EXPECT_TRUE(ExecJs(subframe, blob_script));
  observer.Wait();
  RenderFrameHostImpl* subframe_rfh = subframe->current_frame_host();
  EXPECT_TRUE(subframe_rfh->GetLastCommittedURL().SchemeIsBlob());

  // Open a cross-site popup and repeat these steps.
  GURL popup_url(embedded_test_server()->GetURL(
      "b.com", "/navigation_controller/page_with_data_iframe.html"));
  Shell* new_shell = OpenPopup(root, popup_url, "");
  FrameTreeNode* popup_root =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetPrimaryFrameTree()
          .root();
  FrameTreeNode* popup_subframe = popup_root->child_at(0);

  TestNavigationObserver popup_observer(new_shell->web_contents());
  EXPECT_TRUE(ExecJs(popup_subframe, blob_script));
  popup_observer.Wait();
  RenderFrameHostImpl* popup_subframe_rfh =
      popup_subframe->current_frame_host();
  EXPECT_TRUE(popup_subframe_rfh->GetLastCommittedURL().SchemeIsBlob());

  // Ensure that the two blob subframes don't share a process or SiteInstance.
  EXPECT_NE(subframe->current_frame_host()->GetSiteInstance(),
            popup_subframe->current_frame_host()->GetSiteInstance());
  EXPECT_NE(
      subframe->current_frame_host()->GetSiteInstance()->GetProcess(),
      popup_subframe->current_frame_host()->GetSiteInstance()->GetProcess());
  EXPECT_NE(
      subframe->current_frame_host()->GetSiteInstance()->GetSiteURL(),
      popup_subframe->current_frame_host()->GetSiteInstance()->GetSiteURL());
}

// Ensure that when a process is about to be destroyed after the last active
// frame in it goes away, an attempt to reuse a proxy in that process doesn't
// result in a crash.  See https://crbug.com/794625.
// TODO(crbug.com/42050611): This is flaky on Fuchsia because the
// MessagePort is not cleared on the other side, resulting in Zircon killing the
// process. See the comment referencing the same bug in
// //mojo/core/channel_fuchsia.cc
#if BUILDFLAG(IS_FUCHSIA)
#define MAYBE_RenderFrameProxyNotRecreatedDuringProcessShutdown \
  DISABLED_RenderFrameProxyNotRecreatedDuringProcessShutdown
#else
#define MAYBE_RenderFrameProxyNotRecreatedDuringProcessShutdown \
  RenderFrameProxyNotRecreatedDuringProcessShutdown
#endif
IN_PROC_BROWSER_TEST_P(
    SitePerProcessBrowserTest,
    MAYBE_RenderFrameProxyNotRecreatedDuringProcessShutdown) {
  DisableBackForwardCacheForTesting(
      web_contents(), content::BackForwardCache::TEST_REQUIRES_NO_CACHING);
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  GURL popup_url(embedded_test_server()->GetURL(
      "b.com", "/title1.html"));
  Shell* new_shell = OpenPopup(root, popup_url, "foo");
  FrameTreeNode* popup_root =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetPrimaryFrameTree()
          .root();
  auto* rfh = popup_root->current_frame_host();

  // Disable the unload timer to prevent flakiness.
  rfh->DisableUnloadTimerForTesting();

  // This will be used to monitor that b.com process exits cleanly.
  RenderProcessHostWatcher b_process_observer(
      popup_root->current_frame_host()->GetProcess(),
      RenderProcessHostWatcher::WATCH_FOR_HOST_DESTRUCTION);

  // In the first tab, install a postMessage handler to navigate the popup to a
  // hung b.com URL once the first message is received.
  GURL hung_b_url(embedded_test_server()->GetURL("b.com", "/hung"));
  TestNavigationManager manager(new_shell->web_contents(), hung_b_url);
  EXPECT_TRUE(ExecJs(shell(), JsReplace(R"(
      window.done = false;
      window.onmessage = () => {
        if (!window.done) {
          window.open($1, 'foo');
          window.done = true;
        }
      };)",
                                        hung_b_url)));

  // In the popup, install a pagehide handler to send a lot of postMessages to
  // the opener.  This keeps the MessageLoop in the b.com process busy after
  // navigating away from the current document.  In https://crbug.com/794625,
  // this was needed so that a subsequent IPC to recreate a proxy arrives
  // before the process fully shuts down.
  EXPECT_TRUE(ExecJs(new_shell, R"(
      window.onpagehide = () => {
        for (var i=0; i<10000; i++)
          opener.postMessage('hi','*');
      })"));

  // Navigate popup to a.com.  This unloads the last active frame in the b.com
  // process, and hence initiates process shutdown.
  TestFrameNavigationObserver commit_observer(popup_root);
  GURL another_a_url(embedded_test_server()->GetURL("a.com", "/title3.html"));
  EXPECT_TRUE(ExecJs(new_shell, JsReplace("location = $1", another_a_url)));
  commit_observer.WaitForCommit();

  // At this point, popup's original RFH is pending deletion.
  EXPECT_TRUE(rfh->IsPendingDeletion());

  // When the opener receives a postMessage from the popup's pagehide handler,
  // it should start a navigation back to b.com.  Wait for it.  This navigation
  // creates a speculative RFH which reuses the proxy that was created as part
  // of navigating from |popup_url| to |another_a_url|.
  EXPECT_TRUE(manager.WaitForRequestStart());

  // Cancel the started navigation (to /hung) in the popup and make sure the
  // b.com renderer process exits cleanly without a crash.  In
  // https://crbug.com/794625, the crash was caused by trying to recreate the
  // reused proxy, which had been incorrectly set as non-live.
  popup_root->ResetNavigationRequest(
      NavigationDiscardReason::kExplicitCancellation);
  b_process_observer.Wait();
  EXPECT_TRUE(b_process_observer.did_exit_normally());
}

IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       CommitTimeoutForHungRenderer) {
  // Navigate first tab to a.com.
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), a_url));
  RenderProcessHost* a_process =
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess();

  // Open b.com in a second tab.  Using a renderer-initiated navigation is
  // important to leave a.com and b.com SiteInstances in the same
  // BrowsingInstance (so the b.com -> a.com navigation in the next test step
  // will reuse the process associated with the first a.com tab).
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  Shell* new_shell = OpenPopup(shell()->web_contents(), b_url, "newtab");
  WebContents* new_contents = new_shell->web_contents();
  EXPECT_TRUE(WaitForLoadStop(new_contents));
  RenderProcessHost* b_process =
      new_contents->GetPrimaryMainFrame()->GetProcess();
  EXPECT_NE(a_process, b_process);

  // Hang the first tab's renderer.
  const char* kHungScript = "setTimeout(function() { for (;;) {}; }, 0);";
  EXPECT_TRUE(ExecJs(shell()->web_contents(), kHungScript));

  // Attempt to navigate the second tab to a.com.  This will attempt to reuse
  // the hung process.
  NavigationRequest::SetCommitTimeoutForTesting(base::Milliseconds(100));
  GURL hung_url(embedded_test_server()->GetURL("a.com", "/title3.html"));
  UnresponsiveRendererObserver unresponsive_renderer_observer(new_contents);
  EXPECT_TRUE(
      ExecJs(new_contents, JsReplace("window.location = $1", hung_url)));

  // Verify that we will be notified about the unresponsive renderer.  Before
  // changes in https://crrev.com/c/1089797, the test would hang here forever.
  RenderProcessHost* hung_process = unresponsive_renderer_observer.Wait();
  EXPECT_EQ(hung_process, a_process);

  // Reset the timeout.
  NavigationRequest::SetCommitTimeoutForTesting(base::TimeDelta());
}

// This is a regression test for https://crbug.com/881812 which complained that
// the hung renderer dialog used to undesirably show up for background tabs
// (typically during session restore when many navigations would be happening in
// backgrounded processes).
// TODO(crbug.com/40196588): Flaky on LaCrOS, Mac, and Windows.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_NoCommitTimeoutForInvisibleWebContents \
  DISABLED_NoCommitTimeoutForInvisibleWebContents
#else
#define MAYBE_NoCommitTimeoutForInvisibleWebContents \
  NoCommitTimeoutForInvisibleWebContents
#endif
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       MAYBE_NoCommitTimeoutForInvisibleWebContents) {
  // Navigate first tab to a.com.
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), a_url));
  RenderProcessHost* a_process =
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess();

  // Open b.com in a second tab.  Using a renderer-initiated navigation is
  // important to leave a.com and b.com SiteInstances in the same
  // BrowsingInstance (so the b.com -> a.com navigation in the next test step
  // will reuse the process associated with the first a.com tab).
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  Shell* new_shell = OpenPopup(shell()->web_contents(), b_url, "newtab");
  WebContents* new_contents = new_shell->web_contents();
  EXPECT_TRUE(WaitForLoadStop(new_contents));
  RenderProcessHost* b_process =
      new_contents->GetPrimaryMainFrame()->GetProcess();
  EXPECT_NE(a_process, b_process);

  // Hang the first tab's renderer.
  const char* kHungScript = "setTimeout(function() { for (;;) {}; }, 0);";
  EXPECT_TRUE(ExecJs(shell()->web_contents(), kHungScript));

  // Hide the second tab.  This should prevent reporting of hangs in this tab
  // (see https://crbug.com/881812).
  new_contents->WasHidden();
  EXPECT_EQ(Visibility::HIDDEN, new_contents->GetVisibility());

  // Attempt to navigate the second tab to a.com.  This will attempt to reuse
  // the hung process.
  base::TimeDelta kTimeout = base::Milliseconds(100);
  NavigationRequest::SetCommitTimeoutForTesting(kTimeout);
  GURL hung_url(embedded_test_server()->GetURL("a.com", "/title3.html"));
  UnresponsiveRendererObserver unresponsive_renderer_observer(new_contents);
  EXPECT_TRUE(
      ExecJs(new_contents, JsReplace("window.location = $1", hung_url)));

  // Verify that we will not be notified about the unresponsive renderer.
  // Before changes in https://crrev.com/c/1089797, the test would get notified
  // and therefore |hung_process| would be non-null.
  RenderProcessHost* hung_process =
      unresponsive_renderer_observer.Wait(kTimeout * 10);
  EXPECT_FALSE(hung_process);

  // Reset the timeout.
  NavigationRequest::SetCommitTimeoutForTesting(base::TimeDelta());
}

// Tests that an inner WebContents will reattach to its outer WebContents after
// a navigation that causes a process swap.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, ProcessSwapOnInnerContents) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* child_frame =
      web_contents()->GetPrimaryFrameTree().root()->child_at(0);
  WebContentsImpl* inner_contents =
      static_cast<WebContentsImpl*>(CreateAndAttachInnerContents(
          ToRenderFrameHost(child_frame).render_frame_host()));
  FrameTreeNode* inner_contents_root =
      inner_contents->GetPrimaryFrameTree().root();
  RenderFrameProxyHost* outer_proxy =
      inner_contents_root->render_manager()->GetProxyToOuterDelegate();
  CrossProcessFrameConnector* outer_connector =
      outer_proxy->cross_process_frame_connector();
  EXPECT_NE(nullptr, outer_connector->get_view_for_testing());

  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(inner_contents_root, a_url));
  SiteInstance* a_site_instance =
      inner_contents->GetPrimaryMainFrame()->GetSiteInstance();
  RenderProcessHost* a_process = a_site_instance->GetProcess();
  RenderWidgetHostViewChildFrame* a_view =
      outer_connector->get_view_for_testing();

  GURL b_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(inner_contents_root, b_url));
  SiteInstance* b_site_instance =
      inner_contents->GetPrimaryMainFrame()->GetSiteInstance();
  RenderProcessHost* b_process = b_site_instance->GetProcess();
  RenderWidgetHostViewChildFrame* b_view =
      outer_connector->get_view_for_testing();

  // Ensure that the SiteInstances have changed, we've completed a process swap
  // and reattached the inner WebContents creating a new RenderWidgetHostView.
  EXPECT_NE(a_site_instance, b_site_instance);
  EXPECT_NE(a_process, b_process);
  EXPECT_NE(nullptr, a_view);
  EXPECT_NE(nullptr, b_view);
  EXPECT_NE(a_view, b_view);
}

// This test ensures that WebContentsImpl::FocusOwningWebContents() focuses an
// inner WebContents when it is given an OOPIF's RenderWidgetHost inside that
// inner WebContents.  This setup isn't currently supported in Chrome
// (requiring issue 614463), but it can happen in embedders.  See
// https://crbug.com/1026056.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, FocusInnerContentsFromOOPIF) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Set up and attach an artificial inner WebContents.
  FrameTreeNode* child_frame =
      web_contents()->GetPrimaryFrameTree().root()->child_at(0);
  WebContentsImpl* inner_contents =
      static_cast<WebContentsImpl*>(CreateAndAttachInnerContents(
          ToRenderFrameHost(child_frame).render_frame_host()));
  FrameTreeNode* inner_contents_root =
      inner_contents->GetPrimaryFrameTree().root();

  // Navigate inner WebContents to b.com, and then navigate a subframe on that
  // page to c.com.
  GURL b_url(embedded_test_server()->GetURL(
      "b.com", "/cross_site_iframe_factory.html?b(b)"));
  EXPECT_TRUE(NavigateToURLFromRenderer(inner_contents_root, b_url));
  GURL c_url(embedded_test_server()->GetURL("c.com", "/title1.html"));
  FrameTreeNode* inner_child = inner_contents_root->child_at(0);
  EXPECT_TRUE(NavigateToURLFromRenderer(inner_child, c_url));

  // Because |inner_contents| was set up without kGuestScheme, it can actually
  // have OOPIFs.  Ensure that the subframe is in an OOPIF.
  EXPECT_NE(inner_contents_root->current_frame_host()->GetSiteInstance(),
            inner_child->current_frame_host()->GetSiteInstance());
  EXPECT_TRUE(inner_child->current_frame_host()->IsCrossProcessSubframe());

  // Make sure the outer WebContents is focused to start with.
  web_contents()->Focus();
  web_contents()->SetAsFocusedWebContentsIfNecessary();
  EXPECT_EQ(web_contents(), web_contents()->GetFocusedWebContents());

  // Focus the inner WebContents as if an event were received and dispatched
  // directly on the |inner_child|'s RenderWidgetHost, and ensure that this
  // took effect.
  inner_contents->FocusOwningWebContents(
      inner_child->current_frame_host()->GetRenderWidgetHost());
  EXPECT_EQ(inner_contents, web_contents()->GetFocusedWebContents());
}

// Check that a web frame can't navigate a remote subframe to a file: URL.  The
// frame should stay at the old URL, and the navigation attempt should produce
// a console error message.  See https://crbug.com/894399.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       FileURLBlockedWithConsoleErrorInRemoteFrameNavigation) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* child =
      web_contents()->GetPrimaryFrameTree().root()->child_at(0);
  GURL original_frame_url(child->current_frame_host()->GetLastCommittedURL());
  EXPECT_EQ("b.com", original_frame_url.host());

  WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern("Not allowed to load local resource: file:*");

  GURL file_url("file:///");
  EXPECT_TRUE(
      ExecJs(web_contents(),
             JsReplace("document.querySelector('iframe').src = $1", file_url)));
  ASSERT_TRUE(console_observer.Wait());

  // The iframe should've stayed at the original URL.
  EXPECT_EQ(original_frame_url,
            child->current_frame_host()->GetLastCommittedURL());
}

// Touchscreen DoubleTapZoom is only supported on Android & ChromeOS at present.
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_ANDROID)
// A test ContentBrowserClient implementation which enforces
// WebPreferences' |double_tap_to_zoom_enabled| to be true.
class DoubleTapZoomContentBrowserClient
    : public ContentBrowserTestContentBrowserClient {
 public:
  DoubleTapZoomContentBrowserClient() = default;

  DoubleTapZoomContentBrowserClient(const DoubleTapZoomContentBrowserClient&) =
      delete;
  DoubleTapZoomContentBrowserClient& operator=(
      const DoubleTapZoomContentBrowserClient&) = delete;

  void OverrideWebkitPrefs(
      content::WebContents* web_contents,
      blink::web_pref::WebPreferences* web_prefs) override {
    web_prefs->double_tap_to_zoom_enabled = true;
  }
};

IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       TouchscreenAnimateDoubleTapZoomInOOPIF) {
  // Install a client forcing double-tap zoom to be enabled.
  DoubleTapZoomContentBrowserClient content_browser_client;
  web_contents()->OnWebPreferencesChanged();

  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  ASSERT_EQ(1u, root->child_count());
  FrameTreeNode* child_b = root->child_at(0);
  ASSERT_TRUE(child_b);

  RenderFrameSubmissionObserver observer_a(root);
  // We need to observe a root frame submission to pick up the initial page
  // scale factor.
  observer_a.WaitForAnyFrameSubmission();
  float original_page_scale =
      observer_a.LastRenderFrameMetadata().page_scale_factor;

  // Must do this before it's safe to use the coordinate transform functions.
  WaitForHitTestData(child_b->current_frame_host());

  // Select a tap point inside the OOPIF.
  gfx::PointF tap_position =
      child_b->current_frame_host()
          ->GetRenderWidgetHost()
          ->GetView()
          ->TransformPointToRootCoordSpaceF(gfx::PointF(10, 10));

  // Generate a double-tap.
  static constexpr char kActionsTemplate[] = R"HTML(
      [{
        "source" : "touch",
        "actions" : [
          { "name": "pointerDown", "x": %f, "y": %f},
          { "name": "pointerUp"},
          { "name": "pause", "duration": 50 },
          { "name": "pointerDown", "x": %f, "y": %f},
          { "name": "pointerUp"}
        ]
      }]
  )HTML";
  std::string double_tap_actions_json =
      base::StringPrintf(kActionsTemplate, tap_position.x(), tap_position.y(),
                         tap_position.x(), tap_position.y());
  auto parsed_json =
      base::JSONReader::ReadAndReturnValueWithError(double_tap_actions_json);
  ASSERT_TRUE(parsed_json.has_value()) << parsed_json.error().message;
  ActionsParser actions_parser(std::move(*parsed_json));

  ASSERT_TRUE(actions_parser.Parse());
  auto synthetic_gesture_doubletap = std::make_unique<SyntheticPointerAction>(
      actions_parser.pointer_action_params());

  // Queue the event and wait for it to be acked.
  InputEventAckWaiter ack_waiter(
      child_b->current_frame_host()->GetRenderWidgetHost(),
      blink::WebInputEvent::Type::kGestureDoubleTap);
  auto* host = static_cast<RenderWidgetHostImpl*>(
      root->current_frame_host()->GetRenderWidgetHost());
  host->QueueSyntheticGesture(
      std::move(synthetic_gesture_doubletap),
      base::BindOnce([](SyntheticGesture::Result result) {
        EXPECT_EQ(SyntheticGesture::GESTURE_FINISHED, result);
      }));
  // Waiting for the ack on the child frame ensures the event actually routed
  // through the oopif.
  ack_waiter.Wait();

  // Wait for page scale to change. We'll assume the OOPIF is scaled up by
  // at least 10%.
  float target_scale = 1.1f * original_page_scale;
  float new_page_scale = original_page_scale;
  do {
    observer_a.WaitForAnyFrameSubmission();
    new_page_scale = observer_a.LastRenderFrameMetadata().page_scale_factor;
  } while (new_page_scale < target_scale);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_ANDROID)

class CrossProcessNavigationObjectElementTest
    : public SitePerProcessBrowserTestBase,
      public testing::WithParamInterface<
          std::tuple<std::string, std::string, std::string>> {};

// This test verifies the correctness of rendering fallback in <object> when the
// a cross-origin navigation leads to a 404 error. Assuming the page's origin
// is "a.com", the test cases are:
// 1- Navigating an <object> from "a.com" to invalid "b.com" resource. In this
//    case the load fails for a provisional frame and at that time there is no
//    proxy to parent.
// 2- Navigating an <object> from "b.com" to invalid "b.com". Since navigation
//    is not cross-origin the failure happens for a non-provisional frame.
// 3- Navigation an <object> from "b.com" to invalid "c.com". The load fails for
//    a provisional frame, and at that time there is a proxy to parent.
IN_PROC_BROWSER_TEST_P(CrossProcessNavigationObjectElementTest, FallbackShown) {
  const GURL main_url = embedded_test_server()->GetURL(
      base::StringPrintf("%s.com", std::get<0>(GetParam()).c_str()),
      "/page_with_object_fallback.html");
  const GURL object_valid_url = embedded_test_server()->GetURL(
      base::StringPrintf("%s.com", std::get<1>(GetParam()).c_str()),
      "/title1.html");
  const GURL object_invalid_url = embedded_test_server()->GetURL(
      base::StringPrintf("%s.com", std::get<2>(GetParam()).c_str()),
      "/does-not-exist-throws-404.html");

  ASSERT_TRUE(NavigateToURL(shell(), main_url));

  // Load the contents of <object> (first navigation which is to a valid
  // existing resource) and wait for 'load' event on <object>.
  ASSERT_EQ("OBJECT_LOAD",
            EvalJs(web_contents(), JsReplace("setUrl($1);", object_valid_url)));

  // Verify fallback content is not shown.
  ASSERT_EQ(false, EvalJs(web_contents(), "fallbackVisible()"));

  // Navigate the <object>'s frame to invalid origin. Make sure we do not report
  // the 'load' event (the 404 content loads inside the <object>'s frame and the
  // 'load' event might fire before fallback is detected).
  ASSERT_EQ(true, EvalJs(web_contents(), JsReplace("setUrl($1);"
                                                   "notifyWhenFallbackShown();",
                                                   object_invalid_url)));
}

INSTANTIATE_TEST_SUITE_P(SitePerProcess,
                         CrossProcessNavigationObjectElementTest,
                         testing::Values(std::make_tuple("a", "a", "b"),
                                         std::make_tuple("a", "b", "b"),
                                         std::make_tuple("a", "b", "c")));

#if !BUILDFLAG(IS_ANDROID)
// This test verifies that after occluding a WebContents the RAF inside a
// cross-process child frame is throttled.
// Disabled due to flakiness. crbug.com/1293207
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       DISABLED_OccludedRenderWidgetThrottlesRAF) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* subframe = root->child_at(0);
  GURL page_with_raf_counter =
      embedded_test_server()->GetURL("a.com", "/page_with_raf_counter.html");
  EXPECT_TRUE(NavigateToURLFromRenderer(subframe, page_with_raf_counter));

  // Initially page is visible - wait some time and then ensure a good number of
  // rafs have been generated. On Mac the number of RAFs that occur in 500ms is
  // quite low, see https://crbug.com/1098715.
  auto allow_time_for_rafs = []() {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(1000));
    run_loop.Run();
  };

  ASSERT_TRUE(ExecJs(subframe, "reset_count();"));
  allow_time_for_rafs();
  int32_t default_raf_count = EvalJs(subframe, "raf_count").ExtractInt();
  // On a 60 fps we should expect more than 30 counts - however purely for
  // sanity checking and avoiding unnecessary flakes adding a comparison for a
  // much lower value. This verifies that we did get *some* rAFs.
  EXPECT_GT(default_raf_count, 5);
  web_contents()->WasOccluded();
  ASSERT_TRUE(ExecJs(subframe, "reset_count();"));
  allow_time_for_rafs();
  int32_t raf_count = EvalJs(subframe, "raf_count").ExtractInt();
  // If the frame is throttled, we should expect 0 rAFs.
  EXPECT_EQ(raf_count, 0);
  // Sanity-check: unoccluding will reverse the effect.
  web_contents()->WasShown();
  ASSERT_TRUE(ExecJs(subframe, "reset_count();"));
  allow_time_for_rafs();
  raf_count = EvalJs(subframe, "raf_count").ExtractInt();
  EXPECT_GT(raf_count, 5);
}
#endif

// Test that a renderer locked to origin A will be terminated if it tries to
// commit a navigation to origin B.  See also https://crbug.com/770239.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       CommittedOriginIncompatibleWithOriginLock) {
  GURL start_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  GURL another_url(embedded_test_server()->GetURL("a.com", "/title2.html"));
  const GURL bad_url = GURL("https://b.com");

  // Sanity check the process lock logic.
  auto process_lock =
      root->current_frame_host()->GetProcess()->GetProcessLock();
  IsolationContext isolation_context(
      shell()->web_contents()->GetBrowserContext());
  ProcessLock start_url_lock = ProcessLock::FromSiteInfo(
      SiteInfo::CreateForTesting(isolation_context, start_url));
  ProcessLock another_url_lock = ProcessLock::FromSiteInfo(
      SiteInfo::CreateForTesting(isolation_context, another_url));
  ProcessLock bad_url_lock = ProcessLock::FromSiteInfo(
      SiteInfo::CreateForTesting(isolation_context, bad_url));
  EXPECT_EQ(start_url_lock, process_lock);
  EXPECT_EQ(another_url_lock, process_lock);
  EXPECT_NE(bad_url_lock, process_lock);

  // Leave the commit URL alone, so the URL checks will pass, but change the
  // origin to one that does not match the origin lock of the process.
  PwnCommitIPC(shell()->web_contents(), another_url, another_url,
               url::Origin::Create(bad_url));
  EXPECT_TRUE(
      BeginNavigateToURLFromRenderer(shell()->web_contents(), another_url));

  // Due to the origin lock mismatch, the render process should be killed when
  // it tries to commit.
  RenderProcessHostBadIpcMessageWaiter kill_waiter(
      root->current_frame_host()->GetProcess());
  EXPECT_EQ(bad_message::RFH_INVALID_ORIGIN_ON_COMMIT, kill_waiter.Wait());
}

// This test verifies that plugin elements containing cross-process-frames do
// not become unresponsive during style changes. (see https://crbug.com/781880).
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       PluginElementResponsiveInCrossProcessNavigations) {
  GURL main_frame_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), main_frame_url));
  GURL cross_origin(embedded_test_server()->GetURL("b.com", "/title1.html"));
  std::string msg =
      EvalJs(shell(), JsReplace("var object = document.createElement('object');"
                                "document.body.appendChild(object);"
                                "object.data = $1;"
                                "object.type='text/html';"
                                "object.notify = true;"
                                "new Promise(resolve => {"
                                "  object.onload = () => {"
                                "    if (!object.notify) return;"
                                "    object.notify = false;"
                                "    resolve('done');"
                                "  };"
                                "});",
                                cross_origin))
          .ExtractString();
  ASSERT_EQ("done", msg);
  // To track the frame's visibility an EmbeddedContentView is needed. The
  // following steps make sure the visibility is tracked properly on the browser
  // side.
  auto* frame_connector = web_contents()
                              ->GetPrimaryFrameTree()
                              .root()
                              ->child_at(0)
                              ->render_manager()
                              ->GetProxyToParent()
                              ->cross_process_frame_connector();
  ASSERT_FALSE(frame_connector->IsHidden());
  ASSERT_TRUE(ExecJs(
      shell(), "document.querySelector('object').style.display = 'none';"));
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return frame_connector->IsHidden(); }));
  ASSERT_TRUE(ExecJs(
      shell(), "document.querySelector('object').style.display = 'block';"));
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return !frame_connector->IsHidden(); }));
}

// Pending navigations must be canceled when a frame becomes pending deletion.
//
// 1) Initial state: A(B).
// 2) Navigation from B to C. The server is slow to respond.
// 3) Deletion of B.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       NavigationCommitInIframePendingDeletionAB) {
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/hung"));

  // 1) Initial state: A(B).
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = web_contents()->GetPrimaryMainFrame();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();

  // RFH B has an unload handler.
  rfh_b->DoNotDeleteForTesting();
  EXPECT_TRUE(ExecJs(rfh_b, "onunload=function(){}"));

  // 2) Navigation from B to C. The server is slow to respond.
  TestNavigationManager navigation_observer(web_contents(), url_c);
  EXPECT_TRUE(ExecJs(rfh_b, JsReplace("location.href=$1;", url_c)));
  navigation_observer.WaitForSpeculativeRenderFrameHostCreation();
  RenderFrameHostImpl* rfh_c =
      rfh_b->frame_tree_node()->render_manager()->speculative_frame_host();

  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kActive,
            rfh_a->lifecycle_state());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kActive,
            rfh_b->lifecycle_state());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kSpeculative,
            rfh_c->lifecycle_state());

  // 3) Deletion of B. The unload handler takes times to execute.
  RenderFrameDeletedObserver delete_b(rfh_b), delete_c(rfh_c);
  EXPECT_TRUE(
      ExecJs(rfh_a, JsReplace("document.querySelector('iframe').remove();")));
  EXPECT_FALSE(delete_b.deleted());
  EXPECT_TRUE(delete_c.deleted());  // The speculative RFH is deleted.
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kActive,
            rfh_a->lifecycle_state());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kRunningUnloadHandlers,
            rfh_b->lifecycle_state());

  // The navigation has been canceled.
  ASSERT_TRUE(navigation_observer.WaitForNavigationFinished());
  EXPECT_FALSE(navigation_observer.was_successful());

  // |rfh_b| will complete its deletion at some point:
  EXPECT_FALSE(delete_b.deleted());
  rfh_b->DetachForTesting();
  EXPECT_TRUE(delete_b.deleted());
}

// Pending navigations must be canceled when a frame becomes pending deletion.
//
// 1) Initial state: A(B(C)).
// 2) Navigation from C to D. The server is slow to respond.
// 3) Deletion of B.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       NavigationCommitInIframePendingDeletionABC) {
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c))"));
  GURL url_d(embedded_test_server()->GetURL("d.com", "/hung"));

  // 1) Initial state: A(B(C)).
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = web_contents()->GetPrimaryMainFrame();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();
  RenderFrameHostImpl* rfh_c = rfh_b->child_at(0)->current_frame_host();

  // Leave rfh_c in pending deletion state.
  LeaveInPendingDeletionState(rfh_c);

  // 2) Navigation from C to D. The server is slow to respond.
  TestNavigationManager navigation_observer(web_contents(), url_d);
  EXPECT_TRUE(ExecJs(rfh_c, JsReplace("location.href=$1;", url_d)));
  navigation_observer.WaitForSpeculativeRenderFrameHostCreation();
  RenderFrameHostImpl* rfh_d =
      rfh_c->frame_tree_node()->render_manager()->speculative_frame_host();

  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kActive,
            rfh_a->lifecycle_state());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kActive,
            rfh_b->lifecycle_state());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kActive,
            rfh_c->lifecycle_state());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kSpeculative,
            rfh_d->lifecycle_state());

  // 3) Deletion of D. The unload handler takes times to execute.
  RenderFrameDeletedObserver delete_b(rfh_b), delete_c(rfh_c), delete_d(rfh_d);
  EXPECT_TRUE(
      ExecJs(rfh_a, JsReplace("document.querySelector('iframe').remove();")));
  EXPECT_FALSE(delete_b.deleted());
  EXPECT_FALSE(delete_c.deleted());
  EXPECT_TRUE(delete_d.deleted());  // The speculative RFH is deleted.
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kActive,
            rfh_a->lifecycle_state());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kReadyToBeDeleted,
            rfh_b->lifecycle_state());
  EXPECT_EQ(RenderFrameHost::LifecycleState::kPendingDeletion,
            rfh_b->GetLifecycleState());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kRunningUnloadHandlers,
            rfh_c->lifecycle_state());
  EXPECT_EQ(RenderFrameHost::LifecycleState::kPendingDeletion,
            rfh_c->GetLifecycleState());

  // The navigation has been canceled.
  ASSERT_TRUE(navigation_observer.WaitForNavigationFinished());
  EXPECT_FALSE(navigation_observer.was_successful());

  // |rfh_b| and |rfh_c| will complete their deletion at some point:
  EXPECT_FALSE(delete_b.deleted());
  EXPECT_FALSE(delete_c.deleted());
  rfh_c->DetachForTesting();
  EXPECT_TRUE(delete_b.deleted());
  EXPECT_TRUE(delete_c.deleted());
}

// A same document commit from the renderer process is received while the
// RenderFrameHost is pending deletion.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       SameDocumentCommitWhilePendingDeletion) {
  GURL url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  RenderFrameHostImpl* rfh_a = web_contents()->GetPrimaryMainFrame();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();

  // Frame B has a unload handler. The browser process needs to wait before
  // deleting it.
  EXPECT_TRUE(ExecJs(rfh_b, "onunload=function(){}"));

  RenderFrameDeletedObserver deleted_observer(rfh_b);
  DidStartNavigationObserver did_start_navigation_observer(web_contents());

  // Start a same-document navigation on B.
  ExecuteScriptAsync(rfh_b, "location.href='#fragment'");

  // Simulate A deleting B.
  // It starts before receiving the same-document navigation. The detach ACK is
  // received after.
  rfh_b->DetachFromProxy();
  deleted_observer.WaitUntilDeleted();

  // The navigation was ignored.
  EXPECT_FALSE(did_start_navigation_observer.observed());
}

// An history navigation from the renderer process is received while the
// RenderFrameHost is pending deletion.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       HistoryNavigationWhilePendingDeletion) {
  GURL url_ab(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url_ab));
  RenderFrameHostImpl* rfh_a = web_contents()->GetPrimaryMainFrame();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();
  EXPECT_TRUE(NavigateToURLFromRenderer(rfh_b->frame_tree_node(), url_c));
  RenderFrameHostImpl* rfh_c = rfh_a->child_at(0)->current_frame_host();

  // Set a value in rfh_a that we'll check later to ensure we didn't
  // incorrectly reload it.
  EXPECT_TRUE(ExecJs(rfh_a, "window.foo='bar';"));

  // Frame C has a unload handler. The browser process needs to wait before
  // deleting it.
  EXPECT_TRUE(ExecJs(rfh_c, "onunload=function(){}"));

  RenderFrameDeletedObserver deleted_observer(rfh_c);

  // History navigation on C.
  ExecuteScriptAsync(rfh_c, "history.back();");

  // Simulate A deleting C.
  // It starts before receiving the history navigation. The detach ACK is
  // received after.
  rfh_c->DetachFromProxy();
  deleted_observer.WaitUntilDeleted();

  // The NavigationController won't be able to find the subframe to navigate
  // since it was just detached, so it should cancel the history navigation and
  // not reload the main page.  Verify this by waiting for any pending
  // navigation (there shouldn't be any) and checking that JavaScript state in
  // rfh_a hasn't changed.  Note that because we've waited for rfh_c to be
  // deleted, we know that the browser process has already received an ack for
  // completion of its unload handler, and thus it has also processed the
  // preceding history.back() IPC.
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ("bar", EvalJs(rfh_a, "window.foo"));
}

// One frame navigates using window.open while it is pending deletion. The two
// frames lives in different processes.
// See https://crbug.com/932087.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       OpenUrlToRemoteFramePendingDeletion) {
  GURL url_ab(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url_ab));
  RenderFrameHostImpl* rfh_a = web_contents()->GetPrimaryMainFrame();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();

  // Frame B has a unload handler. The browser process needs to wait before
  // deleting it.
  EXPECT_TRUE(ExecJs(rfh_b, "onunload=function(){}"));
  RenderFrameDeletedObserver deleted_observer(rfh_b);

  // window.open from A in B to url_c.
  DidStartNavigationObserver did_start_navigation_observer(web_contents());
  EXPECT_TRUE(ExecJs(rfh_b, "window.name = 'name';"));
  ExecuteScriptAsync(rfh_a, JsReplace("window.open($1, 'name');", url_c));

  // Simulate A deleting C.
  // It starts before receiving the navigation. The detach ACK is
  // received after.
  rfh_b->DetachFromProxy();
  deleted_observer.WaitUntilDeleted();

  EXPECT_FALSE(did_start_navigation_observer.observed());
}

// Check that if a frame starts a navigation, and the frame's current process
// dies before the response for the navigation comes back, the response will
// not trigger a process kill and will be allowed to commit in a new process.
// See https://crbug.com/968259.
// Note: This test needs to do a browser-initiated navigation because doing
// a renderer-initiated navigation would lead to the navigation being canceled.
// This behavior change has been introduced when navigation moved to use Mojo
// IPCs and is documented here https://crbug.com/988368.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       ProcessDiesBeforeCrossSiteNavigationCompletes) {
  GURL first_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), first_url));
  scoped_refptr<SiteInstanceImpl> first_site_instance(
      web_contents()->GetPrimaryMainFrame()->GetSiteInstance());

  // Start a cross-site navigation and proceed only up to the request start.
  GURL second_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  TestNavigationManager delayer(web_contents(), second_url);
  web_contents()->GetController().LoadURL(
      second_url, Referrer(), ui::PageTransition::PAGE_TRANSITION_TYPED,
      std::string());
  EXPECT_TRUE(delayer.WaitForRequestStart());

  // Terminate the current a.com process.
  RenderProcessHost* first_process =
      web_contents()->GetPrimaryMainFrame()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      first_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  EXPECT_TRUE(first_process->Shutdown(0));
  crash_observer.Wait();
  EXPECT_FALSE(web_contents()->GetPrimaryMainFrame()->IsRenderFrameLive());

  // Resume the cross-site navigation and ensure it commits in a new
  // SiteInstance and process.
  ASSERT_TRUE(delayer.WaitForNavigationFinished());
  EXPECT_TRUE(web_contents()->GetPrimaryMainFrame()->IsRenderFrameLive());
  EXPECT_NE(web_contents()->GetPrimaryMainFrame()->GetProcess(), first_process);
  EXPECT_NE(web_contents()->GetPrimaryMainFrame()->GetSiteInstance(),
            first_site_instance);
  EXPECT_EQ(second_url,
            web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL());
}

enum class InnerWebContentsAttachChildFrameOriginType {
  kSameOriginAboutBlank,
  kSameOriginOther,
  kCrossOrigin
};

class InnerWebContentsAttachTest
    : public SitePerProcessBrowserTestBase,
      public testing::WithParamInterface<
          std::tuple<InnerWebContentsAttachChildFrameOriginType,
                     bool /* original frame has beforeunload handlers */,
                     bool /* user proceeds with attaching */>> {
 public:
  InnerWebContentsAttachTest() {}

  InnerWebContentsAttachTest(const InnerWebContentsAttachTest&) = delete;
  InnerWebContentsAttachTest& operator=(const InnerWebContentsAttachTest&) =
      delete;

  ~InnerWebContentsAttachTest() override {}

 protected:
  // Helper class to initiate and conclude a frame preparation process for
  // attaching an inner WebContents.
  class PrepareFrameJob {
   public:
    PrepareFrameJob(RenderFrameHostImpl* original_render_frame_host,
                    bool proceed_through_beforeunload) {
      auto* web_contents =
          WebContents::FromRenderFrameHost(original_render_frame_host);
      // Need user gesture for 'beforeunload' to fire.
      PrepContentsForBeforeUnloadTest(web_contents);
      // Simulate user choosing to stay on the page after beforeunload fired.
      SetShouldProceedOnBeforeUnload(Shell::FromWebContents(web_contents),
                                     true /* always_proceed */,
                                     proceed_through_beforeunload);
      RenderFrameHost::PrepareForInnerWebContentsAttachCallback callback =
          base::BindOnce(&PrepareFrameJob::OnPrepare, base::Unretained(this));
      original_render_frame_host->PrepareForInnerWebContentsAttach(
          std::move(callback));
    }

    PrepareFrameJob(const PrepareFrameJob&) = delete;
    PrepareFrameJob& operator=(const PrepareFrameJob&) = delete;

    virtual ~PrepareFrameJob() {}

    void WaitForPreparedFrame() {
      if (did_call_prepare_)
        return;
      run_loop_.Run();
    }

    RenderFrameHostImpl* prepared_frame() const {
      return new_render_frame_host_;
    }

   private:
    void OnPrepare(RenderFrameHost* render_frame_host) {
      did_call_prepare_ = true;
      new_render_frame_host_ =
          static_cast<RenderFrameHostImpl*>(render_frame_host);
      if (run_loop_.running())
        run_loop_.Quit();
    }

    bool did_call_prepare_ = false;
    raw_ptr<RenderFrameHostImpl> new_render_frame_host_ = nullptr;
    base::RunLoop run_loop_;
  };
};

// This is a test for the FrameTreeNode preparation process for various types
// of outer WebContents RenderFrameHosts; essentially when connecting two
// WebContents through a frame in a WebPage it is possible that the frame itself
// has a nontrivial document (other than about:blank) with a beforeunload
// handler, or even it is a cross-process frame. For such cases the frame first
// needs to be sanitized to be later consumed by the WebContents attaching API.
IN_PROC_BROWSER_TEST_P(InnerWebContentsAttachTest, PrepareFrame) {
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(
                                 "a.com", "/page_with_object_fallback.html")));
  InnerWebContentsAttachChildFrameOriginType child_frame_origin_type =
      std::get<0>(GetParam());
  bool test_beforeunload = std::get<1>(GetParam());
  bool proceed_through_beforeunload = std::get<2>(GetParam());
  GURL child_frame_url =
      child_frame_origin_type ==
              InnerWebContentsAttachChildFrameOriginType::kSameOriginAboutBlank
          ? GURL(url::kAboutBlankURL)
          : child_frame_origin_type ==
                    InnerWebContentsAttachChildFrameOriginType::kSameOriginOther
                ? embedded_test_server()->GetURL("a.com", "/title1.html")
                : embedded_test_server()->GetURL("b.com", "/title1.html");
  SCOPED_TRACE(testing::Message()
               << " Child frame URL:" << child_frame_url.spec()
               << " 'beforeunload' modal shown: " << test_beforeunload
               << " proceed through'beforeunload':  "
               << proceed_through_beforeunload);
  auto* child_node = web_contents()->GetPrimaryFrameTree().root()->child_at(0);
  EXPECT_TRUE(NavigateToURLFromRenderer(child_node, child_frame_url));
  if (test_beforeunload) {
    if (base::FeatureList::IsEnabled(
            blink::features::kBeforeunloadEventCancelByPreventDefault)) {
      EXPECT_TRUE(ExecJs(child_node,
                         "window.addEventListener('beforeunload', (e) => {"
                         "e.preventDefault(); return e; });"));
    } else {
      EXPECT_TRUE(ExecJs(child_node,
                         "window.addEventListener('beforeunload', (e) => {"
                         "e.returnValue = 'Not empty string'; return e; });"));
    }
  }
  auto* original_child_frame = child_node->current_frame_host();
  RenderFrameDeletedObserver original_child_frame_observer(
      original_child_frame);
  AppModalDialogWaiter dialog_waiter(shell());
  PrepareFrameJob prepare_job(original_child_frame,
                              proceed_through_beforeunload);
  if (test_beforeunload)
    dialog_waiter.Wait();
  prepare_job.WaitForPreparedFrame();
  auto* new_render_frame_host = prepare_job.prepared_frame();
  bool did_prepare_frame = new_render_frame_host;
  bool same_frame_used = (new_render_frame_host == original_child_frame);
  // If a frame was not prepared, then it has to be due to beforeunload being
  // dismissed.
  ASSERT_TRUE(did_prepare_frame ||
              (test_beforeunload && !proceed_through_beforeunload));
  // If the original frame is in the same SiteInstance as its parent, then it
  // can be reused; otherwise a new frame is expected here.
  bool is_same_origin =
      child_frame_origin_type !=
      InnerWebContentsAttachChildFrameOriginType::kCrossOrigin;
  if (!is_same_origin && did_prepare_frame) {
    // For the cross-origin case we expect the original RenderFrameHost to go
    // away during preparation.
    original_child_frame_observer.WaitUntilDeleted();
  }
  ASSERT_TRUE(!did_prepare_frame || (is_same_origin == same_frame_used));
  ASSERT_TRUE(!did_prepare_frame ||
              (original_child_frame_observer.deleted() != is_same_origin));
  // Finally, try the WebContents attach API and make sure we are doing OK.
  if (new_render_frame_host)
    CreateAndAttachInnerContents(new_render_frame_host);
}

INSTANTIATE_TEST_SUITE_P(
    SitePerProcess,
    InnerWebContentsAttachTest,
    testing::Combine(
        testing::ValuesIn(
            {InnerWebContentsAttachChildFrameOriginType::kSameOriginAboutBlank,
             InnerWebContentsAttachChildFrameOriginType::kSameOriginOther,
             InnerWebContentsAttachChildFrameOriginType::kCrossOrigin}),
        testing::Bool(),
        testing::Bool()));

// This checks what process is used when an iframe is navigated to about:blank.
// The new document should be loaded in the process of its initiator.
//
// Test case:
// 1. Navigate to A1(B2).
// 2. B2 navigates itself to B3 = about:blank. Process B is used.
// 3. A1 makes B3 to navigate to A4 = about:blank. Process A is used.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       SameAndCrossProcessIframeAboutBlankNavigation) {
  // 1. Navigate to A1(B2).
  GURL a1_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), a1_url));
  RenderFrameHostImpl* a1_rfh = web_contents()->GetPrimaryMainFrame();
  RenderFrameHostImpl* b2_rfh = a1_rfh->child_at(0)->current_frame_host();

  // 2. B2 navigates itself to B3 = about:blank. Process B is used.
  {
    scoped_refptr<SiteInstance> b2_site_instance = b2_rfh->GetSiteInstance();
    TestNavigationManager navigation_manager(web_contents(),
                                             GURL("about:blank"));
    EXPECT_TRUE(ExecJs(b2_rfh, "location.href = 'about:blank';"));
    ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());

    RenderFrameHostImpl* b3_rfh = a1_rfh->child_at(0)->current_frame_host();
    DCHECK_EQ(b3_rfh->GetSiteInstance(), b2_site_instance);
    DCHECK_NE(a1_rfh->GetProcess(), b3_rfh->GetProcess());
  }

  // 3. A1 makes B3 to navigate to A4 = about:blank. Process A is used.
  {
    TestNavigationManager navigation_manager(web_contents(),
                                             GURL("about:blank"));
    EXPECT_TRUE(ExecJs(a1_rfh, R"(
      document.querySelector("iframe").src = "about:blank";
    )"));
    ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());

    RenderFrameHostImpl* b4_rfh = a1_rfh->child_at(0)->current_frame_host();
    DCHECK_EQ(a1_rfh->GetSiteInstance(), b4_rfh->GetSiteInstance());
  }
}

IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       AccessWindowProxyOfCrashedFrameAfterNavigation) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));
  const GURL cross_site_url =
      embedded_test_server()->GetURL("b.com", "/title1.html");
  TestNavigationObserver observer(cross_site_url);
  observer.StartWatchingNewWebContents();
  EXPECT_TRUE(ExecJs(
      shell(), JsReplace("openedWindow = window.open($1)", cross_site_url)));
  observer.WaitForNavigationFinished();

  EXPECT_EQ(2u, Shell::windows().size());
  CrashTab(Shell::windows()[1]->web_contents());

  // When starting a navigation in a crashed frame, the navigation code
  // immediately swaps in the speculative RFH.
  EXPECT_TRUE(
      ExecJs(shell(), "openedWindow.location = 'data:text/html,content'"));
  // The early-swapped frame should not be scriptable from another frame--nor
  // should trying to script it result in a crash.
  std::string result =
      EvalJs(shell(),
             "try { openedWindow.document } catch (e) { e.toString(); }")
          .ExtractString();
  EXPECT_THAT(
      result,
      ::testing::MatchesRegex(
          "SecurityError: Failed to read a named property 'document' from "
          "'Window': Blocked a frame with origin \"http://a.com:\\d+\" "
          "from accessing a cross-origin frame."));
}

// Make sure that a popup with a cross site subframe can be closed from the
// subframe.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, CloseNoopenerWindow) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Open a same site popup with a subframe using the noopener ref.
  GURL popup_url(
      embedded_test_server()->GetURL("a.com", "/page_with_blank_iframe.html"));
  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(ExecJs(
      shell(),
      JsReplace("popup = window.open($1,'_blank','noopener');", popup_url)));
  Shell* popup = new_shell_observer.GetShell();
  WebContentsImpl* popup_web_contents =
      static_cast<WebContentsImpl*>(popup->web_contents());
  FrameTreeNode* popup_root = popup_web_contents->GetPrimaryFrameTree().root();
  EXPECT_TRUE(WaitForLoadStop(popup_web_contents));

  // Navigate the popup subframe cross site to b.com.
  FrameTreeNode* child = popup_root->child_at(0);
  GURL cross_origin_url(
      embedded_test_server()->GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(child, cross_origin_url));

  // Check that the popup successfully closes from the subframe.
  WebContentsDestroyedWatcher destroyed_watcher(popup->web_contents());
  EXPECT_TRUE(ExecJs(child, "window.parent.close()"));
  destroyed_watcher.Wait();
}

// Check that initial navigations to renderer debug URLs mark the renderer
// process as used, so that future navigations to sites that require a
// dedicated process do not reuse that process.
IN_PROC_BROWSER_TEST_P(
    SitePerProcessBrowserTest,
    ProcessNotReusedAfterInitialNavigationToRendererDebugURL) {
  // Load a javascript URL, which is a renderer debug URL.  This navigation
  // won't commit, but the renderer process will synchronously process the
  // javascript URL and install an HTML document that contains "foo".
  GURL javascript_url("javascript:'foo'");
  shell()->LoadURL(javascript_url);
  EXPECT_EQ("foo", EvalJs(shell(), "document.body.innerText"));

  RenderProcessHost* js_process =
      web_contents()->GetPrimaryMainFrame()->GetProcess();

  // Because the javascript URL can run arbitrary scripts in the renderer
  // process, it is unsafe to reuse the renderer process later for navigations
  // to sites that require a dedicated process.  Ensure that this is the case.
  EXPECT_FALSE(js_process->IsUnused());

  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));
  EXPECT_NE(js_process, web_contents()->GetPrimaryMainFrame()->GetProcess());
}

// Test that cross-site navigations clear user activation.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, UserActivationCrossSite) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);

  // Sanity check that there is no sticky user activation at first.
  EXPECT_FALSE(child->current_frame_host()->HasStickyUserActivation());
  EXPECT_EQ(false, EvalJs(child->current_frame_host(),
                          "navigator.userActivation.hasBeenActive",
                          EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Load cross-site page into iframe and verify there is still no sticky user
  // activation.
  GURL first_http_url(embedded_test_server()->GetURL("d.com", "/title1.html"));
  EXPECT_TRUE(
      NavigateToURLFromRendererWithoutUserGesture(child, first_http_url));
  EXPECT_FALSE(child->current_frame_host()->HasStickyUserActivation());
  EXPECT_EQ(false, EvalJs(child->current_frame_host(),
                          "navigator.userActivation.hasBeenActive",
                          EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Give the child iframe user activation.
  EXPECT_TRUE(ExecJs(child, "// No-op script"));
  EXPECT_TRUE(child->current_frame_host()->HasStickyUserActivation());
  EXPECT_EQ(true, EvalJs(child->current_frame_host(),
                         "navigator.userActivation.hasBeenActive",
                         EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Perform another cross-site navigation in the iframe.
  GURL http_url(embedded_test_server()->GetURL("c.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURLFromRendererWithoutUserGesture(child, http_url));

  // The cross-site navigation should have cleared the user activation.
  EXPECT_FALSE(child->current_frame_host()->HasStickyUserActivation());
  EXPECT_EQ(false, EvalJs(child->current_frame_host(),
                          "navigator.userActivation.hasBeenActive",
                          EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Ensure that a top-level navigation cannot happen.
  EXPECT_TRUE(ExecJs(child->current_frame_host(),
                     JsReplace("window.open($1, $2)", http_url, "_top"),
                     EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_NE(http_url, shell()->web_contents()->GetLastCommittedURL());
}

// Test that same-site cross-origin navigations keep user activation.
// TODO(crbug.com/40228985): Find a way to reset activation here without
// breaking sites in practice.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, UserActivationSameSite) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);

  // Sanity check that there is no sticky user activation at first.
  EXPECT_FALSE(child->current_frame_host()->HasStickyUserActivation());
  EXPECT_EQ(false, EvalJs(child->current_frame_host(),
                          "navigator.userActivation.hasBeenActive",
                          EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Load cross-origin same-site page into iframe and verify there is still no
  // sticky user activation.
  GURL first_http_url(
      embedded_test_server()->GetURL("subdomain.b.com", "/title1.html"));
  EXPECT_TRUE(
      NavigateToURLFromRendererWithoutUserGesture(child, first_http_url));
  EXPECT_FALSE(child->current_frame_host()->HasStickyUserActivation());
  EXPECT_EQ(false, EvalJs(child->current_frame_host(),
                          "navigator.userActivation.hasBeenActive",
                          EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Give the child iframe user activation.
  EXPECT_TRUE(ExecJs(child, "// No-op script"));
  EXPECT_TRUE(child->current_frame_host()->HasStickyUserActivation());
  EXPECT_EQ(true, EvalJs(child->current_frame_host(),
                         "navigator.userActivation.hasBeenActive",
                         EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Perform another same-site navigation in the iframe.
  GURL http_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURLFromRendererWithoutUserGesture(child, http_url));

  // The cross-origin same-site navigation should keep the sticky user
  // activation from the previous page.
  EXPECT_TRUE(child->current_frame_host()->HasStickyUserActivation());
  EXPECT_EQ(true, EvalJs(child->current_frame_host(),
                         "navigator.userActivation.hasBeenActive",
                         EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Ensure that top-level navigations can still happen.
  EXPECT_TRUE(ExecJs(child->current_frame_host(),
                     JsReplace("window.open($1, $2)", http_url, "_top"),
                     EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(http_url, shell()->web_contents()->GetLastCommittedURL());
}

// Test that same-origin navigations keep user activation.
// TODO(crbug.com/40228985): Find a way to reset activation here without
// breaking sites in practice.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, UserActivationSameOrigin) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);

  // Sanity check that there is no sticky user activation at first.
  EXPECT_FALSE(child->current_frame_host()->HasStickyUserActivation());
  EXPECT_EQ(false, EvalJs(child->current_frame_host(),
                          "navigator.userActivation.hasBeenActive",
                          EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Load cross-site page into iframe and verify there is still no sticky user
  // activation.
  GURL first_http_url(embedded_test_server()->GetURL("c.com", "/title1.html"));
  EXPECT_TRUE(NavigateIframeToURL(web_contents(), "child-0", first_http_url));
  EXPECT_FALSE(child->current_frame_host()->HasStickyUserActivation());
  EXPECT_EQ(false, EvalJs(child->current_frame_host(),
                          "navigator.userActivation.hasBeenActive",
                          EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Give the child iframe user activation.
  EXPECT_TRUE(ExecJs(child, "// No-op script"));
  EXPECT_TRUE(child->current_frame_host()->HasStickyUserActivation());
  EXPECT_EQ(true, EvalJs(child->current_frame_host(),
                         "navigator.userActivation.hasBeenActive",
                         EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Load same-origin page into iframe.
  GURL http_url(embedded_test_server()->GetURL("c.com", "/title2.html"));
  EXPECT_TRUE(NavigateIframeToURL(web_contents(), "child-0", http_url));

  // The same-origin navigation should keep the sticky user activation from the
  // previous page.
  EXPECT_TRUE(child->current_frame_host()->HasStickyUserActivation());
  EXPECT_EQ(true, EvalJs(child->current_frame_host(),
                         "navigator.userActivation.hasBeenActive",
                         EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Ensure that top-level navigations can still happen.
  EXPECT_TRUE(ExecJs(child->current_frame_host(),
                     JsReplace("window.open($1, $2)", http_url, "_top"),
                     EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(http_url, shell()->web_contents()->GetLastCommittedURL());
}

// Test which captures behavior of navigation to about:blank in a newly created
// WebContents when an initial SiteInstance is supplied as part of the creation.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       AboutBlankInNewWindowWithInitialSiteInstance) {
  // Start by navigating to a page on a normal web site.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/empty.html")));

  // Now do a browser-initiated navigation to about:blank in a new tab created
  // in the previous SiteInstance. The current behavior is for the navigation
  // to switch to a new SiteInstance, though there is no real requirement for
  // that. In the past the existing SiteInstance was used.
  WebContents::CreateParams new_contents_params(
      web_contents()->GetBrowserContext(), web_contents()->GetSiteInstance());
  std::unique_ptr<WebContents> new_web_contents(
      WebContents::Create(new_contents_params));

  EXPECT_TRUE(NavigateToURL(new_web_contents.get(), GURL(url::kAboutBlankURL)));
  EXPECT_NE(web_contents()->GetPrimaryMainFrame()->GetProcess(),
            new_web_contents->GetPrimaryMainFrame()->GetProcess());
}

// Tests that verify the feature disabling process reuse.
class DisableProcessReusePolicyTest : public SitePerProcessBrowserTest {
 public:
  DisableProcessReusePolicyTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kDisableProcessReuse);
  }
  ~DisableProcessReusePolicyTest() override = default;

  DisableProcessReusePolicyTest(const DisableProcessReusePolicyTest&) = delete;
  DisableProcessReusePolicyTest& operator=(
      const DisableProcessReusePolicyTest&) = delete;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// In two tabs with the same site, open a cross site iframe in each (same site
// for the iframes). Make sure these do not have the same process ID.
IN_PROC_BROWSER_TEST_P(DisableProcessReusePolicyTest,
                       DisableProcessReusePolicy) {
  GURL url(
      embedded_test_server()->GetURL("www.foo.com", "/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);

  // Navigate the subframe cross site, and make sure it is an OOPIF.
  GURL cross_site_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  TestNavigationObserver observer(shell()->web_contents());
  EXPECT_TRUE(NavigateToURLFromRenderer(child, cross_site_url));
  EXPECT_TRUE(child->current_frame_host()->IsCrossProcessSubframe());

  // Open an new tab in a separate BrowsingInstance with the same url as the
  // first tab and open a subframe, also to |cross_site_url|.
  Shell* second_shell = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(second_shell, url));
  FrameTreeNode* second_root =
      static_cast<WebContentsImpl*>(second_shell->web_contents())
          ->GetPrimaryFrameTree()
          .root();
  FrameTreeNode* second_child = second_root->child_at(0);
  EXPECT_TRUE(NavigateToURLFromRenderer(second_child, cross_site_url));
  EXPECT_TRUE(second_child->current_frame_host()->IsCrossProcessSubframe());

  scoped_refptr<SiteInstanceImpl> second_shell_instance =
      second_child->current_frame_host()->GetSiteInstance();
  EXPECT_NE(ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE_WORKER,
            second_shell_instance->process_reuse_policy());
  EXPECT_NE(ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE_SUBFRAME,
            second_shell_instance->process_reuse_policy());

  EXPECT_NE(child->current_frame_host()->GetProcess(),
            second_child->current_frame_host()->GetProcess());
}

class SitePerProcessWithMainFrameThresholdTestBase
    : public SitePerProcessBrowserTestBase {
 public:
  static constexpr size_t kDefaultThreshold = 3;

  explicit SitePerProcessWithMainFrameThresholdTestBase(
      size_t frame_threshold = kDefaultThreshold,
      size_t total_memory_threshold = 0) {
    base::FieldTrialParams params = {
        {"ProcessPerSiteMainFrameThreshold",
         base::StringPrintf("%zu", frame_threshold)}};
    if (total_memory_threshold != 0) {
      params["ProcessPerSiteMainFrameTotalMemoryLimit"] =
          base::StringPrintf("%zu", total_memory_threshold);
    }
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kProcessPerSiteUpToMainFrameThreshold, params);
  }
  ~SitePerProcessWithMainFrameThresholdTestBase() override = default;

  Shell* CreateShellAndNavigateToURL(const GURL& url) {
    const GURL kOtherUrl =
        embedded_test_server()->GetURL("bar.test", "/title1.html");

    Shell* shell = CreateBrowser();
    // Navigate to a different site first so that the new shell has  a non empty
    // site info before navigating to the target site.
    // TODO(crbug.com/40264958): Remove this workaround once we figure
    // out how to handle navigation from an empty site to a new site.
    CHECK(NavigateToURL(shell, kOtherUrl));
    CHECK(NavigateToURL(shell, url));
    return shell;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class SitePerProcessWithMainFrameThresholdTest
    : public SitePerProcessWithMainFrameThresholdTestBase,
      public ::testing::WithParamInterface<std::string> {
 public:
  SitePerProcessWithMainFrameThresholdTest() = default;
  ~SitePerProcessWithMainFrameThresholdTest() override = default;
};

// Tests that a RenderProcessHost is reused up to a certain threshold against
// number of main frames, if the corresponding SiteInstance requires a dedicated
// process. Subframes are irrelevant to the threshold. Once the number of main
// frame reaches to the threshold, a new RenderProcessHost should be created and
// the existing RenderProcessHost should not be reused.
IN_PROC_BROWSER_TEST_P(SitePerProcessWithMainFrameThresholdTest,
                       ReuseProcessUpToThreshold) {
  const GURL kUrl =
      embedded_test_server()->GetURL("foo.test", "/page_with_iframe.html");
  const GURL kOtherUrl =
      embedded_test_server()->GetURL("bar.test", "/title1.html");

  ASSERT_TRUE(NavigateToURL(shell(), kUrl));
  RenderFrameHostImpl* main_frame_in_main_shell =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetPrimaryMainFrame();
  RenderFrameHostImpl* subframe_in_main_shell =
      main_frame_in_main_shell->child_at(0)->current_frame_host();
  ASSERT_EQ(main_frame_in_main_shell->GetProcess(),
            subframe_in_main_shell->GetProcess());

  std::vector<Shell*> shells;
  for (size_t i = 0; i < kDefaultThreshold - 1; ++i) {
    Shell* new_shell = CreateShellAndNavigateToURL(kUrl);
    RenderFrameHostImpl* new_frame =
        static_cast<WebContentsImpl*>(new_shell->web_contents())
            ->GetPrimaryMainFrame();
    // Currently the reuse policy is only applied for sites that require a
    // dedicated process, and if this not the case, the two main frames won't
    // share a process due to being under the process limit.
    if (main_frame_in_main_shell->GetSiteInstance()
            ->RequiresDedicatedProcess()) {
      ASSERT_EQ(main_frame_in_main_shell->GetProcess(),
                new_frame->GetProcess());
    } else {
      ASSERT_NE(main_frame_in_main_shell->GetProcess(),
                new_frame->GetProcess());
    }
    shells.emplace_back(new_shell);
  }

  Shell* non_shared_shell = CreateBrowser();
  // TODO(crbug.com/40264958): Remove this workaround once we figure
  // out how to handle navigation from an empty site to a new site.
  ASSERT_TRUE(NavigateToURL(non_shared_shell, kOtherUrl));
  ASSERT_TRUE(NavigateToURL(non_shared_shell, kUrl));
  RenderFrameHostImpl* main_frame_in_non_shared_frame =
      static_cast<WebContentsImpl*>(non_shared_shell->web_contents())
          ->GetPrimaryMainFrame();
  ASSERT_NE(main_frame_in_main_shell->GetProcess(),
            main_frame_in_non_shared_frame->GetProcess());
  shells.emplace_back(non_shared_shell);

  for (auto*& shell : shells) {
    shell->Close();
  }
}

// A test fixture that provides an upper limit of 4 bytes, so should fail the
// assignment of another outermost main frame into the process.
class SitePerProcessWithMainFrameThresholdWithTotalLimitTest
    : public SitePerProcessWithMainFrameThresholdTestBase,
      public ::testing::WithParamInterface<std::string> {
 public:
  SitePerProcessWithMainFrameThresholdWithTotalLimitTest()
      : SitePerProcessWithMainFrameThresholdTestBase(
            /*frame_threshold=*/10,
            /*total_memory_threshold=*/9) {}
  ~SitePerProcessWithMainFrameThresholdWithTotalLimitTest() override = default;
};

class RendererHostInterceptor
    : public mojom::RendererHostInterceptorForTesting {
 public:
  explicit RendererHostInterceptor(RenderProcessHostImpl* process_host)
      : swapped_impl_(process_host->renderer_host_receiver_for_testing(),
                      this) {}
  mojom::RendererHost* GetForwardingInterface() override {
    return swapped_impl_.old_impl();
  }

#if BUILDFLAG(IS_ANDROID)
  void SetPrivateMemoryFootprint(
      uint64_t private_memory_footprint_bytes) override {
    // Drop this message from the renderer.
  }
#endif

 private:
  mojo::test::ScopedSwapImplForTesting<mojom::RendererHost> swapped_impl_;
};

// Tests that a RenderProcessHost is not reused when the private memory
// footprint of the process exceeds a certain amount.
IN_PROC_BROWSER_TEST_P(SitePerProcessWithMainFrameThresholdWithTotalLimitTest,
                       ExcessiveAllocation) {
  const GURL kUrl =
      embedded_test_server()->GetURL("foo.test", "/page_with_iframe.html");

  base::HistogramTester histograms;
  ASSERT_TRUE(NavigateToURL(shell(), kUrl));
  RenderFrameHostImpl* main_frame_in_main_shell =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetPrimaryMainFrame();
  RenderFrameHostImpl* subframe_in_main_shell =
      main_frame_in_main_shell->child_at(0)->current_frame_host();
  ASSERT_EQ(main_frame_in_main_shell->GetProcess(),
            subframe_in_main_shell->GetProcess());

  Shell* new_shell = CreateShellAndNavigateToURL(kUrl);
  RenderFrameHostImpl* new_frame =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetPrimaryMainFrame();
  ASSERT_NE(main_frame_in_main_shell->GetProcess(), new_frame->GetProcess());
  new_shell->Close();

  // Verify that we hit a limit histogram.
  histograms.ExpectTotalCount(
      "BrowserRenderProcessHost.ProcessPerSiteMainFrameLimit", 1);
  histograms.ExpectBucketCount(
      "BrowserRenderProcessHost.ProcessPerSiteMainFrameLimit", 1, 1);
}

// Tests that opening a fourth tab will put it over the limit and will allocate
// a new process. We allocate 3 main frames that are 2 bytes each. Placing
// a fourth would exceeded the limit of 9.
IN_PROC_BROWSER_TEST_P(SitePerProcessWithMainFrameThresholdWithTotalLimitTest,
                       AllowedAllocation) {
  const GURL kUrl =
      embedded_test_server()->GetURL("foo.test", "/page_with_iframe.html");

  base::HistogramTester histograms;
  ASSERT_TRUE(NavigateToURL(shell(), kUrl));
  RenderFrameHostImpl* main_frame_in_main_shell =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetPrimaryMainFrame();
  RenderFrameHostImpl* subframe_in_main_shell =
      main_frame_in_main_shell->child_at(0)->current_frame_host();
  ASSERT_EQ(main_frame_in_main_shell->GetProcess(),
            subframe_in_main_shell->GetProcess());

  auto* process_host = static_cast<RenderProcessHostImpl*>(
      main_frame_in_main_shell->GetProcess());
  RendererHostInterceptor interceptor(process_host);
  process_host->SetPrivateMemoryFootprintForTesting(2);

  std::vector<Shell*> shells;
  for (size_t i = 0; i < 2; ++i) {
    Shell* new_shell = CreateShellAndNavigateToURL(kUrl);
    RenderFrameHostImpl* new_frame =
        static_cast<WebContentsImpl*>(new_shell->web_contents())
            ->GetPrimaryMainFrame();
    // Currently the reuse policy is only applied for sites that require a
    // dedicated process, and if this not the case, the two main frames won't
    // share a process due to being under the process limit.
    if (main_frame_in_main_shell->GetSiteInstance()
            ->RequiresDedicatedProcess()) {
      ASSERT_EQ(main_frame_in_main_shell->GetProcess(),
                new_frame->GetProcess());
    } else {
      ASSERT_NE(main_frame_in_main_shell->GetProcess(),
                new_frame->GetProcess());
    }
    process_host->SetPrivateMemoryFootprintForTesting(2 * (i + 2));
    shells.emplace_back(new_shell);
  }
  EXPECT_EQ(
      6u, main_frame_in_main_shell->GetProcess()->GetPrivateMemoryFootprint());

  // The 4th outermostmain frame will not fit.
  // The expected size of a frame will be 2, with a scale factor of 1.5
  // 6 + (2 * 1.5) > 9 so the check should fail.
  Shell* fourth_shell = CreateShellAndNavigateToURL(kUrl);
  RenderFrameHostImpl* fourth_frame =
      static_cast<WebContentsImpl*>(fourth_shell->web_contents())
          ->GetPrimaryMainFrame();
  ASSERT_NE(main_frame_in_main_shell->GetProcess(), fourth_frame->GetProcess());
  shells.emplace_back(fourth_shell);
  for (auto*& shell : shells) {
    shell->Close();
  }
  // Verify that we hit a limit histogram.
  histograms.ExpectTotalCount(
      "BrowserRenderProcessHost.ProcessPerSiteMainFrameLimit", 1);
  histograms.ExpectBucketCount(
      "BrowserRenderProcessHost.ProcessPerSiteMainFrameLimit", 3, 1);
}

// Tests that opening a new tab from an existing page via ctrl-click reuses a
// process when both pages are the same-site.
IN_PROC_BROWSER_TEST_P(SitePerProcessWithMainFrameThresholdTest,
                       ReuseProcessOpenTabByCtrlClickLink) {
  const GURL kUrl = embedded_test_server()->GetURL(
      "foo.test", "/ctrl-click-subframe-link.html");
  ASSERT_TRUE(NavigateToURL(shell(), kUrl));
  RenderFrameHostImpl* main_frame =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetPrimaryMainFrame();
  ShellAddedObserver new_shell_observer;
  ASSERT_TRUE(ExecJs(main_frame,
                     "window.domAutomationController.send(ctrlClickLink());"));
  Shell* popup = new_shell_observer.GetShell();
  ASSERT_EQ(main_frame->GetProcess(),
            static_cast<WebContentsImpl*>(popup->web_contents())
                ->GetPrimaryMainFrame()
                ->GetProcess());
}

// Tests that opening a new tab from an existing page via window.open reuses a
// process when both pages are the same-site.
// TODO(crbug.com/40264958): Change this test to use 'noopener' once we
// figure out how to handle navigation from an empty site to a new site.
IN_PROC_BROWSER_TEST_P(SitePerProcessWithMainFrameThresholdTest,
                       ReuseProcessWithOpener) {
  const GURL kUrl = embedded_test_server()->GetURL("foo.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), kUrl));
  RenderFrameHostImpl* main_frame =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetPrimaryMainFrame();
  ShellAddedObserver new_shell_observer;
  ASSERT_TRUE(
      ExecJs(main_frame, "popup = window.open('/title1.html', '_blank');"));
  Shell* popup = new_shell_observer.GetShell();
  ASSERT_EQ(main_frame->GetProcess(),
            static_cast<WebContentsImpl*>(popup->web_contents())
                ->GetPrimaryMainFrame()
                ->GetProcess());
}

class SitePerProcessWithMainFrameThresholdLocalhostTest
    : public SitePerProcessWithMainFrameThresholdTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  SitePerProcessWithMainFrameThresholdLocalhostTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kProcessPerSiteUpToMainFrameThreshold,
        {{"ProcessPerSiteMainFrameThreshold",
          base::StringPrintf("%zu", kDefaultThreshold)},
         {"ProcessPerSiteMainFrameAllowIPAndLocalhost",
          IsLocalhostAllowed() ? "true" : "false"}});
  }
  ~SitePerProcessWithMainFrameThresholdLocalhostTest() override = default;

  bool IsLocalhostAllowed() { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that process reuse is allowed or disallowed for localhost based on a
// feature parameter.
IN_PROC_BROWSER_TEST_P(SitePerProcessWithMainFrameThresholdLocalhostTest,
                       AllowReuseLocalHost) {
  const GURL kUrl = embedded_test_server()->GetURL("localhost", "/title1.html");
  ASSERT_TRUE(net::IsLocalHostname(kUrl.host()));

  ASSERT_TRUE(NavigateToURL(shell(), kUrl));
  Shell* second_shell = CreateShellAndNavigateToURL(kUrl);

  RenderFrameHostImpl* main_frame =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetPrimaryMainFrame();
  RenderFrameHostImpl* second_frame =
      static_cast<WebContentsImpl*>(second_shell->web_contents())
          ->GetPrimaryMainFrame();
  if (IsLocalhostAllowed()) {
    ASSERT_EQ(main_frame->GetProcess(), second_frame->GetProcess());
  } else {
    ASSERT_NE(main_frame->GetProcess(), second_frame->GetProcess());
  }
}

class SitePerProcessWithMainFrameThresholdDevToolsTest
    : public SitePerProcessWithMainFrameThresholdTestBase,
      public TestDevToolsProtocolClient {
 public:
  SitePerProcessWithMainFrameThresholdDevToolsTest() = default;
  ~SitePerProcessWithMainFrameThresholdDevToolsTest() override = default;

  void TearDown() override {
    DetachProtocolClient();
    SitePerProcessWithMainFrameThresholdTestBase::TearDown();
  }
};

// Tests that process reuse is diallowed when DevTools is attached to the
// renderer process.
IN_PROC_BROWSER_TEST_F(SitePerProcessWithMainFrameThresholdDevToolsTest,
                       DevToolsAttached) {
  const GURL kUrl = embedded_test_server()->GetURL("foo.test", "/title1.html");

  ASSERT_TRUE(NavigateToURL(shell(), kUrl));

  AttachToWebContents(shell()->web_contents());
  set_agent_host_can_close();

  Shell* second_shell = CreateShellAndNavigateToURL(kUrl);
  RenderFrameHostImpl* main_frame =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetPrimaryMainFrame();
  RenderFrameHostImpl* second_frame =
      static_cast<WebContentsImpl*>(second_shell->web_contents())
          ->GetPrimaryMainFrame();
  ASSERT_NE(main_frame->GetProcess(), second_frame->GetProcess());
}

// Helper class to enable subframe process reuse thresholds and set the total
// allowed memory limit to 8 bytes.
class SitePerProcessWithSubframeProcessReuseThresholdsTest
    : public SitePerProcessBrowserTestBase,
      public ::testing::WithParamInterface<std::string> {
 public:
  SitePerProcessWithSubframeProcessReuseThresholdsTest() {
    size_t total_memory_limit = 8;
    base::FieldTrialParams params = {
        {"SubframeProcessReuseMemoryThreshold",
         base::StringPrintf("%zu", total_memory_limit)}};
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kSubframeProcessReuseThresholds, params);
  }
  ~SitePerProcessWithSubframeProcessReuseThresholdsTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verify that a subframe will only reuse an existing process if adding
// another subframe to that process won't exceed the memory threshold.
IN_PROC_BROWSER_TEST_P(SitePerProcessWithSubframeProcessReuseThresholdsTest,
                       SubframeReuseRespectsMemoryThreshold) {
  base::HistogramTester histograms;

  // Start with a simple a(b) page.
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  RenderFrameHostImpl* main_frame1 =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetPrimaryMainFrame();
  RenderFrameHostImpl* subframe1 =
      main_frame1->child_at(0)->current_frame_host();
  auto* subframe_process =
      static_cast<RenderProcessHostImpl*>(subframe1->GetProcess());
  ASSERT_NE(main_frame1->GetProcess(), subframe_process);

  // Ignore private memory footprint updates from the renderer, and pretend
  // that the subframe process's PMF is currently 5 bytes.
  RendererHostInterceptor interceptor(subframe_process);
  subframe_process->SetPrivateMemoryFootprintForTesting(5);

  // Create an unrelated tab and navigate it to a(b).
  Shell* shell2 = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(shell2, main_url));
  RenderFrameHostImpl* main_frame2 =
      static_cast<WebContentsImpl*>(shell2->web_contents())
          ->GetPrimaryMainFrame();
  RenderFrameHostImpl* subframe2 =
      main_frame2->child_at(0)->current_frame_host();
  ASSERT_NE(main_frame2->GetProcess(), subframe2->GetProcess());

  // The new b.com subframe should reuse the available b.com process from the
  // first tab. This is because the process uses 5 bytes of memory, which is
  // below the reuse threshold of 8 bytes.
  EXPECT_EQ(subframe2->GetProcess(), subframe_process);

  // Update the subframe process's PMF to 10, pretending that the second
  // subframe also takes up 5 bytes.
  subframe_process->SetPrivateMemoryFootprintForTesting(10);

  // Create a third tab and navigate it to a(b).
  Shell* shell3 = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(shell3, main_url));
  RenderFrameHostImpl* main_frame3 =
      static_cast<WebContentsImpl*>(shell3->web_contents())
          ->GetPrimaryMainFrame();
  RenderFrameHostImpl* subframe3 =
      main_frame3->child_at(0)->current_frame_host();
  ASSERT_NE(main_frame3->GetProcess(), subframe3->GetProcess());

  // This time, the new b.com subframe should not reuse the available b.com
  // process from the first two tabs. This is because the process is consuming
  // 10 bytes of memory, which is above the reuse threshold of 8 bytes.
  EXPECT_NE(subframe3->GetProcess(), subframe_process);

  // Check that the histogram was recorded when the memory threshold was
  // exceeded for `subframe_process`. At that time, the process should've had
  // two total frames.
  histograms.ExpectTotalCount(
      "BrowserRenderProcessHost.SubframeProcessReuseThreshold.TotalFrames", 1);
  histograms.ExpectBucketCount(
      "BrowserRenderProcessHost.SubframeProcessReuseThreshold.TotalFrames", 2,
      1);
}

INSTANTIATE_TEST_SUITE_P(All,
                         RequestDelayingSitePerProcessBrowserTest,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));
#if BUILDFLAG(IS_ANDROID)
INSTANTIATE_TEST_SUITE_P(All,
                         SitePerProcessAndroidImeTest,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));
#endif  // BUILDFLAG(IS_ANDROID)
INSTANTIATE_TEST_SUITE_P(All,
                         SitePerProcessAndProcessPerSiteBrowserTest,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));
INSTANTIATE_TEST_SUITE_P(All,
                         SitePerProcessAutoplayBrowserTest,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));
INSTANTIATE_TEST_SUITE_P(All,
                         SitePerProcessBrowserTest,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));
INSTANTIATE_TEST_SUITE_P(All,
                         SitePerProcessNoSharingBrowserTest,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));
INSTANTIATE_TEST_SUITE_P(All,
                         SitePerProcessBrowserTestWithoutSpeculativeRFHDelay,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));
INSTANTIATE_TEST_SUITE_P(All,
                         SitePerProcessBrowserTouchActionTest,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));
INSTANTIATE_TEST_SUITE_P(All,
                         SitePerProcessIgnoreCertErrorsBrowserTest,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));
INSTANTIATE_TEST_SUITE_P(All,
                         DisableProcessReusePolicyTest,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));
INSTANTIATE_TEST_SUITE_P(All,
                         SitePerProcessWithMainFrameThresholdTest,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));
INSTANTIATE_TEST_SUITE_P(All,
                         SitePerProcessWithMainFrameThresholdWithTotalLimitTest,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));
#if BUILDFLAG(IS_ANDROID)
INSTANTIATE_TEST_SUITE_P(All,
                         TouchSelectionControllerClientAndroidSiteIsolationTest,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));
#endif  // BUILDFLAG(IS_ANDROID)
INSTANTIATE_TEST_SUITE_P(All,
                         SitePerProcessBrowserTestWithLeakDetector,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));

INSTANTIATE_TEST_SUITE_P(All,
                         SitePerProcessWithMainFrameThresholdLocalhostTest,
                         testing::Bool());

INSTANTIATE_TEST_SUITE_P(All,
                         SitePerProcessWithSubframeProcessReuseThresholdsTest,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));

}  // namespace content
