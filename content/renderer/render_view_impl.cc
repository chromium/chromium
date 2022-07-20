// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_view_impl.h"

#include <map>
#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_piece.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "cc/trees/ukm_manager.h"
#include "content/common/agent_scheduling_group.mojom.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/window_features_converter.h"
#include "content/renderer/agent_scheduling_group.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_frame_proxy.h"
#include "third_party/blink/public/mojom/page/page.mojom.h"
#include "third_party/blink/public/platform/modules/video_capture/web_video_capture_impl_manager.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_url_request_util.h"
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
    case blink::kWebNavigationPolicyPictureInPicture:
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
                               const mojom::CreateViewParams& params)
    : routing_id_(params.view_id),
      renderer_wide_named_frame_lookup_(
          params.renderer_wide_named_frame_lookup),
      agent_scheduling_group_(agent_scheduling_group) {
  // Please put all logic in RenderViewImpl::Initialize().
}

void RenderViewImpl::Initialize(
    mojom::CreateViewParamsPtr params,
    bool was_created_by_renderer,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(RenderThread::IsMainThread());

  WebFrame* opener_frame = nullptr;
  if (params->opener_frame_token)
    opener_frame = WebFrame::FromFrameToken(params->opener_frame_token.value());

  // The newly created webview_ is owned by this instance.
  webview_ = WebView::Create(
      this, params->hidden, params->is_prerendering,
      params->type == mojom::ViewWidgetType::kPortal ? true : false,
      params->type == mojom::ViewWidgetType::kFencedFrame
          ? params->fenced_frame_mode
          : static_cast<absl::optional<blink::mojom::FencedFrameMode>>(
                absl::nullopt),
      /*compositing_enabled=*/true, params->never_composited,
      opener_frame ? opener_frame->View() : nullptr,
      std::move(params->blink_page_broadcast),
      agent_scheduling_group_.agent_group_scheduler(),
      params->session_storage_namespace_id, params->base_background_color);

  g_view_map.Get().insert(std::make_pair(GetWebView(), this));
  g_routing_id_view_map.Get().insert(std::make_pair(routing_id_, this));

  bool local_main_frame = params->main_frame->is_local_params();

  webview_->SetRendererPreferences(params->renderer_preferences);
  webview_->SetWebPreferences(params->web_preferences);

  if (local_main_frame) {
    RenderFrameImpl::CreateMainFrame(
        agent_scheduling_group_, this, opener_frame,
        /*is_for_nested_main_frame=*/params->type !=
            mojom::ViewWidgetType::kTopLevel,
        /*is_for_scalable_page=*/params->type !=
            mojom::ViewWidgetType::kFencedFrame,
        std::move(params->replication_state), params->devtools_main_frame_token,
        std::move(params->main_frame->get_local_params()));
  } else {
    RenderFrameProxy::CreateFrameProxy(
        agent_scheduling_group_, params->main_frame->get_remote_params()->token,
        params->opener_frame_token, routing_id_, absl::nullopt,
        blink::mojom::TreeScopeType::kDocument /* ignored for main frames */,
        std::move(params->replication_state), params->devtools_main_frame_token,
        std::move(params->main_frame->get_remote_params()->frame_interfaces),
        std::move(
            params->main_frame->get_remote_params()->main_frame_interfaces));
  }

  // TODO(davidben): Move this state from Blink into content.
  if (params->window_was_opened_by_another_window)
    GetWebView()->SetOpenedByDOM();

  GetContentClient()->renderer()->WebViewCreated(webview_,
                                                 was_created_by_renderer);
}

RenderViewImpl::~RenderViewImpl() {
  DCHECK(destroying_);  // Always deleted through Destroy().

  g_routing_id_view_map.Get().erase(routing_id_);

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
RenderViewImpl* RenderViewImpl::FromWebView(blink::WebView* webview) {
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

/*static*/
void RenderViewImpl::DestroyAllRenderViewImpls() {
  DCHECK(RenderThread::IsMainThread());
  while (!g_view_map.Get().empty()) {
    g_view_map.Get().begin()->second->Destroy();
  }
}

/*static*/
RenderViewImpl* RenderViewImpl::Create(
    AgentSchedulingGroup& agent_scheduling_group,
    mojom::CreateViewParamsPtr params,
    bool was_created_by_renderer,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(params->view_id != MSG_ROUTING_NONE);
  DCHECK(!params->session_storage_namespace_id.empty())
      << "Session storage namespace must be populated.";

  RenderViewImpl* render_view =
      new RenderViewImpl(agent_scheduling_group, *params);
  render_view->Initialize(std::move(params), was_created_by_renderer,
                          std::move(task_runner));
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
    const absl::optional<blink::Impression>& impression,
    const absl::optional<blink::WebPictureInPictureWindowOptions>&
        pip_options) {
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
  if (!features.noopener) {
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

  params->is_form_submission = request.IsFormSubmission();
  params->form_submission_post_data =
      blink::GetRequestBodyForWebURLRequest(request);
  params->form_submission_post_content_type = request.HttpContentType().Utf8();

  params->impression = impression;

  if (pip_options) {
    CHECK_EQ(policy, blink::kWebNavigationPolicyPictureInPicture);
    auto pip_mojom_opts = blink::mojom::PictureInPictureWindowOptions::New();
    pip_mojom_opts->initial_aspect_ratio = pip_options->initial_aspect_ratio;
    pip_mojom_opts->lock_aspect_ratio = pip_options->lock_aspect_ratio;
    params->pip_options = std::move(pip_mojom_opts);
  }

  params->download_policy.ApplyDownloadFramePolicy(
      /*is_opener_navigation=*/false, request.HasUserGesture(),
      // `openee_can_access_opener_origin` only matters for opener navigations,
      // so its value here is irrelevant.
      /*openee_can_access_opener_origin=*/true, !creator->IsAllowedToDownload(),
      creator->IsAdSubframe());

  // We preserve this information before sending the message since |params| is
  // moved on send.
  bool is_background_tab =
      params->disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB;

  mojom::CreateNewWindowStatus status;
  mojom::CreateNewWindowReplyPtr reply;
  auto* frame_host = creator_frame->GetFrameHost();
  if (!frame_host->CreateNewWindow(std::move(params), &status, &reply)) {
    // The sync IPC failed, e.g. maybe the render process is in the middle of
    // shutting down. Can't create a new window without the browser process,
    // so just bail out.
    return nullptr;
  }

  // If creation of the window was blocked (e.g. because this frame doesn't
  // have user activation), return before consuming user activation. A frame
  // that isn't allowed to open a window  shouldn't be able to consume the
  // activation for the rest of the frame tree.
  if (status == mojom::CreateNewWindowStatus::kBlocked)
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

  // Consume the transient user activation in the current renderer.
  consumed_user_gesture = creator->ConsumeTransientUserActivation(
      blink::UserActivationUpdateSource::kBrowser);

  // If we should ignore the new window (e.g. because of `noopener`), return
  // now that user activation was consumed.
  if (status == mojom::CreateNewWindowStatus::kIgnore)
    return nullptr;

  DCHECK(reply);
  DCHECK_NE(MSG_ROUTING_NONE, reply->route_id);
  DCHECK_NE(MSG_ROUTING_NONE, reply->main_frame_route_id);
  DCHECK_NE(MSG_ROUTING_NONE, reply->widget_params->routing_id);

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
  DCHECK_EQ(routing_id_, creator_frame->render_view()->routing_id_);

  view_params->window_was_opened_by_another_window = true;
  view_params->renderer_preferences = webview_->GetRendererPreferences();
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
  main_frame_params->subresource_loader_factories =
      base::WrapUnique(static_cast<blink::PendingURLLoaderFactoryBundle*>(
          creator_frame->CloneLoaderFactories()->Clone().release()));

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
      agent_scheduling_group_, std::move(view_params),
      /*was_created_by_renderer=*/true,
      creator->GetTaskRunner(blink::TaskType::kInternalDefault));

  if (reply->wait_for_debugger) {
    blink::WebFrameWidget* frame_widget = view->GetWebView()
                                              ->MainFrame()
                                              ->ToWebLocalFrame()
                                              ->LocalRoot()
                                              ->FrameWidget();
    frame_widget->WaitForDebuggerWhenShown();
  }

  return view->GetWebView();
}

// RenderView implementation ---------------------------------------------------

blink::WebView* RenderViewImpl::GetWebView() {
  return webview_;
}

}  // namespace content
