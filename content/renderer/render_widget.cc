// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_widget.h"

#include <cmath>
#include <limits>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/ukm_manager.h"
#include "content/common/drag_messages.h"
#include "content/common/widget_messages.h"
#include "content/public/common/drop_data.h"
#include "content/renderer/agent_scheduling_group.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "ppapi/buildflags/buildflags.h"
#include "third_party/blink/public/common/page/drag_operation.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/web_drag_data.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_page_popup.h"
#include "third_party/blink/public/web/web_widget.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/gfx/geometry/rect_conversions.h"

using blink::DragOperation;
using blink::DragOperationsMask;
using blink::WebDragData;
using blink::WebNavigationPolicy;
using blink::WebString;

namespace content {

namespace {

RenderWidget::CreateRenderWidgetFunction g_create_render_widget_for_frame =
    nullptr;

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

}  // namespace

// RenderWidget ---------------------------------------------------------------

// static
void RenderWidget::InstallCreateForFrameHook(
    CreateRenderWidgetFunction create_widget) {
  g_create_render_widget_for_frame = create_widget;
}

std::unique_ptr<RenderWidget> RenderWidget::CreateForFrame(
    AgentSchedulingGroup& agent_scheduling_group,
    int32_t widget_routing_id,
    CompositorDependencies* compositor_deps) {
  if (g_create_render_widget_for_frame) {
    return g_create_render_widget_for_frame(agent_scheduling_group,
                                            widget_routing_id, compositor_deps);
  }

  return std::make_unique<RenderWidget>(agent_scheduling_group,
                                        widget_routing_id, compositor_deps);
}

RenderWidget* RenderWidget::CreateForPopup(
    AgentSchedulingGroup& agent_scheduling_group,
    int32_t widget_routing_id,
    CompositorDependencies* compositor_deps) {
  return new RenderWidget(agent_scheduling_group, widget_routing_id,
                          compositor_deps);
}

RenderWidget::RenderWidget(AgentSchedulingGroup& agent_scheduling_group,
                           int32_t widget_routing_id,
                           CompositorDependencies* compositor_deps)
    : agent_scheduling_group_(agent_scheduling_group),
      routing_id_(widget_routing_id),
      compositor_deps_(compositor_deps) {
  DCHECK_NE(routing_id_, MSG_ROUTING_NONE);
  DCHECK(RenderThread::IsMainThread());
  DCHECK(compositor_deps_);
}

RenderWidget::~RenderWidget() {
  DCHECK(!webwidget_) << "Leaking our WebWidget!";
  DCHECK(closing_)
      << " RenderWidget must be destroyed via RenderWidget::Close()";
}

void RenderWidget::InitForPopup(RenderWidget* opener_widget,
                                blink::WebPagePopup* web_page_popup,
                                const blink::ScreenInfo& screen_info) {
  for_popup_ = true;
  Initialize(base::NullCallback(), web_page_popup, screen_info);
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

  agent_scheduling_group_.AddRoute(routing_id_, this);

  webwidget_ = web_widget;
  if (auto* scheduler_state = GetWebWidget()->RendererWidgetSchedulingState())
    scheduler_state->SetHidden(web_widget->IsHidden());

  InitCompositing(screen_info);

  // If the widget is hidden, delay starting the compositor until the user
  // shows it. Otherwise start the compositor immediately. If the widget is
  // for a provisional frame, this importantly starts the compositor before
  // the frame is inserted into the frame tree, which impacts first paint
  // metrics.
  if (!web_widget->IsHidden())
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
    IPC_MESSAGE_HANDLER(WidgetMsg_SetBounds_ACK, OnRequestSetBoundsAck)
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

  return agent_scheduling_group_.Send(message);
}

void RenderWidget::BrowserClosedIpcChannelForPopupWidget() {
  DCHECK(for_popup_);

  Close(base::WrapUnique(this));
}

void RenderWidget::OnRequestSetBoundsAck() {
  GetWebWidget()->AckPendingWindowRect();
}

void RenderWidget::SetPendingWindowRect(const gfx::Rect& rect) {
  GetWebWidget()->SetPendingWindowRect(rect);
}

void RenderWidget::RequestPresentation(PresentationTimeCallback callback) {
  layer_tree_host_->RequestPresentationTimeForNextFrame(std::move(callback));
  layer_tree_host_->SetNeedsCommitWithForcedRedraw();
}

void RenderWidget::ScheduleAnimation() {
  // This call is not needed in single thread mode for tests without a
  // scheduler, but they override this method in order to schedule a synchronous
  // composite task themselves.
  // TODO(dtapuska): ScheduleAnimation might get called before layer_tree_host_
  // is assigned, inside the InitializeCompositing call. This should eventually
  // go away when this is moved inside blink. https://crbug.com/1097816
  if (layer_tree_host_)
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

void RenderWidget::UpdateTextInputState() {
  GetWebWidget()->UpdateTextInputState();
}

///////////////////////////////////////////////////////////////////////////////
// WebWidgetClient

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
      compositor_deps_->GetWebMainThreadScheduler(),
      compositor_deps_->GetTaskGraphRunner(), for_child_local_root_frame_,
      screen_info, compositor_deps_->CreateUkmRecorderFactory(),
      /*settings=*/nullptr);
  DCHECK(layer_tree_host_);
}

void RenderWidget::Close(std::unique_ptr<RenderWidget> widget) {
  // At the end of this method, |widget| which points to this is deleted.
  DCHECK_EQ(widget.get(), this);
  DCHECK(RenderThread::IsMainThread());
  DCHECK(!closing_);

  closing_ = true;

  // Browser correspondence is no longer needed at this point.
  if (routing_id_ != MSG_ROUTING_NONE) {
    agent_scheduling_group_.RemoveRoute(routing_id_);
  }

  webwidget_->Close(compositor_deps_->GetCleanupTaskRunner());
  webwidget_ = nullptr;

  // |layer_tree_host_| is valid only when |webwidget_| is valid. Close may
  // use the WebWidgetClient while unloading the Frame so we clear this
  // after.
  layer_tree_host_ = nullptr;
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

void RenderWidget::OnDragTargetDragEnter(
    const std::vector<DropData::Metadata>& drop_meta_data,
    const gfx::PointF& client_point,
    const gfx::PointF& screen_point,
    DragOperationsMask ops,
    int key_modifiers) {
  blink::WebFrameWidget* frame_widget = GetFrameWidget();
  if (!frame_widget)
    return;

  DragOperation operation = frame_widget->DragTargetDragEnter(
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

void RenderWidget::UpdateSelectionBounds() {
  GetWebWidget()->UpdateSelectionBounds();
}

viz::FrameSinkId RenderWidget::GetFrameSinkId() {
  return viz::FrameSinkId(RenderThread::Get()->GetClientId(), routing_id());
}

void RenderWidget::DidNavigate(ukm::SourceId source_id, const GURL& url) {
  // Update the URL and the document source id used to key UKM metrics in the
  // compositor. Note that the metrics for all frames are keyed to the main
  // frame's URL.
  layer_tree_host_->SetSourceURL(source_id, url);

  DCHECK(for_frame());
  RenderFrameImpl* render_frame =
      RenderFrameImpl::FromWebFrame(GetFrameWidget()->LocalRoot());
  auto shmem = layer_tree_host_->CreateSharedMemoryForSmoothnessUkm();
  if (shmem.IsValid()) {
    render_frame->SetUpSharedMemoryForSmoothness(std::move(shmem));
  }
}

}  // namespace content
