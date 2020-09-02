// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_widget.h"

#include <cmath>
#include <limits>
#include <utility>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/base/features.h"
#include "cc/base/switches.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/ukm_manager.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/switches.h"
#include "content/common/content_switches_internal.h"
#include "content/common/drag_event_source_info.h"
#include "content/common/drag_messages.h"
#include "content/common/render_message_filter.mojom.h"
#include "content/common/widget_messages.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/drop_data.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/common/untrustworthy_context_menu_params.h"
#include "content/public/common/use_zoom_for_dsf_policy.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/drop_data_builder.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_frame_proxy.h"
#include "content/renderer/render_process.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/renderer_blink_platform_impl.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/config/gpu_finch_features.h"
#include "ipc/ipc_message_start.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ppapi/buildflags/buildflags.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/page/web_drag_operation.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/web_render_widget_scheduling_state.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/platform/web_drag_data.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_autofill_client.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/public/web/web_page_popup.h"
#include "third_party/blink/public/web/web_popup_menu_info.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_widget.h"
#include "third_party/skia/include/core/SkShader.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_switches.h"
#include "ui/native_theme/native_theme_features.h"
#include "ui/native_theme/overlay_scrollbar_constants_aura.h"
#include "ui/surface/transport_dib.h"

#if defined(OS_ANDROID)
#include <android/keycodes.h>
#include "base/time/time.h"
#include "components/viz/common/viz_utils.h"
#endif

#if defined(OS_POSIX)
#include "third_party/skia/include/core/SkMallocPixelRef.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#endif  // defined(OS_POSIX)

using blink::WebDragData;
using blink::WebDragOperation;
using blink::WebDragOperationsMask;
using blink::WebFrameWidget;
using blink::WebLocalFrame;
using blink::WebNavigationPolicy;
using blink::WebRect;
using blink::WebString;

namespace content {

namespace {

RenderWidget::CreateRenderWidgetFunction g_create_render_widget_for_frame =
    nullptr;

static const char* kOOPIF = "OOPIF";
static const char* kRenderer = "Renderer";

class WebWidgetLockTarget : public content::MouseLockDispatcher::LockTarget {
 public:
  explicit WebWidgetLockTarget(RenderWidget* render_widget)
      : render_widget_(render_widget) {}

  void OnLockMouseACK(bool succeeded) override {
    if (succeeded)
      render_widget_->GetWebWidget()->DidAcquirePointerLock();
    else
      render_widget_->GetWebWidget()->DidNotAcquirePointerLock();
  }

  void OnMouseLockLost() override {
    render_widget_->GetWebWidget()->DidLosePointerLock();
  }

  bool HandleMouseLockedInputEvent(const blink::WebMouseEvent& event) override {
    // The WebWidget handles mouse lock in Blink's handleInputEvent().
    return false;
  }

 private:
  // The RenderWidget owns this instance and is guaranteed to outlive it.
  RenderWidget* render_widget_;
};

WebDragData DropMetaDataToWebDragData(
    const std::vector<DropData::Metadata>& drop_meta_data) {
  std::vector<WebDragData::Item> item_list;
  for (const auto& meta_data_item : drop_meta_data) {
    if (meta_data_item.kind == DropData::Kind::STRING) {
      WebDragData::Item item;
      item.storage_type = WebDragData::Item::kStorageTypeString;
      item.string_type = WebString::FromUTF16(meta_data_item.mime_type);
      // Have to pass a dummy URL here instead of an empty URL because the
      // DropData received by browser_plugins goes through a round trip:
      // DropData::MetaData --> WebDragData-->DropData. In the end, DropData
      // will contain an empty URL (which means no URL is dragged) if the URL in
      // WebDragData is empty.
      if (base::EqualsASCII(meta_data_item.mime_type, ui::kMimeTypeURIList)) {
        item.string_data = WebString::FromUTF8("about:dragdrop-placeholder");
      }
      item_list.push_back(item);
      continue;
    }

    // TODO(hush): crbug.com/584789. Blink needs to support creating a file with
    // just the mimetype. This is needed to drag files to WebView on Android
    // platform.
    if ((meta_data_item.kind == DropData::Kind::FILENAME) &&
        !meta_data_item.filename.empty()) {
      WebDragData::Item item;
      item.storage_type = WebDragData::Item::kStorageTypeFilename;
      item.filename_data = blink::FilePathToWebString(meta_data_item.filename);
      item_list.push_back(item);
      continue;
    }

    if (meta_data_item.kind == DropData::Kind::FILESYSTEMFILE) {
      WebDragData::Item item;
      item.storage_type = WebDragData::Item::kStorageTypeFileSystemFile;
      item.file_system_url = meta_data_item.file_system_url;
      item_list.push_back(item);
      continue;
    }
  }

  WebDragData result;
  result.SetItems(item_list);
  return result;
}

#if BUILDFLAG(ENABLE_PLUGINS)
blink::WebTextInputType ConvertTextInputType(ui::TextInputType type) {
  // Check the type is in the range representable by ui::TextInputType.
  DCHECK_LE(type, static_cast<int>(ui::TEXT_INPUT_TYPE_MAX))
      << "blink::WebTextInputType and ui::TextInputType not synchronized";
  return static_cast<blink::WebTextInputType>(type);
}
#endif

viz::FrameSinkId GetRemoteFrameSinkId(const blink::WebHitTestResult& result) {
  const blink::WebNode& node = result.GetNode();
  DCHECK(!node.IsNull());
  blink::WebFrame* result_frame = blink::WebFrame::FromFrameOwnerElement(node);
  if (!result_frame || !result_frame->IsWebRemoteFrame())
    return viz::FrameSinkId();

  blink::WebRemoteFrame* remote_frame = result_frame->ToWebRemoteFrame();
  if (remote_frame->IsIgnoredForHitTest() || !result.ContentBoxContainsPoint())
    return viz::FrameSinkId();

  return RenderFrameProxy::FromWebFrame(remote_frame)->frame_sink_id();
}

}  // namespace

// RenderWidget ---------------------------------------------------------------

// static
void RenderWidget::InstallCreateForFrameHook(
    CreateRenderWidgetFunction create_widget) {
  g_create_render_widget_for_frame = create_widget;
}

std::unique_ptr<RenderWidget> RenderWidget::CreateForFrame(
    int32_t widget_routing_id,
    CompositorDependencies* compositor_deps,
    bool never_composited) {
  if (g_create_render_widget_for_frame) {
    return g_create_render_widget_for_frame(widget_routing_id, compositor_deps,
                                            /*hidden=*/true, never_composited);
  }

  return std::make_unique<RenderWidget>(widget_routing_id, compositor_deps,
                                        /*hidden=*/true, never_composited);
}

RenderWidget* RenderWidget::CreateForPopup(
    int32_t widget_routing_id,
    CompositorDependencies* compositor_deps,
    bool hidden,
    bool never_composited) {
  return new RenderWidget(widget_routing_id, compositor_deps, hidden,
                          never_composited);
}

RenderWidget::RenderWidget(int32_t widget_routing_id,
                           CompositorDependencies* compositor_deps,
                           bool hidden,
                           bool never_composited)
    : routing_id_(widget_routing_id),
      compositor_deps_(compositor_deps),
      is_hidden_(hidden),
      never_composited_(never_composited) {
  DCHECK_NE(routing_id_, MSG_ROUTING_NONE);
  DCHECK(RenderThread::IsMainThread());
  DCHECK(compositor_deps_);
}

RenderWidget::~RenderWidget() {
  DCHECK(!webwidget_) << "Leaking our WebWidget!";
  DCHECK(closing_)
      << " RenderWidget must be destroyed via RenderWidget::Close()";
}

void RenderWidget::InitForPopup(ShowCallback show_callback,
                                RenderWidget* opener_widget,
                                blink::WebPagePopup* web_page_popup,
                                const blink::ScreenInfo& screen_info) {
  for_popup_ = true;
  Initialize(std::move(show_callback), web_page_popup, screen_info);
}

void RenderWidget::InitForPepperFullscreen(
    ShowCallback show_callback,
    blink::WebWidget* web_widget,
    const blink::ScreenInfo& screen_info) {
  for_pepper_fullscreen_ = true;
  Initialize(std::move(show_callback), web_widget, screen_info);
}

void RenderWidget::InitForMainFrame(ShowCallback show_callback,
                                    blink::WebFrameWidget* web_frame_widget,
                                    const blink::ScreenInfo& screen_info,
                                    RenderWidgetDelegate& delegate) {
  delegate_ = &delegate;
  Initialize(std::move(show_callback), web_frame_widget, screen_info);
}

void RenderWidget::InitForChildLocalRoot(
    blink::WebFrameWidget* web_frame_widget,
    const blink::ScreenInfo& screen_info) {
  for_child_local_root_frame_ = true;
  Initialize(base::NullCallback(), web_frame_widget, screen_info);
}

void RenderWidget::CloseForFrame(std::unique_ptr<RenderWidget> widget) {
  DCHECK(for_frame());
  DCHECK_EQ(widget.get(), this);  // This method takes ownership of |this|.

  Close(std::move(widget));
}

void RenderWidget::Initialize(ShowCallback show_callback,
                              blink::WebWidget* web_widget,
                              const blink::ScreenInfo& screen_info) {
  DCHECK_NE(routing_id_, MSG_ROUTING_NONE);
  DCHECK(web_widget);

  show_callback_ = std::move(show_callback);

  webwidget_mouse_lock_target_.reset(new WebWidgetLockTarget(this));
  mouse_lock_dispatcher_.reset(new RenderWidgetMouseLockDispatcher(this));

  RenderThread::Get()->AddRoute(routing_id_, this);

  webwidget_ = web_widget;
  if (auto* scheduler_state = GetWebWidget()->RendererWidgetSchedulingState())
    scheduler_state->SetHidden(is_hidden());

  InitCompositing(screen_info);

  // If the widget is hidden, delay starting the compositor until the user
  // shows it. Otherwise start the compositor immediately. If the widget is
  // for a provisional frame, this importantly starts the compositor before
  // the frame is inserted into the frame tree, which impacts first paint
  // metrics.
  if (!is_hidden_ && !never_composited_)
    web_widget->SetCompositorVisible(true);
}

bool RenderWidget::OnMessageReceived(const IPC::Message& message) {
  // We shouldn't receive IPC messages on provisional frames. It's possible the
  // message was destined for a RenderWidget that was destroyed and then
  // recreated since it keeps the same routing id. Just drop it here if that
  // happened.
  if (IsForProvisionalFrame())
    return false;

  bool handled = false;
  IPC_BEGIN_MESSAGE_MAP(RenderWidget, message)
    IPC_MESSAGE_HANDLER(WidgetMsg_Close, OnClose)
    IPC_MESSAGE_HANDLER(WidgetMsg_WasHidden, OnWasHidden)
    IPC_MESSAGE_HANDLER(WidgetMsg_WasShown, OnWasShown)
    IPC_MESSAGE_HANDLER(WidgetMsg_SetBounds_ACK, OnRequestSetBoundsAck)
    IPC_MESSAGE_HANDLER(WidgetMsg_SetViewportIntersection,
                        OnSetViewportIntersection)
    IPC_MESSAGE_HANDLER(DragMsg_TargetDragEnter, OnDragTargetDragEnter)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool RenderWidget::Send(IPC::Message* message) {
  // Provisional frames don't send IPCs until they are swapped in/committed.
  CHECK(!IsForProvisionalFrame());
  // Don't send any messages during shutdown.
  DCHECK(!closing_);

  // If given a messsage without a routing ID, then assign our routing ID.
  if (message->routing_id() == MSG_ROUTING_NONE)
    message->set_routing_id(routing_id_);

  return RenderThread::Get()->Send(message);
}

void RenderWidget::OnClose() {
  DCHECK(for_popup_ || for_pepper_fullscreen_);

  Close(base::WrapUnique(this));
}

void RenderWidget::UpdateVisualProperties(
    bool emulator_enabled,
    const blink::VisualProperties& visual_properties) {
  if (delegate()) {
    if (size_ != visual_properties.new_size) {
      // Only hide popups when the size changes. Eg https://crbug.com/761908.
      blink::WebView* web_view = GetFrameWidget()->LocalRoot()->View();
      web_view->CancelPagePopup();
    }

    browser_controls_params_ = visual_properties.browser_controls_params;
  }

  if (!emulator_enabled) {
    // We can ignore browser-initialized resizing during synchronous
    // (renderer-controlled) mode, unless it is switching us to/from
    // fullsreen mode or changing the device scale factor.
    bool ignore_resize_ipc = synchronous_resize_mode_for_testing_;
    if (ignore_resize_ipc) {
      // TODO(danakj): Does the browser actually change DSF inside a web test??
      // TODO(danakj): Isn't the display mode check redundant with the
      // fullscreen one?
      if (visual_properties.is_fullscreen_granted !=
              IsFullscreenGrantedForFrame() ||
          visual_properties.screen_info.device_scale_factor !=
              GetWebWidget()->GetScreenInfo().device_scale_factor)
        ignore_resize_ipc = false;
    }

    // When controlling the size in the renderer, we should ignore sizes given
    // by the browser IPC here.
    // TODO(danakj): There are many things also being ignored that aren't the
    // widget's size params. It works because tests that use this mode don't
    // change those parameters, I guess. But it's more complicated then because
    // it looks like they are related to sync resize mode. Let's move them out
    // of this block.
    if (!ignore_resize_ipc) {
      gfx::Rect new_compositor_viewport_pixel_rect =
          visual_properties.compositor_viewport_pixel_rect;
      if (AutoResizeMode()) {
        new_compositor_viewport_pixel_rect = gfx::Rect(gfx::ScaleToCeiledSize(
            size_, visual_properties.screen_info.device_scale_factor));
      }

      GetWebWidget()->UpdateSurfaceAndScreenInfo(
          visual_properties.local_surface_id_allocation.value_or(
              viz::LocalSurfaceIdAllocation()),
          new_compositor_viewport_pixel_rect, visual_properties.screen_info);

      // Store this even when auto-resizing, it is the size of the full viewport
      // used for clipping, and this value is propagated down the RenderWidget
      // hierarchy via the VisualProperties waterfall.
      GetWebWidget()->SetVisibleViewportSize(
          visual_properties.visible_viewport_size);

      if (!AutoResizeMode()) {
        SetSize(visual_properties.new_size);
      }
    }
  }
}

void RenderWidget::OnWasHidden() {
  // A provisional frame widget will never be hidden since that would require it
  // to be shown first. A frame must be attached to the frame tree before
  // changing visibility.
  DCHECK(!IsForProvisionalFrame());

  TRACE_EVENT0("renderer", "RenderWidget::OnWasHidden");

  SetHidden(true);

  tab_switch_time_recorder_.TabWasHidden();

  for (auto& observer : render_frames_)
    observer.WasHidden();
}

void RenderWidget::OnWasShown(
    base::TimeTicks show_request_timestamp,
    bool was_evicted,
    const blink::mojom::RecordContentToVisibleTimeRequestPtr&
        record_tab_switch_time_request) {
  // The frame must be attached to the frame tree (which makes it no longer
  // provisional) before changing visibility.
  DCHECK(!IsForProvisionalFrame());

  TRACE_EVENT_WITH_FLOW0("renderer", "RenderWidget::OnWasShown", routing_id(),
                         TRACE_EVENT_FLAG_FLOW_IN);

  SetHidden(false);
  if (record_tab_switch_time_request) {
    layer_tree_host_->RequestPresentationTimeForNextFrame(
        tab_switch_time_recorder_.TabWasShown(
            false /* has_saved_frames */,
            record_tab_switch_time_request.Clone(), show_request_timestamp));
  }

  for (auto& observer : render_frames_)
    observer.WasShown();
  if (was_evicted) {
    for (auto& observer : render_frame_proxies_)
      observer.WasEvicted();
  }
}

void RenderWidget::OnRequestSetBoundsAck() {
  DCHECK(pending_window_rect_count_);
  pending_window_rect_count_--;
  if (pending_window_rect_count_ == 0)
    GetWebWidget()->SetPendingWindowRect(nullptr);
}

void RenderWidget::RequestPresentation(PresentationTimeCallback callback) {
  layer_tree_host_->RequestPresentationTimeForNextFrame(std::move(callback));
  layer_tree_host_->SetNeedsCommitWithForcedRedraw();
}

viz::FrameSinkId RenderWidget::GetFrameSinkIdAtPoint(const gfx::PointF& point,
                                                     gfx::PointF* local_point) {
  blink::WebHitTestResult result = GetHitTestResultAtPoint(point);

  blink::WebNode result_node = result.GetNode();
  *local_point = gfx::PointF(point);

  // TODO(crbug.com/797828): When the node is null the caller may
  // need to do extra checks. Like maybe update the layout and then
  // call the hit-testing API. Either way it might be better to have
  // a DCHECK for the node rather than a null check here.
  if (result_node.IsNull()) {
    return GetFrameSinkId();
  }

  viz::FrameSinkId frame_sink_id = GetRemoteFrameSinkId(result);
  if (frame_sink_id.is_valid()) {
    *local_point = gfx::PointF(result.LocalPointWithoutContentBoxOffset());
    if (compositor_deps()->IsUseZoomForDSFEnabled()) {
      *local_point = gfx::ConvertPointToDIP(
          GetWebWidget()->GetOriginalScreenInfo().device_scale_factor,
          *local_point);
    }
    return frame_sink_id;
  }

  // Return the FrameSinkId for the current widget if the point did not hit
  // test to a remote frame, or the point is outside of the remote frame's
  // content box, or the remote frame doesn't have a valid FrameSinkId yet.
  return GetFrameSinkId();
}

void RenderWidget::SetActive(bool active) {
  if (delegate())
    delegate()->SetActiveForWidget(active);
}

void RenderWidget::FocusChanged(bool enable) {
  if (delegate())
    delegate()->DidReceiveSetFocusEventForWidget();

  for (auto& observer : render_frames_)
    observer.RenderWidgetSetFocus(enable);
}

void RenderWidget::RequestNewLayerTreeFrameSink(
    LayerTreeFrameSinkCallback callback) {
  // For widgets that are never visible, we don't start the compositor, so we
  // never get a request for a cc::LayerTreeFrameSink.
  DCHECK(!never_composited_);

  GURL url = GetWebWidget()->GetURLForDebugTrace();
  // The |url| is not always available, fallback to a fixed string.
  if (url.is_empty())
    url = GURL("chrome://gpu/RenderWidget::RequestNewLayerTreeFrameSink");
  // TODO(danakj): This may not be accurate, depending on the intent. A child
  // local root could be in the same process as the view, so if the client is
  // meant to designate the process type, it seems kRenderer would be the
  // correct choice. If client is meant to designate the widget type, then
  // kOOPIF would denote that it is not for the main frame. However, kRenderer
  // would also be used for other widgets such as popups.
  const char* client_name = for_child_local_root_frame_ ? kOOPIF : kRenderer;
  compositor_deps_->RequestNewLayerTreeFrameSink(
      this, std::move(url), std::move(callback), client_name);
}

void RenderWidget::DidCommitAndDrawCompositorFrame() {
  // NOTE: Tests may break if this event is renamed or moved. See
  // tab_capture_performancetest.cc.
  TRACE_EVENT0("gpu", "RenderWidget::DidCommitAndDrawCompositorFrame");

  for (auto& observer : render_frames_)
    observer.DidCommitAndDrawCompositorFrame();
}

void RenderWidget::DidCommitCompositorFrame(base::TimeTicks commit_start_time) {
  if (delegate())
    delegate()->DidCommitCompositorFrameForWidget();
}

void RenderWidget::DidCompletePageScaleAnimation() {
  if (delegate())
    delegate()->DidCompletePageScaleAnimationForWidget();
}

void RenderWidget::ScheduleAnimation() {
  // This call is not needed in single thread mode for tests without a
  // scheduler, but they override this method in order to schedule a synchronous
  // composite task themselves.
  layer_tree_host_->SetNeedsAnimate();
}

void RenderWidget::RecordTimeToFirstActivePaint(base::TimeDelta duration) {
  RenderThreadImpl* render_thread_impl = RenderThreadImpl::current();
  if (render_thread_impl->NeedsToRecordFirstActivePaint(TTFAP_AFTER_PURGED)) {
    UMA_HISTOGRAM_TIMES("PurgeAndSuspend.Experimental.TimeToFirstActivePaint",
                        duration);
  }
  if (render_thread_impl->NeedsToRecordFirstActivePaint(
          TTFAP_5MIN_AFTER_BACKGROUNDED)) {
    UMA_HISTOGRAM_TIMES(
        "PurgeAndSuspend.Experimental.TimeToFirstActivePaint."
        "AfterBackgrounded.5min",
        duration);
  }
}

bool RenderWidget::CanComposeInline() {
#if BUILDFLAG(ENABLE_PLUGINS)
  if (auto* plugin = GetFocusedPepperPluginInsideWidget())
    return plugin->IsPluginAcceptingCompositionEvents();
#endif
  return true;
}

bool RenderWidget::ShouldDispatchImeEventsToPepper() {
#if BUILDFLAG(ENABLE_PLUGINS)
  return GetFocusedPepperPluginInsideWidget();
#else
  return false;
#endif
}

blink::WebTextInputType RenderWidget::GetPepperTextInputType() {
#if BUILDFLAG(ENABLE_PLUGINS)
  return ConvertTextInputType(
      GetFocusedPepperPluginInsideWidget()->text_input_type());
#else
  NOTREACHED();
  return blink::WebTextInputType::kWebTextInputTypeNone;
#endif
}

gfx::Rect RenderWidget::GetPepperCaretBounds() {
#if BUILDFLAG(ENABLE_PLUGINS)
  blink::WebRect caret(GetFocusedPepperPluginInsideWidget()->GetCaretBounds());
  ConvertViewportToWindow(&caret);
  return caret;
#else
  NOTREACHED();
  return gfx::Rect();
#endif
}

void RenderWidget::UpdateTextInputState() {
  GetWebWidget()->UpdateTextInputState();
}

bool RenderWidget::WillHandleGestureEvent(const blink::WebGestureEvent& event) {
  possible_drag_event_info_.event_source = ui::mojom::DragEventSource::kTouch;
  possible_drag_event_info_.event_location =
      gfx::ToFlooredPoint(event.PositionInScreen());

  return false;
}

bool RenderWidget::WillHandleMouseEvent(const blink::WebMouseEvent& event) {
  for (auto& observer : render_frames_)
    observer.RenderWidgetWillHandleMouseEvent();

  possible_drag_event_info_.event_source = ui::mojom::DragEventSource::kMouse;
  possible_drag_event_info_.event_location =
      gfx::Point(event.PositionInScreen().x(), event.PositionInScreen().y());

  return mouse_lock_dispatcher()->WillHandleMouseEvent(event);
}

void RenderWidget::ResizeWebWidget() {
  // In auto resize mode, blink controls sizes and RenderWidget should not be
  // passing values back in.
  DCHECK(!AutoResizeMode());

  // The widget size given to blink is scaled by the (non-emulated,
  // see https://crbug.com/819903) device scale factor (if UseZoomForDSF is
  // enabled).
  gfx::Size size_for_blink;
  if (!compositor_deps_->IsUseZoomForDSFEnabled()) {
    size_for_blink = size_;
  } else {
    size_for_blink = gfx::ScaleToCeiledSize(
        size_, GetWebWidget()->GetOriginalScreenInfo().device_scale_factor);
  }

  // The |visible_viewport_size| given to blink is scaled by the (non-emulated,
  // see https://crbug.com/819903) device scale factor (if UseZoomForDSF is
  // enabled).
  gfx::Size visible_viewport_size_for_blink;
  if (!compositor_deps_->IsUseZoomForDSFEnabled()) {
    visible_viewport_size_for_blink = GetWebWidget()->VisibleViewportSize();
  } else {
    visible_viewport_size_for_blink = gfx::ScaleToCeiledSize(
        GetWebWidget()->VisibleViewportSize(),
        GetWebWidget()->GetOriginalScreenInfo().device_scale_factor);
  }

  if (delegate()) {
    // When associated with a RenderView, the RenderView is in control of the
    // main frame's size, because it includes other factors for top and bottom
    // controls.
    delegate()->ResizeWebWidgetForWidget(size_for_blink,
                                         visible_viewport_size_for_blink,
                                         browser_controls_params_);
  } else {
    // Child frames set the |visible_viewport_size| on the RenderView/WebView to
    // limit the size blink tries to composite when the widget is not visible,
    // such as when it is scrolled out of the main frame's view.
    if (for_frame()) {
      RenderFrameImpl* render_frame =
          RenderFrameImpl::FromWebFrame(GetFrameWidget()->LocalRoot());
      render_frame->SetVisibleViewportSizeForChildLocalRootOnRenderView(
          visible_viewport_size_for_blink);
    }

    // For child frame widgets, popups, and pepper, the RenderWidget is in
    // control of the WebWidget's size.
    GetWebWidget()->Resize(size_for_blink);
  }
}

///////////////////////////////////////////////////////////////////////////////
// WebWidgetClient

void RenderWidget::DidMeaningfulLayout(blink::WebMeaningfulLayout layout_type) {
  for (auto& observer : render_frames_)
    observer.DidMeaningfulLayout(layout_type);
}

void RenderWidget::SetHandlingInputEvent(bool handling_input_event) {
  GetWebWidget()->SetHandlingInputEvent(handling_input_event);
}

// We are supposed to get a single call to Show for a newly created RenderWidget
// that was created via RenderWidget::CreateWebView.  So, we wait until this
// point to dispatch the ShowWidget message.
//
// This method provides us with the information about how to display the newly
// created RenderWidget (i.e., as a blocked popup or as a new tab).
//
void RenderWidget::Show(WebNavigationPolicy policy) {
  if (!show_callback_) {
    if (delegate()) {
      // When SupportsMultipleWindows is disabled, popups are reusing
      // the view's RenderWidget. In some scenarios, this makes blink to call
      // Show() twice. But otherwise, if it is enabled, we should not visit
      // Show() more than once.
      DCHECK(!delegate()->SupportsMultipleWindowsForWidget());
      return;
    } else {
      NOTREACHED() << "received extraneous Show call";
    }
  }

  DCHECK(routing_id_ != MSG_ROUTING_NONE);

  // The opener is responsible for actually showing this widget.
  std::move(show_callback_).Run(this, policy, initial_rect_);

  // NOTE: initial_rect_ may still have its default values at this point, but
  // that's okay.  It'll be ignored if as_popup is false, or the browser
  // process will impose a default position otherwise.
  SetPendingWindowRect(initial_rect_);
}

void RenderWidget::InitCompositing(const blink::ScreenInfo& screen_info) {
  TRACE_EVENT0("blink", "RenderWidget::InitializeLayerTreeView");

  layer_tree_host_ = webwidget_->InitializeCompositing(
      never_composited_, compositor_deps_->GetWebMainThreadScheduler(),
      compositor_deps_->GetTaskGraphRunner(), for_child_local_root_frame_,
      screen_info, compositor_deps_->CreateUkmRecorderFactory(),
      /*settings=*/nullptr);
  DCHECK(layer_tree_host_);
  webwidget_->UpdateScreenInfo(screen_info);
}

// static
void RenderWidget::DoDeferredClose(int widget_routing_id) {
  // DoDeferredClose() was a posted task, which means the RenderWidget may have
  // been destroyed in the meantime. So break the dependency on RenderWidget
  // here, by making this method static and going to RenderThread directly to
  // send.
  RenderThread::Get()->Send(new WidgetHostMsg_Close(widget_routing_id));
}

void RenderWidget::ClosePopupWidgetSoon() {
  // Only should be called for popup widgets.
  DCHECK(!for_child_local_root_frame_);
  DCHECK(!delegate_);

  CloseWidgetSoon();
}

void RenderWidget::CloseWidgetSoon() {
  DCHECK(RenderThread::IsMainThread());

  // If a page calls window.close() twice, we'll end up here twice, but that's
  // OK.  It is safe to send multiple Close messages.
  //
  // Ask the RenderWidgetHost to initiate close.  We could be called from deep
  // in Javascript.  If we ask the RenderWidgetHost to close now, the window
  // could be closed before the JS finishes executing, thanks to nested message
  // loops running and handling the resuliting Close IPC. So instead, post a
  // message back to the message loop, which won't run until the JS is
  // complete, and then the Close request can be sent.
  compositor_deps_->GetCleanupTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&RenderWidget::DoDeferredClose, routing_id_));
}

void RenderWidget::Close(std::unique_ptr<RenderWidget> widget) {
  // At the end of this method, |widget| which points to this is deleted.
  DCHECK_EQ(widget.get(), this);
  DCHECK(RenderThread::IsMainThread());
  DCHECK(!closing_);

  closing_ = true;

  // Browser correspondence is no longer needed at this point.
  if (routing_id_ != MSG_ROUTING_NONE) {
    RenderThread::Get()->RemoveRoute(routing_id_);
  }

  webwidget_->Close(compositor_deps_->GetCleanupTaskRunner());
  webwidget_ = nullptr;

  // |layer_tree_host_| is valid only when |webwidget_| is valid. Close may
  // use the WebWidgetClient while unloading the Frame so we clear this
  // after.
  layer_tree_host_ = nullptr;

  // Note the ACK is a control message going to the RenderProcessHost.
  RenderThread::Get()->Send(new WidgetHostMsg_Close_ACK(routing_id()));
}

blink::WebFrameWidget* RenderWidget::GetFrameWidget() const {
  // TODO(danakj): Remove this check and don't call this method for non-frames.
  if (!for_frame())
    return nullptr;
  return static_cast<blink::WebFrameWidget*>(webwidget_);
}

bool RenderWidget::IsForProvisionalFrame() const {
  if (!for_frame())
    return false;
  // No widget here means the main frame is remote and there is no
  // provisional frame at the moment.
  if (!webwidget_)
    return false;
  auto* frame_widget = static_cast<blink::WebFrameWidget*>(webwidget_);
  return frame_widget->LocalRoot()->IsProvisional();
}

void RenderWidget::SetWindowRect(const gfx::Rect& window_rect) {
  // This path is for the renderer to change the on-screen position/size of
  // the widget by changing its window rect. This is not possible for
  // RenderWidgets whose position/size are controlled by layout from another
  // frame tree (ie. child local root frames), as the window rect can only be
  // set by the browser.
  if (for_child_local_root_frame_)
    return;

  if (synchronous_resize_mode_for_testing_) {
    // This is a web-test-only path. At one point, it was planned to be
    // removed. See https://crbug.com/309760.
    SetWindowRectSynchronously(window_rect);
    return;
  }

  if (show_callback_) {
    // The widget is not shown yet. Delay the |window_rect| being sent to the
    // browser until Show() is called so it can be sent with that IPC, once the
    // browser is ready for the info.
    initial_rect_ = window_rect;
  } else {
    Send(new WidgetHostMsg_RequestSetBounds(routing_id_, window_rect));
    SetPendingWindowRect(window_rect);
  }
}

void RenderWidget::SetPendingWindowRect(const gfx::Rect& rect) {
  GetWebWidget()->SetPendingWindowRect(&rect);
  pending_window_rect_count_++;

  // Popups don't get size updates back from the browser so just store the set
  // values.
  if (!for_frame()) {
    GetWebWidget()->SetScreenRects(rect, rect);
  }
}

void RenderWidget::SetSize(const gfx::Size& new_size) {
  size_ = new_size;
  ResizeWebWidget();
}

void RenderWidget::ImeSetCompositionForPepper(
    const blink::WebString& text,
    const std::vector<ui::ImeTextSpan>& ime_text_spans,
    const gfx::Range& replacement_range,
    int selection_start,
    int selection_end) {
#if BUILDFLAG(ENABLE_PLUGINS)
  auto* plugin = GetFocusedPepperPluginInsideWidget();
  DCHECK(plugin);
  plugin->render_frame()->OnImeSetComposition(text.Utf16(), ime_text_spans,
                                              selection_start, selection_end);
#endif
}

void RenderWidget::ImeCommitTextForPepper(
    const blink::WebString& text,
    const std::vector<ui::ImeTextSpan>& ime_text_spans,
    const gfx::Range& replacement_range,
    int relative_cursor_pos) {
#if BUILDFLAG(ENABLE_PLUGINS)
  auto* plugin = GetFocusedPepperPluginInsideWidget();
  DCHECK(plugin);
  plugin->render_frame()->OnImeCommitText(text.Utf16(), replacement_range,
                                          relative_cursor_pos);
#endif
}

void RenderWidget::ImeFinishComposingTextForPepper(bool keep_selection) {
#if BUILDFLAG(ENABLE_PLUGINS)
  auto* plugin = GetFocusedPepperPluginInsideWidget();
  DCHECK(plugin);
  plugin->render_frame()->OnImeFinishComposingText(keep_selection);
#endif
}

void RenderWidget::SetWindowRectSynchronously(
    const gfx::Rect& new_window_rect) {
  // This method is only call in tests, and it applies the |new_window_rect| to
  // all three of:
  // a) widget size (in |size_|)
  // b) blink viewport (in |visible_viewport_size_|)
  // c) compositor viewport (in cc::LayerTreeHost)
  // Normally the browser controls these three things independently, but this is
  // used in tests to control the size from the renderer.

  // We are resizing the window from the renderer, so allocate a new
  // viz::LocalSurfaceId to avoid surface invariants violations in tests.
  layer_tree_host_->RequestNewLocalSurfaceId();

  gfx::Rect compositor_viewport_pixel_rect(gfx::ScaleToCeiledSize(
      new_window_rect.size(),
      GetWebWidget()->GetScreenInfo().device_scale_factor));
  GetWebWidget()->UpdateCompositorViewportRect(compositor_viewport_pixel_rect);

  GetWebWidget()->SetVisibleViewportSize(new_window_rect.size());
  SetSize(new_window_rect.size());
  GetWebWidget()->SetScreenRects(new_window_rect, new_window_rect);

  if (show_callback_) {
    // Tests may call here directly to control the window rect. If
    // Show() did not happen yet, the rect is stored to be passed to the
    // browser when the RenderWidget requests Show().
    initial_rect_ = new_window_rect;
  }
}

void RenderWidget::OnSetViewportIntersection(
    const blink::ViewportIntersectionState& intersection_state) {
  if (auto* frame_widget = GetFrameWidget()) {
    frame_widget->SetRemoteViewportIntersection(intersection_state);
  }
}

void RenderWidget::OnDragTargetDragEnter(
    const std::vector<DropData::Metadata>& drop_meta_data,
    const gfx::PointF& client_point,
    const gfx::PointF& screen_point,
    WebDragOperationsMask ops,
    int key_modifiers) {
  blink::WebFrameWidget* frame_widget = GetFrameWidget();
  if (!frame_widget)
    return;

  WebDragOperation operation = frame_widget->DragTargetDragEnter(
      DropMetaDataToWebDragData(drop_meta_data), client_point, screen_point,
      ops, key_modifiers);

  Send(new DragHostMsg_UpdateDragCursor(routing_id(), operation));
}

void RenderWidget::ConvertViewportToWindow(blink::WebRect* rect) {
  if (compositor_deps_->IsUseZoomForDSFEnabled()) {
    float reverse =
        1 / GetWebWidget()->GetOriginalScreenInfo().device_scale_factor;
    // TODO(oshima): We may need to allow pixel precision here as the the
    // anchor element can be placed at half pixel.
    gfx::Rect window_rect = gfx::ScaleToEnclosedRect(gfx::Rect(*rect), reverse);
    rect->x = window_rect.x();
    rect->y = window_rect.y();
    rect->width = window_rect.width();
    rect->height = window_rect.height();
  }
}

void RenderWidget::ConvertViewportToWindow(blink::WebFloatRect* rect) {
  if (compositor_deps_->IsUseZoomForDSFEnabled()) {
    float device_scale_factor =
        GetWebWidget()->GetOriginalScreenInfo().device_scale_factor;
    rect->x /= device_scale_factor;
    rect->y /= device_scale_factor;
    rect->width /= device_scale_factor;
    rect->height /= device_scale_factor;
  }
}

void RenderWidget::ConvertWindowToViewport(blink::WebFloatRect* rect) {
  if (compositor_deps_->IsUseZoomForDSFEnabled()) {
    float device_scale_factor =
        GetWebWidget()->GetOriginalScreenInfo().device_scale_factor;
    rect->x *= device_scale_factor;
    rect->y *= device_scale_factor;
    rect->width *= device_scale_factor;
    rect->height *= device_scale_factor;
  }
}

void RenderWidget::SetHidden(bool hidden) {
  // A provisional frame widget will never be shown or hidden, as the frame must
  // be attached to the frame tree before changing visibility.
  DCHECK(!IsForProvisionalFrame());

  if (is_hidden_ == hidden)
    return;

  // The status has changed.  Tell the RenderThread about it and ensure
  // throttled acks are released in case frame production ceases.
  is_hidden_ = hidden;

  if (auto* scheduler_state = GetWebWidget()->RendererWidgetSchedulingState())
    scheduler_state->SetHidden(hidden);

  // If the renderer was hidden, resolve any pending synthetic gestures so they
  // aren't blocked waiting for a compositor frame to be generated.
  if (is_hidden_)
    webwidget_->FlushInputProcessedCallback();

  if (!never_composited_)
    webwidget_->SetCompositorVisible(!is_hidden_);
}

void RenderWidget::UpdateSelectionBounds() {
  GetWebWidget()->UpdateSelectionBounds();
}

void RenderWidget::DidAutoResize(const gfx::Size& new_size) {
  WebRect new_size_in_window(0, 0, new_size.width(), new_size.height());
  ConvertViewportToWindow(&new_size_in_window);
  if (size_.width() != new_size_in_window.width ||
      size_.height() != new_size_in_window.height) {
    size_ = gfx::Size(new_size_in_window.width, new_size_in_window.height);

    if (synchronous_resize_mode_for_testing_) {
      gfx::Rect new_pos(GetWebWidget()->WindowRect().origin(), size_);
      GetWebWidget()->SetScreenRects(new_pos, new_pos);
    }
  }
}

void RenderWidget::RequestDecode(const cc::PaintImage& image,
                                 base::OnceCallback<void(bool)> callback) {
  layer_tree_host_->QueueImageDecode(image, std::move(callback));
}

viz::FrameSinkId RenderWidget::GetFrameSinkId() {
  return viz::FrameSinkId(RenderThread::Get()->GetClientId(), routing_id());
}

void RenderWidget::RegisterRenderFrameProxy(RenderFrameProxy* proxy) {
  render_frame_proxies_.AddObserver(proxy);
}

void RenderWidget::UnregisterRenderFrameProxy(RenderFrameProxy* proxy) {
  render_frame_proxies_.RemoveObserver(proxy);
}

void RenderWidget::RegisterRenderFrame(RenderFrameImpl* frame) {
  render_frames_.AddObserver(frame);
}

void RenderWidget::UnregisterRenderFrame(RenderFrameImpl* frame) {
  render_frames_.RemoveObserver(frame);
}

gfx::PointF RenderWidget::ConvertWindowPointToViewport(
    const gfx::PointF& point) {
  blink::WebFloatRect point_in_viewport(point.x(), point.y(), 0, 0);
  ConvertWindowToViewport(&point_in_viewport);
  return gfx::PointF(point_in_viewport.x, point_in_viewport.y);
}

gfx::Point RenderWidget::ConvertWindowPointToViewport(const gfx::Point& point) {
  return gfx::ToRoundedPoint(ConvertWindowPointToViewport(gfx::PointF(point)));
}

bool RenderWidget::RequestPointerLock(
    WebLocalFrame* requester_frame,
    blink::WebWidgetClient::PointerLockCallback callback,
    bool request_unadjusted_movement) {
  return mouse_lock_dispatcher_->LockMouse(webwidget_mouse_lock_target_.get(),
                                           requester_frame, std::move(callback),
                                           request_unadjusted_movement);
}

bool RenderWidget::RequestPointerLockChange(
    blink::WebLocalFrame* requester_frame,
    blink::WebWidgetClient::PointerLockCallback callback,
    bool request_unadjusted_movement) {
  return mouse_lock_dispatcher_->ChangeMouseLock(
      webwidget_mouse_lock_target_.get(), requester_frame, std::move(callback),
      request_unadjusted_movement);
}

void RenderWidget::RequestPointerUnlock() {
  mouse_lock_dispatcher_->UnlockMouse(webwidget_mouse_lock_target_.get());
}

bool RenderWidget::IsPointerLocked() {
  return mouse_lock_dispatcher_->IsMouseLockedTo(
      webwidget_mouse_lock_target_.get());
}

void RenderWidget::StartDragging(const WebDragData& data,
                                 WebDragOperationsMask mask,
                                 const SkBitmap& drag_image,
                                 const gfx::Point& web_image_offset) {
  blink::WebRect offset_in_window(web_image_offset.x(), web_image_offset.y(), 0,
                                  0);
  ConvertViewportToWindow(&offset_in_window);
  DropData drop_data(DropDataBuilder::Build(data));
  gfx::Vector2d image_offset(offset_in_window.x, offset_in_window.y);
  Send(new DragHostMsg_StartDragging(routing_id(), drop_data, mask, drag_image,
                                     image_offset, possible_drag_event_info_));
}

void RenderWidget::DidNavigate(ukm::SourceId source_id, const GURL& url) {
  // Update the URL and the document source id used to key UKM metrics in the
  // compositor. Note that the metrics for all frames are keyed to the main
  // frame's URL.
  layer_tree_host_->SetSourceURL(source_id, url);
}

blink::WebInputMethodController* RenderWidget::GetInputMethodController()
    const {
  if (auto* frame_widget = GetFrameWidget())
    return frame_widget->GetActiveWebInputMethodController();

  return nullptr;
}

void RenderWidget::UseSynchronousResizeModeForTesting(bool enable) {
  synchronous_resize_mode_for_testing_ = enable;
}

blink::WebHitTestResult RenderWidget::GetHitTestResultAtPoint(
    const gfx::PointF& point) {
  gfx::PointF point_in_pixel(point);
  if (compositor_deps()->IsUseZoomForDSFEnabled()) {
    point_in_pixel = gfx::ConvertPointToPixel(
        GetWebWidget()->GetOriginalScreenInfo().device_scale_factor,
        point_in_pixel);
  }
  return GetWebWidget()->HitTestResultAt(point_in_pixel);
}

void RenderWidget::SetDeviceScaleFactorForTesting(float factor) {
  DCHECK(for_frame());
  GetFrameWidget()->SetDeviceScaleFactorForTesting(factor);

  // Receiving a 0 is used to reset between tests, it removes the override in
  // order to listen to the browser for the next test.
  if (!factor)
    return;

  blink::ScreenInfo info = GetWebWidget()->GetScreenInfo();
  info.device_scale_factor = factor;
  gfx::Size viewport_pixel_size = gfx::ScaleToCeiledSize(size_, factor);
  GetWebWidget()->UpdateCompositorViewportAndScreenInfo(
      gfx::Rect(viewport_pixel_size), info);
  if (!AutoResizeMode())
    ResizeWebWidget();  // This picks up the new device scale factor in |info|.
}

void RenderWidget::SetWindowRectSynchronouslyForTesting(
    const gfx::Rect& new_window_rect) {
  SetWindowRectSynchronously(new_window_rect);
}

#if BUILDFLAG(ENABLE_PLUGINS)
PepperPluginInstanceImpl* RenderWidget::GetFocusedPepperPluginInsideWidget() {
  blink::WebFrameWidget* frame_widget = GetFrameWidget();
  if (!frame_widget)
    return nullptr;

  // Focused pepper instance might not always be in the focused frame. For
  // instance if a pepper instance and its embedder frame are focused an then
  // another frame takes focus using javascript, the embedder frame will no
  // longer be focused while the pepper instance is (the embedder frame's
  // |focused_pepper_plugin_| is not nullptr). Especially, if the pepper plugin
  // is fullscreen, clicking into the pepper will not refocus the embedder
  // frame. This is why we have to traverse the whole frame tree to find the
  // focused plugin.
  blink::WebFrame* current_frame = frame_widget->LocalRoot();
  while (current_frame) {
    RenderFrameImpl* render_frame =
        current_frame->IsWebLocalFrame()
            ? RenderFrameImpl::FromWebFrame(current_frame)
            : nullptr;
    if (render_frame && render_frame->focused_pepper_plugin())
      return render_frame->focused_pepper_plugin();
    current_frame = current_frame->TraverseNext();
  }
  return nullptr;
}
#endif

bool RenderWidget::AutoResizeMode() {
  if (!delegate_)
    return false;
  return delegate_->AutoResizeMode();
}

bool RenderWidget::IsFullscreenGrantedForFrame() {
  if (!for_frame())
    return false;
  return GetFrameWidget()->IsFullscreenGranted();
}

}  // namespace content
