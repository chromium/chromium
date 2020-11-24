// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_impl.h"

#include <stddef.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/process/process.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/optional_trace_event.h"
#include "base/trace_event/trace_conversion_helper.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/download/public/common/download_stats.h"
#include "components/rappor/public/rappor_utils.h"
#include "components/url_formatter/url_formatter.h"
#include "content/browser/accessibility/accessibility_event_recorder.h"
#include "content/browser/accessibility/accessibility_tree_formatter_blink.h"
#include "content/browser/bad_message.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/browser_plugin/browser_plugin_embedder.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/conversions/conversion_host.h"
#include "content/browser/devtools/protocol/page_handler.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/display_cutout/display_cutout_host_impl.h"
#include "content/browser/dom_storage/dom_storage_context_wrapper.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "content/browser/download/mhtml_generation_manager.h"
#include "content/browser/download/save_package.h"
#include "content/browser/find_request_manager.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/manifest/manifest_manager_host.h"
#include "content/browser/media/audio_stream_broker.h"
#include "content/browser/media/audio_stream_monitor.h"
#include "content/browser/media/capture/web_contents_audio_muter.h"
#include "content/browser/media/media_web_contents_observer.h"
#include "content/browser/media/session/media_session_impl.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/browser/plugin_content_origin_allowlist.h"
#include "content/browser/portal/portal.h"
#include "content/browser/renderer_host/agent_scheduling_group_host.h"
#include "content/browser/renderer_host/cross_process_frame_connector.h"
#include "content/browser/renderer_host/frame_token_message_queue.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/renderer_host/text_input_manager.h"
#include "content/browser/screen_enumeration/screen_change_monitor.h"
#include "content/browser/screen_orientation/screen_orientation_provider.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_contents/javascript_dialog_navigation_deferrer.h"
#include "content/browser/web_contents/web_contents_view_child_frame.h"
#include "content/browser/web_package/save_as_web_bundle_job.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/browser/webui/web_ui_impl.h"
#include "content/browser/xr/service/xr_runtime_manager_impl.h"
#include "content/common/content_switches_internal.h"
#include "content/common/drag_messages.h"
#include "content/common/frame_messages.h"
#include "content/common/input_messages.h"
#include "content/common/page_messages.h"
#include "content/common/page_state_serialization.h"
#include "content/common/render_message_filter.mojom.h"
#include "content/common/view_messages.h"
#include "content/common/widget_messages.h"
#include "content/public/browser/accessibility_tree_formatter.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/focused_node_details.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/load_notification_details.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/restore_type.h"
#include "content/public/browser/security_style_explanations.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_receiver_set.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/page_state.h"
#include "content/public/common/referrer_type_converters.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/url_utils.h"
#include "content/public/common/use_zoom_for_dsf_policy.h"
#include "media/base/media_switches.h"
#include "media/base/user_input_monitor.h"
#include "net/base/url_util.h"
#include "net/http/http_cache.h"
#include "net/http/http_transaction_factory.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/web_sandbox_flags.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/public/common/security/security_style.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "third_party/blink/public/mojom/loader/pause_subresource_loading_handle.mojom.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/accessibility/ax_tree_combiner.h"
#include "ui/base/device_form_factor.h"
#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif
#include "ui/base/layout.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/display/screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gl/gl_switches.h"

#if defined(OS_WIN)
#include "content/browser/renderer_host/dip_util.h"
#include "ui/gfx/geometry/dip_util.h"
#endif

#if defined(OS_ANDROID)
#include "content/browser/android/date_time_chooser_android.h"
#include "content/browser/android/java_interfaces_impl.h"
#include "content/browser/android/nfc_host.h"
#include "content/browser/web_contents/web_contents_android.h"
#include "services/device/public/mojom/nfc.mojom.h"
#include "ui/android/view_android.h"
#else  // !OS_ANDROID
#include "content/browser/host_zoom_map_impl.h"
#endif  // OS_ANDROID

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/browser/media/session/pepper_playback_observer.h"
#endif

namespace content {

using AccessibilityEventCallback =
    base::RepeatingCallback<void(const std::string&)>;

namespace {

const int kMinimumDelayBetweenLoadingUpdatesMS = 100;
const char kDotGoogleDotCom[] = ".google.com";

using LifecycleState = RenderFrameHostImpl::LifecycleState;

// TODO(crbug.com/1059903): Clean up after the initial investigation.
constexpr base::Feature kCheckWebContentsAccessFromNonCurrentFrame{
    "CheckWebContentsAccessFromNonCurrentFrame",
    base::FEATURE_DISABLED_BY_DEFAULT};

base::LazyInstance<std::vector<
    WebContentsImpl::FriendWrapper::CreatedCallback>>::DestructorAtExit
    g_created_callbacks = LAZY_INSTANCE_INITIALIZER;

bool HasMatchingProcess(FrameTree* tree, int render_process_id) {
  for (FrameTreeNode* node : tree->Nodes()) {
    if (node->current_frame_host()->GetProcess()->GetID() == render_process_id)
      return true;
  }
  return false;
}

bool HasMatchingWidgetHost(FrameTree* tree, RenderWidgetHost* host) {
  // This method scans the frame tree rather than checking whether
  // host->delegate() == this, which allows it to return false when the host
  // for a frame that is pending or pending deletion.
  if (!host)
    return false;

  for (FrameTreeNode* node : tree->Nodes()) {
    if (node->current_frame_host()->GetRenderWidgetHost() == host)
      return true;
  }
  return false;
}

void UpdateAccessibilityModeOnFrame(RenderFrameHost* frame_host) {
  static_cast<RenderFrameHostImpl*>(frame_host)->UpdateAccessibilityMode();
}

void ResetAccessibility(RenderFrameHost* rfh) {
  static_cast<RenderFrameHostImpl*>(rfh)->AccessibilityReset();
}

RenderFrameHostImpl* FindOpenerRFH(const WebContents::CreateParams& params) {
  RenderFrameHostImpl* opener_rfh = nullptr;
  if (params.opener_render_frame_id != MSG_ROUTING_NONE) {
    opener_rfh = RenderFrameHostImpl::FromID(params.opener_render_process_id,
                                             params.opener_render_frame_id);
  }
  return opener_rfh;
}

// Returns |true| if |type| is the kind of user input that should trigger the
// user interaction observers.
bool IsUserInteractionInputType(blink::WebInputEvent::Type type) {
  // Ideally, this list would be based more off of
  // https://whatwg.org/C/interaction.html#triggered-by-user-activation.
  return type == blink::WebInputEvent::Type::kMouseDown ||
         type == blink::WebInputEvent::Type::kGestureScrollBegin ||
         type == blink::WebInputEvent::Type::kTouchStart ||
         type == blink::WebInputEvent::Type::kRawKeyDown;
}

// Ensures that OnDialogClosed is only called once.
class CloseDialogCallbackWrapper
    : public base::RefCountedThreadSafe<CloseDialogCallbackWrapper> {
 public:
  using CloseCallback =
      base::OnceCallback<void(bool, bool, const base::string16&)>;

  explicit CloseDialogCallbackWrapper(CloseCallback callback)
      : callback_(std::move(callback)) {}

  void Run(bool dialog_was_suppressed,
           bool success,
           const base::string16& user_input) {
    if (callback_.is_null())
      return;
    std::move(callback_).Run(dialog_was_suppressed, success, user_input);
  }

 private:
  friend class base::RefCountedThreadSafe<CloseDialogCallbackWrapper>;
  ~CloseDialogCallbackWrapper() = default;

  CloseCallback callback_;
};

bool FrameCompareDepth(RenderFrameHostImpl* a, RenderFrameHostImpl* b) {
  return a->frame_tree_node()->depth() < b->frame_tree_node()->depth();
}

bool AreValidRegisterProtocolHandlerArguments(const std::string& protocol,
                                              const GURL& url,
                                              const url::Origin& origin) {
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  if (policy->IsPseudoScheme(protocol))
    return false;

  url::Origin url_origin = url::Origin::Create(url);
  if (url_origin.opaque())
    return false;

  if (!url_origin.IsSameOriginWith(origin))
    return false;

  return true;
}

void RecordMaxFrameCountUMA(size_t max_frame_count) {
  UMA_HISTOGRAM_COUNTS_10000("Navigation.MainFrame.MaxFrameCount",
                             max_frame_count);
}

// Returns whether the condition provided applies to any inner contents.
// This check is not recursive (however, the predicate provided may itself
// recurse each contents' own inner contents).
//
// For example, if this is used to aggregate state from inner contents to outer
// contents, then that propagation will gather transitive descendants without
// need for this helper to do so. In fact, in such cases recursing on inner
// contents here would make that operation quadratic rather than linear.
template <typename Functor>
bool AnyInnerWebContents(WebContents* web_contents, const Functor& f) {
  const auto& inner_contents = web_contents->GetInnerWebContents();
  return std::any_of(inner_contents.begin(), inner_contents.end(), f);
}

std::vector<RenderFrameHost*> GetAllFramesImpl(FrameTree& frame_tree,
                                               bool include_pending) {
  std::vector<RenderFrameHost*> frame_hosts;
  for (FrameTreeNode* node : frame_tree.Nodes()) {
    frame_hosts.push_back(node->current_frame_host());
    if (include_pending) {
      RenderFrameHostImpl* pending_frame_host =
          node->render_manager()->speculative_frame_host();
      if (pending_frame_host)
        frame_hosts.push_back(pending_frame_host);
    }
  }
  return frame_hosts;
}

int SendToAllFramesImpl(FrameTree& frame_tree,
                        bool include_pending,
                        IPC::Message* message) {
  int number_of_messages = 0;
  std::vector<RenderFrameHost*> frame_hosts =
      GetAllFramesImpl(frame_tree, include_pending);
  for (RenderFrameHost* rfh : frame_hosts) {
    if (!rfh->IsRenderFrameLive())
      continue;

    ++number_of_messages;
    IPC::Message* message_copy = new IPC::Message(*message);
    message_copy->set_routing_id(rfh->GetRoutingID());
    rfh->Send(message_copy);
  }
  delete message;
  return number_of_messages;
}

// Returns the set of all WebContentses that are reachable from |web_contents|
// by applying some combination of WebContents::GetOriginalOpener() and
// WebContents::GetOuterWebContents(). The |web_contents| parameter will be
// included in the returned set.
base::flat_set<WebContentsImpl*> GetAllOpeningWebContents(
    WebContentsImpl* web_contents) {
  base::flat_set<WebContentsImpl*> result;
  base::flat_set<WebContentsImpl*> current;

  current.insert(web_contents);

  while (!current.empty()) {
    WebContentsImpl* current_contents = *current.begin();
    current.erase(current.begin());
    auto insert_result = result.insert(current_contents);

    if (insert_result.second) {
      RenderFrameHostImpl* opener_rfh = current_contents->GetOriginalOpener();
      if (opener_rfh) {
        current.insert(static_cast<WebContentsImpl*>(
            WebContents::FromRenderFrameHost(opener_rfh)));
      }

      WebContentsImpl* outer_contents = current_contents->GetOuterWebContents();
      if (outer_contents)
        current.insert(outer_contents);
    }
  }

  return result;
}

#if defined(OS_ANDROID)
float GetDeviceScaleAdjustment(int min_width) {
  static const float kMinFSM = 1.05f;
  static const int kWidthForMinFSM = 320;
  static const float kMaxFSM = 1.3f;
  static const int kWidthForMaxFSM = 800;

  if (min_width <= kWidthForMinFSM)
    return kMinFSM;
  if (min_width >= kWidthForMaxFSM)
    return kMaxFSM;

  // The font scale multiplier varies linearly between kMinFSM and kMaxFSM.
  float ratio = static_cast<float>(min_width - kWidthForMinFSM) /
                (kWidthForMaxFSM - kWidthForMinFSM);
  return ratio * (kMaxFSM - kMinFSM) + kMinFSM;
}
#endif

// Used to attach the "set of fullscreen contents" to a browser context. Storing
// sets of WebContents on their browser context is done for two reasons. One,
// related WebContentses must necessarily share a browser context, so this saves
// lookup time by restricting to one specific browser context. Two, separating
// by browser context is preemptive paranoia about keeping things separate.
class FullscreenContentsHolder : public base::SupportsUserData::Data {
 public:
  FullscreenContentsHolder() = default;
  ~FullscreenContentsHolder() override = default;

  FullscreenContentsHolder(const FullscreenContentsHolder&) = delete;
  FullscreenContentsHolder& operator=(const FullscreenContentsHolder&) = delete;

  base::flat_set<WebContentsImpl*>* set() { return &set_; }

 private:
  base::flat_set<WebContentsImpl*> set_;
};

const char kFullscreenContentsSet[] = "fullscreen-contents";

base::flat_set<WebContentsImpl*>* FullscreenContentsSet(
    BrowserContext* browser_context) {
  auto* set_holder = static_cast<FullscreenContentsHolder*>(
      browser_context->GetUserData(kFullscreenContentsSet));
  if (!set_holder) {
    auto new_holder = std::make_unique<FullscreenContentsHolder>();
    set_holder = new_holder.get();
    browser_context->SetUserData(kFullscreenContentsSet, std::move(new_holder));
  }

  return set_holder->set();
}

// Returns true if |host| has the Window Placement permission granted.
bool IsWindowPlacementGranted(RenderFrameHost* host) {
  auto* controller =
      PermissionControllerImpl::FromBrowserContext(host->GetBrowserContext());
  return controller && controller->GetPermissionStatusForFrame(
                           PermissionType::WINDOW_PLACEMENT, host,
                           host->GetLastCommittedURL()) ==
                           blink::mojom::PermissionStatus::GRANTED;
}

// Adjust the requested |bounds| for opening or placing a window and return the
// id of the display where the window will be placed. The bounds may not extend
// outside a single screen's work area, and the |host| requires permission to
// specify bounds on a screen other than its current screen.
// TODO(crbug.com/897300): These adjustments are inaccurate for window.open(),
// which specifies the inner content size, and for window.moveTo, resizeTo, etc.
// calls on newly created windows, which may pass empty sizes or positions to
// indicate uninitialized placement information in the renderer. Constraints
// enforced later should resolve most inaccuracies, but this early enforcement
// is needed to ensure bounds indicate the appropriate display.
int64_t AdjustRequestedWindowBounds(gfx::Rect* bounds, RenderFrameHost* host) {
  auto* screen = display::Screen::GetScreen();
  auto display = screen->GetDisplayMatching(*bounds);

  // Check, but do not prompt, for permission to place windows on other screens.
  // Sites generally need permission to get such bounds in the first place.
  // Also clamp offscreen bounds to the window's current screen.
  if (!bounds->Intersects(display.bounds()) || !IsWindowPlacementGranted(host))
    display = screen->GetDisplayNearestView(host->GetNativeView());

  bounds->AdjustToFit(display.work_area());
  return display.id();
}

}  // namespace

CreatedWindow::CreatedWindow() = default;
CreatedWindow::CreatedWindow(std::unique_ptr<WebContentsImpl> contents,
                             GURL target_url)
    : contents(std::move(contents)), target_url(std::move(target_url)) {}
CreatedWindow::~CreatedWindow() = default;
CreatedWindow::CreatedWindow(CreatedWindow&&) = default;
CreatedWindow& CreatedWindow::operator=(CreatedWindow&&) = default;

std::unique_ptr<WebContents> WebContents::Create(
    const WebContents::CreateParams& params) {
  return WebContentsImpl::Create(params);
}

std::unique_ptr<WebContentsImpl> WebContentsImpl::Create(
    const CreateParams& params) {
  return CreateWithOpener(params, FindOpenerRFH(params));
}

std::unique_ptr<WebContents> WebContents::CreateWithSessionStorage(
    const WebContents::CreateParams& params,
    const SessionStorageNamespaceMap& session_storage_namespace_map) {
  OPTIONAL_TRACE_EVENT0("content", "WebContents::CreateWithSessionStorage");
  std::unique_ptr<WebContentsImpl> new_contents(
      new WebContentsImpl(params.browser_context));
  RenderFrameHostImpl* opener_rfh = FindOpenerRFH(params);
  FrameTreeNode* opener = nullptr;
  if (opener_rfh)
    opener = opener_rfh->frame_tree_node();
  new_contents->SetOpenerForNewContents(opener, params.opener_suppressed);

  for (const auto& it : session_storage_namespace_map) {
    new_contents->GetController().SetSessionStorageNamespace(it.first,
                                                             it.second.get());
  }

  WebContentsImpl* outer_web_contents = nullptr;
  if (params.guest_delegate) {
    // This makes |new_contents| act as a guest.
    // For more info, see comment above class BrowserPluginGuest.
    BrowserPluginGuest::CreateInWebContents(new_contents.get(),
                                            params.guest_delegate);
    outer_web_contents = static_cast<WebContentsImpl*>(
        params.guest_delegate->GetOwnerWebContents());
  }

  new_contents->Init(params);
  if (outer_web_contents)
    outer_web_contents->InnerWebContentsCreated(new_contents.get());
  return new_contents;
}

void WebContentsImpl::FriendWrapper::AddCreatedCallbackForTesting(
    const CreatedCallback& callback) {
  g_created_callbacks.Get().push_back(callback);
}

void WebContentsImpl::FriendWrapper::RemoveCreatedCallbackForTesting(
    const CreatedCallback& callback) {
  for (size_t i = 0; i < g_created_callbacks.Get().size(); ++i) {
    if (g_created_callbacks.Get().at(i) == callback) {
      g_created_callbacks.Get().erase(g_created_callbacks.Get().begin() + i);
      return;
    }
  }
}

WebContents* WebContents::FromRenderViewHost(RenderViewHost* rvh) {
  OPTIONAL_TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("content.verbose"),
                        "WebContents::FromRenderViewHost", "render_view_host",
                        rvh);
  if (!rvh)
    return nullptr;
  return rvh->GetDelegate()->GetAsWebContents();
}

WebContents* WebContents::FromRenderFrameHost(RenderFrameHost* rfh) {
  OPTIONAL_TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("content.verbose"),
                        "WebContents::FromRenderFrameHost", "render_frame_host",
                        base::trace_event::ToTracedValue(rfh));
  if (!rfh)
    return nullptr;
  if (!rfh->IsCurrent() && base::FeatureList::IsEnabled(
                               kCheckWebContentsAccessFromNonCurrentFrame)) {
    // TODO(crbug.com/1059903): return nullptr here eventually.
    base::debug::DumpWithoutCrashing();
  }
  return static_cast<RenderFrameHostImpl*>(rfh)->delegate()->GetAsWebContents();
}

WebContents* WebContents::FromFrameTreeNodeId(int frame_tree_node_id) {
  OPTIONAL_TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("content.verbose"),
                        "WebContents::FromFrameTreeNodeId",
                        "frame_tree_node_id", frame_tree_node_id);
  FrameTreeNode* frame_tree_node =
      FrameTreeNode::GloballyFindByID(frame_tree_node_id);
  if (!frame_tree_node)
    return nullptr;
  return WebContentsImpl::FromFrameTreeNode(frame_tree_node);
}

void WebContents::SetScreenOrientationDelegate(
    ScreenOrientationDelegate* delegate) {
  ScreenOrientationProvider::SetDelegate(delegate);
}

// WebContentsImpl::DestructionObserver ----------------------------------------

class WebContentsImpl::DestructionObserver : public WebContentsObserver {
 public:
  DestructionObserver(WebContentsImpl* owner, WebContents* watched_contents)
      : WebContentsObserver(watched_contents), owner_(owner) {}

  // WebContentsObserver:
  void WebContentsDestroyed() override {
    owner_->OnWebContentsDestroyed(
        static_cast<WebContentsImpl*>(web_contents()));
  }

 private:
  WebContentsImpl* owner_;

  DISALLOW_COPY_AND_ASSIGN(DestructionObserver);
};

// TODO(sreejakshetty): Make |WebContentsImpl::ColorChooser| per-frame instead
// of WebContents-owned.
// WebContentsImpl::ColorChooser ----------------------------------------------
class WebContentsImpl::ColorChooser : public blink::mojom::ColorChooser {
 public:
  ColorChooser(content::ColorChooser* chooser,
               mojo::PendingReceiver<blink::mojom::ColorChooser> receiver,
               mojo::PendingRemote<blink::mojom::ColorChooserClient> client)
      : chooser_(chooser),
        receiver_(this, std::move(receiver)),
        client_(std::move(client)) {
    receiver_.set_disconnect_handler(
        base::BindOnce([](content::ColorChooser* chooser) { chooser->End(); },
                       base::Unretained(chooser)));
  }

  ~ColorChooser() override { chooser_->End(); }

  void SetSelectedColor(SkColor color) override {
    OPTIONAL_TRACE_EVENT0("content",
                          "WebContentsImpl::ColorChooser::SetSelectedColor");
    chooser_->SetSelectedColor(color);
  }

  void DidChooseColorInColorChooser(SkColor color) {
    OPTIONAL_TRACE_EVENT0(
        "content",
        "WebContentsImpl::ColorChooser::DidChooseColorInColorChooser");
    client_->DidChooseColor(color);
  }

 private:
  // Color chooser that was opened by this tab.
  std::unique_ptr<content::ColorChooser> chooser_;

  // mojo receiver.
  mojo::Receiver<blink::mojom::ColorChooser> receiver_;

  // mojo renderer client.
  mojo::Remote<blink::mojom::ColorChooserClient> client_;
};

// WebContentsImpl::WebContentsTreeNode ----------------------------------------
WebContentsImpl::WebContentsTreeNode::WebContentsTreeNode(
    WebContentsImpl* current_web_contents)
    : current_web_contents_(current_web_contents),
      outer_web_contents_(nullptr),
      outer_contents_frame_tree_node_id_(
          FrameTreeNode::kFrameTreeNodeInvalidId),
      focused_web_contents_(current_web_contents) {}

WebContentsImpl::WebContentsTreeNode::~WebContentsTreeNode() = default;

std::unique_ptr<WebContents>
WebContentsImpl::WebContentsTreeNode::DisconnectFromOuterWebContents() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsTreeNode::DisconnectFromOuterWebContents");
  std::unique_ptr<WebContents> inner_contents =
      outer_web_contents_->node_.DetachInnerWebContents(current_web_contents_);
  OuterContentsFrameTreeNode()->RemoveObserver(this);
  outer_contents_frame_tree_node_id_ = FrameTreeNode::kFrameTreeNodeInvalidId;
  outer_web_contents_ = nullptr;
  return inner_contents;
}

void WebContentsImpl::WebContentsTreeNode::AttachInnerWebContents(
    std::unique_ptr<WebContents> inner_web_contents,
    RenderFrameHostImpl* render_frame_host) {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsTreeNode::AttachInnerWebContents");
  WebContentsImpl* inner_web_contents_impl =
      static_cast<WebContentsImpl*>(inner_web_contents.get());
  WebContentsTreeNode& inner_web_contents_node = inner_web_contents_impl->node_;

  inner_web_contents_node.focused_web_contents_ = nullptr;
  inner_web_contents_node.outer_web_contents_ = current_web_contents_;
  inner_web_contents_node.outer_contents_frame_tree_node_id_ =
      render_frame_host->frame_tree_node()->frame_tree_node_id();

  inner_web_contents_.push_back(std::move(inner_web_contents));

  render_frame_host->frame_tree_node()->AddObserver(&inner_web_contents_node);
  current_web_contents_->InnerWebContentsAttached(inner_web_contents_impl);
}

std::unique_ptr<WebContents>
WebContentsImpl::WebContentsTreeNode::DetachInnerWebContents(
    WebContentsImpl* inner_web_contents) {
  OPTIONAL_TRACE_EVENT0(
      "content",
      "WebContentsImpl::WebContentsTreeNode::DetachInnerWebContents");
  std::unique_ptr<WebContents> detached_contents;
  for (std::unique_ptr<WebContents>& web_contents : inner_web_contents_) {
    if (web_contents.get() == inner_web_contents) {
      detached_contents = std::move(web_contents);
      std::swap(web_contents, inner_web_contents_.back());
      inner_web_contents_.pop_back();
      current_web_contents_->InnerWebContentsDetached(inner_web_contents);
      return detached_contents;
    }
  }

  NOTREACHED();
  return nullptr;
}

FrameTreeNode*
WebContentsImpl::WebContentsTreeNode::OuterContentsFrameTreeNode() const {
  return FrameTreeNode::GloballyFindByID(outer_contents_frame_tree_node_id_);
}

void WebContentsImpl::WebContentsTreeNode::OnFrameTreeNodeDestroyed(
    FrameTreeNode* node) {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsTreeNode::OnFrameTreeNodeDestroyed");
  DCHECK_EQ(outer_contents_frame_tree_node_id_, node->frame_tree_node_id())
      << "WebContentsTreeNode should only receive notifications for the "
         "FrameTreeNode in its outer WebContents that hosts it.";

  // Deletes |this| too.
  outer_web_contents_->node_.DetachInnerWebContents(current_web_contents_);
}

void WebContentsImpl::WebContentsTreeNode::SetFocusedWebContents(
    WebContentsImpl* web_contents) {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsTreeNode::SetFocusedWebContents");
  DCHECK(!outer_web_contents())
      << "Only the outermost WebContents tracks focus.";
  focused_web_contents_ = web_contents;
}

WebContentsImpl*
WebContentsImpl::WebContentsTreeNode::GetInnerWebContentsInFrame(
    const FrameTreeNode* frame) {
  auto ftn_id = frame->frame_tree_node_id();
  for (auto& contents : inner_web_contents_) {
    WebContentsImpl* impl = static_cast<WebContentsImpl*>(contents.get());
    if (impl->node_.outer_contents_frame_tree_node_id() == ftn_id) {
      return impl;
    }
  }
  return nullptr;
}

std::vector<WebContentsImpl*>
WebContentsImpl::WebContentsTreeNode::GetInnerWebContents() const {
  std::vector<WebContentsImpl*> inner_web_contents;
  for (auto& contents : inner_web_contents_)
    inner_web_contents.push_back(static_cast<WebContentsImpl*>(contents.get()));

  return inner_web_contents;
}

// WebContentsObserverList -----------------------------------------------------
WebContentsImpl::WebContentsObserverList::WebContentsObserverList() = default;
WebContentsImpl::WebContentsObserverList::~WebContentsObserverList() = default;

void WebContentsImpl::WebContentsObserverList::AddObserver(
    WebContentsObserver* observer) {
  observers_.AddObserver(observer);
}

void WebContentsImpl::WebContentsObserverList::RemoveObserver(
    WebContentsObserver* observer) {
  observers_.RemoveObserver(observer);
}

// WebContentsImpl -------------------------------------------------------------

WebContentsImpl::WebContentsImpl(BrowserContext* browser_context)
    : delegate_(nullptr),
      controller_(this, browser_context),
      render_view_host_delegate_view_(nullptr),
      created_with_opener_(false),
      node_(this),
      frame_tree_(&controller_, this, this, this, this, this),
      is_load_to_different_document_(false),
      crashed_status_(base::TERMINATION_STATUS_STILL_RUNNING),
      crashed_error_code_(0),
      waiting_for_response_(false),
      load_state_(net::LOAD_STATE_IDLE, base::string16()),
      upload_size_(0),
      upload_position_(0),
      is_resume_pending_(false),
      has_accessed_initial_document_(false),
      visible_capturer_count_(0),
      hidden_capturer_count_(0),
      is_being_destroyed_(false),
      notify_disconnection_(false),
      dialog_manager_(nullptr),
      is_showing_before_unload_dialog_(false),
      last_active_time_(base::TimeTicks::Now()),
      closed_by_user_gesture_(false),
      minimum_zoom_percent_(
          static_cast<int>(blink::kMinimumPageZoomFactor * 100)),
      maximum_zoom_percent_(
          static_cast<int>(blink::kMaximumPageZoomFactor * 100)),
      zoom_scroll_remainder_(0),
      fullscreen_widget_process_id_(ChildProcessHost::kInvalidUniqueID),
      fullscreen_widget_routing_id_(MSG_ROUTING_NONE),
      fullscreen_widget_had_focus_at_shutdown_(false),
      force_disable_overscroll_content_(false),
      last_dialog_suppressed_(false),
      accessibility_mode_(
          GetContentClient()->browser()->GetAXModeForBrowserContext(
              browser_context)),
      audio_stream_monitor_(this),
      media_web_contents_observer_(
          std::make_unique<MediaWebContentsObserver>(this)),
#if !defined(OS_ANDROID)
      page_scale_factor_is_one_(true),
#endif  // !defined(OS_ANDROID)
      is_overlay_content_(false),
      showing_context_menu_(false),
      text_autosizer_page_info_({0, 0, 1.f}),
      native_theme_observer_(this) {
  TRACE_EVENT0("content", "WebContentsImpl::WebContentsImpl");
  frame_tree_.SetFrameRemoveListener(base::BindRepeating(
      &WebContentsImpl::OnFrameRemoved, base::Unretained(this)));
#if BUILDFLAG(ENABLE_PLUGINS)
  pepper_playback_observer_ = std::make_unique<PepperPlaybackObserver>(this);
#endif

#if defined(OS_ANDROID)
  display_cutout_host_impl_ = std::make_unique<DisplayCutoutHostImpl>(this);
#endif

  ui::NativeTheme* native_theme = ui::NativeTheme::GetInstanceForWeb();
  native_theme_observer_.Add(native_theme);
  using_dark_colors_ = native_theme->ShouldUseDarkColors();
  preferred_color_scheme_ = native_theme->GetPreferredColorScheme();

  screen_change_monitor_ =
      std::make_unique<ScreenChangeMonitor>(base::BindRepeating(
          &WebContentsImpl::OnScreensChange, base::Unretained(this)));

  // ConversionHost takes a weak ref on |this|, so it must be created outside of
  // the initializer list.
  if (base::FeatureList::IsEnabled(features::kConversionMeasurement)) {
    conversion_host_ = std::make_unique<ConversionHost>(this);
  }
}

WebContentsImpl::~WebContentsImpl() {
  TRACE_EVENT0("content", "WebContentsImpl::~WebContentsImpl");

  // Imperfect sanity check against double free, given some crashes unexpectedly
  // observed in the wild.
  CHECK(!is_being_destroyed_);

  // We generally keep track of is_being_destroyed_ to let other features know
  // to avoid certain actions during destruction.
  is_being_destroyed_ = true;

  // A WebContents should never be deleted while it is notifying observers,
  // since this will lead to a use-after-free as it continues to notify later
  // observers.
  CHECK(!observers_.is_notifying_observers());

  FullscreenContentsSet(GetBrowserContext())->erase(this);

  rwh_input_event_router_.reset();

  for (auto& entry : receiver_sets_)
    entry.second->CloseAllReceivers();

  WebContentsImpl* outermost = GetOutermostWebContents();
  if (this != outermost && ContainsOrIsFocusedWebContents()) {
    // If the current WebContents is in focus, unset it.
    outermost->SetAsFocusedWebContentsIfNecessary();
  }

  if (mouse_lock_widget_)
    mouse_lock_widget_->RejectMouseLockOrUnlockIfNecessary(
        blink::mojom::PointerLockResult::kElementDestroyed);

  for (FrameTreeNode* node : frame_tree_.Nodes()) {
    // Delete all RFHs pending shutdown, which will lead the corresponding RVHs
    // to be shutdown and be deleted as well.
    node->render_manager()->ClearRFHsPendingShutdown();
    node->render_manager()->ClearWebUIInstances();
  }

  for (RenderWidgetHostImpl* widget : created_widgets_)
    widget->DetachDelegate();
  created_widgets_.clear();

  // Clear out any JavaScript state.
  if (dialog_manager_) {
    dialog_manager_->CancelDialogs(this, /*reset_state=*/true);
  }

  color_chooser_.reset();
  find_request_manager_.reset();

  NotifyDisconnected();

  // Notify any observer that have a reference on this WebContents.
  NotificationService::current()->Notify(
      NOTIFICATION_WEB_CONTENTS_DESTROYED,
      Source<WebContents>(this),
      NotificationService::NoDetails());

  // Destroy all subframes now. This notifies observers.
  GetMainFrame()->ResetChildren();
  GetRenderManager()->ResetProxyHosts();

  // Manually call the observer methods for the root frame tree node. It is
  // necessary to manually delete all objects tracking navigations
  // (NavigationHandle, NavigationRequest) for observers to be properly
  // notified of these navigations stopping before the WebContents is
  // destroyed.
  RenderFrameHostManager* root = GetRenderManager();

  GetController().GetBackForwardCache().Shutdown();

  root->current_frame_host()->SetRenderFrameCreated(false);
  root->current_frame_host()->ResetNavigationRequests();

  // Do not update state as the WebContents is being destroyed.
  frame_tree_.root()->ResetNavigationRequest(true);
  if (root->speculative_frame_host()) {
    root->speculative_frame_host()->DeleteRenderFrame(
        FrameDeleteIntention::kSpeculativeMainFrameForShutdown);
    root->speculative_frame_host()->SetRenderFrameCreated(false);
    root->speculative_frame_host()->ResetNavigationRequests();
  }

#if BUILDFLAG(ENABLE_PLUGINS)
  // Call this before WebContentsDestroyed() is broadcasted since
  // AudioFocusManager will be destroyed after that.
  pepper_playback_observer_.reset();
#endif  // defined(ENABLED_PLUGINS)

  // If audio is playing then notify external observers of the audio stream
  // disappearing.
  if (is_currently_audible_) {
    is_currently_audible_ = false;
    observers_.ForEachObserver([&](WebContentsObserver* observer) {
      observer->OnAudioStateChanged(false);
    });
    if (GetOuterWebContents())
      GetOuterWebContents()->OnAudioStateChanged();
  }

  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->FrameDeleted(root->current_frame_host());
  });
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->RenderViewDeleted(root->current_host());
  });
#if defined(OS_ANDROID)
  // For simplicity, destroy the Java WebContents before we notify of the
  // destruction of the WebContents.
  ClearWebContentsAndroid();
#endif

  observers_.ForEachObserver(
      [&](WebContentsObserver* observer) { observer->WebContentsDestroyed(); });
  if (display_cutout_host_impl_)
    display_cutout_host_impl_->WebContentsDestroyed();

  observers_.ForEachObserver(
      [&](WebContentsObserver* observer) { observer->ResetWebContents(); });
  SetDelegate(nullptr);
}

std::unique_ptr<WebContentsImpl> WebContentsImpl::CreateWithOpener(
    const WebContents::CreateParams& params,
    RenderFrameHostImpl* opener_rfh) {
  OPTIONAL_TRACE_EVENT1("browser", "WebContentsImpl::CreateWithOpener",
                        "opener", base::trace_event::ToTracedValue(opener_rfh));
  FrameTreeNode* opener = nullptr;
  if (opener_rfh)
    opener = opener_rfh->frame_tree_node();
  std::unique_ptr<WebContentsImpl> new_contents(
      new WebContentsImpl(params.browser_context));
  new_contents->SetOpenerForNewContents(opener, params.opener_suppressed);

  // If the opener is sandboxed, a new popup must inherit the opener's sandbox
  // flags, and these flags take effect immediately.  An exception is if the
  // opener's sandbox flags lack the PropagatesToAuxiliaryBrowsingContexts
  // bit (which is controlled by the "allow-popups-to-escape-sandbox" token).
  // See https://html.spec.whatwg.org/#attr-iframe-sandbox.
  FrameTreeNode* new_root = new_contents->GetFrameTree()->root();
  if (opener) {
    network::mojom::WebSandboxFlags opener_flags =
        opener_rfh->active_sandbox_flags();
    const network::mojom::WebSandboxFlags inherit_flag =
        network::mojom::WebSandboxFlags::kPropagatesToAuxiliaryBrowsingContexts;
    bool sandbox_propagates_to_auxilary_context =
        (opener_flags & inherit_flag) == inherit_flag;
    if (sandbox_propagates_to_auxilary_context)
      new_root->SetPendingFramePolicy({opener_flags,
                                       {} /* container_policy */,
                                       {} /* required_document_policy */});
    if (opener_flags == network::mojom::WebSandboxFlags::kNone ||
        sandbox_propagates_to_auxilary_context) {
      // TODO(ekaramad, iclelland): Do not propagate feature policies from non-
      // sandboxed disowned openers (rel=noopener).
      // If the current page is not sandboxed, or if the sandbox is to propagate
      // to the popups then opener's feature policy will apply to the new popup
      // as well.
      new_root->SetOpenerFeaturePolicyState(
          opener_rfh->feature_policy()->GetFeatureState());
    }

    new_root->SetInitialPopupURL(params.initial_popup_url);
    new_root->SetPopupCreatorOrigin(opener_rfh->GetLastCommittedOrigin());
  }

  // Apply starting sandbox flags.
  blink::FramePolicy frame_policy(new_root->pending_frame_policy());
  frame_policy.sandbox_flags |= params.starting_sandbox_flags;
  new_root->SetPendingFramePolicy(frame_policy);
  new_root->CommitFramePolicy(frame_policy);

  // This may be true even when opener is null, such as when opening blocked
  // popups.
  if (params.created_with_opener)
    new_contents->created_with_opener_ = true;

  WebContentsImpl* outer_web_contents = nullptr;
  if (params.guest_delegate) {
    // This makes |new_contents| act as a guest.
    // For more info, see comment above class BrowserPluginGuest.
    BrowserPluginGuest::CreateInWebContents(new_contents.get(),
                                            params.guest_delegate);
    outer_web_contents = static_cast<WebContentsImpl*>(
        params.guest_delegate->GetOwnerWebContents());
  }

  new_contents->Init(params);
  if (outer_web_contents)
    outer_web_contents->InnerWebContentsCreated(new_contents.get());
  return new_contents;
}

// static
std::vector<WebContentsImpl*> WebContentsImpl::GetAllWebContents() {
  OPTIONAL_TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("content.verbose"),
                        "WebContentsImpl::GetAllWebContents");
  std::vector<WebContentsImpl*> result;
  std::unique_ptr<RenderWidgetHostIterator> widgets(
      RenderWidgetHostImpl::GetRenderWidgetHosts());
  while (RenderWidgetHost* rwh = widgets->GetNextHost()) {
    RenderViewHost* rvh = RenderViewHost::From(rwh);
    if (!rvh)
      continue;
    WebContents* web_contents = WebContents::FromRenderViewHost(rvh);
    if (!web_contents)
      continue;
    if (web_contents->GetRenderViewHost() != rvh)
      continue;
    // Because a WebContents can only have one current RVH at a time, there will
    // be no duplicate WebContents here.
    result.push_back(static_cast<WebContentsImpl*>(web_contents));
  }
  return result;
}

// static
WebContentsImpl* WebContentsImpl::FromFrameTreeNode(
    const FrameTreeNode* frame_tree_node) {
  OPTIONAL_TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("content.verbose"),
                        "WebContentsImpl::FromFrameTreeNode", "frame_tree_node",
                        frame_tree_node);
  return static_cast<WebContentsImpl*>(
      WebContents::FromRenderFrameHost(frame_tree_node->current_frame_host()));
}

// static
WebContents* WebContentsImpl::FromRenderFrameHostID(
    GlobalFrameRoutingId render_frame_host_id) {
  OPTIONAL_TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("content.verbose"),
                        "WebContentsImpl::FromRenderFrameHostID", "process_id",
                        render_frame_host_id.child_id, "frame_id",
                        render_frame_host_id.frame_routing_id);
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         !BrowserThread::IsThreadInitialized(BrowserThread::UI));
  RenderFrameHost* render_frame_host =
      RenderFrameHost::FromID(render_frame_host_id);
  if (!render_frame_host)
    return nullptr;

  return WebContents::FromRenderFrameHost(render_frame_host);
}

// static
WebContents* WebContentsImpl::FromRenderFrameHostID(int render_process_host_id,
                                                    int render_frame_host_id) {
  return FromRenderFrameHostID(
      GlobalFrameRoutingId(render_process_host_id, render_frame_host_id));
}

// static
WebContentsImpl* WebContentsImpl::FromOuterFrameTreeNode(
    const FrameTreeNode* frame_tree_node) {
  OPTIONAL_TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("content.verbose"),
                        "WebContentsImpl::FromOuterFrameTreeNode",
                        "frame_tree_node", frame_tree_node);
  return WebContentsImpl::FromFrameTreeNode(frame_tree_node)
      ->node_.GetInnerWebContentsInFrame(frame_tree_node);
}

RenderFrameHostManager* WebContentsImpl::GetRenderManagerForTesting() {
  return GetRenderManager();
}

bool WebContentsImpl::OnMessageReceived(RenderViewHostImpl* render_view_host,
                                        const IPC::Message& message) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::OnMessageReceived",
                        "render_view_host", render_view_host);
  for (auto& observer : observers_.observer_list()) {
    // TODO(nick, creis): https://crbug.com/758026: Replace all uses of this
    // variant of OnMessageReceived with the version that takes a
    // RenderFrameHost, and then delete it.
    if (observer.OnMessageReceived(message))
      return true;
  }

#if BUILDFLAG(ENABLE_PLUGINS)
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(WebContentsImpl, message, render_view_host)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RequestPpapiBrokerPermission,
                        OnRequestPpapiBrokerPermission)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
#else
  return false;
#endif
}

bool WebContentsImpl::OnMessageReceived(RenderFrameHostImpl* render_frame_host,
                                        const IPC::Message& message) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::OnMessageReceived",
                        "render_frame_host",
                        base::trace_event::ToTracedValue(render_frame_host));

  for (auto& observer : observers_.observer_list()) {
    if (observer.OnMessageReceived(message, render_frame_host))
      return true;
  }

  bool handled = true;
#if BUILDFLAG(ENABLE_PLUGINS)
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(WebContentsImpl, message, render_frame_host)
    IPC_MESSAGE_HANDLER(FrameHostMsg_PepperInstanceCreated,
                        OnPepperInstanceCreated)
    IPC_MESSAGE_HANDLER(FrameHostMsg_PepperInstanceDeleted,
                        OnPepperInstanceDeleted)
    IPC_MESSAGE_HANDLER(FrameHostMsg_PepperPluginHung, OnPepperPluginHung)
    IPC_MESSAGE_HANDLER(FrameHostMsg_PepperStartsPlayback,
                        OnPepperStartsPlayback)
    IPC_MESSAGE_HANDLER(FrameHostMsg_PepperStopsPlayback,
                        OnPepperStopsPlayback)
    IPC_MESSAGE_HANDLER(FrameHostMsg_PluginCrashed, OnPluginCrashed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
#else
  handled = false;
#endif

  return handled;
}

NavigationControllerImpl& WebContentsImpl::GetController() {
  return controller_;
}

BrowserContext* WebContentsImpl::GetBrowserContext() {
  return controller_.GetBrowserContext();
}

const GURL& WebContentsImpl::GetURL() {
  return GetVisibleURL();
}

const GURL& WebContentsImpl::GetVisibleURL() {
  // We may not have a navigation entry yet.
  NavigationEntry* entry = controller_.GetVisibleEntry();
  return entry ? entry->GetVirtualURL() : GURL::EmptyGURL();
}

const GURL& WebContentsImpl::GetLastCommittedURL() {
  // We may not have a navigation entry yet.
  NavigationEntry* entry = controller_.GetLastCommittedEntry();
  return entry ? entry->GetVirtualURL() : GURL::EmptyGURL();
}

WebContentsDelegate* WebContentsImpl::GetDelegate() {
  return delegate_;
}

void WebContentsImpl::SetDelegate(WebContentsDelegate* delegate) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::SetDelegate", "delegate",
                        delegate);
  // TODO(cbentzel): remove this debugging code?
  if (delegate == delegate_)
    return;
  if (delegate_)
    delegate_->Detach(this);
  delegate_ = delegate;
  if (delegate_) {
    delegate_->Attach(this);
    // Ensure the visible RVH reflects the new delegate's preferences.
    if (view_)
      view_->SetOverscrollControllerEnabled(CanOverscrollContent());
    if (GetRenderViewHost())
      RenderFrameDevToolsAgentHost::WebContentsCreated(this);
  }
}

RenderFrameHostImpl* WebContentsImpl::GetMainFrame() {
  return frame_tree_.root()->current_frame_host();
}

RenderFrameHostImpl* WebContentsImpl::GetFocusedFrame() {
  FrameTreeNode* focused_node = frame_tree_.GetFocusedFrame();
  if (!focused_node)
    return nullptr;
  return focused_node->current_frame_host();
}

RenderFrameHostImpl* WebContentsImpl::FindFrameByFrameTreeNodeId(
    int frame_tree_node_id,
    int process_id) {
  OPTIONAL_TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("content.verbose"),
                        "WebContentsImpl::FindFrameByFrameTreeNodeId",
                        "frame_tree_node_id", frame_tree_node_id, "process_id",
                        process_id);
  FrameTreeNode* frame = frame_tree_.FindByID(frame_tree_node_id);

  // Sanity check that this is in the caller's expected process. Otherwise a
  // recent cross-process navigation may have led to a privilege change that the
  // caller is not expecting.
  if (!frame ||
      frame->current_frame_host()->GetProcess()->GetID() != process_id)
    return nullptr;

  return frame->current_frame_host();
}

RenderFrameHostImpl* WebContentsImpl::UnsafeFindFrameByFrameTreeNodeId(
    int frame_tree_node_id) {
  OPTIONAL_TRACE_EVENT1("content",
                        "WebContentsImpl::UnsafeFindFrameByFrameTreeNodeId",
                        "frame_tree_node_id", frame_tree_node_id);
  // Beware using this! The RenderFrameHost may have changed since the caller
  // obtained frame_tree_node_id.
  FrameTreeNode* frame = frame_tree_.FindByID(frame_tree_node_id);
  return frame ? frame->current_frame_host() : nullptr;
}

void WebContentsImpl::ForEachFrame(
    const base::RepeatingCallback<void(RenderFrameHost*)>& on_frame) {
  OPTIONAL_TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("content.verbose"),
                        "WebContentsImpl::ForEachFrame");
  for (FrameTreeNode* node : frame_tree_.Nodes()) {
    on_frame.Run(node->current_frame_host());
  }
}

std::vector<RenderFrameHost*> WebContentsImpl::GetAllFrames() {
  return GetAllFramesImpl(frame_tree_, /*include_pending=*/false);
}

std::vector<RenderFrameHost*> WebContentsImpl::GetAllFramesIncludingPending() {
  return GetAllFramesImpl(frame_tree_, /*include_pending=*/true);
}

int WebContentsImpl::SendToAllFrames(IPC::Message* message) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::SendToAllFrames");
  return SendToAllFramesImpl(frame_tree_, /*include_pending=*/false, message);
}

int WebContentsImpl::SendToAllFramesIncludingPending(IPC::Message* message) {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::SentToAllFramesIncludingPending");
  return SendToAllFramesImpl(frame_tree_, /*include_pending=*/true, message);
}

void WebContentsImpl::SendPageMessage(IPC::Message* msg) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::SendPageMessage");
  frame_tree_.root()->render_manager()->SendPageMessage(msg, nullptr);
}

void WebContentsImpl::ExecutePageBroadcastMethod(
    PageBroadcastMethodCallback callback) {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::ExecutePageBroadcastMethod");
  frame_tree_.root()->render_manager()->ExecutePageBroadcastMethod(callback);
}

RenderViewHostImpl* WebContentsImpl::GetRenderViewHost() {
  return GetRenderManager()->current_host();
}

void WebContentsImpl::CancelActiveAndPendingDialogs() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::CancelActiveAndPendingDialogs");
  if (dialog_manager_) {
    dialog_manager_->CancelDialogs(this, /*reset_state=*/false);
  }
  if (browser_plugin_embedder_)
    browser_plugin_embedder_->CancelGuestDialogs();
}

void WebContentsImpl::ClosePage() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::ClosePage");
  GetRenderViewHost()->ClosePage();
}

RenderWidgetHostView* WebContentsImpl::GetRenderWidgetHostView() {
  return GetRenderManager()->GetRenderWidgetHostView();
}

RenderWidgetHostView* WebContentsImpl::GetTopLevelRenderWidgetHostView() {
  if (GetOuterWebContents())
    return GetOuterWebContents()->GetTopLevelRenderWidgetHostView();
  return GetRenderManager()->GetRenderWidgetHostView();
}

RenderWidgetHostView* WebContentsImpl::GetFullscreenRenderWidgetHostView() {
  if (auto* widget_host = GetFullscreenRenderWidgetHost())
    return widget_host->GetView();
  return nullptr;
}

WebContentsView* WebContentsImpl::GetView() const {
  return view_.get();
}

void WebContentsImpl::OnScreensChange(bool is_multi_screen_changed) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::OnScreensChange",
                        "is_multi_screen_changed", is_multi_screen_changed);
  // Allow fullscreen requests shortly after user-generated screens changes.
  transient_allow_fullscreen_.Activate();
  // Send |is_multi_screen_changed| events to all visible frames, but limit
  // other events to frames with the Window Placement permission. This obviates
  // the most pressing need for sites to poll isMultiScreen(), which is exposed
  // without explicit permission, while also protecting privacy.
  // TODO(crbug.com/1109989): Postpone events; refine utility/privacy balance.
  for (FrameTreeNode* node : frame_tree_.Nodes()) {
    RenderFrameHostImpl* rfh = node->current_frame_host();
    if ((is_multi_screen_changed &&
         rfh->GetVisibilityState() == PageVisibilityState::kVisible) ||
        IsWindowPlacementGranted(rfh)) {
      rfh->GetAssociatedLocalFrame()->OnScreensChange();
    }
  }
}

void WebContentsImpl::OnScreenOrientationChange() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::OnScreenOrientationChange");
  DCHECK(screen_orientation_provider_);
  DidChangeScreenOrientation();
  screen_orientation_provider_->OnOrientationChange();
}

base::Optional<SkColor> WebContentsImpl::GetThemeColor() {
  return GetRenderViewHost()->theme_color();
}

base::Optional<SkColor> WebContentsImpl::GetBackgroundColor() {
  return GetRenderViewHost()->background_color();
}

void WebContentsImpl::SetAccessibilityMode(ui::AXMode mode) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::SetAccessibilityMode",
                        "mode", mode.ToString(), "previous_mode",
                        accessibility_mode_.ToString());

  if (mode == accessibility_mode_)
    return;

  // Don't allow accessibility to be enabled for WebContents that are never
  // user-visible, like background pages.
  if (IsNeverComposited())
    return;

  accessibility_mode_ = mode;

  for (FrameTreeNode* node : frame_tree_.Nodes()) {
    UpdateAccessibilityModeOnFrame(node->current_frame_host());
    // Also update accessibility mode on the speculative RenderFrameHost for
    // this FrameTreeNode, if one exists.
    RenderFrameHost* speculative_frame_host =
        node->render_manager()->speculative_frame_host();
    if (speculative_frame_host)
      UpdateAccessibilityModeOnFrame(speculative_frame_host);
  }
}

void WebContentsImpl::AddAccessibilityMode(ui::AXMode mode) {
  ui::AXMode new_mode(accessibility_mode_);
  new_mode |= mode;
  SetAccessibilityMode(new_mode);
}

// Helper class used by WebContentsImpl::RequestAXTreeSnapshot.
// Handles the callbacks from parallel snapshot requests to each frame,
// and feeds the results to an AXTreeCombiner, which converts them into a
// single combined accessibility tree.
class WebContentsImpl::AXTreeSnapshotCombiner
    : public base::RefCounted<AXTreeSnapshotCombiner> {
 public:
  explicit AXTreeSnapshotCombiner(AXTreeSnapshotCallback callback)
      : callback_(std::move(callback)) {}

  AXTreeSnapshotCallback AddFrame(bool is_root) {
    // Adds a reference to |this|.
    return base::BindOnce(&AXTreeSnapshotCombiner::ReceiveSnapshot, this,
                          is_root);
  }

  void ReceiveSnapshot(bool is_root, const ui::AXTreeUpdate& snapshot) {
    combiner_.AddTree(snapshot, is_root);
  }

 private:
  friend class base::RefCounted<AXTreeSnapshotCombiner>;

  // This is called automatically after the last call to ReceiveSnapshot
  // when there are no more references to this object.
  ~AXTreeSnapshotCombiner() {
    combiner_.Combine();
    std::move(callback_).Run(combiner_.combined());
  }

  ui::AXTreeCombiner combiner_;
  AXTreeSnapshotCallback callback_;
};

void WebContentsImpl::RequestAXTreeSnapshot(AXTreeSnapshotCallback callback,
                                            ui::AXMode ax_mode) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::RequestAXTreeSnapshot",
                        "mode", ax_mode.ToString());
  // Send a request to each of the frames in parallel. Each one will return
  // an accessibility tree snapshot, and AXTreeSnapshotCombiner will combine
  // them into a single tree and call |callback| with that result, then
  // delete |combiner|.
  FrameTreeNode* root_node = frame_tree_.root();
  auto combiner =
      base::MakeRefCounted<AXTreeSnapshotCombiner>(std::move(callback));

  RecursiveRequestAXTreeSnapshotOnFrame(root_node, combiner.get(), ax_mode);
}

void WebContentsImpl::RecursiveRequestAXTreeSnapshotOnFrame(
    FrameTreeNode* root_node,
    AXTreeSnapshotCombiner* combiner,
    ui::AXMode ax_mode) {
  OPTIONAL_TRACE_EVENT0(
      "content", "WebContentsImpl::RecursiveRequestAXTreeSnapshotOnFrame");
  for (FrameTreeNode* frame_tree_node : frame_tree_.Nodes()) {
    WebContentsImpl* inner_contents =
        node_.GetInnerWebContentsInFrame(frame_tree_node);
    if (inner_contents) {
      inner_contents->RecursiveRequestAXTreeSnapshotOnFrame(root_node, combiner,
                                                            ax_mode);
    } else {
      bool is_root = frame_tree_node == root_node;
      frame_tree_node->current_frame_host()->RequestAXTreeSnapshot(
          combiner->AddFrame(is_root), ax_mode);
    }
  }
}

void WebContentsImpl::NotifyViewportFitChanged(
    blink::mojom::ViewportFit value) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::NotifyViewportFitChanged",
                        "value", static_cast<int>(value));
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->ViewportFitChanged(value);
  });
}

FindRequestManager* WebContentsImpl::GetFindRequestManagerForTesting() {
  return GetFindRequestManager();
}

#if !defined(OS_ANDROID)
void WebContentsImpl::UpdateZoom() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::UpdateZoom");
  RenderWidgetHostImpl* rwh = GetRenderViewHost()->GetWidget();
  if (rwh->GetView())
    rwh->SynchronizeVisualProperties();
}


void WebContentsImpl::UpdateZoomIfNecessary(const std::string& scheme,
                                            const std::string& host) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::UpdateZoomIfNecessary",
                        "scheme", scheme, "host", host);
  NavigationEntry* entry = GetController().GetLastCommittedEntry();
  if (!entry)
    return;

  GURL url = HostZoomMap::GetURLFromEntry(entry);
  if (host != net::GetHostOrSpecFromURL(url) ||
      (!scheme.empty() && !url.SchemeIs(scheme))) {
    return;
  }

  UpdateZoom();
}
#endif  // !defined(OS_ANDROID)

base::OnceClosure WebContentsImpl::AddReceiverSet(
    const std::string& interface_name,
    WebContentsReceiverSet* receiver_set) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::AddReceiverSet",
                        "interface_name", interface_name);
  auto result =
      receiver_sets_.insert(std::make_pair(interface_name, receiver_set));
  DCHECK(result.second);
  return base::BindOnce(&WebContentsImpl::RemoveReceiverSet,
                        weak_factory_.GetWeakPtr(), interface_name);
}

WebContentsReceiverSet* WebContentsImpl::GetReceiverSet(
    const std::string& interface_name) {
  auto it = receiver_sets_.find(interface_name);
  if (it == receiver_sets_.end())
    return nullptr;
  return it->second;
}

void WebContentsImpl::RemoveReceiverSetForTesting(
    const std::string& interface_name) {
  OPTIONAL_TRACE_EVENT1("content",
                        "WebContentsImpl::RemoveReceiverSetForTesting",
                        "interface_name", interface_name);
  RemoveReceiverSet(interface_name);
}

std::vector<WebContentsImpl*> WebContentsImpl::GetWebContentsAndAllInner() {
  std::vector<WebContentsImpl*> all_contents(1, this);

  for (size_t i = 0; i != all_contents.size(); ++i) {
    for (auto* inner_contents : all_contents[i]->GetInnerWebContents()) {
      all_contents.push_back(static_cast<WebContentsImpl*>(inner_contents));
    }
  }

  return all_contents;
}

void WebContentsImpl::NotifyManifestUrlChanged(
    RenderFrameHost* rfh,
    const base::Optional<GURL>& manifest_url) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::NotifyManifestUrlChanged",
                        "render_frame_host",
                        base::trace_event::ToTracedValue(rfh), "manifest_url",
                        base::trace_event::ValueToString(manifest_url));
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->DidUpdateWebManifestURL(rfh, manifest_url);
  });
}

WebUI* WebContentsImpl::GetWebUI() {
  WebUI* committed_web_ui = GetCommittedWebUI();
  if (committed_web_ui)
    return committed_web_ui;

  if (GetRenderManager()->speculative_frame_host())
    return GetRenderManager()->speculative_frame_host()->web_ui();

  return nullptr;
}

WebUI* WebContentsImpl::GetCommittedWebUI() {
  return frame_tree_.root()->current_frame_host()->web_ui();
}

void WebContentsImpl::SetUserAgentOverride(
    const blink::UserAgentOverride& ua_override,
    bool override_in_new_tabs) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::SetUserAgentOverride",
                        "ua_override", ua_override.ua_string_override,
                        "override_in_new_tabs", override_in_new_tabs);
  DCHECK(!ua_override.ua_metadata_override.has_value() ||
         !ua_override.ua_string_override.empty());

  if (GetUserAgentOverride() == ua_override)
    return;

  should_override_user_agent_in_new_tabs_ = override_in_new_tabs;

  renderer_preferences_.user_agent_override = ua_override;

  // Send the new override string to all renderers in the current page.
  SyncRendererPrefs();

  // Reload the page if a load is currently in progress to avoid having
  // different parts of the page loaded using different user agents.
  // No need to reload if the current entry matches that of the
  // NavigationRequest supplied to DidStartNavigation() as NavigationRequest
  // handles it.
  NavigationEntry* entry = controller_.GetVisibleEntry();
  if (IsLoading() && entry != nullptr && entry->GetIsOverridingUserAgent() &&
      (!frame_tree_.root()->navigation_request() ||
       frame_tree_.root()->navigation_request()->ua_change_requires_reload())) {
    controller_.Reload(ReloadType::BYPASSING_CACHE, true);
  }

  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->UserAgentOverrideSet(ua_override);
  });
}

void WebContentsImpl::SetRendererInitiatedUserAgentOverrideOption(
    NavigationController::UserAgentOverrideOption option) {
  OPTIONAL_TRACE_EVENT0(
      "content",
      "WebContentsImpl::SetRendererInitiatedUserAgentOverrideOption");
  renderer_initiated_user_agent_override_option_ = option;
}

const blink::UserAgentOverride& WebContentsImpl::GetUserAgentOverride() {
  return renderer_preferences_.user_agent_override;
}

bool WebContentsImpl::ShouldOverrideUserAgentForRendererInitiatedNavigation() {
  NavigationEntryImpl* current_entry = controller_.GetLastCommittedEntry();
  if (!current_entry)
    return should_override_user_agent_in_new_tabs_;

  switch (renderer_initiated_user_agent_override_option_) {
    case NavigationController::UA_OVERRIDE_INHERIT:
      return current_entry->GetIsOverridingUserAgent();
    case NavigationController::UA_OVERRIDE_TRUE:
      return true;
    case NavigationController::UA_OVERRIDE_FALSE:
      return false;
    default:
      break;
  }
  return false;
}

void WebContentsImpl::EnableWebContentsOnlyAccessibilityMode() {
  OPTIONAL_TRACE_EVENT0(
      "content", "WebContentsImpl::EnableWebContentsOnlyAccessibilityMode");
  // If accessibility is already enabled, we'll need to force a reset
  // in order to ensure new observers of accessibility events get the
  // full accessibility tree from scratch.
  bool need_reset = GetAccessibilityMode().has_mode(ui::AXMode::kWebContents);

  ui::AXMode desired_mode =
      GetContentClient()->browser()->GetAXModeForBrowserContext(
          GetBrowserContext());
  desired_mode |= ui::kAXModeWebContentsOnly;
  AddAccessibilityMode(desired_mode);

  if (need_reset) {
    for (RenderFrameHost* rfh : GetAllFrames())
      ResetAccessibility(rfh);
  }
}

bool WebContentsImpl::IsWebContentsOnlyAccessibilityModeForTesting() {
  return accessibility_mode_ == ui::kAXModeWebContentsOnly;
}

bool WebContentsImpl::IsFullAccessibilityModeForTesting() {
  return accessibility_mode_ == ui::kAXModeComplete;
}

#if defined(OS_ANDROID)

void WebContentsImpl::SetDisplayCutoutSafeArea(gfx::Insets insets) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::SetDisplayCutoutSafeArea");
  if (display_cutout_host_impl_)
    display_cutout_host_impl_->SetDisplayCutoutSafeArea(insets);
}

#endif

const base::string16& WebContentsImpl::GetTitle() {
  WebUI* our_web_ui =
      GetRenderManager()->speculative_frame_host()
          ? GetRenderManager()->speculative_frame_host()->web_ui()
          : GetRenderManager()->current_frame_host()->web_ui();
  if (our_web_ui) {
    // Don't override the title in view source mode.
    NavigationEntry* entry = controller_.GetVisibleEntry();
    if (!(entry && entry->IsViewSourceMode())) {
      // Give the Web UI the chance to override our title.
      const base::string16& title = our_web_ui->GetOverriddenTitle();
      if (!title.empty())
        return title;
    }
  }

  // We use the title for the last committed entry rather than a pending
  // navigation entry. For example, when the user types in a URL, we want to
  // keep the old page's title until the new load has committed and we get a new
  // title.
  NavigationEntry* entry = controller_.GetLastCommittedEntry();

  // We make an exception for initial navigations. We only want to use the title
  // from the visible entry if:
  // 1. The pending entry has been explicitly assigned a title to display.
  // 2. The user is doing a history navigation in a new tab (e.g., Ctrl+Back),
  //    which case there is a pending entry index other than -1.
  //
  // Otherwise, we want to stick with the last committed entry's title during
  // new navigations, which have pending entries at index -1 with no title.
  if (controller_.IsInitialNavigation() &&
      ((controller_.GetVisibleEntry() &&
        !controller_.GetVisibleEntry()->GetTitle().empty()) ||
       controller_.GetPendingEntryIndex() != -1)) {
    entry = controller_.GetVisibleEntry();
  }

  if (entry) {
    return entry->GetTitleForDisplay();
  }

  // |page_title_when_no_navigation_entry_| is finally used
  // if no title cannot be retrieved.
  return page_title_when_no_navigation_entry_;
}

SiteInstanceImpl* WebContentsImpl::GetSiteInstance() {
  return GetRenderManager()->current_host()->GetSiteInstance();
}

bool WebContentsImpl::IsLoading() {
  return frame_tree_.IsLoading();
}

double WebContentsImpl::GetLoadProgress() {
  return frame_tree_.load_progress();
}

bool WebContentsImpl::IsLoadingToDifferentDocument() {
  return IsLoading() && is_load_to_different_document_;
}

bool WebContentsImpl::IsDocumentOnLoadCompletedInMainFrame() {
  return GetRenderViewHost()->IsDocumentOnLoadCompletedInMainFrame();
}

bool WebContentsImpl::IsWaitingForResponse() {
  NavigationRequest* ongoing_navigation_request =
      frame_tree_.root()->navigation_request();

  // An ongoing navigation request means we're waiting for a response.
  return ongoing_navigation_request != nullptr;
}

const net::LoadStateWithParam& WebContentsImpl::GetLoadState() {
  return load_state_;
}

const base::string16& WebContentsImpl::GetLoadStateHost() {
  return load_state_host_;
}

uint64_t WebContentsImpl::GetUploadSize() {
  return upload_size_;
}

uint64_t WebContentsImpl::GetUploadPosition() {
  return upload_position_;
}

const std::string& WebContentsImpl::GetEncoding() {
  return GetMainFrame()->GetEncoding();
}

bool WebContentsImpl::WasDiscarded() {
  return GetFrameTree()->root()->was_discarded();
}

void WebContentsImpl::SetWasDiscarded(bool was_discarded) {
  GetFrameTree()->root()->set_was_discarded();
}

void WebContentsImpl::IncrementCapturerCount(const gfx::Size& capture_size,
                                             bool stay_hidden) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::IncrementCapturerCount");
  DCHECK(!is_being_destroyed_);
  if (stay_hidden) {
    // A hidden capture should not have side effect on the web contents, so it
    // should not pass a non-empty |capture_size| which will cause side effect.
    DCHECK(capture_size.IsEmpty());
    ++hidden_capturer_count_;
  } else {
    ++visible_capturer_count_;
  }

  // Note: This provides a hint to upstream code to size the views optimally
  // for quality (e.g., to avoid scaling).
  if (!capture_size.IsEmpty() && preferred_size_for_capture_.IsEmpty()) {
    preferred_size_for_capture_ = capture_size;
    OnPreferredSizeChanged(preferred_size_);
  }

  UpdateVisibilityAndNotifyPageAndView(GetVisibility());
}

void WebContentsImpl::DecrementCapturerCount(bool stay_hidden) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::DecrementCapturerCount");
  if (stay_hidden)
    --hidden_capturer_count_;
  else
    --visible_capturer_count_;
  DCHECK_GE(hidden_capturer_count_, 0);
  DCHECK_GE(visible_capturer_count_, 0);

  if (is_being_destroyed_)
    return;

  if (!IsBeingCaptured()) {
    const gfx::Size old_size = preferred_size_for_capture_;
    preferred_size_for_capture_ = gfx::Size();
    OnPreferredSizeChanged(old_size);
  }

  UpdateVisibilityAndNotifyPageAndView(GetVisibility());
}

bool WebContentsImpl::IsBeingCaptured() {
  return visible_capturer_count_ + hidden_capturer_count_ > 0;
}

bool WebContentsImpl::IsBeingVisiblyCaptured() {
  return visible_capturer_count_ > 0;
}

bool WebContentsImpl::IsAudioMuted() {
  return audio_stream_factory_ && audio_stream_factory_->IsMuted();
}

void WebContentsImpl::SetAudioMuted(bool mute) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::SetAudioMuted", "mute",
                        mute);
  DVLOG(1) << "SetAudioMuted(mute=" << mute << "), was " << IsAudioMuted()
           << " for WebContentsImpl@" << this;

  if (mute == IsAudioMuted())
    return;

  GetAudioStreamFactory()->SetMuted(mute);

  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->DidUpdateAudioMutingState(mute);
  });
  // Notification for UI updates in response to the changed muting state.
  NotifyNavigationStateChanged(INVALIDATE_TYPE_AUDIO);
}

bool WebContentsImpl::IsCurrentlyAudible() {
  return is_currently_audible_;
}

bool WebContentsImpl::IsConnectedToBluetoothDevice() {
  return bluetooth_connected_device_count_ > 0;
}

bool WebContentsImpl::IsScanningForBluetoothDevices() {
  return bluetooth_scanning_sessions_count_ > 0;
}

bool WebContentsImpl::IsConnectedToSerialPort() {
  return serial_active_frame_count_ > 0;
}

bool WebContentsImpl::IsConnectedToHidDevice() {
  return hid_active_frame_count_ > 0;
}

bool WebContentsImpl::HasNativeFileSystemHandles() {
  return native_file_system_handle_count_ > 0;
}

bool WebContentsImpl::HasPictureInPictureVideo() {
  return has_picture_in_picture_video_;
}

void WebContentsImpl::SetHasPictureInPictureVideo(
    bool has_picture_in_picture_video) {
  OPTIONAL_TRACE_EVENT1("content",
                        "WebContentsImpl::SetHasPictureInPictureVideo",
                        "has_pip_video", has_picture_in_picture_video);
  // If status of |this| is already accurate, there is no need to update.
  if (has_picture_in_picture_video == has_picture_in_picture_video_)
    return;
  has_picture_in_picture_video_ = has_picture_in_picture_video;
  NotifyNavigationStateChanged(INVALIDATE_TYPE_TAB);
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->MediaPictureInPictureChanged(has_picture_in_picture_video_);
  });
}

bool WebContentsImpl::IsCrashed() {
  switch (crashed_status_) {
    case base::TERMINATION_STATUS_PROCESS_CRASHED:
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
    case base::TERMINATION_STATUS_OOM:
    case base::TERMINATION_STATUS_LAUNCH_FAILED:
#if defined(OS_CHROMEOS) || BUILDFLAG(IS_LACROS)
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM:
#endif
#if defined(OS_ANDROID)
    case base::TERMINATION_STATUS_OOM_PROTECTED:
#endif
#if defined(OS_WIN)
    case base::TERMINATION_STATUS_INTEGRITY_FAILURE:
#endif
      return true;
    case base::TERMINATION_STATUS_NORMAL_TERMINATION:
    case base::TERMINATION_STATUS_STILL_RUNNING:
      return false;
    case base::TERMINATION_STATUS_MAX_ENUM:
      NOTREACHED();
      return false;
  }

  NOTREACHED();
  return false;
}

void WebContentsImpl::SetIsCrashed(base::TerminationStatus status,
                                   int error_code) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::SetIsCrashed", "status",
                        static_cast<int>(status), "old_status",
                        static_cast<int>(crashed_status_));
  if (status == crashed_status_)
    return;

  crashed_status_ = status;
  crashed_error_code_ = error_code;
  NotifyNavigationStateChanged(INVALIDATE_TYPE_TAB);
}

base::TerminationStatus WebContentsImpl::GetCrashedStatus() {
  return crashed_status_;
}

int WebContentsImpl::GetCrashedErrorCode() {
  return crashed_error_code_;
}

bool WebContentsImpl::IsBeingDestroyed() {
  return is_being_destroyed_;
}

void WebContentsImpl::NotifyNavigationStateChanged(
    InvalidateTypes changed_flags) {
  TRACE_EVENT1("content,navigation",
               "WebContentsImpl::NotifyNavigationStateChanged", "changed_flags",
               static_cast<int>(changed_flags));
  // Notify the media observer of potential audibility changes.
  if (changed_flags & INVALIDATE_TYPE_AUDIO) {
    media_web_contents_observer_->MaybeUpdateAudibleState();
  }

  if (delegate_)
    delegate_->NavigationStateChanged(this, changed_flags);

  if (GetOuterWebContents())
    GetOuterWebContents()->NotifyNavigationStateChanged(changed_flags);
}

RenderFrameHostImpl* WebContentsImpl::GetFocusedFrameFromFocusedDelegate() {
  FrameTreeNode* focused_node =
      GetFocusedWebContents()->frame_tree_.GetFocusedFrame();
  return focused_node ? focused_node->current_frame_host() : nullptr;
}

void WebContentsImpl::OnVerticalScrollDirectionChanged(
    viz::VerticalScrollDirection scroll_direction) {
  OPTIONAL_TRACE_EVENT1("content",
                        "WebContentsImpl::OnVerticalScrollDirectionChanged",
                        "scroll_direction", static_cast<int>(scroll_direction));
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->DidChangeVerticalScrollDirection(scroll_direction);
  });
}

void WebContentsImpl::OnAudioStateChanged() {
  // This notification can come from any embedded contents or from this
  // WebContents' stream monitor. Aggregate these signals to get the actual
  // state.
  //
  // Note that guests may not be attached as inner contents, and so may need to
  // be checked separately.
  bool is_currently_audible =
      audio_stream_monitor_.IsCurrentlyAudible() ||
      (browser_plugin_embedder_ &&
       browser_plugin_embedder_->AreAnyGuestsCurrentlyAudible()) ||
      AnyInnerWebContents(this, [](WebContents* inner_contents) {
        return inner_contents->IsCurrentlyAudible();
      });
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::OnAudioStateChanged",
                        "is_currently_audible", is_currently_audible,
                        "was_audible", is_currently_audible_);
  if (is_currently_audible == is_currently_audible_)
    return;

  // Update internal state.
  is_currently_audible_ = is_currently_audible;
  was_ever_audible_ = was_ever_audible_ || is_currently_audible_;

  ExecutePageBroadcastMethod(base::BindRepeating(
      [](bool is_currently_audible, RenderViewHostImpl* rvh) {
        if (auto& broadcast = rvh->GetAssociatedPageBroadcast())
          broadcast->AudioStateChanged(is_currently_audible);
      },
      is_currently_audible_));

  // Notification for UI updates in response to the changed audio state.
  NotifyNavigationStateChanged(INVALIDATE_TYPE_AUDIO);

  // Ensure that audio state changes propagate from innermost to outermost
  // WebContents.
  if (GetOuterWebContents())
    GetOuterWebContents()->OnAudioStateChanged();

  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->OnAudioStateChanged(is_currently_audible_);
  });
}

base::TimeTicks WebContentsImpl::GetLastActiveTime() {
  return last_active_time_;
}

void WebContentsImpl::WasShown() {
  TRACE_EVENT0("content", "WebContentsImpl::WasShown");
  UpdateVisibilityAndNotifyPageAndView(Visibility::VISIBLE);
}

void WebContentsImpl::WasHidden() {
  TRACE_EVENT0("content", "WebContentsImpl::WasHidden");
  UpdateVisibilityAndNotifyPageAndView(Visibility::HIDDEN);
}

bool WebContentsImpl::HasRecentInteractiveInputEvent() {
  static constexpr base::TimeDelta kMaxInterval =
      base::TimeDelta::FromSeconds(5);
  base::TimeDelta delta =
      ui::EventTimeForNow() - last_interactive_input_event_time_;
  // Note: the expectation is that the caller is typically expecting an input
  // event, e.g. validating that a WebUI message that requires a gesture is
  // actually attached to a gesture.
  return delta <= kMaxInterval;
}

void WebContentsImpl::SetIgnoreInputEvents(bool ignore_input_events) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::SetIgnoreInputEvents",
                        "ignore_input_events", ignore_input_events);
  ignore_input_events_ = ignore_input_events;
}

#if defined(OS_ANDROID)
void WebContentsImpl::SetMainFrameImportance(
    ChildProcessImportance importance) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::SetMainFrameImportance",
                        "importance", static_cast<int>(importance));
  GetMainFrame()->GetRenderWidgetHost()->SetImportance(importance);
}
#endif

void WebContentsImpl::WasOccluded() {
  TRACE_EVENT0("content", "WebContentsImpl::WasOccluded");
  UpdateVisibilityAndNotifyPageAndView(Visibility::OCCLUDED);
}

Visibility WebContentsImpl::GetVisibility() {
  return visibility_;
}

bool WebContentsImpl::NeedToFireBeforeUnloadOrUnloadEvents() {
  if (!notify_disconnection_)
    return false;

  // Don't fire if the main frame's RenderViewHost indicates that beforeunload
  // and unload have already executed (e.g., after receiving a ClosePage ACK)
  // or should be ignored.
  if (GetRenderViewHost()->SuddenTerminationAllowed())
    return false;

  // Check whether any frame in the frame tree needs to run beforeunload or
  // unload-time event handlers.
  for (FrameTreeNode* node : frame_tree_.Nodes()) {
    RenderFrameHostImpl* rfh = node->current_frame_host();

    // No need to run beforeunload/unload-time events if the RenderFrame isn't
    // live.
    if (!rfh->IsRenderFrameLive())
      continue;
    bool should_run_before_unload_handler =
        rfh->GetSuddenTerminationDisablerState(
            blink::mojom::SuddenTerminationDisablerType::kBeforeUnloadHandler);
    bool should_run_unload_handler = rfh->GetSuddenTerminationDisablerState(
        blink::mojom::SuddenTerminationDisablerType::kUnloadHandler);
    bool should_run_page_hide_handler = rfh->GetSuddenTerminationDisablerState(
        blink::mojom::SuddenTerminationDisablerType::kPageHideHandler);
    auto* rvh = static_cast<RenderViewHostImpl*>(rfh->GetRenderViewHost());
    // If the tab is already hidden, we should not run visibilitychange
    // handlers.
    bool is_page_visible = rvh->GetPageLifecycleStateManager()
                               ->CalculatePageLifecycleState()
                               ->visibility == PageVisibilityState::kVisible;

    bool should_run_visibility_change_handler =
        is_page_visible && rfh->GetSuddenTerminationDisablerState(
                               blink::mojom::SuddenTerminationDisablerType::
                                   kVisibilityChangeHandler);
    if (should_run_before_unload_handler || should_run_unload_handler ||
        should_run_page_hide_handler || should_run_visibility_change_handler) {
      return true;
    }
  }

  return false;
}

void WebContentsImpl::DispatchBeforeUnload(bool auto_cancel) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::DispatchBeforeUnload",
                        "auto_cancel", auto_cancel);
  auto before_unload_type =
      auto_cancel ? RenderFrameHostImpl::BeforeUnloadType::DISCARD
                  : RenderFrameHostImpl::BeforeUnloadType::TAB_CLOSE;
  GetMainFrame()->DispatchBeforeUnload(before_unload_type, false);
}

bool WebContentsImpl::IsInnerWebContentsForGuest() {
  return !!browser_plugin_guest_;
}

void WebContentsImpl::AttachInnerWebContents(
    std::unique_ptr<WebContents> inner_web_contents,
    RenderFrameHost* render_frame_host,
    bool is_full_page) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::AttachInnerWebContents",
                        "inner_web_contents", inner_web_contents.get(),
                        "is_full_page", is_full_page);
  WebContentsImpl* inner_web_contents_impl =
      static_cast<WebContentsImpl*>(inner_web_contents.get());
  DCHECK(!inner_web_contents_impl->node_.outer_web_contents());
  auto* render_frame_host_impl =
      static_cast<RenderFrameHostImpl*>(render_frame_host);
  DCHECK_EQ(&frame_tree_, render_frame_host_impl->frame_tree());

  // Mark |render_frame_host_impl| as outer delegate frame.
  render_frame_host_impl->SetIsOuterDelegateFrame(true);

  RenderFrameHostManager* inner_render_manager =
      inner_web_contents_impl->GetRenderManager();
  RenderFrameHostImpl* inner_main_frame =
      inner_render_manager->current_frame_host();
  RenderViewHostImpl* inner_render_view_host =
      inner_render_manager->current_host();
  auto* outer_render_manager =
      render_frame_host_impl->frame_tree_node()->render_manager();

  // When attaching a WebContents as an inner WebContents, we need to replace
  // the Webcontents' view with a WebContentsViewChildFrame.
  inner_web_contents_impl->view_ = std::make_unique<WebContentsViewChildFrame>(
      inner_web_contents_impl,
      GetContentClient()->browser()->GetWebContentsViewDelegate(
          inner_web_contents_impl),
      &inner_web_contents_impl->render_view_host_delegate_view_);

  // When the WebContents being initialized has an opener, the  browser side
  // Render{View,Frame}Host must be initialized and the RenderWidgetHostView
  // created. This is needed because the usual initialization happens during
  // the first navigation, but when attaching a new window we don't navigate
  // before attaching. If the browser side is already initialized, the calls
  // below will just early return.
  inner_render_manager->InitRenderView(inner_render_view_host, nullptr);
  inner_main_frame->Init();
  if (!inner_render_manager->GetRenderWidgetHostView()) {
    inner_web_contents_impl->CreateRenderWidgetHostViewForRenderManager(
        inner_render_view_host);
  }

  inner_web_contents_impl->RecursivelyUnregisterFrameSinkIds();

  // Create a link to our outer WebContents.
  node_.AttachInnerWebContents(std::move(inner_web_contents),
                               render_frame_host_impl);

  // Create a proxy in top-level RenderFrameHostManager, pointing to the
  // SiteInstance of the outer WebContents. The proxy will be used to send
  // postMessage to the inner WebContents.
  auto* proxy = inner_render_manager->CreateOuterDelegateProxy(
      render_frame_host_impl->GetSiteInstance());

  // When attaching a GuestView as an inner WebContents, there should already be
  // a live RenderFrame, which has to be swapped. When attaching a portal, there
  // will not be a live RenderFrame before creating the proxy.
  if (render_frame_host_impl->IsRenderFrameLive()) {
    inner_render_manager->SwapOuterDelegateFrame(render_frame_host_impl, proxy);

    inner_web_contents_impl->ReattachToOuterWebContentsFrame();
  }

  if (frame_tree_.GetFocusedFrame() ==
      render_frame_host_impl->frame_tree_node()) {
    inner_web_contents_impl->SetFocusedFrame(
        inner_web_contents_impl->frame_tree_.root(),
        render_frame_host_impl->GetSiteInstance());
  }
  outer_render_manager->set_attach_complete();

  // If the inner WebContents is full frame, give it focus.
  if (is_full_page) {
    // There should only ever be one inner WebContents when |is_full_page| is
    // true, and it is the one we just attached.
    DCHECK_EQ(1u, node_.GetInnerWebContents().size());
    inner_web_contents_impl->SetAsFocusedWebContentsIfNecessary();
  }

  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->InnerWebContentsAttached(inner_web_contents_impl,
                                       render_frame_host, is_full_page);
  });

  // Make sure that the inner web contents and its outer delegate get properly
  // linked via the embedding token now that inner web contents are attached.
  inner_main_frame->PropagateEmbeddingTokenToParentFrame();
}

std::unique_ptr<WebContents> WebContentsImpl::DetachFromOuterWebContents() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::DetachFromOuterWebContents");
  auto* outer_web_contents = GetOuterWebContents();
  DCHECK(outer_web_contents);
  GetMainFrame()->ParentOrOuterDelegateFrame()->SetIsOuterDelegateFrame(false);

  RecursivelyUnregisterFrameSinkIds();

  // Each RenderViewHost has a RenderWidgetHost which can have a
  // RenderWidgetHostView,  and it needs to be re-created with the appropriate
  // platform view. It is important to re-create all child views, not only the
  // current one, since the view can be swapped due to a cross-origin
  // navigation.
  std::set<RenderViewHostImpl*> render_view_hosts;
  for (auto& render_view_host : GetFrameTree()->render_view_hosts()) {
    if (render_view_host.second->GetWidget() &&
        render_view_host.second->GetWidget()->GetView()) {
      DCHECK(render_view_host.second->GetWidget()
                 ->GetView()
                 ->IsRenderWidgetHostViewChildFrame());
      render_view_hosts.insert(render_view_host.second);
    }
  }

  for (auto* render_view_host : render_view_hosts)
    render_view_host->GetWidget()->GetView()->Destroy();

  GetRenderManager()->DeleteOuterDelegateProxy(
      node_.OuterContentsFrameTreeNode()
          ->current_frame_host()
          ->GetSiteInstance());
  view_.reset(CreateWebContentsView(
      this, GetContentClient()->browser()->GetWebContentsViewDelegate(this),
      &render_view_host_delegate_view_));
  view_->CreateView(nullptr);
  std::unique_ptr<WebContents> web_contents =
      node_.DisconnectFromOuterWebContents();
  DCHECK_EQ(web_contents.get(), this);
  node_.SetFocusedWebContents(this);

  for (auto* render_view_host : render_view_hosts)
    CreateRenderWidgetHostViewForRenderManager(render_view_host);

  RecursivelyRegisterFrameSinkIds();
  // TODO(adithyas): |browser_plugin_embedder_ax_tree_id| should either not be
  // used for portals, or it should get a different name.
  GetMainFrame()->set_browser_plugin_embedder_ax_tree_id(ui::AXTreeIDUnknown());
  GetMainFrame()->UpdateAXTreeData();

  // Invoke on the *outer* web contents observers for symmetry.
  outer_web_contents->observers_.ForEachObserver(
      [&](WebContentsObserver* observer) {
        observer->InnerWebContentsDetached(this);
      });

  return web_contents;
}

void WebContentsImpl::RecursivelyRegisterFrameSinkIds() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::RecursivelyRegisterFrameSinkIds");
  for (auto* view : GetRenderWidgetHostViewsInWebContentsTree()) {
    auto* rwhvb = static_cast<RenderWidgetHostViewBase*>(view);
    if (rwhvb->IsRenderWidgetHostViewChildFrame())
      static_cast<RenderWidgetHostViewChildFrame*>(view)->RegisterFrameSinkId();
  }
}

void WebContentsImpl::RecursivelyUnregisterFrameSinkIds() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::RecursivelyUnregisterFrameSinkIds");
  for (auto* view : GetRenderWidgetHostViewsInWebContentsTree()) {
    auto* rwhvb = static_cast<RenderWidgetHostViewBase*>(view);
    if (rwhvb->IsRenderWidgetHostViewChildFrame()) {
      static_cast<RenderWidgetHostViewChildFrame*>(view)
          ->UnregisterFrameSinkId();
    }
  }
}

void WebContentsImpl::ReattachToOuterWebContentsFrame() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::ReattachToOuterWebContentsFrame");
  DCHECK(node_.outer_web_contents());
  auto* render_manager = GetRenderManager();
  auto* parent_frame =
      node_.OuterContentsFrameTreeNode()->current_frame_host()->GetParent();
  render_manager->SetRWHViewForInnerContents(
      render_manager->GetRenderWidgetHostView());

  RecursivelyRegisterFrameSinkIds();

  // Set up the the guest's AX tree to point back at the embedder's AX tree.
  GetMainFrame()->set_browser_plugin_embedder_ax_tree_id(
      parent_frame->GetAXTreeID());
  GetMainFrame()->UpdateAXTreeData();
}

void WebContentsImpl::DidActivatePortal(
    WebContentsImpl* predecessor_web_contents,
    base::TimeTicks activation_time) {
  TRACE_EVENT2("content", "WebContentsImpl::DidActivatePortal", "predecessor",
               predecessor_web_contents, "activation_time", activation_time);
  DCHECK(predecessor_web_contents);
  NotifyInsidePortal(false);
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->DidActivatePortal(predecessor_web_contents, activation_time);
  });
  GetDelegate()->WebContentsBecamePortal(predecessor_web_contents);
}

void WebContentsImpl::NotifyInsidePortal(bool inside_portal) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::NotifyInsidePortal",
                        "inside_portal", inside_portal);
  ExecutePageBroadcastMethod(base::BindRepeating(
      [](bool inside_portal, RenderViewHostImpl* rvh) {
        if (auto& broadcast = rvh->GetAssociatedPageBroadcast())
          broadcast->SetInsidePortal(inside_portal);
      },
      inside_portal));
}

void WebContentsImpl::DidChangeVisibleSecurityState() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::DidChangeVisibleSecurityState");
  if (delegate_)
    delegate_->VisibleSecurityStateChanged(this);
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->DidChangeVisibleSecurityState();
  });
}

const blink::web_pref::WebPreferences WebContentsImpl::ComputeWebPreferences() {
  OPTIONAL_TRACE_EVENT0("browser", "WebContentsImpl::ComputeWebPreferences");

  blink::web_pref::WebPreferences prefs;

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  SetSlowWebPreferences(command_line, &prefs);

  prefs.web_security_enabled =
      !command_line.HasSwitch(switches::kDisableWebSecurity);

  prefs.remote_fonts_enabled =
      !command_line.HasSwitch(switches::kDisableRemoteFonts);
  prefs.application_cache_enabled =
      base::FeatureList::IsEnabled(blink::features::kAppCache);
  prefs.local_storage_enabled =
      !command_line.HasSwitch(switches::kDisableLocalStorage);
  prefs.databases_enabled =
      !command_line.HasSwitch(switches::kDisableDatabases);

  prefs.webgl1_enabled = !command_line.HasSwitch(switches::kDisable3DAPIs) &&
                         !command_line.HasSwitch(switches::kDisableWebGL);
  prefs.webgl2_enabled = !command_line.HasSwitch(switches::kDisable3DAPIs) &&
                         !command_line.HasSwitch(switches::kDisableWebGL) &&
                         !command_line.HasSwitch(switches::kDisableWebGL2);

  prefs.pepper_3d_enabled = !command_line.HasSwitch(switches::kDisablePepper3d);

  prefs.flash_3d_enabled = !command_line.HasSwitch(switches::kDisableFlash3d);
  prefs.flash_stage3d_enabled =
      !command_line.HasSwitch(switches::kDisableFlashStage3d);
  prefs.flash_stage3d_baseline_enabled =
      !command_line.HasSwitch(switches::kDisableFlashStage3d);

  prefs.allow_file_access_from_file_urls =
      command_line.HasSwitch(switches::kAllowFileAccessFromFiles);

  prefs.accelerated_2d_canvas_enabled =
      !command_line.HasSwitch(switches::kDisableAccelerated2dCanvas);
  prefs.new_canvas_2d_api_enabled =
      command_line.HasSwitch(switches::kEnableNewCanvas2DAPI) ||
      base::FeatureList::IsEnabled(features::kEnableNewCanvas2DAPI);
  prefs.antialiased_2d_canvas_disabled =
      command_line.HasSwitch(switches::kDisable2dCanvasAntialiasing);
  prefs.antialiased_clips_2d_canvas_enabled =
      !command_line.HasSwitch(switches::kDisable2dCanvasClipAntialiasing);

  prefs.disable_ipc_flooding_protection =
      command_line.HasSwitch(switches::kDisableIpcFloodingProtection) ||
      command_line.HasSwitch(switches::kDisablePushStateThrottle);

  prefs.accelerated_video_decode_enabled =
      !command_line.HasSwitch(switches::kDisableAcceleratedVideoDecode);

  std::string autoplay_policy = media::GetEffectiveAutoplayPolicy(command_line);
  if (autoplay_policy == switches::autoplay::kNoUserGestureRequiredPolicy) {
    prefs.autoplay_policy =
        blink::web_pref::AutoplayPolicy::kNoUserGestureRequired;
  } else if (autoplay_policy ==
             switches::autoplay::kUserGestureRequiredPolicy) {
    prefs.autoplay_policy =
        blink::web_pref::AutoplayPolicy::kUserGestureRequired;
  } else if (autoplay_policy ==
             switches::autoplay::kDocumentUserActivationRequiredPolicy) {
    prefs.autoplay_policy =
        blink::web_pref::AutoplayPolicy::kDocumentUserActivationRequired;
  } else {
    NOTREACHED();
  }

  prefs.dont_send_key_events_to_javascript =
      base::FeatureList::IsEnabled(features::kDontSendKeyEventsToJavascript);

// TODO(dtapuska): Enable barrel button selection drag support on Android.
// crbug.com/758042
#if defined(OS_WIN)
  prefs.barrel_button_for_drag_enabled =
      base::FeatureList::IsEnabled(features::kDirectManipulationStylus);
#endif  // defined(OS_WIN)

  prefs.enable_scroll_animator =
      command_line.HasSwitch(switches::kEnableSmoothScrolling) ||
      (!command_line.HasSwitch(switches::kDisableSmoothScrolling) &&
       gfx::Animation::ScrollAnimationsEnabledBySystem());

  prefs.prefers_reduced_motion = gfx::Animation::PrefersReducedMotion();

  if (ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
          GetRenderViewHost()->GetProcess()->GetID())) {
    prefs.loads_images_automatically = true;
    prefs.javascript_enabled = true;
  }

  prefs.viewport_enabled = command_line.HasSwitch(switches::kEnableViewport);

  if (IsOverridingUserAgent())
    prefs.viewport_meta_enabled = false;

  prefs.main_frame_resizes_are_orientation_changes =
      command_line.HasSwitch(switches::kMainFrameResizesAreOrientationChanges);

  prefs.spatial_navigation_enabled =
      command_line.HasSwitch(switches::kEnableSpatialNavigation);

  if (IsSpatialNavigationDisabled())
    prefs.spatial_navigation_enabled = false;

  prefs.disable_reading_from_canvas =
      command_line.HasSwitch(switches::kDisableReadingFromCanvas);

  prefs.strict_mixed_content_checking =
      command_line.HasSwitch(switches::kEnableStrictMixedContentChecking);

  prefs.strict_powerful_feature_restrictions = command_line.HasSwitch(
      switches::kEnableStrictPowerfulFeatureRestrictions);

  const std::string blockable_mixed_content_group =
      base::FieldTrialList::FindFullName("BlockableMixedContent");
  prefs.strictly_block_blockable_mixed_content =
      blockable_mixed_content_group == "StrictlyBlockBlockableMixedContent";

  const std::string plugin_mixed_content_status =
      base::FieldTrialList::FindFullName("PluginMixedContentStatus");
  prefs.block_mixed_plugin_content =
      plugin_mixed_content_status == "BlockableMixedContent";

  prefs.v8_cache_options = GetV8CacheOptions();

  prefs.user_gesture_required_for_presentation = !command_line.HasSwitch(
      switches::kDisableGestureRequirementForPresentation);

  if (HideDownloadUI())
    prefs.hide_download_ui = true;

  // `media_controls_enabled` is `true` by default.
  if (HasPersistentVideo())
    prefs.media_controls_enabled = false;

#if defined(OS_ANDROID)
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  gfx::Size size = display.GetSizeInPixel();
  int min_width = size.width() < size.height() ? size.width() : size.height();
  prefs.device_scale_adjustment = GetDeviceScaleAdjustment(
      static_cast<int>(min_width / display.device_scale_factor()));
#endif  // OS_ANDROID

  GetContentClient()->browser()->OverrideWebkitPrefs(GetRenderViewHost(),
                                                     &prefs);
  return prefs;
}

void WebContentsImpl::SetSlowWebPreferences(
    const base::CommandLine& command_line,
    blink::web_pref::WebPreferences* prefs) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::SetSlowWebPreferences");

  if (web_preferences_.get()) {
#define SET_FROM_CACHE(prefs, field) prefs->field = web_preferences_->field

    SET_FROM_CACHE(prefs, touch_event_feature_detection_enabled);
    SET_FROM_CACHE(prefs, available_pointer_types);
    SET_FROM_CACHE(prefs, available_hover_types);
    SET_FROM_CACHE(prefs, primary_pointer_type);
    SET_FROM_CACHE(prefs, primary_hover_type);
    SET_FROM_CACHE(prefs, pointer_events_max_touch_points);
    SET_FROM_CACHE(prefs, number_of_cpu_cores);

#if defined(OS_ANDROID)
    SET_FROM_CACHE(prefs, video_fullscreen_orientation_lock_enabled);
    SET_FROM_CACHE(prefs, video_rotate_to_fullscreen_enabled);
#endif

#undef SET_FROM_CACHE
  } else {
    // Every prefs->field modified below should have a SET_FROM_CACHE entry
    // above.

    // On Android, Touch event feature detection is enabled by default,
    // Otherwise default is disabled.
    std::string touch_enabled_default_switch =
        switches::kTouchEventFeatureDetectionDisabled;
#if defined(OS_ANDROID)
    touch_enabled_default_switch = switches::kTouchEventFeatureDetectionEnabled;
#endif  // defined(OS_ANDROID)
    const std::string touch_enabled_switch =
        command_line.HasSwitch(switches::kTouchEventFeatureDetection)
            ? command_line.GetSwitchValueASCII(
                  switches::kTouchEventFeatureDetection)
            : touch_enabled_default_switch;

    prefs->touch_event_feature_detection_enabled =
        (touch_enabled_switch == switches::kTouchEventFeatureDetectionAuto)
            ? (ui::GetTouchScreensAvailability() ==
               ui::TouchScreensAvailability::ENABLED)
            : (touch_enabled_switch.empty() ||
               touch_enabled_switch ==
                   switches::kTouchEventFeatureDetectionEnabled);

    std::tie(prefs->available_pointer_types, prefs->available_hover_types) =
        ui::GetAvailablePointerAndHoverTypes();
    prefs->primary_pointer_type =
        ui::GetPrimaryPointerType(prefs->available_pointer_types);
    prefs->primary_hover_type =
        ui::GetPrimaryHoverType(prefs->available_hover_types);

    prefs->pointer_events_max_touch_points = ui::MaxTouchPoints();

    prefs->number_of_cpu_cores = base::SysInfo::NumberOfProcessors();

#if defined(OS_ANDROID)
    const bool device_is_phone =
        ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_PHONE;
    prefs->video_fullscreen_orientation_lock_enabled = device_is_phone;
    prefs->video_rotate_to_fullscreen_enabled = device_is_phone;
#endif
  }
}

void WebContentsImpl::OnWebPreferencesChanged() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::OnWebPreferencesChanged");

  // This is defensive code to avoid infinite loops due to code run inside
  // SetWebPreferences() accidentally updating more preferences and thus
  // calling back into this code. See crbug.com/398751 for one past example.
  if (updating_web_preferences_)
    return;
  updating_web_preferences_ = true;
  SetWebPreferences(ComputeWebPreferences());
#if defined(OS_ANDROID)
  for (FrameTreeNode* node : frame_tree_.Nodes()) {
    RenderFrameHostImpl* rfh = node->current_frame_host();
    if (rfh->is_local_root()) {
      if (auto* rwh = rfh->GetRenderWidgetHost())
        rwh->SetForceEnableZoom(web_preferences_->force_enable_zoom);
    }
  }
#endif
  updating_web_preferences_ = false;
}

void WebContentsImpl::NotifyPreferencesChanged() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::NotifyPreferencesChanged");

  // Recompute the WebPreferences based on the current state of the WebContents,
  // etc. Note that OnWebPreferencesChanged will also call SetWebPreferences and
  // send the updated WebPreferences to all RenderViews for this WebContents.
  OnWebPreferencesChanged();
}

void WebContentsImpl::SyncRendererPrefs() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::SyncRendererPrefs");

  blink::mojom::RendererPreferences renderer_preferences = GetRendererPrefs();
  RenderViewHostImpl::GetPlatformSpecificPrefs(&renderer_preferences);
  SendPageMessage(
      new PageMsg_SetRendererPrefs(MSG_ROUTING_NONE, renderer_preferences));
}

void WebContentsImpl::OnCookiesAccessed(NavigationHandle* navigation,
                                        const CookieAccessDetails& details) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::OnCookiesAccessed",
                        "navigation_handle", navigation);
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->OnCookiesAccessed(navigation, details);
  });
}

void WebContentsImpl::OnCookiesAccessed(RenderFrameHostImpl* rfh,
                                        const CookieAccessDetails& details) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::OnCookiesAccessed",
                        "render_frame_host",
                        base::trace_event::ToTracedValue(rfh));
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->OnCookiesAccessed(rfh, details);
  });
}

void WebContentsImpl::Stop() {
  TRACE_EVENT0("content", "WebContentsImpl::Stop");
  for (FrameTreeNode* node : frame_tree_.Nodes())
    node->StopLoading();
  observers_.ForEachObserver(
      [&](WebContentsObserver* observer) { observer->NavigationStopped(); });
}

void WebContentsImpl::SetPageFrozen(bool frozen) {
  TRACE_EVENT1("content", "WebContentsImpl::SetPageFrozen", "frozen", frozen);

  // A visible page is never frozen.
  DCHECK_NE(Visibility::VISIBLE, GetVisibility());

  for (auto& entry : frame_tree_.render_view_hosts()) {
    entry.second->SetIsFrozen(frozen);
  }
}

std::unique_ptr<WebContents> WebContentsImpl::Clone() {
  TRACE_EVENT0("content", "WebContentsImpl::Clone");

  // We use our current SiteInstance since the cloned entry will use it anyway.
  // We pass our own opener so that the cloned page can access it if it was set
  // before.
  CreateParams create_params(GetBrowserContext(), GetSiteInstance());
  FrameTreeNode* opener = frame_tree_.root()->opener();
  RenderFrameHostImpl* opener_rfh = nullptr;
  if (opener)
    opener_rfh = opener->current_frame_host();
  std::unique_ptr<WebContentsImpl> tc =
      CreateWithOpener(create_params, opener_rfh);
  tc->GetController().CopyStateFrom(&controller_, true);
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->DidCloneToNewWebContents(this, tc.get());
  });
  return tc;
}

void WebContentsImpl::Observe(int type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::Observe", "type", type);
  switch (type) {
    case NOTIFICATION_RENDER_WIDGET_HOST_DESTROYED: {
      RenderWidgetHost* host = Source<RenderWidgetHost>(source).ptr();
      RenderWidgetHostView* view = host->GetView();
      if (view == GetFullscreenRenderWidgetHostView()) {
        // We cannot just call view_->RestoreFocus() here.  On some platforms,
        // attempting to focus the currently-invisible WebContentsView will be
        // flat-out ignored.  Therefore, this boolean is used to track whether
        // we will request focus after the fullscreen widget has been
        // destroyed.
        fullscreen_widget_had_focus_at_shutdown_ = (view && view->HasFocus());
      } else {
        for (auto i = pending_widget_views_.begin();
             i != pending_widget_views_.end(); ++i) {
          if (host->GetView() == i->second) {
            pending_widget_views_.erase(i);
            break;
          }
        }
      }
      break;
    }
    default:
      NOTREACHED();
  }
}

WebContents* WebContentsImpl::GetWebContents() {
  return this;
}

void WebContentsImpl::Init(const WebContents::CreateParams& params) {
  TRACE_EVENT0("content", "WebContentsImpl::Init");

  // This is set before initializing the render manager since
  // RenderFrameHostManager::Init calls back into us via its delegate to ask if
  // it should be hidden.
  visibility_ =
      params.initially_hidden ? Visibility::HIDDEN : Visibility::VISIBLE;

  if (!params.last_active_time.is_null())
    last_active_time_ = params.last_active_time;

  scoped_refptr<SiteInstance> site_instance = params.site_instance;
  if (!site_instance)
    site_instance = SiteInstance::Create(params.browser_context);
  if (params.desired_renderer_state == CreateParams::kNoRendererProcess) {
    static_cast<SiteInstanceImpl*>(site_instance.get())
        ->PreventAssociationWithSpareProcess();
  }

  GetRenderManager()->InitRoot(site_instance.get(),
                               params.renderer_initiated_creation);

  // blink::FrameTree::setName always keeps |unique_name| empty in case of a
  // main frame - let's do the same thing here.
  std::string unique_name;
  frame_tree_.root()->SetFrameName(params.main_frame_name, unique_name);

  WebContentsViewDelegate* delegate =
      GetContentClient()->browser()->GetWebContentsViewDelegate(this);

  if (browser_plugin_guest_) {
    view_ = std::make_unique<WebContentsViewChildFrame>(
        this, delegate, &render_view_host_delegate_view_);
  } else {
    view_.reset(CreateWebContentsView(this, delegate,
                                      &render_view_host_delegate_view_));
  }
  CHECK(render_view_host_delegate_view_);
  CHECK(view_.get());

  view_->CreateView(params.context);

#if BUILDFLAG(ENABLE_PLUGINS)
  plugin_content_origin_allowlist_ =
      std::make_unique<PluginContentOriginAllowlist>(this);
#endif

  registrar_.Add(this,
                 NOTIFICATION_RENDER_WIDGET_HOST_DESTROYED,
                 NotificationService::AllBrowserContextsAndSources());

  screen_orientation_provider_ =
      std::make_unique<ScreenOrientationProvider>(this);

#if defined(OS_ANDROID)
  DateTimeChooserAndroid::CreateForWebContents(this);
#endif

  // BrowserPluginGuest::Init needs to be called after this WebContents has
  // a RenderWidgetHostViewChildFrame. That is, |view_->CreateView| above.
  if (browser_plugin_guest_)
    browser_plugin_guest_->Init();

  for (auto& i : g_created_callbacks.Get())
    i.Run(this);

  // If the WebContents creation was renderer-initiated, it means that the
  // corresponding RenderView and main RenderFrame have already been created.
  // Ensure observers are notified about this.
  if (params.renderer_initiated_creation) {
    GetRenderViewHost()->GetWidget()->set_renderer_initialized(true);
    GetRenderViewHost()->DispatchRenderViewCreated();
    GetRenderManager()->current_frame_host()->SetRenderFrameCreated(true);
  }

  // Create the renderer process in advance if requested.
  if (params.desired_renderer_state ==
      CreateParams::kInitializeAndWarmupRendererProcess) {
    if (!GetRenderManager()->current_frame_host()->IsRenderFrameLive()) {
      GetRenderManager()->InitRenderView(GetRenderViewHost(), nullptr);
    }
  }

  // Ensure that observers are notified of the creation of this WebContents's
  // main RenderFrameHost. It must be done here for main frames, since the
  // NotifySwappedFromRenderManager expects view_ to already be created and that
  // happens after RenderFrameHostManager::Init.
  NotifySwappedFromRenderManager(
      nullptr, GetRenderManager()->current_frame_host(), true);

  // For WebContents that are never shown, do critical initialization here which
  // would normally only happen when the WebContents is shown.
  if (params.is_never_visible) {
    // This has just been created so there can only be one frame. Thus it is
    // safe to initialize the root.
    GetMainFrame()->Init();
  }
}

void WebContentsImpl::OnWebContentsDestroyed(WebContentsImpl* web_contents) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::OnWebContentsDestroyed");

  RemoveDestructionObserver(web_contents);

  // Clear a pending contents that has been closed before being shown.
  for (auto iter = pending_contents_.begin(); iter != pending_contents_.end();
       ++iter) {
    if (iter->second.contents.get() != web_contents)
      continue;

    // Someone else has deleted the WebContents. That should never happen!
    // TODO(erikchen): Fix semantics here. https://crbug.com/832879.
    iter->second.contents.release();
    pending_contents_.erase(iter);
    return;
  }
  NOTREACHED();
}

void WebContentsImpl::AddDestructionObserver(WebContentsImpl* web_contents) {
  OPTIONAL_TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("content.verbose"),
                        "WebContentsImpl::AddDestructionObserver");

  if (!base::Contains(destruction_observers_, web_contents)) {
    destruction_observers_[web_contents] =
        std::make_unique<DestructionObserver>(this, web_contents);
  }
}

void WebContentsImpl::RemoveDestructionObserver(WebContentsImpl* web_contents) {
  OPTIONAL_TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("content.verbose"),
                        "WebContentsImpl::RemoveDestructionObserver");
  destruction_observers_.erase(web_contents);
}

void WebContentsImpl::AddObserver(WebContentsObserver* observer) {
  OPTIONAL_TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("content.verbose"),
                        "WebContentsImpl::AddObserver");
  observers_.AddObserver(observer);
}

void WebContentsImpl::RemoveObserver(WebContentsObserver* observer) {
  OPTIONAL_TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("content.verbose"),
                        "WebContentsImpl::RemoveObserver");
  observers_.RemoveObserver(observer);
}

std::set<RenderWidgetHostView*>
WebContentsImpl::GetRenderWidgetHostViewsInTree() {
  std::set<RenderWidgetHostView*> set;
  for (RenderFrameHost* rfh : GetAllFrames()) {
    if (RenderWidgetHostView* rwhv = static_cast<RenderFrameHostImpl*>(rfh)
                                         ->frame_tree_node()
                                         ->render_manager()
                                         ->GetRenderWidgetHostView()) {
      set.insert(rwhv);
    }
  }
  return set;
}

std::set<RenderWidgetHostView*>
WebContentsImpl::GetRenderWidgetHostViewsInWebContentsTree() {
  std::set<RenderWidgetHostView*> result;
  GetRenderWidgetHostViewsInWebContentsTree(result);
  return result;
}

void WebContentsImpl::GetRenderWidgetHostViewsInWebContentsTree(
    std::set<RenderWidgetHostView*>& result) {
  std::set<RenderWidgetHostView*> views = GetRenderWidgetHostViewsInTree();
  result.insert(views.begin(), views.end());
  for (auto* inner_web_contents : GetInnerWebContents()) {
    static_cast<WebContentsImpl*>(inner_web_contents)
        ->GetRenderWidgetHostViewsInWebContentsTree(result);
  }
}

void WebContentsImpl::Activate() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::Activate");
  if (delegate_)
    delegate_->ActivateContents(this);
}

void WebContentsImpl::LostCapture(RenderWidgetHostImpl* render_widget_host) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::LostCapture",
                        "render_widget_host", render_widget_host);
  if (!RenderViewHostImpl::From(render_widget_host))
    return;

  if (delegate_)
    delegate_->LostCapture();
}

ukm::SourceId
WebContentsImpl::GetUkmSourceIdForLastCommittedSourceIncludingSameDocument()
    const {
  return last_committed_source_id_including_same_document_;
}

void WebContentsImpl::SetTopControlsShownRatio(
    RenderWidgetHostImpl* render_widget_host,
    float ratio) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::SetTopControlsShownRatio",
                        "render_widget_host", render_widget_host, "ratio",
                        ratio);
  if (!delegate_)
    return;

  RenderFrameHostImpl* rfh = GetMainFrame();
  if (!rfh || render_widget_host != rfh->GetRenderWidgetHost())
    return;

  delegate_->SetTopControlsShownRatio(this, ratio);
}

void WebContentsImpl::SetTopControlsGestureScrollInProgress(bool in_progress) {
  OPTIONAL_TRACE_EVENT1(
      "content", "WebContentsImpl::SetTopControlsGestureScrollInProgress",
      "in_progress", in_progress);
  if (delegate_)
    delegate_->SetTopControlsGestureScrollInProgress(in_progress);
}

void WebContentsImpl::RenderWidgetCreated(
    RenderWidgetHostImpl* render_widget_host) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::RenderWidgetCreated",
                        "render_widget_host", render_widget_host);
  created_widgets_.insert(render_widget_host);
}

void WebContentsImpl::RenderWidgetDeleted(
    RenderWidgetHostImpl* render_widget_host) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::RenderWidgetDeleted",
                        "render_widget_host", render_widget_host);
  // Note that |is_being_destroyed_| can be true at this point as
  // ~WebContentsImpl() calls RFHM::ClearRFHsPendingShutdown(), which might lead
  // us here.
  created_widgets_.erase(render_widget_host);

  if (is_being_destroyed_)
    return;

  if (render_widget_host &&
      render_widget_host->GetRoutingID() == fullscreen_widget_routing_id_ &&
      render_widget_host->GetProcess()->GetID() ==
          fullscreen_widget_process_id_) {
    if (delegate_ && delegate_->EmbedsFullscreenWidget())
      delegate_->ExitFullscreenModeForTab(this);
    observers_.ForEachObserver([&](WebContentsObserver* observer) {
      observer->DidDestroyFullscreenWidget();
    });
    fullscreen_widget_process_id_ = ChildProcessHost::kInvalidUniqueID;
    fullscreen_widget_routing_id_ = MSG_ROUTING_NONE;
    if (fullscreen_widget_had_focus_at_shutdown_)
      view_->RestoreFocus();
  }

  if (render_widget_host == mouse_lock_widget_)
    LostMouseLock(mouse_lock_widget_);

  CancelKeyboardLock(render_widget_host);
}

void WebContentsImpl::RenderWidgetGotFocus(
    RenderWidgetHostImpl* render_widget_host) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::RenderWidgetGotFocus",
                        "render_widget_host", render_widget_host);
  // Notify the observers if an embedded fullscreen widget was focused.
  if (delegate_ && render_widget_host && delegate_->EmbedsFullscreenWidget() &&
      render_widget_host->GetView() == GetFullscreenRenderWidgetHostView()) {
    NotifyWebContentsFocused(render_widget_host);
  }
}

void WebContentsImpl::RenderWidgetLostFocus(
    RenderWidgetHostImpl* render_widget_host) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::RenderWidgetLostFocus",
                        "render_widget_host", render_widget_host);
  // Notify the observers if an embedded fullscreen widget lost focus.
  if (delegate_ && render_widget_host && delegate_->EmbedsFullscreenWidget() &&
      render_widget_host->GetView() == GetFullscreenRenderWidgetHostView()) {
    NotifyWebContentsLostFocus(render_widget_host);
  }
}

void WebContentsImpl::RenderWidgetWasResized(
    RenderWidgetHostImpl* render_widget_host,
    bool width_changed) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::RenderWidgetWasResized",
                        "render_widget_host", render_widget_host,
                        "width_changed", width_changed);
  RenderFrameHostImpl* rfh = GetMainFrame();
  if (!rfh || render_widget_host != rfh->GetRenderWidgetHost())
    return;

  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->MainFrameWasResized(width_changed);
  });
}

KeyboardEventProcessingResult WebContentsImpl::PreHandleKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  OPTIONAL_TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("content.verbose"),
                        "WebContentsImpl::PreHandleKeyboardEvent");
  auto* outermost_contents = GetOutermostWebContents();
  // TODO(wjmaclean): Generalize this to forward all key events to the outermost
  // delegate's handler.
  if (outermost_contents != this && IsFullscreen() &&
      event.windows_key_code == ui::VKEY_ESCAPE) {
    // When an inner WebContents has focus and is fullscreen, redirect <esc>
    // key events to the outermost WebContents so it can be handled by that
    // WebContents' delegate.
    if (outermost_contents->PreHandleKeyboardEvent(event) ==
        KeyboardEventProcessingResult::HANDLED) {
      return KeyboardEventProcessingResult::HANDLED;
    }
  }
  return delegate_ ? delegate_->PreHandleKeyboardEvent(this, event)
                   : KeyboardEventProcessingResult::NOT_HANDLED;
}

bool WebContentsImpl::HandleMouseEvent(const blink::WebMouseEvent& event) {
  OPTIONAL_TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("content.verbose"),
                        "WebContentsImpl::HandleMouseEvent");
  // Handle mouse button back/forward in the browser process after the render
  // process is done with the event. This ensures all renderer-initiated history
  // navigations can be treated consistently.
  if (event.GetType() == blink::WebInputEvent::Type::kMouseUp) {
    WebContentsImpl* outermost = GetOutermostWebContents();
    if (event.button == blink::WebPointerProperties::Button::kBack &&
        outermost->controller_.CanGoBack()) {
      outermost->controller_.GoBack();
      return true;
    } else if (event.button == blink::WebPointerProperties::Button::kForward &&
               outermost->controller_.CanGoForward()) {
      outermost->controller_.GoForward();
      return true;
    }
  }
  return false;
}

bool WebContentsImpl::HandleKeyboardEvent(const NativeWebKeyboardEvent& event) {
  OPTIONAL_TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("content.verbose"),
                        "WebContentsImpl::HandleKeyboardEvent");
  if (browser_plugin_embedder_ &&
      browser_plugin_embedder_->HandleKeyboardEvent(event)) {
    return true;
  }
  return delegate_ && delegate_->HandleKeyboardEvent(this, event);
}

bool WebContentsImpl::HandleWheelEvent(const blink::WebMouseWheelEvent& event) {
  OPTIONAL_TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("content.verbose"),
                        "WebContentsImpl::HandleWheelEvent");
#if !defined(OS_MAC)
  // On platforms other than Mac, control+mousewheel may change zoom. On Mac,
  // this isn't done for two reasons:
  //   -the OS already has a gesture to do this through pinch-zoom
  //   -if a user starts an inertial scroll, let's go, and presses control
  //      (i.e. control+tab) then the OS's buffered scroll events will come in
  //      with control key set which isn't what the user wants
  if (delegate_ && event.wheel_ticks_y &&
      event.event_action == blink::WebMouseWheelEvent::EventAction::kPageZoom) {
    // Count only integer cumulative scrolls as zoom events; this handles
    // smooth scroll and regular scroll device behavior.
    zoom_scroll_remainder_ += event.wheel_ticks_y;
    int whole_zoom_scroll_remainder_ = std::lround(zoom_scroll_remainder_);
    zoom_scroll_remainder_ -= whole_zoom_scroll_remainder_;
    if (whole_zoom_scroll_remainder_ != 0) {
      delegate_->ContentsZoomChange(whole_zoom_scroll_remainder_ > 0);
    }
    return true;
  }
#endif
  return false;
}

bool WebContentsImpl::PreHandleGestureEvent(
    const blink::WebGestureEvent& event) {
  OPTIONAL_TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("content.verbose"),
                        "WebContentsImpl::PreHandleGestureEvent");
  return delegate_ && delegate_->PreHandleGestureEvent(this, event);
}

RenderWidgetHostInputEventRouter* WebContentsImpl::GetInputEventRouter() {
  if (!is_being_destroyed_ && GetOuterWebContents())
    return GetOuterWebContents()->GetInputEventRouter();

  if (!rwh_input_event_router_.get() && !is_being_destroyed_)
    rwh_input_event_router_ =
        std::make_unique<RenderWidgetHostInputEventRouter>();
  return rwh_input_event_router_.get();
}

void WebContentsImpl::ReplicatePageFocus(bool is_focused) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::ReplicatePageFocus",
                        "is_focused", is_focused);
  // Focus loss may occur while this WebContents is being destroyed.  Don't
  // send the message in this case, as the main frame's RenderFrameHost and
  // other state has already been cleared.
  if (is_being_destroyed_)
    return;

  frame_tree_.ReplicatePageFocus(is_focused);
}

RenderWidgetHostImpl* WebContentsImpl::GetFocusedRenderWidgetHost(
    RenderWidgetHostImpl* receiving_widget) {
  // Events for widgets other than the main frame (e.g., popup menus) should be
  // forwarded directly to the widget they arrived on.
  if (receiving_widget != GetMainFrame()->GetRenderWidgetHost())
    return receiving_widget;

  // If the focused WebContents is a guest WebContents, then get the focused
  // frame in the embedder WebContents instead.
  FrameTreeNode* focused_frame =
      GetFocusedWebContents()->frame_tree_.GetFocusedFrame();

  if (!focused_frame)
    return receiving_widget;

  // The view may be null if a subframe's renderer process has crashed while
  // the subframe has focus.  Drop the event in that case.  Do not give
  // it to the main frame, so that the user doesn't unexpectedly type into the
  // wrong frame if a focused subframe renderer crashes while they type.
  RenderWidgetHostView* view = focused_frame->current_frame_host()->GetView();
  if (!view)
    return nullptr;

  return RenderWidgetHostImpl::From(view->GetRenderWidgetHost());
}

RenderWidgetHostImpl* WebContentsImpl::GetRenderWidgetHostWithPageFocus() {
  WebContentsImpl* focused_web_contents = GetFocusedWebContents();

  return focused_web_contents->GetMainFrame()->GetRenderWidgetHost();
}

bool WebContentsImpl::CanEnterFullscreenMode() {
  // It's possible that this WebContents was spawned while blocking UI was on
  // the screen, or that it was downstream from a WebContents when UI was
  // blocked. Therefore, disqualify it from fullscreen if it or any upstream
  // WebContents has an active blocker.
  auto openers = GetAllOpeningWebContents(this);
  return std::all_of(openers.begin(), openers.end(), [](auto* opener) {
    return opener->fullscreen_blocker_count_ == 0;
  });
}

void WebContentsImpl::EnterFullscreenMode(
    RenderFrameHost* requesting_frame,
    const blink::mojom::FullscreenOptions& options) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::EnterFullscreenMode");
  DCHECK(CanEnterFullscreenMode());

  // This method is being called to enter renderer-initiated fullscreen mode.
  // Make sure any existing fullscreen widget is shut down first.
  RenderWidgetHostView* const widget_view = GetFullscreenRenderWidgetHostView();
  if (widget_view) {
    RenderWidgetHostImpl::From(widget_view->GetRenderWidgetHost())
        ->ShutdownAndDestroyWidget(true);
  }

  if (delegate_) {
    delegate_->EnterFullscreenModeForTab(requesting_frame, options);

    if (keyboard_lock_widget_)
      delegate_->RequestKeyboardLock(this, esc_key_locked_);
  }

  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->DidToggleFullscreenModeForTab(IsFullscreen(), false);
  });
  FullscreenContentsSet(GetBrowserContext())->insert(this);
}

void WebContentsImpl::ExitFullscreenMode(bool will_cause_resize) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::ExitFullscreenMode",
                        "will_cause_resize", will_cause_resize);
  // This method is being called to leave renderer-initiated fullscreen mode.
  // Make sure any existing fullscreen widget is shut down first.
  RenderWidgetHostView* const widget_view = GetFullscreenRenderWidgetHostView();
  if (widget_view) {
    RenderWidgetHostImpl::From(widget_view->GetRenderWidgetHost())
        ->ShutdownAndDestroyWidget(true);
  }

  if (delegate_) {
    delegate_->ExitFullscreenModeForTab(this);

    if (keyboard_lock_widget_)
      delegate_->CancelKeyboardLockRequest(this);
  }

  // The fullscreen state is communicated to the renderer through a resize
  // message. If the change in fullscreen state doesn't cause a view resize
  // then we must ensure web contents exit the fullscreen state by explicitly
  // sending a resize message. This is required for the situation of the browser
  // moving the view into a "browser fullscreen" state and then the contents
  // entering "tab fullscreen". Exiting the contents "tab fullscreen" then won't
  // have the side effect of the view resizing, hence the explicit call here is
  // required.
  if (!will_cause_resize) {
    if (RenderWidgetHostView* rwhv = GetRenderWidgetHostView()) {
        if (RenderWidgetHost* render_widget_host = rwhv->GetRenderWidgetHost())
          render_widget_host->SynchronizeVisualProperties();
    }
  }

  current_fullscreen_frame_ = nullptr;

  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->DidToggleFullscreenModeForTab(IsFullscreen(), will_cause_resize);
  });

  if (display_cutout_host_impl_)
    display_cutout_host_impl_->DidExitFullscreen();

  FullscreenContentsSet(GetBrowserContext())->erase(this);
}

void WebContentsImpl::FullscreenStateChanged(RenderFrameHost* rfh,
                                             bool is_fullscreen) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::FullscreenStateChanged",
                        "render_frame_host", rfh, "is_fullscreen",
                        is_fullscreen);
  RenderFrameHostImpl* frame = static_cast<RenderFrameHostImpl*>(rfh);

  if (is_fullscreen) {
    if (!base::Contains(fullscreen_frames_, frame)) {
      fullscreen_frames_.insert(frame);
      FullscreenFrameSetUpdated();
    }
    return;
  }

  // If |frame| is no longer in fullscreen, remove it and any descendants.
  // See https://fullscreen.spec.whatwg.org.
  size_t size_before_deletion = fullscreen_frames_.size();
  base::EraseIf(fullscreen_frames_, [&](RenderFrameHostImpl* current) {
    return (current == frame || current->IsDescendantOf(frame));
  });

  if (size_before_deletion != fullscreen_frames_.size())
    FullscreenFrameSetUpdated();
}

void WebContentsImpl::FullscreenFrameSetUpdated() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::FullscreenFrameSetUpdated");
  if (fullscreen_frames_.empty()) {
    current_fullscreen_frame_ = nullptr;
    return;
  }

  // Find the current fullscreen frame and call the observers.
  // If frame A is fullscreen, then frame B goes into inner fullscreen, then B
  // exits fullscreen - that will result in A being fullscreen.
  RenderFrameHostImpl* new_fullscreen_frame = *std::max_element(
      fullscreen_frames_.begin(), fullscreen_frames_.end(), FrameCompareDepth);

  // If we have already notified observers about this frame then we should not
  // fire the observers again.
  if (new_fullscreen_frame == current_fullscreen_frame_)
    return;
  current_fullscreen_frame_ = new_fullscreen_frame;

  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->DidAcquireFullscreen(new_fullscreen_frame);
  });
  if (display_cutout_host_impl_)
    display_cutout_host_impl_->DidAcquireFullscreen(new_fullscreen_frame);
}

void WebContentsImpl::UpdateVisibilityAndNotifyPageAndView(
    Visibility new_visibility) {
  OPTIONAL_TRACE_EVENT1("content",
                        "WebContentsImpl::UpdateVisibilityAndNotifyPageAndView",
                        "new_visibility", static_cast<int>(new_visibility));
  // Only hide the page if there are no entities capturing screenshots
  // or video (e.g. mirroring or WebXR). If there are, apply the correct state
  // of kHidden or kHiddenButPainting.
  bool web_contents_visible_in_vr = false;
#if BUILDFLAG(ENABLE_VR)
  web_contents_visible_in_vr =
      XRRuntimeManagerImpl::GetImmersiveSessionWebContents() == this;
#endif

  PageVisibilityState page_visibility;
  if (new_visibility == Visibility::VISIBLE || visible_capturer_count_ > 0 ||
      web_contents_visible_in_vr) {
    page_visibility = PageVisibilityState::kVisible;
  } else if (hidden_capturer_count_ > 0) {
    page_visibility = PageVisibilityState::kHiddenButPainting;
  } else {
    page_visibility = PageVisibilityState::kHidden;
  }
  // If there are entities in Picture-in-Picture mode, don't activate
  // the "disable rendering" optimization.
  const bool view_is_visible =
      page_visibility != PageVisibilityState::kHidden ||
      HasPictureInPictureVideo();

  if (page_visibility != PageVisibilityState::kHidden) {
    // We cannot show a page or capture video unless there is a valid renderer
    // associated with this web contents. The navigation controller for this
    // page must be set to active (allowing navigation to complete, a renderer
    // and its associated views to be created, etc.) if any of these conditions
    // holds.
    //
    // Previously, it was possible for browser-side code to try to capture video
    // from a restored tab (for a variety of reasons, including the browser
    // creating preview thumbnails) and the tab would never actually load. By
    // keying this behavior off of |page_visibility| instead of just
    // |new_visibility| we avoid this case. See crbug.com/1020782 for more
    // context.
    controller_.SetActive(true);

    // This shows the Page before showing the individual RenderWidgets, as
    // RenderWidgets will work to produce compositor frames and handle input
    // as soon as they are shown. But the Page and other classes do not expect
    // to be producing frames when the Page is hidden. So we make sure the Page
    // is shown first.
    for (auto* rvh : GetRenderViewHostsIncludingBackForwardCached()) {
      rvh->SetVisibility(page_visibility);
    }
  }

  // |GetRenderWidgetHostView()| can be null if the user middle clicks a link to
  // open a tab in the background, then closes the tab before selecting it.
  // This is because closing the tab calls WebContentsImpl::Destroy(), which
  // removes the |GetRenderViewHost()|; then when we actually destroy the
  // window, OnWindowPosChanged() notices and calls WasHidden() (which
  // calls us).
  if (auto* view = GetRenderWidgetHostView()) {
    if (view_is_visible) {
      view->Show();
    } else if (new_visibility == Visibility::HIDDEN) {
      view->Hide();
    } else {
      view->WasOccluded();
    }
  }

  SetVisibilityForChildViews(view_is_visible);

  // Make sure to call SetVisibilityAndNotifyObservers(VISIBLE) before notifying
  // the CrossProcessFrameConnector.
  if (new_visibility == Visibility::VISIBLE) {
    last_active_time_ = base::TimeTicks::Now();
    SetVisibilityAndNotifyObservers(new_visibility);
  }

  if (page_visibility == PageVisibilityState::kHidden) {
    // Similar to when showing the page, we only hide the page after
    // hiding the individual RenderWidgets.
    for (auto* rvh : GetRenderViewHostsIncludingBackForwardCached()) {
      rvh->SetVisibility(page_visibility);
    }

  } else {
    for (FrameTreeNode* node : frame_tree_.Nodes()) {
      RenderFrameProxyHost* parent = node->render_manager()->GetProxyToParent();
      if (!parent)
        continue;

      parent->cross_process_frame_connector()->DelegateWasShown();
    }
  }

  if (new_visibility != Visibility::VISIBLE)
    SetVisibilityAndNotifyObservers(new_visibility);
}

#if defined(OS_ANDROID)
void WebContentsImpl::UpdateUserGestureCarryoverInfo() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::UpdateUserGestureCarryoverInfo");
  if (delegate_)
    delegate_->UpdateUserGestureCarryoverInfo(this);
}
#endif

bool WebContentsImpl::IsFullscreen() {
  return delegate_ ? delegate_->IsFullscreenForTabOrPending(this) : false;
}

bool WebContentsImpl::ShouldShowStaleContentOnEviction() {
  return GetDelegate() && GetDelegate()->ShouldShowStaleContentOnEviction(this);
}

blink::mojom::DisplayMode WebContentsImpl::GetDisplayMode() const {
  return delegate_ ? delegate_->GetDisplayMode(this)
                   : blink::mojom::DisplayMode::kBrowser;
}

void WebContentsImpl::RequestToLockMouse(
    RenderWidgetHostImpl* render_widget_host,
    bool user_gesture,
    bool last_unlocked_by_target,
    bool privileged) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::RequestToLockMouse",
                        "render_widget_host", render_widget_host, "privileged",
                        privileged);
  for (WebContentsImpl* current = this; current;
       current = current->GetOuterWebContents()) {
    if (current->mouse_lock_widget_) {
      render_widget_host->GotResponseToLockMouseRequest(
          blink::mojom::PointerLockResult::kAlreadyLocked);
      return;
    }
  }

  if (privileged) {
    DCHECK(!GetOuterWebContents());
    mouse_lock_widget_ = render_widget_host;
    render_widget_host->GotResponseToLockMouseRequest(
        blink::mojom::PointerLockResult::kSuccess);
    return;
  }

  bool widget_in_frame_tree = false;
  for (FrameTreeNode* node : frame_tree_.Nodes()) {
    if (node->current_frame_host()->GetRenderWidgetHost() ==
        render_widget_host) {
      widget_in_frame_tree = true;
      break;
    }
  }

  if (widget_in_frame_tree && delegate_) {
    for (WebContentsImpl* current = this; current;
         current = current->GetOuterWebContents()) {
      current->mouse_lock_widget_ = render_widget_host;
    }

    delegate_->RequestToLockMouse(this, user_gesture, last_unlocked_by_target);
  } else {
    render_widget_host->GotResponseToLockMouseRequest(
        blink::mojom::PointerLockResult::kWrongDocument);
  }
}

void WebContentsImpl::LostMouseLock(RenderWidgetHostImpl* render_widget_host) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::LostMouseLock",
                        "render_widget_host", render_widget_host);
  CHECK(mouse_lock_widget_);

  if (mouse_lock_widget_->delegate()->GetAsWebContents() != this)
    return mouse_lock_widget_->delegate()->LostMouseLock(render_widget_host);

  mouse_lock_widget_->SendMouseLockLost();
  for (WebContentsImpl* current = this; current;
       current = current->GetOuterWebContents()) {
    current->mouse_lock_widget_ = nullptr;
  }

  if (delegate_)
    delegate_->LostMouseLock();
}

bool WebContentsImpl::HasMouseLock(RenderWidgetHostImpl* render_widget_host) {
  // To verify if the mouse is locked, the mouse_lock_widget_ needs to be
  // assigned to the widget that requested the mouse lock, and the top-level
  // platform RenderWidgetHostView needs to hold the mouse lock from the OS.
  auto* widget_host = GetTopLevelRenderWidgetHostView();
  return mouse_lock_widget_ == render_widget_host && widget_host &&
         widget_host->IsMouseLocked();
}

RenderWidgetHostImpl* WebContentsImpl::GetMouseLockWidget() {
  auto* widget_host = GetTopLevelRenderWidgetHostView();
  if ((widget_host && widget_host->IsMouseLocked()) ||
      (GetFullscreenRenderWidgetHostView() &&
       GetFullscreenRenderWidgetHostView()->IsMouseLocked())) {
    return mouse_lock_widget_;
  }

  return nullptr;
}

bool WebContentsImpl::RequestKeyboardLock(
    RenderWidgetHostImpl* render_widget_host,
    bool esc_key_locked) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::RequestKeyboardLock",
                        "render_widget_host", render_widget_host,
                        "esc_key_locked", esc_key_locked);
  DCHECK(render_widget_host);
  if (render_widget_host->delegate()->GetAsWebContents() != this) {
    NOTREACHED();
    return false;
  }

  // KeyboardLock is only supported when called by the top-level browsing
  // context and is not supported in embedded content scenarios.
  if (GetOuterWebContents())
    return false;

  esc_key_locked_ = esc_key_locked;
  keyboard_lock_widget_ = render_widget_host;

  if (delegate_)
    delegate_->RequestKeyboardLock(this, esc_key_locked_);
  return true;
}

void WebContentsImpl::CancelKeyboardLock(
    RenderWidgetHostImpl* render_widget_host) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::CancelKeyboardLockRequest",
                        "render_widget_host", render_widget_host);
  if (!keyboard_lock_widget_ || render_widget_host != keyboard_lock_widget_)
    return;

  RenderWidgetHostImpl* old_keyboard_lock_widget = keyboard_lock_widget_;
  keyboard_lock_widget_ = nullptr;

  if (delegate_)
    delegate_->CancelKeyboardLockRequest(this);

  old_keyboard_lock_widget->CancelKeyboardLock();
}

RenderWidgetHostImpl* WebContentsImpl::GetKeyboardLockWidget() {
  return keyboard_lock_widget_;
}

void WebContentsImpl::OnRenderFrameProxyVisibilityChanged(
    blink::mojom::FrameVisibility visibility) {
  OPTIONAL_TRACE_EVENT1("content",
                        "WebContentsImpl::OnRenderFrameProxyVisibilityChanged",
                        "visibility", static_cast<int>(visibility));
  switch (visibility) {
    case blink::mojom::FrameVisibility::kRenderedInViewport:
      WasShown();
      break;
    case blink::mojom::FrameVisibility::kNotRendered:
      WasHidden();
      break;
    case blink::mojom::FrameVisibility::kRenderedOutOfViewport:
      WasOccluded();
      break;
  }
}

RenderFrameHostDelegate* WebContentsImpl::CreateNewWindow(
    RenderFrameHost* opener,
    const mojom::CreateNewWindowParams& params,
    bool is_new_browsing_instance,
    bool has_user_gesture,
    SessionStorageNamespace* session_storage_namespace) {
  TRACE_EVENT2("browser,content,navigation", "WebContentsImpl::CreateNewWindow",
               "opener", base::trace_event::ToTracedValue(opener), "params",
               base::trace_event::ToTracedValue(params));
  DCHECK(opener);

  int render_process_id = opener->GetProcess()->GetID();

  SiteInstance* source_site_instance = opener->GetSiteInstance();

  const GURL source_site_url = source_site_instance->GetSiteURL();
  const std::string& partition_id =
      GetContentClient()->browser()->GetStoragePartitionIdForSite(
          GetBrowserContext(), source_site_url);
  {
    StoragePartition* partition = BrowserContext::GetStoragePartitionForSite(
        GetBrowserContext(), source_site_url);
    DOMStorageContextWrapper* dom_storage_context =
        static_cast<DOMStorageContextWrapper*>(
            partition->GetDOMStorageContext());
    SessionStorageNamespaceImpl* session_storage_namespace_impl =
        static_cast<SessionStorageNamespaceImpl*>(session_storage_namespace);
    CHECK(session_storage_namespace_impl->IsFromContext(dom_storage_context));
  }

  if (delegate_ && delegate_->IsWebContentsCreationOverridden(
                       source_site_instance, params.window_container_type,
                       opener->GetLastCommittedURL(), params.frame_name,
                       params.target_url)) {
    return static_cast<WebContentsImpl*>(delegate_->CreateCustomWebContents(
        opener, source_site_instance, is_new_browsing_instance,
        opener->GetLastCommittedURL(), params.frame_name, params.target_url,
        partition_id, session_storage_namespace));
  }

  bool renderer_started_hidden =
      params.disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB;

  // We usually create the new window in the same BrowsingInstance (group of
  // script-related windows), by passing in the current SiteInstance.  However,
  // if the opener is being suppressed (in a non-guest), we do not provide
  // a SiteInstance which causes a new one to get created in its own
  // BrowsingInstance.
  bool is_guest = BrowserPluginGuest::IsGuest(this);
  scoped_refptr<SiteInstance> site_instance =
      params.opener_suppressed && !is_guest ? nullptr : source_site_instance;

  // Create the new web contents. This will automatically create the new
  // WebContentsView. In the future, we may want to create the view separately.
  CreateParams create_params(GetBrowserContext(), site_instance.get());
  create_params.main_frame_name = params.frame_name;
  create_params.opener_render_process_id = render_process_id;
  create_params.opener_render_frame_id = opener->GetRoutingID();
  create_params.opener_suppressed = params.opener_suppressed;
  create_params.initially_hidden = renderer_started_hidden;
  create_params.initial_popup_url = params.target_url;

  // Even though all codepaths leading here are in response to a renderer
  // tryng to open a new window, if the new window ends up in a different
  // browsing instance, then the RenderViewHost, RenderWidgetHost,
  // RenderFrameHost constellation is effectively browser initiated
  // the opener's process will not given the routing IDs for the new
  // objects.
  create_params.renderer_initiated_creation = !is_new_browsing_instance;

  std::unique_ptr<WebContentsImpl> new_contents;
  if (!is_guest) {
    create_params.context = view_->GetNativeView();
    new_contents = WebContentsImpl::Create(create_params);
  } else {
    new_contents = base::WrapUnique(static_cast<WebContentsImpl*>(
        GetBrowserPluginGuest()->CreateNewGuestWindow(create_params)));
  }
  auto* new_contents_impl = new_contents.get();

  new_contents_impl->GetController().SetSessionStorageNamespace(
      partition_id, session_storage_namespace);

  // If the new frame has a name, make sure any SiteInstances that can find
  // this named frame have proxies for it.  Must be called after
  // SetSessionStorageNamespace, since this calls CreateRenderView, which uses
  // GetSessionStorageNamespace.
  if (!params.frame_name.empty())
    new_contents_impl->GetRenderManager()->CreateProxiesForNewNamedFrame();

  // Save the window for later if we're not suppressing the opener (since it
  // will be shown immediately).
  if (!params.opener_suppressed) {
    if (!is_guest) {
      WebContentsView* new_view = new_contents_impl->view_.get();

      // TODO(brettw): It seems bogus that we have to call this function on the
      // newly created object and give it one of its own member variables.
      RenderWidgetHostView* widget_view = new_view->CreateViewForWidget(
          new_contents_impl->GetRenderViewHost()->GetWidget());
      if (!renderer_started_hidden) {
        // RenderWidgets for frames always initialize as hidden. If the renderer
        // created this window as visible, then we show it here.
        widget_view->Show();
      }
    }
    // Save the created window associated with the route so we can show it
    // later.
    //
    // TODO(ajwong): This should be keyed off the RenderFrame routing id or the
    // FrameTreeNode id instead of the routing id of the Widget for the main
    // frame.  https://crbug.com/545684
    int32_t main_frame_routing_id = new_contents_impl->GetMainFrame()
                                        ->GetRenderWidgetHost()
                                        ->GetRoutingID();
    GlobalRoutingID id(render_process_id, main_frame_routing_id);
    pending_contents_[id] =
        CreatedWindow(std::move(new_contents), params.target_url);
    AddDestructionObserver(new_contents_impl);
  }

  if (delegate_) {
    delegate_->WebContentsCreated(this, render_process_id,
                                  opener->GetRoutingID(), params.frame_name,
                                  params.target_url, new_contents_impl);
  }

  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->DidOpenRequestedURL(new_contents_impl, opener, params.target_url,
                                  params.referrer.To<Referrer>(),
                                  params.disposition, ui::PAGE_TRANSITION_LINK,
                                  false,  // started_from_context_menu
                                  true);  // renderer_initiated
  });

  if (params.opener_suppressed) {
    // When the opener is suppressed, the original renderer cannot access the
    // new window.  As a result, we need to show and navigate the window here.
    bool was_blocked = false;

    if (delegate_) {
      base::WeakPtr<WebContentsImpl> weak_new_contents =
          new_contents_impl->weak_factory_.GetWeakPtr();

      gfx::Rect initial_rect;  // Report an empty initial rect.
      delegate_->AddNewContents(this, std::move(new_contents),
                                params.target_url, params.disposition,
                                initial_rect, has_user_gesture, &was_blocked);
      // The delegate may delete |new_contents_impl| during AddNewContents().
      if (!weak_new_contents)
        return nullptr;
    }

    if (!was_blocked) {
      std::unique_ptr<NavigationController::LoadURLParams> load_params =
          std::make_unique<NavigationController::LoadURLParams>(
              params.target_url);
      load_params->initiator_origin = opener->GetLastCommittedOrigin();
      // Avoiding setting |load_params->source_site_instance| when
      // |opener_suppressed| is true, because in that case we do not want to use
      // the old SiteInstance and/or BrowsingInstance.  See also the test here:
      // NewPopupCOOP_SameOriginPolicyAndCrossOriginIframeSetsNoopener.
      load_params->referrer = params.referrer.To<Referrer>();
      load_params->transition_type = ui::PAGE_TRANSITION_LINK;
      load_params->is_renderer_initiated = true;
      load_params->has_user_gesture = has_user_gesture;

      if (delegate_ && !is_guest &&
          !delegate_->ShouldResumeRequestsForCreatedWindow()) {
        // We are in asynchronous add new contents path, delay navigation.
        DCHECK(!new_contents_impl->delayed_open_url_params_);
        new_contents_impl->delayed_load_url_params_ = std::move(load_params);
      } else {
        new_contents_impl->controller_.LoadURLWithParams(*load_params.get());
        if (!is_guest)
          new_contents_impl->Focus();
      }
    }
  }
  return new_contents_impl;
}

void WebContentsImpl::CreateNewWidget(
    AgentSchedulingGroupHost& agent_scheduling_group,
    int32_t route_id,
    mojo::PendingAssociatedReceiver<blink::mojom::WidgetHost> blink_widget_host,
    mojo::PendingAssociatedRemote<blink::mojom::Widget> blink_widget) {
  CreateNewWidget(agent_scheduling_group, route_id,
                  /*is_fullscreen=*/false, std::move(blink_widget_host),
                  std::move(blink_widget));
}

void WebContentsImpl::CreateNewFullscreenWidget(
    AgentSchedulingGroupHost& agent_scheduling_group,
    int32_t route_id,
    mojo::PendingAssociatedReceiver<blink::mojom::WidgetHost> blink_widget_host,
    mojo::PendingAssociatedRemote<blink::mojom::Widget> blink_widget) {
  CreateNewWidget(agent_scheduling_group, route_id,
                  /*is_fullscreen=*/true, std::move(blink_widget_host),
                  std::move(blink_widget));
}

void WebContentsImpl::CreateNewWidget(
    AgentSchedulingGroupHost& agent_scheduling_group,
    int32_t route_id,
    bool is_fullscreen,
    mojo::PendingAssociatedReceiver<blink::mojom::WidgetHost> blink_widget_host,
    mojo::PendingAssociatedRemote<blink::mojom::Widget> blink_widget) {
  OPTIONAL_TRACE_EVENT1(
      "content", "WebContentsImpl::CreateNewWidget", "params",
      base::trace_event::TracedValue::Build(
          {{"route_id", route_id}, {"is_fullscreen", is_fullscreen}}));
  RenderProcessHost* process = agent_scheduling_group.GetProcess();
  // A message to create a new widget can only come from an active process for
  // this WebContentsImpl instance. If any other process sends the request,
  // it is invalid and the process must be terminated.
  if (!HasMatchingProcess(&frame_tree_, process->GetID())) {
    ReceivedBadMessage(process, bad_message::WCI_NEW_WIDGET_PROCESS_MISMATCH);
    return;
  }

  RenderWidgetHostImpl* widget_host = new RenderWidgetHostImpl(
      this, agent_scheduling_group, route_id, IsHidden(),
      std::make_unique<FrameTokenMessageQueue>());

  widget_host->BindWidgetInterfaces(std::move(blink_widget_host),
                                    std::move(blink_widget));
  RenderWidgetHostViewBase* widget_view =
      static_cast<RenderWidgetHostViewBase*>(
          view_->CreateViewForChildWidget(widget_host));
  if (!widget_view)
    return;
  // Fullscreen child widgets are frames, other child widgets are popups, and
  // popups should not get activated.
  if (!is_fullscreen)
    widget_view->SetWidgetType(WidgetType::kPopup);
  // Save the created widget associated with the route so we can show it later.
  pending_widget_views_[GlobalRoutingID(process->GetID(), route_id)] =
      widget_view;
}

void WebContentsImpl::ShowCreatedWindow(RenderFrameHost* opener,
                                        int main_frame_widget_route_id,
                                        WindowOpenDisposition disposition,
                                        const gfx::Rect& initial_rect,
                                        bool user_gesture) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::ShowCreatedWindow",
                        "opener", base::trace_event::ToTracedValue(opener),
                        "main_frame_widget_route_id",
                        main_frame_widget_route_id);
  // This method is the renderer requesting an existing top level window to
  // show a new top level window that the renderer created. Each top level
  // window is associated with a WebContents. In this case it was created
  // earlier but showing it was deferred until the renderer requested for it
  // to be shown. We find that previously created WebContents here.
  // TODO(danakj): Why do we defer this show step until the renderer asks for it
  // when it will always do so. What needs to happen in the renderer before we
  // reach here?
  base::Optional<CreatedWindow> owned_created = GetCreatedWindow(
      opener->GetProcess()->GetID(), main_frame_widget_route_id);

  // The browser may have rejected the request to make a new window, or the
  // renderer could be sending an invalid route id. Ignore the request then.
  if (!owned_created || !owned_created->contents)
    return;

  WebContentsImpl* created = owned_created->contents.get();

  // This uses the delegate for the WebContents where the window was created
  // from, to control how to show the newly created window.
  WebContentsDelegate* delegate = GetDelegate();

  // Individual members of |initial_rect| may be 0 to indicate that the
  // window.open() feature string did not specify a value. This code does not
  // have the ability to distinguish between an unspecified value and 0.
  // Assume that if any single value is non-zero, all values should be used.
  // TODO(crbug.com/897300): Plumb values as specified; set defaults here?
  gfx::Rect adjusted_rect = initial_rect;
  int64_t display_id = display::kInvalidDisplayId;
  if (adjusted_rect != gfx::Rect())
    display_id = AdjustRequestedWindowBounds(&adjusted_rect, opener);

  // Drop fullscreen when opening a WebContents to prohibit deceptive behavior.
  // Only drop fullscreen on the specific destination display, if it is known.
  // This supports sites using cross-screen window placement capabilities to
  // retain fullscreen and open a window on another screen.
  ForSecurityDropFullscreen(display_id).RunAndReset();

  // The delegate can be null in tests, so we must check for it :(.
  if (delegate) {
    // Mark the web contents as pending resume, then immediately do
    // the resume if the delegate wants it.
    created->is_resume_pending_ = true;
    if (delegate->ShouldResumeRequestsForCreatedWindow())
      created->ResumeLoadingCreatedWebContents();

    base::WeakPtr<WebContentsImpl> weak_created =
        created->weak_factory_.GetWeakPtr();
    delegate->AddNewContents(this, std::move(owned_created->contents),
                             std::move(owned_created->target_url), disposition,
                             adjusted_rect, user_gesture, nullptr);
    // The delegate may delete |created| during AddNewContents().
    if (!weak_created)
      return;
  }

  RenderWidgetHostImpl* rwh = created->GetMainFrame()->GetRenderWidgetHost();
  DCHECK_EQ(main_frame_widget_route_id, rwh->GetRoutingID());
  rwh->Send(new WidgetMsg_SetBounds_ACK(rwh->GetRoutingID()));
}

void WebContentsImpl::ShowCreatedWidget(int process_id,
                                        int widget_route_id,
                                        const gfx::Rect& initial_rect) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::ShowCreatedWidget",
                        "process_id", process_id, "widget_route_id",
                        widget_route_id);
  ShowCreatedWidget(process_id, widget_route_id, false, initial_rect);
}

void WebContentsImpl::ShowCreatedFullscreenWidget(int process_id,
                                                  int widget_route_id) {
  OPTIONAL_TRACE_EVENT2(
      "content", "WebContentsImpl::ShowCreatedFullscreenWidget", "process_id",
      process_id, "widget_route_id", widget_route_id);
  ShowCreatedWidget(process_id, widget_route_id, true, gfx::Rect());
}

void WebContentsImpl::ShowCreatedWidget(int process_id,
                                        int route_id,
                                        bool is_fullscreen,
                                        const gfx::Rect& initial_rect) {
  RenderWidgetHostViewBase* widget_host_view =
      static_cast<RenderWidgetHostViewBase*>(
          GetCreatedWidget(process_id, route_id));
  if (!widget_host_view)
    return;

  // GetOutermostWebContents() returns |this| if there are no outer WebContents.
  auto* outer_web_contents = GetOuterWebContents();
  auto* outermost_web_contents = GetOutermostWebContents();
  RenderWidgetHostView* view =
      outermost_web_contents->GetRenderWidgetHostView();
  // It's not entirely obvious why we need the transform only in the case where
  // the outer webcontents is not the same as the outermost webcontents. It may
  // be due to the fact that oopifs that are children of the mainframe get
  // correct values for their screenrects, but deeper cross-process frames do
  // not. Hopefully this can be resolved with https://crbug.com/928825.
  // Handling these cases separately is needed for http://crbug.com/1015298.
  bool needs_transform = this != outermost_web_contents &&
                         outermost_web_contents != outer_web_contents;

  gfx::Rect transformed_rect(initial_rect);
  RenderWidgetHostView* this_view = GetRenderWidgetHostView();
  if (needs_transform) {
    // We need to transform the coordinates of initial_rect.
    gfx::Point origin =
        this_view->TransformPointToRootCoordSpace(initial_rect.origin());
    gfx::Point bottom_right =
        this_view->TransformPointToRootCoordSpace(initial_rect.bottom_right());
    transformed_rect =
        gfx::Rect(origin.x(), origin.y(), bottom_right.x() - origin.x(),
                  bottom_right.y() - origin.y());
  }

  // Fullscreen child widgets are frames, other child widgets are popups.
  if (is_fullscreen) {
    DCHECK_EQ(MSG_ROUTING_NONE, fullscreen_widget_routing_id_);
    view_->StoreFocus();
    fullscreen_widget_process_id_ =
        widget_host_view->GetRenderWidgetHost()->GetProcess()->GetID();
    fullscreen_widget_routing_id_ = route_id;
    if (delegate_ && delegate_->EmbedsFullscreenWidget()) {
      widget_host_view->InitAsChild(GetRenderWidgetHostView()->GetNativeView());
      delegate_->EnterFullscreenModeForTab(GetMainFrame(),
                                           blink::mojom::FullscreenOptions());
    } else {
      widget_host_view->InitAsFullscreen(view);
    }
    observers_.ForEachObserver([&](WebContentsObserver* observer) {
      observer->DidShowFullscreenWidget();
    });
    if (!widget_host_view->HasFocus())
      widget_host_view->Focus();
  } else {
    widget_host_view->InitAsPopup(view, transformed_rect);
  }

  RenderWidgetHostImpl* render_widget_host_impl = widget_host_view->host();
  render_widget_host_impl->Init();
  // Only allow privileged mouse lock for fullscreen render widget, which is
  // used to implement Pepper Flash fullscreen.
  render_widget_host_impl->set_allow_privileged_mouse_lock(is_fullscreen);
}

base::Optional<CreatedWindow> WebContentsImpl::GetCreatedWindow(
    int process_id,
    int main_frame_widget_route_id) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::GetCreatedWindow",
                        "process_id", process_id, "main_frame_widget_route_id",
                        main_frame_widget_route_id);

  auto key = GlobalRoutingID(process_id, main_frame_widget_route_id);
  auto iter = pending_contents_.find(key);

  // Certain systems can block the creation of new windows. If we didn't succeed
  // in creating one, just return NULL.
  if (iter == pending_contents_.end())
    return base::nullopt;

  CreatedWindow result = std::move(iter->second);
  WebContentsImpl* new_contents = result.contents.get();
  pending_contents_.erase(key);
  RemoveDestructionObserver(new_contents);

  // Don't initialize the guest WebContents immediately.
  if (BrowserPluginGuest::IsGuest(new_contents))
    return result;

  if (!new_contents->GetMainFrame()->GetProcess()->IsInitializedAndNotDead() ||
      !new_contents->GetMainFrame()->GetView()) {
    return base::nullopt;
  }

  return result;
}

RenderWidgetHostView* WebContentsImpl::GetCreatedWidget(int process_id,
                                                        int route_id) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::GetCreatedWidget",
                        "process_id", process_id, "route_id", route_id);

  auto iter = pending_widget_views_.find(GlobalRoutingID(process_id, route_id));
  if (iter == pending_widget_views_.end()) {
    DCHECK(false);
    return nullptr;
  }

  RenderWidgetHostView* widget_host_view = iter->second;
  pending_widget_views_.erase(GlobalRoutingID(process_id, route_id));

  RenderWidgetHost* widget_host = widget_host_view->GetRenderWidgetHost();
  if (!widget_host->GetProcess()->IsInitializedAndNotDead()) {
    // The view has gone away or the renderer crashed. Nothing to do.
    return nullptr;
  }

  return widget_host_view;
}

void WebContentsImpl::RequestMediaAccessPermission(
    const MediaStreamRequest& request,
    MediaResponseCallback callback) {
  OPTIONAL_TRACE_EVENT2("content",
                        "WebContentsImpl::RequestMediaAccessPermission",
                        "render_process_id", request.render_process_id,
                        "render_frame_id", request.render_frame_id);

  if (delegate_) {
    delegate_->RequestMediaAccessPermission(this, request, std::move(callback));
  } else {
    std::move(callback).Run(
        blink::MediaStreamDevices(),
        blink::mojom::MediaStreamRequestResult::FAILED_DUE_TO_SHUTDOWN,
        std::unique_ptr<MediaStreamUI>());
  }
}

bool WebContentsImpl::CheckMediaAccessPermission(
    RenderFrameHost* render_frame_host,
    const url::Origin& security_origin,
    blink::mojom::MediaStreamType type) {
  OPTIONAL_TRACE_EVENT2("content",
                        "WebContentsImpl::CheckMediaAccessPermission",
                        "render_frame_host", render_frame_host,
                        "security_origin", security_origin.Serialize());

  DCHECK(type == blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE ||
         type == blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);
  return delegate_ && delegate_->CheckMediaAccessPermission(
                          render_frame_host, security_origin.GetURL(), type);
}

std::string WebContentsImpl::GetDefaultMediaDeviceID(
    blink::mojom::MediaStreamType type) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::GetDefaultMediaDeviceID",
                        "type", static_cast<int>(type));

  if (!delegate_)
    return std::string();
  return delegate_->GetDefaultMediaDeviceID(this, type);
}

SessionStorageNamespace* WebContentsImpl::GetSessionStorageNamespace(
    SiteInstance* instance) {
  return controller_.GetSessionStorageNamespace(instance);
}

SessionStorageNamespaceMap WebContentsImpl::GetSessionStorageNamespaceMap() {
  return controller_.GetSessionStorageNamespaceMap();
}

FrameTree* WebContentsImpl::GetFrameTree() {
  return &frame_tree_;
}

bool WebContentsImpl::IsOverridingUserAgent() {
  return GetController().GetVisibleEntry() &&
         GetController().GetVisibleEntry()->GetIsOverridingUserAgent();
}

bool WebContentsImpl::IsJavaScriptDialogShowing() const {
  return is_showing_javascript_dialog_;
}

bool WebContentsImpl::ShouldIgnoreUnresponsiveRenderer() {
  if (suppress_unresponsive_renderer_count_ > 0)
    return true;

  // Ignore unresponsive renderers if the debugger is attached to them since the
  // unresponsiveness might be a result of the renderer sitting on a breakpoint.
  //
#ifdef OS_WIN
  // Check if a windows debugger is attached to the renderer process.
  base::ProcessHandle process_handle =
      GetMainFrame()->GetProcess()->GetProcess().Handle();
  BOOL debugger_present = FALSE;
  if (CheckRemoteDebuggerPresent(process_handle, &debugger_present) &&
      debugger_present)
    return true;
#endif  // OS_WIN

  // TODO(pfeldman): Fix this to only return true if the renderer is *actually*
  // sitting on a breakpoint. https://crbug.com/684202
  return DevToolsAgentHost::IsDebuggerAttached(this);
}

ui::AXMode WebContentsImpl::GetAccessibilityMode() {
  return accessibility_mode_;
}

void WebContentsImpl::AXTreeIDForMainFrameHasChanged() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::AXTreeIDForMainFrameHasChanged");

  RenderWidgetHostViewBase* rwhv =
      static_cast<RenderWidgetHostViewBase*>(GetRenderWidgetHostView());
  if (rwhv)
    rwhv->SetMainFrameAXTreeID(GetMainFrame()->GetAXTreeID());

  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->AXTreeIDForMainFrameHasChanged();
  });
}

void WebContentsImpl::AccessibilityEventReceived(
    const AXEventNotificationDetails& details) {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::AccessibilityEventReceived");
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->AccessibilityEventReceived(details);
  });
}

void WebContentsImpl::AccessibilityLocationChangesReceived(
    const std::vector<AXLocationChangeNotificationDetails>& details) {
  OPTIONAL_TRACE_EVENT0(
      "content", "WebContentsImpl::AccessibilityLocationChangesReceived");
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->AccessibilityLocationChangesReceived(details);
  });
}

std::string WebContentsImpl::DumpAccessibilityTree(
    bool internal,
    std::vector<AccessibilityTreeFormatter::PropertyFilter> property_filters) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::DumpAccessibilityTree");
  auto* ax_mgr = GetOrCreateRootBrowserAccessibilityManager();
  DCHECK(ax_mgr);
  return AccessibilityTreeFormatterBase::DumpAccessibilityTreeFromManager(
      ax_mgr, internal, property_filters);
}

void WebContentsImpl::RecordAccessibilityEvents(
    bool start_recording,
    base::Optional<AccessibilityEventCallback> callback) {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::RecordAccessibilityEvents");
  // Only pass a callback to RecordAccessibilityEvents when starting to record.
  DCHECK_EQ(start_recording, callback.has_value());
  if (start_recording) {
    SetAccessibilityMode(ui::AXMode::kWebContents);
    auto* ax_mgr = GetOrCreateRootBrowserAccessibilityManager();
    DCHECK(ax_mgr);
    base::ProcessId pid = base::Process::Current().Pid();
    event_recorder_ = content::AccessibilityEventRecorder::Create(ax_mgr, pid);
    event_recorder_->ListenToEvents(*callback);
  } else {
    if (event_recorder_) {
      event_recorder_->FlushAsyncEvents();
      event_recorder_.reset(nullptr);
    }
  }
}

device::mojom::GeolocationContext* WebContentsImpl::GetGeolocationContext() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::GetGeolocationContext");
  if (delegate_) {
    auto* installed_webapp_context =
        delegate_->GetInstalledWebappGeolocationContext();
    if (installed_webapp_context)
      return installed_webapp_context;
  }

  if (!geolocation_context_) {
    GetDeviceService().BindGeolocationContext(
        geolocation_context_.BindNewPipeAndPassReceiver());
  }
  return geolocation_context_.get();
}

device::mojom::WakeLockContext* WebContentsImpl::GetWakeLockContext() {
  if (!wake_lock_context_host_)
    wake_lock_context_host_ = std::make_unique<WakeLockContextHost>(this);
  return wake_lock_context_host_->GetWakeLockContext();
}

#if defined(OS_ANDROID)
void WebContentsImpl::GetNFC(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<device::mojom::NFC> receiver) {
  if (!nfc_host_)
    nfc_host_ = std::make_unique<NFCHost>(this);
  nfc_host_->GetNFC(render_frame_host, std::move(receiver));
}
#endif

void WebContentsImpl::SetNotWaitingForResponse() {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::SetNotWaitingForResponse",
                        "was_waiting_for_response", waiting_for_response_);
  if (waiting_for_response_ == false)
    return;

  waiting_for_response_ = false;
  observers_.ForEachObserver(
      [&](WebContentsObserver* observer) { observer->DidReceiveResponse(); });

  // LoadingStateChanged must be called last in case it triggers deletion of
  // |this| due to recursive message pumps.
  if (delegate_)
    delegate_->LoadingStateChanged(this, is_load_to_different_document_);
}

void WebContentsImpl::SendScreenRects() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::SendScreenRects");
  for (FrameTreeNode* node : frame_tree_.Nodes()) {
    if (node->current_frame_host()->is_local_root())
      node->current_frame_host()->GetRenderWidgetHost()->SendScreenRects();
  }
}

TextInputManager* WebContentsImpl::GetTextInputManager() {
  if (GetOuterWebContents())
    return GetOuterWebContents()->GetTextInputManager();

  if (!text_input_manager_ && !browser_plugin_guest_) {
    text_input_manager_ = std::make_unique<TextInputManager>(
        GetBrowserContext() &&
        !GetBrowserContext()->IsOffTheRecord() /* should_do_learning */);
  }

  return text_input_manager_.get();
}

bool WebContentsImpl::OnUpdateDragCursor() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::OnUpdateDragCursor");
  return browser_plugin_embedder_ &&
         browser_plugin_embedder_->OnUpdateDragCursor();
}

bool WebContentsImpl::IsWidgetForMainFrame(
    RenderWidgetHostImpl* render_widget_host) {
  return render_widget_host == GetMainFrame()->GetRenderWidgetHost();
}

BrowserAccessibilityManager*
WebContentsImpl::GetRootBrowserAccessibilityManager() {
  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(GetMainFrame());
  return rfh ? rfh->browser_accessibility_manager() : nullptr;
}

BrowserAccessibilityManager*
WebContentsImpl::GetOrCreateRootBrowserAccessibilityManager() {
  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(GetMainFrame());
  return rfh ? rfh->GetOrCreateBrowserAccessibilityManager() : nullptr;
}

void WebContentsImpl::ExecuteEditCommand(
    const std::string& command,
    const base::Optional<base::string16>& value) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::ExecuteEditCommand");
  auto* input_handler = GetFocusedFrameWidgetInputHandler();
  if (!input_handler)
    return;

  input_handler->ExecuteEditCommand(command, value);
}

void WebContentsImpl::MoveRangeSelectionExtent(const gfx::Point& extent) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::MoveRangeSelectionExtent");
  auto* input_handler = GetFocusedFrameWidgetInputHandler();
  if (!input_handler)
    return;

  input_handler->MoveRangeSelectionExtent(extent);
}

void WebContentsImpl::SelectRange(const gfx::Point& base,
                                  const gfx::Point& extent) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::SelectRange");
  auto* input_handler = GetFocusedFrameWidgetInputHandler();
  if (!input_handler)
    return;

  input_handler->SelectRange(base, extent);
}

void WebContentsImpl::MoveCaret(const gfx::Point& extent) {
  OPTIONAL_TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("content.verbose"),
                        "WebContentsImpl::MoveCaret");
  auto* input_handler = GetFocusedFrameWidgetInputHandler();
  if (!input_handler)
    return;

  input_handler->MoveCaret(extent);
}

void WebContentsImpl::AdjustSelectionByCharacterOffset(
    int start_adjust,
    int end_adjust,
    bool show_selection_menu) {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::AdjustSelectionByCharacterOffset");
  auto* input_handler = GetFocusedFrameWidgetInputHandler();
  if (!input_handler)
    return;

  using blink::mojom::SelectionMenuBehavior;
  input_handler->AdjustSelectionByCharacterOffset(
      start_adjust, end_adjust,
      show_selection_menu ? SelectionMenuBehavior::kShow
                          : SelectionMenuBehavior::kHide);
}

void WebContentsImpl::UpdatePreferredSize(const gfx::Size& pref_size) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::UpdatePreferredSize");
  const gfx::Size old_size = GetPreferredSize();
  preferred_size_ = pref_size;
  OnPreferredSizeChanged(old_size);
}

void WebContentsImpl::ResizeDueToAutoResize(
    RenderWidgetHostImpl* render_widget_host,
    const gfx::Size& new_size) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::ResizeDueToAutoResize",
                        "render_widget_host", render_widget_host);
  if (render_widget_host != GetRenderViewHost()->GetWidget())
    return;

  if (delegate_)
    delegate_->ResizeDueToAutoResize(this, new_size);
}

WebContents* WebContentsImpl::OpenURL(const OpenURLParams& params) {
  TRACE_EVENT1("content", "WebContentsImpl::OpenURL", "url",
               base::trace_event::ValueToString(params.url));
#if DCHECK_IS_ON()
  DCHECK(params.Valid());
#endif

  if (!delegate_) {
    // Embedder can delay setting a delegate on new WebContents with
    // WebContentsDelegate::ShouldResumeRequestsForCreatedWindow. In the mean
    // time, navigations, including the initial one, that goes through OpenURL
    // should be delayed until embedder is ready to resume loading.
    delayed_open_url_params_ = std::make_unique<OpenURLParams>(params);

    // If there was a navigation deferred when creating the window through
    // CreateNewWindow, drop it in favor of this navigation.
    delayed_load_url_params_.reset();

    return nullptr;
  }

  WebContents* new_contents = delegate_->OpenURLFromTab(this, params);

  RenderFrameHost* source_render_frame_host = RenderFrameHost::FromID(
      params.source_render_process_id, params.source_render_frame_id);

  if (source_render_frame_host && params.source_site_instance) {
    CHECK_EQ(source_render_frame_host->GetSiteInstance(),
             params.source_site_instance.get());
  }

  if (new_contents && source_render_frame_host && new_contents != this) {
    observers_.ForEachObserver([&](WebContentsObserver* observer) {
      observer->DidOpenRequestedURL(
          new_contents, source_render_frame_host, params.url, params.referrer,
          params.disposition, params.transition,
          params.started_from_context_menu, params.is_renderer_initiated);
    });
  }

  return new_contents;
}

void WebContentsImpl::SetHistoryOffsetAndLength(int history_offset,
                                                int history_length) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::SetHistoryOffsetAndLength",
                        "history_offset", history_offset, "history_length",
                        history_length);
  SendPageMessage(new PageMsg_SetHistoryOffsetAndLength(
      MSG_ROUTING_NONE, history_offset, history_length));
}

void WebContentsImpl::SetHistoryOffsetAndLengthForView(
    RenderViewHost* render_view_host,
    int history_offset,
    int history_length) {
  OPTIONAL_TRACE_EVENT2(
      "content", "WebContentsImpl::SetHistoryOffsetAndLengthForView",
      "history_offset", history_offset, "history_length", history_length);
  render_view_host->Send(new PageMsg_SetHistoryOffsetAndLength(
      render_view_host->GetRoutingID(), history_offset, history_length));
}

void WebContentsImpl::ReloadFocusedFrame() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::ReloadFocusedFrame");
  RenderFrameHost* focused_frame = GetFocusedFrame();
  if (!focused_frame)
    return;

  focused_frame->Reload();
}

void WebContentsImpl::Undo() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::Undo");
  auto* input_handler = GetFocusedFrameWidgetInputHandler();
  if (!input_handler)
    return;

  input_handler->Undo();
  RecordAction(base::UserMetricsAction("Undo"));
}

void WebContentsImpl::Redo() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::Redo");
  auto* input_handler = GetFocusedFrameWidgetInputHandler();
  if (!input_handler)
    return;

  input_handler->Redo();
  RecordAction(base::UserMetricsAction("Redo"));
}

void WebContentsImpl::Cut() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::Cut");
  auto* input_handler = GetFocusedFrameWidgetInputHandler();
  if (!input_handler)
    return;

  input_handler->Cut();
  RecordAction(base::UserMetricsAction("Cut"));
}

void WebContentsImpl::Copy() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::Copy");
  auto* input_handler = GetFocusedFrameWidgetInputHandler();
  if (!input_handler)
    return;

  input_handler->Copy();
  RecordAction(base::UserMetricsAction("Copy"));
}

void WebContentsImpl::CopyToFindPboard() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::CopyToFindPboard");
#if defined(OS_MAC)
  auto* input_handler = GetFocusedFrameWidgetInputHandler();
  if (!input_handler)
    return;

  // Windows/Linux don't have the concept of a find pasteboard.
  input_handler->CopyToFindPboard();
  RecordAction(base::UserMetricsAction("CopyToFindPboard"));
#endif
}

void WebContentsImpl::Paste() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::Paste");
  auto* input_handler = GetFocusedFrameWidgetInputHandler();
  if (!input_handler)
    return;

  input_handler->Paste();
  observers_.ForEachObserver(
      [&](WebContentsObserver* observer) { observer->OnPaste(); });
  RecordAction(base::UserMetricsAction("Paste"));
}

void WebContentsImpl::PasteAndMatchStyle() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::PasteAndMatchStyle");
  auto* input_handler = GetFocusedFrameWidgetInputHandler();
  if (!input_handler)
    return;

  input_handler->PasteAndMatchStyle();
  observers_.ForEachObserver(
      [&](WebContentsObserver* observer) { observer->OnPaste(); });
  RecordAction(base::UserMetricsAction("PasteAndMatchStyle"));
}

void WebContentsImpl::Delete() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::Delete");
  auto* input_handler = GetFocusedFrameWidgetInputHandler();
  if (!input_handler)
    return;

  input_handler->Delete();
  RecordAction(base::UserMetricsAction("DeleteSelection"));
}

void WebContentsImpl::SelectAll() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::SelectAll");
  auto* input_handler = GetFocusedFrameWidgetInputHandler();
  if (!input_handler)
    return;

  input_handler->SelectAll();
  RecordAction(base::UserMetricsAction("SelectAll"));
}

void WebContentsImpl::CollapseSelection() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::CollapseSelection");
  auto* input_handler = GetFocusedFrameWidgetInputHandler();
  if (!input_handler)
    return;

  input_handler->CollapseSelection();
}

void WebContentsImpl::Replace(const base::string16& word) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::Replace");
  auto* input_handler = GetFocusedFrameWidgetInputHandler();
  if (!input_handler)
    return;

  input_handler->Replace(word);
}

void WebContentsImpl::ReplaceMisspelling(const base::string16& word) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::ReplaceMisspelling");
  auto* input_handler = GetFocusedFrameWidgetInputHandler();
  if (!input_handler)
    return;

  input_handler->ReplaceMisspelling(word);
}

void WebContentsImpl::NotifyContextMenuClosed(
    const CustomContextMenuContext& context) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::NotifyContextMenuClosed");
  RenderFrameHost* focused_frame = GetFocusedFrame();
  if (!focused_frame)
    return;

  focused_frame->Send(new FrameMsg_ContextMenuClosed(
      focused_frame->GetRoutingID(), context));
}

void WebContentsImpl::ExecuteCustomContextMenuCommand(
    int action, const CustomContextMenuContext& context) {
  OPTIONAL_TRACE_EVENT1("content",
                        "WebContentsImpl::ExecuteCustomContextMenuCommand",
                        "action", action);
  RenderFrameHost* focused_frame = GetFocusedFrame();
  if (!focused_frame)
    return;

  focused_frame->Send(new FrameMsg_CustomContextMenuAction(
      focused_frame->GetRoutingID(), context, action));
}

gfx::NativeView WebContentsImpl::GetNativeView() {
  return view_->GetNativeView();
}

gfx::NativeView WebContentsImpl::GetContentNativeView() {
  return view_->GetContentNativeView();
}

gfx::NativeWindow WebContentsImpl::GetTopLevelNativeWindow() {
  return view_->GetTopLevelNativeWindow();
}

gfx::Rect WebContentsImpl::GetViewBounds() {
  return view_->GetViewBounds();
}

gfx::Rect WebContentsImpl::GetContainerBounds() {
  return view_->GetContainerBounds();
}

DropData* WebContentsImpl::GetDropData() {
  return view_->GetDropData();
}

void WebContentsImpl::Focus() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::Focus");
  view_->Focus();
}

void WebContentsImpl::SetInitialFocus() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::SetInitialFocus");
  view_->SetInitialFocus();
}

void WebContentsImpl::StoreFocus() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::StoreFocus");
  view_->StoreFocus();
}

void WebContentsImpl::RestoreFocus() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::RestoreFocus");
  view_->RestoreFocus();
}

void WebContentsImpl::FocusThroughTabTraversal(bool reverse) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::FocusThroughTabTraversal",
                        "reverse", reverse);
  view_->FocusThroughTabTraversal(reverse);
}

bool WebContentsImpl::IsSavable() {
  // WebKit creates Document object when MIME type is application/xhtml+xml,
  // so we also support this MIME type.
  std::string mime_type = GetContentsMimeType();
  return mime_type == "text/html" || mime_type == "text/xml" ||
         mime_type == "application/xhtml+xml" || mime_type == "text/plain" ||
         mime_type == "text/css" ||
         blink::IsSupportedJavascriptMimeType(mime_type);
}

void WebContentsImpl::OnSavePage() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::OnSavePage");
  // If we can not save the page, try to download it.
  if (!IsSavable()) {
    download::RecordSavePackageEvent(
        download::SAVE_PACKAGE_DOWNLOAD_ON_NON_HTML);
    SaveFrame(GetLastCommittedURL(), Referrer());
    return;
  }

  Stop();

  // Create the save package and possibly prompt the user for the name to save
  // the page as. The user prompt is an asynchronous operation that runs on
  // another thread.
  save_package_ = new SavePackage(this);
  save_package_->GetSaveInfo();
}

// Used in automated testing to bypass prompting the user for file names.
// Instead, the names and paths are hard coded rather than running them through
// file name sanitation and extension / mime checking.
bool WebContentsImpl::SavePage(const base::FilePath& main_file,
                               const base::FilePath& dir_path,
                               SavePageType save_type) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::SavePage", "main_file",
                        base::trace_event::ValueToString(main_file), "dir_path",
                        base::trace_event::ValueToString(dir_path));
  // Stop the page from navigating.
  Stop();

  save_package_ = new SavePackage(this, save_type, main_file, dir_path);
  return save_package_->Init(SavePackageDownloadCreatedCallback());
}

void WebContentsImpl::SaveFrame(const GURL& url, const Referrer& referrer) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::SaveFrame");
  SaveFrameWithHeaders(url, referrer, std::string(), base::string16());
}

void WebContentsImpl::SaveFrameWithHeaders(
    const GURL& url,
    const Referrer& referrer,
    const std::string& headers,
    const base::string16& suggested_filename) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::SaveFrameWithHeaders",
                        "url", base::trace_event::ValueToString(url), "headers",
                        headers);
  // Check and see if the guest can handle this.
  if (delegate_) {
    WebContents* guest_web_contents = nullptr;
    if (browser_plugin_embedder_) {
      BrowserPluginGuest* guest = browser_plugin_embedder_->GetFullPageGuest();
      if (guest)
        guest_web_contents = guest->GetWebContents();
    } else if (browser_plugin_guest_) {
      guest_web_contents = this;
    }

    if (guest_web_contents && delegate_->GuestSaveFrame(guest_web_contents))
      return;
  }

  if (!GetLastCommittedURL().is_valid())
    return;
  if (delegate_ && delegate_->SaveFrame(url, referrer))
    return;

  // TODO(nasko): This check for main frame is incorrect and should be fixed
  // by explicitly passing in which frame this method should target. This would
  // indicate whether it's the main frame, and also tell us the frame pointer
  // to use for routing.
  bool is_main_frame = (url == GetLastCommittedURL());
  RenderFrameHost* frame_host = GetMainFrame();

  int64_t post_id = -1;
  if (is_main_frame) {
    NavigationEntry* entry = controller_.GetLastCommittedEntry();
    if (entry)
      post_id = entry->GetPostID();
  }
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("download_web_contents_frame", R"(
        semantics {
          sender: "Save Page Action"
          description:
            "Saves the given frame's URL to the local file system."
          trigger:
            "The user has triggered a save operation on the frame through a "
            "context menu or other mechanism."
          data: "None."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "This feature cannot be disabled by settings, but it's is only "
            "triggered by user request."
          policy_exception_justification: "Not implemented."
        })");
  auto params = std::make_unique<download::DownloadUrlParameters>(
      url, frame_host->GetProcess()->GetID(),
      frame_host->GetRenderViewHost()->GetRoutingID(),
      frame_host->GetRoutingID(), traffic_annotation);
  params->set_referrer(referrer.url);
  params->set_referrer_policy(
      Referrer::ReferrerPolicyForUrlRequest(referrer.policy));
  params->set_post_id(post_id);
  if (post_id >= 0)
    params->set_method("POST");
  params->set_prompt(true);

  if (headers.empty()) {
    params->set_prefer_cache(true);
  } else {
    for (download::DownloadUrlParameters::RequestHeadersNameValuePair
             key_value : ParseDownloadHeaders(headers)) {
      params->add_request_header(key_value.first, key_value.second);
    }
  }
  params->set_suggested_name(suggested_filename);
  params->set_download_source(download::DownloadSource::WEB_CONTENTS_API);
  BrowserContext::GetDownloadManager(GetBrowserContext())
      ->DownloadUrl(std::move(params));
}

void WebContentsImpl::GenerateMHTML(
    const MHTMLGenerationParams& params,
    base::OnceCallback<void(int64_t)> callback) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::GenerateMHTML");
  base::OnceCallback<void(const MHTMLGenerationResult&)> wrapper_callback =
      base::BindOnce(
          [](base::OnceCallback<void(int64_t)> size_callback,
             const MHTMLGenerationResult& result) {
            std::move(size_callback).Run(result.file_size);
          },
          std::move(callback));
  MHTMLGenerationManager::GetInstance()->SaveMHTML(this, params,
                                                   std::move(wrapper_callback));
}

void WebContentsImpl::GenerateMHTMLWithResult(
    const MHTMLGenerationParams& params,
    MHTMLGenerationResult::GenerateMHTMLCallback callback) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::GenerateMHTMLWithResult");
  MHTMLGenerationManager::GetInstance()->SaveMHTML(this, params,
                                                   std::move(callback));
}

void WebContentsImpl::GenerateWebBundle(
    const base::FilePath& file_path,
    base::OnceCallback<void(uint64_t /* file_size */,
                            data_decoder::mojom::WebBundlerError)> callback) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::GenerateWebBundle",
                        "file_path",
                        base::trace_event::ValueToString(file_path));
  SaveAsWebBundleJob::Start(this, file_path, std::move(callback));
}

const std::string& WebContentsImpl::GetContentsMimeType() {
  return GetRenderViewHost()->contents_mime_type();
}

blink::mojom::RendererPreferences* WebContentsImpl::GetMutableRendererPrefs() {
  return &renderer_preferences_;
}

void WebContentsImpl::Close() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::Close");
  Close(GetRenderViewHost());
}

void WebContentsImpl::DragSourceEndedAt(float client_x,
                                        float client_y,
                                        float screen_x,
                                        float screen_y,
                                        blink::DragOperation operation,
                                        RenderWidgetHost* source_rwh) {
  OPTIONAL_TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("content.verbose"),
                        "WebContentsImpl::DragSourceEndedAt");
  if (browser_plugin_embedder_) {
    browser_plugin_embedder_->DragSourceEndedAt(client_x, client_y, screen_x,
                                                screen_y, operation);
  }
  if (source_rwh) {
    source_rwh->DragSourceEndedAt(gfx::PointF(client_x, client_y),
                                  gfx::PointF(screen_x, screen_y), operation);
  }
}

void WebContentsImpl::LoadStateChanged(
    const std::string& host,
    const net::LoadStateWithParam& load_state,
    uint64_t upload_position,
    uint64_t upload_size) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::LoadStateChanged", "host",
                        host, "load_state", static_cast<int>(load_state.state));
  base::string16 host16 = url_formatter::IDNToUnicode(host);
  // Drop no-op updates.
  if (load_state_.state == load_state.state &&
      load_state_.param == load_state.param &&
      upload_position_ == upload_position && upload_size_ == upload_size &&
      load_state_host_ == host16) {
    return;
  }
  load_state_ = load_state;
  upload_position_ = upload_position;
  upload_size_ = upload_size;
  load_state_host_ = host16;
  if (load_state_.state == net::LOAD_STATE_READING_RESPONSE)
    SetNotWaitingForResponse();
}

void WebContentsImpl::SetVisibilityAndNotifyObservers(Visibility visibility) {
  OPTIONAL_TRACE_EVENT1("content",
                        "WebContentsImpl::SetVisibilityAndNotifyObservers",
                        "visibility", static_cast<int>(visibility));
  const Visibility previous_visibility = visibility_;
  visibility_ = visibility;

  // Notify observers if the visibility changed or if WasShown() is being called
  // for the first time.
  if (visibility != previous_visibility ||
      (visibility == Visibility::VISIBLE && !did_first_set_visible_)) {
    observers_.ForEachObserver([&](WebContentsObserver* observer) {
      observer->OnVisibilityChanged(visibility);
    });
  }
}

void WebContentsImpl::NotifyWebContentsFocused(
    RenderWidgetHost* render_widget_host) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::NotifyWebContentsFocused",
                        "render_widget_host", render_widget_host);
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->OnWebContentsFocused(render_widget_host);
  });
}

void WebContentsImpl::NotifyWebContentsLostFocus(
    RenderWidgetHost* render_widget_host) {
  OPTIONAL_TRACE_EVENT1("content",
                        "WebContentsImpl::NotifyWebContentsLostFocus",
                        "render_widget_host", render_widget_host);
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->OnWebContentsLostFocus(render_widget_host);
  });
}

void WebContentsImpl::SystemDragEnded(RenderWidgetHost* source_rwh) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::SystemDragEnded",
                        "render_widget_host", source_rwh);
  if (source_rwh)
    source_rwh->DragSourceSystemDragEnded();
  if (browser_plugin_embedder_)
    browser_plugin_embedder_->SystemDragEnded();
}

void WebContentsImpl::SetClosedByUserGesture(bool value) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::SetClosedByUserGesture",
                        "value", value);
  closed_by_user_gesture_ = value;
}

bool WebContentsImpl::GetClosedByUserGesture() {
  return closed_by_user_gesture_;
}

int WebContentsImpl::GetMinimumZoomPercent() {
  return minimum_zoom_percent_;
}

int WebContentsImpl::GetMaximumZoomPercent() {
  return maximum_zoom_percent_;
}

void WebContentsImpl::SetPageScale(float scale_factor) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::SetPageScale",
                        "scale_factor", scale_factor);
  GetMainFrame()->GetAssociatedLocalMainFrame()->SetScaleFactor(scale_factor);
}

gfx::Size WebContentsImpl::GetPreferredSize() {
  return IsBeingCaptured() ? preferred_size_for_capture_ : preferred_size_;
}

bool WebContentsImpl::GotResponseToLockMouseRequest(
    blink::mojom::PointerLockResult result) {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::GotResponseToLockMouseRequest");
  if (mouse_lock_widget_) {
    if (mouse_lock_widget_->delegate()->GetAsWebContents() != this) {
      return mouse_lock_widget_->delegate()
          ->GetAsWebContents()
          ->GotResponseToLockMouseRequest(result);
    }

    if (mouse_lock_widget_->GotResponseToLockMouseRequest(result))
      return true;
  }

  for (WebContentsImpl* current = this; current;
       current = current->GetOuterWebContents()) {
    current->mouse_lock_widget_ = nullptr;
  }

  return false;
}

void WebContentsImpl::GotLockMousePermissionResponse(bool allowed) {
  GotResponseToLockMouseRequest(
      allowed ? blink::mojom::PointerLockResult::kSuccess
              : blink::mojom::PointerLockResult::kPermissionDenied);
}

bool WebContentsImpl::GotResponseToKeyboardLockRequest(bool allowed) {
  OPTIONAL_TRACE_EVENT1("content",
                        "WebContentsImpl::GotResponseToKeyboardLockRequest",
                        "allowed", allowed);

  if (!keyboard_lock_widget_)
    return false;

  if (keyboard_lock_widget_->delegate()->GetAsWebContents() != this) {
    NOTREACHED();
    return false;
  }

  // KeyboardLock is only supported when called by the top-level browsing
  // context and is not supported in embedded content scenarios.
  if (GetOuterWebContents())
    return false;

  keyboard_lock_widget_->GotResponseToKeyboardLockRequest(allowed);
  return true;
}

bool WebContentsImpl::HasOpener() {
  return GetOpener() != nullptr;
}

RenderFrameHostImpl* WebContentsImpl::GetOpener() {
  FrameTreeNode* opener_ftn = frame_tree_.root()->opener();
  return opener_ftn ? opener_ftn->current_frame_host() : nullptr;
}

bool WebContentsImpl::HasOriginalOpener() {
  return GetOriginalOpener() != nullptr;
}

RenderFrameHostImpl* WebContentsImpl::GetOriginalOpener() {
  FrameTreeNode* opener_ftn = frame_tree_.root()->original_opener();
  return opener_ftn ? opener_ftn->current_frame_host() : nullptr;
}

void WebContentsImpl::DidChooseColorInColorChooser(SkColor color) {
  OPTIONAL_TRACE_EVENT1("content",
                        "WebContentsImpl::DidChooseColorInColorChooser",
                        "color", color);
  color_chooser_->DidChooseColorInColorChooser(color);
}

void WebContentsImpl::DidEndColorChooser() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::DidEndColorChooser");
  color_chooser_.reset();
}

int WebContentsImpl::DownloadImage(
    const GURL& url,
    bool is_favicon,
    uint32_t preferred_size,
    uint32_t max_bitmap_size,
    bool bypass_cache,
    WebContents::ImageDownloadCallback callback) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::DownloadImage", "url",
                        base::trace_event::ValueToString(url));
  return DownloadImageInFrame(GlobalFrameRoutingId(), url, is_favicon,
                              preferred_size, max_bitmap_size, bypass_cache,
                              std::move(callback));
}

int WebContentsImpl::DownloadImageInFrame(
    const GlobalFrameRoutingId& initiator_frame_routing_id,
    const GURL& url,
    bool is_favicon,
    uint32_t preferred_size,
    uint32_t max_bitmap_size,
    bool bypass_cache,
    WebContents::ImageDownloadCallback callback) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::DownloadImageInFrame");
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  static int next_image_download_id = 0;

  RenderFrameHostImpl* initiator_frame =
      initiator_frame_routing_id.child_id
          ? RenderFrameHostImpl::FromID(initiator_frame_routing_id)
          : GetMainFrame();
  const mojo::Remote<blink::mojom::ImageDownloader>& mojo_image_downloader =
      initiator_frame->GetMojoImageDownloader();
  const int download_id = ++next_image_download_id;
  if (!mojo_image_downloader) {
    // If the renderer process is dead (i.e. crash, or memory pressure on
    // Android), the downloader service will be invalid. Pre-Mojo, this would
    // hang the callback indefinitely since the IPC would be dropped. Now,
    // respond with a 400 HTTP error code to indicate that something went wrong.
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&WebContentsImpl::OnDidDownloadImage,
                       weak_factory_.GetWeakPtr(), std::move(callback),
                       download_id, url, 400, std::vector<SkBitmap>(),
                       std::vector<gfx::Size>()));
    return download_id;
  }

  mojo_image_downloader->DownloadImage(
      url, is_favicon, preferred_size, max_bitmap_size, bypass_cache,
      base::BindOnce(&WebContentsImpl::OnDidDownloadImage,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     download_id, url));
  return download_id;
}

void WebContentsImpl::Find(int request_id,
                           const base::string16& search_text,
                           blink::mojom::FindOptionsPtr options) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::Find");
  // Cowardly refuse to search for no text.
  if (search_text.empty()) {
    NOTREACHED();
    return;
  }

  GetOrCreateFindRequestManager()->Find(request_id, search_text,
                                        std::move(options));
}

void WebContentsImpl::StopFinding(StopFindAction action) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::StopFinding");
  if (FindRequestManager* manager = GetFindRequestManager())
    manager->StopFinding(action);
}

bool WebContentsImpl::WasEverAudible() {
  return was_ever_audible_;
}

void WebContentsImpl::GetManifest(GetManifestCallback callback) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::GetManifest");
  // TODO(yuzus, 1061899): Move this function to RenderFrameHostImpl.
  ManifestManagerHost* manifest_manager_host =
      ManifestManagerHost::GetOrCreateForCurrentDocument(GetMainFrame());
  manifest_manager_host->GetManifest(std::move(callback));
}

void WebContentsImpl::ExitFullscreen(bool will_cause_resize) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::ExitFullscreen");
  // Clean up related state and initiate the fullscreen exit.
  GetRenderViewHost()->GetWidget()->RejectMouseLockOrUnlockIfNecessary(
      blink::mojom::PointerLockResult::kUserRejected);
  ExitFullscreenMode(will_cause_resize);
}

base::ScopedClosureRunner WebContentsImpl::ForSecurityDropFullscreen(
    int64_t display_id) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::ForSecurityDropFullscreen",
                        "display_id", display_id);
  // Kick WebContentses that are "related" to this WebContents out of
  // fullscreen. This needs to be done with two passes, because it is simple to
  // walk _up_ the chain of openers and outer contents, but it not simple to
  // walk _down_ the chain.

  // First, determine if any WebContents that is in fullscreen has this
  // WebContents as an upstream contents. Drop that WebContents out of
  // fullscreen if it does. This is theoretically quadratic-ish (fullscreen
  // contentses x each one's opener length) but neither of those is expected to
  // ever be a large number.

  auto* screen = display::Screen::GetScreen();

  auto fullscreen_set_copy = *FullscreenContentsSet(GetBrowserContext());
  for (auto* fullscreen_contents : fullscreen_set_copy) {
    // Checking IsFullscreen() for tabs in the fullscreen set may seem
    // redundant, but teeeeechnically fullscreen is run by the delegate, and
    // it's possible that the delegate's notion of fullscreen may have changed
    // outside of WebContents's notice.
    if (fullscreen_contents->IsFullscreen() &&
        (display_id == display::kInvalidDisplayId || !screen ||
         display_id ==
             screen->GetDisplayNearestView(fullscreen_contents->GetNativeView())
                 .id())) {
      auto opener_contentses = GetAllOpeningWebContents(fullscreen_contents);
      if (opener_contentses.count(this))
        fullscreen_contents->ExitFullscreen(true);
    }
  }

  // Second, walk upstream from this WebContents, and drop the fullscreen of
  // all WebContentses that are in fullscreen. Block all the WebContentses in
  // the chain from entering fullscreen while the returned closure runner is
  // alive. It's OK that this set doesn't contain downstream WebContentses, as
  // any request to enter fullscreen will have the upstream of the WebContents
  // checked. (See CanEnterFullscreenMode().)

  std::vector<base::WeakPtr<WebContentsImpl>> blocked_contentses;

  for (auto* opener : GetAllOpeningWebContents(this)) {
    // Drop fullscreen if the WebContents is in it, and...
    if (opener->IsFullscreen() &&
        (display_id == display::kInvalidDisplayId || !screen ||
         display_id ==
             screen->GetDisplayNearestView(opener->GetNativeView()).id()))
      opener->ExitFullscreen(true);

    // ...block the WebContents from entering fullscreen until further notice.
    ++opener->fullscreen_blocker_count_;
    blocked_contentses.push_back(opener->weak_factory_.GetWeakPtr());
  }

  return base::ScopedClosureRunner(base::BindOnce(
      [](std::vector<base::WeakPtr<WebContentsImpl>> blocked_contentses) {
        for (base::WeakPtr<WebContentsImpl>& web_contents :
             blocked_contentses) {
          if (web_contents) {
            DCHECK_GT(web_contents->fullscreen_blocker_count_, 0);
            --web_contents->fullscreen_blocker_count_;
          }
        }
      },
      std::move(blocked_contentses)));
}

void WebContentsImpl::ResumeLoadingCreatedWebContents() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::ResumeLoadingCreatedWebContents");
  if (delayed_load_url_params_.get()) {
    DCHECK(!delayed_open_url_params_);
    controller_.LoadURLWithParams(*delayed_load_url_params_.get());
    delayed_load_url_params_.reset(nullptr);
    return;
  }

  if (delayed_open_url_params_.get()) {
    OpenURL(*delayed_open_url_params_.get());
    delayed_open_url_params_.reset(nullptr);
    return;
  }

  // Resume blocked requests for both the RenderViewHost and RenderFrameHost.
  // TODO(brettw): It seems bogus to reach into here and initialize the host.
  if (is_resume_pending_) {
    is_resume_pending_ = false;
    GetRenderViewHost()->GetWidget()->Init();
    GetMainFrame()->Init();
  }
}

bool WebContentsImpl::FocusLocationBarByDefault() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::FocusLocationBarByDefault");
  if (should_focus_location_bar_by_default_)
    return true;

  return delegate_ && delegate_->ShouldFocusLocationBarByDefault(this);
}

void WebContentsImpl::SetFocusToLocationBar() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::SetFocusToLocationBar");
  if (delegate_)
    delegate_->SetFocusToLocationBar();
}

void WebContentsImpl::DidStartNavigation(NavigationHandle* navigation_handle) {
  TRACE_EVENT1("navigation", "WebContentsImpl::DidStartNavigation",
               "navigation_handle", navigation_handle);
  if (navigation_handle->IsInMainFrame())
    favicon_urls_.clear();

  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->DidStartNavigation(navigation_handle);
  });
  if (navigation_handle->IsInMainFrame()) {
    // When the browser is started with about:blank as the startup URL, focus
    // the location bar (which will also select its contents) so people can
    // simply begin typing to navigate elsewhere.
    //
    // We need to be careful not to trigger this for anything other than the
    // startup navigation. In particular, if we allow an attacker to open a
    // popup to about:blank, then navigate, focusing the Omnibox will cause the
    // end of the new URL to be scrolled into view instead of the start,
    // allowing the attacker to spoof other URLs. The conditions checked here
    // are all aimed at ensuring no such attacker-controlled navigation can
    // trigger this.
    should_focus_location_bar_by_default_ =
        controller_.IsInitialNavigation() &&
        !navigation_handle->IsRendererInitiated() &&
        navigation_handle->GetURL() == url::kAboutBlankURL;
  }
}

void WebContentsImpl::DidRedirectNavigation(
    NavigationHandle* navigation_handle) {
  TRACE_EVENT1("navigation", "WebContentsImpl::DidRedirectNavigation",
               "navigation_handle", navigation_handle);
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->DidRedirectNavigation(navigation_handle);
  });
  // Notify accessibility if this is a reload. This has to called on the
  // BrowserAccessibilityManager associated with the old RFHI.
  if (navigation_handle->GetReloadType() != ReloadType::NONE) {
    NavigationRequest* request = NavigationRequest::From(navigation_handle);
    BrowserAccessibilityManager* manager =
        request->frame_tree_node()
            ->current_frame_host()
            ->browser_accessibility_manager();
    if (manager)
      manager->UserIsReloading();
  }
}

void WebContentsImpl::ReadyToCommitNavigation(
    NavigationHandle* navigation_handle) {
  TRACE_EVENT1("navigation", "WebContentsImpl::ReadyToCommitNavigation",
               "navigation_handle", navigation_handle);
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->ReadyToCommitNavigation(navigation_handle);
  });
  // If any domains are blocked from accessing 3D APIs because they may
  // have caused the GPU to reset recently, unblock them here if the user
  // initiated this navigation. This implies that the user was involved in
  // the decision to navigate, so there's no concern about
  // denial-of-service issues. Want to do this as early as
  // possible to avoid race conditions with pages attempting to access
  // WebGL early on.
  //
  // TODO(crbug.com/617904): currently navigations initiated by the browser
  // (reload button, reload menu option, pressing return in the Omnibox)
  // return false from HasUserGesture(). If or when that is addressed,
  // remove the check for IsRendererInitiated() below.
  //
  // TODO(crbug.com/832180): HasUserGesture comes from the renderer
  // process and isn't validated. Until it is, don't trust it.
  if (!navigation_handle->IsRendererInitiated()) {
    GpuDataManagerImpl::GetInstance()->UnblockDomainFrom3DAPIs(
        navigation_handle->GetURL());
  }

  if (navigation_handle->IsSameDocument())
    return;

  // SSLInfo is not needed on subframe navigations since the main-frame
  // certificate is the only one that can be inspected (using the info
  // bubble) without refreshing the page with DevTools open.
  // We don't call DidStartResourceResponse on net errors, since that results on
  // existing cert exceptions being revoked, which leads to weird behavior with
  // committed interstitials or while offline. We only need the error check for
  // the main frame case because unlike this method, SubresourceResponseStarted
  // does not get called on network errors.
  if (navigation_handle->IsInMainFrame() &&
      navigation_handle->GetNetErrorCode() == net::OK) {
    controller_.ssl_manager()->DidStartResourceResponse(
        navigation_handle->GetURL(),
        navigation_handle->GetSSLInfo().has_value()
            ? net::IsCertStatusError(
                  navigation_handle->GetSSLInfo()->cert_status)
            : false);
  }

  // LoadingStateChanged must be called last in case it triggers deletion of
  // |this| due to recursive message pumps.
  SetNotWaitingForResponse();
}

void WebContentsImpl::DidFinishNavigation(NavigationHandle* navigation_handle) {
  TRACE_EVENT1("navigation", "WebContentsImpl::DidFinishNavigation",
               "navigation_handle", navigation_handle);

  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->DidFinishNavigation(navigation_handle);
  });
  if (display_cutout_host_impl_)
    display_cutout_host_impl_->DidFinishNavigation(navigation_handle);

  if (navigation_handle->HasCommitted()) {
    // TODO(domfarolino, dmazzoni): Do this using WebContentsObserver. See
    // https://crbug.com/981271.
    BrowserAccessibilityManager* manager =
        static_cast<RenderFrameHostImpl*>(
            navigation_handle->GetRenderFrameHost())
            ->browser_accessibility_manager();
    if (manager) {
      if (navigation_handle->IsErrorPage()) {
        manager->NavigationFailed();
      } else {
        manager->NavigationSucceeded();
      }
    }

    if (navigation_handle->IsInMainFrame()) {
      last_committed_source_id_including_same_document_ =
          ukm::ConvertToSourceId(navigation_handle->GetNavigationId(),
                                 ukm::SourceIdType::NAVIGATION_ID);

      if (!navigation_handle->IsSameDocument()) {
        was_ever_audible_ = false;
      }
    }

    if (!navigation_handle->IsSameDocument())
      last_screen_orientation_change_time_ = base::TimeTicks();
  }

  // If we didn't end up on about:blank after setting this in DidStartNavigation
  // then don't focus the location bar.
  if (should_focus_location_bar_by_default_ &&
      navigation_handle->GetURL() != url::kAboutBlankURL) {
    should_focus_location_bar_by_default_ = false;
  }

  if (navigation_handle->IsInMainFrame() && first_navigation_completed_)
    RecordMaxFrameCountUMA(max_loaded_frame_count_);

  // If navigation has successfully finished in the main frame, set
  // |first_navigation_completed_| to true so that we will record
  // |max_loaded_frame_count_| above when future main frame navigations finish.
  if (navigation_handle->IsInMainFrame() && !navigation_handle->IsErrorPage()) {
    first_navigation_completed_ = true;

    // Navigation has completed in main frame. Reset |max_loaded_frame_count_|.
    // |max_loaded_frame_count_| is not necessarily 1 if the navigation was
    // served from BackForwardCache.
    max_loaded_frame_count_ =
        GetMainFrame()->frame_tree_node()->GetFrameTreeSize();
  }

  if (web_preferences_) {
    // Update the WebPreferences for this WebContents that depends on changes
    // that might occur during navigation. This will only update the preferences
    // that needs to be updated (and won't cause an update/overwrite preferences
    // that needs to stay the same after navigations).
    bool value_changed_due_to_override =
        GetContentClient()->browser()->OverrideWebPreferencesAfterNavigation(
            this, web_preferences_.get());
    // We need to update the WebPreferences value on the renderer if the value
    // is changed due to the override above, or if the navigation is served from
    // the back-forward cache, because the WebPreferences value stored in the
    // renderer might be stale (because we don't send WebPreferences updates to
    // bfcached renderers).
    // TODO(rakina): Maybe handle the back-forward cache case in
    // ReadyToCommitNavigation instead?
    if (value_changed_due_to_override ||
        NavigationRequest::From(navigation_handle)
            ->IsServedFromBackForwardCache()) {
      SetWebPreferences(*web_preferences_.get());
    }
  }
}

void WebContentsImpl::DidFailLoadWithError(
    RenderFrameHostImpl* render_frame_host,
    const GURL& url,
    int error_code) {
  TRACE_EVENT2("content,navigation", "WebContentsImpl::DidFailLoadWithError",
               "render_frame_host",
               base::trace_event::ToTracedValue(render_frame_host), "url",
               base::trace_event::ValueToString(url));
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->DidFailLoad(render_frame_host, url, error_code);
  });
}

void WebContentsImpl::NotifyChangedNavigationState(
    InvalidateTypes changed_flags) {
  NotifyNavigationStateChanged(changed_flags);
}

bool WebContentsImpl::ShouldTransferNavigation(bool is_main_frame_navigation) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::ShouldTransferNavigation",
                        "is_main_frame_navigation", is_main_frame_navigation);
  if (!delegate_)
    return true;
  return delegate_->ShouldTransferNavigation(is_main_frame_navigation);
}

bool WebContentsImpl::ShouldPreserveAbortedURLs() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::ShouldPreserveAbortedURLs");
  if (!delegate_)
    return false;
  return delegate_->ShouldPreserveAbortedURLs(this);
}

void WebContentsImpl::DidNavigateMainFramePreCommit(
    bool navigation_is_within_page) {
  TRACE_EVENT1("content,navigation",
               "WebContentsImpl::DidNavigateMainFramePreCommit",
               "navigation_is_within_page", navigation_is_within_page);
  // Ensure fullscreen mode is exited before committing the navigation to a
  // different page.  The next page will not start out assuming it is in
  // fullscreen mode.
  if (navigation_is_within_page) {
    // No page change?  Then, the renderer and browser can remain in fullscreen.
    return;
  }
  if (IsFullscreen())
    ExitFullscreen(false);
  DCHECK(!IsFullscreen());

  // Clean up keyboard lock state when navigating.
  CancelKeyboardLock(keyboard_lock_widget_);
}

void WebContentsImpl::DidNavigateMainFramePostCommit(
    RenderFrameHostImpl* render_frame_host,
    const LoadCommittedDetails& details,
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params) {
  OPTIONAL_TRACE_EVENT1(
      "content,navigation", "WebContentsImpl::DidNavigateMainFramePostCommit",
      "render_frame_host", base::trace_event::ToTracedValue(render_frame_host));
  if (details.is_navigation_to_different_page()) {
    // Clear the status bubble. This is a workaround for a bug where WebKit
    // doesn't let us know that the cursor left an element during a
    // transition (this is also why the mouse cursor remains as a hand after
    // clicking on a link); see bugs 1184641 and 980803. We don't want to
    // clear the bubble when a user navigates to a named anchor in the same
    // page.
    ClearTargetURL();

    RenderWidgetHostViewBase* rwhvb =
        static_cast<RenderWidgetHostViewBase*>(GetRenderWidgetHostView());
    if (rwhvb)
      rwhvb->OnDidNavigateMainFrameToNewPage();
  }

  if (delegate_)
    delegate_->DidNavigateMainFramePostCommit(this);
  view_->SetOverscrollControllerEnabled(CanOverscrollContent());

  // The following events will not fire again if the page is restored from the
  // BackForwardCache. So fire them ourselves if needed.
  if (details.is_navigation_to_different_page() &&
      GetRenderViewHost()->did_first_visually_non_empty_paint()) {
    DidFirstVisuallyNonEmptyPaint(GetRenderViewHost());
  }
  if (GetRenderViewHost()->theme_color() != last_sent_theme_color_)
    OnThemeColorChanged(GetRenderViewHost());
  if (GetRenderViewHost()->background_color() != last_sent_background_color_)
    OnBackgroundColorChanged(GetRenderViewHost());
}

void WebContentsImpl::DidNavigateAnyFramePostCommit(
    RenderFrameHostImpl* render_frame_host,
    const LoadCommittedDetails& details,
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params) {
  OPTIONAL_TRACE_EVENT1(
      "content,navigation", "WebContentsImpl::DidNavigateAnyFramePostCommit",
      "render_frame_host", base::trace_event::ToTracedValue(render_frame_host));
  // Now that something has committed, we don't need to track whether the
  // initial page has been accessed.
  has_accessed_initial_document_ = false;

  // If we navigate off the page, close all JavaScript dialogs.
  if (!details.is_same_document)
    CancelActiveAndPendingDialogs();

  // If this is a user-initiated navigation, start allowing JavaScript dialogs
  // again.
  if (params.gesture == NavigationGestureUser && dialog_manager_) {
    dialog_manager_->CancelDialogs(this, /*reset_state=*/true);
  }
}

bool WebContentsImpl::CanOverscrollContent() const {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::CanOverscrollContent");
  // Disable overscroll when touch emulation is on. See crbug.com/369938.
  if (force_disable_overscroll_content_)
    return false;

  if (delegate_)
    return delegate_->CanOverscrollContent();

  return false;
}

void WebContentsImpl::OnThemeColorChanged(RenderViewHostImpl* source) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::OnThemeColorChanged",
                        "render_view_host", source);
  if (source->did_first_visually_non_empty_paint() &&
      last_sent_theme_color_ != source->theme_color()) {
    observers_.ForEachObserver([&](WebContentsObserver* observer) {
      observer->DidChangeThemeColor();
    });
    last_sent_theme_color_ = source->theme_color();
  }
}

void WebContentsImpl::OnBackgroundColorChanged(RenderViewHostImpl* source) {
  if (source->did_first_visually_non_empty_paint() &&
      last_sent_background_color_ != source->background_color()) {
    observers_.ForEachObserver([&](WebContentsObserver* observer) {
      observer->OnBackgroundColorChanged();
    });
    last_sent_background_color_ = source->background_color();
  }
}

void WebContentsImpl::DidLoadResourceFromMemoryCache(
    RenderFrameHostImpl* source,
    const GURL& url,
    const std::string& http_method,
    const std::string& mime_type,
    network::mojom::RequestDestination request_destination) {
  OPTIONAL_TRACE_EVENT2(
      "content", "WebContentsImpl::DidLoadResourceFromMemoryCache",
      "render_frame_host", base::trace_event::ToTracedValue(source), "url",
      base::trace_event::ValueToString(url));
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->DidLoadResourceFromMemoryCache(url, mime_type,
                                             request_destination);
  });

  if (url.is_valid() && url.SchemeIsHTTPOrHTTPS()) {
    StoragePartition* partition = source->GetProcess()->GetStoragePartition();
    partition->GetNetworkContext()->NotifyExternalCacheHit(
        url, http_method, source->GetNetworkIsolationKey());
  }
}

void WebContentsImpl::DidDisplayInsecureContent() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::DidDisplayInsecureContent");
  controller_.ssl_manager()->DidDisplayMixedContent();
}

void WebContentsImpl::DidContainInsecureFormAction() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::DidContainInsecureFormAction");
  controller_.ssl_manager()->DidContainInsecureFormAction();
}

void WebContentsImpl::DocumentAvailableInMainFrame() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::DocumentAvailableInMainFrame");
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->DocumentAvailableInMainFrame();
  });
}

void WebContentsImpl::OnDidRunInsecureContent(RenderFrameHostImpl* source,
                                              const GURL& security_origin,
                                              const GURL& target_url) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::OnDidRunInsecureContent",
                        "render_frame_host",
                        base::trace_event::ToTracedValue(source));
  // TODO(nick, estark): Should we call FilterURL using |source|'s process on
  // these parameters? |target_url| seems unused, except for a log message. And
  // |security_origin| might be replaceable with the origin of the main frame.
  DidRunInsecureContent(security_origin, target_url);
}

void WebContentsImpl::DidRunInsecureContent(const GURL& security_origin,
                                            const GURL& target_url) {
  OPTIONAL_TRACE_EVENT2(
      "content", "WebContentsImpl::DidRunInsecureContent", "security_origin",
      base::trace_event::ValueToString(security_origin), "target_url",
      base::trace_event::ValueToString(target_url));
  LOG(WARNING) << security_origin << " ran insecure content from "
               << target_url.possibly_invalid_spec();
  RecordAction(base::UserMetricsAction("SSL.RanInsecureContent"));
  if (base::EndsWith(security_origin.spec(), kDotGoogleDotCom,
                     base::CompareCase::INSENSITIVE_ASCII))
    RecordAction(base::UserMetricsAction("SSL.RanInsecureContentGoogle"));
  controller_.ssl_manager()->DidRunMixedContent(security_origin);
}

void WebContentsImpl::PassiveInsecureContentFound(const GURL& resource_url) {
  OPTIONAL_TRACE_EVENT1(
      "content", "WebContentsImpl::PassiveInsecureContentFound", "resource_url",
      base::trace_event::ValueToString(resource_url));
  if (delegate_) {
    delegate_->PassiveInsecureContentFound(resource_url);
  }
}

bool WebContentsImpl::ShouldAllowRunningInsecureContent(
    bool allowed_per_prefs,
    const url::Origin& origin,
    const GURL& resource_url) {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::ShouldAllowRunningInsecureContent");
  if (delegate_) {
    return delegate_->ShouldAllowRunningInsecureContent(this, allowed_per_prefs,
                                                        origin, resource_url);
  }

  return allowed_per_prefs;
}

void WebContentsImpl::ViewSource(RenderFrameHostImpl* frame) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::ViewSource",
                        "render_frame_host",
                        base::trace_event::ToTracedValue(frame));
  DCHECK_EQ(this, WebContents::FromRenderFrameHost(frame));

  // Don't do anything if there is no |delegate_| that could accept and show the
  // new WebContents containing the view-source.
  if (!delegate_)
    return;

  // Use the last committed entry, since the pending entry hasn't loaded yet and
  // won't be copied into the cloned tab.
  NavigationEntryImpl* last_committed_entry =
      GetController().GetLastCommittedEntry();
  if (!last_committed_entry)
    return;

  FrameNavigationEntry* frame_entry =
      last_committed_entry->GetFrameEntry(frame->frame_tree_node());
  if (!frame_entry)
    return;

  // Any new WebContents opened while this WebContents is in fullscreen can be
  // used to confuse the user, so drop fullscreen.
  base::ScopedClosureRunner fullscreen_block = ForSecurityDropFullscreen();
  // The new view source contents will be independent of this contents, so
  // release the fullscreen block.
  fullscreen_block.RunAndReset();

  // We intentionally don't share the SiteInstance with the original frame so
  // that view source has a consistent process model and always ends up in a new
  // process (https://crbug.com/699493).
  scoped_refptr<SiteInstanceImpl> site_instance_for_view_source = nullptr;
  // Referrer and initiator are not important, because view-source should not
  // hit the network, but should be served from the cache instead.
  Referrer referrer_for_view_source;
  base::Optional<url::Origin> initiator_for_view_source = base::nullopt;
  // Do not restore title, derive it from the url.
  base::string16 title_for_view_source;
  auto navigation_entry = std::make_unique<NavigationEntryImpl>(
      site_instance_for_view_source, frame_entry->url(),
      referrer_for_view_source, initiator_for_view_source,
      title_for_view_source, ui::PAGE_TRANSITION_LINK,
      /* is_renderer_initiated = */ false,
      /* blob_url_loader_factory = */ nullptr);
  const GURL url(content::kViewSourceScheme + std::string(":") +
                 frame_entry->url().spec());
  navigation_entry->SetVirtualURL(url);
  navigation_entry->set_isolation_info(
      frame->GetIsolationInfoForSubresources());

  // Do not restore scroller position.
  // TODO(creis, lukasza, arthursonzogni): Do not reuse the original PageState,
  // but start from a new one and only copy the needed data.
  const PageState& new_page_state =
      frame_entry->page_state().RemoveScrollOffset();

  scoped_refptr<FrameNavigationEntry> new_frame_entry =
      navigation_entry->root_node()->frame_entry;
  new_frame_entry->set_method(frame_entry->method());
  new_frame_entry->SetPageState(new_page_state);

  // Create a new WebContents, which is used to display the source code.
  std::unique_ptr<WebContents> view_source_contents =
      Create(CreateParams(GetBrowserContext()));

  // Restore the previously created NavigationEntry.
  std::vector<std::unique_ptr<NavigationEntry>> navigation_entries;
  navigation_entries.push_back(std::move(navigation_entry));
  view_source_contents->GetController().Restore(0, RestoreType::CURRENT_SESSION,
                                                &navigation_entries);

  // Add |view_source_contents| as a new tab.
  gfx::Rect initial_rect;
  constexpr bool kUserGesture = true;
  bool ignored_was_blocked;
  delegate_->AddNewContents(this, std::move(view_source_contents), url,
                            WindowOpenDisposition::NEW_FOREGROUND_TAB,
                            initial_rect, kUserGesture, &ignored_was_blocked);
  // Note that the |delegate_| could have deleted |view_source_contents| during
  // AddNewContents method call.
}

void WebContentsImpl::SubresourceResponseStarted(const GURL& url,
                                                 net::CertStatus cert_status) {
  OPTIONAL_TRACE_EVENT1("content",
                        "WebContentsImpl::SubresourceResponseStarted", "url",
                        base::trace_event::ValueToString(url));
  controller_.ssl_manager()->DidStartResourceResponse(url, cert_status);
  SetNotWaitingForResponse();
}

void WebContentsImpl::ResourceLoadComplete(
    RenderFrameHost* render_frame_host,
    const GlobalRequestID& request_id,
    blink::mojom::ResourceLoadInfoPtr resource_load_info) {
  OPTIONAL_TRACE_EVENT2(
      "content", "WebContentsImpl::ResourceLoadComplete", "render_frame_host",
      base::trace_event::ToTracedValue(render_frame_host), "request_id",
      base::trace_event::TracedValue::Build(
          {{"child_id", request_id.child_id},
           {"request_id", request_id.request_id}}));
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->ResourceLoadComplete(render_frame_host, request_id,
                                   *resource_load_info);
  });
}

const blink::web_pref::WebPreferences&
WebContentsImpl::GetOrCreateWebPreferences() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::GetOrCreateWebPreferences");
  // Compute WebPreferences based on the current state if it's null.
  if (!web_preferences_)
    OnWebPreferencesChanged();
  return *web_preferences_.get();
}

bool WebContentsImpl::IsWebPreferencesSet() const {
  return web_preferences_.get();
}

void WebContentsImpl::SetWebPreferences(
    const blink::web_pref::WebPreferences& prefs) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::SetWebPreferences");
  web_preferences_ = std::make_unique<blink::web_pref::WebPreferences>(prefs);
  // Get all the RenderViewHosts (except the ones for currently back-forward
  // cached pages), and make them send the current WebPreferences
  // to the renderer. WebPreferences updates for back-forward cached pages will
  // be sent when we restore those pages from the back-forward cache.
  for (auto& rvh : frame_tree_.render_view_hosts()) {
    rvh.second->SendWebPreferencesToRenderer();
  }
}

void WebContentsImpl::RecomputeWebPreferencesSlow() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::RecomputeWebPreferencesSlow");
  // OnWebPreferencesChanged is a no-op when this is true.
  if (updating_web_preferences_)
    return;
  // Resets |web_preferences_| so that we won't have any cached value for slow
  // attributes (which won't get recomputed if we have pre-existing values for
  // them).
  web_preferences_.reset();
  OnWebPreferencesChanged();
}

void WebContentsImpl::PrintCrossProcessSubframe(
    const gfx::Rect& rect,
    int document_cookie,
    RenderFrameHost* subframe_host) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::PrintCrossProcessSubframe",
                        "subframe",
                        base::trace_event::ToTracedValue(subframe_host));
  auto* outer_contents = GetOuterWebContents();
  if (outer_contents) {
    // When an extension or app page is printed, the content should be
    // composited with outer content, so the outer contents should handle the
    // print request.
    outer_contents->PrintCrossProcessSubframe(rect, document_cookie,
                                              subframe_host);
    return;
  }

  // If there is no delegate such as in tests or during deletion, do nothing.
  if (!delegate_)
    return;

  delegate_->PrintCrossProcessSubframe(this, rect, document_cookie,
                                       subframe_host);
}

void WebContentsImpl::CapturePaintPreviewOfCrossProcessSubframe(
    const gfx::Rect& rect,
    const base::UnguessableToken& guid,
    RenderFrameHost* render_frame_host) {
  OPTIONAL_TRACE_EVENT1(
      "content", "WebContentsImpl::CapturePaintPreviewOfCrossProcessSubframe",
      "render_frame_host", base::trace_event::ToTracedValue(render_frame_host));
  if (!delegate_)
    return;
  delegate_->CapturePaintPreviewOfSubframe(this, rect, guid, render_frame_host);
}

#if defined(OS_ANDROID)
base::android::ScopedJavaLocalRef<jobject>
WebContentsImpl::GetJavaRenderFrameHostDelegate() {
  return GetJavaWebContents();
}
#endif

void WebContentsImpl::OnDidDisplayContentWithCertificateErrors() {
  OPTIONAL_TRACE_EVENT0(
      "content", "WebContentsImpl::OnDidDisplayContentWithCertificateErrors");
  controller_.ssl_manager()->DidDisplayContentWithCertErrors();
}

void WebContentsImpl::OnDidRunContentWithCertificateErrors(
    RenderFrameHostImpl* source) {
  OPTIONAL_TRACE_EVENT1(
      "content", "WebContentsImpl::OnDidRunContentWithCertificateErrors",
      "render_frame_host", base::trace_event::ToTracedValue(source));
  // For RenderFrameHosts that are inactive and going to be discarded, we can
  // disregard this message; there's no need to update the UI if the UI will
  // never be shown again.
  //
  // We still process this message for speculative RenderFrameHosts. This can
  // happen when a subframe's main resource has a certificate error. The
  // currently committed navigation entry will get marked as having run insecure
  // content and that will carry over to the navigation entry for the
  // speculative RFH when it commits.
  if (source->lifecycle_state() !=
          RenderFrameHostImpl::LifecycleState::kSpeculative &&
      source->IsInactiveAndDisallowReactivation()) {
    return;
  }
  controller_.ssl_manager()->DidRunContentWithCertErrors(
      source->GetMainFrame()->GetLastCommittedOrigin().GetURL());
}

void WebContentsImpl::DOMContentLoaded(RenderFrameHost* render_frame_host) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::DOMContentLoaded",
                        "render_frame_host",
                        base::trace_event::ToTracedValue(render_frame_host));
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->DOMContentLoaded(render_frame_host);
  });
}

void WebContentsImpl::OnDidFinishLoad(RenderFrameHost* render_frame_host,
                                      const GURL& url) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::OnDidFinishLoad",
                        "render_frame_host",
                        base::trace_event::ToTracedValue(render_frame_host),
                        "url", base::trace_event::ValueToString(url));
  GURL validated_url(url);
  render_frame_host->GetProcess()->FilterURL(false, &validated_url);

  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->DidFinishLoad(render_frame_host, validated_url);
  });
  size_t tree_size = frame_tree_.root()->GetFrameTreeSize();
  if (max_loaded_frame_count_ < tree_size)
    max_loaded_frame_count_ = tree_size;

  if (!render_frame_host->GetParent())
    UMA_HISTOGRAM_COUNTS_1000("Navigation.MainFrame.FrameCount", tree_size);
}

void WebContentsImpl::OnGoToEntryAtOffset(RenderFrameHostImpl* source,
                                          int32_t offset,
                                          bool has_user_gesture) {
  OPTIONAL_TRACE_EVENT2(
      "content", "WebContentsImpl::OnGoToEntryAtOffset", "render_frame_host",
      base::trace_event::ToTracedValue(source), "offset", offset);
  // Non-user initiated navigations coming from the renderer should be ignored
  // if there is an ongoing browser-initiated navigation.
  // See https://crbug.com/879965.
  // TODO(arthursonzogni): See if this should check for ongoing navigations in
  // the frame(s) affected by the session history navigation, rather than just
  // the main frame.
  if (Navigator::ShouldIgnoreIncomingRendererRequest(
          frame_tree_.root()->navigation_request(), has_user_gesture))
    return;

  // All frames are allowed to navigate the global history.
  if (!delegate_ || delegate_->OnGoToEntryOffset(offset)) {
    if (source->IsSandboxed(network::mojom::WebSandboxFlags::kTopNavigation)) {
      // Keep track of whether this is a session history from a sandboxed iframe
      // with top level navigation disallowed.
      controller_.GoToOffsetInSandboxedFrame(offset,
                                             source->GetFrameTreeNodeId());
    } else {
      controller_.GoToOffset(offset);
    }
  }
}

void WebContentsImpl::OnPageScaleFactorChanged(RenderFrameHostImpl* source,
                                               float page_scale_factor) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::OnPageScaleFactorChanged",
                        "render_frame_host",
                        base::trace_event::ToTracedValue(source),
                        "page_scale_factor", page_scale_factor);
#if !defined(OS_ANDROID)
  // While page scale factor is used on mobile, this PageScaleFactorIsOne logic
  // is only needed on desktop.
  bool is_one = page_scale_factor == 1.f;
  if (is_one != page_scale_factor_is_one_) {
    page_scale_factor_is_one_ = is_one;

    HostZoomMapImpl* host_zoom_map =
        static_cast<HostZoomMapImpl*>(HostZoomMap::GetForWebContents(this));

    if (host_zoom_map) {
      host_zoom_map->SetPageScaleFactorIsOneForView(
          source->GetProcess()->GetID(),
          source->GetRenderViewHost()->GetRoutingID(),
          page_scale_factor_is_one_);
    }
  }
#endif  // !defined(OS_ANDROID)

  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->OnPageScaleFactorChanged(page_scale_factor);
  });
}

void WebContentsImpl::OnTextAutosizerPageInfoChanged(
    RenderFrameHostImpl* source,
    blink::mojom::TextAutosizerPageInfoPtr page_info) {
  OPTIONAL_TRACE_EVENT1(
      "content", "WebContentsImpl::OnTextAutosizerPageInfoChanged",
      "render_frame_host", base::trace_event::ToTracedValue(source));
  // Keep a copy of |page_info| in case we create a new RenderView before
  // the next update.
  text_autosizer_page_info_.main_frame_width = page_info->main_frame_width;
  text_autosizer_page_info_.main_frame_layout_width =
      page_info->main_frame_layout_width;
  text_autosizer_page_info_.device_scale_adjustment =
      page_info->device_scale_adjustment;

  auto remote_frames_broadcast_callback = base::BindRepeating(
      [](const blink::mojom::TextAutosizerPageInfo& page_info,
         RenderFrameProxyHost* proxy_host) {
        DCHECK(proxy_host);
        proxy_host->GetAssociatedRemoteMainFrame()->UpdateTextAutosizerPageInfo(
            page_info.Clone());
      },
      text_autosizer_page_info_);

  frame_tree_.root()->render_manager()->ExecuteRemoteFramesBroadcastMethod(
      std::move(remote_frames_broadcast_callback), source->GetSiteInstance());
}

void WebContentsImpl::EnumerateDirectory(
    RenderFrameHost* render_frame_host,
    scoped_refptr<FileChooserImpl::FileSelectListenerImpl> listener,
    const base::FilePath& directory_path) {
  OPTIONAL_TRACE_EVENT2(
      "content", "WebContentsImpl::EnumerateDirectory", "render_frame_host",
      base::trace_event::ToTracedValue(render_frame_host), "directory_path",
      base::trace_event::ValueToString(directory_path));
  // Any explicit focusing of another window while this WebContents is in
  // fullscreen can be used to confuse the user, so drop fullscreen.
  base::ScopedClosureRunner fullscreen_block = ForSecurityDropFullscreen();
  listener->SetFullscreenBlock(std::move(fullscreen_block));

  if (delegate_)
    delegate_->EnumerateDirectory(this, std::move(listener), directory_path);
  else
    listener->FileSelectionCanceled();
}

void WebContentsImpl::RegisterProtocolHandler(RenderFrameHostImpl* source,
                                              const std::string& protocol,
                                              const GURL& url,
                                              const base::string16& title,
                                              bool user_gesture) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::RegisterProtocolHandler",
                        "render_frame_host",
                        base::trace_event::ToTracedValue(source), "protocol",
                        protocol);
  // TODO(nick): Do we need to apply FilterURL to |url|?
  if (!delegate_)
    return;

  if (!AreValidRegisterProtocolHandlerArguments(
          protocol, url, source->GetLastCommittedOrigin())) {
    ReceivedBadMessage(source->GetProcess(),
                       bad_message::REGISTER_PROTOCOL_HANDLER_INVALID_URL);
    return;
  }

  delegate_->RegisterProtocolHandler(source, protocol, url, user_gesture);
}

void WebContentsImpl::UnregisterProtocolHandler(RenderFrameHostImpl* source,
                                                const std::string& protocol,
                                                const GURL& url,
                                                bool user_gesture) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::UnregisterProtocolHandler",
                        "render_frame_host",
                        base::trace_event::ToTracedValue(source), "protocol",
                        protocol);
  // TODO(nick): Do we need to apply FilterURL to |url|?
  if (!delegate_)
    return;

  if (!AreValidRegisterProtocolHandlerArguments(
          protocol, url, source->GetLastCommittedOrigin())) {
    ReceivedBadMessage(source->GetProcess(),
                       bad_message::REGISTER_PROTOCOL_HANDLER_INVALID_URL);
    return;
  }

  delegate_->UnregisterProtocolHandler(source, protocol, url, user_gesture);
}

void WebContentsImpl::OnAppCacheAccessed(const GURL& manifest_url,
                                         bool blocked_by_policy) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::OnAppCacheAccessed");
  // TODO(nick): Should we consider |source| here? Should we call FilterURL on
  // |manifest_url|?

  // Notify observers about navigation.
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->AppCacheAccessed(manifest_url, blocked_by_policy);
  });
}

void WebContentsImpl::DomOperationResponse(const std::string& json_string) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::DomOperationResponse",
                        "json_string", json_string);

  // TODO(lukasza): The notification below should probably indicate which
  // RenderFrameHostImpl the message is coming from. This can enable tests to
  // talk to 2 frames at the same time, without being confused which frame a
  // given response comes from.  See also a corresponding TODO in the
  // ExecuteScriptHelper in //content/public/test/browser_test_utils.cc
  NotificationService::current()->Notify(
      NOTIFICATION_DOM_OPERATION_RESPONSE, Source<WebContents>(this),
      Details<const std::string>(&json_string));
}

void WebContentsImpl::SavableResourceLinksResponse(
    RenderFrameHostImpl* source,
    const std::vector<GURL>& resources_list,
    blink::mojom::ReferrerPtr referrer,
    const std::vector<blink::mojom::SavableSubframePtr>& subframes) {
  OPTIONAL_TRACE_EVENT1(
      "content", "WebContentsImpl::SavableResourceLinksResponse",
      "render_frame_host", base::trace_event::ToTracedValue(source));
  save_package_->SavableResourceLinksResponse(source, resources_list,
                                              std::move(referrer), subframes);
}

void WebContentsImpl::SavableResourceLinksError(RenderFrameHostImpl* source) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::SavableResourceLinksError",
                        "render_frame_host",
                        base::trace_event::ToTracedValue(source));
  save_package_->SavableResourceLinksError(source);
}

void WebContentsImpl::OnServiceWorkerAccessed(
    RenderFrameHost* render_frame_host,
    const GURL& scope,
    AllowServiceWorkerResult allowed) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::OnServiceWorkerAccessed",
                        "render_frame_host",
                        base::trace_event::ToTracedValue(render_frame_host),
                        "scope", base::trace_event::ValueToString(scope));
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->OnServiceWorkerAccessed(render_frame_host, scope, allowed);
  });
}

void WebContentsImpl::OnServiceWorkerAccessed(
    NavigationHandle* navigation,
    const GURL& scope,
    AllowServiceWorkerResult allowed) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::OnServiceWorkerAccessed",
                        "navigation_handle", navigation, "scope",
                        base::trace_event::ValueToString(scope));
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->OnServiceWorkerAccessed(navigation, scope, allowed);
  });
}

void WebContentsImpl::OnColorChooserFactoryReceiver(
    mojo::PendingReceiver<blink::mojom::ColorChooserFactory> receiver) {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::OnColorChooserFactoryReceiver");
  color_chooser_factory_receivers_.Add(this, std::move(receiver));
}

void WebContentsImpl::OpenColorChooser(
    mojo::PendingReceiver<blink::mojom::ColorChooser> chooser_receiver,
    mojo::PendingRemote<blink::mojom::ColorChooserClient> client,
    SkColor color,
    std::vector<blink::mojom::ColorSuggestionPtr> suggestions) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::OpenColorChooser");
  content::ColorChooser* new_color_chooser =
      delegate_ ? delegate_->OpenColorChooser(this, color, suggestions)
                : nullptr;
  if (!new_color_chooser)
    return;

  color_chooser_.reset();
  color_chooser_ = std::make_unique<ColorChooser>(
      new_color_chooser, std::move(chooser_receiver), std::move(client));
}

#if BUILDFLAG(ENABLE_PLUGINS)
void WebContentsImpl::OnPepperInstanceCreated(RenderFrameHostImpl* source,
                                              int32_t pp_instance) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::OnPepperInstanceCreated",
                        "render_frame_host",
                        base::trace_event::ToTracedValue(source));
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->PepperInstanceCreated();
  });
  pepper_playback_observer_->PepperInstanceCreated(source, pp_instance);
}

void WebContentsImpl::OnPepperInstanceDeleted(RenderFrameHostImpl* source,
                                              int32_t pp_instance) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::OnPepperInstanceDeleted",
                        "render_frame_host",
                        base::trace_event::ToTracedValue(source));
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->PepperInstanceDeleted();
  });
  pepper_playback_observer_->PepperInstanceDeleted(source, pp_instance);
}

void WebContentsImpl::OnPepperPluginHung(RenderFrameHostImpl* source,
                                         int plugin_child_id,
                                         const base::FilePath& path,
                                         bool is_hung) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::OnPepperPluginHung",
                        "render_frame_host",
                        base::trace_event::ToTracedValue(source));
  UMA_HISTOGRAM_COUNTS_1M("Pepper.PluginHung", 1);

  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->PluginHungStatusChanged(plugin_child_id, path, is_hung);
  });
}

void WebContentsImpl::OnPepperStartsPlayback(RenderFrameHostImpl* source,
                                             int32_t pp_instance) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::OnPepperStartsPlayback",
                        "render_frame_host",
                        base::trace_event::ToTracedValue(source));
  pepper_playback_observer_->PepperStartsPlayback(source, pp_instance);
}

void WebContentsImpl::OnPepperStopsPlayback(RenderFrameHostImpl* source,
                                            int32_t pp_instance) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::OnPepperStopsPlayback",
                        "render_frame_host",
                        base::trace_event::ToTracedValue(source));
  pepper_playback_observer_->PepperStopsPlayback(source, pp_instance);
}

void WebContentsImpl::OnPluginCrashed(RenderFrameHostImpl* source,
                                      const base::FilePath& plugin_path,
                                      base::ProcessId plugin_pid) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::OnPluginCrashed",
                        "render_frame_host",
                        base::trace_event::ToTracedValue(source));
  // TODO(nick): Eliminate the |plugin_pid| parameter, which can't be trusted,
  // and is only used by WebTestControlHost.
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->PluginCrashed(plugin_path, plugin_pid);
  });
}

void WebContentsImpl::OnRequestPpapiBrokerPermission(
    RenderViewHostImpl* source,
    int ppb_broker_route_id,
    const GURL& url,
    const base::FilePath& plugin_path) {
  OPTIONAL_TRACE_EVENT1("content",
                        "WebContentsImpl::OnRequestPpapiBrokerPermission",
                        "render_view_host", source);
  base::OnceCallback<void(bool)> permission_result_callback = base::BindOnce(
      &WebContentsImpl::SendPpapiBrokerPermissionResult, base::Unretained(this),
      source->GetProcess()->GetID(), ppb_broker_route_id);
  if (!delegate_) {
    std::move(permission_result_callback).Run(false);
    return;
  }

  delegate_->RequestPpapiBrokerPermission(
      this, url, plugin_path, std::move(permission_result_callback));
}

void WebContentsImpl::SendPpapiBrokerPermissionResult(int process_id,
                                                      int ppb_broker_route_id,
                                                      bool result) {
  OPTIONAL_TRACE_EVENT2(
      "content", "WebContentsImpl::SendPpapiBrokerPermissionResult",
      "process_id", process_id, "ppb_broker_route_id", ppb_broker_route_id);
  RenderProcessHost* rph = RenderProcessHost::FromID(process_id);
  if (rph) {
    // TODO(nick): Convert this from ViewMsg_ to a Ppapi msg, since it
    // is not routed to a RenderView.
    rph->Send(
        new ViewMsg_PpapiBrokerPermissionResult(ppb_broker_route_id, result));
  }
}
#endif  // BUILDFLAG(ENABLE_PLUGINS)

void WebContentsImpl::UpdateFaviconURL(
    RenderFrameHost* source,
    std::vector<blink::mojom::FaviconURLPtr> candidates) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::UpdateFaviconURL",
                        "render_frame_host",
                        base::trace_event::ToTracedValue(source));
  // Ignore favicons for non-main frame.
  if (source->GetParent()) {
    NOTREACHED();
    return;
  }

  // We get updated favicon URLs after the page stops loading. If a cross-site
  // navigation occurs while a page is still loading, the initial page
  // may stop loading and send us updated favicon URLs after the navigation
  // for the new page has committed.
  if (!source->IsCurrent())
    return;

  favicon_urls_ = std::move(candidates);

  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->DidUpdateFaviconURL(source, favicon_urls_);
  });
}

void WebContentsImpl::SetIsOverlayContent(bool is_overlay_content) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::SetIsOverlayContent",
                        "is_overlay_content", is_overlay_content);
  is_overlay_content_ = is_overlay_content;
}

void WebContentsImpl::DidFirstVisuallyNonEmptyPaint(
    RenderViewHostImpl* source) {
  OPTIONAL_TRACE_EVENT1("content",
                        "WebContentsImpl::DidFirstVisuallyNonEmptyPaint",
                        "render_view_host", source);
  // TODO(nick): When this is ported to FrameHostMsg_, we should only listen if
  // |source| is the main frame.
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->DidFirstVisuallyNonEmptyPaint();
  });
  if (source->theme_color() != last_sent_theme_color_) {
    // Theme color should have updated by now if there was one.
    observers_.ForEachObserver([&](WebContentsObserver* observer) {
      observer->DidChangeThemeColor();
    });
    last_sent_theme_color_ = source->theme_color();
  }

  if (source->background_color() != last_sent_background_color_) {
    // Background color should have updated by now if there was one.
    observers_.ForEachObserver([&](WebContentsObserver* observer) {
      observer->OnBackgroundColorChanged();
    });
    last_sent_background_color_ = source->background_color();
  }
}

bool WebContentsImpl::IsPortal() {
  return portal();
}

WebContentsImpl* WebContentsImpl::GetPortalHostWebContents() {
  return portal() ? portal()->GetPortalHostContents() : nullptr;
}

void WebContentsImpl::NotifyBeforeFormRepostWarningShow() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::NotifyBeforeFormRepostWarningShow");
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->BeforeFormRepostWarningShow();
  });
}

void WebContentsImpl::ActivateAndShowRepostFormWarningDialog() {
  OPTIONAL_TRACE_EVENT0(
      "content", "WebContentsImpl::ActivateAndShowRepostFormWarningDialog");
  Activate();
  if (delegate_)
    delegate_->ShowRepostFormWarningDialog(this);
}

bool WebContentsImpl::HasAccessedInitialDocument() {
  return has_accessed_initial_document_;
}

void WebContentsImpl::UpdateTitleForEntry(NavigationEntry* entry,
                                          const base::string16& title) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::UpdateTitleForEntry",
                        "title", base::trace_event::ValueToString(title));
  base::string16 final_title;
  base::TrimWhitespace(title, base::TRIM_ALL, &final_title);

  // If a page is created via window.open and never navigated,
  // there will be no navigation entry. In this situation,
  // |page_title_when_no_navigation_entry_| will be used for page title.
  if (entry) {
    if (final_title == entry->GetTitle())
      return;  // Nothing changed, don't bother.

    entry->SetTitle(final_title);

    // The title for display may differ from the title just set; grab it.
    final_title = entry->GetTitleForDisplay();
  } else {
    if (page_title_when_no_navigation_entry_ == final_title)
      return;  // Nothing changed, don't bother.

    page_title_when_no_navigation_entry_ = final_title;
  }

  // Lastly, set the title for the view.
  view_->SetPageTitle(final_title);

  observers_.ForEachObserver(
      [&](WebContentsObserver* observer) { observer->TitleWasSet(entry); });
  // Broadcast notifications when the UI should be updated.
  if (entry == controller_.GetEntryAtOffset(0))
    NotifyNavigationStateChanged(INVALIDATE_TYPE_TITLE);
}

void WebContentsImpl::SendChangeLoadProgress() {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::SendChangeLoadProgress",
                        "load_progress", frame_tree_.load_progress());
  loading_last_progress_update_ = base::TimeTicks::Now();
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->LoadProgressChanged(frame_tree_.load_progress());
  });
}

void WebContentsImpl::ResetLoadProgressState() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::ResetLoadProgressState");
  frame_tree_.ResetLoadProgress();
  loading_weak_factory_.InvalidateWeakPtrs();
  loading_last_progress_update_ = base::TimeTicks();
}

// Notifies the RenderWidgetHost instance about the fact that the page is
// loading, or done loading.
void WebContentsImpl::LoadingStateChanged(bool to_different_document,
                                          LoadNotificationDetails* details) {
  if (is_being_destroyed_)
    return;

  bool is_loading = IsLoading();

  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::LoadingStateChanged",
                        "is_loading", is_loading);

  if (!is_loading) {
    load_state_ = net::LoadStateWithParam(net::LOAD_STATE_IDLE,
                                          base::string16());
    load_state_host_.clear();
    upload_size_ = 0;
    upload_position_ = 0;
  }

  GetRenderManager()->SetIsLoading(is_loading);

  waiting_for_response_ = is_loading;
  is_load_to_different_document_ = to_different_document;

  if (delegate_)
    delegate_->LoadingStateChanged(this, to_different_document);
  NotifyNavigationStateChanged(INVALIDATE_TYPE_LOAD);

  std::string url = (details ? details->url.possibly_invalid_spec() : "NULL");
  if (is_loading) {
    TRACE_EVENT_ASYNC_BEGIN2("browser,navigation", "WebContentsImpl Loading",
                             this, "URL", url, "Main FrameTreeNode id",
                             GetFrameTree()->root()->frame_tree_node_id());
    observers_.ForEachObserver(
        [&](WebContentsObserver* observer) { observer->DidStartLoading(); });
  } else {
    TRACE_EVENT_ASYNC_END1("browser,navigation", "WebContentsImpl Loading",
                           this, "URL", url);
    observers_.ForEachObserver(
        [&](WebContentsObserver* observer) { observer->DidStopLoading(); });
  }

  // TODO(avi): Remove. http://crbug.com/170921
  int type = is_loading ? NOTIFICATION_LOAD_START : NOTIFICATION_LOAD_STOP;
  NotificationDetails det = NotificationService::NoDetails();
  if (details)
      det = Details<LoadNotificationDetails>(details);
  NotificationService::current()->Notify(
      type, Source<NavigationController>(&controller_), det);
}

void WebContentsImpl::NotifyViewSwapped(RenderViewHost* old_view,
                                        RenderViewHost* new_view) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::NotifyViewSwapped",
                        "old_view", old_view, "new_view", new_view);
  DCHECK_NE(old_view, new_view);
  // After sending out a swap notification, we need to send a disconnect
  // notification so that clients that pick up a pointer to |this| can NULL the
  // pointer.  See Bug 1230284.
  notify_disconnection_ = true;
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->RenderViewHostChanged(old_view, new_view);
  });
  view_->RenderViewHostChanged(old_view, new_view);

  // If this is an inner WebContents that has swapped views, we need to reattach
  // it to its outer WebContents.
  if (node_.outer_web_contents())
    ReattachToOuterWebContentsFrame();

  // Ensure that the associated embedder gets cleared after a RenderViewHost
  // gets swapped, so we don't reuse the same embedder next time a
  // RenderViewHost is attached to this WebContents.
  RemoveBrowserPluginEmbedder();
}

void WebContentsImpl::NotifyFrameSwapped(RenderFrameHost* old_frame,
                                         RenderFrameHost* new_frame,
                                         bool is_main_frame) {
  TRACE_EVENT2("content", "WebContentsImpl::NotifyFrameSwapped", "old_frame",
               base::trace_event::ToTracedValue(old_frame), "new_frame",
               base::trace_event::ToTracedValue(new_frame));
#if defined(OS_ANDROID)
  // Copy importance from |old_frame| if |new_frame| is a main frame.
  if (old_frame && !new_frame->GetParent()) {
    RenderWidgetHostImpl* old_widget =
        static_cast<RenderFrameHostImpl*>(old_frame)->GetRenderWidgetHost();
    RenderWidgetHostImpl* new_widget =
        static_cast<RenderFrameHostImpl*>(new_frame)->GetRenderWidgetHost();
    new_widget->SetImportance(old_widget->importance());
  }
#endif
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->RenderFrameHostChanged(old_frame, new_frame);
  });
}

// TODO(avi): Remove this entire function because this notification is already
// covered by two observer functions. http://crbug.com/170921
void WebContentsImpl::NotifyDisconnected() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::NotifyDisconnected");
  if (!notify_disconnection_)
    return;

  notify_disconnection_ = false;
  NotificationService::current()->Notify(
      NOTIFICATION_WEB_CONTENTS_DISCONNECTED,
      Source<WebContents>(this),
      NotificationService::NoDetails());
}

void WebContentsImpl::NotifyNavigationEntryCommitted(
    const LoadCommittedDetails& load_details) {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::NotifyNavigationEntryCommitted");
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->NavigationEntryCommitted(load_details);
  });
}

void WebContentsImpl::NotifyNavigationEntryChanged(
    const EntryChangedDetails& change_details) {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::NotifyNavigationEntryChanged");
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->NavigationEntryChanged(change_details);
  });
}

void WebContentsImpl::NotifyNavigationListPruned(
    const PrunedDetails& pruned_details) {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::NotifyNavigationListPruned");
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->NavigationListPruned(pruned_details);
  });
}

void WebContentsImpl::NotifyNavigationEntriesDeleted() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::NotifyNavigationEntriesDeleted");
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->NavigationEntriesDeleted();
  });
}

void WebContentsImpl::OnAssociatedInterfaceRequest(
    RenderFrameHost* render_frame_host,
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  OPTIONAL_TRACE_EVENT2(
      "content", "WebContentsImpl::OnAssociatedInterfaceRequest",
      "render_frame_host", base::trace_event::ToTracedValue(render_frame_host),
      "interface_name", interface_name);
  auto it = receiver_sets_.find(interface_name);
  if (it != receiver_sets_.end())
    it->second->OnReceiverForFrame(render_frame_host, std::move(handle));
}

void WebContentsImpl::OnInterfaceRequest(
    RenderFrameHost* render_frame_host,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle* interface_pipe) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::OnInterfaceRequest",
                        "render_frame_host",
                        base::trace_event::ToTracedValue(render_frame_host),
                        "interface_name", interface_name);
  for (auto& observer : observers_.observer_list()) {
    observer.OnInterfaceRequestFromFrame(render_frame_host, interface_name,
                                         interface_pipe);
    if (!interface_pipe->is_valid())
      break;
  }
}

void WebContentsImpl::OnDidBlockNavigation(
    const GURL& blocked_url,
    const GURL& initiator_url,
    blink::mojom::NavigationBlockedReason reason) {
  OPTIONAL_TRACE_EVENT1(
      "content", "WebContentsImpl::OnDidBlockNavigation", "details",
      base::trace_event::TracedValue::Build(
          {{"blocked_url", base::trace_event::ValueToString(blocked_url)},
           {"initiator_url", base::trace_event::ValueToString(initiator_url)},
           {"reason", base::trace_event::ValueToString(reason)}}));
  if (delegate_)
    delegate_->OnDidBlockNavigation(this, blocked_url, initiator_url, reason);
}

const GURL& WebContentsImpl::GetMainFrameLastCommittedURL() {
  return GetLastCommittedURL();
}

void WebContentsImpl::RenderFrameCreated(RenderFrameHost* render_frame_host) {
  TRACE_EVENT1("content", "WebContentsImpl::RenderFrameCreated",
               "render_frame_host", render_frame_host);
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->RenderFrameCreated(render_frame_host);
  });
  UpdateAccessibilityModeOnFrame(render_frame_host);

  if (display_cutout_host_impl_)
    display_cutout_host_impl_->RenderFrameCreated(render_frame_host);

  if (!render_frame_host->IsRenderFrameLive() || render_frame_host->GetParent())
    return;

  NavigationEntry* entry = controller_.GetPendingEntry();
  if (entry && entry->IsViewSourceMode()) {
    // Put the renderer in view source mode.
    static_cast<RenderFrameHostImpl*>(render_frame_host)
        ->GetAssociatedLocalFrame()
        ->EnableViewSourceMode();
  }
}

void WebContentsImpl::RenderFrameDeleted(RenderFrameHost* render_frame_host) {
  TRACE_EVENT1("content", "WebContentsImpl::RenderFrameDeleted",
               "render_frame_host",
               base::trace_event::ToTracedValue(render_frame_host));
  if (IsBeingDestroyed() && !render_frame_host->GetParent() &&
      first_navigation_completed_ &&
      !render_frame_host->IsInBackForwardCache()) {
    // Main frame has been deleted because WebContents is being destroyed.
    // Note that we aren't recording this here when the main frame is in the
    // back-forward cache because that means we've actually already navigated
    // away from it (and we got to this point because the WebContents is
    // deleted), which means |max_loaded_frame_count_| is already overwritten.
    // The |max_loaded_frame_count_| value will instead be recorded from within
    // |WebContentsImpl::DidFinishNavigation()|.
    RecordMaxFrameCountUMA(max_loaded_frame_count_);
  }

  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->RenderFrameDeleted(render_frame_host);
  });
#if BUILDFLAG(ENABLE_PLUGINS)
  pepper_playback_observer_->RenderFrameDeleted(render_frame_host);
#endif

  if (display_cutout_host_impl_)
    display_cutout_host_impl_->RenderFrameDeleted(render_frame_host);

  // Remove any fullscreen state that the frame has stored.
  FullscreenStateChanged(render_frame_host, false /* is_fullscreen */);
}

void WebContentsImpl::ShowContextMenu(RenderFrameHost* render_frame_host,
                                      const ContextMenuParams& params) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::ShowContextMenu",
                        "render_frame_host",
                        base::trace_event::ToTracedValue(render_frame_host));
  // If a renderer fires off a second command to show a context menu before the
  // first context menu is closed, just ignore it. https://crbug.com/707534
  if (showing_context_menu_)
    return;

  ContextMenuParams context_menu_params(params);
  // Allow WebContentsDelegates to handle the context menu operation first.
  if (delegate_ &&
      delegate_->HandleContextMenu(render_frame_host, context_menu_params))
    return;

  render_view_host_delegate_view_->ShowContextMenu(render_frame_host,
                                                   context_menu_params);
}

namespace {
// Normalizes the line endings: \r\n -> \n, lone \r -> \n.
base::string16 NormalizeLineBreaks(const base::string16& source) {
  static const base::NoDestructor<base::string16> kReturnNewline(
      base::ASCIIToUTF16("\r\n"));
  static const base::NoDestructor<base::string16> kReturn(
      base::ASCIIToUTF16("\r"));
  static const base::NoDestructor<base::string16> kNewline(
      base::ASCIIToUTF16("\n"));

  std::vector<base::StringPiece16> pieces;

  for (const auto& rn_line : base::SplitStringPieceUsingSubstr(
           source, *kReturnNewline, base::KEEP_WHITESPACE,
           base::SPLIT_WANT_ALL)) {
    auto r_lines = base::SplitStringPieceUsingSubstr(
        rn_line, *kReturn, base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    std::move(std::begin(r_lines), std::end(r_lines),
              std::back_inserter(pieces));
  }

  return base::JoinString(pieces, *kNewline);
}
}  // namespace

void WebContentsImpl::RunJavaScriptDialog(
    RenderFrameHost* render_frame_host,
    const base::string16& message,
    const base::string16& default_prompt,
    JavaScriptDialogType dialog_type,
    JavaScriptDialogCallback response_callback) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::RunJavaScriptDialog",
                        "render_frame_host",
                        base::trace_event::ToTracedValue(render_frame_host));
  // Ensure that if showing a dialog is the first thing that a page does, that
  // the contents of the previous page aren't shown behind it. This is required
  // because showing a dialog freezes the renderer, so no frames will be coming
  // from it. https://crbug.com/823353
  auto* render_widget_host_impl =
      static_cast<RenderFrameHostImpl*>(render_frame_host)
          ->GetRenderWidgetHost();
  if (render_widget_host_impl)
    render_widget_host_impl->ForceFirstFrameAfterNavigationTimeout();

  // Running a dialog causes an exit to webpage-initiated fullscreen.
  // http://crbug.com/728276
  base::ScopedClosureRunner fullscreen_block = ForSecurityDropFullscreen();

  auto callback =
      base::BindOnce(&WebContentsImpl::OnDialogClosed, base::Unretained(this),
                     render_frame_host->GetProcess()->GetID(),
                     render_frame_host->GetRoutingID(),
                     std::move(response_callback), std::move(fullscreen_block));

  std::vector<protocol::PageHandler*> page_handlers =
      protocol::PageHandler::EnabledForWebContents(this);

  if (delegate_)
    dialog_manager_ = delegate_->GetJavaScriptDialogManager(this);

  // While a JS message dialog is showing, defer commits in this WebContents.
  javascript_dialog_navigation_deferrer_ =
      std::make_unique<JavaScriptDialogNavigationDeferrer>();

  // Suppress JavaScript dialogs when requested.
  bool should_suppress = delegate_ && delegate_->ShouldSuppressDialogs(this);
  bool has_non_devtools_handlers = delegate_ && dialog_manager_;
  bool has_handlers = page_handlers.size() || has_non_devtools_handlers;
  bool suppress_this_message = should_suppress || !has_handlers;

  if (suppress_this_message) {
    std::move(callback).Run(true, false, base::string16());
    return;
  }

  scoped_refptr<CloseDialogCallbackWrapper> wrapper =
      new CloseDialogCallbackWrapper(std::move(callback));

  is_showing_javascript_dialog_ = true;

  base::string16 normalized_message = NormalizeLineBreaks(message);

  for (auto* handler : page_handlers) {
    handler->DidRunJavaScriptDialog(
        render_frame_host->GetLastCommittedURL(), normalized_message,
        default_prompt, dialog_type, has_non_devtools_handlers,
        base::BindOnce(&CloseDialogCallbackWrapper::Run, wrapper, false));
  }

  if (dialog_manager_) {
    dialog_manager_->RunJavaScriptDialog(
        this, render_frame_host, dialog_type, normalized_message,
        default_prompt,
        base::BindOnce(&CloseDialogCallbackWrapper::Run, wrapper, false),
        &suppress_this_message);
  }

  if (suppress_this_message) {
    // If we are suppressing messages, just reply as if the user immediately
    // pressed "Cancel", passing true to |dialog_was_suppressed|.
    wrapper->Run(true, false, base::string16());
  }
}

void WebContentsImpl::RunBeforeUnloadConfirm(
    RenderFrameHost* render_frame_host,
    bool is_reload,
    JavaScriptDialogCallback response_callback) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::RunBeforeUnloadConfirm",
                        "render_frame_host",
                        base::trace_event::ToTracedValue(render_frame_host),
                        "is_reload", is_reload);
  // Ensure that if showing a dialog is the first thing that a page does, that
  // the contents of the previous page aren't shown behind it. This is required
  // because showing a dialog freezes the renderer, so no frames will be coming
  // from it. https://crbug.com/823353
  auto* render_widget_host_impl =
      static_cast<RenderFrameHostImpl*>(render_frame_host)
          ->GetRenderWidgetHost();
  if (render_widget_host_impl)
    render_widget_host_impl->ForceFirstFrameAfterNavigationTimeout();

  // Running a dialog causes an exit to webpage-initiated fullscreen.
  // http://crbug.com/728276
  base::ScopedClosureRunner fullscreen_block = ForSecurityDropFullscreen();

  RenderFrameHostImpl* rfhi =
      static_cast<RenderFrameHostImpl*>(render_frame_host);
  if (delegate_)
    delegate_->WillRunBeforeUnloadConfirm();

  auto callback =
      base::BindOnce(&WebContentsImpl::OnDialogClosed, base::Unretained(this),
                     render_frame_host->GetProcess()->GetID(),
                     render_frame_host->GetRoutingID(),
                     std::move(response_callback), std::move(fullscreen_block));

  std::vector<protocol::PageHandler*> page_handlers =
      protocol::PageHandler::EnabledForWebContents(this);

  if (delegate_)
    dialog_manager_ = delegate_->GetJavaScriptDialogManager(this);

  // While a JS beforeunload dialog is showing, defer commits in this
  // WebContents.
  javascript_dialog_navigation_deferrer_ =
      std::make_unique<JavaScriptDialogNavigationDeferrer>();

  bool should_suppress = !rfhi->IsCurrent() ||
                         (delegate_ && delegate_->ShouldSuppressDialogs(this));
  bool has_non_devtools_handlers = delegate_ && dialog_manager_;
  bool has_handlers = page_handlers.size() || has_non_devtools_handlers;
  if (should_suppress || !has_handlers) {
    std::move(callback).Run(false, true, base::string16());
    return;
  }

  is_showing_before_unload_dialog_ = true;

  scoped_refptr<CloseDialogCallbackWrapper> wrapper =
      new CloseDialogCallbackWrapper(std::move(callback));

  GURL frame_url = rfhi->GetLastCommittedURL();
  for (auto* handler : page_handlers) {
    handler->DidRunBeforeUnloadConfirm(
        frame_url, has_non_devtools_handlers,
        base::BindOnce(&CloseDialogCallbackWrapper::Run, wrapper, false));
  }

  if (dialog_manager_) {
    dialog_manager_->RunBeforeUnloadDialog(
        this, render_frame_host, is_reload,
        base::BindOnce(&CloseDialogCallbackWrapper::Run, wrapper, false));
  }
}

void WebContentsImpl::RunFileChooser(
    RenderFrameHost* render_frame_host,
    scoped_refptr<FileChooserImpl::FileSelectListenerImpl> listener,
    const blink::mojom::FileChooserParams& params) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::RunFileChooser",
                        "render_frame_host",
                        base::trace_event::ToTracedValue(render_frame_host));
  // Any explicit focusing of another window while this WebContents is in
  // fullscreen can be used to confuse the user, so drop fullscreen.
  base::ScopedClosureRunner fullscreen_block = ForSecurityDropFullscreen();
  listener->SetFullscreenBlock(std::move(fullscreen_block));

  if (delegate_)
    delegate_->RunFileChooser(render_frame_host, std::move(listener), params);
  else
    listener->FileSelectionCanceled();
}

WebContents* WebContentsImpl::GetAsWebContents() {
  return this;
}

#if !defined(OS_ANDROID)
double WebContentsImpl::GetPendingPageZoomLevel() {
  NavigationEntry* pending_entry = GetController().GetPendingEntry();
  if (!pending_entry)
    return HostZoomMap::GetZoomLevel(this);

  GURL url = pending_entry->GetURL();
  return HostZoomMap::GetForWebContents(this)->GetZoomLevelForHostAndScheme(
      url.scheme(), net::GetHostOrSpecFromURL(url));
}
#endif  // !defined(OS_ANDROID)

bool WebContentsImpl::HideDownloadUI() const {
  return is_overlay_content_;
}

bool WebContentsImpl::HasPersistentVideo() const {
  return has_persistent_video_;
}

bool WebContentsImpl::IsSpatialNavigationDisabled() const {
  return is_spatial_navigation_disabled_;
}

RenderFrameHostImpl* WebContentsImpl::GetPendingMainFrame() {
  return GetRenderManager()->speculative_frame_host();
}

bool WebContentsImpl::HasActiveEffectivelyFullscreenVideo() const {
  return media_web_contents_observer_->HasActiveEffectivelyFullscreenVideo();
}

bool WebContentsImpl::IsPictureInPictureAllowedForFullscreenVideo() const {
  return media_web_contents_observer_
      ->IsPictureInPictureAllowedForFullscreenVideo();
}

bool WebContentsImpl::IsFocusedElementEditable() {
  RenderFrameHostImpl* frame = GetFocusedFrame();
  return frame && frame->has_focused_editable_element();
}

bool WebContentsImpl::IsShowingContextMenu() {
  return showing_context_menu_;
}

void WebContentsImpl::SetShowingContextMenu(bool showing) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::SetShowingContextMenu",
                        "showing", showing);

  DCHECK_NE(showing_context_menu_, showing);
  showing_context_menu_ = showing;

  if (auto* view = GetRenderWidgetHostView()) {
    // Notify the main frame's RWHV to run the platform-specific code, if any.
    static_cast<RenderWidgetHostViewBase*>(view)->SetShowingContextMenu(
        showing);
  }
}

void WebContentsImpl::ClearFocusedElement() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::ClearFocusedElement");
  if (auto* frame = GetFocusedFrame())
    frame->ClearFocusedElement();
}

bool WebContentsImpl::IsNeverComposited() {
  if (!delegate_)
    return false;
  return delegate_->IsNeverComposited(this);
}

RenderViewHostDelegateView* WebContentsImpl::GetDelegateView() {
  return render_view_host_delegate_view_;
}

blink::mojom::RendererPreferences WebContentsImpl::GetRendererPrefs() const {
  return renderer_preferences_;
}

RenderFrameHostImpl* WebContentsImpl::GetOuterWebContentsFrame() {
  if (GetOuterDelegateFrameTreeNodeId() ==
      FrameTreeNode::kFrameTreeNodeInvalidId) {
    return nullptr;
  }

  FrameTreeNode* outer_node =
      FrameTreeNode::GloballyFindByID(GetOuterDelegateFrameTreeNodeId());
  // The outer node should be in the outer WebContents.
  DCHECK_EQ(outer_node->frame_tree(), GetOuterWebContents()->GetFrameTree());
  return outer_node->parent();
}

WebContentsImpl* WebContentsImpl::GetOuterWebContents() {
  return node_.outer_web_contents();
}

std::vector<WebContents*> WebContentsImpl::GetInnerWebContents() {
  std::vector<WebContents*> all_inner_contents;
  const auto& inner_contents = node_.GetInnerWebContents();
  all_inner_contents.insert(all_inner_contents.end(), inner_contents.begin(),
                            inner_contents.end());
  return all_inner_contents;
}

WebContentsImpl* WebContentsImpl::GetResponsibleWebContents() {
  // Iteratively ask delegates which other contents is responsible until a fixed
  // point is found.
  WebContentsImpl* contents = this;
  while (WebContentsDelegate* delegate = contents->GetDelegate()) {
    auto* responsible_contents = static_cast<WebContentsImpl*>(
        delegate->GetResponsibleWebContents(contents));
    if (responsible_contents == contents)
      break;
    contents = responsible_contents;
  }
  return contents;
}

WebContentsImpl* WebContentsImpl::GetFocusedWebContents() {
  return GetOutermostWebContents()->node_.focused_web_contents();
}

bool WebContentsImpl::ContainsOrIsFocusedWebContents() {
  for (WebContentsImpl* focused_contents = GetFocusedWebContents();
       focused_contents;
       focused_contents = focused_contents->GetOuterWebContents()) {
    if (focused_contents == this)
      return true;
  }

  return false;
}

void WebContentsImpl::RemoveBrowserPluginEmbedder() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::RemoveBrowserPluginEmbedder");
  browser_plugin_embedder_.reset();
}

WebContentsImpl* WebContentsImpl::GetOutermostWebContents() {
  WebContentsImpl* root = this;
  while (root->GetOuterWebContents())
    root = root->GetOuterWebContents();
  return root;
}

void WebContentsImpl::FocusOuterAttachmentFrameChain() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::FocusOuterAttachmentFrameChain");
  WebContentsImpl* outer_contents = GetOuterWebContents();
  if (!outer_contents)
    return;

  FrameTreeNode* outer_node =
      FrameTreeNode::GloballyFindByID(GetOuterDelegateFrameTreeNodeId());
  outer_contents->frame_tree_.SetFocusedFrame(outer_node, nullptr);

  // For a browser initiated focus change, let embedding renderer know of the
  // change. Otherwise, if the currently focused element is just across a
  // process boundary in focus order, it will not be possible to move across
  // that boundary. This is because the target element will already be focused
  // (that renderer was not notified) and drop the event.
  if (GetRenderManager()->GetProxyToOuterDelegate())
    GetRenderManager()->GetProxyToOuterDelegate()->SetFocusedFrame();

  outer_contents->FocusOuterAttachmentFrameChain();
}

void WebContentsImpl::InnerWebContentsCreated(WebContents* inner_web_contents) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::InnerWebContentsCreated");
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->InnerWebContentsCreated(inner_web_contents);
  });
}

void WebContentsImpl::InnerWebContentsAttached(
    WebContents* inner_web_contents) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::InnerWebContentsDetached");
  if (inner_web_contents->IsCurrentlyAudible())
    OnAudioStateChanged();
}

void WebContentsImpl::InnerWebContentsDetached(
    WebContents* inner_web_contents) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::InnerWebContentsCreated");
  if (!is_being_destroyed_)
    OnAudioStateChanged();
}

void WebContentsImpl::RenderViewCreated(RenderViewHost* render_view_host) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::RenderViewCreated",
                        "render_view_host", render_view_host);
  if (delegate_)
    view_->SetOverscrollControllerEnabled(CanOverscrollContent());

  NotificationService::current()->Notify(
      NOTIFICATION_WEB_CONTENTS_RENDER_VIEW_HOST_CREATED,
      Source<WebContents>(this),
      Details<RenderViewHost>(render_view_host));

  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->RenderViewCreated(render_view_host);
  });
  if (delegate_)
    RenderFrameDevToolsAgentHost::WebContentsCreated(this);
}

void WebContentsImpl::RenderViewReady(RenderViewHost* rvh) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::RenderViewReady",
                        "render_view_host", rvh);
  if (rvh != GetRenderViewHost()) {
    // Don't notify the world, since this came from a renderer in the
    // background.
    return;
  }

  RenderWidgetHostViewBase* rwhv =
      static_cast<RenderWidgetHostViewBase*>(GetRenderWidgetHostView());
  if (rwhv)
    rwhv->SetMainFrameAXTreeID(GetMainFrame()->GetAXTreeID());

  notify_disconnection_ = true;

  bool was_crashed = IsCrashed();
  SetIsCrashed(base::TERMINATION_STATUS_STILL_RUNNING, 0);

  // Restore the focus to the tab (otherwise the focus will be on the top
  // window).
  if (was_crashed && !FocusLocationBarByDefault() &&
      (!delegate_ || delegate_->ShouldFocusPageAfterCrash())) {
    view_->Focus();
  }

  observers_.ForEachObserver(
      [&](WebContentsObserver* observer) { observer->RenderViewReady(); });
  view_->RenderViewReady();
}

void WebContentsImpl::RenderViewTerminated(RenderViewHost* rvh,
                                           base::TerminationStatus status,
                                           int error_code) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::RenderViewTerminated",
                        "render_view_host", rvh, "status",
                        static_cast<int>(status));
  if (rvh != GetRenderViewHost()) {
    // The pending page's RenderViewHost is gone.
    return;
  }
  // Ensure fullscreen mode is exited in the |delegate_| since a crashed
  // renderer may not have made a clean exit.
  if (IsFullscreen())
    ExitFullscreenMode(false);

  // Ensure any video in Picture-in-Picture is exited in the |delegate_| since
  // a crashed renderer may not have made a clean exit.
  if (HasPictureInPictureVideo())
    ExitPictureInPicture();

  // Cancel any visible dialogs so they are not left dangling over the sad tab.
  CancelActiveAndPendingDialogs();

  audio_stream_monitor_.RenderProcessGone(rvh->GetProcess()->GetID());

  // Reset the loading progress. TODO(avi): What does it mean to have a
  // "renderer crash" when there is more than one renderer process serving a
  // webpage? Once this function is called at a more granular frame level, we
  // probably will need to more granularly reset the state here.
  ResetLoadProgressState();
  NotifyDisconnected();
  SetIsCrashed(status, error_code);

  TRACE_EVENT0("content",
               "Dispatching WebContentsObserver::RenderViewTerminated");
  // Some observers might destroy WebContents in RenderViewTerminated.
  base::WeakPtr<WebContentsImpl> weak_ptr = weak_factory_.GetWeakPtr();
  auto crashed_status = GetCrashedStatus();
  for (auto& observer : observers_.observer_list()) {
    observer.RenderProcessGone(crashed_status);
    if (!weak_ptr)
      return;
  }

  // |this| might have been deleted. Do not add code here.
}

void WebContentsImpl::RenderViewDeleted(RenderViewHost* rvh) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::RenderViewDeleted",
                        "render_view_host", rvh);
  observers_.ForEachObserver(
      [&](WebContentsObserver* observer) { observer->RenderViewDeleted(rvh); });
}

void WebContentsImpl::ClearTargetURL() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::ClearTargetURL");
  frame_that_set_last_target_url_ = nullptr;
  if (delegate_)
    delegate_->UpdateTargetURL(this, GURL());
}

void WebContentsImpl::Close(RenderViewHost* rvh) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::Close", "render_view_host",
                        rvh);
#if defined(OS_MAC)
  // The UI may be in an event-tracking loop, such as between the
  // mouse-down and mouse-up in text selection or a button click.
  // Defer the close until after tracking is complete, so that we
  // don't free objects out from under the UI.
  // TODO(shess): This could get more fine-grained.  For instance,
  // closing a tab in another window while selecting text in the
  // current window's Omnibox should be just fine.
  if (view_->CloseTabAfterEventTrackingIfNeeded())
    return;
#endif

  // Ignore this if it comes from a RenderViewHost that we aren't showing.
  if (delegate_ && rvh == GetRenderViewHost())
    delegate_->CloseContents(this);
}

void WebContentsImpl::RequestSetBounds(const gfx::Rect& new_bounds) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::RequestSetBounds");
  if (!delegate_)
    return;

  // Members of |new_bounds| may be 0 to indicate uninitialized values for newly
  // opened windows, even if the |GetContainerBounds()| inner rect is correct.
  // TODO(crbug.com/897300): Plumb values as specified; fallback on outer rect.
  auto bounds = new_bounds;
  if (bounds.IsEmpty())
    bounds.set_size(GetContainerBounds().size());

  // Only requests from the main frame, not subframes, should reach this code.
  int64_t display_id = AdjustRequestedWindowBounds(&bounds, GetMainFrame());

  // Drop fullscreen when placing a WebContents to prohibit deceptive behavior.
  // Only drop fullscreen on the specific destination display, which is known.
  // This supports sites using cross-screen window placement capabilities to
  // retain fullscreen and place a window on another screen.
  ForSecurityDropFullscreen(display_id).RunAndReset();

  delegate_->SetContentsBounds(this, bounds);
}

void WebContentsImpl::DidStartLoading(FrameTreeNode* frame_tree_node,
                                      bool to_different_document) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::DidStartLoading",
                        "frame_tree_node", frame_tree_node);
  LoadingStateChanged(frame_tree_node->IsMainFrame() && to_different_document,
                      nullptr);

  // Reset the focus state from DidStartNavigation to false if a new load starts
  // afterward, in case loading logic triggers a FocusLocationBarByDefault call.
  should_focus_location_bar_by_default_ = false;

  // Notify accessibility that the user is navigating away from the
  // current document.
  // TODO(domfarolino, dmazzoni): Do this using WebContentsObserver. See
  // https://crbug.com/981271.
  BrowserAccessibilityManager* manager =
      frame_tree_node->current_frame_host()->browser_accessibility_manager();
  if (manager)
    manager->UserIsNavigatingAway();
}

void WebContentsImpl::DidStopLoading() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::DidStopLoading");
  std::unique_ptr<LoadNotificationDetails> details;

  // Use the last committed entry rather than the active one, in case a
  // pending entry has been created.
  NavigationEntry* entry = controller_.GetLastCommittedEntry();

  // An entry may not exist for a stop when loading an initial blank page or
  // if an iframe injected by script into a blank page finishes loading.
  if (entry) {
    details = std::make_unique<LoadNotificationDetails>(
        entry->GetVirtualURL(), &controller_,
        controller_.GetCurrentEntryIndex());
  }

  LoadingStateChanged(true, details.get());
}

void WebContentsImpl::DidChangeLoadProgress() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::DidChangeLoadProgress");
  if (is_being_destroyed_)
    return;
  double load_progress = frame_tree_.load_progress();

  // The delegate is notified immediately for the first and last updates. Also,
  // since the message loop may be pretty busy when a page is loaded, it might
  // not execute a posted task in a timely manner so the progress report is sent
  // immediately if enough time has passed.
  base::TimeDelta min_delay =
      base::TimeDelta::FromMilliseconds(kMinimumDelayBetweenLoadingUpdatesMS);
  bool delay_elapsed = loading_last_progress_update_.is_null() ||
      base::TimeTicks::Now() - loading_last_progress_update_ > min_delay;

  if (load_progress == 0.0 || load_progress == 1.0 || delay_elapsed) {
    // If there is a pending task to send progress, it is now obsolete.
    loading_weak_factory_.InvalidateWeakPtrs();

    // Notify the load progress change.
    SendChangeLoadProgress();

    // Clean-up the states if needed.
    if (load_progress == 1.0)
      ResetLoadProgressState();
    return;
  }

  if (loading_weak_factory_.HasWeakPtrs())
    return;

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&WebContentsImpl::SendChangeLoadProgress,
                     loading_weak_factory_.GetWeakPtr()),
      min_delay);
}

std::vector<std::unique_ptr<NavigationThrottle>>
WebContentsImpl::CreateThrottlesForNavigation(
    NavigationHandle* navigation_handle) {
  OPTIONAL_TRACE_EVENT1("content",
                        "WebContentsImpl::CreateThrottlesForNavigation",
                        "navigation", navigation_handle);
  auto throttles = GetContentClient()->browser()->CreateThrottlesForNavigation(
      navigation_handle);

  // This is not a normal place to be adding a throttle. However, in the case
  // javascript dialogs, related logic is present in the web_contents/ layer,
  // and the purpose of the throttle is to ensure that navigation commits are
  // deferred for the entire WebContents. Most throttles are either added by
  // the embederrer outside of content/, or are per-frame and added by
  // NavigationThrottleRunner.
  std::unique_ptr<content::NavigationThrottle> dialog_throttle =
      JavaScriptDialogNavigationThrottle::MaybeCreateThrottleFor(
          navigation_handle);
  if (dialog_throttle)
    throttles.push_back(std::move(dialog_throttle));

  return throttles;
}

std::unique_ptr<NavigationUIData> WebContentsImpl::GetNavigationUIData(
    NavigationHandle* navigation_handle) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::GetNavigationUIData");
  return GetContentClient()->browser()->GetNavigationUIData(navigation_handle);
}

void WebContentsImpl::RegisterExistingOriginToPreventOptInIsolation(
    const url::Origin& origin,
    NavigationRequest* navigation_request_to_exclude) {
  OPTIONAL_TRACE_EVENT2(
      "content",
      "WebContentsImpl::RegisterExistingOriginToPreventOptInIsolation",
      "origin", base::trace_event::ValueToString(origin),
      "navigation_request_to_exclude", navigation_request_to_exclude);
  // Note: This function can be made static if we ever need call it without
  // a WebContentsImpl instance, in which case we can use a wrapper to
  // implement the override from NavigatorDelegate.
  for (WebContentsImpl* web_contents : GetAllWebContents()) {
    web_contents->controller_.RegisterExistingOriginToPreventOptInIsolation(
        origin);
    // Walk the frame tree to pick up any frames without FrameNavigationEntries.
    // * Some frames won't have FrameNavigationEntries (Issues 524208, 608402).
    // * Some pending navigations won't have NavigationEntries.
    web_contents->GetFrameTree()->RegisterExistingOriginToPreventOptInIsolation(
        origin, navigation_request_to_exclude);
  }
}

void WebContentsImpl::DidCancelLoading() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::DidCancelLoading");
  controller_.DiscardNonCommittedEntries();

  // Update the URL display.
  NotifyNavigationStateChanged(INVALIDATE_TYPE_URL);
}

void WebContentsImpl::DidAccessInitialDocument() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::DidAccessInitialDocument");
  has_accessed_initial_document_ = true;

  // We may have left a failed browser-initiated navigation in the address bar
  // to let the user edit it and try again.  Clear it now that content might
  // show up underneath it.
  if (!IsLoading() && controller_.GetPendingEntry())
    controller_.DiscardPendingEntry(false);

  // Update the URL display.
  NotifyNavigationStateChanged(INVALIDATE_TYPE_URL);
}

void WebContentsImpl::DidChangeName(RenderFrameHost* render_frame_host,
                                    const std::string& name) {
  OPTIONAL_TRACE_EVENT2(
      "content", "WebContentsImpl::DidChangeName", "render_frame_host",
      base::trace_event::ToTracedValue(render_frame_host), "name", name);
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->FrameNameChanged(render_frame_host, name);
  });
}

void WebContentsImpl::DidReceiveFirstUserActivation(
    RenderFrameHost* render_frame_host) {
  OPTIONAL_TRACE_EVENT1(
      "content", "WebContentsImpl::DidReceiveFirstUserActivation",
      "render_frame_host", base::trace_event::ToTracedValue(render_frame_host));
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->FrameReceivedFirstUserActivation(render_frame_host);
  });
}

void WebContentsImpl::DidChangeDisplayState(RenderFrameHost* render_frame_host,
                                            bool is_display_none) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::DidChangeDisplayState",
                        "render_frame_host",
                        base::trace_event::ToTracedValue(render_frame_host),
                        "is_display_none", is_display_none);
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->FrameDisplayStateChanged(render_frame_host, is_display_none);
  });
}

void WebContentsImpl::FrameSizeChanged(RenderFrameHost* render_frame_host,
                                       const gfx::Size& frame_size) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::FrameSizeChanged",
                        "render_frame_host",
                        base::trace_event::ToTracedValue(render_frame_host));
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->FrameSizeChanged(render_frame_host, frame_size);
  });
}

void WebContentsImpl::DocumentOnLoadCompleted(
    RenderFrameHost* render_frame_host) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::DocumentOnLoadCompleted",
                        "render_frame_host",
                        base::trace_event::ToTracedValue(render_frame_host));
  ShowInsecureLocalhostWarningIfNeeded();

  GetRenderViewHost()->DocumentOnLoadCompletedInMainFrame();

  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->DocumentOnLoadCompletedInMainFrame();
  });

  // TODO(avi): Remove. http://crbug.com/170921
  NotificationService::current()->Notify(
      NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
      Source<WebContents>(this),
      NotificationService::NoDetails());
}

void WebContentsImpl::UpdateStateForFrame(RenderFrameHost* render_frame_host,
                                          const PageState& page_state) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::UpdateStateForFrame",
                        "render_frame_host",
                        base::trace_event::ToTracedValue(render_frame_host));
  // The state update affects the last NavigationEntry associated with the given
  // |render_frame_host|. This may not be the last committed NavigationEntry (as
  // in the case of an UpdateState from a frame being swapped out). We track
  // which entry this is in the RenderFrameHost's nav_entry_id.
  RenderFrameHostImpl* rfhi =
      static_cast<RenderFrameHostImpl*>(render_frame_host);
  NavigationEntryImpl* entry =
      controller_.GetEntryWithUniqueID(rfhi->nav_entry_id());
  if (!entry)
    return;

  FrameNavigationEntry* frame_entry =
      entry->GetFrameEntry(rfhi->frame_tree_node());
  if (!frame_entry)
    return;

  // The SiteInstance might not match if we do a cross-process navigation with
  // replacement (e.g., auto-subframe), in which case the swap out of the old
  // RenderFrameHost runs in the background after the old FrameNavigationEntry
  // has already been replaced and destroyed.
  if (frame_entry->site_instance() != rfhi->GetSiteInstance())
    return;

  if (page_state == frame_entry->page_state())
    return;  // Nothing to update.

  DCHECK(page_state.IsValid()) << "Shouldn't set an empty PageState.";

  // The document_sequence_number and item_sequence_number recorded in the
  // FrameNavigationEntry should not differ from the one coming with the update,
  // since it must come from the same document. Do not update it if a difference
  // is detected, as this indicates that |frame_entry| is not the correct one.
  ExplodedPageState exploded_state;
  if (!DecodePageState(page_state.ToEncodedData(), &exploded_state))
    return;

  if (exploded_state.top.document_sequence_number !=
          frame_entry->document_sequence_number() ||
      exploded_state.top.item_sequence_number !=
          frame_entry->item_sequence_number()) {
    return;
  }

  frame_entry->SetPageState(page_state);
  controller_.NotifyEntryChanged(entry);
}

void WebContentsImpl::UpdateTitle(RenderFrameHost* render_frame_host,
                                  const base::string16& title,
                                  base::i18n::TextDirection title_direction) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::UpdateTitle",
                        "render_frame_host",
                        base::trace_event::ToTracedValue(render_frame_host),
                        "title", base::trace_event::ValueToString(title));
  // Try to find the navigation entry, which might not be the current one.
  // For example, it might be from a recently swapped out RFH.
  NavigationEntryImpl* entry = controller_.GetEntryWithUniqueID(
      static_cast<RenderFrameHostImpl*>(render_frame_host)->nav_entry_id());

  // We can handle title updates when we don't have an entry in
  // UpdateTitleForEntry, but only if the update is from the current RVH.
  // TODO(avi): Change to make decisions based on the RenderFrameHost.
  if (!entry && render_frame_host != GetMainFrame())
    return;

  // TODO(evan): make use of title_direction.
  // http://code.google.com/p/chromium/issues/detail?id=27094
  UpdateTitleForEntry(entry, title);
}

void WebContentsImpl::UpdateTargetURL(RenderFrameHost* render_frame_host,
                                      const GURL& url) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::UpdateTargetURL",
                        "render_frame_host",
                        base::trace_event::ToTracedValue(render_frame_host),
                        "url", base::trace_event::ValueToString(url));
  if (fullscreen_widget_routing_id_ != MSG_ROUTING_NONE) {
    // If we're in flash fullscreen (i.e. Pepper plugin fullscreen) only update
    // the url if it's from the fullscreen renderer.
    RenderWidgetHostView* fs = GetFullscreenRenderWidgetHostView();
    RenderViewHost* render_view_host = render_frame_host->GetRenderViewHost();
    if (fs && fs->GetRenderWidgetHost() != render_view_host->GetWidget())
      return;
  }

  // In case of racey updates from multiple RenderViewHosts, the last URL should
  // be shown - see also some discussion in https://crbug.com/807776.
  if (!url.is_valid() && render_frame_host != frame_that_set_last_target_url_)
    return;
  frame_that_set_last_target_url_ =
      url.is_valid() ? render_frame_host : nullptr;

  if (delegate_)
    delegate_->UpdateTargetURL(this, url);
}

bool WebContentsImpl::ShouldRouteMessageEvent(
    RenderFrameHost* target_rfh,
    SiteInstance* source_site_instance) const {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::ShouldRouteMessageEvent",
                        "render_frame_host",
                        base::trace_event::ToTracedValue(target_rfh),
                        "source_site_instance", source_site_instance);
  // Allow the message if this WebContents is dedicated to a browser plugin
  // guest.
  // Note: This check means that an embedder could theoretically receive a
  // postMessage from anyone (not just its own guests). However, this is
  // probably not a risk for apps since other pages won't have references
  // to App windows.
  return GetBrowserPluginGuest() || GetBrowserPluginEmbedder();
}

void WebContentsImpl::EnsureOpenerProxiesExist(RenderFrameHost* source_rfh) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::EnsureOpenerProxiesExist",
                        "render_frame_host",
                        base::trace_event::ToTracedValue(source_rfh));
  WebContentsImpl* source_web_contents = static_cast<WebContentsImpl*>(
      WebContents::FromRenderFrameHost(source_rfh));

  if (source_web_contents) {
    // If this message is going to outer WebContents from inner WebContents,
    // then we should not create a RenderView. AttachToOuterWebContentsFrame()
    // already created a RenderFrameProxyHost for that purpose.
    if (GetBrowserPluginEmbedder() &&
        source_web_contents->browser_plugin_guest_) {
      return;
    }

    if (this != source_web_contents && GetBrowserPluginGuest()) {
      // We create a RenderFrameProxyHost for the embedder in the guest's render
      // process but we intentionally do not expose the embedder's opener chain
      // to it.
      source_web_contents->GetRenderManager()->CreateRenderFrameProxy(
          GetSiteInstance());
    } else {
      RenderFrameHostImpl* source_rfhi =
          static_cast<RenderFrameHostImpl*>(source_rfh);
      source_rfhi->frame_tree_node()->render_manager()->CreateOpenerProxies(
          GetSiteInstance(), nullptr);
    }
  }
}

void WebContentsImpl::SetAsFocusedWebContentsIfNecessary() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::SetAsFocusedWebContentsIfNecessary");
  DCHECK(!portal());
  // Only change focus if we are not currently focused.
  WebContentsImpl* old_contents = GetFocusedWebContents();
  if (old_contents == this)
    return;

  GetOutermostWebContents()->node_.SetFocusedWebContents(this);

  // Send a page level blur to the old contents so that it displays inactive UI
  // and focus this contents to activate it.
  if (old_contents)
    old_contents->GetMainFrame()->GetRenderWidgetHost()->SetPageFocus(false);

  // Make sure the outer web contents knows our frame is focused. Otherwise, the
  // outer renderer could have the element before or after the frame element
  // focused which would return early without actually advancing focus.
  FocusOuterAttachmentFrameChain();

  GetMainFrame()->GetRenderWidgetHost()->SetPageFocus(true);
}

void WebContentsImpl::SetFocusedFrame(FrameTreeNode* node,
                                      SiteInstance* source) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::SetFocusedFrame",
                        "frame_tree_node", node, "source_site_instance",
                        source);
  DCHECK(!portal());
  frame_tree_.SetFocusedFrame(node, source);

  if (auto* inner_contents = node_.GetInnerWebContentsInFrame(node)) {
    // |this| is an outer WebContents and |node| represents an inner
    // WebContents. Transfer the focus to the inner contents if |this| is
    // focused.
    if (GetFocusedWebContents() == this)
      inner_contents->SetAsFocusedWebContentsIfNecessary();
  } else if (node_.OuterContentsFrameTreeNode() &&
             node_.OuterContentsFrameTreeNode()
                     ->current_frame_host()
                     ->GetSiteInstance() == source) {
    // |this| is an inner WebContents, |node| is its main FrameTreeNode and
    // the outer WebContents FrameTreeNode is at |source|'s SiteInstance.
    // Transfer the focus to the inner WebContents if the outer WebContents is
    // focused. This branch is used when an inner WebContents is focused through
    // its RenderFrameProxyHost (via FrameFocused mojo call, used to
    // implement the window.focus() API).
    if (GetFocusedWebContents() == GetOuterWebContents())
      SetAsFocusedWebContentsIfNecessary();
  } else if (!GetOuterWebContents()) {
    // This is an outermost WebContents.
    SetAsFocusedWebContentsIfNecessary();
  }
}

void WebContentsImpl::DidCallFocus() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::DidCallFocus");
  // Any explicit focusing of another window while this WebContents is in
  // fullscreen can be used to confuse the user, so drop fullscreen.
  base::ScopedClosureRunner fullscreen_block = ForSecurityDropFullscreen();
  // The other contents is independent of this contents, so release the
  // fullscreen block.
  fullscreen_block.RunAndReset();
}

RenderFrameHost* WebContentsImpl::GetFocusedFrameIncludingInnerWebContents() {
  OPTIONAL_TRACE_EVENT0(
      "content", "WebContentsImpl::GetFocusedFrameIncludingInnerWebContents");
  WebContentsImpl* contents = this;
  FrameTreeNode* focused_node = contents->frame_tree_.GetFocusedFrame();

  // If there is no focused frame in the outer WebContents, we need to return
  // null.
  if (!focused_node)
    return nullptr;

  // If the focused frame is embedding an inner WebContents, we must descend
  // into that contents. If the current WebContents does not have a focused
  // frame, return the main frame of this contents instead of the focused empty
  // frame embedding this contents.
  while (true) {
    contents = contents->node_.GetInnerWebContentsInFrame(focused_node);
    if (!contents)
      return focused_node->current_frame_host();

    focused_node = contents->frame_tree_.GetFocusedFrame();
    if (!focused_node)
      return contents->GetMainFrame();
  }
}

void WebContentsImpl::OnAdvanceFocus(RenderFrameHostImpl* source_rfh) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::OnAdvanceFocus",
                        "render_frame_host",
                        base::trace_event::ToTracedValue(source_rfh));
  // When a RenderFrame needs to advance focus to a RenderFrameProxy (by hitting
  // TAB), the RenderFrameProxy sends an IPC to RenderFrameProxyHost. When this
  // RenderFrameProxyHost represents an inner WebContents, the outer WebContents
  // needs to focus the inner WebContents.
  if (GetOuterWebContents() &&
      GetOuterWebContents() == source_rfh->delegate()->GetAsWebContents() &&
      GetFocusedWebContents() == GetOuterWebContents()) {
    SetAsFocusedWebContentsIfNecessary();
  }
}

void WebContentsImpl::OnFocusedElementChangedInFrame(
    RenderFrameHostImpl* frame,
    const gfx::Rect& bounds_in_root_view,
    blink::mojom::FocusType focus_type) {
  OPTIONAL_TRACE_EVENT1(
      "content", "WebContentsImpl::OnFocusedElementChangedInFrame",
      "render_frame_host", base::trace_event::ToTracedValue(frame));
  RenderWidgetHostViewBase* root_view =
      static_cast<RenderWidgetHostViewBase*>(GetRenderWidgetHostView());
  if (!root_view || !frame->GetView())
    return;
  // Convert to screen coordinates from window coordinates by adding the
  // window's origin.
  gfx::Point origin = bounds_in_root_view.origin();
  origin += root_view->GetViewBounds().OffsetFromOrigin();
  gfx::Rect bounds_in_screen(origin, bounds_in_root_view.size());

  root_view->FocusedNodeChanged(frame->has_focused_editable_element(),
                                bounds_in_screen);

  FocusedNodeDetails details = {frame->has_focused_editable_element(),
                                bounds_in_screen, focus_type};

  // TODO(ekaramad): We should replace this with an observer notification
  // (https://crbug.com/675975).
  NotificationService::current()->Notify(
      NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
      Source<RenderViewHost>(GetRenderViewHost()),
      Details<FocusedNodeDetails>(&details));

  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->OnFocusChangedInPage(&details);
  });
}

bool WebContentsImpl::DidAddMessageToConsole(
    blink::mojom::ConsoleMessageLevel log_level,
    const base::string16& message,
    int32_t line_no,
    const base::string16& source_id) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::DidAddMessageToConsole",
                        "message", base::trace_event::ValueToString(message));

  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->OnDidAddMessageToConsole(log_level, message, line_no, source_id);
  });

  if (!delegate_)
    return false;
  return delegate_->DidAddMessageToConsole(this, log_level, message, line_no,
                                           source_id);
}

void WebContentsImpl::DidReceiveInputEvent(
    RenderWidgetHostImpl* render_widget_host,
    const blink::WebInputEvent& event) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::DidReceiveInputEvent",
                        "render_widget_host", render_widget_host);

  if (!IsUserInteractionInputType(event.GetType()))
    return;

  // Ignore unless the widget is currently in the frame tree.
  if (!HasMatchingWidgetHost(&frame_tree_, render_widget_host))
    return;

  if (event.GetType() != blink::WebInputEvent::Type::kGestureScrollBegin)
    last_interactive_input_event_time_ = ui::EventTimeForNow();

  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->DidGetUserInteraction(event);
  });
}

bool WebContentsImpl::ShouldIgnoreInputEvents() {
  WebContentsImpl* web_contents = this;
  while (web_contents) {
    if (web_contents->ignore_input_events_)
      return true;
    web_contents = web_contents->GetOuterWebContents();
  }

  return false;
}

void WebContentsImpl::FocusOwningWebContents(
    RenderWidgetHostImpl* render_widget_host) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::FocusOwningWebContents",
                        "render_widget_host", render_widget_host);
  RenderWidgetHostImpl* main_frame_widget_host =
      GetMainFrame()->GetRenderWidgetHost();
  RenderWidgetHostImpl* focused_widget =
      GetFocusedRenderWidgetHost(main_frame_widget_host);

  if (focused_widget != render_widget_host &&
      (!focused_widget ||
       focused_widget->delegate() != render_widget_host->delegate())) {
    SetAsFocusedWebContentsIfNecessary();
  }
}

void WebContentsImpl::OnIgnoredUIEvent() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::OnIgnoredUIEvent");
  // Notify observers.
  observers_.ForEachObserver(
      [&](WebContentsObserver* observer) { observer->DidGetIgnoredUIEvent(); });
}

void WebContentsImpl::RendererUnresponsive(
    RenderWidgetHostImpl* render_widget_host,
    base::RepeatingClosure hang_monitor_restarter) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::RendererUnresponsive",
                        "render_widget_host", render_widget_host);
  if (ShouldIgnoreUnresponsiveRenderer())
    return;

  // Do not report hangs (to task manager, to hang renderer dialog, etc.) for
  // invisible tabs (like extension background page, background tabs).  See
  // https://crbug.com/881812 for rationale and for choosing the visibility
  // (rather than process priority) as the signal here.
  if (GetVisibility() != Visibility::VISIBLE)
    return;

  if (!render_widget_host->renderer_initialized())
    return;

  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->OnRendererUnresponsive(render_widget_host->GetProcess());
  });
  if (delegate_)
    delegate_->RendererUnresponsive(this, render_widget_host,
                                    std::move(hang_monitor_restarter));
}

void WebContentsImpl::RendererResponsive(
    RenderWidgetHostImpl* render_widget_host) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::RenderResponsive",
                        "render_widget_host", render_widget_host);
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->OnRendererResponsive(render_widget_host->GetProcess());
  });

  if (delegate_)
    delegate_->RendererResponsive(this, render_widget_host);
}

void WebContentsImpl::SubframeCrashed(
    blink::mojom::FrameVisibility visibility) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::SubframeCrashed",
                        "visibility",
                        base::trace_event::ValueToString(visibility));
  // If a subframe crashed on a hidden tab, mark the tab for reload to avoid
  // showing a sad frame to the user if they ever switch back to that tab. Do
  // this for subframes that are either visible in viewport or visible but
  // scrolled out of view, but skip subframes that are not rendered (e.g., via
  // "display:none"), since in that case the user wouldn't see a sad frame
  // anyway.
  bool did_mark_for_reload = false;
  if (IsHidden() && visibility != blink::mojom::FrameVisibility::kNotRendered &&
      base::FeatureList::IsEnabled(
          features::kReloadHiddenTabsWithCrashedSubframes)) {
    controller_.SetNeedsReload(
        NavigationControllerImpl::NeedsReloadType::kCrashedSubframe);
    did_mark_for_reload = true;
    UMA_HISTOGRAM_ENUMERATION(
        "Stability.ChildFrameCrash.TabMarkedForReload.Visibility", visibility);
  }

  UMA_HISTOGRAM_BOOLEAN("Stability.ChildFrameCrash.TabMarkedForReload",
                        did_mark_for_reload);
}

void WebContentsImpl::BeforeUnloadFiredFromRenderManager(
    bool proceed, const base::TimeTicks& proceed_time,
    bool* proceed_to_fire_unload) {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::BeforeUnloadFiredFromRenderManager");
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->BeforeUnloadFired(proceed, proceed_time);
  });
  if (delegate_)
    delegate_->BeforeUnloadFired(this, proceed, proceed_to_fire_unload);
  // Note: |this| might be deleted at this point.
}

void WebContentsImpl::RenderProcessGoneFromRenderManager(
    RenderViewHost* render_view_host) {
  DCHECK(crashed_status_ != base::TERMINATION_STATUS_STILL_RUNNING);
  RenderViewTerminated(render_view_host, crashed_status_, crashed_error_code_);
}

void WebContentsImpl::CancelModalDialogsForRenderManager() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::CancelModalDialogsForRenderManager");
  // We need to cancel modal dialogs when doing a process swap, since the load
  // deferrer would prevent us from swapping out. We also clear the state
  // because this is a cross-process navigation, which means that it's a new
  // site that should not have to pay for the sins of its predecessor.
  //
  // Note that we don't bother telling |browser_plugin_embedder_| because the
  // cross-process navigation will either destroy the browser plugins or not
  // require their dialogs to close.
  if (dialog_manager_) {
    dialog_manager_->CancelDialogs(this, /*reset_state=*/true);
  }
}

void WebContentsImpl::NotifySwappedFromRenderManager(RenderFrameHost* old_frame,
                                                     RenderFrameHost* new_frame,
                                                     bool is_main_frame) {
  TRACE_EVENT2(
      "content", "WebContentsImpl::NotifySwappedFromRenderManager",
      "old_render_frame_host", base::trace_event::ToTracedValue(old_frame),
      "new_render_frame_host", base::trace_event::ToTracedValue(new_frame));
  if (is_main_frame) {
    // The |new_frame| and its various compadres are already swapped into place
    // for the WebContentsImpl when this method is called.
    DCHECK(GetMainFrame() == new_frame);
    DCHECK(GetRenderViewHost() == new_frame->GetRenderViewHost());
    DCHECK(GetRenderWidgetHostView() == new_frame->GetView());

    RenderViewHost* old_rvh =
        old_frame ? old_frame->GetRenderViewHost() : nullptr;
    RenderViewHost* new_rvh = new_frame->GetRenderViewHost();
    // |old_rvh| and |new_rvh| might be equal when navigating from a crashed
    // RenderFrameHost to a new same-site one. With RenderDocument, this will
    // happen for every same-site navigation.
    if (old_rvh != new_rvh)
      NotifyViewSwapped(old_rvh, new_rvh);

    // Make sure the visible RVH reflects the new delegate's preferences.
    if (delegate_)
      view_->SetOverscrollControllerEnabled(CanOverscrollContent());

    auto* rwhv = static_cast<RenderWidgetHostViewBase*>(new_frame->GetView());
    if (rwhv) {
      rwhv->SetMainFrameAXTreeID(new_frame->GetAXTreeID());

      // The RenderWidgetHostView for the speculative RenderFrameHost is not
      // resized with the current RenderFrameHost while a navigation is
      // pending. So when we swap in the main frame, we need to update the
      // RenderWidgetHostView's size.
      //
      // Historically, this was done to fix b/1079768 for interstitials.
      rwhv->SetSize(GetSizeForMainFrame());
    }
  }

  NotifyFrameSwapped(old_frame, new_frame, is_main_frame);
}

void WebContentsImpl::NotifyMainFrameSwappedFromRenderManager(
    RenderFrameHost* old_frame,
    RenderFrameHost* new_frame) {
  NotifyViewSwapped(old_frame ? old_frame->GetRenderViewHost() : nullptr,
                    new_frame->GetRenderViewHost());
}

NavigationControllerImpl& WebContentsImpl::GetControllerForRenderManager() {
  return GetController();
}

std::unique_ptr<WebUIImpl> WebContentsImpl::CreateWebUIForRenderFrameHost(
    RenderFrameHost* frame_host,
    const GURL& url) {
  return CreateWebUI(frame_host, url);
}

void WebContentsImpl::CreateRenderWidgetHostViewForRenderManager(
    RenderViewHost* render_view_host) {
  OPTIONAL_TRACE_EVENT1(
      "content", "WebContentsImpl::CreateRenderWidgetHostViewForRenderManager",
      "render_view_host", render_view_host);
  RenderWidgetHostViewBase* rwh_view =
      view_->CreateViewForWidget(render_view_host->GetWidget());
  rwh_view->SetSize(GetSizeForMainFrame());
}

bool WebContentsImpl::CreateRenderViewForRenderManager(
    RenderViewHost* render_view_host,
    const base::Optional<base::UnguessableToken>& opener_frame_token,
    int proxy_routing_id) {
  TRACE_EVENT1("browser,navigation",
               "WebContentsImpl::CreateRenderViewForRenderManager",
               "render_view_host", render_view_host);

  if (proxy_routing_id == MSG_ROUTING_NONE)
    CreateRenderWidgetHostViewForRenderManager(render_view_host);

  if (!static_cast<RenderViewHostImpl*>(render_view_host)
           ->CreateRenderView(opener_frame_token, proxy_routing_id,
                              created_with_opener_)) {
    return false;
  }
  // Set the TextAutosizer state from the main frame's renderer on the new view,
  // but only if it's not for the main frame. Main frame renderers should create
  // this state themselves from up-to-date values, so we shouldn't override it
  // with the cached values.
  if (!render_view_host->GetMainFrame()) {
    auto* proxy_host = GetRenderManager()->GetRenderFrameProxyHost(
        render_view_host->GetSiteInstance());
    proxy_host->GetAssociatedRemoteMainFrame()->UpdateTextAutosizerPageInfo(
        text_autosizer_page_info_.Clone());
  }

  if (proxy_routing_id == MSG_ROUTING_NONE && node_.outer_web_contents())
    ReattachToOuterWebContentsFrame();

  SetHistoryOffsetAndLengthForView(render_view_host,
                                   controller_.GetLastCommittedEntryIndex(),
                                   controller_.GetEntryCount());

#if defined(OS_POSIX) && !defined(OS_MAC) && !defined(OS_ANDROID)
  // Force a ViewMsg_Resize to be sent, needed to make plugins show up on
  // linux. See crbug.com/83941.
  RenderWidgetHostView* rwh_view = render_view_host->GetWidget()->GetView();
  if (rwh_view) {
    if (RenderWidgetHost* render_widget_host = rwh_view->GetRenderWidgetHost())
      render_widget_host->SynchronizeVisualProperties();
  }
#endif

  return true;
}

#if defined(OS_ANDROID)

base::android::ScopedJavaLocalRef<jobject>
WebContentsImpl::GetJavaWebContents() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return GetWebContentsAndroid()->GetJavaObject();
}

WebContentsAndroid* WebContentsImpl::GetWebContentsAndroid() {
  if (!web_contents_android_) {
    web_contents_android_ = std::make_unique<WebContentsAndroid>(this);
  }
  return web_contents_android_.get();
}

void WebContentsImpl::ClearWebContentsAndroid() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  web_contents_android_.reset();
}

void WebContentsImpl::ActivateNearestFindResult(float x,
                                                float y) {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::ActivateNearestFindResult");
  GetOrCreateFindRequestManager()->ActivateNearestFindResult(x, y);
}

void WebContentsImpl::RequestFindMatchRects(int current_version) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::RequestFindMatchRects");
  GetOrCreateFindRequestManager()->RequestFindMatchRects(current_version);
}

service_manager::InterfaceProvider* WebContentsImpl::GetJavaInterfaces() {
  if (!java_interfaces_) {
    mojo::PendingRemote<service_manager::mojom::InterfaceProvider> provider;
    BindInterfaceRegistryForWebContents(
        provider.InitWithNewPipeAndPassReceiver(), this);
    java_interfaces_.reset(new service_manager::InterfaceProvider);
    java_interfaces_->Bind(std::move(provider));
  }
  return java_interfaces_.get();
}

#endif

bool WebContentsImpl::CompletedFirstVisuallyNonEmptyPaint() {
  return GetRenderViewHost()->did_first_visually_non_empty_paint();
}

void WebContentsImpl::OnDidDownloadImage(
    ImageDownloadCallback callback,
    int id,
    const GURL& image_url,
    int32_t http_status_code,
    const std::vector<SkBitmap>& images,
    const std::vector<gfx::Size>& original_image_sizes) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::OnDidDownloadImage",
                        "image_url",
                        base::trace_event::ValueToString(image_url));
  std::move(callback).Run(id, http_status_code, image_url, images,
                          original_image_sizes);
}

void WebContentsImpl::OnDialogClosed(int render_process_id,
                                     int render_frame_id,
                                     JavaScriptDialogCallback response_callback,
                                     base::ScopedClosureRunner fullscreen_block,
                                     bool dialog_was_suppressed,
                                     bool success,
                                     const base::string16& user_input) {
  RenderFrameHostImpl* rfh = RenderFrameHostImpl::FromID(render_process_id,
                                                         render_frame_id);
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::OnDialogClosed",
                        "render_frame_host",
                        base::trace_event::ToTracedValue(rfh));
  last_dialog_suppressed_ = dialog_was_suppressed;
  fullscreen_block.RunAndReset();

  javascript_dialog_navigation_deferrer_.reset();

  if (is_showing_before_unload_dialog_ && !success) {
    // It is possible for the current RenderFrameHost to have changed in the
    // meantime.  Do not reset the navigation state in that case.
    if (rfh && rfh == rfh->frame_tree_node()->current_frame_host()) {
      rfh->frame_tree_node()->BeforeUnloadCanceled();
      controller_.DiscardNonCommittedEntries();
    }

    // Update the URL display either way, to avoid showing a stale URL.
    NotifyNavigationStateChanged(INVALIDATE_TYPE_URL);

    observers_.ForEachObserver([&](WebContentsObserver* observer) {
      observer->BeforeUnloadDialogCancelled();
    });
  }

  std::move(response_callback).Run(success, user_input);

  std::vector<protocol::PageHandler*> page_handlers =
      protocol::PageHandler::EnabledForWebContents(this);
  for (auto* handler : page_handlers)
    handler->DidCloseJavaScriptDialog(success, user_input);

  is_showing_javascript_dialog_ = false;
  is_showing_before_unload_dialog_ = false;
}

bool WebContentsImpl::IsHidden() {
  return !IsBeingCaptured() && visibility_ != Visibility::VISIBLE;
}

int WebContentsImpl::GetOuterDelegateFrameTreeNodeId() {
  return node_.outer_contents_frame_tree_node_id();
}

RenderWidgetHostImpl* WebContentsImpl::GetFullscreenRenderWidgetHost() const {
  return RenderWidgetHostImpl::FromID(fullscreen_widget_process_id_,
                                      fullscreen_widget_routing_id_);
}

RenderFrameHostManager* WebContentsImpl::GetRenderManager() const {
  return frame_tree_.root()->render_manager();
}

BrowserPluginGuest* WebContentsImpl::GetBrowserPluginGuest() const {
  return browser_plugin_guest_.get();
}

void WebContentsImpl::SetBrowserPluginGuest(
    std::unique_ptr<BrowserPluginGuest> guest) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::SetBrowserPluginGuest");
  DCHECK(!browser_plugin_guest_);
  DCHECK(guest);
  browser_plugin_guest_ = std::move(guest);
}

base::UnguessableToken WebContentsImpl::GetAudioGroupId() {
  return GetAudioStreamFactory()->group_id();
}

const std::vector<blink::mojom::FaviconURLPtr>&
WebContentsImpl::GetFaviconURLs() {
  return favicon_urls_;
}

// The Mac implementation  of the next two methods is in
// web_contents_impl_mac.mm
#if !defined(OS_MAC)

void WebContentsImpl::Resize(const gfx::Rect& new_bounds) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::Resize");
#if defined(USE_AURA)
  aura::Window* window = GetNativeView();
  window->SetBounds(gfx::Rect(window->bounds().origin(), new_bounds.size()));
#elif defined(OS_ANDROID)
  content::RenderWidgetHostView* view = GetRenderWidgetHostView();
  if (view)
    view->SetBounds(new_bounds);
#endif
}

gfx::Size WebContentsImpl::GetSize() {
#if defined(USE_AURA)
  aura::Window* window = GetNativeView();
  return window->bounds().size();
#elif defined(OS_ANDROID)
  ui::ViewAndroid* view_android = GetNativeView();
  return view_android->bounds().size();
#endif
}

#endif  // !defined(OS_MAC)

BrowserPluginEmbedder* WebContentsImpl::GetBrowserPluginEmbedder() const {
  return browser_plugin_embedder_.get();
}

void WebContentsImpl::CreateBrowserPluginEmbedderIfNecessary() {
  OPTIONAL_TRACE_EVENT0(
      "content", "WebContentsImpl::CreateBrowserPluginEmbedderIfNecessary");
  if (browser_plugin_embedder_)
    return;
  browser_plugin_embedder_.reset(BrowserPluginEmbedder::Create(this));
}

gfx::Size WebContentsImpl::GetSizeForMainFrame() {
  if (delegate_) {
    // The delegate has a chance to specify a size independent of the UI.
    gfx::Size delegate_size = delegate_->GetSizeForNewRenderView(this);
    if (!delegate_size.IsEmpty())
      return delegate_size;
  }

  // Device emulation, when enabled, can specify a size independent of the UI.
  if (!device_emulation_size_.IsEmpty())
    return device_emulation_size_;

  return GetContainerBounds().size();
}

void WebContentsImpl::OnFrameRemoved(RenderFrameHost* render_frame_host) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::OnFrameRemoved",
                        "render_frame_host",
                        base::trace_event::ToTracedValue(render_frame_host));
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->FrameDeleted(render_frame_host);
  });
}

void WebContentsImpl::OnPreferredSizeChanged(const gfx::Size& old_size) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::OnPreferredSizeChanged");
  if (!delegate_)
    return;
  const gfx::Size new_size = GetPreferredSize();
  if (new_size != old_size)
    delegate_->UpdatePreferredSize(this, new_size);
}

std::unique_ptr<WebUIImpl> WebContentsImpl::CreateWebUI(
    RenderFrameHost* frame_host,
    const GURL& url) {
  TRACE_EVENT2("content", "WebContentsImpl::CreateWebUI", "frame_host",
               base::trace_event::ToTracedValue(frame_host), "url",
               base::trace_event::ValueToString(url));
  std::unique_ptr<WebUIImpl> web_ui =
      std::make_unique<WebUIImpl>(this, frame_host);
  std::unique_ptr<WebUIController> controller(
      WebUIControllerFactoryRegistry::GetInstance()
          ->CreateWebUIControllerForURL(web_ui.get(), url));
  if (controller) {
    web_ui->SetController(std::move(controller));
    return web_ui;
  }

  return nullptr;
}

FindRequestManager* WebContentsImpl::GetFindRequestManager() {
  for (auto* contents = this; contents;
       contents = contents->GetOuterWebContents()) {
    if (contents->find_request_manager_)
      return contents->find_request_manager_.get();
  }

  return nullptr;
}

FindRequestManager* WebContentsImpl::GetOrCreateFindRequestManager() {
  if (FindRequestManager* manager = GetFindRequestManager())
    return manager;

  DCHECK(!browser_plugin_guest_ || GetOuterWebContents());

  // No existing FindRequestManager found, so one must be created.
  find_request_manager_ = std::make_unique<FindRequestManager>(this);

  // Concurrent find sessions must not overlap, so destroy any existing
  // FindRequestManagers in any inner WebContentses.
  for (WebContents* contents : GetWebContentsAndAllInner()) {
    auto* web_contents_impl = static_cast<WebContentsImpl*>(contents);
    if (web_contents_impl == this)
      continue;
    if (web_contents_impl->find_request_manager_) {
      web_contents_impl->find_request_manager_->StopFinding(
          STOP_FIND_ACTION_CLEAR_SELECTION);
      web_contents_impl->find_request_manager_.release();
    }
  }

  return find_request_manager_.get();
}

void WebContentsImpl::NotifyFindReply(int request_id,
                                      int number_of_matches,
                                      const gfx::Rect& selection_rect,
                                      int active_match_ordinal,
                                      bool final_update) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::NotifyFindReply");
  if (delegate_ && !is_being_destroyed_ &&
      !GetMainFrame()->GetProcess()->FastShutdownStarted()) {
    delegate_->FindReply(this, request_id, number_of_matches, selection_rect,
                         active_match_ordinal, final_update);
  }
}

void WebContentsImpl::IncrementBluetoothConnectedDeviceCount() {
  OPTIONAL_TRACE_EVENT0(
      "content", "WebContentsImpl::IncrementBluetoothConnectedDeviceCount");
  // Trying to invalidate the tab state while being destroyed could result in a
  // use after free.
  if (IsBeingDestroyed()) {
    return;
  }
  // Notify for UI updates if the state changes.
  bluetooth_connected_device_count_++;
  if (bluetooth_connected_device_count_ == 1) {
    OnIsConnectedToBluetoothDeviceChanged(true);
  }
}

void WebContentsImpl::DecrementBluetoothConnectedDeviceCount() {
  OPTIONAL_TRACE_EVENT0(
      "content", "WebContentsImpl::DecrementBluetoothConnectedDeviceCount");
  // Trying to invalidate the tab state while being destroyed could result in a
  // use after free.
  if (IsBeingDestroyed()) {
    return;
  }
  // Notify for UI updates if the state changes.
  DCHECK_NE(bluetooth_connected_device_count_, 0u);
  bluetooth_connected_device_count_--;
  if (bluetooth_connected_device_count_ == 0) {
    OnIsConnectedToBluetoothDeviceChanged(false);
  }
}

void WebContentsImpl::OnIsConnectedToBluetoothDeviceChanged(
    bool is_connected_to_bluetooth_device) {
  OPTIONAL_TRACE_EVENT0(
      "content", "WebContentsImpl::OnIsConnectedToBluetoothDeviceChanged");
  NotifyNavigationStateChanged(INVALIDATE_TYPE_TAB);
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->OnIsConnectedToBluetoothDeviceChanged(
        is_connected_to_bluetooth_device);
  });
}

void WebContentsImpl::IncrementBluetoothScanningSessionsCount() {
  OPTIONAL_TRACE_EVENT0(
      "content", "WebContentsImpl::IncrementBluetoothScanningSessionsCount");
  // Trying to invalidate the tab state while being destroyed could result in a
  // use after free.
  if (IsBeingDestroyed())
    return;

  // Notify for UI updates if the state changes.
  bluetooth_scanning_sessions_count_++;
  if (bluetooth_scanning_sessions_count_ == 1)
    NotifyNavigationStateChanged(INVALIDATE_TYPE_TAB);
}

void WebContentsImpl::DecrementBluetoothScanningSessionsCount() {
  OPTIONAL_TRACE_EVENT0(
      "content", "WebContentsImpl::DecrementBluetoothScanningSessionsCount");
  // Trying to invalidate the tab state while being destroyed could result in a
  // use after free.
  if (IsBeingDestroyed())
    return;

  DCHECK_NE(0u, bluetooth_scanning_sessions_count_);
  bluetooth_scanning_sessions_count_--;
  if (bluetooth_scanning_sessions_count_ == 0)
    NotifyNavigationStateChanged(INVALIDATE_TYPE_TAB);
}

void WebContentsImpl::IncrementSerialActiveFrameCount() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::IncrementSerialActiveFrameCount");
  // Trying to invalidate the tab state while being destroyed could result in a
  // use after free.
  if (IsBeingDestroyed())
    return;

  // Notify for UI updates if the state changes.
  serial_active_frame_count_++;
  if (serial_active_frame_count_ == 1)
    NotifyNavigationStateChanged(INVALIDATE_TYPE_TAB);
}

void WebContentsImpl::DecrementSerialActiveFrameCount() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::DecrementSerialActiveFrameCount");
  // Trying to invalidate the tab state while being destroyed could result in a
  // use after free.
  if (IsBeingDestroyed())
    return;

  // Notify for UI updates if the state changes.
  DCHECK_NE(0u, serial_active_frame_count_);
  serial_active_frame_count_--;
  if (serial_active_frame_count_ == 0)
    NotifyNavigationStateChanged(INVALIDATE_TYPE_TAB);
}

void WebContentsImpl::IncrementHidActiveFrameCount() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::IncrementHidActiveFrameCount");
  // Trying to invalidate the tab state while being destroyed could result in a
  // use after free.
  if (IsBeingDestroyed())
    return;

  // Notify for UI updates if the active frame count transitions from zero to
  // non-zero.
  hid_active_frame_count_++;
  if (hid_active_frame_count_ == 1)
    NotifyNavigationStateChanged(INVALIDATE_TYPE_TAB);
}

void WebContentsImpl::DecrementHidActiveFrameCount() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::DecrementHidActiveFrameCount");
  // Trying to invalidate the tab state while being destroyed could result in a
  // use after free.
  if (IsBeingDestroyed())
    return;

  // Notify for UI updates if the active frame count transitions from non-zero
  // to zero.
  DCHECK_NE(0u, hid_active_frame_count_);
  hid_active_frame_count_--;
  if (hid_active_frame_count_ == 0)
    NotifyNavigationStateChanged(INVALIDATE_TYPE_TAB);
}

void WebContentsImpl::IncrementNativeFileSystemHandleCount() {
  OPTIONAL_TRACE_EVENT0(
      "content", "WebContentsImpl::IncrementNativeFileSystemHandleCount");
  // Trying to invalidate the tab state while being destroyed could result in a
  // use after free.
  if (IsBeingDestroyed())
    return;

  // Notify for UI updates if the state changes. Need both TYPE_TAB and TYPE_URL
  // to update both the tab-level usage indicator and the usage indicator in the
  // omnibox.
  native_file_system_handle_count_++;
  if (native_file_system_handle_count_ == 1) {
    NotifyNavigationStateChanged(static_cast<content::InvalidateTypes>(
        INVALIDATE_TYPE_TAB | INVALIDATE_TYPE_URL));
  }
}

void WebContentsImpl::DecrementNativeFileSystemHandleCount() {
  OPTIONAL_TRACE_EVENT0(
      "content", "WebContentsImpl::DecrementNativeFileSystemHandleCount");
  // Trying to invalidate the tab state while being destroyed could result in a
  // use after free.
  if (IsBeingDestroyed())
    return;

  // Notify for UI updates if the state changes. Need both TYPE_TAB and TYPE_URL
  // to update both the tab-level usage indicator and the usage indicator in the
  // omnibox.
  DCHECK_NE(0u, native_file_system_handle_count_);
  native_file_system_handle_count_--;
  if (native_file_system_handle_count_ == 0) {
    NotifyNavigationStateChanged(static_cast<content::InvalidateTypes>(
        INVALIDATE_TYPE_TAB | INVALIDATE_TYPE_URL));
  }
}

void WebContentsImpl::SetHasPersistentVideo(bool has_persistent_video) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::SetHasPersistentVideo",
                        "has_persistent_video", has_persistent_video,
                        "had_persistent_value", has_persistent_video_);
  if (has_persistent_video_ == has_persistent_video)
    return;

  has_persistent_video_ = has_persistent_video;
  NotifyPreferencesChanged();
  media_web_contents_observer()->RequestPersistentVideo(has_persistent_video);
}

void WebContentsImpl::SetSpatialNavigationDisabled(bool disabled) {
  OPTIONAL_TRACE_EVENT2(
      "content", "WebContentsImpl::SetSpatialNavigationDisabled", "disabled",
      disabled, "was_disabled", is_spatial_navigation_disabled_);
  if (is_spatial_navigation_disabled_ == disabled)
    return;

  is_spatial_navigation_disabled_ = disabled;
  NotifyPreferencesChanged();
}

void WebContentsImpl::BrowserPluginGuestWillDetach() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::BrowserPluginGuestWillDetach");
  WebContentsImpl* outermost = GetOutermostWebContents();
  if (this != outermost && ContainsOrIsFocusedWebContents())
    outermost->SetAsFocusedWebContentsIfNecessary();
}

PictureInPictureResult WebContentsImpl::EnterPictureInPicture(
    const viz::SurfaceId& surface_id,
    const gfx::Size& natural_size) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::EnterPictureInPicture");
  return delegate_
             ? delegate_->EnterPictureInPicture(this, surface_id, natural_size)
             : PictureInPictureResult::kNotSupported;
}

void WebContentsImpl::ExitPictureInPicture() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::ExitPictureInPicture");
  if (delegate_)
    delegate_->ExitPictureInPicture();
}

#if defined(OS_ANDROID)
void WebContentsImpl::NotifyFindMatchRectsReply(
    int version,
    const std::vector<gfx::RectF>& rects,
    const gfx::RectF& active_rect) {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::NotifyFindMatchRectsReply");
  if (delegate_)
    delegate_->FindMatchRectsReply(this, version, rects, active_rect);
}
#endif

void WebContentsImpl::SetForceDisableOverscrollContent(bool force_disable) {
  OPTIONAL_TRACE_EVENT1("content",
                        "WebContentsImpl::SetForceDisableOverscrollContent",
                        "force_disable", force_disable);
  force_disable_overscroll_content_ = force_disable;
  if (view_)
    view_->SetOverscrollControllerEnabled(CanOverscrollContent());
}

bool WebContentsImpl::SetDeviceEmulationSize(const gfx::Size& new_size) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::SetDeviceEmulationSize");
  device_emulation_size_ = new_size;
  RenderWidgetHostView* rwhv = GetMainFrame()->GetView();

  const gfx::Size current_size = rwhv->GetViewBounds().size();
  if (view_size_before_emulation_.IsEmpty())
    view_size_before_emulation_ = current_size;

  if (current_size != new_size)
    rwhv->SetSize(new_size);

  return current_size != new_size;
}

void WebContentsImpl::ClearDeviceEmulationSize() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::ClearDeviceEmulationSize");
  RenderWidgetHostView* rwhv = GetMainFrame()->GetView();
  // WebContentsView could get resized during emulation, which also resizes
  // RWHV. If it happens, assume user would like to keep using the size after
  // emulation.
  // TODO(jzfeng): Prohibit resizing RWHV through any other means (at least when
  // WebContentsView size changes).
  if (!view_size_before_emulation_.IsEmpty() && rwhv &&
      rwhv->GetViewBounds().size() == device_emulation_size_) {
    rwhv->SetSize(view_size_before_emulation_);
  }
  device_emulation_size_ = gfx::Size();
  view_size_before_emulation_ = gfx::Size();
}

ForwardingAudioStreamFactory* WebContentsImpl::GetAudioStreamFactory() {
  if (!audio_stream_factory_) {
    audio_stream_factory_.emplace(
        this,
        // BrowserMainLoop::GetInstance() may be null in unit tests.
        BrowserMainLoop::GetInstance()
            ? static_cast<media::UserInputMonitorBase*>(
                  BrowserMainLoop::GetInstance()->user_input_monitor())
            : nullptr,
        AudioStreamBrokerFactory::CreateImpl());
  }

  return &*audio_stream_factory_;
}

void WebContentsImpl::MediaStartedPlaying(
    const WebContentsObserver::MediaPlayerInfo& media_info,
    const MediaPlayerId& id) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::MediaStartedPlaying");
  if (media_info.has_video)
    currently_playing_video_count_++;

  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->MediaStartedPlaying(media_info, id);
  });
}

void WebContentsImpl::MediaStoppedPlaying(
    const WebContentsObserver::MediaPlayerInfo& media_info,
    const MediaPlayerId& id,
    WebContentsObserver::MediaStoppedReason reason) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::MediaStoppedPlaying");
  if (media_info.has_video)
    currently_playing_video_count_--;

  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->MediaStoppedPlaying(media_info, id, reason);
  });
}

void WebContentsImpl::MediaResized(const gfx::Size& size,
                                   const MediaPlayerId& id) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::MediaResized");
  cached_video_sizes_[id] = size;

  observers_.ForEachObserver(
      [&](WebContentsObserver* observer) { observer->MediaResized(size, id); });
}

void WebContentsImpl::MediaBufferUnderflow(const MediaPlayerId& id) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::MediaBufferUnderflow");
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->MediaBufferUnderflow(id);
  });
}

void WebContentsImpl::MediaEffectivelyFullscreenChanged(bool is_fullscreen) {
  OPTIONAL_TRACE_EVENT1("content",
                        "WebContentsImpl::MediaEffectivelyFullscreenChanged",
                        "is_fullscreen", is_fullscreen);
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->MediaEffectivelyFullscreenChanged(is_fullscreen);
  });
}

base::Optional<gfx::Size> WebContentsImpl::GetFullscreenVideoSize() {
  base::Optional<MediaPlayerId> id =
      media_web_contents_observer_->GetFullscreenVideoMediaPlayerId();
  if (id && base::Contains(cached_video_sizes_, id.value()))
    return base::Optional<gfx::Size>(cached_video_sizes_[id.value()]);
  return base::nullopt;
}

int WebContentsImpl::GetCurrentlyPlayingVideoCount() {
  return currently_playing_video_count_;
}

void WebContentsImpl::AudioContextPlaybackStarted(RenderFrameHost* host,
                                                  int context_id) {
  OPTIONAL_TRACE_EVENT1(
      "content", "WebContentsImpl::AudioContextPlaybackStarted",
      "render_frame_host", base::trace_event::ToTracedValue(host));
  WebContentsObserver::AudioContextId audio_context_id(host, context_id);
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->AudioContextPlaybackStarted(audio_context_id);
  });
}

void WebContentsImpl::AudioContextPlaybackStopped(RenderFrameHost* host,
                                                  int context_id) {
  OPTIONAL_TRACE_EVENT1(
      "content", "WebContentsImpl::AudioContextPlaybackStopped",
      "render_frame_host", base::trace_event::ToTracedValue(host));
  WebContentsObserver::AudioContextId audio_context_id(host, context_id);
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->AudioContextPlaybackStopped(audio_context_id);
  });
}

void WebContentsImpl::OnFrameAudioStateChanged(RenderFrameHost* host,
                                               bool is_audible) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::OnFrameAudioStateChanged",
                        "render_frame_host",
                        base::trace_event::ToTracedValue(host), "is_audible",
                        is_audible);
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->OnFrameAudioStateChanged(host, is_audible);
  });
}

media::MediaMetricsProvider::RecordAggregateWatchTimeCallback
WebContentsImpl::GetRecordAggregateWatchTimeCallback() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::RecordAggregateWatchTimeCallback");
  if (!delegate_ || !delegate_->GetDelegateWeakPtr())
    return base::DoNothing();

  return base::BindRepeating(
      [](base::WeakPtr<WebContentsDelegate> delegate, GURL last_committed_url,
         base::TimeDelta total_watch_time, base::TimeDelta time_stamp,
         bool has_video, bool has_audio) {
        content::MediaPlayerWatchTime watch_time(
            last_committed_url, last_committed_url.GetOrigin(),
            total_watch_time, time_stamp, has_video, has_audio);

        // Save the watch time if the delegate is still alive.
        if (delegate)
          delegate->MediaWatchTimeChanged(watch_time);
      },
      delegate_->GetDelegateWeakPtr(), GetMainFrameLastCommittedURL());
}

RenderFrameHostImpl* WebContentsImpl::GetMainFrameForInnerDelegate(
    FrameTreeNode* frame_tree_node) {
  if (auto* web_contents = node_.GetInnerWebContentsInFrame(frame_tree_node))
    return web_contents->GetMainFrame();
  return nullptr;
}

bool WebContentsImpl::IsFrameLowPriority(
    const RenderFrameHost* render_frame_host) {
  if (!delegate_)
    return false;
  return delegate_->IsFrameLowPriority(this, render_frame_host);
}

void WebContentsImpl::IsClipboardPasteAllowed(
    const GURL& url,
    const ui::ClipboardFormatType& data_type,
    const std::string& data,
    IsClipboardPasteAllowedCallback callback) {
  ++suppress_unresponsive_renderer_count_;
  GetContentClient()->browser()->IsClipboardPasteAllowed(
      this, url, data_type, data,
      base::BindOnce(&WebContentsImpl::IsClipboardPasteAllowedWrapperCallback,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void WebContentsImpl::IsClipboardPasteAllowedWrapperCallback(
    IsClipboardPasteAllowedCallback callback,
    ClipboardPasteAllowed allowed) {
  std::move(callback).Run(allowed);
  --suppress_unresponsive_renderer_count_;
}

bool WebContentsImpl::HasSeenRecentScreenOrientationChange() {
  static constexpr base::TimeDelta kMaxInterval =
      base::TimeDelta::FromSeconds(1);
  base::TimeDelta delta =
      ui::EventTimeForNow() - last_screen_orientation_change_time_;
  // Return whether a screen orientation change happened in the last 1 second.
  return delta <= kMaxInterval;
}

bool WebContentsImpl::IsTransientAllowFullscreenActive() const {
  return transient_allow_fullscreen_.IsActive();
}

void WebContentsImpl::DidChangeScreenOrientation() {
  last_screen_orientation_change_time_ = ui::EventTimeForNow();
}

bool WebContentsImpl::ShowPopupMenu(
    RenderFrameHostImpl* render_frame_host,
    mojo::PendingRemote<blink::mojom::PopupMenuClient>* popup_client,
    const gfx::Rect& bounds,
    int32_t item_height,
    double font_size,
    int32_t selected_item,
    std::vector<blink::mojom::MenuItemPtr>* menu_items,
    bool right_aligned,
    bool allow_multiple_selection) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::ShowPopupMenu",
                        "render_frame_host",
                        base::trace_event::ToTracedValue(render_frame_host));
  for (auto& observer : observers_.observer_list()) {
    if (observer.ShowPopupMenu(render_frame_host, popup_client, bounds,
                               item_height, font_size, selected_item,
                               menu_items, right_aligned,
                               allow_multiple_selection)) {
      return true;
    }
  }
  return false;
}

void WebContentsImpl::UpdateWebContentsVisibility(Visibility visibility) {
  OPTIONAL_TRACE_EVENT1("content",
                        "WebContentsImpl::UpdateWebContentsVisibility",
                        "visibility", visibility);
  // Occlusion is disabled when |features::kWebContentsOcclusion| is disabled
  // (for power and speed impact assessment) or when
  // |switches::kDisableBackgroundingOccludedWindowsForTesting| is specified on
  // the command line (to avoid flakiness in browser tests).
  const bool occlusion_is_disabled =
      !base::FeatureList::IsEnabled(features::kWebContentsOcclusion) ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableBackgroundingOccludedWindowsForTesting);
  if (occlusion_is_disabled && visibility == Visibility::OCCLUDED)
    visibility = Visibility::VISIBLE;

  if (!did_first_set_visible_) {
    if (visibility == Visibility::VISIBLE) {
      // A WebContents created with CreateParams::initially_hidden = false
      // starts with GetVisibility() == Visibility::VISIBLE even though it is
      // not really visible. Call WasShown() when it becomes visible for real as
      // the page load mechanism and some WebContentsObserver rely on that.
      WasShown();
      did_first_set_visible_ = true;
    }

    // Trust the initial visibility of the WebContents and do not switch it to
    // HIDDEN or OCCLUDED before it becomes VISIBLE for real. Doing so would
    // result in destroying resources that would immediately be recreated (e.g.
    // UpdateWebContents(HIDDEN) can be called when a WebContents is added to a
    // hidden window that is about to be shown).

    return;
  }

  if (visibility == visibility_)
    return;

  UpdateVisibilityAndNotifyPageAndView(visibility);
}

void WebContentsImpl::UpdateOverridingUserAgent() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::UpdateOverridingUserAgent");
  NotifyPreferencesChanged();
}

void WebContentsImpl::SetJavaScriptDialogManagerForTesting(
    JavaScriptDialogManager* dialog_manager) {
  OPTIONAL_TRACE_EVENT0(
      "content", "WebContentsImpl::SetJavaScriptDialogManagerForTesting");
  dialog_manager_ = dialog_manager;
}

void WebContentsImpl::RemoveReceiverSet(const std::string& interface_name) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::RemoveReceiverSet",
                        "interface_name", interface_name);
  auto it = receiver_sets_.find(interface_name);
  if (it != receiver_sets_.end())
    receiver_sets_.erase(it);
}

bool WebContentsImpl::AddDomainInfoToRapporSample(rappor::Sample* sample) {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::AddDomainInfoToRapporSample");
  // Here we associate this metric to the main frame URL regardless of what
  // caused it.
  sample->SetStringField("Domain", ::rappor::GetDomainAndRegistrySampleFromGURL(
                                       GetLastCommittedURL()));
  return true;
}

void WebContentsImpl::ShowInsecureLocalhostWarningIfNeeded() {
  OPTIONAL_TRACE_EVENT0(
      "content", "WebContentsImpl::ShowInsecureLocalhostWarningIfNeeded");
  bool allow_localhost = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAllowInsecureLocalhost);
  if (!allow_localhost)
    return;

  content::NavigationEntry* entry = GetController().GetLastCommittedEntry();
  if (!entry || !net::IsLocalhost(entry->GetURL()))
    return;

  content::SSLStatus ssl_status = entry->GetSSL();
  if (!net::IsCertStatusError(ssl_status.cert_status))
    return;

  GetMainFrame()->AddMessageToConsole(
      blink::mojom::ConsoleMessageLevel::kWarning,
      base::StringPrintf("This site does not have a valid SSL "
                         "certificate! Without SSL, your site's and "
                         "visitors' data is vulnerable to theft and "
                         "tampering. Get a valid SSL certificate before"
                         " releasing your website to the public."));
}

bool WebContentsImpl::IsShowingContextMenuOnPage() const {
  return showing_context_menu_;
}

download::DownloadUrlParameters::RequestHeadersType
WebContentsImpl::ParseDownloadHeaders(const std::string& headers) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::ParseDownloadHeaders",
                        "headers", headers);
  download::DownloadUrlParameters::RequestHeadersType request_headers;
  for (const base::StringPiece& key_value : base::SplitStringPiece(
           headers, "\r\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    std::vector<std::string> pair = base::SplitString(
        key_value, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    if (2ul == pair.size())
      request_headers.push_back(make_pair(pair[0], pair[1]));
  }
  return request_headers;
}

void WebContentsImpl::SetOpenerForNewContents(FrameTreeNode* opener,
                                              bool opener_suppressed) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::SetOpenerForNewContents");
  if (opener) {
    FrameTreeNode* new_root = GetFrameTree()->root();

    // For the "original opener", track the opener's main frame instead, because
    // if the opener is a subframe, the opener tracking could be easily bypassed
    // by spawning from a subframe and deleting the subframe.
    // https://crbug.com/705316
    new_root->SetOriginalOpener(opener->frame_tree()->root());
    new_root->SetOpenerDevtoolsFrameToken(opener->devtools_frame_token());

    if (!opener_suppressed) {
      new_root->SetOpener(opener);
      created_with_opener_ = true;
    }
  }
}

void WebContentsImpl::MediaMutedStatusChanged(const MediaPlayerId& id,
                                              bool muted) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::MediaMutedStatusChanged",
                        "muted", muted);
  observers_.ForEachObserver([&](WebContentsObserver* observer) {
    observer->MediaMutedStatusChanged(id, muted);
  });
}

void WebContentsImpl::SetVisibilityForChildViews(bool visible) {
  OPTIONAL_TRACE_EVENT1("content",
                        "WebContentsImpl::SetVisibilityForChildViews",
                        "visible", visible);
  GetMainFrame()->SetVisibilityForChildViews(visible);
}

void WebContentsImpl::OnNativeThemeUpdated(ui::NativeTheme* observed_theme) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::OnNativeThemeUpdated");
  DCHECK(native_theme_observer_.IsObserving(observed_theme));

  bool using_dark_colors = observed_theme->ShouldUseDarkColors();
  ui::NativeTheme::PreferredColorScheme preferred_color_scheme =
      observed_theme->GetPreferredColorScheme();
  bool preferences_changed = false;

  if (using_dark_colors_ != using_dark_colors) {
    using_dark_colors_ = using_dark_colors;
    preferences_changed = true;
  }
  if (preferred_color_scheme_ != preferred_color_scheme) {
    preferred_color_scheme_ = preferred_color_scheme;
    preferences_changed = true;
  }

  if (preferences_changed)
    NotifyPreferencesChanged();
}

blink::mojom::FrameWidgetInputHandler*
WebContentsImpl::GetFocusedFrameWidgetInputHandler() {
  auto* focused_render_widget_host =
      GetFocusedRenderWidgetHost(GetMainFrame()->GetRenderWidgetHost());
  if (!focused_render_widget_host)
    return nullptr;
  return focused_render_widget_host->GetFrameWidgetInputHandler();
}

ukm::SourceId WebContentsImpl::GetCurrentPageUkmSourceId() {
  return GetMainFrame()->GetPageUkmSourceId();
}

std::set<RenderViewHostImpl*>
WebContentsImpl::GetRenderViewHostsIncludingBackForwardCached() {
  std::set<RenderViewHostImpl*> render_view_hosts;

  // Add RenderViewHostImpls outside of BackForwardCache.
  for (auto& render_view_host : frame_tree_.render_view_hosts()) {
    render_view_hosts.insert(render_view_host.second);
  }

  // Add RenderViewHostImpls in BackForwardCache.
  const auto& entries = GetController().GetBackForwardCache().GetEntries();
  for (const auto& entry : entries) {
    std::set<RenderViewHostImpl*> bfcached_hosts = entry->render_view_hosts;
    render_view_hosts.insert(bfcached_hosts.begin(), bfcached_hosts.end());
  }

  return render_view_hosts;
}

void WebContentsImpl::RenderFrameHostStateChanged(
    RenderFrameHost* render_frame_host,
    LifecycleState old_state,
    LifecycleState new_state) {
  OPTIONAL_TRACE_EVENT2(
      "content", "WebContentsImpl::RenderFrameHostStateChanged",
      "render_frame_host", base::trace_event::ToTracedValue(render_frame_host),
      "states",
      base::trace_event::TracedValue::Build(
          {{"old", base::trace_event::ValueToString(old_state)},
           {"new", base::trace_event::ValueToString(new_state)}}));
  if (render_frame_host->GetParent())
    return;

  if (old_state == LifecycleState::kActive &&
      new_state != LifecycleState::kActive) {
    // TODO(sreejakshetty): Remove this reset when ColorChooser becomes
    // per-frame.
    // Close the color chooser popup when RenderFrameHost changes state from
    // kActive.
    color_chooser_.reset();
  }
}

}  // namespace content
