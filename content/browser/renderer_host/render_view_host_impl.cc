// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_view_host_impl.h"

#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/hash/hash.h"
#include "base/i18n/rtl.h"
#include "base/json/json_reader.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "content/browser/bad_message.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/renderer_host/input/timeout_monitor.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/scoped_active_url.h"
#include "content/common/browser_plugin/browser_plugin_messages.h"
#include "content/common/content_switches_internal.h"
#include "content/common/frame_messages.h"
#include "content/common/input_messages.h"
#include "content/common/inter_process_time_ticks_converter.h"
#include "content/common/render_message_filter.mojom.h"
#include "content/common/renderer.mojom.h"
#include "content/common/swapped_out_messages.h"
#include "content/common/view_messages.h"
#include "content/common/widget_messages.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "media/base/media_switches.h"
#include "net/base/url_util.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/public/cpp/features.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/device_form_factor.h"
#include "ui/base/pointer/pointer_device.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/display/display_switches.h"
#include "ui/events/blink/blink_features.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gpu_switching_manager.h"
#include "ui/native_theme/native_theme_features.h"
#include "url/url_constants.h"

#if defined(OS_WIN)
#include "ui/display/win/screen_win.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/system_fonts_win.h"
#endif

#if !defined(OS_ANDROID)
#include "content/browser/host_zoom_map_impl.h"
#endif

using base::TimeDelta;

using blink::MediaPlayerAction;
using blink::PluginAction;
using blink::WebConsoleMessage;
using blink::WebInputEvent;

namespace content {
namespace {

// <process id, routing id>
using RenderViewHostID = std::pair<int32_t, int32_t>;
using RoutingIDViewMap =
    std::unordered_map<RenderViewHostID,
                       RenderViewHostImpl*,
                       base::IntPairHash<RenderViewHostID>>;
base::LazyInstance<RoutingIDViewMap>::Leaky g_routing_id_view_map =
    LAZY_INSTANCE_INITIALIZER;

#if defined(OS_WIN)
// Fetches the name and font size of a particular Windows system font.
void GetFontInfo(gfx::win::SystemFont system_font,
                 base::string16* name,
                 int32_t* size) {
  const gfx::Font& font = gfx::win::GetSystemFont(system_font);
  *name = base::UTF8ToUTF16(font.GetFontName());
  *size = font.GetFontSize();
}
#endif  // OS_WIN

}  // namespace

// static
const int64_t RenderViewHostImpl::kUnloadTimeoutMS = 500;

///////////////////////////////////////////////////////////////////////////////
// RenderViewHost, public:

// static
RenderViewHost* RenderViewHost::FromID(int render_process_id,
                                       int render_view_id) {
  return RenderViewHostImpl::FromID(render_process_id, render_view_id);
}

// static
RenderViewHost* RenderViewHost::From(RenderWidgetHost* rwh) {
  return RenderViewHostImpl::From(rwh);
}

///////////////////////////////////////////////////////////////////////////////
// RenderViewHostImpl, public:

// static
RenderViewHostImpl* RenderViewHostImpl::FromID(int process_id, int routing_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RoutingIDViewMap* views = g_routing_id_view_map.Pointer();
  auto it = views->find(RenderViewHostID(process_id, routing_id));
  return it == views->end() ? nullptr : it->second;
}

// static
RenderViewHostImpl* RenderViewHostImpl::From(RenderWidgetHost* rwh) {
  DCHECK(rwh);
  RenderWidgetHostOwnerDelegate* owner_delegate =
      RenderWidgetHostImpl::From(rwh)->owner_delegate();
  if (!owner_delegate)
    return nullptr;
  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(owner_delegate);
  DCHECK_EQ(rwh, rvh->GetWidget());
  return rvh;
}

// static
void RenderViewHostImpl::GetPlatformSpecificPrefs(
    blink::mojom::RendererPreferences* prefs) {
#if defined(OS_WIN)
  // Note that what is called "height" in this struct is actually the font size;
  // font "height" typically includes ascender, descender, and padding and is
  // often a third or so larger than the given font size.
  GetFontInfo(gfx::win::SystemFont::kCaption, &prefs->caption_font_family_name,
              &prefs->caption_font_height);
  GetFontInfo(gfx::win::SystemFont::kSmallCaption,
              &prefs->small_caption_font_family_name,
              &prefs->small_caption_font_height);
  GetFontInfo(gfx::win::SystemFont::kMenu, &prefs->menu_font_family_name,
              &prefs->menu_font_height);
  GetFontInfo(gfx::win::SystemFont::kMessage, &prefs->message_font_family_name,
              &prefs->message_font_height);
  GetFontInfo(gfx::win::SystemFont::kStatus, &prefs->status_font_family_name,
              &prefs->status_font_height);

  prefs->vertical_scroll_bar_width_in_dips =
      display::win::ScreenWin::GetSystemMetricsInDIP(SM_CXVSCROLL);
  prefs->horizontal_scroll_bar_height_in_dips =
      display::win::ScreenWin::GetSystemMetricsInDIP(SM_CYHSCROLL);
  prefs->arrow_bitmap_height_vertical_scroll_bar_in_dips =
      display::win::ScreenWin::GetSystemMetricsInDIP(SM_CYVSCROLL);
  prefs->arrow_bitmap_width_horizontal_scroll_bar_in_dips =
      display::win::ScreenWin::GetSystemMetricsInDIP(SM_CXHSCROLL);
#elif defined(OS_LINUX)
  prefs->system_font_family_name = gfx::Font().GetFontName();
#endif
}

RenderViewHostImpl::RenderViewHostImpl(
    SiteInstance* instance,
    std::unique_ptr<RenderWidgetHostImpl> widget,
    RenderViewHostDelegate* delegate,
    int32_t routing_id,
    int32_t main_frame_routing_id,
    bool swapped_out,
    bool has_initialized_audio_host)
    : render_widget_host_(std::move(widget)),
      delegate_(delegate),
      instance_(static_cast<SiteInstanceImpl*>(instance)),
      is_swapped_out_(swapped_out),
      routing_id_(routing_id),
      main_frame_routing_id_(main_frame_routing_id) {
  DCHECK(instance_.get());
  CHECK(delegate_);  // http://crbug.com/82827
  DCHECK_NE(GetRoutingID(), render_widget_host_->GetRoutingID());

  std::pair<RoutingIDViewMap::iterator, bool> result =
      g_routing_id_view_map.Get().emplace(
          RenderViewHostID(GetProcess()->GetID(), routing_id_), this);
  CHECK(result.second) << "Inserting a duplicate item!";
  GetProcess()->AddRoute(routing_id_, this);

  GetProcess()->AddObserver(this);
  ui::GpuSwitchingManager::GetInstance()->AddObserver(this);

  // New views may be created during RenderProcessHost::ProcessDied(), within a
  // brief window where the internal ChannelProxy is null. This ensures that the
  // ChannelProxy is re-initialized in such cases so that subsequent messages
  // make their way to the new renderer once its restarted.
  GetProcess()->EnableSendQueue();

  if (!is_active())
    GetWidget()->UpdatePriority();

  close_timeout_.reset(new TimeoutMonitor(base::Bind(
      &RenderViewHostImpl::ClosePageTimeout, weak_factory_.GetWeakPtr())));

  input_device_change_observer_.reset(new InputDeviceChangeObserver(this));

  GetWidget()->set_owner_delegate(this);
}

RenderViewHostImpl::~RenderViewHostImpl() {
  // We can't release the SessionStorageNamespace until our peer
  // in the renderer has wound down.
  if (GetProcess()->IsInitializedAndNotDead()) {
    RenderProcessHostImpl::ReleaseOnCloseACK(
        GetProcess(), delegate_->GetSessionStorageNamespaceMap(),
        GetWidget()->GetRoutingID());
  }

  // Destroy the RenderWidgetHost.
  GetWidget()->ShutdownAndDestroyWidget(false);
  if (IsRenderViewLive()) {
    // Destroy the RenderView, which will also destroy the RenderWidget.
    GetProcess()->GetRendererInterface()->DestroyView(GetRoutingID());
  }

  ui::GpuSwitchingManager::GetInstance()->RemoveObserver(this);

  // Detach the routing ID as the object is going away.
  GetProcess()->RemoveRoute(GetRoutingID());
  g_routing_id_view_map.Get().erase(
      RenderViewHostID(GetProcess()->GetID(), GetRoutingID()));

  delegate_->RenderViewDeleted(this);
  GetProcess()->RemoveObserver(this);

  // This can be called inside the FrameTree destructor. When the delegate is
  // the InterstialPageImpl, the |frame_tree| is set to null before deleting it.
  if (FrameTree* frame_tree = GetDelegate()->GetFrameTree()) {
    // If |this| is in the BackForwardCache, then it was already removed from
    // the FrameTree at the time it entered the BackForwardCache.
    if (!is_in_back_forward_cache_)
      frame_tree->UnregisterRenderViewHost(this);
  }
}

RenderViewHostDelegate* RenderViewHostImpl::GetDelegate() {
  return delegate_;
}

SiteInstanceImpl* RenderViewHostImpl::GetSiteInstance() {
  return instance_.get();
}

bool RenderViewHostImpl::CreateRenderView(
    int opener_frame_route_id,
    int proxy_route_id,
    const base::UnguessableToken& devtools_frame_token,
    const FrameReplicationState& replicated_frame_state,
    bool window_was_created_with_opener) {
  TRACE_EVENT0("renderer_host,navigation",
               "RenderViewHostImpl::CreateRenderView");
  DCHECK(!IsRenderViewLive()) << "Creating view twice";

  // The process may (if we're sharing a process with another host that already
  // initialized it) or may not (we have our own process or the old process
  // crashed) have been initialized. Calling Init multiple times will be
  // ignored, so this is safe.
  if (!GetProcess()->Init())
    return false;
  DCHECK(GetProcess()->IsInitializedAndNotDead());
  DCHECK(GetProcess()->GetBrowserContext());

  // Exactly one of main_frame_routing_id_ or proxy_route_id should be set.
  CHECK((main_frame_routing_id_ != MSG_ROUTING_NONE &&
         proxy_route_id == MSG_ROUTING_NONE) ||
        (main_frame_routing_id_ == MSG_ROUTING_NONE &&
         proxy_route_id != MSG_ROUTING_NONE));

  RenderFrameHostImpl* main_rfh = nullptr;
  if (main_frame_routing_id_ != MSG_ROUTING_NONE) {
    main_rfh = RenderFrameHostImpl::FromID(GetProcess()->GetID(),
                                           main_frame_routing_id_);
    DCHECK(main_rfh);
  }

  GetWidget()->set_renderer_initialized(true);

  mojom::CreateViewParamsPtr params = mojom::CreateViewParams::New();
  params->renderer_preferences =
      delegate_->GetRendererPrefs(GetProcess()->GetBrowserContext()).Clone();
  RenderViewHostImpl::GetPlatformSpecificPrefs(
      params->renderer_preferences.get());
  params->web_preferences = GetWebkitPreferences();
  params->view_id = GetRoutingID();
  params->main_frame_routing_id = main_frame_routing_id_;
  params->main_frame_widget_routing_id = render_widget_host_->GetRoutingID();
  if (main_rfh) {
    params->main_frame_interface_bundle =
        mojom::DocumentScopedInterfaceBundle::New();
    main_rfh->BindInterfaceProviderRequest(mojo::MakeRequest(
        &params->main_frame_interface_bundle->interface_provider));
    main_rfh->BindBrowserInterfaceBrokerReceiver(
        params->main_frame_interface_bundle->browser_interface_broker
            .InitWithNewPipeAndPassReceiver());
    RenderWidgetHostImpl* main_rwh = main_rfh->GetRenderWidgetHost();
    params->main_frame_widget_routing_id = main_rwh->GetRoutingID();
  }
  params->session_storage_namespace_id =
      delegate_->GetSessionStorageNamespace(instance_.get())->id();
  // Ensure the RenderView sets its opener correctly.
  params->opener_frame_route_id = opener_frame_route_id;
  params->replicated_frame_state = replicated_frame_state;
  params->proxy_routing_id = proxy_route_id;
  params->hidden = GetWidget()->delegate()->IsHidden();
  params->never_visible = delegate_->IsNeverVisible();
  params->window_was_created_with_opener = window_was_created_with_opener;
  if (main_rfh) {
    params->has_committed_real_load =
        main_rfh->frame_tree_node()->has_committed_real_load();
  }
  params->devtools_main_frame_token = devtools_frame_token;
  // GuestViews in the same StoragePartition need to find each other's frames.
  params->renderer_wide_named_frame_lookup =
      GetSiteInstance()->GetSiteURL().SchemeIs(kGuestScheme);
  params->inside_portal = delegate_->IsPortal();

  // TODO(danakj): Make the visual_properties optional in the message.
  if (proxy_route_id == MSG_ROUTING_NONE) {
    params->visual_properties = GetWidget()->GetVisualProperties();
    GetWidget()->SetInitialVisualProperties(params->visual_properties);
  }

  // The RenderView is owned by this process. This call must be accompanied by a
  // DestroyView [see destructor] or else there will be a leak in the renderer
  // process.
  GetProcess()->GetRendererInterface()->CreateView(std::move(params));

  // Let our delegate know that we created a RenderView.
  DispatchRenderViewCreated();

  // Since this method can create the main RenderFrame in the renderer process,
  // set the proper state on its corresponding RenderFrameHost.
  if (main_rfh)
    main_rfh->SetRenderFrameCreated(true);
  GetWidget()->delegate()->SendScreenRects();
  PostRenderViewReady();

  return true;
}

void RenderViewHostImpl::SetMainFrameRoutingId(int routing_id) {
  main_frame_routing_id_ = routing_id;
  GetWidget()->UpdatePriority();
}

void RenderViewHostImpl::EnterBackForwardCache() {
  FrameTree* frame_tree = GetDelegate()->GetFrameTree();
  frame_tree->UnregisterRenderViewHost(this);
  is_in_back_forward_cache_ = true;
}

void RenderViewHostImpl::LeaveBackForwardCache() {
  FrameTree* frame_tree = GetDelegate()->GetFrameTree();
  // At this point, the frames |this| RenderViewHostImpl belongs to are
  // guaranteed to be committed, so it should be reused going forward.
  frame_tree->RegisterRenderViewHost(this);
  is_in_back_forward_cache_ = false;
}

bool RenderViewHostImpl::IsRenderViewLive() {
  return GetProcess()->IsInitializedAndNotDead() &&
         GetWidget()->renderer_initialized();
}

void RenderViewHostImpl::SetBackgroundOpaque(bool opaque) {
  Send(new ViewMsg_SetBackgroundOpaque(GetRoutingID(), opaque));
}

bool RenderViewHostImpl::IsMainFrameActive() {
  return is_active();
}

bool RenderViewHostImpl::IsNeverVisible() {
  return GetDelegate()->IsNeverVisible();
}

WebPreferences RenderViewHostImpl::GetWebkitPreferencesForWidget() {
  return GetWebkitPreferences();
}

FrameTreeNode* RenderViewHostImpl::GetFocusedFrame() {
  return GetDelegate()->GetFrameTree()->GetFocusedFrame();
}

void RenderViewHostImpl::ShowContextMenu(RenderFrameHost* render_frame_host,
                                         const ContextMenuParams& params) {
  GetDelegate()->GetDelegateView()->ShowContextMenu(render_frame_host, params);
}

const WebPreferences RenderViewHostImpl::ComputeWebPreferences() {
  TRACE_EVENT0("browser", "RenderViewHostImpl::GetWebkitPrefs");
  WebPreferences prefs;

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  SetSlowWebPreferences(command_line, &prefs);

  prefs.web_security_enabled =
      !command_line.HasSwitch(switches::kDisableWebSecurity);

  prefs.remote_fonts_enabled =
      !command_line.HasSwitch(switches::kDisableRemoteFonts);
  prefs.application_cache_enabled = true;
  prefs.local_storage_enabled =
      !command_line.HasSwitch(switches::kDisableLocalStorage);
  prefs.databases_enabled =
      !command_line.HasSwitch(switches::kDisableDatabases);

  prefs.webgl1_enabled = !command_line.HasSwitch(switches::kDisable3DAPIs) &&
                         !command_line.HasSwitch(switches::kDisableWebGL);
  prefs.webgl2_enabled = !command_line.HasSwitch(switches::kDisable3DAPIs) &&
                         !command_line.HasSwitch(switches::kDisableWebGL) &&
                         !command_line.HasSwitch(switches::kDisableWebGL2);

  prefs.pepper_3d_enabled =
      !command_line.HasSwitch(switches::kDisablePepper3d);

  prefs.flash_3d_enabled =
      !command_line.HasSwitch(switches::kDisableFlash3d);
  prefs.flash_stage3d_enabled =
      !command_line.HasSwitch(switches::kDisableFlashStage3d);
  prefs.flash_stage3d_baseline_enabled =
      !command_line.HasSwitch(switches::kDisableFlashStage3d);

  prefs.allow_file_access_from_file_urls =
      command_line.HasSwitch(switches::kAllowFileAccessFromFiles);

  prefs.accelerated_2d_canvas_enabled =
      !command_line.HasSwitch(switches::kDisableAccelerated2dCanvas);
  prefs.antialiased_2d_canvas_disabled =
      command_line.HasSwitch(switches::kDisable2dCanvasAntialiasing);
  prefs.antialiased_clips_2d_canvas_enabled =
      !command_line.HasSwitch(switches::kDisable2dCanvasClipAntialiasing);
  prefs.accelerated_2d_canvas_msaa_sample_count =
      atoi(command_line.GetSwitchValueASCII(
      switches::kAcceleratedCanvas2dMSAASampleCount).c_str());

  prefs.disable_ipc_flooding_protection =
      command_line.HasSwitch(switches::kDisableIpcFloodingProtection) ||
      command_line.HasSwitch(switches::kDisablePushStateThrottle);

  prefs.accelerated_video_decode_enabled =
      !command_line.HasSwitch(switches::kDisableAcceleratedVideoDecode);

  std::string autoplay_policy = media::GetEffectiveAutoplayPolicy(command_line);
  if (autoplay_policy == switches::autoplay::kNoUserGestureRequiredPolicy) {
    prefs.autoplay_policy = AutoplayPolicy::kNoUserGestureRequired;
  } else if (autoplay_policy ==
             switches::autoplay::kUserGestureRequiredPolicy) {
    prefs.autoplay_policy = AutoplayPolicy::kUserGestureRequired;
  } else if (autoplay_policy ==
             switches::autoplay::kDocumentUserActivationRequiredPolicy) {
    prefs.autoplay_policy = AutoplayPolicy::kDocumentUserActivationRequired;
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

  prefs.touch_adjustment_enabled =
      !command_line.HasSwitch(switches::kDisableTouchAdjustment);

  prefs.enable_scroll_animator =
      command_line.HasSwitch(switches::kEnableSmoothScrolling) ||
      (!command_line.HasSwitch(switches::kDisableSmoothScrolling) &&
      gfx::Animation::ScrollAnimationsEnabledBySystem());

  prefs.prefers_reduced_motion = gfx::Animation::PrefersReducedMotion();

  if (ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
          GetProcess()->GetID())) {
    prefs.loads_images_automatically = true;
    prefs.javascript_enabled = true;
  }

  prefs.viewport_enabled = command_line.HasSwitch(switches::kEnableViewport);

  if (delegate_ && delegate_->IsOverridingUserAgent())
    prefs.viewport_meta_enabled = false;

  prefs.main_frame_resizes_are_orientation_changes =
      command_line.HasSwitch(switches::kMainFrameResizesAreOrientationChanges);

  prefs.spatial_navigation_enabled = command_line.HasSwitch(
      switches::kEnableSpatialNavigation);

  if (delegate_ && delegate_->IsSpatialNavigationDisabled())
    prefs.spatial_navigation_enabled = false;

  prefs.caret_browsing_enabled =
      command_line.HasSwitch(switches::kEnableCaretBrowsing);

  prefs.disable_reading_from_canvas = command_line.HasSwitch(
      switches::kDisableReadingFromCanvas);

  prefs.strict_mixed_content_checking = command_line.HasSwitch(
      switches::kEnableStrictMixedContentChecking);

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

  if (delegate_ && delegate_->HideDownloadUI())
    prefs.hide_download_ui = true;

  // `media_controls_enabled` is `true` by default.
  if (delegate_ && delegate_->HasPersistentVideo())
    prefs.media_controls_enabled = false;

  GetContentClient()->browser()->OverrideWebkitPrefs(this, &prefs);
  return prefs;
}

void RenderViewHostImpl::SetSlowWebPreferences(
    const base::CommandLine& command_line,
    WebPreferences* prefs) {
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

void RenderViewHostImpl::DispatchRenderViewCreated() {
  if (has_notified_about_creation_)
    return;

  // Only send RenderViewCreated if there is a current or pending main frame
  // RenderFrameHost (current or pending).  Don't send notifications if this is
  // an inactive RVH that is either used by subframe RFHs or not used by any
  // RFHs at all (e.g., when created for the opener chain).
  //
  // While it would be nice to uniformly dispatch RenderViewCreated for all
  // cases, some existing code (e.g., ExtensionViewHost) assumes it won't
  // hear RenderViewCreated for a RVH created for an OOPIF.
  //
  // TODO(alexmos, creis): Revisit this as part of migrating RenderViewCreated
  // usage to RenderFrameCreated.  See https://crbug.com/763548.
  if (!GetMainFrame())
    return;

  delegate_->RenderViewCreated(this);
  has_notified_about_creation_ = true;
}

void RenderViewHostImpl::ClosePage() {
  is_waiting_for_close_ack_ = true;

  bool is_javascript_dialog_showing = delegate_->IsJavaScriptDialogShowing();

  // If there is a JavaScript dialog up, don't bother sending the renderer the
  // close event because it is known unresponsive, waiting for the reply from
  // the dialog.
  if (IsRenderViewLive() && !is_javascript_dialog_showing) {
    close_timeout_->Start(TimeDelta::FromMilliseconds(kUnloadTimeoutMS));

    // TODO(creis): Should this be moved to Shutdown?  It may not be called for
    // RenderViewHosts that have been swapped out.
#if !defined(OS_ANDROID)
    static_cast<HostZoomMapImpl*>(HostZoomMap::Get(GetSiteInstance()))
        ->WillCloseRenderView(GetProcess()->GetID(), GetRoutingID());
#endif

    Send(new ViewMsg_ClosePage(GetRoutingID()));
  } else {
    // This RenderViewHost doesn't have a live renderer, so just skip the close
    // event and close the page.
    ClosePageIgnoringUnloadEvents();
  }
}

void RenderViewHostImpl::ClosePageIgnoringUnloadEvents() {
  close_timeout_->Stop();
  is_waiting_for_close_ack_ = false;

  sudden_termination_allowed_ = true;
  delegate_->Close(this);
}

void RenderViewHostImpl::RenderProcessExited(
    RenderProcessHost* host,
    const ChildProcessTerminationInfo& info) {
  if (!GetWidget()->renderer_initialized())
    return;

  GetWidget()->RendererExited();
  delegate_->RenderViewTerminated(this, info.status, info.exit_code);
}

bool RenderViewHostImpl::Send(IPC::Message* msg) {
  return GetWidget()->Send(msg);
}

RenderWidgetHostImpl* RenderViewHostImpl::GetWidget() {
  return render_widget_host_.get();
}

RenderProcessHost* RenderViewHostImpl::GetProcess() {
  return GetWidget()->GetProcess();
}

int RenderViewHostImpl::GetRoutingID() {
  return routing_id_;
}

RenderFrameHost* RenderViewHostImpl::GetMainFrame() {
  // If the RenderViewHost is active, it should always have a main frame
  // RenderFrameHost.  If it is inactive, it could've been created for a
  // pending main frame navigation, in which case it will transition to active
  // once that navigation commits. In this case, return the pending main frame
  // RenderFrameHost, as that's expected by certain code paths,
  // such as RenderViewHostImpl::SetUIProperty().  If there's no pending main
  // frame navigation, return nullptr.
  //
  // TODO(alexmos, creis): Migrate these code paths to use RenderFrameHost APIs
  // and remove this fallback.  See https://crbug.com/763548.
  if (is_active()) {
    return RenderFrameHostImpl::FromID(GetProcess()->GetID(),
                                       main_frame_routing_id_);
  }
  return delegate_->GetPendingMainFrame();
}

void RenderViewHostImpl::RenderWidgetGotFocus() {
  RenderViewHostDelegateView* view = delegate_->GetDelegateView();
  if (view)
    view->GotFocus(GetWidget());
}

void RenderViewHostImpl::RenderWidgetLostFocus() {
  RenderViewHostDelegateView* view = delegate_->GetDelegateView();
  if (view)
    view->LostFocus(GetWidget());
}

void RenderViewHostImpl::SetInitialFocus(bool reverse) {
  Send(new ViewMsg_SetInitialFocus(GetRoutingID(), reverse));
}

void RenderViewHostImpl::RenderWidgetDidFirstVisuallyNonEmptyPaint() {
  did_first_visually_non_empty_paint_ = true;
  delegate_->DidFirstVisuallyNonEmptyPaint(this);
}

bool RenderViewHostImpl::SuddenTerminationAllowed() const {
  return sudden_termination_allowed_;
}

///////////////////////////////////////////////////////////////////////////////
// RenderViewHostImpl, IPC message handlers:

bool RenderViewHostImpl::OnMessageReceived(const IPC::Message& msg) {
  // Filter out most IPC messages if this renderer is swapped out.
  // We still want to handle certain ACKs to keep our state consistent.
  if (is_swapped_out_) {
    if (!SwappedOutMessages::CanHandleWhileSwappedOut(msg)) {
      // If this is a synchronous message and we decided not to handle it,
      // we must send an error reply, or else the renderer will be stuck
      // and won't respond to future requests.
      if (msg.is_sync()) {
        IPC::Message* reply = IPC::SyncMessage::GenerateReply(&msg);
        reply->set_reply_error();
        Send(reply);
      }
      // Don't continue looking for someone to handle it.
      return true;
    }
  }

  // Crash reports trigerred by the IPC messages below should be associated
  // with URL of the main frame.
  ScopedActiveURL scoped_active_url(this);

  if (delegate_->OnMessageReceived(this, msg))
    return true;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderViewHostImpl, msg)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShowWidget, OnShowWidget)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShowFullscreenWidget,
                        OnShowFullscreenWidget)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RouteCloseEvent, OnRouteCloseEvent)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateTargetURL, OnUpdateTargetURL)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DocumentAvailableInMainFrame,
                        OnDocumentAvailableInMainFrame)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidContentsPreferredSizeChange,
                        OnDidContentsPreferredSizeChange)
    IPC_MESSAGE_HANDLER(ViewHostMsg_TakeFocus, OnTakeFocus)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ClosePage_ACK, OnClosePageACK)
    IPC_MESSAGE_HANDLER(ViewHostMsg_Focus, OnFocus)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void RenderViewHostImpl::RenderWidgetDidInit() {
  PostRenderViewReady();
}

void RenderViewHostImpl::RenderWidgetDidClose() {
  // If the renderer is telling us to close, it has already run the unload
  // events, and we can take the fast path.
  ClosePageIgnoringUnloadEvents();
}

void RenderViewHostImpl::CreateNewWidget(
    int32_t widget_route_id,
    mojo::PendingRemote<mojom::Widget> widget) {
  delegate_->CreateNewWidget(GetProcess()->GetID(), widget_route_id,
                             std::move(widget), this);
}

void RenderViewHostImpl::CreateNewFullscreenWidget(
    int32_t widget_route_id,
    mojo::PendingRemote<mojom::Widget> widget) {
  delegate_->CreateNewFullscreenWidget(GetProcess()->GetID(), widget_route_id,
                                       std::move(widget), this);
}

void RenderViewHostImpl::OnShowWidget(int widget_route_id,
                                      const gfx::Rect& initial_rect) {
  delegate_->ShowCreatedWidget(GetProcess()->GetID(), widget_route_id,
                               initial_rect);
  Send(new WidgetMsg_SetBounds_ACK(widget_route_id));
}

void RenderViewHostImpl::OnShowFullscreenWidget(int widget_route_id) {
  delegate_->ShowCreatedFullscreenWidget(GetProcess()->GetID(),
                                         widget_route_id);
  Send(new WidgetMsg_SetBounds_ACK(widget_route_id));
}

void RenderViewHostImpl::OnRouteCloseEvent() {
  // This is only used when the RenderViewHost is not active, to signal to
  // the active RenderViewHost that JS has requested the page to close.
  //
  // TODO(https://crbug.com/419087): Move to RenderFrameHost or
  // RenderFrameProxyHost.
  //
  // The delegate will route the close request to the active RenderViewHost.
  delegate_->RouteCloseEvent(this);
}

void RenderViewHostImpl::OnUpdateTargetURL(const GURL& url) {
  delegate_->UpdateTargetURL(this, url);

  // Send a notification back to the renderer that we are ready to
  // receive more target urls.
  Send(new ViewMsg_UpdateTargetURL_ACK(GetRoutingID()));
}

void RenderViewHostImpl::OnDocumentAvailableInMainFrame(
    bool uses_temporary_zoom_level) {
  delegate_->DocumentAvailableInMainFrame(this);

  if (!uses_temporary_zoom_level)
    return;

#if !defined(OS_ANDROID)
  HostZoomMapImpl* host_zoom_map =
      static_cast<HostZoomMapImpl*>(HostZoomMap::Get(GetSiteInstance()));
  host_zoom_map->SetTemporaryZoomLevel(GetProcess()->GetID(),
                                       GetRoutingID(),
                                       host_zoom_map->GetDefaultZoomLevel());
#endif  // !defined(OS_ANDROID)
}

void RenderViewHostImpl::OnDidContentsPreferredSizeChange(
    const gfx::Size& new_size) {
  delegate_->UpdatePreferredSize(new_size);
}

void RenderViewHostImpl::OnTakeFocus(bool reverse) {
  RenderViewHostDelegateView* view = delegate_->GetDelegateView();
  if (view)
    view->TakeFocus(reverse);
}

void RenderViewHostImpl::OnClosePageACK() {
  ClosePageIgnoringUnloadEvents();
}

void RenderViewHostImpl::OnFocus() {
  // Note: We allow focus and blur from swapped out RenderViewHosts, even when
  // the active RenderViewHost is in a different BrowsingInstance (e.g., WebUI).
  delegate_->Activate();
}

void RenderViewHostImpl::RenderWidgetDidForwardMouseEvent(
    const blink::WebMouseEvent& mouse_event) {
  if (mouse_event.GetType() == WebInputEvent::kMouseWheel &&
      GetWidget()->IsIgnoringInputEvents()) {
    delegate_->OnIgnoredUIEvent();
  }
}

bool RenderViewHostImpl::MayRenderWidgetForwardKeyboardEvent(
    const NativeWebKeyboardEvent& key_event) {
  if (GetWidget()->IsIgnoringInputEvents()) {
    if (key_event.GetType() == WebInputEvent::kRawKeyDown)
      delegate_->OnIgnoredUIEvent();
    return false;
  }
  return true;
}

bool RenderViewHostImpl::ShouldContributePriorityToProcess() {
  return is_active();
}

void RenderViewHostImpl::RequestSetBounds(const gfx::Rect& bounds) {
  if (is_active())
    delegate_->RequestSetBounds(bounds);
}

WebPreferences RenderViewHostImpl::GetWebkitPreferences() {
  if (!web_preferences_.get()) {
    OnWebkitPreferencesChanged();
  }
  return *web_preferences_;
}

void RenderViewHostImpl::UpdateWebkitPreferences(const WebPreferences& prefs) {
  web_preferences_.reset(new WebPreferences(prefs));
  Send(new ViewMsg_UpdateWebPreferences(GetRoutingID(), prefs));
}

void RenderViewHostImpl::OnWebkitPreferencesChanged() {
  // This is defensive code to avoid infinite loops due to code run inside
  // UpdateWebkitPreferences() accidentally updating more preferences and thus
  // calling back into this code. See crbug.com/398751 for one past example.
  if (updating_web_preferences_)
    return;
  updating_web_preferences_ = true;
  UpdateWebkitPreferences(ComputeWebPreferences());
#if defined(OS_ANDROID)
  for (FrameTreeNode* node : GetDelegate()->GetFrameTree()->Nodes()) {
    RenderFrameHostImpl* rfh = node->current_frame_host();
    if (rfh->is_local_root()) {
      if (auto* rwh = rfh->GetRenderWidgetHost())
        rwh->SetForceEnableZoom(web_preferences_->force_enable_zoom);
    }
  }
#endif
  updating_web_preferences_ = false;
}

void RenderViewHostImpl::OnHardwareConfigurationChanged() {
  // OnWebkitPreferencesChanged is a no-op when this is true.
  if (updating_web_preferences_)
    return;
  web_preferences_.reset();
  OnWebkitPreferencesChanged();
}

void RenderViewHostImpl::EnablePreferredSizeMode() {
  Send(new ViewMsg_EnablePreferredSizeChangedMode(GetRoutingID()));
}

void RenderViewHostImpl::ExecutePluginActionAtLocation(
    const gfx::Point& location,
    const blink::PluginAction& action) {
  // TODO(wjmaclean): See if this needs to be done for OOPIFs as well.
  // https://crbug.com/776807
  gfx::PointF local_location_f =
      GetWidget()->GetView()->TransformRootPointToViewCoordSpace(
          gfx::PointF(location.x(), location.y()));
  gfx::Point local_location(local_location_f.x(), local_location_f.y());
  Send(new ViewMsg_PluginActionAt(GetRoutingID(), local_location, action));
}

void RenderViewHostImpl::NotifyMoveOrResizeStarted() {
  Send(new ViewMsg_MoveOrResizeStarted(GetRoutingID()));
}

void RenderViewHostImpl::PostRenderViewReady() {
  GetProcess()->PostTaskWhenProcessIsReady(base::BindOnce(
      &RenderViewHostImpl::RenderViewReady, weak_factory_.GetWeakPtr()));
}

void RenderViewHostImpl::OnGpuSwitched(gl::GpuPreference active_gpu_heuristic) {
  OnHardwareConfigurationChanged();
}

void RenderViewHostImpl::RenderViewReady() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  delegate_->RenderViewReady(this);
}

void RenderViewHostImpl::ClosePageTimeout() {
  if (delegate_->ShouldIgnoreUnresponsiveRenderer())
    return;

  ClosePageIgnoringUnloadEvents();
}

std::vector<viz::SurfaceId> RenderViewHostImpl::CollectSurfaceIdsForEviction() {
  if (!is_active())
    return {};
  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(GetMainFrame());
  if (!rfh || !rfh->IsCurrent())
    return {};
  FrameTreeNode* root = rfh->frame_tree_node();
  FrameTree* tree = root->frame_tree();
  std::vector<viz::SurfaceId> ids;
  for (FrameTreeNode* node : tree->SubtreeNodes(root)) {
    if (!node->current_frame_host()->is_local_root())
      continue;
    RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
        node->current_frame_host()->GetView());
    if (!view)
      continue;
    viz::SurfaceId id = view->GetCurrentSurfaceId();
    if (id.is_valid())
      ids.push_back(id);
    view->set_is_evicted();
  }
  return ids;
}

void RenderViewHostImpl::ResetPerPageState() {
  did_first_visually_non_empty_paint_ = false;
  main_frame_theme_color_.reset();
  is_document_on_load_completed_in_main_frame_ = false;
}

void RenderViewHostImpl::OnThemeColorChanged(
    RenderFrameHostImpl* rfh,
    const base::Optional<SkColor>& theme_color) {
  if (GetMainFrame() != rfh)
    return;
  main_frame_theme_color_ = theme_color;
  delegate_->OnThemeColorChanged(this);
}

void RenderViewHostImpl::DocumentOnLoadCompletedInMainFrame() {
  is_document_on_load_completed_in_main_frame_ = true;
}

bool RenderViewHostImpl::IsDocumentOnLoadCompletedInMainFrame() {
  return is_document_on_load_completed_in_main_frame_;
}

bool RenderViewHostImpl::IsTestRenderViewHost() const {
  return false;
}

}  // namespace content
