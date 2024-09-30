// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_view_host_impl.h"

#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/hash/hash.h"
#include "base/i18n/rtl.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/supports_user_data.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"
#include "base/values.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/input/timeout_monitor.h"
#include "components/viz/common/features.h"
#include "content/browser/bad_message.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "content/browser/fenced_frame/fenced_frame.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/preloading/prerender/prerender_host.h"
#include "content/browser/renderer_host/agent_scheduling_group_host.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/page_delegate.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/scoped_active_url.h"
#include "content/common/agent_scheduling_group.mojom.h"
#include "content/common/content_switches_internal.h"
#include "content/common/features.h"
#include "content/common/render_message_filter.mojom.h"
#include "content/common/renderer.mojom.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/page/browsing_context_group_info.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/page/prerender_page_param.mojom.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"
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
#include "ui/native_theme/native_theme_features.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_WIN)
#include "ui/display/win/screen_win.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/system_fonts_win.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "content/browser/host_zoom_map_impl.h"
#endif

using blink::WebInputEvent;

namespace content {
namespace {

using perfetto::protos::pbzero::ChromeTrackEvent;

// <process id, routing id>
using RenderViewHostID = std::pair<int32_t, int32_t>;
using RoutingIDViewMap =
    std::unordered_map<RenderViewHostID,
                       RenderViewHostImpl*,
                       base::IntPairHash<RenderViewHostID>>;
base::LazyInstance<RoutingIDViewMap>::Leaky g_routing_id_view_map =
    LAZY_INSTANCE_INITIALIZER;

#if BUILDFLAG(IS_WIN)
// Fetches the name and font size of a particular Windows system font.
void GetFontInfo(gfx::win::SystemFont system_font,
                 std::u16string* name,
                 int32_t* size) {
  const gfx::Font& font = gfx::win::GetSystemFont(system_font);
  *name = base::UTF8ToUTF16(font.GetFontName());
  *size = font.GetFontSize();
}
#endif  // BUILDFLAG(IS_WIN)

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
    CHECK(it != render_view_host_instances_.end(), base::NotFatalUntil::M130);
    render_view_host_instances_.erase(it);
  }

  bool HasNonBackForwardCachedInstances() const {
    return !base::ranges::all_of(render_view_host_instances_,
                                 &RenderViewHostImpl::is_in_back_forward_cache);
  }

 private:
  static const void* UserDataKey() { return &kUserDataKey; }

  static const int kUserDataKey = 0;

  std::unordered_set<raw_ptr<const RenderViewHostImpl, CtnExperimental>>
      render_view_host_instances_;
};

const int PerProcessRenderViewHostSet::kUserDataKey;

// Finds all viz::SurfaceIds within `node_range` and adds them to `out_ids`.
void CollectSurfaceIdsForEvictionForFrameTreeNodeRange(
    FrameTree::NodeRange& node_range,
    std::vector<viz::SurfaceId>& out_ids) {
  for (FrameTreeNode* node : node_range) {
    if (!node->current_frame_host()->is_local_root()) {
      continue;
    }
    RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
        node->current_frame_host()->GetView());
    if (!view) {
      continue;
    }
    viz::SurfaceId id = view->GetCurrentSurfaceId();
    if (id.is_valid()) {
      out_ids.push_back(id);
    }
    view->set_is_evicted();
  }
}

}  // namespace

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
    blink::RendererPreferences* prefs) {
#if BUILDFLAG(IS_WIN)
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
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kSystemFontFamily)) {
    prefs->system_font_family_name =
        command_line->GetSwitchValueASCII(switches::kSystemFontFamily);
  } else {
    prefs->system_font_family_name = gfx::Font().GetFontName();
  }
#elif BUILDFLAG(IS_FUCHSIA)
  // Make Blink's "focus ring" invisible. The focus ring is a hairline border
  // that's rendered around clickable targets.
  // TODO(crbug.com/40124608): Consider exposing this as a FIDL parameter.
  prefs->focus_ring_color = SK_AlphaTRANSPARENT;
#endif
#if BUILDFLAG(IS_OZONE)
  prefs->selection_clipboard_buffer_available =
      ui::Clipboard::IsSupportedClipboardBuffer(
          ui::ClipboardBuffer::kSelection);
#endif
}

// static
bool RenderViewHostImpl::HasNonBackForwardCachedInstancesForProcess(
    RenderProcessHost* process) {
  return PerProcessRenderViewHostSet::GetOrCreateForProcess(process)
      ->HasNonBackForwardCachedInstances();
}

RenderViewHostImpl::RenderViewHostImpl(
    FrameTree* frame_tree,
    SiteInstanceGroup* group,
    const StoragePartitionConfig& storage_partition_config,
    std::unique_ptr<RenderWidgetHostImpl> widget,
    RenderViewHostDelegate* delegate,
    int32_t routing_id,
    int32_t main_frame_routing_id,
    bool has_initialized_audio_host,
    scoped_refptr<BrowsingContextState> main_browsing_context_state,
    CreateRenderViewHostCase create_case)
    : render_widget_host_(std::move(widget)),
      delegate_(delegate),
      render_view_host_map_id_(frame_tree->GetRenderViewHostMapId(group)),
      site_instance_group_(group->GetWeakPtrToAllowDangling()),
      storage_partition_config_(storage_partition_config),
      routing_id_(routing_id),
      main_frame_routing_id_(main_frame_routing_id),
      frame_tree_(frame_tree),
      main_browsing_context_state_(
          main_browsing_context_state
              ? std::make_optional(main_browsing_context_state->GetSafeRef())
              : std::nullopt),
      is_speculative_(create_case == CreateRenderViewHostCase::kSpeculative) {
  base::ScopedUmaHistogramTimer histogram_timer(
      "Navigation.RenderViewHostConstructor");
  TRACE_EVENT("navigation", "RenderViewHostImpl::RenderViewHostImpl",
              ChromeTrackEvent::kRenderViewHost, *this);
  TRACE_EVENT_BEGIN("navigation", "RenderViewHost",
                    perfetto::Track::FromPointer(this),
                    "render_view_host_when_created", this);

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

  // New views may be created during RenderProcessHost::ProcessDied(), within a
  // brief window where the internal ChannelProxy is null. This ensures that the
  // ChannelProxy is re-initialized in such cases so that subsequent messages
  // make their way to the new renderer once its restarted.
  // TODO(crbug.com/40142495): Should this go via AgentSchedulingGroupHost? Is
  // it even needed after the migration?
  GetProcess()->EnableSendQueue();

  if (!is_active())
    GetWidget()->UpdatePriority();

  bool initially_hidden = frame_tree_->delegate()->IsHidden();
  page_lifecycle_state_manager_ = std::make_unique<PageLifecycleStateManager>(
      this, initially_hidden ? blink::mojom::PageVisibilityState::kHidden
                             : blink::mojom::PageVisibilityState::kVisible);

  GetWidget()->set_owner_delegate(this);
}

RenderViewHostImpl::~RenderViewHostImpl() {
  TRACE_EVENT_INSTANT("navigation", "~RenderViewHostImpl()",
                      ChromeTrackEvent::kRenderViewHost, *this);
  base::ScopedUmaHistogramTimer histogram_timer(
      "Navigation.RenderViewHostDestructor");

  PerProcessRenderViewHostSet::GetOrCreateForProcess(GetProcess())->Erase(this);

  // Destroy the RenderWidgetHost.
  GetWidget()->ShutdownAndDestroyWidget(false);

  // Detach the routing ID as the object is going away.
  GetAgentSchedulingGroup().RemoveRoute(GetRoutingID());
  g_routing_id_view_map.Get().erase(
      RenderViewHostID(GetProcess()->GetID(), GetRoutingID()));

  delegate_->RenderViewDeleted(this);
  GetProcess()->RemoveObserver(this);

  // We may have already unregistered the RenderViewHost when marking this not
  // available for reuse.
  if (registered_with_frame_tree_)
    frame_tree_->UnregisterRenderViewHost(render_view_host_map_id_, this);

  // Corresponds to the TRACE_EVENT_BEGIN in RenderViewHostImpl's constructor.
  TRACE_EVENT_END("navigation", perfetto::Track::FromPointer(this));
}

RenderViewHostDelegate* RenderViewHostImpl::GetDelegate() {
  return delegate_;
}

base::WeakPtr<RenderViewHostImpl> RenderViewHostImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void RenderViewHostImpl::DisallowReuse() {
  if (registered_with_frame_tree_) {
    frame_tree_->UnregisterRenderViewHost(render_view_host_map_id_, this);
    registered_with_frame_tree_ = false;
  }
}

bool RenderViewHostImpl::CreateRenderView(
    const std::optional<blink::FrameToken>& opener_frame_token,
    int proxy_route_id,
    bool window_was_opened_by_another_window) {
  TRACE_EVENT0("renderer_host,navigation",
               "RenderViewHostImpl::CreateRenderView");
  DCHECK(!IsRenderViewLive()) << "Creating view twice";

  // The process may (if we're sharing a process with another host that already
  // initialized it) or may not (we have our own process or the old process
  // crashed) have been initialized. Calling Init() multiple times will be
  // ignored, so this is safe.
  if (!GetAgentSchedulingGroup().Init())
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
    DCHECK(main_rfph);
  }
  FrameTreeNode* const frame_tree_node =
      main_rfh ? main_rfh->frame_tree_node() : main_rfph->frame_tree_node();

  mojom::CreateViewParamsPtr params = mojom::CreateViewParams::New();

  params->renderer_preferences = delegate_->GetRendererPrefs();
  RenderViewHostImpl::GetPlatformSpecificPrefs(&params->renderer_preferences);
  params->web_preferences = delegate_->GetOrCreateWebPreferences();
  params->color_provider_colors = delegate_->GetColorProviderColorMaps();
  params->opener_frame_token = opener_frame_token;
  params->replication_state =
      frame_tree_node->current_replication_state().Clone();
  params->devtools_main_frame_token =
      frame_tree_node->current_frame_host()->devtools_frame_token();
  DCHECK_EQ(&frame_tree_node->frame_tree(), frame_tree_);

  if (frame_tree_->is_prerendering() ||
      frame_tree_->page_delegate()->IsPageInPreviewMode()) {
    auto prerender_param = blink::mojom::PrerenderParam::New();
    if (frame_tree_->is_prerendering()) {
      auto* prerender_host =
          static_cast<PrerenderHost*>(frame_tree_->delegate());
      CHECK(prerender_host);
      prerender_param->page_metric_suffix =
          prerender_host->GetHistogramSuffix();
      prerender_param->should_warm_up_compositor =
          prerender_host->should_warm_up_compositor();
    } else {
      prerender_param->page_metric_suffix = ".Preview";
      prerender_param->should_warm_up_compositor = false;
    }
    params->prerender_param = std::move(prerender_param);
  }

  params->attribution_support = delegate_->GetAttributionSupport();

  if (main_rfh) {
    auto local_frame_params = mojom::CreateLocalMainFrameParams::New();
    local_frame_params->frame_token = main_rfh->GetFrameToken();
    local_frame_params->routing_id = main_frame_routing_id_;
    mojo::PendingAssociatedRemote<mojom::Frame> pending_frame_remote;
    local_frame_params->frame =
        pending_frame_remote.InitWithNewEndpointAndPassReceiver();
    main_rfh->SetMojomFrameRemote(std::move(pending_frame_remote));
    main_rfh->BindBrowserInterfaceBrokerReceiver(
        local_frame_params->interface_broker.InitWithNewPipeAndPassReceiver());
    main_rfh->BindAssociatedInterfaceProviderReceiver(
        local_frame_params->associated_interface_provider_remote
            .InitWithNewEndpointAndPassReceiver());

    local_frame_params->is_on_initial_empty_document =
        main_rfh->frame_tree_node()->is_on_initial_empty_document();
    // It is safe to ignore safety restrictions here, since it is necessary to
    // retrieve the document token, even if the frame is speculative, in order
    // to create the corresponding renderer-side objects.
    local_frame_params->document_token =
        main_rfh->GetDocumentTokenIgnoringSafetyRestrictions();

    // If this is a new RenderFrameHost for a frame that has already committed a
    // document, we don't have a PolicyContainerHost yet. Indeed, in that case,
    // this RenderFrameHost will not display any document until it commits a
    // navigation. The policy container for the navigated document will be sent
    // to Blink at CommitNavigation time and then stored in this RenderFrameHost
    // in DidCommitNewDocument.
    if (main_rfh->policy_container_host()) {
      local_frame_params->policy_container =
          main_rfh->policy_container_host()->CreatePolicyContainerForBlink();
    }

    local_frame_params->widget_params =
        main_rfh->GetRenderWidgetHost()
            ->BindAndGenerateCreateFrameWidgetParams();

    local_frame_params->subresource_loader_factories =
        main_rfh->CreateSubresourceLoaderFactoriesForInitialEmptyDocument();

    if (is_speculative_ &&
        frame_tree_node->current_frame_host()->IsRenderFrameLive() &&
        frame_tree_node->current_frame_host()->GetSiteInstance()->group() ==
            site_instance_group_.get()) {
      // The speculative RenderViewHost has the same SiteInstanceGroup as the
      // current RenderFrameHost. This means when the speculative
      // RenderFrameHost commits, it must do a local RenderFrame swap with the
      // previous RenderFrame. Pass down the frame token of the current
      // RenderFrameHost, so that the speculative RenderFrame can find the right
      // RenderFrame.
      local_frame_params->previous_frame_token =
          frame_tree_node->current_frame_host()->GetFrameToken();

      if (frame_tree_node->current_frame_host()->ShouldReuseCompositing(
              *main_rfh->GetSiteInstance())) {
        local_frame_params->widget_params
            ->previous_frame_token_for_compositor_reuse =
            frame_tree_node->current_frame_host()->GetFrameToken();
        main_rfh->NotifyWillCreateRenderWidgetOnCommit();
      }
    }

    params->main_frame = mojom::CreateMainFrameUnion::NewLocalParams(
        std::move(local_frame_params));
  } else {
    params->main_frame = mojom::CreateMainFrameUnion::NewRemoteParams(
        mojom::CreateRemoteMainFrameParams::New(
            main_rfph->GetFrameToken(),
            main_rfph->CreateAndBindRemoteFrameInterfaces(),
            main_rfph->CreateAndBindRemoteMainFrameInterfaces()));
  }

  params->session_storage_namespace_id =
      frame_tree_->controller()
          .GetSessionStorageNamespace(storage_partition_config_)
          ->id();
  params->hidden = frame_tree_->delegate()->IsHidden();
  params->never_composited = delegate_->IsNeverComposited();
  params->window_was_opened_by_another_window =
      window_was_opened_by_another_window;
  params->base_background_color = delegate_->GetBaseBackgroundColor();
  if (auto* parent_rfh = frame_tree_node->GetParentOrOuterDocument()) {
    url::Origin outermost_origin =
        parent_rfh->GetOutermostMainFrame()->GetLastCommittedOrigin();
    if (GetContentClient()->browser()->ShouldSendOutermostOriginToRenderer(
            outermost_origin)) {
      params->outermost_origin = outermost_origin;
    }
  }

  params->type = ViewWidgetType();
  if (params->type == mojom::ViewWidgetType::kFencedFrame) {
    params->fenced_frame_mode =
        frame_tree_->root()->GetDeprecatedFencedFrameMode();
  }

  // Send the current page's browsing context group to the renderer. It is
  // guaranteed to be consistent for the entire FrameTree, main frame and
  // subframes. For this reason we simply use the main frame's browsing context
  // group. Note that we cannot use this RenderViewHost's site_instance_group(),
  // which may not match in a popup case. For example, if A opens a
  // cross-browsing-context-group popup to B, the RenderViewHost for the opener
  // in B's process should have A's BrowsingContextGroupInfo, which is the
  // current page in the opener.
  params->browsing_context_group_info = blink::BrowsingContextGroupInfo(
      frame_tree_->GetMainFrame()->GetSiteInstance()->browsing_instance_token(),
      frame_tree_->GetMainFrame()
          ->GetSiteInstance()
          ->coop_related_group_token());

  // RenderViewHostImpl is reused after a crash, so reset any endpoint that
  // might be a leftover from a crash.
  page_broadcast_.reset();
  params->blink_page_broadcast =
      page_broadcast_.BindNewEndpointAndPassReceiver();

  // We must send access information relative to the popin opener in order for
  // the renderer to properly conduct checks.
  // See https://explainers-by-googlers.github.io/partitioned-popins/
  if (!frame_tree_->GetMainFrame()->IsNestedWithinFencedFrame() &&
      frame_tree_->GetMainFrame()->delegate()->IsPartitionedPopin()) {
    RenderFrameHostImpl* partitioned_popin_opener =
        frame_tree_->GetMainFrame()->delegate()->PartitionedPopinOpener();
    params->partitioned_popin_params =
        blink::mojom::PartitionedPopinParams::New(
            partitioned_popin_opener->ComputeTopFrameOrigin(
                partitioned_popin_opener->GetLastCommittedOrigin()),
            partitioned_popin_opener->ComputeSiteForCookies());
  }

  // The renderer process's `blink::WebView` is owned by this lifecycle of
  // the `page_broadcast_` channel.
  GetAgentSchedulingGroup().CreateView(std::move(params));

  // Set the bit saying we've made the `blink::WebView` in the renderer and
  // notify content public observers.
  RenderViewCreated(main_rfh);

  // This must be posted after the RenderViewHost is marked live, with
  // `renderer_view_created_`.
  PostRenderViewReady();
  return true;
}

void RenderViewHostImpl::SetMainFrameRoutingId(int routing_id) {
  main_frame_routing_id_ = routing_id;
  render_widget_host_->ClearVisualProperties();
  GetWidget()->UpdatePriority();
  // TODO(crbug.com/40387047): If a local main frame is no longer attached to
  // this `blink::WebView` then the RenderWidgetHostImpl owned by this class
  // should be informed that its renderer widget is no longer created. The
  // RenderViewHost will need to track its own live-ness then.
}

void RenderViewHostImpl::SetFrameTree(FrameTree& frame_tree) {
  TRACE_EVENT("navigation", "RenderViewHostImpl::SetFrameTree",
              ChromeTrackEvent::kRenderViewHost, *this);
  DCHECK(registered_with_frame_tree_);
  frame_tree_->UnregisterRenderViewHost(render_view_host_map_id_, this);
  frame_tree_ = &frame_tree;
  frame_tree_->RegisterRenderViewHost(render_view_host_map_id_, this);
  render_widget_host_->SetFrameTree(frame_tree);
}

void RenderViewHostImpl::EnterBackForwardCache() {
  if (!will_enter_back_forward_cache_callback_for_testing_.is_null())
    will_enter_back_forward_cache_callback_for_testing_.Run();

  TRACE_EVENT("navigation", "RenderViewHostImpl::EnterBackForwardCache",
              ChromeTrackEvent::kRenderViewHost, *this);
  DCHECK(registered_with_frame_tree_);
  // Only unregister the RenderViewHost if the FrameTree is the primary
  // FrameTree, inner FrameTrees hold their state when they enter back/forward
  // cache.
  if (frame_tree_->is_primary()) {
    frame_tree_->UnregisterRenderViewHost(render_view_host_map_id_, this);
    registered_with_frame_tree_ = false;
  }
  is_in_back_forward_cache_ = true;
  page_lifecycle_state_manager_->SetIsInBackForwardCache(
      is_in_back_forward_cache_, /*page_restore_params=*/nullptr);
}

void RenderViewHostImpl::PrepareToLeaveBackForwardCache(
    base::OnceClosure done_cb) {
  // We wrap `done_cb` in a default invoke because if this RenderViewHostImpl
  // disappears we still need to call `done_cb` otherwise the navigation
  // will be blocked indefinitely.
  page_lifecycle_state_manager_->SetIsLeavingBackForwardCache(
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(done_cb)));
}

void RenderViewHostImpl::LeaveBackForwardCache(
    blink::mojom::PageRestoreParamsPtr page_restore_params) {
  TRACE_EVENT("navigation", "RenderViewHostImpl::LeaveBackForwardCache",
              ChromeTrackEvent::kRenderViewHost, *this);
  // At this point, the frames |this| RenderViewHostImpl belongs to are
  // guaranteed to be committed, so it should be reused going forward.
  // `registered_with_frame_tree_` will already be true for inner frame
  // trees.
  if (!registered_with_frame_tree_) {
    registered_with_frame_tree_ = true;
    frame_tree_->RegisterRenderViewHost(render_view_host_map_id_, this);
  }
  is_in_back_forward_cache_ = false;
  page_lifecycle_state_manager_->SetIsInBackForwardCache(
      is_in_back_forward_cache_, std::move(page_restore_params));
}

void RenderViewHostImpl::ActivatePrerenderedPage(
    blink::mojom::PrerenderPageActivationParamsPtr
        prerender_page_activation_params,
    base::OnceClosure callback) {
  // TODO(crbug.com/40185437): Consider using a ScopedClosureRunner here
  // in case the renderer crashes before it can send us the callback. But we
  // can't do that until the linked bug is fixed, or else we can reach
  // DidActivateForPrerendering() outside of a Mojo message dispatch which
  // breaks the DCHECK for releasing Mojo Capability Control.
  page_broadcast_->ActivatePrerenderedPage(
      std::move(prerender_page_activation_params), std::move(callback));
}

void RenderViewHostImpl::SetFrameTreeVisibility(
    blink::mojom::PageVisibilityState visibility) {
  page_lifecycle_state_manager_->SetFrameTreeVisibility(visibility);
}

void RenderViewHostImpl::SetIsFrozen(bool frozen) {
  page_lifecycle_state_manager_->SetIsFrozen(frozen);
}

void RenderViewHostImpl::OnBackForwardCacheTimeout() {
  auto entries = frame_tree_->controller()
                     .GetBackForwardCache()
                     .GetEntriesForRenderViewHostImpl(this);
  for (auto* entry : entries) {
    entry->render_frame_host()->EvictFromBackForwardCacheWithReason(
        BackForwardCacheMetrics::NotRestoredReason::kTimeoutPuttingInCache);
  }
}

void RenderViewHostImpl::MaybeEvictFromBackForwardCache() {
  auto entries = frame_tree_->controller()
                     .GetBackForwardCache()
                     .GetEntriesForRenderViewHostImpl(this);
  for (auto* entry : entries) {
    entry->render_frame_host()->MaybeEvictFromBackForwardCache();
  }
}

void RenderViewHostImpl::EnforceBackForwardCacheSizeLimit() {
  frame_tree_->controller().GetBackForwardCache().EnforceCacheSizeLimit();
}

bool RenderViewHostImpl::DidReceiveBackForwardCacheAck() {
  return GetPageLifecycleStateManager()->DidReceiveBackForwardCacheAck();
}

bool RenderViewHostImpl::IsRenderViewLive() const {
  return GetProcess()->IsInitializedAndNotDead() && renderer_view_created_;
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

void RenderViewHostImpl::RenderViewCreated(
    RenderFrameHostImpl* local_main_frame) {
  renderer_view_created_ = true;
  if (local_main_frame) {
    // If there is a main frame in this RenderViewHost, then the renderer-side
    // main frame will be created along with the `blink::WebView`. The
    // RenderFrameHost initializes its RenderWidgetHost as well, if it exists.
    local_main_frame->RenderFrameCreated();
  }
}

RenderFrameHostImpl* RenderViewHostImpl::GetMainRenderFrameHost() {
  // Only active RenderViewHosts have a main frame RenderFrameHostImpl.
  // Inactive RenderViewHosts would have a main frame RenderFrameProxyHost
  // instead.
  if (!is_active()) {
    return nullptr;
  }

  return RenderFrameHostImpl::FromID(GetProcess()->GetID(),
                                     main_frame_routing_id_);
}

void RenderViewHostImpl::ZoomToFindInPageRect(const gfx::Rect& rect_to_zoom) {
  GetMainRenderFrameHost()->GetAssociatedLocalMainFrame()->ZoomToFindInPageRect(
      rect_to_zoom);
}

void RenderViewHostImpl::RenderProcessExited(
    RenderProcessHost* host,
    const ChildProcessTerminationInfo& info) {
  renderer_view_created_ = false;
  GetWidget()->RendererExited();
  delegate_->RenderViewTerminated(this, info.status, info.exit_code);
  // |this| might have been deleted. Do not add code here.
}

RenderWidgetHostImpl* RenderViewHostImpl::GetWidget() const {
  return render_widget_host_.get();
}

AgentSchedulingGroupHost& RenderViewHostImpl::GetAgentSchedulingGroup() const {
  return render_widget_host_->agent_scheduling_group();
}

RenderProcessHost* RenderViewHostImpl::GetProcess() const {
  return GetAgentSchedulingGroup().GetProcess();
}

int RenderViewHostImpl::GetRoutingID() const {
  return routing_id_;
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
  GetMainRenderFrameHost()->GetAssociatedLocalMainFrame()->SetInitialFocus(
      reverse);
}

void RenderViewHostImpl::AnimateDoubleTapZoom(const gfx::Point& point,
                                              const gfx::Rect& rect) {
  GetMainRenderFrameHost()->GetAssociatedLocalMainFrame()->AnimateDoubleTapZoom(
      point, rect);
}

///////////////////////////////////////////////////////////////////////////////
// RenderViewHostImpl, IPC message handlers:

bool RenderViewHostImpl::OnMessageReceived(const IPC::Message& msg) {
  return false;
}

std::string RenderViewHostImpl::ToDebugString() {
  return "RVHI:" + delegate_->GetCreatorLocation().ToString();
}

void RenderViewHostImpl::OnTakeFocus(bool reverse) {
  RenderViewHostDelegateView* view = delegate_->GetDelegateView();
  if (view)
    view->TakeFocus(reverse);
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
      GetWidget()->IsIgnoringWebInputEvents(mouse_event)) {
    delegate_->OnIgnoredUIEvent();
  }
}

bool RenderViewHostImpl::MayRenderWidgetForwardKeyboardEvent(
    const input::NativeWebKeyboardEvent& key_event) {
  if (GetWidget()->IsIgnoringWebInputEvents(key_event)) {
    if (key_event.GetType() == WebInputEvent::Type::kRawKeyDown)
      delegate_->OnIgnoredUIEvent();
    return false;
  }
  return true;
}

bool RenderViewHostImpl::ShouldContributePriorityToProcess() {
  return is_active();
}

void RenderViewHostImpl::SendWebPreferencesToRenderer() {
  if (auto& broadcast = GetAssociatedPageBroadcast()) {
    if (!will_send_web_preferences_callback_for_testing_.is_null()) {
      will_send_web_preferences_callback_for_testing_.Run();
    }
    broadcast->UpdateWebPreferences(delegate_->GetOrCreateWebPreferences());
  }
}

void RenderViewHostImpl::SendRendererPreferencesToRenderer(
    const blink::RendererPreferences& preferences) {
  if (auto& broadcast = GetAssociatedPageBroadcast()) {
    if (!will_send_renderer_preferences_callback_for_testing_.is_null())
      will_send_renderer_preferences_callback_for_testing_.Run(preferences);
    broadcast->UpdateRendererPreferences(preferences);
  }
}

void RenderViewHostImpl::EnablePreferredSizeMode() {
  if (is_active()) {
    GetMainRenderFrameHost()
        ->GetAssociatedLocalMainFrame()
        ->EnablePreferredSizeChangedMode();
  }
}

void RenderViewHostImpl::PostRenderViewReady() {
  GetProcess()->PostTaskWhenProcessIsReady(base::BindOnce(
      &RenderViewHostImpl::RenderViewReady, weak_factory_.GetWeakPtr()));
}

void RenderViewHostImpl::RenderViewReady() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  delegate_->RenderViewReady(this);
}

std::vector<viz::SurfaceId> RenderViewHostImpl::CollectSurfaceIdsForEviction() {
  std::vector<viz::SurfaceId> ids;
  if (is_active()) {
    RenderFrameHostImpl* rfh = GetMainRenderFrameHost();
    if (!rfh || !rfh->IsActive()) {
      return {};
    }

    FrameTreeNode* root = rfh->frame_tree_node();
    FrameTree& tree = root->frame_tree();

    // Inner tree nodes are used for several purposes, e.g. fenced frames,
    // <webview>, and PDF. These may have a compositor surface as well,
    // in which case we need to explore not the outer node only, but the inner
    // ones as well.
    FrameTree::NodeRange node_range =
        base::FeatureList::IsEnabled(
            features::kInnerFrameCompositorSurfaceEviction)
            ? tree.NodesIncludingInnerTreeNodes()
            : tree.SubtreeNodes(root);
    CollectSurfaceIdsForEvictionForFrameTreeNodeRange(node_range, ids);
  } else if (is_in_back_forward_cache_) {
    // `FrameTree::SubtreeAndInnerTreeNodes` starts with the children of `rfh`
    // so we need to add our current viz::SurfaceId to ensure it is evicted.
    if (render_widget_host_) {
      auto* view = render_widget_host_->GetView();
      if (view) {
        if (view->GetCurrentSurfaceId().is_valid()) {
          ids.push_back(view->GetCurrentSurfaceId());
          view->set_is_evicted();
        }
      }
    }

    auto entries = frame_tree_->controller()
                       .GetBackForwardCache()
                       .GetEntriesForRenderViewHostImpl(this);
    for (auto* entry : entries) {
      auto* rfh = entry->render_frame_host();
      if (!rfh) {
        continue;
      }
      // While `is_in_back_forward_cache_` there is no `main_frame_routing_id_`
      // so there is no `GetMainRenderFrameHost`. Furthermore the root of the
      // `FrameTree` is now associated to the foreground
      // `RenderWidgetHostView*`. Due to this `NodesIncludingInnerTreeNodes`
      // does not find the children nodes associated with the BFCache entry.
      //
      // Instead we build a `FrameTree::NodeRange` that starts with the children
      // of `rfh`. This will also be equivalent to
      // `should_descend_into_inner_trees=true`. Thus finding all the compositor
      // surfaces in the BFCache.
      FrameTree::NodeRange node_range = FrameTree::SubtreeAndInnerTreeNodes(
          rfh,
          /*include_delegate_nodes_for_inner_frame_trees=*/true);
      CollectSurfaceIdsForEvictionForFrameTreeNodeRange(node_range, ids);
    }
  }

  return ids;
}

bool RenderViewHostImpl::IsTestRenderViewHost() const {
  return false;
}

void RenderViewHostImpl::SetWillEnterBackForwardCacheCallbackForTesting(
    const WillEnterBackForwardCacheCallbackForTesting& callback) {
  will_enter_back_forward_cache_callback_for_testing_ = callback;
}

void RenderViewHostImpl::SetWillSendRendererPreferencesCallbackForTesting(
    const WillSendRendererPreferencesCallbackForTesting& callback) {
  will_send_renderer_preferences_callback_for_testing_ = callback;
}

void RenderViewHostImpl::SetWillSendWebPreferencesCallbackForTesting(
    const WillSendWebPreferencesCallbackForTesting& callback) {
  will_send_web_preferences_callback_for_testing_ = callback;
}

void RenderViewHostImpl::WriteIntoTrace(
    perfetto::TracedProto<TraceProto> proto) const {
  proto->set_rvh_map_id(render_view_host_map_id_.value());
  proto->set_routing_id(GetRoutingID());
  proto.Set(TraceProto::kProcess, GetProcess());
  proto->set_is_in_back_forward_cache(is_in_back_forward_cache_);
  proto->set_renderer_view_created(renderer_view_created_);
}

base::SafeRef<RenderViewHostImpl> RenderViewHostImpl::GetSafeRef() {
  return weak_factory_.GetSafeRef();
}

mojom::ViewWidgetType RenderViewHostImpl::ViewWidgetType() {
  if (view_widget_type_) {
    return *view_widget_type_;
  }

  bool is_guest_view = delegate_->IsGuest();
  bool is_fenced_frame = frame_tree_->is_fenced_frame();

  if (is_fenced_frame) {
    view_widget_type_ = mojom::ViewWidgetType::kFencedFrame;
  } else if (is_guest_view) {
    view_widget_type_ = mojom::ViewWidgetType::kGuestView;
  } else {
    view_widget_type_ = mojom::ViewWidgetType::kTopLevel;
  }

  return *view_widget_type_;
}

}  // namespace content
