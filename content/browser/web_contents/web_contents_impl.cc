// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_impl.h"

#include <stddef.h>

#include <cmath>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "base/base_switches.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/process/process.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/optional_trace_event.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cc/input/browser_controls_offset_tags_info.h"
#include "components/attribution_reporting/features.h"
#include "components/download/public/common/download_stats.h"
#include "components/input/cursor_manager.h"
#include "components/input/render_widget_host_input_event_router.h"
#include "components/url_formatter/url_formatter.h"
#include "components/viz/common/features.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/attribution_reporting/attribution_host.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_os_level_manager.h"
#include "content/browser/bad_message.h"
#include "content/browser/browser_context_impl.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/browser_plugin/browser_plugin_embedder.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/closewatcher/close_listener_manager.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/device_posture/device_posture_provider_impl.h"
#include "content/browser/devtools/protocol/page_handler.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/display_cutout/display_cutout_host_impl.h"
#include "content/browser/dom_storage/dom_storage_context_wrapper.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "content/browser/download/mhtml_generation_manager.h"
#include "content/browser/download/save_package.h"
#include "content/browser/fenced_frame/fenced_frame.h"
#include "content/browser/find_request_manager.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/host_zoom_map_impl.h"
#include "content/browser/media/audio_stream_monitor.h"
#include "content/browser/media/media_web_contents_observer.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/browser/permissions/permission_util.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/browser/preloading/preloading.h"
#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "content/browser/preloading/prerender/prerender_host_registry.h"
#include "content/browser/preloading/prerender/prerender_metrics.h"
#include "content/browser/preloading/prerender/prerender_new_tab_handle.h"
#include "content/browser/renderer_host/agent_scheduling_group_host.h"
#include "content/browser/renderer_host/cross_process_frame_connector.h"
#include "content/browser/renderer_host/frame_token_message_queue.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/input/touch_emulator_impl.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/page_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_factory.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/renderer_host/spare_render_process_host_manager_impl.h"
#include "content/browser/renderer_host/text_input_manager.h"
#include "content/browser/renderer_host/visible_time_request_trigger.h"
#include "content/browser/screen_details/screen_change_monitor.h"
#include "content/browser/screen_orientation/screen_orientation_provider.h"
#include "content/browser/shared_storage/shared_storage_budget_charger.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/wake_lock/wake_lock_context_host.h"
#include "content/browser/web_contents/java_script_dialog_commit_deferring_condition.h"
#include "content/browser/web_contents/slow_web_preference_cache.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "content/browser/web_contents/web_contents_view_child_frame.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/browser/webui/web_ui_impl.h"
#include "content/common/content_switches_internal.h"
#include "content/common/features.h"
#include "content/public/browser/ax_inspect_factory.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_plugin_guest_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/color_chooser.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/disallow_activation_reason.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/focused_node_details.h"
#include "content/public/browser/frame_type.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/preview_cancel_reason.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/render_widget_host_observer.h"
#include "content/public/browser/restore_type.h"
#include "content/public/browser/scoped_accessibility_mode.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/referrer_type_converters.h"
#include "content/public/common/url_constants.h"
#include "media/base/media_switches.h"
#include "media/base/user_input_monitor.h"
#include "net/base/url_util.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "partition_alloc/buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "services/network/public/cpp/request_destination.h"
#include "services/network/public/cpp/web_sandbox_flags.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/blink/public/common/custom_handlers/protocol_handler_utils.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/resource_type_util.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/public/common/page_state/page_state.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/common/security/protocol_handler_security_level.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/common/widget/constants.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "third_party/blink/public/mojom/image_downloader/image_downloader.mojom.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom-shared.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "third_party/blink/public/mojom/page/draggable_region.mojom.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/accessibility/ax_tree_combiner.h"
#include "ui/accessibility/platform/browser_accessibility.h"
#include "ui/base/ime/mojom/virtual_keyboard_types.mojom.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/pointer/pointer_device.h"
#include "ui/base/ui_base_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_manager.h"
#include "ui/color/color_provider_utils.h"
#include "ui/compositor/compositor.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/animation/animation.h"

#if BUILDFLAG(IS_WIN)
#include "base/threading/thread_restrictions.h"
#include "content/browser/renderer_host/dip_util.h"
#include "ui/gfx/geometry/dip_util.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "content/browser/android/java_interfaces_impl.h"
#include "content/browser/android/nfc_host.h"
#include "content/browser/navigation_transitions/back_forward_transition_animation_manager_android.h"
#include "content/browser/web_contents/web_contents_android.h"
#include "content/browser/web_contents/web_contents_view_android.h"
#include "services/device/public/mojom/nfc.mojom.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "ui/android/view_android.h"
#include "ui/base/device_form_factor.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
#include "content/browser/date_time_chooser/date_time_chooser.h"
#endif

#if BUILDFLAG(ENABLE_PPAPI)
#include "content/browser/media/session/pepper_playback_observer.h"
#endif

#if BUILDFLAG(ENABLE_VR)
#include "content/browser/xr/service/xr_runtime_manager_impl.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/wm/core/window_util.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "content/public/browser/document_picture_in_picture_window_controller.h"
#include "content/public/browser/picture_in_picture_window_controller.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace content {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(CrashRepHandlingOutcome)
enum class CrashRepHandlingOutcome {
  // Dialog wasn't suppressed and the crash report will be potentially queued.
  kPotentiallyQueued = 0,
  // Dialog was suppressed so the crash report was dropped.
  kDropped = 1,
  kMaxValue = kDropped,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:CrashRepHandlingOutcome)

// The window which we dobounce load info updates in.
constexpr auto kUpdateLoadStatesInterval = base::Milliseconds(250);

// Kill switch for crash immediately on dangling BrowserContext.
BASE_FEATURE(kCrashOnDanglingBrowserContext,
             "CrashOnDanglingBrowserContext",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Kill switch for inner WebContents visibility updates.
BASE_FEATURE(kUpdateInnerWebContentsVisibility,
             "UpdateInnerWebContentsVisibility",
             base::FEATURE_ENABLED_BY_DEFAULT);

using LifecycleState = RenderFrameHost::LifecycleState;
using LifecycleStateImpl = RenderFrameHostImpl::LifecycleStateImpl;
using AttributionReportingOsRegistrar =
    ContentBrowserClient::AttributionReportingOsRegistrar;

base::LazyInstance<base::RepeatingCallbackList<void(WebContents*)>>::
    DestructorAtExit g_created_callbacks = LAZY_INSTANCE_INITIALIZER;

bool HasMatchingWidgetHost(FrameTree* tree, RenderWidgetHostImpl* host) {
  // This method scans the frame tree rather than checking whether
  // host->delegate() == this, which allows it to return false when the host
  // for a frame that is pending or pending deletion.
  if (!host) {
    return false;
  }

  for (FrameTreeNode* node : tree->NodesIncludingInnerTreeNodes()) {
    // We might cross a WebContents boundary here, but it's fine as we are only
    // comparing the RWHI with the given `host`, which is always guaranteed to
    // belong to the same WebContents as `tree`.
    if (node->current_frame_host()->GetRenderWidgetHost() == host) {
      DCHECK_EQ(WebContentsImpl::FromFrameTreeNode(node),
                WebContentsImpl::FromRenderWidgetHostImpl(host));
      return true;
    }
  }
  return false;
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
  // TODO(mustaq): This list should be based off the HTML spec:
  // https://html.spec.whatwg.org/multipage/interaction.html#tracking-user-activation,
  // and kGestureScrollBegin is a clear outlier.
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
      base::OnceCallback<void(bool, bool, const std::u16string&)>;

  explicit CloseDialogCallbackWrapper(CloseCallback callback)
      : callback_(std::move(callback)) {}

  void Run(bool dialog_was_suppressed,
           bool success,
           const std::u16string& user_input) {
    if (callback_.is_null()) {
      return;
    }
    std::move(callback_).Run(dialog_was_suppressed, success, user_input);
  }

 private:
  friend class base::RefCountedThreadSafe<CloseDialogCallbackWrapper>;
  ~CloseDialogCallbackWrapper() = default;

  CloseCallback callback_;
};

bool FrameCompareDepth(RenderFrameHostImpl* a, RenderFrameHostImpl* b) {
  return a->GetFrameDepth() < b->GetFrameDepth();
}

bool AreValidRegisterProtocolHandlerArguments(
    const std::string& protocol,
    const GURL& url,
    const url::Origin& origin,
    blink::ProtocolHandlerSecurityLevel security_level) {
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  if (policy->IsPseudoScheme(protocol)) {
    return false;
  }

  // Implementation of the protocol handler arguments normalization steps
  // defined in the spec.
  // https://html.spec.whatwg.org/multipage/system-state.html#normalize-protocol-handler-parameters
  //
  // Verify custom handler schemes for errors as described in steps 1 and 2
  if (!blink::IsValidCustomHandlerScheme(protocol, security_level)) {
    return false;
  }

  blink::URLSyntaxErrorCode code =
      blink::IsValidCustomHandlerURLSyntax(url, url.spec());
  if (code != blink::URLSyntaxErrorCode::kNoError) {
    return false;
  }

  // Verify custom handler URL security as described in steps 6 and 7
  if (!blink::IsAllowedCustomHandlerURL(url, security_level)) {
    return false;
  }
  url::Origin url_origin = url::Origin::Create(url);
  if (url_origin.opaque()) {
    return false;
  }
  if (security_level < blink::ProtocolHandlerSecurityLevel::kUntrustedOrigins &&
      !origin.IsSameOriginWith(url)) {
    return false;
  }

  return true;
}

void RecordMaxFrameCountUMA(size_t max_frame_count) {
  UMA_HISTOGRAM_COUNTS_10000("Navigation.MainFrame.MaxFrameCount",
                             max_frame_count);
}

// Returns the set of all WebContentses that are reachable from |web_contents|
// by applying some combination of
// WebContents::GetFirstWebContentsInLiveOriginalOpenerChain() and
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
      if (WebContents* opener_contents =
              current_contents
                  ->GetFirstWebContentsInLiveOriginalOpenerChain()) {
        current.insert(static_cast<WebContentsImpl*>(opener_contents));
      }

      WebContentsImpl* outer_contents = current_contents->GetOuterWebContents();
      if (outer_contents) {
        current.insert(outer_contents);
      }
    }
  }

  return result;
}

#if BUILDFLAG(IS_ANDROID)
float GetDeviceScaleAdjustment(int min_width) {
  static const float kMinFSM = 1.05f;
  static const int kWidthForMinFSM = 320;
  static const float kMaxFSM = 1.3f;
  static const int kWidthForMaxFSM = 800;

  if (min_width <= kWidthForMinFSM) {
    return kMinFSM;
  }
  if (min_width >= kWidthForMaxFSM) {
    return kMaxFSM;
  }

  // The font scale multiplier varies linearly between kMinFSM and kMaxFSM.
  float ratio = static_cast<float>(min_width - kWidthForMinFSM) /
                (kWidthForMaxFSM - kWidthForMinFSM);
  return ratio * (kMaxFSM - kMinFSM) + kMinFSM;
}
#endif

// Store a set of fullscreen WebContents and metadata for the browser context.
// Storing this information on the browser context is done for two reasons. One,
// related WebContentses must necessarily share a browser context, so this saves
// lookup time by restricting to one specific browser context. Two, separating
// by browser context is preemptive paranoia about keeping things separate.
class FullscreenUserData : public base::SupportsUserData::Data {
 public:
  FullscreenUserData() = default;
  ~FullscreenUserData() override = default;

  FullscreenUserData(const FullscreenUserData&) = delete;
  FullscreenUserData& operator=(const FullscreenUserData&) = delete;

  base::flat_set<raw_ptr<WebContentsImpl, CtnExperimental>>* set() {
    return &set_;
  }

  std::map<url::Origin, base::TimeTicks>* last_exits() { return &last_exits_; }

 private:
  base::flat_set<raw_ptr<WebContentsImpl, CtnExperimental>> set_;
  // Track latest exits by origin to briefly block re-entry without a gesture.
  std::map<url::Origin, base::TimeTicks> last_exits_;
};

const char kFullscreenUserData[] = "fullscreen-user-data";

FullscreenUserData* GetFullscreenUserData(BrowserContext* browser_context) {
  auto* set_holder = static_cast<FullscreenUserData*>(
      browser_context->GetUserData(kFullscreenUserData));
  if (!set_holder) {
    auto new_holder = std::make_unique<FullscreenUserData>();
    set_holder = new_holder.get();
    browser_context->SetUserData(kFullscreenUserData, std::move(new_holder));
  }
  return set_holder;
}

base::flat_set<raw_ptr<WebContentsImpl, CtnExperimental>>*
FullscreenContentsSet(BrowserContext* browser_context) {
  return GetFullscreenUserData(browser_context)->set();
}

// Returns true if `host` has the Window Management permission granted.
bool IsWindowManagementGranted(RenderFrameHost* host) {
  content::PermissionController* permission_controller =
      host->GetBrowserContext()->GetPermissionController();
  CHECK(permission_controller);

  return permission_controller->GetPermissionStatusForCurrentDocument(
             blink::PermissionType::WINDOW_MANAGEMENT, host) ==
         blink::mojom::PermissionStatus::GRANTED;
}

// Returns true if `host` has the Automatic Fullscreen permission granted.
bool IsAutomaticFullscreenGranted(RenderFrameHost* host) {
  content::PermissionController* permission_controller =
      host->GetBrowserContext()->GetPermissionController();
  CHECK(permission_controller);

  return permission_controller->GetPermissionStatusForCurrentDocument(
             blink::PermissionType::AUTOMATIC_FULLSCREEN, host) ==
         blink::mojom::PermissionStatus::GRANTED;
}

// Adjust the requested `rect` for opening or placing a window and return the id
// of the display where the window will be placed. The bounds may not extend
// outside a single screen's work area, and the `host` requires permission to
// specify bounds on a screen other than its current screen.
// TODO(crbug.com/40092782): These adjustments are inaccurate for window.open(),
// which specifies the inner content size, and for window.moveTo, resizeTo, etc.
// calls on newly created windows, which may pass empty sizes or positions to
// indicate uninitialized placement information in the renderer. Constraints
// enforced later should resolve most inaccuracies, but this early enforcement
// is needed to ensure bounds indicate the appropriate display.
int64_t AdjustWindowRectForDisplay(gfx::Rect* rect, RenderFrameHost* host) {
  auto* screen = display::Screen::GetScreen();
  auto display = screen->GetDisplayMatching(*rect);

  // Check, but do not prompt, for permission to place windows on other screens.
  // Sites generally need permission to get such bounds in the first place.
  // Also clamp offscreen bounds to the window's current screen.
  if (!rect->Intersects(display.bounds()) || !IsWindowManagementGranted(host)) {
    // Use the main frame's NativeView; cross-origin iframes yield null.
    gfx::NativeView view = host->GetOutermostMainFrame()->GetNativeView();
    display = screen->GetDisplayNearestView(view);
  }
  rect->AdjustToFit(display.work_area());
  return display.id();
}

// Adjusts the bounds to the minimum window size provided. Defaults to
// `blink::kMinimumWindowSize` but can be overridden, e.g. for borderless apps.
void AdjustWindowRectForMinimum(gfx::Rect* bounds,
                                int minimum_size = blink::kMinimumWindowSize) {
  // Size 0 indicates default size, not minimum.
  if (bounds->width()) {
    bounds->set_width(std::max(minimum_size, bounds->width()));
  }
  if (bounds->height()) {
    bounds->set_height(std::max(minimum_size, bounds->height()));
  }
}

// A ColorProviderSource used when one has not been explicitly set. This source
// only reflects theme information present in the web NativeTheme singleton.
// Keep this an implementation detail of WebContentsImpl as we should not be
// exposing a default ColorProviderSource generally.
class DefaultColorProviderSource : public ui::ColorProviderSource,
                                   public ui::NativeThemeObserver {
 public:
  DefaultColorProviderSource() {
    native_theme_observation_.Observe(ui::NativeTheme::GetInstanceForWeb());
  }
  DefaultColorProviderSource(const DefaultColorProviderSource&) = delete;
  DefaultColorProviderSource& operator=(const DefaultColorProviderSource&) =
      delete;
  ~DefaultColorProviderSource() override = default;

  static DefaultColorProviderSource* GetInstance() {
    static base::NoDestructor<DefaultColorProviderSource> instance;
    return instance.get();
  }

  // ui::ColorProviderSource:
  const ui::ColorProvider* GetColorProvider() const override {
    return ui::ColorProviderManager::Get().GetColorProviderFor(
        GetColorProviderKey());
  }

  ui::RendererColorMap GetRendererColorMap(
      ui::ColorProviderKey::ColorMode color_mode,
      ui::ColorProviderKey::ForcedColors forced_colors) const override {
    auto key = GetColorProviderKey();
    key.color_mode = color_mode;
    key.forced_colors = forced_colors;
    ui::ColorProvider* color_provider =
        ui::ColorProviderManager::Get().GetColorProviderFor(key);
    CHECK(color_provider);

    return ui::CreateRendererColorMap(*color_provider);
  }

  // ui::NativeThemeObserver:
  void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) override {
    DCHECK(native_theme_observation_.IsObservingSource(observed_theme));
    NotifyColorProviderChanged();
  }

 protected:
  // ui::ColorProviderSource:
  ui::ColorProviderKey GetColorProviderKey() const override {
    return ui::NativeTheme::GetInstanceForWeb()->GetColorProviderKey(nullptr);
  }

 private:
  base::ScopedObservation<ui::NativeTheme, ui::NativeThemeObserver>
      native_theme_observation_{this};
};

size_t GetFrameTreeSize(FrameTree* frame_tree) {
  size_t tree_size = 0;
  FrameTree::NodeRange node_range = frame_tree->NodesIncludingInnerTreeNodes();
  FrameTree::NodeIterator node_iter = node_range.begin();
  while (node_iter != node_range.end()) {
    // Skip over collapsed frame trees.
    if ((*node_iter)->is_collapsed()) {
      node_iter.AdvanceSkippingChildren();
    } else {
      ++tree_size;
      ++node_iter;
    }
  }
  return tree_size;
}

using RenderWidgetHostAtPointCallback =
    base::OnceCallback<void(base::WeakPtr<RenderWidgetHostViewBase>,
                            std::optional<gfx::PointF>)>;

void RunCallback(RenderWidgetHostAtPointCallback callback,
                 base::WeakPtr<input::RenderWidgetHostViewInput> view,
                 std::optional<gfx::PointF> point) {
  auto* target = static_cast<RenderWidgetHostViewBase*>(view.get());
  if (!callback.is_null()) {
    std::move(callback).Run(target ? target->GetWeakPtr() : nullptr, point);
  }
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class RenderProcessHostPriority {
  kBestEffort = 0,
  kUserVisible = 1,
  kUserBlocking = 2,
  kMissingRenderProcessHost = 3,
  kMaxValue = kMissingRenderProcessHost,
};

RenderProcessHostPriority GetRenderProcessHostPriority(
    RenderProcessHost* render_process_host) {
  if (!render_process_host) {
    return RenderProcessHostPriority::kMissingRenderProcessHost;
  }
  switch (render_process_host->GetPriority()) {
    case base::Process::Priority::kBestEffort:
      return RenderProcessHostPriority::kBestEffort;
    case base::Process::Priority::kUserVisible:
      return RenderProcessHostPriority::kUserVisible;
    case base::Process::Priority::kUserBlocking:
      return RenderProcessHostPriority::kUserBlocking;
  }
  NOTREACHED();
}

void RecordRendererUnresponsiveMetrics(
    bool web_contents_visible,
    RenderWidgetHostImpl* render_widget_host) {
  base::UmaHistogramBoolean("Renderer.Unresponsive.Visibility",
                            web_contents_visible);

  if (!web_contents_visible) {
    return;
  }

  RenderProcessHostPriority rph_priority =
      GetRenderProcessHostPriority(render_widget_host->GetProcess());
  base::UmaHistogramEnumeration(
      "Renderer.Unresponsive.PageVisible.RenderProcessHostPriority",
      rph_priority);

  bool widget_visible = !render_widget_host->is_hidden();
  base::UmaHistogramBoolean(
      "Renderer.Unresponsive.PageVisible.WidgetVisibility", widget_visible);

  if (!widget_visible) {
    return;
  }

  base::UmaHistogramEnumeration(
      "Renderer.Unresponsive.WidgetVisible.RenderProcessHostPriority",
      rph_priority);
}

// Returns a GroupingID used by VizCompositor to allow grouping
// CompositorFrameSinks from same WebContents.
uint32_t NextCompositorFrameSinkGroupingId() {
  static uint32_t grouping_id = 0;
  return grouping_id++;
}

}  // namespace

// This is a small helper class created while a JavaScript dialog is showing
// and destroyed when it's dismissed. Clients can register callbacks to receive
// a notification when the dialog is dismissed.
class JavaScriptDialogDismissNotifier {
 public:
  JavaScriptDialogDismissNotifier() = default;

  JavaScriptDialogDismissNotifier(const JavaScriptDialogDismissNotifier&) =
      delete;
  JavaScriptDialogDismissNotifier& operator=(
      const JavaScriptDialogDismissNotifier&) = delete;

  ~JavaScriptDialogDismissNotifier() {
    for (auto& callback : callbacks_) {
      std::move(callback).Run();
    }
  }

  void NotifyOnDismiss(base::OnceClosure callback) {
    callbacks_.push_back(std::move(callback));
  }

 private:
  std::vector<base::OnceClosure> callbacks_;
};

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
  if (opener_rfh) {
    opener = opener_rfh->frame_tree_node();
  }
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

  new_contents->Init(params, blink::FramePolicy());
  if (outer_web_contents) {
    outer_web_contents->InnerWebContentsCreated(new_contents.get());
  }
  return new_contents;
}

base::CallbackListSubscription
WebContentsImpl::FriendWrapper::AddCreatedCallbackForTesting(
    const CreatedCallback& callback) {
  return g_created_callbacks.Get().Add(callback);
}

WebContents* WebContents::FromRenderViewHost(RenderViewHost* rvh) {
  OPTIONAL_TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("content.verbose"),
                        "WebContents::FromRenderViewHost", "render_view_host",
                        rvh);
  if (!rvh) {
    return nullptr;
  }
  return static_cast<WebContentsImpl*>(
      static_cast<RenderViewHostImpl*>(rvh)->GetDelegate());
}

WebContents* WebContents::FromRenderFrameHost(RenderFrameHost* rfh) {
  OPTIONAL_TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("content.verbose"),
                        "WebContents::FromRenderFrameHost", "render_frame_host",
                        rfh);
  return WebContentsImpl::FromRenderFrameHostImpl(
      static_cast<RenderFrameHostImpl*>(rfh));
}

WebContentsImpl* WebContentsImpl::FromRenderFrameHostImpl(
    RenderFrameHostImpl* rfh) {
  OPTIONAL_TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("content.verbose"),
                        "WebContentsImpl::FromRenderFrameHostImpl",
                        "render_frame_host", rfh);
  if (!rfh) {
    return nullptr;
  }
  return static_cast<WebContentsImpl*>(rfh->delegate());
}

WebContents* WebContents::FromFrameTreeNodeId(
    FrameTreeNodeId frame_tree_node_id) {
  OPTIONAL_TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("content.verbose"),
                        "WebContents::FromFrameTreeNodeId",
                        "frame_tree_node_id", frame_tree_node_id);
  FrameTreeNode* frame_tree_node =
      FrameTreeNode::GloballyFindByID(frame_tree_node_id);
  if (!frame_tree_node) {
    return nullptr;
  }
  return WebContentsImpl::FromFrameTreeNode(frame_tree_node);
}

WebContentsImpl* WebContentsImpl::FromRenderWidgetHostImpl(
    RenderWidgetHostImpl* rwh) {
  if (!rwh) {
    return nullptr;
  }
  return static_cast<WebContentsImpl*>(rwh->delegate());
}

bool WebContentsImpl::IsPopup() const {
  return is_popup_;
}

bool WebContentsImpl::IsPartitionedPopin() const {
  // The feature must be enabled if a popin was opened.
  DCHECK(base::FeatureList::IsEnabled(blink::features::kPartitionedPopins) ||
         !partitioned_popin_opener_);

  return !!partitioned_popin_opener_;
}

RenderFrameHostImpl* WebContentsImpl::PartitionedPopinOpener() const {
  // A popin cannot open a popin so at most one could be set at a time.
  DCHECK(!partitioned_popin_opener_ || !opened_partitioned_popin_);

  // The feature must be enabled if the popin opener is set.
  DCHECK(base::FeatureList::IsEnabled(blink::features::kPartitionedPopins) ||
         !partitioned_popin_opener_);

  return partitioned_popin_opener_.get();
}

WebContents* WebContentsImpl::OpenedPartitionedPopin() const {
  // A popin cannot open a popin so at most one could be set at a time.
  DCHECK(!partitioned_popin_opener_ || !opened_partitioned_popin_);

  // The feature must be enabled if a popin was opened.
  DCHECK(base::FeatureList::IsEnabled(blink::features::kPartitionedPopins) ||
         !opened_partitioned_popin_);

  return opened_partitioned_popin_.get();
}

void WebContents::SetScreenOrientationDelegate(
    ScreenOrientationDelegate* delegate) {
  ScreenOrientationProvider::SetDelegate(delegate);
}

// WebContentsImpl::RenderWidgetHostDestructionObserver -----------------------

class WebContentsImpl::RenderWidgetHostDestructionObserver
    : public RenderWidgetHostObserver {
 public:
  RenderWidgetHostDestructionObserver(WebContentsImpl* owner,
                                      RenderWidgetHost* watched_host)
      : owner_(owner), watched_host_(watched_host) {
    watched_host_->AddObserver(this);
  }

  RenderWidgetHostDestructionObserver(
      const RenderWidgetHostDestructionObserver&) = delete;
  RenderWidgetHostDestructionObserver& operator=(
      const RenderWidgetHostDestructionObserver&) = delete;

  ~RenderWidgetHostDestructionObserver() override {
    watched_host_->RemoveObserver(this);
  }

  // RenderWidgetHostObserver:
  void RenderWidgetHostDestroyed(RenderWidgetHost* widget_host) override {
    owner_->OnRenderWidgetHostDestroyed(widget_host);
  }

 private:
  raw_ptr<WebContentsImpl> owner_;
  raw_ptr<RenderWidgetHost> watched_host_;
};

// WebContentsImpl::WebContentsDestructionObserver ----------------------------

class WebContentsImpl::WebContentsDestructionObserver
    : public WebContentsObserver {
 public:
  WebContentsDestructionObserver(WebContentsImpl* owner,
                                 WebContents* watched_contents)
      : WebContentsObserver(watched_contents), owner_(owner) {}

  WebContentsDestructionObserver(const WebContentsDestructionObserver&) =
      delete;
  WebContentsDestructionObserver& operator=(
      const WebContentsDestructionObserver&) = delete;

  // WebContentsObserver:
  void WebContentsDestroyed() override {
    owner_->OnWebContentsDestroyed(
        static_cast<WebContentsImpl*>(web_contents()));
  }

 private:
  raw_ptr<WebContentsImpl> owner_;
};

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
// TODO(sreejakshetty): Make |WebContentsImpl::ColorChooserHolder| per-frame
// instead of WebContents-owned.
// WebContentsImpl::ColorChooserHolder -----------------------------------------
class WebContentsImpl::ColorChooserHolder : public blink::mojom::ColorChooser {
 public:
  ColorChooserHolder(
      mojo::PendingReceiver<blink::mojom::ColorChooser> receiver,
      mojo::PendingRemote<blink::mojom::ColorChooserClient> client)
      : receiver_(this, std::move(receiver)), client_(std::move(client)) {}

  ~ColorChooserHolder() override {
    if (chooser_) {
      chooser_->End();
    }
  }

  void SetChooser(std::unique_ptr<content::ColorChooser> chooser) {
    chooser_ = std::move(chooser);
    if (chooser_) {
      receiver_.set_disconnect_handler(
          base::BindOnce([](content::ColorChooser* chooser) { chooser->End(); },
                         base::Unretained(chooser_.get())));
    }
  }

  void SetSelectedColor(SkColor color) override {
    OPTIONAL_TRACE_EVENT0(
        "content", "WebContentsImpl::ColorChooserHolder::SetSelectedColor");
    if (chooser_) {
      chooser_->SetSelectedColor(color);
    }
  }

  void DidChooseColorInColorChooser(SkColor color) {
    OPTIONAL_TRACE_EVENT0(
        "content",
        "WebContentsImpl::ColorChooserHolder::DidChooseColorInColorChooser");
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
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

// WebContentsImpl::WebContentsTreeNode ----------------------------------------
WebContentsImpl::WebContentsTreeNode::WebContentsTreeNode(
    WebContentsImpl* current_web_contents)
    : current_web_contents_(current_web_contents),
      outer_web_contents_(nullptr) {}

WebContentsImpl::WebContentsTreeNode::~WebContentsTreeNode() = default;

void WebContentsImpl::WebContentsTreeNode::AttachInnerWebContents(
    std::unique_ptr<WebContents> inner_web_contents,
    RenderFrameHostImpl* render_frame_host) {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsTreeNode::AttachInnerWebContents");
  WebContentsImpl* inner_web_contents_impl =
      static_cast<WebContentsImpl*>(inner_web_contents.get());
  WebContentsTreeNode& inner_web_contents_node = inner_web_contents_impl->node_;

  inner_web_contents_node.outer_web_contents_ = current_web_contents_;
  inner_web_contents_node.outer_contents_frame_tree_node_id_ =
      render_frame_host->frame_tree_node()->frame_tree_node_id();

  if (inner_web_contents) {
    inner_web_contents->SetOwnerLocationForDebug(FROM_HERE);
  }
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
      if (detached_contents) {
        detached_contents->SetOwnerLocationForDebug(std::nullopt);
      }
      return detached_contents;
    }
  }

  NOTREACHED_IN_MIGRATION();
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

  node->RemoveObserver(this);
  // Deletes |this| too.
  outer_web_contents_->node_.DetachInnerWebContents(current_web_contents_);
}

FrameTree* WebContentsImpl::WebContentsTreeNode::focused_frame_tree() {
  CHECK(focused_frame_tree_);
  return focused_frame_tree_;
}

void WebContentsImpl::WebContentsTreeNode::SetFocusedFrameTree(
    FrameTree* frame_tree) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsTreeNode::SetFocusedFrameTree");
  DCHECK(!outer_web_contents())
      << "Only the outermost WebContents tracks focus.";
  focused_frame_tree_ = frame_tree;
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
  for (auto& contents : inner_web_contents_) {
    inner_web_contents.push_back(static_cast<WebContentsImpl*>(contents.get()));
  }

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

namespace {

// A helper for ensuring that WebContents are closed (or otherwise destroyed)
// *before* their BrowserContext is destroyed.
class WebContentsOfBrowserContext : public base::SupportsUserData::Data {
 public:
  static void Attach(WebContentsImpl& web_contents) {
    WebContentsOfBrowserContext* self =
        GetOrCreate(*web_contents.GetBrowserContext());
    self->web_contents_set_.insert(&web_contents);
  }

  static void Detach(WebContentsImpl& web_contents) {
    WebContentsOfBrowserContext* self =
        GetOrCreate(*web_contents.GetBrowserContext());
    self->web_contents_set_.erase(&web_contents);
  }

  ~WebContentsOfBrowserContext() override {
    // The ~WebContentsOfBrowserContext destructor is called when the
    // BrowserContext (and its UserData) gets destroyed.  At this point
    // in time all the WebContents of this BrowserContext should have been
    // already closed (i.e. we expect the `web_contents_set_` set to be empty at
    // this point - emptied by calls to the `Detach` method above).
    if (web_contents_set_.empty()) {
      return;  // Everything is okay - nothing to warn about.
    }

#if BUILDFLAG(IS_ANDROID)
    JNIEnv* env = base::android::AttachCurrentThread();
#endif  // BUILDFLAG(IS_ANDROID)

    // Any remaining WebContents contain dangling pointers to the
    // BrowserContext being destroyed.  Such WebContents (and their
    // RenderFrameHosts, SiteInstances, etc.) risk causing
    // use-after-free bugs.  For more discussion about managing the
    // lifetime of WebContents please see https://crbug.com/1376879#c44.
    for (WebContentsImpl* web_contents_with_dangling_ptr_to_browser_context :
         web_contents_set_) {
      std::string creator = web_contents_with_dangling_ptr_to_browser_context
                                ->GetCreatorLocation()
                                .ToString();
      SCOPED_CRASH_KEY_STRING256("shutdown", "web_contents/creator", creator);

      const std::optional<base::Location>& ownership_location =
          web_contents_with_dangling_ptr_to_browser_context
              ->ownership_location();
      std::string owner;
      if (ownership_location) {
        if (ownership_location->has_source_info()) {
          owner = std::string(ownership_location->function_name()) + "@" +
                  ownership_location->file_name();
        } else {
          owner = "no_source_info";
        }
      } else {
        owner = "unknown";
      }
      SCOPED_CRASH_KEY_STRING256("shutdown", "web_contents/owner", owner);

#if BUILDFLAG(IS_ANDROID)
      // On Android, also report the Java stack trace from WebContents's
      // creation.
      WebContentsAndroid::ReportDanglingPtrToBrowserContext(
          env, web_contents_with_dangling_ptr_to_browser_context);
#endif  // BUILDFLAG(IS_ANDROID)

      if (base::FeatureList::IsEnabled(kCrashOnDanglingBrowserContext)) {
        LOG(FATAL)
            << "BrowserContext is getting destroyed without first closing all "
            << "WebContents (for more info see https://crbug.com/1376879#c44); "
            << "creator = " << creator;
      } else {
        NOTREACHED_IN_MIGRATION()
            << "BrowserContext is getting destroyed without first closing all "
            << "WebContents (for more info see https://crbug.com/1376879#c44); "
            << "creator = " << creator;
        base::debug::DumpWithoutCrashing();
      }
    }
  }

  std::unique_ptr<Data> Clone() override {
    // Indicate to `SupportsUserData` / `BrowserContext` that cloning is not
    // supported.
    return nullptr;
  }

 private:
  WebContentsOfBrowserContext() = default;

  // Gets WebContentsOfBrowserContext associated with the given
  // `browser_context` (creating a new WebContentsOfBrowserContext if
  // necessary - if one hasn't been created yet).
  static WebContentsOfBrowserContext* GetOrCreate(
      BrowserContext& browser_context) {
    static const char* kUserDataKey = "WebContentsOfBrowserContext/UserDataKey";
    WebContentsOfBrowserContext* result =
        static_cast<WebContentsOfBrowserContext*>(
            browser_context.GetUserData(kUserDataKey));
    if (!result) {
      result = new WebContentsOfBrowserContext();
      browser_context.SetUserData(kUserDataKey, base::WrapUnique(result));
    }
    return result;
  }

  // Set of all `WebContents` within the tracked `BrowserContext`.
  //
  // Usage of `raw_ptr` below is okay (i.e. it shouldn't dangle), because
  // when `WebContentsImpl`'s destructor runs, then it removes the set entry
  // (by calling `Detach`).
  std::set<raw_ptr<WebContentsImpl>> web_contents_set_;
};

}  // namespace

WebContentsImpl::WebContentsImpl(BrowserContext* browser_context)
    : ColorProviderSourceObserver(DefaultColorProviderSource::GetInstance()),
      delegate_(nullptr),
      render_view_host_delegate_view_(nullptr),
      opened_by_another_window_(false),
      node_(this),
      primary_frame_tree_(browser_context,
                          this,
                          this,
                          this,
                          this,
                          this,
                          this,
                          this,
                          this,
                          FrameTree::Type::kPrimary),
      primary_main_frame_process_status_(
          base::TERMINATION_STATUS_STILL_RUNNING),
      primary_main_frame_process_error_code_(0),
      load_state_(net::LOAD_STATE_IDLE, std::u16string()),
      upload_size_(0),
      upload_position_(0),
      is_resume_pending_(false),
      notify_disconnection_(false),
      dialog_manager_(nullptr),
      is_showing_before_unload_dialog_(false),
      last_active_time_ticks_(base::TimeTicks::Now()),
      last_active_time_(base::Time::Now()),
      closed_by_user_gesture_(false),
      minimum_zoom_percent_(
          static_cast<int>(blink::kMinimumBrowserZoomFactor * 100)),
      maximum_zoom_percent_(
          static_cast<int>(blink::kMaximumBrowserZoomFactor * 100)),
      zoom_scroll_remainder_(0),
      force_disable_overscroll_content_(false),
      last_dialog_suppressed_(false),
      accessibility_mode_(
          BrowserAccessibilityState::GetInstance()
              ->GetAccessibilityModeForBrowserContext(browser_context)),
      audio_stream_monitor_(this),
      media_web_contents_observer_(
          std::make_unique<MediaWebContentsObserver>(this)),
      is_overlay_content_(false),
      showing_context_menu_(false),
      prerender_host_registry_(std::make_unique<PrerenderHostRegistry>(*this)),
      compositor_frame_sink_grouping_id_(NextCompositorFrameSinkGroupingId()) {
  TRACE_EVENT0("content", "WebContentsImpl::WebContentsImpl");
  WebContentsOfBrowserContext::Attach(*this);
  node_.SetFocusedFrameTree(&primary_frame_tree_);
#if BUILDFLAG(ENABLE_PPAPI)
  pepper_playback_observer_ = std::make_unique<PepperPlaybackObserver>(this);
#endif

#if BUILDFLAG(IS_ANDROID)
  safe_area_insets_host_ = SafeAreaInsetsHost::Create(this);
#endif

  ui::NativeTheme* native_theme = ui::NativeTheme::GetInstanceForWeb();
  native_theme_observation_.Observe(native_theme);
  slow_web_preference_cache_observation_.Observe(
      SlowWebPreferenceCache::GetInstance());
  using_dark_colors_ = native_theme->ShouldUseDarkColors();
  in_forced_colors_ = native_theme->InForcedColorsMode();
  preferred_color_scheme_ = native_theme->GetPreferredColorScheme();
  preferred_contrast_ = native_theme->GetPreferredContrast();
  prefers_reduced_transparency_ = native_theme->GetPrefersReducedTransparency();
  inverted_colors_ = native_theme->GetInvertedColors();
  renderer_preferences_.caret_blink_interval =
      native_theme->GetCaretBlinkInterval();

  screen_change_monitor_ =
      std::make_unique<ScreenChangeMonitor>(base::BindRepeating(
          &WebContentsImpl::OnScreensChange, base::Unretained(this)));

  if (base::FeatureList::IsEnabled(blink::features::kSharedStorageAPI)) {
    SharedStorageBudgetCharger::CreateForWebContents(this);
  }
}

WebContentsImpl::~WebContentsImpl() {
  TRACE_EVENT0("content", "WebContentsImpl::~WebContentsImpl");
  WebContentsOfBrowserContext::Detach(*this);

  // Imperfect sanity check against double free, given some crashes unexpectedly
  // observed in the wild.
  CHECK(!IsBeingDestroyed());

  // We generally keep track of is_being_destroyed_ to let other features know
  // to avoid certain actions during destruction.
  is_being_destroyed_ = true;

  // A WebContents should never be deleted while it is notifying observers,
  // since this will lead to a use-after-free as it continues to notify later
  // observers.
  CHECK(!observers_.is_notifying_observers());
  // This is used to watch for one-off observers (i.e. non-WebContentsObservers)
  // destroying WebContents, which they should not do.
  CHECK(!prevent_destruction_);

  // We usually record `max_loaded_frame_count_` in `DidFinishNavigation()`
  // for pages the user navigated away from other than the last one. We record
  // it for the last page here.
  if (first_primary_navigation_completed_) {
    RecordMaxFrameCountUMA(max_loaded_frame_count_);
  }

  FullscreenContentsSet(GetBrowserContext())->erase(this);

  touch_emulator_.reset();

  rwh_input_event_router_.reset();

  WebContentsImpl* outermost = GetOutermostWebContents();
  if (this != outermost && ContainsOrIsFocusedWebContents()) {
    // If the current WebContents is in focus, unset it.
    outermost->SetAsFocusedWebContentsIfNecessary();
  }

  if (pointer_lock_widget_) {
    pointer_lock_widget_->RejectPointerLockOrUnlockIfNecessary(
        blink::mojom::PointerLockResult::kElementDestroyed);

    // Normally, the call above clears mouse_lock_widget_ pointers on the
    // entire WebContents chain, since it results in calling LostPointerLock()
    // when the mouse lock is already active. However, this doesn't work for
    // <webview> guests if the mouse lock request is still pending while the
    // <webview> is destroyed. Hence, ensure that all mouse lock widget
    // pointers are cleared. See https://crbug.com/1346245.
    for (WebContentsImpl* current = this; current;
         current = current->GetOuterWebContents()) {
      current->pointer_lock_widget_ = nullptr;
    }
  }

  for (RenderWidgetHostImpl* widget : created_widgets_) {
    widget->DetachDelegate();
  }
  created_widgets_.clear();

  // Clear out any JavaScript state.
  if (dialog_manager_) {
    dialog_manager_->CancelDialogs(this, /*reset_state=*/true);
  }

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  color_chooser_holder_.reset();
#endif
  find_request_manager_.reset();

  // Shutdown the primary FrameTree.
  primary_frame_tree_.Shutdown();

  // Shutdown the non-primary FrameTrees.
  //
  // Do this here rather than relying on the owner of the FrameTree to shutdown
  // on WebContentsDestroyed(), so that all the FrameTrees are shutdown at the
  // same time for consistency. Also, destroying a FrameTree results in other
  // observer functions like RenderFrameDeleted() being called, which are not
  // expected to be called after WebContentsDestroyed().
  //
  // Currently the only instances of the non-primary FrameTrees are for
  // prerendering. Shutdown them by destructing PrerenderHostRegistry.
  prerender_host_registry_.reset();

  // For historical reasons, it is the requestor's responsibility to reset
  // `PrefetchContainer`s that were created by the requestor (= `this`) but have
  // not yet started prefetching (Otherwise, they will stay alive forever in
  // `PrefetchService`).
  // TODO(crbug.com/40946257): Refactor to handle this case better.
  if (base::FeatureList::IsEnabled(
          features::kPrefetchBrowserInitiatedTriggers)) {
    PrefetchService* prefetch_service =
        BrowserContextImpl::From(GetBrowserContext())->GetPrefetchService();
    if (prefetch_service) {
      for (const auto& prefetch_container : prefetch_containers_) {
        if (prefetch_container) {
          switch (prefetch_container->GetLoadState()) {
            case PrefetchContainer::LoadState::kNotStarted:
            case PrefetchContainer::LoadState::kEligible:
            case PrefetchContainer::LoadState::kFailedIneligible:
            case PrefetchContainer::LoadState::kFailedHeldback:
              prefetch_service->ResetPrefetch(prefetch_container);
              break;
            case PrefetchContainer::LoadState::kStarted:
              break;
          }
        }
      }
    }
    prefetch_containers_.clear();
  }

#if BUILDFLAG(ENABLE_PPAPI)
  // Call this before WebContentsDestroyed() is broadcasted since
  // AudioFocusManager will be destroyed after that.
  pepper_playback_observer_.reset();
#endif  // defined(ENABLED_PLUGINS)

  // If audio is playing then notify external observers of the audio stream
  // disappearing.
  if (is_currently_audible_) {
    is_currently_audible_ = false;
    observers_.NotifyObservers(&WebContentsObserver::OnAudioStateChanged,
                               false);
    if (GetOuterWebContents()) {
      GetOuterWebContents()->OnAudioStateChanged();
    }
  }

  // |save_package_| is refcounted so make sure we clear the page before
  // we toss out our reference.
  if (save_package_) {
    save_package_->ClearPage();
  }

  observers_.NotifyObservers(&WebContentsObserver::WebContentsDestroyed);

#if BUILDFLAG(IS_ANDROID)
  // Destroy the WebContentsAndroid here, so that its observers still can access
  // `this`.
  ClearWebContentsAndroid();
#endif

  observers_.NotifyObservers(&WebContentsObserver::ResetWebContents);
  SetDelegate(nullptr);
}

std::unique_ptr<WebContentsImpl> WebContentsImpl::CreateWithOpener(
    const WebContents::CreateParams& params,
    RenderFrameHostImpl* opener_rfh) {
  OPTIONAL_TRACE_EVENT1("browser", "WebContentsImpl::CreateWithOpener",
                        "opener", opener_rfh);
  FrameTreeNode* opener = nullptr;
  if (opener_rfh) {
    opener = opener_rfh->frame_tree_node();
  }
  std::unique_ptr<WebContentsImpl> new_contents(
      new WebContentsImpl(params.browser_context));
  new_contents->SetOpenerForNewContents(opener, params.opener_suppressed);

  // If the opener is sandboxed, a new popup must inherit the opener's sandbox
  // flags, and these flags take effect immediately.  An exception is if the
  // opener's sandbox flags lack the PropagatesToAuxiliaryBrowsingContexts
  // bit (which is controlled by the "allow-popups-to-escape-sandbox" token).
  // See https://html.spec.whatwg.org/C/#attr-iframe-sandbox.
  FrameTreeNode* new_root = new_contents->GetPrimaryFrameTree().root();
  if (opener) {
    network::mojom::WebSandboxFlags opener_flags =
        opener_rfh->active_sandbox_flags();
    if (opener_rfh->IsSandboxed(network::mojom::WebSandboxFlags::
                                    kPropagatesToAuxiliaryBrowsingContexts)) {
      new_root->SetPendingFramePolicy({opener_flags,
                                       {} /* container_policy */,
                                       {} /* required_document_policy */});
    }
    new_root->SetInitialPopupURL(params.initial_popup_url);
    new_root->SetPopupCreatorOrigin(opener_rfh->GetLastCommittedOrigin());
  }

  // Apply starting sandbox flags.
  blink::FramePolicy frame_policy(new_root->pending_frame_policy());
  frame_policy.sandbox_flags |= params.starting_sandbox_flags;
  new_root->SetPendingFramePolicy(frame_policy);

  // This may be true even when opener is null, such as when opening blocked
  // popups.
  if (params.opened_by_another_window) {
    new_contents->opened_by_another_window_ = true;
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

  // Multi-network CCT relies on the following invariant: WebContents associated
  // with a CCT tab targeting a network will always have
  // WebContents::GetTargetNetwork == that target network. We MUST set the
  // target_network before WebContents initialization. Otherwise, the renderer,
  // during WebContents initialization, might create (and keep around)
  // URLLoaderFactories that won't load resources from the target network. For
  // WebContents created from Java (e.g., WebContents rendering a CCT), this is
  // guaranteed by the Java side setting
  // WebContents::CreateParams::target_network_ when that CCT is targeting a
  // network.
  new_contents->target_network_ = params.target_network;
  if (new_contents->target_network_ == net::handles::kInvalidNetworkHandle) {
    // For WebContents opened by another WebContents (e.g. through
    // window.open(), instead of from the UI / by the embedder),
    // WebContents::CreateParams::target_network_ won't be set, as the calling
    // code is not aware of multi-network CCT. Instead, to handle that, we pass
    // along the target network (if present) of the WebContents associated with
    // the opener frame.
    auto* web_contents = WebContentsImpl::FromRenderFrameHostImpl(opener_rfh);
    if (web_contents) {
      new_contents->target_network_ = web_contents->GetTargetNetwork();
    }
  }

  new_contents->Init(params, frame_policy);
  if (outer_web_contents) {
    outer_web_contents->InnerWebContentsCreated(new_contents.get());
  }
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
    if (!rvh) {
      continue;
    }
    WebContents* web_contents = WebContents::FromRenderViewHost(rvh);
    if (!web_contents) {
      continue;
    }
    if (web_contents->GetPrimaryMainFrame()->GetRenderViewHost() != rvh) {
      continue;
    }
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
                        static_cast<const void*>(frame_tree_node));
  return static_cast<WebContentsImpl*>(
      WebContents::FromRenderFrameHost(frame_tree_node->current_frame_host()));
}

// static
WebContents* WebContentsImpl::FromRenderFrameHostID(
    GlobalRenderFrameHostId render_frame_host_id) {
  OPTIONAL_TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("content.verbose"),
                        "WebContentsImpl::FromRenderFrameHostID", "process_id",
                        render_frame_host_id.child_id, "frame_id",
                        render_frame_host_id.frame_routing_id);
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         !BrowserThread::IsThreadInitialized(BrowserThread::UI));
  RenderFrameHost* render_frame_host =
      RenderFrameHost::FromID(render_frame_host_id);
  if (!render_frame_host) {
    return nullptr;
  }

  return WebContents::FromRenderFrameHost(render_frame_host);
}

// static
WebContents* WebContentsImpl::FromRenderFrameHostID(int render_process_host_id,
                                                    int render_frame_host_id) {
  return FromRenderFrameHostID(
      GlobalRenderFrameHostId(render_process_host_id, render_frame_host_id));
}

// static
WebContentsImpl* WebContentsImpl::FromOuterFrameTreeNode(
    const FrameTreeNode* frame_tree_node) {
  OPTIONAL_TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("content.verbose"),
                        "WebContentsImpl::FromOuterFrameTreeNode",
                        "frame_tree_node",
                        static_cast<const void*>(frame_tree_node));
  return WebContentsImpl::FromFrameTreeNode(frame_tree_node)
      ->node_.GetInnerWebContentsInFrame(frame_tree_node);
}

bool WebContentsImpl::OnMessageReceived(RenderFrameHostImpl* render_frame_host,
                                        const IPC::Message& message) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::OnMessageReceived",
                        "render_frame_host", render_frame_host);

  for (auto& observer : observers_.observer_list()) {
    if (observer.OnMessageReceived(message, render_frame_host)) {
      return true;
    }
  }

  return false;
}

std::string WebContentsImpl::GetTitleForMediaControls() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::GetTitleForMediaControls");

  if (!delegate_) {
    return std::string();
  }
  return delegate_->GetTitleForMediaControls(this);
}

// Returns the NavigationController for the primary FrameTree, i.e. the one
// whose URL is shown in the omnibox. With MPArch we can have multiple
// FrameTrees in one WebContents and each has its own NavigationController.
// TODO(crbug.com/40165692): Make sure callers are aware of this.
NavigationControllerImpl& WebContentsImpl::GetController() {
  return primary_frame_tree_.controller();
}

BrowserContext* WebContentsImpl::GetBrowserContext() {
  return GetController().GetBrowserContext();
}

base::WeakPtr<WebContents> WebContentsImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

const GURL& WebContentsImpl::GetURL() {
  return GetVisibleURL();
}

const GURL& WebContentsImpl::GetVisibleURL() {
  // We may not have a navigation entry yet.
  NavigationEntry* entry = GetController().GetVisibleEntry();
  return entry ? entry->GetVirtualURL() : GURL::EmptyGURL();
}

const GURL& WebContentsImpl::GetLastCommittedURL() {
  // We may not have a navigation entry yet.
  NavigationEntry* entry = GetController().GetLastCommittedEntry();
  return entry ? entry->GetVirtualURL() : GURL::EmptyGURL();
}

WebContentsDelegate* WebContentsImpl::GetDelegate() {
  return delegate_;
}

void WebContentsImpl::SetDelegate(WebContentsDelegate* delegate) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::SetDelegate", "delegate",
                        static_cast<void*>(delegate));
  // TODO(cbentzel): remove this debugging code?
  if (delegate == delegate_) {
    return;
  }
  const bool had_delegate = !!delegate_;
  if (had_delegate) {
    delegate_->Detach(this);
  }
  delegate_ = delegate;
  if (delegate_) {
    delegate_->Attach(this);
    // RenderFrameDevToolsAgentHost should not be told about the WebContents
    // until there is a `delegate_`.
    if (!had_delegate) {
      RenderFrameDevToolsAgentHost::AttachToWebContents(this);
    }
  }

  // Re-read values from the new delegate and apply them.
  if (view_) {
    view_->SetOverscrollControllerEnabled(CanOverscrollContent());
  }
}

const RenderFrameHostImpl* WebContentsImpl::GetPrimaryMainFrame() const {
  return primary_frame_tree_.root()->current_frame_host();
}

RenderFrameHostImpl* WebContentsImpl::GetPrimaryMainFrame() {
  return const_cast<RenderFrameHostImpl*>(
      std::as_const(*this).GetPrimaryMainFrame());
}

PageImpl& WebContentsImpl::GetPrimaryPage() {
  // We should not be accessing Page during the destruction of this WebContents,
  // as the Page has already been cleared.
  //
  // Please note that IsBeingDestroyed() should be checked to ensure that we
  // don't access Page related data that is going to be destroyed.
  CHECK(primary_frame_tree_.root()->current_frame_host());
  return primary_frame_tree_.root()->current_frame_host()->GetPage();
}

RenderFrameHostImpl* WebContentsImpl::GetFocusedFrame() {
  // If this method is called on an inner WebContents, don't return frames from
  // outside of the inner WebContents's subtree.
  if (GetOuterWebContents() && !ContainsOrIsFocusedWebContents()) {
    return nullptr;
  }

  FrameTreeNode* focused_node = GetFocusedFrameTree()->GetFocusedFrame();
  if (!focused_node) {
    return nullptr;
  }

  // If an inner frame tree has focus, we should return a RenderFrameHost from
  // the inner frame tree and not the placeholder RenderFrameHost.
  DCHECK(focused_node->current_frame_host()
             ->inner_tree_main_frame_tree_node_id()
             .is_null());

  return focused_node->current_frame_host();
}

bool WebContentsImpl::IsPrerenderedFrame(FrameTreeNodeId frame_tree_node_id) {
  if (frame_tree_node_id.is_null()) {
    return false;
  }

  FrameTreeNode* frame_tree_node =
      FrameTreeNode::GloballyFindByID(frame_tree_node_id);
  if (!frame_tree_node) {
    return false;
  }

  // In the case of inner frame trees in a prerender, the inner frame tree's
  // type, would not be FrameTree::Type::kPrerender. So if this is not the
  // outermost frame, we use the lifecycle state of the document that owns this.
  // TODO(1196715, 1232528): This relies on the LifecycleState being correct
  // in the case of inner frame trees.
  if (frame_tree_node->GetParentOrOuterDocumentOrEmbedder()) {
    return frame_tree_node->GetParentOrOuterDocumentOrEmbedder()
               ->lifecycle_state() ==
           RenderFrameHostImpl::LifecycleStateImpl::kPrerendering;
  }
  return frame_tree_node->GetFrameType() == FrameType::kPrerenderMainFrame;
}

RenderFrameHostImpl* WebContentsImpl::UnsafeFindFrameByFrameTreeNodeId(
    FrameTreeNodeId frame_tree_node_id) {
  OPTIONAL_TRACE_EVENT1("content",
                        "WebContentsImpl::UnsafeFindFrameByFrameTreeNodeId",
                        "frame_tree_node_id", frame_tree_node_id);
  // Beware using this! The RenderFrameHost may have changed since the caller
  // obtained frame_tree_node_id.
  FrameTreeNode* ftn = FrameTreeNode::GloballyFindByID(frame_tree_node_id);
  if (!ftn) {
    return nullptr;
  }
  if (this != WebContents::FromRenderFrameHost(ftn->current_frame_host())) {
    return nullptr;
  }
  return ftn->current_frame_host();
}

void WebContentsImpl::ForEachRenderFrameHostWithAction(
    base::FunctionRef<FrameIterationAction(RenderFrameHost*)> on_frame) {
  ForEachRenderFrameHostWithAction(
      [on_frame](RenderFrameHostImpl* rfh) { return on_frame(rfh); });
}

void WebContentsImpl::ForEachRenderFrameHost(
    base::FunctionRef<void(RenderFrameHost*)> on_frame) {
  ForEachRenderFrameHost(
      [on_frame](RenderFrameHostImpl* rfh) { on_frame(rfh); });
}

void WebContentsImpl::ForEachRenderFrameHostWithAction(
    base::FunctionRef<FrameIterationAction(RenderFrameHostImpl*)> on_frame) {
  ForEachRenderFrameHostImpl(on_frame, /* include_speculative */ false);
}

void WebContentsImpl::ForEachRenderFrameHost(
    base::FunctionRef<void(RenderFrameHostImpl*)> on_frame) {
  ForEachRenderFrameHostWithAction([on_frame](RenderFrameHostImpl* rfh) {
    on_frame(rfh);
    return FrameIterationAction::kContinue;
  });
}

void WebContentsImpl::ForEachRenderFrameHostIncludingSpeculativeWithAction(
    base::FunctionRef<FrameIterationAction(RenderFrameHostImpl*)> on_frame) {
  ForEachRenderFrameHostImpl(on_frame, /* include_speculative */ true);
}

void WebContentsImpl::ForEachRenderFrameHostIncludingSpeculative(
    base::FunctionRef<void(RenderFrameHostImpl*)> on_frame) {
  ForEachRenderFrameHostIncludingSpeculativeWithAction(
      [on_frame](RenderFrameHostImpl* rfh) {
        on_frame(rfh);
        return FrameIterationAction::kContinue;
      });
}

void WebContentsImpl::ForEachRenderFrameHostImpl(
    base::FunctionRef<FrameIterationAction(RenderFrameHostImpl*)> on_frame,
    bool include_speculative) {
  // Since |RenderFrameHostImpl::ForEachRenderFrameHost| will reach the
  // RenderFrameHosts descending from a specified root, it is enough to start
  // iteration from each of the outermost main frames to reach everything in
  // this WebContents. However, if iteration stops early in
  // |RenderFrameHostImpl::ForEachRenderFrameHost|, we also need to stop early
  // by not iterating over additional outermost main frames.
  bool iteration_stopped = false;
  auto on_frame_with_termination =
      [on_frame, &iteration_stopped](RenderFrameHostImpl* rfh) {
        const auto action = on_frame(rfh);
        if (action == FrameIterationAction::kStop) {
          iteration_stopped = true;
        }
        return action;
      };

  for (auto* rfh : GetOutermostMainFrames()) {
    if (include_speculative) {
      rfh->ForEachRenderFrameHostIncludingSpeculativeWithAction(
          on_frame_with_termination);
    } else {
      rfh->ForEachRenderFrameHostWithAction(on_frame_with_termination);
    }

    if (iteration_stopped) {
      return;
    }
  }
}

void WebContentsImpl::ForEachFrameTree(
    FrameTreeIterationCallback on_frame_tree) {
  // Consider all outermost frame trees, and for each, iterate through their
  // FrameTreeNodes to find any inner frame trees.
  for (FrameTree* outermost_frame_tree : GetOutermostFrameTrees()) {
    FrameTree::NodeRange node_range =
        outermost_frame_tree->NodesIncludingInnerTreeNodes();
    FrameTree::NodeIterator node_iter = node_range.begin();
    while (node_iter != node_range.end()) {
      FrameTreeNode* node = *node_iter;
      if (FromFrameTreeNode(node) != this) {
        // Exclude any inner frame trees based on multi-WebContents
        // architecture.
        node_iter.AdvanceSkippingChildren();
      } else {
        if (node->IsMainFrame()) {
          on_frame_tree.Run(node->frame_tree());
        }
        ++node_iter;
      }
    }
  }
}

std::vector<FrameTree*> WebContentsImpl::GetOutermostFrameTrees() {
  // If the WebContentsImpl is being destroyed, then we should not
  // perform the tree traversal.
  if (IsBeingDestroyed()) {
    return {};
  }

  std::vector<FrameTree*> result;
  result.push_back(&GetPrimaryFrameTree());

  const std::vector<FrameTree*> prerender_frame_trees =
      GetPrerenderHostRegistry()->GetPrerenderFrameTrees();
  result.insert(result.end(), prerender_frame_trees.begin(),
                prerender_frame_trees.end());

  return result;
}

std::vector<RenderFrameHostImpl*> WebContentsImpl::GetOutermostMainFrames() {
  // Do nothing if the WebContents is currently being initialized or destroyed.
  if (!GetPrimaryMainFrame()) {
    return {};
  }

  std::vector<RenderFrameHostImpl*> result;

  for (FrameTree* outermost_frame_tree : GetOutermostFrameTrees()) {
    DCHECK(outermost_frame_tree->GetMainFrame());
    result.push_back(outermost_frame_tree->GetMainFrame());
  }

  for (const auto& entry : GetController().GetBackForwardCache().GetEntries()) {
    result.push_back(entry->render_frame_host());
  }

  // In the case of inner WebContents, we still allow this method to be called,
  // but the semantics of the values being returned are "outermost
  // within this WebContents" as opposed to truly outermost. We would not expect
  // any other outermost pages besides the primary page in the case of inner
  // WebContents.
  DCHECK(!GetOuterWebContents() || (result.size() == 1));

  return result;
}

void WebContentsImpl::ExecutePageBroadcastMethod(
    PageBroadcastMethodCallback callback) {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::ExecutePageBroadcastMethod");
  primary_frame_tree_.root()->render_manager()->ExecutePageBroadcastMethod(
      callback);
}

void WebContentsImpl::ExecutePageBroadcastMethodForAllPages(
    PageBroadcastMethodCallback callback) {
  OPTIONAL_TRACE_EVENT0(
      "content", "WebContentsImpl::ExecutePageBroadcastMethodForAllPages");
  ForEachFrameTree(base::BindRepeating(
      [](PageBroadcastMethodCallback* callback, FrameTree& frame_tree) {
        frame_tree.root()->render_manager()->ExecutePageBroadcastMethod(
            *callback);
      },
      &callback));
}

RenderViewHostImpl* WebContentsImpl::GetRenderViewHost() {
  return GetRenderManager()->current_frame_host()->render_view_host();
}

void WebContentsImpl::CancelActiveAndPendingDialogs() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::CancelActiveAndPendingDialogs");
  if (dialog_manager_) {
    dialog_manager_->CancelDialogs(this, /*reset_state=*/false);
  }
  if (browser_plugin_embedder_) {
    browser_plugin_embedder_->CancelGuestDialogs();
  }
}

void WebContentsImpl::ClosePage() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::ClosePage");
  GetPrimaryMainFrame()->ClosePage(
      RenderFrameHostImpl::ClosePageSource::kBrowser);
}

RenderWidgetHostView* WebContentsImpl::GetRenderWidgetHostView() {
  return GetRenderManager()->GetRenderWidgetHostView();
}

RenderWidgetHostView* WebContentsImpl::GetTopLevelRenderWidgetHostView() {
  if (GetOuterWebContents()) {
    return GetOuterWebContents()->GetTopLevelRenderWidgetHostView();
  }
  return GetRenderManager()->GetRenderWidgetHostView();
}

WebContentsView* WebContentsImpl::GetView() const {
  return view_.get();
}

void WebContentsImpl::OnScreensChange(bool is_multi_screen_changed) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::OnScreensChange",
                        "is_multi_screen_changed", is_multi_screen_changed);
  // Mac display info may originate from a remote process hosting the NSWindow;
  // this local process display::Screen signal should not trigger updates.
  // TODO(crbug.com/40165350): Unify screen info plumbing, caching, etc.
#if !BUILDFLAG(IS_MAC)
  // This updates Screen attributes and fires Screen.change events as needed,
  // propagating to all widgets through the VisualProperties update waterfall.
  // This is triggered by system changes, not renderer IPC, so explicitly check
  // that the RenderWidgetHostView is valid before sending an update.
  if (RenderWidgetHostViewBase* view =
          GetRenderViewHost()->GetWidget()->GetView()) {
    // Only update top-level views, as child frames will have their ScreenInfos
    // updated by the visual property flow.
    if (!view->IsRenderWidgetHostViewChildFrame()) {
      view->UpdateScreenInfo();
    }
  }
#endif  // !BUILDFLAG(IS_MAC)
}

void WebContentsImpl::OnScreenOrientationChange() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::OnScreenOrientationChange");
  DCHECK(screen_orientation_provider_);
  DidChangeScreenOrientation();
  screen_orientation_provider_->OnOrientationChange();
}

std::optional<SkColor> WebContentsImpl::GetThemeColor() {
  return GetPrimaryPage().theme_color();
}

std::optional<SkColor> WebContentsImpl::GetBackgroundColor() {
  return GetPrimaryPage().background_color();
}

void WebContentsImpl::SetPageBaseBackgroundColor(std::optional<SkColor> color) {
  if (page_base_background_color_ == color) {
    return;
  }
  page_base_background_color_ = color;
  ExecutePageBroadcastMethod(base::BindRepeating(
      [](std::optional<SkColor> color, RenderViewHostImpl* rvh) {
        // Null `broadcast` can happen before view is created on the renderer
        // side, in which case this color will be sent in CreateView.
        if (auto& broadcast = rvh->GetAssociatedPageBroadcast()) {
          broadcast->SetPageBaseBackgroundColor(color);
        }
      },
      page_base_background_color_));
}

void WebContentsImpl::SetColorProviderSource(ui::ColorProviderSource* source) {
  ColorProviderSourceObserver::Observe(source);
}

ui::ColorProviderKey::ColorMode WebContentsImpl::GetColorMode() const {
  // A ColorProviderSource should always be set.
  auto* source = GetColorProviderSource();
  CHECK(source);
  return source->GetColorMode();
}

void WebContentsImpl::SetAccessibilityMode(ui::AXMode mode) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::SetAccessibilityMode",
                        "mode", mode.ToString(), "previous_mode",
                        accessibility_mode_.ToString());

  if (unrecoverable_accessibility_error_) {
    DUMP_WILL_BE_CHECK(accessibility_mode_.is_mode_off());
    return;
  }

  if (mode == accessibility_mode_) {
    return;
  }

  // Don't allow accessibility to be enabled for WebContents that are never
  // user-visible, like background pages.
  if (IsNeverComposited()) {
    return;
  }

  accessibility_mode_ = mode;

  // Update state for all frames in this tree and inner trees, including
  // speculative frame hosts and those in the back-forward cache. Take care not
  // to touch frame hosts that belong to another WebContents (e.g., guest views)
  // -- it is the responsibility of other WebContents' to update their own
  // frames. Note that content::BrowserAccessibilityState will propagate mode
  // flag changes that target the whole process (CreateScopedModeForProcess) or
  // a BrowserContext (CreateScopedModeForBrowserContext) to all relevant
  // WebContents, so inner WebContents will automatically receive the same mode
  // flag changes. In cases where mode flag changes apply to a specific
  // WebContents (via CreateScopedModeForWebContents), it is the responsibility
  // of the owner of the ScopedAccessibilityMode to choose whether or not to
  // create scopers for inner WebContents.
  ForEachRenderFrameHostIncludingSpeculativeWithAction(
      [this](RenderFrameHostImpl* frame_host) {
        if (WebContentsImpl::FromRenderFrameHostImpl(frame_host) == this) {
          frame_host->UpdateAccessibilityMode();
          return FrameIterationAction::kContinue;
        }
        return FrameIterationAction::kSkipChildren;
      });
}

void WebContentsImpl::DidCapturedSurfaceControl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  observers_.NotifyObservers(&WebContentsObserver::OnCapturedSurfaceControl);
}

void WebContentsImpl::ResetAccessibility() {
  // In contrast to the above, do not bother with frames in the back-forward
  // cache since the reset is intended to generate new trees for observers of
  // active frames.
  GetPrimaryMainFrame()->ForEachRenderFrameHostIncludingSpeculative(
      [this](RenderFrameHostImpl* frame_host) {
        if (WebContentsImpl::FromRenderFrameHostImpl(frame_host) == this) {
          frame_host->AccessibilityReset();
        }
      });
}

void WebContentsImpl::AddAccessibilityModeForTesting(ui::AXMode mode) {
  ui::AXMode new_mode(accessibility_mode_);
  new_mode |= mode;
  SetAccessibilityMode(new_mode);
}

// Helper class used by WebContentsImpl::RequestAXTreeSnapshot.
// Handles the callbacks from parallel snapshot requests to each frame,
// and feeds the results to an AXTreeCombiner, which converts them into a
// single combined accessibility tree.
class AXTreeSnapshotCombiner : public base::RefCounted<AXTreeSnapshotCombiner> {
 public:
  AXTreeSnapshotCombiner(WebContents::AXTreeSnapshotCallback callback,
                         mojom::SnapshotAccessibilityTreeParamsPtr params)
      : callback_(std::move(callback)), params_(std::move(params)) {}

  WebContents::AXTreeSnapshotCallback AddFrame(bool is_root) {
    // Adds a reference to |this|.
    return base::BindOnce(&AXTreeSnapshotCombiner::ReceiveSnapshot, this,
                          is_root);
  }

  void ReceiveSnapshot(bool is_root, ui::AXTreeUpdate& snapshot) {
    combiner_.AddTree(snapshot, is_root);
  }

  void AXTreeSnapshotOnFrame(RenderFrameHostImpl* rfhi) {
    OPTIONAL_TRACE_EVENT0("content",
                          "AXTreeSnapshotCombiner::AXTreeSnapshotOnFrame");
    bool is_root = rfhi->AccessibilityIsRootFrame();
    rfhi->RequestAXTreeSnapshot(AddFrame(is_root), params_.Clone());
  }

 private:
  friend class base::RefCounted<AXTreeSnapshotCombiner>;

  // This is called automatically after the last call to ReceiveSnapshot
  // when there are no more references to this object.
  ~AXTreeSnapshotCombiner() {
    combiner_.Combine();
    CHECK(combiner_.combined());
    ui::AXTreeUpdate update = std::move(combiner_.combined().value());
    std::move(callback_).Run(update);
  }

  ui::AXTreeCombiner combiner_;
  WebContents::AXTreeSnapshotCallback callback_;
  mojom::SnapshotAccessibilityTreeParamsPtr params_;
};

void WebContentsImpl::RequestAXTreeSnapshot(AXTreeSnapshotCallback callback,
                                            ui::AXMode ax_mode,
                                            size_t max_nodes,
                                            base::TimeDelta timeout,
                                            AXTreeSnapshotPolicy policy) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::RequestAXTreeSnapshot",
                        "mode", ax_mode.ToString());
  // Send a request to each of the frames in parallel. Each one will return
  // an accessibility tree snapshot, and AXTreeSnapshotCombiner will combine
  // them into a single tree and call |callback| with that result, then
  // delete |combiner|.
  auto params = mojom::SnapshotAccessibilityTreeParams::New();
  params->ax_mode = ax_mode.flags();
  params->max_nodes = max_nodes;
  params->timeout = timeout;

  auto combiner = base::MakeRefCounted<AXTreeSnapshotCombiner>(
      std::move(callback), std::move(params));
  GetPrimaryMainFrame()->ForEachRenderFrameHostWithAction(
      [this, &combiner, policy](RenderFrameHostImpl* rfh) {
        switch (policy) {
          case AXTreeSnapshotPolicy::kAll:
            break;
          case AXTreeSnapshotPolicy::kSameOriginDirectDescendants:
            if (GetPrimaryMainFrame()->GetSiteInstance() !=
                    rfh->GetSiteInstance() ||
                rfh->IsFencedFrameRoot() ||
                !GetPrimaryMainFrame()
                     ->GetLastCommittedOrigin()
                     .IsSameOriginWith(rfh->GetLastCommittedOrigin())) {
              return FrameIterationAction::kSkipChildren;
            }
            break;
        }
        combiner->AXTreeSnapshotOnFrame(rfh);
        return FrameIterationAction::kContinue;
      });
}

void WebContentsImpl::NotifyViewportFitChanged(
    blink::mojom::ViewportFit value) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::NotifyViewportFitChanged",
                        "value", static_cast<int>(value));
  observers_.NotifyObservers(&WebContentsObserver::ViewportFitChanged, value);
}

FindRequestManager* WebContentsImpl::GetFindRequestManagerForTesting() {
  return GetOrCreateFindRequestManager();
}

void WebContentsImpl::UpdateZoom() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::UpdateZoom");
  RenderWidgetHostImpl* rwh = GetRenderViewHost()->GetWidget();
  if (rwh->GetView()) {
    rwh->SynchronizeVisualProperties();
  }
}

void WebContentsImpl::UpdateZoomIfNecessary(const std::string& scheme,
                                            const std::string& host) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::UpdateZoomIfNecessary",
                        "scheme", scheme, "host", host);
  NavigationEntry* entry = GetController().GetLastCommittedEntry();
  if (!entry) {
    return;
  }

  GURL url = HostZoomMap::GetURLFromEntry(entry);
  if (host != net::GetHostOrSpecFromURL(url) ||
      (!scheme.empty() && !url.SchemeIs(scheme))) {
    return;
  }

  UpdateZoom();
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

void WebContentsImpl::OnManifestUrlChanged(PageImpl& page) {
  std::optional<GURL> manifest_url = page.GetManifestUrl();
  if (!manifest_url.has_value()) {
    return;
  }

  if (!page.IsPrimary()) {
    return;
  }

  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::NotifyManifestUrlChanged",
                        "render_frame_host", &page.GetMainDocument(),
                        "manifest_url", manifest_url);
  observers_.NotifyObservers(&WebContentsObserver::DidUpdateWebManifestURL,
                             &page.GetMainDocument(), *manifest_url);
}

WebUI* WebContentsImpl::GetWebUI() {
  return primary_frame_tree_.root()->current_frame_host()->web_ui();
}

void WebContentsImpl::SetAlwaysSendSubresourceNotifications() {
  if (!base::FeatureList::IsEnabled(
          features::kReduceSubresourceResponseStartedIPC)) {
    return;
  }

  if (GetSendSubresourceNotification()) {
    return;
  }

  // Updates all the renderers if the user allows certificate error or HTTP
  // exception, but doesn't update renderers when all exceptions are revoked
  // from all hosts since this causes superfluous IPCs.
  for (WebContentsImpl* web_contents : GetAllWebContents()) {
    DCHECK(!web_contents->GetSendSubresourceNotification());
    web_contents->renderer_preferences_.send_subresource_notification = true;
    web_contents->SyncRendererPrefs();
  }
}

bool WebContentsImpl::GetSendSubresourceNotification() {
  return GetRendererPrefs().send_subresource_notification;
}

void WebContentsImpl::SetUserAgentOverride(
    const blink::UserAgentOverride& ua_override,
    bool override_in_new_tabs) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::SetUserAgentOverride",
                        "ua_override", ua_override.ua_string_override,
                        "override_in_new_tabs", override_in_new_tabs);
  DCHECK(!ua_override.ua_metadata_override.has_value() ||
         !ua_override.ua_string_override.empty());

  if (GetUserAgentOverride() == ua_override) {
    return;
  }

  if (!ua_override.ua_string_override.empty() &&
      !net::HttpUtil::IsValidHeaderValue(ua_override.ua_string_override)) {
    return;
  }

  should_override_user_agent_in_new_tabs_ = override_in_new_tabs;

  // Update any in-flight load requests with overrides for new tabs.
  if (delayed_load_url_params_.get()) {
    delayed_load_url_params_->override_user_agent =
        override_in_new_tabs ? NavigationController::UA_OVERRIDE_TRUE
                             : NavigationController::UA_OVERRIDE_FALSE;
  }

  renderer_preferences_.user_agent_override = ua_override;

  // Send the new override string to all renderers in the current page.
  SyncRendererPrefs();

  // Reload the page if a load is currently in progress to avoid having
  // different parts of the page loaded using different user agents.
  // No need to reload if the current entry matches that of the
  // NavigationRequest supplied to DidStartNavigation() as NavigationRequest
  // handles it.
  ForEachFrameTree(base::BindRepeating([](FrameTree& frame_tree) {
    // For prerendering, we don't want to activate a prerendered page loaded
    // with a stale UA and will handle it even if it finishes loading.
    if (!frame_tree.IsLoadingIncludingInnerFrameTrees() &&
        !frame_tree.is_prerendering()) {
      return;
    }

    NavigationEntry* entry = frame_tree.controller().GetVisibleEntry();
    if (!entry || !entry->GetIsOverridingUserAgent()) {
      return;
    }
    if (frame_tree.root()->navigation_request() &&
        !frame_tree.root()->navigation_request()->ua_change_requires_reload()) {
      return;
    }
    if (frame_tree.is_prerendering()) {
      // Just cancel if the FrameTree is for prerendering, as prerendered
      // page may not allow another navigation including a reload, depending
      // on conditions.
      frame_tree.GetMainFrame()->CancelPrerendering(PrerenderCancellationReason(
          PrerenderFinalStatus::kUaChangeRequiresReload));
    } else {
      frame_tree.controller().Reload(ReloadType::BYPASSING_CACHE, true);
    }
  }));

  observers_.NotifyObservers(&WebContentsObserver::UserAgentOverrideSet,
                             ua_override);
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
  NavigationEntryImpl* current_entry = GetController().GetLastCommittedEntry();
  if (!current_entry || current_entry->IsInitialEntry()) {
    return should_override_user_agent_in_new_tabs_;
  }

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

bool WebContentsImpl::IsWebContentsOnlyAccessibilityModeForTesting() {
  return accessibility_mode_ == ui::kAXModeWebContentsOnly;
}

bool WebContentsImpl::IsFullAccessibilityModeForTesting() {
  return accessibility_mode_ == ui::kAXModeComplete;
}

#if BUILDFLAG(IS_ANDROID)

void WebContentsImpl::SetDisplayCutoutSafeArea(gfx::Insets insets) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::SetDisplayCutoutSafeArea");
  if (safe_area_insets_host_) {
    safe_area_insets_host_->SetDisplayCutoutSafeArea(insets);
  }
}

#endif

const std::u16string& WebContentsImpl::GetTitle() {
  WebUI* our_web_ui =
      GetRenderManager()->speculative_frame_host()
          ? GetRenderManager()->speculative_frame_host()->web_ui()
          : GetRenderManager()->current_frame_host()->web_ui();
  if (our_web_ui) {
    // Don't override the title in view source mode.
    NavigationEntry* entry = GetController().GetVisibleEntry();
    if (!(entry && entry->IsViewSourceMode())) {
      // Give the Web UI the chance to override our title.
      const std::u16string& title = our_web_ui->GetOverriddenTitle();
      if (!title.empty()) {
        return title;
      }
    }
  }

  return GetNavigationEntryForTitle()->GetTitleForDisplay();
}

const std::optional<std::u16string>& WebContentsImpl::GetAppTitle() {
  return GetNavigationEntryForTitle()->GetAppTitle();
}

SiteInstanceImpl* WebContentsImpl::GetSiteInstance() {
  return GetRenderManager()->current_frame_host()->GetSiteInstance();
}

bool WebContentsImpl::IsLoading() {
  return primary_frame_tree_.IsLoadingIncludingInnerFrameTrees();
}

double WebContentsImpl::GetLoadProgress() {
  // TODO(crbug.com/40177943): Make this MPArch friendly considering primary
  // frame tree and its descendants.
  return primary_frame_tree_.GetLoadProgress();
}

bool WebContentsImpl::ShouldShowLoadingUI() {
  return primary_frame_tree_.GetLoadingState() ==
         LoadingState::LOADING_UI_REQUESTED;
}

bool WebContentsImpl::IsDocumentOnLoadCompletedInPrimaryMainFrame() {
  // TODO(mparch): This should be moved to Page, and callers should use it
  // directly.
  return GetPrimaryPage().is_on_load_completed_in_main_document();
}

bool WebContentsImpl::IsWaitingForResponse() {
  NavigationRequest* ongoing_navigation_request =
      primary_frame_tree_.root()->navigation_request();

  // An ongoing navigation request means we're waiting for a response.
  return ongoing_navigation_request != nullptr;
}

bool WebContentsImpl::HasUncommittedNavigationInPrimaryMainFrame() {
  return primary_frame_tree_.root()->HasNavigation();
}

const net::LoadStateWithParam& WebContentsImpl::GetLoadState() {
  return load_state_;
}

const std::u16string& WebContentsImpl::GetLoadStateHost() {
  return load_state_host_;
}

uint64_t WebContentsImpl::GetUploadSize() {
  return upload_size_;
}

uint64_t WebContentsImpl::GetUploadPosition() {
  return upload_position_;
}

const std::string& WebContentsImpl::GetEncoding() {
  return GetPrimaryPage().GetEncoding();
}

void WebContentsImpl::Discard() {
  if (!base::FeatureList::IsEnabled(features::kWebContentsDiscard)) {
    NOTREACHED_NORETURN();
  }

  AboutToBeDiscarded(this);
  notify_disconnection_ = false;
  CancelAllPrerendering();
  primary_frame_tree_.Discard();
  NotifyWasDiscarded();
}

bool WebContentsImpl::WasDiscarded() {
  return GetPrimaryFrameTree().root()->was_discarded();
}

void WebContentsImpl::SetWasDiscarded(bool was_discarded) {
  // It's set based on a tab and the setting value is started from a primary
  // tree and propagated to all children nodes including a fenced frame node.
  GetPrimaryFrameTree().root()->set_was_discarded();
}

base::ScopedClosureRunner WebContentsImpl::IncrementCapturerCount(
    const gfx::Size& capture_size,
    bool stay_hidden,
    bool stay_awake,
    bool is_activity) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::IncrementCapturerCount");
  DCHECK(!IsBeingDestroyed());
  if (stay_hidden) {
    // A hidden capture should not have side effect on the web contents, so it
    // should not pass a non-empty |capture_size| which will cause side effect.
    DCHECK(capture_size.IsEmpty());
    ++hidden_capturer_count_;
  } else {
    ++visible_capturer_count_;
  }

  if (stay_awake) {
    ++stay_awake_capturer_count_;
  }

  view_->OnCapturerCountChanged();

  // Note: This provides a hint to upstream code to size the views optimally
  // for quality (e.g., to avoid scaling).
  if (!capture_size.IsEmpty() && preferred_size_for_capture_.IsEmpty()) {
    preferred_size_for_capture_ = capture_size;
    OnPreferredSizeChanged(preferred_size_);
  }

  if (!capture_wake_lock_ && stay_awake_capturer_count_) {
    if (auto* wake_lock_context = GetWakeLockContext()) {
      auto receiver = capture_wake_lock_.BindNewPipeAndPassReceiver();
      wake_lock_context->GetWakeLock(
          device::mojom::WakeLockType::kPreventDisplaySleepAllowDimming,
          device::mojom::WakeLockReason::kOther, "Capturing",
          std::move(receiver));
    }
  }

  if (capture_wake_lock_) {
    capture_wake_lock_->RequestWakeLock();
  }

  UpdateVisibilityAndNotifyPageAndView(GetVisibility(), is_activity);

  return base::ScopedClosureRunner(base::BindOnce(
      &WebContentsImpl::DecrementCapturerCount, weak_factory_.GetWeakPtr(),
      stay_hidden, stay_awake, is_activity));
}

const blink::mojom::CaptureHandleConfig&
WebContentsImpl::GetCaptureHandleConfig() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return capture_handle_config_;
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

  if (mute == IsAudioMuted()) {
    return;
  }

  GetAudioStreamFactory()->SetMuted(mute);

  observers_.NotifyObservers(&WebContentsObserver::DidUpdateAudioMutingState,
                             mute);
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

bool WebContentsImpl::IsConnectedToUsbDevice() {
  return usb_active_frame_count_ > 0;
}

bool WebContentsImpl::HasFileSystemAccessHandles() {
  return file_system_access_handle_count_ > 0;
}

bool WebContentsImpl::HasPictureInPictureVideo() {
  return has_picture_in_picture_video_;
}

bool WebContentsImpl::HasPictureInPictureDocument() {
  return has_picture_in_picture_document_;
}

void WebContentsImpl::SetHasPictureInPictureCommon(
    bool has_picture_in_picture) {
  NotifyNavigationStateChanged(INVALIDATE_TYPE_TAB);
  observers_.NotifyObservers(&WebContentsObserver::MediaPictureInPictureChanged,
                             has_picture_in_picture);

  // Picture-in-picture state can affect how we notify visibility for non-
  // visible pages.
  if (visibility_ != Visibility::VISIBLE) {
    UpdateVisibilityAndNotifyPageAndView(visibility_);
  }
}

void WebContentsImpl::DisallowCustomCursorScopeExpired() {
  --disallow_custom_cursor_scope_count_;
}

void WebContentsImpl::SetHasPictureInPictureVideo(
    bool has_picture_in_picture_video) {
  OPTIONAL_TRACE_EVENT1("content",
                        "WebContentsImpl::SetHasPictureInPictureVideo",
                        "has_pip_video", has_picture_in_picture_video);
  // If status of |this| is already accurate, there is no need to update.
  if (has_picture_in_picture_video == has_picture_in_picture_video_) {
    return;
  }
  has_picture_in_picture_video_ = has_picture_in_picture_video;
  SetHasPictureInPictureCommon(has_picture_in_picture_video);
}

void WebContentsImpl::SetHasPictureInPictureDocument(
    bool has_picture_in_picture_document) {
  OPTIONAL_TRACE_EVENT1("content",
                        "WebContentsImpl::SetHasPictureInPictureDocument",
                        "has_pip_document", has_picture_in_picture_document);
  // If status of |this| is already accurate, there is no need to update.
  if (has_picture_in_picture_document == has_picture_in_picture_document_) {
    return;
  }
  has_picture_in_picture_document_ = has_picture_in_picture_document;
  SetHasPictureInPictureCommon(has_picture_in_picture_document);
}

bool WebContentsImpl::IsCrashed() {
  switch (primary_main_frame_process_status_) {
    case base::TERMINATION_STATUS_PROCESS_CRASHED:
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
    case base::TERMINATION_STATUS_OOM:
    case base::TERMINATION_STATUS_LAUNCH_FAILED:
#if BUILDFLAG(IS_CHROMEOS)
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM:
#endif
#if BUILDFLAG(IS_ANDROID)
    case base::TERMINATION_STATUS_OOM_PROTECTED:
#endif
#if BUILDFLAG(IS_WIN)
    case base::TERMINATION_STATUS_INTEGRITY_FAILURE:
#endif
      return true;
    case base::TERMINATION_STATUS_NORMAL_TERMINATION:
    case base::TERMINATION_STATUS_STILL_RUNNING:
      return false;
    case base::TERMINATION_STATUS_MAX_ENUM:
      NOTREACHED_IN_MIGRATION();
      return false;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

void WebContentsImpl::SetPrimaryMainFrameProcessStatus(
    base::TerminationStatus status,
    int error_code) {
  OPTIONAL_TRACE_EVENT2("content",
                        "WebContentsImpl::SetPrimaryMainFrameProcessStatus",
                        "status", static_cast<int>(status), "old_status",
                        static_cast<int>(primary_main_frame_process_status_));
  if (status == primary_main_frame_process_status_) {
    return;
  }

  primary_main_frame_process_status_ = status;
  primary_main_frame_process_error_code_ = error_code;
  NotifyNavigationStateChanged(INVALIDATE_TYPE_TAB);
}

base::TerminationStatus WebContentsImpl::GetCrashedStatus() {
  return primary_main_frame_process_status_;
}

int WebContentsImpl::GetCrashedErrorCode() {
  return primary_main_frame_process_error_code_;
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

  if (delegate_) {
    delegate_->NavigationStateChanged(this, changed_flags);
  }

  if (GetOuterWebContents()) {
    GetOuterWebContents()->NotifyNavigationStateChanged(changed_flags);
  }
}

void WebContentsImpl::OnVerticalScrollDirectionChanged(
    viz::VerticalScrollDirection scroll_direction) {
  OPTIONAL_TRACE_EVENT1("content",
                        "WebContentsImpl::OnVerticalScrollDirectionChanged",
                        "scroll_direction", static_cast<int>(scroll_direction));
  observers_.NotifyObservers(
      &WebContentsObserver::DidChangeVerticalScrollDirection, scroll_direction);
}

int WebContentsImpl::GetVirtualKeyboardResizeHeight() {
  // Only consider a web contents to be insetted by the virtual keyboard if it
  // is in the currently active tab.
  if (GetVisibility() != Visibility::VISIBLE) {
    return 0;
  }

  // The only mode where the virtual keyboard causes the web contents to be
  // resized is kResizesContent.
  if (GetPrimaryPage().virtual_keyboard_mode() !=
      ui::mojom::VirtualKeyboardMode::kResizesContent) {
    return 0;
  }

  // The virtual keyboard never resizes content when fullscreened.
  if (IsFullscreen()) {
    return 0;
  }

  return GetDelegate() ? GetDelegate()->GetVirtualKeyboardHeight(this) : 0;
}

bool WebContentsImpl::ShouldDoLearning() {
  return !GetBrowserContext()->IsOffTheRecord();
}

void WebContentsImpl::OnAudioStateChanged() {
  // This notification can come from any embedded contents or from this
  // WebContents' stream monitor. Aggregate these signals to get the actual
  // state.
  //
  // Note that guests may not be attached as inner contents, and so may need to
  // be checked separately.  This intentionally does not do a recursive search,
  // since the state is aggregated in each inner WebContents and reflects that
  // whole subtree.
  bool is_currently_audible =
      audio_stream_monitor_.IsCurrentlyAudible() ||
      (browser_plugin_embedder_ &&
       browser_plugin_embedder_->AreAnyGuestsCurrentlyAudible()) ||
      base::ranges::any_of(GetInnerWebContents(),
                           [](WebContents* inner_contents) {
                             return inner_contents->IsCurrentlyAudible();
                           });
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::OnAudioStateChanged",
                        "is_currently_audible", is_currently_audible,
                        "was_audible", is_currently_audible_);
  if (is_currently_audible == is_currently_audible_) {
    return;
  }

  // Update internal state.
  is_currently_audible_ = is_currently_audible;
  was_ever_audible_ = was_ever_audible_ || is_currently_audible_;

  ExecutePageBroadcastMethod(base::BindRepeating(
      [](bool is_currently_audible, RenderViewHostImpl* rvh) {
        if (auto& broadcast = rvh->GetAssociatedPageBroadcast()) {
          broadcast->AudioStateChanged(is_currently_audible);
        }
      },
      is_currently_audible_));

  // Notification for UI updates in response to the changed audio state.
  NotifyNavigationStateChanged(INVALIDATE_TYPE_AUDIO);

  // Ensure that audio state changes propagate from innermost to outermost
  // WebContents.
  if (GetOuterWebContents()) {
    GetOuterWebContents()->OnAudioStateChanged();
  }

  observers_.NotifyObservers(&WebContentsObserver::OnAudioStateChanged,
                             is_currently_audible_);
}

base::TimeTicks WebContentsImpl::GetLastActiveTimeTicks() {
  return last_active_time_ticks_;
}

base::Time WebContentsImpl::GetLastActiveTime() {
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

bool WebContentsImpl::HasRecentInteraction() {
  if (last_interaction_time_.is_null()) {
    return false;
  }

  static constexpr base::TimeDelta kMaxInterval = base::Seconds(5);
  base::TimeDelta delta = ui::EventTimeForNow() - last_interaction_time_;
  // Note: the expectation is that the caller is typically expecting an input
  // event, e.g. validating that a WebUI message that requires a gesture is
  // actually attached to a gesture.
  return delta <= kMaxInterval;
}

WebContents::ScopedIgnoreInputEvents WebContentsImpl::IgnoreInputEvents(
    std::optional<WebInputEventAuditCallback> audit_callback) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::IgnoreInputEvents");

  uint64_t callback_id = 0;
  if (audit_callback.has_value()) {
    do {
      callback_id = next_web_input_event_audit_callback_id_++;
    } while (web_input_event_audit_callbacks_.contains(callback_id));
    web_input_event_audit_callbacks_[callback_id] = std::move(*audit_callback);
  } else {
#if BUILDFLAG(IS_ANDROID)
    if (ignore_input_events_count_ == 0) {
      // Reset gesture detection before starting input suppression so that any
      // ongoing scroll gesture is correctly finished.
      //
      // TODO(crbug.com/362301376): This might be a side-effect of the
      // referenced bug. Revisit restoring the CHECK when it's resolved.
      if (auto* view = GetRenderWidgetHostView()) {
        static_cast<RenderWidgetHostViewBase*>(view)->ResetGestureDetection();
      }
    }
#endif
    ++ignore_input_events_count_;
  }

  // Bind weakly, since the token might outlive us.
  return ScopedIgnoreInputEvents(base::BindOnce(
      [](base::WeakPtr<WebContentsImpl> wc,
         std::optional<uint64_t> callback_id) {
        if (wc) {
          OPTIONAL_TRACE_EVENT0("content",
                                "WebContentsImpl::IgnoreInputEvents.Release");
          if (callback_id.has_value()) {
            CHECK(wc->web_input_event_audit_callbacks_.contains(*callback_id));
            wc->web_input_event_audit_callbacks_.erase(*callback_id);
          } else {
#if BUILDFLAG(IS_ANDROID)
            // Reset gesture detection so that we don't continue to generate new
            // gestures from suppressed touches. These suppressed gestures would
            // otherwise confuse the event stream validator when input is
            // re-enabled.
            //
            // This needs to be done while input is still suppressed since
            // resetting can generate gesture end events for a gesture sequence
            // which was being suppressed.
            if (wc->ignore_input_events_count_ == 1) {
              if (auto* view = wc->GetRenderWidgetHostView()) {
                static_cast<RenderWidgetHostViewBase*>(view)
                    ->ResetGestureDetection();
              }
            }
#endif
            --wc->ignore_input_events_count_;
          }
        }
      },
      weak_factory_.GetWeakPtr(),
      audit_callback.has_value() ? std::make_optional<uint64_t>(callback_id)
                                 : std::nullopt));
}

bool WebContentsImpl::HasActiveEffectivelyFullscreenVideo() {
  return IsFullscreen() &&
         media_web_contents_observer_->HasActiveEffectivelyFullscreenVideo();
}

void WebContentsImpl::WriteIntoTrace(perfetto::TracedValue context) {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("root_frame_tree_node_id",
           primary_frame_tree_.root()->frame_tree_node_id());
}

const base::Location& WebContentsImpl::GetCreatorLocation() {
  return creator_location_;
}

const std::optional<blink::mojom::PictureInPictureWindowOptions>&
WebContentsImpl::GetPictureInPictureOptions() const {
  return picture_in_picture_options_;
}

#if BUILDFLAG(IS_ANDROID)
void WebContentsImpl::SetPrimaryMainFrameImportance(
    ChildProcessImportance importance) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::SetMainFrameImportance",
                        "importance", static_cast<int>(importance));
  GetPrimaryMainFrame()->GetRenderWidgetHost()->SetImportance(importance);
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
  if (!notify_disconnection_) {
    return false;
  }

  // Don't fire if the main frame indicates that beforeunload and unload have
  // already executed (e.g., after receiving a ClosePage ACK) or should be
  // ignored.
  if (GetPrimaryMainFrame()->IsPageReadyToBeClosed()) {
    return false;
  }

  // Check whether any frame in the frame tree needs to run beforeunload or
  // unload-time event handlers.
  for (FrameTreeNode* node : primary_frame_tree_.Nodes()) {
    RenderFrameHostImpl* rfh = node->current_frame_host();

    // No need to run beforeunload/unload-time events if the RenderFrame isn't
    // live.
    if (!rfh->IsRenderFrameLive()) {
      continue;
    }
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
  GetPrimaryMainFrame()->DispatchBeforeUnload(before_unload_type, false);
}

bool WebContentsImpl::IsInnerWebContentsForGuest() {
  return IsGuest();
}

void WebContentsImpl::AttachInnerWebContents(
    std::unique_ptr<WebContents> inner_web_contents,
    RenderFrameHost* render_frame_host,
    bool is_full_page) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::AttachInnerWebContents",
                        "inner_web_contents",
                        static_cast<void*>(inner_web_contents.get()),
                        "is_full_page", is_full_page);
  WebContentsImpl* inner_web_contents_impl =
      static_cast<WebContentsImpl*>(inner_web_contents.get());
  DCHECK(!inner_web_contents_impl->node_.outer_web_contents());
  auto* render_frame_host_impl =
      static_cast<RenderFrameHostImpl*>(render_frame_host);
  DCHECK_EQ(this, WebContents::FromRenderFrameHost(render_frame_host_impl));
  DCHECK(render_frame_host_impl->GetParent());

  // Inner WebContents aren't supported with prerendering. See
  // https://crbug.com/40191159 for details.
  CHECK_NE(RenderFrameHostImpl::LifecycleStateImpl::kPrerendering,
           render_frame_host_impl->lifecycle_state());

  RenderFrameHostManager* inner_render_manager =
      inner_web_contents_impl->GetRenderManager();
  RenderFrameHostImpl* inner_main_frame =
      inner_render_manager->current_frame_host();
  RenderViewHostImpl* inner_render_view_host =
      inner_main_frame->render_view_host();
  auto* outer_render_manager =
      render_frame_host_impl->frame_tree_node()->render_manager();

  // Make |render_frame_host_impl| an outer delegate frame.
  render_frame_host_impl->set_inner_tree_main_frame_tree_node_id(
      inner_main_frame->frame_tree_node()->frame_tree_node_id());

  // When attaching a WebContents as an inner WebContents, we need to replace
  // the Webcontents' view with a WebContentsViewChildFrame.
  inner_web_contents_impl->view_ = std::make_unique<WebContentsViewChildFrame>(
      inner_web_contents_impl,
      GetContentClient()->browser()->GetWebContentsViewDelegate(
          inner_web_contents_impl),
      &inner_web_contents_impl->render_view_host_delegate_view_);
  // On platforms where destroying the WebContents' view does not also destroy
  // the platform RenderWidgetHostView, we need to destroy it if it exists.
  // TODO(mcnee): Should all platforms' WebContentsView destroy the platform
  // RWHV?
  if (RenderWidgetHostViewBase* prev_rwhv =
          inner_render_manager->GetRenderWidgetHostView()) {
    if (!prev_rwhv->IsRenderWidgetHostViewChildFrame()) {
      prev_rwhv->Destroy();
    }
  }

  // When the WebContents being initialized has an opener, the  browser side
  // Render{View,Frame}Host must be initialized and the RenderWidgetHostView
  // created. This is needed because the usual initialization happens during
  // the first navigation, but when attaching a new window we don't navigate
  // before attaching. If the browser side is already initialized, the calls
  // below will just early return.
  inner_render_manager->InitRenderView(
      inner_main_frame->GetSiteInstance()->group(), inner_render_view_host,
      nullptr);
  if (!inner_render_manager->GetRenderWidgetHostView()) {
    inner_web_contents_impl->CreateRenderWidgetHostViewForRenderManager(
        inner_render_view_host);
  }

  inner_web_contents_impl->RecursivelyUnregisterRenderWidgetHostViews();

  // Create a link to our outer WebContents.
  node_.AttachInnerWebContents(std::move(inner_web_contents),
                               render_frame_host_impl);

  // Create a proxy in top-level RenderFrameHostManager, pointing to the
  // SiteInstanceGroup of the outer WebContents. The proxy will be used to send
  // postMessage to the inner WebContents.
  auto* proxy =
      inner_main_frame->browsing_context_state()->CreateOuterDelegateProxy(
          render_frame_host_impl->GetSiteInstance()->group(),
          inner_main_frame->frame_tree_node(), blink::RemoteFrameToken());
  // Since the inner WebContents is created from the browser side we do
  // not have RemoteFrame mojo channels. New channels will be bound when the
  // `CreateView` IPC is sent.

  // When attaching a GuestView as an inner WebContents, there should already be
  // a live RenderFrame, which has to be swapped.
  if (render_frame_host_impl->IsRenderFrameLive()) {
    inner_render_manager->SwapOuterDelegateFrame(render_frame_host_impl, proxy);

    inner_web_contents_impl->ReattachToOuterWebContentsFrame();
  }

  if (primary_frame_tree_.GetFocusedFrame() ==
      render_frame_host_impl->frame_tree_node()) {
    inner_web_contents_impl->SetFocusedFrame(
        inner_web_contents_impl->primary_frame_tree_.root(),
        render_frame_host_impl->GetSiteInstance()->group());
  }
  outer_render_manager->set_attach_complete();

  // If the inner WebContents is full frame, give it focus.
  if (is_full_page) {
    // There should only ever be one inner WebContents when |is_full_page| is
    // true, and it is the one we just attached.
    DCHECK_EQ(1u, node_.GetInnerWebContents().size());
    inner_web_contents_impl->SetAsFocusedWebContentsIfNecessary();
  }

  observers_.NotifyObservers(&WebContentsObserver::InnerWebContentsAttached,
                             inner_web_contents_impl, render_frame_host);

  // Make sure that the inner web contents and its outer delegate get properly
  // linked via the embedding token now that inner web contents are attached.
  inner_main_frame->PropagateEmbeddingTokenToParentFrame();
}

void WebContentsImpl::RecursivelyRegisterRenderWidgetHostViews() {
  OPTIONAL_TRACE_EVENT0(
      "content", "WebContentsImpl::RecursivelyRegisterRenderWidgetHostViews");

  auto* text_input_manager = GetTextInputManager();
  for (auto* view : GetRenderWidgetHostViewsInWebContentsTree()) {
    if (text_input_manager) {
      // Use RenderWidgetHostView::GetTextInputManager() for its side effects,
      // rather than registering the view directly; the view caches the
      // TextInputManager.
      //
      // TODO(crbug.com/40212969): This probably could be made more symmetrical
      // with unregistration. Getting rid of lazy registration might also allow
      // eliminating some special cases in TextInputManager.
      auto* text_input_manager_for_view = view->GetTextInputManager();
      DCHECK_EQ(text_input_manager, text_input_manager_for_view);
    }

    if (view->IsRenderWidgetHostViewChildFrame()) {
      static_cast<RenderWidgetHostViewChildFrame*>(view)->RegisterFrameSinkId();
    }
  }
}

void WebContentsImpl::RecursivelyUnregisterRenderWidgetHostViews() {
  OPTIONAL_TRACE_EVENT0(
      "content", "WebContentsImpl::RecursivelyUnregisterRenderWidgetHostViews");

  auto* text_input_manager = GetTextInputManager();
  for (auto* view : GetRenderWidgetHostViewsInWebContentsTree()) {
    if (text_input_manager) {
      // The RenderWidgetHostView will drop the cached TextInputManager itself.
      text_input_manager->Unregister(view);
    }

    if (view->IsRenderWidgetHostViewChildFrame()) {
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
  auto* child_rwhv = render_manager->GetRenderWidgetHostView();
  DCHECK(child_rwhv);
  DCHECK(child_rwhv->IsRenderWidgetHostViewChildFrame());
  render_manager->SetRWHViewForInnerFrameTree(
      static_cast<RenderWidgetHostViewChildFrame*>(child_rwhv));

  RecursivelyRegisterRenderWidgetHostViews();
  GetPrimaryMainFrame()->UpdateAXTreeData();
}

void WebContentsImpl::DidActivatePreviewedPage(
    base::TimeTicks activation_time) {
  TRACE_EVENT1("content", "WebContentsImpl::DidActivatePreviewedPage",
               "activation_time", activation_time);
  observers_.NotifyObservers(&WebContentsObserver::DidActivatePreviewedPage,
                             activation_time);
  GetDelegate()->DidActivatePreviewedPage();
}

void WebContentsImpl::DidChangeVisibleSecurityState() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::DidChangeVisibleSecurityState");
  if (delegate_) {
    delegate_->VisibleSecurityStateChanged(this);
  }

  SCOPED_UMA_HISTOGRAM_TIMER(
      "WebContentsObserver.DidChangeVisibleSecurityState");
  observers_.NotifyObservers(
      &WebContentsObserver::DidChangeVisibleSecurityState);
}

const blink::web_pref::WebPreferences WebContentsImpl::ComputeWebPreferences() {
  OPTIONAL_TRACE_EVENT0("browser", "WebContentsImpl::ComputeWebPreferences");

  blink::web_pref::WebPreferences prefs;

  // Sets the hardware-related fields in |prefs| that are slow to compute. The
  // fields are set from cache if available, otherwise recomputed.
  SlowWebPreferenceCache::GetInstance()->Load(&prefs);

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  prefs.web_security_enabled =
      !command_line.HasSwitch(switches::kDisableWebSecurity);

  prefs.remote_fonts_enabled =
      !command_line.HasSwitch(switches::kDisableRemoteFonts);
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

  prefs.allow_file_access_from_file_urls =
      command_line.HasSwitch(switches::kAllowFileAccessFromFiles);

  prefs.accelerated_2d_canvas_enabled =
      !command_line.HasSwitch(switches::kDisableAccelerated2dCanvas);
  prefs.canvas_2d_layers_enabled =
      command_line.HasSwitch(switches::kEnableCanvas2DLayers) ||
      base::FeatureList::IsEnabled(features::kEnableCanvas2DLayers);
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
        blink::mojom::AutoplayPolicy::kNoUserGestureRequired;
  } else if (autoplay_policy ==
             switches::autoplay::kUserGestureRequiredPolicy) {
    prefs.autoplay_policy = blink::mojom::AutoplayPolicy::kUserGestureRequired;
  } else if (autoplay_policy ==
             switches::autoplay::kDocumentUserActivationRequiredPolicy) {
    prefs.autoplay_policy =
        blink::mojom::AutoplayPolicy::kDocumentUserActivationRequired;
  } else {
    NOTREACHED_IN_MIGRATION();
  }

  prefs.dont_send_key_events_to_javascript =
      base::FeatureList::IsEnabled(features::kDontSendKeyEventsToJavascript);

// TODO(dtapuska): Enable barrel button selection drag support on Android.
// crbug.com/758042
#if BUILDFLAG(IS_WIN)
  prefs.barrel_button_for_drag_enabled = true;
#endif  // BUILDFLAG(IS_WIN)

  prefs.enable_scroll_animator =
      command_line.HasSwitch(switches::kEnableSmoothScrolling) ||
      (!command_line.HasSwitch(switches::kDisableSmoothScrolling) &&
       gfx::Animation::ScrollAnimationsEnabledBySystem());

  prefs.prefers_reduced_motion = gfx::Animation::PrefersReducedMotion();
  prefs.prefers_reduced_transparency = prefers_reduced_transparency_;
  prefs.inverted_colors = inverted_colors_;

  if (ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
          GetRenderViewHost()->GetProcess()->GetID())) {
    prefs.loads_images_automatically = true;
    prefs.javascript_enabled = true;
  }

  prefs.viewport_enabled = command_line.HasSwitch(switches::kEnableViewport);

#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/40925473): GetPrimaryDisplay() won't be correct for
  // externally connected displays. Get the display where Chrome is opened
  // instead.
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  gfx::Size size = display.GetSizeInPixel();
  int min_width = size.width() < size.height() ? size.width() : size.height();
  int min_width_in_dp =
      static_cast<int>(min_width / display.device_scale_factor());
  if (prefs.viewport_enabled &&
      base::FeatureList::IsEnabled(
          blink::features::kDefaultViewportIsDeviceWidth) &&
      (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_AUTOMOTIVE ||
       (min_width_in_dp >= kAndroidMinimumTabletWidthDp &&
        ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TV))) {
    prefs.viewport_style = blink::mojom::ViewportStyle::kDefault;
  }
#endif

  if (GetController().GetVisibleEntry() &&
      GetController().GetVisibleEntry()->GetIsOverridingUserAgent()) {
#if BUILDFLAG(IS_ANDROID)
    // Only ignore viewport meta tag when Request Desktop Site is used, but not
    // in other situations where embedder changes to arbitrary mobile UA string.
    bool is_request_desktop_site =
        renderer_preferences_.user_agent_override.ua_metadata_override &&
        !renderer_preferences_.user_agent_override.ua_metadata_override->mobile;
    prefs.viewport_meta_enabled = !is_request_desktop_site;
#else
    prefs.viewport_meta_enabled = false;
#endif
  }

  prefs.spatial_navigation_enabled =
      command_line.HasSwitch(switches::kEnableSpatialNavigation);

  if (is_spatial_navigation_disabled_) {
    prefs.spatial_navigation_enabled = false;
  }

#if BUILDFLAG(IS_ANDROID)
  prefs.long_press_link_select_text = long_press_link_select_text_;
#endif

  prefs.stylus_handwriting_enabled = stylus_handwriting_enabled_;

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

  if (is_overlay_content_) {
    prefs.hide_download_ui = true;
  }

  // `media_controls_enabled` is `true` by default.
  if (has_persistent_video_) {
    prefs.media_controls_enabled = false;
  }

#if BUILDFLAG(IS_ANDROID)
  prefs.device_scale_adjustment = GetDeviceScaleAdjustment(min_width_in_dp);
#endif  // BUILDFLAG(IS_ANDROID)

  // GuestViews in the same StoragePartition need to find each other's frames.
  prefs.renderer_wide_named_frame_lookup = IsGuest();

  if (command_line.HasSwitch(switches::kHideScrollbars)) {
    prefs.hide_scrollbars = true;
  }

  GetContentClient()->browser()->OverrideWebkitPrefs(this, &prefs);
  return prefs;
}

void WebContentsImpl::OnWebPreferencesChanged() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::OnWebPreferencesChanged");

  // This is defensive code to avoid infinite loops due to code run inside
  // SetWebPreferences() accidentally updating more preferences and thus
  // calling back into this code. See crbug.com/398751 for one past example.
  if (updating_web_preferences_) {
    return;
  }
  updating_web_preferences_ = true;
  SetWebPreferences(ComputeWebPreferences());
#if BUILDFLAG(IS_ANDROID)
  for (FrameTreeNode* node : primary_frame_tree_.Nodes()) {
    RenderFrameHostImpl* rfh = node->current_frame_host();
    if (rfh->is_local_root()) {
      if (auto* rwh = rfh->GetRenderWidgetHost()) {
        rwh->SetForceEnableZoom(web_preferences_->force_enable_zoom);
      }
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

  blink::RendererPreferences renderer_preferences = GetRendererPrefs();
  RenderViewHostImpl::GetPlatformSpecificPrefs(&renderer_preferences);
  ExecutePageBroadcastMethodForAllPages(base::BindRepeating(
      [](const blink::RendererPreferences& preferences,
         RenderViewHostImpl* rvh) {
        rvh->SendRendererPreferencesToRenderer(preferences);
      },
      renderer_preferences));
}

void WebContentsImpl::OnCookiesAccessed(NavigationHandle* navigation,
                                        const CookieAccessDetails& details) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::OnCookiesAccessed",
                        "navigation_handle", navigation);
  // Use a variable to select between overloads.
  void (WebContentsObserver::*func)(NavigationHandle*,
                                    const CookieAccessDetails&) =
      &WebContentsObserver::OnCookiesAccessed;
  observers_.NotifyObservers(func, navigation, details);
}

void WebContentsImpl::OnCookiesAccessed(RenderFrameHostImpl* rfh,
                                        const CookieAccessDetails& details) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::OnCookiesAccessed",
                        "render_frame_host", rfh);
  // Use a variable to select between overloads.
  void (WebContentsObserver::*func)(RenderFrameHost*,
                                    const CookieAccessDetails&) =
      &WebContentsObserver::OnCookiesAccessed;
  observers_.NotifyObservers(func, rfh, details);
}

void WebContentsImpl::OnTrustTokensAccessed(
    NavigationHandle* navigation,
    const TrustTokenAccessDetails& details) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::OnTrustTokensAccessed",
                        "navigation_handle", navigation);
  // Use a variable to select between overloads.
  void (WebContentsObserver::*func)(NavigationHandle*,
                                    const TrustTokenAccessDetails&) =
      &WebContentsObserver::OnTrustTokensAccessed;
  observers_.NotifyObservers(func, navigation, details);
}

void WebContentsImpl::OnTrustTokensAccessed(
    RenderFrameHostImpl* rfh,
    const TrustTokenAccessDetails& details) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::OnTrustTokensAccessed",
                        "render_frame_host", rfh);
  // Use a variable to select between overloads.
  void (WebContentsObserver::*func)(RenderFrameHost*,
                                    const TrustTokenAccessDetails&) =
      &WebContentsObserver::OnTrustTokensAccessed;
  observers_.NotifyObservers(func, rfh, details);
}

void WebContentsImpl::OnSharedDictionaryAccessed(
    NavigationHandle* navigation,
    const network::mojom::SharedDictionaryAccessDetails& details) {
  OPTIONAL_TRACE_EVENT1("content",
                        "WebContentsImpl::OnSharedDictionaryAccessed",
                        "navigation_handle", navigation);
  // Use a variable to select between overloads.
  void (WebContentsObserver::*func)(
      NavigationHandle*, const network::mojom::SharedDictionaryAccessDetails&) =
      &WebContentsObserver::OnSharedDictionaryAccessed;
  observers_.NotifyObservers(func, navigation, details);
}

void WebContentsImpl::OnSharedDictionaryAccessed(
    RenderFrameHostImpl* rfh,
    const network::mojom::SharedDictionaryAccessDetails& details) {
  OPTIONAL_TRACE_EVENT1("content",
                        "WebContentsImpl::OnSharedDictionaryAccessed",
                        "render_frame_host", rfh);
  // Use a variable to select between overloads.
  void (WebContentsObserver::*func)(
      RenderFrameHost*, const network::mojom::SharedDictionaryAccessDetails&) =
      &WebContentsObserver::OnSharedDictionaryAccessed;
  observers_.NotifyObservers(func, rfh, details);
}

void WebContentsImpl::NotifyStorageAccessed(
    RenderFrameHostImpl* rfh,
    blink::mojom::StorageTypeAccessed storage_type,
    bool blocked) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::NotifyStorageAccessed",
                        "render_frame_host", rfh);
  observers_.NotifyObservers(&WebContentsObserver::NotifyStorageAccessed, rfh,
                             storage_type, blocked);
}

void WebContentsImpl::OnVibrate(RenderFrameHostImpl* rfh) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::OnVibrate",
                        "render_frame_host", rfh);
  observers_.NotifyObservers(&WebContentsObserver::VibrationRequested);
}

std::optional<blink::ParsedPermissionsPolicy>
WebContentsImpl::GetPermissionsPolicyForIsolatedWebApp(
    RenderFrameHostImpl* source) {
  WebExposedIsolationInfo weii =
      source->GetSiteInstance()->GetWebExposedIsolationInfo();
  CHECK(weii.is_isolated_application());
  return GetContentClient()->browser()->GetPermissionsPolicyForIsolatedWebApp(
      this, weii.origin());
}

void WebContentsImpl::Stop() {
  TRACE_EVENT0("content", "WebContentsImpl::Stop");
  ForEachFrameTree(base::BindRepeating(
      [](FrameTree& frame_tree) { frame_tree.StopLoading(); }));
  GetPrerenderHostRegistry()->CancelAllHosts(PrerenderFinalStatus::kStop);
  observers_.NotifyObservers(&WebContentsObserver::NavigationStopped);
}

void WebContentsImpl::SetPageFrozen(bool frozen) {
  TRACE_EVENT1("content", "WebContentsImpl::SetPageFrozen", "frozen", frozen);

  // A visible page is never frozen.
  DCHECK_NE(Visibility::VISIBLE, GetVisibility());

  primary_frame_tree_.ForEachRenderViewHost(
      [&frozen](RenderViewHostImpl* rvh) { rvh->SetIsFrozen(frozen); });
}

std::unique_ptr<WebContents> WebContentsImpl::Clone() {
  TRACE_EVENT0("content", "WebContentsImpl::Clone");

  // We use our current SiteInstance since the cloned entry will use it anyway.
  // We pass our own opener so that the cloned page can access it if it was set
  // before.
  CreateParams create_params(GetBrowserContext(), GetSiteInstance());
  FrameTreeNode* opener = primary_frame_tree_.root()->opener();
  RenderFrameHostImpl* opener_rfh = nullptr;
  if (opener) {
    opener_rfh = opener->current_frame_host();
  }
  std::unique_ptr<WebContentsImpl> tc =
      CreateWithOpener(create_params, opener_rfh);
  tc->GetController().CopyStateFrom(&primary_frame_tree_.controller(), true);
  observers_.NotifyObservers(&WebContentsObserver::DidCloneToNewWebContents,
                             this, tc.get());
  return tc;
}

void WebContentsImpl::Init(const WebContents::CreateParams& params,
                           blink::FramePolicy primary_main_frame_policy) {
  TRACE_EVENT0("content", "WebContentsImpl::Init");

  is_in_preview_mode_ = params.preview_mode;
  creator_location_ = params.creator_location;
#if BUILDFLAG(IS_ANDROID)
  java_creator_location_ = params.java_creator_location;
#endif  // BUILDFLAG(IS_ANDROID)

  if (params.picture_in_picture_options.has_value()) {
    picture_in_picture_options_ = params.picture_in_picture_options;
    if (GetOpener()) {
      picture_in_picture_opener_ =
          FromRenderFrameHostImpl(GetOpener())->GetWeakPtr();
    }
  }

  // This is set before initializing the render manager since
  // RenderFrameHostManager::Init calls back into us via its delegate to ask if
  // it should be hidden.
  visibility_ =
      params.initially_hidden ? Visibility::HIDDEN : Visibility::VISIBLE;
  GetController().SetActive(visibility_ == Visibility::VISIBLE);

  enable_wake_locks_ = params.enable_wake_locks;

  if (!params.last_active_time_ticks.is_null()) {
    last_active_time_ticks_ = params.last_active_time_ticks;
  }
  if (!params.last_active_time.is_null()) {
    last_active_time_ = params.last_active_time;
  }

  scoped_refptr<SiteInstanceImpl> site_instance =
      static_cast<SiteInstanceImpl*>(params.site_instance.get());
  if (!site_instance) {
    site_instance = SiteInstanceImpl::Create(params.browser_context);
  }
  if (params.desired_renderer_state == CreateParams::kNoRendererProcess) {
    site_instance->PreventAssociationWithSpareProcess();
  }

  // Iniitalize the primary FrameTree.
  // Note that GetOpener() is used here to get the opener for origin
  // inheritance, instead of other similar functions:
  // - GetOriginalOpener(), which would always return the main frame of the
  // opener, which might be different from the actual opener.
  // - FindOpenerRFH(), which will still return the opener frame if the
  // opener is suppressed (e.g. due to 'noopener'). The opener should not
  // be used for origin inheritance purposes in those cases, so this should
  // not pass the opener for those cases.
  primary_frame_tree_.Init(
      site_instance.get(), params.renderer_initiated_creation,
      params.main_frame_name, GetOpener(), primary_main_frame_policy,
      base::UnguessableToken::Create());

  std::unique_ptr<WebContentsViewDelegate> delegate =
      GetContentClient()->browser()->GetWebContentsViewDelegate(this);

  if (browser_plugin_guest_) {
    view_ = std::make_unique<WebContentsViewChildFrame>(
        this, std::move(delegate), &render_view_host_delegate_view_);
  } else {
    view_ = CreateWebContentsView(this, std::move(delegate),
                                  &render_view_host_delegate_view_);
  }
  CHECK(render_view_host_delegate_view_);
  CHECK(view_.get());

  view_->CreateView(params.context);

  screen_orientation_provider_ =
      std::make_unique<ScreenOrientationProvider>(this);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  DateTimeChooser::CreateDateTimeChooser(this);
#endif

  // AttributionHost must be created after `view_->CreateView()` is called as it
  // may invoke `WebContentsAndroid::AddObserver()`.
  if (base::FeatureList::IsEnabled(
          attribution_reporting::features::kConversionMeasurement)) {
    AttributionHost::CreateForWebContents(this);
  }

  // BrowserPluginGuest::Init needs to be called after this WebContents has
  // a RenderWidgetHostViewChildFrame. That is, |view_->CreateView| above.
  if (browser_plugin_guest_) {
    browser_plugin_guest_->Init();
  }

  g_created_callbacks.Get().Notify(this);

  // Create the renderer process in advance if requested.
  if (params.desired_renderer_state ==
      CreateParams::kInitializeAndWarmupRendererProcess) {
    if (!GetRenderManager()->current_frame_host()->IsRenderFrameLive()) {
      GetRenderManager()->InitRenderView(site_instance->group(),
                                         GetRenderViewHost(), nullptr);
    }
  }

  // Let the embedder know about the newly created and initialized WebContents.
  GetContentClient()->browser()->OnWebContentsCreated(this);

  // Ensure that observers are notified of the creation of this WebContents's
  // main RenderFrameHost. It must be done here for main frames, since the
  // NotifySwappedFromRenderManager expects view_ to already be created and that
  // happens after RenderFrameHostManager::Init.
  NotifySwappedFromRenderManager(nullptr,
                                 GetRenderManager()->current_frame_host());

  // Checks whether the associated ssl_manager has any certificate error or HTTP
  // exceptions for any host and updates the renderer preferences.
  if (base::FeatureList::IsEnabled(
          features::kReduceSubresourceResponseStartedIPC) &&
      GetController().ssl_manager()->HasAllowExceptionForAnyHost()) {
    renderer_preferences_.send_subresource_notification = true;
    SyncRendererPrefs();
  }
}

void WebContentsImpl::OnWebContentsDestroyed(WebContentsImpl* web_contents) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::OnWebContentsDestroyed");

  RemoveWebContentsDestructionObserver(web_contents);

  // Clear a pending contents that has been closed before being shown.
  for (auto iter = pending_contents_.begin(); iter != pending_contents_.end();
       ++iter) {
    if (iter->second.contents.get() != web_contents) {
      continue;
    }

    // Someone else has deleted the WebContents. That should never happen!
    // TODO(erikchen): Fix semantics here. https://crbug.com/832879.
    iter->second.contents.release();
    pending_contents_.erase(iter);
    return;
  }
  NOTREACHED_IN_MIGRATION();
}

void WebContentsImpl::OnRenderWidgetHostDestroyed(
    RenderWidgetHost* render_widget_host) {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::OnRenderWidgetHostDestroyed");

  RemoveRenderWidgetHostDestructionObserver(render_widget_host);

  // Clear a pending widget that has been closed before being shown.
  size_t num_erased =
      std::erase_if(pending_widgets_, [render_widget_host](const auto& pair) {
        return pair.second == render_widget_host;
      });
  DCHECK_EQ(1u, num_erased);
}

void WebContentsImpl::AddWebContentsDestructionObserver(
    WebContentsImpl* web_contents) {
  OPTIONAL_TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("content.verbose"),
                        "WebContentsImpl::AddWebContentsDestructionObserver");

  if (!base::Contains(web_contents_destruction_observers_, web_contents)) {
    web_contents_destruction_observers_[web_contents] =
        std::make_unique<WebContentsDestructionObserver>(this, web_contents);
  }
}

void WebContentsImpl::RemoveWebContentsDestructionObserver(
    WebContentsImpl* web_contents) {
  OPTIONAL_TRACE_EVENT0(
      TRACE_DISABLED_BY_DEFAULT("content.verbose"),
      "WebContentsImpl::RemoveWebContentsDestructionObserver");
  web_contents_destruction_observers_.erase(web_contents);
}

void WebContentsImpl::AddRenderWidgetHostDestructionObserver(
    RenderWidgetHost* render_widget_host) {
  OPTIONAL_TRACE_EVENT0(
      TRACE_DISABLED_BY_DEFAULT("content.verbose"),
      "WebContentsImpl::AddRenderWidgetHostDestructionObserver");

  DCHECK(!base::Contains(render_widget_host_destruction_observers_,
                         render_widget_host));

  render_widget_host_destruction_observers_.insert(
      {render_widget_host,
       std::make_unique<RenderWidgetHostDestructionObserver>(
           this, render_widget_host)});
}

void WebContentsImpl::RemoveRenderWidgetHostDestructionObserver(
    RenderWidgetHost* render_widget_host) {
  OPTIONAL_TRACE_EVENT0(
      TRACE_DISABLED_BY_DEFAULT("content.verbose"),
      "WebContentsImpl::RemoveRenderWidgetHostDestructionObserver");
  render_widget_host_destruction_observers_.erase(render_widget_host);
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

std::set<RenderWidgetHostViewBase*>
WebContentsImpl::GetRenderWidgetHostViewsInWebContentsTree() {
  std::set<RenderWidgetHostViewBase*> result;
  GetPrimaryMainFrame()->ForEachRenderFrameHost([&result](
                                                    RenderFrameHostImpl* rfh) {
    if (auto* view = static_cast<RenderWidgetHostViewBase*>(rfh->GetView())) {
      result.insert(view);
    }
  });
  return result;
}

void WebContentsImpl::Activate() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::Activate");
  if (delegate_) {
    delegate_->ActivateContents(this);
  }
}

void WebContentsImpl::SetTopControlsShownRatio(
    RenderWidgetHostImpl* render_widget_host,
    float ratio) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::SetTopControlsShownRatio",
                        "render_widget_host", render_widget_host, "ratio",
                        ratio);
  if (!delegate_) {
    return;
  }

  RenderFrameHostImpl* rfh = GetPrimaryMainFrame();
  if (!rfh || render_widget_host != rfh->GetRenderWidgetHost()) {
    return;
  }

  delegate_->SetTopControlsShownRatio(this, ratio);
}

void WebContentsImpl::SetTopControlsGestureScrollInProgress(bool in_progress) {
  OPTIONAL_TRACE_EVENT1(
      "content", "WebContentsImpl::SetTopControlsGestureScrollInProgress",
      "in_progress", in_progress);
  if (delegate_) {
    delegate_->SetTopControlsGestureScrollInProgress(in_progress);
  }
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
  // Note that IsBeingDestroyed() can return true at this point as
  // ~WebContentsImpl() calls RFHM::ClearRFHsPendingShutdown(), which might lead
  // us here.
  created_widgets_.erase(render_widget_host);

  if (IsBeingDestroyed()) {
    return;
  }

  if (render_widget_host == pointer_lock_widget_) {
    LostPointerLock(pointer_lock_widget_);
  }

  CancelKeyboardLock(render_widget_host);
}

void WebContentsImpl::RenderWidgetWasResized(
    RenderWidgetHostImpl* render_widget_host,
    bool width_changed) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::RenderWidgetWasResized",
                        "render_widget_host", render_widget_host,
                        "width_changed", width_changed);
  RenderFrameHostImpl* rfh = GetPrimaryMainFrame();
  if (!rfh || render_widget_host != rfh->GetRenderWidgetHost()) {
    return;
  }

  observers_.NotifyObservers(&WebContentsObserver::PrimaryMainFrameWasResized,
                             width_changed);
}

KeyboardEventProcessingResult WebContentsImpl::PreHandleKeyboardEvent(
    const input::NativeWebKeyboardEvent& event) {
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
        outermost->GetController().CanGoBack()) {
      outermost->GetController().GoBack();
      return true;
    } else if (event.button == blink::WebPointerProperties::Button::kForward &&
               outermost->GetController().CanGoForward()) {
      outermost->GetController().GoForward();
      return true;
    }
  }
  return false;
}

bool WebContentsImpl::HandleKeyboardEvent(
    const input::NativeWebKeyboardEvent& event) {
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
#if !BUILDFLAG(IS_MAC)
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

input::RenderWidgetHostInputEventRouter*
WebContentsImpl::GetInputEventRouter() {
  if (!IsBeingDestroyed()) {
    if (GetOuterWebContents()) {
      return GetOuterWebContents()->GetInputEventRouter();
    }

    if (!rwh_input_event_router_.get()) {
      rwh_input_event_router_ =
          MakeRefCounted<input::RenderWidgetHostInputEventRouter>(
              GetHostFrameSinkManager(), this);
    }
  }
  return rwh_input_event_router_.get();
}

void WebContentsImpl::GetRenderWidgetHostAtPointAsynchronously(
    RenderWidgetHostViewBase* root_view,
    const gfx::PointF& point,
    base::OnceCallback<void(base::WeakPtr<RenderWidgetHostViewBase>,
                            std::optional<gfx::PointF>)> callback) {
  GetInputEventRouter()->GetRenderWidgetHostAtPointAsynchronously(
      root_view, point, base::BindOnce(&RunCallback, std::move(callback)));
}

std::vector<RenderWidgetHostView*>
WebContentsImpl::GetRenderWidgetHostViewsForTests() {
  auto input_hosts =
      GetInputEventRouter()->GetRenderWidgetHostViewInputsForTests();
  std::vector<RenderWidgetHostView*> hosts;
  for (auto host : input_hosts) {
    hosts.push_back(static_cast<RenderWidgetHostViewBase*>(host));
  }
  return hosts;
}

RenderWidgetHostImpl* WebContentsImpl::GetFocusedRenderWidgetHost(
    RenderWidgetHostImpl* receiving_widget) {
  // Events for widgets other than the main frame (e.g., popup menus) should be
  // forwarded directly to the widget they arrived on.
  if (receiving_widget != GetPrimaryMainFrame()->GetRenderWidgetHost()) {
    return receiving_widget;
  }

  // If the focused WebContents is a guest WebContents, then get the focused
  // frame in the embedder WebContents instead.
  FrameTreeNode* focused_frame = GetFocusedFrameTree()->GetFocusedFrame();

  if (!focused_frame) {
    return receiving_widget;
  }

  // The view may be null if a subframe's renderer process has crashed while
  // the subframe has focus.  Drop the event in that case.  Do not give
  // it to the main frame, so that the user doesn't unexpectedly type into the
  // wrong frame if a focused subframe renderer crashes while they type.
  RenderWidgetHostView* view = focused_frame->current_frame_host()->GetView();
  if (!view) {
    return nullptr;
  }

  return RenderWidgetHostImpl::From(view->GetRenderWidgetHost());
}

RenderWidgetHostImpl* WebContentsImpl::GetRenderWidgetHostWithPageFocus() {
  FrameTree* focused_frame_tree = GetFocusedFrameTree();
  return focused_frame_tree->root()
      ->current_frame_host()
      ->GetRenderWidgetHost();
}

bool WebContentsImpl::CanEnterFullscreenMode(
    RenderFrameHostImpl* requesting_frame) {
  // It's possible that this WebContents was spawned while blocking UI was on
  // the screen, or that it was downstream from a WebContents when UI was
  // blocked. Therefore, disqualify it from fullscreen if it or any upstream
  // WebContents has an active blocker.
  return delegate_ &&
         base::ranges::all_of(GetAllOpeningWebContents(this),
                              [](auto* opener) {
                                return opener->fullscreen_blocker_count_ == 0;
                              }) &&
         delegate_->CanEnterFullscreenModeForTab(requesting_frame);
}

void WebContentsImpl::EnterFullscreenMode(
    RenderFrameHostImpl* requesting_frame,
    const blink::mojom::FullscreenOptions& options) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::EnterFullscreenMode");
  DCHECK(CanEnterFullscreenMode(requesting_frame));
  DCHECK(requesting_frame->IsActive());
  DCHECK(ContainsOrIsFocusedWebContents());
  if (base::FeatureList::IsEnabled(
          features::kAutomaticFullscreenContentSetting)) {
    // Ensure the window is made active to take input focus. The user may have
    // activated another window between making a gesture and the site handling
    // that gesture to request fullscreen. The experimental automatic fullscreen
    // feature also enables allowlisted sites to request fullscreen without any
    // gesture, even if the window was inactive. Note: requests from inactive
    // tabs of multi-tab windows should be rejected before reaching this code.
    Activate();
  }

  // When WebView is the `delegate_` we can end up with VisualProperties changes
  // synchronously. Notify the view ahead so it can handle the transition.
  if (auto* view = GetRenderWidgetHostView()) {
    static_cast<RenderWidgetHostViewBase*>(view)->EnterFullscreenMode(options);
  }

  if (delegate_) {
    delegate_->EnterFullscreenModeForTab(requesting_frame, options);

    if (keyboard_lock_widget_) {
      delegate_->RequestKeyboardLock(this, esc_key_locked_);
    }
  }

  observers_.NotifyObservers(
      &WebContentsObserver::DidToggleFullscreenModeForTab, IsFullscreen(),
      false);
  FullscreenContentsSet(GetBrowserContext())->insert(this);
}

void WebContentsImpl::ExitFullscreenMode(bool will_cause_resize) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::ExitFullscreenMode",
                        "will_cause_resize", will_cause_resize);
  // When WebView is the `delegate_` we can end up with VisualProperties changes
  // synchronously. Notify the view ahead so it can handle the transition.
  if (auto* view = GetRenderWidgetHostView()) {
    static_cast<RenderWidgetHostViewBase*>(view)->ExitFullscreenMode();
  }

  GetFullscreenUserData(GetBrowserContext())
      ->last_exits()
      ->insert_or_assign(GetPrimaryMainFrame()->GetLastCommittedOrigin(),
                         base::TimeTicks::Now());

  if (delegate_) {
    // This may spin the message loop and destroy this object crbug.com/1506535
    base::WeakPtr<WebContentsImpl> weak_ptr = weak_factory_.GetWeakPtr();
    delegate_->ExitFullscreenModeForTab(this);
    if (!weak_ptr) {
      return;
    }

    if (keyboard_lock_widget_) {
      delegate_->CancelKeyboardLockRequest(this);
    }
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
      if (RenderWidgetHost* render_widget_host = rwhv->GetRenderWidgetHost()) {
        render_widget_host->SynchronizeVisualProperties();
      }
    }
  }

  current_fullscreen_frame_id_ = GlobalRenderFrameHostId();

  observers_.NotifyObservers(
      &WebContentsObserver::DidToggleFullscreenModeForTab, IsFullscreen(),
      will_cause_resize);

  if (safe_area_insets_host_) {
    safe_area_insets_host_->DidExitFullscreen();
  }

  FullscreenContentsSet(GetBrowserContext())->erase(this);
}

void WebContentsImpl::FullscreenStateChanged(
    RenderFrameHostImpl* rfh,
    bool is_fullscreen,
    blink::mojom::FullscreenOptionsPtr options) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::FullscreenStateChanged",
                        "render_frame_host", rfh, "is_fullscreen",
                        is_fullscreen);

  GetView()->FullscreenStateChanged(is_fullscreen);

  if (is_fullscreen) {
    if (options.is_null()) {
      ReceivedBadMessage(rfh->GetProcess(),
                         bad_message::WCI_INVALID_FULLSCREEN_OPTIONS);
      return;
    }

    if (delegate_) {
      delegate_->FullscreenStateChangedForTab(rfh, *options);
    }

    if (!base::Contains(fullscreen_frames_, rfh)) {
      fullscreen_frames_.insert(rfh);
      FullscreenFrameSetUpdated();
    }
    return;
  }

  // If |rfh| is no longer in fullscreen, remove it and any descendants.
  // See https://fullscreen.spec.whatwg.org.
  size_t size_before_deletion = fullscreen_frames_.size();
  std::erase_if(fullscreen_frames_, [&](RenderFrameHostImpl* current) {
    while (current) {
      if (current == rfh) {
        return true;
      }
      // We only look for direct parents. fencedframes will not enter
      // fullscreen.
      current = current->GetParent();
    }
    return false;
  });

  if (size_before_deletion != fullscreen_frames_.size()) {
    FullscreenFrameSetUpdated();
  }
}

bool WebContentsImpl::CanUseWindowingControls(
    RenderFrameHostImpl* requesting_frame) {
  return GetDelegate() &&
         GetDelegate()->CanUseWindowingControls(requesting_frame);
}

void WebContentsImpl::Maximize() {
  if (!GetDelegate()) {
    return;
  }
  GetDelegate()->MaximizeFromWebAPI();
}

void WebContentsImpl::Minimize() {
  if (!GetDelegate()) {
    return;
  }
  GetDelegate()->MinimizeFromWebAPI();
}

void WebContentsImpl::Restore() {
  if (!GetDelegate()) {
    return;
  }
  GetDelegate()->RestoreFromWebAPI();
}

// TODO(laurila, crbug.com/1466855): Map into new `ui::DisplayState` enum
// instead of `ui::mojom::WindowShowState`.
ui::mojom::WindowShowState WebContentsImpl::GetWindowShowState() {
  return GetDelegate() ? GetDelegate()->GetWindowShowState()
                       : ui::mojom::WindowShowState::kDefault;
}

blink::mojom::DevicePostureProvider*
WebContentsImpl::GetDevicePostureProvider() {
  return DevicePostureProviderImpl::GetOrCreate(this);
}

bool WebContentsImpl::GetResizable() {
  return GetDelegate() && GetDelegate()->GetCanResize();
}

void WebContentsImpl::FullscreenFrameSetUpdated() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::FullscreenFrameSetUpdated");
  if (fullscreen_frames_.empty()) {
    current_fullscreen_frame_id_ = GlobalRenderFrameHostId();
    return;
  }

  // Find the current fullscreen frame and call the observers.
  // If frame A is fullscreen, then frame B goes into inner fullscreen, then B
  // exits fullscreen - that will result in A being fullscreen.
  RenderFrameHostImpl* new_fullscreen_frame = *std::max_element(
      fullscreen_frames_.begin(), fullscreen_frames_.end(), FrameCompareDepth);

  // If we have already notified observers about this frame then we should not
  // fire the observers again.
  if (new_fullscreen_frame->GetGlobalId() == current_fullscreen_frame_id_) {
    return;
  }
  current_fullscreen_frame_id_ = new_fullscreen_frame->GetGlobalId();

  observers_.NotifyObservers(&WebContentsObserver::DidAcquireFullscreen,
                             new_fullscreen_frame);
  if (safe_area_insets_host_) {
    safe_area_insets_host_->DidAcquireFullscreen(new_fullscreen_frame);
  }
}

PageVisibilityState WebContentsImpl::CalculatePageVisibilityState(
    Visibility visibility) const {
  // Only hide the page if there are no entities capturing screenshots
  // or video (e.g. mirroring or WebXR). If there are, apply the correct state
  // of kHidden or kHiddenButPainting.
  bool web_contents_visible_in_vr = false;
#if BUILDFLAG(ENABLE_VR)
  web_contents_visible_in_vr =
      XRRuntimeManagerImpl::GetImmersiveSessionWebContents() == this;
#endif

  // If there are entities in Picture-in-Picture mode, don't activate the
  // "disable rendering" optimization, since it still needs to drive the video,
  // and possibly other elements on the page like canvas, to keep the picture in
  // picture window up to date.
  if (visibility == Visibility::VISIBLE || visible_capturer_count_ > 0 ||
      web_contents_visible_in_vr) {
    return PageVisibilityState::kVisible;
  } else if (hidden_capturer_count_ > 0 || has_picture_in_picture_video_ ||
             has_picture_in_picture_document_) {
    return PageVisibilityState::kHiddenButPainting;
  }
  return PageVisibilityState::kHidden;
}

PageVisibilityState WebContentsImpl::GetPageVisibilityState() const {
  return CalculatePageVisibilityState(visibility_);
}

void WebContentsImpl::UpdateVisibilityAndNotifyPageAndView(
    Visibility new_visibility,
    bool is_activity) {
  DCHECK(!IsBeingDestroyed());

  PageVisibilityState page_visibility =
      CalculatePageVisibilityState(new_visibility);

  // A crashed frame might be covered by a sad tab. See docs on SadTabHelper
  // exactly when it is or isn't. Either way, don't make it visible.
  bool view_is_visible =
      !IsCrashed() && page_visibility != PageVisibilityState::kHidden;

  // Prerendering relies on overriding FrameTree::Delegate::IsHidden,
  // while for other frame trees FrameTree::Delegate::IsHidden
  // resolves to WebContents' visibility, so we avoid Prerender RennderViewHosts
  // here.
  ForEachRenderViewHostTypes view_mask =
      static_cast<ForEachRenderViewHostTypes>(
          ForEachRenderViewHostTypes::kBackForwardCacheViews |
          ForEachRenderViewHostTypes::kActiveViews);

  RenderViewHostIterationCallback update_frame_tree_visibility =
      base::BindRepeating(
          [](PageVisibilityState page_visibility,
             RenderViewHostImpl* render_view_host) {
            render_view_host->SetFrameTreeVisibility(page_visibility);
          },
          page_visibility);

  if (page_visibility == PageVisibilityState::kHidden) {
    GetController().SetActive(false);
  } else {
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
    GetController().SetActive(true);

    // This shows the Page before showing the individual RenderWidgets, as
    // RenderWidgets will work to produce compositor frames and handle input
    // as soon as they are shown. But the Page and other classes do not expect
    // to be producing frames when the Page is hidden. So we make sure the Page
    // is shown first.
    ForEachRenderViewHost(view_mask, update_frame_tree_visibility);
  }

  // |GetRenderWidgetHostView()| can be null if the user middle clicks a link to
  // open a tab in the background, then closes the tab before selecting it.
  // This is because closing the tab calls WebContentsImpl::Destroy(), which
  // removes the |GetRenderViewHost()|; then when we actually destroy the
  // window, OnWindowPosChanged() notices and calls WasHidden() (which
  // calls us).
  if (auto* view = GetRenderWidgetHostView()) {
    if (view_is_visible) {
      static_cast<RenderWidgetHostViewBase*>(view)->ShowWithVisibility(
          page_visibility);
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
    if (is_activity) {
      last_active_time_ticks_ = base::TimeTicks::Now();
      last_active_time_ = base::Time::Now();
    }
    SetVisibilityAndNotifyObservers(new_visibility);
  }

  if (page_visibility == PageVisibilityState::kHidden) {
    // Similar to when showing the page, we only hide the page after
    // hiding the individual RenderWidgets.
    ForEachRenderViewHost(view_mask, update_frame_tree_visibility);
  } else {
    for (FrameTreeNode* node :
         FrameTree::SubtreeAndInnerTreeNodes(GetPrimaryMainFrame())) {
      RenderFrameProxyHost* proxy_to_parent_or_outer_delegate =
          node->render_manager()->GetProxyToParentOrOuterDelegate();
      if (!proxy_to_parent_or_outer_delegate) {
        continue;
      }

      // DelegateWasShown keeps track of crash metrics. This is safe to
      // call for GuestViews, and inner frame trees.
      proxy_to_parent_or_outer_delegate->cross_process_frame_connector()
          ->DelegateWasShown();
    }
  }

  if (new_visibility != Visibility::VISIBLE) {
    SetVisibilityAndNotifyObservers(new_visibility);
  }

  if (base::FeatureList::IsEnabled(kUpdateInnerWebContentsVisibility)) {
    // Inner WebContents are skipped in ForEachRenderViewHost() above, which
    // causes inner WebContents to not be notified of visibility changes.
    //
    // Note: An inner WebContents that is hidden within the embedder could
    // spuriously be set to visible (e.g. if its parent is display:none), but
    // this is ignored here for now.
    for (WebContents* inner : GetInnerWebContents()) {
      static_cast<WebContentsImpl*>(inner)
          ->UpdateVisibilityAndNotifyPageAndView(new_visibility, is_activity);
    }
  }
}

#if BUILDFLAG(IS_ANDROID)
void WebContentsImpl::UpdateUserGestureCarryoverInfo() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::UpdateUserGestureCarryoverInfo");
  if (delegate_) {
    delegate_->UpdateUserGestureCarryoverInfo(this);
  }
}
#endif

bool WebContentsImpl::IsFullscreen() {
  return delegate_ && delegate_->IsFullscreenForTabOrPending(this);
}

bool WebContentsImpl::ShouldShowStaleContentOnEviction() {
  return GetDelegate() && GetDelegate()->ShouldShowStaleContentOnEviction(this);
}

blink::mojom::DisplayMode WebContentsImpl::GetDisplayMode() const {
  return delegate_ ? delegate_->GetDisplayMode(this)
                   : blink::mojom::DisplayMode::kBrowser;
}

void WebContentsImpl::RequestToLockPointer(
    RenderWidgetHostImpl* render_widget_host,
    bool user_gesture,
    bool last_unlocked_by_target,
    bool privileged) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::RequestPointerLock",
                        "render_widget_host", render_widget_host, "privileged",
                        privileged);
  if (render_widget_host->frame_tree()->is_fenced_frame()) {
    // The renderer should have checked and disallowed the request for fenced
    // frames in PointerLockController and dispatched pointerlockerror. Ignore
    // the request and mark it as bad if it didn't happen for some reason.
    ReceivedBadMessage(render_widget_host->GetProcess(),
                       bad_message::WCI_REQUEST_LOCK_MOUSE_FENCED_FRAME);
    return;
  }
  for (WebContentsImpl* current = this; current;
       current = current->GetOuterWebContents()) {
    if (current->pointer_lock_widget_) {
      render_widget_host->GotResponseToPointerLockRequest(
          blink::mojom::PointerLockResult::kAlreadyLocked);
      return;
    }
  }

  if (privileged) {
    DCHECK(!GetOuterWebContents());
    pointer_lock_widget_ = render_widget_host;
    render_widget_host->GotResponseToPointerLockRequest(
        blink::mojom::PointerLockResult::kSuccess);
    return;
  }

  bool widget_in_frame_tree = false;
  for (FrameTreeNode* node : primary_frame_tree_.Nodes()) {
    if (node->current_frame_host()->GetRenderWidgetHost() ==
        render_widget_host) {
      widget_in_frame_tree = true;
      break;
    }
  }

  if (widget_in_frame_tree && delegate_) {
    for (WebContentsImpl* current = this; current;
         current = current->GetOuterWebContents()) {
      current->pointer_lock_widget_ = render_widget_host;
    }
    observers_.NotifyObservers(&WebContentsObserver::PointerLockRequested);
    delegate_->RequestPointerLock(this, user_gesture, last_unlocked_by_target);
  } else {
    render_widget_host->GotResponseToPointerLockRequest(
        blink::mojom::PointerLockResult::kWrongDocument);
  }
}

bool WebContentsImpl::IsWaitingForPointerLockPrompt(
    RenderWidgetHostImpl* render_widget_host) {
  if (!delegate_ || (pointer_lock_widget_ != render_widget_host)) {
    return false;
  }
  return delegate_->IsWaitingForPointerLockPrompt(this);
}

void WebContentsImpl::LostPointerLock(
    RenderWidgetHostImpl* render_widget_host) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::LostPointerLock",
                        "render_widget_host", render_widget_host);
  CHECK(pointer_lock_widget_);

  if (WebContentsImpl::FromRenderWidgetHostImpl(pointer_lock_widget_) != this) {
    return pointer_lock_widget_->delegate()->LostPointerLock(
        render_widget_host);
  }

  pointer_lock_widget_->SendPointerLockLost();
  for (WebContentsImpl* current = this; current;
       current = current->GetOuterWebContents()) {
    current->pointer_lock_widget_ = nullptr;
  }

  if (delegate_) {
    delegate_->LostPointerLock();
  }
}

bool WebContentsImpl::HasPointerLock(RenderWidgetHostImpl* render_widget_host) {
  // To verify if the mouse is locked, the mouse_lock_widget_ needs to be
  // assigned to the widget that requested the mouse lock, and the top-level
  // platform RenderWidgetHostView needs to hold the mouse lock from the OS.
  auto* widget_host = GetTopLevelRenderWidgetHostView();
  return pointer_lock_widget_ == render_widget_host && widget_host &&
         widget_host->IsPointerLocked();
}

RenderWidgetHostImpl* WebContentsImpl::GetPointerLockWidget() {
  auto* widget_host = GetTopLevelRenderWidgetHostView();
  if (widget_host && widget_host->IsPointerLocked()) {
    return pointer_lock_widget_;
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
  if (WebContentsImpl::FromRenderWidgetHostImpl(render_widget_host) != this) {
    NOTREACHED_IN_MIGRATION();
    return false;
  }

  // KeyboardLock is only supported when called by the top-level browsing
  // context and is not supported in embedded content scenarios.
  if (GetOuterWebContents()) {
    render_widget_host->GotResponseToKeyboardLockRequest(false);
    return false;
  }

  esc_key_locked_ = esc_key_locked;
  keyboard_lock_widget_ = render_widget_host;

  if (delegate_) {
    observers_.NotifyObservers(&WebContentsObserver::KeyboardLockRequested);
    delegate_->RequestKeyboardLock(this, esc_key_locked_);
  }
  return true;
}

void WebContentsImpl::CancelKeyboardLock(
    RenderWidgetHostImpl* render_widget_host) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::CancelKeyboardLockRequest",
                        "render_widget_host", render_widget_host);
  if (!keyboard_lock_widget_ || render_widget_host != keyboard_lock_widget_) {
    return;
  }

  RenderWidgetHostImpl* old_keyboard_lock_widget = keyboard_lock_widget_;
  keyboard_lock_widget_ = nullptr;

  if (delegate_) {
    delegate_->CancelKeyboardLockRequest(this);
  }

  old_keyboard_lock_widget->CancelKeyboardLock();
}

RenderWidgetHostImpl* WebContentsImpl::GetKeyboardLockWidget() {
  return keyboard_lock_widget_;
}

bool WebContentsImpl::OnRenderFrameProxyVisibilityChanged(
    RenderFrameProxyHost* render_frame_proxy_host,
    blink::mojom::FrameVisibility visibility) {
  OPTIONAL_TRACE_EVENT1("content",
                        "WebContentsImpl::OnRenderFrameProxyVisibilityChanged",
                        "visibility", static_cast<int>(visibility));

  // Check that we are responsible for the RenderFrameProxyHost by checking that
  // its FrameTreeNode matches our `primary_frame_tree_` root. Otherwise this is
  // not a visibility change that affects all frames in an inner WebContents.
  if (render_frame_proxy_host->frame_tree_node() !=
      primary_frame_tree_.root()) {
    return false;
  }

  DCHECK(GetOuterWebContents());

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
  return true;
}

FrameTree* WebContentsImpl::CreateNewWindow(
    RenderFrameHostImpl* opener,
    const mojom::CreateNewWindowParams& params,
    bool is_new_browsing_instance,
    bool has_user_gesture,
    SessionStorageNamespace* session_storage_namespace) {
  TRACE_EVENT2("browser,content,navigation", "WebContentsImpl::CreateNewWindow",
               "opener", opener, "params", params);
  DCHECK(opener);

  if (active_file_chooser_) {
    // Do not allow opening a new window or tab while a file select is active
    // file chooser to avoid user confusion over which tab triggered the file
    // chooser.
    opener->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kWarning,
        "window.open blocked due to active file chooser.");
    return nullptr;
  }

  int render_process_id = opener->GetProcess()->GetID();
  SiteInstanceImpl* source_site_instance = opener->GetSiteInstance();
  const auto& partition_config =
      source_site_instance->GetStoragePartitionConfig();

  {
    StoragePartition* partition =
        GetBrowserContext()->GetStoragePartition(source_site_instance);
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
    auto* web_contents_impl =
        static_cast<WebContentsImpl*>(delegate_->CreateCustomWebContents(
            opener, source_site_instance, is_new_browsing_instance,
            opener->GetLastCommittedURL(), params.frame_name, params.target_url,
            partition_config, session_storage_namespace));
    if (!web_contents_impl) {
      return nullptr;
    }
    web_contents_impl->is_popup_ =
        params.disposition == WindowOpenDisposition::NEW_POPUP;
    SetPartitionedPopinOpenerOnNewWindowIfNeeded(web_contents_impl, params,
                                                 opener);
    return &web_contents_impl->GetPrimaryFrameTree();
  }

  bool renderer_started_hidden =
      params.disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB;

  // While some guest types do not have a guest SiteInstance, the ones that
  // don't all override WebContents creation above.
  CHECK_EQ(source_site_instance->IsGuest(), IsGuest());
  bool is_guest = IsGuest();

  // We usually create the new window in the same BrowsingInstance (group of
  // script-related windows), by passing in the current SiteInstance.  However,
  // if the opener is being suppressed, we need to ensure that the new
  // SiteInstance is created in a new BrowsingInstance.
  scoped_refptr<SiteInstance> site_instance;
  if (params.opener_suppressed) {
    if (is_guest) {
      // For guests, noopener windows can be created in a new BrowsingInstance
      // as long as they preserve the guest's StoragePartition.
      site_instance =
          SiteInstance::CreateForGuest(GetBrowserContext(), partition_config);
    } else {
      site_instance = SiteInstance::Create(GetBrowserContext());
    }
  } else {
    site_instance = source_site_instance;
  }

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
  // trying to open a new window, if the new window ends up in a different
  // browsing instance, then the RenderViewHost, RenderWidgetHost,
  // RenderFrameHost constellation is effectively browser initiated
  // the opener's process will not given the routing IDs for the new
  // objects.
  create_params.renderer_initiated_creation = !is_new_browsing_instance;

  if (params.pip_options) {
    create_params.picture_in_picture_options = *(params.pip_options);
  }

  // Check whether there is an available prerendered page for this navigation if
  // this is not for guest. If it exists, take WebContents pre-created for
  // hosting the prerendered page instead of creating new WebContents.
  // TODO(crbug.com/40234240): Instead of filtering out the guest case here,
  // check it and drop prerender requests before starting prerendering.
  std::unique_ptr<WebContentsImpl> new_contents;
  if (base::FeatureList::IsEnabled(blink::features::kPrerender2InNewTab) &&
      !is_guest) {
    new_contents =
        GetPrerenderHostRegistry()->TakePreCreatedWebContentsForNewTabIfExists(
            params, create_params);
    if (new_contents) {
      // The SiteInstance of the pre-created WebContents should be in a
      // different BrowsingInstance from the source SiteInstance, while they
      // should be in the same StoragePartition.
      SiteInstanceImpl* new_site_instance = new_contents->GetSiteInstance();
      DCHECK(!new_site_instance->IsRelatedSiteInstance(source_site_instance));
      DCHECK_EQ(new_site_instance->GetStoragePartitionConfig(),
                source_site_instance->GetStoragePartitionConfig());
    }
  }

  if (!new_contents) {
    if (!is_guest) {
      create_params.context = view_->GetNativeView();
      new_contents = WebContentsImpl::Create(create_params);
    } else {
      new_contents =
          GetBrowserPluginGuest()->CreateNewGuestWindow(create_params);
    }
    new_contents->GetController().SetSessionStorageNamespace(
        partition_config, session_storage_namespace);
  }

  auto* new_contents_impl = new_contents.get();
  new_contents_impl->is_popup_ =
      params.disposition == WindowOpenDisposition::NEW_POPUP;
  SetPartitionedPopinOpenerOnNewWindowIfNeeded(new_contents_impl, params,
                                               opener);

  // If the new frame has a name, make sure any SiteInstances that can find
  // this named frame have proxies for it.  Must be called after
  // SetSessionStorageNamespace, since this calls CreateRenderView, which uses
  // GetSessionStorageNamespace.
  if (!params.frame_name.empty()) {
    new_contents_impl->GetRenderManager()->CreateProxiesForNewNamedFrame(
        new_contents_impl->GetPrimaryMainFrame()->browsing_context_state());
  }

  // Save the window for later if we're not suppressing the opener (since it
  // will be shown immediately).
  if (!params.opener_suppressed) {
    if (!is_guest) {
      WebContentsView* new_view = new_contents_impl->view_.get();

      // TODO(brettw): It seems bogus that we have to call this function on the
      // newly created object and give it one of its own member variables.
      RenderWidgetHostView* widget_view = new_view->CreateViewForWidget(
          new_contents_impl->GetRenderViewHost()->GetWidget());
      view_->SetOverscrollControllerEnabled(CanOverscrollContent());
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
    int32_t main_frame_routing_id = new_contents_impl->GetPrimaryMainFrame()
                                        ->GetRenderWidgetHost()
                                        ->GetRoutingID();
    GlobalRoutingID id(render_process_id, main_frame_routing_id);
    pending_contents_[id] =
        CreatedWindow(std::move(new_contents), params.target_url);
    AddWebContentsDestructionObserver(new_contents_impl);
  }

  if (delegate_) {
    delegate_->WebContentsCreated(this, render_process_id,
                                  opener->GetRoutingID(), params.frame_name,
                                  params.target_url, new_contents_impl);
  }

  observers_.NotifyObservers(&WebContentsObserver::DidOpenRequestedURL,
                             new_contents_impl, opener, params.target_url,
                             params.referrer.To<Referrer>(), params.disposition,
                             ui::PAGE_TRANSITION_LINK,
                             false,  // started_from_context_menu
                             true);  // renderer_initiated

  if (!params.opener_suppressed) {
    return &new_contents_impl->GetPrimaryFrameTree();
  }

  // When the opener is suppressed, the original renderer cannot access the
  // new window.  As a result, we need to show and navigate the window here.
  bool was_blocked = false;
  base::WeakPtr<WebContentsImpl> weak_new_contents =
      new_contents_impl->weak_factory_.GetWeakPtr();
  WebContentsImpl* contents_to_load = new_contents_impl;
  if (delegate_) {
    WebContents* web_contents_navigated = delegate_->AddNewContents(
        this, std::move(new_contents), params.target_url, params.disposition,
        *params.features, has_user_gesture, &was_blocked);

    if (base::FeatureList::IsEnabled(features::kPwaNavigationCapturing)) {
      // The delegate may delete |new_contents_impl| during AddNewContents().
      // If that occurs and there isn't a replacement contents returned, exit.
      // Otherwise, use the replacement web contents that was navigated in.
      if (web_contents_navigated != nullptr && weak_new_contents) {
        CHECK(web_contents_navigated == weak_new_contents.get());
      }

      if (!weak_new_contents) {
        if (web_contents_navigated == nullptr) {
          return nullptr;
        }
        contents_to_load =
            static_cast<WebContentsImpl*>(web_contents_navigated);
      }
    } else if (!weak_new_contents) {
      return nullptr;
    }
  }

  if (!was_blocked) {
    std::unique_ptr<NavigationController::LoadURLParams> load_params =
        std::make_unique<NavigationController::LoadURLParams>(
            params.target_url);
    load_params->initiator_origin = opener->GetLastCommittedOrigin();
    load_params->initiator_process_id = opener->GetProcess()->GetID();
    load_params->initiator_frame_token = opener->GetFrameToken();
    // Avoiding setting |load_params->source_site_instance| when
    // |opener_suppressed| is true, because in that case we do not want to use
    // the old SiteInstance and/or BrowsingInstance.  See also the test here:
    // NewPopupCOOP_SameOriginPolicyAndCrossOriginIframeSetsNoopener.
    load_params->referrer = params.referrer.To<Referrer>();
    load_params->transition_type = ui::PAGE_TRANSITION_LINK;
    load_params->is_renderer_initiated = true;
    load_params->was_opener_suppressed = true;
    load_params->has_user_gesture = has_user_gesture;
    load_params->is_form_submission = params.is_form_submission;
    if (params.form_submission_post_data) {
      load_params->load_type = NavigationController::LOAD_TYPE_HTTP_POST;
      load_params->post_data = params.form_submission_post_data;
      load_params->post_content_type = params.form_submission_post_content_type;
    }
    load_params->impression = params.impression;
    load_params->override_user_agent =
        contents_to_load->should_override_user_agent_in_new_tabs_
            ? NavigationController::UA_OVERRIDE_TRUE
            : NavigationController::UA_OVERRIDE_FALSE;
    load_params->download_policy = params.download_policy;
    load_params->initiator_activation_and_ad_status =
        params.initiator_activation_and_ad_status;

    if (delegate_ && !is_guest &&
        !delegate_->ShouldResumeRequestsForCreatedWindow()) {
      // We are in asynchronous add new contents path, delay navigation.
      DCHECK(!contents_to_load->delayed_open_url_params_);
      contents_to_load->delayed_load_url_params_ = std::move(load_params);
    } else {
      contents_to_load->GetController().LoadURLWithParams(*load_params.get());
      if (!is_guest) {
        contents_to_load->Focus();
      }
    }
  }

  if (weak_new_contents) {
    return &new_contents_impl->GetPrimaryFrameTree();
  }
  return nullptr;
}

RenderWidgetHostImpl* WebContentsImpl::CreateNewPopupWidget(
    base::SafeRef<SiteInstanceGroup> site_instance_group,
    int32_t route_id,
    mojo::PendingAssociatedReceiver<blink::mojom::PopupWidgetHost>
        blink_popup_widget_host,
    mojo::PendingAssociatedReceiver<blink::mojom::WidgetHost> blink_widget_host,
    mojo::PendingAssociatedRemote<blink::mojom::Widget> blink_widget) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::CreateNewPopupWidget",
                        "route_id", route_id);

  if (visibility_ != Visibility::VISIBLE) {
    // Don't create popups for hidden tabs. http://crbug.com/1521345
    return nullptr;
  }

  RenderWidgetHostImpl* widget_host = RenderWidgetHostFactory::CreateSelfOwned(
      &primary_frame_tree_, this, site_instance_group, route_id, IsHidden());

  widget_host->BindWidgetInterfaces(std::move(blink_widget_host),
                                    std::move(blink_widget));
  widget_host->BindPopupWidgetInterface(std::move(blink_popup_widget_host));
  RenderWidgetHostViewBase* widget_view =
      static_cast<RenderWidgetHostViewBase*>(
          view_->CreateViewForChildWidget(widget_host));
  if (!widget_view) {
    return nullptr;
  }
  widget_view->SetWidgetType(WidgetType::kPopup);

  // Save the created widget associated with the route so we can show it later.
  pending_widgets_.insert(
      {GlobalRoutingID(site_instance_group->process()->GetID(), route_id),
       widget_host});
  AddRenderWidgetHostDestructionObserver(widget_host);

  return widget_host;
}

int64_t WebContentsImpl::AdjustWindowRect(gfx::Rect* bounds,
                                          RenderFrameHostImpl* opener) {
  // Auto-resize can override other mechanisms for enforcing min/max window size
  // for some modals and popups to fit the size of their contents. Borderless
  // apps shouldn't have overlap with auto-resize mode windows.
  if (!(GetRenderWidgetHostView() &&
        static_cast<RenderWidgetHostViewBase*>(GetRenderWidgetHostView())
            ->IsAutoResizeEnabled())) {
    // For borderless apps the minimum size is
    // `blink::kMinimumBorderlessWindowSize` instead of the default
    // `blink::kMinimumWindowSize`.
    int minimum_size =
        GetDisplayMode() == blink::mojom::DisplayMode::kBorderless &&
                IsWindowManagementGranted(opener)
            ? blink::kMinimumBorderlessWindowSize
            : blink::kMinimumWindowSize;
    AdjustWindowRectForMinimum(bounds, minimum_size);
  }

  int64_t display_id = display::kInvalidDisplayId;
  if (*bounds != gfx::Rect()) {
    display_id = AdjustWindowRectForDisplay(bounds, opener);
  }
  return display_id;
}

void WebContentsImpl::ShowCreatedWindow(
    RenderFrameHostImpl* opener,
    int main_frame_widget_route_id,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::ShowCreatedWindow",
                        "opener", opener, "main_frame_widget_route_id",
                        main_frame_widget_route_id);
  // This method is the renderer requesting an existing top level window to
  // show a new top level window that the renderer created. Each top level
  // window is associated with a WebContents. In this case it was created
  // earlier but showing it was deferred until the renderer requested for it
  // to be shown. We find that previously created WebContents here.
  // TODO(danakj): Why do we defer this show step until the renderer asks for it
  // when it will always do so. What needs to happen in the renderer before we
  // reach here?
  std::optional<CreatedWindow> owned_created = GetCreatedWindow(
      opener->GetProcess()->GetID(), main_frame_widget_route_id);

  // The browser may have rejected the request to make a new window, or the
  // renderer could be requesting to show a previously shown window (occurs when
  // mojom::CreateNewWindowStatus::kReuse is used). Ignore the request then.
  if (!owned_created || !owned_created->contents) {
    return;
  }

  if (base::FeatureList::IsEnabled(features::kWindowOpenFileSelectFix) &&
      active_file_chooser_) {
    // Do not allow opening a new window or tab while a file select is active
    // file chooser to avoid user confusion over which tab triggered the file
    // chooser.
    opener->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kWarning,
        "window.open blocked due to active file chooser.");
    return;
  }

  WebContentsImpl* created = owned_created->contents.get();

  // This uses the delegate for the WebContents where the window was created
  // from, to control how to show the newly created window.
  WebContentsDelegate* delegate = GetDelegate();

  // Individual members of |window_features.bounds| may be 0 to indicate that
  // the window.open() feature string did not specify a value. This code does
  // not distinguish between an unspecified value and 0.
  // Assume that if any single value is non-zero, all values should be used.
  // TODO(crbug.com/40092782): Utilize window_features.has_x and others.
  blink::mojom::WindowFeatures adjusted_features = window_features;
  int64_t display_id = AdjustWindowRect(&adjusted_features.bounds, opener);

  // Drop fullscreen when opening a WebContents to prohibit deceptive behavior.
  // Only drop fullscreen on the specific destination display, if it is known.
  // This supports sites using cross-screen window management capabilities to
  // retain fullscreen and open a window on another screen.
  ForSecurityDropFullscreen(display_id).RunAndReset();

  // The delegate can be null in tests, so we must check for it :(.
  if (delegate) {
    // Mark the web contents as pending resume, then immediately do
    // the resume if the delegate wants it.
    created->is_resume_pending_ = true;
    if (delegate->ShouldResumeRequestsForCreatedWindow()) {
      created->ResumeLoadingCreatedWebContents();
    }

    delegate->AddNewContents(this, std::move(owned_created->contents),
                             std::move(owned_created->target_url), disposition,
                             adjusted_features, user_gesture, nullptr);
  }
}

void WebContentsImpl::ShowCreatedWidget(int process_id,
                                        int widget_route_id,
                                        const gfx::Rect& initial_rect,
                                        const gfx::Rect& initial_anchor_rect) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::ShowCreatedWidget",
                        "process_id", process_id, "widget_route_id",
                        widget_route_id);
  RenderWidgetHostViewBase* widget_host_view =
      static_cast<RenderWidgetHostViewBase*>(
          GetCreatedWidget(process_id, widget_route_id));
  if (!widget_host_view) {
    return;
  }

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
  gfx::Rect transformed_anchor_rect(initial_anchor_rect);
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

    origin =
        this_view->TransformPointToRootCoordSpace(initial_anchor_rect.origin());
    bottom_right = this_view->TransformPointToRootCoordSpace(
        initial_anchor_rect.bottom_right());
    transformed_anchor_rect =
        gfx::Rect(origin.x(), origin.y(), bottom_right.x() - origin.x(),
                  bottom_right.y() - origin.y());
  }

  RenderWidgetHostImpl* render_widget_host_impl = widget_host_view->host();
  auto permission_exclusion_area_bounds =
      PermissionControllerImpl::FromBrowserContext(GetBrowserContext())
          ->GetExclusionAreaBoundsInScreen(outermost_web_contents);
  if (permission_exclusion_area_bounds &&
      permission_exclusion_area_bounds->Intersects(transformed_rect)) {
    render_widget_host_impl->ShutdownAndDestroyWidget(true);
    return;
  }

  widget_host_view->InitAsPopup(view, transformed_rect,
                                transformed_anchor_rect);

  // Renderer-owned popup widgets wait for the renderer to request for them
  // to be shown. We signal that this condition is satisfied by calling Init().
  render_widget_host_impl->Init();
}

std::optional<CreatedWindow> WebContentsImpl::GetCreatedWindow(
    int process_id,
    int main_frame_widget_route_id) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::GetCreatedWindow",
                        "process_id", process_id, "main_frame_widget_route_id",
                        main_frame_widget_route_id);

  auto key = GlobalRoutingID(process_id, main_frame_widget_route_id);
  auto iter = pending_contents_.find(key);

  // Certain systems can block the creation of new windows. If we didn't succeed
  // in creating one, just return NULL.
  if (iter == pending_contents_.end()) {
    return std::nullopt;
  }

  CreatedWindow result = std::move(iter->second);
  WebContentsImpl* new_contents = result.contents.get();
  pending_contents_.erase(key);
  RemoveWebContentsDestructionObserver(new_contents);

  // Don't initialize the guest WebContents immediately.
  if (new_contents->IsGuest()) {
    return result;
  }

  if (!new_contents->GetPrimaryMainFrame()
           ->GetProcess()
           ->IsInitializedAndNotDead() ||
      !new_contents->GetPrimaryMainFrame()->GetView()) {
    return std::nullopt;
  }

  return result;
}

RenderWidgetHostView* WebContentsImpl::GetCreatedWidget(int process_id,
                                                        int route_id) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::GetCreatedWidget",
                        "process_id", process_id, "route_id", route_id);

  auto iter = pending_widgets_.find(GlobalRoutingID(process_id, route_id));
  if (iter == pending_widgets_.end()) {
    DCHECK(false);
    return nullptr;
  }

  RenderWidgetHost* widget_host = iter->second;
  pending_widgets_.erase(GlobalRoutingID(process_id, route_id));
  RemoveRenderWidgetHostDestructionObserver(widget_host);

  if (!widget_host->GetProcess()->IsInitializedAndNotDead()) {
    // The view has gone away or the renderer crashed. Nothing to do.
    return nullptr;
  }

  return widget_host->GetView();
}

void WebContentsImpl::CreateMediaPlayerHostForRenderFrameHost(
    RenderFrameHostImpl* frame_host,
    mojo::PendingAssociatedReceiver<media::mojom::MediaPlayerHost> receiver) {
  media_web_contents_observer()->BindMediaPlayerHost(frame_host->GetGlobalId(),
                                                     std::move(receiver));
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
        blink::mojom::StreamDevicesSet(),
        blink::mojom::MediaStreamRequestResult::FAILED_DUE_TO_SHUTDOWN,
        std::unique_ptr<MediaStreamUI>());
  }
}

bool WebContentsImpl::CheckMediaAccessPermission(
    RenderFrameHostImpl* render_frame_host,
    const url::Origin& security_origin,
    blink::mojom::MediaStreamType type) {
  OPTIONAL_TRACE_EVENT2("content",
                        "WebContentsImpl::CheckMediaAccessPermission",
                        "render_frame_host", render_frame_host,
                        "security_origin", security_origin);

  DCHECK(type == blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE ||
         type == blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);
  return delegate_ && delegate_->CheckMediaAccessPermission(
                          render_frame_host, security_origin, type);
}

void WebContentsImpl::SetCaptureHandleConfig(
    blink::mojom::CaptureHandleConfigPtr config) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (capture_handle_config_ == *config) {
    return;  // Avoid unnecessary notifications.
  }

  capture_handle_config_ = std::move(*config);

  // Propagates the capture-handle-config inside of the browser process.
  // Only render processes which are eligible based on |permittedOrigins|
  // will get this.
  observers_.NotifyObservers(&WebContentsObserver::OnCaptureHandleConfigUpdate,
                             capture_handle_config_);
}

bool WebContentsImpl::IsJavaScriptDialogShowing() const {
  return is_showing_javascript_dialog_;
}

bool WebContentsImpl::ShouldIgnoreUnresponsiveRenderer() {
  // Suppress unresponsive renderers if the command line asks for it.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableHangMonitor)) {
    return true;
  }

  if (IsBeingDestroyed()) {
    return true;
  }

  if (suppress_unresponsive_renderer_count_ > 0) {
    return true;
  }

  // Ignore unresponsive renderers if the debugger is attached to them since the
  // unresponsiveness might be a result of the renderer sitting on a breakpoint.
  //
#if BUILDFLAG(IS_WIN)
  // Check if a windows debugger is attached to the renderer process.
  base::ProcessHandle process_handle =
      GetPrimaryMainFrame()->GetProcess()->GetProcess().Handle();
  BOOL debugger_present = FALSE;
  if (CheckRemoteDebuggerPresent(process_handle, &debugger_present) &&
      debugger_present) {
    return true;
  }
#endif  // BUILDFLAG(IS_WIN)

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
  if (rwhv) {
    rwhv->SetMainFrameAXTreeID(GetPrimaryMainFrame()->GetAXTreeID());
  }

  observers_.NotifyObservers(
      &WebContentsObserver::AXTreeIDForMainFrameHasChanged);
}

void WebContentsImpl::ProcessAccessibilityUpdatesAndEvents(
    ui::AXUpdatesAndEvents& details) {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::AccessibilityEventReceived");

  // First, supply the data to consumers that won't change it.
  observers_.NotifyObservers(&WebContentsObserver::AccessibilityEventReceived,
                             details);

  // Next, supply the data to consumers that may change it or who need to avoid
  // extra copying. Note that this also includes those who will pass to mojo
  // pipes not taking const.
  // TODO(accessibility): add support for this. WebContentsDelegate isn't the
  // right abstraction since it contains many other side effects.
}

void WebContentsImpl::AccessibilityLocationChangesReceived(
    const ui::AXTreeID& tree_id,
    ui::AXLocationAndScrollUpdates& details) {
  OPTIONAL_TRACE_EVENT0(
      "content", "WebContentsImpl::AccessibilityLocationChangesReceived");
  observers_.NotifyObservers(
      &WebContentsObserver::AccessibilityLocationChangesReceived, tree_id,
      details);
}

ui::AXNode* WebContentsImpl::GetAccessibilityRootNode() {
  ui::BrowserAccessibilityManager* manager =
      GetRootBrowserAccessibilityManager();
  if (!manager || !manager->ax_tree()) {
    return nullptr;
  }
  return manager->ax_tree()->root();
}

std::string WebContentsImpl::DumpAccessibilityTree(
    bool internal,
    std::vector<ui::AXPropertyFilter> property_filters) {
  ui::AXApiType::Type api_type =
      internal ? ui::AXApiType::Type(ui::AXApiType::kBlink)
               : AXInspectFactory::DefaultPlatformFormatterType();
  return DumpAccessibilityTree(api_type, property_filters);
}

std::string WebContentsImpl::DumpAccessibilityTree(
    ui::AXApiType::Type api_type,
    std::vector<ui::AXPropertyFilter> property_filters) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::DumpAccessibilityTree");
  auto* ax_mgr = GetOrCreateRootBrowserAccessibilityManager();
  // Since for Web Content we get the AXTree updates through the renderer at a
  // point after the manager is created, there are cases where at this point in
  // the lifecycle the AXTree associated with `ax_mgr` does not have a valid
  // tree ID. As such, if this is the case we return an empty string early. If
  // we don't have this check, there will be a scenario where we then try to get
  // the manager using the ID (which at this point is invalid) which leads to a
  // crash. See https://crbug.com/1405036.
  if (!ax_mgr || !ax_mgr->HasValidTreeID()) {
    return "-";
  }

  // Developer mode: crash immediately on any accessibility fatal error.
  // This only runs during integration tests, or if a developer is
  // using an inspection tool, e.g. chrome://accessibility.
  ui::AXTreeManager::AlwaysFailFast();
  DCHECK(base::Contains(AXInspectFactory::SupportedApis(), api_type));
  std::unique_ptr<ui::AXTreeFormatter> formatter =
      AXInspectFactory::CreateFormatter(api_type);

  formatter->SetPropertyFilters(property_filters);
  return formatter->Format(ax_mgr->GetBrowserAccessibilityRoot());
}

void WebContentsImpl::RecordAccessibilityEvents(
    bool start_recording,
    std::optional<ui::AXEventCallback> callback) {
  RecordAccessibilityEvents(AXInspectFactory::DefaultPlatformRecorderType(),
                            start_recording, callback);
}
void WebContentsImpl::RecordAccessibilityEvents(
    ui::AXApiType::Type api_type,
    bool start_recording,
    std::optional<ui::AXEventCallback> callback) {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::RecordAccessibilityEvents");

  // Only pass a callback to RecordAccessibilityEvents when starting to record.
  DCHECK_EQ(start_recording, callback.has_value());
  if (start_recording) {
    // TODO(grt): Do we need to do the same for all inner WebContentses?.
    recording_mode_ =
        BrowserAccessibilityState::GetInstance()
            ->CreateScopedModeForWebContents(this, ui::kAXModeBasic);
    auto* ax_mgr = GetOrCreateRootBrowserAccessibilityManager();
    CHECK(ax_mgr);
    base::ProcessId pid = base::Process::Current().Pid();
    gfx::AcceleratedWidget widget =
        ax_mgr->GetBrowserAccessibilityRoot()
            ->GetTargetForNativeAccessibilityEvent();

    DCHECK(base::Contains(AXInspectFactory::SupportedApis(), api_type));
    event_recorder_ = content::AXInspectFactory::CreateRecorder(
        api_type, ax_mgr, pid, ui::AXTreeSelector(widget));
    event_recorder_->ListenToEvents(*callback);
  } else {
    if (event_recorder_) {
      event_recorder_->WaitForDoneRecording();
      event_recorder_.reset(nullptr);
    }
    recording_mode_.reset();
  }
}

void WebContentsImpl::UnrecoverableAccessibilityError() {
  SetAccessibilityMode(ui::AXMode::kNone);
  unrecoverable_accessibility_error_ = true;
}

device::mojom::GeolocationContext* WebContentsImpl::GetGeolocationContext() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::GetGeolocationContext");
  if (delegate_) {
    auto* installed_webapp_context =
        delegate_->GetInstalledWebappGeolocationContext();
    if (installed_webapp_context) {
      return installed_webapp_context;
    }
  }

  if (!geolocation_context_) {
    GetDeviceService().BindGeolocationContext(
        geolocation_context_.BindNewPipeAndPassReceiver());
  }
  return geolocation_context_.get();
}

device::mojom::WakeLockContext* WebContentsImpl::GetWakeLockContext() {
  if (!enable_wake_locks_) {
    return nullptr;
  }
  if (!wake_lock_context_host_) {
    wake_lock_context_host_ = std::make_unique<WakeLockContextHost>(this);
  }
  return wake_lock_context_host_->GetWakeLockContext();
}

#if BUILDFLAG(IS_ANDROID)
void WebContentsImpl::GetNFC(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<device::mojom::NFC> receiver) {
  if (!nfc_host_) {
    nfc_host_ = std::make_unique<NFCHost>(this);
  }
  nfc_host_->GetNFC(render_frame_host, std::move(receiver));
}
#endif

void WebContentsImpl::SendScreenRects() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::SendScreenRects");

  DCHECK(!IsBeingDestroyed());

  GetPrimaryMainFrame()->ForEachRenderFrameHost(
      [](RenderFrameHostImpl* render_frame_host) {
        if (render_frame_host->is_local_root()) {
          render_frame_host->GetRenderWidgetHost()->SendScreenRects();
        }
      });
}

void WebContentsImpl::SendActiveState(bool active) {
  DCHECK(!IsBeingDestroyed());

  // Replicate the active state to all LocalRoots.
  GetPrimaryMainFrame()->ForEachRenderFrameHost(
      [active](RenderFrameHostImpl* render_frame_host) {
        if (render_frame_host->is_local_root()) {
          render_frame_host->GetRenderWidgetHost()->SetActive(active);
        }
      });
}

TextInputManager* WebContentsImpl::GetTextInputManager() {
  if (suppress_ime_events_for_testing_) {
    return nullptr;
  }

  if (GetOuterWebContents()) {
    return GetOuterWebContents()->GetTextInputManager();
  }

  if (!text_input_manager_ && !browser_plugin_guest_) {
    text_input_manager_ = std::make_unique<TextInputManager>();
  }

  return text_input_manager_.get();
}

bool WebContentsImpl::IsWidgetForPrimaryMainFrame(
    RenderWidgetHostImpl* render_widget_host) {
  return render_widget_host == GetPrimaryMainFrame()->GetRenderWidgetHost();
}

ui::BrowserAccessibilityManager*
WebContentsImpl::GetRootBrowserAccessibilityManager() {
  RenderFrameHostImpl* rfh =
      static_cast<RenderFrameHostImpl*>(GetPrimaryMainFrame());
  return rfh ? rfh->browser_accessibility_manager() : nullptr;
}

ui::BrowserAccessibilityManager*
WebContentsImpl::GetOrCreateRootBrowserAccessibilityManager() {
  RenderFrameHostImpl* rfh =
      static_cast<RenderFrameHostImpl*>(GetPrimaryMainFrame());
  return rfh ? rfh->GetOrCreateBrowserAccessibilityManager() : nullptr;
}

void WebContentsImpl::ExecuteEditCommand(
    const std::string& command,
    const std::optional<std::u16string>& value) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::ExecuteEditCommand");
  auto* input_handler = GetFocusedFrameWidgetInputHandler();
  if (!input_handler) {
    return;
  }

  input_handler->ExecuteEditCommand(command, value);
}

void WebContentsImpl::MoveRangeSelectionExtent(const gfx::Point& extent) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::MoveRangeSelectionExtent");
  auto* input_handler = GetFocusedFrameWidgetInputHandler();
  if (!input_handler) {
    return;
  }

  input_handler->MoveRangeSelectionExtent(extent);
}

void WebContentsImpl::SelectRange(const gfx::Point& base,
                                  const gfx::Point& extent) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::SelectRange");
  auto* input_handler = GetFocusedFrameWidgetInputHandler();
  if (!input_handler) {
    return;
  }

  input_handler->SelectRange(base, extent);
}

void WebContentsImpl::SelectAroundCaret(
    blink::mojom::SelectionGranularity granularity,
    bool should_show_handle,
    bool should_show_context_menu) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::SelectAroundCaret");
  auto* input_handler = GetFocusedFrameWidgetInputHandler();
  if (!input_handler) {
    return;
  }

  last_interaction_time_ = ui::EventTimeForNow();
  input_handler->SelectAroundCaret(granularity, should_show_handle,
                                   should_show_context_menu, base::DoNothing());
}

void WebContentsImpl::MoveCaret(const gfx::Point& extent) {
  OPTIONAL_TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("content.verbose"),
                        "WebContentsImpl::MoveCaret");
  auto* input_handler = GetFocusedFrameWidgetInputHandler();
  if (!input_handler) {
    return;
  }

  input_handler->MoveCaret(extent);
}

uint32_t WebContentsImpl::GetCompositorFrameSinkGroupingId() const {
  return compositor_frame_sink_grouping_id_;
}

void WebContentsImpl::AdjustSelectionByCharacterOffset(
    int start_adjust,
    int end_adjust,
    bool show_selection_menu) {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::AdjustSelectionByCharacterOffset");
  auto* input_handler = GetFocusedFrameWidgetInputHandler();
  if (!input_handler) {
    return;
  }

  using blink::mojom::SelectionMenuBehavior;
  input_handler->AdjustSelectionByCharacterOffset(
      start_adjust, end_adjust,
      show_selection_menu ? SelectionMenuBehavior::kShow
                          : SelectionMenuBehavior::kHide);
}

void WebContentsImpl::ResizeDueToAutoResize(
    RenderWidgetHostImpl* render_widget_host,
    const gfx::Size& new_size) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::ResizeDueToAutoResize",
                        "render_widget_host", render_widget_host);
  if (render_widget_host != GetRenderViewHost()->GetWidget()) {
    return;
  }

  if (delegate_) {
    delegate_->ResizeDueToAutoResize(this, new_size);
  }
}

WebContents* WebContentsImpl::OpenURL(
    const OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  TRACE_EVENT1("content", "WebContentsImpl::OpenURL", "url", params.url);
#if DCHECK_IS_ON()
  DCHECK(params.Valid());
#endif

  if (!delegate_) {
    // Embedder can delay setting a delegate on new WebContents with
    // WebContentsDelegate::ShouldResumeRequestsForCreatedWindow. In the mean
    // time, navigations, including the initial one, that goes through OpenURL
    // should be delayed until embedder is ready to resume loading.
    delayed_open_url_params_ = std::make_unique<OpenURLParams>(params);
    delayed_navigation_handle_callback_ = std::move(navigation_handle_callback);

    // If there was a navigation deferred when creating the window through
    // CreateNewWindow, drop it in favor of this navigation.
    delayed_load_url_params_.reset();

    return nullptr;
  }

  RenderFrameHost* source_render_frame_host = RenderFrameHost::FromID(
      params.source_render_process_id, params.source_render_frame_id);

  // Prevent frames that are not active (e.g. a prerendering page) from opening
  // new windows, tabs, popups, etc.
  if (params.disposition != WindowOpenDisposition::CURRENT_TAB &&
      source_render_frame_host && !source_render_frame_host->IsActive()) {
    return nullptr;
  }

  if (params.frame_tree_node_id) {
    if (auto* frame_tree_node =
            FrameTreeNode::GloballyFindByID(params.frame_tree_node_id)) {
      // If a frame tree node ID is specified and it exists, ensure it is for a
      // node within this WebContents. Note: this WebContents could be hosting
      // multiple frame trees (e.g. prerendering) so it's not enough to check
      // against this->primary_frame_tree_. Check against page_delegate(), which
      // is always a WebContentsImpl, while delegate() may be implemented by
      // something else such as for prerendered frame trees.
      FrameTree& frame_tree = frame_tree_node->frame_tree();
      CHECK_EQ(frame_tree.page_delegate(), this);

      // Prerendering and fenced frame navigations are hidden from embedders.
      // If the navigation is targeting a frame in a prerendering or fenced
      // frame tree, we shouldn't run that navigation through the embedder
      // delegate. Embedder implementations of
      // `WebContentsDelegate::OpenURLFromTab` assume that the primary
      // frame tree Navigation controller should be used for navigating.
      // Instead, we just navigate directly on the relevant frame
      // tree.
      if (frame_tree.is_prerendering() ||
          frame_tree_node->IsInFencedFrameTree()) {
        DCHECK_EQ(params.disposition, WindowOpenDisposition::CURRENT_TAB);
        frame_tree.controller().LoadURLWithParams(
            NavigationController::LoadURLParams(params));
        return this;
      }
    } else {
      // If the node doesn't exist it was probably removed from its frame tree.
      // In that case, abort since continuing would navigate the root frame.
      return nullptr;
    }
  }

  WebContents* new_contents = delegate_->OpenURLFromTab(
      this, params, std::move(navigation_handle_callback));

  if (source_render_frame_host && params.source_site_instance) {
    CHECK_EQ(source_render_frame_host->GetSiteInstance(),
             params.source_site_instance.get());
  }

  if (new_contents && source_render_frame_host && new_contents != this) {
    observers_.NotifyObservers(
        &WebContentsObserver::DidOpenRequestedURL, new_contents,
        source_render_frame_host, params.url, params.referrer,
        params.disposition, params.transition, params.started_from_context_menu,
        params.is_renderer_initiated);
  }

  return new_contents;
}

void WebContentsImpl::SetHistoryOffsetAndLengthForView(
    RenderViewHost* render_view_host,
    int history_offset,
    int history_length) {
  OPTIONAL_TRACE_EVENT2(
      "content", "WebContentsImpl::SetHistoryOffsetAndLengthForView",
      "history_offset", history_offset, "history_length", history_length);
  if (auto& broadcast = static_cast<RenderViewHostImpl*>(render_view_host)
                            ->GetAssociatedPageBroadcast()) {
    broadcast->SetHistoryOffsetAndLength(history_offset, history_length);
  }
}

void WebContentsImpl::ReloadFocusedFrame() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::ReloadFocusedFrame");
  RenderFrameHost* focused_frame = GetFocusedFrame();
  if (!focused_frame) {
    return;
  }

  focused_frame->Reload();
}

void WebContentsImpl::Undo() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::Undo");
  auto* input_handler = GetFocusedFrameWidgetInputHandler();
  if (!input_handler) {
    return;
  }

  last_interaction_time_ = ui::EventTimeForNow();
  input_handler->Undo();
  RecordAction(base::UserMetricsAction("Undo"));
}

void WebContentsImpl::Redo() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::Redo");
  auto* input_handler = GetFocusedFrameWidgetInputHandler();
  if (!input_handler) {
    return;
  }

  last_interaction_time_ = ui::EventTimeForNow();
  input_handler->Redo();
  RecordAction(base::UserMetricsAction("Redo"));
}

void WebContentsImpl::Cut() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::Cut");
  auto* input_handler = GetFocusedFrameWidgetInputHandler();
  if (!input_handler) {
    return;
  }

  last_interaction_time_ = ui::EventTimeForNow();
  input_handler->Cut();
  RecordAction(base::UserMetricsAction("Cut"));
}

void WebContentsImpl::Copy() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::Copy");
  auto* input_handler = GetFocusedFrameWidgetInputHandler();
  if (!input_handler) {
    return;
  }

  last_interaction_time_ = ui::EventTimeForNow();
  input_handler->Copy();
  RecordAction(base::UserMetricsAction("Copy"));
}

void WebContentsImpl::CopyToFindPboard() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::CopyToFindPboard");
#if BUILDFLAG(IS_MAC)
  auto* input_handler = GetFocusedFrameWidgetInputHandler();
  if (!input_handler) {
    return;
  }

  last_interaction_time_ = ui::EventTimeForNow();
  // Windows/Linux don't have the concept of a find pasteboard.
  input_handler->CopyToFindPboard();
  RecordAction(base::UserMetricsAction("CopyToFindPboard"));
#endif
}

void WebContentsImpl::CenterSelection() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::CenterSelection");
#if BUILDFLAG(IS_MAC)
  auto* input_handler = GetFocusedFrameWidgetInputHandler();
  if (!input_handler) {
    return;
  }

  last_interaction_time_ = ui::EventTimeForNow();
  input_handler->CenterSelection();
#endif
}

void WebContentsImpl::Paste() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::Paste");
  auto* input_handler = GetFocusedFrameWidgetInputHandler();
  if (!input_handler) {
    return;
  }

  last_interaction_time_ = ui::EventTimeForNow();
  input_handler->Paste();
  observers_.NotifyObservers(&WebContentsObserver::OnPaste);
  RecordAction(base::UserMetricsAction("Paste"));
}

void WebContentsImpl::PasteAndMatchStyle() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::PasteAndMatchStyle");
  auto* input_handler = GetFocusedFrameWidgetInputHandler();
  if (!input_handler) {
    return;
  }

  last_interaction_time_ = ui::EventTimeForNow();
  input_handler->PasteAndMatchStyle();
  observers_.NotifyObservers(&WebContentsObserver::OnPaste);
  RecordAction(base::UserMetricsAction("PasteAndMatchStyle"));
}

void WebContentsImpl::Delete() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::Delete");
  auto* input_handler = GetFocusedFrameWidgetInputHandler();
  if (!input_handler) {
    return;
  }

  last_interaction_time_ = ui::EventTimeForNow();
  input_handler->Delete();
  RecordAction(base::UserMetricsAction("DeleteSelection"));
}

void WebContentsImpl::SelectAll() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::SelectAll");
  auto* input_handler = GetFocusedFrameWidgetInputHandler();
  if (!input_handler) {
    return;
  }

  last_interaction_time_ = ui::EventTimeForNow();
  input_handler->SelectAll();
  RecordAction(base::UserMetricsAction("SelectAll"));
}

void WebContentsImpl::CollapseSelection() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::CollapseSelection");
  auto* input_handler = GetFocusedFrameWidgetInputHandler();
  if (!input_handler) {
    return;
  }

  last_interaction_time_ = ui::EventTimeForNow();
  input_handler->CollapseSelection();
}

void WebContentsImpl::ScrollToTopOfDocument() {
  ExecuteEditCommand("ScrollToBeginningOfDocument", std::nullopt);
}

void WebContentsImpl::ScrollToBottomOfDocument() {
  ExecuteEditCommand("ScrollToEndOfDocument", std::nullopt);
}

void WebContentsImpl::Replace(const std::u16string& word) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::Replace");
  auto* input_handler = GetFocusedFrameWidgetInputHandler();
  if (!input_handler) {
    return;
  }

  last_interaction_time_ = ui::EventTimeForNow();
  input_handler->Replace(word);
}

void WebContentsImpl::ReplaceMisspelling(const std::u16string& word) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::ReplaceMisspelling");
  auto* input_handler = GetFocusedFrameWidgetInputHandler();
  if (!input_handler) {
    return;
  }

  last_interaction_time_ = ui::EventTimeForNow();
  input_handler->ReplaceMisspelling(word);
}

void WebContentsImpl::NotifyContextMenuClosed(const GURL& link_followed) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::NotifyContextMenuClosed");
  RenderFrameHost* focused_frame = GetFocusedFrame();
  if (!focused_frame) {
    return;
  }

  if (context_menu_client_) {
    context_menu_client_->ContextMenuClosed(link_followed);
  }

  context_menu_client_.reset();
}

void WebContentsImpl::ExecuteCustomContextMenuCommand(
    int action,
    const GURL& link_followed) {
  OPTIONAL_TRACE_EVENT1("content",
                        "WebContentsImpl::ExecuteCustomContextMenuCommand",
                        "action", action);
  RenderFrameHost* focused_frame = GetFocusedFrame();
  if (!focused_frame) {
    return;
  }

  if (context_menu_client_) {
    context_menu_client_->CustomContextMenuAction(action);
  }
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
    SaveFrame(GetLastCommittedURL(), Referrer(), GetPrimaryMainFrame());
    return;
  }

  Stop();

  // Create the save package and possibly prompt the user for the name to save
  // the page as. The user prompt is an asynchronous operation that runs on
  // another thread.
  save_package_ = new SavePackage(GetPrimaryPage());
  save_package_->GetSaveInfo();
}

// Used in automated testing to bypass prompting the user for file names.
// Instead, the names and paths are hard coded rather than running them through
// file name sanitation and extension / mime checking.
bool WebContentsImpl::SavePage(const base::FilePath& main_file,
                               const base::FilePath& dir_path,
                               SavePageType save_type) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::SavePage", "main_file",
                        main_file, "dir_path", dir_path);
  // Stop the page from navigating.
  Stop();

  save_package_ =
      new SavePackage(GetPrimaryPage(), save_type, main_file, dir_path);
  return save_package_->Init(SavePackageDownloadCreatedCallback());
}

void WebContentsImpl::SaveFrame(const GURL& url,
                                const Referrer& referrer,
                                RenderFrameHost* rfh) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::SaveFrame");
  SaveFrameWithHeaders(url, referrer, std::string(), std::u16string(), rfh,
                       /*is_subresource=*/false);
}

void WebContentsImpl::SaveFrameWithHeaders(
    const GURL& url,
    const Referrer& referrer,
    const std::string& headers,
    const std::u16string& suggested_filename,
    RenderFrameHost* rfh,
    bool is_subresource) {
  DCHECK(rfh);
  auto& rfhi = *static_cast<RenderFrameHostImpl*>(rfh);

  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::SaveFrameWithHeaders",
                        "url", url, "headers", headers);
  // Check and see if the guest can handle this.
  if (delegate_) {
    WebContents* guest_web_contents = nullptr;
    if (browser_plugin_embedder_) {
      BrowserPluginGuest* guest = browser_plugin_embedder_->GetFullPageGuest();
      if (guest) {
        guest_web_contents = guest->GetWebContents();
      }
    } else if (browser_plugin_guest_) {
      guest_web_contents = this;
    }

    if (guest_web_contents && delegate_->GuestSaveFrame(guest_web_contents)) {
      return;
    }
  }

  if (!GetLastCommittedURL().is_valid()) {
    return;
  }
  if (delegate_ && delegate_->SaveFrame(url, referrer, rfh)) {
    return;
  }

  int64_t post_id = -1;
  if (rfhi.is_main_frame() && !is_subresource) {
    NavigationEntry* entry =
        rfhi.frame_tree()->controller().GetLastCommittedEntry();
    if (entry) {
      post_id = entry->GetPostID();
    }
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
      url, rfh->GetProcess()->GetID(), rfh->GetRoutingID(), traffic_annotation);
  params->set_referrer(referrer.url);
  params->set_referrer_policy(
      Referrer::ReferrerPolicyForUrlRequest(referrer.policy));
  params->set_post_id(post_id);
  if (post_id >= 0) {
    params->set_method("POST");
  }
  params->set_prompt(true);

  if (!headers.empty()) {
    for (download::DownloadUrlParameters::RequestHeadersNameValuePair
             key_value : ParseDownloadHeaders(headers)) {
      params->add_request_header(key_value.first, key_value.second);
    }
  }
  params->set_prefer_cache(true);
  params->set_suggested_name(suggested_filename);
  params->set_download_source(download::DownloadSource::WEB_CONTENTS_API);
  params->set_isolation_info(rfhi.ComputeIsolationInfoForNavigation(url));

  FrameTreeNode* frame_tree_node = rfhi.frame_tree_node();
  FrameNavigationEntry* frame_navigation_entry =
      frame_tree_node->frame_tree()
          .controller()
          .GetLastCommittedEntry()
          ->GetFrameEntry(frame_tree_node);
  if (frame_navigation_entry) {
    params->set_initiator(frame_navigation_entry->initiator_origin());
  }

  GetBrowserContext()->GetDownloadManager()->DownloadUrl(std::move(params));
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

const std::string& WebContentsImpl::GetContentsMimeType() {
  return GetPrimaryPage().GetContentsMimeType();
}

blink::RendererPreferences* WebContentsImpl::GetMutableRendererPrefs() {
  return &renderer_preferences_;
}

void WebContentsImpl::DragSourceEndedAt(float client_x,
                                        float client_y,
                                        float screen_x,
                                        float screen_y,
                                        ui::mojom::DragOperation operation,
                                        RenderWidgetHost* source_rwh) {
  OPTIONAL_TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("content.verbose"),
                        "WebContentsImpl::DragSourceEndedAt");
  if (source_rwh) {
    source_rwh->DragSourceEndedAt(gfx::PointF(client_x, client_y),
                                  gfx::PointF(screen_x, screen_y), operation,
                                  base::DoNothing());
  }
}

void WebContentsImpl::LoadStateChanged(network::mojom::LoadInfoPtr load_info) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::LoadStateChanged", "host",
                        load_info->host, "load_state", load_info->load_state);

  // If the new load state isn't progressed as far as the current loading state
  // or both are sending an upload and the upload is smaller, return early
  // discarding the new load state.
  if (load_info_timestamp_ + kUpdateLoadStatesInterval > load_info->timestamp &&
      (load_state_.state > load_info->load_state ||
       (load_state_.state == load_info->load_state &&
        load_state_.state == net::LOAD_STATE_SENDING_REQUEST &&
        upload_size_ > load_info->upload_size))) {
    return;
  }

  load_info_timestamp_ = load_info->timestamp;
  std::u16string host16 = url_formatter::IDNToUnicode(load_info->host);
  // Drop no-op updates.
  if (load_state_.state == load_info->load_state &&
      load_state_.param == load_info->state_param &&
      upload_position_ == load_info->upload_position &&
      upload_size_ == load_info->upload_size && load_state_host_ == host16) {
    return;
  }
  load_state_ = net::LoadStateWithParam(
      static_cast<net::LoadState>(load_info->load_state),
      load_info->state_param);
  load_state_host_ = host16;
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
    SCOPED_UMA_HISTOGRAM_TIMER("WebContentsObserver.OnVisibilityChanged");
    observers_.NotifyObservers(&WebContentsObserver::OnVisibilityChanged,
                               visibility);
  }
}

void WebContentsImpl::NotifyWebContentsFocused(
    RenderWidgetHost* render_widget_host) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::NotifyWebContentsFocused",
                        "render_widget_host", render_widget_host);
  observers_.NotifyObservers(&WebContentsObserver::OnWebContentsFocused,
                             render_widget_host);
}

void WebContentsImpl::NotifyWebContentsLostFocus(
    RenderWidgetHost* render_widget_host) {
  OPTIONAL_TRACE_EVENT1("content",
                        "WebContentsImpl::NotifyWebContentsLostFocus",
                        "render_widget_host", render_widget_host);
  observers_.NotifyObservers(&WebContentsObserver::OnWebContentsLostFocus,
                             render_widget_host);
}

void WebContentsImpl::SystemDragEnded(RenderWidgetHost* source_rwh) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::SystemDragEnded",
                        "render_widget_host", source_rwh);
  if (source_rwh) {
    source_rwh->DragSourceSystemDragEnded();
  }
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
  GetPrimaryMainFrame()->GetAssociatedLocalMainFrame()->SetScaleFactor(
      scale_factor);
}

gfx::Size WebContentsImpl::GetPreferredSize() {
  return IsBeingCaptured() ? preferred_size_for_capture_ : preferred_size_;
}

bool WebContentsImpl::GotResponseToPointerLockRequest(
    blink::mojom::PointerLockResult result) {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::GotResponseToPointerLockRequest");
  if (pointer_lock_widget_) {
    auto* web_contents =
        WebContentsImpl::FromRenderWidgetHostImpl(pointer_lock_widget_);
    if (web_contents != this) {
      return web_contents->GotResponseToPointerLockRequest(result);
    }

    if (pointer_lock_widget_->GotResponseToPointerLockRequest(result)) {
      return true;
    }
  }

  for (WebContentsImpl* current = this; current;
       current = current->GetOuterWebContents()) {
    current->pointer_lock_widget_ = nullptr;
  }

  return false;
}

void WebContentsImpl::GotPointerLockPermissionResponse(bool allowed) {
  GotResponseToPointerLockRequest(
      allowed ? blink::mojom::PointerLockResult::kSuccess
              : blink::mojom::PointerLockResult::kPermissionDenied);
}

void WebContentsImpl::DropPointerLockForTesting() {
  if (pointer_lock_widget_) {
    pointer_lock_widget_->RejectPointerLockOrUnlockIfNecessary(
        blink::mojom::PointerLockResult::kUnknownError);
    for (WebContentsImpl* current = this; current;
         current = current->GetOuterWebContents()) {
      current->pointer_lock_widget_ = nullptr;
    }
  }
}

bool WebContentsImpl::GotResponseToKeyboardLockRequest(bool allowed) {
  OPTIONAL_TRACE_EVENT1("content",
                        "WebContentsImpl::GotResponseToKeyboardLockRequest",
                        "allowed", allowed);
  if (!keyboard_lock_widget_) {
    return false;
  }
  // Exits early here if GotResponseToKeyboardLockRequest() was called from
  // the dtor for `this`.
  if (WebContentsImpl::FromRenderWidgetHostImpl(keyboard_lock_widget_) !=
      this) {
    return false;
  }
  // KeyboardLock is only supported when called by the top-level browsing
  // context and is not supported in embedded content scenarios.
  if (GetOuterWebContents()) {
    keyboard_lock_widget_->GotResponseToKeyboardLockRequest(false);
    return false;
  }
  keyboard_lock_widget_->GotResponseToKeyboardLockRequest(allowed);
  return true;
}

bool WebContentsImpl::HasOpener() {
  return GetOpener() != nullptr;
}

RenderFrameHostImpl* WebContentsImpl::GetOpener() {
  FrameTreeNode* opener_ftn = primary_frame_tree_.root()->opener();
  return opener_ftn ? opener_ftn->current_frame_host() : nullptr;
}

bool WebContentsImpl::HasLiveOriginalOpenerChain() {
  return GetFirstWebContentsInLiveOriginalOpenerChain() != nullptr;
}

WebContents* WebContentsImpl::GetFirstWebContentsInLiveOriginalOpenerChain() {
  FrameTreeNode* opener_ftn =
      primary_frame_tree_.root()
          ->first_live_main_frame_in_original_opener_chain();
  return opener_ftn ? WebContents::FromRenderFrameHost(
                          opener_ftn->current_frame_host())
                    : nullptr;
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
void WebContentsImpl::DidChooseColorInColorChooser(SkColor color) {
  OPTIONAL_TRACE_EVENT1("content",
                        "WebContentsImpl::DidChooseColorInColorChooser",
                        "color", color);
  if (color_chooser_holder_) {
    color_chooser_holder_->DidChooseColorInColorChooser(color);
  }
}

void WebContentsImpl::DidEndColorChooser() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::DidEndColorChooser");
  color_chooser_holder_.reset();
}
#endif

int WebContentsImpl::DownloadImageFromAxNode(const ui::AXTreeID tree_id,
                                             const ui::AXNodeID node_id,
                                             const gfx::Size& preferred_size,
                                             uint32_t max_bitmap_size,
                                             bool bypass_cache,
                                             ImageDownloadCallback callback) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::DownloadImage",
                        "tree_id,node_id",
                        tree_id.ToString() + "," + base::ToString(node_id));
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const int download_id = GetNextDownloadId();
  // Always use the main frame when downloading via A11y ids.
  RenderFrameHostImpl* main_frame = GetPrimaryMainFrame();
  if (!main_frame->IsRenderFrameLive()) {
    // If the renderer process is dead (i.e. crash, or memory pressure on
    // Android), the downloader service will be invalid. Pre-Mojo, this would
    // hang the callback indefinitely since the IPC would be dropped. Now,
    // respond with a 400 HTTP error code to indicate that something went wrong.
    // Additionally for A11y requests, send an empty URL back since it won't be
    // used anyways.
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&WebContentsImpl::OnDidDownloadImage,
                       weak_factory_.GetWeakPtr(), main_frame->GetWeakPtr(),
                       std::move(callback), download_id, GURL(), 400,
                       std::vector<SkBitmap>(), std::vector<gfx::Size>()));
    return download_id;
  }
  CHECK_EQ(main_frame->GetAXTreeID(), tree_id);
  main_frame->GetMojoImageDownloader()->DownloadImageFromAxNode(
      node_id, preferred_size, max_bitmap_size, bypass_cache,
      base::BindOnce(&WebContentsImpl::OnDidDownloadImage,
                     weak_factory_.GetWeakPtr(), main_frame->GetWeakPtr(),
                     std::move(callback), download_id, GURL()));
  return download_id;
}
int WebContentsImpl::DownloadImage(
    const GURL& url,
    bool is_favicon,
    const gfx::Size& preferred_size,
    uint32_t max_bitmap_size,
    bool bypass_cache,
    WebContents::ImageDownloadCallback callback) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::DownloadImage", "url",
                        url);
  return DownloadImageInFrame(GlobalRenderFrameHostId(), url, is_favicon,
                              preferred_size, max_bitmap_size, bypass_cache,
                              std::move(callback));
}

int WebContentsImpl::DownloadImageInFrame(
    const GlobalRenderFrameHostId& initiator_frame_routing_id,
    const GURL& url,
    bool is_favicon,
    const gfx::Size& preferred_size,
    uint32_t max_bitmap_size,
    bool bypass_cache,
    WebContents::ImageDownloadCallback callback) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::DownloadImageInFrame");
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const int download_id = GetNextDownloadId();

  RenderFrameHostImpl* initiator_frame =
      initiator_frame_routing_id.child_id
          ? RenderFrameHostImpl::FromID(initiator_frame_routing_id)
          : GetPrimaryMainFrame();
  if (!initiator_frame->IsRenderFrameLive()) {
    // If the renderer process is dead (i.e. crash, or memory pressure on
    // Android), the downloader service will be invalid. Pre-Mojo, this would
    // hang the callback indefinitely since the IPC would be dropped. Now,
    // respond with a 400 HTTP error code to indicate that something went wrong.
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &WebContentsImpl::OnDidDownloadImage, weak_factory_.GetWeakPtr(),
            initiator_frame->GetWeakPtr(), std::move(callback), download_id,
            url, 400, std::vector<SkBitmap>(), std::vector<gfx::Size>()));
    return download_id;
  }

  initiator_frame->GetMojoImageDownloader()->DownloadImage(
      url, is_favicon, preferred_size, max_bitmap_size, bypass_cache,
      base::BindOnce(&WebContentsImpl::OnDidDownloadImage,
                     weak_factory_.GetWeakPtr(), initiator_frame->GetWeakPtr(),
                     std::move(callback), download_id, url));
  return download_id;
}

void WebContentsImpl::Find(int request_id,
                           const std::u16string& search_text,
                           blink::mojom::FindOptionsPtr options,
                           bool skip_delay) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::Find");
  // Cowardly refuse to search for no text.
  if (search_text.empty()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  GetOrCreateFindRequestManager()->Find(request_id, search_text,
                                        std::move(options), skip_delay);
}

void WebContentsImpl::StopFinding(StopFindAction action) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::StopFinding");
  if (FindRequestManager* manager = GetFindRequestManager()) {
    manager->StopFinding(action);
  }
}

bool WebContentsImpl::WasEverAudible() {
  return was_ever_audible_;
}

void WebContentsImpl::ExitFullscreen(bool will_cause_resize) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::ExitFullscreen");
  // Clean up related state and initiate the fullscreen exit.
  GetRenderViewHost()->GetWidget()->RejectPointerLockOrUnlockIfNecessary(
      blink::mojom::PointerLockResult::kUserRejected);
  ExitFullscreenMode(will_cause_resize);
}

base::ScopedClosureRunner WebContentsImpl::ForSecurityDropFullscreen(
    int64_t display_id) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::ForSecurityDropFullscreen",
                        "display_id", display_id);
  // Make WebContentses "related" to this instance exit HTML element fullscreen,
  // ignoring browser fullscreen and fullscreen-within-tab modes. This needs to
  // be done with two passes, because it is simple to walk _up_ the chain of
  // openers and outer contents, but it not simple to walk _down_ the chain.
  auto is_fullscreen = [](WebContentsImpl* tab, int64_t display_id) {
    if (!tab || !tab->GetDelegate()) {
      return false;
    }
    const FullscreenState state = tab->GetDelegate()->GetFullscreenState(tab);
    return state.target_mode == FullscreenMode::kContent &&
           (display_id == display::kInvalidDisplayId ||
            state.target_display_id == display::kInvalidDisplayId ||
            state.target_display_id == display_id);
  };

  // First, determine if any fullscreen WebContents has this WebContents as an
  // upstream contents. Drop that WebContents out of fullscreen if it does. This
  // is theoretically quadratic-ish (fullscreen contentses x each one's opener
  // length) but neither of those is expected to ever be a large number.
  auto fullscreen_set_copy = *FullscreenContentsSet(GetBrowserContext());
  for (WebContentsImpl* fullscreen_contents : fullscreen_set_copy) {
    if (is_fullscreen(fullscreen_contents, display_id)) {
      auto opener_contentses = GetAllOpeningWebContents(fullscreen_contents);
      if (opener_contentses.count(this)) {
        fullscreen_contents->ExitFullscreen(true);
      }
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
    if (is_fullscreen(opener, display_id)) {
      opener->ExitFullscreen(true);
    }

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
    base::WeakPtr<NavigationHandle> navigation =
        GetController().LoadURLWithParams(*delayed_load_url_params_.get());
    if (delayed_navigation_handle_callback_ && navigation) {
      std::move(delayed_navigation_handle_callback_).Run(*navigation);
    }
    delayed_navigation_handle_callback_.Reset();
    delayed_load_url_params_.reset(nullptr);
    return;
  }

  CHECK(!delayed_navigation_handle_callback_);

  if (delayed_open_url_params_.get()) {
    OpenURL(*delayed_open_url_params_.get(),
            std::move(delayed_navigation_handle_callback_));
    delayed_open_url_params_.reset(nullptr);
    return;
  }

  // Renderer-created main frames wait for the renderer to request for them to
  // perform navigations and to be shown. We signal that this condition is
  // satisfied by calling Init().
  if (is_resume_pending_) {
    is_resume_pending_ = false;
    GetPrimaryMainFrame()->Init();
  }
}

bool WebContentsImpl::FocusLocationBarByDefault() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::FocusLocationBarByDefault");
  if (should_focus_location_bar_by_default_) {
    return true;
  }

  return delegate_ && delegate_->ShouldFocusLocationBarByDefault(this);
}

void WebContentsImpl::DidStartNavigation(NavigationHandle* navigation_handle) {
  TRACE_EVENT1("navigation", "WebContentsImpl::DidStartNavigation",
               "navigation_handle", navigation_handle);
  base::ElapsedTimer duration;
  observers_.NotifyObservers(&WebContentsObserver::DidStartNavigation,
                             navigation_handle);
  base::TimeDelta elapsed = duration.Elapsed();
  base::UmaHistogramTimes("WebContentsObserver.DidStartNavigation", elapsed);
  base::UmaHistogramTimes(
      base::StrCat(
          {"WebContentsObserver.DidStartNavigation.",
           navigation_handle->IsInMainFrame() ? "MainFrame" : "Subframe"}),
      elapsed);
  if (navigation_handle->IsInPrimaryMainFrame()) {
    // `notify_disconnection_` may be reset during discard operations, ensure
    // this is restored when the when contents is re-navigated.
    notify_disconnection_ = true;

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
        GetController().IsInitialNavigation() &&
        !navigation_handle->IsRendererInitiated() &&
        navigation_handle->GetURL() == url::kAboutBlankURL;
  }
}

void WebContentsImpl::DidRedirectNavigation(
    NavigationHandle* navigation_handle) {
  TRACE_EVENT1("navigation", "WebContentsImpl::DidRedirectNavigation",
               "navigation_handle", navigation_handle);
  {
    SCOPED_UMA_HISTOGRAM_TIMER("WebContentsObserver.DidRedirectNavigation");
    observers_.NotifyObservers(&WebContentsObserver::DidRedirectNavigation,
                               navigation_handle);
  }
  // Notify accessibility if this is a reload. This has to called on the
  // BrowserAccessibilityManager associated with the old RFHI.
  if (navigation_handle->GetReloadType() != ReloadType::NONE) {
    NavigationRequest* request = NavigationRequest::From(navigation_handle);
    ui::BrowserAccessibilityManager* manager =
        request->frame_tree_node()
            ->current_frame_host()
            ->browser_accessibility_manager();
    if (manager) {
      manager->UserIsReloading();
    }
  }
}

void WebContentsImpl::ReadyToCommitNavigation(
    NavigationHandle* navigation_handle) {
  TRACE_EVENT1("navigation", "WebContentsImpl::ReadyToCommitNavigation",
               "navigation_handle", navigation_handle);

  // Cross-document navigation of the top-level frame resets the capture
  // handle config. Using IsInPrimaryMainFrame is valid here since the browser
  // caches this state for the active main frame only.
  if (!navigation_handle->IsSameDocument() &&
      navigation_handle->IsInPrimaryMainFrame()) {
    SetCaptureHandleConfig(blink::mojom::CaptureHandleConfig::New());
  }

  observers_.NotifyObservers(&WebContentsObserver::ReadyToCommitNavigation,
                             navigation_handle);

  // If any domains are blocked from accessing 3D APIs because they may
  // have caused the GPU to reset recently, unblock them here if the user
  // initiated this navigation. This implies that the user was involved in
  // the decision to navigate, so there's no concern about
  // denial-of-service issues. Want to do this as early as
  // possible to avoid race conditions with pages attempting to access
  // WebGL early on.
  //
  // TODO(crbug.com/41257523): currently navigations initiated by the browser
  // (reload button, reload menu option, pressing return in the Omnibox)
  // return false from HasUserGesture(). If or when that is addressed,
  // remove the check for IsRendererInitiated() below.
  //
  // TODO(crbug.com/40571460): HasUserGesture comes from the renderer
  // process and isn't validated. Until it is, don't trust it.
  if (!navigation_handle->IsRendererInitiated()) {
    GpuDataManagerImpl::GetInstance()->UnblockDomainFrom3DAPIs(
        navigation_handle->GetURL());
  }

  if (navigation_handle->IsSameDocument()) {
    return;
  }

  // SSLInfo is not needed on subframe navigations since the main-frame
  // certificate is the only one that can be inspected (using the info
  // bubble) without refreshing the page with DevTools open.
  // We don't call DidStartResourceResponse on net errors, since that results on
  // existing cert exceptions being revoked, which leads to weird behavior with
  // committed interstitials or while offline. We only need the error check for
  // the main frame case.
  if (navigation_handle->IsInMainFrame() &&
      navigation_handle->GetNetErrorCode() == net::OK) {
    static_cast<NavigationRequest*>(navigation_handle)
        ->frame_tree_node()
        ->frame_tree()
        .controller()
        .ssl_manager()
        ->DidStartResourceResponse(
            url::SchemeHostPort(navigation_handle->GetURL()),
            navigation_handle->GetSSLInfo().has_value()
                ? net::IsCertStatusError(
                      navigation_handle->GetSSLInfo()->cert_status)
                : false);
  }
}

void WebContentsImpl::DidFinishNavigation(NavigationHandle* navigation_handle) {
  TRACE_EVENT1("navigation", "WebContentsImpl::DidFinishNavigation",
               "navigation_handle", navigation_handle);

  {
    SCOPED_UMA_HISTOGRAM_TIMER("WebContentsObserver.DidFinishNavigation");
    observers_.NotifyObservers(&WebContentsObserver::DidFinishNavigation,
                               navigation_handle);
  }
  if (safe_area_insets_host_) {
    safe_area_insets_host_->DidFinishNavigation(navigation_handle);
  }

  if (navigation_handle->HasCommitted()) {
    // TODO(domfarolino, dmazzoni): Do this using WebContentsObserver. See
    // https://crbug.com/981271.
    ui::BrowserAccessibilityManager* manager =
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

    // TODO(crbug.com/40774464) : Move this tracking to PageImpl.
    if (navigation_handle->IsInPrimaryMainFrame() &&
        !navigation_handle->IsSameDocument()) {
      was_ever_audible_ = false;
    }

    if (!navigation_handle->IsSameDocument()) {
      last_screen_orientation_change_time_ = base::TimeTicks();
    }
  }

  // If we didn't end up on about:blank after setting this in DidStartNavigation
  // then don't focus the location bar.
  if (should_focus_location_bar_by_default_ &&
      navigation_handle->GetURL() != url::kAboutBlankURL) {
    should_focus_location_bar_by_default_ = false;
  }

  if (navigation_handle->IsInPrimaryMainFrame() &&
      first_primary_navigation_completed_) {
    RecordMaxFrameCountUMA(max_loaded_frame_count_);
  }

  // If navigation has successfully finished in the main frame, set
  // |first_primary_navigation_completed_| to true so that we will record
  // |max_loaded_frame_count_| above when future main frame navigations finish.
  if (navigation_handle->IsInPrimaryMainFrame() &&
      !navigation_handle->IsErrorPage()) {
    first_primary_navigation_completed_ = true;

    // Navigation has completed in main frame. Reset |max_loaded_frame_count_|.
    // |max_loaded_frame_count_| is not necessarily 1 if the navigation was
    // served from BackForwardCache.
    max_loaded_frame_count_ = GetFrameTreeSize(&primary_frame_tree_);
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
    // bfcached renderers). Same for prerendering.
    // TODO(rakina): Maybe handle the back-forward cache case in
    // ReadyToCommitNavigation instead?
    // TODO(crbug.com/40758687): Maybe sync RendererPreferences as well?
    if (value_changed_due_to_override ||
        NavigationRequest::From(navigation_handle)->IsPageActivation()) {
      SetWebPreferences(*web_preferences_.get());
    }
  }

  if (navigation_handle->HasCommitted() &&
      navigation_handle->IsPrerenderedPageActivation()) {
    // We defer favicon and manifest URL updates while prerendering. Upon
    // activation, we must inform interested parties about our candidate favicon
    // URLs and the manifest URL.
    DCHECK(navigation_handle->IsInPrimaryMainFrame());
    auto* rfhi = static_cast<RenderFrameHostImpl*>(
        navigation_handle->GetRenderFrameHost());
    UpdateFaviconURL(rfhi, rfhi->FaviconURLs());
    OnManifestUrlChanged(rfhi->GetPage());

    // The page might have set its title while prerendering, and if it was, we
    // skipped notifying observers then, and we need to notify them now after
    // the page is activated.
    DCHECK(navigation_handle->IsInPrimaryMainFrame());
    NavigationEntryImpl* entry = GetController().GetLastCommittedEntry();
    DCHECK(entry);
    if (!entry->GetTitle().empty()) {
      NotifyTitleUpdateForEntry(entry);
    }
  }
}

void WebContentsImpl::DidCancelNavigationBeforeStart(
    NavigationHandle* navigation_handle) {
#if BUILDFLAG(IS_ANDROID)
  if (auto* animation_manager =
          static_cast<BackForwardTransitionAnimationManagerAndroid*>(
              GetBackForwardTransitionAnimationManager())) {
    animation_manager->OnNavigationCancelledBeforeStart(navigation_handle);
  }
#endif
}

void WebContentsImpl::DidFailLoadWithError(
    RenderFrameHostImpl* render_frame_host,
    const GURL& url,
    int error_code) {
  TRACE_EVENT2("content,navigation", "WebContentsImpl::DidFailLoadWithError",
               "render_frame_host", render_frame_host, "url", url);
  observers_.NotifyObservers(&WebContentsObserver::DidFailLoad,
                             render_frame_host, url, error_code);
}

void WebContentsImpl::DraggableRegionsChanged(
    const std::vector<blink::mojom::DraggableRegionPtr>& regions) {
  if (!GetDelegate()) {
    return;
  }
  GetDelegate()->DraggableRegionsChanged(regions, this);
}

void WebContentsImpl::NotifyChangedNavigationState(
    InvalidateTypes changed_flags) {
  NotifyNavigationStateChanged(changed_flags);
}

bool WebContentsImpl::ShouldAllowRendererInitiatedCrossProcessNavigation(
    bool is_outermost_main_frame_navigation) {
  OPTIONAL_TRACE_EVENT1(
      "content",
      "WebContentsImpl::ShouldAllowRendererInitiatedCrossProcessNavigation",
      "is_outermost_main_frame_navigation", is_outermost_main_frame_navigation);
  if (!delegate_) {
    return true;
  }
  return delegate_->ShouldAllowRendererInitiatedCrossProcessNavigation(
      is_outermost_main_frame_navigation);
}

bool WebContentsImpl::ShouldPreserveAbortedURLs() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::ShouldPreserveAbortedURLs");
  if (!delegate_) {
    return false;
  }
  return delegate_->ShouldPreserveAbortedURLs(this);
}

void WebContentsImpl::NotifyNavigationStateChangedFromController(
    InvalidateTypes changed_flags) {
  NotifyNavigationStateChanged(changed_flags);
}

input::TouchEmulator* WebContentsImpl::GetTouchEmulator(
    bool create_if_necessary) {
  CHECK(rwh_input_event_router_);

  if (!touch_emulator_ && create_if_necessary) {
    touch_emulator_ = std::make_unique<TouchEmulatorImpl>(
        rwh_input_event_router_.get(),
        rwh_input_event_router_->last_device_scale_factor());
  }

  return touch_emulator_.get();
}

void WebContentsImpl::DidNavigateMainFramePreCommit(
    NavigationHandle* navigation_handle,
    bool navigation_is_within_page) {
  auto* request = static_cast<NavigationRequest*>(navigation_handle);
  FrameTreeNode* frame_tree_node = request->frame_tree_node();

  // The `frame_tree_node` is always a main frame.
  DCHECK(frame_tree_node->IsMainFrame());
  TRACE_EVENT1("content,navigation",
               "WebContentsImpl::DidNavigateMainFramePreCommit",
               "navigation_is_within_page", navigation_is_within_page);
  const bool is_primary_mainframe =
      frame_tree_node->GetFrameType() == FrameType::kPrimaryMainFrame;
  // If running for a non-primary main frame, early out.
  if (!is_primary_mainframe) {
    return;
  }

#if BUILDFLAG(IS_ANDROID)
  auto* animation_manager =
      static_cast<BackForwardTransitionAnimationManagerAndroid*>(
          GetBackForwardTransitionAnimationManager());
  if (animation_manager) {
    animation_manager->OnDidNavigatePrimaryMainFramePreCommit(
        request, frame_tree_node->render_manager()->current_frame_host(),
        request->GetRenderFrameHost());
  }
#endif

  // Ensure fullscreen mode is exited before committing the navigation to a
  // different page.  The next page will not start out assuming it is in
  // fullscreen mode.
  if (navigation_is_within_page) {
    // No page change?  Then, the renderer and browser can remain in fullscreen.
    return;
  }

  if (IsFullscreen()) {
    ExitFullscreen(false);
  }

  auto* rwhvb = static_cast<RenderWidgetHostViewBase*>(
      frame_tree_node->current_frame_host()->GetView());
  if (rwhvb) {
    rwhvb->OnOldViewDidNavigatePreCommit();
  }

  // Clean up keyboard lock state when navigating.
  CancelKeyboardLock(keyboard_lock_widget_);
}

void WebContentsImpl::DidNavigateMainFramePostCommit(
    RenderFrameHostImpl* render_frame_host,
    const LoadCommittedDetails& details) {
  // The render_frame_host is always a main frame.
  DCHECK(render_frame_host->is_main_frame());
  OPTIONAL_TRACE_EVENT1("content,navigation",
                        "WebContentsImpl::DidNavigateMainFramePostCommit",
                        "render_frame_host", render_frame_host);
  const bool is_primary_main_frame = render_frame_host->IsInPrimaryMainFrame();

  auto* rwhvb = static_cast<RenderWidgetHostViewBase*>(
      render_frame_host->GetMainFrame()->GetView());

  if (details.is_navigation_to_different_page()) {
    if (is_primary_main_frame) {
      // Clear the status bubble. This is a workaround for a bug where WebKit
      // doesn't let us know that the cursor left an element during a
      // transition (this is also why the mouse cursor remains as a hand after
      // clicking on a link); see bugs 1184641 and 980803. We don't want to
      // clear the bubble when a user navigates to a named anchor in the same
      // page.
      ClearTargetURL();

      // Run the post-commit tasks on the new View.
      if (rwhvb) {
        rwhvb->OnNewViewDidNavigatePostCommit();
      }
    }
  }

  PageImpl& page = render_frame_host->GetPage();
  if (page.IsPrimary()) {
    // The following events will not fire again if the this is a back-forward
    // cache restore or prerendering activation. Fire them ourselves if needed.
    if (details.is_navigation_to_different_page() &&
        page.did_first_visually_non_empty_paint()) {
      OnFirstVisuallyNonEmptyPaint(page);
    }
    OnThemeColorChanged(page);
    OnBackgroundColorChanged(page);
    DidInferColorScheme(page);
    AXTreeIDForMainFrameHasChanged();
  }
}

void WebContentsImpl::DidNavigateAnyFramePostCommit(
    RenderFrameHostImpl* render_frame_host,
    const LoadCommittedDetails& details) {
  OPTIONAL_TRACE_EVENT1("content,navigation",
                        "WebContentsImpl::DidNavigateAnyFramePostCommit",
                        "render_frame_host", render_frame_host);

  // This function can be called by prerendered frames or other inactive frames,
  // we want to guard the dialog cancellations below.
  const bool is_active = render_frame_host->IsActive();

  // If we navigate off the active non-fenced frame page, close all JavaScript
  // dialogs. We do not cancel the dialogs for fenced frames because it can be
  // used as a communication channel. Please see also:
  // RenderFrameHostManager::UnloadOldFrame in
  // content/browser/renderer_host/render_frame_host_manager.cc
  //
  // TODO(crbug.com/40215909): Note that fenced frames cannot open modal dialogs
  // so this only affects dialogs outside the fenced frame tree. If this is ever
  // changed then the navigation should be deferred till the dialog from within
  // the fenced frame is open.
  if (is_active && !render_frame_host->IsNestedWithinFencedFrame() &&
      !details.is_same_document) {
    CancelActiveAndPendingDialogs();
  }

  // If this is a user-initiated navigation, start allowing JavaScript dialogs
  // again.
  //
  // TODO(crbug.com/40249773): Consider using the actual value of
  // "whether a navigation started with user activation or not" instead of
  // has_user_gesture, which might get filtered out when navigating from
  // proxies. If so, we can remove tracking of
  // `last_committed_common_params_has_user_gesture` entirely.
  if (render_frame_host->last_committed_common_params_has_user_gesture() &&
      dialog_manager_) {
    DCHECK(is_active);
    dialog_manager_->CancelDialogs(this, /*reset_state=*/true);
  }
}

void WebContentsImpl::DidUpdateNavigationHandleTiming(
    NavigationHandle* navigation_handle) {
  SCOPED_UMA_HISTOGRAM_TIMER(
      "WebContentsObserver.DidUpdateNavigationHandleTiming");
  observers_.NotifyObservers(
      &WebContentsObserver::DidUpdateNavigationHandleTiming, navigation_handle);
}

bool WebContentsImpl::CanOverscrollContent() const {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::CanOverscrollContent");
  // Disable overscroll when touch emulation is on. See crbug.com/369938.
  if (force_disable_overscroll_content_) {
    return false;
  }
  return delegate_ && delegate_->CanOverscrollContent();
}

void WebContentsImpl::OnThemeColorChanged(PageImpl& page) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::OnThemeColorChanged",
                        "page", page);
  if (!page.IsPrimary()) {
    return;
  }

  if (page.did_first_visually_non_empty_paint() &&
      last_sent_theme_color_ != page.theme_color()) {
    observers_.NotifyObservers(&WebContentsObserver::DidChangeThemeColor);
    last_sent_theme_color_ = page.theme_color();
  }
}

void WebContentsImpl::OnBackgroundColorChanged(PageImpl& page) {
  if (!page.IsPrimary()) {
    return;
  }

  if (page.did_first_visually_non_empty_paint() &&
      last_sent_background_color_ != page.background_color()) {
    observers_.NotifyObservers(&WebContentsObserver::OnBackgroundColorChanged);
    last_sent_background_color_ = page.background_color();
    return;
  }

  if (page.background_color().has_value()) {
    if (auto* view = GetRenderWidgetHostView()) {
      static_cast<RenderWidgetHostViewBase*>(view)->SetContentBackgroundColor(
          page.background_color().value());
    }
  }
}

void WebContentsImpl::DidInferColorScheme(PageImpl& page) {
  // If the page is primary, notify embedders that the current inferred color
  // scheme for this WebContents has changed.
  if (page.IsPrimary()) {
    observers_.NotifyObservers(&WebContentsObserver::InferredColorSchemeUpdated,
                               page.inferred_color_scheme());
    if (page.inferred_color_scheme().has_value()) {
      bool dark = page.inferred_color_scheme().value() ==
                  blink::mojom::PreferredColorScheme::kDark;
      base::UmaHistogramBoolean("Power.DarkMode.InferredDarkPageColorScheme",
                                dark);
      if (web_preferences_ && web_preferences_->preferred_color_scheme ==
                                  blink::mojom::PreferredColorScheme::kDark) {
        base::UmaHistogramBoolean(
            "Power.DarkMode.DarkColorScheme.InferredDarkPageColorScheme", dark);
      }
    }
  }
}

void WebContentsImpl::OnVirtualKeyboardModeChanged(PageImpl& page) {
  if (!page.IsPrimary()) {
    return;
  }

  observers_.NotifyObservers(&WebContentsObserver::VirtualKeyboardModeChanged,
                             page.virtual_keyboard_mode());
}

void WebContentsImpl::DidLoadResourceFromMemoryCache(
    RenderFrameHostImpl* source,
    const GURL& url,
    const std::string& http_method,
    const std::string& mime_type,
    network::mojom::RequestDestination request_destination,
    bool include_credentials) {
  OPTIONAL_TRACE_EVENT2("content",
                        "WebContentsImpl::DidLoadResourceFromMemoryCache",
                        "render_frame_host", source, "url", url);
  observers_.NotifyObservers(
      &WebContentsObserver::DidLoadResourceFromMemoryCache, source, url,
      mime_type, request_destination);

  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  StoragePartition* partition = source->GetProcess()->GetStoragePartition();

  // This method should only be called for resource loads (not navigations), so
  // CHECK that here using `request_destination`. Note that
  // `network::mojom::RequestDestination::kObject` and
  // `network::mojom::RequestDestination::kEmbed` can correspond to navigations
  // (see `blink::IsRequestDestinationFrame()`) but can also correspond to
  // resource loads, so exclude those from the CHECK.
  CHECK(request_destination != network::mojom::RequestDestination::kDocument);
  CHECK(!network::IsRequestDestinationEmbeddedFrame(request_destination));

  partition->GetNetworkContext()->NotifyExternalCacheHit(
      url, http_method, source->GetNetworkIsolationKey(),
      /*include_credentials=*/include_credentials);
}

void WebContentsImpl::PrimaryMainDocumentElementAvailable() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::PrimaryMainDocumentElementAvailable");
  SCOPED_UMA_HISTOGRAM_TIMER(
      "WebContentsObserver.PrimaryMainDocumentElementAvailable");
  observers_.NotifyObservers(
      &WebContentsObserver::PrimaryMainDocumentElementAvailable);
}

void WebContentsImpl::PassiveInsecureContentFound(const GURL& resource_url) {
  OPTIONAL_TRACE_EVENT1("content",
                        "WebContentsImpl::PassiveInsecureContentFound",
                        "resource_url", resource_url);
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
                        "render_frame_host", frame);
  DCHECK_EQ(this, WebContents::FromRenderFrameHost(frame));

  // Don't do anything if there is no |delegate_| that could accept and show the
  // new WebContents containing the view-source.
  if (!delegate_) {
    return;
  }

  // Use the last committed entry, since the pending entry hasn't loaded yet and
  // won't be copied into the cloned tab.
  NavigationEntryImpl* last_committed_entry =
      frame->frame_tree()->controller().GetLastCommittedEntry();
  if (!last_committed_entry) {
    return;
  }

  FrameNavigationEntry* frame_entry =
      last_committed_entry->GetFrameEntry(frame->frame_tree_node());
  if (!frame_entry) {
    return;
  }

  // Any new WebContents opened while this WebContents is in fullscreen can be
  // used to confuse the user, so drop fullscreen.
  base::ScopedClosureRunner fullscreen_block =
      ForSecurityDropFullscreen(/*display_id=*/display::kInvalidDisplayId);
  // The new view source contents will be independent of this contents, so
  // release the fullscreen block.
  fullscreen_block.RunAndReset();

  // We intentionally don't share the SiteInstance with the original frame so
  // that view source has a consistent process model and always ends up in a new
  // process (https://crbug.com/699493).
  scoped_refptr<SiteInstanceImpl> site_instance_for_view_source;
  // Referrer and initiator are not important, because view-source should not
  // hit the network, but should be served from the cache instead.
  Referrer referrer_for_view_source;
  std::optional<url::Origin> initiator_for_view_source = std::nullopt;
  std::optional<GURL> initiator_base_url_for_view_source = std::nullopt;
  // Do not restore title, derive it from the url.
  std::u16string title_for_view_source;
  auto navigation_entry = std::make_unique<NavigationEntryImpl>(
      site_instance_for_view_source, frame_entry->url(),
      referrer_for_view_source, initiator_for_view_source,
      initiator_base_url_for_view_source, title_for_view_source,
      ui::PAGE_TRANSITION_LINK,
      /* is_renderer_initiated = */ false,
      /* blob_url_loader_factory = */ nullptr, /* is_initial_entry = */ false);
  const GURL url(content::kViewSourceScheme + std::string(":") +
                 frame_entry->url().spec());
  navigation_entry->SetVirtualURL(url);

  // View source opens the URL in a new tab as a top-level navigation. A
  // top-level navigation may have a different IsolationInfo than the source
  // iframe, so preserve the IsolationInfo from the origin frame, to use the
  // same network shard and increase chances of a cache hit.
  navigation_entry->set_isolation_info(
      frame->ComputeIsolationInfoForNavigation(navigation_entry->GetURL()));

  // Do not restore scroller position.
  // TODO(creis, lukasza, arthursonzogni): Do not reuse the original PageState,
  // but start from a new one and only copy the needed data.
  const blink::PageState& new_page_state =
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
  view_source_contents->GetController().Restore(0, RestoreType::kRestored,
                                                &navigation_entries);

  // Add |view_source_contents| as a new tab.
  constexpr bool kUserGesture = true;
  bool ignored_was_blocked;
  delegate_->AddNewContents(this, std::move(view_source_contents), url,
                            WindowOpenDisposition::NEW_FOREGROUND_TAB,
                            blink::mojom::WindowFeatures(), kUserGesture,
                            &ignored_was_blocked);
  // Note that the |delegate_| could have deleted |view_source_contents| during
  // AddNewContents method call.
}

void WebContentsImpl::ResourceLoadComplete(
    RenderFrameHostImpl* render_frame_host,
    const GlobalRequestID& request_id,
    blink::mojom::ResourceLoadInfoPtr resource_load_info) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::ResourceLoadComplete",
                        "render_frame_host", render_frame_host, "request_id",
                        request_id);
  const blink::mojom::ResourceLoadInfo& resource_load_info_ref =
      *resource_load_info;
  observers_.NotifyObservers(&WebContentsObserver::ResourceLoadComplete,
                             render_frame_host, request_id,
                             resource_load_info_ref);
}

const blink::web_pref::WebPreferences&
WebContentsImpl::GetOrCreateWebPreferences() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::GetOrCreateWebPreferences");
  // Compute WebPreferences based on the current state if it's null.
  if (!web_preferences_) {
    OnWebPreferencesChanged();
  }
  return *web_preferences_.get();
}

void WebContentsImpl::SetWebPreferences(
    const blink::web_pref::WebPreferences& prefs) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::SetWebPreferences");
  web_preferences_ = std::make_unique<blink::web_pref::WebPreferences>(prefs);
  // Get all the RenderViewHosts (except the ones for currently back-forward
  // cached pages), and make them send the current WebPreferences
  // to the renderer. WebPreferences updates for back-forward cached pages will
  // be sent when we restore those pages from the back-forward cache.
  primary_frame_tree_.ForEachRenderViewHost(
      [](RenderViewHostImpl* rvh) { rvh->SendWebPreferencesToRenderer(); });
}

std::optional<SkColor> WebContentsImpl::GetBaseBackgroundColor() {
  return page_base_background_color_;
}

blink::ColorProviderColorMaps WebContentsImpl::GetColorProviderColorMaps()
    const {
  const auto* color_mode_source = GetColorProviderSource();

  // Unlike preferred color scheme, ForcedColors should always use the
  // default color provider source, which reflects the NativeTheme web instance.
  // This is because the Page colors feature only modifies the Forced colors
  // mode for web without affecting the UI.
  const auto* forced_colors_source = DefaultColorProviderSource::GetInstance();
  ui::ColorProviderKey::ForcedColors forced_colors =
      forced_colors_source->GetForcedColors();
  if (forced_colors == ui::ColorProviderKey::ForcedColors::kNone) {
    forced_colors = ui::ColorProviderKey::ForcedColors::kActive;
  }

  return blink::ColorProviderColorMaps{
      color_mode_source->GetRendererColorMap(
          ui::ColorProviderKey::ColorMode::kLight,
          ui::ColorProviderKey::ForcedColors::kNone),
      color_mode_source->GetRendererColorMap(
          ui::ColorProviderKey::ColorMode::kDark,
          ui::ColorProviderKey::ForcedColors::kNone),
      forced_colors_source->GetRendererColorMap(
          forced_colors_source->GetColorMode(), forced_colors)};
}

void WebContentsImpl::PrintCrossProcessSubframe(
    const gfx::Rect& rect,
    int document_cookie,
    RenderFrameHostImpl* subframe_host) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::PrintCrossProcessSubframe",
                        "subframe", subframe_host);
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
  if (!delegate_) {
    return;
  }

  delegate_->PrintCrossProcessSubframe(this, rect, document_cookie,
                                       subframe_host);
}

void WebContentsImpl::CapturePaintPreviewOfCrossProcessSubframe(
    const gfx::Rect& rect,
    const base::UnguessableToken& guid,
    RenderFrameHostImpl* render_frame_host) {
  OPTIONAL_TRACE_EVENT1(
      "content", "WebContentsImpl::CapturePaintPreviewOfCrossProcessSubframe",
      "render_frame_host", render_frame_host);
  if (!delegate_) {
    return;
  }
  delegate_->CapturePaintPreviewOfSubframe(this, rect, guid, render_frame_host);
}

#if BUILDFLAG(IS_ANDROID)
base::android::ScopedJavaLocalRef<jobject>
WebContentsImpl::GetJavaRenderFrameHostDelegate() {
  return GetJavaWebContents();
}
#endif

void WebContentsImpl::DOMContentLoaded(RenderFrameHostImpl* render_frame_host) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::DOMContentLoaded",
                        "render_frame_host", render_frame_host);
  SCOPED_UMA_HISTOGRAM_TIMER("WebContentsObserver.DOMContentLoaded");
  observers_.NotifyObservers(&WebContentsObserver::DOMContentLoaded,
                             render_frame_host);
}

void WebContentsImpl::OnDidFinishLoad(RenderFrameHostImpl* render_frame_host,
                                      const GURL& url) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::OnDidFinishLoad",
                        "render_frame_host", render_frame_host, "url", url);
  GURL validated_url(url);
  render_frame_host->GetProcess()->FilterURL(false, &validated_url);

  {
    SCOPED_UMA_HISTOGRAM_TIMER("WebContentsObserver.DidFinishLoad");
    observers_.NotifyObservers(&WebContentsObserver::DidFinishLoad,
                               render_frame_host, validated_url);
  }
  size_t tree_size = GetFrameTreeSize(&primary_frame_tree_);
  if (max_loaded_frame_count_ < tree_size) {
    max_loaded_frame_count_ = tree_size;
  }

  if (!render_frame_host->GetParentOrOuterDocument()) {
    UMA_HISTOGRAM_COUNTS_1000("Navigation.MainFrame.FrameCount", tree_size);
  }
}

bool WebContentsImpl::IsAllowedToGoToEntryAtOffset(int32_t offset) {
  // TODO(crbug.com/40165695): This should probably be renamed to
  // WebContentsDelegate::IsAllowedToGoToEntryAtOffset or
  // ShouldGoToEntryAtOffset
  return !delegate_ || delegate_->OnGoToEntryOffset(offset);
}

void WebContentsImpl::OnPageScaleFactorChanged(PageImpl& source) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::OnPageScaleFactorChanged",
                        "source", source);

  if (source.IsPrimary()) {
    observers_.NotifyObservers(&WebContentsObserver::OnPageScaleFactorChanged,
                               source.GetPageScaleFactor());
  }
}

void WebContentsImpl::EnumerateDirectory(
    base::WeakPtr<FileChooserImpl> file_chooser,
    RenderFrameHost* render_frame_host,
    scoped_refptr<FileChooserImpl::FileSelectListenerImpl> listener,
    const base::FilePath& directory_path) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::EnumerateDirectory",
                        "render_frame_host", render_frame_host,
                        "directory_path", directory_path);
  absl::Cleanup cancel_chooser = [&listener] {
    listener->FileSelectionCanceled();
  };
  if (visibility_ == Visibility::HIDDEN) {
    // Do not allow background tab to open file chooser.
    return;
  }
  if (active_file_chooser_) {
    // Only allow one active file chooser at one time.
    return;
  }

  // Any explicit focusing of another window while this WebContents is in
  // fullscreen can be used to confuse the user, so drop fullscreen.
  base::ScopedClosureRunner fullscreen_block =
      ForSecurityDropFullscreen(/*display_id=*/display::kInvalidDisplayId);
  listener->SetFullscreenBlock(std::move(fullscreen_block));

  if (delegate_) {
    active_file_chooser_ = std::move(file_chooser);
    delegate_->EnumerateDirectory(this, std::move(listener), directory_path);
    std::move(cancel_chooser).Cancel();
  }
}

void WebContentsImpl::RegisterProtocolHandler(RenderFrameHostImpl* source,
                                              const std::string& protocol,
                                              const GURL& url,
                                              bool user_gesture) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::RegisterProtocolHandler",
                        "render_frame_host", source, "protocol", protocol);
  // TODO(nick): Do we need to apply FilterURL to |url|?
  if (!delegate_) {
    return;
  }

  blink::ProtocolHandlerSecurityLevel security_level =
      delegate_->GetProtocolHandlerSecurityLevel(source);

  // Run the protocol handler arguments normalization process defined in the
  // spec.
  // https://html.spec.whatwg.org/multipage/system-state.html#normalize-protocol-handler-parameters
  if (!AreValidRegisterProtocolHandlerArguments(
          protocol, url, source->GetLastCommittedOrigin(), security_level)) {
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
                        "render_frame_host", source, "protocol", protocol);
  // TODO(nick): Do we need to apply FilterURL to |url|?
  if (!delegate_) {
    return;
  }

  blink::ProtocolHandlerSecurityLevel security_level =
      delegate_->GetProtocolHandlerSecurityLevel(source);

  if (!AreValidRegisterProtocolHandlerArguments(
          protocol, url, source->GetLastCommittedOrigin(), security_level)) {
    ReceivedBadMessage(source->GetProcess(),
                       bad_message::REGISTER_PROTOCOL_HANDLER_INVALID_URL);
    return;
  }

  delegate_->UnregisterProtocolHandler(source, protocol, url, user_gesture);
}

void WebContentsImpl::DomOperationResponse(RenderFrameHost* render_frame_host,
                                           const std::string& json_string) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::DomOperationResponse",
                        "render_frame_host", render_frame_host, "json_string",
                        json_string);

  observers_.NotifyObservers(&WebContentsObserver::DomOperationResponse,
                             render_frame_host, json_string);
}

void WebContentsImpl::SavableResourceLinksResponse(
    RenderFrameHostImpl* source,
    const std::vector<GURL>& resources_list,
    blink::mojom::ReferrerPtr referrer,
    const std::vector<blink::mojom::SavableSubframePtr>& subframes) {
  OPTIONAL_TRACE_EVENT1("content",
                        "WebContentsImpl::SavableResourceLinksResponse",
                        "render_frame_host", source);
  if (save_package_) {
    save_package_->SavableResourceLinksResponse(source, resources_list,
                                                std::move(referrer), subframes);
  }
}

void WebContentsImpl::SavableResourceLinksError(RenderFrameHostImpl* source) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::SavableResourceLinksError",
                        "render_frame_host", source);
  if (save_package_) {
    save_package_->SavableResourceLinksError(source);
  }
}

void WebContentsImpl::OnServiceWorkerAccessed(
    RenderFrameHost* render_frame_host,
    const GURL& scope,
    AllowServiceWorkerResult allowed) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::OnServiceWorkerAccessed",
                        "render_frame_host", render_frame_host, "scope", scope);
  // Use a variable to select between overloads.
  void (WebContentsObserver::*func)(RenderFrameHost*, const GURL&,
                                    AllowServiceWorkerResult) =
      &WebContentsObserver::OnServiceWorkerAccessed;
  observers_.NotifyObservers(func, render_frame_host, scope, allowed);
}

void WebContentsImpl::OnServiceWorkerAccessed(
    NavigationHandle* navigation,
    const GURL& scope,
    AllowServiceWorkerResult allowed) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::OnServiceWorkerAccessed",
                        "navigation_handle", navigation, "scope", scope);
  // Use a variable to select between overloads.
  void (WebContentsObserver::*func)(NavigationHandle*, const GURL&,
                                    AllowServiceWorkerResult) =
      &WebContentsObserver::OnServiceWorkerAccessed;
  observers_.NotifyObservers(func, navigation, scope, allowed);
}

void WebContentsImpl::OnColorChooserFactoryReceiver(
    mojo::PendingReceiver<blink::mojom::ColorChooserFactory> receiver) {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::OnColorChooserFactoryReceiver");
  color_chooser_factory_receivers_.Add(this, std::move(receiver));
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
void WebContentsImpl::OpenColorChooser(
    mojo::PendingReceiver<blink::mojom::ColorChooser> chooser_receiver,
    mojo::PendingRemote<blink::mojom::ColorChooserClient> client,
    SkColor color,
    std::vector<blink::mojom::ColorSuggestionPtr> suggestions) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::OpenColorChooser");
  // Create `color_chooser_holder_` before calling OpenColorChooser since
  // OpenColorChooser may callback with results.
  color_chooser_holder_.reset();
  color_chooser_holder_ = std::make_unique<ColorChooserHolder>(
      std::move(chooser_receiver), std::move(client));

  auto new_color_chooser =
      delegate_ ? delegate_->OpenColorChooser(this, color, suggestions)
                : nullptr;
  if (color_chooser_holder_ && new_color_chooser) {
    color_chooser_holder_->SetChooser(std::move(new_color_chooser));
  } else if (new_color_chooser) {
    // OpenColorChooser synchronously called back to DidEndColorChooser.
    DCHECK(!color_chooser_holder_);
    new_color_chooser->End();
  } else if (color_chooser_holder_) {
    DCHECK(!new_color_chooser);
    color_chooser_holder_.reset();
  }
}
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

#if BUILDFLAG(ENABLE_PPAPI)
void WebContentsImpl::OnPepperInstanceCreated(RenderFrameHostImpl* source,
                                              int32_t pp_instance) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::OnPepperInstanceCreated",
                        "render_frame_host", source);
  observers_.NotifyObservers(&WebContentsObserver::PepperInstanceCreated);
  pepper_playback_observer_->PepperInstanceCreated(source, pp_instance);
}

void WebContentsImpl::OnPepperInstanceDeleted(RenderFrameHostImpl* source,
                                              int32_t pp_instance) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::OnPepperInstanceDeleted",
                        "render_frame_host", source);
  observers_.NotifyObservers(&WebContentsObserver::PepperInstanceDeleted);
  pepper_playback_observer_->PepperInstanceDeleted(source, pp_instance);
}

void WebContentsImpl::OnPepperPluginHung(RenderFrameHostImpl* source,
                                         int plugin_child_id,
                                         const base::FilePath& path,
                                         bool is_hung) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::OnPepperPluginHung",
                        "render_frame_host", source);
  observers_.NotifyObservers(&WebContentsObserver::PluginHungStatusChanged,
                             plugin_child_id, path, is_hung);
}

void WebContentsImpl::OnPepperStartsPlayback(RenderFrameHostImpl* source,
                                             int32_t pp_instance) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::OnPepperStartsPlayback",
                        "render_frame_host", source);
  pepper_playback_observer_->PepperStartsPlayback(source, pp_instance);
}

void WebContentsImpl::OnPepperStopsPlayback(RenderFrameHostImpl* source,
                                            int32_t pp_instance) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::OnPepperStopsPlayback",
                        "render_frame_host", source);
  pepper_playback_observer_->PepperStopsPlayback(source, pp_instance);
}

void WebContentsImpl::OnPepperPluginCrashed(RenderFrameHostImpl* source,
                                            const base::FilePath& plugin_path,
                                            base::ProcessId plugin_pid) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::OnPepperPluginCrashed",
                        "render_frame_host", source);
  // TODO(nick): Eliminate the |plugin_pid| parameter, which can't be trusted,
  // and is only used by WebTestControlHost.
  observers_.NotifyObservers(&WebContentsObserver::PluginCrashed, plugin_path,
                             plugin_pid);
}

#endif  // BUILDFLAG(ENABLE_PPAPI)

void WebContentsImpl::UpdateFaviconURL(
    RenderFrameHostImpl* source,
    const std::vector<blink::mojom::FaviconURLPtr>& candidates) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::UpdateFaviconURL",
                        "render_frame_host", source);
  // We get updated favicon URLs after the page stops loading. If a cross-site
  // navigation occurs while a page is still loading, the initial page
  // may stop loading and send us updated favicon URLs after the navigation
  // for the new page has committed.
  if (!source->IsInPrimaryMainFrame()) {
    return;
  }

  observers_.NotifyObservers(&WebContentsObserver::DidUpdateFaviconURL, source,
                             candidates);
}

void WebContentsImpl::SetIsOverlayContent(bool is_overlay_content) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::SetIsOverlayContent",
                        "is_overlay_content", is_overlay_content);
  is_overlay_content_ = is_overlay_content;
}

void WebContentsImpl::OnFirstVisuallyNonEmptyPaint(PageImpl& page) {
  OPTIONAL_TRACE_EVENT1(
      "content", "WebContentsImpl::OnFirstVisuallyNonEmptyPaint", "page", page);
  if (!page.IsPrimary()) {
    return;
  }

  {
    SCOPED_UMA_HISTOGRAM_TIMER(
        "WebContentsObserver.DidFirstVisuallyNonEmptyPaint");
    observers_.NotifyObservers(
        &WebContentsObserver::DidFirstVisuallyNonEmptyPaint);
  }
  if (page.theme_color() != last_sent_theme_color_) {
    // Theme color should have updated by now if there was one.
    observers_.NotifyObservers(&WebContentsObserver::DidChangeThemeColor);
    last_sent_theme_color_ = GetPrimaryPage().theme_color();
  }

  if (page.background_color() != last_sent_background_color_) {
    // Background color should have updated by now if there was one.
    observers_.NotifyObservers(&WebContentsObserver::OnBackgroundColorChanged);
    last_sent_background_color_ = page.background_color();
  }
#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          features::kAndroidWarmUpSpareRendererWithTimeout) &&
      features::kAndroidSpareRendererCreationTiming.Get() ==
          features::kAndroidSpareRendererCreationAfterFirstPaint) {
    WarmUpAndroidSpareRenderer();
  }
#endif
}

bool WebContentsImpl::IsGuest() {
  return !!browser_plugin_guest_;
}

void WebContentsImpl::NotifyBeforeFormRepostWarningShow() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::NotifyBeforeFormRepostWarningShow");
  observers_.NotifyObservers(&WebContentsObserver::BeforeFormRepostWarningShow);
}

void WebContentsImpl::ActivateAndShowRepostFormWarningDialog() {
  OPTIONAL_TRACE_EVENT0(
      "content", "WebContentsImpl::ActivateAndShowRepostFormWarningDialog");
  Activate();
  if (delegate_) {
    delegate_->ShowRepostFormWarningDialog(this);
  }
}

bool WebContentsImpl::HasAccessedInitialDocument() {
  return GetPrimaryFrameTree().has_accessed_initial_main_document();
}

void WebContentsImpl::UpdateTitleForEntry(NavigationEntry* entry,
                                          const std::u16string& title) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::UpdateTitleForEntry",
                        "title", title);
  NavigationEntryImpl* entry_impl =
      NavigationEntryImpl::FromNavigationEntry(entry);
  bool title_changed = UpdateTitleForEntryImpl(entry_impl, title);
  if (title_changed) {
    NotifyTitleUpdateForEntry(entry_impl);
  }
}

bool WebContentsImpl::UpdateTitleForEntryImpl(NavigationEntryImpl* entry,
                                              const std::u16string& title) {
  DCHECK(entry);
  std::u16string final_title;
  base::TrimWhitespace(title, base::TRIM_ALL, &final_title);

  if (final_title == entry->GetTitle()) {
    return false;  // Nothing changed, don't bother.
  }

  entry->SetTitle(std::move(final_title));
  return true;
}

void WebContentsImpl::NotifyTitleUpdateForEntry(NavigationEntryImpl* entry) {
  // |entry| must belong to the primary frame tree's NavigationController.
  DCHECK(GetController().GetEntryWithUniqueIDIncludingPending(
      entry->GetUniqueID()));
  std::u16string final_title = entry->GetTitleForDisplay();
  bool did_web_contents_title_change = entry == GetNavigationEntryForTitle();
  if (did_web_contents_title_change) {
    view_->SetPageTitle(final_title);
  }

  {
    SCOPED_UMA_HISTOGRAM_TIMER("WebContentsObserver.TitleWasSet");
    observers_.NotifyObservers(&WebContentsObserver::TitleWasSet, entry);
  }

  if (did_web_contents_title_change) {
    NotifyNavigationStateChanged(INVALIDATE_TYPE_TITLE);
  }
}

NavigationEntry* WebContentsImpl::GetNavigationEntryForTitle() {
  // We use the title for the last committed entry rather than a pending
  // navigation entry. For example, when the user types in a URL, we want to
  // keep the old page's title until the new load has committed and we get a new
  // title.
  NavigationEntry* entry = GetController().GetLastCommittedEntry();

  // We make an exception for initial navigations. We only want to use the title
  // from the visible entry if:
  // 1. The pending entry has been explicitly assigned a title to display.
  // 2. The user is doing a history navigation in a new tab (e.g., Ctrl+Back),
  //    which case there is a pending entry index other than -1.
  //
  // Otherwise, we want to stick with the last committed entry's title during
  // new navigations, which have pending entries at index -1 with no title.
  if (GetController().IsInitialNavigation() &&
      ((GetController().GetVisibleEntry() &&
        !GetController().GetVisibleEntry()->GetTitle().empty()) ||
       GetController().GetPendingEntryIndex() != -1)) {
    entry = GetController().GetVisibleEntry();
  }

  return entry;
}

void WebContentsImpl::SendChangeLoadProgress() {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::SendChangeLoadProgress",
                        "load_progress", GetLoadProgress());
  loading_last_progress_update_ = base::TimeTicks::Now();

  SCOPED_UMA_HISTOGRAM_TIMER("WebContentsObserver.LoadProgressChanged");
  observers_.NotifyObservers(&WebContentsObserver::LoadProgressChanged,
                             GetLoadProgress());
}

void WebContentsImpl::ResetLoadProgressState() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::ResetLoadProgressState");
  GetPrimaryPage().set_load_progress(0.0);
  loading_weak_factory_.InvalidateWeakPtrs();
  loading_last_progress_update_ = base::TimeTicks();
}

// Notifies the RenderWidgetHost instance about the fact that the page is
// loading, or done loading.
void WebContentsImpl::LoadingStateChanged(LoadingState new_state) {
  if (IsBeingDestroyed()) {
    return;
  }

  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::LoadingStateChanged",
                        "loading_state", new_state);

  if (new_state == LoadingState::NONE) {
    load_state_ =
        net::LoadStateWithParam(net::LOAD_STATE_IDLE, std::u16string());
    load_state_host_.clear();
    upload_size_ = 0;
    upload_position_ = 0;
  }

  if (delegate_) {
    delegate_->LoadingStateChanged(
        this, new_state == LoadingState::LOADING_UI_REQUESTED);
  }
  NotifyNavigationStateChanged(INVALIDATE_TYPE_LOAD);
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
  observers_.NotifyObservers(&WebContentsObserver::RenderViewHostChanged,
                             old_view, new_view);
  view_->RenderViewHostChanged(old_view, new_view);

  // If this is an inner WebContents that has swapped views, we need to reattach
  // it to its outer WebContents.
  if (node_.outer_web_contents()) {
    ReattachToOuterWebContentsFrame();
  }

  // Ensure that the associated embedder gets cleared after a RenderViewHost
  // gets swapped, so we don't reuse the same embedder next time a
  // RenderViewHost is attached to this WebContents.
  RemoveBrowserPluginEmbedder();
}

void WebContentsImpl::NotifyFrameSwapped(RenderFrameHostImpl* old_frame,
                                         RenderFrameHostImpl* new_frame) {
  TRACE_EVENT2("content", "WebContentsImpl::NotifyFrameSwapped", "old_frame",
               old_frame, "new_frame", new_frame);
#if BUILDFLAG(IS_ANDROID)
  // Copy importance from |old_frame| if |new_frame| is a main frame.
  if (old_frame && !new_frame->GetParent()) {
    RenderWidgetHostImpl* old_widget = old_frame->GetRenderWidgetHost();
    RenderWidgetHostImpl* new_widget = new_frame->GetRenderWidgetHost();
    new_widget->SetImportance(old_widget->importance());
  }
#endif
  observers_.NotifyObservers(&WebContentsObserver::RenderFrameHostChanged,
                             old_frame, new_frame);
}

void WebContentsImpl::NotifyNavigationEntryCommitted(
    const LoadCommittedDetails& load_details) {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::NotifyNavigationEntryCommitted");
  SCOPED_UMA_HISTOGRAM_TIMER("WebContentsObserver.NavigationEntryCommitted");
  observers_.NotifyObservers(&WebContentsObserver::NavigationEntryCommitted,
                             load_details);
}

void WebContentsImpl::NotifyNavigationEntryChanged(
    const EntryChangedDetails& change_details) {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::NotifyNavigationEntryChanged");
  SCOPED_UMA_HISTOGRAM_TIMER("WebContentsObserver.NavigationEntryChanged");
  observers_.NotifyObservers(&WebContentsObserver::NavigationEntryChanged,
                             change_details);
}

void WebContentsImpl::NotifyNavigationListPruned(
    const PrunedDetails& pruned_details) {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::NotifyNavigationListPruned");
  observers_.NotifyObservers(&WebContentsObserver::NavigationListPruned,
                             pruned_details);
}

void WebContentsImpl::NotifyNavigationEntriesDeleted() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::NotifyNavigationEntriesDeleted");
  SCOPED_UMA_HISTOGRAM_TIMER("WebContentsObserver.NavigationEntryDeleted");
  observers_.NotifyObservers(&WebContentsObserver::NavigationEntriesDeleted);
}

void WebContentsImpl::OnDidBlockNavigation(
    const GURL& blocked_url,
    const GURL& initiator_url,
    blink::mojom::NavigationBlockedReason reason) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::OnDidBlockNavigation",
                        "details", [&](perfetto::TracedValue context) {
                          // TODO(crbug.com/40751990): Replace this with passing
                          // more parameters to TRACE_EVENT directly when
                          // available.
                          auto dict = std::move(context).WriteDictionary();
                          dict.Add("blocked_url", blocked_url);
                          dict.Add("initiator_url", initiator_url);
                          dict.Add("reason", reason);
                        });
  if (delegate_) {
    delegate_->OnDidBlockNavigation(this, blocked_url, initiator_url, reason);
  }
}

void WebContentsImpl::RenderFrameCreated(
    RenderFrameHostImpl* render_frame_host) {
  TRACE_EVENT1("content", "WebContentsImpl::RenderFrameCreated",
               "render_frame_host", render_frame_host);

  if (render_frame_host->IsInPrimaryMainFrame()) {
    NotifyPrimaryMainFrameProcessIsAlive();
  }

  {
    SCOPED_UMA_HISTOGRAM_TIMER("WebContentsObserver.RenderFrameCreated");
    observers_.NotifyObservers(&WebContentsObserver::RenderFrameCreated,
                               render_frame_host);
  }
  render_frame_host->UpdateAccessibilityMode();

  if (safe_area_insets_host_) {
    safe_area_insets_host_->RenderFrameCreated(render_frame_host);
  }
}

void WebContentsImpl::RenderFrameDeleted(
    RenderFrameHostImpl* render_frame_host) {
  TRACE_EVENT1("content", "WebContentsImpl::RenderFrameDeleted",
               "render_frame_host", render_frame_host);
  {
    SCOPED_UMA_HISTOGRAM_TIMER("WebContentsObserver.RenderFrameDeleted");
    observers_.NotifyObservers(&WebContentsObserver::RenderFrameDeleted,
                               render_frame_host);
  }
#if BUILDFLAG(ENABLE_PPAPI)
  pepper_playback_observer_->RenderFrameDeleted(render_frame_host);
#endif

  if (safe_area_insets_host_) {
    safe_area_insets_host_->RenderFrameDeleted(render_frame_host);
  }

  // Remove any fullscreen state that the frame has stored.
  FullscreenStateChanged(render_frame_host, false /* is_fullscreen */,
                         blink::mojom::FullscreenOptionsPtr());
}

void WebContentsImpl::ShowContextMenu(
    RenderFrameHost& render_frame_host,
    mojo::PendingAssociatedRemote<blink::mojom::ContextMenuClient>
        context_menu_client,
    const ContextMenuParams& params) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::ShowContextMenu",
                        "render_frame_host", render_frame_host);
  // If a renderer fires off a second command to show a context menu before the
  // first context menu is closed, just ignore it. https://crbug.com/707534
  if (showing_context_menu_) {
    return;
  }

  if (context_menu_client) {
    context_menu_client_.reset();
    context_menu_client_.Bind(std::move(context_menu_client));
  }

  ContextMenuParams context_menu_params(params);
  // Allow WebContentsDelegates to handle the context menu operation first.
  if (delegate_ &&
      delegate_->HandleContextMenu(render_frame_host, context_menu_params)) {
    return;
  }

  render_view_host_delegate_view_->ShowContextMenu(render_frame_host,
                                                   context_menu_params);
}

namespace {
// Normalizes the line endings: \r\n -> \n, lone \r -> \n.
std::u16string NormalizeLineBreaks(const std::u16string& source) {
  static const base::NoDestructor<std::u16string> kReturnNewline(u"\r\n");
  static const base::NoDestructor<std::u16string> kReturn(u"\r");
  static const base::NoDestructor<std::u16string> kNewline(u"\n");

  std::vector<std::u16string_view> pieces;

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
    RenderFrameHostImpl* render_frame_host,
    const std::u16string& message,
    const std::u16string& default_prompt,
    JavaScriptDialogType dialog_type,
    bool disable_third_party_subframe_suppresion,
    JavaScriptDialogCallback response_callback) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::RunJavaScriptDialog",
                        "render_frame_host", render_frame_host);
  DCHECK(render_frame_host->GetPage().IsPrimary());

  // Ensure that if showing a dialog is the first thing that a page does, that
  // the contents of the previous page aren't shown behind it. This is required
  // because showing a dialog freezes the renderer, so no frames will be coming
  // from it. https://crbug.com/823353
  auto* render_widget_host_impl = render_frame_host->GetRenderWidgetHost();
  if (render_widget_host_impl) {
    render_widget_host_impl->ForceFirstFrameAfterNavigationTimeout();
  }

  // Running a dialog causes an exit to webpage-initiated fullscreen.
  // http://crbug.com/728276
  base::ScopedClosureRunner fullscreen_block =
      ForSecurityDropFullscreen(/*display_id=*/display::kInvalidDisplayId);

  auto callback = base::BindOnce(
      &WebContentsImpl::OnDialogClosed, weak_factory_.GetWeakPtr(),
      render_frame_host->GetProcess()->GetID(),
      render_frame_host->GetRoutingID(), std::move(response_callback),
      std::move(fullscreen_block));

  std::vector<protocol::PageHandler*> page_handlers =
      protocol::PageHandler::EnabledForWebContents(this);

  if (delegate_) {
    dialog_manager_ = delegate_->GetJavaScriptDialogManager(this);
  }

  // While a JS message dialog is showing, defer commits in this WebContents.
  javascript_dialog_dismiss_notifier_ =
      std::make_unique<JavaScriptDialogDismissNotifier>();

  // Suppress JavaScript dialogs when requested.
  bool should_suppress = delegate_ && delegate_->ShouldSuppressDialogs(this);
  bool has_non_devtools_handlers = delegate_ && dialog_manager_;
  bool has_handlers = page_handlers.size() || has_non_devtools_handlers;
  bool suppress_this_message = should_suppress || !has_handlers;

  if (!disable_third_party_subframe_suppresion &&
      GetContentClient()->browser()->SuppressDifferentOriginSubframeJSDialogs(
          GetBrowserContext())) {
    // We can't check for opaque origin cases, default to allowing them to
    // trigger dialogs.
    // TODO(carlosil): The main use case for opaque use cases are tests,
    // investigate if there are uses in the wild, otherwise adapt tests that
    // require dialogs so they commit an origin first, and remove this
    // conditional.
    if (!render_frame_host->GetLastCommittedOrigin().opaque()) {
      bool is_different_origin_subframe =
          render_frame_host->GetLastCommittedOrigin() !=
          render_frame_host->GetOutermostMainFrame()->GetLastCommittedOrigin();
      suppress_this_message |= is_different_origin_subframe;
      if (is_different_origin_subframe) {
        GetPrimaryMainFrame()->AddMessageToConsole(
            blink::mojom::ConsoleMessageLevel::kWarning,
            base::StringPrintf(
                "A different origin subframe tried to create a JavaScript "
                "dialog. This is no longer allowed and was blocked. See "
                "https://www.chromestatus.com/feature/5148698084376576 for "
                "more details."));
      }
    }
  }

  if (suppress_this_message) {
    std::move(callback).Run(true, false, std::u16string());
    return;
  }

  scoped_refptr<CloseDialogCallbackWrapper> wrapper =
      new CloseDialogCallbackWrapper(std::move(callback));

  is_showing_javascript_dialog_ = true;

  std::u16string normalized_message = NormalizeLineBreaks(message);

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
    wrapper->Run(true, false, std::u16string());
  }
}

void WebContentsImpl::NotifyOnJavaScriptDialogDismiss(
    base::OnceClosure callback) {
  DCHECK(javascript_dialog_dismiss_notifier_);
  javascript_dialog_dismiss_notifier_->NotifyOnDismiss(std::move(callback));
}

void WebContentsImpl::RunBeforeUnloadConfirm(
    RenderFrameHostImpl* render_frame_host,
    bool is_reload,
    JavaScriptDialogCallback response_callback) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::RunBeforeUnloadConfirm",
                        "render_frame_host", render_frame_host, "is_reload",
                        is_reload);
  DCHECK(render_frame_host->GetPage().IsPrimary());

  // Ensure that if showing a dialog is the first thing that a page does, that
  // the contents of the previous page aren't shown behind it. This is required
  // because showing a dialog freezes the renderer, so no frames will be coming
  // from it. https://crbug.com/823353
  auto* render_widget_host_impl = render_frame_host->GetRenderWidgetHost();
  if (render_widget_host_impl) {
    render_widget_host_impl->ForceFirstFrameAfterNavigationTimeout();
  }

  // Running a dialog causes an exit to webpage-initiated fullscreen.
  // http://crbug.com/728276
  base::ScopedClosureRunner fullscreen_block =
      ForSecurityDropFullscreen(/*display_id=*/display::kInvalidDisplayId);

  auto callback = base::BindOnce(
      &WebContentsImpl::OnDialogClosed, weak_factory_.GetWeakPtr(),
      render_frame_host->GetProcess()->GetID(),
      render_frame_host->GetRoutingID(), std::move(response_callback),
      std::move(fullscreen_block));

  std::vector<protocol::PageHandler*> page_handlers =
      protocol::PageHandler::EnabledForWebContents(this);

  if (delegate_) {
    dialog_manager_ = delegate_->GetJavaScriptDialogManager(this);
  }

  // While a JS beforeunload dialog is showing, defer commits in this
  // WebContents.
  javascript_dialog_dismiss_notifier_ =
      std::make_unique<JavaScriptDialogDismissNotifier>();

  bool should_suppress = delegate_ && delegate_->ShouldSuppressDialogs(this);
  bool has_non_devtools_handlers = delegate_ && dialog_manager_;
  bool has_handlers = page_handlers.size() || has_non_devtools_handlers;
  if (should_suppress || !has_handlers) {
    std::move(callback).Run(false, true, std::u16string());
    return;
  }

  is_showing_before_unload_dialog_ = true;

  scoped_refptr<CloseDialogCallbackWrapper> wrapper =
      new CloseDialogCallbackWrapper(std::move(callback));

  GURL frame_url = render_frame_host->GetLastCommittedURL();
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
    base::WeakPtr<FileChooserImpl> file_chooser,
    RenderFrameHost* render_frame_host,
    scoped_refptr<FileChooserImpl::FileSelectListenerImpl> listener,
    const blink::mojom::FileChooserParams& params) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::RunFileChooser",
                        "render_frame_host", render_frame_host);

  absl::Cleanup cancel_chooser = [&listener] {
    listener->FileSelectionCanceled();
  };
  if (visibility_ == Visibility::HIDDEN) {
    // Do not allow background tab to open file chooser.
    return;
  }
  if (active_file_chooser_) {
    // Only allow one active file chooser at one time.
    return;
  }

  // Any explicit focusing of another window while this WebContents is in
  // fullscreen can be used to confuse the user, so drop fullscreen.
  base::ScopedClosureRunner fullscreen_block =
      ForSecurityDropFullscreen(/*display_id=*/display::kInvalidDisplayId);
  listener->SetFullscreenBlock(std::move(fullscreen_block));

  if (delegate_) {
    active_file_chooser_ = std::move(file_chooser);
    delegate_->RunFileChooser(render_frame_host, std::move(listener), params);
    std::move(cancel_chooser).Cancel();
  }
}

double WebContentsImpl::GetPendingPageZoomLevel() {
#if BUILDFLAG(IS_ANDROID)
  // On Android, use the default page zoom level when the AccessibilityPageZoom
  // feature is disabled.
  if (!base::FeatureList::IsEnabled(features::kAccessibilityPageZoom)) {
    return 0.0;
  }
#endif
  NavigationEntry* pending_entry = GetController().GetPendingEntry();
  if (!pending_entry) {
    return HostZoomMap::GetZoomLevel(this);
  }

  GURL url = pending_entry->GetURL();
#if BUILDFLAG(IS_ANDROID)
  return HostZoomMap::GetForWebContents(this)
      ->GetZoomLevelForHostAndSchemeAndroid(url.scheme(),
                                            net::GetHostOrSpecFromURL(url));
#else
  return HostZoomMap::GetForWebContents(this)->GetZoomLevelForHostAndScheme(
      url.scheme(), net::GetHostOrSpecFromURL(url));
#endif
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
  if (auto* frame = GetFocusedFrame()) {
    frame->ClearFocusedElement();
  }
}

bool WebContentsImpl::IsNeverComposited() {
  if (!delegate_) {
    return false;
  }
  return delegate_->IsNeverComposited(this);
}

RenderViewHostDelegateView* WebContentsImpl::GetDelegateView() {
  return render_view_host_delegate_view_;
}

const blink::RendererPreferences& WebContentsImpl::GetRendererPrefs() const {
  return renderer_preferences_;
}

RenderFrameHostImpl* WebContentsImpl::GetOuterWebContentsFrame() {
  if (GetOuterDelegateFrameTreeNodeId().is_null()) {
    return nullptr;
  }

  FrameTreeNode* outer_node =
      FrameTreeNode::GloballyFindByID(GetOuterDelegateFrameTreeNodeId());
  // The outer node should be in the outer WebContents.
  DCHECK_EQ(&outer_node->frame_tree(),
            &GetOuterWebContents()->GetPrimaryFrameTree());
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
  return FromRenderFrameHostImpl(
      GetPrimaryMainFrame()->GetOutermostMainFrameOrEmbedder());
}

WebContentsImpl* WebContentsImpl::GetFocusedWebContents() {
  return WebContentsImpl::FromFrameTreeNode(GetFocusedFrameTree()->root());
}

FrameTree* WebContentsImpl::GetFocusedFrameTree() {
  return GetOutermostWebContents()->node_.focused_frame_tree();
}

void WebContentsImpl::SetFocusToLocationBar() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::SetFocusToLocationBar");
  if (delegate_) {
    delegate_->SetFocusToLocationBar();
  }
}

bool WebContentsImpl::ContainsOrIsFocusedWebContents() {
  for (WebContentsImpl* focused_contents = GetFocusedWebContents();
       focused_contents;
       focused_contents = focused_contents->GetOuterWebContents()) {
    if (focused_contents == this) {
      return true;
    }
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
  while (root->GetOuterWebContents()) {
    root = root->GetOuterWebContents();
  }
  return root;
}

void WebContentsImpl::InnerWebContentsCreated(WebContents* inner_web_contents) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::InnerWebContentsCreated");
  observers_.NotifyObservers(&WebContentsObserver::InnerWebContentsCreated,
                             inner_web_contents);
}

void WebContentsImpl::InnerWebContentsAttached(
    WebContents* inner_web_contents) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::InnerWebContentsDetached");
  if (inner_web_contents->IsCurrentlyAudible()) {
    OnAudioStateChanged();
  }
}

void WebContentsImpl::InnerWebContentsDetached(
    WebContents* inner_web_contents) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::InnerWebContentsCreated");
  if (!IsBeingDestroyed()) {
    OnAudioStateChanged();
  }
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
  if (rwhv) {
    rwhv->SetMainFrameAXTreeID(GetPrimaryMainFrame()->GetAXTreeID());
  }

  notify_disconnection_ = true;

  {
    SCOPED_UMA_HISTOGRAM_TIMER("WebContentsObserver.RenderViewReady");
    observers_.NotifyObservers(&WebContentsObserver::RenderViewReady);
  }
  view_->RenderViewReady();
}

void WebContentsImpl::RenderViewTerminated(RenderViewHost* rvh,
                                           base::TerminationStatus status,
                                           int error_code) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::RenderViewTerminated",
                        "render_view_host", rvh, "status",
                        static_cast<int>(status));

  // It is possible to get here while the WebContentsImpl is being destroyed,
  // in particular when the destruction of the main frame's RenderFrameHost and
  // RenderViewHost triggers cleanup of the main frame's process, which in turn
  // dispatches RenderProcessExited observers, one of which calls in here.  In
  // this state, we cannot check GetRenderViewHost() below, since
  // current_frame_host() for the root FrameTreeNode has already been cleared.
  // Since the WebContents is going away, none of the work here is needed, so
  // just return early.
  if (IsBeingDestroyed()) {
    return;
  }

  if (rvh != GetRenderViewHost()) {
    // The pending page's RenderViewHost is gone.
    return;
  }

  auto* rvh_impl = static_cast<RenderViewHostImpl*>(rvh);
  DCHECK(rvh_impl->GetMainRenderFrameHost()->IsInPrimaryMainFrame())
      << "GetRenderViewHost() must belong to the primary frame tree";

  // Ensure fullscreen mode is exited in the |delegate_| since a crashed
  // renderer may not have made a clean exit.
  if (IsFullscreen()) {
    ExitFullscreenMode(false);
  }

  // Ensure any video or document in Picture-in-Picture is exited in the
  // |delegate_| since a crashed renderer may not have made a clean exit.
  if (HasPictureInPictureVideo() || HasPictureInPictureDocument()) {
    ExitPictureInPicture();
  }

  // Cancel any visible dialogs so they are not left dangling over the sad tab.
  CancelActiveAndPendingDialogs();

  audio_stream_monitor_.RenderProcessGone(rvh_impl->GetProcess()->GetID());

  // Reset the loading progress. TODO(avi): What does it mean to have a
  // "renderer crash" when there is more than one renderer process serving a
  // webpage? Once this function is called at a more granular frame level, we
  // probably will need to more granularly reset the state here.
  ResetLoadProgressState();

  SetPrimaryMainFrameProcessStatus(status, error_code);

  TRACE_EVENT0(
      "content",
      "Dispatching WebContentsObserver::PrimaryMainFrameRenderProcessGone");
  // Some observers might destroy WebContents in
  // PrimaryMainFrameRenderProcessGone.
  base::WeakPtr<WebContentsImpl> weak_ptr = weak_factory_.GetWeakPtr();
  for (auto& observer : observers_.observer_list()) {
    observer.PrimaryMainFrameRenderProcessGone(status);
    if (!weak_ptr) {
      return;
    }
  }

  // |this| might have been deleted. Do not add code here.
}

void WebContentsImpl::RenderViewDeleted(RenderViewHost* rvh) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::RenderViewDeleted",
                        "render_view_host", rvh);
  observers_.NotifyObservers(&WebContentsObserver::RenderViewDeleted, rvh);
}

void WebContentsImpl::ClearTargetURL() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::ClearTargetURL");
  frame_that_set_last_target_url_ = nullptr;
  if (delegate_) {
    delegate_->UpdateTargetURL(this, GURL());
  }
}

void WebContentsImpl::Close() {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::Close",
                        "render_frame_host", GetPrimaryMainFrame());
#if BUILDFLAG(IS_MAC)
  // The UI may be in an event-tracking loop, such as between the
  // mouse-down and mouse-up in text selection or a button click.
  // Defer the close until after tracking is complete, so that we
  // don't free objects out from under the UI.
  // TODO(shess): This could get more fine-grained.  For instance,
  // closing a tab in another window while selecting text in the
  // current window's Omnibox should be just fine.
  if (view_->CloseTabAfterEventTrackingIfNeeded()) {
    return;
  }
#endif

  if (delegate_) {
    delegate_->CloseContents(this);
  }
}

void WebContentsImpl::SetWindowRect(const gfx::Rect& new_bounds) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::SetWindowRect");
  if (!delegate_) {
    return;
  }

  // Members of |new_bounds| may be 0 to indicate uninitialized values for newly
  // opened windows, even if the |GetContainerBounds()| inner rect is correct.
  // TODO(crbug.com/40092782): Plumb values as specified; fallback on outer
  // rect.
  auto bounds = new_bounds;
  if (bounds.IsEmpty()) {
    bounds.set_size(GetContainerBounds().size());
  }

  // Only requests from the main frame, not subframes, should reach this code.
  int64_t display_id = AdjustWindowRect(&bounds, GetPrimaryMainFrame());

  // Drop fullscreen when placing a WebContents to prohibit deceptive behavior.
  // Only drop fullscreen on the specific destination display, which is known.
  // This supports sites using cross-screen window management capabilities to
  // retain fullscreen and place a window on another screen.
  ForSecurityDropFullscreen(display_id).RunAndReset();

  delegate_->SetContentsBounds(this, bounds);
}

void WebContentsImpl::UpdateWindowPreferredSize(const gfx::Size& pref_size) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::UpdatePreferredSize");
  const gfx::Size old_size = GetPreferredSize();
  preferred_size_ = pref_size;
  OnPreferredSizeChanged(old_size);
}

std::vector<RenderFrameHostImpl*>
WebContentsImpl::GetActiveTopLevelDocumentsInGroup(
    RenderFrameHostImpl* render_frame_host,
    GroupType group_type) {
  std::vector<RenderFrameHostImpl*> out;
  for (WebContentsImpl* web_contents : GetAllWebContents()) {
    RenderFrameHostImpl* other_render_frame_host =
        web_contents->GetPrimaryMainFrame();

    // Filters out inactive documents.
    if (other_render_frame_host->lifecycle_state() !=
        RenderFrameHostImpl::LifecycleStateImpl::kActive) {
      continue;
    }

    // If we're looking for frames in the same browsing context group, filter
    // frames in different browsing context groups.
    if (group_type == GroupType::kBrowsingContextGroup &&
        !render_frame_host->GetSiteInstance()->IsRelatedSiteInstance(
            other_render_frame_host->GetSiteInstance())) {
      continue;
    }

    // If we're looking for frames in the same CoopRelatedGroup, filter frames
    // in different CoopRelatedGroups.
    if (group_type == GroupType::kCoopRelatedGroup &&
        !render_frame_host->GetSiteInstance()->IsCoopRelatedSiteInstance(
            other_render_frame_host->GetSiteInstance())) {
      continue;
    }

    out.push_back(other_render_frame_host);
  }
  return out;
}

std::vector<RenderFrameHostImpl*>
WebContentsImpl::GetActiveTopLevelDocumentsInBrowsingContextGroup(
    RenderFrameHostImpl* render_frame_host) {
  return GetActiveTopLevelDocumentsInGroup(render_frame_host,
                                           GroupType::kBrowsingContextGroup);
}

std::vector<RenderFrameHostImpl*>
WebContentsImpl::GetActiveTopLevelDocumentsInCoopRelatedGroup(
    RenderFrameHostImpl* render_frame_host) {
  return GetActiveTopLevelDocumentsInGroup(render_frame_host,
                                           GroupType::kCoopRelatedGroup);
}

PrerenderHostRegistry* WebContentsImpl::GetPrerenderHostRegistry() {
  DCHECK(prerender_host_registry_);
  return prerender_host_registry_.get();
}

void WebContentsImpl::DidStartLoading(FrameTreeNode* frame_tree_node) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::DidStartLoading",
                        "frame_tree_node", frame_tree_node);

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN2(
      "browser,navigation", "WebContentsImpl Loading", this, "URL", "NULL",
      "Primary Main FrameTreeNode id",
      GetPrimaryFrameTree().root()->frame_tree_node_id());
  SCOPED_UMA_HISTOGRAM_TIMER("WebContentsObserver.DidStartLoading");
  observers_.NotifyObservers(&WebContentsObserver::DidStartLoading);

  // Reset the focus state from DidStartNavigation to false if a new load starts
  // afterward, in case loading logic triggers a FocusLocationBarByDefault call.
  should_focus_location_bar_by_default_ = false;

  // Notify accessibility that the user is navigating away from the
  // current document.
  // TODO(domfarolino, dmazzoni): Do this using WebContentsObserver. See
  // https://crbug.com/981271.
  ui::BrowserAccessibilityManager* manager =
      frame_tree_node->current_frame_host()->browser_accessibility_manager();
  if (manager) {
    manager->UserIsNavigatingAway();
  }
}

void WebContentsImpl::DidStopLoading() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::DidStopLoading");
  if (IsBeingDestroyed()) {
    return;
  }

  // Use the last committed entry rather than the active one, in case a
  // pending entry has been created.
  // An entry may not exist for a stop when loading an initial blank page or
  // if an iframe injected by script into a blank page finishes loading.
  NavigationEntry* entry = GetController().GetLastCommittedEntry();
  std::string url =
      (entry ? entry->GetVirtualURL().possibly_invalid_spec() : "NULL");

  TRACE_EVENT_NESTABLE_ASYNC_END1("browser,navigation",
                                  "WebContentsImpl Loading", this, "URL", url);
  SCOPED_UMA_HISTOGRAM_TIMER("WebContentsObserver.DidStopLoading");
  observers_.NotifyObservers(&WebContentsObserver::DidStopLoading);

  GetPrimaryMainFrame()->ForEachRenderFrameHost(
      [](RenderFrameHostImpl* render_frame_host) {
        ui::BrowserAccessibilityManager* manager =
            render_frame_host->browser_accessibility_manager();
        if (manager) {
          manager->DidStopLoading();
        }
      });

#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          features::kAndroidWarmUpSpareRendererWithTimeout) &&
      features::kAndroidSpareRendererCreationTiming.Get() ==
          features::kAndroidSpareRendererCreationAfterLoading) {
    WarmUpAndroidSpareRenderer();
  }
#endif
}

void WebContentsImpl::DidChangeLoadProgressForPrimaryMainFrame() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::DidChangeLoadProgress");
  if (IsBeingDestroyed()) {
    return;
  }
  double load_progress = GetLoadProgress();

  // The delegate is notified immediately for the first and last updates. Also,
  // since the message loop may be pretty busy when a page is loaded, it might
  // not execute a posted task in a timely manner so the progress report is sent
  // immediately if enough time has passed.
  base::TimeDelta min_delay = minimum_delay_between_loading_updates_ms_;
  bool delay_elapsed =
      loading_last_progress_update_.is_null() ||
      base::TimeTicks::Now() - loading_last_progress_update_ > min_delay;

  if (load_progress == 0.0 || load_progress == 1.0 || delay_elapsed) {
    // If there is a pending task to send progress, it is now obsolete.
    loading_weak_factory_.InvalidateWeakPtrs();

    // Notify the load progress change.
    SendChangeLoadProgress();

    // Clean-up the states if needed.
    if (load_progress == 1.0) {
      ResetLoadProgressState();
    }
    return;
  }

  if (loading_weak_factory_.HasWeakPtrs()) {
    return;
  }

  if (min_delay == base::Milliseconds(0)) {
    SendChangeLoadProgress();
  } else {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&WebContentsImpl::SendChangeLoadProgress,
                       loading_weak_factory_.GetWeakPtr()),
        min_delay);
  }
}

bool WebContentsImpl::IsHidden() {
  return GetPageVisibilityState() == PageVisibilityState::kHidden;
}

std::vector<std::unique_ptr<NavigationThrottle>>
WebContentsImpl::CreateThrottlesForNavigation(
    NavigationHandle* navigation_handle) {
  OPTIONAL_TRACE_EVENT1("content",
                        "WebContentsImpl::CreateThrottlesForNavigation",
                        "navigation", navigation_handle);
  auto throttles = GetContentClient()->browser()->CreateThrottlesForNavigation(
      navigation_handle);

  return throttles;
}

std::vector<std::unique_ptr<CommitDeferringCondition>>
WebContentsImpl::CreateDeferringConditionsForNavigationCommit(
    NavigationHandle& navigation_handle,
    CommitDeferringCondition::NavigationType type) {
  OPTIONAL_TRACE_EVENT2(
      "content", "WebContentsImpl::CreateDeferringConditionsForNavigation",
      "navigation", navigation_handle, "NavigationType", type);
  std::vector<std::unique_ptr<CommitDeferringCondition>> conditions =
      GetContentClient()
          ->browser()
          ->CreateCommitDeferringConditionsForNavigation(&navigation_handle,
                                                         type);

  if (auto condition = JavaScriptDialogCommitDeferringCondition::MaybeCreate(
          static_cast<NavigationRequest&>(navigation_handle))) {
    conditions.push_back(std::move(condition));
  }
  return conditions;
}

std::unique_ptr<NavigationUIData> WebContentsImpl::GetNavigationUIData(
    NavigationHandle* navigation_handle) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::GetNavigationUIData");
  return GetContentClient()->browser()->GetNavigationUIData(navigation_handle);
}

void WebContentsImpl::RegisterExistingOriginAsHavingDefaultIsolation(
    const url::Origin& origin,
    NavigationRequest* navigation_request_to_exclude) {
  OPTIONAL_TRACE_EVENT2(
      "content",
      "WebContentsImpl::RegisterExistingOriginAsHavingDefaultIsolation",
      "origin", origin, "navigation_request_to_exclude",
      navigation_request_to_exclude);
  // Note: This function can be made static if we ever need call it without
  // a WebContentsImpl instance, in which case we can use a wrapper to
  // implement the override from NavigatorDelegate.
  for (WebContentsImpl* web_contents : GetAllWebContents()) {
    // We only need to search entries in the same BrowserContext as us.
    if (web_contents->GetBrowserContext() != GetBrowserContext()) {
      continue;
    }

    // Walk the frame trees to notify each one's NavigationController about the
    // opting-in or opting-out `origin`, and also pick up any frames without
    // FrameNavigationEntries.
    // * Some frames won't have FrameNavigationEntries (Issues 524208, 608402).
    // * Some pending navigations won't have NavigationEntries.
    web_contents->ForEachFrameTree(base::BindRepeating(
        [](const url::Origin& origin,
           NavigationRequest* navigation_request_to_exclude,
           FrameTree& frame_tree) {
          frame_tree.RegisterExistingOriginAsHavingDefaultIsolation(
              origin, navigation_request_to_exclude);
        },
        origin, navigation_request_to_exclude));
  }
}

bool WebContentsImpl::MaybeCopyContentAreaAsBitmap(
    base::OnceCallback<void(const SkBitmap&)> callback) {
  if (!GetDelegate()) {
    return false;
  }
  return GetDelegate()->MaybeCopyContentAreaAsBitmap(std::move(callback));
}

void WebContentsImpl::DidChangeName(RenderFrameHostImpl* render_frame_host,
                                    const std::string& name) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::DidChangeName",
                        "render_frame_host", render_frame_host, "name", name);
  observers_.NotifyObservers(&WebContentsObserver::FrameNameChanged,
                             render_frame_host, name);
}

void WebContentsImpl::DidReceiveUserActivation(
    RenderFrameHostImpl* render_frame_host) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::DidReceiveUserActivation",
                        "render_frame_host", render_frame_host);
  observers_.NotifyObservers(&WebContentsObserver::FrameReceivedUserActivation,
                             render_frame_host);
}

void WebContentsImpl::WebAuthnAssertionRequestSucceeded(
    RenderFrameHostImpl* render_frame_host) {
  OPTIONAL_TRACE_EVENT1("content",
                        "WebContentsImpl::WebAuthnAssertionRequestSucceeded",
                        "render_frame_host", render_frame_host);
  observers_.NotifyObservers(
      &WebContentsObserver::WebAuthnAssertionRequestSucceeded,
      render_frame_host);
}

void WebContentsImpl::BindDisplayCutoutHost(
    RenderFrameHostImpl* render_frame_host,
    mojo::PendingAssociatedReceiver<blink::mojom::DisplayCutoutHost> receiver) {
  if (safe_area_insets_host_) {
    safe_area_insets_host_->BindReceiver(std::move(receiver),
                                         render_frame_host);
  }
}

void WebContentsImpl::DidChangeDisplayState(
    RenderFrameHostImpl* render_frame_host,
    bool is_display_none) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::DidChangeDisplayState",
                        "render_frame_host", render_frame_host,
                        "is_display_none", is_display_none);
  observers_.NotifyObservers(&WebContentsObserver::FrameDisplayStateChanged,
                             render_frame_host, is_display_none);
}

void WebContentsImpl::FrameSizeChanged(RenderFrameHostImpl* render_frame_host,
                                       const gfx::Size& frame_size) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::FrameSizeChanged",
                        "render_frame_host", render_frame_host);
  observers_.NotifyObservers(&WebContentsObserver::FrameSizeChanged,
                             render_frame_host, frame_size);
}

void WebContentsImpl::DocumentOnLoadCompleted(
    RenderFrameHostImpl* render_frame_host) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::DocumentOnLoadCompleted",
                        "render_frame_host", render_frame_host);
  DCHECK(render_frame_host->IsInPrimaryMainFrame());
  ShowInsecureLocalhostWarningIfNeeded(render_frame_host->GetPage());

  observers_.NotifyObservers(
      &WebContentsObserver::DocumentOnLoadCompletedInPrimaryMainFrame);
}

void WebContentsImpl::UpdateTitle(RenderFrameHostImpl* render_frame_host,
                                  const std::u16string& title,
                                  base::i18n::TextDirection title_direction) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::UpdateTitle",
                        "render_frame_host", render_frame_host, "title", title);
  // Try to find the navigation entry, which might not be the current one.
  // For example, it might be from a pending deletion RFH, or it might be from a
  // non-primary frame tree.
  NavigationEntryImpl* entry =
      render_frame_host->frame_tree()->controller().GetEntryWithUniqueID(
          render_frame_host->nav_entry_id());

  if (!entry) {
    // We can handle title updates with no matching NavigationEntry, but only if
    // the update is for the initial NavigationEntry. In that case the initial
    // empty document RFH wouldn't have a `nav_entry_id` set because it hasn't
    // committed any navigation. Note that if the title update came from the
    // initial empty document but the WebContents is doing a session restore,
    // we will ignore the title update (because GetLastCommittedEntry() would
    // return the non-initial restored entry), which avoids accidental
    // overwriting of the restored entry's title.
    if (render_frame_host->GetParent() || !render_frame_host->frame_tree()
                                               ->controller()
                                               .GetLastCommittedEntry()
                                               ->IsInitialEntry()) {
      return;
    }
    entry =
        render_frame_host->frame_tree()->controller().GetLastCommittedEntry();
  }

  // TODO(evan): make use of title_direction.
  // http://code.google.com/p/chromium/issues/detail?id=27094
  bool title_changed = UpdateTitleForEntryImpl(entry, title);
  if (title_changed && render_frame_host == GetPrimaryMainFrame()) {
    NotifyTitleUpdateForEntry(entry);
  }
}

void WebContentsImpl::UpdateAppTitle(RenderFrameHostImpl* render_frame_host,
                                     const std::u16string& app_title) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::UpdateTitle",
                        "render_frame_host", render_frame_host, "app_title",
                        app_title);
  // Same logic as UpdateTitle() above.
  NavigationEntryImpl* entry =
      render_frame_host->frame_tree()->controller().GetEntryWithUniqueID(
          render_frame_host->nav_entry_id());
  if (!entry) {
    if (render_frame_host->GetParent() || !render_frame_host->frame_tree()
                                               ->controller()
                                               .GetLastCommittedEntry()
                                               ->IsInitialEntry()) {
      return;
    }
    entry =
        render_frame_host->frame_tree()->controller().GetLastCommittedEntry();
  }
  std::u16string final_app_title;
  base::TrimWhitespace(app_title, base::TRIM_ALL, &final_app_title);

  if (final_app_title == entry->GetAppTitle()) {
    return;
  }

  entry->SetAppTitle(final_app_title);
  NotifyNavigationStateChanged(INVALIDATE_TYPE_TITLE);
}

void WebContentsImpl::UpdateTargetURL(RenderFrameHostImpl* render_frame_host,
                                      const GURL& url) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::UpdateTargetURL",
                        "render_frame_host", render_frame_host, "url", url);
  // In case of racey updates from multiple RenderViewHosts, the last URL should
  // be shown - see also some discussion in https://crbug.com/807776.
  if (!url.is_valid() && render_frame_host != frame_that_set_last_target_url_) {
    return;
  }
  frame_that_set_last_target_url_ =
      url.is_valid() ? render_frame_host : nullptr;

  if (delegate_) {
    delegate_->UpdateTargetURL(this, url);
  }
}

bool WebContentsImpl::ShouldRouteMessageEvent(
    RenderFrameHostImpl* target_rfh) const {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::ShouldRouteMessageEvent",
                        "render_frame_host", target_rfh);
  // Allow the message if this WebContents is dedicated to a browser plugin
  // guest.
  // Note: This check means that an embedder could theoretically receive a
  // postMessage from anyone (not just its own guests). However, this is
  // probably not a risk for apps since other pages won't have references
  // to App windows.
  return GetBrowserPluginGuest() || GetBrowserPluginEmbedder();
}

void WebContentsImpl::EnsureOpenerProxiesExist(
    RenderFrameHostImpl* source_rfh) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::EnsureOpenerProxiesExist",
                        "render_frame_host", source_rfh);
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
          GetSiteInstance()->group(),
          source_web_contents->GetPrimaryMainFrame()->browsing_context_state());
    } else {
      source_rfh->frame_tree_node()->render_manager()->CreateOpenerProxies(
          GetSiteInstance()->group(), nullptr,
          source_rfh->browsing_context_state());
    }
  }
}

void WebContentsImpl::SetAsFocusedWebContentsIfNecessary() {
  SetFocusedFrameTree(&GetPrimaryFrameTree());
}

void WebContentsImpl::SetFocusedFrameTree(FrameTree* frame_tree_to_focus) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::SetFocusedFrameTree");
  DCHECK(frame_tree_to_focus);

  // Only change focus if we are not currently focused.
  FrameTree* old_focused_frame_tree = GetFocusedFrameTree();
  if (old_focused_frame_tree == frame_tree_to_focus) {
    return;
  }

  GetOutermostWebContents()->node_.SetFocusedFrameTree(frame_tree_to_focus);

  // Send a page level blur to the `old_focused_frame_tree` so that it displays
  // inactive UI and focus `frame_tree_to_focus` to activate it.
  if (old_focused_frame_tree) {
    old_focused_frame_tree->root()
        ->current_frame_host()
        ->GetRenderWidgetHost()
        ->SetPageFocus(false);
  }

  // Make sure the outer frame trees knows our frame is focused. Otherwise, the
  // outer renderer could have the element before or after the frame element
  // focused which would return early without actually advancing focus.
  RenderFrameHostImpl::UpdateAXFocusDeferScope focus_defer_scope(
      *frame_tree_to_focus->root()
           ->current_frame_host()
           ->GetOutermostMainFrameOrEmbedder());
  if (frame_tree_to_focus->GetFocusedFrame() &&
      frame_tree_to_focus->GetFocusedFrame()
          ->current_frame_host()
          ->inner_tree_main_frame_tree_node_id()) {
    // If an inner frame tree, in `frame_tree_to_focus`, had focus, the
    // placeholder RenderFrameHost needs to be unset as the focused frame in
    // `frame_tree_to_focus`.
    frame_tree_to_focus->SetFocusedFrame(frame_tree_to_focus->root(), nullptr);
  }
  frame_tree_to_focus->FocusOuterFrameTrees();

  frame_tree_to_focus->root()
      ->current_frame_host()
      ->GetRenderWidgetHost()
      ->SetPageFocus(true);
}

void WebContentsImpl::SetFocusedFrame(FrameTreeNode* node,
                                      SiteInstanceGroup* source) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::SetFocusedFrame",
                        "frame_tree_node", node, "source_site_instance_group",
                        source);

  if (GetFocusedFrameTree()->GetFocusedFrame()) {
    RenderFrameHostImpl* focused_rfh =
        GetFocusedFrameTree()->GetFocusedFrame()->current_frame_host();
    // This is only enforced for focus changes that cross a fenced frame
    // boundary.
    if (!node->current_frame_host()->VerifyFencedFrameFocusChange(
            focused_rfh)) {
      return;
    }
  }

  // Focus will not be in a consistent state until the focused frame tree is
  // also updated (if necessary).
  RenderFrameHostImpl::UpdateAXFocusDeferScope focus_defer_scope(
      *node->current_frame_host()->GetOutermostMainFrameOrEmbedder());

  node->frame_tree().SetFocusedFrame(node, source);

  auto* inner_contents = node_.GetInnerWebContentsInFrame(node);

  // We currently do not need to descend into inner frame trees that
  // are not an inner WebContents for focus. If `inner_contents` is null,
  // then the only supported inner frame tree type would be fenced frames.
  // An embedding frame focusing a fenced frame is not allowed since that would
  // be an information leak.  If a renderer attempts to do that, that should be
  // blocked by `RenderFrameProxyHost::DidFocusFrame()`.
  DCHECK(inner_contents || node->current_frame_host()
                               ->inner_tree_main_frame_tree_node_id()
                               .is_null());

  if (inner_contents) {
    // An inner WebContents is not created from Fenced Frames so we
    // shouldn't end up in this branch.
    DCHECK(!node->IsInFencedFrameTree());

    // |this| is an outer WebContents and |node| represents an inner
    // WebContents. Transfer the focus to the inner contents if |this| is
    // focused.
    if (GetFocusedWebContents() == this) {
      inner_contents->SetAsFocusedWebContentsIfNecessary();
    }
  } else if (node_.OuterContentsFrameTreeNode() &&
             node_.OuterContentsFrameTreeNode()
                     ->current_frame_host()
                     ->GetSiteInstance()
                     ->group() == source) {
    // A GuestView embedding a fenced frame will have an
    // OuterContentsFrameTreeNode. However, it will not have the same site
    // instance because a FencedFrame creates a new BrowsingInstance.
    DCHECK(!node->IsInFencedFrameTree());

    // |this| is an inner WebContents, |node| is its main FrameTreeNode and
    // the outer WebContents FrameTreeNode is at |source|'s SiteInstance.
    // Transfer the focus to the inner WebContents if the outer WebContents is
    // focused. This branch is used when an inner WebContents is focused through
    // its RenderFrameProxyHost (via FrameFocused mojo call, used to
    // implement the window.focus() API).
    if (GetFocusedWebContents() == GetOuterWebContents()) {
      SetFocusedFrameTree(&node->frame_tree());
    }
  } else if (!GetOuterWebContents() || GetFocusedWebContents() == this) {
    // This is an outermost WebContents or we are currently focused so allow
    // the requested node's frame tree to be focused. The
    // (GetFocusedWebContents() == this) is needed when there are multiple
    // frame trees within an inner WebContents (ie. a GuestView with fenced
    // frames).
    SetFocusedFrameTree(&node->frame_tree());
  }

  CloseListenerManager::DidChangeFocusedFrame(this);
}

FrameTree* WebContentsImpl::GetOwnedPictureInPictureFrameTree() {
#if !BUILDFLAG(IS_ANDROID)
  if (has_picture_in_picture_document_) {
    WebContents* picture_in_picture_web_contents =
        PictureInPictureWindowController::
            GetOrCreateDocumentPictureInPictureController(this)
                ->GetChildWebContents();
    if (picture_in_picture_web_contents) {
      return &(static_cast<WebContentsImpl*>(picture_in_picture_web_contents)
                   ->GetPrimaryFrameTree());
    }
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  return nullptr;
}

FrameTree* WebContentsImpl::GetPictureInPictureOpenerFrameTree() {
#if !BUILDFLAG(IS_ANDROID)
  if (picture_in_picture_opener_) {
    return &(static_cast<WebContentsImpl*>(picture_in_picture_opener_.get())
                 ->GetPrimaryFrameTree());
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  return nullptr;
}

void WebContentsImpl::DidCallFocus() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::DidCallFocus");
  // Any explicit focusing of another window while this WebContents is in
  // fullscreen can be used to confuse the user, so drop fullscreen.
  base::ScopedClosureRunner fullscreen_block =
      ForSecurityDropFullscreen(/*display_id=*/display::kInvalidDisplayId);
  // The other contents is independent of this contents, so release the
  // fullscreen block.
  fullscreen_block.RunAndReset();
}

void WebContentsImpl::OnAdvanceFocus(RenderFrameHostImpl* source_rfh) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::OnAdvanceFocus",
                        "render_frame_host", source_rfh);
  // When a RenderFrame needs to advance focus to a `blink::RemoteFrame` (by
  // hitting TAB), the `blink::RemoteFrame` sends an IPC to
  // RenderFrameProxyHost. When this RenderFrameProxyHost represents an inner
  // WebContents, the outer WebContents needs to focus the inner WebContents.
  if (GetOuterWebContents() &&
      GetOuterWebContents() == WebContents::FromRenderFrameHost(source_rfh) &&
      GetFocusedWebContents() == GetOuterWebContents()) {
    SetAsFocusedWebContentsIfNecessary();
  }
}

void WebContentsImpl::OnFocusedElementChangedInFrame(
    RenderFrameHostImpl* frame,
    const gfx::Rect& bounds_in_root_view,
    blink::mojom::FocusType focus_type) {
  OPTIONAL_TRACE_EVENT1("content",
                        "WebContentsImpl::OnFocusedElementChangedInFrame",
                        "render_frame_host", frame);
  RenderWidgetHostViewBase* root_view =
      static_cast<RenderWidgetHostViewBase*>(GetRenderWidgetHostView());
  if (!root_view || !frame->GetView()) {
    return;
  }
  // Convert to screen coordinates from window coordinates by adding the
  // window's origin.
  gfx::Point origin = bounds_in_root_view.origin();
  origin += root_view->GetViewBounds().OffsetFromOrigin();
  gfx::Rect bounds_in_screen(origin, bounds_in_root_view.size());

  root_view->FocusedNodeChanged(frame->has_focused_editable_element(),
                                bounds_in_screen);

  FocusedNodeDetails details = {frame->has_focused_editable_element(),
                                bounds_in_screen, focus_type};
  BrowserAccessibilityStateImpl::GetInstance()->OnFocusChangedInPage(details);
  observers_.NotifyObservers(&WebContentsObserver::OnFocusChangedInPage,
                             &details);
}

bool WebContentsImpl::DidAddMessageToConsole(
    RenderFrameHostImpl* source_frame,
    blink::mojom::ConsoleMessageLevel log_level,
    const std::u16string& message,
    int32_t line_no,
    const std::u16string& source_id,
    const std::optional<std::u16string>& untrusted_stack_trace) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::DidAddMessageToConsole",
                        "message", message);

  observers_.NotifyObservers(&WebContentsObserver::OnDidAddMessageToConsole,
                             source_frame, log_level, message, line_no,
                             source_id, untrusted_stack_trace);

  if (!delegate_) {
    return false;
  }
  return delegate_->DidAddMessageToConsole(this, log_level, message, line_no,
                                           source_id);
}

void WebContentsImpl::DidReceiveInputEvent(
    RenderWidgetHostImpl* render_widget_host,
    const blink::WebInputEvent& event) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::DidReceiveInputEvent",
                        "render_widget_host", render_widget_host);

  if (event.GetType() == blink::WebInputEvent::Type::kMouseDown &&
      static_cast<const blink::WebMouseEvent&>(event).button ==
          blink::WebPointerProperties::Button::kBack) {
    BackNavigationLikely(content_preloading_predictor::kMouseBackButton,
                         WindowOpenDisposition::CURRENT_TAB);
  }

  if (!IsUserInteractionInputType(event.GetType())) {
    return;
  }

  // Ignore unless the widget is currently in the frame tree.
  if (!HasMatchingWidgetHost(&primary_frame_tree_, render_widget_host)) {
    return;
  }

  if (event.GetType() != blink::WebInputEvent::Type::kGestureScrollBegin) {
    last_interaction_time_ = ui::EventTimeForNow();
  }

  observers_.NotifyObservers(&WebContentsObserver::DidGetUserInteraction,
                             event);
}

bool WebContentsImpl::ShouldIgnoreWebInputEvents(
    const blink::WebInputEvent& event) {
  if (ignore_input_events_count_ > 0) {
    return true;
  }
  for (const auto& callback : web_input_event_audit_callbacks_) {
    if (!callback.second.Run(event)) {
      return true;
    }
  }
  WebContentsImpl* web_contents = GetOuterWebContents();
  if (!web_contents) {
    return false;
  }
  return web_contents->ShouldIgnoreWebInputEvents(event);
}

bool WebContentsImpl::ShouldIgnoreInputEvents() {
  if (ignore_input_events_count_ > 0 ||
      !web_input_event_audit_callbacks_.empty()) {
    return true;
  }
  WebContentsImpl* web_contents = GetOuterWebContents();
  if (!web_contents) {
    return false;
  }
  return web_contents->ShouldIgnoreInputEvents();
}

void WebContentsImpl::FocusOwningWebContents(
    RenderWidgetHostImpl* render_widget_host) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::FocusOwningWebContents",
                        "render_widget_host", render_widget_host);
  RenderWidgetHostImpl* main_frame_widget_host =
      GetPrimaryMainFrame()->GetRenderWidgetHost();
  RenderWidgetHostImpl* focused_widget =
      GetFocusedRenderWidgetHost(main_frame_widget_host);

  if (focused_widget != render_widget_host &&
      (!focused_widget ||
       focused_widget->delegate() != render_widget_host->delegate())) {
#if BUILDFLAG(IS_ANDROID)
    if (&GetPrimaryFrameTree() != GetFocusedFrameTree()) {
      UMA_HISTOGRAM_BOOLEAN("Android.FocusChanged.FocusOwningWebContents",
                            true);
    }
#endif
    SetAsFocusedWebContentsIfNecessary();
  } else {
#if BUILDFLAG(IS_ANDROID)
    UMA_HISTOGRAM_BOOLEAN("Android.FocusChanged.FocusOwningWebContents", false);
#endif
  }
}

void WebContentsImpl::OnIgnoredUIEvent() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::OnIgnoredUIEvent");
  // Notify observers.
  observers_.NotifyObservers(&WebContentsObserver::DidGetIgnoredUIEvent);
}

void WebContentsImpl::RendererUnresponsive(
    RenderWidgetHostImpl* render_widget_host,
    base::RepeatingClosure hang_monitor_restarter) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::RendererUnresponsive",
                        "render_widget_host", render_widget_host);
  if (ShouldIgnoreUnresponsiveRenderer()) {
    return;
  }

  bool visible = GetVisibility() == Visibility::VISIBLE;
  RecordRendererUnresponsiveMetrics(visible, render_widget_host);

  // Do not report hangs (to task manager, to hang renderer dialog, etc.) for
  // invisible tabs (like extension background page, background tabs).  See
  // https://crbug.com/881812 for rationale and for choosing the visibility
  // (rather than process priority) as the signal here.
  if (!visible) {
    return;
  }

  if (!render_widget_host->renderer_initialized()) {
    return;
  }

  CrashRepHandlingOutcome outcome =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kNoErrorDialogs)
          ? CrashRepHandlingOutcome::kDropped
          : CrashRepHandlingOutcome::kPotentiallyQueued;
  base::UmaHistogramEnumeration(
      "ReportingAndNEL.UnresponsiveRenderer.CrashReportOutcome", outcome);

  if (base::FeatureList::IsEnabled(features::kCrashReporting) &&
      base::FeatureList::IsEnabled(
          blink::features::kDocumentPolicyIncludeJSCallStacksInCrashReports) &&
      this->GetLastCommittedURL().SchemeIsHTTPOrHTTPS()) {
    RenderProcessHost* rph = render_widget_host->GetProcess();
    if (rph) {
      RenderProcessHostImpl* process_host =
          static_cast<RenderProcessHostImpl*>(rph);
      process_host->InterruptJavaScriptIsolateAndCollectCallStack();
    }
  }

  observers_.NotifyObservers(&WebContentsObserver::OnRendererUnresponsive,
                             render_widget_host->GetProcess());
  if (delegate_) {
    delegate_->RendererUnresponsive(this, render_widget_host,
                                    std::move(hang_monitor_restarter));
  }
}

void WebContentsImpl::RendererResponsive(
    RenderWidgetHostImpl* render_widget_host) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::RenderResponsive",
                        "render_widget_host", render_widget_host);
  observers_.NotifyObservers(&WebContentsObserver::OnRendererResponsive,
                             render_widget_host->GetProcess());

  if (delegate_) {
    delegate_->RendererResponsive(this, render_widget_host);
  }
}

void WebContentsImpl::BeforeUnloadFiredFromRenderManager(
    bool proceed,
    bool* proceed_to_fire_unload) {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::BeforeUnloadFiredFromRenderManager");
  observers_.NotifyObservers(&WebContentsObserver::BeforeUnloadFired, proceed);
  if (delegate_) {
    delegate_->BeforeUnloadFired(this, proceed, proceed_to_fire_unload);
  }
  // Note: |this| might be deleted at this point.
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

void WebContentsImpl::NotifySwappedFromRenderManager(
    RenderFrameHostImpl* old_frame,
    RenderFrameHostImpl* new_frame) {
  TRACE_EVENT2("content", "WebContentsImpl::NotifySwappedFromRenderManager",
               "old_render_frame_host", old_frame, "new_render_frame_host",
               new_frame);
  DCHECK_NE(new_frame->lifecycle_state(),
            RenderFrameHostImpl::LifecycleStateImpl::kSpeculative);

  // Only fire RenderViewHostChanged if it is related to our FrameTree, as
  // observers can not deal with events coming from non-primary FrameTree.
  // TODO(crbug.com/40165060): Update observers to deal with the events,
  // and fire events for all frame trees.
  if (new_frame->IsInPrimaryMainFrame()) {
    // The |new_frame| and its various compadres are already swapped into place
    // for the WebContentsImpl when this method is called.
    DCHECK_EQ(
        primary_frame_tree_.root()->render_manager()->GetRenderWidgetHostView(),
        new_frame->GetView());

    RenderViewHost* old_rvh =
        old_frame ? old_frame->GetRenderViewHost() : nullptr;
    RenderViewHost* new_rvh = new_frame->GetRenderViewHost();
    // |old_rvh| and |new_rvh| might be equal when navigating from a crashed
    // RenderFrameHost to a new same-site one. With RenderDocument, this will
    // happen for every same-site navigation.
    if (old_rvh != new_rvh) {
      NotifyViewSwapped(old_rvh, new_rvh);
    }

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

    NotifyPrimaryMainFrameProcessIsAlive();
  }

  NotifyFrameSwapped(old_frame, new_frame);
}

void WebContentsImpl::NotifySwappedFromRenderManagerWithoutFallbackContent(
    RenderFrameHostImpl* new_frame) {
  auto* rwhv = static_cast<RenderWidgetHostViewBase*>(new_frame->GetView());
  if (rwhv) {
    rwhv->ClearFallbackSurfaceForCommitPending();
  }
}

void WebContentsImpl::NotifyMainFrameSwappedFromRenderManager(
    RenderFrameHostImpl* old_frame,
    RenderFrameHostImpl* new_frame) {
  // Only fire RenderViewHostChanged if it is
  // related to our FrameTree, as observers cannot deal with events coming
  // from non-primary FrameTree.
  // TODO(crbug.com/40165060): Update observers to deal with the events,
  // and fire events for all frame trees.
  if (!new_frame->IsInPrimaryMainFrame()) {
    return;
  }
  NotifyViewSwapped(old_frame ? old_frame->GetRenderViewHost() : nullptr,
                    new_frame->GetRenderViewHost());
}

void WebContentsImpl::CreateRenderWidgetHostViewForRenderManager(
    RenderViewHost* render_view_host) {
  OPTIONAL_TRACE_EVENT1(
      "content", "WebContentsImpl::CreateRenderWidgetHostViewForRenderManager",
      "render_view_host", render_view_host);

  bool is_inner_frame_tree = [&]() {
    FrameTree* frame_tree =
        static_cast<RenderViewHostImpl*>(render_view_host)->frame_tree();
    switch (frame_tree->type()) {
      case FrameTree::Type::kPrimary:
      case FrameTree::Type::kPrerender:
        return false;
      case FrameTree::Type::kFencedFrame:
        return true;
    }
  }();

  if (is_inner_frame_tree) {
    WebContentsViewChildFrame::CreateRenderWidgetHostViewForInnerFrameTree(
        this, render_view_host->GetWidget());
  } else {
    RenderWidgetHostViewBase* rwh_view =
        view_->CreateViewForWidget(render_view_host->GetWidget());
    view_->SetOverscrollControllerEnabled(CanOverscrollContent());
    rwh_view->SetSize(GetSizeForMainFrame());
  }
}

void WebContentsImpl::ReattachOuterDelegateIfNeeded() {
  if (node_.outer_web_contents()) {
    ReattachToOuterWebContentsFrame();
  }
}

bool WebContentsImpl::CreateRenderViewForRenderManager(
    RenderViewHost* render_view_host,
    const std::optional<blink::FrameToken>& opener_frame_token,
    RenderFrameProxyHost* proxy_host) {
  TRACE_EVENT1("browser,navigation",
               "WebContentsImpl::CreateRenderViewForRenderManager",
               "render_view_host", render_view_host);
  auto* rvh_impl = static_cast<RenderViewHostImpl*>(render_view_host);
  // Observers should not destroy the WebContents here or we will crash as the
  // stack unwinds. See crbug.com/1181043.
  base::AutoReset<bool> scope(&prevent_destruction_, true);

  if (!proxy_host) {
    CreateRenderWidgetHostViewForRenderManager(render_view_host);
  }

  const auto proxy_routing_id =
      proxy_host ? proxy_host->GetRoutingID() : MSG_ROUTING_NONE;
  // TODO(crbug.com/40166243): Given MPArch, should we pass
  // opened_by_another_window_ for non primary FrameTrees?
  if (!rvh_impl->CreateRenderView(opener_frame_token, proxy_routing_id,
                                  opened_by_another_window_)) {
    return false;
  }

  // Set the TextAutosizer state from the main frame's renderer on the new view,
  // but only if it's not for the main frame. Main frame renderers should create
  // this state themselves from up-to-date values, so we shouldn't override it
  // with the cached values.
  if (!rvh_impl->GetMainRenderFrameHost() && proxy_host) {
    proxy_host->GetAssociatedRemoteMainFrame()->UpdateTextAutosizerPageInfo(
        proxy_host->frame_tree_node()
            ->current_frame_host()
            ->GetPage()
            .text_autosizer_page_info()
            .Clone());
  }

  // If `render_view_host` is for an inner WebContents, ensure that its
  // RenderWidgetHostView is properly reattached to the outer WebContents. Note
  // that this should only be done when `render_view_host` is already the
  // current RenderViewHost in this WebContents.  If it isn't, the reattachment
  // will happen later when `render_view_host` becomes current as part of
  // committing the speculative RenderFrameHost it's associated with.
  if (!proxy_host && render_view_host == GetRenderViewHost()) {
    ReattachOuterDelegateIfNeeded();
  }

  SetHistoryOffsetAndLengthForView(
      render_view_host,
      rvh_impl->frame_tree()->controller().GetLastCommittedEntryIndex(),
      rvh_impl->frame_tree()->controller().GetEntryCount());

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_ANDROID)
  // Force a ViewMsg_Resize to be sent, needed to make plugins show up on
  // linux. See crbug.com/83941.
  RenderWidgetHostView* rwh_view = render_view_host->GetWidget()->GetView();
  if (rwh_view) {
    if (RenderWidgetHost* render_widget_host =
            rwh_view->GetRenderWidgetHost()) {
      render_widget_host->SynchronizeVisualProperties();
    }
  }
#endif

  return true;
}

#if BUILDFLAG(IS_ANDROID)

base::android::ScopedJavaLocalRef<jobject>
WebContentsImpl::GetJavaWebContents() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return GetWebContentsAndroid()->GetJavaObject();
}

base::android::ScopedJavaLocalRef<jthrowable>
WebContentsImpl::GetJavaCreatorLocation() {
  return base::android::ScopedJavaLocalRef<jthrowable>(java_creator_location_);
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

void WebContentsImpl::ActivateNearestFindResult(float x, float y) {
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
    java_interfaces_ = std::make_unique<service_manager::InterfaceProvider>(
        base::SingleThreadTaskRunner::GetCurrentDefault());
    java_interfaces_->Bind(std::move(provider));
  }
  return java_interfaces_.get();
}

#endif

bool WebContentsImpl::CompletedFirstVisuallyNonEmptyPaint() {
  return GetPrimaryPage().did_first_visually_non_empty_paint();
}

void WebContentsImpl::OnDidDownloadImage(
    base::WeakPtr<RenderFrameHostImpl> rfh,
    ImageDownloadCallback callback,
    int id,
    const GURL& image_url,
    int32_t http_status_code,
    const std::vector<SkBitmap>& images,
    const std::vector<gfx::Size>& original_image_sizes) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::OnDidDownloadImage",
                        "image_url", image_url);

  // Guard against buggy or compromised renderers that could violate the API
  // contract that |images| and |original_image_sizes| must have the same
  // length.
  if (images.size() != original_image_sizes.size()) {
    if (rfh) {
      ReceivedBadMessage(rfh->GetProcess(),
                         bad_message::WCI_INVALID_DOWNLOAD_IMAGE_RESULT);
    }
    // Respond with a 400 to indicate that something went wrong.
    std::move(callback).Run(id, 400, image_url, std::vector<SkBitmap>(),
                            std::vector<gfx::Size>());
    return;
  }

  std::move(callback).Run(id, http_status_code, image_url, images,
                          original_image_sizes);
}

int WebContentsImpl::GetNextDownloadId() {
  static int next_image_download_id = 0;
  return ++next_image_download_id;
}

void WebContentsImpl::OnDialogClosed(int render_process_id,
                                     int render_frame_id,
                                     JavaScriptDialogCallback response_callback,
                                     base::ScopedClosureRunner fullscreen_block,
                                     bool dialog_was_suppressed,
                                     bool success,
                                     const std::u16string& user_input) {
  RenderFrameHostImpl* rfh =
      RenderFrameHostImpl::FromID(render_process_id, render_frame_id);
  // The user confirms and closes a dialog even after the page has navigated
  // away and got into BackForwardCache.
  DCHECK(!rfh || rfh->GetPage().IsPrimary() || rfh->IsInBackForwardCache());
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::OnDialogClosed",
                        "render_frame_host", rfh);
  last_dialog_suppressed_ = dialog_was_suppressed;
  fullscreen_block.RunAndReset();

  javascript_dialog_dismiss_notifier_.reset();

  if (is_showing_before_unload_dialog_ && !success) {
    // It is possible for the current RenderFrameHost to have changed in the
    // meantime.  Do not reset the navigation state in that case.
    if (rfh && rfh == rfh->frame_tree_node()->current_frame_host()) {
      rfh->frame_tree_node()->BeforeUnloadCanceled();
      rfh->frame_tree()->controller().DiscardNonCommittedEntries();
    }

    // Update the URL display either way, to avoid showing a stale URL.
    NotifyNavigationStateChanged(INVALIDATE_TYPE_URL);

    observers_.NotifyObservers(
        &WebContentsObserver::BeforeUnloadDialogCancelled);
  }

  std::move(response_callback).Run(success, user_input);

  std::vector<protocol::PageHandler*> page_handlers =
      protocol::PageHandler::EnabledForWebContents(this);
  for (auto* handler : page_handlers) {
    handler->DidCloseJavaScriptDialog(success, user_input);
  }

  is_showing_javascript_dialog_ = false;
  is_showing_before_unload_dialog_ = false;
}

RenderFrameHostManager* WebContentsImpl::GetRenderManager() {
  return primary_frame_tree_.root()->render_manager();
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
  return GetPrimaryMainFrame()->FaviconURLs();
}

// The Mac implementation  of the next two methods is in
// web_contents_impl_mac.mm
#if !BUILDFLAG(IS_MAC)

void WebContentsImpl::Resize(const gfx::Rect& new_bounds) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::Resize");
#if defined(USE_AURA)
  aura::Window* window = GetNativeView();
  window->SetBounds(gfx::Rect(window->bounds().origin(), new_bounds.size()));
#elif BUILDFLAG(IS_ANDROID)
  content::RenderWidgetHostView* view = GetRenderWidgetHostView();
  if (view) {
    view->SetBounds(new_bounds);
  }
#endif
}

gfx::Size WebContentsImpl::GetSize() {
#if defined(USE_AURA)
  aura::Window* window = GetNativeView();
  return window->bounds().size();
#elif BUILDFLAG(IS_ANDROID)
  ui::ViewAndroid* view_android = GetNativeView();
  return view_android->bounds().size();
#elif BUILDFLAG(IS_IOS)
  // TODO(crbug.com/40254930): Implement me.
  NOTREACHED_IN_MIGRATION();
  return gfx::Size();
#endif
}

#endif  // !BUILDFLAG(IS_MAC)

gfx::Rect WebContentsImpl::GetWindowsControlsOverlayRect() const {
  return window_controls_overlay_rect_;
}

void WebContentsImpl::UpdateWindowControlsOverlay(
    const gfx::Rect& bounding_rect) {
  if (window_controls_overlay_rect_ == bounding_rect) {
    return;
  }

  window_controls_overlay_rect_ = bounding_rect;

  // Updates to the |window_controls_overlay_rect_| are sent via
  // the VisualProperties message.
  if (RenderWidgetHost* render_widget_host =
          GetPrimaryMainFrame()->GetRenderWidgetHost()) {
    render_widget_host->SynchronizeVisualProperties();
  }

  view_->UpdateWindowControlsOverlay(bounding_rect);
}

BrowserPluginEmbedder* WebContentsImpl::GetBrowserPluginEmbedder() const {
  return browser_plugin_embedder_.get();
}

void WebContentsImpl::CreateBrowserPluginEmbedderIfNecessary() {
  OPTIONAL_TRACE_EVENT0(
      "content", "WebContentsImpl::CreateBrowserPluginEmbedderIfNecessary");
  if (browser_plugin_embedder_) {
    return;
  }
  browser_plugin_embedder_.reset(BrowserPluginEmbedder::Create(this));
}

gfx::Size WebContentsImpl::GetSizeForMainFrame() {
  if (delegate_) {
    // The delegate has a chance to specify a size independent of the UI.
    gfx::Size delegate_size = delegate_->GetSizeForNewRenderView(this);
    if (!delegate_size.IsEmpty()) {
      return delegate_size;
    }
  }

  // Device emulation, when enabled, can specify a size independent of the UI.
  if (!device_emulation_size_.IsEmpty()) {
    return device_emulation_size_;
  }

  return GetContainerBounds().size();
}

void WebContentsImpl::OnFrameTreeNodeDestroyed(FrameTreeNode* node) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::OnFrameTreeNodeDestroyed",
                        "node", node);
  // If we are the focused frame tree and are destroying the root node
  // reassign the focused frame tree node.
  if (!node->parent() && GetFocusedFrameTree() == &node->frame_tree() &&
      &node->frame_tree() != &primary_frame_tree_) {
    FrameTreeNode* frame_in_embedder =
        node->render_manager()->GetOuterDelegateNode();
    SetFocusedFrameTree(frame_in_embedder ? &frame_in_embedder->frame_tree()
                                          : &primary_frame_tree_);
  }

  observers_.NotifyObservers(&WebContentsObserver::FrameDeleted,
                             node->frame_tree_node_id());
}

void WebContentsImpl::OnPreferredSizeChanged(const gfx::Size& old_size) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::OnPreferredSizeChanged");
  if (!delegate_) {
    return;
  }
  const gfx::Size new_size = GetPreferredSize();
  if (new_size != old_size) {
    delegate_->UpdatePreferredSize(this, new_size);
  }
}

FindRequestManager* WebContentsImpl::GetFindRequestManager() {
  for (auto* contents = this; contents;
       contents = contents->GetOuterWebContents()) {
    if (contents->find_request_manager_) {
      return contents->find_request_manager_.get();
    }
  }

  return nullptr;
}

FindRequestManager* WebContentsImpl::GetOrCreateFindRequestManager() {
  if (FindRequestManager* manager = GetFindRequestManager()) {
    return manager;
  }

  DCHECK(!browser_plugin_guest_ || GetOuterWebContents());

  // No existing FindRequestManager found, so one must be created.
  find_request_manager_ = std::make_unique<FindRequestManager>(this);

  // Concurrent find sessions must not overlap, so destroy any existing
  // FindRequestManagers in any inner WebContentses.
  for (WebContents* contents : GetWebContentsAndAllInner()) {
    auto* web_contents_impl = static_cast<WebContentsImpl*>(contents);
    if (web_contents_impl == this) {
      continue;
    }
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
  if (delegate_ && !IsBeingDestroyed() &&
      !GetPrimaryMainFrame()->GetProcess()->FastShutdownStarted()) {
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
    OnDeviceConnectionTypesChanged(
        WebContentsObserver::DeviceConnectionType::kBluetooth, /*used=*/true);
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
    OnDeviceConnectionTypesChanged(
        WebContentsObserver::DeviceConnectionType::kBluetooth,
        /*used=*/false);
  }
}

void WebContentsImpl::IncrementBluetoothScanningSessionsCount() {
  OPTIONAL_TRACE_EVENT0(
      "content", "WebContentsImpl::IncrementBluetoothScanningSessionsCount");
  // Trying to invalidate the tab state while being destroyed could result in a
  // use after free.
  if (IsBeingDestroyed()) {
    return;
  }

  // Notify for UI updates if the state changes.
  bluetooth_scanning_sessions_count_++;
  if (bluetooth_scanning_sessions_count_ == 1) {
    NotifyNavigationStateChanged(INVALIDATE_TYPE_TAB);
  }
}

void WebContentsImpl::DecrementBluetoothScanningSessionsCount() {
  OPTIONAL_TRACE_EVENT0(
      "content", "WebContentsImpl::DecrementBluetoothScanningSessionsCount");
  // Trying to invalidate the tab state while being destroyed could result in a
  // use after free.
  if (IsBeingDestroyed()) {
    return;
  }

  DCHECK_NE(0u, bluetooth_scanning_sessions_count_);
  bluetooth_scanning_sessions_count_--;
  if (bluetooth_scanning_sessions_count_ == 0) {
    NotifyNavigationStateChanged(INVALIDATE_TYPE_TAB);
  }
}

void WebContentsImpl::IncrementSerialActiveFrameCount() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::IncrementSerialActiveFrameCount");
  // Trying to invalidate the tab state while being destroyed could result in a
  // use after free.
  if (IsBeingDestroyed()) {
    return;
  }

  // Notify for UI updates if the state changes.
  serial_active_frame_count_++;
  if (serial_active_frame_count_ == 1) {
    OnDeviceConnectionTypesChanged(
        WebContentsObserver::DeviceConnectionType::kSerial, /*used=*/true);
  }
}

void WebContentsImpl::DecrementSerialActiveFrameCount() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::DecrementSerialActiveFrameCount");
  // Trying to invalidate the tab state while being destroyed could result in a
  // use after free.
  if (IsBeingDestroyed()) {
    return;
  }

  // Notify for UI updates if the state changes.
  DCHECK_NE(0u, serial_active_frame_count_);
  serial_active_frame_count_--;
  if (serial_active_frame_count_ == 0) {
    OnDeviceConnectionTypesChanged(
        WebContentsObserver::DeviceConnectionType::kSerial, /*used=*/false);
  }
}

void WebContentsImpl::IncrementHidActiveFrameCount() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::IncrementHidActiveFrameCount");
  // Trying to invalidate the tab state while being destroyed could result in a
  // use after free.
  if (IsBeingDestroyed()) {
    return;
  }

  // Notify for UI updates if the active frame count transitions from zero to
  // non-zero.
  hid_active_frame_count_++;
  if (hid_active_frame_count_ == 1) {
    OnDeviceConnectionTypesChanged(
        WebContentsObserver::DeviceConnectionType::kHID, /*used=*/true);
  }
}

void WebContentsImpl::DecrementHidActiveFrameCount() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::DecrementHidActiveFrameCount");
  // Trying to invalidate the tab state while being destroyed could result in a
  // use after free.
  if (IsBeingDestroyed()) {
    return;
  }

  // Notify for UI updates if the active frame count transitions from non-zero
  // to zero.
  DCHECK_NE(0u, hid_active_frame_count_);
  hid_active_frame_count_--;
  if (hid_active_frame_count_ == 0) {
    OnDeviceConnectionTypesChanged(
        WebContentsObserver::DeviceConnectionType::kHID, /*used=*/false);
  }
}

void WebContentsImpl::OnDeviceConnectionTypesChanged(
    WebContentsObserver::DeviceConnectionType device_connection_type,
    bool used) {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::OnDeviceConnectionTypesChanged");
  NotifyNavigationStateChanged(INVALIDATE_TYPE_TAB);
  observers_.NotifyObservers(
      &WebContentsObserver::OnDeviceConnectionTypesChanged,
      device_connection_type, used);
}

void WebContentsImpl::IncrementUsbActiveFrameCount() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::IncrementUsbActiveFrameCount");
  // Trying to invalidate the tab state while being destroyed could result in a
  // use after free.
  if (IsBeingDestroyed()) {
    return;
  }

  // Notify for UI updates if the active frame count transitions from zero to
  // non-zero.
  usb_active_frame_count_++;
  if (usb_active_frame_count_ == 1) {
    OnDeviceConnectionTypesChanged(
        WebContentsObserver::DeviceConnectionType::kUSB, /*used=*/true);
  }
}

void WebContentsImpl::DecrementUsbActiveFrameCount() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::DecrementUsbActiveFrameCount");
  // Trying to invalidate the tab state while being destroyed could result in a
  // use after free.
  if (IsBeingDestroyed()) {
    return;
  }

  // Notify for UI updates if the active frame count transitions from non-zero
  // to zero.
  DCHECK_NE(0u, usb_active_frame_count_);
  usb_active_frame_count_--;
  if (usb_active_frame_count_ == 0) {
    OnDeviceConnectionTypesChanged(
        WebContentsObserver::DeviceConnectionType::kUSB, /*used=*/false);
  }
}

void WebContentsImpl::IncrementFileSystemAccessHandleCount() {
  OPTIONAL_TRACE_EVENT0(
      "content", "WebContentsImpl::IncrementFileSystemAccessHandleCount");
  // Trying to invalidate the tab state while being destroyed could result in a
  // use after free.
  if (IsBeingDestroyed()) {
    return;
  }

  // Notify for UI updates if the state changes. Need both TYPE_TAB and TYPE_URL
  // to update both the tab-level usage indicator and the usage indicator in the
  // omnibox.
  file_system_access_handle_count_++;
  if (file_system_access_handle_count_ == 1) {
    NotifyNavigationStateChanged(static_cast<content::InvalidateTypes>(
        INVALIDATE_TYPE_TAB | INVALIDATE_TYPE_URL));
  }
}

void WebContentsImpl::DecrementFileSystemAccessHandleCount() {
  OPTIONAL_TRACE_EVENT0(
      "content", "WebContentsImpl::DecrementFileSystemAccessHandleCount");
  // Trying to invalidate the tab state while being destroyed could result in a
  // use after free.
  if (IsBeingDestroyed()) {
    return;
  }

  // Notify for UI updates if the state changes. Need both TYPE_TAB and TYPE_URL
  // to update both the tab-level usage indicator and the usage indicator in the
  // omnibox.
  DCHECK_NE(0u, file_system_access_handle_count_);
  file_system_access_handle_count_--;
  if (file_system_access_handle_count_ == 0) {
    NotifyNavigationStateChanged(static_cast<content::InvalidateTypes>(
        INVALIDATE_TYPE_TAB | INVALIDATE_TYPE_URL));
  }
}

void WebContentsImpl::SetHasPersistentVideo(bool has_persistent_video) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::SetHasPersistentVideo",
                        "has_persistent_video", has_persistent_video,
                        "had_persistent_value", has_persistent_video_);
  if (has_persistent_video_ == has_persistent_video) {
    return;
  }

  has_persistent_video_ = has_persistent_video;
  NotifyPreferencesChanged();
  media_web_contents_observer()->RequestPersistentVideo(has_persistent_video);

  // This is Picture-in-Picture on Android S+.
  if (auto* view = GetRenderWidgetHostView()) {
    static_cast<RenderWidgetHostViewBase*>(view)->SetHasPersistentVideo(
        has_persistent_video);
  }
}

void WebContentsImpl::SetSpatialNavigationDisabled(bool disabled) {
  OPTIONAL_TRACE_EVENT2(
      "content", "WebContentsImpl::SetSpatialNavigationDisabled", "disabled",
      disabled, "was_disabled", is_spatial_navigation_disabled_);
  if (is_spatial_navigation_disabled_ == disabled) {
    return;
  }

  is_spatial_navigation_disabled_ = disabled;
  NotifyPreferencesChanged();
}

void WebContentsImpl::SetStylusHandwritingEnabled(bool enabled) {
  if (stylus_handwriting_enabled_ == enabled) {
    return;
  }
  stylus_handwriting_enabled_ = enabled;
  NotifyPreferencesChanged();
}

PictureInPictureResult WebContentsImpl::EnterPictureInPicture() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::EnterPictureInPicture");
  return delegate_ ? delegate_->EnterPictureInPicture(this)
                   : PictureInPictureResult::kNotSupported;
}

void WebContentsImpl::ExitPictureInPicture() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::ExitPictureInPicture");
  if (delegate_) {
    delegate_->ExitPictureInPicture();
  }
}

void WebContentsImpl::OnXrHasRenderTarget(
    const viz::FrameSinkId& frame_sink_id) {
  xr_render_target_ = frame_sink_id;
  observers_.NotifyObservers(&WebContentsObserver::CaptureTargetChanged);
}

WebContentsImpl::CaptureTarget WebContentsImpl::GetCaptureTarget() {
  // We don't provide a view for the Android AR capturer. This is not a problem
  // because it is currently only used by the MouseCursorOverlayController,
  // which isn't used on Android anyway.
  if (xr_render_target_.is_valid()) {
    return CaptureTarget{.sink_id = xr_render_target_};
  }

  RenderWidgetHostView* host_view = GetRenderWidgetHostView();
  if (!host_view) {
    return {};
  }

  RenderWidgetHostViewBase* base_view =
      static_cast<RenderWidgetHostViewBase*>(host_view);
  return CaptureTarget{.sink_id = base_view->GetFrameSinkId(),
                       .view = host_view->GetNativeView()};
}

#if BUILDFLAG(IS_ANDROID)
void WebContentsImpl::NotifyFindMatchRectsReply(
    int version,
    const std::vector<gfx::RectF>& rects,
    const gfx::RectF& active_rect) {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::NotifyFindMatchRectsReply");
  if (delegate_) {
    delegate_->FindMatchRectsReply(this, version, rects, active_rect);
  }
}
#endif

void WebContentsImpl::SetForceDisableOverscrollContent(bool force_disable) {
  OPTIONAL_TRACE_EVENT1("content",
                        "WebContentsImpl::SetForceDisableOverscrollContent",
                        "force_disable", force_disable);
  force_disable_overscroll_content_ = force_disable;
  if (view_) {
    view_->SetOverscrollControllerEnabled(CanOverscrollContent());
  }
}

bool WebContentsImpl::SetDeviceEmulationSize(const gfx::Size& new_size) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::SetDeviceEmulationSize");
  device_emulation_size_ = new_size;
  RenderWidgetHostView* rwhv = GetPrimaryMainFrame()->GetView();

  const gfx::Size current_size = rwhv->GetViewBounds().size();
  if (view_size_before_emulation_.IsEmpty()) {
    view_size_before_emulation_ = current_size;
  }

  if (current_size != new_size) {
    rwhv->SetSize(new_size);
  }

  return current_size != new_size;
}

void WebContentsImpl::ClearDeviceEmulationSize() {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::ClearDeviceEmulationSize");
  RenderWidgetHostView* rwhv = GetPrimaryMainFrame()->GetView();
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
    std::unique_ptr<AudioStreamBrokerFactory> broker_factory;
    if (delegate_) {
      broker_factory = delegate_->CreateAudioStreamBrokerFactory(this);
    }
    if (!broker_factory) {
      broker_factory = AudioStreamBrokerFactory::CreateImpl();
    }
    audio_stream_factory_.emplace(
        this,
        // BrowserMainLoop::GetInstance() may be null in unit tests.
        BrowserMainLoop::GetInstance()
            ? static_cast<media::UserInputMonitorBase*>(
                  BrowserMainLoop::GetInstance()->user_input_monitor())
            : nullptr,
        std::move(broker_factory));
  }

  return &*audio_stream_factory_;
}

void WebContentsImpl::MediaStartedPlaying(
    const WebContentsObserver::MediaPlayerInfo& media_info,
    const MediaPlayerId& id) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::MediaStartedPlaying");
  if (media_info.has_video) {
    currently_playing_video_count_++;
  }

  observers_.NotifyObservers(&WebContentsObserver::MediaStartedPlaying,
                             media_info, id);
}

void WebContentsImpl::MediaStoppedPlaying(
    const WebContentsObserver::MediaPlayerInfo& media_info,
    const MediaPlayerId& id,
    WebContentsObserver::MediaStoppedReason reason) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::MediaStoppedPlaying");
  if (media_info.has_video) {
    currently_playing_video_count_--;
  }

  observers_.NotifyObservers(&WebContentsObserver::MediaStoppedPlaying,
                             media_info, id, reason);
}

void WebContentsImpl::MediaResized(const gfx::Size& size,
                                   const MediaPlayerId& id) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::MediaResized");
  cached_video_sizes_[id] = size;

  observers_.NotifyObservers(&WebContentsObserver::MediaResized, size, id);
}

void WebContentsImpl::MediaEffectivelyFullscreenChanged(bool is_fullscreen) {
  OPTIONAL_TRACE_EVENT1("content",
                        "WebContentsImpl::MediaEffectivelyFullscreenChanged",
                        "is_fullscreen", is_fullscreen);
  observers_.NotifyObservers(
      &WebContentsObserver::MediaEffectivelyFullscreenChanged, is_fullscreen);
}

void WebContentsImpl::MediaDestroyed(const MediaPlayerId& id) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::MediaDestroyed");
  observers_.NotifyObservers(&WebContentsObserver::MediaDestroyed, id);
}

void WebContentsImpl::MediaSessionCreated(MediaSession* media_session) {
  observers_.NotifyObservers(&WebContentsObserver::MediaSessionCreated,
                             media_session);
}

int WebContentsImpl::GetCurrentlyPlayingVideoCount() {
  return currently_playing_video_count_;
}

std::optional<gfx::Size> WebContentsImpl::GetFullscreenVideoSize() {
  std::optional<MediaPlayerId> id =
      media_web_contents_observer_->GetFullscreenVideoMediaPlayerId();
  if (id && base::Contains(cached_video_sizes_, id.value())) {
    return std::optional<gfx::Size>(cached_video_sizes_[id.value()]);
  }
  return std::nullopt;
}

void WebContentsImpl::AudioContextPlaybackStarted(RenderFrameHostImpl* host,
                                                  int context_id) {
  OPTIONAL_TRACE_EVENT1("content",
                        "WebContentsImpl::AudioContextPlaybackStarted",
                        "render_frame_host", host);
  WebContentsObserver::AudioContextId audio_context_id(host, context_id);
  observers_.NotifyObservers(&WebContentsObserver::AudioContextPlaybackStarted,
                             audio_context_id);
}

void WebContentsImpl::AudioContextPlaybackStopped(RenderFrameHostImpl* host,
                                                  int context_id) {
  OPTIONAL_TRACE_EVENT1("content",
                        "WebContentsImpl::AudioContextPlaybackStopped",
                        "render_frame_host", host);
  WebContentsObserver::AudioContextId audio_context_id(host, context_id);
  observers_.NotifyObservers(&WebContentsObserver::AudioContextPlaybackStopped,
                             audio_context_id);
}

void WebContentsImpl::OnFrameAudioStateChanged(RenderFrameHostImpl* host,
                                               bool is_audible) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::OnFrameAudioStateChanged",
                        "render_frame_host", host, "is_audible", is_audible);
  observers_.NotifyObservers(&WebContentsObserver::OnFrameAudioStateChanged,
                             host, is_audible);
}

void WebContentsImpl::OnFrameVisibilityChanged(
    RenderFrameHostImpl* host,
    blink::mojom::FrameVisibility visibility) {
  OPTIONAL_TRACE_EVENT2("content", "WebContentsImpl::OnFrameVisibilityChanged",
                        "render_frame_host", host, "visibility", visibility);
  observers_.NotifyObservers(&WebContentsObserver::OnFrameVisibilityChanged,
                             host, visibility);
}

void WebContentsImpl::OnRemoteSubframeViewportIntersectionStateChanged(
    RenderFrameHostImpl* host,
    const blink::mojom::ViewportIntersectionState&
        viewport_intersection_state) {
  OPTIONAL_TRACE_EVENT2(
      "content",
      "WebContentsImpl::OnRemoteSubframeViewportIntersectionStateChanged",
      "render_frame_host", host, "viewport_intersection_state",
      viewport_intersection_state);
  observers_.NotifyObservers(
      &WebContentsObserver::OnRemoteSubframeViewportIntersectionStateChanged,
      host, viewport_intersection_state);
}

void WebContentsImpl::OnFrameIsCapturingMediaStreamChanged(
    RenderFrameHostImpl* host,
    bool is_capturing_media_stream) {
  observers_.NotifyObservers(
      &WebContentsObserver::OnFrameIsCapturingMediaStreamChanged, host,
      is_capturing_media_stream);
}

// Cf. `GetProspectiveOuterDocument` which applies to the same situation, but is
// for ascending.
std::vector<FrameTreeNode*> WebContentsImpl::GetUnattachedOwnedNodes(
    RenderFrameHostImpl* owner) {
  std::vector<FrameTreeNode*> unattached_owned_nodes;
  BrowserPluginGuestManager* guest_manager =
      GetBrowserContext()->GetGuestManager();
  if (owner == GetPrimaryMainFrame() && guest_manager) {
    guest_manager->ForEachUnattachedGuest(
        this, [&](WebContents* guest_contents) {
          unattached_owned_nodes.push_back(
              static_cast<WebContentsImpl*>(guest_contents)
                  ->primary_frame_tree_.root());
        });
  }
  return unattached_owned_nodes;
}

void WebContentsImpl::IsClipboardPasteAllowedByPolicy(
    const ClipboardEndpoint& source,
    const ClipboardEndpoint& destination,
    const ClipboardMetadata& metadata,
    ClipboardPasteData clipboard_paste_data,
    IsClipboardPasteAllowedCallback callback) {
  ++suppress_unresponsive_renderer_count_;
  GetContentClient()->browser()->IsClipboardPasteAllowedByPolicy(
      source, destination, metadata, std::move(clipboard_paste_data),
      base::BindOnce(&WebContentsImpl::IsClipboardPasteAllowedWrapperCallback,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void WebContentsImpl::OnTextCopiedToClipboard(
    RenderFrameHostImpl* render_frame_host,
    const std::u16string& copied_text) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::OnTextCopiedToClipboard",
                        "render_frame_host", render_frame_host);
  observers_.NotifyObservers(&WebContentsObserver::OnTextCopiedToClipboard,
                             render_frame_host, copied_text);
}

void WebContentsImpl::IsClipboardPasteAllowedWrapperCallback(
    IsClipboardPasteAllowedCallback callback,
    std::optional<ClipboardPasteData> clipboard_paste_data) {
  std::move(callback).Run(std::move(clipboard_paste_data));
  --suppress_unresponsive_renderer_count_;
}

void WebContentsImpl::BindScreenOrientation(
    RenderFrameHost* rfh,
    mojo::PendingAssociatedReceiver<device::mojom::ScreenOrientation>
        receiver) {
  screen_orientation_provider_->BindScreenOrientation(rfh, std::move(receiver));
}

bool WebContentsImpl::IsTransientActivationRequiredForHtmlFullscreen() {
  // Allow fullscreen if the screen orientation changed in the last 1 second.
  static constexpr base::TimeDelta kMaxInterval = base::Seconds(1);
  const base::TimeDelta delta =
      ui::EventTimeForNow() - last_screen_orientation_change_time_;
  if (delta <= kMaxInterval) {
    return false;
  }

  // Require transient activation shortly after a same-origin WebContents exit.
  RenderFrameHostImpl* host = GetPrimaryMainFrame();
  auto* last_exits = GetFullscreenUserData(GetBrowserContext())->last_exits();
  auto last_exit = last_exits->find(host->GetLastCommittedOrigin());
  constexpr base::TimeDelta kCooldown = base::Seconds(5);
  if (last_exit != last_exits->end() &&
      base::TimeTicks::Now() < last_exit->second + kCooldown) {
    return true;
  }

  // Waive transient activation requirements if Automatic Fullscreen is granted.
  if (IsAutomaticFullscreenGranted(host)) {
    return false;
  }

  return true;
}

bool WebContentsImpl::IsBackForwardCacheSupported() {
  if (!GetDelegate()) {
    return false;
  }
  return GetDelegate()->IsBackForwardCacheSupported(*this);
}

FrameTree* WebContentsImpl::LoadingTree() {
  return &GetPrimaryFrameTree();
}

void WebContentsImpl::DidChangeScreenOrientation() {
  last_screen_orientation_change_time_ = ui::EventTimeForNow();
}

void WebContentsImpl::UpdateWebContentsVisibility(Visibility visibility) {
  OPTIONAL_TRACE_EVENT1("content",
                        "WebContentsImpl::UpdateWebContentsVisibility",
                        "visibility", visibility);
  // Occlusion is disabled when
  // |switches::kDisableBackgroundingOccludedWindowsForTesting| is specified on
  // the command line (to avoid flakiness in browser tests).
  const bool occlusion_is_disabled =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableBackgroundingOccludedWindowsForTesting);
  if (occlusion_is_disabled && visibility == Visibility::OCCLUDED) {
    visibility = Visibility::VISIBLE;
  }

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

  if (visibility == visibility_) {
    return;
  }

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

void WebContentsImpl::ShowInsecureLocalhostWarningIfNeeded(PageImpl& page) {
  OPTIONAL_TRACE_EVENT0(
      "content", "WebContentsImpl::ShowInsecureLocalhostWarningIfNeeded");

  bool allow_localhost = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAllowInsecureLocalhost);
  if (!allow_localhost) {
    return;
  }

  RenderFrameHostImpl& frame = page.GetMainDocument();
  NavigationEntry* entry =
      frame.frame_tree()->controller().GetLastCommittedEntry();
  if (!entry || !net::IsLocalhost(entry->GetURL())) {
    return;
  }

  SSLStatus ssl_status = entry->GetSSL();
  if (!net::IsCertStatusError(ssl_status.cert_status)) {
    return;
  }

  frame.AddMessageToConsole(blink::mojom::ConsoleMessageLevel::kWarning,
                            "This site does not have a valid SSL "
                            "certificate! Without SSL, your site's and "
                            "visitors' data is vulnerable to theft and "
                            "tampering. Get a valid SSL certificate before "
                            " releasing your website to the public.");
}

bool WebContentsImpl::IsShowingContextMenuOnPage() const {
  return showing_context_menu_;
}

download::DownloadUrlParameters::RequestHeadersType
WebContentsImpl::ParseDownloadHeaders(const std::string& headers) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::ParseDownloadHeaders",
                        "headers", headers);
  download::DownloadUrlParameters::RequestHeadersType request_headers;
  for (std::string_view key_value : base::SplitStringPiece(
           headers, "\r\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    std::vector<std::string> pair = base::SplitString(
        key_value, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    if (2ul == pair.size()) {
      request_headers.push_back(make_pair(pair[0], pair[1]));
    }
  }
  return request_headers;
}

void WebContentsImpl::SetOpenerForNewContents(FrameTreeNode* opener,
                                              bool opener_suppressed) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::SetOpenerForNewContents");
  if (opener) {
    FrameTreeNode* new_root = GetPrimaryFrameTree().root();

    // For the "original opener", track the opener's main frame instead, because
    // if the opener is a subframe, the opener tracking could be easily bypassed
    // by spawning from a subframe and deleting the subframe.
    // https://crbug.com/705316
    new_root->SetOriginalOpener(opener->frame_tree().root());
    new_root->SetOpenerDevtoolsFrameToken(
        opener->current_frame_host()->devtools_frame_token());
    opened_by_another_window_ = true;

    if (!opener_suppressed) {
      new_root->SetOpener(opener);
    }
  }
}

void WebContentsImpl::MediaMutedStatusChanged(const MediaPlayerId& id,
                                              bool muted) {
  OPTIONAL_TRACE_EVENT1("content", "WebContentsImpl::MediaMutedStatusChanged",
                        "muted", muted);
  observers_.NotifyObservers(&WebContentsObserver::MediaMutedStatusChanged, id,
                             muted);
}

void WebContentsImpl::SetVisibilityForChildViews(bool visible) {
  OPTIONAL_TRACE_EVENT1("content",
                        "WebContentsImpl::SetVisibilityForChildViews",
                        "visible", visible);
  GetPrimaryMainFrame()->SetVisibilityForChildViews(visible);
}

void WebContentsImpl::OnNativeThemeUpdated(ui::NativeTheme* observed_theme) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::OnNativeThemeUpdated");
  DCHECK(native_theme_observation_.IsObservingSource(observed_theme));

  bool using_dark_colors = observed_theme->ShouldUseDarkColors();
  bool in_forced_colors = observed_theme->InForcedColorsMode();
  ui::NativeTheme::PreferredColorScheme preferred_color_scheme =
      observed_theme->GetPreferredColorScheme();
  ui::NativeTheme::PreferredContrast preferred_contrast =
      observed_theme->GetPreferredContrast();
  bool prefers_reduced_transparency =
      observed_theme->GetPrefersReducedTransparency();
  bool inverted_colors = observed_theme->GetInvertedColors();
  base::TimeDelta caret_blink_interval =
      observed_theme->GetCaretBlinkInterval();
  bool preferences_changed = false;

  if (using_dark_colors_ != using_dark_colors) {
    using_dark_colors_ = using_dark_colors;
    preferences_changed = true;
  }
  if (in_forced_colors_ != in_forced_colors) {
    in_forced_colors_ = in_forced_colors;
    preferences_changed = true;
  }
  if (preferred_color_scheme_ != preferred_color_scheme) {
    preferred_color_scheme_ = preferred_color_scheme;
    preferences_changed = true;
  }
  if (preferred_contrast_ != preferred_contrast) {
    preferred_contrast_ = preferred_contrast;
    preferences_changed = true;
  }
  if (prefers_reduced_transparency_ != prefers_reduced_transparency) {
    prefers_reduced_transparency_ = prefers_reduced_transparency;
    preferences_changed = true;
  }
  if (inverted_colors_ != inverted_colors) {
    inverted_colors_ = inverted_colors;
    preferences_changed = true;
  }

  if (preferences_changed) {
    NotifyPreferencesChanged();
  }

  // Only caret blink interval from NativeTheme impacts
  // blink::RendererPreferences, which are not synced in
  // NotifyPreferencesChanged(). Sync these if the interval has changed.
  if (renderer_preferences_.caret_blink_interval != caret_blink_interval) {
    renderer_preferences_.caret_blink_interval = caret_blink_interval;
    SyncRendererPrefs();
  }
}

void WebContentsImpl::OnCaptionStyleUpdated() {
  NotifyPreferencesChanged();
}

void WebContentsImpl::OnColorProviderChanged() {
  // OnColorProviderChanged() might have been triggered as the result of the
  // observed source being reset. If this is the case fallback to the default
  // source.
  if (!GetColorProviderSource()) {
    SetColorProviderSource(DefaultColorProviderSource::GetInstance());
    return;
  }

  blink::ColorProviderColorMaps color_map = GetColorProviderColorMaps();
  ExecutePageBroadcastMethodForAllPages(base::BindRepeating(
      [](const blink::ColorProviderColorMaps& color_map,
         RenderViewHostImpl* rvh) {
        if (auto& broadcast = rvh->GetAssociatedPageBroadcast()) {
          broadcast->UpdateColorProviders(color_map);
        }
      },
      color_map));

  observers_.NotifyObservers(&WebContentsObserver::OnColorProviderChanged);

  // Web preferences may change in response to events such as
  // OnNativeThemeUpdated(). However web preferences may also depend on
  // ColorProvider state and the associated ColorProvider may change
  // independently of the native theme. Ensure we propagate web preferences here
  // to cover this case.
  // OnColorProviderChanged() can be emitted during the WebContentsImpl's
  // constructor in response to setting the ColorProviderSource. In this case
  // Init() will not yet have been called and the current frame host will not be
  // defined, so we must guard against this here.
  // TODO(tluk): There may be a more appropriate way to identify this condition.
  if (GetRenderManager()->current_frame_host()) {
    NotifyPreferencesChanged();
  }
}

const ui::ColorProvider& WebContentsImpl::GetColorProvider() const {
  auto* source = GetColorProviderSource();
  DCHECK(source);
  auto* color_provider = source->GetColorProvider();
  DCHECK(color_provider);
  return *color_provider;
}

void WebContentsImpl::OnSlowWebPreferenceChanged() {
  OnWebPreferencesChanged();
}

blink::mojom::FrameWidgetInputHandler*
WebContentsImpl::GetFocusedFrameWidgetInputHandler() {
  auto* focused_render_widget_host =
      GetFocusedRenderWidgetHost(GetPrimaryMainFrame()->GetRenderWidgetHost());
  if (!focused_render_widget_host) {
    return nullptr;
  }
  return focused_render_widget_host->GetFrameWidgetInputHandler();
}

ukm::SourceId WebContentsImpl::GetCurrentPageUkmSourceId() {
  return GetPrimaryMainFrame()->GetPageUkmSourceId();
}

void WebContentsImpl::ForEachRenderViewHost(
    ForEachRenderViewHostTypes view_mask,
    RenderViewHostIterationCallback on_render_view_host) {
  std::set<RenderViewHostImpl*> render_view_hosts;

  if ((view_mask & (ForEachRenderViewHostTypes::kPrerenderViews |
                    ForEachRenderViewHostTypes::kActiveViews)) != 0) {
    ForEachFrameTree(base::BindRepeating(
        [](ForEachRenderViewHostTypes view_mask,
           std::set<RenderViewHostImpl*>& render_view_hosts,
           FrameTree& frame_tree) {
          // Check the view masks.
          if (frame_tree.is_prerendering()) {
            // We are in a prerendering page.
            if ((view_mask & ForEachRenderViewHostTypes::kPrerenderViews) ==
                0) {
              return;
            }
          } else {
            // We are in an active page.
            if ((view_mask & ForEachRenderViewHostTypes::kActiveViews) == 0) {
              return;
            }
          }
          frame_tree.ForEachRenderViewHost(
              [&render_view_hosts](RenderViewHostImpl* rvh) {
                render_view_hosts.insert(rvh);
              });
        },
        view_mask, std::ref(render_view_hosts)));
  }

  if ((view_mask & ForEachRenderViewHostTypes::kBackForwardCacheViews) != 0) {
    // Add RenderViewHostImpls in BackForwardCache.
    const auto& entries = GetController().GetBackForwardCache().GetEntries();
    for (const auto& entry : entries) {
      for (const auto& render_view : entry->render_view_hosts()) {
        render_view_hosts.insert(&*render_view);
      }
    }
  }

  for (auto* render_view_host : render_view_hosts) {
    on_render_view_host.Run(render_view_host);
  }
}

void WebContentsImpl::NotifyPageBecamePrimary(PageImpl& page) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::PrimaryPageChanged");

  DCHECK_EQ(&page, &GetPrimaryPage());

  // Clear |save_package_| since the primary page changed.
  if (save_package_) {
    save_package_->ClearPage();
    save_package_.reset();
  }
  observers_.NotifyObservers(&WebContentsObserver::PrimaryPageChanged, page);
}

bool WebContentsImpl::IsPageInPreviewMode() const {
  return IsInPreviewMode();
}

void WebContentsImpl::CancelPreviewByMojoBinderPolicy(
    const std::string& interface_name) {
  if (delegate_) {
    delegate_->CancelPreview(
        PreviewCancelReason::BlockedByMojoBinderPolicy(interface_name));
  }
}

void WebContentsImpl::OnCanResizeFromWebAPIChanged() {
  delegate_->OnCanResizeFromWebAPIChanged();
}

FrameTreeNodeId WebContentsImpl::GetOuterDelegateFrameTreeNodeId() {
  return node_.outer_contents_frame_tree_node_id();
}

// Cf. `GetUnattachedOwnedNodes` which applies to the same situation, but is for
// descending.
RenderFrameHostImpl* WebContentsImpl::GetProspectiveOuterDocument() {
  // If the outer WebContents is already known, then there was no need to call
  // this method.
  DCHECK(!GetOuterWebContents());

  RenderFrameHostImpl* unattached_guest_owner =
      browser_plugin_guest_
          ? browser_plugin_guest_->GetProspectiveOuterDocument()
          : nullptr;
  if (unattached_guest_owner) {
    return unattached_guest_owner;
  }

  return nullptr;
}

void WebContentsImpl::RenderFrameHostStateChanged(
    RenderFrameHost* render_frame_host,
    LifecycleState old_state,
    LifecycleState new_state) {
  DCHECK_NE(old_state, new_state);
  OPTIONAL_TRACE_EVENT2("content",
                        "WebContentsImpl::RenderFrameHostStateChanged",
                        "render_frame_host", render_frame_host, "states",
                        [&](perfetto::TracedValue context) {
                          // TODO(crbug.com/40751990): Replace this with passing
                          // more parameters to TRACE_EVENT directly when
                          // available.
                          auto dict = std::move(context).WriteDictionary();
                          dict.Add("old", old_state);
                          dict.Add("new", new_state);
                        });

#if BUILDFLAG(IS_ANDROID)
  if (old_state == LifecycleState::kActive && !render_frame_host->GetParent()) {
    // TODO(sreejakshetty): Remove this reset when ColorChooserHolder becomes
    // per-frame.
    // Close the color chooser popup when RenderFrameHost changes state from
    // kActive.
    color_chooser_holder_.reset();
  }
#endif  // BUILDFLAG(IS_ANDROID)

  observers_.NotifyObservers(&WebContentsObserver::RenderFrameHostStateChanged,
                             render_frame_host, old_state, new_state);
}

void WebContentsImpl::DecrementCapturerCount(bool stay_hidden,
                                             bool stay_awake,
                                             bool is_activity) {
  OPTIONAL_TRACE_EVENT0("content", "WebContentsImpl::DecrementCapturerCount");
  if (stay_hidden) {
    --hidden_capturer_count_;
  } else {
    --visible_capturer_count_;
  }
  if (stay_awake) {
    --stay_awake_capturer_count_;
  }
  DCHECK_GE(hidden_capturer_count_, 0);
  DCHECK_GE(visible_capturer_count_, 0);
  DCHECK_GE(stay_awake_capturer_count_, 0);

  if (IsBeingDestroyed()) {
    return;
  }

  view_->OnCapturerCountChanged();

  const bool is_being_captured = IsBeingCaptured();
  if (!is_being_captured) {
    const gfx::Size old_size = preferred_size_for_capture_;
    preferred_size_for_capture_ = gfx::Size();
    OnPreferredSizeChanged(old_size);
  }

  if (capture_wake_lock_ &&
      (!is_being_captured || !stay_awake_capturer_count_)) {
    capture_wake_lock_->CancelWakeLock();
  }

  UpdateVisibilityAndNotifyPageAndView(GetVisibility(), is_activity);
}

void WebContentsImpl::NotifyPrimaryMainFrameProcessIsAlive() {
  // The WebContents tracks the process state for the primary main frame's
  // renderer.
  // Consider renderer as terminated when exited with any termination status.
  bool was_renderer_terminated = primary_main_frame_process_status_ !=
                                 base::TERMINATION_STATUS_STILL_RUNNING;
  SetPrimaryMainFrameProcessStatus(base::TERMINATION_STATUS_STILL_RUNNING, 0);
  // Restore the focus to the tab (otherwise the focus will be on the top
  // window).
  if (was_renderer_terminated && !FocusLocationBarByDefault()) {
    if (!delegate_ || delegate_->ShouldFocusPageAfterCrash(this)) {
      view_->Focus();
    }
  }
}

void WebContentsImpl::UpdateBrowserControlsState(
    cc::BrowserControlsState constraints,
    cc::BrowserControlsState current,
    bool animate,
    const std::optional<cc::BrowserControlsOffsetTagsInfo>& offset_tags_info) {
  // Browser controls should be synchronised with the scroll state. Therefore,
  // they are controlled from the renderer by the main RenderFrame(Host).
  GetPrimaryPage().UpdateBrowserControlsState(constraints, current, animate,
                                              offset_tags_info);
}

void WebContentsImpl::SetV8CompileHints(base::ReadOnlySharedMemoryRegion data) {
  GetPrimaryMainFrame()->GetAssociatedLocalMainFrame()->SetV8CompileHints(
      std::move(data));
}

void WebContentsImpl::SetTabSwitchStartTime(base::TimeTicks start_time,
                                            bool destination_is_loaded) {
  GetVisibleTimeRequestTrigger().UpdateRequest(
      start_time, destination_is_loaded,
      /*show_reason_tab_switching=*/true,
      /*show_reason_bfcache_restore=*/false);
}

bool WebContentsImpl::IsInPreviewMode() const {
  return is_in_preview_mode_;
}

void WebContentsImpl::WillActivatePreviewPage() {
  CHECK(is_in_preview_mode_);
  is_in_preview_mode_ = false;
}

void WebContentsImpl::ActivatePreviewPage() {
  TRACE_EVENT0("content", "WebContentsImpl::ActivatePreviewPage");

  // WillActivatePreviewPage() should be called to reset it beforehand.
  CHECK(!is_in_preview_mode_);

  PageImpl& preview_page = GetPrimaryPage();
  preview_page.SetActivationStartTime(base::TimeTicks::Now());

  // TODO(b:299240273): Gather all relevant RVHs.
  StoredPage::RenderViewHostImplSafeRefSet render_view_hosts;
  render_view_hosts.insert(GetRenderViewHost()->GetSafeRef());

  preview_page.Activate(
      PageImpl::ActivationType::kPreview, render_view_hosts, std::nullopt,
      base::BindOnce(&WebContentsImpl::DidActivatePreviewedPage,
                     weak_factory_.GetWeakPtr()));
}

VisibleTimeRequestTrigger& WebContentsImpl::GetVisibleTimeRequestTrigger() {
  return visible_time_request_trigger_;
}

gfx::mojom::DelegatedInkPointRenderer* WebContentsImpl::GetDelegatedInkRenderer(
    ui::Compositor* compositor) {
  if (!delegated_ink_point_renderer_.is_bound()) {
    // The remote can't be bound if the compositor is null, so bail if
    // that is the case so we don't crash by trying to use an unbound
    // remote.
    if (!compositor) {
      return nullptr;
    }

    TRACE_EVENT_INSTANT0("delegated_ink_trails",
                         "Binding mojo interface for delegated ink points.",
                         TRACE_EVENT_SCOPE_THREAD);
    compositor->SetDelegatedInkPointRenderer(
        delegated_ink_point_renderer_.BindNewPipeAndPassReceiver());
    delegated_ink_point_renderer_.reset_on_disconnect();
  }
  return delegated_ink_point_renderer_.get();
}

void WebContentsImpl::OnInputIgnored(const blink::WebInputEvent& event) {
#if BUILDFLAG(IS_ANDROID)
  if (auto* animation_manager =
          static_cast<BackForwardTransitionAnimationManagerAndroid*>(
              GetBackForwardTransitionAnimationManager())) {
    animation_manager->MaybeRecordIgnoredInput(event);
  }
#endif
}

void WebContentsImpl::StartPrefetch(
    const GURL& prefetch_url,
    bool use_prefetch_proxy,
    const blink::mojom::Referrer& referrer,
    const std::optional<url::Origin>& referring_origin,
    base::WeakPtr<PreloadingAttempt> attempt,
    std::optional<PreloadingHoldbackStatus> holdback_status_override) {
  if (!base::FeatureList::IsEnabled(
          features::kPrefetchBrowserInitiatedTriggers)) {
    return;
  }

  PrefetchService* prefetch_service =
      BrowserContextImpl::From(GetBrowserContext())->GetPrefetchService();
  if (!prefetch_service) {
    return;
  }

  PrefetchType prefetch_type(PreloadingTriggerType::kEmbedder,
                             use_prefetch_proxy);

  auto container = std::make_unique<PrefetchContainer>(
      *this, prefetch_url, prefetch_type, referrer, referring_origin,
      /*no_vary_search_expected=*/std::nullopt, std::move(attempt),
      holdback_status_override);

  // TODO(crbug.com/40946257): Update this list when prefetch container is
  // eliminated from `PrefetchService`.
  prefetch_containers_.push_back(container->GetWeakPtr());
  prefetch_service->AddPrefetchContainer(std::move(container));
}

std::unique_ptr<PrerenderHandle> WebContentsImpl::StartPrerendering(
    const GURL& prerendering_url,
    PreloadingTriggerType trigger_type,
    const std::string& embedder_histogram_suffix,
    ui::PageTransition page_transition,
    bool should_warm_up_compositor,
    PreloadingHoldbackStatus holdback_status_override,
    PreloadingAttempt* preloading_attempt,
    base::RepeatingCallback<bool(const GURL&,
                                 const std::optional<UrlMatchType>&)>
        url_match_predicate,
    base::RepeatingCallback<void(NavigationHandle&)>
        prerender_navigation_handle_callback) {
  PrerenderAttributes attributes(
      prerendering_url, trigger_type, embedder_histogram_suffix,
      /*target_hint=*/std::nullopt, content::Referrer(),
      /*eagerness=*/std::nullopt,
      /*no_vary_search_expected=*/std::nullopt,
      /*initiator_origin=*/std::nullopt,
      content::ChildProcessHost::kInvalidUniqueID, GetWeakPtr(),
      /*initiator_frame_token=*/std::nullopt,
      /*initiator_frame_tree_node_id=*/FrameTreeNodeId(), ukm::kInvalidSourceId,
      page_transition, should_warm_up_compositor,
      std::move(url_match_predicate),
      std::move(prerender_navigation_handle_callback));
  attributes.holdback_status_override = holdback_status_override;

  FrameTreeNodeId frame_tree_node_id =
      GetPrerenderHostRegistry()->CreateAndStartHost(attributes,
                                                     preloading_attempt);

  if (frame_tree_node_id) {
    return std::make_unique<PrerenderHandleImpl>(
        GetPrerenderHostRegistry()->GetWeakPtr(), frame_tree_node_id,
        prerendering_url);
  }
  return nullptr;
}

void WebContentsImpl::CancelAllPrerendering() {
  GetPrerenderHostRegistry()->CancelAllHosts(
      PrerenderFinalStatus::kAllPrerenderingCanceled);
}

void WebContentsImpl::BackNavigationLikely(PreloadingPredictor predictor,
                                           WindowOpenDisposition disposition) {
  CHECK(!IsBeingDestroyed());

  // See the comment of `last_back_navigation_hint_time_` for why this cooldown
  // exists. The choice of 5 seconds is arbitrary.
  constexpr base::TimeDelta kCooldown = base::Seconds(5);
  base::TimeTicks now = ui::EventTimeForNow();
  if (now - last_back_navigation_hint_time_ < kCooldown) {
    return;
  }
  last_back_navigation_hint_time_ = now;

  if (disposition != WindowOpenDisposition::CURRENT_TAB) {
    RecordPrerenderBackNavigationEligibility(
        predictor, PrerenderBackNavigationEligibility::kTargetingOtherWindow,
        nullptr);
    return;
  }

  GetPrerenderHostRegistry()->BackNavigationLikely(predictor);
}

void WebContentsImpl::SetOwnerLocationForDebug(
    std::optional<base::Location> owner_location) {
  ownership_location_ = owner_location;
}

void WebContentsImpl::AboutToBeDiscarded(WebContents* new_contents) {
  observers_.NotifyObservers(&WebContentsObserver::AboutToBeDiscarded,
                             new_contents);
}

void WebContentsImpl::NotifyWasDiscarded() {
  observers_.NotifyObservers(&WebContentsObserver::WasDiscarded);
}

base::ScopedClosureRunner WebContentsImpl::CreateDisallowCustomCursorScope(
    int max_dimension_dips) {
  auto* render_widget_host_base = GetPrimaryMainFrame()
                                      ->GetRenderWidgetHost()
                                      ->GetRenderWidgetHostViewBase();

  // It's possible for |render_widget_host_base| to be null if the renderer
  // crashed. To avoid race conditions, null-check here. See crbug.com/1421552
  // as well.
  if (!render_widget_host_base) {
    return base::ScopedClosureRunner();
  }

  auto* cursor_manager = render_widget_host_base->GetCursorManager();
  return cursor_manager->CreateDisallowCustomCursorScope(max_dimension_dips);
}

bool WebContentsImpl::CancelPrerendering(FrameTreeNode* frame_tree_node,
                                         PrerenderFinalStatus final_status) {
  if (!frame_tree_node) {
    return false;
  }

  DCHECK_EQ(this, FromFrameTreeNode(frame_tree_node));

  // A prerendered page is identified by its root FrameTreeNode id, so if the
  // given `frame_tree_node` is in any way embedded, we need to iterate up to
  // the prerender root.
  if (frame_tree_node->GetParentOrOuterDocumentOrEmbedder()) {
    return frame_tree_node->GetParentOrOuterDocumentOrEmbedder()
        ->CancelPrerendering(PrerenderCancellationReason(final_status));
  }
  return GetPrerenderHostRegistry()->CancelHost(
      frame_tree_node->frame_tree_node_id(), final_status);
}

ui::mojom::VirtualKeyboardMode WebContentsImpl::GetVirtualKeyboardMode() const {
  return primary_frame_tree_.root()
      ->current_frame_host()
      ->GetPage()
      .virtual_keyboard_mode();
}

void WebContentsImpl::SetOverscrollNavigationEnabled(bool enabled) {
  GetView()->SetOverscrollControllerEnabled(enabled);
}

network::mojom::AttributionSupport WebContentsImpl::GetAttributionSupport() {
  ContentBrowserClient::AttributionReportingOsRegistrars reportTypes =
      AttributionOsLevelManager::GetAttributionReportingOsRegistrars(this);

  return AttributionManager::GetAttributionSupport(
      reportTypes.source_registrar ==
          AttributionReportingOsRegistrar::kDisabled &&
      reportTypes.trigger_registrar ==
          AttributionReportingOsRegistrar::kDisabled);
}

void WebContentsImpl::UpdateAttributionSupportRenderer() {
  OPTIONAL_TRACE_EVENT0("content",
                        "WebContentsImpl::UpdateAttributionSupportRenderer");

  network::mojom::AttributionSupport support = GetAttributionSupport();
  ExecutePageBroadcastMethodForAllPages(base::BindRepeating(
      [](network::mojom::AttributionSupport support, RenderViewHostImpl* rvh) {
        if (auto& broadcast = rvh->GetAssociatedPageBroadcast()) {
          broadcast->SetPageAttributionSupport(support);
        }
      },
      support));
}

BackForwardTransitionAnimationManager*
WebContentsImpl::GetBackForwardTransitionAnimationManager() {
  return GetView()->GetBackForwardTransitionAnimationManager();
}

#if BUILDFLAG(IS_ANDROID)
void WebContentsImpl::SetLongPressLinkSelectText(bool enabled) {
  if (long_press_link_select_text_ == enabled) {
    return;
  }
  long_press_link_select_text_ = enabled;
  NotifyPreferencesChanged();
}
#endif

net::handles::NetworkHandle WebContentsImpl::GetTargetNetwork() {
  return target_network_;
}

// static
void WebContentsImpl::UpdateAttributionSupportAllRenderers() {
  for (WebContentsImpl* web_contents : GetAllWebContents()) {
    web_contents->UpdateAttributionSupportRenderer();
  }
}

void WebContentsImpl::GetMediaCaptureRawDeviceIdsOpened(
    blink::mojom::MediaStreamType type,
    base::OnceCallback<void(std::vector<std::string>)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(type == blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE ||
        type == blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);

  MediaStreamManager* media_stream_manager =
      BrowserMainLoop::GetInstance()->media_stream_manager();
  if (!media_stream_manager) {
    std::move(callback).Run({});
    return;
  }

  media_stream_manager->GetRawDeviceIdsOpenedForFrame(
      GetPrimaryMainFrame(), type,
      base::BindPostTaskToCurrentDefault(std::move(callback)));
}

void WebContentsImpl::WarmUpAndroidSpareRenderer() {
  int renderer_timeout_seconds =
      features::kAndroidSpareRendererTimeoutSeconds.Get();
  if (renderer_timeout_seconds < 0) {
    SpareRenderProcessHostManagerImpl::Get().WarmupSpare(GetBrowserContext());
  } else {
    base::TimeDelta timeout = base::Seconds(renderer_timeout_seconds);
    SpareRenderProcessHostManagerImpl::Get().WarmupSpare(GetBrowserContext(),
                                                         timeout);
  }
}

void WebContentsImpl::SetPartitionedPopinOpenerOnNewWindowIfNeeded(
    WebContentsImpl* new_window,
    const mojom::CreateNewWindowParams& params,
    RenderFrameHostImpl* opener) {
  // We should not take action if the feature is disabled.
  if (!base::FeatureList::IsEnabled(blink::features::kPartitionedPopins)) {
    return;
  }

  // All popins should be counted as popups to ensure proper UX treatment.
  if (!params.features->is_partitioned_popin || !new_window->is_popup_) {
    return;
  }

  new_window->partitioned_popin_opener_ = opener->GetWeakPtr();
  opened_partitioned_popin_ = new_window->GetWeakPtr();
}

}  // namespace content
