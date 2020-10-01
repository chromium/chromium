// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_view_host_impl.h"

#include <algorithm>
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
#include "base/supports_user_data.h"
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
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/renderer_host/agent_scheduling_group_host.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/input/timeout_monitor.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/scoped_active_url.h"
#include "content/common/content_switches_internal.h"
#include "content/common/frame_messages.h"
#include "content/common/input_messages.h"
#include "content/common/inter_process_time_ticks_converter.h"
#include "content/common/page_messages.h"
#include "content/common/render_message_filter.mojom.h"
#include "content/common/renderer.mojom.h"
#include "content/common/view_messages.h"
#include "content/common/widget_messages.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/context_menu_params.h"
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
#include "content/public/common/result_codes.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "media/base/media_switches.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/device_form_factor.h"
#include "ui/base/pointer/pointer_device.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/display/display.h"
#include "ui/display/display_switches.h"
#include "ui/display/screen.h"
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

#if defined(USE_OZONE)
#include "ui/base/ui_base_features.h"
#endif

using base::TimeDelta;

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

#if defined(USE_OZONE) || defined(USE_X11)
bool IsSelectionBufferAvailable() {
#if defined(USE_OZONE)
  if (features::IsUsingOzonePlatform())
    return ui::Clipboard::GetForCurrentThread()->IsSelectionBufferAvailable();
#endif
#if defined(USE_X11)
  return true;
#else
  return false;
#endif
}
#endif  // defined(USE_OZONE) || defined(USE_X11)

// Set of RenderViewHostImpl* that can be attached as UserData to a
// RenderProcessHost. Used to keep track of whether any RenderViewHostImpl
// instances are in the bfcache.
class PerProcessRenderViewHostSet : public base::SupportsUserData::Data {
 public:
  static PerProcessRenderViewHostSet* GetOrCreateForProcess(
      RenderProcessHost* process) {
    DCHECK(process);
    auto* set = static_cast<PerProcessRenderViewHostSet*>(
        process->GetUserData(UserDataKey()));
    if (!set) {
      auto new_set = std::make_unique<PerProcessRenderViewHostSet>();
      set = new_set.get();
      process->SetUserData(UserDataKey(), std::move(new_set));
    }
    return set;
  }

  void Insert(const RenderViewHostImpl* rvh) {
    render_view_host_instances_.insert(rvh);
  }

  void Erase(const RenderViewHostImpl* rvh) {
    auto it = render_view_host_instances_.find(rvh);
    DCHECK(it != render_view_host_instances_.end());
    render_view_host_instances_.erase(it);
  }

  bool HasNonBackForwardCachedInstances() const {
    return std::find_if(render_view_host_instances_.begin(),
                        render_view_host_instances_.end(),
                        [](const RenderViewHostImpl* rvh) {
                          return !rvh->is_in_back_forward_cache();
                        }) != render_view_host_instances_.end();
  }

 private:
  static const void* UserDataKey() { return &kUserDataKey; }

  static constexpr int kUserDataKey = 0;

  std::unordered_set<const RenderViewHostImpl*> render_view_host_instances_;
};

const int PerProcessRenderViewHostSet::kUserDataKey;

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
#elif defined(OS_LINUX) || defined(OS_CHROMEOS)
  prefs->system_font_family_name = gfx::Font().GetFontName();
#elif defined(OS_FUCHSIA)
  // Make Blink's "focus ring" invisible. The focus ring is a hairline border
  // that's rendered around clickable targets.
  // TODO(crbug.com/1066605): Consider exposing this as a FIDL parameter.
  prefs->focus_ring_color = SK_AlphaTRANSPARENT;
#endif
#if defined(USE_OZONE) || defined(USE_X11)
  prefs->selection_clipboard_buffer_available = IsSelectionBufferAvailable();
#endif
}

// static
bool RenderViewHostImpl::HasNonBackForwardCachedInstancesForProcess(
    RenderProcessHost* process) {
  return PerProcessRenderViewHostSet::GetOrCreateForProcess(process)
      ->HasNonBackForwardCachedInstances();
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
      routing_id_(routing_id),
      main_frame_routing_id_(main_frame_routing_id) {
  DCHECK(instance_.get());
  DCHECK(delegate_);
  DCHECK_NE(GetRoutingID(), render_widget_host_->GetRoutingID());

  PerProcessRenderViewHostSet::GetOrCreateForProcess(GetProcess())
      ->Insert(this);

  std::pair<RoutingIDViewMap::iterator, bool> result =
      g_routing_id_view_map.Get().emplace(
          RenderViewHostID(GetProcess()->GetID(), routing_id_), this);
  CHECK(result.second) << "Inserting a duplicate item!";
  GetAgentSchedulingGroup().AddRoute(routing_id_, this);

  GetProcess()->AddObserver(this);
  ui::GpuSwitchingManager::GetInstance()->AddObserver(this);

  // New views may be created during RenderProcessHost::ProcessDied(), within a
  // brief window where the internal ChannelProxy is null. This ensures that the
  // ChannelProxy is re-initialized in such cases so that subsequent messages
  // make their way to the new renderer once its restarted.
  // TODO(crbug.com/1111231): Should this go via ASGH? Is it even needed after
  // the migration?
  GetProcess()->EnableSendQueue();

  if (!is_active())
    GetWidget()->UpdatePriority();

  close_timeout_ = std::make_unique<TimeoutMonitor>(base::BindRepeating(
      &RenderViewHostImpl::ClosePageTimeout, weak_factory_.GetWeakPtr()));

  input_device_change_observer_ =
      std::make_unique<InputDeviceChangeObserver>(this);

  bool initially_visible = !GetWidget()->delegate()->IsHidden();
  page_lifecycle_state_manager_ = std::make_unique<PageLifecycleStateManager>(
      this, initially_visible ? blink::mojom::PageVisibilityState::kVisible
                              : blink::mojom::PageVisibilityState::kHidden);

  GetWidget()->set_owner_delegate(this);
  GetDelegate()->GetFrameTree()->RegisterRenderViewHost(this);
}

RenderViewHostImpl::~RenderViewHostImpl() {
  PerProcessRenderViewHostSet::GetOrCreateForProcess(GetProcess())->Erase(this);

  // We can't release the SessionStorageNamespace until our peer
  // in the renderer has wound down.
  // TODO(crbug.com/1111231): `ReleaseOnCloseACK()` should probably be called on
  // the ASGH rather than the RPHI. If that happens, does it still make sense to
  // test if the process is still alive, or should that be encapsulated in
  // `ASGH::ReleaseOnCloseAck()`?
  if (GetProcess()->IsInitializedAndNotDead()) {
    RenderProcessHostImpl::ReleaseOnCloseACK(
        GetProcess(), delegate_->GetSessionStorageNamespaceMap(),
        GetWidget()->GetRoutingID());
  }

  // Destroy the RenderWidgetHost.
  GetWidget()->ShutdownAndDestroyWidget(false);
  if (IsRenderViewLive()) {
    // Destroy the RenderView, which will also destroy the RenderWidget.
    GetAgentSchedulingGroup().DestroyView(GetRoutingID());
  }

  ui::GpuSwitchingManager::GetInstance()->RemoveObserver(this);

  // Detach the routing ID as the object is going away.
  GetAgentSchedulingGroup().RemoveRoute(GetRoutingID());
  g_routing_id_view_map.Get().erase(
      RenderViewHostID(GetProcess()->GetID(), GetRoutingID()));

  delegate_->RenderViewDeleted(this);
  GetProcess()->RemoveObserver(this);

  // If |this| is in the BackForwardCache, then it was already removed from
  // the FrameTree at the time it entered the BackForwardCache.
  if (!is_in_back_forward_cache_)
    GetDelegate()->GetFrameTree()->UnregisterRenderViewHost(this);
}

RenderViewHostDelegate* RenderViewHostImpl::GetDelegate() {
  return delegate_;
}

SiteInstanceImpl* RenderViewHostImpl::GetSiteInstance() {
  return instance_.get();
}

bool RenderViewHostImpl::CreateRenderView(
    const base::Optional<base::UnguessableToken>& opener_frame_token,
    int proxy_route_id,
    bool window_was_created_with_opener) {
  TRACE_EVENT0("renderer_host,navigation",
               "RenderViewHostImpl::CreateRenderView");
  DCHECK(!IsRenderViewLive()) << "Creating view twice";

  // The process may (if we're sharing a process with another host that already
  // initialized it) or may not (we have our own process or the old process
  // crashed) have been initialized. Calling Init multiple times will be
  // ignored, so this is safe.
  if (!GetAgentSchedulingGroup().InitProcessAndMojos())
    return false;
  DCHECK(GetProcess()->IsInitializedAndNotDead());
  DCHECK(GetProcess()->GetBrowserContext());

  // Exactly one of main_frame_routing_id_ or proxy_route_id should be set.
  CHECK(!(main_frame_routing_id_ != MSG_ROUTING_NONE &&
          proxy_route_id != MSG_ROUTING_NONE));
  CHECK(!(main_frame_routing_id_ == MSG_ROUTING_NONE &&
          proxy_route_id == MSG_ROUTING_NONE));

  RenderFrameHostImpl* main_rfh = nullptr;
  RenderFrameProxyHost* main_rfph = nullptr;
  if (main_frame_routing_id_ != MSG_ROUTING_NONE) {
    main_rfh = RenderFrameHostImpl::FromID(GetProcess()->GetID(),
                                           main_frame_routing_id_);
    DCHECK(main_rfh);
  } else {
    main_rfph =
        RenderFrameProxyHost::FromID(GetProcess()->GetID(), proxy_route_id);
  }
  const FrameTreeNode* const frame_tree_node =
      main_rfh ? main_rfh->frame_tree_node() : main_rfph->frame_tree_node();

  GetWidget()->set_renderer_initialized(true);

  mojom::CreateViewParamsPtr params = mojom::CreateViewParams::New();
  params->renderer_preferences = delegate_->GetRendererPrefs().Clone();
  RenderViewHostImpl::GetPlatformSpecificPrefs(
      params->renderer_preferences.get());
  params->web_preferences = delegate_->GetOrCreateWebPreferences();
  params->view_id = GetRoutingID();
  if (main_rfh) {
    params->main_frame_routing_id = main_frame_routing_id_;
    params->main_frame_widget_routing_id =
        main_rfh->GetRenderWidgetHost()->GetRoutingID();
    params->main_frame_interface_bundle =
        mojom::DocumentScopedInterfaceBundle::New();
    main_rfh->BindInterfaceProviderReceiver(
        params->main_frame_interface_bundle->interface_provider
            .InitWithNewPipeAndPassReceiver());
    main_rfh->BindBrowserInterfaceBrokerReceiver(
        params->main_frame_interface_bundle->browser_interface_broker
            .InitWithNewPipeAndPassReceiver());

    std::tie(params->widget_host, params->widget) =
        main_rfh->GetRenderWidgetHost()->BindNewWidgetInterfaces();
    std::tie(params->frame_widget_host, params->frame_widget) =
        main_rfh->GetRenderWidgetHost()->BindNewFrameWidgetInterfaces();
  }
  params->main_frame_frame_token =
      main_rfh ? main_rfh->GetFrameToken() : main_rfph->GetFrameToken();
  params->session_storage_namespace_id =
      delegate_->GetSessionStorageNamespace(instance_.get())->id();
  // Ensure the RenderView sets its opener correctly.
  params->opener_frame_token = opener_frame_token;
  params->replicated_frame_state = frame_tree_node->current_replication_state();
  params->proxy_routing_id = proxy_route_id;
  params->hidden = GetWidget()->delegate()->IsHidden();
  params->never_composited = delegate_->IsNeverComposited();
  params->window_was_created_with_opener = window_was_created_with_opener;
  if (main_rfh) {
    params->has_committed_real_load =
        main_rfh->frame_tree_node()->has_committed_real_load();
    DCHECK_EQ(params->main_frame_frame_token, main_rfh->GetFrameToken());
  }
  params->devtools_main_frame_token = frame_tree_node->devtools_frame_token();
  // GuestViews in the same StoragePartition need to find each other's frames.
  params->renderer_wide_named_frame_lookup = GetSiteInstance()->IsGuest();

  bool is_portal = delegate_->IsPortal();
  bool is_guest_view = GetSiteInstance()->IsGuest();

  // A view cannot be inside both a <portal> and inside a <webview>.
  DCHECK(!is_portal || !is_guest_view);
  if (is_portal) {
    params->type = mojom::ViewWidgetType::kPortal;
  } else if (is_guest_view) {
    params->type = mojom::ViewWidgetType::kGuestView;
  } else {
    params->type = mojom::ViewWidgetType::kTopLevel;
  }

  // RenderViweHostImpls is reused after a crash, so reset any endpoint that
  // might be a leftover from a crash.
  page_broadcast_.reset();
  params->blink_page_broadcast =
      page_broadcast_.BindNewEndpointAndPassReceiver();
  // TODO(danakj): Make the visual_properties optional in the message.
  if (proxy_route_id == MSG_ROUTING_NONE) {
    params->visual_properties = GetWidget()->GetInitialVisualProperties();
  }

  // The renderer process's `RenderView` is owned by this `RenderViewHost`. This
  // call must, therefore, be accompanied by a `DestroyView()` [see destructor]
  // or else there will be a leak in the renderer process.
  GetAgentSchedulingGroup().CreateView(std::move(params));

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
  if (!will_enter_back_forward_cache_callback_for_testing_.is_null())
    will_enter_back_forward_cache_callback_for_testing_.Run();

  TRACE_EVENT0("navigation", "RenderViewHostImpl::EnterBackForwardCache");
  GetDelegate()->GetFrameTree()->UnregisterRenderViewHost(this);
  is_in_back_forward_cache_ = true;
  page_lifecycle_state_manager_->SetIsInBackForwardCache(
      is_in_back_forward_cache_, /*page_restore_params=*/nullptr);
}

void RenderViewHostImpl::LeaveBackForwardCache(
    blink::mojom::PageRestoreParamsPtr page_restore_params) {
  TRACE_EVENT0("navigation", "RenderViewHostImpl::LeaveBackForwardCache");
  // At this point, the frames |this| RenderViewHostImpl belongs to are
  // guaranteed to be committed, so it should be reused going forward.
  GetDelegate()->GetFrameTree()->RegisterRenderViewHost(this);
  is_in_back_forward_cache_ = false;
  page_lifecycle_state_manager_->SetIsInBackForwardCache(
      is_in_back_forward_cache_, std::move(page_restore_params));
}

void RenderViewHostImpl::SetVisibility(
    blink::mojom::PageVisibilityState visibility) {
  page_lifecycle_state_manager_->SetWebContentsVisibility(visibility);
}

void RenderViewHostImpl::SetIsFrozen(bool frozen) {
  page_lifecycle_state_manager_->SetIsFrozen(frozen);
}

void RenderViewHostImpl::OnBackForwardCacheTimeout() {
  // TODO(yuzus): Implement a method to get a list of RenderFrameHosts
  // associated with |this|, instead of iterating through all the
  // RenderFrameHosts in bfcache.
  const auto& entries = delegate_->GetFrameTree()
                            ->controller()
                            ->GetBackForwardCache()
                            .GetEntries();
  for (auto& entry : entries) {
    for (auto* const rvh : entry->render_view_hosts) {
      if (rvh == this) {
        RenderFrameHostImpl* rfh = entry->render_frame_host.get();
        rfh->EvictFromBackForwardCacheWithReason(
            BackForwardCacheMetrics::NotRestoredReason::kTimeoutPuttingInCache);
        break;
      }
    }
  }
}

bool RenderViewHostImpl::IsRenderViewLive() {
  return GetProcess()->IsInitializedAndNotDead() &&
         GetWidget()->renderer_initialized();
}

void RenderViewHostImpl::SetBackgroundOpaque(bool opaque) {
  GetWidget()->GetAssociatedFrameWidget()->SetBackgroundOpaque(opaque);
}

bool RenderViewHostImpl::IsMainFrameActive() {
  return is_active();
}

bool RenderViewHostImpl::IsNeverComposited() {
  return GetDelegate()->IsNeverComposited();
}

blink::web_pref::WebPreferences
RenderViewHostImpl::GetWebkitPreferencesForWidget() {
  if (!delegate_)
    return blink::web_pref::WebPreferences();
  return delegate_->GetOrCreateWebPreferences();
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
  is_waiting_for_page_close_completion_ = true;

  if (IsRenderViewLive() && !SuddenTerminationAllowed()) {
    close_timeout_->Start(TimeDelta::FromMilliseconds(kUnloadTimeoutMS));

    // TODO(creis): Should this be moved to Shutdown?  It may not be called for
    // RenderViewHosts that have been swapped out.
#if !defined(OS_ANDROID)
    static_cast<HostZoomMapImpl*>(HostZoomMap::Get(GetSiteInstance()))
        ->WillCloseRenderView(GetProcess()->GetID(), GetRoutingID());
#endif

    static_cast<RenderFrameHostImpl*>(GetMainFrame())
        ->GetAssociatedLocalMainFrame()
        ->ClosePage(base::BindOnce(&RenderViewHostImpl::OnPageClosed,
                                   weak_factory_.GetWeakPtr()));
  } else {
    // This RenderViewHost doesn't have a live renderer, so just skip the close
    // event and close the page.
    ClosePageIgnoringUnloadEvents();
  }
}

void RenderViewHostImpl::ClosePageIgnoringUnloadEvents() {
  close_timeout_->Stop();
  is_waiting_for_page_close_completion_ = false;

  sudden_termination_allowed_ = true;
  delegate_->Close(this);
}

void RenderViewHostImpl::ZoomToFindInPageRect(const gfx::Rect& rect_to_zoom) {
  static_cast<RenderFrameHostImpl*>(GetMainFrame())
      ->GetAssociatedLocalMainFrame()
      ->ZoomToFindInPageRect(rect_to_zoom);
}

void RenderViewHostImpl::RenderProcessExited(
    RenderProcessHost* host,
    const ChildProcessTerminationInfo& info) {
  if (!GetWidget()->renderer_initialized())
    return;

  GetWidget()->RendererExited();
  delegate_->RenderViewTerminated(this, info.status, info.exit_code);
  // |this| might have been deleted. Do not add code here.
}

bool RenderViewHostImpl::Send(IPC::Message* msg) {
  return GetWidget()->Send(msg);
}

RenderWidgetHostImpl* RenderViewHostImpl::GetWidget() {
  return render_widget_host_.get();
}

AgentSchedulingGroupHost& RenderViewHostImpl::GetAgentSchedulingGroup() {
  return render_widget_host_->agent_scheduling_group();
}

RenderProcessHost* RenderViewHostImpl::GetProcess() {
  return GetAgentSchedulingGroup().GetProcess();
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
  static_cast<RenderFrameHostImpl*>(GetMainFrame())
      ->GetAssociatedLocalMainFrame()
      ->SetInitialFocus(reverse);
}

void RenderViewHostImpl::AnimateDoubleTapZoom(const gfx::Point& point,
                                              const gfx::Rect& rect) {
  static_cast<RenderFrameHostImpl*>(GetMainFrame())
      ->GetAssociatedLocalMainFrame()
      ->AnimateDoubleTapZoom(point, rect);
}

void RenderViewHostImpl::RenderWidgetDidFirstVisuallyNonEmptyPaint() {
  did_first_visually_non_empty_paint_ = true;
  delegate_->DidFirstVisuallyNonEmptyPaint(this);
}

bool RenderViewHostImpl::SuddenTerminationAllowed() {
  // If there is a JavaScript dialog up, don't bother sending the renderer the
  // close event because it is known unresponsive, waiting for the reply from
  // the dialog.
  return sudden_termination_allowed_ ||
         delegate_->IsJavaScriptDialogShowing() ||
         static_cast<RenderFrameHostImpl*>(GetMainFrame())
             ->BeforeUnloadTimedOut();
}

///////////////////////////////////////////////////////////////////////////////
// RenderViewHostImpl, IPC message handlers:

bool RenderViewHostImpl::OnMessageReceived(const IPC::Message& msg) {
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
    IPC_MESSAGE_HANDLER(ViewHostMsg_TakeFocus, OnTakeFocus)
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

void RenderViewHostImpl::OnDidContentsPreferredSizeChange(
    const gfx::Size& new_size) {
  delegate_->UpdatePreferredSize(new_size);
}

void RenderViewHostImpl::OnTakeFocus(bool reverse) {
  RenderViewHostDelegateView* view = delegate_->GetDelegateView();
  if (view)
    view->TakeFocus(reverse);
}

void RenderViewHostImpl::OnPageClosed() {
  ClosePageIgnoringUnloadEvents();
}

void RenderViewHostImpl::OnFocus() {
  // Note: We allow focus and blur from swapped out RenderViewHosts, even when
  // the active RenderViewHost is in a different BrowsingInstance (e.g., WebUI).
  delegate_->Activate();
}

void RenderViewHostImpl::BindPageBroadcast(
    mojo::PendingAssociatedRemote<blink::mojom::PageBroadcast> page_broadcast) {
  page_broadcast_.reset();
  page_broadcast_.Bind(std::move(page_broadcast));
}

const mojo::AssociatedRemote<blink::mojom::PageBroadcast>&
RenderViewHostImpl::GetAssociatedPageBroadcast() {
  return page_broadcast_;
}

void RenderViewHostImpl::RenderWidgetDidForwardMouseEvent(
    const blink::WebMouseEvent& mouse_event) {
  if (mouse_event.GetType() == WebInputEvent::Type::kMouseWheel &&
      GetWidget()->IsIgnoringInputEvents()) {
    delegate_->OnIgnoredUIEvent();
  }
}

bool RenderViewHostImpl::MayRenderWidgetForwardKeyboardEvent(
    const NativeWebKeyboardEvent& key_event) {
  if (GetWidget()->IsIgnoringInputEvents()) {
    if (key_event.GetType() == WebInputEvent::Type::kRawKeyDown)
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

void RenderViewHostImpl::SendWebPreferencesToRenderer() {
  if (auto& broadcast = GetAssociatedPageBroadcast())
    broadcast->UpdateWebPreferences(delegate_->GetOrCreateWebPreferences());
}

void RenderViewHostImpl::OnHardwareConfigurationChanged() {
  delegate_->RecomputeWebPreferencesSlow();
}

void RenderViewHostImpl::EnablePreferredSizeMode() {
  if (is_active()) {
    static_cast<RenderFrameHostImpl*>(GetMainFrame())
        ->GetAssociatedLocalMainFrame()
        ->EnablePreferredSizeChangedMode();
  }
}

void RenderViewHostImpl::ExecutePluginActionAtLocation(
    const gfx::Point& location,
    blink::mojom::PluginActionType plugin_action) {
  // TODO(wjmaclean): See if this needs to be done for OOPIFs as well.
  // https://crbug.com/776807
  gfx::PointF local_location_f =
      GetWidget()->GetView()->TransformRootPointToViewCoordSpace(
          gfx::PointF(location.x(), location.y()));
  gfx::Point local_location(local_location_f.x(), local_location_f.y());

  static_cast<RenderFrameHostImpl*>(GetMainFrame())
      ->GetAssociatedLocalMainFrame()
      ->PluginActionAt(local_location, plugin_action);
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

void RenderViewHostImpl::DidChangeBackgroundColor(
    RenderFrameHostImpl* rfh,
    const SkColor& background_color) {
  if (GetMainFrame() != rfh)
    return;

  main_frame_background_color_ = background_color;
  delegate_->OnBackgroundColorChanged(this);
}

void RenderViewHostImpl::SetContentsMimeType(const std::string mime_type) {
  contents_mime_type_ = mime_type;
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

void RenderViewHostImpl::SetWillEnterBackForwardCacheCallbackForTesting(
    const WillEnterBackForwardCacheCallbackForTesting& callback) {
  will_enter_back_forward_cache_callback_for_testing_ = callback;
}

}  // namespace content
