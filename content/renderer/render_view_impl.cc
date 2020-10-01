// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_view_impl.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/i18n/rtl.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/metrics/field_trial.h"
#include "base/process/kill.h"
#include "base/process/process.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "cc/paint/skia_paint_canvas.h"
#include "cc/trees/layer_tree_host.h"
#include "content/common/content_constants_internal.h"
#include "content/common/drag_messages.h"
#include "content/common/frame_messages.h"
#include "content/common/frame_replication_state.h"
#include "content/common/input_messages.h"
#include "content/common/page_messages.h"
#include "content/common/render_message_filter.mojom.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/page_state.h"
#include "content/public/common/referrer_type_converters.h"
#include "content/public/common/three_d_api_types.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/public/renderer/render_view_visitor.h"
#include "content/public/renderer/window_features_converter.h"
#include "content/renderer/agent_scheduling_group.h"
#include "content/renderer/history_serialization.h"
#include "content/renderer/internal_document_state_data.h"
#include "content/renderer/loader/request_extra_data.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_frame_proxy.h"
#include "content/renderer/render_process.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_widget_fullscreen_pepper.h"
#include "content/renderer/renderer_blink_platform_impl.h"
#include "content/renderer/v8_value_converter_impl.h"
#include "content/renderer/web_ui_extension_data.h"
#include "media/audio/audio_output_device.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"
#include "media/renderers/audio_renderer_impl.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/base/data_url.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/http/http_util.h"
#include "net/nqe/effective_connection_type.h"
#include "ppapi/buildflags/buildflags.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/dom_storage/session_storage_namespace_id.h"
#include "third_party/blink/public/common/frame/user_activation_update_source.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/modules/video_capture/web_video_capture_impl_manager.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_connection_type.h"
#include "third_party/blink/public/platform/web_effective_connection_type.h"
#include "third_party/blink/public/platform/web_http_body.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/public/platform/web_network_state_notifier.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/public_buildflags.h"
#include "third_party/blink/public/web/modules/mediastream/web_media_stream_device_observer.h"
#include "third_party/blink/public/web/web_autofill_client.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_dom_event.h"
#include "third_party/blink/public/web/web_dom_message_event.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_frame_content_dumper.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_history_item.h"
#include "third_party/blink/public/web/web_hit_test_result.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_navigation_policy.h"
#include "third_party/blink/public/web/web_page_popup.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "third_party/blink/public/web/web_range.h"
#include "third_party/blink/public/web/web_render_theme.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/public/web/web_searchable_form_data.h"
#include "third_party/blink/public/web/web_security_policy.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_window_features.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/icu/source/common/unicode/uscript.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/switches.h"
#include "ui/latency/latency_info.h"
#include "url/origin.h"
#include "url/url_constants.h"
#include "v8/include/v8.h"

#if defined(OS_ANDROID)
#include <cpu-features.h>

#include "base/android/build_info.h"
#include "content/child/child_thread_impl.h"
#include "ui/gfx/geometry/rect_f.h"

#elif defined(OS_MAC)
#include "skia/ext/skia_utils_mac.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/pepper_plugin_registry.h"
#endif

using blink::DragOperation;
using blink::WebAXObject;
using blink::WebConsoleMessage;
using blink::WebData;
using blink::WebDocument;
using blink::WebElement;
using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebFrame;
using blink::WebFrameContentDumper;
using blink::WebGestureEvent;
using blink::WebHistoryItem;
using blink::WebHitTestResult;
using blink::WebHTTPBody;
using blink::WebInputElement;
using blink::WebInputEvent;
using blink::WebLocalFrame;
using blink::WebMouseEvent;
using blink::WebNavigationPolicy;
using blink::WebNavigationType;
using blink::WebNode;
using blink::WebRect;
using blink::WebRuntimeFeatures;
using blink::WebScriptSource;
using blink::WebSearchableFormData;
using blink::WebSecurityOrigin;
using blink::WebSecurityPolicy;
using blink::WebSettings;
using blink::WebSize;
using blink::WebString;
using blink::WebTouchEvent;
using blink::WebURL;
using blink::WebURLError;
using blink::WebURLRequest;
using blink::WebURLResponse;
using blink::WebVector;
using blink::WebView;
using blink::WebWidget;
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

static RenderViewImpl* (*g_create_render_view_impl)(
    AgentSchedulingGroup&,
    CompositorDependencies*,
    const mojom::CreateViewParams&) = nullptr;

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
      agent_scheduling_group_(agent_scheduling_group),
      session_storage_namespace_id_(params.session_storage_namespace_id) {
  DCHECK(!session_storage_namespace_id_.empty())
      << "Session storage namespace must be populated.";
  // Please put all logic in RenderViewImpl::Initialize().
}

void RenderViewImpl::Initialize(
    CompositorDependencies* compositor_deps,
    mojom::CreateViewParamsPtr params,
    RenderWidget::ShowCallback show_callback,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(RenderThread::IsMainThread());

  agent_scheduling_group_.AddRoute(routing_id_, this);

#if defined(OS_ANDROID)
  bool has_show_callback = !!show_callback;
#endif

  WebFrame* opener_frame = nullptr;
  if (params->opener_frame_token)
    opener_frame = WebFrame::FromFrameToken(params->opener_frame_token.value());

  // The newly created webview_ is owned by this instance.
  webview_ = WebView::Create(
      this, params->hidden,
      params->type == mojom::ViewWidgetType::kPortal ? true : false,
      /*compositing_enabled=*/true,
      opener_frame ? opener_frame->View() : nullptr,
      std::move(params->blink_page_broadcast));

  g_view_map.Get().insert(std::make_pair(GetWebView(), this));
  g_routing_id_view_map.Get().insert(std::make_pair(GetRoutingID(), this));

  bool local_main_frame = params->main_frame_routing_id != MSG_ROUTING_NONE;

  webview_->SetWebPreferences(params->web_preferences);

  if (local_main_frame) {
    main_render_frame_ = RenderFrameImpl::CreateMainFrame(
        agent_scheduling_group_, this, compositor_deps, opener_frame, &params,
        std::move(show_callback));
  } else {
    RenderFrameProxy::CreateFrameProxy(
        agent_scheduling_group_, params->proxy_routing_id, GetRoutingID(),
        params->opener_frame_token, MSG_ROUTING_NONE,
        params->replicated_frame_state, params->main_frame_frame_token,
        params->devtools_main_frame_token);
  }

  // TODO(davidben): Move this state from Blink into content.
  if (params->window_was_created_with_opener)
    GetWebView()->SetOpenedByDOM();

  OnSetRendererPrefs(*params->renderer_preferences);

  GetContentClient()->renderer()->RenderViewCreated(this);

  nav_state_sync_timer_.SetTaskRunner(task_runner);

#if defined(OS_ANDROID)
  // TODO(sgurun): crbug.com/325351 Needed only for android webview's deprecated
  // HandleNavigation codepath.
  // Renderer-created RenderViews have a ShowCallback because they send a Show
  // request (ViewHostMsg_ShowWidget, ViewHostMsg_ShowFullscreenWidget, or
  // FrameHostMsg_ShowCreatedWindow) to the browser to attach them to the UI
  // there. Browser-created RenderViews do not send a Show request to the
  // browser, so have no such callback.
  was_created_by_renderer_ = has_show_callback;
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

  for (auto& observer : observers_)
    observer.RenderViewGone();
  for (auto& observer : observers_)
    observer.OnDestruct();
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

/*static*/
RenderView* RenderView::FromRoutingID(int routing_id) {
  return RenderViewImpl::FromRoutingID(routing_id);
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
    RenderWidget::ShowCallback show_callback,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(params->view_id != MSG_ROUTING_NONE);
  // Frame and widget routing ids come together.
  DCHECK_EQ(params->main_frame_routing_id == MSG_ROUTING_NONE,
            params->main_frame_widget_routing_id == MSG_ROUTING_NONE);
  // We have either a main frame or a proxy routing id.
  DCHECK_NE(params->main_frame_routing_id != MSG_ROUTING_NONE,
            params->proxy_routing_id != MSG_ROUTING_NONE);

  RenderViewImpl* render_view;
  if (g_create_render_view_impl) {
    render_view = g_create_render_view_impl(agent_scheduling_group,
                                            compositor_deps, *params);
  } else {
    render_view =
        new RenderViewImpl(agent_scheduling_group, compositor_deps, *params);
  }

  render_view->Initialize(compositor_deps, std::move(params),
                          std::move(show_callback), std::move(task_runner));
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

// static
void RenderViewImpl::InstallCreateHook(RenderViewImpl* (
    *create_render_view_impl)(AgentSchedulingGroup&,
                              CompositorDependencies*,
                              const mojom::CreateViewParams&)) {
  CHECK(!g_create_render_view_impl);
  g_create_render_view_impl = create_render_view_impl;
}

void RenderViewImpl::AddObserver(RenderViewObserver* observer) {
  observers_.AddObserver(observer);
}

void RenderViewImpl::RemoveObserver(RenderViewObserver* observer) {
  observer->RenderViewGone();
  observers_.RemoveObserver(observer);
}

// RenderWidgetOwnerDelegate -----------------------------------------

void RenderViewImpl::SetActiveForWidget(bool active) {
  if (GetWebView())
    GetWebView()->SetIsActive(active);
}

bool RenderViewImpl::SupportsMultipleWindowsForWidget() {
  return webview_->GetWebPreferences().supports_multiple_windows;
}

bool RenderViewImpl::ShouldAckSyntheticInputImmediately() {
  // TODO(bokan): The RequestPresentation API appears not to function in VR. As
  // a short term workaround for https://crbug.com/940063, ACK input
  // immediately rather than using RequestPresentation.
  if (webview_->GetWebPreferences().immersive_mode_enabled)
    return true;
  return false;
}

bool RenderViewImpl::AutoResizeMode() {
  return GetWebView()->AutoResizeMode();
}

void RenderViewImpl::DidCommitCompositorFrameForWidget() {
  for (auto& observer : observers_)
    observer.DidCommitCompositorFrame();

  if (GetWebView())
    GetWebView()->UpdatePreferredSize();
}

void RenderViewImpl::DidCompletePageScaleAnimationForWidget() {
  if (auto* focused_frame = GetWebView()->FocusedFrame()) {
    if (focused_frame->AutofillClient())
      focused_frame->AutofillClient()->DidCompleteFocusChangeInFrame();
  }
}

void RenderViewImpl::ResizeWebWidgetForWidget(
    const gfx::Size& widget_size,
    const gfx::Size& visible_viewport_size,
    cc::BrowserControlsParams browser_controls_params) {
  GetWebView()->ResizeWithBrowserControls(widget_size, visible_viewport_size,
                                          browser_controls_params);
}

// IPC message handlers -----------------------------------------

void RenderViewImpl::OnSetHistoryOffsetAndLength(int history_offset,
                                                 int history_length) {
  // -1 <= history_offset < history_length <= kMaxSessionHistoryEntries(50).
  DCHECK_LE(-1, history_offset);
  DCHECK_LT(history_offset, history_length);
  DCHECK_LE(history_length, kMaxSessionHistoryEntries);

  history_list_offset_ = history_offset;
  history_list_length_ = history_length;
}

///////////////////////////////////////////////////////////////////////////////

void RenderViewImpl::ShowCreatedPopupWidget(RenderWidget* popup_widget,
                                            WebNavigationPolicy policy,
                                            const gfx::Rect& initial_rect) {
  Send(new ViewHostMsg_ShowWidget(GetRoutingID(), popup_widget->routing_id(),
                                  initial_rect));
}

void RenderViewImpl::ShowCreatedFullscreenWidget(
    RenderWidget* fullscreen_widget,
    WebNavigationPolicy policy,
    const gfx::Rect& initial_rect) {
  Send(new ViewHostMsg_ShowFullscreenWidget(GetRoutingID(),
                                            fullscreen_widget->routing_id()));
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
  WebFrame* main_frame = GetWebView() ? GetWebView()->MainFrame() : nullptr;
  if (main_frame) {
    GURL active_url;
    if (main_frame->IsWebLocalFrame())
      active_url = main_frame->ToWebLocalFrame()->GetDocument().Url();
    GetContentClient()->SetActiveURL(
        active_url, main_frame->Top()->GetSecurityOrigin().ToString().Utf8());
  }

  for (auto& observer : observers_) {
    if (observer.OnMessageReceived(message))
      return true;
  }

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderViewImpl, message)
    IPC_MESSAGE_HANDLER(ViewMsg_MoveOrResizeStarted, OnMoveOrResizeStarted)

    // Page messages.
    IPC_MESSAGE_HANDLER(PageMsg_SetHistoryOffsetAndLength,
                        OnSetHistoryOffsetAndLength)
    IPC_MESSAGE_HANDLER(PageMsg_SetRendererPrefs, OnSetRendererPrefs)

    // Adding a new message? Add platform independent ones first, then put the
    // platform specific ones at the end.
  IPC_END_MESSAGE_MAP()

  return handled;
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
    const blink::FeaturePolicyFeatureState& opener_feature_state,
    const blink::SessionStorageNamespaceId& session_storage_namespace_id) {
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
  // TODO(dmurph): Don't copy session storage when features.noopener is true:
  // https://html.spec.whatwg.org/multipage/browsers.html#copy-session-storage
  // https://crbug.com/771959
  params->clone_from_session_storage_namespace_id =
      session_storage_namespace_id_;

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
  DCHECK_NE(MSG_ROUTING_NONE, reply->main_frame_widget_route_id);

  // The browser allowed creation of a new window and consumed the user
  // activation.
  bool was_consumed = creator->ConsumeTransientUserActivation(
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
  view_params->renderer_preferences = renderer_preferences_.Clone();
  view_params->web_preferences = webview_->GetWebPreferences();
  view_params->view_id = reply->route_id;
  view_params->main_frame_frame_token = reply->main_frame_frame_token;
  view_params->main_frame_routing_id = reply->main_frame_route_id;
  view_params->frame_widget_host = std::move(reply->frame_widget_host);
  view_params->frame_widget = std::move(reply->frame_widget);
  view_params->widget_host = std::move(reply->widget_host);
  view_params->widget = std::move(reply->widget),
  view_params->blink_page_broadcast = std::move(reply->page_broadcast);
  view_params->main_frame_interface_bundle =
      mojom::DocumentScopedInterfaceBundle::New(
          std::move(reply->main_frame_interface_bundle->interface_provider),
          std::move(
              reply->main_frame_interface_bundle->browser_interface_broker));
  view_params->main_frame_widget_routing_id = reply->main_frame_widget_route_id;
  view_params->session_storage_namespace_id =
      reply->cloned_session_storage_namespace_id;
  DCHECK(!view_params->session_storage_namespace_id.empty())
      << "Session storage namespace must be populated.";
  view_params->replicated_frame_state.frame_policy.sandbox_flags =
      sandbox_flags;
  view_params->replicated_frame_state.opener_feature_state =
      opener_feature_state;
  view_params->replicated_frame_state.name = frame_name_utf8;
  view_params->devtools_main_frame_token = reply->devtools_main_frame_token;
  view_params->hidden = is_background_tab;
  view_params->never_composited = never_composited;
  view_params->visual_properties = reply->visual_properties;

  // Unretained() is safe here because our calling function will also call
  // show().
  RenderWidget::ShowCallback show_callback =
      base::BindOnce(&RenderFrameImpl::ShowCreatedWindow,
                     base::Unretained(creator_frame), was_consumed);

  RenderViewImpl* view = RenderViewImpl::Create(
      agent_scheduling_group_, compositor_deps_, std::move(view_params),
      std::move(show_callback),
      creator->GetTaskRunner(blink::TaskType::kInternalDefault));

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

  // Do a synchronous IPC to obtain a routing ID.
  int32_t widget_routing_id = MSG_ROUTING_NONE;
  bool success =
      RenderFrameImpl::FromWebFrame(creator)->GetFrameHost()->CreateNewWidget(
          std::move(blink_widget_host_receiver), std::move(blink_widget),
          &widget_routing_id);
  if (!success) {
    // When the renderer is being killed the mojo message will fail.
    return nullptr;
  }

  RenderWidget::ShowCallback opener_callback = base::BindOnce(
      &RenderViewImpl::ShowCreatedPopupWidget, weak_ptr_factory_.GetWeakPtr());

  RenderWidget* opener_render_widget =
      RenderFrameImpl::FromWebFrame(creator)->GetLocalRootRenderWidget();

  RenderWidget* popup_widget =
      RenderWidget::CreateForPopup(agent_scheduling_group_, widget_routing_id,
                                   opener_render_widget->compositor_deps());

  // The returned WebPagePopup is self-referencing, so the pointer here is not
  // an owning pointer. It is de-referenced by calling Close().
  blink::WebPagePopup* popup_web_widget =
      blink::WebPagePopup::Create(popup_widget, std::move(blink_widget_host),
                                  std::move(blink_widget_receiver));

  // Adds a self-reference on the |popup_widget| so it will not be destroyed
  // when leaving scope. The WebPagePopup takes responsibility for Close()ing
  // and thus destroying the RenderWidget.
  popup_widget->InitForPopup(
      std::move(opener_callback), opener_render_widget, popup_web_widget,
      opener_render_widget->GetWebWidget()->GetOriginalScreenInfo());
  return popup_web_widget;
}

base::StringPiece RenderViewImpl::GetSessionStorageNamespaceId() {
  CHECK(!session_storage_namespace_id_.empty());
  return session_storage_namespace_id_;
}

void RenderViewImpl::PrintPage(WebLocalFrame* frame) {
  RenderFrameImpl* render_frame = RenderFrameImpl::FromWebFrame(frame);
  RenderWidget* render_widget = render_frame->GetLocalRootRenderWidget();

  render_frame->ScriptedPrint(
      render_widget->GetWebWidget()->HandlingInputEvent());
}

void RenderViewImpl::ZoomLevelChanged() {
  for (auto& observer : observers_)
    observer.OnZoomLevelChanged();
}

void RenderViewImpl::PropagatePageZoomToNewlyAttachedFrame(
    bool use_zoom_for_dsf,
    float device_scale_factor) {
  if (use_zoom_for_dsf)
    GetWebView()->SetZoomFactorForDeviceScaleFactor(device_scale_factor);
  else
    GetWebView()->SetZoomLevel(GetWebView()->ZoomLevel());
}

void RenderViewImpl::SetValidationMessageDirection(
    base::string16* wrapped_main_text,
    base::i18n::TextDirection main_text_hint,
    base::string16* wrapped_sub_text,
    base::i18n::TextDirection sub_text_hint) {
  if (main_text_hint == base::i18n::LEFT_TO_RIGHT) {
    *wrapped_main_text =
        base::i18n::GetDisplayStringInLTRDirectionality(*wrapped_main_text);
  } else if (main_text_hint == base::i18n::RIGHT_TO_LEFT &&
             !base::i18n::IsRTL()) {
    base::i18n::WrapStringWithRTLFormatting(wrapped_main_text);
  }

  if (!wrapped_sub_text->empty()) {
    if (sub_text_hint == base::i18n::RIGHT_TO_LEFT) {
      *wrapped_sub_text =
          base::i18n::GetDisplayStringInLTRDirectionality(*wrapped_sub_text);
    } else if (sub_text_hint == base::i18n::LEFT_TO_RIGHT) {
      base::i18n::WrapStringWithRTLFormatting(wrapped_sub_text);
    }
  }
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
  return renderer_preferences_.can_accept_load_drops;
}

void RenderViewImpl::FocusNext() {
  Send(new ViewHostMsg_TakeFocus(GetRoutingID(), false));
}

void RenderViewImpl::FocusPrevious() {
  Send(new ViewHostMsg_TakeFocus(GetRoutingID(), true));
}

void RenderViewImpl::DidUpdateMainFrameLayout() {
  for (auto& observer : observers_)
    observer.DidUpdateMainFrameLayout();
}

void RenderViewImpl::RegisterRendererPreferenceWatcher(
    mojo::PendingRemote<blink::mojom::RendererPreferenceWatcher> watcher) {
  renderer_preference_watchers_.Add(std::move(watcher));
}

int RenderViewImpl::HistoryBackListCount() {
  return history_list_offset_ < 0 ? 0 : history_list_offset_;
}

int RenderViewImpl::HistoryForwardListCount() {
  return history_list_length_ - HistoryBackListCount() - 1;
}

// blink::WebWidgetClient ----------------------------------------------------

bool RenderViewImpl::CanHandleGestureEvent() {
  return true;
}

// TODO(https://crbug.com/937569): Remove this in Chrome 88.
bool RenderViewImpl::AllowPopupsDuringPageUnload() {
  // The switch version is for enabling via enterprise policy. The feature
  // version is for enabling via about:flags and Finch policy.
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  return command_line.HasSwitch(switches::kAllowPopupsDuringPageUnload) ||
         base::FeatureList::IsEnabled(features::kAllowPopupsDuringPageUnload);
}

void RenderViewImpl::OnPageVisibilityChanged(PageVisibilityState visibility) {
#if defined(OS_ANDROID)
  SuspendVideoCaptureDevices(visibility != PageVisibilityState::kVisible);
#endif
  for (auto& observer : observers_)
    observer.OnPageVisibilityChanged(visibility);
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

const std::string& RenderViewImpl::GetAcceptLanguages() {
  return renderer_preferences_.accept_languages;
}

blink::WebString RenderViewImpl::AcceptLanguages() {
  return WebString::FromUTF8(renderer_preferences_.accept_languages);
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

float RenderViewImpl::GetZoomLevel() {
  return webview_->ZoomLevel();
}

const blink::web_pref::WebPreferences& RenderViewImpl::GetBlinkPreferences() {
  return webview_->GetWebPreferences();
}

void RenderViewImpl::SetBlinkPreferences(
    const blink::web_pref::WebPreferences& preferences) {
  webview_->SetWebPreferences(preferences);
}

blink::WebView* RenderViewImpl::GetWebView() {
  return webview_;
}

bool RenderViewImpl::GetContentStateImmediately() {
  return send_content_state_immediately_;
}

void RenderViewImpl::OnSetRendererPrefs(
    const blink::mojom::RendererPreferences& renderer_prefs) {
  std::string old_accept_languages = renderer_preferences_.accept_languages;

  renderer_preferences_ = renderer_prefs;

  for (auto& watcher : renderer_preference_watchers_)
    watcher->NotifyUpdate(renderer_prefs.Clone());

  UpdateFontRenderingFromRendererPrefs();
  UpdateThemePrefs();
  blink::SetCaretBlinkInterval(
      renderer_prefs.caret_blink_interval.has_value()
          ? renderer_prefs.caret_blink_interval.value()
          : base::TimeDelta::FromMilliseconds(
                blink::mojom::kDefaultCaretBlinkIntervalInMilliseconds));

#if defined(USE_AURA)
  if (renderer_prefs.use_custom_colors) {
    blink::SetFocusRingColor(renderer_prefs.focus_ring_color);
    blink::SetSelectionColors(renderer_prefs.active_selection_bg_color,
                              renderer_prefs.active_selection_fg_color,
                              renderer_prefs.inactive_selection_bg_color,
                              renderer_prefs.inactive_selection_fg_color);
    if (GetWebView() && GetWebView()->MainFrameWidget())
      GetWebView()->MainFrameWidget()->ThemeChanged();
  }
#endif

  if (features::IsFormControlsRefreshEnabled() &&
      renderer_prefs.use_custom_colors) {
    blink::SetFocusRingColor(renderer_prefs.focus_ring_color);
  }

  if (GetWebView()) {
    if (old_accept_languages != renderer_preferences_.accept_languages)
      GetWebView()->AcceptLanguagesChanged();

    GetWebView()->GetSettings()->SetCaretBrowsingEnabled(
        renderer_preferences_.caret_browsing_enabled);
  }

#if defined(USE_X11) || defined(USE_OZONE)
  GetWebView()->GetSettings()->SetSelectionClipboardBufferAvailable(
      renderer_preferences_.selection_clipboard_buffer_available);
#endif  // defined(USE_X11) || defined(USE_OZONE)
}

void RenderViewImpl::OnMoveOrResizeStarted() {
  if (GetWebView())
    GetWebView()->CancelPagePopup();
}

void RenderViewImpl::SetPageFrozen(bool frozen) {
  if (GetWebView())
    GetWebView()->SetPageFrozen(frozen);
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

unsigned RenderViewImpl::GetLocalSessionHistoryLengthForTesting() const {
  return history_list_length_;
}

}  // namespace content
