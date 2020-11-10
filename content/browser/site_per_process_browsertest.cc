// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_timeouts.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "cc/input/touch_action.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/viz/common/features.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/renderer_host/agent_scheduling_group_host.h"
#include "content/browser/renderer_host/cross_process_frame_connector.h"
#include "content/browser/renderer_host/frame_navigation_entry.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/input/input_router.h"
#include "content/browser/renderer_host/input/synthetic_gesture.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target.h"
#include "content/browser/renderer_host/input/synthetic_smooth_scroll_gesture.h"
#include "content/browser/renderer_host/input/synthetic_tap_gesture.h"
#include "content/browser/renderer_host/input/synthetic_touchscreen_pinch_gesture.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/common/frame.mojom-test-utils.h"
#include "content/common/frame_messages.h"
#include "content/common/input/actions_parser.h"
#include "content/common/input/synthetic_pinch_gesture_params.h"
#include "content/common/input_messages.h"
#include "content/common/page_messages.h"
#include "content/common/renderer.mojom.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_widget_host_observer.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/use_zoom_for_dsf_policy.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/render_frame_host_test_support.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/shell/common/shell_switches.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/did_commit_navigation_interceptor.h"
#include "content/test/render_document_feature.h"
#include "content/test/test_content_browser_client.h"
#include "ipc/constants.mojom.h"
#include "ipc/ipc_security_test_util.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/dns/mock_host_resolver.h"
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
#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/public/common/feature_policy/policy_value.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-test-utils.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom.h"
#include "ui/display/display_switches.h"
#include "ui/display/screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/blink/blink_features.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/transform.h"
#include "ui/latency/latency_info.h"
#include "ui/native_theme/native_theme_features.h"

#if defined(USE_AURA)
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#endif

#if defined(OS_MAC)
#include "content/browser/renderer_host/input/synthetic_touchpad_pinch_gesture.h"
#include "ui/base/test/scoped_preferred_scroller_style_mac.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "content/browser/android/gesture_listener_manager.h"
#include "content/browser/android/ime_adapter_android.h"
#include "content/browser/renderer_host/input/touch_selection_controller_client_manager_android.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/browser/web_contents/web_contents_view_android.h"
#include "content/public/browser/android/child_process_importance.h"
#include "content/public/common/content_client.h"
#include "content/test/mock_overscroll_refresh_handler_android.h"
#include "content/test/test_content_browser_client.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/events/android/event_handler_android.h"
#include "ui/events/android/motion_event_android.h"
#include "ui/gfx/geometry/point_f.h"
#endif

#if defined(OS_CHROMEOS)
#include "ui/aura/test/test_screen.h"
#endif

using ::testing::SizeIs;
using ::testing::WhenSorted;
using ::testing::ElementsAre;

namespace content {

namespace {

// Helper function to send a postMessage and wait for a reply message.  The
// |post_message_script| is executed on the |sender_ftn| frame, and the sender
// frame is expected to post |reply_status| from the DOMAutomationController
// when it receives a reply.
void PostMessageAndWaitForReply(FrameTreeNode* sender_ftn,
                                const std::string& post_message_script,
                                const std::string& reply_status) {
  // Subtle: msg_queue needs to be declared before the ExecuteScript below, or
  // else it might miss the message of interest.  See https://crbug.com/518729.
  DOMMessageQueue msg_queue;

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

double GetFrameDeviceScaleFactor(const ToRenderFrameHost& adapter) {
  return EvalJs(adapter, "window.devicePixelRatio;").ExtractDouble();
}

class RedirectNotificationObserver : public NotificationObserver {
 public:
  // Register to listen for notifications of the given type from either a
  // specific source, or from all sources if |source| is
  // NotificationService::AllSources().
  RedirectNotificationObserver(int notification_type,
                               const NotificationSource& source);
  ~RedirectNotificationObserver() override;

  // Wait until the specified notification occurs.  If the notification was
  // emitted between the construction of this object and this call then it
  // returns immediately.
  void Wait();

  // Returns NotificationService::AllSources() if we haven't observed a
  // notification yet.
  const NotificationSource& source() const {
    return source_;
  }

  const NotificationDetails& details() const {
    return details_;
  }

  // NotificationObserver:
  void Observe(int type,
               const NotificationSource& source,
               const NotificationDetails& details) override;

 private:
  bool seen_;
  bool seen_twice_;
  bool running_;
  NotificationRegistrar registrar_;

  NotificationSource source_;
  NotificationDetails details_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(RedirectNotificationObserver);
};

RedirectNotificationObserver::RedirectNotificationObserver(
    int notification_type,
    const NotificationSource& source)
    : seen_(false),
      running_(false),
      source_(NotificationService::AllSources()) {
  registrar_.Add(this, notification_type, source);
}

RedirectNotificationObserver::~RedirectNotificationObserver() {}

void RedirectNotificationObserver::Wait() {
  if (seen_ && seen_twice_)
    return;

  running_ = true;
  run_loop_.Run();
  EXPECT_TRUE(seen_);
}

void RedirectNotificationObserver::Observe(
    int type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  source_ = source;
  details_ = details;
  seen_twice_ = seen_;
  seen_ = true;
  if (!running_)
    return;

  run_loop_.Quit();
  running_ = false;
}

// This observer keeps track of the number of created RenderFrameHosts.  Tests
// can use this to ensure that a certain number of child frames has been
// created after navigating.
class RenderFrameHostCreatedObserver : public WebContentsObserver {
 public:
  RenderFrameHostCreatedObserver(WebContents* web_contents,
                                 int expected_frame_count)
      : WebContentsObserver(web_contents),
        expected_frame_count_(expected_frame_count),
        frames_created_(0) {}

  ~RenderFrameHostCreatedObserver() override;

  // Runs a nested run loop and blocks until the expected number of
  // RenderFrameHosts is created.
  void Wait();

 private:
  // WebContentsObserver
  void RenderFrameCreated(RenderFrameHost* render_frame_host) override;

  // The number of RenderFrameHosts to wait for.
  int expected_frame_count_;

  // The number of RenderFrameHosts that have been created.
  int frames_created_;

  // The RunLoop used to spin the message loop.
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameHostCreatedObserver);
};

RenderFrameHostCreatedObserver::~RenderFrameHostCreatedObserver() {
}

void RenderFrameHostCreatedObserver::Wait() {
  run_loop_.Run();
}

void RenderFrameHostCreatedObserver::RenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  frames_created_++;
  if (frames_created_ == expected_frame_count_) {
    run_loop_.Quit();
  }
}

// This observer detects when WebContents receives notification of a user
// gesture having occurred, following a user input event targeted to
// a RenderWidgetHost under that WebContents.
class UserInteractionObserver : public WebContentsObserver {
 public:
  explicit UserInteractionObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents), user_interaction_received_(false) {}

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

  DISALLOW_COPY_AND_ASSIGN(UserInteractionObserver);
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

class RenderWidgetHostVisibilityObserver : public RenderWidgetHostObserver {
 public:
  explicit RenderWidgetHostVisibilityObserver(RenderWidgetHostImpl* rwhi,
                                              bool expected_visibility_state)
      : expected_visibility_state_(expected_visibility_state),
        observer_(this),
        was_observed_(false),
        did_fail_(false),
        render_widget_(rwhi) {
    observer_.Add(render_widget_);
    message_loop_runner_ = new MessageLoopRunner;
  }

  bool WaitUntilSatisfied() {
    if (!was_observed_)
      message_loop_runner_->Run();
    if (observer_.IsObserving(render_widget_))
      observer_.Remove(render_widget_);
    return !did_fail_;
  }

 private:
  void RenderWidgetHostVisibilityChanged(RenderWidgetHost* widget_host,
                                         bool became_visible) override {
    was_observed_ = true;
    did_fail_ = expected_visibility_state_ != became_visible;
    if (message_loop_runner_->loop_running())
      message_loop_runner_->Quit();
  }

  void RenderWidgetHostDestroyed(RenderWidgetHost* widget_host) override {
    observer_.Remove(widget_host);
  }

  bool expected_visibility_state_;
  scoped_refptr<MessageLoopRunner> message_loop_runner_;
  ScopedObserver<RenderWidgetHost, RenderWidgetHostObserver> observer_;
  bool was_observed_;
  bool did_fail_;
  RenderWidgetHost* render_widget_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostVisibilityObserver);
};

bool ConvertJSONToPoint(const std::string& str, gfx::PointF* point) {
  base::Optional<base::Value> value = base::JSONReader::Read(str);
  if (!value.has_value())
    return false;
  if (!value->is_dict())
    return false;
  base::Optional<double> x = value->FindDoubleKey("x");
  base::Optional<double> y = value->FindDoubleKey("y");
  if (!x.has_value())
    return false;
  if (!y.has_value())
    return false;
  point->set_x(x.value());
  point->set_y(y.value());
  return true;
}

void OpenURLBlockUntilNavigationComplete(Shell* shell, const GURL& url) {
  EXPECT_TRUE(WaitForLoadStop(shell->web_contents()));
  TestNavigationObserver same_tab_observer(shell->web_contents(), 1);

  OpenURLParams params(
      url,
      content::Referrer(shell->web_contents()->GetLastCommittedURL(),
                        network::mojom::ReferrerPolicy::kAlways),
      WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_LINK,
      true /* is_renderer_initiated */);
  params.initiator_origin = url::Origin::Create(url);
  shell->OpenURLFromTab(shell->web_contents(), params);

  same_tab_observer.Wait();
}

// Helper function to generate a feature policy for a single feature and a list
// of origins.
// (Equivalent to the declared policy "feature origin1 origin2 ...".)
// If the origins list is empty, it's treated as matches all origins
// (Equivalent to the declared policy "feature *")
blink::ParsedFeaturePolicyDeclaration CreateParsedFeaturePolicyDeclaration(
    blink::mojom::FeaturePolicyFeature feature,
    const std::vector<GURL>& origins,
    bool match_all_origins = false) {
  blink::ParsedFeaturePolicyDeclaration declaration;

  declaration.feature = feature;
  declaration.matches_all_origins = match_all_origins;
  declaration.matches_opaque_src = match_all_origins;

  for (const auto& origin : origins)
    declaration.allowed_origins.push_back(url::Origin::Create(origin));

  std::sort(declaration.allowed_origins.begin(),
            declaration.allowed_origins.end());

  return declaration;
}

blink::ParsedFeaturePolicy CreateParsedFeaturePolicy(
    const std::vector<blink::mojom::FeaturePolicyFeature>& features,
    const std::vector<GURL>& origins,
    bool match_all_origins = false) {
  blink::ParsedFeaturePolicy result;
  result.reserve(features.size());
  for (const auto& feature : features)
    result.push_back(CreateParsedFeaturePolicyDeclaration(feature, origins,
                                                          match_all_origins));
  return result;
}

blink::ParsedFeaturePolicy CreateParsedFeaturePolicyMatchesAll(
    const std::vector<blink::mojom::FeaturePolicyFeature>& features) {
  return CreateParsedFeaturePolicy(features, {}, true);
}

blink::ParsedFeaturePolicy CreateParsedFeaturePolicyMatchesNone(
    const std::vector<blink::mojom::FeaturePolicyFeature>& features) {
  return CreateParsedFeaturePolicy(features, {});
}

// Check frame depth on node, widget, and process all match expected depth.
void CheckFrameDepth(unsigned int expected_depth, FrameTreeNode* node) {
  EXPECT_EQ(expected_depth, node->depth());
  RenderProcessHost::Priority priority =
      node->current_frame_host()->GetRenderWidgetHost()->GetPriority();
  EXPECT_EQ(expected_depth, priority.frame_depth);
  EXPECT_EQ(expected_depth,
            node->current_frame_host()->GetProcess()->GetFrameDepth());
}

// Check |intersects_viewport| on widget and process.
bool CheckIntersectsViewport(bool expected, FrameTreeNode* node) {
  RenderProcessHost::Priority priority =
      node->current_frame_host()->GetRenderWidgetHost()->GetPriority();
  return priority.intersects_viewport == expected &&
         node->current_frame_host()->GetProcess()->GetIntersectsViewport() ==
             expected;
}

// Layout child frames in cross_site_iframe_factory.html so that they are the
// same width as the viewport, and 75% of the height of the window. This is for
// testing viewport intersection. Note this does not recurse into child frames
// and re-layout in the same way since children might be in a different origin.
void LayoutNonRecursiveForTestingViewportIntersection(
    WebContents* web_contents) {
  static const char kRafScript[] = R"(
      let width = window.innerWidth;
      let height = window.innerHeight * 0.75;
      for (let i = 0; i < window.frames.length; i++) {
        let child = document.getElementById("child-" + i);
        child.width = width;
        child.height = height;
      }
  )";
  ASSERT_TRUE(
      EvalJsAfterLifecycleUpdate(web_contents, kRafScript, "").error.empty());
}

void GenerateTapDownGesture(RenderWidgetHost* rwh) {
  blink::WebGestureEvent gesture_tap_down(
      blink::WebGestureEvent::Type::kGestureTapDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests(),
      blink::WebGestureDevice::kTouchscreen);
  gesture_tap_down.is_source_touch_event_set_non_blocking = true;
  rwh->ForwardGestureEvent(gesture_tap_down);
}

// Class to monitor incoming FrameHostMsg_UpdateViewportIntersection messages.
class UpdateViewportIntersectionMessageFilter
    : public content::BrowserMessageFilter {
 public:
  // If no routing_id is specified, filter will match all routing id's.
  explicit UpdateViewportIntersectionMessageFilter(
      int routing_id = MSG_ROUTING_NONE)
      : content::BrowserMessageFilter(FrameMsgStart),
        routing_id_(routing_id),
        msg_received_(false) {}

  bool OnMessageReceived(const IPC::Message& message) override {
    if (routing_id_ == MSG_ROUTING_NONE ||
        message.routing_id() == routing_id_) {
      IPC_BEGIN_MESSAGE_MAP(UpdateViewportIntersectionMessageFilter, message)
        IPC_MESSAGE_HANDLER(FrameHostMsg_UpdateViewportIntersection,
                            OnUpdateViewportIntersection)
      IPC_END_MESSAGE_MAP()
    }
    return false;
  }

  const blink::ViewportIntersectionState& GetIntersectionState() const {
    return intersection_state_;
  }

  bool MessageReceived() const { return msg_received_; }

  void Clear() {
    msg_received_ = false;
    intersection_state_ = blink::ViewportIntersectionState();
  }

  void Wait() {
    DCHECK(!run_loop_);
    if (msg_received_) {
      msg_received_ = false;
      return;
    }
    std::unique_ptr<base::RunLoop> run_loop(new base::RunLoop);
    run_loop_ = run_loop.get();
    run_loop_->Run();
    run_loop_ = nullptr;
    msg_received_ = false;
  }

  void set_run_loop(base::RunLoop* run_loop) { run_loop_ = run_loop; }

 private:
  ~UpdateViewportIntersectionMessageFilter() override {}

  void OnUpdateViewportIntersection(
      const blink::ViewportIntersectionState& intersection_state) {
    intersection_state_ = intersection_state;
    msg_received_ = true;
    if (run_loop_)
      run_loop_->Quit();
  }
  const int routing_id_;
  base::RunLoop* run_loop_ = nullptr;
  bool msg_received_;
  blink::ViewportIntersectionState intersection_state_;
  DISALLOW_COPY_AND_ASSIGN(UpdateViewportIntersectionMessageFilter);
};

}  // namespace

//
// SitePerProcessBrowserTestBase
//

SitePerProcessBrowserTestBase::SitePerProcessBrowserTestBase() {
#if !defined(OS_ANDROID)
  // TODO(bokan): Needed for scrollability check in
  // FrameOwnerPropertiesPropagationScrolling. crbug.com/662196.
  feature_list_.InitAndDisableFeature(features::kOverlayScrollbar);
#endif
}

std::string SitePerProcessBrowserTestBase::DepictFrameTree(
    FrameTreeNode* node) {
  return visualizer_.DepictFrameTree(node);
}

void SitePerProcessBrowserTestBase::SetUpCommandLine(
    base::CommandLine* command_line) {
  ContentBrowserTest::SetUpCommandLine(command_line);
  IsolateAllSitesForTesting(command_line);

  command_line->AppendSwitch(switches::kValidateInputEventStream);
}

void SitePerProcessBrowserTestBase::SetUpOnMainThread() {
  host_resolver()->AddRule("*", "127.0.0.1");
  SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
}

//
// SitePerProcessBrowserTest
//

SitePerProcessBrowserTest::SitePerProcessBrowserTest() {
  InitAndEnableRenderDocumentFeature(&feature_list_, GetParam());
}

//
// SitePerProcessHighDPIBrowserTest
//

class SitePerProcessHighDPIBrowserTest : public SitePerProcessBrowserTest {
 public:
  const double kDeviceScaleFactor = 2.0;

  SitePerProcessHighDPIBrowserTest() {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SitePerProcessBrowserTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kForceDeviceScaleFactor,
        base::StringPrintf("%f", kDeviceScaleFactor));
  }
};

// SitePerProcessIgnoreCertErrorsBrowserTest

class SitePerProcessIgnoreCertErrorsBrowserTest
    : public SitePerProcessBrowserTest {
 public:
  SitePerProcessIgnoreCertErrorsBrowserTest() {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SitePerProcessBrowserTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }
};

// SitePerProcessFeaturePolicyBrowserTest

class SitePerProcessFeaturePolicyBrowserTest
    : public SitePerProcessBrowserTestBase {
 public:
  SitePerProcessFeaturePolicyBrowserTest() = default;

  // Enable tests for parameterized features.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SitePerProcessBrowserTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "ExperimentalProductivityFeatures");
  }
};

// SitePerProcessAutoplayBrowserTest

class SitePerProcessAutoplayBrowserTest : public SitePerProcessBrowserTest {
 public:
  SitePerProcessAutoplayBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SitePerProcessBrowserTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kAutoplayPolicy,
        switches::autoplay::kDocumentUserActivationRequiredPolicy);
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "FeaturePolicyAutoplayFeature");
  }

  bool AutoplayAllowed(const ToRenderFrameHost& adapter,
                       bool with_user_gesture) {
    RenderFrameHost* rfh = adapter.render_frame_host();
    const char* test_script = "attemptPlay();";
    bool worked = false;
    if (with_user_gesture) {
      EXPECT_TRUE(ExecuteScriptAndExtractBool(rfh, test_script, &worked));
    } else {
      EXPECT_TRUE(ExecuteScriptWithoutUserGestureAndExtractBool(
          rfh, test_script, &worked));
    }
    return worked;
  }

  void NavigateFrameAndWait(FrameTreeNode* node, const GURL& url) {
    NavigateFrameToURL(node, url);
    EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
    EXPECT_EQ(url, node->current_url());
  }
};

class SitePerProcessScrollAnchorTest : public SitePerProcessBrowserTest {
 public:
  SitePerProcessScrollAnchorTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SitePerProcessBrowserTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "ScrollAnchorSerialization");
  }
};

// SitePerProcessEmbedderCSPEnforcementBrowserTest

class SitePerProcessEmbedderCSPEnforcementBrowserTest
    : public SitePerProcessBrowserTest {
 public:
  SitePerProcessEmbedderCSPEnforcementBrowserTest() {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SitePerProcessBrowserTestBase::SetUpCommandLine(command_line);
    // TODO(amalika): Remove this switch when the EmbedderCSPEnforcement becomes
    // stable
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "EmbedderCSPEnforcement");
  }
};

// SitePerProcessProgrammaticScrollTest.

class SitePerProcessProgrammaticScrollTest : public SitePerProcessBrowserTest {
 public:
  SitePerProcessProgrammaticScrollTest()
      : kInfinity(1000000U), kPositiveXYPlane(0, 0, kInfinity, kInfinity) {}

 protected:
  const size_t kInfinity;
  const std::string kIframeOutOfViewHTML = "/iframe_out_of_view.html";
  const std::string kIframeClippedHTML = "/iframe_clipped.html";
  const std::string kInputBoxHTML = "/input_box.html";
  const std::string kIframeSelector = "iframe";
  const std::string kInputSelector = "input";
  const gfx::Rect kPositiveXYPlane;

  // Waits until the |load| handle is called inside the frame.
  void WaitForOnLoad(FrameTreeNode* node) {
    RunCommandAndWaitForResponse(node, "notifyWhenLoaded();", "LOADED");
  }

  void WaitForElementVisible(FrameTreeNode* node, const std::string& sel) {
    RunCommandAndWaitForResponse(
        node,
        base::StringPrintf("notifyWhenVisible(document.querySelector('%s'));",
                           sel.c_str()),
        "VISIBLE");
  }

  void WaitForViewportToStabilize(FrameTreeNode* node) {
    RunCommandAndWaitForResponse(node, "notifyWhenViewportStable(0);",
                                 "VIEWPORT_STABLE");
  }

  void AddFocusedInputField(FrameTreeNode* node) {
    ASSERT_TRUE(ExecuteScript(node, "addFocusedInputField();"));
  }

  void SetWindowScroll(FrameTreeNode* node, int x, int y) {
    ASSERT_TRUE(ExecuteScript(
        node, base::StringPrintf("window.scrollTo(%d, %d);", x, y)));
  }

  // Helper function to retrieve the bounding client rect of the element
  // identified by |sel| inside |rfh|.
  gfx::Rect GetBoundingClientRect(FrameTreeNode* node, const std::string& sel) {
    std::string result;
    EXPECT_TRUE(ExecuteScriptAndExtractString(
        node,
        base::StringPrintf(
            "window.domAutomationController.send(rectAsString("
            "    document.querySelector('%s').getBoundingClientRect()));",
            sel.c_str()),
        &result));
    return GetRectFromString(result);
  }

  // Returns a rect representing the current |visualViewport| in the main frame
  // of |contents|.
  gfx::Rect GetVisualViewport(FrameTreeNode* node) {
    std::string result;
    EXPECT_TRUE(ExecuteScriptAndExtractString(
        node,
        "window.domAutomationController.send("
        "    rectAsString(visualViewportAsRect()));",
        &result));
    return GetRectFromString(result);
  }

  float GetVisualViewportScale(FrameTreeNode* node) {
    double scale;
    EXPECT_TRUE(ExecuteScriptAndExtractDouble(
        node, "window.domAutomationController.send(visualViewport.scale);",
        &scale));
    return static_cast<float>(scale);
  }

 private:
  void RunCommandAndWaitForResponse(FrameTreeNode* node,
                                    const std::string& command,
                                    const std::string& response) {
    std::string msg_from_renderer;
    ASSERT_TRUE(
        ExecuteScriptAndExtractString(node, command, &msg_from_renderer));
    ASSERT_EQ(response, msg_from_renderer);
  }

  gfx::Rect GetRectFromString(const std::string& str) {
    std::vector<std::string> tokens = base::SplitString(
        str, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    EXPECT_EQ(4U, tokens.size());
    double x = 0.0, y = 0.0, width = 0.0, height = 0.0;
    EXPECT_TRUE(base::StringToDouble(tokens[0], &x));
    EXPECT_TRUE(base::StringToDouble(tokens[1], &y));
    EXPECT_TRUE(base::StringToDouble(tokens[2], &width));
    EXPECT_TRUE(base::StringToDouble(tokens[3], &height));
    return {static_cast<int>(x), static_cast<int>(y), static_cast<int>(width),
            static_cast<int>(height)};
  }

  DISALLOW_COPY_AND_ASSIGN(SitePerProcessProgrammaticScrollTest);
};

IN_PROC_BROWSER_TEST_P(SitePerProcessHighDPIBrowserTest,
                       SubframeLoadsWithCorrectDeviceScaleFactor) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // On Android forcing device scale factor does not work for tests, therefore
  // we ensure that make frame and iframe have the same DIP scale there, but
  // not necessarily kDeviceScaleFactor.
  const double expected_dip_scale =
#if defined(OS_ANDROID)
      GetFrameDeviceScaleFactor(web_contents());
#else
      SitePerProcessHighDPIBrowserTest::kDeviceScaleFactor;
#endif

  EXPECT_EQ(expected_dip_scale, GetFrameDeviceScaleFactor(web_contents()));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  EXPECT_EQ(expected_dip_scale, GetFrameDeviceScaleFactor(root));
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child = root->child_at(0);
  EXPECT_EQ(expected_dip_scale, GetFrameDeviceScaleFactor(child));
}

#if defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       SubframeUpdateToCorrectDeviceScaleFactor) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  EXPECT_EQ(1.0, GetFrameDeviceScaleFactor(web_contents()));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child = root->child_at(0);
  EXPECT_EQ(1.0, GetFrameDeviceScaleFactor(child));

  double expected_dip_scale = 2.0;

  // TODO(oshima): allow DeviceScaleFactor change on other platforms
  // (win, linux, mac, android and mus).
  aura::TestScreen* test_screen =
      static_cast<aura::TestScreen*>(display::Screen::GetScreen());
  test_screen->CreateHostForPrimaryDisplay();
  test_screen->SetDeviceScaleFactor(expected_dip_scale);

  // This forces |expected_dip_scale| to be applied to the aura::WindowTreeHost
  // and aura::Window.
  aura::WindowTreeHost* window_tree_host = shell()->window()->GetHost();
  window_tree_host->SetBoundsInPixels(window_tree_host->GetBoundsInPixels());

  double device_scale_factor = 0;
  // Wait until dppx becomes 2 if the frame's dpr hasn't beeen updated
  // to 2 yet.
  const char kScript[] =
      "function sendDpr() "
      "{window.domAutomationController.send(window.devicePixelRatio);}; "
      "if (window.devicePixelRatio == 2) sendDpr();"
      "window.matchMedia('screen and "
      "(min-resolution: 2dppx)').addListener(function(e) { if (e.matches) { "
      "sendDpr();}})";
  // Make sure that both main frame and iframe are updated to 2x.
  EXPECT_TRUE(
      ExecuteScriptAndExtractDouble(child, kScript, &device_scale_factor));
  EXPECT_EQ(expected_dip_scale, device_scale_factor);

  device_scale_factor = 0;
  EXPECT_TRUE(ExecuteScriptAndExtractDouble(web_contents(), kScript,
                                            &device_scale_factor));
  EXPECT_EQ(expected_dip_scale, device_scale_factor);
}

#endif

class TextAutosizerPageInfoInterceptor
    : public blink::mojom::LocalMainFrameHostInterceptorForTesting {
 public:
  explicit TextAutosizerPageInfoInterceptor(
      RenderFrameHostImpl* render_frame_host)
      : render_frame_host_(render_frame_host) {
    render_frame_host_->local_main_frame_host_receiver_for_testing()
        .SwapImplForTesting(this);
  }

  ~TextAutosizerPageInfoInterceptor() override = default;

  LocalMainFrameHost* GetForwardingInterface() override {
    return render_frame_host_;
  }

  void WaitForPageInfo(base::Optional<int> target_main_frame_width,
                       base::Optional<float> target_device_scale_adjustment) {
    if (remote_page_info_seen_)
      return;
    target_main_frame_width_ = target_main_frame_width;
    target_device_scale_adjustment_ = target_device_scale_adjustment;
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    run_loop_.reset();
  }

  const blink::mojom::TextAutosizerPageInfo& GetTextAutosizerPageInfo() {
    return *remote_page_info_;
  }

  void TextAutosizerPageInfoChanged(
      blink::mojom::TextAutosizerPageInfoPtr remote_page_info) override {
    if ((!target_main_frame_width_ ||
         remote_page_info->main_frame_width != target_main_frame_width_) &&
        (!target_device_scale_adjustment_ ||
         remote_page_info->device_scale_adjustment !=
             target_device_scale_adjustment_)) {
      return;
    }
    remote_page_info_ = remote_page_info.Clone();
    remote_page_info_seen_ = true;
    if (run_loop_)
      run_loop_->Quit();
    GetForwardingInterface()->TextAutosizerPageInfoChanged(
        std::move(remote_page_info));
  }

 private:
  RenderFrameHostImpl* render_frame_host_;
  bool remote_page_info_seen_ = false;
  blink::mojom::TextAutosizerPageInfoPtr remote_page_info_ =
      blink::mojom::TextAutosizerPageInfo::New(/*main_frame_width=*/0,
                                               /*main_frame_layout_width=*/0,
                                               /*device_scale_adjustment=*/1.f);
  std::unique_ptr<base::RunLoop> run_loop_;
  base::Optional<int> target_main_frame_width_;
  base::Optional<float> target_device_scale_adjustment_;
};

// TODO(tonikitoo): Move to fake_remote_frame.h|cc in case it is useful
// for other tests.
class FakeRemoteMainFrame : public blink::mojom::RemoteMainFrame {
 public:
  FakeRemoteMainFrame() = default;
  ~FakeRemoteMainFrame() override = default;

  void Init(blink::AssociatedInterfaceProvider* provider) {
    provider->OverrideBinderForTesting(
        blink::mojom::RemoteMainFrame::Name_,
        base::BindRepeating(&FakeRemoteMainFrame::BindFrameHostReceiver,
                            base::Unretained(this)));
  }

  // blink::mojom::RemoteMainFrame overrides:
  void UpdateTextAutosizerPageInfo(
      blink::mojom::TextAutosizerPageInfoPtr page_info) override {}

 private:
  void BindFrameHostReceiver(mojo::ScopedInterfaceEndpointHandle handle) {
    receiver_.Bind(
        mojo::PendingAssociatedReceiver<blink::mojom::RemoteMainFrame>(
            std::move(handle)));
  }

  mojo::AssociatedReceiver<blink::mojom::RemoteMainFrame> receiver_{this};
};

// This class intercepts RenderFrameProxyHost creations, and overrides their
// respective blink::mojom::RemoteMainFrame instances, so that it can watch for
// text autosizer page info updates.
class UpdateTextAutosizerInfoProxyObserver {
 public:
  UpdateTextAutosizerInfoProxyObserver() {
    RenderFrameProxyHost::SetCreatedCallbackForTesting(
        base::BindRepeating(&UpdateTextAutosizerInfoProxyObserver::
                                RenderFrameProxyHostCreatedCallback,
                            base::Unretained(this)));
  }
  ~UpdateTextAutosizerInfoProxyObserver() {
    RenderFrameProxyHost::SetCreatedCallbackForTesting(
        RenderFrameProxyHost::CreatedCallback());
  }

  const blink::mojom::TextAutosizerPageInfo& TextAutosizerPageInfo(
      RenderFrameProxyHost* proxy) {
    return remote_frames_[proxy]->page_info();
  }

 private:
  class Remote : public FakeRemoteMainFrame {
   public:
    explicit Remote(RenderFrameProxyHost* proxy) {
      Init(proxy->GetRemoteAssociatedInterfacesTesting());
    }
    void UpdateTextAutosizerPageInfo(
        blink::mojom::TextAutosizerPageInfoPtr page_info) override {
      page_info_ = *page_info;
    }
    const blink::mojom::TextAutosizerPageInfo& page_info() {
      return page_info_;
    }

   private:
    blink::mojom::TextAutosizerPageInfo page_info_;
  };

  void RenderFrameProxyHostCreatedCallback(RenderFrameProxyHost* proxy_host) {
    remote_frames_[proxy_host] = std::make_unique<Remote>(proxy_host);
  }

  std::map<RenderFrameProxyHost*, std::unique_ptr<Remote>> remote_frames_;
};

// Make sure that when a relevant feature of the main frame changes, e.g. the
// frame width, that the browser is notified.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, TextAutosizerPageInfo) {
  UpdateTextAutosizerInfoProxyObserver update_text_autosizer_info_observer;

  blink::web_pref::WebPreferences prefs =
      web_contents()->GetOrCreateWebPreferences();
  prefs.text_autosizing_enabled = true;

  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  web_contents()->SetWebPreferences(prefs);

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* b_child = root->child_at(0);

  blink::mojom::TextAutosizerPageInfo received_page_info;
  auto interceptor = std::make_unique<TextAutosizerPageInfoInterceptor>(
      web_contents()->GetMainFrame());
#if defined(OS_ANDROID)
  prefs.device_scale_adjustment += 0.05f;
  // Change the device scale adjustment to trigger a RemotePageInfo update.
  web_contents()->SetWebPreferences(prefs);
  // Make sure we receive a ViewHostMsg from the main frame's renderer.
  interceptor->WaitForPageInfo(base::Optional<int>(),
                               prefs.device_scale_adjustment);
  // Make sure the correct page message is sent to the child.
  base::RunLoop().RunUntilIdle();
  received_page_info = interceptor->GetTextAutosizerPageInfo();
  EXPECT_EQ(prefs.device_scale_adjustment,
            received_page_info.device_scale_adjustment);
#else
  // Resize the main frame, then wait to observe that the RemotePageInfo message
  // arrives.
  auto* view = web_contents()->GetRenderWidgetHostView();
  gfx::Rect old_bounds = view->GetViewBounds();
  gfx::Rect new_bounds(
      old_bounds.origin(),
      gfx::Size(old_bounds.width() - 20, old_bounds.height() - 20));

  view->SetBounds(new_bounds);
  // Make sure we receive a ViewHostMsg from the main frame's renderer.
  interceptor->WaitForPageInfo(new_bounds.width(), base::Optional<float>());
  // Make sure the correct page message is sent to the child.
  base::RunLoop().RunUntilIdle();
  received_page_info = interceptor->GetTextAutosizerPageInfo();
  EXPECT_EQ(new_bounds.width(), received_page_info.main_frame_width);
#endif  // defined(OS_ANDROID)

  // Dynamically create a new, cross-process frame to test sending the cached
  // TextAutosizerPageInfo.

  GURL c_url = embedded_test_server()->GetURL("c.com", "/title1.html");
  // The following is a hack so we can get an IPC watcher connected to the
  // RenderProcessHost for C before the RenderView is created for it, and the
  // TextAutosizerPageInfo IPC is sent to it.
  scoped_refptr<SiteInstance> c_site =
      web_contents()->GetSiteInstance()->GetRelatedSiteInstance(c_url);
  // Force creation of a render process for c's SiteInstance, this will get
  // used when we dynamically create the new frame.
  auto* c_rph = static_cast<RenderProcessHostImpl*>(c_site->GetProcess());
  ASSERT_TRUE(c_rph);
  ASSERT_NE(c_rph, root->current_frame_host()->GetProcess());
  ASSERT_NE(c_rph, b_child->current_frame_host()->GetProcess());

  // Create the subframe now.
  std::string create_frame_script = base::StringPrintf(
      "var new_iframe = document.createElement('iframe');"
      "new_iframe.src = '%s';"
      "document.body.appendChild(new_iframe);",
      c_url.spec().c_str());
  EXPECT_TRUE(ExecuteScript(root, create_frame_script));
  ASSERT_EQ(2U, root->child_count());

  // Ensure IPC is sent.
  base::RunLoop().RunUntilIdle();
  blink::mojom::TextAutosizerPageInfo page_info_sent_to_remote_main_frames =
      update_text_autosizer_info_observer.TextAutosizerPageInfo(
          web_contents()
              ->GetRenderManager()
              ->GetAllProxyHostsForTesting()
              .begin()
              ->second.get());

  EXPECT_EQ(received_page_info.main_frame_width,
            page_info_sent_to_remote_main_frames.main_frame_width);
  EXPECT_EQ(received_page_info.main_frame_layout_width,
            page_info_sent_to_remote_main_frames.main_frame_layout_width);
  EXPECT_EQ(received_page_info.device_scale_adjustment,
            page_info_sent_to_remote_main_frames.device_scale_adjustment);
}

// Ensure that navigating subframes in --site-per-process mode works and the
// correct documents are committed.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, CrossSiteIframe) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a,a(a,a(a)))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  TestNavigationObserver observer(shell()->web_contents());

  // Load same-site page into iframe.
  FrameTreeNode* child = root->child_at(0);
  GURL http_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  NavigateFrameToURL(child, http_url);
  EXPECT_EQ(http_url, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());
  {
    // There should be only one RenderWidgetHost when there are no
    // cross-process iframes.
    std::set<RenderWidgetHostView*> views_set =
        web_contents()->GetRenderWidgetHostViewsInTree();
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
    NavigateFrameToURL(root->child_at(0), url);
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
  EXPECT_NE(shell()->web_contents()->GetRenderViewHost(), rvh);
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(), site_instance);
  EXPECT_NE(shell()->web_contents()->GetMainFrame()->GetProcess(), rph);
  {
    // There should be now two RenderWidgetHosts, one for each process
    // rendering a frame.
    std::set<RenderWidgetHostView*> views_set =
        web_contents()->GetRenderWidgetHostViewsInTree();
    EXPECT_EQ(2U, views_set.size());
  }
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
    NavigateFrameToURL(root->child_at(0), url);
    deleted_observer.WaitUntilDeleted();
  }
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(url, observer.last_navigation_url());

  // Check again that a new process is created and is different from the
  // top level one and the previous one.
  ASSERT_EQ(2U, root->child_count());
  child = root->child_at(0);
  EXPECT_NE(shell()->web_contents()->GetRenderViewHost(),
            child->current_frame_host()->render_view_host());
  EXPECT_NE(rvh, child->current_frame_host()->render_view_host());
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_NE(site_instance,
            child->current_frame_host()->GetSiteInstance());
  EXPECT_NE(shell()->web_contents()->GetMainFrame()->GetProcess(),
            child->current_frame_host()->GetProcess());
  EXPECT_NE(rph, child->current_frame_host()->GetProcess());
  {
    std::set<RenderWidgetHostView*> views_set =
        web_contents()->GetRenderWidgetHostViewsInTree();
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

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Make the main frame update its title after the subframe loads.
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(),
                            "document.querySelector('iframe').onload = "
                            "    function() { document.title = 'loaded'; };"));
  EXPECT_TRUE(
      ExecuteScript(shell()->web_contents(), "document.title = 'not loaded';"));
  base::string16 expected_title(base::UTF8ToUTF16("loaded"));
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);

  // Navigate the iframe cross-site.
  TestNavigationObserver load_observer(shell()->web_contents());
  GURL frame_url = embedded_test_server()->GetURL("b.com", "/title2.html");
  EXPECT_TRUE(ExecuteScript(root->child_at(0)->current_frame_host(),
                            JsReplace("window.location.href = $1", frame_url)));
  load_observer.Wait();

  // Wait for the title to update and ensure it affects the right NavEntry.
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  NavigationEntry* entry =
      shell()->web_contents()->GetController().GetLastCommittedEntry();
  EXPECT_EQ(expected_title, entry->GetTitle());
}

// Test that the physical backing size and view bounds for a scaled out-of-
// process iframe are set and updated correctly.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       CompositorViewportPixelSizeTest) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_scaled_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* parent_iframe_node = root->child_at(0);

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site A ------- proxies for B\n"
      "        +--Site B -- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://baz.com/",
      DepictFrameTree(root));

  FrameTreeNode* nested_iframe_node = parent_iframe_node->child_at(0);
  RenderFrameProxyHost* proxy_to_parent =
      nested_iframe_node->render_manager()->GetProxyToParent();
  CrossProcessFrameConnector* connector =
      proxy_to_parent->cross_process_frame_connector();
  RenderWidgetHostViewBase* rwhv_nested =
      static_cast<RenderWidgetHostViewBase*>(
          nested_iframe_node->current_frame_host()
              ->GetRenderWidgetHost()
              ->GetView());

  RenderFrameSubmissionObserver frame_observer(nested_iframe_node);
  frame_observer.WaitForMetadataChange();

  // Verify that applying a CSS scale transform does not impact the size of the
  // content of the nested iframe.
  // The screen_space_rect_in_dip may be off by 1 due to rounding. There is no
  // good way to avoid this due to various device-scale-factor. (e.g. when
  // dsf=3.375, ceil(round(50 * 3.375) / 3.375) = 51. Thus, we allow the screen
  // size in dip to be off by 1 here.
  EXPECT_NEAR(50, connector->screen_space_rect_in_dip().size().width(), 1);
  EXPECT_NEAR(50, connector->screen_space_rect_in_dip().size().height(), 1);
  EXPECT_EQ(gfx::Size(100, 100), rwhv_nested->GetViewBounds().size());
  EXPECT_EQ(gfx::Size(100, 100), connector->local_frame_size_in_dip());
  EXPECT_EQ(connector->local_frame_size_in_pixels(),
            rwhv_nested->GetCompositorViewportPixelSize());
}

// Verify an OOPIF resize handler doesn't fire immediately after load without
// the frame having been resized. See https://crbug.com/826457.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, NoResizeAfterIframeLoad) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  FrameTreeNode* iframe = root->child_at(0);
  GURL site_url =
      embedded_test_server()->GetURL("b.com", "/page_with_resize_handler.html");
  NavigateFrameToURL(iframe, site_url);
  base::RunLoop().RunUntilIdle();

  int resizes = -1;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      iframe->current_frame_host(),
      "window.domAutomationController.send(resize_count);", &resizes));

  // Should be zero because the iframe only has its initial size from parent.
  EXPECT_EQ(resizes, 0);
}

// Test that the view bounds for an out-of-process iframe are set and updated
// correctly, including accounting for local frame offsets in the parent and
// scroll positions.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, ViewBoundsInNestedFrameTest) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  RenderWidgetHostViewBase* rwhv_root = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* parent_iframe_node = root->child_at(0);
  GURL site_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_positioned_frame.html"));
  NavigateFrameToURL(parent_iframe_node, site_url);
  RenderFrameSubmissionObserver frame_observer(shell()->web_contents());

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site A ------- proxies for B\n"
      "        +--Site B -- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://baz.com/",
      DepictFrameTree(root));

  FrameTreeNode* nested_iframe_node = parent_iframe_node->child_at(0);
  RenderWidgetHostViewBase* rwhv_nested =
      static_cast<RenderWidgetHostViewBase*>(
          nested_iframe_node->current_frame_host()
              ->GetRenderWidgetHost()
              ->GetView());
  WaitForHitTestData(nested_iframe_node->current_frame_host());

  float scale_factor =
      frame_observer.LastRenderFrameMetadata().page_scale_factor;

  // Get the view bounds of the nested iframe, which should account for the
  // relative offset of its direct parent within the root frame.
  gfx::Rect bounds = rwhv_nested->GetViewBounds();

  auto filter =
      base::MakeRefCounted<SynchronizeVisualPropertiesMessageFilter>();
  root->current_frame_host()->GetProcess()->AddFilter(filter.get());

  // Scroll the parent frame downward to verify that the child rect gets updated
  // correctly.
  blink::WebMouseWheelEvent scroll_event(
      blink::WebInputEvent::Type::kMouseWheel,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());

  scroll_event.SetPositionInWidget(
      std::floor((bounds.x() - rwhv_root->GetViewBounds().x() - 5) *
                 scale_factor),
      std::floor((bounds.y() - rwhv_root->GetViewBounds().y() - 5) *
                 scale_factor));
  scroll_event.delta_x = 0.0f;
  scroll_event.delta_y = -30.0f;
  scroll_event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
  rwhv_root->ProcessMouseWheelEvent(scroll_event, ui::LatencyInfo());
  filter->WaitForRect();

  // The precise amount of scroll for the first view position update is not
  // deterministic, so this simply verifies that the OOPIF moved from its
  // earlier position.
  gfx::Rect update_rect = filter->last_rect();
  EXPECT_LT(update_rect.y(), bounds.y() - rwhv_root->GetViewBounds().y());
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
                            ->GetFrameTree()
                            ->root();
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
  gesture_scroll_update.data.scroll_update.velocity_y = 5.f;

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

// Test that scrolling a nested out-of-process iframe bubbles unused scroll
// delta to a parent frame.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, ScrollBubblingFromOOPIFTest) {
  ui::GestureConfiguration::GetInstance()->set_scroll_debounce_interval_in_ms(
      0);
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* parent_iframe_node = root->child_at(0);

  // This test uses the position of the nested iframe within the parent iframe
  // to infer the scroll position of the parent.
  // SynchronizeVisualPropertiesMessageFilter catches updates to the position in
  // order to avoid busy waiting. It gets created early to catch the initial
  // rects from the navigation.
  auto filter =
      base::MakeRefCounted<SynchronizeVisualPropertiesMessageFilter>();
  parent_iframe_node->current_frame_host()->GetProcess()->AddFilter(
      filter.get());

  GURL site_url(embedded_test_server()->GetURL(
      "b.com", "/frame_tree/page_with_positioned_frame.html"));
  NavigateFrameToURL(parent_iframe_node, site_url);

  InputEventAckWaiter ack_observer(
      parent_iframe_node->current_frame_host()->GetRenderWidgetHost(),
      blink::WebInputEvent::Type::kGestureScrollEnd);

  // Navigate the nested frame to a page large enough to have scrollbars.
  FrameTreeNode* nested_iframe_node = parent_iframe_node->child_at(0);
  GURL nested_site_url(embedded_test_server()->GetURL(
      "baz.com", "/tall_page.html"));
  NavigateFrameToURL(nested_iframe_node, nested_site_url);

  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   +--Site B ------- proxies for A C\n"
      "        +--Site C -- proxies for A B\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/\n"
      "      C = http://baz.com/",
      DepictFrameTree(root));

  RenderWidgetHostViewBase* rwhv_parent =
      static_cast<RenderWidgetHostViewBase*>(
          parent_iframe_node->current_frame_host()
              ->GetRenderWidgetHost()
              ->GetView());

  RenderWidgetHostViewBase* rwhv_nested =
      static_cast<RenderWidgetHostViewBase*>(
          nested_iframe_node->current_frame_host()
              ->GetRenderWidgetHost()
              ->GetView());

  WaitForHitTestData(nested_iframe_node->current_frame_host());

  // Save the original offset as a point of reference.
  filter->WaitForRect();
  gfx::Rect update_rect = filter->last_rect();
  int initial_y = update_rect.y();
  filter->ResetRectRunLoop();

  // Scroll the parent frame downward.
  blink::WebMouseWheelEvent scroll_event(
      blink::WebInputEvent::Type::kMouseWheel,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  scroll_event.SetPositionInWidget(1, 1);
  // Use precise pixels to keep these events off the animated scroll pathways,
  // which currently break this test.
  // https://bugs.chromium.org/p/chromium/issues/detail?id=710513
  scroll_event.delta_units = ui::ScrollGranularity::kScrollByPrecisePixel;
  scroll_event.delta_x = 0.0f;
  scroll_event.delta_y = -5.0f;
  scroll_event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
  rwhv_parent->ProcessMouseWheelEvent(scroll_event, ui::LatencyInfo());

  // The event router sends wheel events of a single scroll sequence to the
  // target under the first wheel event. Send a wheel end event to the current
  // target view before sending a wheel event to a different one.
  scroll_event.delta_y = 0.0f;
  scroll_event.phase = blink::WebMouseWheelEvent::kPhaseEnded;
  scroll_event.dispatch_type =
      blink::WebInputEvent::DispatchType::kEventNonBlocking;
  rwhv_parent->ProcessMouseWheelEvent(scroll_event, ui::LatencyInfo());

  // Ensure that the view position is propagated to the child properly.
  filter->WaitForRect();
  update_rect = filter->last_rect();
  EXPECT_LT(update_rect.y(), initial_y);
  filter->ResetRectRunLoop();
  ack_observer.Reset();

  // Now scroll the nested frame upward, which should bubble to the parent.
  // The upscroll exceeds the amount that the frame was initially scrolled
  // down to account for rounding.
  scroll_event.delta_y = 6.0f;
  scroll_event.dispatch_type = blink::WebInputEvent::DispatchType::kBlocking;
  scroll_event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
  rwhv_nested->ProcessMouseWheelEvent(scroll_event, ui::LatencyInfo());

  filter->WaitForRect();
  // This loop isn't great, but it accounts for the possibility of multiple
  // incremental updates happening as a result of the scroll animation.
  // A failure condition of this test is that the loop might not terminate
  // due to bubbling not working properly. If the overscroll bubbles to the
  // parent iframe then the nested frame's y coord will return to its
  // initial position.
  update_rect = filter->last_rect();
  while (update_rect.y() > initial_y) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
    update_rect = filter->last_rect();
  }

  // The event router sends wheel events of a single scroll sequence to the
  // target under the first wheel event. Send a wheel end event to the current
  // target view before sending a wheel event to a different one.
  scroll_event.delta_y = 0.0f;
  scroll_event.phase = blink::WebMouseWheelEvent::kPhaseEnded;
  scroll_event.dispatch_type =
      blink::WebInputEvent::DispatchType::kEventNonBlocking;
  rwhv_nested->ProcessMouseWheelEvent(scroll_event, ui::LatencyInfo());

  filter->ResetRectRunLoop();
  // Once we've sent a wheel to the nested iframe that we expect to turn into
  // a bubbling scroll, we need to delay to make sure the GestureScrollBegin
  // from this new scroll doesn't hit the RenderWidgetHostImpl before the
  // GestureScrollEnd bubbled from the child.
  // This timing only seems to be needed for CrOS, but we'll enable it on
  // all platforms just to lessen the possibility of tests being flakey
  // on non-CrOS platforms.
  ack_observer.Wait();

  // Scroll the parent down again in order to test scroll bubbling from
  // gestures.
  scroll_event.delta_y = -5.0f;
  scroll_event.dispatch_type = blink::WebInputEvent::DispatchType::kBlocking;
  scroll_event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
  rwhv_parent->ProcessMouseWheelEvent(scroll_event, ui::LatencyInfo());

  // The event router sends wheel events of a single scroll sequence to the
  // target under the first wheel event. Send a wheel end event to the current
  // target view before sending a wheel event to a different one.
  scroll_event.delta_y = 0.0f;
  scroll_event.phase = blink::WebMouseWheelEvent::kPhaseEnded;
  scroll_event.dispatch_type =
      blink::WebInputEvent::DispatchType::kEventNonBlocking;
  rwhv_parent->ProcessMouseWheelEvent(scroll_event, ui::LatencyInfo());

  // Ensure ensuing offset change is received, and then reset the filter.
  filter->WaitForRect();
  filter->ResetRectRunLoop();

  // Scroll down the nested iframe via gesture. This requires 3 separate input
  // events.
  blink::WebGestureEvent gesture_event(
      blink::WebGestureEvent::Type::kGestureScrollBegin,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests(),
      blink::WebGestureDevice::kTouchpad);
  gesture_event.SetPositionInWidget(gfx::PointF(1, 1));
  gesture_event.data.scroll_begin.delta_x_hint = 0.0f;
  gesture_event.data.scroll_begin.delta_y_hint = 6.0f;
  rwhv_nested->GetRenderWidgetHost()->ForwardGestureEvent(gesture_event);

  gesture_event =
      blink::WebGestureEvent(blink::WebGestureEvent::Type::kGestureScrollUpdate,
                             blink::WebInputEvent::kNoModifiers,
                             blink::WebInputEvent::GetStaticTimeStampForTests(),
                             blink::WebGestureDevice::kTouchpad);
  gesture_event.SetPositionInWidget(gfx::PointF(1, 1));
  gesture_event.data.scroll_update.delta_x = 0.0f;
  gesture_event.data.scroll_update.delta_y = 6.0f;
  gesture_event.data.scroll_update.velocity_x = 0;
  gesture_event.data.scroll_update.velocity_y = 0;
  rwhv_nested->GetRenderWidgetHost()->ForwardGestureEvent(gesture_event);

  gesture_event =
      blink::WebGestureEvent(blink::WebGestureEvent::Type::kGestureScrollEnd,
                             blink::WebInputEvent::kNoModifiers,
                             blink::WebInputEvent::GetStaticTimeStampForTests(),
                             blink::WebGestureDevice::kTouchpad);
  gesture_event.SetPositionInWidget(gfx::PointF(1, 1));
  rwhv_nested->GetRenderWidgetHost()->ForwardGestureEvent(gesture_event);

  filter->WaitForRect();
  update_rect = filter->last_rect();
  // As above, if this loop does not terminate then it indicates an issue
  // with scroll bubbling.
  while (update_rect.y() > initial_y) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
    update_rect = filter->last_rect();
  }

  // Test that when the child frame absorbs all of the scroll delta, it does
  // not propagate to the parent (see https://crbug.com/621624).
  filter->ResetRectRunLoop();
  scroll_event.delta_y = -5.0f;
  scroll_event.dispatch_type = blink::WebInputEvent::DispatchType::kBlocking;
  scroll_event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
  rwhv_nested->ProcessMouseWheelEvent(scroll_event, ui::LatencyInfo());
  // It isn't possible to busy loop waiting on the renderer here because we
  // are explicitly testing that something does *not* happen. This creates a
  // small chance of false positives but shouldn't result in false negatives,
  // so flakiness implies this test is failing.
  {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::action_timeout());
    run_loop.Run();
  }
  DCHECK_EQ(filter->last_rect().x(), 0);
  DCHECK_EQ(filter->last_rect().y(), 0);
}

// Tests that scrolling with the keyboard will bubble unused scroll to the
// OOPIF's parent.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       KeyboardScrollBubblingFromOOPIF) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_iframe_in_scrollable_div.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* iframe_node = root->child_at(0);

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  RenderWidgetHostViewBase* rwhv_child = static_cast<RenderWidgetHostViewBase*>(
      iframe_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  // This test does not involve hit testing, but input events could be dropped
  // by the renderer before the first compositor commit, so we wait here anyway
  // to avoid that.
  WaitForHitTestData(iframe_node->current_frame_host());

  double initial_y = 0.0;
  ASSERT_TRUE(content::ExecuteScriptAndExtractDouble(
      root,
      "var wrapperDiv = document.getElementById('wrapper-div');"
      "var initial_y = wrapperDiv.scrollTop;"
      "var waitForScrollDownPromise = new Promise(function(resolve) {"
      "  wrapperDiv.addEventListener('scroll', () => {"
      "    if (wrapperDiv.scrollTop > initial_y)"
      "      resolve(wrapperDiv.scrollTop);"
      "  });"
      "});"
      "window.domAutomationController.send(initial_y);",
      &initial_y));
  EXPECT_DOUBLE_EQ(0.0, initial_y);

  NativeWebKeyboardEvent key_event(
      blink::WebKeyboardEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  key_event.windows_key_code = ui::VKEY_DOWN;
  key_event.native_key_code =
      ui::KeycodeConverter::DomCodeToNativeKeycode(ui::DomCode::ARROW_DOWN);
  key_event.dom_code = static_cast<int>(ui::DomCode::ARROW_DOWN);
  key_event.dom_key = ui::DomKey::ARROW_DOWN;

  rwhv_child->GetRenderWidgetHost()->ForwardKeyboardEvent(key_event);

  key_event.SetType(blink::WebKeyboardEvent::Type::kKeyUp);
  rwhv_child->GetRenderWidgetHost()->ForwardKeyboardEvent(key_event);

  double scrolled_y = 0.0;
  ASSERT_TRUE(content::ExecuteScriptAndExtractDouble(
      root,
      "waitForScrollDownPromise.then((scrolled_y) => {"
      "  window.domAutomationController.send(scrolled_y);"
      "});",
      &scrolled_y));
  EXPECT_GT(scrolled_y, 0.0);
}

// Test that fling on an out-of-process iframe progresses properly.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       TouchscreenGestureFlingStart) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
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
                            ->GetFrameTree()
                            ->root();
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

class ScrollObserver : public RenderWidgetHost::InputEventObserver {
 public:
  ScrollObserver(double delta_x, double delta_y) { Reset(delta_x, delta_y); }
  ~ScrollObserver() override {}

  void OnInputEvent(const blink::WebInputEvent& event) override {
    if (event.GetType() == blink::WebInputEvent::Type::kGestureScrollUpdate) {
      blink::WebGestureEvent received_update =
          *static_cast<const blink::WebGestureEvent*>(&event);
      remaining_delta_x_ -= received_update.data.scroll_update.delta_x;
      remaining_delta_y_ -= received_update.data.scroll_update.delta_y;
    } else if (event.GetType() ==
               blink::WebInputEvent::Type::kGestureScrollEnd) {
      if (message_loop_runner_->loop_running())
        message_loop_runner_->Quit();
      DCHECK_EQ(0, remaining_delta_x_);
      DCHECK_EQ(0, remaining_delta_y_);
      scroll_end_received_ = true;
    }
  }

  void Wait() {
    if (!scroll_end_received_) {
      message_loop_runner_->Run();
    }
  }

  void Reset(double delta_x, double delta_y) {
    message_loop_runner_ = new content::MessageLoopRunner;
    remaining_delta_x_ = delta_x;
    remaining_delta_y_ = delta_y;
    scroll_end_received_ = false;
  }

 private:
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
  double remaining_delta_x_;
  double remaining_delta_y_;
  bool scroll_end_received_;

  DISALLOW_COPY_AND_ASSIGN(ScrollObserver);
};

// Android: crbug.com/825629
// NDEBUG: crbug.com/1063045
#if defined(OS_ANDROID) || defined(NDEBUG)
#define MAYBE_ScrollBubblingFromNestedOOPIFTest \
  DISABLED_ScrollBubblingFromNestedOOPIFTest
#else
#define MAYBE_ScrollBubblingFromNestedOOPIFTest \
  ScrollBubblingFromNestedOOPIFTest
#endif
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       MAYBE_ScrollBubblingFromNestedOOPIFTest) {
  ui::GestureConfiguration::GetInstance()->set_scroll_debounce_interval_in_ms(
      0);
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_nested_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  RenderFrameSubmissionObserver frame_observer(shell()->web_contents());

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* parent_iframe_node = root->child_at(0);
  GURL site_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_positioned_frame.html"));
  EXPECT_EQ(site_url, parent_iframe_node->current_url());

  FrameTreeNode* nested_iframe_node = parent_iframe_node->child_at(0);
  GURL nested_site_url(
      embedded_test_server()->GetURL("baz.com", "/title1.html"));
  EXPECT_EQ(nested_site_url, nested_iframe_node->current_url());

  RenderWidgetHostViewBase* root_view = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());

  RenderWidgetHostViewBase* rwhv_nested =
      static_cast<RenderWidgetHostViewBase*>(
          nested_iframe_node->current_frame_host()
              ->GetRenderWidgetHost()
              ->GetView());

  WaitForHitTestData(nested_iframe_node->current_frame_host());

  InputEventAckWaiter ack_observer(
      root->current_frame_host()->GetRenderWidgetHost(),
      blink::WebInputEvent::Type::kGestureScrollBegin);

  std::unique_ptr<ScrollObserver> scroll_observer;

  // All GSU events will be wrapped between a single GSB-GSE pair. The expected
  // delta value is equal to summation of all scroll update deltas.
  scroll_observer = std::make_unique<ScrollObserver>(0, 15);

  root->current_frame_host()->GetRenderWidgetHost()->AddInputEventObserver(
      scroll_observer.get());

  // Now scroll the nested frame upward, this must bubble all the way up to the
  // root.
  blink::WebMouseWheelEvent scroll_event(
      blink::WebInputEvent::Type::kMouseWheel,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  gfx::Rect bounds = rwhv_nested->GetViewBounds();
  float scale_factor =
      frame_observer.LastRenderFrameMetadata().page_scale_factor;
  scroll_event.SetPositionInWidget(
      std::ceil((bounds.x() - root_view->GetViewBounds().x() + 10) *
                scale_factor),
      std::ceil((bounds.y() - root_view->GetViewBounds().y() + 10) *
                scale_factor));
  scroll_event.delta_units = ui::ScrollGranularity::kScrollByPrecisePixel;
  scroll_event.delta_x = 0.0f;
  scroll_event.delta_y = 5.0f;
  scroll_event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
  rwhv_nested->ProcessMouseWheelEvent(scroll_event, ui::LatencyInfo());
  ack_observer.Wait();

  // Send 10 wheel events with delta_y = 1 to the nested oopif.
  scroll_event.delta_y = 1.0f;
  scroll_event.phase = blink::WebMouseWheelEvent::kPhaseChanged;
  for (int i = 0; i < 10; i++)
    rwhv_nested->ProcessMouseWheelEvent(scroll_event, ui::LatencyInfo());

  // Send a wheel end event to complete the scrolling sequence.
  scroll_event.delta_y = 0.0f;
  scroll_event.phase = blink::WebMouseWheelEvent::kPhaseEnded;
  rwhv_nested->ProcessMouseWheelEvent(scroll_event, ui::LatencyInfo());
  scroll_observer->Wait();
}

// Tests that scrolling bubbles from an oopif if its source body has
// "overflow:hidden" style.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       ScrollBubblingFromOOPIFWithBodyOverflowHidden) {
  GURL url_domain_a(embedded_test_server()->GetURL(
      "a.com", "/scrollable_page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_domain_a));
  RenderFrameSubmissionObserver frame_observer(shell()->web_contents());
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  FrameTreeNode* iframe_node = root->child_at(0);
  GURL url_domain_b(
      embedded_test_server()->GetURL("b.com", "/body_overflow_hidden.html"));
  NavigateFrameToURL(iframe_node, url_domain_b);
  WaitForHitTestData(iframe_node->current_frame_host());

  RenderWidgetHostViewBase* root_view = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());

  RenderWidgetHostViewBase* child_view = static_cast<RenderWidgetHostViewBase*>(
      iframe_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  ScrollObserver scroll_observer(0, -5);
  root->current_frame_host()->GetRenderWidgetHost()->AddInputEventObserver(
      &scroll_observer);

  // Now scroll the nested frame downward, this must bubble to the root since
  // the iframe source body is not scrollable.
  blink::WebMouseWheelEvent scroll_event(
      blink::WebInputEvent::Type::kMouseWheel,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  gfx::Rect bounds = child_view->GetViewBounds();
  float scale_factor =
      frame_observer.LastRenderFrameMetadata().page_scale_factor;
  scroll_event.SetPositionInWidget(
      std::ceil((bounds.x() - root_view->GetViewBounds().x() + 10) *
                scale_factor),
      std::ceil((bounds.y() - root_view->GetViewBounds().y() + 10) *
                scale_factor));
  scroll_event.delta_units = ui::ScrollGranularity::kScrollByPrecisePixel;
  scroll_event.delta_x = 0.0f;
  scroll_event.delta_y = -5.0f;
  scroll_event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
  child_view->ProcessMouseWheelEvent(scroll_event, ui::LatencyInfo());

  // Send a wheel end event to complete the scrolling sequence.
  scroll_event.delta_y = 0.0f;
  scroll_event.phase = blink::WebMouseWheelEvent::kPhaseEnded;
  child_view->ProcessMouseWheelEvent(scroll_event, ui::LatencyInfo());

  scroll_observer.Wait();
}

// Ensure that the scrollability of a local subframe in an OOPIF is considered
// when acknowledging GestureScrollBegin events sent to OOPIFs.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, ScrollLocalSubframeInOOPIF) {
  ui::GestureConfiguration::GetInstance()->set_scroll_debounce_interval_in_ms(
      0);

  // This must be tall enough such that the outer iframe is not scrollable.
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_tall_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* parent_iframe_node = root->child_at(0);
  GURL outer_frame_url(embedded_test_server()->GetURL(
      "baz.com", "/frame_tree/page_with_positioned_frame.html"));
  NavigateFrameToURL(parent_iframe_node, outer_frame_url);

  // This must be tall enough such that the inner iframe is scrollable.
  FrameTreeNode* nested_iframe_node = parent_iframe_node->child_at(0);
  GURL inner_frame_url(
      embedded_test_server()->GetURL("baz.com", "/tall_page.html"));
  NavigateFrameToURL(nested_iframe_node, inner_frame_url);

  ASSERT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "        +--Site B -- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://baz.com/",
      DepictFrameTree(root));

  RenderWidgetHostViewBase* rwhv_child = static_cast<RenderWidgetHostViewBase*>(
      nested_iframe_node->current_frame_host()
          ->GetRenderWidgetHost()
          ->GetView());

  WaitForHitTestData(parent_iframe_node->current_frame_host());

  // When we scroll the inner frame, we should have the GSB be consumed.
  // The outer iframe not being scrollable should not cause the GSB to go
  // unconsumed.
  InputEventAckWaiter ack_observer(
      parent_iframe_node->current_frame_host()->GetRenderWidgetHost(),
      base::BindRepeating([](blink::mojom::InputEventResultSource,
                             blink::mojom::InputEventResultState state,
                             const blink::WebInputEvent& event) {
        return event.GetType() ==
                   blink::WebGestureEvent::Type::kGestureScrollBegin &&
               state == blink::mojom::InputEventResultState::kConsumed;
      }));

  // Wait until renderer's compositor thread is synced. Otherwise the non fast
  // scrollable regions won't be set when the event arrives.
  MainThreadFrameObserver observer(rwhv_child->GetRenderWidgetHost());
  observer.Wait();

  // Now scroll the inner frame downward.
  blink::WebMouseWheelEvent scroll_event(
      blink::WebInputEvent::Type::kMouseWheel,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  scroll_event.SetPositionInWidget(90, 110);
  scroll_event.delta_units = ui::ScrollGranularity::kScrollByPrecisePixel;
  scroll_event.delta_x = 0.0f;
  scroll_event.delta_y = -50.0f;
  scroll_event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
  rwhv_child->ProcessMouseWheelEvent(scroll_event, ui::LatencyInfo());
  ack_observer.Wait();
}

// This test verifies that scrolling an element to view works across OOPIFs. The
// testing methodology is based on measuring bounding client rect position of
// nested <iframe>'s after the inner-most frame scrolls into view. The
// measurements are for two identical pages where one page does not have any
// OOPIFs while the other has some nested OOPIFs.
// TODO(crbug.com/827431): This test is flaking on all platforms.
IN_PROC_BROWSER_TEST_P(SitePerProcessProgrammaticScrollTest,
                       DISABLED_ScrollElementIntoView) {
  const GURL url_a(
      embedded_test_server()->GetURL("a.com", kIframeOutOfViewHTML));
  const GURL url_b(
      embedded_test_server()->GetURL("b.com", kIframeOutOfViewHTML));
  const GURL url_c(
      embedded_test_server()->GetURL("c.com", kIframeOutOfViewHTML));

  // Number of <iframe>'s which will not be empty. The actual frame tree has two
  // more nodes one for root and one for the inner-most empty <iframe>.
  const size_t kNonEmptyIframesCount = 5;
  const std::string kScrollIntoViewScript =
      "document.body.scrollIntoView({'behavior' : 'instant'});";
  const int kRectDimensionErrorTolerance = 0;

  // First, recursively set the |scrollTop| and |scrollLeft| of |document.body|
  // to its maximum and then navigate the <iframe> to |url_a|. The page will be
  // structured as a(a(a(a(a(a(a)))))) where the inner-most <iframe> is empty.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  FrameTreeNode* node = web_contents()->GetFrameTree()->root();
  WaitForOnLoad(node);
  std::vector<gfx::Rect> reference_page_bounds_before_scroll = {
      GetBoundingClientRect(node, kIframeSelector)};
  node = node->child_at(0);
  for (size_t index = 0; index < kNonEmptyIframesCount; ++index) {
    NavigateFrameToURL(node, url_a);
    WaitForOnLoad(node);
    // Store |document.querySelector('iframe').getBoundingClientRect()|.
    reference_page_bounds_before_scroll.push_back(
        GetBoundingClientRect(node, kIframeSelector));
    node = node->child_at(0);
  }
  // Sanity-check: If the page is setup properly then all the <iframe>s should
  // be out of view and their bounding rect should not intersect with the
  // positive XY plane.
  for (const auto& rect : reference_page_bounds_before_scroll)
    ASSERT_FALSE(rect.Intersects(kPositiveXYPlane));
  // Now scroll the inner-most frame into view.
  ASSERT_TRUE(ExecuteScript(node, kScrollIntoViewScript));
  // Store current client bounds origins to later compare against those from the
  // page which contains OOPIFs.
  node = web_contents()->GetFrameTree()->root();
  std::vector<gfx::Rect> reference_page_bounds_after_scroll = {
      GetBoundingClientRect(node, kIframeSelector)};
  node = node->child_at(0);
  for (size_t index = 0; index < kNonEmptyIframesCount; ++index) {
    reference_page_bounds_after_scroll.push_back(
        GetBoundingClientRect(node, kIframeSelector));
    node = node->child_at(0);
  }

  // Repeat the same process for the page containing OOPIFs. The page is
  // structured as b(b(a(c(a(a(a)))))) where the inner-most <iframe> is empty.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));
  node = web_contents()->GetFrameTree()->root();
  WaitForOnLoad(node);
  std::vector<gfx::Rect> test_page_bounds_before_scroll = {
      GetBoundingClientRect(node, kIframeSelector)};
  const GURL iframe_urls[] = {url_b, url_a, url_c, url_a, url_a};
  node = node->child_at(0);
  for (size_t index = 0; index < kNonEmptyIframesCount; ++index) {
    NavigateFrameToURL(node, iframe_urls[index]);
    WaitForOnLoad(node);
    test_page_bounds_before_scroll.push_back(
        GetBoundingClientRect(node, kIframeSelector));
    node = node->child_at(0);
  }
  // Sanity-check: The bounds should match those from non-OOPIF page.
  for (size_t index = 0; index < kNonEmptyIframesCount; ++index) {
    ASSERT_TRUE(test_page_bounds_before_scroll[index].ApproximatelyEqual(
        reference_page_bounds_before_scroll[index],
        kRectDimensionErrorTolerance));
  }
  // Scroll the inner most OOPIF.
  ASSERT_TRUE(ExecuteScript(node, kScrollIntoViewScript));
  // Now traverse the chain bottom to top and verify the bounds match for each
  // <iframe>.
  int index = kNonEmptyIframesCount;
  RenderFrameHostImpl* current_rfh = node->current_frame_host()->GetParent();
  while (current_rfh) {
    gfx::Rect current_bounds =
        GetBoundingClientRect(current_rfh->frame_tree_node(), kIframeSelector);
    gfx::Rect reference_bounds = reference_page_bounds_after_scroll[index];
    if (current_bounds.ApproximatelyEqual(reference_bounds,
                                          kRectDimensionErrorTolerance)) {
      current_rfh = current_rfh->GetParent();
      --index;
    } else {
      base::RunLoop run_loop;
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
      run_loop.Run();
    }
  }
}

// This test verifies that ScrollFocusedEditableElementIntoView works correctly
// for OOPIFs. Essentially, the test verifies that in a similar setup, the
// resultant page scale factor is the same for OOPIF and non-OOPIF cases. This
// also verifies that in response to the scroll command, the root-layer scrolls
// correctly and the <input> is visible in visual viewport.
#if defined(OS_ANDROID)
// crbug.com/793616
#define MAYBE_ScrollFocusedEditableElementIntoView \
  DISABLED_ScrollFocusedEditableElementIntoView
#else
#define MAYBE_ScrollFocusedEditableElementIntoView \
  ScrollFocusedEditableElementIntoView
#endif
IN_PROC_BROWSER_TEST_P(SitePerProcessProgrammaticScrollTest,
                       MAYBE_ScrollFocusedEditableElementIntoView) {
  GURL url_a(embedded_test_server()->GetURL("a.com", kIframeOutOfViewHTML));
  GURL url_b(embedded_test_server()->GetURL("b.com", kIframeOutOfViewHTML));

#if defined(OS_ANDROID)
  // The reason for Android specific code is that
  // AutoZoomFocusedNodeToLegibleScale is in blink's WebSettings and difficult
  // to access from here. It so happens that the setting is on for Android.

  // A lower bound on the ratio of page scale factor after scroll. The actual
  // value depends on minReadableCaretHeight / caret_bounds.Height(). The page
  // is setup so caret height is quite small so the expected scale should be
  // larger than 2.0.
  float kLowerBoundOnScaleAfterScroll = 2.0;
  float kEpsilon = 0.1;
#endif

  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  WaitForOnLoad(root);
  NavigateFrameToURL(root->child_at(0), url_a);
  WaitForOnLoad(root->child_at(0));
#if defined(OS_ANDROID)
  float scale_before_scroll_nonoopif = GetVisualViewportScale(root);
#endif
  AddFocusedInputField(root->child_at(0));
  // Focusing <input> causes scrollIntoView(). The following line makes sure
  // that the <iframe> is out of view again.
  SetWindowScroll(root, 0, 0);
  ASSERT_FALSE(GetVisualViewport(root).Intersects(
      GetBoundingClientRect(root, kIframeSelector)));
  root->child_at(0)
      ->current_frame_host()
      ->GetRenderWidgetHost()
      ->GetFrameWidgetInputHandler()
      ->ScrollFocusedEditableNodeIntoRect(gfx::Rect());
  WaitForElementVisible(root, kIframeSelector);
#if defined(OS_ANDROID)
  float scale_after_scroll_nonoopif = GetVisualViewportScale(root);
  // Increased scale means zoom triggered correctly.
  EXPECT_GT(scale_after_scroll_nonoopif - scale_before_scroll_nonoopif,
            kEpsilon);
  EXPECT_GT(scale_after_scroll_nonoopif, kLowerBoundOnScaleAfterScroll);
#endif

  // Retry the test on an OOPIF page.
  Shell* new_shell = CreateBrowser();
  ASSERT_TRUE(NavigateToURL(new_shell, url_b));
  root = static_cast<WebContentsImpl*>(new_shell->web_contents())
             ->GetFrameTree()
             ->root();
  WaitForOnLoad(root);
#if defined(OS_ANDROID)
  float scale_before_scroll_oopif = GetVisualViewportScale(root);
  // Sanity-check:
  ASSERT_NEAR(scale_before_scroll_oopif, scale_before_scroll_nonoopif,
              kEpsilon);
#endif
  NavigateFrameToURL(root->child_at(0), url_a);
  WaitForOnLoad(root->child_at(0));
  AddFocusedInputField(root->child_at(0));
  SetWindowScroll(root, 0, 0);
  ASSERT_FALSE(GetVisualViewport(root).Intersects(
      GetBoundingClientRect(root, kIframeSelector)));
  root->child_at(0)
      ->current_frame_host()
      ->GetRenderWidgetHost()
      ->GetFrameWidgetInputHandler()
      ->ScrollFocusedEditableNodeIntoRect(gfx::Rect());
  WaitForElementVisible(root, kIframeSelector);
#if defined(OS_ANDROID)
  float scale_after_scroll_oopif = GetVisualViewportScale(root);
  EXPECT_GT(scale_after_scroll_oopif - scale_before_scroll_oopif, kEpsilon);
  EXPECT_GT(scale_after_scroll_oopif, kLowerBoundOnScaleAfterScroll);
  // The scale is based on the caret height and it should be the same in both
  // OOPIF and non-OOPIF pages.
  EXPECT_NEAR(scale_after_scroll_oopif, scale_after_scroll_nonoopif, kEpsilon);
#endif
  // Make sure the <input> is at least partly visible in the |visualViewport|.
  gfx::Rect final_visual_viewport_oopif = GetVisualViewport(root);
  gfx::Rect iframe_bounds_after_scroll_oopif =
      GetBoundingClientRect(root, kIframeSelector);
  gfx::Rect input_bounds_after_scroll_oopif =
      GetBoundingClientRect(root->child_at(0), kInputSelector);
  input_bounds_after_scroll_oopif +=
      iframe_bounds_after_scroll_oopif.OffsetFromOrigin();
  ASSERT_TRUE(
      final_visual_viewport_oopif.Intersects(input_bounds_after_scroll_oopif));
}

IN_PROC_BROWSER_TEST_P(SitePerProcessProgrammaticScrollTest,
                       ScrollClippedFocusedEditableElementIntoView) {
  GURL url_a(embedded_test_server()->GetURL("a.com", kIframeClippedHTML));
  GURL child_url_b(embedded_test_server()->GetURL("b.com", kInputBoxHTML));

  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  WaitForOnLoad(root);
  NavigateFrameToURL(root->child_at(0), child_url_b);
  WaitForOnLoad(root->child_at(0));

  SetWindowScroll(root, 0, 0);
  SetWindowScroll(root->child_at(0), 1000, 2000);

  float scale_before = GetVisualViewportScale(root);

  // The input_box page focuses the input box on load. This call should
  // simulate the scroll into view we do when an input box is tapped.
  root->child_at(0)
      ->current_frame_host()
      ->GetRenderWidgetHost()
      ->GetFrameWidgetInputHandler()
      ->ScrollFocusedEditableNodeIntoRect(gfx::Rect());

  // The scroll into view is animated on the compositor. Make sure we wait
  // until that's completed before testing the rects.
  WaitForElementVisible(root, kIframeSelector);
  WaitForViewportToStabilize(root);

  // These rects are in the coordinate space of the root frame.
  gfx::Rect visual_viewport_rect = GetVisualViewport(root);
  gfx::Rect window_rect = GetBoundingClientRect(root, ":root");
  gfx::Rect iframe_rect = GetBoundingClientRect(root, "iframe");
  gfx::Rect clip_rect = GetBoundingClientRect(root, "#clip");

  // This is in the coordinate space of the iframe, we'll add the iframe offset
  // after to put it into the root frame's coordinate space.
  gfx::Rect input_rect = GetBoundingClientRect(root->child_at(0), "input");

  // Make sure the input rect is visible in the iframe.
  EXPECT_TRUE(gfx::Rect(iframe_rect.size()).Intersects(input_rect))
      << "Input box [" << input_rect.ToString() << "] isn't visible in iframe ["
      << gfx::Rect(iframe_rect.size()).ToString() << "]";

  input_rect += iframe_rect.OffsetFromOrigin();

  // Make sure the input rect is visible through the clipping layer.
  EXPECT_TRUE(clip_rect.Intersects(input_rect))
      << "Input box [" << input_rect.ToString() << "] isn't scrolled into view "
      << "of the clipping layer [" << clip_rect.ToString() << "]";

  // And finally, it should be visible in the layout and visual viewports.
  EXPECT_TRUE(window_rect.Intersects(input_rect))
      << "Input box [" << input_rect.ToString() << "] isn't visible in the "
      << "layout viewport [" << window_rect.ToString() << "]";
  EXPECT_TRUE(visual_viewport_rect.Intersects(input_rect))
      << "Input box [" << input_rect.ToString() << "] isn't visible in the "
      << "visual viewport [" << visual_viewport_rect.ToString() << "]";

  float scale_after = GetVisualViewportScale(root);

// Make sure we still zoom in on the input box on platforms that zoom into the
// focused editable.
#if defined(OS_ANDROID)
  EXPECT_GT(scale_after, scale_before);
#else
  EXPECT_FLOAT_EQ(scale_after, scale_before);
#endif
}

IN_PROC_BROWSER_TEST_P(SitePerProcessProgrammaticScrollTest,
                       ScrolledOutOfView) {
  GURL main_frame(
      embedded_test_server()->GetURL("a.com", kIframeOutOfViewHTML));
  GURL child_url_b(
      embedded_test_server()->GetURL("b.com", kIframeOutOfViewHTML));

  // This will set up the page frame tree as A(B()).
  ASSERT_TRUE(NavigateToURL(shell(), main_frame));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  WaitForOnLoad(root);
  NavigateFrameToURL(root->child_at(0), child_url_b);
  WaitForOnLoad(root->child_at(0));

  FrameTreeNode* nested_iframe_node = root->child_at(0);
  RenderFrameProxyHost* proxy_to_parent =
      nested_iframe_node->render_manager()->GetProxyToParent();
  CrossProcessFrameConnector* connector =
      proxy_to_parent->cross_process_frame_connector();

  while (blink::mojom::FrameVisibility::kRenderedOutOfViewport !=
         connector->visibility()) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }
}

// This test verifies that smooth scrolling works correctly inside nested OOPIFs
// which are same origin with the parent. Note that since the frame tree has
// a A(B(A1())) structure, if and A1 and A2 shared the same
// SmoothScrollSequencer, then this test would time out or at best be flaky with
// random time outs. See https://crbug.com/865446 for more context.
IN_PROC_BROWSER_TEST_P(SitePerProcessProgrammaticScrollTest,
                       SmoothScrollInNestedSameProcessOOPIF) {
  GURL main_frame(
      embedded_test_server()->GetURL("a.com", kIframeOutOfViewHTML));
  GURL child_url_b(
      embedded_test_server()->GetURL("b.com", kIframeOutOfViewHTML));
  GURL same_origin(
      embedded_test_server()->GetURL("a.com", kIframeOutOfViewHTML));

  // This will set up the page frame tree as A(B(A1(A2()))) where A1 is later
  // asked to scroll the <iframe> element of A2 into view. The important bit
  // here is that the inner frame A1 is recursively scrolling (smoothly) an
  // element inside its document into view (A2's origin is irrelevant here).
  ASSERT_TRUE(NavigateToURL(shell(), main_frame));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  WaitForOnLoad(root);
  NavigateFrameToURL(root->child_at(0), child_url_b);
  WaitForOnLoad(root->child_at(0));
  auto* nested_ftn = root->child_at(0)->child_at(0);
  NavigateFrameToURL(nested_ftn, same_origin);
  WaitForOnLoad(nested_ftn);

  // *Smoothly* scroll the inner most frame into view.
  ASSERT_TRUE(ExecuteScript(
      nested_ftn,
      "document.querySelector('iframe').scrollIntoView({behavior: 'smooth'})"));
  WaitForElementVisible(root, kIframeSelector);
  WaitForElementVisible(root->child_at(0), kIframeSelector);
  WaitForElementVisible(nested_ftn, kIframeSelector);
}

// Tests OOPIF rendering by checking that the RWH of the iframe generates
// OnSwapCompositorFrame message.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, CompositorFrameSwapped) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(baz)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
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
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a,a(a,a(a)))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  TestNavigationObserver observer(shell()->web_contents());

  // Load a cross-site page into both iframes.
  GURL foo_url = embedded_test_server()->GetURL("foo.com", "/title2.html");
  NavigateFrameToURL(root->child_at(0), foo_url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(foo_url, observer.last_navigation_url());
  NavigateFrameToURL(root->child_at(1), foo_url);
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
  EXPECT_TRUE(ExecuteScript(shell(),
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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  TestNavigationObserver observer(shell()->web_contents());

  // Load same-site page into iframe.
  FrameTreeNode* child = root->child_at(0);
  GURL http_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  NavigateFrameToURL(child, http_url);
  EXPECT_EQ(http_url, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());

  // Load cross-site page into iframe.
  GURL url = embedded_test_server()->GetURL("foo.com", "/title2.html");
  {
    RenderFrameDeletedObserver deleted_observer(child->current_frame_host());
    NavigateFrameToURL(root->child_at(0), url);
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
    NavigateFrameToURL(child, http_url);
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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  TestNavigationObserver observer(shell()->web_contents());

  // Load same-site page into iframe.
  FrameTreeNode* child = root->child_at(0);
  GURL http_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  NavigateFrameToURL(child, http_url);
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
  NavigateFrameToURL(child, url);
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
  NavigateFrameToURL(child, url);
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
  NavigateFrameToURL(child, url);
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
  ExecuteScriptAsync(child, "window.location.href = 'about:blank#foo';");
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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

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
  NavigateFrameToURL(node3, site_b_url);
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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  TestNavigationObserver observer(shell()->web_contents());
  ASSERT_EQ(1U, root->child_count());

  // Make sure node2 points to the correct cross-site page.
  GURL site_b_url = embedded_test_server()->GetURL(
      "bar.com", "/cross_site_iframe_factory.html?bar.com()");
  FrameTreeNode* node2 = root->child_at(0);
  EXPECT_EQ(site_b_url, node2->current_url());

  web_contents()->SetFocusedFrame(
      node2, node2->current_frame_host()->GetSiteInstance());

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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
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
  NavigateFrameToURL(node3, url);
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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Load same-site page into iframe.
  {
    TestNavigationObserver observer(shell()->web_contents());
    FrameTreeNode* child = root->child_at(0);
    GURL http_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
    NavigateFrameToURL(child, http_url);
    EXPECT_EQ(http_url, observer.last_navigation_url());
    EXPECT_TRUE(observer.last_navigation_succeeded());
    observer.Wait();
  }

  // Load cross-site page into iframe.
  {
    TestNavigationObserver observer(shell()->web_contents());
    FrameTreeNode* child = root->child_at(0);
    GURL url = embedded_test_server()->GetURL("foo.com", "/title2.html");
    NavigateFrameToURL(root->child_at(0), url);
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
  // This test doesn't yet work properly with RenderDocument for subframes,
  // because it does not correctly identify the repeated navigation to the same
  // URL as same-page, and fails to reuse the same NavigationEntry in this
  // case. See https://crbug.com/1068965.
  // TODO(fergal,alexmos): Re-enable once this is fixed.
  if (ShouldCreateNewHostForSameSiteSubframe())
    return;

  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);
  GURL url_a = child->current_url();

  // Disable host resolution in the test server and try to navigate the subframe
  // cross-site, which will lead to a committed net error.
  GURL url_b = embedded_test_server()->GetURL("b.com", "/title3.html");
  std::unique_ptr<URLLoaderInterceptor> url_loader_interceptor;
  url_loader_interceptor = std::make_unique<URLLoaderInterceptor>(
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

  NavigateIframeToURL(shell()->web_contents(), "child-0", url_b);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(url_b, observer.last_navigation_url());

  // The FrameTreeNode should have updated its URL and origin.
  EXPECT_EQ(url_b, child->current_frame_host()->last_successful_url());
  EXPECT_EQ(url_b, child->current_url());
  EXPECT_EQ(url_b.GetOrigin().spec(),
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
  EXPECT_EQ(url_a.GetOrigin().spec(),
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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
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
  NavigateFrameToURL(root->child_at(1), b_url);
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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   |--Site A ------- proxies for B\n"
      "   +--Site A ------- proxies for B\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));
  SiteInstance* b_site_instance =
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
  EXPECT_TRUE(ExecuteScript(
      root->child_at(2),
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
      grandchild->render_manager()->GetRenderFrameProxyHost(b_site_instance);
  EXPECT_FALSE(grandchild_rfph->is_render_frame_proxy_live());

  // Navigate the second subframe to b.com to recreate process B.
  TestNavigationObserver observer(shell()->web_contents());
  GURL b_url = embedded_test_server()->GetURL("b.com", "/title1.html");
  NavigateFrameToURL(root->child_at(1), b_url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(b_url, observer.last_navigation_url());

  // Ensure that the grandchild RenderFrameProxy in B was created when process
  // B was restored.
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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  SiteInstance* site_instance_a = root->current_frame_host()->GetSiteInstance();

  // Open a popup and navigate it cross-process to b.com.
  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(ExecuteScript(root, "popup = window.open('about:blank');"));
  Shell* popup = new_shell_observer.GetShell();
  GURL popup_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(popup, popup_url));

  // Verify that each top-level frame has proxies in the other's SiteInstance.
  FrameTreeNode* popup_root =
      static_cast<WebContentsImpl*>(popup->web_contents())
          ->GetFrameTree()
          ->root();
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
      popup_root->render_manager()->GetRenderFrameProxyHost(site_instance_a);
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
  EXPECT_TRUE(ExecuteScript(
      popup, "document.body.appendChild(document.createElement('iframe'));"));
  frame_observer.Wait();

  // Both the child frame's and its parent's proxies should still not be live.
  // The main page can't reach them since it lost reference to the popup after
  // it crashed, so there is no need to create them.
  EXPECT_FALSE(rfph->is_render_frame_proxy_live());
  RenderFrameProxyHost* child_rfph =
      popup_root->child_at(0)->render_manager()->GetRenderFrameProxyHost(
          site_instance_a);
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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
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

  // |site_instance_c|'s frames and proxies are expected to go away once we kill
  // |child_process_b| below.
  scoped_refptr<SiteInstanceImpl> site_instance_c =
      node4->current_frame_host()->GetSiteInstance();

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

  EXPECT_GT(site_instance_c->active_frame_count(), 0U);

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

  EXPECT_EQ(0U, site_instance_c->active_frame_count());
}

// Crash a subframe and ensures its children are cleared from the FrameTree.
// See http://crbug.com/338508.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, CrashSubframe) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Check the subframe process.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
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
  EXPECT_FALSE(child->current_frame_host()->render_frame_created_);

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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
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
  EXPECT_TRUE(ExecuteScript(shell(), "addFrame('data:text/html,foo');"));
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

    RedirectNotificationObserver load_observer2(
        NOTIFICATION_LOAD_STOP, Source<NavigationController>(
                                    &shell()->web_contents()->GetController()));

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
    RedirectNotificationObserver load_observer2(
        NOTIFICATION_LOAD_STOP, Source<NavigationController>(
                                    &shell()->web_contents()->GetController()));

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
    RedirectNotificationObserver load_observer2(
        NOTIFICATION_LOAD_STOP, Source<NavigationController>(
                                    &shell()->web_contents()->GetController()));

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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  EXPECT_TRUE(root->child_at(1) != nullptr);
  EXPECT_EQ(2U, root->child_at(1)->child_count());

  {
    // Load same-site page into iframe.
    TestNavigationObserver observer(shell()->web_contents());
    GURL http_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
    NavigateFrameToURL(root->child_at(0), http_url);
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
    TestFrameNavigationObserver navigation_observer(child);
    NavigationController::LoadURLParams params(cross_site_url);
    params.transition_type = PageTransitionFromInt(ui::PAGE_TRANSITION_LINK);
    params.frame_tree_node_id = child->frame_tree_node_id();
    child->navigator().GetController()->LoadURLWithParams(params);

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
    navigation_observer.Wait();
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
    TestFrameNavigationObserver navigation_observer(child);
    NavigationController::LoadURLParams params(cross_site_url);
    params.transition_type = PageTransitionFromInt(ui::PAGE_TRANSITION_LINK);
    params.frame_tree_node_id = child->frame_tree_node_id();
    child->navigator().GetController()->LoadURLWithParams(params);

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

    navigation_observer.Wait();
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_EQ(cross_site_url, observer.last_navigation_url());
    EXPECT_EQ(0U, child->child_count());
  }
}

// Verify that "scrolling" property on frame elements propagates to child frames
// correctly.
// Does not work on android since android has scrollbars overlayed.
// TODO(bokan): Pretty soon most/all platforms will use overlay scrollbars. This
// test should find a better way to check for scrollability. crbug.com/662196.
// Flaky on Linux. crbug.com/790929.
#if defined(OS_ANDROID) || defined(OS_LINUX) || defined(OS_CHROMEOS)
#define MAYBE_FrameOwnerPropertiesPropagationScrolling \
  DISABLED_FrameOwnerPropertiesPropagationScrolling
#else
#define MAYBE_FrameOwnerPropertiesPropagationScrolling \
  FrameOwnerPropertiesPropagationScrolling
#endif
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       MAYBE_FrameOwnerPropertiesPropagationScrolling) {
#if defined(OS_MAC)
  ui::test::ScopedPreferredScrollerStyle scroller_style_override(false);
#endif
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_owner_properties_scrolling.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  ASSERT_EQ(1u, root->child_count());

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  FrameTreeNode* child = root->child_at(0);

  // If the available client width within the iframe is smaller than the
  // frame element's width, we assume there's a scrollbar.
  // Also note that just comparing clientHeight and scrollHeight of the frame's
  // document will not work.
  auto has_scrollbar = [](RenderFrameHostImpl* rfh) {
    int client_width = EvalJs(rfh, "document.body.clientWidth").ExtractInt();
    const int kFrameElementWidth = 200;
    return client_width < kFrameElementWidth;
  };

  auto set_scrolling_property = [](RenderFrameHostImpl* parent_rfh,
                                   const std::string& value) {
    EXPECT_TRUE(ExecJs(
        parent_rfh,
        base::StringPrintf("document.getElementById('child-1').setAttribute("
                           "    'scrolling', '%s');",
                           value.c_str())));
  };

  // Run the test over variety of parent/child cases.
  GURL urls[] = {// Remote to remote.
                 embedded_test_server()->GetURL("c.com", "/tall_page.html"),
                 // Remote to local.
                 embedded_test_server()->GetURL("a.com", "/tall_page.html"),
                 // Local to remote.
                 embedded_test_server()->GetURL("b.com", "/tall_page.html")};
  const std::string scrolling_values[] = {"yes", "auto", "no"};

  for (size_t i = 0; i < base::size(scrolling_values); ++i) {
    bool expect_scrollbar = scrolling_values[i] != "no";
    set_scrolling_property(root->current_frame_host(), scrolling_values[i]);
    for (size_t j = 0; j < base::size(urls); ++j) {
      NavigateFrameToURL(child, urls[j]);
      EXPECT_EQ(expect_scrollbar, has_scrollbar(child->current_frame_host()));
    }
  }
}

// Verify that "marginwidth" and "marginheight" properties on frame elements
// propagate to child frames correctly.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       FrameOwnerPropertiesPropagationMargin) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_owner_properties_margin.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  ASSERT_EQ(1u, root->child_count());

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  FrameTreeNode* child = root->child_at(0);

  EXPECT_EQ("10", EvalJs(child, "document.body.getAttribute('marginwidth');"));
  EXPECT_EQ("50", EvalJs(child, "document.body.getAttribute('marginheight');"));

  // Run the test over variety of parent/child cases.
  GURL urls[] = {// Remote to remote.
                 embedded_test_server()->GetURL("c.com", "/title2.html"),
                 // Remote to local.
                 embedded_test_server()->GetURL("a.com", "/title1.html"),
                 // Local to remote.
                 embedded_test_server()->GetURL("b.com", "/title2.html")};

  int current_margin_width = 15;
  int current_margin_height = 25;

  // Before each navigation, we change the marginwidth and marginheight
  // properties of the frame. We then check whether those properties are applied
  // correctly after the navigation has completed.
  for (size_t i = 0; i < base::size(urls); ++i) {
    // Change marginwidth and marginheight before navigating.
    EXPECT_TRUE(ExecJs(
        root,
        base::StringPrintf("var child = document.getElementById('child-1');"
                           "child.setAttribute('marginwidth', '%d');"
                           "child.setAttribute('marginheight', '%d');",
                           current_margin_width, current_margin_height)));

    NavigateFrameToURL(child, urls[i]);

    EXPECT_EQ(base::NumberToString(current_margin_width),
              EvalJs(child, "document.body.getAttribute('marginwidth');"));
    EXPECT_EQ(base::NumberToString(current_margin_height),
              EvalJs(child, "document.body.getAttribute('marginheight');"));

    current_margin_width += 5;
    current_margin_height += 10;
  }
}

// Verify that "csp" property on frame elements propagates to child frames
// correctly. See  https://crbug.com/647588
IN_PROC_BROWSER_TEST_P(SitePerProcessEmbedderCSPEnforcementBrowserTest,
                       FrameOwnerPropertiesPropagationCSP) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_owner_properties_csp.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  ASSERT_EQ(1u, root->child_count());

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  FrameTreeNode* child = root->child_at(0);

  EXPECT_EQ(
      "object-src \'none\'",
      EvalJs(root, "document.getElementById('child-1').getAttribute('csp');"));

  // Run the test over variety of parent/child cases.
  GURL urls[] = {// Remote to remote.
                 embedded_test_server()->GetURL("c.com", "/title2.html"),
                 // Remote to local.
                 embedded_test_server()->GetURL("a.com", "/title1.html"),
                 // Local to remote.
                 embedded_test_server()->GetURL("b.com", "/title2.html")};

  std::vector<std::string> csp_values = {"default-src a.com",
                                         "default-src b.com", "img-src c.com"};

  // Before each navigation, we change the csp property of the frame.
  // We then check whether that property is applied
  // correctly after the navigation has completed.
  for (size_t i = 0; i < base::size(urls); ++i) {
    // Change csp before navigating.
    EXPECT_TRUE(ExecuteScript(
        root,
        base::StringPrintf("document.getElementById('child-1').setAttribute("
                           "    'csp', '%s');",
                           csp_values[i].c_str())));

    NavigateFrameToURL(child, urls[i]);
    if (!base::FeatureList::IsEnabled(network::features::kOutOfBlinkCSPEE)) {
      EXPECT_EQ(csp_values[i], child->frame_owner_properties().required_csp);
    } else {
      EXPECT_EQ(csp_values[i], child->csp_attribute()->header->header_value);
    }
    // TODO(amalika): add checks that the CSP replication takes effect

    const url::Origin child_origin =
        child->current_frame_host()->GetLastCommittedOrigin();
    // TODO(https://crbug.com/1000804): Enable check once bug is fixed.
    // EXPECT_TRUE(child_origin.opaque());
    EXPECT_EQ(url::Origin::Create(urls[i].GetOrigin())
                  .GetTupleOrPrecursorTupleIfOpaque(),
              child_origin.GetTupleOrPrecursorTupleIfOpaque());
  }
}

// Verify origin replication with an A-embed-B-embed-C-embed-A hierarchy.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, OriginReplication) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c(a),b), a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

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
  // RenderFrameProxy in b.com's process.
  EXPECT_EQ(ListValueOf(a_origin),
            EvalJs(tiptop_child, "Array.from(location.ancestorOrigins);"));

  // Check that c.com frame's location.ancestorOrigins contains the correct
  // origin for its two ancestors. The topmost parent origin should be
  // replicated as part of mojom::Renderer::CreateView, and the middle frame
  // (b.com's) origin should be replicated as part of
  // mojom::Renderer::CreateFrameProxy sent for b.com's frame in c.com's
  // process.
  EXPECT_EQ(ListValueOf(b_origin, a_origin),
            EvalJs(middle_child, "Array.from(location.ancestorOrigins);"));

  // Check that the nested a.com frame's location.ancestorOrigins contains the
  // correct origin for its three ancestors.
  EXPECT_EQ(ListValueOf(c_origin, b_origin, a_origin),
            EvalJs(lowest_child, "Array.from(location.ancestorOrigins);"));
}

// Test that HasReceivedUserGesture and HasReceivedUserGestureBeforeNavigation
// are propagated correctly across origins.
// Flaky. https://crbug.com/1014175
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
  OpenURLBlockUntilNavigationComplete(shell(), main_url);
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Navigate the subframes to cross-origin pages.
  NavigateFrameAndWait(root->child_at(0), foo_url);
  NavigateFrameAndWait(root->child_at(0)->child_at(0), bar_url);

  // Test that all frames can autoplay if there has been a gesture in the top
  // frame.
  EXPECT_TRUE(AutoplayAllowed(shell(), true));
  EXPECT_TRUE(AutoplayAllowed(root->child_at(0), false));
  EXPECT_TRUE(AutoplayAllowed(root->child_at(0)->child_at(0), false));

  // Navigate to a new page on the same origin.
  OpenURLBlockUntilNavigationComplete(shell(), secondary_url);
  root = web_contents()->GetFrameTree()->root();

  // Navigate the subframes to cross-origin pages.
  NavigateFrameAndWait(root->child_at(0), foo_url);
  NavigateFrameAndWait(root->child_at(0)->child_at(0), bar_url);

  // Test that all frames can autoplay because the gesture bit has been passed
  // through the navigation.
  EXPECT_TRUE(AutoplayAllowed(shell(), false));
  EXPECT_TRUE(AutoplayAllowed(root->child_at(0), false));
  EXPECT_TRUE(AutoplayAllowed(root->child_at(0)->child_at(0), false));

  // Navigate to a page with autoplay disabled.
  OpenURLBlockUntilNavigationComplete(shell(), disabled_url);
  NavigateFrameAndWait(root->child_at(0), foo_url);

  // Test that autoplay is no longer allowed.
  EXPECT_TRUE(AutoplayAllowed(shell(), false));
  EXPECT_FALSE(AutoplayAllowed(root->child_at(0), false));

  // Navigate to another origin and make sure autoplay is disabled.
  OpenURLBlockUntilNavigationComplete(shell(), foo_url);
  NavigateFrameAndWait(root->child_at(0), bar_url);
  EXPECT_FALSE(AutoplayAllowed(shell(), false));
  EXPECT_FALSE(AutoplayAllowed(shell(), false));
}

IN_PROC_BROWSER_TEST_P(SitePerProcessScrollAnchorTest,
                       RemoteToLocalScrollAnchorRestore) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/page_with_samesite_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);

  GURL frame_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  NavigateFrameToURL(child, frame_url);

  EXPECT_NE(child->current_frame_host()->GetSiteInstance(),
            root->current_frame_host()->GetSiteInstance());

  TestFrameNavigationObserver frame_observer2(child);
  EXPECT_TRUE(ExecuteScript(root, "window.history.back()"));
  frame_observer2.Wait();

  EXPECT_EQ(child->current_frame_host()->GetSiteInstance(),
            root->current_frame_host()->GetSiteInstance());
}

// Check that iframe sandbox flags are replicated correctly.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, SandboxFlagsReplication) {
  GURL main_url(embedded_test_server()->GetURL("/sandboxed_frames.html"));
  const url::Origin main_origin = url::Origin::Create(main_url);
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  TestNavigationObserver observer(shell()->web_contents());

  // Navigate the second (sandboxed) subframe to a cross-site page with a
  // subframe.
  GURL foo_url(
      embedded_test_server()->GetURL("foo.com", "/frame_tree/1-1.html"));
  NavigateFrameToURL(root->child_at(1), foo_url);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // We can't use a TestNavigationObserver to verify the URL here,
  // since the frame has children that may have clobbered it in the observer.
  EXPECT_EQ(foo_url, root->child_at(1)->current_url());

  // Load cross-site page into subframe's subframe.
  ASSERT_EQ(2U, root->child_at(1)->child_count());
  GURL bar_url(embedded_test_server()->GetURL("bar.com", "/title1.html"));
  NavigateFrameToURL(root->child_at(1)->child_at(0), bar_url);
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
  GURL main_url(
      embedded_test_server()->GetURL("/frame_tree/page_with_two_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  TestNavigationObserver observer(shell()->web_contents());
  ASSERT_EQ(2U, root->child_count());

  // Make sure first frame starts out at the correct cross-site page.
  EXPECT_EQ(embedded_test_server()->GetURL("bar.com", "/title1.html"),
            root->child_at(0)->current_url());

  // Navigate second frame to another cross-site page.
  GURL baz_url(embedded_test_server()->GetURL("baz.com", "/title1.html"));
  NavigateFrameToURL(root->child_at(1), baz_url);
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
  EXPECT_TRUE(ExecuteScript(
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
  NavigateFrameToURL(root->child_at(0), bar_url);
  // (The new page has a subframe; wait for it to load as well.)
  ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(bar_url, root->child_at(0)->current_url());
  ASSERT_EQ(1U, root->child_at(0)->child_count());

  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   |--Site B ------- proxies for A C\n"
      "   |    +--Site B -- proxies for A C\n"
      "   +--Site C ------- proxies for A B\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://bar.com/\n"
      "      C = http://baz.com/",
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
  GURL baz_child_url(embedded_test_server()->GetURL("baz.com", "/title2.html"));
  NavigateFrameToURL(root->child_at(0)->child_at(0), baz_child_url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(baz_child_url, observer.last_navigation_url());

  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   |--Site B ------- proxies for A C\n"
      "   |    +--Site C -- proxies for A B\n"
      "   +--Site C ------- proxies for A B\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://bar.com/\n"
      "      C = http://baz.com/",
      DepictFrameTree(root));

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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  TestNavigationObserver observer(shell()->web_contents());
  ASSERT_EQ(2U, root->child_count());

  // Make sure the two frames starts out at correct URLs.
  EXPECT_EQ(embedded_test_server()->GetURL("bar.com", "/title1.html"),
            root->child_at(0)->current_url());
  EXPECT_EQ(embedded_test_server()->GetURL("/title1.html"),
            root->child_at(1)->current_url());

  // Update the second frame's sandbox flags.
  EXPECT_TRUE(ExecuteScript(
      shell(),
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
  NavigateFrameToURL(root->child_at(1), bar_url);
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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

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
  EXPECT_TRUE(ExecuteScript(
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
  ASSERT_TRUE(
      ExecuteScript(root->child_at(0), "window.location.href='/title2.html'"));
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
// TODO(alexmos): Re-enable when https://crbug.com/610893 is fixed.
IN_PROC_BROWSER_TEST_P(
    SitePerProcessBrowserTest,
    DISABLED_ProxiesForNewChildFramesHaveCorrectReplicationState) {
  GURL main_url(
      embedded_test_server()->GetURL("/frame_tree/page_with_one_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  TestNavigationObserver observer(shell()->web_contents());

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://baz.com/",
      DepictFrameTree(root));

  // In the root frame, add a new sandboxed local frame, which itself has a
  // child frame on baz.com.  Wait for three RenderFrameHosts to be created:
  // the new sandboxed local frame, its child (while it's still local), and a
  // pending RFH when starting the cross-site navigation to baz.com.
  RenderFrameHostCreatedObserver frame_observer(shell()->web_contents(), 3);
  EXPECT_TRUE(ExecuteScript(root,
                            "addFrame('/frame_tree/page_with_one_frame.html',"
                            "         'allow-scripts allow-same-origin'))"));
  frame_observer.Wait();

  // Wait for the cross-site navigation to baz.com in the grandchild to finish.
  FrameTreeNode* bottom_child = root->child_at(1)->child_at(0);
  TestFrameNavigationObserver navigation_observer(bottom_child);
  navigation_observer.Wait();

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   +--Site A ------- proxies for B\n"
      "        +--Site B -- proxies for A\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://baz.com/",
      DepictFrameTree(root));

  // Use location.ancestorOrigins to check that the grandchild on baz.com sees
  // correct origin for its parent.
  EXPECT_EQ(ListValueOf(url::Origin::Create(main_url)),
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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  TestNavigationObserver observer(shell()->web_contents());

  // Load cross-site page into iframe.
  GURL frame_url =
      embedded_test_server()->GetURL("foo.com", "/frame_tree/3-1.html");
  NavigateFrameToURL(root->child_at(0), frame_url);
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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  TestNavigationObserver observer(shell()->web_contents());

  // Load cross-site page into iframe.
  GURL frame_url =
      embedded_test_server()->GetURL("foo.com", "/frame_tree/3-1.html");
  NavigateFrameToURL(root->child_at(0), frame_url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(frame_url, observer.last_navigation_url());

  // Browser process should know the child frame's original window.name
  // specified in the iframe element.
  EXPECT_EQ(root->child_at(0)->frame_name(), "3-1-name");

  // Update the child frame's window.name.
  EXPECT_TRUE(
      ExecuteScript(root->child_at(0), "window.name = 'updated-name';"));

  // The change should propagate to the browser process.
  EXPECT_EQ(root->child_at(0)->frame_name(), "updated-name");

  // The proxy in the parent process should also receive the updated name.
  // Now iframe's name and the content window's name differ, so it shouldn't
  // be possible to access to the content window with the updated name.
  //
  // TODO(yukishiino): The following expectation should be |true|, but we're
  // intentionally disabling the name and origin check of the named access on
  // window.  See also crbug.com/538562 and crbug.com/701489.
  EXPECT_EQ(false, EvalJs(shell(), "frames['updated-name'] === undefined;"));
  // Change iframe's name to match the content window's name so that it can
  // reference the child frame by its new name in case of cross origin.
  EXPECT_TRUE(ExecuteScript(root, "window['3-1-id'].name = 'updated-name';"));
  EXPECT_EQ(true, EvalJs(shell(), "frames['updated-name'] == frames[0];"));

  // Issue a renderer-initiated navigation from the root frame to the child
  // frame using the frame's name. Make sure correct frame is navigated.
  //
  // TODO(alexmos): When blink::createWindow is refactored to handle
  // RemoteFrames, this should also be tested via window.open(url, frame_name)
  // and a more complicated frame hierarchy (https://crbug.com/463742)
  TestFrameNavigationObserver frame_observer(root->child_at(0));
  GURL foo_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  EXPECT_TRUE(ExecuteScript(
      shell(),
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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
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
  NavigateFrameToURL(root->child_at(1), frame_url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(frame_url, observer.last_navigation_url());

  // The first frame can't directly observe the second frame's origin with
  // JavaScript.  Instead, try to navigate the second frame from the first
  // frame.  This should fail with a console error message, which should
  // contain the second frame's updated origin (see blink::Frame::canNavigate).
  WebContentsConsoleObserver console_observer(shell()->web_contents());
  console_observer.SetPattern(
      "Unsafe JavaScript attempt to initiate navigation*");

  // frames[1] can't be used due to a bug where RemoteFrames are created out of
  // order (https://crbug.com/478792).  Instead, target second frame by name.
  EXPECT_TRUE(ExecuteScript(root->child_at(0),
                            "try { parent.frames['frame2'].location.href = "
                            "'data:text/html,foo'; } catch (e) {}"));
  console_observer.Wait();

  std::string frame_origin = root->child_at(1)->current_origin().Serialize();
  EXPECT_EQ(frame_origin + "/", frame_url.GetOrigin().spec());
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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  TestNavigationObserver observer(shell()->web_contents());

  // Load same-site page into iframe.
  FrameTreeNode* child = root->child_at(0);
  GURL http_url(embedded_test_server()->GetURL("/title1.html"));
  NavigateFrameToURL(child, http_url);
  EXPECT_EQ(http_url, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());

  // Load cross-site page into iframe.
  TestNavigationObserver nav_observer(shell()->web_contents(), 1);
  GURL url = embedded_test_server()->GetURL("foo.com", "/title2.html");
  NavigationController::LoadURLParams params(url);
  params.transition_type = ui::PAGE_TRANSITION_LINK;
  params.frame_tree_node_id = child->frame_tree_node_id();
  child->navigator().GetController()->LoadURLWithParams(params);
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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
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
  NavigateFrameToURL(node3, title_url);
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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  FrameTreeNode* node2 = root->child_at(0);
  FrameTreeNode* node3 = root->child_at(1);

  // Navigate the second iframe to the same process as the first.
  GURL frame_url = embedded_test_server()->GetURL("bar.com", "/title1.html");
  NavigateFrameToURL(node3, frame_url);

  // Verify that they are in the same process.
  EXPECT_EQ(node2->current_frame_host()->GetSiteInstance(),
            node3->current_frame_host()->GetSiteInstance());
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            node3->current_frame_host()->GetSiteInstance());

  // Navigate the first iframe into its parent's process.
  GURL title_url = embedded_test_server()->GetURL("/title2.html");
  NavigateFrameToURL(node2, title_url);
  EXPECT_NE(node2->current_frame_host()->GetSiteInstance(),
            node3->current_frame_host()->GetSiteInstance());

  // Return the first iframe to the same process as its sibling, and ensure
  // that it does not crash.
  NavigateFrameToURL(node2, frame_url);
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
    base::string16 expected_title(base::UTF8ToUTF16("LOADED"));
    TitleWatcher title_watcher(shell()->web_contents(), expected_title);
    EXPECT_TRUE(NavigateToURL(shell(), main_url));
    EXPECT_EQ(title_watcher.WaitAndGetTitle(), expected_title);
  }

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Load another cross-site page into the iframe and check that the load event
  // is fired.
  {
    GURL foo_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
    base::string16 expected_title(base::UTF8ToUTF16("LOADEDLOADED"));
    TitleWatcher title_watcher(shell()->web_contents(), expected_title);
    TestNavigationObserver observer(shell()->web_contents());
    NavigateFrameToURL(root->child_at(0), foo_url);
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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

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
  base::string16 expected_title(base::ASCIIToUTF16("subframe-msg"));
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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

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
  EXPECT_TRUE(ExecuteScript(root->child_at(0), "openPopup('about:blank');"));
  Shell* popup = new_shell_observer.GetShell();
  GURL popup_url(
      embedded_test_server()->GetURL("bar.com", "/post_message.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(popup, popup_url));

  // From the popup, open another popup for baz.com.  This will be used to
  // check that the whole opener chain is processed when creating proxies and
  // not just an immediate opener.
  ShellAddedObserver new_shell_observer2;
  EXPECT_TRUE(ExecuteScript(popup, "openPopup('about:blank');"));
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
          ->GetFrameTree()
          ->root();
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
  EXPECT_TRUE(ExecuteScript(popup, "window.name = 'popup';"));
  PostMessageAndWaitForReply(popup_root, "postToOpener('subframe-msg', '*')",
                             "\"done-popup\"");

  // Second a postMessage from popup2 to window.opener.opener, which should
  // resolve to subframe1.  This tests opener chains of length greater than 1.
  // As before, subframe1 will send a reply to popup2.
  FrameTreeNode* popup2_root =
      static_cast<WebContentsImpl*>(popup2->web_contents())
          ->GetFrameTree()
          ->root();
  EXPECT_TRUE(ExecuteScript(popup2, "window.name = 'popup2';"));
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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
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
  NavigateFrameToURL(child0, b_url);
  NavigateFrameToURL(child1, c_url);
  NavigateFrameToURL(child2, d_url);

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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  TestNavigationObserver observer(shell()->web_contents());

  // Load cross-site page into iframe.
  FrameTreeNode* child = root->child_at(0);
  GURL url = embedded_test_server()->GetURL("foo.com", "/title2.html");
  {
    RenderFrameDeletedObserver deleted_observer(child->current_frame_host());
    NavigateFrameToURL(root->child_at(0), url);
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
    NavigateFrameToURL(child, url);
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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Navigate first child cross-site.
  GURL frame_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  NavigateFrameToURL(root->child_at(0), frame_url);

  // Open a popup from the first child.
  Shell* new_shell =
      OpenPopup(root->child_at(0), GURL(url::kAboutBlankURL), "");
  EXPECT_TRUE(new_shell);

  // Check that the popup's opener is correct on both the browser and renderer
  // sides.
  FrameTreeNode* popup_root =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetFrameTree()
          ->root();
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
          ->GetFrameTree()
          ->root();
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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Navigate first child cross-site.
  GURL frame_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  NavigateFrameToURL(root->child_at(0), frame_url);

  // Open a popup named "foo" from the first child.
  Shell* foo_shell =
      OpenPopup(root->child_at(0), GURL(url::kAboutBlankURL), "foo");
  EXPECT_TRUE(foo_shell);

  // Check that a proxy was created for the "foo" popup in a.com.
  FrameTreeNode* foo_root =
      static_cast<WebContentsImpl*>(foo_shell->web_contents())
          ->GetFrameTree()
          ->root();
  SiteInstance* site_instance_a = root->current_frame_host()->GetSiteInstance();
  RenderFrameProxyHost* popup_rfph_for_a =
      foo_root->render_manager()->GetRenderFrameProxyHost(site_instance_a);
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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Open a same-site popup from the main frame.
  GURL a_com_url(embedded_test_server()->GetURL("a.com", "/title3.html"));
  Shell* a_com_shell = OpenPopup(root->child_at(0), a_com_url, "");
  EXPECT_TRUE(a_com_shell);

  // Navigate first child on main frame cross-site.
  GURL frame_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  NavigateFrameToURL(root->child_at(0), frame_url);

  // Open an unnamed popup from the first child frame.
  Shell* foo_shell =
      OpenPopup(root->child_at(0), GURL(url::kAboutBlankURL), "");
  EXPECT_TRUE(foo_shell);

  // There should be no proxy created for the "foo" popup in a.com, since
  // there's no way for the two a.com frames to access it yet.
  FrameTreeNode* foo_root =
      static_cast<WebContentsImpl*>(foo_shell->web_contents())
          ->GetFrameTree()
          ->root();
  SiteInstance* site_instance_a = root->current_frame_host()->GetSiteInstance();
  EXPECT_FALSE(
      foo_root->render_manager()->GetRenderFrameProxyHost(site_instance_a));

  // Set window.name in the popup's frame.
  EXPECT_TRUE(ExecuteScript(foo_shell, "window.name = 'foo'"));

  // A proxy for the popup should now exist in a.com.
  EXPECT_TRUE(
      foo_root->render_manager()->GetRenderFrameProxyHost(site_instance_a));

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

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
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
          ->GetFrameTree()
          ->root();
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

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Open a popup with a cross-site page that has a subframe.
  GURL popup_url(embedded_test_server()->GetURL(
      "b.com", "/cross_site_iframe_factory.html?b(b)"));
  Shell* popup_shell = OpenPopup(shell(), popup_url, "popup");
  EXPECT_TRUE(popup_shell);
  FrameTreeNode* popup_root =
      static_cast<WebContentsImpl*>(popup_shell->web_contents())
          ->GetFrameTree()
          ->root();
  EXPECT_EQ(1U, popup_root->child_count());

  // Check that the popup's opener is correct in the browser process.
  EXPECT_EQ(root, popup_root->opener());

  // Navigate popup's subframe to another site.
  GURL frame_url(embedded_test_server()->GetURL("c.com", "/post_message.html"));
  NavigateFrameToURL(popup_root->child_at(0), frame_url);

  // Check that the new subframe process still sees correct opener for its
  // parent by sending a postMessage to subframe's parent.opener.
  EXPECT_EQ(true, EvalJs(popup_root->child_at(0), "!!parent.opener;"));

  base::string16 expected_title = base::ASCIIToUTF16("msg");
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

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
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
  NavigateFrameToURL(root->child_at(0), frame_url);

  // Check that the subframe still sees correct opener in its new process.
  EXPECT_EQ(true,
            EvalJs(root->child_at(0), "window.opener === window.parent;"));

  // Navigate second subframe to a new site.  Check that the proxy that's
  // created for the first subframe in the new SiteInstance has correct opener.
  GURL frame2_url(embedded_test_server()->GetURL("qux.com", "/title1.html"));
  NavigateFrameToURL(root->child_at(1), frame2_url);

  EXPECT_EQ(true, EvalJs(root->child_at(1),
                         "parent.frames['frame1'].opener === parent;"));
}

// Check that if a subframe has an opener, that opener is preserved when a new
// RenderFrameProxy is created for that subframe in another renderer process.
// Similar to NavigateSubframeWithOpener, but this test verifies the subframe
// opener plumbing for mojom::Renderer::CreateFrameProxy(), whereas
// NavigateSubframeWithOpener targets mojom::Renderer::CreateFrame().
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       NewRenderFrameProxyPreservesOpener) {
  GURL main_url(
      embedded_test_server()->GetURL("foo.com", "/post_message.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Open a popup with a cross-site page that has two subframes.
  GURL popup_url(embedded_test_server()->GetURL(
      "bar.com", "/frame_tree/page_with_post_message_frames.html"));
  Shell* popup_shell = OpenPopup(shell(), popup_url, "popup");
  EXPECT_TRUE(popup_shell);
  FrameTreeNode* popup_root =
      static_cast<WebContentsImpl*>(popup_shell->web_contents())
          ->GetFrameTree()
          ->root();
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
  NavigateFrameToURL(popup_root->child_at(0), frame_url);

  // Check that the second subframe's opener is still correct in the first
  // subframe's new process.  Verify it both in JS and with a postMessage.
  EXPECT_EQ(true,
            EvalJs(popup_root->child_at(0),
                   "parent.frames['subframe2'].opener && "
                   "    parent.frames['subframe2'].opener === parent.opener;"));

  base::string16 expected_title = base::ASCIIToUTF16("msg");
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);
  EXPECT_EQ(true, EvalJs(popup_root->child_at(0),
                         "postToOpenerOfSibling('subframe2', 'msg', '*');"));
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

// Test for https://crbug.com/515302. Perform two navigations, A1 -> B2 -> A3,
// and drop the FrameHostMsg_Unload_ACK from the A1 -> B2 navigation, so that
// the second B2 -> A3 navigation is initiated before the first page receives
// the FrameHostMsg_Unload_ACK. Ensure that this doesn't crash and that the
// RVH(A1) is not reused in that case.
#if defined(OS_MAC)
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

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  RenderFrameHostImpl* rfh = root->current_frame_host();
  RenderViewHostImpl* rvh = rfh->render_view_host();
  int rvh_routing_id = rvh->GetRoutingID();
  int rvh_process_id = rvh->GetProcess()->GetID();
  SiteInstanceImpl* site_instance = rfh->GetSiteInstance();
  RenderFrameDeletedObserver deleted_observer(rfh);

  // Install a BrowserMessageFilter to drop FrameHostMsg_Unload_ACK messages in
  // A's process.
  auto filter = base::MakeRefCounted<DropMessageFilter>(
      FrameMsgStart, FrameHostMsg_Unload_ACK::ID);
  rfh->GetProcess()->AddFilter(filter.get());
  rfh->DisableUnloadTimerForTesting();

  // Navigate to B.  This must wait for DidCommitProvisionalLoad and not
  // DidStopLoading, so that the Unload timer doesn't call OnUnloaded and
  // destroy |rfh| and |rvh| before they are checked in the test.
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  TestFrameNavigationObserver commit_observer(root);
  EXPECT_TRUE(ExecuteScript(shell(), JsReplace("location = $1", b_url)));
  commit_observer.WaitForCommit();
  EXPECT_FALSE(deleted_observer.deleted());

  // The previous RFH should be either:
  // 1) In the BackForwardCache, if back-forward cache is enabled.
  // 2) Pending deletion otherwise, since the FrameHostMsg_Unload_ACK for A->B
  // is dropped.
  EXPECT_THAT(
      rfh->lifecycle_state(),
      testing::AnyOf(
          testing::Eq(
              RenderFrameHostImpl::LifecycleState::kRunningUnloadHandlers),
          testing::Eq(
              RenderFrameHostImpl::LifecycleState::kInBackForwardCache)));

  // Without the FrameHostMsg_Unload_ACK and timer, the process A will never
  // shutdown. Simulate the process being killed now.
  content::RenderProcessHostWatcher crash_observer(
      rvh->GetProcess(),
      content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  EXPECT_TRUE(rvh->GetProcess()->Shutdown(0));
  crash_observer.Wait();

  // Verify that the RVH and RFH for A were cleaned up.
  EXPECT_FALSE(root->frame_tree()->GetRenderViewHost(site_instance));
  EXPECT_TRUE(deleted_observer.deleted());

  // Start a navigation back to A, being careful to stay in the same
  // BrowsingInstance, and check that the RenderViewHost wasn't reused.
  TestNavigationObserver navigation_observer(shell()->web_contents());
  shell()->LoadURLForFrame(a_url, std::string(),
                           ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK));
  RenderViewHostImpl* pending_rvh =
      root->render_manager()->speculative_frame_host()->render_view_host();

  // When ProactivelySwapBrowsingInstance A1 and A3 aren't using the same
  // BrowsingInstance.
  if (CanCrossSiteNavigationsProactivelySwapBrowsingInstances())
    EXPECT_NE(site_instance, pending_rvh->GetSiteInstance());
  else
    EXPECT_EQ(site_instance, pending_rvh->GetSiteInstance());

  EXPECT_FALSE(rvh_routing_id == pending_rvh->GetRoutingID() &&
               rvh_process_id == pending_rvh->GetProcess()->GetID());

  // Make sure the last navigation finishes without crashing.
  navigation_observer.Wait();
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
  EXPECT_TRUE(ExecuteScript(shell(), script));

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

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  // Focus the main frame's text field.  The return value "input-focus"
  // indicates that the focus event was fired correctly.
  std::string result;
  EXPECT_TRUE(
      ExecuteScriptAndExtractString(shell(), "focusInputField()", &result));
  EXPECT_EQ(result, "input-focus");

  // The main frame should be focused.
  EXPECT_EQ(root, root->frame_tree()->GetFocusedFrame());

  DOMMessageQueue msg_queue;

  // Click on the cross-process subframe.
  SimulateMouseClick(
      root->child_at(0)->current_frame_host()->GetRenderWidgetHost(), 1, 1);

  // Check that the main frame lost focus and fired blur event on the input
  // text field.
  std::string status;
  while (msg_queue.WaitForMessage(&status)) {
    if (status == "\"input-blur\"")
      break;
  }

  // The subframe should now be focused.
  EXPECT_EQ(root->child_at(0), root->frame_tree()->GetFocusedFrame());

  // Click on the root frame.
  SimulateMouseClick(shell()->web_contents()->GetRenderViewHost()->GetWidget(),
                     1, 1);

  // Check that the subframe lost focus and fired blur event on its
  // document's body.
  while (msg_queue.WaitForMessage(&status)) {
    if (status == "\"document-blur\"")
      break;
  }

  // The root frame should be focused again.
  EXPECT_EQ(root, root->frame_tree()->GetFocusedFrame());
}

// Check that when a cross-process subframe is focused, its parent's
// document.activeElement correctly returns the corresponding <iframe> element.
// The test sets up an A-embed-B-embed-C page and shifts focus A->B->A->C,
// checking document.activeElement after each change.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, DocumentActiveElement) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

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
  EXPECT_EQ(root, root->frame_tree()->GetFocusedFrame());

  // Focus the b.com frame.
  FocusFrame(child);
  EXPECT_EQ(child, root->frame_tree()->GetFocusedFrame());

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
  EXPECT_EQ(root, root->frame_tree()->GetFocusedFrame());

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

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

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
  EXPECT_EQ(root, root->frame_tree()->GetFocusedFrame());

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

  EXPECT_EQ(child1, root->frame_tree()->GetFocusedFrame());

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
  EXPECT_EQ(child2, root->frame_tree()->GetFocusedFrame());

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
  EXPECT_EQ(root, root->frame_tree()->GetFocusedFrame());
}

// Check that when a subframe has focus, and another subframe navigates
// cross-site to a new renderer process, this doesn't reset the focused frame
// to the main frame.  See https://crbug.com/802156.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       SubframeFocusNotLostWhenAnotherFrameNavigatesCrossSite) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a,a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child1 = root->child_at(0);
  FrameTreeNode* child2 = root->child_at(1);

  // The main frame should be focused to start with.
  EXPECT_EQ(root, root->frame_tree()->GetFocusedFrame());

  // Add an <input> element to the first subframe.
  ExecuteScriptAsync(
      child1, "document.body.appendChild(document.createElement('input'))");

  // Focus the first subframe using window.focus().
  FrameFocusedObserver focus_observer(child1->current_frame_host());
  ExecuteScriptAsync(root, "frames[0].focus()");
  focus_observer.Wait();
  EXPECT_EQ(child1, root->frame_tree()->GetFocusedFrame());

  // Give focus to the <input> element in the first subframe.
  ExecuteScriptAsync(child1, "document.querySelector('input').focus()");

  // Now, navigate second subframe cross-site.  Ensure that this won't change
  // the focused frame.
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  NavigateFrameToURL(child2, b_url);
  // This is needed because the incorrect focused frame change as in
  // https://crbug.com/802156 requires an additional post-commit IPC roundtrip.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(child1, root->frame_tree()->GetFocusedFrame());

  // The <input> in first subframe should still be the activeElement.
  std::string activeTag;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      child1, "domAutomationController.send(document.activeElement.tagName)",
      &activeTag));
  EXPECT_EQ("input", base::ToLowerASCII(activeTag));
}

// Tests that we are using the correct RenderFrameProxy when navigating an
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
  EXPECT_TRUE(ExecuteScript(
      popup, JsReplace("window.opener.location.href = $1", cross_url)));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), cross_url);
}

// Test for https://crbug.com/526304, where a parent frame executes a
// remote-to-local navigation on a child frame and immediately removes the same
// child frame.  This test exercises the path where the detach happens before
// the provisional local frame is created.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       NavigateProxyAndDetachBeforeProvisionalFrameCreation) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContents* contents = shell()->web_contents();
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(contents)->GetFrameTree()->root();
  EXPECT_EQ(2U, root->child_count());

  // Navigate the first child frame to 'about:blank' (which is a
  // remote-to-local transition), and then detach it.
  FrameDeletedObserver observer(root->child_at(0)->current_frame_host());
  std::string script =
      "var f = document.querySelector('iframe');"
      "f.contentWindow.location.href = 'about:blank';"
      "setTimeout(function() { document.body.removeChild(f); }, 0);";
  EXPECT_TRUE(ExecuteScript(root, script));
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
      static_cast<WebContentsImpl*>(contents)->GetFrameTree()->root();
  EXPECT_EQ(2U, root->child_count());
  FrameTreeNode* child = root->child_at(0);

  // Start a remote-to-local navigation for the child, but don't wait for
  // commit.
  GURL same_site_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  NavigationController::LoadURLParams params(same_site_url);
  params.transition_type = ui::PAGE_TRANSITION_LINK;
  params.frame_tree_node_id = child->frame_tree_node_id();
  child->navigator().GetController()->LoadURLWithParams(params);

  // Tell parent to remove the first child.  This should happen after the
  // previous navigation starts but before it commits.
  FrameDeletedObserver observer(child->current_frame_host());
  EXPECT_TRUE(ExecuteScript(
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
      static_cast<WebContentsImpl*>(contents)->GetFrameTree()->root();
  EXPECT_EQ(1U, root->child_count());
  FrameTreeNode* child = root->child_at(0);
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());

  // Navigate the child frame to "about:blank" from the parent document and
  // wait for it to be removed.
  FrameDeletedObserver observer(child->current_frame_host());
  EXPECT_TRUE(ExecuteScript(
      root, base::StringPrintf("f.src = '%s'", url::kAboutBlankURL)));
  observer.Wait();

  // Make sure the a.com renderer does not crash and the frame is removed.
  EXPECT_EQ(0, EvalJs(root, "frames.length;"));
}

// Test for https://crbug.com/568670.  In A-embed-B, simultaneously have B
// create a new (local) child frame, and have A detach B's proxy.  The child
// frame creation sends an IPC to create a new proxy in A's process, and if
// that IPC arrives after the detach, the new frame's parent (a proxy) won't be
// available, and this shouldn't cause RenderFrameProxy::CreateFrameProxy to
// crash.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       RaceBetweenCreateChildFrameAndDetachParentProxy) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContents* contents = shell()->web_contents();
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(contents)->GetFrameTree()->root();

  // Simulate subframe B creating a new child frame in parallel to main frame A
  // detaching subframe B.  We can't use ExecuteScript in both A and B to do
  // this simultaneously, as that won't guarantee the timing that we want.
  // Instead, tell A to detach B and then send a fake proxy creation IPC to A
  // that would've come from create-child-frame code in B.  Prepare parameters
  // for that IPC ahead of the detach, while B's FrameTreeNode still exists.
  SiteInstance* site_instance_a = root->current_frame_host()->GetSiteInstance();
  RenderProcessHost* process_a =
      root->render_manager()->current_frame_host()->GetProcess();
  AgentSchedulingGroupHost* agent_scheduling_group_a =
      AgentSchedulingGroupHost::Get(*site_instance_a, *process_a);
  int new_routing_id = process_a->GetNextRoutingID();
  int view_routing_id =
      root->frame_tree()->GetRenderViewHost(site_instance_a)->GetRoutingID();
  int parent_routing_id =
      root->child_at(0)->render_manager()->GetProxyToParent()->GetRoutingID();

  // Tell main frame A to delete its subframe B.
  FrameDeletedObserver observer(root->child_at(0)->current_frame_host());
  EXPECT_TRUE(ExecuteScript(
      root, "document.body.removeChild(document.querySelector('iframe'));"));

  // Send the message to create a proxy for B's new child frame in A.  This
  // used to crash, as parent_routing_id refers to a proxy that doesn't exist
  // anymore.
  agent_scheduling_group_a->CreateFrameProxy(
      new_routing_id, view_routing_id, base::nullopt, parent_routing_id,
      FrameReplicationState(), base::UnguessableToken::Create(),
      base::UnguessableToken::Create());

  // Ensure the subframe is detached in the browser process.
  observer.Wait();
  EXPECT_EQ(0U, root->child_count());

  // Make sure process A did not crash.
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
  FrameTreeNode* node = web_contents()->GetFrameTree()->root()->child_at(0);
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
    node->navigator().GetController()->LoadURLWithParams(params);
    EXPECT_TRUE(stalled_navigation.WaitForResponse());
  }

  // Grab the routing id of the pending RenderFrameHost and set up a process
  // observer to ensure there is no crash when a new RenderFrame creation is
  // attempted.
  RenderProcessHost* process =
      node->render_manager()->speculative_frame_host()->GetProcess();
  AgentSchedulingGroupHost* agent_scheduling_group =
      AgentSchedulingGroupHost::Get(
          *node->render_manager()->speculative_frame_host()->GetSiteInstance(),
          *process);
  RenderProcessHostWatcher watcher(
      process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  int frame_routing_id =
      node->render_manager()->speculative_frame_host()->GetRoutingID();
  base::UnguessableToken frame_token =
      node->render_manager()->speculative_frame_host()->GetFrameToken();
  int previous_routing_id =
      node->render_manager()->GetProxyToParent()->GetRoutingID();

  // Now go to c.com so the navigation to a.com is cancelled and send an IPC
  // to create a new RenderFrame with the routing id of the previously pending
  // one.
  NavigateFrameToURL(node,
                     embedded_test_server()->GetURL("c.com", "/title2.html"));
  {
    mojom::CreateFrameParamsPtr params = mojom::CreateFrameParams::New();
    params->routing_id = frame_routing_id;
    params->interface_bundle = mojom::DocumentScopedInterfaceBundle::New();
    ignore_result(params->interface_bundle->interface_provider
                      .InitWithNewPipeAndPassReceiver());
    ignore_result(params->interface_bundle->browser_interface_broker
                      .InitWithNewPipeAndPassReceiver());
    params->previous_routing_id = previous_routing_id;
    params->opener_frame_token = base::nullopt;
    params->parent_routing_id =
        shell()->web_contents()->GetMainFrame()->GetRoutingID();
    params->previous_sibling_routing_id = IPC::mojom::kRoutingIdNone;
    params->frame_owner_properties = blink::mojom::FrameOwnerProperties::New();
    params->frame_token = frame_token;
    params->devtools_frame_token = base::UnguessableToken::Create();
    agent_scheduling_group->CreateFrame(std::move(params));
  }

  // Disable the BackForwardCache to ensure the old process is going to be
  // released.
  DisableBackForwardCacheForTesting(web_contents(),
                                    BackForwardCache::TEST_ASSUMES_NO_CACHING);

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
  EXPECT_EQ(2U, contents->GetFrameTree()->root()->child_count());

  // Capture the FrameTreeNode this test will be navigating.
  FrameTreeNode* node = contents->GetFrameTree()->root()->child_at(0);
  EXPECT_TRUE(node);
  EXPECT_NE(node->current_frame_host()->GetSiteInstance(),
            node->parent()->GetSiteInstance());

  // Grab the routing id of the first child RenderFrameHost and set up a process
  // observer to ensure there is no crash when a new RenderFrame creation is
  // attempted.
  RenderProcessHost* process = node->current_frame_host()->GetProcess();
  AgentSchedulingGroupHost* agent_scheduling_group =
      AgentSchedulingGroupHost::Get(
          *node->current_frame_host()->GetSiteInstance(), *process);
  RenderProcessHostWatcher watcher(
      process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  int frame_routing_id = node->current_frame_host()->GetRoutingID();
  base::UnguessableToken frame_token =
      node->current_frame_host()->GetFrameToken();
  int widget_routing_id =
      node->current_frame_host()->GetRenderWidgetHost()->GetRoutingID();
  int parent_routing_id =
      node->parent()
          ->frame_tree_node()
          ->render_manager()
          ->GetRoutingIdForSiteInstance(
              node->current_frame_host()->GetSiteInstance());

  // Have the parent frame remove the child frame from its DOM. This should
  // result in the child RenderFrame being deleted in the remote process.
  EXPECT_TRUE(ExecuteScript(contents,
                            "document.body.removeChild("
                            "document.querySelectorAll('iframe')[0])"));
  EXPECT_EQ(1U, contents->GetFrameTree()->root()->child_count());

  {
    mojom::CreateFrameParamsPtr params = mojom::CreateFrameParams::New();
    params->routing_id = frame_routing_id;
    params->interface_bundle = mojom::DocumentScopedInterfaceBundle::New();
    ignore_result(params->interface_bundle->interface_provider
                      .InitWithNewPipeAndPassReceiver());
    ignore_result(params->interface_bundle->browser_interface_broker
                      .InitWithNewPipeAndPassReceiver());
    params->previous_routing_id = IPC::mojom::kRoutingIdNone;
    params->opener_frame_token = base::nullopt;
    params->parent_routing_id = parent_routing_id;
    params->previous_sibling_routing_id = IPC::mojom::kRoutingIdNone;
    params->frame_owner_properties = blink::mojom::FrameOwnerProperties::New();
    params->widget_params = mojom::CreateFrameWidgetParams::New();
    params->widget_params->routing_id = widget_routing_id;
    mojo::PendingAssociatedRemote<blink::mojom::FrameWidget> blink_frame_widget;
    params->widget_params->frame_widget =
        blink_frame_widget.InitWithNewEndpointAndPassReceiver();
    mojo::PendingAssociatedRemote<blink::mojom::Widget> blink_widget;
    params->widget_params->widget =
        blink_widget.InitWithNewEndpointAndPassReceiver();
    ignore_result(params->widget_params->frame_widget_host
                      .InitWithNewEndpointAndPassReceiver());
    ignore_result(params->widget_params->widget_host
                      .InitWithNewEndpointAndPassReceiver());
    params->replication_state.name = "name";
    params->replication_state.unique_name = "name";
    params->frame_token = frame_token;
    params->devtools_frame_token = base::UnguessableToken::Create();
    agent_scheduling_group->CreateFrame(std::move(params));
  }

  // The test must wait for the process to exit, but if there is no leak, the
  // RenderFrame will be properly created and there will be no crash.
  // Therefore, navigate the remaining subframe to completely different site,
  // which will cause the original process to exit cleanly.
  NavigateFrameToURL(contents->GetFrameTree()->root()->child_at(0),
                     embedded_test_server()->GetURL("d.com", "/title3.html"));
  watcher.Wait();
  EXPECT_TRUE(watcher.did_exit_normally());
}

// TODO(ekaramad): Move this test out of this file when addressing
// https://crbug.com/754726.
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
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, FindImmediateLocalRoots) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com",
      "/cross_site_iframe_factory.html?a(a(b(d),a(d)),b(b(c,c)),a(a(c),c))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Each entry is of the frame "LABEL:ILR1ILR2..." where ILR stands for
  // immediate local root.
  std::string immediate_local_roots[] = {
      "A0:B1B2C2D2C5", "A1:B2D2", "B1:C3C4", "A2:C2C5", "B2:D1",
      "A3:D2",         "B3:C3C4", "A4:C5",   "C2:",     "D1:",
      "D2:",           "C3:",     "C4:",     "C5:"};

  std::map<RenderFrameHostImpl*, std::string>
      frame_to_immediate_local_roots_map;
  std::map<RenderFrameHostImpl*, std::string> frame_to_label_map;
  size_t index = 0;
  // Map each RenderFrameHostImpl to its label and set of immediate local roots.
  for (auto* ftn : web_contents()->GetFrameTree()->Nodes()) {
    std::string roots = immediate_local_roots[index++];
    frame_to_immediate_local_roots_map[ftn->current_frame_host()] = roots;
    frame_to_label_map[ftn->current_frame_host()] = roots.substr(0, 2);
  }

  // For each frame in the tree, verify that ForEachImmediateLocalRoot properly
  // visits each and only each immediate local root in a BFS traversal order.
  for (auto* ftn : web_contents()->GetFrameTree()->Nodes()) {
    RenderFrameHostImpl* current_frame_host = ftn->current_frame_host();
    std::list<RenderFrameHostImpl*> frame_list;
    current_frame_host->ForEachImmediateLocalRoot(base::BindRepeating(
        [](std::list<RenderFrameHostImpl*>* ilr_list,
           RenderFrameHostImpl* rfh) { ilr_list->push_back(rfh); },
        &frame_list));

    std::string result = frame_to_label_map[current_frame_host];
    result.append(":");
    for (auto* ilr_ptr : frame_list)
      result.append(frame_to_label_map[ilr_ptr]);
    EXPECT_EQ(frame_to_immediate_local_roots_map[current_frame_host], result);
  }
}

// This test verifies that changing the CSS visibility of a cross-origin
// <iframe> is forwarded to its corresponding RenderWidgetHost and all other
// RenderWidgetHosts corresponding to the nested cross-origin frame.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, CSSVisibilityChanged) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(b(c(d(d(a))))))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Find all child RenderWidgetHosts.
  std::vector<RenderWidgetHostImpl*> child_widget_hosts;
  FrameTreeNode* first_cross_process_child =
      web_contents()->GetFrameTree()->root()->child_at(0);
  for (auto* ftn : web_contents()->GetFrameTree()->SubtreeNodes(
           first_cross_process_child)) {
    RenderFrameHostImpl* frame_host = ftn->current_frame_host();
    if (!frame_host->is_local_root())
      continue;

    child_widget_hosts.push_back(frame_host->GetRenderWidgetHost());
  }

  // Ignoring the root, there is exactly 4 local roots and hence 5
  // RenderWidgetHosts on the page.
  EXPECT_EQ(4U, child_widget_hosts.size());

  // Initially all the RenderWidgetHosts should be visible.
  for (size_t index = 0; index < child_widget_hosts.size(); ++index) {
    EXPECT_FALSE(child_widget_hosts[index]->is_hidden())
        << "The RWH at distance " << index + 1U
        << " from root RWH should not be hidden.";
  }

  std::string show_script =
      "document.querySelector('iframe').style.visibility = 'visible';";
  std::string hide_script =
      "document.querySelector('iframe').style.visibility = 'hidden';";

  // Define observers for notifications about hiding child RenderWidgetHosts.
  std::vector<std::unique_ptr<RenderWidgetHostVisibilityObserver>>
      hide_widget_host_observers(child_widget_hosts.size());
  for (size_t index = 0U; index < child_widget_hosts.size(); ++index) {
    hide_widget_host_observers[index].reset(
        new RenderWidgetHostVisibilityObserver(child_widget_hosts[index],
                                               false));
  }

  EXPECT_TRUE(ExecuteScript(shell(), hide_script));
  for (size_t index = 0U; index < child_widget_hosts.size(); ++index) {
    EXPECT_TRUE(hide_widget_host_observers[index]->WaitUntilSatisfied())
        << "Expected RenderWidgetHost at distance " << index + 1U
        << " from root RenderWidgetHost to become hidden.";
  }

  // Define observers for notifications about showing child RenderWidgetHosts.
  std::vector<std::unique_ptr<RenderWidgetHostVisibilityObserver>>
      show_widget_host_observers(child_widget_hosts.size());
  for (size_t index = 0U; index < child_widget_hosts.size(); ++index) {
    show_widget_host_observers[index].reset(
        new RenderWidgetHostVisibilityObserver(child_widget_hosts[index],
                                               true));
  }

  EXPECT_TRUE(ExecuteScript(shell(), show_script));
  for (size_t index = 0U; index < child_widget_hosts.size(); ++index) {
    EXPECT_TRUE(show_widget_host_observers[index]->WaitUntilSatisfied())
        << "Expected RenderWidgetHost at distance " << index + 1U
        << " from root RenderWidgetHost to become shown.";
  }
}

// This test verifies that hiding an OOPIF in CSS will stop generating
// compositor frames for the OOPIF and any nested OOPIFs inside it. This holds
// even when the whole page is shown.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       HiddenOOPIFWillNotGenerateCompositorFrames) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_two_frames.html"));
  ASSERT_TRUE(NavigateToURL(shell(), main_url));
  ASSERT_EQ(shell()->web_contents()->GetLastCommittedURL(), main_url);

  GURL cross_site_url_b =
      embedded_test_server()->GetURL("b.com", "/counter.html");

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  NavigateFrameToURL(root->child_at(0), cross_site_url_b);

  NavigateFrameToURL(root->child_at(1), cross_site_url_b);

  // Now inject code in the first frame to create a nested OOPIF.
  RenderFrameHostCreatedObserver new_frame_created_observer(
      shell()->web_contents(), 1);
  ASSERT_TRUE(ExecuteScript(
      root->child_at(0)->current_frame_host(),
      "document.body.appendChild(document.createElement('iframe'));"));
  new_frame_created_observer.Wait();

  GURL cross_site_url_a =
      embedded_test_server()->GetURL("a.com", "/counter.html");

  // Navigate the nested frame.
  TestFrameNavigationObserver observer(root->child_at(0)->child_at(0));
  ASSERT_TRUE(
      ExecuteScript(root->child_at(0)->current_frame_host(),
                    JsReplace("document.querySelector('iframe').src = $1",
                              cross_site_url_a)));
  observer.Wait();

  RenderWidgetHostViewChildFrame* first_child_view =
      static_cast<RenderWidgetHostViewChildFrame*>(
          root->child_at(0)->current_frame_host()->GetView());
  RenderWidgetHostViewChildFrame* second_child_view =
      static_cast<RenderWidgetHostViewChildFrame*>(
          root->child_at(1)->current_frame_host()->GetView());
  RenderWidgetHostViewChildFrame* nested_child_view =
      static_cast<RenderWidgetHostViewChildFrame*>(
          root->child_at(0)->child_at(0)->current_frame_host()->GetView());

  RenderFrameSubmissionObserver first_frame_counter(
      first_child_view->host_->render_frame_metadata_provider());
  RenderFrameSubmissionObserver second_frame_counter(
      second_child_view->host_->render_frame_metadata_provider());
  RenderFrameSubmissionObserver third_frame_counter(
      nested_child_view->host_->render_frame_metadata_provider());

  const int kFrameCountLimit = 20;

  // Wait for a minimum number of compositor frames for the second frame.
  while (second_frame_counter.render_frame_count() < kFrameCountLimit)
    second_frame_counter.WaitForAnyFrameSubmission();
  ASSERT_LE(kFrameCountLimit, second_frame_counter.render_frame_count());

  // Now make sure all frames have roughly the counter value in the sense that
  // no counter value is more than twice any other.
  float ratio = static_cast<float>(first_frame_counter.render_frame_count()) /
                static_cast<float>(second_frame_counter.render_frame_count());
  EXPECT_GT(2.5f, ratio + 1 / ratio) << "Ratio is: " << ratio;

  ratio = static_cast<float>(first_frame_counter.render_frame_count()) /
          static_cast<float>(third_frame_counter.render_frame_count());
  EXPECT_GT(2.5f, ratio + 1 / ratio) << "Ratio is: " << ratio;

  // Make sure all views can become visible.
  EXPECT_TRUE(first_child_view->CanBecomeVisible());
  EXPECT_TRUE(second_child_view->CanBecomeVisible());
  EXPECT_TRUE(nested_child_view->CanBecomeVisible());

  // Hide the first frame and wait for the notification to be posted by its
  // RenderWidgetHost.
  RenderWidgetHostVisibilityObserver hide_observer(
      root->child_at(0)->current_frame_host()->GetRenderWidgetHost(), false);

  // Hide the first frame.
  ASSERT_TRUE(ExecuteScript(
      shell(),
      "document.getElementsByName('frame1')[0].style.visibility = 'hidden'"));
  ASSERT_TRUE(hide_observer.WaitUntilSatisfied());
  EXPECT_TRUE(first_child_view->FrameConnectorForTesting()->IsHidden());

  // Verify that only the second view can become visible now.
  EXPECT_FALSE(first_child_view->CanBecomeVisible());
  EXPECT_TRUE(second_child_view->CanBecomeVisible());
  EXPECT_FALSE(nested_child_view->CanBecomeVisible());

  // Now hide and show the WebContents (to simulate a tab switch).
  shell()->web_contents()->WasHidden();
  shell()->web_contents()->WasShown();

  first_frame_counter.ResetCounter();
  second_frame_counter.ResetCounter();
  third_frame_counter.ResetCounter();

  // We expect the second counter to keep running.
  while (second_frame_counter.render_frame_count() < kFrameCountLimit)
    second_frame_counter.WaitForAnyFrameSubmission();
  ASSERT_LT(kFrameCountLimit, second_frame_counter.render_frame_count() + 1);

  // Verify that the counter for other two frames did not count much.
  ratio = static_cast<float>(first_frame_counter.render_frame_count()) /
          static_cast<float>(second_frame_counter.render_frame_count());
  EXPECT_GT(0.5f, ratio) << "Ratio is: " << ratio;

  ratio = static_cast<float>(third_frame_counter.render_frame_count()) /
          static_cast<float>(second_frame_counter.render_frame_count());
  EXPECT_GT(0.5f, ratio) << "Ratio is: " << ratio;
}

// This test verifies that navigating a hidden OOPIF to cross-origin will not
// lead to creating compositor frames for the new OOPIF renderer.
IN_PROC_BROWSER_TEST_P(
    SitePerProcessBrowserTest,
    HiddenOOPIFWillNotGenerateCompositorFramesAfterNavigation) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_two_frames.html"));
  ASSERT_TRUE(NavigateToURL(shell(), main_url));
  ASSERT_EQ(shell()->web_contents()->GetLastCommittedURL(), main_url);

  GURL cross_site_url_b =
      embedded_test_server()->GetURL("b.com", "/counter.html");

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  NavigateFrameToURL(root->child_at(0), cross_site_url_b);

  NavigateFrameToURL(root->child_at(1), cross_site_url_b);

  // Hide the first frame and wait for the notification to be posted by its
  // RenderWidgetHost.
  RenderWidgetHostVisibilityObserver hide_observer(
      root->child_at(0)->current_frame_host()->GetRenderWidgetHost(), false);

  // Hide the first frame.
  ASSERT_TRUE(ExecuteScript(
      shell(),
      "document.getElementsByName('frame1')[0].style.visibility = 'hidden'"));
  ASSERT_TRUE(hide_observer.WaitUntilSatisfied());

  // Now navigate the first frame to another OOPIF process.
  TestFrameNavigationObserver navigation_observer(
      root->child_at(0)->current_frame_host());
  GURL cross_site_url_c =
      embedded_test_server()->GetURL("c.com", "/counter.html");
  ASSERT_TRUE(ExecuteScript(
      web_contents(),
      JsReplace("document.getElementsByName('frame1')[0].src = $1",
                cross_site_url_c)));
  navigation_observer.Wait();

  // Now investigate compositor frame creation.
  RenderWidgetHostViewChildFrame* first_child_view =
      static_cast<RenderWidgetHostViewChildFrame*>(
          root->child_at(0)->current_frame_host()->GetView());

  RenderWidgetHostViewChildFrame* second_child_view =
      static_cast<RenderWidgetHostViewChildFrame*>(
          root->child_at(1)->current_frame_host()->GetView());

  EXPECT_FALSE(first_child_view->CanBecomeVisible());

  RenderFrameSubmissionObserver first_frame_counter(
      first_child_view->host_->render_frame_metadata_provider());
  RenderFrameSubmissionObserver second_frame_counter(
      second_child_view->host_->render_frame_metadata_provider());

  const int kFrameCountLimit = 20;

  // Wait for a certain number of swapped compositor frames generated for the
  // second child view. During the same interval the first frame should not have
  // swapped any compositor frames.
  while (second_frame_counter.render_frame_count() < kFrameCountLimit)
    second_frame_counter.WaitForAnyFrameSubmission();
  ASSERT_LT(kFrameCountLimit, second_frame_counter.render_frame_count() + 1);

  float ratio = static_cast<float>(first_frame_counter.render_frame_count()) /
                static_cast<float>(second_frame_counter.render_frame_count());
  EXPECT_GT(0.5f, ratio) << "Ratio is: " << ratio;
}

// Verify that sandbox flags inheritance works across multiple levels of
// frames.  See https://crbug.com/576845.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, SandboxFlagsInheritance) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Set sandbox flags for child frame.
  EXPECT_TRUE(ExecuteScript(
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
  NavigateFrameToURL(root->child_at(0), frame_url);

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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Set sandbox flags for child frame.
  EXPECT_TRUE(ExecuteScript(
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
  EXPECT_TRUE(ExecuteScript(
      child, "document.body.appendChild(document.createElement('iframe'));"));
  frame_observer.Wait();

  FrameTreeNode* grandchild = child->child_at(0);
  GURL frame_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  TestFrameNavigationObserver navigation_observer(grandchild);
  NavigateFrameToURL(grandchild, frame_url);
  navigation_observer.Wait();

  // Since the update flags haven't yet taken effect in its parent, this
  // grandchild frame should not be sandboxed.
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            grandchild->pending_frame_policy().sandbox_flags);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            grandchild->effective_frame_policy().sandbox_flags);

  // Check that the grandchild frame isn't sandboxed on the renderer side.  If
  // sandboxed, its origin would be unique ("null").
  std::string expected_origin = url::Origin::Create(frame_url).Serialize();
  EXPECT_EQ(expected_origin, GetOriginFromRenderer(grandchild));
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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

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
      ~network::mojom::WebSandboxFlags::kScripts &
      ~network::mojom::WebSandboxFlags::kAutomaticFeatures &
      ~network::mojom::WebSandboxFlags::kPopups;
  EXPECT_EQ(expected_flags,
            root->child_at(0)->pending_frame_policy().sandbox_flags);

  // Navigate child frame cross-site.  The sandbox flags should take effect.
  GURL frame_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  TestFrameNavigationObserver frame_observer(root->child_at(0));
  NavigateFrameToURL(root->child_at(0), frame_url);
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
          ->GetFrameTree()
          ->root();

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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Set sandbox flags for child frame, specifying that popups opened from it
  // should not be sandboxed.
  EXPECT_TRUE(ExecuteScript(
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
      ~network::mojom::WebSandboxFlags::kPropagatesToAuxiliaryBrowsingContexts;
  EXPECT_EQ(expected_flags,
            root->child_at(0)->pending_frame_policy().sandbox_flags);

  // Navigate child frame cross-site.  The sandbox flags should take effect.
  GURL frame_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  TestFrameNavigationObserver frame_observer(root->child_at(0));
  NavigateFrameToURL(root->child_at(0), frame_url);
  frame_observer.Wait();
  EXPECT_EQ(expected_flags,
            root->child_at(0)->effective_frame_policy().sandbox_flags);

  // Open a cross-site popup named "foo" from the child frame.
  GURL b_url(embedded_test_server()->GetURL("c.com", "/title1.html"));
  Shell* foo_shell = OpenPopup(root->child_at(0), b_url, "foo");
  EXPECT_TRUE(foo_shell);

  FrameTreeNode* foo_root =
      static_cast<WebContentsImpl*>(foo_shell->web_contents())
          ->GetFrameTree()
          ->root();

  // Check that the sandbox flags for new popup are correct in the browser
  // process.  They should not have been inherited.
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            foo_root->effective_frame_policy().sandbox_flags);

  // The popup's origin should match |b_url|, since it's not sandboxed.
  EXPECT_EQ(url::Origin::Create(b_url).Serialize(),
            EvalJs(foo_root, "self.origin;"));
}

// Tests that the WebContents is notified when passive mixed content is
// displayed in an OOPIF. The test ignores cert errors so that an HTTPS
// iframe can be loaded from a site other than localhost (the
// EmbeddedTestServer serves a certificate that is valid for localhost).
// This test crashes on Windows under Dr. Memory, see https://crbug.com/600942.
#if defined(OS_WIN)
#define MAYBE_PassiveMixedContentInIframe DISABLED_PassiveMixedContentInIframe
#else
#define MAYBE_PassiveMixedContentInIframe PassiveMixedContentInIframe
#endif
IN_PROC_BROWSER_TEST_P(SitePerProcessIgnoreCertErrorsBrowserTest,
                       MAYBE_PassiveMixedContentInIframe) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  SetupCrossSiteRedirector(&https_server);
  ASSERT_TRUE(https_server.Start());

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  GURL iframe_url(
      https_server.GetURL("/mixed-content/basic-passive-in-iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), iframe_url));
  NavigationEntry* entry = web_contents->GetController().GetVisibleEntry();
  EXPECT_TRUE(!!(entry->GetSSL().content_status &
                 SSLStatus::DISPLAYED_INSECURE_CONTENT));

  // When the subframe navigates, the WebContents should still be marked
  // as having displayed insecure content.
  GURL navigate_url(https_server.GetURL("/title1.html"));
  FrameTreeNode* root = web_contents->GetFrameTree()->root();
  NavigateFrameToURL(root->child_at(0), navigate_url);
  entry = web_contents->GetController().GetVisibleEntry();
  EXPECT_TRUE(!!(entry->GetSSL().content_status &
                 SSLStatus::DISPLAYED_INSECURE_CONTENT));

  // When the main frame navigates, it should no longer be marked as
  // displaying insecure content.
  EXPECT_TRUE(
      NavigateToURL(shell(), https_server.GetURL("b.com", "/title1.html")));
  entry = web_contents->GetController().GetVisibleEntry();
  EXPECT_FALSE(!!(entry->GetSSL().content_status &
                  SSLStatus::DISPLAYED_INSECURE_CONTENT));
}

// Tests that, when a parent frame is set to strictly block mixed
// content via Content Security Policy, child OOPIFs cannot display
// mixed content.
IN_PROC_BROWSER_TEST_P(SitePerProcessIgnoreCertErrorsBrowserTest,
                       PassiveMixedContentInIframeWithStrictBlocking) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  SetupCrossSiteRedirector(&https_server);
  ASSERT_TRUE(https_server.Start());

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  GURL iframe_url_with_strict_blocking(https_server.GetURL(
      "/mixed-content/basic-passive-in-iframe-with-strict-blocking.html"));
  EXPECT_TRUE(NavigateToURL(shell(), iframe_url_with_strict_blocking));
  NavigationEntry* entry = web_contents->GetController().GetVisibleEntry();
  EXPECT_FALSE(!!(entry->GetSSL().content_status &
                  SSLStatus::DISPLAYED_INSECURE_CONTENT));

  FrameTreeNode* root = web_contents->GetFrameTree()->root();
  EXPECT_EQ(blink::mojom::InsecureRequestPolicy::kBlockAllMixedContent,
            root->current_replication_state().insecure_request_policy);
  EXPECT_EQ(
      blink::mojom::InsecureRequestPolicy::kBlockAllMixedContent,
      root->child_at(0)->current_replication_state().insecure_request_policy);

  // When the subframe navigates, it should still be marked as enforcing
  // strict mixed content.
  GURL navigate_url(https_server.GetURL("/title1.html"));
  NavigateFrameToURL(root->child_at(0), navigate_url);
  EXPECT_EQ(blink::mojom::InsecureRequestPolicy::kBlockAllMixedContent,
            root->current_replication_state().insecure_request_policy);
  EXPECT_EQ(
      blink::mojom::InsecureRequestPolicy::kBlockAllMixedContent,
      root->child_at(0)->current_replication_state().insecure_request_policy);

  // When the main frame navigates, it should no longer be marked as
  // enforcing strict mixed content.
  EXPECT_TRUE(
      NavigateToURL(shell(), https_server.GetURL("b.com", "/title1.html")));
  EXPECT_EQ(blink::mojom::InsecureRequestPolicy::kLeaveInsecureRequestsAlone,
            root->current_replication_state().insecure_request_policy);
}

// Tests that, when a parent frame is set to upgrade insecure requests
// via Content Security Policy, child OOPIFs will upgrade as well.
IN_PROC_BROWSER_TEST_P(SitePerProcessIgnoreCertErrorsBrowserTest,
                       PassiveMixedContentInIframeWithUpgrade) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  SetupCrossSiteRedirector(&https_server);
  ASSERT_TRUE(https_server.Start());

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  GURL iframe_url_with_upgrade(https_server.GetURL(
      "/mixed-content/basic-passive-in-iframe-with-upgrade.html"));
  EXPECT_TRUE(NavigateToURL(shell(), iframe_url_with_upgrade));
  NavigationEntry* entry = web_contents->GetController().GetVisibleEntry();
  EXPECT_FALSE(!!(entry->GetSSL().content_status &
                  SSLStatus::DISPLAYED_INSECURE_CONTENT));

  FrameTreeNode* root = web_contents->GetFrameTree()->root();
  EXPECT_EQ(blink::mojom::InsecureRequestPolicy::kUpgradeInsecureRequests,
            root->current_replication_state().insecure_request_policy);
  EXPECT_EQ(
      blink::mojom::InsecureRequestPolicy::kUpgradeInsecureRequests,
      root->child_at(0)->current_replication_state().insecure_request_policy);

  // When the subframe navigates, it should still be marked as upgrading
  // insecure requests.
  GURL navigate_url(https_server.GetURL("/title1.html"));
  NavigateFrameToURL(root->child_at(0), navigate_url);
  EXPECT_EQ(blink::mojom::InsecureRequestPolicy::kUpgradeInsecureRequests,
            root->current_replication_state().insecure_request_policy);
  EXPECT_EQ(
      blink::mojom::InsecureRequestPolicy::kUpgradeInsecureRequests,
      root->child_at(0)->current_replication_state().insecure_request_policy);

  // When the main frame navigates, it should no longer be marked as
  // upgrading insecure requests.
  EXPECT_TRUE(
      NavigateToURL(shell(), https_server.GetURL("b.com", "/title1.html")));
  EXPECT_EQ(blink::mojom::InsecureRequestPolicy::kLeaveInsecureRequestsAlone,
            root->current_replication_state().insecure_request_policy);
}

// Tests that active mixed content is blocked in an OOPIF. The test
// ignores cert errors so that an HTTPS iframe can be loaded from a site
// other than localhost (the EmbeddedTestServer serves a certificate
// that is valid for localhost).
IN_PROC_BROWSER_TEST_P(SitePerProcessIgnoreCertErrorsBrowserTest,
                       ActiveMixedContentInIframe) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  SetupCrossSiteRedirector(&https_server);
  ASSERT_TRUE(https_server.Start());

  GURL iframe_url(
      https_server.GetURL("/mixed-content/basic-active-in-iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), iframe_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* mixed_child = root->child_at(0)->child_at(0);
  ASSERT_TRUE(mixed_child);
  // The child iframe attempted to create a mixed iframe; this should
  // have been blocked, so the mixed iframe should not have committed a
  // load.
  EXPECT_FALSE(mixed_child->has_committed_real_load());
}

// Test that subresources with certificate errors get reported to the
// browser. That is, if https://example.test frames https://a.com which
// loads an image with certificate errors, the browser should be
// notified about the subresource with certificate errors and downgrade
// the UI appropriately.
// TODO(crbug.com/1105145): Flaky.
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

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  RenderWidgetHost* root_render_widget_host =
      root->current_frame_host()->GetRenderWidgetHost();

  // Set the iframe to display: none.
  EXPECT_TRUE(ExecuteScript(
      shell(), "document.querySelector('iframe').style.display = 'none'"));

  // Waits until pending frames are done.
  std::unique_ptr<MainThreadFrameObserver> observer(
      new MainThreadFrameObserver(root_render_widget_host));
  observer->Wait();

  // Force the renderer to generate a new frame.
  EXPECT_TRUE(
      ExecuteScript(shell(), "document.body.style.background = 'black'"));

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

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Add a load event handler for the iframe element.
  EXPECT_TRUE(ExecuteScript(shell(),
                            "document.querySelector('iframe').onload = "
                            "    function() { document.title = 'loaded'; };"));

  const struct {
    const char* url;
    bool use_error_page;
  } kTestCases[] = {
      {"/frame-ancestors-none.html", false},
      {"/x-frame-options-deny.html", true},
  };

  for (const auto& test : kTestCases) {
    GURL blocked_url = embedded_test_server()->GetURL("b.com", test.url);
    EXPECT_TRUE(ExecuteScript(shell(), "document.title = 'not loaded';"));
    base::string16 expected_title(base::UTF8ToUTF16("loaded"));
    TitleWatcher title_watcher(shell()->web_contents(), expected_title);

    // Navigate the subframe to a blocked URL.
    TestNavigationObserver load_observer(shell()->web_contents());
    EXPECT_TRUE(ExecuteScript(
        shell(), JsReplace("frames[0].location.href = $1", blocked_url)));
    load_observer.Wait();

    // The blocked frame's origin should become unique.
    const url::Origin child_origin =
        root->child_at(0)->current_frame_host()->GetLastCommittedOrigin();
    EXPECT_TRUE(child_origin.opaque());
    EXPECT_EQ(url::Origin::Create(blocked_url.GetOrigin())
                  .GetTupleOrPrecursorTupleIfOpaque(),
              child_origin.GetTupleOrPrecursorTupleIfOpaque());

    // X-Frame-Options and CSP frame-ancestors behave differently. XFO commits
    // an error page, while CSP commits a "data:," URL.
    // TODO(https://crbug.com/870815): Use an error page for both.
    EXPECT_FALSE(load_observer.last_navigation_succeeded());
    EXPECT_EQ(net::ERR_BLOCKED_BY_RESPONSE,
              load_observer.last_net_error_code());
    EXPECT_EQ(root->child_at(0)->current_frame_host()->GetLastCommittedURL(),
              blocked_url);
    EXPECT_EQ("Error", EvalJs(root->child_at(0), "document.title"));

    // The blocked frame should still fire a load event in its parent's process.
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

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
    EXPECT_EQ(c_url.GetOrigin().spec(),
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

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Sanity-check that the test page has the expected shape for testing.
  GURL old_subframe_url(
      embedded_test_server()->GetURL("b.com", "/title2.html"));
  EXPECT_FALSE(root->child_at(0)->HasSameOrigin(*root));
  EXPECT_EQ(old_subframe_url, root->child_at(0)->current_url());
  const std::vector<network::mojom::ContentSecurityPolicyHeader>& root_csp =
      root->current_replication_state().accumulated_csp_headers;
  EXPECT_EQ(1u, root_csp.size());
  EXPECT_EQ("frame-src 'self' http://b.com:*", root_csp[0].header_value);

  // Monitor subframe's load events via main frame's title.
  EXPECT_TRUE(ExecuteScript(shell(),
                            "document.querySelector('iframe').onload = "
                            "    function() { document.title = 'loaded'; };"));
  EXPECT_TRUE(ExecuteScript(shell(), "document.title = 'not loaded';"));
  base::string16 expected_title(base::UTF8ToUTF16("loaded"));
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);

  // Try to navigate the subframe to a blocked URL.
  TestNavigationObserver load_observer(shell()->web_contents());
  GURL blocked_url = embedded_test_server()->GetURL("c.com", "/title3.html");
  EXPECT_TRUE(ExecuteScript(
      root->child_at(0), JsReplace("window.location.href = $1", blocked_url)));

  // The blocked frame should still fire a load event in its parent's process.
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  // Check that the current RenderFrameHost has stopped loading.
  if (root->child_at(0)->current_frame_host()->is_loading())
    load_observer.Wait();

  // The last successful url shouldn't be the blocked url.
  EXPECT_EQ(old_subframe_url,
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

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Navigate the subframe to a location we will disallow in the future.
  GURL old_subframe_url(
      embedded_test_server()->GetURL("b.com", "/title2.html"));
  NavigateFrameToURL(root->child_at(0), old_subframe_url);

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
  const std::vector<network::mojom::ContentSecurityPolicyHeader>& root_csp =
      root->current_replication_state().accumulated_csp_headers;
  EXPECT_EQ(1u, root_csp.size());
  EXPECT_EQ("frame-src https://a.com:*", root_csp[0].header_value);

  // Monitor subframe's load events via main frame's title.
  EXPECT_TRUE(ExecJs(shell(),
                     "document.querySelector('iframe').onload = "
                     "    function() { document.title = 'loaded'; };"));
  EXPECT_TRUE(ExecJs(shell(), "document.title = 'not loaded';"));
  base::string16 expected_title(base::UTF8ToUTF16("loaded"));
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
  EXPECT_EQ(old_subframe_url,
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

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
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
  const std::vector<network::mojom::ContentSecurityPolicyHeader>& srcdoc_csp =
      srcdoc_frame->current_replication_state().accumulated_csp_headers;
  EXPECT_EQ(1u, srcdoc_csp.size());
  EXPECT_EQ("frame-src 'self' http://b.com:*", srcdoc_csp[0].header_value);

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
  base::string16 expected_title(base::UTF8ToUTF16("loaded"));
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
  EXPECT_EQ(old_subframe_url,
            navigating_frame->current_frame_host()->last_successful_url());

  // The blocked frame should go to an error page. Errors currently commit
  // with the URL of the blocked page.
  EXPECT_EQ(blocked_url, navigating_frame->current_url());

  // The page should get the title of an error page (i.e "Error") and not the
  // title of the blocked page.
  EXPECT_EQ("Error", EvalJs(navigating_frame, "document.title"));

  // Navigate the subframe to a URL without CSP.
  NavigateFrameToURL(srcdoc_frame,
                     embedded_test_server()->GetURL("a.com", "/title1.html"));

  // Verify that the frame's CSP got correctly reset to an empty set.
  EXPECT_EQ(
      0u,
      srcdoc_frame->current_replication_state().accumulated_csp_headers.size());
}

IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, ScreenCoordinates) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);

  const char* properties[] = {"screenX", "screenY", "outerWidth",
                              "outerHeight"};

  for (const char* property : properties) {
    std::string script = base::StringPrintf("window.%s;", property);
    int root_value = EvalJs(root, script).ExtractInt();
    int child_value = EvalJs(child, script).ExtractInt();
    EXPECT_EQ(root_value, child_value);
  }
}

// Tests that the state of the RenderViewHost is properly reset when the main
// frame is navigated to the same SiteInstance as one of its child frames.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       NavigateMainFrameToChildSite) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsImpl* contents = web_contents();
  FrameTreeNode* root = contents->GetFrameTree()->root();
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
  RenderViewHostImpl* rvh =
      contents->GetFrameTree()
          ->GetRenderViewHost(
              root->child_at(0)->current_frame_host()->GetSiteInstance())
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
  EXPECT_TRUE(ExecuteScript(root->child_at(0), script));
  frame_observer.Wait();
  EXPECT_EQ(b_url, root->current_url());

  // Verify that the same RenderViewHost is preserved and that it is now active.
  EXPECT_EQ(rvh, contents->GetFrameTree()->GetRenderViewHost(
                     root->current_frame_host()->GetSiteInstance()));
  EXPECT_TRUE(rvh->is_active());
}

// Test for https://crbug.com/568836.  From an A-embed-B page, navigate the
// subframe from B to A.  This cleans up the process for B, but the test delays
// the browser side from killing the B process right away.  This allows the
// B process to process two WidgetMsg_Close messages sent to the subframe's
// RenderWidget and to the RenderView, in that order.  In the bug, the latter
// crashed while detaching the subframe's LocalFrame (triggered as part of
// closing the RenderView), because this tried to access the subframe's
// WebFrameWidget (from RenderFrameImpl::didChangeSelection), which had already
// been cleared by the former.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       CloseSubframeWidgetAndViewOnProcessExit) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  // "Select all" in the subframe.  The bug only happens if there's a selection
  // change, which triggers the path through didChangeSelection.
  root->child_at(0)
      ->current_frame_host()
      ->GetRenderWidgetHost()
      ->GetFrameWidgetInputHandler()
      ->SelectAll();

  // Prevent b.com process from terminating right away once the subframe
  // navigates away from b.com below.  This is necessary so that the renderer
  // process has time to process the closings of RenderWidget and RenderView,
  // which is where the original bug was triggered.  Incrementing the keep alive
  // ref count will cause RenderProcessHostImpl::Cleanup to forego process
  // termination.
  RenderProcessHost* subframe_process =
      root->child_at(0)->current_frame_host()->GetProcess();
  subframe_process->IncrementKeepAliveRefCount();

  // Navigate the subframe away from b.com.  Since this is the last active
  // frame in the b.com process, this causes the RenderWidget and RenderView to
  // be closed.
  NavigateFrameToURL(root->child_at(0),
                     embedded_test_server()->GetURL("a.com", "/title1.html"));

  // Release the process.
  RenderProcessHostWatcher process_shutdown_observer(
      subframe_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  subframe_process->DecrementKeepAliveRefCount();
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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
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

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
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
  NavigateFrameToURL(child, data_url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(data_url, observer.last_navigation_url());
  scoped_refptr<SiteInstanceImpl> orig_site_instance =
      child->current_frame_host()->GetSiteInstance();
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(), orig_site_instance);

  // Navigate it to another cross-site url.
  GURL cross_site_url(embedded_test_server()->GetURL("c.com", "/title1.html"));
  NavigateFrameToURL(child, cross_site_url);
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

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
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
    EXPECT_TRUE(
        ExecuteScript(child_0, JsReplace("location.href = $1", data_url_0)));
    commit_observer.WaitForCommit();
  }
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(data_url_0, observer.last_navigation_url());
  EXPECT_EQ(child_site_instance_0,
            child_0->current_frame_host()->GetSiteInstance());

  GURL data_url_1("data:text/html,dataurl_1");
  {
    TestFrameNavigationObserver commit_observer(child_1);
    EXPECT_TRUE(
        ExecuteScript(child_1, JsReplace("location.href = $1", data_url_1)));
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
              main_url, Referrer(), base::nullopt, ui::PAGE_TRANSITION_RELOAD,
              false, std::string(), controller.GetBrowserContext(),
              nullptr /* blob_url_loader_factory */));
  EXPECT_EQ(0U, restored_entry->root_node()->children.size());
  restored_entry->SetPageState(entry->GetPageState());
  ASSERT_EQ(2U, restored_entry->root_node()->children.size());

  // Restore the NavigationEntry into a new tab and check that the data URLs are
  // not loaded into the parent's SiteInstance.
  std::vector<std::unique_ptr<NavigationEntry>> entries;
  entries.push_back(std::move(restored_entry));
  Shell* new_shell = Shell::CreateNewWindow(
      controller.GetBrowserContext(), GURL::EmptyGURL(), nullptr, gfx::Size());
  FrameTreeNode* new_root =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetFrameTree()
          ->root();
  NavigationControllerImpl& new_controller =
      static_cast<NavigationControllerImpl&>(
          new_shell->web_contents()->GetController());
  new_controller.Restore(entries.size() - 1,
                         RestoreType::LAST_SESSION_EXITED_CLEANLY, &entries);
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

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
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
    EXPECT_TRUE(
        ExecuteScript(child_0, JsReplace("location.href = $1", blank_url)));
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
        ExecuteScript(child_1, JsReplace("location.href = $1", blank_url_ref)));
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
              main_url, Referrer(), base::nullopt, ui::PAGE_TRANSITION_RELOAD,
              false, std::string(), controller.GetBrowserContext(),
              nullptr /* blob_url_loader_factory */));
  EXPECT_EQ(0U, restored_entry->root_node()->children.size());
  restored_entry->SetPageState(entry->GetPageState());
  ASSERT_EQ(2U, restored_entry->root_node()->children.size());

  // Restore the NavigationEntry into a new tab and check that the about:blank
  // URLs are not loaded into the parent's SiteInstance.
  std::vector<std::unique_ptr<NavigationEntry>> entries;
  entries.push_back(std::move(restored_entry));
  Shell* new_shell = Shell::CreateNewWindow(
      controller.GetBrowserContext(), GURL::EmptyGURL(), nullptr, gfx::Size());
  FrameTreeNode* new_root =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetFrameTree()
          ->root();
  NavigationControllerImpl& new_controller =
      static_cast<NavigationControllerImpl&>(
          new_shell->web_contents()->GetController());
  new_controller.Restore(entries.size() - 1,
                         RestoreType::LAST_SESSION_EXITED_CLEANLY, &entries);
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

  // Origin for child frames should be opaque.
  EXPECT_EQ(
      new_root->current_frame_host()->GetLastCommittedOrigin().Serialize(),
      GetOriginFromRenderer(new_root));
  EXPECT_TRUE(
      new_child_0->current_frame_host()->GetLastCommittedOrigin().opaque());
  EXPECT_EQ("null", GetOriginFromRenderer(new_child_0));
  EXPECT_TRUE(
      new_child_1->current_frame_host()->GetLastCommittedOrigin().opaque());
  EXPECT_EQ("null", GetOriginFromRenderer(new_child_1));

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

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
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
              main_url, Referrer(), base::nullopt, ui::PAGE_TRANSITION_RELOAD,
              false, std::string(), controller.GetBrowserContext(),
              nullptr /* blob_url_loader_factory */));
  EXPECT_EQ(0U, restored_entry->root_node()->children.size());
  restored_entry->SetPageState(entry->GetPageState());
  ASSERT_EQ(1U, restored_entry->root_node()->children.size());

  // Restore the NavigationEntry into a new tab and check that the srcdoc URLs
  // are still loaded into the parent's SiteInstance.
  std::vector<std::unique_ptr<NavigationEntry>> entries;
  entries.push_back(std::move(restored_entry));
  Shell* new_shell = Shell::CreateNewWindow(
      controller.GetBrowserContext(), GURL::EmptyGURL(), nullptr, gfx::Size());
  FrameTreeNode* new_root =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetFrameTree()
          ->root();
  NavigationControllerImpl& new_controller =
      static_cast<NavigationControllerImpl&>(
          new_shell->web_contents()->GetController());
  new_controller.Restore(entries.size() - 1,
                         RestoreType::LAST_SESSION_EXITED_CLEANLY, &entries);
  ASSERT_EQ(0u, entries.size());
  {
    TestNavigationObserver restore_observer(new_shell->web_contents());
    new_controller.LoadIfNecessary();
    restore_observer.Wait();
  }
  ASSERT_EQ(1U, new_root->child_count());
  EXPECT_EQ(main_url, new_root->current_url());
  EXPECT_TRUE(new_root->child_at(0)->current_url().IsAboutSrcdoc());

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

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
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
  NavigateFrameToURL(child, about_blank_url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(about_blank_url, observer.last_navigation_url());
  scoped_refptr<SiteInstanceImpl> orig_site_instance =
    child->current_frame_host()->GetSiteInstance();
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(), orig_site_instance);

  // Navigate it to another cross-site url.
  GURL cross_site_url(embedded_test_server()->GetURL("c.com", "/title1.html"));
  NavigateFrameToURL(child, cross_site_url);
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

// Helper filter class to wait for a ShowWidget message, record the routing ID
// from the message, and then drop the message.
const uint32_t kMessageClasses[] = {ViewMsgStart, FrameMsgStart};
class PendingWidgetMessageFilter : public BrowserMessageFilter {
 public:
  PendingWidgetMessageFilter()
      : BrowserMessageFilter(kMessageClasses, base::size(kMessageClasses)),
        routing_id_(MSG_ROUTING_NONE) {}

  bool OnMessageReceived(const IPC::Message& message) override {
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(PendingWidgetMessageFilter, message)
      IPC_MESSAGE_HANDLER(ViewHostMsg_ShowWidget, OnShowWidget)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

  void Wait() { run_loop_.Run(); }

  int routing_id() { return routing_id_; }

 private:
  ~PendingWidgetMessageFilter() override {}

  void OnShowCreatedWindow(int pending_widget_routing_id,
                           WindowOpenDisposition disposition,
                           const gfx::Rect& initial_rect,
                           bool user_gesture) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&PendingWidgetMessageFilter::OnReceivedRoutingIDOnUI,
                       this, pending_widget_routing_id));
  }

  void OnShowWidget(int routing_id, const gfx::Rect& initial_rect) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&PendingWidgetMessageFilter::OnReceivedRoutingIDOnUI,
                       this, routing_id));
  }

  void OnReceivedRoutingIDOnUI(int widget_routing_id) {
    routing_id_ = widget_routing_id;
    run_loop_.Quit();
  }

  int routing_id_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(PendingWidgetMessageFilter);
};

// Intercepts calls to RenderFramHost's ShowCreatedWindow mojo method, and
// invokes the provided callback.
class ShowCreatedWindowInterceptor
    : public mojom::FrameHostInterceptorForTesting {
 public:
  ShowCreatedWindowInterceptor(
      RenderFrameHostImpl* render_frame_host,
      base::RepeatingCallback<void(int32_t pending_widget_routing_id)>
          test_callback)
      : render_frame_host_(render_frame_host),
        test_callback_(std::move(test_callback)) {
    render_frame_host_->frame_host_receiver_for_testing().SwapImplForTesting(
        this);
  }

  ~ShowCreatedWindowInterceptor() override {}

  FrameHost* GetForwardingInterface() override { return render_frame_host_; }

  void ShowCreatedWindow(int32_t pending_widget_routing_id,
                         WindowOpenDisposition disposition,
                         const gfx::Rect& initial_rect,
                         bool user_gesture) override {
    test_callback_.Run(pending_widget_routing_id);
  }

 private:
  RenderFrameHostImpl* render_frame_host_;
  base::RepeatingCallback<void(int32_t pending_widget_routing_id)>
      test_callback_;
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

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
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
  ShowCreatedWindowInterceptor interceptor1(
      frame1,
      base::BindLambdaForTesting([&](int32_t pending_widget_routing_id) {
        routing_id1 = pending_widget_routing_id;
        run_loop1.Quit();
      }));
  EXPECT_TRUE(ExecuteScript(child1, "window.open();"));
  run_loop1.Run();

  base::RunLoop run_loop2;
  int32_t routing_id2;
  ShowCreatedWindowInterceptor interceptor2(
      frame2,
      base::BindLambdaForTesting([&](int32_t pending_widget_routing_id) {
        routing_id2 = pending_widget_routing_id;
        run_loop2.Quit();
      }));

  EXPECT_TRUE(ExecuteScript(child2, "window.open();"));
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

  // Now, simulate that both FrameHostMsg_ShowCreatedWindow messages arrive by
  // showing both of the pending WebContents.
  web_contents()->ShowCreatedWindow(frame1, routing_id1,
                                    WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                    gfx::Rect(), true);
  web_contents()->ShowCreatedWindow(frame2, routing_id2,
                                    WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                    gfx::Rect(), true);

  // Verify that both shells were properly created.
  EXPECT_EQ(3u, Shell::windows().size());
}

// Test for https://crbug.com/612276.  Similar to
// TwoSubframesOpenWindowsSimultaneously, but use popup menu widgets instead of
// windows.
//
// The plumbing that this test is verifying is not utilized on Mac/Android,
// where popup menus don't create a popup RenderWidget, but rather they trigger
// a FrameHostMsg_ShowPopup to ask the browser to build and display the actual
// popup using native controls.
#if !defined(OS_MAC) && !defined(OS_ANDROID)
// Disable the test due to flaky: https://crbug.com/1126165
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
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

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child1 = root->child_at(0);
  FrameTreeNode* child2 = root->child_at(1);
  RenderProcessHost* process1 = child1->current_frame_host()->GetProcess();
  RenderProcessHost* process2 = child2->current_frame_host()->GetProcess();

  // Navigate both subframes to a page with a <select> element.
  NavigateFrameToURL(child1, embedded_test_server()->GetURL(
      "b.com", "/site_isolation/page-with-select.html"));
  NavigateFrameToURL(child2, embedded_test_server()->GetURL(
      "c.com", "/site_isolation/page-with-select.html"));

  // Open both <select> menus by focusing each item and sending a space key
  // at the focused node. This creates a popup widget in both processes.
  // Wait for and then drop the ViewHostMsg_ShowWidget messages, so that both
  // widgets are left in pending-but-not-shown state.
  NativeWebKeyboardEvent event(
      blink::WebKeyboardEvent::Type::kChar, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  event.text[0] = ' ';

  scoped_refptr<PendingWidgetMessageFilter> filter1 =
      new PendingWidgetMessageFilter();
  process1->AddFilter(filter1.get());
  EXPECT_TRUE(ExecuteScript(child1, "focusSelectMenu();"));
  child1->current_frame_host()->GetRenderWidgetHost()->ForwardKeyboardEvent(
      event);
  filter1->Wait();

  scoped_refptr<PendingWidgetMessageFilter> filter2 =
      new PendingWidgetMessageFilter();
  process2->AddFilter(filter2.get());
  EXPECT_TRUE(ExecuteScript(child2, "focusSelectMenu();"));
  child2->current_frame_host()->GetRenderWidgetHost()->ForwardKeyboardEvent(
      event);
  filter2->Wait();

  // At this point, we should have two pending widgets.
  EXPECT_TRUE(base::Contains(
      web_contents()->pending_widget_views_,
      GlobalRoutingID(process1->GetID(), filter1->routing_id())));
  EXPECT_TRUE(base::Contains(
      web_contents()->pending_widget_views_,
      GlobalRoutingID(process2->GetID(), filter2->routing_id())));

  // Both subframes were set up in the same way, so the next routing ID for the
  // new popup widgets should match up (this led to the collision in the
  // pending widgets map in the original bug).
  EXPECT_EQ(filter1->routing_id(), filter2->routing_id());

  // Now simulate both widgets being shown.
  web_contents()->ShowCreatedWidget(process1->GetID(), filter1->routing_id(),
                                    false, gfx::Rect());
  web_contents()->ShowCreatedWidget(process2->GetID(), filter2->routing_id(),
                                    false, gfx::Rect());
  EXPECT_FALSE(base::Contains(
      web_contents()->pending_widget_views_,
      GlobalRoutingID(process1->GetID(), filter1->routing_id())));
  EXPECT_FALSE(base::Contains(
      web_contents()->pending_widget_views_,
      GlobalRoutingID(process2->GetID(), filter2->routing_id())));
}
#endif

// Test for https://crbug.com/615575. It ensures that file chooser triggered
// by a document in an out-of-process subframe works properly.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, FileChooserInSubframe) {
  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)")));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  GURL url(embedded_test_server()->GetURL("b.com", "/file_input.html"));
  NavigateFrameToURL(root->child_at(0), url);

  // Use FileChooserDelegate to avoid showing the actual dialog and to respond
  // back to the renderer process with predefined file.
  base::RunLoop run_loop;
  base::FilePath file;
  EXPECT_TRUE(base::PathService::Get(base::DIR_TEMP, &file));
  file = file.AppendASCII("bar");
  std::unique_ptr<FileChooserDelegate> delegate(
      new FileChooserDelegate(file, run_loop.QuitClosure()));
  shell()->web_contents()->SetDelegate(delegate.get());
  EXPECT_TRUE(ExecuteScript(root->child_at(0),
                            "document.getElementById('fileinput').click();"));
  run_loop.Run();

  // Also, extract the file from the renderer process to ensure that the
  // response made it over successfully and the proper filename is set.
  EXPECT_EQ("bar",
            EvalJs(root->child_at(0),
                   "document.getElementById('fileinput').files[0].name;"));
}

// Tests that an out-of-process iframe receives the visibilitychange event.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, VisibilityChange) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  EXPECT_TRUE(
      ExecJs(root->child_at(0),
             "var event_fired = 0;\n"
             "document.addEventListener('visibilitychange',\n"
             "                          function() { event_fired++; });\n"));

  shell()->web_contents()->WasHidden();

  EXPECT_EQ(1, EvalJs(root->child_at(0), "event_fired"));

  shell()->web_contents()->WasShown();

  EXPECT_EQ(2, EvalJs(root->child_at(0), "event_fired"));
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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Open a popup at b.com.
  GURL popup_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  Shell* popup_shell = OpenPopup(root, popup_url, "foo");
  EXPECT_TRUE(popup_shell);

  // The RenderViewHost for b.com in the main tab should not be active.
  SiteInstance* b_instance = popup_shell->web_contents()->GetSiteInstance();
  RenderViewHostImpl* rvh =
      web_contents()->GetFrameTree()->GetRenderViewHost(b_instance).get();
  EXPECT_FALSE(rvh->is_active());

  // Navigate main tab to a b.com URL that will not commit.
  GURL stall_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  TestNavigationManager delayer(shell()->web_contents(), stall_url);
  EXPECT_TRUE(ExecuteScript(shell(), JsReplace("location = $1", stall_url)));
  EXPECT_TRUE(delayer.WaitForRequestStart());

  // The pending RFH should be in the same process as the popup.
  RenderFrameHostImpl* pending_rfh =
      root->render_manager()->speculative_frame_host();
  RenderProcessHost* pending_process = pending_rfh->GetProcess();
  EXPECT_EQ(pending_process,
            popup_shell->web_contents()->GetMainFrame()->GetProcess());

  // Kill the b.com process, currently in use by the pending RenderFrameHost
  // and the popup.
  RenderProcessHostWatcher crash_observer(
      pending_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  EXPECT_TRUE(pending_process->Shutdown(0));
  crash_observer.Wait();

  // The pending RFH should have been canceled and destroyed, so that it won't
  // be reused while it's not live in the next navigation.
  {
    RenderFrameHostImpl* pending_rfh =
        root->render_manager()->speculative_frame_host();
    EXPECT_FALSE(pending_rfh);
  }

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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Open a popup at b.com.
  GURL popup_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  Shell* popup_shell = OpenPopup(root, popup_url, "foo");
  EXPECT_TRUE(popup_shell);

  // The RenderViewHost for b.com in the main tab should not be active.
  SiteInstance* b_instance = popup_shell->web_contents()->GetSiteInstance();
  RenderViewHostImpl* rvh =
      web_contents()->GetFrameTree()->GetRenderViewHost(b_instance).get();
  EXPECT_FALSE(rvh->is_active());

  // Navigate main tab to a b.com URL that will not commit.
  GURL stall_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  NavigationHandleObserver handle_observer(shell()->web_contents(), stall_url);
  TestNavigationManager delayer(shell()->web_contents(), stall_url);
  EXPECT_TRUE(ExecuteScript(shell(), JsReplace("location = $1", stall_url)));
  EXPECT_TRUE(delayer.WaitForRequestStart());

  // Kill the b.com process, currently in use by the pending RenderFrameHost
  // and the popup.
  RenderProcessHost* pending_process =
      popup_shell->web_contents()->GetMainFrame()->GetProcess();
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
  // https://crbug.com/627893, recreating the opener RenderView was hitting a
  // CHECK(params.swapped_out) in the renderer process, since its
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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);

  // Set up a postMessage handler in the main frame for later use.
  EXPECT_TRUE(ExecuteScript(
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
  base::string16 expected_title(base::UTF8ToUTF16("I am alive!"));
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);
  EXPECT_TRUE(ExecuteScript(child->current_frame_host(),
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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
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
  NavigateFrameToURL(child1,
                     embedded_test_server()->GetURL("b.com", "/title1.html"));
  EXPECT_EQ(2, history_length(root));
  EXPECT_EQ(2, history_length(child1));
  EXPECT_EQ(2, history_length(child2));

  // Navigate second child same-site.
  GURL child2_last_url(embedded_test_server()->GetURL("a.com", "/title2.html"));
  NavigateFrameToURL(child2, child2_last_url);
  EXPECT_EQ(3, history_length(root));
  EXPECT_EQ(3, history_length(child1));
  EXPECT_EQ(3, history_length(child2));

  // Navigate first child same-site to another b.com URL.
  GURL child1_last_url(embedded_test_server()->GetURL("b.com", "/title3.html"));
  NavigateFrameToURL(child1, child1_last_url);
  EXPECT_EQ(4, history_length(root));
  EXPECT_EQ(4, history_length(child1));
  EXPECT_EQ(4, history_length(child2));

  // Go back three entries using the history API from the main frame. This
  // checks that both history length and offset are not stale in a.com, as
  // otherwise this navigation might be dropped by Blink.
  EXPECT_TRUE(ExecuteScript(root, "history.go(-3);"));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(main_url, root->current_url());
  EXPECT_EQ(child_first_url, child1->current_url());
  EXPECT_EQ(child_first_url, child2->current_url());

  // Now go forward three entries from the child1 frame and check that the
  // history length and offset are not stale in b.com.
  EXPECT_TRUE(ExecuteScript(child1, "history.go(3);"));
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
      : render_frame_host_(render_frame_host) {
    render_frame_host_->local_frame_host_receiver_for_testing()
        .SwapImplForTesting(this);
  }

  ~DispatchLoadInterceptor() override = default;

  LocalFrameHost* GetForwardingInterface() override {
    return render_frame_host_;
  }

  // Discard incoming calls to LocalFrameHost::DispatchLoad().
  void DispatchLoad() override {}

 private:
  RenderFrameHostImpl* render_frame_host_;
};

// Test that the renderer isn't killed when a frame generates a load event just
// after becoming pending deletion.  See https://crbug.com/636513.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       LoadEventForwardingWhilePendingDeletion) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);

  // Open a popup in the b.com process for later use.
  GURL popup_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  Shell* popup_shell = OpenPopup(root, popup_url, "foo");
  EXPECT_TRUE(popup_shell);

  // Navigate subframe to b.com.  Wait for commit but not full load.
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  {
    TestFrameNavigationObserver commit_observer(child);
    EXPECT_TRUE(ExecuteScript(child, JsReplace("location.href = $1", b_url)));
    commit_observer.WaitForCommit();
  }
  RenderFrameHostImpl* child_rfh = child->current_frame_host();
  child_rfh->DisableUnloadTimerForTesting();

  // At this point, the subframe should have a proxy in its parent's
  // SiteInstance, a.com.
  EXPECT_TRUE(child->render_manager()->GetProxyToParent());

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

  // In the bug, DispatchLoad killed the b.com renderer.  Ensure that this is
  // not the case. Note that the process kill doesn't happen immediately, so
  // IsRenderFrameLive() can't be checked here (yet).  Instead, check that
  // JavaScript can still execute in b.com using the popup.
  EXPECT_TRUE(ExecuteScript(popup_shell->web_contents(), "true"));
}

IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       RFHTransfersWhilePendingDeletion) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

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
  EXPECT_TRUE(
      ExecuteScript(root, JsReplace("location.href = $1", cross_site_url_2)));
  EXPECT_TRUE(transfer_manager.WaitForResponse());

  // Now have the cross-process navigation commit and mark the current RFH as
  // pending deletion.
  cross_site_manager.WaitForNavigationFinished();

  // Resume the navigation in the previous RFH that has just been marked as
  // pending deletion. We should not crash.
  transfer_manager.WaitForNavigationFinished();
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
  EXPECT_TRUE(
      ExecuteScript(shell()->web_contents(),
                    JsReplace("window.frames[0].location = $1", frame_url)));
  load_observer.Wait();
}

// Test that when canceling a pending RenderFrameHost in the middle of a
// redirect, and then killing the corresponding RenderView's renderer process,
// the RenderViewHost isn't reused in an improper state later.  Previously this
// led to a crash in CreateRenderView when recreating the RenderView due to a
// stale main frame routing ID.  See https://crbug.com/627400.
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

  // Kill the b.com process (which currently hosts a RenderFrameProxy that
  // replaced the pending RenderFrame in |popup2|, as well as the RenderFrame
  // for |popup|).
  RenderProcessHost* b_process =
      popup->web_contents()->GetMainFrame()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      b_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  b_process->Shutdown(0);
  crash_observer.Wait();

  // Navigate the second popup to b.com.  This used to crash when creating the
  // RenderView, because it reused the RenderViewHost created by the canceled
  // navigation to b.com, and that RenderViewHost had a stale main frame
  // routing ID and active state.
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
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
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
          ->GetFrameTree()
          ->root();
  RenderFrameProxyHost* proxy =
      popup2_root->render_manager()->GetRenderFrameProxyHost(b_instance);
  EXPECT_TRUE(proxy);
  EXPECT_TRUE(proxy->is_render_frame_proxy_live());

  // Add a postMessage listener in |popup2| (currently at a c.com URL).
  EXPECT_TRUE(
      ExecuteScript(popup2,
                    "window.addEventListener('message', function(event) {\n"
                    "  document.title=event.data;\n"
                    "});"));

  // Check that a postMessage can be sent via |proxy| above.  This needs to be
  // done from the b.com process.  |popup| is currently in b.com, but it can't
  // reach the window reference for |popup2| due to a security restriction in
  // Blink. So, navigate the main tab to b.com and then send a postMessage to
  // |popup2|. This is allowed since the main tab is |popup2|'s opener.
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), b_url));

  base::string16 expected_title(base::UTF8ToUTF16("foo"));
  TitleWatcher title_watcher(popup2->web_contents(), expected_title);
  EXPECT_TRUE(ExecuteScript(
      shell(), "window.open('','popup2').postMessage('foo', '*');"));
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(SitePerProcessFeaturePolicyBrowserTest,
                       TestPolicyReplicationOnSameOriginNavigation) {
  GURL start_url(
      embedded_test_server()->GetURL("a.com", "/feature-policy1.html"));
  GURL first_nav_url(
      embedded_test_server()->GetURL("a.com", "/feature-policy2.html"));
  GURL second_nav_url(embedded_test_server()->GetURL("a.com", "/title2.html"));

  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  EXPECT_EQ(CreateParsedFeaturePolicy(
                {blink::mojom::FeaturePolicyFeature::kGeolocation,
                 blink::mojom::FeaturePolicyFeature::kPayment},
                {start_url.GetOrigin()}),
            root->current_replication_state().feature_policy_header);

  // When the main frame navigates to a page with a new policy, it should
  // overwrite the old one.
  EXPECT_TRUE(NavigateToURL(shell(), first_nav_url));
  EXPECT_EQ(CreateParsedFeaturePolicyMatchesAll(
                {blink::mojom::FeaturePolicyFeature::kGeolocation,
                 blink::mojom::FeaturePolicyFeature::kPayment}),
            root->current_replication_state().feature_policy_header);

  // When the main frame navigates to a page without a policy, the replicated
  // policy header should be cleared.
  EXPECT_TRUE(NavigateToURL(shell(), second_nav_url));
  EXPECT_TRUE(root->current_replication_state().feature_policy_header.empty());
}

IN_PROC_BROWSER_TEST_F(SitePerProcessFeaturePolicyBrowserTest,
                       TestPolicyReplicationOnCrossOriginNavigation) {
  GURL start_url(
      embedded_test_server()->GetURL("a.com", "/feature-policy1.html"));
  GURL first_nav_url(
      embedded_test_server()->GetURL("b.com", "/feature-policy2.html"));
  GURL second_nav_url(embedded_test_server()->GetURL("c.com", "/title2.html"));

  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  EXPECT_EQ(CreateParsedFeaturePolicy(
                {blink::mojom::FeaturePolicyFeature::kGeolocation,
                 blink::mojom::FeaturePolicyFeature::kPayment},
                {start_url.GetOrigin()}),
            root->current_replication_state().feature_policy_header);

  // When the main frame navigates to a page with a new policy, it should
  // overwrite the old one.
  EXPECT_TRUE(NavigateToURL(shell(), first_nav_url));
  EXPECT_EQ(CreateParsedFeaturePolicyMatchesAll(
                {blink::mojom::FeaturePolicyFeature::kGeolocation,
                 blink::mojom::FeaturePolicyFeature::kPayment}),
            root->current_replication_state().feature_policy_header);

  // When the main frame navigates to a page without a policy, the replicated
  // policy header should be cleared.
  EXPECT_TRUE(NavigateToURL(shell(), second_nav_url));
  EXPECT_TRUE(root->current_replication_state().feature_policy_header.empty());
}

// Test that the replicated feature policy header is correct in subframes as
// they navigate.
IN_PROC_BROWSER_TEST_F(SitePerProcessFeaturePolicyBrowserTest,
                       TestPolicyReplicationFromRemoteFrames) {
  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/feature-policy-main.html"));
  GURL first_nav_url(
      embedded_test_server()->GetURL("b.com", "/feature-policy2.html"));
  GURL second_nav_url(embedded_test_server()->GetURL("c.com", "/title2.html"));

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  EXPECT_EQ(CreateParsedFeaturePolicy(
                {blink::mojom::FeaturePolicyFeature::kGeolocation,
                 blink::mojom::FeaturePolicyFeature::kPayment},
                {main_url.GetOrigin(), GURL("http://example.com/")}),
            root->current_replication_state().feature_policy_header);
  EXPECT_EQ(1UL, root->child_count());
  EXPECT_EQ(
      CreateParsedFeaturePolicy(
          {blink::mojom::FeaturePolicyFeature::kGeolocation,
           blink::mojom::FeaturePolicyFeature::kPayment},
          {main_url.GetOrigin()}),
      root->child_at(0)->current_replication_state().feature_policy_header);

  // Navigate the iframe cross-site.
  NavigateFrameToURL(root->child_at(0), first_nav_url);
  EXPECT_EQ(
      CreateParsedFeaturePolicyMatchesAll(
          {blink::mojom::FeaturePolicyFeature::kGeolocation,
           blink::mojom::FeaturePolicyFeature::kPayment}),
      root->child_at(0)->current_replication_state().feature_policy_header);

  // Navigate the iframe to another location, this one with no policy header
  NavigateFrameToURL(root->child_at(0), second_nav_url);
  EXPECT_TRUE(root->child_at(0)
                  ->current_replication_state()
                  .feature_policy_header.empty());

  // Navigate the iframe back to a page with a policy
  NavigateFrameToURL(root->child_at(0), first_nav_url);
  EXPECT_EQ(
      CreateParsedFeaturePolicyMatchesAll(
          {blink::mojom::FeaturePolicyFeature::kGeolocation,
           blink::mojom::FeaturePolicyFeature::kPayment}),
      root->child_at(0)->current_replication_state().feature_policy_header);
}

// Test that the replicated feature policy header is correct in remote proxies
// after the local frame has navigated.
IN_PROC_BROWSER_TEST_F(SitePerProcessFeaturePolicyBrowserTest,
                       TestFeaturePolicyReplicationToProxyOnNavigation) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_two_frames.html"));
  GURL first_nav_url(
      embedded_test_server()->GetURL("a.com", "/feature-policy3.html"));
  GURL second_nav_url(
      embedded_test_server()->GetURL("a.com", "/feature-policy4.html"));

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  EXPECT_TRUE(root->current_replication_state().feature_policy_header.empty());
  EXPECT_EQ(2UL, root->child_count());
  EXPECT_TRUE(root->child_at(1)
                  ->current_replication_state()
                  .feature_policy_header.empty());

  // Navigate the iframe to a page with a policy, and a nested cross-site iframe
  // (to the same site as a root->child_at(1) so that the render process already
  // exists.)
  NavigateFrameToURL(root->child_at(1), first_nav_url);
  EXPECT_EQ(
      CreateParsedFeaturePolicyMatchesNone(
          {blink::mojom::FeaturePolicyFeature::kGeolocation,
           blink::mojom::FeaturePolicyFeature::kPayment}),
      root->child_at(1)->current_replication_state().feature_policy_header);

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
  NavigateFrameToURL(root->child_at(1), second_nav_url);
  EXPECT_TRUE(root->child_at(1)
                  ->current_replication_state()
                  .feature_policy_header.empty());
  EXPECT_EQ(1UL, root->child_at(1)->child_count());

  // Ask the deepest iframe to report the enabled state of the geolocation
  // feature. If its parent frame's policy was replicated correctly to the
  // proxy, then this will now be allowed.
  EXPECT_EQ(true,
            EvalJs(root->child_at(1)->child_at(0),
                   "document.featurePolicy.allowsFeature('geolocation')"));
}

// Test that the constructed feature policy is correct in sandboxed
// frames. Sandboxed frames have an opaque origin, and if the frame policy,
// which is constructed in the parent frame, cannot send that origin through
// the browser process to the sandboxed frame, then the sandboxed frame's
// policy will be incorrect.
//
// This is a regression test for https://crbug.com/690520
IN_PROC_BROWSER_TEST_F(SitePerProcessFeaturePolicyBrowserTest,
                       TestAllowAttributeInSandboxedFrame) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com",
      "/cross_site_iframe_factory.html?"
      "a(b{allow-geolocation,sandbox-allow-scripts})"));
  GURL nav_url(embedded_test_server()->GetURL("c.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  EXPECT_TRUE(root->current_replication_state().feature_policy_header.empty());
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
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      root->child_at(0),
      "window.domAutomationController.send("
      "document.featurePolicy.allowsFeature('geolocation'));",
      &success));
  EXPECT_TRUE(success);

  TestNavigationObserver load_observer(shell()->web_contents());
  EXPECT_TRUE(ExecuteScript(root->child_at(0),
                            JsReplace("document.location.href=$1", nav_url)));
  load_observer.Wait();

  // Verify that the child frame is sandboxed with an opaque origin.
  EXPECT_TRUE(root->child_at(0)
                  ->current_frame_host()
                  ->GetLastCommittedOrigin()
                  .opaque());
  // And verify that the origin in the replication state is also opaque.
  EXPECT_TRUE(root->child_at(0)->current_origin().opaque());

  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      root->child_at(0),
      "window.domAutomationController.send("
      "document.featurePolicy.allowsFeature('geolocation'));",
      &success));
  EXPECT_TRUE(success);
}

// Test that the constructed feature policy is correct in sandboxed
// frames. Sandboxed frames have an opaque origin, and if the frame policy,
// which is constructed in the parent frame, cannot send that origin through
// the browser process to the sandboxed frame, then the sandboxed frame's
// policy will be incorrect.
//
// This is a regression test for https://crbug.com/690520
IN_PROC_BROWSER_TEST_F(SitePerProcessFeaturePolicyBrowserTest,
                       TestAllowAttributeInOpaqueOriginAfterNavigation) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/page_with_data_iframe_and_allow.html"));
  GURL nav_url(embedded_test_server()->GetURL("c.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  EXPECT_TRUE(root->current_replication_state().feature_policy_header.empty());
  EXPECT_EQ(1UL, root->child_count());
  // Verify that the child frame has an opaque origin.
  EXPECT_TRUE(root->child_at(0)
                  ->current_frame_host()
                  ->GetLastCommittedOrigin()
                  .opaque());
  // And verify that the origin in the replication state is also opaque.
  EXPECT_TRUE(root->child_at(0)->current_origin().opaque());

  // Verify that geolocation is enabled in the document.
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      root->child_at(0),
      "window.domAutomationController.send("
      "document.featurePolicy.allowsFeature('geolocation'));",
      &success));
  EXPECT_TRUE(success);

  TestNavigationObserver load_observer(shell()->web_contents());
  EXPECT_TRUE(ExecuteScript(root->child_at(0),
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
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      root->child_at(0),
      "window.domAutomationController.send("
      "document.featurePolicy.allowsFeature('geolocation'));",
      &success));
  EXPECT_FALSE(success);
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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
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
  EXPECT_TRUE(ExecuteScript(
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

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
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
  EXPECT_TRUE(ExecuteScript(
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
      shell()->web_contents()->GetMainFrame());
  auto filter = base::MakeRefCounted<DropMessageFilter>(
      FrameMsgStart, FrameHostMsg_Unload_ACK::ID);
  rfh->GetProcess()->AddFilter(filter.get());
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
  rfh->OnMessageReceived(
      FrameHostMsg_ContextMenu(rfh->GetRoutingID(), ContextMenuParams()));
}

// Test iframe container policy is replicated properly to the browser.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, ContainerPolicy) {
  GURL url(embedded_test_server()->GetURL("/allowed_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

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
      embedded_test_server()->GetURL("b.com", "/feature-policy2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  EXPECT_EQ(
      2UL, root->child_at(2)->effective_frame_policy().container_policy.size());

  // Removing the "allow" attribute; pending policy should update, but effective
  // policy remains unchanged.
  EXPECT_TRUE(ExecuteScript(
      root, "document.getElementById('child-2').setAttribute('allow','')"));
  EXPECT_EQ(
      2UL, root->child_at(2)->effective_frame_policy().container_policy.size());
  EXPECT_EQ(0UL,
            root->child_at(2)->pending_frame_policy().container_policy.size());

  // Navigate the frame; pending policy should be committed.
  NavigateFrameToURL(root->child_at(2), nav_url);
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
  FrameTreeNode* root = contents->GetFrameTree()->root();

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
  EXPECT_TRUE(ExecuteScript(
      root, "document.getElementById('child-0').allowFullscreen='true'"));

  // No change is expected to the container policy for dynamic modification of
  // a loaded frame.
  EXPECT_EQ(false, is_fullscreen_allowed(root->child_at(0)));

  // Cross-site navigation should update the container policy in the new render
  // frame.
  NavigateFrameToURL(root->child_at(0),
                     embedded_test_server()->GetURL("c.com", "/title1.html"));
  EXPECT_EQ(true, is_fullscreen_allowed(root->child_at(0)));
}

// Test that dynamic updates to iframe sandbox attribute correctly set the
// replicated container policy.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       ContainerPolicySandboxDynamic) {
  GURL main_url(embedded_test_server()->GetURL("/allowed_frames.html"));
  GURL nav_url(
      embedded_test_server()->GetURL("b.com", "/feature-policy2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Validate that the effective container policy contains a single non-unique
  // origin.
  const blink::ParsedFeaturePolicy initial_effective_policy =
      root->child_at(2)->effective_frame_policy().container_policy;
  EXPECT_EQ(1UL, initial_effective_policy[0].allowed_origins.size());
  EXPECT_FALSE(initial_effective_policy[0].allowed_origins.begin()->opaque());

  // Set the "sandbox" attribute; pending policy should update, and should now
  // be flagged as matching the opaque origin of the frame (without containing
  // an actual opaque origin, since the parent frame doesn't actually have that
  // origin yet) but the effective policy should remain unchanged.
  EXPECT_TRUE(ExecuteScript(
      root, "document.getElementById('child-2').setAttribute('sandbox','')"));
  const blink::ParsedFeaturePolicy updated_effective_policy =
      root->child_at(2)->effective_frame_policy().container_policy;
  const blink::ParsedFeaturePolicy updated_pending_policy =
      root->child_at(2)->pending_frame_policy().container_policy;
  EXPECT_EQ(1UL, updated_effective_policy[0].allowed_origins.size());
  EXPECT_FALSE(updated_effective_policy[0].allowed_origins.begin()->opaque());
  EXPECT_TRUE(updated_pending_policy[0].matches_opaque_src);
  EXPECT_EQ(0UL, updated_pending_policy[0].allowed_origins.size());

  // Navigate the frame; pending policy should now be committed.
  NavigateFrameToURL(root->child_at(2), nav_url);
  const blink::ParsedFeaturePolicy final_effective_policy =
      root->child_at(2)->effective_frame_policy().container_policy;
  EXPECT_TRUE(final_effective_policy[0].matches_opaque_src);
  EXPECT_EQ(0UL, final_effective_policy[0].allowed_origins.size());
}

// Test that creating a new remote frame at the same origin as its parent
// results in the correct feature policy in the RemoteSecurityContext.
// https://crbug.com/852102
IN_PROC_BROWSER_TEST_F(SitePerProcessFeaturePolicyBrowserTest,
                       FeaturePolicyConstructionInExistingProxy) {
  WebContentsImpl* contents = web_contents();
  FrameTreeNode* root = contents->GetFrameTree()->root();

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
  EXPECT_TRUE(ExecuteScript(root, create_subframe_script));
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
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      root->child_at(1)->child_at(0),
      "window.domAutomationController.send("
      "document.featurePolicy.allowsFeature('autoplay'));",
      &success));
  EXPECT_TRUE(success);
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
  void AddDelayedResponse(const net::test_server::SendBytesCallback& send,
                          net::test_server::SendCompleteCallback done) {
    // Just create a closure that closes the socket without sending a response.
    // This will propagate an error to the underlying request.
    send_response_closures_.push_back(
        base::BindOnce(send, "", std::move(done)));
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
    return std::unique_ptr<net::test_server::BasicHttpResponse>();
  }

  // If there are no more requests to delay, post a series of tasks finishing
  // all the delayed tasks. This will be called on the test server's thread.
  void MaybeStartRequests() {
    for (auto it : num_remaining_requests_to_delay_for_path_) {
      if (it.second > 0)
        return;
    }
    for (auto& it : send_response_closures_) {
      std::move(it).Run();
    }
  }

  // This class passes the callbacks needed to respond to a request to the
  // underlying test fixture.
  class DelayedResponse : public net::test_server::BasicHttpResponse {
   public:
    explicit DelayedResponse(
        RequestDelayingSitePerProcessBrowserTest* test_harness)
        : test_harness_(test_harness) {}
    void SendResponse(const net::test_server::SendBytesCallback& send,
                      net::test_server::SendCompleteCallback done) override {
      test_harness_->AddDelayedResponse(send, std::move(done));
    }

   private:
    RequestDelayingSitePerProcessBrowserTest* test_harness_;

    DISALLOW_COPY_AND_ASSIGN(DelayedResponse);
  };

  // Set of closures to call which will complete delayed requests. May only be
  // modified on the test_server_'s thread.
  std::vector<base::OnceClosure> send_response_closures_;

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
  bool result;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(shell(), "createFrames()", &result));
  EXPECT_TRUE(result);
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
  bool result;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(shell(), "createFrames()", &result));
  EXPECT_TRUE(result);
}

#if defined(OS_ANDROID)
class TextSelectionObserver : public TextInputManager::Observer {
 public:
  explicit TextSelectionObserver(TextInputManager* text_input_manager)
      : text_input_manager_(text_input_manager) {
    text_input_manager->AddObserver(this);
  }

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

  TextInputManager* const text_input_manager_;
  std::string last_selected_text_;
  std::string expected_text_;
  scoped_refptr<MessageLoopRunner> loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(TextSelectionObserver);
};

class SitePerProcessAndroidImeTest : public SitePerProcessBrowserTest {
 public:
  SitePerProcessAndroidImeTest() : SitePerProcessBrowserTest() {}
  ~SitePerProcessAndroidImeTest() override {}

 protected:
  ImeAdapterAndroid* ime_adapter() {
    return static_cast<RenderWidgetHostViewAndroid*>(
               web_contents()->GetRenderWidgetHostView())
        ->ime_adapter_for_testing();
  }

  void FocusInputInFrame(RenderFrameHostImpl* frame) {
    ASSERT_TRUE(ExecuteScript(frame, "window.focus(); input.focus();"));
  }

  // Creates a page with multiple (nested) OOPIFs and populates all of them
  // with an <input> element along with the required handlers for the test.
  void LoadPage() {
    ASSERT_TRUE(NavigateToURL(
        shell(),
        GURL(embedded_test_server()->GetURL(
            "a.com", "/cross_site_iframe_factory.html?a(b,c(a(b)))"))));
    FrameTreeNode* root = web_contents()->GetFrameTree()->root();
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

    for (auto* frame : frames_)
      ASSERT_TRUE(ExecuteScript(frame, add_input_script));
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

  std::vector<RenderFrameHostImpl*> frames_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SitePerProcessAndroidImeTest);
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
#endif  // OS_ANDROID

// Test that an OOPIF at b.com can navigate to a cross-site a.com URL that
// transfers back to b.com.  See https://crbug.com/681077#c10 and
// https://crbug.com/660407.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       SubframeTransfersToCurrentRFH) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  ASSERT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
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
                            ->GetFrameTree()
                            ->root();

  // The main frame and the subframe live on different processes.
  EXPECT_EQ(1u, root->child_count());
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            root->child_at(0)->current_frame_host()->GetSiteInstance());

  // Make a form submission from the main frame and target the OOPIF.
  GURL form_url(embedded_test_server()->GetURL("/echoall"));
  TestNavigationObserver form_post_observer(shell()->web_contents(), 1);
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(), JsReplace(R"(
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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // The main frame and the subframe live on different processes.
  EXPECT_EQ(1u, root->child_count());
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            root->child_at(0)->current_frame_host()->GetSiteInstance());

  // Make a form submission from the subframe and target its parent frame.
  GURL form_url(embedded_test_server()->GetURL("/echoall"));
  TestNavigationObserver form_post_observer(web_contents());
  EXPECT_TRUE(ExecuteScript(root->child_at(0)->current_frame_host(),
                            JsReplace(R"(
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
  std::string body;
  EXPECT_TRUE(ExecuteScriptAndExtractString(root, R"(
    var body = document.getElementsByTagName('pre')[0].innerText;
    window.domAutomationController.send(body);)", &body));
  EXPECT_EQ("my_token=my_value\n", body);

  // Reload the main frame and ensure the POST body is preserved.  This checks
  // that the POST body was saved in the FrameNavigationEntry.
  web_contents()->GetController().Reload(ReloadType::NORMAL,
                                         false /* check_for_repost */);
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  body = "";
  EXPECT_TRUE(ExecuteScriptAndExtractString(root, R"(
    var body = document.getElementsByTagName('pre')[0].innerText;
    window.domAutomationController.send(body);)", &body));
  EXPECT_EQ("my_token=my_value\n", body);
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
  EXPECT_EQ(bar_url, web_contents()->GetMainFrame()->GetLastCommittedURL());

  // Go back again.  This should go to foo.com.
  {
    TestNavigationObserver back_observer(web_contents());
    web_contents()->GetController().GoBack();
    back_observer.Wait();
  }
  EXPECT_EQ(foo_url, web_contents()->GetMainFrame()->GetLastCommittedURL());
}

// Tests that when a frame contains a modal <dialog> element, out-of-process
// iframe children cannot take focus, because they are inert.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, CrossProcessInertSubframe) {
  // This uses a(b,b) instead of a(b) to preserve the b.com process even when
  // the first subframe is navigated away from it.
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  ASSERT_EQ(2U, root->child_count());

  FrameTreeNode* iframe_node = root->child_at(0);

  EXPECT_TRUE(ExecuteScript(
      iframe_node,
      "document.head.innerHTML = '';"
      "document.body.innerHTML = '<input id=\"text1\"> <input id=\"text2\">';"
      "text1.focus();"));

  // Add a <dialog> to the root frame and call showModal on it.
  EXPECT_TRUE(ExecuteScript(root,
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

  // Attempt to change focus in the inert subframe. This should fail.
  // The setTimeout ensures that the inert bit can propagate before the
  // test JS code runs.
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      iframe_node,
      "window.setTimeout(() => {text2.focus();"
      "domAutomationController.send(document.activeElement.id);}, 0)",
      &focused_element));
  EXPECT_EQ("", focused_element);

  // Navigate the child frame to another site, so that it moves into a new
  // process.
  GURL site_url(embedded_test_server()->GetURL("c.com", "/title1.html"));
  NavigateFrameToURL(iframe_node, site_url);

  // NavigateFrameToURL returns when the navigation commits, at which point
  // frame state has to be re-sent to the new frame.
  // Yield the thread to prevent races with the inertness update.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(ExecuteScript(
      iframe_node,
      "document.head.innerHTML = '';"
      "document.body.innerHTML = '<input id=\"text1\"> <input id=\"text2\">';"
      "text1.focus();"));

  // Verify that inertness was preserved across the navigation.
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      iframe_node,
      "text2.focus();"
      "domAutomationController.send(document.activeElement.id);",
      &focused_element));
  EXPECT_EQ("", focused_element);

  // Navigate the subframe back into its parent process to verify that the
  // new local frame remains inert.
  GURL same_site_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  NavigateFrameToURL(iframe_node, same_site_url);

  EXPECT_TRUE(ExecuteScript(
      iframe_node,
      "document.head.innerHTML = '';"
      "document.body.innerHTML = '<input id=\"text1\"> <input id=\"text2\">';"
      "text1.focus();"));

  // Verify that inertness was preserved across the navigation.
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      iframe_node,
      "text2.focus();"
      "domAutomationController.send(document.activeElement.id);",
      &focused_element));
  EXPECT_EQ("", focused_element);
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

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Create an unrelated shell window.
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));
  Shell* new_shell = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(new_shell, url_b));

  FrameTreeNode* new_shell_root =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetFrameTree()
          ->root();

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

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

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
          ->GetFrameTree()
          ->root();

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
// All cross-process navigations now require creating a RenderFrameProxy before
// creating a RenderFrame, which makes such navigations follow the provisional
// frame (remote-to-local navigation) paths, where such a scenario is no longer
// possible.  See https://crbug.com/756790.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       TwoCrossSitePendingNavigationsAndMainFrameWins) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);

  // Navigate both frames cross-site to b.com simultaneously.
  GURL new_url_1(embedded_test_server()->GetURL("b.com", "/title1.html"));
  GURL new_url_2(embedded_test_server()->GetURL("b.com", "/title2.html"));
  TestNavigationManager manager1(web_contents(), new_url_1);
  TestNavigationManager manager2(web_contents(), new_url_2);
  auto script = JsReplace("location = $1; frames[0].location = $2;", new_url_1,
                          new_url_2);
  EXPECT_TRUE(ExecuteScript(web_contents(), script));

  // Wait for main frame request, but don't commit it yet.  This should create
  // a speculative RenderFrameHost.
  ASSERT_TRUE(manager1.WaitForRequestStart());
  RenderFrameHostImpl* root_speculative_rfh =
      root->render_manager()->speculative_frame_host();
  EXPECT_TRUE(root_speculative_rfh);
  scoped_refptr<SiteInstanceImpl> b_root_site_instance(
      root_speculative_rfh->GetSiteInstance());

  // There should now be a live b.com proxy for the root, since it is doing a
  // cross-process navigation.
  RenderFrameProxyHost* root_proxy =
      root->render_manager()->GetRenderFrameProxyHost(
          b_root_site_instance.get());
  EXPECT_TRUE(root_proxy);
  EXPECT_TRUE(root_proxy->is_render_frame_proxy_live());

  // Wait for subframe request, but don't commit it yet.
  ASSERT_TRUE(manager2.WaitForRequestStart());
  RenderFrameHostImpl* subframe_speculative_rfh =
      child->render_manager()->speculative_frame_host();
  EXPECT_TRUE(child->render_manager()->speculative_frame_host());
  scoped_refptr<SiteInstanceImpl> b_subframe_site_instance(
      subframe_speculative_rfh->GetSiteInstance());

  // Similarly, the subframe should also have a b.com proxy (unused in this
  // test), since it is also doing a cross-process navigation.
  RenderFrameProxyHost* child_proxy =
      child->render_manager()->GetRenderFrameProxyHost(
          b_subframe_site_instance.get());
  EXPECT_TRUE(child_proxy);
  EXPECT_TRUE(child_proxy->is_render_frame_proxy_live());

  // Now let the main frame commit.
  manager1.WaitForNavigationFinished();

  // Make sure the process is live and at the new URL.
  EXPECT_TRUE(b_root_site_instance->GetProcess()->IsInitializedAndNotDead());
  EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());
  EXPECT_EQ(root_speculative_rfh, root->current_frame_host());
  EXPECT_EQ(new_url_1, root->current_frame_host()->GetLastCommittedURL());

  // The subframe should be gone, so the second navigation should have no
  // effect.
  manager2.WaitForNavigationFinished();

  // The new commit should have detached the old child frame.
  EXPECT_EQ(0U, root->child_count());
  int length = -1;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      web_contents(), "domAutomationController.send(frames.length);", &length));
  EXPECT_EQ(0, length);

  // The root proxy should be gone.
  EXPECT_FALSE(root->render_manager()->GetRenderFrameProxyHost(
      b_subframe_site_instance.get()));
}

// Similar to TwoCrossSitePendingNavigationsAndMainFrameWins, but checks the
// case where the subframe navigation commits before the main frame.  See
// https://crbug.com/756790.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       TwoCrossSitePendingNavigationsAndSubframeWins) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a,a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);
  FrameTreeNode* child2 = root->child_at(1);

  // Install postMessage handlers in main frame and second subframe for later
  // use.
  EXPECT_TRUE(
      ExecuteScript(root->current_frame_host(),
                    "window.addEventListener('message', function(event) {\n"
                    "  event.source.postMessage(event.data + '-reply', '*');\n"
                    "});"));
  EXPECT_TRUE(ExecuteScript(
      child2->current_frame_host(),
      "window.addEventListener('message', function(event) {\n"
      "  event.source.postMessage(event.data + '-subframe-reply', '*');\n"
      "});"));

  // Start a main frame navigation to b.com.
  GURL new_url_1(embedded_test_server()->GetURL("b.com", "/title1.html"));
  TestNavigationManager manager1(web_contents(), new_url_1);
  EXPECT_TRUE(
      ExecuteScript(web_contents(), JsReplace("location = $1", new_url_1)));

  // Wait for main frame request and check the frame tree.  There should be a
  // proxy for b.com at the root, but nowhere else at this point.
  ASSERT_TRUE(manager1.WaitForRequestStart());
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
  EXPECT_TRUE(ExecuteScript(web_contents(),
                            JsReplace("frames[0].location = $1", new_url_2)));

  // Wait for subframe request.
  ASSERT_TRUE(manager2.WaitForRequestStart());
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
  manager2.WaitForNavigationFinished();

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
      ExecuteScript(child->current_frame_host(),
                    "window.addEventListener('message', function(event) {\n"
                    "  domAutomationController.send(event.data);\n"
                    "});"));
  std::string response;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      child, "parent.postMessage('root-ping', '*')", &response));
  EXPECT_EQ("root-ping-reply", response);

  EXPECT_TRUE(ExecuteScriptAndExtractString(
      child, "parent.frames[1].postMessage('sibling-ping', '*')", &response));
  EXPECT_EQ("sibling-ping-subframe-reply", response);

  // Cancel the pending main frame navigation, and verify that the subframe can
  // still communicate with the (old) main frame.
  root->navigator().CancelNavigation(root);
  EXPECT_FALSE(root->render_manager()->speculative_frame_host());
  response = "";
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      child, "parent.postMessage('root-ping', '*')", &response));
  EXPECT_EQ("root-ping-reply", response);
}

// Similar to TwoCrossSitePendingNavigations* tests above, but checks the case
// where the current window and its opener navigate simultaneously.
// See https://crbug.com/756790.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       TwoCrossSitePendingNavigationsWithOpener) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Install a postMessage handler in main frame for later use.
  EXPECT_TRUE(
      ExecuteScript(web_contents(),
                    "window.addEventListener('message', function(event) {\n"
                    "  event.source.postMessage(event.data + '-reply', '*');\n"
                    "});"));

  Shell* popup_shell =
      OpenPopup(shell()->web_contents(), GURL(url::kAboutBlankURL), "popup");

  // Start a navigation to b.com in the first (opener) tab.
  GURL new_url_1(embedded_test_server()->GetURL("b.com", "/title1.html"));
  TestNavigationManager manager(web_contents(), new_url_1);
  EXPECT_TRUE(
      ExecuteScript(web_contents(), JsReplace("location = $1", new_url_1)));
  ASSERT_TRUE(manager.WaitForRequestStart());

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
      root->render_manager()->GetRenderFrameProxyHost(b_site_instance.get());
  EXPECT_TRUE(proxy);
  EXPECT_TRUE(proxy->is_render_frame_proxy_live());

  // Make sure the second tab can communicate to its (old) opener remote frame.
  // The postMessage should go to the current RenderFrameHost rather than the
  // pending one in the first tab's main frame.
  EXPECT_TRUE(
      ExecuteScript(popup_shell->web_contents(),
                    "window.addEventListener('message', function(event) {\n"
                    "  domAutomationController.send(event.data);\n"
                    "});"));

  std::string response;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      popup_shell->web_contents(), "opener.postMessage('opener-ping', '*');",
      &response));
  EXPECT_EQ("opener-ping-reply", response);

  // Cancel the pending main frame navigation, and verify that the subframe can
  // still communicate with the (old) main frame.
  root->navigator().CancelNavigation(root);
  EXPECT_FALSE(root->render_manager()->speculative_frame_host());
  response = "";
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      popup_shell->web_contents(), "opener.postMessage('opener-ping', '*')",
      &response));
  EXPECT_EQ("opener-ping-reply", response);
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
  ASSERT_TRUE(nav_manager.WaitForRequestStart());
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
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

#if defined(OS_ANDROID)

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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Start a cross-site navigation.
  GURL url2(embedded_test_server()->GetURL("b.com", "/title2.html"));
  TestNavigationManager nav_manager(web_contents(), url2);
  shell()->LoadURL(url2);

  // Wait for the request, but don't commit it yet. This should create a
  // speculative RenderFrameHost.
  ASSERT_TRUE(nav_manager.WaitForRequestStart());
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
  ui::MotionEventAndroid event(nullptr, nullptr, 1.f / root_view->GetDipScale(),
                               0.f, 0.f, 0.f, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0,
                               false, &pointer0, &pointer1);
  root_view->OnTouchEventForTesting(event);

  EXPECT_TRUE(mock_handler.did_receive_event());
  EXPECT_FALSE(mock_handler_speculative.did_receive_event());
}

IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, TestChildProcessImportance) {
  web_contents()->SetMainFrameImportance(ChildProcessImportance::MODERATE);

  // Construct root page with one child in different domain.
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
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
  web_contents()->SetMainFrameImportance(ChildProcessImportance::NORMAL);
  EXPECT_EQ(ChildProcessImportance::NORMAL,
            root->current_frame_host()->GetProcess()->GetEffectiveImportance());
  EXPECT_EQ(
      ChildProcessImportance::NORMAL,
      child->current_frame_host()->GetProcess()->GetEffectiveImportance());
  web_contents()->SetMainFrameImportance(ChildProcessImportance::IMPORTANT);
  EXPECT_EQ(ChildProcessImportance::IMPORTANT,
            root->current_frame_host()->GetProcess()->GetEffectiveImportance());
  EXPECT_EQ(
      ChildProcessImportance::NORMAL,
      child->current_frame_host()->GetProcess()->GetEffectiveImportance());

  // Check importance is maintained if child navigates to new domain.
  int old_child_process_id = child->current_frame_host()->GetProcess()->GetID();
  GURL url = embedded_test_server()->GetURL("foo.com", "/title2.html");
  NavigateFrameToURL(root->child_at(0), url);
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
  NavigateFrameToURL(root, url);
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

  ~TouchSelectionControllerClientTestWrapper() override {}

  void InitWaitForSelectionEvent(ui::SelectionEventType expected_event) {
    DCHECK(!run_loop_);
    expected_event_ = expected_event;
    run_loop_.reset(new base::RunLoop());
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

  void OnDragUpdate(const gfx::PointF& position) override {}

  ui::SelectionEventType expected_event_;
  std::unique_ptr<base::RunLoop> run_loop_;
  // Not owned.
  ui::TouchSelectionControllerClient* client_;

  DISALLOW_COPY_AND_ASSIGN(TouchSelectionControllerClientTestWrapper);
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
    DelayBy(base::TimeDelta::FromMilliseconds(2000));
    SendTouch(main_view, ui::MotionEvent::Action::UP, point);
  }

  void SimpleTap(gfx::Point point) {
    // Get main frame view for event insertion.
    RenderWidgetHostViewAndroid* main_view = GetRenderWidgetHostViewAndroid();

    SendTouch(main_view, ui::MotionEvent::Action::DOWN, point);
    // tiny_timeout() is way shorter than a reasonable user-created tap gesture,
    // so we use a custom timeout here.
    DelayBy(base::TimeDelta::FromMilliseconds(300));
    SendTouch(main_view, ui::MotionEvent::Action::UP, point);
  }

  void SetupTest() {
    GURL test_url(embedded_test_server()->GetURL(
        "a.com", "/cross_site_iframe_factory.html?a(a)"));
    EXPECT_TRUE(NavigateToURL(shell(), test_url));
    frame_observer_ = std::make_unique<RenderFrameSubmissionObserver>(
        shell()->web_contents());
    FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                              ->GetFrameTree()
                              ->root();
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
        base::WrapUnique(selection_controller_client_));

    // We need to load the desired subframe and then wait until it's stable,
    // i.e. generates no new compositor frames for some reasonable time period:
    // a stray frame between touch selection's pre-handling of GestureLongPress
    // and the expected frame containing the selected region can confuse the
    // TouchSelectionController, causing it to fail to show selection handles.
    // Note this is an issue with the TouchSelectionController in general, and
    // not a property of this test.
    GURL child_url(
        embedded_test_server()->GetURL("b.com", "/touch_selection.html"));
    NavigateFrameToURL(child_frame_tree_node_, child_url);
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
    std::string str;
    EXPECT_TRUE(ExecuteScriptAndExtractString(
        child_frame_tree_node_->current_frame_host(), "get_point_inside_text()",
        &str));
    ConvertJSONToPoint(str, &point_f);
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
        DelayBy(base::TimeDelta::FromMilliseconds(100));
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
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
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
    auto time_ms = (ui::EventTimeForNow() - base::TimeTicks()).InMilliseconds();
    ui::MotionEventAndroid touch(
        env, nullptr, 1.f, 0, 0, 0, time_ms,
        ui::MotionEventAndroid::GetAndroidAction(action), 1, 0, 0, 0, 0, 0, 0,
        0, 0, false, &p, nullptr);
    view->OnTouchEvent(touch);
  }

  RenderWidgetHostViewAndroid* root_rwhv_;
  RenderWidgetHostViewChildFrame* child_rwhv_;
  FrameTreeNode* child_frame_tree_node_;
  std::unique_ptr<RenderFrameSubmissionObserver> frame_observer_;
  TouchSelectionControllerClientTestWrapper* selection_controller_client_;

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
  DelayBy(base::TimeDelta::FromMilliseconds(2000));

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
#if defined(OS_ANDROID)  // Flaky on Android.  See https://crbug.com/906204.
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
#endif  // defined(OS_ANDROID)

class TouchEventObserver : public RenderWidgetHost::InputEventObserver {
 public:
  TouchEventObserver(std::vector<uint32_t>* outgoing_touch_event_ids,
                     std::vector<uint32_t>* acked_touch_event_ids)
      : outgoing_touch_event_ids_(outgoing_touch_event_ids),
        acked_touch_event_ids_(acked_touch_event_ids) {}

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
  std::vector<uint32_t>* outgoing_touch_event_ids_;
  std::vector<uint32_t>* acked_touch_event_ids_;
  DISALLOW_COPY_AND_ASSIGN(TouchEventObserver);
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

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  ASSERT_EQ(1u, root->child_count());
  FrameTreeNode* child_node = root->child_at(0);

  // Add a *slow* & non-passive touch event handler in the child. It needs to
  // be non-passive to ensure TouchStart doesn't get acked until after the
  // touch handler completes.
  EXPECT_TRUE(ExecuteScript(child_node,
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
    std::string str;
    EXPECT_TRUE(ExecuteScriptAndExtractString(
        child_node,
        "var rect = document.body.getBoundingClientRect();\
         var point = {\
           x: rect.left + rect.width / 2,\
           y: rect.top + rect.height / 2\
         };\
         window.domAutomationController.send(JSON.stringify(point));",
        &str));
    ConvertJSONToPoint(str, &child_tap_point);
    child_tap_point = child_node->current_frame_host()
                          ->GetView()
                          ->TransformPointToRootCoordSpaceF(child_tap_point);
  }
  SyntheticTapGestureParams child_tap_params;
  child_tap_params.position = child_tap_point;
  child_tap_params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  child_tap_params.duration_ms = 300.f;
  auto child_tap_gesture =
      std::make_unique<SyntheticTapGesture>(child_tap_params);

  // Create GestureTap for root.
  SyntheticTapGestureParams root_tap_params;
  root_tap_params.position = gfx::PointF(5.f, 5.f);
  root_tap_params.duration_ms = 300.f;
  root_tap_params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
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
  int child_touch_event_count = 0;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      child_node, "window.domAutomationController.send(touch_event_count);",
      &child_touch_event_count));
  EXPECT_EQ(2, child_touch_event_count);

  // Verify Acks from parent arrive first.
  EXPECT_EQ(4u, outgoing_touch_event_ids.size());
  EXPECT_EQ(4u, acked_touch_event_ids.size());
  EXPECT_EQ(outgoing_touch_event_ids[2], acked_touch_event_ids[0]);
  EXPECT_EQ(outgoing_touch_event_ids[3], acked_touch_event_ids[1]);

  // Verify no DCHECKs from GestureRecognizer, indicating acks happened in
  // order.
  child_gesture_tap_ack_waiter.Wait();
}

// This test verifies that the main-frame's page scale factor propagates to
// the compositor layertrees in each of the child processes.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       PageScaleFactorPropagatesToOOPIFs) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c),d)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  ASSERT_EQ(2u, root->child_count());
  FrameTreeNode* child_b = root->child_at(0);
  FrameTreeNode* child_c = root->child_at(1);
  ASSERT_EQ(1U, child_b->child_count());
  FrameTreeNode* child_d = child_b->child_at(0);

  ASSERT_TRUE(child_b);
  ASSERT_TRUE(child_c);
  ASSERT_TRUE(child_d);

  EXPECT_EQ(
      " Site A ------------ proxies for B C D\n"
      "   |--Site B ------- proxies for A C D\n"
      "   |    +--Site C -- proxies for A B D\n"
      "   +--Site D ------- proxies for A B C\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/\n"
      "      C = http://c.com/\n"
      "      D = http://d.com/",
      DepictFrameTree(root));

  RenderFrameSubmissionObserver observer_a(root);
  RenderFrameSubmissionObserver observer_b(child_b);
  RenderFrameSubmissionObserver observer_c(child_c);
  RenderFrameSubmissionObserver observer_d(child_d);

  // Monitor visual sync messages coming from the mainframe to make sure
  // |is_pinch_gesture_active| goes true during the pinch gesture.
  auto filter_mainframe =
      base::MakeRefCounted<SynchronizeVisualPropertiesMessageFilter>();
  root->current_frame_host()->GetProcess()->AddFilter(filter_mainframe.get());
  // Monitor frame sync messages coming from child_b as it will need to
  // relay them to child_d.
  auto filter_child_b =
      base::MakeRefCounted<SynchronizeVisualPropertiesMessageFilter>();
  child_b->current_frame_host()->GetProcess()->AddFilter(filter_child_b.get());

  // We need to observe a root frame submission to pick up the initial page
  // scale factor.
  observer_a.WaitForAnyFrameSubmission();

  const float kPageScaleDelta = 2.f;
  // On desktop systems we expect |current_page_scale| to be 1.f, but on
  // Android it will typically be less than 1.f, and may take on arbitrary
  // values.
  float current_page_scale =
      observer_a.LastRenderFrameMetadata().page_scale_factor;
  float target_page_scale = current_page_scale * kPageScaleDelta;

  SyntheticPinchGestureParams params;
  auto* host = static_cast<RenderWidgetHostImpl*>(
      root->current_frame_host()->GetRenderWidgetHost());
  gfx::Rect bounds(host->GetView()->GetViewBounds().size());
  // The synthetic gesture code expects a location in root-view coordinates.
  params.anchor = gfx::PointF(bounds.CenterPoint());
  // In SyntheticPinchGestureParams, |scale_factor| is really a delta.
  params.scale_factor = kPageScaleDelta;
#if defined(OS_MAC)
  auto synthetic_pinch_gesture =
      std::make_unique<SyntheticTouchpadPinchGesture>(params);
#else
  auto synthetic_pinch_gesture =
      std::make_unique<SyntheticTouchscreenPinchGesture>(params);
#endif

  // Send pinch gesture and verify we receive the ack.
  InputEventAckWaiter ack_waiter(host,
                                 blink::WebInputEvent::Type::kGesturePinchEnd);
  host->QueueSyntheticGesture(
      std::move(synthetic_pinch_gesture),
      base::BindOnce([](SyntheticGesture::Result result) {
        EXPECT_EQ(SyntheticGesture::GESTURE_FINISHED, result);
      }));
  ack_waiter.Wait();

  // Make sure all the page scale values behave as expected.
  const float kScaleTolerance = 0.1f;
  observer_a.WaitForPageScaleFactor(target_page_scale, kScaleTolerance);
  observer_b.WaitForExternalPageScaleFactor(target_page_scale, kScaleTolerance);
  observer_c.WaitForExternalPageScaleFactor(target_page_scale, kScaleTolerance);
  observer_d.WaitForExternalPageScaleFactor(target_page_scale, kScaleTolerance);

  // The change in |is_pinch_gesture_active| that signals the end of the pinch
  // gesture will occur sometime after the ack for GesturePinchEnd, so we need
  // to wait for it from each renderer. If it's never seen, the test fails by
  // timing out.
  filter_mainframe->WaitForPinchGestureEnd();
  EXPECT_TRUE(filter_mainframe->pinch_gesture_active_set());
  EXPECT_TRUE(filter_mainframe->pinch_gesture_active_cleared());
  filter_child_b->WaitForPinchGestureEnd();
  EXPECT_TRUE(filter_child_b->pinch_gesture_active_set());
  EXPECT_TRUE(filter_child_b->pinch_gesture_active_cleared());
}

// Verify that sandbox flags specified by a CSP header are properly inherited by
// child frames, but are removed when the frame navigates.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       ActiveSandboxFlagsMaintainedAcrossNavigation) {
  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/sandbox_main_frame_csp.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  ASSERT_EQ(1u, root->child_count());

  EXPECT_EQ(
      " Site A\n"
      "   +--Site A\n"
      "Where A = http://a.com/",
      DepictFrameTree(root));

  FrameTreeNode* child_node = root->child_at(0);

  EXPECT_EQ(shell()->web_contents()->GetSiteInstance(),
            child_node->current_frame_host()->GetSiteInstance());

  // Main page is served with a CSP header applying sandbox flags allow-popups,
  // allow-pointer-lock and allow-scripts.
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            root->pending_frame_policy().sandbox_flags);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            root->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kAll &
                ~network::mojom::WebSandboxFlags::kPopups &
                ~network::mojom::WebSandboxFlags::kPointerLock &
                ~network::mojom::WebSandboxFlags::kScripts &
                ~network::mojom::WebSandboxFlags::kAutomaticFeatures,
            root->active_sandbox_flags());

  // Child frame has iframe sandbox flags allow-popups, allow-scripts, and
  // allow-orientation-lock. It should receive the intersection of those with
  // the parent sandbox flags: allow-popups and allow-scripts.
  EXPECT_EQ(network::mojom::WebSandboxFlags::kAll &
                ~network::mojom::WebSandboxFlags::kPopups &
                ~network::mojom::WebSandboxFlags::kScripts &
                ~network::mojom::WebSandboxFlags::kAutomaticFeatures,
            root->child_at(0)->pending_frame_policy().sandbox_flags);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kAll &
                ~network::mojom::WebSandboxFlags::kPopups &
                ~network::mojom::WebSandboxFlags::kScripts &
                ~network::mojom::WebSandboxFlags::kAutomaticFeatures,
            root->child_at(0)->effective_frame_policy().sandbox_flags);

  // Document in child frame is served with a CSP header giving sandbox flags
  // allow-scripts, allow-popups and allow-pointer-lock. The final effective
  // flags should only include allow-scripts and allow-popups.
  EXPECT_EQ(network::mojom::WebSandboxFlags::kAll &
                ~network::mojom::WebSandboxFlags::kPopups &
                ~network::mojom::WebSandboxFlags::kScripts &
                ~network::mojom::WebSandboxFlags::kAutomaticFeatures,
            root->child_at(0)->active_sandbox_flags());

  // Navigate the child frame to a new page. This should clear any CSP-applied
  // sandbox flags.
  GURL frame_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  NavigateFrameToURL(root->child_at(0), frame_url);

  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            child_node->current_frame_host()->GetSiteInstance());

  // Navigating should reset the sandbox flags to the frame owner flags:
  // allow-popups and allow-scripts.
  EXPECT_EQ(network::mojom::WebSandboxFlags::kAll &
                ~network::mojom::WebSandboxFlags::kPopups &
                ~network::mojom::WebSandboxFlags::kScripts &
                ~network::mojom::WebSandboxFlags::kAutomaticFeatures,
            root->child_at(0)->active_sandbox_flags());
  EXPECT_EQ(network::mojom::WebSandboxFlags::kAll &
                ~network::mojom::WebSandboxFlags::kPopups &
                ~network::mojom::WebSandboxFlags::kScripts &
                ~network::mojom::WebSandboxFlags::kAutomaticFeatures,
            root->child_at(0)->pending_frame_policy().sandbox_flags);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kAll &
                ~network::mojom::WebSandboxFlags::kPopups &
                ~network::mojom::WebSandboxFlags::kScripts &
                ~network::mojom::WebSandboxFlags::kAutomaticFeatures,
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
                            ->GetFrameTree()
                            ->root();

  RenderFrameHostImpl* rfh =
      static_cast<WebContentsImpl*>(shell()->web_contents())->GetMainFrame();

  // Check sandbox flags on RFH before navigating away.
  EXPECT_EQ(network::mojom::WebSandboxFlags::kAll &
                ~network::mojom::WebSandboxFlags::kPopups &
                ~network::mojom::WebSandboxFlags::kPointerLock &
                ~network::mojom::WebSandboxFlags::kScripts &
                ~network::mojom::WebSandboxFlags::kAutomaticFeatures,
            rfh->active_sandbox_flags());

  // Set up a slow unload handler to force the RFH to linger in the unloaded but
  // not-yet-deleted state.
  EXPECT_TRUE(
      ExecuteScript(rfh, "window.onunload=function(e){ while(1); };\n"));

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
  // 2) Pending deletion, waiting for the FrameHostMsg_Unload_ACK.
  // As a result, it must still be alive.
  ASSERT_TRUE(rfh->IsRenderFrameLive());
  EXPECT_THAT(
      rfh->lifecycle_state(),
      testing::AnyOf(
          testing::Eq(
              RenderFrameHostImpl::LifecycleState::kRunningUnloadHandlers),
          testing::Eq(
              RenderFrameHostImpl::LifecycleState::kInBackForwardCache)));

  ASSERT_FALSE(rfh_observer.deleted());

  // Check sandbox flags on old RFH -- they should be unchanged.
  EXPECT_EQ(network::mojom::WebSandboxFlags::kAll &
                ~network::mojom::WebSandboxFlags::kPopups &
                ~network::mojom::WebSandboxFlags::kPointerLock &
                ~network::mojom::WebSandboxFlags::kScripts &
                ~network::mojom::WebSandboxFlags::kAutomaticFeatures,
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
  GURL main_url(embedded_test_server()->GetURL(
      "foo.com", "/cross_site_iframe_factory.html?foo(bar)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  TestNavigationObserver observer(shell()->web_contents());

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://foo.com/\n"
      "      B = http://bar.com/",
      DepictFrameTree(root));

  // Navigate the child to a CSP-sandboxed page on the same origin as it is
  // currently. This should update the flags in its proxies as well.
  NavigateFrameToURL(
      root->child_at(0),
      embedded_test_server()->GetURL("bar.com", "/csp_sandboxed_frame.html"));

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "        |--Site B -- proxies for A\n"
      "        +--Site B -- proxies for A\n"
      "Where A = http://foo.com/\n"
      "      B = http://bar.com/",
      DepictFrameTree(root));

  // Now navigate the first grandchild to a page on the same origin as the main
  // frame. It should still be sandboxed, as it should get its flags from its
  // (remote) parent.
  NavigateFrameToURL(root->child_at(0)->child_at(0),
                     embedded_test_server()->GetURL("foo.com", "/title1.html"));

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "        |--Site A -- proxies for B\n"
      "        +--Site B -- proxies for A\n"
      "Where A = http://foo.com/\n"
      "      B = http://bar.com/",
      DepictFrameTree(root));

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
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      root->child_at(0)->child_at(0),
      "window.domAutomationController.send("
      "    !window.open('data:text/html,dataurl'));",
      &success));
  EXPECT_TRUE(success);
  EXPECT_EQ(1u, Shell::windows().size());

  // Finally, navigate the grandchild frame to a new origin, creating a new site
  // instance. Again, the new document should be sandboxed, as it should get its
  // flags from its (remote) parent in B.
  NavigateFrameToURL(root->child_at(0)->child_at(0),
                     embedded_test_server()->GetURL("baz.com", "/title1.html"));

  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   +--Site B ------- proxies for A C\n"
      "        |--Site C -- proxies for A B\n"
      "        +--Site B -- proxies for A C\n"
      "Where A = http://foo.com/\n"
      "      B = http://bar.com/\n"
      "      C = http://baz.com/",
      DepictFrameTree(root));

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
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      root->child_at(0)->child_at(0),
      "window.domAutomationController.send("
      "    !window.open('data:text/html,dataurl'));",
      &success));
  EXPECT_TRUE(success);
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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  TestNavigationObserver observer(shell()->web_contents());

  // Navigate the child to a CSP-sandboxed page on the same origin as it is
  // currently. This should update the flags in its proxies as well.
  NavigateFrameToURL(
      root->child_at(0),
      embedded_test_server()->GetURL("bar.com", "/csp_sandboxed_frame.html"));

  // Now navigate the first grandchild to a page on the same origin as the main
  // frame. It should still be sandboxed, as it should get its flags from its
  // (remote) parent.
  NavigateFrameToURL(root->child_at(0)->child_at(0),
                     embedded_test_server()->GetURL("foo.com", "/title1.html"));

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
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      root->child_at(0)->child_at(0),
      "window.domAutomationController.send("
      "    !window.open('data:text/html,dataurl'));",
      &success));
  EXPECT_TRUE(success);
  EXPECT_EQ(1u, Shell::windows().size());

  // Update the sandbox attribute in the child frame. This should be overridden
  // by the CSP-set sandbox on this frame: The grandchild should *not* receive
  // an allowance for popups after it is navigated.
  EXPECT_TRUE(ExecuteScript(root->child_at(0),
                            "document.querySelector('iframe').sandbox = "
                            "    'allow-scripts allow-popups';"));
  // Finally, navigate the grandchild frame to another page on the top-level
  // origin; the active sandbox flags should still come from the it's parent's
  // CSP and the frame owner attributes.
  NavigateFrameToURL(root->child_at(0)->child_at(0),
                     embedded_test_server()->GetURL("foo.com", "/title2.html"));
  EXPECT_EQ(
      network::mojom::WebSandboxFlags::kAll &
          ~network::mojom::WebSandboxFlags::kScripts &
          ~network::mojom::WebSandboxFlags::kAutomaticFeatures,
      root->child_at(0)->child_at(0)->effective_frame_policy().sandbox_flags);
  EXPECT_EQ(
      root->child_at(0)->child_at(0)->active_sandbox_flags(),
      root->child_at(0)->child_at(0)->effective_frame_policy().sandbox_flags);
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      root->child_at(0)->child_at(0),
      "window.domAutomationController.send("
      "    !window.open('data:text/html,dataurl'));",
      &success));
  EXPECT_TRUE(success);
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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
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

  NavigateFrameToURL(
      root->child_at(1),
      embedded_test_server()->GetURL("bar.com", "/sandboxed_child_frame.html"));
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
  EXPECT_TRUE(ExecuteScript(root,
                            "document.querySelectorAll('iframe')[1]"
                            ".removeAttribute('sandbox');"));
  // Finally, navigate that child frame to another page on the same origin with
  // no CSP-set sandbox. Its sandbox flags should be completely cleared, and
  // should be cleared in the proxy in the main frame's renderer as well.
  // We can check that the flags were properly cleared by nesting another frame
  // under the child, and ensuring that *it* saw no sandbox flags in the
  // browser, or in the RemoteSecurityContext in the main frame's renderer.
  NavigateFrameToURL(
      root->child_at(1),
      embedded_test_server()->GetURL(
          "bar.com", "/cross_site_iframe_factory.html?bar(foo)"));

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
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      root->child_at(1)->child_at(0),
      "window.domAutomationController.send("
      "    !!window.open('data:text/html,dataurl'));",
      &success));
  EXPECT_TRUE(success);
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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);

  // Open an unrelated tab in a separate BrowsingInstance, and navigate it to
  // to bar.com.  This SiteInstance should have a default process reuse
  // policy - only subframes attempt process reuse.
  GURL bar_url(
      embedded_test_server()->GetURL("bar.com", "/page_with_iframe.html"));
  Shell* second_shell = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(second_shell, bar_url));
  scoped_refptr<SiteInstanceImpl> second_shell_instance =
      static_cast<SiteInstanceImpl*>(
          second_shell->web_contents()->GetMainFrame()->GetSiteInstance());
  EXPECT_FALSE(second_shell_instance->IsRelatedSiteInstance(
      root->current_frame_host()->GetSiteInstance()));
  RenderProcessHost* bar_process = second_shell_instance->GetProcess();
  EXPECT_EQ(SiteInstanceImpl::ProcessReusePolicy::DEFAULT,
            second_shell_instance->process_reuse_policy());

  // Now navigate the first tab's subframe to bar.com.  Confirm that it reuses
  // |bar_process|.
  NavigateIframeToURL(web_contents(), "test_iframe", bar_url);
  EXPECT_EQ(bar_url, child->current_url());
  EXPECT_EQ(bar_process, child->current_frame_host()->GetProcess());
  EXPECT_EQ(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE,
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
            second_shell->web_contents()->GetMainFrame()->GetProcess());

  // Navigate the second tab's subframe to bar.com, and check that this
  // new subframe reuses the process of the subframe in the first tab, even
  // though the two are in separate BrowsingInstances.
  NavigateIframeToURL(second_shell->web_contents(), "test_iframe", bar_url);
  FrameTreeNode* second_subframe =
      static_cast<WebContentsImpl*>(second_shell->web_contents())
          ->GetFrameTree()
          ->root()
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
      third_shell->web_contents()->GetMainFrame()->GetSiteInstance());
  EXPECT_NE(third_shell_instance,
            second_subframe->current_frame_host()->GetSiteInstance());
  EXPECT_NE(third_shell_instance,
            child->current_frame_host()->GetSiteInstance());
  EXPECT_NE(third_shell_instance->GetProcess(), bar_process);
}

// Check that when a subframe reuses an existing process for the same site
// across BrowsingInstances, a browser-initiated navigation in that subframe's
// tab doesn't unnecessarily share the reused process.  See
// https://crbug.com/803367.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       NoProcessSharingAfterSubframeReusesExistingProcess) {
  GURL foo_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
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
          ->GetFrameTree()
          ->root();
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
  EXPECT_EQ(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE,
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
  ~CommitMessageOrderReverser() override = default;

  void WaitForBothCommits() { outer_run_loop.Run(); }

 protected:
  bool WillProcessDidCommitNavigation(
      RenderFrameHost* render_frame_host,
      NavigationRequest* navigation_request,
      ::FrameHostMsg_DidCommitProvisionalLoad_Params* params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr* interface_params)
      override {
    // The DidCommitProvisionalLoad message is dispatched once this method
    // returns, so to defer committing the the navigation to |deferred_url_|,
    // run a nested message loop until the subsequent other commit message is
    // dispatched.
    if (params->url == deferred_url_) {
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

  DISALLOW_COPY_AND_ASSIGN(CommitMessageOrderReverser);
};

}  // namespace

// Create an out-of-process iframe that causes itself to be detached during
// its layout/animate phase. See https://crbug.com/802932.
//
// TODO(809580): Disabled on Android, Mac, and ChromeOS due to flakiness.
#if defined(OS_ANDROID) || defined(OS_MAC) || defined(OS_CHROMEOS)
#define MAYBE_OOPIFDetachDuringAnimation DISABLED_OOPIFDetachDuringAnimation
#else
#define MAYBE_OOPIFDetachDuringAnimation OOPIFDetachDuringAnimation
#endif
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       MAYBE_OOPIFDetachDuringAnimation) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/frame-detached-in-animationstart-event.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "        +--Site A -- proxies for B\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  FrameTreeNode* nested_child = root->child_at(0)->child_at(0);
  WaitForHitTestData(nested_child->current_frame_host());

  EXPECT_TRUE(
      ExecuteScript(nested_child->current_frame_host(), "startTest();"));

  // Test passes if the main renderer doesn't crash. Ping to verify.
  bool success;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      root->current_frame_host(), "window.domAutomationController.send(true);",
      &success));
  EXPECT_TRUE(success);
}

// Tests that a cross-process iframe asked to navigate to the same URL will
// successfully commit the navigation.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       IFrameSameDocumentNavigation) {
  GURL main_url(embedded_test_server()->GetURL(
      "foo.com", "/cross_site_iframe_factory.html?foo(bar)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* iframe = root->child_at(0);

  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            iframe->current_frame_host()->GetSiteInstance());

  // The iframe navigates same-document to a fragment.
  GURL iframe_fragment_url = GURL(iframe->current_url().spec() + "#foo");
  {
    TestNavigationObserver observer(shell()->web_contents());
    EXPECT_TRUE(
        ExecuteScript(iframe->current_frame_host(),
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
    EXPECT_TRUE(
        ExecuteScript(root->current_frame_host(),
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

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);

  GURL b_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  TestFrameNavigationObserver commit_observer(child);
  NavigationController::LoadURLParams params(b_url);
  params.transition_type = PageTransitionFromInt(ui::PAGE_TRANSITION_LINK);
  params.frame_tree_node_id = child->frame_tree_node_id();
  child->navigator().GetController()->LoadURLWithParams(params);
  commit_observer.WaitForCommit();

  int height = -1;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      child, "window.domAutomationController.send(window.innerHeight);",
      &height));

  EXPECT_GT(height, 0);
}

// Test that a late FrameHostMsg_Unload_ACK won't incorrectly mark
// RenderViewHost as inactive if it's already been reused and switched to active
// by another navigation.  See https://crbug.com/823567.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       RenderViewHostStaysActiveWithLateUnloadACK) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));

  // Open a popup and navigate it to a.com.
  Shell* popup = OpenPopup(
      shell(), embedded_test_server()->GetURL("a.com", "/title2.html"), "foo");
  WebContentsImpl* popup_contents =
      static_cast<WebContentsImpl*>(popup->web_contents());
  RenderFrameHostImpl* rfh = popup_contents->GetMainFrame();
  RenderViewHostImpl* rvh = rfh->render_view_host();

  // Disable the unload ACK and the unload timer.
  auto filter = base::MakeRefCounted<DropMessageFilter>(
      FrameMsgStart, FrameHostMsg_Unload_ACK::ID);
  rfh->GetProcess()->AddFilter(filter.get());
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
  RenderProcessHost* b_process = popup_contents->GetMainFrame()->GetProcess();
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
  // handler and sends the FrameHostMsg_Unload_ACK.
  rfh->OnUnloaded();

  // Wait for the new a.com navigation to finish.
  back_observer.Wait();

  // The RVH for a.com should've been reused, and it should be active.  Its
  // main frame should've been updated to the RFH from the back navigation.
  EXPECT_EQ(popup_contents->GetMainFrame()->render_view_host(), rvh);
  EXPECT_TRUE(rvh->is_active());
  EXPECT_EQ(rvh->GetMainFrame(), popup_contents->GetMainFrame());
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
          ->GetFrameTree()
          ->root()
          ->child_at(0);

  // Navigate popup's subframe to a page on a.com, which will generate
  // continuous compositor frames by incrementing a counter on the page.
  NavigateFrameToURL(popup_child,
                     embedded_test_server()->GetURL("a.com", "/counter.html"));

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

  FrameTreeNode* node = web_contents()->GetFrameTree()->root();
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

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  CheckFrameDepth(0u, root);

  FrameTreeNode* child0 = root->child_at(0);
  {
    EXPECT_EQ(1u, child0->depth());
    RenderProcessHost::Priority priority =
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
    EXPECT_EQ(2u, grand_child->depth());
    RenderProcessHost::Priority priority =
        grand_child->current_frame_host()->GetRenderWidgetHost()->GetPriority();
    EXPECT_EQ(2u, priority.frame_depth);
    // Same process as root
    EXPECT_EQ(0u,
              grand_child->current_frame_host()->GetProcess()->GetFrameDepth());
  }
}

IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, VisibilityFrameDepthTest) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL popup_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  Shell* new_shell = OpenPopup(root->child_at(0), popup_url, "");
  FrameTreeNode* popup_root =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetFrameTree()
          ->root();

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
  EXPECT_EQ(1, popup_process->VisibleClientCount());
  EXPECT_EQ(1u, popup_process->GetFrameDepth());

  // Unhide popup. Should go back to same state as before hide.
  new_shell->web_contents()->WasShown();
  EXPECT_EQ(2, popup_process->VisibleClientCount());
  EXPECT_EQ(0u, popup_process->GetFrameDepth());
}

// Flaky on multiple platforms (crbug.com/1094562).
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       DISABLED_FrameViewportIntersectionTestSimple) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c),d,e(f))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  scoped_refptr<UpdateViewportIntersectionMessageFilter> child2_filter =
      new UpdateViewportIntersectionMessageFilter();
  root->child_at(2)->current_frame_host()->GetProcess()->AddFilter(
      child2_filter.get());

  // Force lifecycle update in root and child2 to make sure child2 has sent
  // viewport intersection into to grand child before child2 becomes throttled.
  ASSERT_TRUE(EvalJsAfterLifecycleUpdate(root->current_frame_host(), "", "")
                  .error.empty());
  ASSERT_TRUE(EvalJsAfterLifecycleUpdate(
                  root->child_at(2)->current_frame_host(), "", "")
                  .error.empty());
  child2_filter->Clear();

  LayoutNonRecursiveForTestingViewportIntersection(shell()->web_contents());

  // Root should always intersect.
  EXPECT_TRUE(CheckIntersectsViewport(true, root));
  // Child 0 should be entirely in viewport.
  EXPECT_TRUE(CheckIntersectsViewport(true, root->child_at(0)));
  // Make sure child0 has has a chance to propagate viewport intersection to
  // grand child.
  ASSERT_TRUE(EvalJsAfterLifecycleUpdate(
                  root->child_at(0)->current_frame_host(), "", "")
                  .error.empty());
  // Grand child should match parent.
  EXPECT_TRUE(CheckIntersectsViewport(true, root->child_at(0)->child_at(0)));
  // Child 1 should be partially in viewport.
  EXPECT_TRUE(CheckIntersectsViewport(true, root->child_at(1)));
  // Child 2 should be not be in viewport.
  EXPECT_TRUE(CheckIntersectsViewport(false, root->child_at(2)));
  // Can't use EvalJsAfterLifecycleUpdate on child2, because it's
  // render-throttled. But it should still have propagated state down to the
  // grandchild.
  child2_filter->Wait();
  // Grand child should match parent.
  EXPECT_TRUE(CheckIntersectsViewport(false, root->child_at(2)->child_at(0)));
}

IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       FrameViewportOffsetTestSimple) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // This will catch b sending viewport intersection information to c.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  scoped_refptr<UpdateViewportIntersectionMessageFilter> filter =
      new UpdateViewportIntersectionMessageFilter();
  root->child_at(0)->current_frame_host()->GetProcess()->AddFilter(
      filter.get());

  // Use EvalJsAfterLifecycleUpdate to force animation frames in `a` and `b` to
  // ensure that the viewport intersection for initial layout state has been
  // propagated. The layout of `a` will not change again, so we can read back
  // its layout info after the animation frame. The layout of `b` will change,
  // so we don't read back its layout yet.
  std::string script(R"(
    let iframe = document.querySelector("iframe");
    [iframe.offsetLeft, iframe.offsetTop];
  )");
  EvalJsResult iframe_b_result =
      EvalJsAfterLifecycleUpdate(root->current_frame_host(), "", script);
  base::ListValue iframe_b_offset = iframe_b_result.ExtractList();
  int iframe_b_offset_left = iframe_b_offset.GetList()[0].GetInt();
  int iframe_b_offset_top = iframe_b_offset.GetList()[1].GetInt();

  // Make sure a new IPC is sent after dirty-ing layout.
  filter->Clear();

  // Dirty layout in `b` to generate a new IPC to `c`. This will be the final
  // layout state for `b`, so read back layout info here.
  std::string raf_script(R"(
    let iframe = document.querySelector("iframe");
    let margin = getComputedStyle(iframe).marginTop.replace("px", "");
    iframe.style.margin = String(parseInt(margin) + 1) + "px";
  )");
  EvalJsResult iframe_c_result = EvalJsAfterLifecycleUpdate(
      root->child_at(0)->current_frame_host(), raf_script, script);
  base::ListValue iframe_c_offset = iframe_c_result.ExtractList();
  int iframe_c_offset_left = iframe_c_offset.GetList()[0].GetInt();
  int iframe_c_offset_top = iframe_c_offset.GetList()[1].GetInt();

  // The IPC should already have been sent
  EXPECT_TRUE(filter->MessageReceived());

  // +4 for a 2px border on each iframe.
  gfx::PointF expected(iframe_b_offset_left + iframe_c_offset_left + 4,
                       iframe_b_offset_top + iframe_c_offset_top + 4);
  blink::ScreenInfo screen_info;
  root->render_manager()->GetRenderWidgetHostView()->GetScreenInfo(
      &screen_info);
  // Convert from CSS to physical pixels
  expected.Scale(screen_info.device_scale_factor);
  gfx::Transform actual = filter->GetIntersectionState().main_frame_transform;
  gfx::Point viewport_offset;
  EXPECT_TRUE(actual.TransformPointReverse(&viewport_offset));
  viewport_offset = gfx::Point(-viewport_offset.x(), -viewport_offset.y());
  float tolerance = ceilf(screen_info.device_scale_factor);
  EXPECT_NEAR(expected.x(), viewport_offset.x(), tolerance);
  EXPECT_NEAR(expected.y(), viewport_offset.y(), tolerance);
}

IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       FrameViewportIntersectionTestAggregate) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,c,a,b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Each immediate child is sized to 100% width and 75% height.
  LayoutNonRecursiveForTestingViewportIntersection(shell()->web_contents());

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Child 2 does not intersect, but shares widget with the main frame.
  FrameTreeNode* node = root->child_at(2);
  RenderProcessHost::Priority priority =
      node->current_frame_host()->GetRenderWidgetHost()->GetPriority();
  EXPECT_TRUE(priority.intersects_viewport);
  EXPECT_TRUE(
      node->current_frame_host()->GetProcess()->GetIntersectsViewport());

  // Child 3 does not intersect, but shares a process with child 0.
  node = root->child_at(3);
  priority = node->current_frame_host()->GetRenderWidgetHost()->GetPriority();
  EXPECT_FALSE(priority.intersects_viewport);
  EXPECT_TRUE(
      node->current_frame_host()->GetProcess()->GetIntersectsViewport());
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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  EXPECT_EQ(root, root->frame_tree()->GetFocusedFrame());

  // Add an onmessage handler to the subframe to send back a bool of whether
  // the subframe has focus.
  EXPECT_TRUE(
      ExecuteScript(root->child_at(0),
                    "window.addEventListener('message', function(event) {\n"
                    "  domAutomationController.send(document.hasFocus());\n"
                    "});"));

  // Now, send a postMessage from main frame to subframe, and then focus the
  // subframe in the same script.  postMessage should be scheduled after the
  // focus() call, so the IPC to focus the subframe should arrive before the
  // postMessage IPC, and the subframe should already know that it's focused in
  // the onmessage handler.
  bool child_has_focus = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(root,
                                          "frames[0].postMessage('','*');\n"
                                          "frames[0].focus();\n",
                                          &child_has_focus));
  EXPECT_TRUE(child_has_focus);
}

// Ensure that if a cross-process postMessage is scheduled, and then the target
// frame is detached before the postMessage is forwarded, the source frame's
// renderer does not crash.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       CrossProcessPostMessageAndDetachTarget) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Send a postMessage to the subframe and then immediately detach the
  // subframe.
  EXPECT_TRUE(ExecuteScript(root,
                            "frames[0].postMessage('','*');\n"
                            "document.body.removeChild(\n"
                            "    document.querySelector('iframe'));\n"));

  // Test passes if the main renderer doesn't crash.  Use setTimeout to ensure
  // this ping is evaluated after the (scheduled) postMessage is processed.
  bool success;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      root,
      "setTimeout(() => { window.domAutomationController.send(true); }, 0)",
      &success));
  EXPECT_TRUE(success);
}

// Tests that the last committed URL is preserved on an RFH even after the RFH
// goes into the pending deletion state.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       LastCommittedURLRetainedAfterUnload) {
  // Navigate to a.com.
  GURL start_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));
  RenderFrameHostImpl* rfh = web_contents()->GetMainFrame();
  EXPECT_EQ(start_url, rfh->GetLastCommittedURL());

  // Disable the unload ACK and the unload timer.
  auto filter = base::MakeRefCounted<DropMessageFilter>(
      FrameMsgStart, FrameHostMsg_Unload_ACK::ID);
  rfh->GetProcess()->AddFilter(filter.get());
  rfh->DisableUnloadTimerForTesting();

  // Open a popup on a.com to keep the process alive.
  OpenPopup(shell(), embedded_test_server()->GetURL("a.com", "/title2.html"),
            "foo");

  // Navigate cross-process to b.com.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title3.html")));

  // The old RFH should be pending deletion.
  EXPECT_TRUE(rfh->IsPendingDeletion());
  EXPECT_FALSE(rfh->IsCurrent());
  EXPECT_NE(rfh, web_contents()->GetMainFrame());

  // Check that it still has a valid last committed URL.
  EXPECT_EQ(start_url, rfh->GetLastCommittedURL());
}

// Tests that when a large OOPIF has been scaled, the compositor raster area
// sent from the embedder is correct.
#if defined(OS_ANDROID) || defined(OS_MAC)
// Temporarily disabled on Android because this doesn't account for browser
// control height or page scale factor.
// Flaky on Mac. https://crbug.com/840314
#define MAYBE_ScaledIframeRasterSize DISABLED_ScaledframeRasterSize
#else
#define MAYBE_ScaledIframeRasterSize ScaledIframeRasterSize
#endif
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       MAYBE_ScaledIframeRasterSize) {
  GURL http_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_scaled_large_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), http_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  scoped_refptr<UpdateViewportIntersectionMessageFilter> filter =
      new UpdateViewportIntersectionMessageFilter();
  root->current_frame_host()->GetProcess()->AddFilter(filter.get());

  // Force a lifecycle update and wait for it to finish; by the time this call
  // returns, the viewport intersection IPC should already have been received
  // by the browser process and handled by the filter.
  EvalJsResult eval_result = EvalJsAfterLifecycleUpdate(
      root->current_frame_host(),
      "document.getElementsByTagName('div')[0].scrollTo(0, 5000);",
      "document.getElementsByTagName('div')[0].getBoundingClientRect().top;");
  ASSERT_TRUE(eval_result.error.empty());
  int div_offset_top = eval_result.ExtractInt();
  gfx::Rect compositing_rect =
      filter->GetIntersectionState().compositor_visible_rect;

  float device_scale_factor = 1.0f;
  if (IsUseZoomForDSFEnabled())
    device_scale_factor = GetFrameDeviceScaleFactor(shell()->web_contents());

  // The math below replicates the calculations in
  // RemoteFrameView::GetCompositingRect(). That could be subject to tweaking,
  // which would have to be reflected in these test expectations. Also, any
  // changes to Blink that would affect the size of the frame rect or the
  // visibile viewport would need to be accounted for.
  // The multiplication by 5 accounts for the 0.2 scale factor in the test,
  // which increases the area that has to be drawn in the OOPIF.
  int view_height = root->current_frame_host()
                        ->GetRenderWidgetHost()
                        ->GetView()
                        ->GetViewBounds()
                        .height() *
                    5 * device_scale_factor;

  // The raster size is expanded by a factor of 1.3 to allow for some scrolling
  // without requiring re-raster. The expanded area to be rasterized should be
  // centered around the iframe's visible area within the parent document, hence
  // the expansion in each direction (top, bottom, left, right) is
  // (0.15 * viewport dimension).
  int expansion = ceilf(view_height * 0.15f);
  int expected_height = view_height + expansion * 2;

  // 5000 = div scroll offset in scaled pixels
  // 5 = scale factor from top-level document to iframe contents
  // 2 = iframe border in scaled pixels
  int expected_offset =
      ((5000 - (div_offset_top * 5) - 2) * device_scale_factor) - expansion;

  // Allow a small amount for rounding differences from applying page and
  // device scale factors at different times.
  float tolerance = ceilf(device_scale_factor);
  EXPECT_NEAR(compositing_rect.height(), expected_height, tolerance);
  EXPECT_NEAR(compositing_rect.y(), expected_offset, tolerance);
}

// Similar to ScaledIFrameRasterSize but with nested OOPIFs to ensure
// propagation works correctly.
#if defined(OS_ANDROID)
// Temporarily disabled on Android because this doesn't account for browser
// control height or page scale factor.
#define MAYBE_ScaledNestedIframeRasterSize DISABLED_ScaledNestedIframeRasterSize
#else
#define MAYBE_ScaledNestedIframeRasterSize ScaledNestedIframeRasterSize
#endif
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       MAYBE_ScaledNestedIframeRasterSize) {
  GURL http_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_scaled_large_frames_nested.html"));
  EXPECT_TRUE(NavigateToURL(shell(), http_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  FrameTreeNode* child = root->child_at(0);

  NavigateFrameToURL(
      child,
      embedded_test_server()->GetURL(
          "bar.com", "/frame_tree/page_with_large_scrollable_frame.html"));

  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   +--Site B ------- proxies for A C\n"
      "        +--Site C -- proxies for A B\n"
      "Where A = http://a.com/\n"
      "      B = http://bar.com/\n"
      "      C = http://baz.com/",
      DepictFrameTree(root));

  // This adds the filter to the immediate child iframe. It verifies that the
  // child sets the nested iframe's compositing rect correctly.
  scoped_refptr<UpdateViewportIntersectionMessageFilter> filter =
      new UpdateViewportIntersectionMessageFilter();
  child->current_frame_host()->GetProcess()->AddFilter(filter.get());

  // Scroll the child frame so that it is partially clipped. This will cause the
  // top 10 pixels of the child frame to be clipped. Applying the scale factor
  // means that in the coordinate system of the subframes, 50px are clipped.
  ASSERT_TRUE(EvalJsAfterLifecycleUpdate(root->current_frame_host(),
                                         "window.scrollBy(0, 10)", "")
                  .error.empty());

  // This scrolls the div containing in the 'Site B' iframe that contains the
  // 'Site C' iframe, and then we verify that the 'Site C' frame receives the
  // correct compositor frame. Force a lifecycle update after the scroll and
  // wait for it to finish; by the time this call returns, the viewport
  // intersection IPC should already have been received by the browser process
  // and handled by the filter. Extract the page offset of the leaf iframe
  // within the middle document.
  EvalJsResult child_eval_result = EvalJsAfterLifecycleUpdate(
      child->current_frame_host(),
      "document.getElementsByTagName('div')[0].scrollTo(0, 5000);",
      "document.getElementsByTagName('div')[0].getBoundingClientRect().top;");
  ASSERT_TRUE(child_eval_result.error.empty());
  int child_div_offset_top = child_eval_result.ExtractInt();

  gfx::Rect compositing_rect =
      filter->GetIntersectionState().compositor_visible_rect;

  float scale_factor = 1.0f;
  if (IsUseZoomForDSFEnabled())
    scale_factor = GetFrameDeviceScaleFactor(shell()->web_contents());

  // See comment in ScaledIframeRasterSize for explanation of this. In this
  // case, the raster area of the large iframe should be restricted to
  // approximately the area of its containing frame which is unclipped by the
  // main frame. The containing frame is clipped by 50 pixels at the top, due
  // to the scroll offset of the main frame, so we subtract that from the full
  // height of the containing frame.
  int view_height = (child->current_frame_host()
                         ->GetRenderWidgetHost()
                         ->GetView()
                         ->GetViewBounds()
                         .height() -
                     50) *
                    scale_factor;
  // 30% padding is added to the view_height to prevent frequent re-rasters.
  // The extra padding is centered around the view height, hence expansion by
  // 0.15 in each direction.
  int expansion = ceilf(view_height * 0.15f);
  int expected_height = view_height + expansion * 2;

  // Explanation of terms:
  //   5000 = offset from top of nested iframe to top of containing div, due to
  //          scroll offset of div
  //   child_div_offset_top = offset of containing div from top of child frame
  //   50 = offset of child frame's intersection with the top document viewport
  //       from the top of the child frame (i.e, clipped amount at top of child)
  //   view_height * 0.15 = padding added to the top of the compositing rect
  //                        (half the the 30% total padding)
  int expected_offset =
      5000 - ((child_div_offset_top - 50) * scale_factor) - expansion;

  // Allow a small amount for rounding differences from applying page and
  // device scale factors at different times.
  EXPECT_NEAR(compositing_rect.height(), expected_height, ceilf(scale_factor));
  EXPECT_NEAR(compositing_rect.y(), expected_offset, ceilf(scale_factor));
}

// Tests that when an OOPIF is inside a multicolumn container, its compositing
// rect is set correctly.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       IframeInMulticolCompositingRect) {
  GURL http_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_iframe_in_multicol.html"));
  EXPECT_TRUE(NavigateToURL(shell(), http_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  scoped_refptr<UpdateViewportIntersectionMessageFilter> filter =
      new UpdateViewportIntersectionMessageFilter();
  root->current_frame_host()->GetProcess()->AddFilter(filter.get());

  // Force a lifecycle update and wait for it to finish. Changing the width of
  // the iframe should cause the parent renderer to propagate a new
  // ViewportIntersectionState while running the rendering pipeline. By the time
  // this call returns, the viewport intersection IPC should already have been
  // received by the browser process and handled by the filter.
  EvalJsResult eval_result = EvalJsAfterLifecycleUpdate(
      root->current_frame_host(),
      "document.querySelector('iframe').style.width = '250px'", "");
  ASSERT_TRUE(filter->MessageReceived());
  gfx::Rect compositing_rect =
      filter->GetIntersectionState().compositor_visible_rect;

  float scale_factor = 1.0f;
  if (IsUseZoomForDSFEnabled())
    scale_factor = GetFrameDeviceScaleFactor(shell()->web_contents());

  gfx::Point visible_offset(0, 0);
  gfx::Size visible_size =
      gfx::ScaleToFlooredSize(gfx::Size(250, 150), scale_factor, scale_factor);
  gfx::Rect visible_rect(visible_offset, visible_size);
  float tolerance = ceilf(scale_factor);
  EXPECT_NEAR(compositing_rect.x(), visible_rect.x(), tolerance);
  EXPECT_NEAR(compositing_rect.y(), visible_rect.y(), tolerance);
  EXPECT_NEAR(compositing_rect.width(), visible_rect.width(), tolerance);
  EXPECT_NEAR(compositing_rect.height(), visible_rect.height(), tolerance);
  EXPECT_TRUE(compositing_rect.Contains(visible_rect));
}

// Verify that OOPIF select element popup menu coordinates account for scroll
// offset in containers embedding frame.
// TODO(crbug.com/859552): Reenable this.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       DISABLED_PopupMenuInTallIframeTest) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_tall_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child_node = root->child_at(0);
  GURL site_url(embedded_test_server()->GetURL(
      "baz.com", "/site_isolation/page-with-select.html"));
  NavigateFrameToURL(child_node, site_url);

  scoped_refptr<UpdateViewportIntersectionMessageFilter> filter =
      new UpdateViewportIntersectionMessageFilter();
  root->current_frame_host()->GetProcess()->AddFilter(filter.get());

  // Position the select element so that it is out of the viewport, then scroll
  // it into view.
  EXPECT_TRUE(ExecuteScript(
      child_node, "document.querySelector('select').style.top='2000px';"));
  EXPECT_TRUE(ExecuteScript(root, "window.scrollTo(0, 1900);"));

  // Wait for a viewport intersection update to be dispatched to the child, and
  // ensure it is processed by the browser before continuing.
  filter->Wait();
  {
    // This yields the UI thread in order to ensure that the new viewport
    // intersection is sent to the to child renderer before the mouse click
    // below.
    base::RunLoop loop;
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  loop.QuitClosure());
    loop.Run();
  }

  scoped_refptr<ShowWidgetMessageFilter> show_widget_filter =
      base::MakeRefCounted<ShowWidgetMessageFilter>(web_contents());
  base::ScopedClosureRunner shutdown_show_widget_filter(
      base::BindOnce(&ShowWidgetMessageFilter::Shutdown, show_widget_filter));
  child_node->current_frame_host()->GetProcess()->AddFilter(
      show_widget_filter.get());

  SimulateMouseClick(child_node->current_frame_host()->GetRenderWidgetHost(),
                     55, 2005);

  // Dismiss the popup.
  SimulateMouseClick(child_node->current_frame_host()->GetRenderWidgetHost(), 1,
                     1);

  // The test passes if this wait returns, indicating that the popup was
  // scrolled into view and the OOPIF renderer displayed it. Other tests verify
  // the correctness of popup menu coordinates.
  show_widget_filter->Wait();
}

#if defined(OS_ANDROID)

// This test ensures that gestures from child frames notify the gesture manager
// which exists only on the root frame. i.e. the gesture manager knows we're in
// a scroll gesture when it's happening in a cross-process child frame. This is
// important in cases like hiding the text selection popup during a scroll.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       GestureManagerListensToChildFrames) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);
  GURL b_url(embedded_test_server()->GetURL("b.com", "/scrollable_page.html"));
  NavigateFrameToURL(child, b_url);

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
#endif  // if defined(OS_ANDROID)

// Test to verify that viewport intersection is propagated to nested OOPIFs
// even when a parent OOPIF has been throttled.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       NestedFrameViewportIntersectionUpdated) {
  GURL main_url(embedded_test_server()->GetURL(
      "foo.com", "/frame_tree/scrollable_page_with_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child_node = root->child_at(0);
  GURL site_url(embedded_test_server()->GetURL(
      "bar.com", "/frame_tree/page_with_positioned_frame.html"));
  NavigateFrameToURL(child_node, site_url);

  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   +--Site B ------- proxies for A C\n"
      "        +--Site C -- proxies for A B\n"
      "Where A = http://foo.com/\n"
      "      B = http://bar.com/\n"
      "      C = http://baz.com/",
      DepictFrameTree(root));

  // This will intercept messages sent from B to C, describing C's viewport
  // intersection.
  scoped_refptr<UpdateViewportIntersectionMessageFilter> filter =
      new UpdateViewportIntersectionMessageFilter();
  child_node->current_frame_host()->GetProcess()->AddFilter(filter.get());

  // Run requestAnimationFrame in A and B to make sure initial layout has
  // completed and initial IPCs sent.
  ASSERT_TRUE(EvalJsAfterLifecycleUpdate(root->current_frame_host(), "", "")
                  .error.empty());
  ASSERT_TRUE(
      EvalJsAfterLifecycleUpdate(child_node->current_frame_host(), "", "")
          .error.empty());
  filter->Clear();

  // Scroll the child frame out of view, causing it to become throttled.
  ASSERT_TRUE(ExecJs(root->current_frame_host(), "window.scrollTo(0, 5000)"));
  filter->Wait();
  EXPECT_TRUE(filter->GetIntersectionState().viewport_intersection.IsEmpty());

  // Scroll the frame back into view.
  ASSERT_TRUE(ExecJs(root->current_frame_host(), "window.scrollTo(0, 0)"));
  filter->Wait();
  EXPECT_FALSE(filter->GetIntersectionState().viewport_intersection.IsEmpty());
}

// Test to verify that the main frame document intersection
// is propagated to out of process iframes by scrolling a nested iframe
// in and out of intersecting with the main frame document.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       NestedFrameMainFrameDocumentIntersectionUpdated) {
  GURL main_url(embedded_test_server()->GetURL(
      "foo.com", "/frame_tree/scrollable_page_with_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child_node = root->child_at(0);
  GURL site_url(embedded_test_server()->GetURL(
      "bar.com", "/frame_tree/scrollable_page_with_positioned_frame.html"));
  NavigateFrameToURL(child_node, site_url);

  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   +--Site B ------- proxies for A C\n"
      "        +--Site C -- proxies for A B\n"
      "Where A = http://foo.com/\n"
      "      B = http://bar.com/\n"
      "      C = http://baz.com/",
      DepictFrameTree(root));

  scoped_refptr<UpdateViewportIntersectionMessageFilter> filter =
      new UpdateViewportIntersectionMessageFilter();
  child_node->current_frame_host()->GetProcess()->AddFilter(filter.get());

  // Run requestAnimationFrame in A and B to make sure initial layout has
  // completed and initial IPC's sent.
  ASSERT_TRUE(EvalJsAfterLifecycleUpdate(root->current_frame_host(), "", "")
                  .error.empty());
  ASSERT_TRUE(
      EvalJsAfterLifecycleUpdate(child_node->current_frame_host(), "", "")
          .error.empty());
  filter->Clear();

  // Scroll the child frame out of view, causing it to become throttled.
  ASSERT_TRUE(
      ExecJs(child_node->current_frame_host(), "window.scrollTo(0, 5000)"));
  filter->Wait();
  EXPECT_TRUE(filter->GetIntersectionState().main_frame_intersection.IsEmpty());

  // Scroll the frame back into view.
  ASSERT_TRUE(
      ExecJs(child_node->current_frame_host(), "window.scrollTo(0, 0)"));
  filter->Wait();
  EXPECT_FALSE(
      filter->GetIntersectionState().main_frame_intersection.IsEmpty());
}

namespace {

// Helper class to intercept DidCommitProvisionalLoad messages and inject a
// call to close the current tab right before them.
class ClosePageBeforeCommitHelper : public DidCommitNavigationInterceptor {
 public:
  explicit ClosePageBeforeCommitHelper(WebContents* web_contents)
      : DidCommitNavigationInterceptor(web_contents) {}

  void Wait() {
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
    run_loop_.reset();
  }

 private:
  // DidCommitNavigationInterceptor:
  bool WillProcessDidCommitNavigation(
      RenderFrameHost* render_frame_host,
      NavigationRequest* navigation_request,
      ::FrameHostMsg_DidCommitProvisionalLoad_Params* params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr* interface_params)
      override {
    RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
        render_frame_host->GetRenderViewHost());
    EXPECT_TRUE(rvh->is_active());
    rvh->ClosePage();
    if (run_loop_)
      run_loop_->Quit();
    return true;
  }

  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(ClosePageBeforeCommitHelper);
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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Open a popup in a.com to keep that process alive.
  GURL same_site_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  Shell* new_shell = OpenPopup(root, same_site_url, "");

  // Add a blank grandchild frame.
  RenderFrameHostCreatedObserver frame_observer(shell()->web_contents(), 1);
  EXPECT_TRUE(ExecuteScript(
      root->child_at(0),
      "document.body.appendChild(document.createElement('iframe'));"));
  frame_observer.Wait();
  FrameTreeNode* grandchild = root->child_at(0)->child_at(0);

  // Navigate grandchild to an a.com URL.  Note that only a frame's initial
  // navigation forwards resource timing info to parent, so it's important that
  // this iframe was initially blank.
  //
  // Just before this URL commits, close the page.
  ClosePageBeforeCommitHelper close_page_helper(web_contents());
  EXPECT_TRUE(
      ExecuteScript(grandchild, JsReplace("location = $1", same_site_url)));
  close_page_helper.Wait();

  // Test passes if the a.com renderer doesn't crash. Ping to verify.
  bool success;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      new_shell, "window.domAutomationController.send(true);", &success));
  EXPECT_TRUE(success);
}

class SitePerProcessBrowserTouchActionTest : public SitePerProcessBrowserTest {
 public:
  SitePerProcessBrowserTouchActionTest() = default;

  bool GetTouchActionForceEnableZoom(RenderWidgetHost* rwh) {
    InputRouterImpl* input_router = static_cast<InputRouterImpl*>(
        static_cast<RenderWidgetHostImpl*>(rwh)->input_router());
    return input_router->touch_action_filter_.force_enable_zoom_;
  }

  // Computes the effective and allowed touch action for |rwhv_child| by
  // dispatching a touch to it through |rwhv_root|. |rwhv_root| is the root
  // frame containing |rwhv_child|. |rwhv_child| is the child (or indirect
  // descendent) of |rwhv_root| to get the touch action of. |event_position|
  // should be within |rwhv_child| in |rwhv_root|'s coordinate space.
  void GetTouchActionsForChild(
      RenderWidgetHostInputEventRouter* router,
      RenderWidgetHostViewBase* rwhv_root,
      RenderWidgetHostViewBase* rwhv_child,
      const gfx::Point& event_position,
      base::Optional<cc::TouchAction>& effective_touch_action,
      base::Optional<cc::TouchAction>& allowed_touch_action) {
    InputEventAckWaiter ack_observer(
        rwhv_child->GetRenderWidgetHost(),
        base::BindRepeating([](blink::mojom::InputEventResultSource source,
                               blink::mojom::InputEventResultState state,
                               const blink::WebInputEvent& event) {
          return event.GetType() == blink::WebGestureEvent::Type::kTouchStart ||
                 event.GetType() == blink::WebGestureEvent::Type::kTouchMove ||
                 event.GetType() == blink::WebGestureEvent::Type::kTouchEnd;
        }));

    InputRouterImpl* input_router = static_cast<InputRouterImpl*>(
        static_cast<RenderWidgetHostImpl*>(rwhv_child->GetRenderWidgetHost())
            ->input_router());
    // Clear the touch actions that were set by previous touches.
    input_router->touch_action_filter_.allowed_touch_action_.reset();
    // Send a touch start event to child to get the TAF filled with child
    // frame's touch action.
    ack_observer.Reset();
    blink::SyntheticWebTouchEvent touch_event;
    int index = touch_event.PressPoint(event_position.x(), event_position.y());
    router->RouteTouchEvent(rwhv_root, &touch_event,
                            ui::LatencyInfo(ui::SourceEventType::TOUCH));
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
    router->RouteTouchEvent(rwhv_root, &touch_event,
                            ui::LatencyInfo(ui::SourceEventType::TOUCH));
    ack_observer.Wait();

    ack_observer.Reset();
    touch_event.ReleasePoint(index);
    router->RouteTouchEvent(rwhv_root, &touch_event,
                            ui::LatencyInfo(ui::SourceEventType::TOUCH));
    ack_observer.Wait();
  }

  void GiveItSomeTime(const base::TimeDelta& t) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
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

#if defined(OS_ANDROID)
// Class to set |force_enable_zoom| to true in WebkitPrefs.
class EnableForceZoomContentClient : public TestContentBrowserClient {
 public:
  EnableForceZoomContentClient() = default;

  void OverrideWebkitPrefs(RenderViewHost* render_view_host,
                           blink::web_pref::WebPreferences* prefs) override {
    DCHECK(old_client_);
    old_client_->OverrideWebkitPrefs(render_view_host, prefs);
    prefs->force_enable_zoom = true;
  }

  void set_old_client(ContentBrowserClient* old_client) {
    old_client_ = old_client;
  }

 private:
  ContentBrowserClient* old_client_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(EnableForceZoomContentClient);
};

IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTouchActionTest,
                       ForceEnableZoomPropagatesToChild) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  ASSERT_EQ(1U, root->child_count());
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  FrameTreeNode* child = root->child_at(0);
  NavigateFrameToURL(child, b_url);
  WaitForHitTestData(child->current_frame_host());

  // Get access to child's TouchActionFilter.
  RenderWidgetHost* child_rwh =
      child->current_frame_host()->GetRenderWidgetHost();
  EXPECT_FALSE(GetTouchActionForceEnableZoom(child_rwh));

  EnableForceZoomContentClient new_client;
  ContentBrowserClient* old_client = SetBrowserClientForTesting(&new_client);
  new_client.set_old_client(old_client);

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
  EXPECT_TRUE(ExecuteScript(root, create_frame_script));
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  ASSERT_EQ(2U, root->child_count());

  FrameTreeNode* new_child = root->child_at(1);
  EXPECT_NE(root->current_frame_host()->GetRenderWidgetHost(),
            new_child->current_frame_host()->GetRenderWidgetHost());
  EXPECT_TRUE(GetTouchActionForceEnableZoom(
      new_child->current_frame_host()->GetRenderWidgetHost()));

  SetBrowserClientForTesting(old_client);
}
#endif  // defined(OS_ANDROID)

// Flaky on every platform, failing most of the time on Android.
// See https://crbug.com/945734
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTouchActionTest,
                       DISABLED_EffectiveTouchActionPropagatesAcrossFrames) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
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
  NavigateFrameToURL(child, b_url);

  // Force the renderer to generate a new frame.
  EXPECT_TRUE(
      ExecuteScript(shell(), "document.body.style.touchAction = 'none'"));
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

  RenderWidgetHostInputEventRouter* router =
      static_cast<WebContentsImpl*>(web_contents())->GetInputEventRouter();

  WaitForTouchActionUpdated(root_thread_observer.get(),
                            child_thread_observer.get());
  base::Optional<cc::TouchAction> effective_touch_action;
  base::Optional<cc::TouchAction> allowed_touch_action;
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

  EXPECT_TRUE(
      ExecuteScript(shell(), "document.body.style.touchAction = 'auto'"));
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

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* parent = root->child_at(0);
  GURL b_url(embedded_test_server()->GetURL(
      "b.com", "/frame_tree/page_with_iframe_in_div.html"));
  NavigateFrameToURL(parent, b_url);

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

  EXPECT_TRUE(
      ExecuteScript(shell(), "document.body.style.touchAction = 'none'"));

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

  RenderWidgetHostInputEventRouter* router =
      static_cast<WebContentsImpl*>(web_contents())->GetInputEventRouter();

  // Child should inherit effective touch action none from root.
  WaitForTouchActionUpdated(root_thread_observer.get(),
                            child_thread_observer.get());
  base::Optional<cc::TouchAction> effective_touch_action;
  base::Optional<cc::TouchAction> allowed_touch_action;
  cc::TouchAction expected_touch_action = cc::TouchAction::kPan;
  GetTouchActionsForChild(router, rwhv_root, rwhv_child, point_inside_child,
                          effective_touch_action, allowed_touch_action);
  cc::TouchAction effective_touch_action_result =
      effective_touch_action.has_value() ? effective_touch_action.value()
                                         : cc::TouchAction::kAuto;
  if (allowed_touch_action.has_value())
    EXPECT_EQ(expected_touch_action, allowed_touch_action.value());

  // Child should inherit effective touch action none from parent.
  EXPECT_TRUE(
      ExecuteScript(shell(), "document.body.style.touchAction = 'auto'"));
  EXPECT_TRUE(ExecuteScript(
      parent,
      "document.getElementById('parent-div').style.touchAction = 'none';"));
  WaitForTouchActionUpdated(root_thread_observer.get(),
                            child_thread_observer.get());
  GetTouchActionsForChild(router, rwhv_root, rwhv_child, point_inside_child,
                          effective_touch_action, allowed_touch_action);
  effective_touch_action_result = effective_touch_action.has_value()
                                      ? effective_touch_action.value()
                                      : cc::TouchAction::kAuto;
  if (allowed_touch_action.has_value())
    EXPECT_EQ(expected_touch_action, allowed_touch_action.value());

  // Child should inherit effective touch action auto from root and parent.
  EXPECT_TRUE(ExecuteScript(
      parent,
      "document.getElementById('parent-div').style.touchAction = 'auto'"));
  WaitForTouchActionUpdated(root_thread_observer.get(),
                            child_thread_observer.get());
  expected_touch_action = cc::TouchAction::kAuto;
  GetTouchActionsForChild(router, rwhv_root, rwhv_child, point_inside_child,
                          effective_touch_action, allowed_touch_action);
  effective_touch_action_result = effective_touch_action.has_value()
                                      ? effective_touch_action.value()
                                      : cc::TouchAction::kAuto;
  if (allowed_touch_action.has_value())
    EXPECT_EQ(expected_touch_action, allowed_touch_action.value());
}

IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTouchActionTest,
                       EffectiveTouchActionPropagatesWhenChildFrameNavigates) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  NavigateFrameToURL(child, b_url);

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

  EXPECT_TRUE(
      ExecuteScript(shell(), "document.body.style.touchAction = 'none'"));

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

  RenderWidgetHostInputEventRouter* router =
      static_cast<WebContentsImpl*>(web_contents())->GetInputEventRouter();
  // Child should inherit effective touch action none from root.
  WaitForTouchActionUpdated(root_thread_observer.get(),
                            child_thread_observer.get());
  base::Optional<cc::TouchAction> effective_touch_action;
  base::Optional<cc::TouchAction> allowed_touch_action;
  cc::TouchAction expected_touch_action = cc::TouchAction::kPan;
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
  NavigateFrameToURL(child, new_url);
  WaitForHitTestData(child->current_frame_host());
  // Navigation destroys the previous RenderWidgetHost, so we need to begin
  // observing the new renderer main thread associated with the child frame.
  child_thread_observer.reset(new MainThreadFrameObserver(
      child->current_frame_host()->GetRenderWidgetHost()));

  rwhv_child = static_cast<RenderWidgetHostViewBase*>(
      child->current_frame_host()->GetRenderWidgetHost()->GetView());

  WaitForTouchActionUpdated(root_thread_observer.get(),
                            child_thread_observer.get());
  GetTouchActionsForChild(router, rwhv_root, rwhv_child, point_inside_child,
                          effective_touch_action, allowed_touch_action);
  if (allowed_touch_action.has_value())
    EXPECT_EQ(expected_touch_action, allowed_touch_action.value());
}

// Failes on Win.  https://crbug.com/1097060
#if defined(OS_WIN)
#define MAYBE_ChildFrameCrashMetrics_KilledWhileVisible \
  DISABLED_ChildFrameCrashMetrics_KilledWhileVisible
#else
#define MAYBE_ChildFrameCrashMetrics_KilledWhileVisible \
  ChildFrameCrashMetrics_KilledWhileVisible
#endif
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       MAYBE_ChildFrameCrashMetrics_KilledWhileVisible) {
  // Set-up a frame tree that helps verify what the metrics tracks:
  // 1) frames (12 frames are affected if B process gets killed) or
  // 2) crashes (simply 1 crash if B process gets killed)?
  // 3) widgets (10 b widgets and 1 c widget are affected if B is killed,
  //    but a sad frame will appear only in 9 widgets - this excludes
  //    widgets for the b,c(b) part of the frame tree) or
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(b,c(b)),b,b,b,b,b,b,b,b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Kill the child frame.
  base::HistogramTester histograms;
  RenderProcessHost* child_process =
      root->child_at(0)->current_frame_host()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      child_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  child_process->Shutdown(0);
  crash_observer.Wait();

  // Verify that the expected metrics got logged.
  histograms.ExpectUniqueSample(
      "Stability.ChildFrameCrash.Visibility",
      CrossProcessFrameConnector::CrashVisibility::kCrashedWhileVisible, 9);

  // Hide and show the web contents and verify that no more metrics got logged.
  web_contents()->UpdateWebContentsVisibility(Visibility::HIDDEN);
  web_contents()->UpdateWebContentsVisibility(Visibility::VISIBLE);
  histograms.ExpectUniqueSample(
      "Stability.ChildFrameCrash.Visibility",
      CrossProcessFrameConnector::CrashVisibility::kCrashedWhileVisible, 9);
}

IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       ChildFrameCrashMetrics_KilledMainFrame) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a(b(b,c)))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

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

// Test is flaky (crbug.com/1097060)
IN_PROC_BROWSER_TEST_P(
    SitePerProcessBrowserTest,
    DISABLED_ChildFrameCrashMetrics_KilledWhileHiddenThenShown) {
  // Set-up a frame tree that helps verify what the metrics tracks:
  // 1) frames (12 frames are affected if B process gets killed) or
  // 2) widgets (10 b widgets and 1 c widget are affected if B is killed) or
  // 3) crashes (1 crash if B process gets killed)?
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(b,c),b,b,b,b,b,b,b,b,b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

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

  // Verify that no child frame metrics got logged (yet - while WebContents are
  // hidden).
  histograms.ExpectTotalCount("Stability.ChildFrameCrash.Visibility", 0);
  histograms.ExpectTotalCount(
      "Stability.ChildFrameCrash.ShownAfterCrashingReason", 0);

  // Show the web contents and verify that the expected metrics got logged.
  web_contents()->UpdateWebContentsVisibility(Visibility::VISIBLE);
  histograms.ExpectUniqueSample(
      "Stability.ChildFrameCrash.Visibility",
      CrossProcessFrameConnector::CrashVisibility::kShownAfterCrashing, 10);
  histograms.ExpectUniqueSample(
      "Stability.ChildFrameCrash.ShownAfterCrashingReason",
      CrossProcessFrameConnector::ShownAfterCrashingReason::kTabWasShown, 10);

  // Hide and show the web contents again and verify that no more metrics got
  // logged.
  web_contents()->UpdateWebContentsVisibility(Visibility::HIDDEN);
  web_contents()->UpdateWebContentsVisibility(Visibility::VISIBLE);
  histograms.ExpectUniqueSample(
      "Stability.ChildFrameCrash.Visibility",
      CrossProcessFrameConnector::CrashVisibility::kShownAfterCrashing, 10);
  histograms.ExpectUniqueSample(
      "Stability.ChildFrameCrash.ShownAfterCrashingReason",
      CrossProcessFrameConnector::ShownAfterCrashingReason::kTabWasShown, 10);
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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

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
  histograms.ExpectUniqueSample(
      "Stability.ChildFrameCrash.Visibility",
      CrossProcessFrameConnector::CrashVisibility::kNeverVisibleAfterCrash, 10);
}

IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       ChildFrameCrashMetrics_ScrolledIntoView) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

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
  EXPECT_TRUE(ExecuteScript(root, filling_script));
  MainThreadFrameObserver main_widget_observer(
      root->current_frame_host()->GetRenderWidgetHost());
  main_widget_observer.Wait();

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
  EXPECT_TRUE(ExecuteScript(root, scrolling_script));
  main_widget_observer.Wait();

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

class SitePerProcessBrowserTestWithoutSadFrameTabReload
    : public SitePerProcessBrowserTest {
 public:
  SitePerProcessBrowserTestWithoutSadFrameTabReload() {
    // Disable the feature to mark hidden tabs with sad frames for reload, since
    // it makes the scenario for which this test collects metrics impossible.
    feature_list_.InitAndDisableFeature(
        features::kReloadHiddenTabsWithCrashedSubframes);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Flaky everywhere: https://crbug.com/1115096
IN_PROC_BROWSER_TEST_P(
    SitePerProcessBrowserTestWithoutSadFrameTabReload,
    DISABLED_ChildFrameCrashMetrics_ScrolledIntoViewAfterTabIsShown) {
  // Start on a page that has a single iframe, which is positioned out of
  // view, and navigate that iframe cross-site.
  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/iframe_out_of_view.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  NavigateFrameToURL(root->child_at(0),
                     embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Hide the web contents (UpdateWebContentsVisibility is called twice to avoid
  // hitting the |!did_first_set_visible_| case).
  web_contents()->UpdateWebContentsVisibility(Visibility::VISIBLE);
  web_contents()->UpdateWebContentsVisibility(Visibility::HIDDEN);

  // Kill the child frame.
  base::HistogramTester histograms;
  RenderProcessHost* child_process =
      root->child_at(0)->current_frame_host()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      child_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  child_process->Shutdown(0);
  crash_observer.Wait();

  // Verify that no child frame crash metrics got logged yet.
  histograms.ExpectTotalCount("Stability.ChildFrameCrash.Visibility", 0);
  histograms.ExpectTotalCount(
      "Stability.ChildFrameCrash.ShownAfterCrashingReason", 0);

  // Show the web contents.  The crash metrics still shouldn't be logged, since
  // the crashed frame is out of view.
  web_contents()->UpdateWebContentsVisibility(Visibility::VISIBLE);
  histograms.ExpectTotalCount("Stability.ChildFrameCrash.Visibility", 0);
  histograms.ExpectTotalCount(
      "Stability.ChildFrameCrash.ShownAfterCrashingReason", 0);

  // Scroll the subframe into view and wait until the scrolled frame draws
  // itself.
  MainThreadFrameObserver main_widget_observer(
      root->current_frame_host()->GetRenderWidgetHost());
  std::string scrolling_script = R"(
    var frame = document.body.querySelector("iframe");
    frame.scrollIntoView();
  )";
  EXPECT_TRUE(ExecuteScript(root, scrolling_script));
  main_widget_observer.Wait();

  // Verify that the expected metrics got logged.
  histograms.ExpectUniqueSample(
      "Stability.ChildFrameCrash.Visibility",
      CrossProcessFrameConnector::CrashVisibility::kShownAfterCrashing, 1);
  histograms.ExpectUniqueSample(
      "Stability.ChildFrameCrash.ShownAfterCrashingReason",
      CrossProcessFrameConnector::ShownAfterCrashingReason::
          kViewportIntersectionAfterTabWasShown,
      1);

  // Hide and show the web contents again and verify that no more metrics got
  // logged.
  web_contents()->UpdateWebContentsVisibility(Visibility::HIDDEN);
  web_contents()->UpdateWebContentsVisibility(Visibility::VISIBLE);
  histograms.ExpectUniqueSample(
      "Stability.ChildFrameCrash.Visibility",
      CrossProcessFrameConnector::CrashVisibility::kShownAfterCrashing, 1);
  histograms.ExpectUniqueSample(
      "Stability.ChildFrameCrash.ShownAfterCrashingReason",
      CrossProcessFrameConnector::ShownAfterCrashingReason::
          kViewportIntersectionAfterTabWasShown,
      1);
}

class SitePerProcessBrowserTestWithSadFrameTabReload
    : public SitePerProcessBrowserTest {
 public:
  SitePerProcessBrowserTestWithSadFrameTabReload() {
    // Disable the feature to mark hidden tabs with sad frames for reload, since
    // it makes the scenario for which this test collects metrics impossible.
    feature_list_.InitAndEnableFeature(
        features::kReloadHiddenTabsWithCrashedSubframes);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Verify the feature where hidden tabs with crashed subframes are marked for
// reload. This avoids showing crashed subframes if a hidden tab is eventually
// shown. See https://crbug.com/841572.
// crbug.com/1010119, fails on Win. crbug.com/1015971, fails on Linux.
// crbug.com/1049885, fails on Android and Mac.
#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_CHROMEOS) || \
    defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_ReloadHiddenTabWithCrashedSubframe \
  DISABLED_ReloadHiddenTabWithCrashedSubframe
#else
#define MAYBE_ReloadHiddenTabWithCrashedSubframe \
  ReloadHiddenTabWithCrashedSubframe
#endif
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTestWithSadFrameTabReload,
                       MAYBE_ReloadHiddenTabWithCrashedSubframe) {
  auto crash_process = [](FrameTreeNode* ftn) {
    RenderProcessHost* process = ftn->current_frame_host()->GetProcess();
    RenderProcessHostWatcher crash_observer(
        process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    process->Shutdown(0);
    crash_observer.Wait();
    EXPECT_FALSE(ftn->current_frame_host()->IsRenderFrameLive());
  };

  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Hide the WebContents (UpdateWebContentsVisibility is called twice to avoid
  // hitting the |!did_first_set_visible_| case).
  web_contents()->UpdateWebContentsVisibility(Visibility::VISIBLE);
  web_contents()->UpdateWebContentsVisibility(Visibility::HIDDEN);
  EXPECT_EQ(Visibility::HIDDEN, web_contents()->GetVisibility());

  // Kill the b.com subframe's process.  This should mark the hidden
  // WebContents for reload.
  {
    SCOPED_TRACE("In-viewport sad frame on a hidden tab");
    base::HistogramTester histograms;
    crash_process(root->child_at(0));
    histograms.ExpectUniqueSample(
        "Stability.ChildFrameCrash.TabMarkedForReload", true, 1);
    histograms.ExpectUniqueSample(
        "Stability.ChildFrameCrash.TabMarkedForReload.Visibility",
        blink::mojom::FrameVisibility::kRenderedInViewport, 1);

    // Show the WebContents.  This should trigger a reload of the main frame.
    web_contents()->UpdateWebContentsVisibility(Visibility::VISIBLE);
    EXPECT_TRUE(WaitForLoadStop(web_contents()));
    histograms.ExpectUniqueSample(
        "Navigation.LoadIfNecessaryType",
        NavigationControllerImpl::NeedsReloadType::kCrashedSubframe, 1);
  }

  // Both frames should now have live renderer processes.
  EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());
  EXPECT_TRUE(root->child_at(0)->current_frame_host()->IsRenderFrameLive());

  // Next, try the same with a crashed subframe that's scrolled out of view.
  // This should also trigger a reload.
  GURL out_of_view_url(
      embedded_test_server()->GetURL("a.com", "/iframe_out_of_view.html"));
  EXPECT_TRUE(NavigateToURL(shell(), out_of_view_url));
  EXPECT_EQ("LOADED", EvalJsWithManualReply(shell(), "notifyWhenLoaded();"));
  NavigateIframeToURL(web_contents(), "test_iframe",
                      embedded_test_server()->GetURL("b.com", "/title1.html"));
  web_contents()->UpdateWebContentsVisibility(Visibility::HIDDEN);
  {
    SCOPED_TRACE("Out-of-viewport sad frame on a hidden tab");
    base::HistogramTester histograms;
    crash_process(root->child_at(0));
    histograms.ExpectUniqueSample(
        "Stability.ChildFrameCrash.TabMarkedForReload", true, 1);
    histograms.ExpectUniqueSample(
        "Stability.ChildFrameCrash.TabMarkedForReload.Visibility",
        blink::mojom::FrameVisibility::kRenderedOutOfViewport, 1);

    web_contents()->UpdateWebContentsVisibility(Visibility::VISIBLE);
    EXPECT_TRUE(WaitForLoadStop(web_contents()));
    histograms.ExpectUniqueSample(
        "Navigation.LoadIfNecessaryType",
        NavigationControllerImpl::NeedsReloadType::kCrashedSubframe, 1);
  }
  EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());
  EXPECT_TRUE(root->child_at(0)->current_frame_host()->IsRenderFrameLive());

  // Now, load a page with where an iframe is hidden with "display:none".
  // Ensure that we do not mark the tab for reload in that case.
  GURL hidden_iframe_url(
      embedded_test_server()->GetURL("a.com", "/page_with_hidden_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), hidden_iframe_url));
  NavigateIframeToURL(web_contents(), "test_iframe",
                      embedded_test_server()->GetURL("b.com", "/title1.html"));
  RenderFrameProxyHost* proxy_to_parent =
      root->child_at(0)->render_manager()->GetProxyToParent();
  EXPECT_TRUE(proxy_to_parent->cross_process_frame_connector()->IsHidden());

  web_contents()->UpdateWebContentsVisibility(Visibility::HIDDEN);
  {
    SCOPED_TRACE("display:none sad frame on a hidden tab");
    base::HistogramTester histograms;
    crash_process(root->child_at(0));
    histograms.ExpectUniqueSample(
        "Stability.ChildFrameCrash.TabMarkedForReload", false, 1);
  }
  web_contents()->UpdateWebContentsVisibility(Visibility::VISIBLE);
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());
  EXPECT_FALSE(root->child_at(0)->current_frame_host()->IsRenderFrameLive());

  // Finally, ensure that the reload policy doesn't trigger for a visible tab,
  // even if it becomes hidden and then visible again.
  EXPECT_EQ(Visibility::VISIBLE, web_contents()->GetVisibility());
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  {
    SCOPED_TRACE("Visible sad frame on a visible tab");
    base::HistogramTester histograms;
    crash_process(root->child_at(0));
    histograms.ExpectUniqueSample(
        "Stability.ChildFrameCrash.TabMarkedForReload", false, 1);
  }
  EXPECT_EQ(Visibility::VISIBLE, web_contents()->GetVisibility());
  web_contents()->UpdateWebContentsVisibility(Visibility::HIDDEN);
  web_contents()->UpdateWebContentsVisibility(Visibility::VISIBLE);
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());
  EXPECT_FALSE(root->child_at(0)->current_frame_host()->IsRenderFrameLive());
}

// Check that when a frame changes a subframe's size twice and then sends a
// postMessage to the subframe, the subframe's onmessage handler sees the new
// size.  In particular, ensure that the postMessage won't get reordered with
// the second resize, which might be throttled if the first resize is still in
// progress. See https://crbug.com/828529.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       ResizeAndCrossProcessPostMessagePreserveOrder) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Add an onmessage handler to the subframe to send back its width.
  EXPECT_TRUE(ExecuteScript(root->child_at(0), R"(
      window.addEventListener('message', function(event) {
        domAutomationController.send(document.body.clientWidth);
      });)"));

  // Drop the visual properties ACKs from the child renderer.  To do this,
  // unsubscribe the child's RenderWidgetHost from its
  // RenderFrameMetadataProvider, which ensures that
  // DidUpdateVisualProperties() won't be called on it, and the ACK won't be
  // reset.  This simulates that the ACK for the first resize below does not
  // arrive before the second resize IPC arrives from the
  // parent, and that the second resize IPC early-exits in
  // SynchronizeVisualProperties() due to the pending visual properties ACK.
  RenderWidgetHostImpl* rwh =
      root->child_at(0)->current_frame_host()->GetRenderWidgetHost();
  rwh->render_frame_metadata_provider_.RemoveObserver(rwh);

  // Now, resize the subframe twice from the main frame and send it a
  // postMessage. The postMessage handler should see the second updated size.
  int width = 0;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(root, R"(
      var f = document.querySelector('iframe');
      f.width = 500;
      f.offsetTop; // force layout; this sends a resize IPC for width of 500.
      f.width = 700;
      f.offsetTop; // force layout; this sends a resize IPC for width of 700.
      f.contentWindow.postMessage('foo', '*');)", &width));
  EXPECT_EQ(width, 700);
}

class SitePerProcessAndProcessPerSiteBrowserTest
    : public SitePerProcessBrowserTest {
 public:
  SitePerProcessAndProcessPerSiteBrowserTest() {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SitePerProcessBrowserTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kProcessPerSite);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SitePerProcessAndProcessPerSiteBrowserTest);
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
  EXPECT_TRUE(ExecuteScript(web_contents(), "document.cookie = 'foo=bar';"));
  std::string cookie;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      web_contents(), "window.domAutomationController.send(document.cookie);",
      &cookie));
  EXPECT_EQ("foo=bar", cookie);
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

  DISALLOW_COPY_AND_ASSIGN(SameDocumentCommitObserver);
};

}  // namespace

// Ensure that a same-document navigation does not cancel an ongoing
// cross-process navigation.  See https://crbug.com/825677.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       ReplaceStateDoesNotCancelCrossSiteNavigation) {
  GURL url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Give the page a beforeunload handler that does a replaceState.  Do this
  // from setTimeout so that the navigation that triggers beforeunload is
  // already started when the replaceState happens.
  EXPECT_TRUE(ExecuteScript(root,
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
  // having a user gesture, which will be the case here since ExecuteScript
  // runs with a user gesture.
  EXPECT_TRUE(ExecuteScript(root, JsReplace("location.href = $1", url2)));
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
  cross_site_navigation.WaitForNavigationFinished();
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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* subframe = root->child_at(0);

  // The subframe should not be sandboxed.
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            subframe->pending_frame_policy().sandbox_flags);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            subframe->effective_frame_policy().sandbox_flags);

  // Set the "sandbox" attribute on the subframe; pending policy should update.
  EXPECT_TRUE(ExecuteScript(
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
  EXPECT_TRUE(
      ExecuteScript(subframe, "history.replaceState({}, 'footitle', 'foo');"));
  replace_state_observer.Wait();

  EXPECT_EQ(expected_flags, subframe->pending_frame_policy().sandbox_flags);
  EXPECT_EQ(network::mojom::WebSandboxFlags::kNone,
            subframe->effective_frame_policy().sandbox_flags);

  // Also try a same-document navigation to a fragment, which also shouldn't
  // commit the pending sandbox flags.
  GURL fragment_url = GURL(subframe->current_url().spec() + "#foo");
  {
    SameDocumentCommitObserver fragment_observer(web_contents());
    EXPECT_TRUE(
        ExecuteScript(subframe, JsReplace("location.href=$1", fragment_url)));
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
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* subframe = root->child_at(0);

  // Create a blob URL in the subframe, and navigate to it.
  TestNavigationObserver observer(shell()->web_contents());
  std::string blob_script =
      "var blob = new Blob(['foo'], {type : 'text/html'});"
      "var url = URL.createObjectURL(blob);"
      "location = url;";
  EXPECT_TRUE(ExecuteScript(subframe, blob_script));
  observer.Wait();
  RenderFrameHostImpl* subframe_rfh = subframe->current_frame_host();
  EXPECT_TRUE(subframe_rfh->GetLastCommittedURL().SchemeIsBlob());

  // Open a cross-site popup and repeat these steps.
  GURL popup_url(embedded_test_server()->GetURL(
      "b.com", "/navigation_controller/page_with_data_iframe.html"));
  Shell* new_shell = OpenPopup(root, popup_url, "");
  FrameTreeNode* popup_root =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetFrameTree()
          ->root();
  FrameTreeNode* popup_subframe = popup_root->child_at(0);

  TestNavigationObserver popup_observer(new_shell->web_contents());
  EXPECT_TRUE(ExecuteScript(popup_subframe, blob_script));
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
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       RenderFrameProxyNotRecreatedDuringProcessShutdown) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  GURL popup_url(embedded_test_server()->GetURL(
      "b.com", "/title1.html"));
  Shell* new_shell = OpenPopup(root, popup_url, "foo");
  FrameTreeNode* popup_root =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetFrameTree()
          ->root();
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
  EXPECT_TRUE(ExecuteScript(shell(), JsReplace(R"(
      window.done = false;
      window.onmessage = () => {
        if (!window.done) {
          window.open($1, 'foo');
          window.done = true;
        }
      };)",
                                               hung_b_url)));

  // In the popup, install an unload handler to send a lot of postMessages to
  // the opener.  This keeps the MessageLoop in the b.com process busy after
  // navigating away from the current document.  In https://crbug.com/794625,
  // this was needed so that a subsequent IPC to recreate a proxy arrives
  // before the process fully shuts down.
  EXPECT_TRUE(ExecuteScript(new_shell, R"(
      window.onunload = () => {
        for (var i=0; i<10000; i++)
          opener.postMessage('hi','*');
      })"));

  // Navigate popup to a.com.  This unloads the last active frame in the b.com
  // process, and hence initiates process shutdown.
  TestFrameNavigationObserver commit_observer(popup_root);
  GURL another_a_url(embedded_test_server()->GetURL("a.com", "/title3.html"));
  EXPECT_TRUE(
      ExecuteScript(new_shell, JsReplace("location = $1", another_a_url)));
  commit_observer.WaitForCommit();

  // At this point, popup's original RFH is pending deletion.
  EXPECT_TRUE(rfh->IsPendingDeletion());

  // When the opener receives a postMessage from the popup's unload handler, it
  // should start a navigation back to b.com.  Wait for it.  This navigation
  // creates a speculative RFH which reuses the proxy that was created as part
  // of navigating from |popup_url| to |another_a_url|.
  EXPECT_TRUE(manager.WaitForRequestStart());

  // Cancel the started navigation (to /hung) in the popup and make sure the
  // b.com renderer process exits cleanly without a crash.  In
  // https://crbug.com/794625, the crash was caused by trying to recreate the
  // reused proxy, which had been incorrectly set as non-live.
  popup_root->ResetNavigationRequest(false);
  b_process_observer.Wait();
  EXPECT_TRUE(b_process_observer.did_exit_normally());
}

IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       CommitTimeoutForHungRenderer) {
  // Navigate first tab to a.com.
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), a_url));
  RenderProcessHost* a_process =
      shell()->web_contents()->GetMainFrame()->GetProcess();

  // Open b.com in a second tab.  Using a renderer-initiated navigation is
  // important to leave a.com and b.com SiteInstances in the same
  // BrowsingInstance (so the b.com -> a.com navigation in the next test step
  // will reuse the process associated with the first a.com tab).
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  Shell* new_shell = OpenPopup(shell()->web_contents(), b_url, "newtab");
  WebContents* new_contents = new_shell->web_contents();
  EXPECT_TRUE(WaitForLoadStop(new_contents));
  RenderProcessHost* b_process = new_contents->GetMainFrame()->GetProcess();
  EXPECT_NE(a_process, b_process);

  // Hang the first tab's renderer.
  const char* kHungScript = "setTimeout(function() { for (;;) {}; }, 0);";
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(), kHungScript));

  // Attempt to navigate the second tab to a.com.  This will attempt to reuse
  // the hung process.
  NavigationRequest::SetCommitTimeoutForTesting(
      base::TimeDelta::FromMilliseconds(100));
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
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       NoCommitTimeoutForInvisibleWebContents) {
  // Navigate first tab to a.com.
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), a_url));
  RenderProcessHost* a_process =
      shell()->web_contents()->GetMainFrame()->GetProcess();

  // Open b.com in a second tab.  Using a renderer-initiated navigation is
  // important to leave a.com and b.com SiteInstances in the same
  // BrowsingInstance (so the b.com -> a.com navigation in the next test step
  // will reuse the process associated with the first a.com tab).
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  Shell* new_shell = OpenPopup(shell()->web_contents(), b_url, "newtab");
  WebContents* new_contents = new_shell->web_contents();
  EXPECT_TRUE(WaitForLoadStop(new_contents));
  RenderProcessHost* b_process = new_contents->GetMainFrame()->GetProcess();
  EXPECT_NE(a_process, b_process);

  // Hang the first tab's renderer.
  const char* kHungScript = "setTimeout(function() { for (;;) {}; }, 0);";
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(), kHungScript));

  // Hide the second tab.  This should prevent reporting of hangs in this tab
  // (see https://crbug.com/881812).
  new_contents->WasHidden();
  EXPECT_EQ(Visibility::HIDDEN, new_contents->GetVisibility());

  // Attempt to navigate the second tab to a.com.  This will attempt to reuse
  // the hung process.
  base::TimeDelta kTimeout = base::TimeDelta::FromMilliseconds(100);
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
      web_contents()->GetFrameTree()->root()->child_at(0);
  WebContentsImpl* inner_contents =
      static_cast<WebContentsImpl*>(CreateAndAttachInnerContents(
          ToRenderFrameHost(child_frame).render_frame_host()));
  FrameTreeNode* inner_contents_root = inner_contents->GetFrameTree()->root();
  RenderFrameProxyHost* outer_proxy =
      inner_contents_root->render_manager()->GetProxyToOuterDelegate();
  CrossProcessFrameConnector* outer_connector =
      outer_proxy->cross_process_frame_connector();
  EXPECT_NE(nullptr, outer_connector->get_view_for_testing());

  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  NavigateFrameToURL(inner_contents_root, a_url);
  SiteInstance* a_site_instance =
      inner_contents->GetMainFrame()->GetSiteInstance();
  RenderProcessHost* a_process = a_site_instance->GetProcess();
  RenderWidgetHostViewChildFrame* a_view =
      outer_connector->get_view_for_testing();

  GURL b_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  NavigateFrameToURL(inner_contents_root, b_url);
  SiteInstance* b_site_instance =
      inner_contents->GetMainFrame()->GetSiteInstance();
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
      web_contents()->GetFrameTree()->root()->child_at(0);
  WebContentsImpl* inner_contents =
      static_cast<WebContentsImpl*>(CreateAndAttachInnerContents(
          ToRenderFrameHost(child_frame).render_frame_host()));
  FrameTreeNode* inner_contents_root = inner_contents->GetFrameTree()->root();

  // Navigate inner WebContents to b.com, and then navigate a subframe on that
  // page to c.com.
  GURL b_url(embedded_test_server()->GetURL(
      "b.com", "/cross_site_iframe_factory.html?b(b)"));
  NavigateFrameToURL(inner_contents_root, b_url);
  GURL c_url(embedded_test_server()->GetURL("c.com", "/title1.html"));
  FrameTreeNode* inner_child = inner_contents_root->child_at(0);
  NavigateFrameToURL(inner_child, c_url);

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

  FrameTreeNode* child = web_contents()->GetFrameTree()->root()->child_at(0);
  GURL original_frame_url(child->current_frame_host()->GetLastCommittedURL());
  EXPECT_EQ("b.com", original_frame_url.host());

  WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern("Not allowed to load local resource: file:*");

  GURL file_url("file:///");
  EXPECT_TRUE(
      ExecJs(web_contents(),
             JsReplace("document.querySelector('iframe').src = $1", file_url)));
  console_observer.Wait();

  // The iframe should've stayed at the original URL.
  EXPECT_EQ(original_frame_url,
            child->current_frame_host()->GetLastCommittedURL());
}

// Touchscreen DoubleTapZoom is only supported on Android & ChromeOS at present.
#if defined(OS_CHROMEOS) || defined(OS_ANDROID)
// A test ContentBrowserClient implementation which enforces
// WebPreferences' |double_tap_to_zoom_enabled| to be true.
class DoubleTapZoomContentBrowserClient : public TestContentBrowserClient {
 public:
  DoubleTapZoomContentBrowserClient() = default;

  void OverrideWebkitPrefs(
      content::RenderViewHost* rvh,
      blink::web_pref::WebPreferences* web_prefs) override {
    web_prefs->double_tap_to_zoom_enabled = true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DoubleTapZoomContentBrowserClient);
};

IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       TouchscreenAnimateDoubleTapZoomInOOPIF) {
  // Install a client forcing double-tap zoom to be enabled.
  DoubleTapZoomContentBrowserClient content_browser_client;
  ContentBrowserClient* old_client =
      SetBrowserClientForTesting(&content_browser_client);
  web_contents()->OnWebPreferencesChanged();

  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
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
  std::string actions_template = R"HTML(
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
      base::StringPrintf(actions_template.c_str(), tap_position.x(),
                         tap_position.y(), tap_position.x(), tap_position.y());
  base::JSONReader::ValueWithError parsed_json =
      base::JSONReader::ReadAndReturnValueWithError(double_tap_actions_json);
  ASSERT_TRUE(parsed_json.value) << parsed_json.error_message;
  ActionsParser actions_parser(std::move(*parsed_json.value));

  ASSERT_TRUE(actions_parser.ParsePointerActionSequence());
  auto synthetic_gesture_doubletap =
      SyntheticGesture::Create(actions_parser.gesture_params());

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

  SetBrowserClientForTesting(old_client);
}
#endif  // defined(OS_CHROMEOS) || defined(OS_ANDROID)

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
  std::string result;
  ASSERT_TRUE(ExecuteScriptAndExtractString(
      web_contents(), JsReplace("setUrl($1, true);", object_valid_url),
      &result));
  ASSERT_EQ("OBJECT_LOAD", result);

  // Verify fallback content is not shown.
  bool fallback_visible = true;
  ASSERT_TRUE(ExecuteScriptAndExtractBool(
      web_contents(), "fallbackVisible(true)", &fallback_visible));
  ASSERT_FALSE(fallback_visible);

  // Navigate the <object>'s frame to invalid origin. Make sure we do not report
  // the 'load' event (the 404 content loads inside the <object>'s frame and the
  // 'load' event might fire before fallback is detected).
  fallback_visible = false;
  ASSERT_TRUE(
      ExecuteScriptAndExtractBool(web_contents(),
                                  JsReplace("setUrl($1, false);"
                                            "notifyWhenFallbackShown();",
                                            object_invalid_url),
                                  &fallback_visible));
  ASSERT_TRUE(fallback_visible);
}

INSTANTIATE_TEST_SUITE_P(SitePerProcess,
                         CrossProcessNavigationObjectElementTest,
                         testing::Values(std::make_tuple("a", "a", "b"),
                                         std::make_tuple("a", "b", "b"),
                                         std::make_tuple("a", "b", "c")));

class ScrollingIntegrationTest : public SitePerProcessBrowserTest {
 public:
  ScrollingIntegrationTest() = default;
  ~ScrollingIntegrationTest() override = default;

  void DoScroll(const gfx::Point& point,
                const gfx::Vector2d& distance,
                SyntheticGestureParams::GestureSourceType source) {
    SyntheticSmoothScrollGestureParams params;
    params.gesture_source_type = source;
    params.anchor = gfx::PointF(point);
    params.distances.push_back(-distance);
    params.granularity = ui::ScrollGranularity::kScrollByPrecisePixel;

    auto gesture = std::make_unique<SyntheticSmoothScrollGesture>(params);

    // Runs until we get the SyntheticGestureCompleted callback
    base::RunLoop run_loop;
    GetRenderWidgetHostImpl()->QueueSyntheticGesture(
        std::move(gesture),
        base::BindLambdaForTesting([&](SyntheticGesture::Result result) {
          EXPECT_EQ(SyntheticGesture::GESTURE_FINISHED, result);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  double GetScrollTop() {
    FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                              ->GetFrameTree()
                              ->root();
    return EvalJs(root, "window.scrollY").ExtractDouble();
  }

  void WaitForVerticalScroll() {
    RenderFrameSubmissionObserver frame_observer(shell()->web_contents());
    gfx::Vector2dF default_scroll_offset;
    while (frame_observer.LastRenderFrameMetadata()
               .root_scroll_offset.value_or(default_scroll_offset)
               .y() <= 0) {
      frame_observer.WaitForMetadataChange();
    }
  }

  RenderWidgetHostImpl* GetRenderWidgetHostImpl() {
    FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                              ->GetFrameTree()
                              ->root();
    return root->current_frame_host()->GetRenderWidgetHost();
  }
};

// Tests basic scrolling after navigating to a new origin works. Guards against
// bugs like https://crbug.com/899234 which are caused by invalid
// initialization due to the cross-origin provisional frame swap.
IN_PROC_BROWSER_TEST_P(ScrollingIntegrationTest,
                       ScrollAfterCrossOriginNavigation) {
  // Navigate to the a.com domain first.
  GURL url_domain_a(
      embedded_test_server()->GetURL("a.com", "/simple_page.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_domain_a));

  // Now navigate to baz.com, this should cause a cross-origin navigation which
  // will load into a provisional frame and then swap in as a local main frame.
  // This test ensures all the correct initialization takes place in the
  // renderer so that a basic scrolling smoke test works.
  GURL url_domain_b(embedded_test_server()->GetURL(
      "baz.com", "/scrollable_page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_domain_b));
  ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));

  {
    // TODO(bokan): We currently don't have a good way to know when the
    // compositor's scrolling layers are ready after changes on the main thread.
    // We wait a timeout but that's really a hack. Fixing is tracked in
    // https://crbug.com/897520
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(),
        base::TimeDelta::FromMilliseconds(3000));
    run_loop.Run();
  }

  SyntheticGestureParams::GestureSourceType source;

// TODO(bokan): Mac doesn't support touch events and for an unknown reason,
// Android doesn't like mouse wheel here. https://crbug.com/897520.
#if defined(OS_ANDROID)
  source = SyntheticGestureParams::TOUCH_INPUT;
#else
  source = SyntheticGestureParams::TOUCHPAD_INPUT;
#endif

  // Perform the scroll (below the iframe), ensure it's correctly processed.
  DoScroll(gfx::Point(100, 110), gfx::Vector2d(0, 500), source);
  WaitForVerticalScroll();
  EXPECT_GT(GetScrollTop(), 0);
}

#if !defined(OS_ANDROID)
// This test verifies that after occluding a WebContents the RAF inside a
// cross-process child frame is throttled.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       OccludedRenderWidgetThrottlesRAF) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* subframe = root->child_at(0);
  GURL page_with_raf_counter =
      embedded_test_server()->GetURL("a.com", "/page_with_raf_counter.html");
  NavigateFrameToURL(subframe, page_with_raf_counter);

  // Initially page is visible - wait some time and then ensure a good number of
  // rafs have been generated. On Mac the number of RAFs that occur in 500ms is
  // quite low, see https://crbug.com/1098715.
  auto allow_time_for_rafs = []() {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(),
        base::TimeDelta::FromMilliseconds(1000));
    run_loop.Run();
  };

  ASSERT_TRUE(ExecuteScript(subframe, "reset_count();"));
  allow_time_for_rafs();
  int32_t default_raf_count = EvalJs(subframe, "raf_count").ExtractInt();
  // On a 60 fps we should expect more than 30 counts - however purely for
  // sanity checking and avoiding unnecessary flakes adding a comparison for a
  // much lower value. This verifies that we did get *some* rAFs.
  EXPECT_GT(default_raf_count, 5);
  web_contents()->WasOccluded();
  ASSERT_TRUE(ExecuteScript(subframe, "reset_count();"));
  allow_time_for_rafs();
  int32_t raf_count = EvalJs(subframe, "raf_count").ExtractInt();
  // If the frame is throttled, we should expect 0 rAFs.
  EXPECT_EQ(raf_count, 0);
  // Sanity-check: unoccluding will reverse the effect.
  web_contents()->WasShown();
  ASSERT_TRUE(ExecuteScript(subframe, "reset_count();"));
  allow_time_for_rafs();
  ASSERT_TRUE(ExecuteScriptAndExtractInt(
      subframe, "window.domAutomationController.send(raf_count)", &raf_count));
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
                            ->GetFrameTree()
                            ->root();

  // Setup an URL which will never commit, allowing this test to send its own,
  // malformed, commit message.
  GURL another_url(embedded_test_server()->GetURL("b.com", "/hung"));

  // Use LoadURL, as the test shouldn't wait for navigation commit.
  NavigationController& controller = shell()->web_contents()->GetController();
  controller.LoadURL(another_url, Referrer(), ui::PAGE_TRANSITION_LINK,
                     std::string());
  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_EQ(another_url, controller.GetPendingEntry()->GetURL());
  NavigationRequest* navigation_request = root->navigation_request();
  ASSERT_TRUE(navigation_request);

  RenderProcessHostBadIpcMessageWaiter kill_waiter(
      root->current_frame_host()->GetProcess());

  // Create commit params with the same URL as the start one, so URL checks
  // pass, but use a different origin than the origin lock of the process.
  std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params> params =
      std::make_unique<FrameHostMsg_DidCommitProvisionalLoad_Params>();
  params->nav_entry_id = 0;
  params->did_create_new_entry = false;
  params->url = start_url;
  params->transition = ui::PAGE_TRANSITION_LINK;
  params->should_update_history = false;
  params->gesture = NavigationGestureAuto;
  params->method = "GET";
  params->page_state = PageState::CreateFromURL(start_url);
  params->navigation_token =
      root->navigation_request()->commit_params().navigation_token;

  // Use an origin mismatched with the origin lock.
  params->origin = url::Origin::Create(another_url);
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  int process_id = root->current_frame_host()->GetProcess()->GetID();
  IsolationContext isolation_context(controller.GetBrowserContext());
  auto start_url_lock = SiteInstanceImpl::DetermineProcessLock(
      isolation_context, UrlInfo::CreateForTesting(start_url),
      false /* is_coop_coep_cross_origin_isolated */,
      base::nullopt /* coop_coep_cross_origin_isolated_origin */);
  auto another_url_lock = SiteInstanceImpl::DetermineProcessLock(
      isolation_context, UrlInfo::CreateForTesting(another_url),
      false /* is_coop_coep_cross_origin_isolated */,
      base::nullopt /* coop_coep_cross_origin_isolated_origin */);
  EXPECT_EQ(start_url_lock, policy->GetProcessLock(process_id));
  EXPECT_NE(another_url_lock, policy->GetProcessLock(process_id));

  // Transfer the NavigationRequest ownership to the RenderFrameHost. The test
  // for NavigationRequest match happens before the check of origin lock and
  // needs to be successful.
  root->TransferNavigationRequestOwnership(root->current_frame_host());
  root->current_frame_host()->OnCrossDocumentCommitProcessed(
      navigation_request, blink::mojom::CommitResult::Ok);

  // Simulate a commit IPC.
  mojo::PendingRemote<service_manager::mojom::InterfaceProvider>
      interface_provider;
  static_cast<mojom::FrameHost*>(root->current_frame_host())
      ->DidCommitProvisionalLoad(
          std::move(params),
          mojom::DidCommitProvisionalLoadInterfaceParams::New(
              interface_provider.InitWithNewPipeAndPassReceiver(),
              mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>()
                  .InitWithNewPipeAndPassReceiver()));

  // When the IPC message is received and validation fails, the process is
  // terminated. However, the notification for that should be processed in a
  // separate task of the message loop, so ensure that the process is still
  // considered alive.
  EXPECT_TRUE(
      root->current_frame_host()->GetProcess()->IsInitializedAndNotDead());

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
      EvalJsWithManualReply(
          shell(), JsReplace("var object = document.createElement('object');"
                             "document.body.appendChild(object);"
                             "object.data = $1;"
                             "object.type='text/html';"
                             "object.notify = true;"
                             "object.onload = () => {"
                             "  if (!object.notify) return;"
                             "  object.notify = false;"
                             "  window.domAutomationController.send('done');"
                             "};",
                             cross_origin))
          .ExtractString();
  ASSERT_EQ("done", msg);
  // To track the frame's visibility an EmbeddedContentView is needed. The
  // following steps make sure the visibility is tracked properly on the browser
  // side.
  auto* frame_connector = web_contents()
                              ->GetFrameTree()
                              ->root()
                              ->child_at(0)
                              ->render_manager()
                              ->GetProxyToParent()
                              ->cross_process_frame_connector();
  ASSERT_FALSE(frame_connector->IsHidden());
  ASSERT_TRUE(ExecJs(
      shell(), "document.querySelector('object').style.display = 'none';"));
  while (!frame_connector->IsHidden()) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }
  ASSERT_TRUE(ExecJs(
      shell(), "document.querySelector('object').style.display = 'block';"));
  while (frame_connector->IsHidden()) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }
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
  RenderFrameHostImpl* rfh_a = web_contents()->GetMainFrame();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();

  // RFH B has an unload handler.
  rfh_b->DoNotDeleteForTesting();
  EXPECT_TRUE(ExecJs(rfh_b, "onunload=function(){}"));

  // 2) Navigation from B to C. The server is slow to respond.
  TestNavigationManager navigation_observer(web_contents(), url_c);
  EXPECT_TRUE(ExecJs(rfh_b, JsReplace("location.href=$1;", url_c)));
  EXPECT_TRUE(navigation_observer.WaitForRequestStart());
  RenderFrameHostImpl* rfh_c =
      rfh_b->frame_tree_node()->render_manager()->speculative_frame_host();

  EXPECT_EQ(RenderFrameHostImpl::LifecycleState::kActive,
            rfh_a->lifecycle_state());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleState::kActive,
            rfh_b->lifecycle_state());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleState::kSpeculative,
            rfh_c->lifecycle_state());

  // 3) Deletion of B. The unload handler takes times to execute.
  RenderFrameDeletedObserver delete_b(rfh_b), delete_c(rfh_c);
  EXPECT_TRUE(
      ExecJs(rfh_a, JsReplace("document.querySelector('iframe').remove();")));
  EXPECT_FALSE(delete_b.deleted());
  EXPECT_TRUE(delete_c.deleted());  // The speculative RFH is deleted.
  EXPECT_EQ(RenderFrameHostImpl::LifecycleState::kActive,
            rfh_a->lifecycle_state());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleState::kRunningUnloadHandlers,
            rfh_b->lifecycle_state());

  // The navigation has been canceled.
  navigation_observer.WaitForNavigationFinished();
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
  RenderFrameHostImpl* rfh_a = web_contents()->GetMainFrame();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();
  RenderFrameHostImpl* rfh_c = rfh_b->child_at(0)->current_frame_host();

  // Leave rfh_c in pending deletion state.
  LeaveInPendingDeletionState(rfh_c);

  // 2) Navigation from C to D. The server is slow to respond.
  TestNavigationManager navigation_observer(web_contents(), url_d);
  EXPECT_TRUE(ExecJs(rfh_c, JsReplace("location.href=$1;", url_d)));
  EXPECT_TRUE(navigation_observer.WaitForRequestStart());
  RenderFrameHostImpl* rfh_d =
      rfh_c->frame_tree_node()->render_manager()->speculative_frame_host();

  EXPECT_EQ(RenderFrameHostImpl::LifecycleState::kActive,
            rfh_a->lifecycle_state());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleState::kActive,
            rfh_b->lifecycle_state());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleState::kActive,
            rfh_c->lifecycle_state());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleState::kSpeculative,
            rfh_d->lifecycle_state());

  // 3) Deletion of D. The unload handler takes times to execute.
  RenderFrameDeletedObserver delete_b(rfh_b), delete_c(rfh_c), delete_d(rfh_d);
  EXPECT_TRUE(
      ExecJs(rfh_a, JsReplace("document.querySelector('iframe').remove();")));
  EXPECT_FALSE(delete_b.deleted());
  EXPECT_FALSE(delete_c.deleted());
  EXPECT_TRUE(delete_d.deleted());  // The speculative RFH is deleted.
  EXPECT_EQ(RenderFrameHostImpl::LifecycleState::kActive,
            rfh_a->lifecycle_state());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleState::kReadyToBeDeleted,
            rfh_b->lifecycle_state());
  EXPECT_EQ(RenderFrameHostImpl::LifecycleState::kRunningUnloadHandlers,
            rfh_c->lifecycle_state());

  // The navigation has been canceled.
  navigation_observer.WaitForNavigationFinished();
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
  RenderFrameHostImpl* rfh_a = web_contents()->GetMainFrame();
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
  RenderFrameHostImpl* rfh_a = web_contents()->GetMainFrame();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();
  NavigateFrameToURL(rfh_b->frame_tree_node(), url_c);
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
  RenderFrameHostImpl* rfh_a = web_contents()->GetMainFrame();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();

  // Frame B has a unload handler. The browser process needs to wait before
  // deleting it.
  EXPECT_TRUE(ExecJs(rfh_b, "onunload=function(){}"));
  RenderFrameDeletedObserver deleted_observer(rfh_b);

  // window.open from A in B to url_c.
  DidStartNavigationObserver did_start_navigation_observer(web_contents());
  EXPECT_TRUE(ExecuteScript(rfh_b, "window.name = 'name';"));
  ExecuteScriptAsync(rfh_a, JsReplace("window.open($1, 'name');", url_c));

  // Simulate A deleting C.
  // It starts before receiving the navigation. The detach ACK is
  // received after.
  rfh_b->DetachFromProxy();
  deleted_observer.WaitUntilDeleted();

  EXPECT_FALSE(did_start_navigation_observer.observed());
}

// This test verifies that when scrolling an OOPIF in a pinched-zoomed page,
// that the scroll-delta matches the distance between TouchStart/End as seen
// by the oopif, i.e. the oopif content 'sticks' to the finger during scrolling.
// The relation is not exact, but should be close.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       ScrollOopifInPinchZoomedPage) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  ASSERT_EQ(1u, root->child_count());
  FrameTreeNode* child = root->child_at(0);
  ASSERT_TRUE(child);

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  // Make B scrollable. The call to document.write will erase the html inside
  // the OOPIF, leaving just a vertical column of 'Hello's.
  std::string script =
      "var s = '<div>Hello</div>\\n';\n"
      "document.write(s.repeat(200));";
  EXPECT_TRUE(ExecuteScript(child, script));

  RenderFrameSubmissionObserver observer_a(root);
  RenderFrameSubmissionObserver observer_b(child);

  // We need to observe a root frame submission to pick up the initial page
  // scale factor.
  observer_a.WaitForAnyFrameSubmission();

  const float kPageScaleDelta = 2.f;
  // On desktop systems we expect |current_page_scale| to be 1.f, but on
  // Android it will typically be less than 1.f, and may take on arbitrary
  // values.
  float original_page_scale =
      observer_a.LastRenderFrameMetadata().page_scale_factor;
  float target_page_scale = original_page_scale * kPageScaleDelta;

  SyntheticPinchGestureParams params;
  auto* host = static_cast<RenderWidgetHostImpl*>(
      root->current_frame_host()->GetRenderWidgetHost());
  RenderWidgetHostViewBase* root_view = host->GetView();
  RenderWidgetHostViewBase* child_view =
      static_cast<RenderWidgetHostImpl*>(
          child->current_frame_host()->GetRenderWidgetHost())
          ->GetView();
  gfx::Rect bounds(root_view->GetViewBounds().size());
  // The synthetic gesture code expects a location in root-view coordinates.
  params.anchor = gfx::PointF(bounds.CenterPoint().x(), 70.f);
  // In SyntheticPinchGestureParams, |scale_factor| is really a delta.
  params.scale_factor = kPageScaleDelta;
#if defined(OS_MAC)
  auto synthetic_pinch_gesture =
      std::make_unique<SyntheticTouchpadPinchGesture>(params);
#else
  auto synthetic_pinch_gesture =
      std::make_unique<SyntheticTouchscreenPinchGesture>(params);
#endif

  // Send pinch gesture and verify we receive the ack.
  {
    InputEventAckWaiter ack_waiter(
        host, blink::WebInputEvent::Type::kGesturePinchEnd);
    host->QueueSyntheticGesture(
        std::move(synthetic_pinch_gesture),
        base::BindOnce([](SyntheticGesture::Result result) {
          EXPECT_EQ(SyntheticGesture::GESTURE_FINISHED, result);
        }));
    ack_waiter.Wait();
  }

  // Make sure all the page scale values behave as expected.
  const float kScaleTolerance = 0.07f;
  observer_a.WaitForPageScaleFactor(target_page_scale, kScaleTolerance);
  observer_b.WaitForExternalPageScaleFactor(target_page_scale, kScaleTolerance);
  float final_page_scale =
      observer_a.LastRenderFrameMetadata().page_scale_factor;

  // Verify scroll position of OOPIF.
  float initial_child_scroll = EvalJs(child, "window.scrollY").ExtractDouble();

  // Send touch-initiated gesture scroll sequence to OOPIF.
  // TODO(wjmaclean): GetViewBounds() is broken for OOPIFs when PSF != 1.f, so
  // we calculate it manually. This will need to be update when GetViewBounds()
  // in RenderWidgetHostViewBase is fixed. See https://crbug.com/928825.
  auto child_bounds = child_view->GetViewBounds();
  gfx::PointF child_upper_left =
      child_view->TransformPointToRootCoordSpaceF(gfx::PointF(0, 0));
  gfx::PointF child_lower_right = child_view->TransformPointToRootCoordSpaceF(
      gfx::PointF(child_bounds.width(), child_bounds.height()));
  gfx::PointF scroll_start_location_in_screen =
      gfx::PointF((child_upper_left.x() + child_lower_right.x()) / 2.f,
                  child_lower_right.y() - 10);
  const float kScrollDelta = 100.f;
  gfx::PointF scroll_end_location_in_screen =
      scroll_start_location_in_screen + gfx::Vector2dF(0, -kScrollDelta);

  // Create touch move sequence with discrete touch moves. Include a brief
  // pause at the end to avoid the scroll flinging.
  std::string actions_template = R"HTML(
      [{
        "source" : "touch",
        "actions" : [
          { "name": "pointerDown", "x": %f, "y": %f},
          { "name": "pointerMove", "x": %f, "y": %f},
          { "name": "pause", "duration": 300 },
          { "name": "pointerUp"}
        ]
      }]
  )HTML";
  std::string touch_move_sequence_json = base::StringPrintf(
      actions_template.c_str(), scroll_start_location_in_screen.x(),
      scroll_start_location_in_screen.y(), scroll_end_location_in_screen.x(),
      scroll_end_location_in_screen.y());
  base::JSONReader::ValueWithError parsed_json =
      base::JSONReader::ReadAndReturnValueWithError(touch_move_sequence_json);
  ASSERT_TRUE(parsed_json.value) << parsed_json.error_message;
  ActionsParser actions_parser(std::move(*parsed_json.value));

  ASSERT_TRUE(actions_parser.ParsePointerActionSequence());
  auto synthetic_scroll_gesture =
      SyntheticGesture::Create(actions_parser.gesture_params());

  {
    auto* child_host = static_cast<RenderWidgetHostImpl*>(
        child->current_frame_host()->GetRenderWidgetHost());
    InputEventAckWaiter ack_waiter(
        child_host, blink::WebInputEvent::Type::kGestureScrollEnd);
    host->QueueSyntheticGesture(
        std::move(synthetic_scroll_gesture),
        base::BindOnce([](SyntheticGesture::Result result) {
          EXPECT_EQ(SyntheticGesture::GESTURE_FINISHED, result);
        }));
    ack_waiter.Wait();
  }

  // Verify new scroll position of OOPIF, should match touch sequence delta.
  float expected_scroll_delta = kScrollDelta / final_page_scale;
  float actual_scroll_delta =
      EvalJs(child, "window.scrollY").ExtractDouble() - initial_child_scroll;

  const float kScrollTolerance = 0.2f;
  EXPECT_GT((1.f + kScrollTolerance) * expected_scroll_delta,
            actual_scroll_delta);
  EXPECT_LT((1.f - kScrollTolerance) * expected_scroll_delta,
            actual_scroll_delta);
}

// Check that if a frame starts a navigation, and the frame's current process
// dies before the response for the navigation comes back, the response will
// not trigger a process kill and will be allowed to commit in a new process.
// See https://crbug.com/968259.
// Note: This test needs to do a browser-initiated navigation because doing
// a renderer-initiated navigation would lead to the navigation being canceled.
// This behavior change has been introduced with PerNavigationMojoInterface and
// is documented here https://crbug.com/988368.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest,
                       ProcessDiesBeforeCrossSiteNavigationCompletes) {
  GURL first_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), first_url));
  scoped_refptr<SiteInstanceImpl> first_site_instance(
      web_contents()->GetMainFrame()->GetSiteInstance());

  // Start a cross-site navigation and proceed only up to the request start.
  GURL second_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  TestNavigationManager delayer(web_contents(), second_url);
  web_contents()->GetController().LoadURL(
      second_url, Referrer(), ui::PageTransition::PAGE_TRANSITION_TYPED,
      std::string());
  EXPECT_TRUE(delayer.WaitForRequestStart());

  // Terminate the current a.com process.
  RenderProcessHost* first_process =
      web_contents()->GetMainFrame()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      first_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  EXPECT_TRUE(first_process->Shutdown(0));
  crash_observer.Wait();
  EXPECT_FALSE(web_contents()->GetMainFrame()->IsRenderFrameLive());

  // Resume the cross-site navigation and ensure it commits in a new
  // SiteInstance and process.
  delayer.WaitForNavigationFinished();
  EXPECT_TRUE(web_contents()->GetMainFrame()->IsRenderFrameLive());
  EXPECT_NE(web_contents()->GetMainFrame()->GetProcess(), first_process);
  EXPECT_NE(web_contents()->GetMainFrame()->GetSiteInstance(),
            first_site_instance);
  EXPECT_EQ(second_url, web_contents()->GetMainFrame()->GetLastCommittedURL());
}

class SitePerProcessFeaturePolicySandboxTest
    : public SitePerProcessFeaturePolicyBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SitePerProcessFeaturePolicyBrowserTest::SetUpCommandLine(command_line);
    feature_list_.InitAndEnableFeature(features::kFeaturePolicyForSandbox);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Check that sandbox flags are correctly propagated to the browser from a
// renderer which sets them via feature policy.
IN_PROC_BROWSER_TEST_F(SitePerProcessFeaturePolicySandboxTest,
                       SandboxFlagsCorrectlySentFromRenderer) {
  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL(
          "a.com",
          "/cross_site_iframe_factory.html?a(b{sandbox-allow-scripts})")));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  network::mojom::WebSandboxFlags expected_flags =
      network::mojom::WebSandboxFlags::kAll &
      ~network::mojom::WebSandboxFlags::kScripts &
      ~network::mojom::WebSandboxFlags::kAutomaticFeatures;
  // Validate sandbox flags bit-by-bit. This is equivalent to an equality check
  // when FeaturePolicyForSandbox is disabled, but when that feature is enabled,
  // this will check the appropriate policy-controlled feature for each expected
  // sandbox flag.
  for (unsigned bit = 0; bit < sizeof(network::mojom::WebSandboxFlags) * 8;
       bit++) {
    network::mojom::WebSandboxFlags flag =
        static_cast<network::mojom::WebSandboxFlags>(1 << bit);
    if (static_cast<unsigned>(expected_flags) & (1 << bit)) {
      EXPECT_TRUE(root->child_at(0)->current_frame_host()->IsSandboxed(flag));
    } else {
      EXPECT_FALSE(root->child_at(0)->current_frame_host()->IsSandboxed(flag));
    }
  }
}

IN_PROC_BROWSER_TEST_F(SitePerProcessFeaturePolicySandboxTest,
                       SandboxFlagsCorrectlySetByFeaturePolicy) {
  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL(
          "a.com",
          "/cross_site_iframe_factory.html?a(b{sandbox,allow-scripts})")));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  network::mojom::WebSandboxFlags expected_flags =
      network::mojom::WebSandboxFlags::kAll &
      ~network::mojom::WebSandboxFlags::kScripts &
      ~network::mojom::WebSandboxFlags::kAutomaticFeatures;
  // Validate sandbox flags bit-by-bit. This is equivalent to an equality check
  // when FeaturePolicyForSandbox is disabled, but when that feature is enabled,
  // this will check the appropriate policy-controlled feature for each expected
  // sandbox flag.
  for (int bit = 0; bit < 32; bit++) {
    network::mojom::WebSandboxFlags flag =
        static_cast<network::mojom::WebSandboxFlags>(1 << bit);
    if (static_cast<unsigned>(expected_flags) & (1 << bit)) {
      EXPECT_TRUE(root->child_at(0)->current_frame_host()->IsSandboxed(flag));
    } else {
      EXPECT_FALSE(root->child_at(0)->current_frame_host()->IsSandboxed(flag));
    }
  }
}

class FeaturePolicyPropagationToAuxiliaryBrowsingContextTest
    : public SitePerProcessFeaturePolicySandboxTest,
      public testing::WithParamInterface<std::tuple<
          const char* /* Whether or not <iframe> is sandbox or can escape it */,
          bool /* <iframe> same origin? */,
          bool /* opened window same origin? */,
          const char* /* window feature in window.open() */>> {};

// This test verifies the correct propagation of FeaturePolicy from an *opener*
// to the opened auxiliary browsing context. This test runs for both cross and
// same origin frames.
IN_PROC_BROWSER_TEST_P(FeaturePolicyPropagationToAuxiliaryBrowsingContextTest,
                       PropagationForDifferentOrigins) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   "a.com", "/feature_policy_window_open_embedder.html")));
  const GURL same_origin_child = embedded_test_server()->GetURL(
      "a.com", "/feature_policy_window_open_embedded.html");
  const GURL cross_origin_child = embedded_test_server()->GetURL(
      "b.com", "/feature_policy_window_open_embedded.html");
  const GURL cross_origin_window_url =
      embedded_test_server()->GetURL("c.com", "/title1.html");
  const GURL same_origin_window_url = GURL("about:blank");
  const std::string kScriptCheckPolicy =
      "document.featurePolicy.allowsFeature('sync-xhr')";
  // Test parameters
  const std::string iframe_type = std::get<0>(GetParam());
  const std::string iframe_src =
      (std::get<1>(GetParam()) ? same_origin_child : cross_origin_child).spec();
  const std::string window_url =
      (std::get<2>(GetParam()) ? same_origin_window_url
                               : cross_origin_window_url)
          .spec();
  const std::string window_feature = std::get<3>(GetParam());
  SCOPED_TRACE(testing::Message() << " <iframe> Type: " << iframe_type
                                  << " <iframe> Source: " << iframe_src
                                  << " window URL: " << window_url
                                  << " window Feature: " << window_feature);
  // TODO(ekaramad): Modify the test expectation once we resolve the following
  // issues in feature policies:
  //   - "rel='noopener'" might end up resetting the feature policies.
  //   - A new feature to escape feature policies is introduced (similar to the
  //     sandbox version "allow-popups-to-escape-sandbox".
  // For now, only the sandbox-escaping case should be allowed to have a new
  // (default) feature policy state.
  bool expected_feature_state_in_auxiliary_browsing_context =
      (iframe_type == "sandboxed-escaping");
  ShellAddedObserver shell_added_observer;
  ASSERT_TRUE(
      ExecJs(shell(), JsReplace("test($1, $2, $3, $4)", iframe_type, iframe_src,
                                window_url, window_feature)));
  auto* new_shell = shell_added_observer.GetShell();
  while (new_shell->web_contents()->GetLastCommittedURL() != GURL(window_url)) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }
  ASSERT_TRUE(WaitForLoadStop(new_shell->web_contents()));
  ASSERT_EQ(new_shell->web_contents()->GetLastCommittedURL(), window_url);
  EXPECT_EQ(expected_feature_state_in_auxiliary_browsing_context,
            EvalJs(new_shell, kScriptCheckPolicy));
  bool browser_side_feature_state =
      static_cast<RenderFrameHostImpl*>(
          new_shell->web_contents()->GetMainFrame())
          ->feature_policy()
          ->IsFeatureEnabled(blink::mojom::FeaturePolicyFeature::kSyncXHR);
  EXPECT_EQ(expected_feature_state_in_auxiliary_browsing_context,
            browser_side_feature_state);
  new_shell->Close();
}

INSTANTIATE_TEST_SUITE_P(SitePerProcess,
                         FeaturePolicyPropagationToAuxiliaryBrowsingContextTest,
                         testing::Combine(
                             /* iframe_type */
                             testing::ValuesIn({"sandboxed", "notsandboxed",
                                                "sandboxed-escaping"}),
                             /* iframe_same_origin */
                             testing::Bool(),
                             /* window_same_origin */
                             testing::Bool(),
                             /* window feature */
                             testing::ValuesIn({"", "noopener"})));

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
    RenderFrameHostImpl* new_render_frame_host_ = nullptr;
    base::RunLoop run_loop_;

    DISALLOW_COPY_AND_ASSIGN(PrepareFrameJob);
  };

 private:
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(InnerWebContentsAttachTest);
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
  auto* child_node = web_contents()->GetFrameTree()->root()->child_at(0);
  NavigateFrameToURL(child_node, child_frame_url);
  if (test_beforeunload) {
    EXPECT_TRUE(ExecJs(child_node,
                       "window.addEventListener('beforeunload', (e) => {"
                       "e.returnValue = ''; return e; });"));
  }
  auto* original_child_frame = child_node->current_frame_host();
  RenderFrameDeletedObserver original_child_frame_observer(
      original_child_frame);
  PrepareFrameJob prepare_job(original_child_frame,
                              proceed_through_beforeunload);
  if (test_beforeunload)
    WaitForAppModalDialog(shell());
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
  RenderFrameHostImpl* a1_rfh = web_contents()->GetMainFrame();
  RenderFrameHostImpl* b2_rfh = a1_rfh->child_at(0)->current_frame_host();

  // 2. B2 navigates itself to B3 = about:blank. Process B is used.
  {
    scoped_refptr<SiteInstance> b2_site_instance = b2_rfh->GetSiteInstance();
    TestNavigationManager navigation_manager(web_contents(),
                                             GURL("about:blank"));
    EXPECT_TRUE(ExecJs(b2_rfh, "location.href = 'about:blank';"));
    navigation_manager.WaitForNavigationFinished();

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
    navigation_manager.WaitForNavigationFinished();

    RenderFrameHostImpl* b4_rfh = a1_rfh->child_at(0)->current_frame_host();
    DCHECK_EQ(a1_rfh->GetSiteInstance(), b4_rfh->GetSiteInstance());
  }
}

// Tests that main_frame_scroll_offset is not shared by frames in the same
// process. This is a regression test for https://crbug.com/1063760.
//
// Set up the frame tree to be A(B1(C1),B2(C2)). Send IPC's with different
// ViewportIntersection information to B1 and B2, and then check that the
// information they propagate to C1 and C2 is different.
IN_PROC_BROWSER_TEST_P(SitePerProcessBrowserTest, MainFrameScrollOffset) {
  GURL a_url = embedded_test_server()->GetURL(
      "a.com", "/frame_tree/scrollable_page_with_two_frames.html");
  GURL b_url = embedded_test_server()->GetURL(
      "b.com", "/frame_tree/page_with_large_iframe.html");
  GURL c_url = embedded_test_server()->GetURL("c.com", "/title1.html");

  EXPECT_TRUE(NavigateToURL(shell(), a_url));
  FrameTreeNode* a_node = web_contents()->GetFrameTree()->root();

  FrameTreeNode* b1_node = a_node->child_at(0);
  NavigateFrameToURL(b1_node, b_url);

  FrameTreeNode* c1_node = b1_node->child_at(0);
  NavigateFrameToURL(c1_node, c_url);

  FrameTreeNode* b2_node = a_node->child_at(1);
  NavigateFrameToURL(b2_node, b_url);

  FrameTreeNode* c2_node = b2_node->child_at(0);
  NavigateFrameToURL(c2_node, c_url);

  // This will intercept messages sent from B1 to C1, describing C1's viewport
  // intersection.
  auto b1_to_c1_message_filter =
      base::MakeRefCounted<UpdateViewportIntersectionMessageFilter>(
          c1_node->render_manager()->GetProxyToParent()->GetRoutingID());
  b1_node->current_frame_host()->GetProcess()->AddFilter(
      b1_to_c1_message_filter.get());

  // This will intercept messages sent from B2 to C2, describing C2's viewport
  // intersection.
  auto b2_to_c2_message_filter =
      base::MakeRefCounted<UpdateViewportIntersectionMessageFilter>(
          c2_node->render_manager()->GetProxyToParent()->GetRoutingID());
  b2_node->current_frame_host()->GetProcess()->AddFilter(
      b2_to_c2_message_filter.get());

  // Running requestAnimationFrame will ensure that any pending IPC's have been
  // sent by the renderer and received by the browser.
  auto flush_ipcs = [](FrameTreeNode* node) {
    ASSERT_TRUE(EvalJsAfterLifecycleUpdate(node->current_frame_host(), "", "")
                    .error.empty());
  };

  flush_ipcs(a_node);
  flush_ipcs(b1_node);
  flush_ipcs(b2_node);
  b1_to_c1_message_filter->Clear();
  b2_to_c2_message_filter->Clear();

  // Now that everything is in a stable, consistent state, we will send viewport
  // intersection IPC's to B1 and B2 that contain a different
  // main_frame_scroll_offset, and then verify that each of them propagates
  // their own value of main_frame_scroll_offset to C1 and C2, respectively.
  // The IPC code mimics messages that A would send to B1 and B2.
  blink::ViewportIntersectionState b1_intersection_state =
      b1_node->render_manager()
          ->GetProxyToParent()
          ->cross_process_frame_connector()
          ->intersection_state();
  b1_intersection_state.main_frame_scroll_offset.Offset(10, 0);
  // A change in main_frame_scroll_offset by itself will not cause B1 to be
  // marked dirty, so we also modify viewport_intersection.
  b1_intersection_state.viewport_intersection.y += 7;
  b1_intersection_state.viewport_intersection.height -= 7;
  {
    FrameHostMsg_UpdateViewportIntersection message(MSG_ROUTING_NONE,
                                                    b1_intersection_state);
    b1_node->render_manager()
        ->GetProxyToParent()
        ->cross_process_frame_connector()
        ->OnMessageReceived(message);
  }

  blink::ViewportIntersectionState b2_intersection_state =
      b2_node->render_manager()
          ->GetProxyToParent()
          ->cross_process_frame_connector()
          ->intersection_state();
  b2_intersection_state.main_frame_scroll_offset.Offset(20, 0);
  b2_intersection_state.viewport_intersection.y += 7;
  b2_intersection_state.viewport_intersection.height -= 7;
  {
    FrameHostMsg_UpdateViewportIntersection message(MSG_ROUTING_NONE,
                                                    b2_intersection_state);
    b2_node->render_manager()
        ->GetProxyToParent()
        ->cross_process_frame_connector()
        ->OnMessageReceived(message);
  }

  // Once IPC's have been flushed to the C frames, we should see conflicting
  // values for main_frame_scroll_offset.
  flush_ipcs(b1_node);
  flush_ipcs(b2_node);
  ASSERT_TRUE(b1_to_c1_message_filter->MessageReceived());
  ASSERT_TRUE(b2_to_c2_message_filter->MessageReceived());
  EXPECT_EQ(
      b1_to_c1_message_filter->GetIntersectionState().main_frame_scroll_offset,
      gfx::Point(10, 0));
  EXPECT_EQ(
      b2_to_c2_message_filter->GetIntersectionState().main_frame_scroll_offset,
      gfx::Point(20, 0));
  b1_to_c1_message_filter->Clear();
  b2_to_c2_message_filter->Clear();

  // If we scroll the main frame, it should propagate IPC's which re-synchronize
  // the values for all child frames.
  ASSERT_TRUE(EvalJsAfterLifecycleUpdate(a_node->current_frame_host(),
                                         "window.scrollTo(0, 5)", "")
                  .error.empty());
  flush_ipcs(b1_node);
  flush_ipcs(b2_node);
  ASSERT_TRUE(b1_to_c1_message_filter->MessageReceived());
  ASSERT_TRUE(b2_to_c2_message_filter->MessageReceived());

  // Window scroll offset will be scaled by device scale factor
  blink::ScreenInfo screen_info;
  a_node->render_manager()->GetRenderWidgetHostView()->GetScreenInfo(
      &screen_info);
  int expected_y = roundf(screen_info.device_scale_factor * 5.0);
  EXPECT_EQ(
      b1_to_c1_message_filter->GetIntersectionState().main_frame_scroll_offset,
      gfx::Point(0, expected_y));
  EXPECT_EQ(
      b2_to_c2_message_filter->GetIntersectionState().main_frame_scroll_offset,
      gfx::Point(0, expected_y));
}

class SitePerProcessCompositorViewportBrowserTest
    : public SitePerProcessBrowserTestBase,
      public testing::WithParamInterface<double> {
 public:
  SitePerProcessCompositorViewportBrowserTest() {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SitePerProcessBrowserTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor,
                                    base::StringPrintf("%f", GetParam()));
  }
};

// DISABLED: crbug.com/1071995
IN_PROC_BROWSER_TEST_P(SitePerProcessCompositorViewportBrowserTest,
                       DISABLED_OopifCompositorViewportSizeRelativeToParent) {
  // Load page with very tall OOPIF.
  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/super_tall_parent.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child = root->child_at(0);

  GURL nested_site_url(
      embedded_test_server()->GetURL("b.com", "/super_tall_page.html"));
  NavigateFrameToURL(child, nested_site_url);

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  // Observe frame submission from parent.
  RenderFrameSubmissionObserver parent_observer(
      root->current_frame_host()
          ->GetRenderWidgetHost()
          ->render_frame_metadata_provider());
  parent_observer.WaitForAnyFrameSubmission();
  gfx::Size parent_viewport_size =
      parent_observer.LastRenderFrameMetadata().viewport_size_in_pixels;

  // Observe frame submission from child.
  RenderFrameSubmissionObserver child_observer(
      child->current_frame_host()
          ->GetRenderWidgetHost()
          ->render_frame_metadata_provider());
  child_observer.WaitForAnyFrameSubmission();
  gfx::Size child_viewport_size =
      child_observer.LastRenderFrameMetadata().viewport_size_in_pixels;

  // Verify child's compositor viewport is no more than about 30% larger than
  // the parent's. See RemoteFrameView::GetCompositingRect() for explanation of
  // the choice of 30%. Add +1 to child viewport height to account for rounding.
  EXPECT_GE(ceilf(1.3f * parent_viewport_size.height()),
            child_viewport_size.height() - 1);

  // Verify the child's ViewBounds are much larger.
  RenderWidgetHostViewBase* child_rwhv = static_cast<RenderWidgetHostViewBase*>(
      child->current_frame_host()->GetRenderWidgetHost()->GetView());
  // 30,000 is based on div/iframe sizes in the test HTML files.
  EXPECT_LT(30000, child_rwhv->GetViewBounds().height());
}

#if defined(OS_ANDROID)
// Android doesn't support forcing device scale factor in tests.
INSTANTIATE_TEST_SUITE_P(SitePerProcess,
                         SitePerProcessCompositorViewportBrowserTest,
                         testing::Values(1.0));
#else
INSTANTIATE_TEST_SUITE_P(SitePerProcess,
                         SitePerProcessCompositorViewportBrowserTest,
                         testing::Values(1.0, 1.5, 2.0));
#endif

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

  RenderProcessHost* js_process = web_contents()->GetMainFrame()->GetProcess();

  // Because the javascript URL can run arbitrary scripts in the renderer
  // process, it is unsafe to reuse the renderer process later for navigations
  // to sites that require a dedicated process.  Ensure that this is the case.
  EXPECT_FALSE(js_process->IsUnused());

  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));
  EXPECT_NE(js_process, web_contents()->GetMainFrame()->GetProcess());
}

INSTANTIATE_TEST_SUITE_P(All,
                         RequestDelayingSitePerProcessBrowserTest,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));
INSTANTIATE_TEST_SUITE_P(All,
                         ScrollingIntegrationTest,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));
#if defined(OS_ANDROID)
INSTANTIATE_TEST_SUITE_P(All,
                         SitePerProcessAndroidImeTest,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));
#endif  // OS_ANDROID
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
                         SitePerProcessBrowserTestWithoutSadFrameTabReload,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));
INSTANTIATE_TEST_SUITE_P(All,
                         SitePerProcessBrowserTestWithSadFrameTabReload,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));
INSTANTIATE_TEST_SUITE_P(All,
                         SitePerProcessBrowserTouchActionTest,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));
INSTANTIATE_TEST_SUITE_P(All,
                         SitePerProcessEmbedderCSPEnforcementBrowserTest,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));
INSTANTIATE_TEST_SUITE_P(All,
                         SitePerProcessHighDPIBrowserTest,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));
INSTANTIATE_TEST_SUITE_P(All,
                         SitePerProcessIgnoreCertErrorsBrowserTest,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));
INSTANTIATE_TEST_SUITE_P(All,
                         SitePerProcessProgrammaticScrollTest,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));
INSTANTIATE_TEST_SUITE_P(All,
                         SitePerProcessScrollAnchorTest,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));
#if defined(OS_ANDROID)
INSTANTIATE_TEST_SUITE_P(All,
                         TouchSelectionControllerClientAndroidSiteIsolationTest,
                         testing::ValuesIn(RenderDocumentFeatureLevelValues()));
#endif  // OS_ANDROID
}  // namespace content
