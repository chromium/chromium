// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_view_impl.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_piece.h"
#include "cc/trees/ukm_manager.h"
#include "content/child/webthemeengine_impl_default.h"
#include "content/common/agent_scheduling_group.mojom.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/render_view_visitor.h"
#include "content/public/renderer/window_features_converter.h"
#include "content/renderer/agent_scheduling_group.h"
#include "content/renderer/render_frame_proxy.h"
#include "content/renderer/render_thread_impl.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/impression_conversions.h"
#include "third_party/blink/public/platform/modules/video_capture/web_video_capture_impl_manager.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/web/modules/mediastream/web_media_stream_device_observer.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_page_popup.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_window_features.h"
#include "ui/base/ui_base_features.h"

using blink::WebFrame;
using blink::WebLocalFrame;
using blink::WebNavigationPolicy;
using blink::WebString;
using blink::WebURLRequest;
using blink::WebView;
using blink::WebWindowFeatures;

namespace content {

//-----------------------------------------------------------------------------

typedef std::map<blink::WebView*, RenderViewImpl*> ViewMap;
static base::LazyInstance<ViewMap>::Leaky g_view_map =
    LAZY_INSTANCE_INITIALIZER;
typedef std::map<int32_t, RenderViewImpl*> RoutingIDViewMap;
static base::LazyInstance<RoutingIDViewMap>::Leaky g_routing_id_view_map =
    LAZY_INSTANCE_INITIALIZER;

// Time, in seconds, we delay before sending content state changes (such as form
// state and scroll position) to the browser. We delay sending changes to avoid
// spamming the browser.
// To avoid having tab/session restore require sending a message to get the
// current content state during tab closing we use a shorter timeout for the
// foreground renderer. This means there is a small window of time from which
// content state is modified and not sent to session restore, but this is
// better than having to wake up all renderers during shutdown.
const int kDelaySecondsForContentStateSyncHidden = 5;
const int kDelaySecondsForContentStateSync = 1;

// static
WindowOpenDisposition RenderViewImpl::NavigationPolicyToDisposition(
    WebNavigationPolicy policy) {
  switch (policy) {
    case blink::kWebNavigationPolicyDownload:
      return WindowOpenDisposition::SAVE_TO_DISK;
    case blink::kWebNavigationPolicyCurrentTab:
      return WindowOpenDisposition::CURRENT_TAB;
    case blink::kWebNavigationPolicyNewBackgroundTab:
      return WindowOpenDisposition::NEW_BACKGROUND_TAB;
    case blink::kWebNavigationPolicyNewForegroundTab:
      return WindowOpenDisposition::NEW_FOREGROUND_TAB;
    case blink::kWebNavigationPolicyNewWindow:
      return WindowOpenDisposition::NEW_WINDOW;
    case blink::kWebNavigationPolicyNewPopup:
      return WindowOpenDisposition::NEW_POPUP;
    default:
      NOTREACHED() << "Unexpected WebNavigationPolicy";
      return WindowOpenDisposition::IGNORE_ACTION;
  }
}

///////////////////////////////////////////////////////////////////////////////

namespace {

content::mojom::WindowContainerType WindowFeaturesToContainerType(
    const blink::WebWindowFeatures& window_features) {
  if (window_features.background) {
    if (window_features.persistent)
      return content::mojom::WindowContainerType::PERSISTENT;
    else
      return content::mojom::WindowContainerType::BACKGROUND;
  } else {
    return content::mojom::WindowContainerType::NORMAL;
  }
}

}  // namespace

RenderViewImpl::RenderViewImpl(AgentSchedulingGroup& agent_scheduling_group,
                               CompositorDependencies* compositor_deps,
                               const mojom::CreateViewParams& params)
    : routing_id_(params.view_id),
      renderer_wide_named_frame_lookup_(
          params.renderer_wide_named_frame_lookup),
      widgets_never_composited_(params.never_composited),
      compositor_deps_(compositor_deps),
      agent_scheduling_group_(agent_scheduling_group) {
  // Please put all logic in RenderViewImpl::Initialize().
}

void RenderViewImpl::Initialize(
    CompositorDependencies* compositor_deps,
    mojom::CreateViewParamsPtr params,
    bool was_created_by_renderer,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(RenderThread::IsMainThread());

  agent_scheduling_group_.AddRoute(routing_id_, this);

  WebFrame* opener_frame = nullptr;
  if (params->opener_frame_token)
    opener_frame = WebFrame::FromFrameToken(params->opener_frame_token.value());

  // The newly created webview_ is owned by this instance.
  webview_ = WebView::Create(
      this, params->hidden,
      params->type == mojom::ViewWidgetType::kPortal ? true : false,
      /*compositing_enabled=*/true,
      opener_frame ? opener_frame->View() : nullptr,
      std::move(params->blink_page_broadcast),
      agent_scheduling_group_.agent_group_scheduler(),
      params->session_storage_namespace_id);

  g_view_map.Get().insert(std::make_pair(GetWebView(), this));
  g_routing_id_view_map.Get().insert(std::make_pair(GetRoutingID(), this));

  bool local_main_frame = params->main_frame->is_local_params();

  webview_->SetWebPreferences(params->web_preferences);

  if (local_main_frame) {
    main_render_frame_ = RenderFrameImpl::CreateMainFrame(
        agent_scheduling_group_, this, compositor_deps, opener_frame,
        params->type != mojom::ViewWidgetType::kTopLevel,
        std::move(params->replication_state), params->devtools_main_frame_token,
        std::move(params->main_frame->get_local_params()));
  } else {
    RenderFrameProxy::CreateFrameProxy(
        agent_scheduling_group_, params->main_frame->get_remote_params()->token,
        params->main_frame->get_remote_params()->routing_id,
        params->opener_frame_token, GetRoutingID(), MSG_ROUTING_NONE,
        std::move(params->replication_state),
        params->devtools_main_frame_token);
  }

  // TODO(davidben): Move this state from Blink into content.
  if (params->window_was_created_with_opener)
    GetWebView()->SetOpenedByDOM();

  webview_->SetRendererPreferences(params->renderer_preferences);

  GetContentClient()->renderer()->RenderViewCreated(this);

  nav_state_sync_timer_.SetTaskRunner(task_runner);

#if defined(OS_ANDROID)
  // TODO(sgurun): crbug.com/325351 Needed only for android webview's deprecated
  // HandleNavigation codepath.
  was_created_by_renderer_ = was_created_by_renderer;
#endif
}

RenderViewImpl::~RenderViewImpl() {
  DCHECK(destroying_);  // Always deleted through Destroy().

  g_routing_id_view_map.Get().erase(routing_id_);
  agent_scheduling_group_.RemoveRoute(routing_id_);

#ifndef NDEBUG
  // Make sure we are no longer referenced by the ViewMap or RoutingIDViewMap.
  ViewMap* views = g_view_map.Pointer();
  for (ViewMap::iterator it = views->begin(); it != views->end(); ++it)
    DCHECK_NE(this, it->second) << "Failed to call Close?";
  RoutingIDViewMap* routing_id_views = g_routing_id_view_map.Pointer();
  for (RoutingIDViewMap::iterator it = routing_id_views->begin();
       it != routing_id_views->end(); ++it)
    DCHECK_NE(this, it->second) << "Failed to call Close?";
#endif
}

/*static*/
RenderView* RenderView::FromWebView(blink::WebView* webview) {
  DCHECK(RenderThread::IsMainThread());
  ViewMap* views = g_view_map.Pointer();
  auto it = views->find(webview);
  return it == views->end() ? NULL : it->second;
}

/*static*/
RenderViewImpl* RenderViewImpl::FromRoutingID(int32_t routing_id) {
  DCHECK(RenderThread::IsMainThread());
  RoutingIDViewMap* views = g_routing_id_view_map.Pointer();
  auto it = views->find(routing_id);
  return it == views->end() ? NULL : it->second;
}

/* static */
size_t RenderView::GetRenderViewCount() {
  return g_view_map.Get().size();
}

/*static*/
void RenderView::ForEach(RenderViewVisitor* visitor) {
  DCHECK(RenderThread::IsMainThread());
  ViewMap* views = g_view_map.Pointer();
  for (auto it = views->begin(); it != views->end(); ++it) {
    if (!visitor->Visit(it->second))
      return;
  }
}

/*static*/
RenderViewImpl* RenderViewImpl::Create(
    AgentSchedulingGroup& agent_scheduling_group,
    CompositorDependencies* compositor_deps,
    mojom::CreateViewParamsPtr params,
    bool was_created_by_renderer,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(params->view_id != MSG_ROUTING_NONE);
  DCHECK(!params->session_storage_namespace_id.empty())
      << "Session storage namespace must be populated.";

  RenderViewImpl* render_view =
      new RenderViewImpl(agent_scheduling_group, compositor_deps, *params);
  render_view->Initialize(compositor_deps, std::move(params),
                          was_created_by_renderer, std::move(task_runner));
  return render_view;
}

void RenderViewImpl::Destroy() {
  destroying_ = true;

  webview_->Close();
  // The webview_ is already destroyed by the time we get here, remove any
  // references to it.
  g_view_map.Get().erase(webview_);
  webview_ = nullptr;

  delete this;
}

void RenderViewImpl::SendFrameStateUpdates() {
  // Tell each frame with pending state to send its UpdateState message.
  for (int render_frame_routing_id : frames_with_pending_state_) {
    RenderFrameImpl* frame =
        RenderFrameImpl::FromRoutingID(render_frame_routing_id);
    if (frame)
      frame->SendUpdateState();
  }
  frames_with_pending_state_.clear();
}

// IPC::Listener -------------------------------------------------------------

bool RenderViewImpl::OnMessageReceived(const IPC::Message& message) {
  return false;
}

// blink::WebViewClient ------------------------------------------------------

// TODO(csharrison): Migrate this method to WebLocalFrameClient /
// RenderFrameImpl, as it is now serviced by a mojo interface scoped to the
// opener frame.
WebView* RenderViewImpl::CreateView(
    WebLocalFrame* creator,
    const WebURLRequest& request,
    const WebWindowFeatures& features,
    const WebString& frame_name,
    WebNavigationPolicy policy,
    network::mojom::WebSandboxFlags sandbox_flags,
    const blink::SessionStorageNamespaceId& session_storage_namespace_id,
    bool& consumed_user_gesture,
    const base::Optional<blink::WebImpression>& impression) {
  consumed_user_gesture = false;
  RenderFrameImpl* creator_frame = RenderFrameImpl::FromWebFrame(creator);
  mojom::CreateNewWindowParamsPtr params = mojom::CreateNewWindowParams::New();

  // The user activation check is done at the browser process through
  // |frame_host->CreateNewWindow()| call below.  But the extensions case
  // handled through the following |if| is an exception.
  params->allow_popup = false;
  if (GetContentClient()->renderer()->AllowPopup())
    params->allow_popup = true;

  params->window_container_type = WindowFeaturesToContainerType(features);

  params->session_storage_namespace_id = session_storage_namespace_id;
  if (!features.noopener ||
      base::FeatureList::IsEnabled(
          blink::features::kCloneSessionStorageForNoOpener)) {
    params->clone_from_session_storage_namespace_id =
        GetWebView()->GetSessionStorageNamespaceId();
  }

  const std::string& frame_name_utf8 = frame_name.Utf8(
      WebString::UTF8ConversionMode::kStrictReplacingErrorsWithFFFD);
  params->frame_name = frame_name_utf8;
  params->opener_suppressed = features.noopener;
  params->disposition = NavigationPolicyToDisposition(policy);
  if (!request.IsNull()) {
    params->target_url = request.Url();
    params->referrer = blink::mojom::Referrer::New(
        blink::WebStringToGURL(request.ReferrerString()),
        request.GetReferrerPolicy());
  }
  params->features = ConvertWebWindowFeaturesToMojoWindowFeatures(features);

  if (impression) {
    params->impression = blink::ConvertWebImpressionToImpression(*impression);
  }

  // We preserve this information before sending the message since |params| is
  // moved on send.
  bool is_background_tab =
      params->disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB;

  mojom::CreateNewWindowStatus status;
  mojom::CreateNewWindowReplyPtr reply;
  auto* frame_host = creator_frame->GetFrameHost();
  bool err = !frame_host->CreateNewWindow(std::move(params), &status, &reply);
  if (err || status == mojom::CreateNewWindowStatus::kIgnore)
    return nullptr;

  // For Android WebView, we support a pop-up like behavior for window.open()
  // even if the embedding app doesn't support multiple windows. In this case,
  // window.open() will return "window" and navigate it to whatever URL was
  // passed. We also don't need to consume user gestures to protect against
  // multiple windows being opened, because, well, the app doesn't support
  // multiple windows.
  // TODO(dcheng): It's awkward that this is plumbed into Blink but not really
  // used much in Blink, except to enable web testing... perhaps this should
  // be checked directly in the browser side.
  if (status == mojom::CreateNewWindowStatus::kReuse)
    return GetWebView();

  DCHECK(reply);
  DCHECK_NE(MSG_ROUTING_NONE, reply->route_id);
  DCHECK_NE(MSG_ROUTING_NONE, reply->main_frame_route_id);
  DCHECK_NE(MSG_ROUTING_NONE, reply->widget_params->routing_id);

  // The browser allowed creation of a new window and consumed the user
  // activation.
  consumed_user_gesture = creator->ConsumeTransientUserActivation(
      blink::UserActivationUpdateSource::kBrowser);

  // While this view may be a background extension page, it can spawn a visible
  // render view. So we just assume that the new one is not another background
  // page instead of passing on our own value.
  // TODO(vangelis): Can we tell if the new view will be a background page?
  bool never_composited = false;

  // The initial hidden state for the RenderViewImpl here has to match what the
  // browser will eventually decide for the given disposition. Since we have to
  // return from this call synchronously, we just have to make our best guess
  // and rely on the browser sending a WasHidden / WasShown message if it
  // disagrees.
  mojom::CreateViewParamsPtr view_params = mojom::CreateViewParams::New();

  view_params->opener_frame_token = creator->GetFrameToken();
  DCHECK_EQ(GetRoutingID(), creator_frame->render_view()->GetRoutingID());

  view_params->window_was_created_with_opener = true;
  view_params->renderer_preferences = GetRendererPreferences();
  view_params->web_preferences = webview_->GetWebPreferences();
  view_params->view_id = reply->route_id;

  view_params->replication_state = blink::mojom::FrameReplicationState::New();
  view_params->replication_state->frame_policy.sandbox_flags = sandbox_flags;
  view_params->replication_state->name = frame_name_utf8;
  view_params->devtools_main_frame_token = reply->devtools_main_frame_token;

  auto main_frame_params = mojom::CreateLocalMainFrameParams::New();
  main_frame_params->token = reply->main_frame_token;
  main_frame_params->routing_id = reply->main_frame_route_id;
  main_frame_params->frame = std::move(reply->frame);
  main_frame_params->interface_broker =
      std::move(reply->main_frame_interface_broker);
  main_frame_params->policy_container = std::move(reply->policy_container);
  main_frame_params->widget_params = std::move(reply->widget_params);
  view_params->main_frame =
      mojom::CreateMainFrameUnion::NewLocalParams(std::move(main_frame_params));
  view_params->blink_page_broadcast = std::move(reply->page_broadcast);
  view_params->session_storage_namespace_id =
      reply->cloned_session_storage_namespace_id;
  DCHECK(!view_params->session_storage_namespace_id.empty())
      << "Session storage namespace must be populated.";
  view_params->hidden = is_background_tab;
  view_params->never_composited = never_composited;

  RenderViewImpl* view = RenderViewImpl::Create(
      agent_scheduling_group_, compositor_deps_, std::move(view_params),
      /*was_created_by_renderer=*/true,
      creator->GetTaskRunner(blink::TaskType::kInternalDefault));
  view->GetMainRenderFrame()->InheritLoaderFactoriesFrom(*creator_frame);

  if (reply->wait_for_debugger) {
    blink::WebFrameWidget* frame_widget =
        view->GetMainRenderFrame()->GetLocalRootWebFrameWidget();
    frame_widget->WaitForDebuggerWhenShown();
  }

  return view->GetWebView();
}

blink::WebPagePopup* RenderViewImpl::CreatePopup(
    blink::WebLocalFrame* creator) {
  mojo::PendingAssociatedRemote<blink::mojom::Widget> blink_widget;
  mojo::PendingAssociatedReceiver<blink::mojom::Widget> blink_widget_receiver =
      blink_widget.InitWithNewEndpointAndPassReceiver();

  mojo::PendingAssociatedRemote<blink::mojom::WidgetHost> blink_widget_host;
  mojo::PendingAssociatedReceiver<blink::mojom::WidgetHost>
      blink_widget_host_receiver =
          blink_widget_host.InitWithNewEndpointAndPassReceiver();

  mojo::PendingAssociatedRemote<blink::mojom::PopupWidgetHost>
      blink_popup_widget_host;
  mojo::PendingAssociatedReceiver<blink::mojom::PopupWidgetHost>
      blink_popup_widget_host_receiver =
          blink_popup_widget_host.InitWithNewEndpointAndPassReceiver();

  RenderFrameImpl::FromWebFrame(creator)->GetFrameHost()->CreateNewPopupWidget(
      std::move(blink_popup_widget_host_receiver),
      std::move(blink_widget_host_receiver), std::move(blink_widget));
  blink::WebFrameWidget* opener_widget =
      RenderFrameImpl::FromWebFrame(creator)->GetLocalRootWebFrameWidget();

  // The returned WebPagePopup is self-referencing, so the pointer here is not
  // an owning pointer. It is de-referenced by calling Close().
  blink::WebPagePopup* popup = blink::WebPagePopup::Create(
      std::move(blink_popup_widget_host), std::move(blink_widget_host),
      std::move(blink_widget_receiver),
      agent_scheduling_group_.agent_group_scheduler().DefaultTaskRunner());
  popup->InitializeCompositing(agent_scheduling_group_.agent_group_scheduler(),
                               compositor_deps_->GetTaskGraphRunner(),
                               opener_widget->GetOriginalScreenInfos(),
                               compositor_deps_->CreateUkmRecorderFactory(),
                               /*settings=*/nullptr,
                               compositor_deps_->GetMainThreadPipeline(),
                               compositor_deps_->GetCompositorThreadPipeline());
  return popup;
}

void RenderViewImpl::PrintPage(WebLocalFrame* frame) {
  RenderFrameImpl* render_frame = RenderFrameImpl::FromWebFrame(frame);
  blink::WebFrameWidget* frame_widget =
      render_frame->GetLocalRootWebFrameWidget();

  render_frame->ScriptedPrint(frame_widget->HandlingInputEvent());
}

void RenderViewImpl::PropagatePageZoomToNewlyAttachedFrame(
    bool use_zoom_for_dsf,
    float device_scale_factor) {
  if (use_zoom_for_dsf)
    GetWebView()->SetZoomFactorForDeviceScaleFactor(device_scale_factor);
  else
    GetWebView()->SetZoomLevel(GetWebView()->ZoomLevel());
}

void RenderViewImpl::StartNavStateSyncTimerIfNecessary(RenderFrameImpl* frame) {
  // Keep track of which frames have pending updates.
  frames_with_pending_state_.insert(frame->GetRoutingID());

  int delay;
  if (send_content_state_immediately_)
    delay = 0;
  else if (GetWebView()->GetVisibilityState() != PageVisibilityState::kVisible)
    delay = kDelaySecondsForContentStateSyncHidden;
  else
    delay = kDelaySecondsForContentStateSync;

  if (nav_state_sync_timer_.IsRunning()) {
    // The timer is already running. If the delay of the timer maches the amount
    // we want to delay by, then return. Otherwise stop the timer so that it
    // gets started with the right delay.
    if (nav_state_sync_timer_.GetCurrentDelay().InSeconds() == delay)
      return;
    nav_state_sync_timer_.Stop();
  }

  // Tell each frame with pending state to inform the browser.
  nav_state_sync_timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(delay),
                              this, &RenderViewImpl::SendFrameStateUpdates);
}

bool RenderViewImpl::AcceptsLoadDrops() {
  return GetRendererPreferences().can_accept_load_drops;
}

void RenderViewImpl::RegisterRendererPreferenceWatcher(
    mojo::PendingRemote<blink::mojom::RendererPreferenceWatcher> watcher) {
  GetWebView()->RegisterRendererPreferenceWatcher(std::move(watcher));
}

const blink::RendererPreferences& RenderViewImpl::GetRendererPreferences()
    const {
  return webview_->GetRendererPreferences();
}

void RenderViewImpl::OnPageVisibilityChanged(PageVisibilityState visibility) {
#if defined(OS_ANDROID)
  SuspendVideoCaptureDevices(visibility != PageVisibilityState::kVisible);
#endif
}

void RenderViewImpl::OnPageFrozenChanged(bool frozen) {
  if (frozen) {
    // Make sure browser has the latest info before the page is frozen. If the
    // page goes into the back-forward cache it could be evicted and some of the
    // updates lost.
    nav_state_sync_timer_.Stop();
    SendFrameStateUpdates();
  }
}

bool RenderViewImpl::CanUpdateLayout() {
  return true;
}

// RenderView implementation ---------------------------------------------------

bool RenderViewImpl::Send(IPC::Message* message) {
  // No messages sent through RenderView come without a routing id, yay. Let's
  // keep that up.
  CHECK_NE(message->routing_id(), MSG_ROUTING_NONE);

  return agent_scheduling_group_.Send(message);
}

RenderFrameImpl* RenderViewImpl::GetMainRenderFrame() {
  return main_render_frame_;
}

int RenderViewImpl::GetRoutingID() {
  return routing_id_;
}

blink::WebView* RenderViewImpl::GetWebView() {
  return webview_;
}

void RenderViewImpl::DidUpdateRendererPreferences() {
#if defined(OS_WIN)
  // Update Theme preferences on Windows.
  const blink::RendererPreferences& renderer_prefs = GetRendererPreferences();
  WebThemeEngineDefault::cacheScrollBarMetrics(
      renderer_prefs.vertical_scroll_bar_width_in_dips,
      renderer_prefs.horizontal_scroll_bar_height_in_dips,
      renderer_prefs.arrow_bitmap_height_vertical_scroll_bar_in_dips,
      renderer_prefs.arrow_bitmap_width_horizontal_scroll_bar_in_dips);
#endif
}

#if defined(OS_ANDROID)
void RenderViewImpl::SuspendVideoCaptureDevices(bool suspend) {
  if (!main_render_frame_)
    return;

  blink::WebMediaStreamDeviceObserver* media_stream_device_observer =
      main_render_frame_->MediaStreamDeviceObserver();
  if (!media_stream_device_observer)
    return;

  blink::MediaStreamDevices video_devices =
      media_stream_device_observer->GetNonScreenCaptureDevices();
  RenderThreadImpl::current()->video_capture_impl_manager()->SuspendDevices(
      video_devices, suspend);
}
#endif  // defined(OS_ANDROID)

}  // namespace content
