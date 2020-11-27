/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"

#include <memory>
#include <utility>

#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/swap_promise.h"
#include "cc/trees/ukm_manager.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom-blink.h"
#include "third_party/blink/public/mojom/input/touch_event.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/web_render_widget_scheduling_state.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/web/web_autofill_client.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_performance.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "third_party/blink/public/web/web_widget_client.h"
#include "third_party/blink/renderer/core/content_capture/content_capture_manager.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/events/current_input_event.h"
#include "third_party/blink/renderer/core/events/web_input_event_conversion.h"
#include "third_party/blink/renderer/core/events/wheel_event.h"
#include "third_party/blink/renderer/core/exported/web_dev_tools_agent_impl.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/exported/web_settings_impl.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame_ukm_aggregator.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/remote_frame_client.h"
#include "third_party/blink/renderer/core/frame/screen_metrics_emulator.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/core/html/portal/document_portals.h"
#include "third_party/blink/renderer/core/html/portal/portal_contents.h"
#include "third_party/blink/renderer/core/input/context_menu_allowed_scope.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/hit_test_request.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_shift_tracker.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/interactive_detector.h"
#include "third_party/blink/renderer/core/page/context_menu_controller.h"
#include "third_party/blink/renderer/core/page/drag_actions.h"
#include "third_party/blink/renderer/core/page/drag_controller.h"
#include "third_party/blink/renderer/core/page/drag_data.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/link_highlight.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/pointer_lock_controller.h"
#include "third_party/blink/renderer/core/page/scrolling/fragment_anchor.h"
#include "third_party/blink/renderer/core/page/validation_message_client.h"
#include "third_party/blink/renderer/core/paint/first_meaningful_paint_detector.h"
#include "third_party/blink/renderer/core/paint/paint_timing_detector.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/platform/graphics/animation_worklet_mutator_dispatcher_impl.h"
#include "third_party/blink/renderer/platform/graphics/compositor_mutator_client.h"
#include "third_party/blink/renderer/platform/graphics/paint_worklet_paint_dispatcher.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/widget/input/main_thread_event_queue.h"
#include "third_party/blink/renderer/platform/widget/input/widget_input_handler_manager.h"
#include "third_party/blink/renderer/platform/widget/widget_base.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-blink.h"
#include "ui/gfx/geometry/point_conversions.h"

#if defined(OS_MAC)
#include "third_party/blink/renderer/core/editing/substring_util.h"
#include "third_party/blink/renderer/platform/fonts/mac/attributed_string_type_converter.h"
#include "ui/base/mojom/attributed_string.mojom-blink.h"
#include "ui/gfx/geometry/point.h"
#endif

namespace WTF {
template <>
struct CrossThreadCopier<blink::WebReportTimeCallback>
    : public CrossThreadCopierByValuePassThrough<blink::WebReportTimeCallback> {
  STATIC_ONLY(CrossThreadCopier);
};

}  // namespace WTF

namespace blink {

namespace {

const int kCaretPadding = 10;
const float kIdealPaddingRatio = 0.3f;

// Returns a rect which is offset and scaled accordingly to |base_rect|'s
// location and size.
FloatRect NormalizeRect(const IntRect& to_normalize, const IntRect& base_rect) {
  FloatRect result(to_normalize);
  result.SetLocation(
      FloatPoint(to_normalize.Location() + (-base_rect.Location())));
  result.Scale(1.0 / base_rect.Width(), 1.0 / base_rect.Height());
  return result;
}

void ForEachLocalFrameControlledByWidget(
    LocalFrame* frame,
    const base::RepeatingCallback<void(WebLocalFrame*)>& callback) {
  callback.Run(WebLocalFrameImpl::FromFrame(frame));
  for (Frame* child = frame->FirstChild(); child;
       child = child->NextSibling()) {
    if (child->IsLocalFrame()) {
      ForEachLocalFrameControlledByWidget(DynamicTo<LocalFrame>(child),
                                          callback);
    }
  }
}

// Iterate the remote children that will be controlled by the widget. Skip over
// any RemoteFrames have have another LocalFrame root as their parent.
void ForEachRemoteFrameChildrenControlledByWidget(
    Frame* frame,
    const base::RepeatingCallback<void(RemoteFrame*)>& callback) {
  for (Frame* child = frame->Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    if (auto* remote_frame = DynamicTo<RemoteFrame>(child)) {
      callback.Run(remote_frame);
      ForEachRemoteFrameChildrenControlledByWidget(remote_frame, callback);
    } else if (auto* local_frame = DynamicTo<LocalFrame>(child)) {
      // If iteration arrives at a local root then don't descend as it will be
      // controlled by another widget.
      if (!local_frame->IsLocalRoot()) {
        ForEachRemoteFrameChildrenControlledByWidget(local_frame, callback);
      }
    }
  }

  // Iterate on any portals owned by a local frame.
  if (auto* local_frame = DynamicTo<LocalFrame>(frame)) {
    if (Document* document = local_frame->GetDocument()) {
      for (PortalContents* portal :
           DocumentPortals::From(*document).GetPortals()) {
        if (RemoteFrame* remote_frame = portal->GetFrame())
          callback.Run(remote_frame);
      }
    }
  }
}

viz::FrameSinkId GetRemoteFrameSinkId(const HitTestResult& result) {
  Node* node = result.InnerNode();
  auto* frame_owner = DynamicTo<HTMLFrameOwnerElement>(node);
  if (!frame_owner || !frame_owner->ContentFrame() ||
      !frame_owner->ContentFrame()->IsRemoteFrame())
    return viz::FrameSinkId();

  RemoteFrame* remote_frame = To<RemoteFrame>(frame_owner->ContentFrame());
  if (remote_frame->IsIgnoredForHitTest())
    return viz::FrameSinkId();
  LayoutObject* object = result.GetLayoutObject();
  DCHECK(object);
  if (!object->IsBox())
    return viz::FrameSinkId();

  IntPoint local_point = RoundedIntPoint(result.LocalPoint());
  if (!To<LayoutBox>(object)->ComputedCSSContentBoxRect().Contains(local_point))
    return viz::FrameSinkId();

  return remote_frame->GetFrameSinkId();
}

}  // namespace

// WebFrameWidget ------------------------------------------------------------

static CreateMainFrameWebFrameWidgetFunction
    g_create_main_frame_web_frame_widget = nullptr;

void InstallCreateMainFrameWebFrameWidgetHook(
    CreateMainFrameWebFrameWidgetFunction create_widget) {
  g_create_main_frame_web_frame_widget = create_widget;
}

WebFrameWidget* WebFrameWidget::CreateForMainFrame(
    WebWidgetClient* client,
    WebLocalFrame* main_frame,
    CrossVariantMojoAssociatedRemote<mojom::blink::FrameWidgetHostInterfaceBase>
        mojo_frame_widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::FrameWidgetInterfaceBase>
        mojo_frame_widget,
    CrossVariantMojoAssociatedRemote<mojom::blink::WidgetHostInterfaceBase>
        mojo_widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::WidgetInterfaceBase>
        mojo_widget,
    const viz::FrameSinkId& frame_sink_id,
    bool is_for_nested_main_frame,
    bool hidden,
    bool never_composited) {
  DCHECK(client) << "A valid WebWidgetClient must be supplied.";
  DCHECK(!main_frame->Parent());  // This is the main frame.

  // Grabs the WebViewImpl associated with the |main_frame|, which will then
  // be wrapped by the WebFrameWidgetImpl, with calls being forwarded to the
  // |main_frame|'s WebViewImpl.
  // Note: this can't DCHECK that the view's main frame points to
  // |main_frame|, as provisional frames violate this precondition.
  WebLocalFrameImpl& main_frame_impl = To<WebLocalFrameImpl>(*main_frame);
  DCHECK(main_frame_impl.ViewImpl());
  WebViewImpl& web_view_impl = *main_frame_impl.ViewImpl();

  WebFrameWidgetImpl* widget = nullptr;
  if (g_create_main_frame_web_frame_widget) {
    widget = g_create_main_frame_web_frame_widget(
        base::PassKey<WebFrameWidget>(), *client,
        std::move(mojo_frame_widget_host), std::move(mojo_frame_widget),
        std::move(mojo_widget_host), std::move(mojo_widget),
        main_frame->Scheduler()->GetAgentGroupScheduler()->DefaultTaskRunner(),
        frame_sink_id, hidden, never_composited,
        /*is_for_child_local_root=*/false, is_for_nested_main_frame);
  } else {
    // Note: this isn't a leak, as the object has a self-reference that the
    // caller needs to release by calling Close().
    // TODO(dcheng): Remove the special bridge class for main frame widgets.
    widget = MakeGarbageCollected<WebFrameWidgetImpl>(
        base::PassKey<WebFrameWidget>(), *client,
        std::move(mojo_frame_widget_host), std::move(mojo_frame_widget),
        std::move(mojo_widget_host), std::move(mojo_widget),
        main_frame->Scheduler()->GetAgentGroupScheduler()->DefaultTaskRunner(),
        frame_sink_id, hidden, never_composited,
        /*is_for_child_local_root=*/false, is_for_nested_main_frame);
  }
  widget->BindLocalRoot(*main_frame);
  web_view_impl.SetMainFrameViewWidget(widget);
  return widget;
}

WebFrameWidget* WebFrameWidget::CreateForChildLocalRoot(
    WebWidgetClient* client,
    WebLocalFrame* local_root,
    CrossVariantMojoAssociatedRemote<mojom::blink::FrameWidgetHostInterfaceBase>
        mojo_frame_widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::FrameWidgetInterfaceBase>
        mojo_frame_widget,
    CrossVariantMojoAssociatedRemote<mojom::blink::WidgetHostInterfaceBase>
        mojo_widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::WidgetInterfaceBase>
        mojo_widget,
    const viz::FrameSinkId& frame_sink_id,
    bool hidden,
    bool never_composited) {
  DCHECK(client) << "A valid WebWidgetClient must be supplied.";
  DCHECK(local_root->Parent());  // This is not the main frame.
  // Frames whose direct ancestor is a remote frame are local roots. Verify this
  // is one. Other frames should be using the widget for their nearest local
  // root.
  DCHECK(local_root->Parent()->IsWebRemoteFrame());

  // Note: this isn't a leak, as the object has a self-reference that the
  // caller needs to release by calling Close().
  auto* widget = MakeGarbageCollected<WebFrameWidgetImpl>(
      base::PassKey<WebFrameWidget>(), *client,
      std::move(mojo_frame_widget_host), std::move(mojo_frame_widget),
      std::move(mojo_widget_host), std::move(mojo_widget),
      local_root->Scheduler()->GetAgentGroupScheduler()->DefaultTaskRunner(),
      frame_sink_id, hidden, never_composited, /*is_for_child_local_root=*/true,
      /*is_for_nested_main_frame=*/false);
  widget->BindLocalRoot(*local_root);
  return widget;
}

bool WebFrameWidgetImpl::ignore_input_events_ = false;

WebFrameWidgetImpl::WebFrameWidgetImpl(
    base::PassKey<WebFrameWidget>,
    WebWidgetClient& client,
    CrossVariantMojoAssociatedRemote<mojom::blink::FrameWidgetHostInterfaceBase>
        frame_widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::FrameWidgetInterfaceBase>
        frame_widget,
    CrossVariantMojoAssociatedRemote<mojom::blink::WidgetHostInterfaceBase>
        widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::WidgetInterfaceBase>
        widget,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const viz::FrameSinkId& frame_sink_id,
    bool hidden,
    bool never_composited,
    bool is_for_child_local_root,
    bool is_for_nested_main_frame)
    : widget_base_(std::make_unique<WidgetBase>(this,
                                                std::move(widget_host),
                                                std::move(widget),
                                                task_runner,
                                                hidden,
                                                never_composited,
                                                is_for_child_local_root)),
      client_(&client),
      frame_sink_id_(frame_sink_id),
      is_for_child_local_root_(is_for_child_local_root),
      self_keep_alive_(PERSISTENT_FROM_HERE, this) {
  DCHECK(task_runner);
  if (is_for_nested_main_frame)
    main_data().is_for_nested_main_frame = is_for_nested_main_frame;
  frame_widget_host_.Bind(std::move(frame_widget_host), task_runner);
  receiver_.Bind(std::move(frame_widget), task_runner);
}

WebFrameWidgetImpl::~WebFrameWidgetImpl() {
  // Ensure that Close is called and we aren't releasing |widget_base_| in the
  // destructor.
  // TODO(crbug.com/1139104): This CHECK can be changed to a DCHECK once
  // the issue is solved.
  CHECK(!widget_base_);
}

void WebFrameWidgetImpl::BindLocalRoot(WebLocalFrame& local_root) {
  local_root_ = To<WebLocalFrameImpl>(local_root);
  local_root_->SetFrameWidget(this);
}

bool WebFrameWidgetImpl::ForTopMostMainFrame() const {
  return ForMainFrame() && !main_data().is_for_nested_main_frame;
}

void WebFrameWidgetImpl::SetIsNestedMainFrameWidget(bool is_nested) {
  main_data().is_for_nested_main_frame = is_nested;
}

void WebFrameWidgetImpl::Close(
    scoped_refptr<base::SingleThreadTaskRunner> cleanup_runner) {
  LocalFrameView* frame_view;
  if (is_for_child_local_root_) {
    frame_view = LocalRootImpl()->GetFrame()->View();
  } else {
    // Scrolling for the root frame is special we need to pass null indicating
    // we are at the top of the tree when setting up the Animation. Which will
    // cause ownership of the timeline and animation host.
    // See ScrollingCoordinator::AnimationHostInitialized.
    frame_view = nullptr;
  }
  GetPage()->WillCloseAnimationHost(frame_view);

  if (ForMainFrame()) {
    // Closing the WebFrameWidgetImpl happens in response to the local main
    // frame being detached from the Page/WebViewImpl.
    View()->SetMainFrameViewWidget(nullptr);
  }

  mutator_dispatcher_ = nullptr;
  local_root_->SetFrameWidget(nullptr);
  local_root_ = nullptr;
  client_ = nullptr;
  widget_base_->Shutdown(std::move(cleanup_runner));
  widget_base_.reset();
  receiver_.reset();
  input_target_receiver_.reset();
  self_keep_alive_.Clear();
}

WebLocalFrame* WebFrameWidgetImpl::LocalRoot() const {
  return local_root_;
}

WebRect WebFrameWidgetImpl::ComputeBlockBound(
    const gfx::Point& point_in_root_frame,
    bool ignore_clipping) const {
  HitTestLocation location(local_root_->GetFrameView()->ConvertFromRootFrame(
      PhysicalOffset(IntPoint(point_in_root_frame))));
  HitTestRequest::HitTestRequestType hit_type =
      HitTestRequest::kReadOnly | HitTestRequest::kActive |
      (ignore_clipping ? HitTestRequest::kIgnoreClipping : 0);
  HitTestResult result =
      local_root_->GetFrame()->GetEventHandler().HitTestResultAtLocation(
          location, hit_type);
  result.SetToShadowHostIfInRestrictedShadowRoot();

  Node* node = result.InnerNodeOrImageMapImage();
  if (!node)
    return WebRect();

  // Find the block type node based on the hit node.
  // FIXME: This wants to walk flat tree with
  // LayoutTreeBuilderTraversal::parent().
  while (node &&
         (!node->GetLayoutObject() || node->GetLayoutObject()->IsInline()))
    node = LayoutTreeBuilderTraversal::Parent(*node);

  // Return the bounding box in the root frame's coordinate space.
  if (node) {
    IntRect absolute_rect = node->GetLayoutObject()->AbsoluteBoundingBoxRect();
    LocalFrame* frame = node->GetDocument().GetFrame();
    return frame->View()->ConvertToRootFrame(absolute_rect);
  }
  return WebRect();
}

void WebFrameWidgetImpl::DragTargetDragEnter(
    const WebDragData& web_drag_data,
    const gfx::PointF& point_in_viewport,
    const gfx::PointF& screen_point,
    DragOperationsMask operations_allowed,
    uint32_t key_modifiers,
    DragTargetDragEnterCallback callback) {
  DCHECK(!current_drag_data_);

  current_drag_data_ = DataObject::Create(web_drag_data);
  operations_allowed_ = operations_allowed;

  blink::DragOperation operation = DragTargetDragEnterOrOver(
      point_in_viewport, screen_point, kDragEnter, key_modifiers);
  std::move(callback).Run(operation);
}

void WebFrameWidgetImpl::DragTargetDragOver(
    const gfx::PointF& point_in_viewport,
    const gfx::PointF& screen_point,
    DragOperationsMask operations_allowed,
    uint32_t key_modifiers,
    DragTargetDragOverCallback callback) {
  operations_allowed_ = operations_allowed;

  blink::DragOperation operation = DragTargetDragEnterOrOver(
      point_in_viewport, screen_point, kDragOver, key_modifiers);
  std::move(callback).Run(operation);
}

void WebFrameWidgetImpl::DragTargetDragLeave(
    const gfx::PointF& point_in_viewport,
    const gfx::PointF& screen_point) {
  DCHECK(current_drag_data_);

  // TODO(paulmeyer): It shouldn't be possible for |current_drag_data_| to be
  // null here, but this is somehow happening (rarely). This suggests that in
  // some cases drag-leave is happening before drag-enter, which should be
  // impossible. This needs to be investigated further. Once fixed, the extra
  // check for |!current_drag_data_| should be removed. (crbug.com/671152)
  if (IgnoreInputEvents() || !current_drag_data_) {
    CancelDrag();
    return;
  }

  gfx::PointF point_in_root_frame(ViewportToRootFrame(point_in_viewport));
  DragData drag_data(current_drag_data_.Get(), FloatPoint(point_in_root_frame),
                     FloatPoint(screen_point), operations_allowed_);

  GetPage()->GetDragController().DragExited(&drag_data,
                                            *local_root_->GetFrame());

  // FIXME: why is the drag scroll timer not stopped here?

  drag_operation_ = kDragOperationNone;
  current_drag_data_ = nullptr;
}

void WebFrameWidgetImpl::DragTargetDrop(const WebDragData& web_drag_data,
                                        const gfx::PointF& point_in_viewport,
                                        const gfx::PointF& screen_point,
                                        uint32_t key_modifiers) {
  gfx::PointF point_in_root_frame(ViewportToRootFrame(point_in_viewport));

  DCHECK(current_drag_data_);
  current_drag_data_ = DataObject::Create(web_drag_data);

  // If this webview transitions from the "drop accepting" state to the "not
  // accepting" state, then our IPC message reply indicating that may be in-
  // flight, or else delayed by javascript processing in this webview.  If a
  // drop happens before our IPC reply has reached the browser process, then
  // the browser forwards the drop to this webview.  So only allow a drop to
  // proceed if our webview m_dragOperation state is not DragOperationNone.

  if (drag_operation_ == kDragOperationNone) {
    // IPC RACE CONDITION: do not allow this drop.
    DragTargetDragLeave(point_in_viewport, screen_point);
    return;
  }

  if (!IgnoreInputEvents()) {
    current_drag_data_->SetModifiers(key_modifiers);
    DragData drag_data(current_drag_data_.Get(),
                       FloatPoint(point_in_root_frame),
                       FloatPoint(screen_point), operations_allowed_);

    GetPage()->GetDragController().PerformDrag(&drag_data,
                                               *local_root_->GetFrame());
  }
  drag_operation_ = kDragOperationNone;
  current_drag_data_ = nullptr;
}

void WebFrameWidgetImpl::DragSourceEndedAt(const gfx::PointF& point_in_viewport,
                                           const gfx::PointF& screen_point,
                                           DragOperation operation) {
  if (!local_root_) {
    // We should figure out why |local_root_| could be nullptr
    // (https://crbug.com/792345).
    return;
  }

  if (IgnoreInputEvents()) {
    CancelDrag();
    return;
  }
  gfx::PointF point_in_root_frame(
      GetPage()->GetVisualViewport().ViewportToRootFrame(
          FloatPoint(point_in_viewport)));

  WebMouseEvent fake_mouse_move(
      WebInputEvent::Type::kMouseMove, point_in_root_frame, screen_point,
      WebPointerProperties::Button::kLeft, 0, WebInputEvent::kNoModifiers,
      base::TimeTicks::Now());
  fake_mouse_move.SetFrameScale(1);
  local_root_->GetFrame()->GetEventHandler().DragSourceEndedAt(fake_mouse_move,
                                                               operation);
}

void WebFrameWidgetImpl::DragSourceSystemDragEnded() {
  CancelDrag();
}

void WebFrameWidgetImpl::SetBackgroundOpaque(bool opaque) {
  if (opaque) {
    View()->ClearBaseBackgroundColorOverride();
    View()->ClearBackgroundColorOverride();
  } else {
    View()->SetBaseBackgroundColorOverride(SK_ColorTRANSPARENT);
    View()->SetBackgroundColorOverride(SK_ColorTRANSPARENT);
  }
}

void WebFrameWidgetImpl::SetTextDirection(base::i18n::TextDirection direction) {
  LocalFrame* focusedFrame = FocusedLocalFrameInWidget();
  if (focusedFrame)
    focusedFrame->SetTextDirection(direction);
}

void WebFrameWidgetImpl::SetInheritedEffectiveTouchActionForSubFrame(
    TouchAction touch_action) {
  DCHECK(ForSubframe());
  LocalRootImpl()->GetFrame()->SetInheritedEffectiveTouchAction(touch_action);
}

void WebFrameWidgetImpl::UpdateRenderThrottlingStatusForSubFrame(
    bool is_throttled,
    bool subtree_throttled) {
  DCHECK(ForSubframe());
  LocalRootImpl()->GetFrameView()->UpdateRenderThrottlingStatus(
      is_throttled, subtree_throttled, true);
}

#if defined(OS_MAC)
void WebFrameWidgetImpl::GetStringAtPoint(const gfx::Point& point_in_local_root,
                                          GetStringAtPointCallback callback) {
  gfx::Point baseline_point;
  ui::mojom::blink::AttributedStringPtr attributed_string = nullptr;
  NSAttributedString* string = SubstringUtil::AttributedWordAtPoint(
      this, point_in_local_root, baseline_point);
  if (string)
    attributed_string = ui::mojom::blink::AttributedString::From(string);

  std::move(callback).Run(std::move(attributed_string), baseline_point);
}
#endif

void WebFrameWidgetImpl::BindWidgetCompositor(
    mojo::PendingReceiver<mojom::blink::WidgetCompositor> receiver) {
  widget_base_->BindWidgetCompositor(std::move(receiver));
}

void WebFrameWidgetImpl::BindInputTargetClient(
    mojo::PendingReceiver<viz::mojom::blink::InputTargetClient> receiver) {
  DCHECK(!input_target_receiver_.is_bound());
  input_target_receiver_.Bind(
      std::move(receiver),
      local_root_->GetTaskRunner(TaskType::kInternalDefault));
}

void WebFrameWidgetImpl::FrameSinkIdAt(const gfx::PointF& point,
                                       const uint64_t trace_id,
                                       FrameSinkIdAtCallback callback) {
  TRACE_EVENT_WITH_FLOW1("viz,benchmark", "Event.Pipeline",
                         TRACE_ID_GLOBAL(trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "step", "FrameSinkIdAt");

  gfx::PointF local_point;
  viz::FrameSinkId id = GetFrameSinkIdAtPoint(point, &local_point);
  std::move(callback).Run(id, local_point);
}

viz::FrameSinkId WebFrameWidgetImpl::GetFrameSinkIdAtPoint(
    const gfx::PointF& point_in_dips,
    gfx::PointF* local_point_in_dips) {
  HitTestResult result =
      CoreHitTestResultAt(widget_base_->DIPsToBlinkSpace(point_in_dips));

  Node* result_node = result.InnerNode();
  *local_point_in_dips = gfx::PointF(point_in_dips);

  // TODO(crbug.com/797828): When the node is null the caller may
  // need to do extra checks. Like maybe update the layout and then
  // call the hit-testing API. Either way it might be better to have
  // a DCHECK for the node rather than a null check here.
  if (!result_node) {
    return frame_sink_id_;
  }

  viz::FrameSinkId remote_frame_sink_id = GetRemoteFrameSinkId(result);
  if (remote_frame_sink_id.is_valid()) {
    FloatPoint local_point = FloatPoint(result.LocalPoint());
    LayoutObject* object = result.GetLayoutObject();
    if (auto* box = DynamicTo<LayoutBox>(object))
      local_point.MoveBy(-FloatPoint(box->PhysicalContentBoxOffset()));

    *local_point_in_dips =
        widget_base_->BlinkSpaceToDIPs(gfx::PointF(local_point));
    return remote_frame_sink_id;
  }

  // Return the FrameSinkId for the current widget if the point did not hit
  // test to a remote frame, or the point is outside of the remote frame's
  // content box, or the remote frame doesn't have a valid FrameSinkId yet.
  return frame_sink_id_;
}

gfx::RectF WebFrameWidgetImpl::BlinkSpaceToDIPs(const gfx::RectF& rect) {
  return widget_base_->BlinkSpaceToDIPs(rect);
}

gfx::Rect WebFrameWidgetImpl::BlinkSpaceToEnclosedDIPs(const gfx::Rect& rect) {
  return widget_base_->BlinkSpaceToEnclosedDIPs(rect);
}

gfx::Size WebFrameWidgetImpl::BlinkSpaceToFlooredDIPs(const gfx::Size& size) {
  return widget_base_->BlinkSpaceToFlooredDIPs(size);
}

gfx::RectF WebFrameWidgetImpl::DIPsToBlinkSpace(const gfx::RectF& rect) {
  return widget_base_->DIPsToBlinkSpace(rect);
}

gfx::PointF WebFrameWidgetImpl::DIPsToBlinkSpace(const gfx::PointF& point) {
  return widget_base_->DIPsToBlinkSpace(point);
}

gfx::Point WebFrameWidgetImpl::DIPsToRoundedBlinkSpace(
    const gfx::Point& point) {
  return widget_base_->DIPsToRoundedBlinkSpace(point);
}

float WebFrameWidgetImpl::DIPsToBlinkSpace(float scalar) {
  return widget_base_->DIPsToBlinkSpace(scalar);
}

gfx::Size WebFrameWidgetImpl::DIPsToCeiledBlinkSpace(const gfx::Size& size) {
  return widget_base_->DIPsToCeiledBlinkSpace(size);
}

void WebFrameWidgetImpl::SetActive(bool active) {
  View()->SetIsActive(active);
}

WebInputEventResult WebFrameWidgetImpl::HandleKeyEvent(
    const WebKeyboardEvent& event) {
  DCHECK((event.GetType() == WebInputEvent::Type::kRawKeyDown) ||
         (event.GetType() == WebInputEvent::Type::kKeyDown) ||
         (event.GetType() == WebInputEvent::Type::kKeyUp));

  // Please refer to the comments explaining the m_suppressNextKeypressEvent
  // member.
  // The m_suppressNextKeypressEvent is set if the KeyDown is handled by
  // Webkit. A keyDown event is typically associated with a keyPress(char)
  // event and a keyUp event. We reset this flag here as this is a new keyDown
  // event.
  suppress_next_keypress_event_ = false;

  // If there is a popup open, it should be the one processing the event,
  // not the page.
  scoped_refptr<WebPagePopupImpl> page_popup = View()->GetPagePopup();
  if (page_popup) {
    page_popup->HandleKeyEvent(event);
    if (event.GetType() == WebInputEvent::Type::kRawKeyDown) {
      suppress_next_keypress_event_ = true;
    }
    return WebInputEventResult::kHandledSystem;
  }

  auto* frame = DynamicTo<LocalFrame>(FocusedCoreFrame());
  if (!frame)
    return WebInputEventResult::kNotHandled;

  WebInputEventResult result = frame->GetEventHandler().KeyEvent(event);
  if (result != WebInputEventResult::kNotHandled) {
    if (WebInputEvent::Type::kRawKeyDown == event.GetType()) {
      // Suppress the next keypress event unless the focused node is a plugin
      // node.  (Flash needs these keypress events to handle non-US keyboards.)
      Element* element = FocusedElement();
      if (element && element->GetLayoutObject() &&
          element->GetLayoutObject()->IsEmbeddedObject()) {
        if (event.windows_key_code == VKEY_TAB) {
          // If the plugin supports keyboard focus then we should not send a tab
          // keypress event.
          WebPluginContainerImpl* plugin_view =
              To<LayoutEmbeddedContent>(element->GetLayoutObject())->Plugin();
          if (plugin_view && plugin_view->SupportsKeyboardFocus()) {
            suppress_next_keypress_event_ = true;
          }
        }
      } else {
        suppress_next_keypress_event_ = true;
      }
    }
    return result;
  }

#if !defined(OS_MAC)
  const WebInputEvent::Type kContextMenuKeyTriggeringEventType =
#if defined(OS_WIN)
      WebInputEvent::Type::kKeyUp;
#else
      WebInputEvent::Type::kRawKeyDown;
#endif
  const WebInputEvent::Type kShiftF10TriggeringEventType =
      WebInputEvent::Type::kRawKeyDown;

  bool is_unmodified_menu_key =
      !(event.GetModifiers() & WebInputEvent::kInputModifiers) &&
      event.windows_key_code == VKEY_APPS;
  bool is_shift_f10 = (event.GetModifiers() & WebInputEvent::kInputModifiers) ==
                          WebInputEvent::kShiftKey &&
                      event.windows_key_code == VKEY_F10;
  if ((is_unmodified_menu_key &&
       event.GetType() == kContextMenuKeyTriggeringEventType) ||
      (is_shift_f10 && event.GetType() == kShiftF10TriggeringEventType)) {
    View()->SendContextMenuEvent();
    return WebInputEventResult::kHandledSystem;
  }
#endif  // !defined(OS_MAC)

  return WebInputEventResult::kNotHandled;
}

void WebFrameWidgetImpl::HandleMouseDown(LocalFrame& local_root,
                                         const WebMouseEvent& event) {
  WebViewImpl* view_impl = View();
  // If there is a popup open, close it as the user is clicking on the page
  // (outside of the popup). We also save it so we can prevent a click on an
  // element from immediately reopening the same popup.
  scoped_refptr<WebPagePopupImpl> page_popup;
  if (event.button == WebMouseEvent::Button::kLeft) {
    page_popup = view_impl->GetPagePopup();
    view_impl->CancelPagePopup();
  }

  // Take capture on a mouse down on a plugin so we can send it mouse events.
  // If the hit node is a plugin but a scrollbar is over it don't start mouse
  // capture because it will interfere with the scrollbar receiving events.
  PhysicalOffset point(LayoutUnit(event.PositionInWidget().x()),
                       LayoutUnit(event.PositionInWidget().y()));
  if (event.button == WebMouseEvent::Button::kLeft) {
    HitTestLocation location(
        LocalRootImpl()->GetFrameView()->ConvertFromRootFrame(point));
    HitTestResult result(
        LocalRootImpl()->GetFrame()->GetEventHandler().HitTestResultAtLocation(
            location));
    result.SetToShadowHostIfInRestrictedShadowRoot();
    Node* hit_node = result.InnerNode();
    auto* html_element = DynamicTo<HTMLElement>(hit_node);
    if (!result.GetScrollbar() && hit_node && hit_node->GetLayoutObject() &&
        hit_node->GetLayoutObject()->IsEmbeddedObject() && html_element &&
        html_element->IsPluginElement()) {
      mouse_capture_element_ = To<HTMLPlugInElement>(hit_node);
      SetMouseCapture(true);
      TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("input", "capturing mouse",
                                        TRACE_ID_LOCAL(this));
    }
  }

  PageWidgetEventHandler::HandleMouseDown(local_root, event);
  // PageWidgetEventHandler may have detached the frame.
  if (!LocalRootImpl())
    return;

  if (view_impl->GetPagePopup() && page_popup &&
      view_impl->GetPagePopup()->HasSamePopupClient(page_popup.get())) {
    // That click triggered a page popup that is the same as the one we just
    // closed.  It needs to be closed.
    view_impl->CancelPagePopup();
  }

  // Dispatch the contextmenu event regardless of if the click was swallowed.
  if (!GetPage()->GetSettings().GetShowContextMenuOnMouseUp()) {
#if defined(OS_MAC)
    if (event.button == WebMouseEvent::Button::kRight ||
        (event.button == WebMouseEvent::Button::kLeft &&
         event.GetModifiers() & WebMouseEvent::kControlKey))
      MouseContextMenu(event);
#else
    if (event.button == WebMouseEvent::Button::kRight)
      MouseContextMenu(event);
#endif
  }
}

void WebFrameWidgetImpl::HandleMouseLeave(LocalFrame& local_root,
                                          const WebMouseEvent& event) {
  View()->SetMouseOverURL(WebURL());
  PageWidgetEventHandler::HandleMouseLeave(local_root, event);
  // PageWidgetEventHandler may have detached the frame.
}

void WebFrameWidgetImpl::MouseContextMenu(const WebMouseEvent& event) {
  GetPage()->GetContextMenuController().ClearContextMenu();

  WebMouseEvent transformed_event =
      TransformWebMouseEvent(LocalRootImpl()->GetFrameView(), event);
  transformed_event.menu_source_type = kMenuSourceMouse;

  // Find the right target frame. See issue 1186900.
  HitTestResult result = HitTestResultForRootFramePos(
      FloatPoint(transformed_event.PositionInRootFrame()));
  Frame* target_frame;
  if (result.InnerNodeOrImageMapImage())
    target_frame = result.InnerNodeOrImageMapImage()->GetDocument().GetFrame();
  else
    target_frame = GetPage()->GetFocusController().FocusedOrMainFrame();

  // This will need to be changed to a nullptr check when focus control
  // is refactored, at which point focusedOrMainFrame will never return a
  // RemoteFrame.
  // See https://crbug.com/341918.
  LocalFrame* target_local_frame = DynamicTo<LocalFrame>(target_frame);
  if (!target_local_frame)
    return;

  {
    ContextMenuAllowedScope scope;
    target_local_frame->GetEventHandler().SendContextMenuEvent(
        transformed_event);
  }
  // Actually showing the context menu is handled by the ContextMenuClient
  // implementation...
}

WebInputEventResult WebFrameWidgetImpl::HandleMouseUp(
    LocalFrame& local_root,
    const WebMouseEvent& event) {
  WebInputEventResult result =
      PageWidgetEventHandler::HandleMouseUp(local_root, event);
  // PageWidgetEventHandler may have detached the frame.
  if (!LocalRootImpl())
    return result;

  if (GetPage()->GetSettings().GetShowContextMenuOnMouseUp()) {
    // Dispatch the contextmenu event regardless of if the click was swallowed.
    // On Mac/Linux, we handle it on mouse down, not up.
    if (event.button == WebMouseEvent::Button::kRight)
      MouseContextMenu(event);
  }
  return result;
}

WebInputEventResult WebFrameWidgetImpl::HandleGestureEvent(
    const WebGestureEvent& event) {
  WebInputEventResult event_result = WebInputEventResult::kNotHandled;

  // Fling events are not sent to the renderer.
  CHECK(event.GetType() != WebInputEvent::Type::kGestureFlingStart);
  CHECK(event.GetType() != WebInputEvent::Type::kGestureFlingCancel);

  WebViewImpl* web_view = View();

  LocalFrame* frame = LocalRootImpl()->GetFrame();
  WebGestureEvent scaled_event = TransformWebGestureEvent(frame->View(), event);

  // Special handling for double tap and scroll events as we don't want to
  // hit test for them.
  switch (event.GetType()) {
    case WebInputEvent::Type::kGestureDoubleTap:
      if (web_view->SettingsImpl()->DoubleTapToZoomEnabled() &&
          web_view->MinimumPageScaleFactor() !=
              web_view->MaximumPageScaleFactor()) {
        IntPoint pos_in_local_frame_root =
            FlooredIntPoint(scaled_event.PositionInRootFrame());
        auto block_bounds =
            gfx::Rect(ComputeBlockBound(pos_in_local_frame_root, false));

        if (ForMainFrame()) {
          web_view->AnimateDoubleTapZoom(pos_in_local_frame_root,
                                         WebRect(block_bounds));
        } else {
          // This sends the tap point and bounds to the main frame renderer via
          // the browser, where their coordinates will be transformed into the
          // main frame's coordinate space.
          GetAssociatedFrameWidgetHost()->AnimateDoubleTapZoomInMainFrame(
              pos_in_local_frame_root, block_bounds);
        }
      }
      event_result = WebInputEventResult::kHandledSystem;
      DidHandleGestureEvent(event);
      return event_result;
    case WebInputEvent::Type::kGestureScrollBegin:
    case WebInputEvent::Type::kGestureScrollEnd:
    case WebInputEvent::Type::kGestureScrollUpdate:
      // If we are getting any scroll toss close any page popup that is open.
      web_view->CancelPagePopup();

      // Scrolling-related gesture events invoke EventHandler recursively for
      // each frame down the chain, doing a single-frame hit-test per frame.
      // This matches handleWheelEvent.  Perhaps we could simplify things by
      // rewriting scroll handling to work inner frame out, and then unify with
      // other gesture events.
      event_result =
          frame->GetEventHandler().HandleGestureScrollEvent(scaled_event);
      DidHandleGestureEvent(event);
      return event_result;
    default:
      break;
  }

  // Hit test across all frames and do touch adjustment as necessary for the
  // event type.
  GestureEventWithHitTestResults targeted_event =
      frame->GetEventHandler().TargetGestureEvent(scaled_event);

  // Link highlight animations are only for the main frame.
  if (ForMainFrame()) {
    // Handle link highlighting outside the main switch to avoid getting lost in
    // the complicated set of cases handled below.
    switch (scaled_event.GetType()) {
      case WebInputEvent::Type::kGestureShowPress:
        // Queue a highlight animation, then hand off to regular handler.
        web_view->EnableTapHighlightAtPoint(targeted_event);
        break;
      case WebInputEvent::Type::kGestureTapCancel:
      case WebInputEvent::Type::kGestureTap:
      case WebInputEvent::Type::kGestureLongPress:
        GetPage()->GetLinkHighlight().StartHighlightAnimationIfNeeded();
        break;
      default:
        break;
    }
  }

  switch (scaled_event.GetType()) {
    case WebInputEvent::Type::kGestureTap: {
      {
        ContextMenuAllowedScope scope;
        event_result =
            frame->GetEventHandler().HandleGestureEvent(targeted_event);
      }

      if (web_view->GetPagePopup() && last_hidden_page_popup_ &&
          web_view->GetPagePopup()->HasSamePopupClient(
              last_hidden_page_popup_.get())) {
        // The tap triggered a page popup that is the same as the one we just
        // closed. It needs to be closed.
        web_view->CancelPagePopup();
      }
      // Don't have this value persist outside of a single tap gesture, plus
      // we're done with it now.
      last_hidden_page_popup_ = nullptr;
      break;
    }
    case WebInputEvent::Type::kGestureTwoFingerTap:
    case WebInputEvent::Type::kGestureLongPress:
    case WebInputEvent::Type::kGestureLongTap:
      if (scaled_event.GetType() == WebInputEvent::Type::kGestureLongTap) {
        if (LocalFrame* inner_frame =
                targeted_event.GetHitTestResult().InnerNodeFrame()) {
          if (!inner_frame->GetEventHandler().LongTapShouldInvokeContextMenu())
            break;
        } else if (!frame->GetEventHandler().LongTapShouldInvokeContextMenu()) {
          break;
        }
      }

      GetPage()->GetContextMenuController().ClearContextMenu();
      {
        ContextMenuAllowedScope scope;
        event_result =
            frame->GetEventHandler().HandleGestureEvent(targeted_event);
      }

      break;
    case WebInputEvent::Type::kGestureTapDown:
      // Touch pinch zoom and scroll on the page (outside of a popup) must hide
      // the popup. In case of a touch scroll or pinch zoom, this function is
      // called with GestureTapDown rather than a GSB/GSU/GSE or GPB/GPU/GPE.
      // When we close a popup because of a GestureTapDown, we also save it so
      // we can prevent the following GestureTap from immediately reopening the
      // same popup.
      // This value should not persist outside of a gesture, so is cleared by
      // GestureTap (where it is used) and by GestureCancel.
      last_hidden_page_popup_ = web_view->GetPagePopup();
      web_view->CancelPagePopup();
      event_result =
          frame->GetEventHandler().HandleGestureEvent(targeted_event);
      break;
    case WebInputEvent::Type::kGestureTapCancel:
      // Don't have this value persist outside of a single tap gesture.
      last_hidden_page_popup_ = nullptr;
      event_result =
          frame->GetEventHandler().HandleGestureEvent(targeted_event);
      break;
    case WebInputEvent::Type::kGestureShowPress:
    case WebInputEvent::Type::kGestureTapUnconfirmed:
      event_result =
          frame->GetEventHandler().HandleGestureEvent(targeted_event);
      break;
    default:
      NOTREACHED();
  }
  DidHandleGestureEvent(event);
  return event_result;
}

WebInputEventResult WebFrameWidgetImpl::HandleMouseWheel(
    LocalFrame& frame,
    const WebMouseWheelEvent& event) {
  View()->CancelPagePopup();
  return PageWidgetEventHandler::HandleMouseWheel(frame, event);
  // PageWidgetEventHandler may have detached the frame.
}

WebInputEventResult WebFrameWidgetImpl::HandleCharEvent(
    const WebKeyboardEvent& event) {
  DCHECK_EQ(event.GetType(), WebInputEvent::Type::kChar);

  // Please refer to the comments explaining the m_suppressNextKeypressEvent
  // member.  The m_suppressNextKeypressEvent is set if the KeyDown is
  // handled by Webkit. A keyDown event is typically associated with a
  // keyPress(char) event and a keyUp event. We reset this flag here as it
  // only applies to the current keyPress event.
  bool suppress = suppress_next_keypress_event_;
  suppress_next_keypress_event_ = false;

  // If there is a popup open, it should be the one processing the event,
  // not the page.
  scoped_refptr<WebPagePopupImpl> page_popup = View()->GetPagePopup();
  if (page_popup)
    return page_popup->HandleKeyEvent(event);

  LocalFrame* frame = To<LocalFrame>(FocusedCoreFrame());
  if (!frame) {
    return suppress ? WebInputEventResult::kHandledSuppressed
                    : WebInputEventResult::kNotHandled;
  }

  EventHandler& handler = frame->GetEventHandler();

  if (!event.IsCharacterKey())
    return WebInputEventResult::kHandledSuppressed;

  // Accesskeys are triggered by char events and can't be suppressed.
  // It is unclear whether a keypress should be dispatched as well
  // crbug.com/563507
  if (handler.HandleAccessKey(event))
    return WebInputEventResult::kHandledSystem;

  // Safari 3.1 does not pass off windows system key messages (WM_SYSCHAR) to
  // the eventHandler::keyEvent. We mimic this behavior on all platforms since
  // for now we are converting other platform's key events to windows key
  // events.
  if (event.is_system_key)
    return WebInputEventResult::kNotHandled;

  if (suppress)
    return WebInputEventResult::kHandledSuppressed;

  WebInputEventResult result = handler.KeyEvent(event);
  if (result != WebInputEventResult::kNotHandled)
    return result;

  return WebInputEventResult::kNotHandled;
}

void WebFrameWidgetImpl::CancelDrag() {
  // It's possible for this to be called while we're not doing a drag if
  // it's from a previous page that got unloaded.
  if (!doing_drag_and_drop_)
    return;
  GetPage()->GetDragController().DragEnded();
  doing_drag_and_drop_ = false;
}

void WebFrameWidgetImpl::StartDragging(const WebDragData& drag_data,
                                       DragOperationsMask operations_allowed,
                                       const SkBitmap& drag_image,
                                       const gfx::Point& drag_image_offset) {
  doing_drag_and_drop_ = true;
  if (Client()->InterceptStartDragging(drag_data, operations_allowed,
                                       drag_image, drag_image_offset)) {
    return;
  }

  gfx::Point offset_in_dips =
      widget_base_->BlinkSpaceToFlooredDIPs(drag_image_offset);
  GetAssociatedFrameWidgetHost()->StartDragging(
      drag_data, operations_allowed, drag_image,
      gfx::Vector2d(offset_in_dips.x(), offset_in_dips.y()),
      possible_drag_event_info_.Clone());
}

DragOperation WebFrameWidgetImpl::DragTargetDragEnterOrOver(
    const gfx::PointF& point_in_viewport,
    const gfx::PointF& screen_point,
    DragAction drag_action,
    uint32_t key_modifiers) {
  DCHECK(current_drag_data_);
  // TODO(paulmeyer): It shouldn't be possible for |m_currentDragData| to be
  // null here, but this is somehow happening (rarely). This suggests that in
  // some cases drag-over is happening before drag-enter, which should be
  // impossible. This needs to be investigated further. Once fixed, the extra
  // check for |!m_currentDragData| should be removed. (crbug.com/671504)
  if (IgnoreInputEvents() || !current_drag_data_) {
    CancelDrag();
    return kDragOperationNone;
  }

  FloatPoint point_in_root_frame(ViewportToRootFrame(point_in_viewport));

  current_drag_data_->SetModifiers(key_modifiers);
  DragData drag_data(current_drag_data_.Get(), FloatPoint(point_in_root_frame),
                     FloatPoint(screen_point), operations_allowed_);

  DragOperation drag_operation =
      GetPage()->GetDragController().DragEnteredOrUpdated(
          &drag_data, *local_root_->GetFrame());

  // Mask the drag operation against the drag source's allowed
  // operations.
  if (!(drag_operation & drag_data.DraggingSourceOperationMask()))
    drag_operation = kDragOperationNone;

  drag_operation_ = drag_operation;

  return drag_operation_;
}

void WebFrameWidgetImpl::SendOverscrollEventFromImplSide(
    const gfx::Vector2dF& overscroll_delta,
    cc::ElementId scroll_latched_element_id) {
  if (!RuntimeEnabledFeatures::OverscrollCustomizationEnabled())
    return;

  Node* target_node = View()->FindNodeFromScrollableCompositorElementId(
      scroll_latched_element_id);
  if (target_node) {
    target_node->GetDocument().EnqueueOverscrollEventForNode(
        target_node, overscroll_delta.x(), overscroll_delta.y());
  }
}

void WebFrameWidgetImpl::SendScrollEndEventFromImplSide(
    cc::ElementId scroll_latched_element_id) {
  if (!RuntimeEnabledFeatures::OverscrollCustomizationEnabled())
    return;

  Node* target_node = View()->FindNodeFromScrollableCompositorElementId(
      scroll_latched_element_id);
  if (target_node)
    target_node->GetDocument().EnqueueScrollEndEventForNode(target_node);
}

WebInputMethodController*
WebFrameWidgetImpl::GetActiveWebInputMethodController() const {
  WebLocalFrameImpl* local_frame =
      WebLocalFrameImpl::FromFrame(FocusedLocalFrameInWidget());
  return local_frame ? local_frame->GetInputMethodController() : nullptr;
}

gfx::PointF WebFrameWidgetImpl::ViewportToRootFrame(
    const gfx::PointF& point_in_viewport) const {
  return GetPage()->GetVisualViewport().ViewportToRootFrame(
      FloatPoint(point_in_viewport));
}

WebViewImpl* WebFrameWidgetImpl::View() const {
  return local_root_->ViewImpl();
}

Page* WebFrameWidgetImpl::GetPage() const {
  return View()->GetPage();
}

mojom::blink::FrameWidgetHost*
WebFrameWidgetImpl::GetAssociatedFrameWidgetHost() const {
  return frame_widget_host_.get();
}

void WebFrameWidgetImpl::RequestDecode(
    const PaintImage& image,
    base::OnceCallback<void(bool)> callback) {
  widget_base_->LayerTreeHost()->QueueImageDecode(image, std::move(callback));

  // In web tests the request does not actually cause a commit, because the
  // compositor is scheduled by the test runner to avoid flakiness. So for this
  // case we must request a main frame.
  Client()->ScheduleAnimationForWebTests();
}

void WebFrameWidgetImpl::Trace(Visitor* visitor) const {
  visitor->Trace(local_root_);
  visitor->Trace(current_drag_data_);
  visitor->Trace(frame_widget_host_);
  visitor->Trace(receiver_);
  visitor->Trace(input_target_receiver_);
  visitor->Trace(mouse_capture_element_);
  visitor->Trace(device_emulator_);
}

void WebFrameWidgetImpl::SetNeedsRecalculateRasterScales() {
  if (!View()->does_composite())
    return;
  widget_base_->LayerTreeHost()->SetNeedsRecalculateRasterScales();
}

void WebFrameWidgetImpl::SetBackgroundColor(SkColor color) {
  if (!View()->does_composite())
    return;
  widget_base_->LayerTreeHost()->set_background_color(color);
}

void WebFrameWidgetImpl::SetOverscrollBehavior(
    const cc::OverscrollBehavior& overscroll_behavior) {
  if (!View()->does_composite())
    return;
  widget_base_->LayerTreeHost()->SetOverscrollBehavior(overscroll_behavior);
}

void WebFrameWidgetImpl::RegisterSelection(cc::LayerSelection selection) {
  if (!View()->does_composite())
    return;
  widget_base_->LayerTreeHost()->RegisterSelection(selection);
}

void WebFrameWidgetImpl::StartPageScaleAnimation(
    const gfx::Vector2d& destination,
    bool use_anchor,
    float new_page_scale,
    base::TimeDelta duration) {
  widget_base_->LayerTreeHost()->StartPageScaleAnimation(
      destination, use_anchor, new_page_scale, duration);
}

void WebFrameWidgetImpl::RequestBeginMainFrameNotExpected(bool request) {
  if (!View()->does_composite())
    return;
  widget_base_->LayerTreeHost()->RequestBeginMainFrameNotExpected(request);
}

void WebFrameWidgetImpl::DidCommitAndDrawCompositorFrame() {
  ForEachLocalFrameControlledByWidget(
      local_root_->GetFrame(),
      WTF::BindRepeating([](WebLocalFrame* local_frame) {
        local_frame->Client()->DidCommitAndDrawCompositorFrame();
      }));
}

void WebFrameWidgetImpl::DidObserveFirstScrollDelay(
    base::TimeDelta first_scroll_delay,
    base::TimeTicks first_scroll_timestamp) {
  if (!local_root_ || !(local_root_->GetFrame()) ||
      !(local_root_->GetFrame()->GetDocument())) {
    return;
  }
  InteractiveDetector* interactive_detector =
      InteractiveDetector::From(*(local_root_->GetFrame()->GetDocument()));
  if (interactive_detector) {
    interactive_detector->DidObserveFirstScrollDelay(first_scroll_delay,
                                                     first_scroll_timestamp);
  }
}

std::unique_ptr<cc::LayerTreeFrameSink>
WebFrameWidgetImpl::AllocateNewLayerTreeFrameSink() {
  return Client()->AllocateNewLayerTreeFrameSink();
}

void WebFrameWidgetImpl::DidBeginMainFrame() {
  Client()->DidBeginMainFrame();
  DCHECK(LocalRootImpl()->GetFrame());
  PageWidgetDelegate::DidBeginFrame(*LocalRootImpl()->GetFrame());
}

void WebFrameWidgetImpl::UpdateLifecycle(WebLifecycleUpdate requested_update,
                                         DocumentUpdateReason reason) {
  TRACE_EVENT0("blink", "WebFrameWidgetImpl::UpdateLifecycle");
  if (!LocalRootImpl())
    return;

  PageWidgetDelegate::UpdateLifecycle(*GetPage(), *LocalRootImpl()->GetFrame(),
                                      requested_update, reason);
  if (requested_update != WebLifecycleUpdate::kAll)
    return;

  View()->UpdatePagePopup();

  // Meaningful layout events and background colors only apply to main frames.
  if (ForMainFrame()) {
    MainFrameData& data = main_data();

    // There is no background color for non-composited WebViews (eg
    // printing).
    if (View()->does_composite()) {
      SkColor background_color = View()->BackgroundColor();
      SetBackgroundColor(background_color);
      if (background_color != data.last_background_color) {
        LocalRootImpl()->GetFrame()->DidChangeBackgroundColor(
            background_color, false /* color_adjust */);
        data.last_background_color = background_color;
      }
    }

    if (LocalFrameView* view = LocalRootImpl()->GetFrameView()) {
      LocalFrame* frame = LocalRootImpl()->GetFrame();

      if (data.should_dispatch_first_visually_non_empty_layout &&
          view->IsVisuallyNonEmpty()) {
        data.should_dispatch_first_visually_non_empty_layout = false;
        // TODO(esprehn): Move users of this callback to something
        // better, the heuristic for "visually non-empty" is bad.
        DidMeaningfulLayout(WebMeaningfulLayout::kVisuallyNonEmpty);
      }

      if (data.should_dispatch_first_layout_after_finished_parsing &&
          frame->GetDocument()->HasFinishedParsing()) {
        data.should_dispatch_first_layout_after_finished_parsing = false;
        DidMeaningfulLayout(WebMeaningfulLayout::kFinishedParsing);
      }

      if (data.should_dispatch_first_layout_after_finished_loading &&
          frame->GetDocument()->IsLoadCompleted()) {
        data.should_dispatch_first_layout_after_finished_loading = false;
        DidMeaningfulLayout(WebMeaningfulLayout::kFinishedLoading);
      }
    }
  }
}

void WebFrameWidgetImpl::WillBeginMainFrame() {
  Client()->WillBeginMainFrame();
}

void WebFrameWidgetImpl::DidCompletePageScaleAnimation() {
  // Page scale animations only happen on the main frame.
  DCHECK(ForMainFrame());
  if (auto* focused_frame = View()->FocusedFrame()) {
    if (focused_frame->AutofillClient())
      focused_frame->AutofillClient()->DidCompleteFocusChangeInFrame();
  }
}

void WebFrameWidgetImpl::ScheduleAnimation() {
  Client()->ScheduleAnimation();
}

void WebFrameWidgetImpl::FocusChanged(bool enable) {
  // TODO(crbug.com/689777): FocusChange events are only sent to the MainFrame
  // these maybe should goto the local root so that the rest of input messages
  // sent to those are preserved in order.
  DCHECK(ForMainFrame());
  View()->SetPageFocus(enable);
}

bool WebFrameWidgetImpl::ShouldAckSyntheticInputImmediately() {
  // TODO(bokan): The RequestPresentation API appears not to function in VR. As
  // a short term workaround for https://crbug.com/940063, ACK input
  // immediately rather than using RequestPresentation.
  if (GetPage()->GetSettings().GetImmersiveModeEnabled())
    return true;
  return false;
}

void WebFrameWidgetImpl::UpdateVisualProperties(
    const VisualProperties& visual_properties) {
  SetZoomLevel(visual_properties.zoom_level);

  // TODO(danakj): In order to synchronize updates between local roots, the
  // display mode should be propagated to RenderFrameProxies and down through
  // their RenderWidgetHosts to child WebFrameWidgetImpl via the
  // VisualProperties waterfall, instead of coming to each WebFrameWidgetImpl
  // independently.
  // https://developer.mozilla.org/en-US/docs/Web/CSS/@media/display-mode
  SetDisplayMode(visual_properties.display_mode);

  if (ForMainFrame()) {
    SetAutoResizeMode(visual_properties.auto_resize_enabled,
                      visual_properties.min_size_for_auto_resize,
                      visual_properties.max_size_for_auto_resize,
                      visual_properties.screen_info.device_scale_factor);
  }

  bool capture_sequence_number_changed =
      visual_properties.capture_sequence_number !=
      last_capture_sequence_number_;
  if (capture_sequence_number_changed) {
    last_capture_sequence_number_ = visual_properties.capture_sequence_number;

    // Send the capture sequence number to RemoteFrames that are below the
    // local root for this widget.
    ForEachRemoteFrameControlledByWidget(WTF::BindRepeating(
        [](uint32_t capture_sequence_number, RemoteFrame* remote_frame) {
          remote_frame->Client()->UpdateCaptureSequenceNumber(
              capture_sequence_number);
        },
        visual_properties.capture_sequence_number));
  }

  if (!View()->AutoResizeMode()) {
    if (visual_properties.is_fullscreen_granted != is_fullscreen_granted_) {
      is_fullscreen_granted_ = visual_properties.is_fullscreen_granted;
      if (is_fullscreen_granted_)
        View()->DidEnterFullscreen();
      else
        View()->DidExitFullscreen();
    }
  }

  gfx::Size old_visible_viewport_size_in_dips =
      widget_base_->VisibleViewportSizeInDIPs();
  ApplyVisualPropertiesSizing(visual_properties);

  if (old_visible_viewport_size_in_dips !=
      widget_base_->VisibleViewportSizeInDIPs()) {
    ForEachLocalFrameControlledByWidget(
        local_root_->GetFrame(),
        WTF::BindRepeating([](WebLocalFrame* local_frame) {
          local_frame->Client()->ResetHasScrolledFocusedEditableIntoView();
        }));

    // Propagate changes down to child local root RenderWidgets and
    // BrowserPlugins in other frame trees/processes.
    ForEachRemoteFrameControlledByWidget(WTF::BindRepeating(
        [](const gfx::Size& visible_viewport_size, RemoteFrame* remote_frame) {
          remote_frame->Client()->DidChangeVisibleViewportSize(
              visible_viewport_size);
        },
        widget_base_->VisibleViewportSizeInDIPs()));
  }

  // All non-top-level Widgets (child local-root frames, Portals, GuestViews,
  // etc.) propagate and consume the page scale factor as "external", meaning
  // that it comes from the top level widget's page scale.
  if (!ForTopMostMainFrame()) {
    // The main frame controls the page scale factor, from blink. For other
    // frame widgets, the page scale is received from its parent as part of
    // the visual properties here. While blink doesn't need to know this
    // page scale factor outside the main frame, the compositor does in
    // order to produce its output at the correct scale.
    widget_base_->LayerTreeHost()->SetExternalPageScaleFactor(
        visual_properties.page_scale_factor,
        visual_properties.is_pinch_gesture_active);

    NotifyPageScaleFactorChanged(visual_properties.page_scale_factor,
                                 visual_properties.is_pinch_gesture_active);
  } else {
    // Ensure the external scale factor in top-level widgets is reset as it may
    // be leftover from when a widget was nested and was promoted to top level
    // (e.g. portal activation).
    widget_base_->LayerTreeHost()->SetExternalPageScaleFactor(
        1.f,
        /*is_pinch_gesture_active=*/false);
  }

  // TODO(crbug.com/939118): ScrollFocusedNodeIntoViewForWidget does not work
  // when the focused node is inside an OOPIF. This code path where
  // scroll_focused_node_into_view is set is used only for WebView, crbug
  // 939118 tracks fixing webviews to not use scroll_focused_node_into_view.
  if (visual_properties.scroll_focused_node_into_view)
    ScrollFocusedEditableElementIntoView();
}

void WebFrameWidgetImpl::ApplyVisualPropertiesSizing(
    const VisualProperties& visual_properties) {
  gfx::Rect new_compositor_viewport_pixel_rect =
      visual_properties.compositor_viewport_pixel_rect;
  if (ForMainFrame()) {
    if (size_ !=
        widget_base_->DIPsToCeiledBlinkSpace(visual_properties.new_size)) {
      // Only hide popups when the size changes. Eg https://crbug.com/761908.
      View()->CancelPagePopup();
    }

    if (auto* device_emulator = DeviceEmulator()) {
      device_emulator->UpdateVisualProperties(visual_properties);
      return;
    }

    if (AutoResizeMode()) {
      new_compositor_viewport_pixel_rect = gfx::Rect(gfx::ScaleToCeiledSize(
          widget_base_->BlinkSpaceToFlooredDIPs(size_.value_or(gfx::Size())),
          visual_properties.screen_info.device_scale_factor));
    }
  }

  SetWindowSegments(visual_properties.root_widget_window_segments);

  widget_base_->UpdateSurfaceAndScreenInfo(
      visual_properties.local_surface_id.value_or(viz::LocalSurfaceId()),
      new_compositor_viewport_pixel_rect, visual_properties.screen_info);

  // Store this even when auto-resizing, it is the size of the full viewport
  // used for clipping, and this value is propagated down the Widget
  // hierarchy via the VisualProperties waterfall.
  widget_base_->SetVisibleViewportSizeInDIPs(
      visual_properties.visible_viewport_size);

  if (ForMainFrame()) {
    if (!AutoResizeMode()) {
      size_ = widget_base_->DIPsToCeiledBlinkSpace(visual_properties.new_size);

      View()->ResizeWithBrowserControls(
          size_.value(),
          widget_base_->DIPsToCeiledBlinkSpace(
              widget_base_->VisibleViewportSizeInDIPs()),
          visual_properties.browser_controls_params);
    }
  } else {
    // Widgets in a WebView's frame tree without a local main frame
    // set the size of the WebView to be the |visible_viewport_size|, in order
    // to limit compositing in (out of process) child frames to what is visible.
    //
    // Note that child frames in the same process/WebView frame tree as the
    // main frame do not do this in order to not clobber the source of truth in
    // the main frame.
    if (!View()->MainFrameImpl()) {
      View()->Resize(widget_base_->DIPsToCeiledBlinkSpace(
          widget_base_->VisibleViewportSizeInDIPs()));
    }

    Resize(widget_base_->DIPsToCeiledBlinkSpace(visual_properties.new_size));
  }
}

void WebFrameWidgetImpl::ScheduleAnimationForWebTests() {
  Client()->ScheduleAnimationForWebTests();
}

int WebFrameWidgetImpl::GetLayerTreeId() {
  if (!View()->does_composite())
    return 0;
  return widget_base_->LayerTreeHost()->GetId();
}

void WebFrameWidgetImpl::SetHaveScrollEventHandlers(bool has_handlers) {
  widget_base_->LayerTreeHost()->SetHaveScrollEventHandlers(has_handlers);
}

void WebFrameWidgetImpl::SetEventListenerProperties(
    cc::EventListenerClass listener_class,
    cc::EventListenerProperties listener_properties) {
  widget_base_->LayerTreeHost()->SetEventListenerProperties(
      listener_class, listener_properties);

  if (listener_class == cc::EventListenerClass::kTouchStartOrMove ||
      listener_class == cc::EventListenerClass::kTouchEndOrCancel) {
    bool has_touch_handlers =
        EventListenerProperties(cc::EventListenerClass::kTouchStartOrMove) !=
            cc::EventListenerProperties::kNone ||
        EventListenerProperties(cc::EventListenerClass::kTouchEndOrCancel) !=
            cc::EventListenerProperties::kNone;
    if (!has_touch_handlers_ || *has_touch_handlers_ != has_touch_handlers) {
      has_touch_handlers_ = has_touch_handlers;

      // Can be null when running tests.
      if (auto* scheduler_state = widget_base_->RendererWidgetSchedulingState())
        scheduler_state->SetHasTouchHandler(has_touch_handlers);
      // Set touch event consumers based on whether there are touch event
      // handlers or the page has hit testable scrollbars.
      auto touch_event_consumers = mojom::blink::TouchEventConsumers::New(
          has_touch_handlers, GetPage()->GetScrollbarTheme().AllowsHitTest());
      frame_widget_host_->SetHasTouchEventConsumers(
          std::move(touch_event_consumers));
    }
  } else if (listener_class == cc::EventListenerClass::kPointerRawUpdate) {
    SetHasPointerRawUpdateEventHandlers(listener_properties !=
                                        cc::EventListenerProperties::kNone);
  }
}

cc::EventListenerProperties WebFrameWidgetImpl::EventListenerProperties(
    cc::EventListenerClass listener_class) const {
  return widget_base_->LayerTreeHost()->event_listener_properties(
      listener_class);
}

mojom::blink::DisplayMode WebFrameWidgetImpl::DisplayMode() const {
  return display_mode_;
}

const WebVector<gfx::Rect>& WebFrameWidgetImpl::WindowSegments() const {
  return window_segments_;
}

void WebFrameWidgetImpl::StartDeferringCommits(base::TimeDelta timeout) {
  if (!View()->does_composite())
    return;
  widget_base_->LayerTreeHost()->StartDeferringCommits(timeout);
}

void WebFrameWidgetImpl::StopDeferringCommits(
    cc::PaintHoldingCommitTrigger triggger) {
  if (!View()->does_composite())
    return;
  widget_base_->LayerTreeHost()->StopDeferringCommits(triggger);
}

std::unique_ptr<cc::ScopedDeferMainFrameUpdate>
WebFrameWidgetImpl::DeferMainFrameUpdate() {
  return widget_base_->LayerTreeHost()->DeferMainFrameUpdate();
}

void WebFrameWidgetImpl::SetBrowserControlsShownRatio(float top_ratio,
                                                      float bottom_ratio) {
  widget_base_->LayerTreeHost()->SetBrowserControlsShownRatio(top_ratio,
                                                              bottom_ratio);
}

void WebFrameWidgetImpl::SetBrowserControlsParams(
    cc::BrowserControlsParams params) {
  widget_base_->LayerTreeHost()->SetBrowserControlsParams(params);
}

cc::LayerTreeDebugState WebFrameWidgetImpl::GetLayerTreeDebugState() {
  return widget_base_->LayerTreeHost()->GetDebugState();
}

void WebFrameWidgetImpl::SetLayerTreeDebugState(
    const cc::LayerTreeDebugState& state) {
  widget_base_->LayerTreeHost()->SetDebugState(state);
}

void WebFrameWidgetImpl::SynchronouslyCompositeForTesting(
    base::TimeTicks frame_time) {
  widget_base_->LayerTreeHost()->CompositeForTest(frame_time, false);
}

void WebFrameWidgetImpl::UseSynchronousResizeModeForTesting(bool enable) {
  main_data().synchronous_resize_mode_for_testing = enable;
}

void WebFrameWidgetImpl::SetDeviceColorSpaceForTesting(
    const gfx::ColorSpace& color_space) {
  DCHECK(ForMainFrame());
  // We are changing the device color space from the renderer, so allocate a
  // new viz::LocalSurfaceId to avoid surface invariants violations in tests.
  widget_base_->LayerTreeHost()->RequestNewLocalSurfaceId();

  blink::ScreenInfo info = widget_base_->GetScreenInfo();
  info.display_color_spaces = gfx::DisplayColorSpaces(color_space);
  widget_base_->UpdateScreenInfo(info);
}

// TODO(665924): Remove direct dispatches of mouse events from
// PointerLockController, instead passing them through EventHandler.
void WebFrameWidgetImpl::PointerLockMouseEvent(
    const WebCoalescedInputEvent& coalesced_event) {
  const WebInputEvent& input_event = coalesced_event.Event();
  const WebMouseEvent& mouse_event =
      static_cast<const WebMouseEvent&>(input_event);
  WebMouseEvent transformed_event =
      TransformWebMouseEvent(local_root_->GetFrameView(), mouse_event);

  AtomicString event_type;
  switch (input_event.GetType()) {
    case WebInputEvent::Type::kMouseDown:
      event_type = event_type_names::kMousedown;
      if (!GetPage() || !GetPage()->GetPointerLockController().GetElement())
        break;
      LocalFrame::NotifyUserActivation(
          GetPage()
              ->GetPointerLockController()
              .GetElement()
              ->GetDocument()
              .GetFrame(),
          mojom::blink::UserActivationNotificationType::kInteraction);
      break;
    case WebInputEvent::Type::kMouseUp:
      event_type = event_type_names::kMouseup;
      break;
    case WebInputEvent::Type::kMouseMove:
      event_type = event_type_names::kMousemove;
      break;
    default:
      NOTREACHED() << input_event.GetType();
  }

  if (GetPage()) {
    GetPage()->GetPointerLockController().DispatchLockedMouseEvent(
        transformed_event,
        TransformWebMouseEventVector(
            local_root_->GetFrameView(),
            coalesced_event.GetCoalescedEventsPointers()),
        TransformWebMouseEventVector(
            local_root_->GetFrameView(),
            coalesced_event.GetPredictedEventsPointers()),
        event_type);
  }
}
bool WebFrameWidgetImpl::IsPointerLocked() {
  if (GetPage()) {
    return GetPage()->GetPointerLockController().IsPointerLocked();
  }
  return false;
}

void WebFrameWidgetImpl::ShowContextMenu(
    ui::mojom::blink::MenuSourceType source_type,
    const gfx::Point& location) {
  host_context_menu_location_ = location;

  if (!GetPage())
    return;
  GetPage()->GetContextMenuController().ClearContextMenu();
  {
    ContextMenuAllowedScope scope;
    if (LocalFrame* focused_frame =
            GetPage()->GetFocusController().FocusedFrame()) {
      focused_frame->GetEventHandler().ShowNonLocatedContextMenu(
          nullptr, static_cast<blink::WebMenuSourceType>(source_type));
    }
  }
  host_context_menu_location_.reset();
}

void WebFrameWidgetImpl::SetViewportIntersection(
    mojom::blink::ViewportIntersectionStatePtr intersection_state) {
  // Remote viewports are only applicable to local frames with remote ancestors.
  // TODO(https://crbug.com/1148960): Should this deal with portals?
  DCHECK(ForSubframe());

  child_data().compositor_visible_rect =
      intersection_state->compositor_visible_rect;
  widget_base_->LayerTreeHost()->SetViewportVisibleRect(
      intersection_state->compositor_visible_rect);
  LocalRootImpl()->GetFrame()->SetViewportIntersectionFromParent(
      *intersection_state);
}

void WebFrameWidgetImpl::EnableDeviceEmulation(
    const DeviceEmulationParams& parameters) {
  // Device Emaulation is only supported for the main frame.
  DCHECK(ForMainFrame());
  if (!device_emulator_) {
    gfx::Size size_in_dips = widget_base_->BlinkSpaceToFlooredDIPs(Size());

    device_emulator_ = MakeGarbageCollected<ScreenMetricsEmulator>(
        this, widget_base_->GetScreenInfo(), size_in_dips,
        widget_base_->VisibleViewportSizeInDIPs(),
        widget_base_->WidgetScreenRect(), widget_base_->WindowScreenRect());
  }
  device_emulator_->ChangeEmulationParams(parameters);
}

void WebFrameWidgetImpl::DisableDeviceEmulation() {
  if (!device_emulator_)
    return;
  device_emulator_->DisableAndApply();
  device_emulator_ = nullptr;
}

void WebFrameWidgetImpl::SetIsInertForSubFrame(bool inert) {
  DCHECK(ForSubframe());
  LocalRootImpl()->GetFrame()->SetIsInert(inert);
}

base::Optional<gfx::Point>
WebFrameWidgetImpl::GetAndResetContextMenuLocation() {
  return std::move(host_context_menu_location_);
}

void WebFrameWidgetImpl::SetZoomLevel(double zoom_level) {
  // Override the zoom level with the testing one if necessary.
  if (zoom_level_for_testing_ != -INFINITY)
    zoom_level = zoom_level_for_testing_;

  View()->SetZoomLevel(zoom_level);

  // Part of the UpdateVisualProperties dance we send the zoom level to
  // RemoteFrames that are below the local root for this widget.
  ForEachRemoteFrameControlledByWidget(WTF::BindRepeating(
      [](double zoom_level, RemoteFrame* remote_frame) {
        remote_frame->Client()->ZoomLevelChanged(zoom_level);
      },
      zoom_level));
}

void WebFrameWidgetImpl::SetAutoResizeMode(bool auto_resize,
                                           const gfx::Size& min_window_size,
                                           const gfx::Size& max_window_size,
                                           float device_scale_factor) {
  // Auto resize only applies to main frames.
  DCHECK(ForMainFrame());

  if (auto_resize) {
    if (!Platform::Current()->IsUseZoomForDSFEnabled())
      device_scale_factor = 1.f;
    View()->EnableAutoResizeMode(
        gfx::ScaleToCeiledSize(min_window_size, device_scale_factor),
        gfx::ScaleToCeiledSize(max_window_size, device_scale_factor));
  } else if (AutoResizeMode()) {
    View()->DisableAutoResizeMode();
  }
}

void WebFrameWidgetImpl::DidAutoResize(const gfx::Size& size) {
  DCHECK(ForMainFrame());
  gfx::Size size_in_dips = widget_base_->BlinkSpaceToFlooredDIPs(size);
  size_ = size;

  if (main_data().synchronous_resize_mode_for_testing) {
    gfx::Rect new_pos(widget_base_->WindowRect());
    new_pos.set_size(size_in_dips);
    SetScreenRects(new_pos, new_pos);
  }

  // TODO(ccameron): Note that this destroys any information differentiating
  // |size| from the compositor's viewport size.
  gfx::Rect size_with_dsf = gfx::Rect(gfx::ScaleToCeiledSize(
      gfx::Rect(size_in_dips).size(),
      widget_base_->GetScreenInfo().device_scale_factor));
  widget_base_->LayerTreeHost()->RequestNewLocalSurfaceId();
  widget_base_->UpdateCompositorViewportRect(size_with_dsf);
}

LocalFrame* WebFrameWidgetImpl::FocusedLocalFrameInWidget() const {
  if (!local_root_) {
    // WebFrameWidget is created in the call to CreateFrame. The corresponding
    // RenderWidget, however, might not swap in right away (InstallNewDocument()
    // will lead to it swapping in). During this interval local_root_ is nullptr
    // (see https://crbug.com/792345).
    return nullptr;
  }

  LocalFrame* frame = GetPage()->GetFocusController().FocusedFrame();
  return (frame && frame->LocalFrameRoot() == local_root_->GetFrame())
             ? frame
             : nullptr;
}

WebLocalFrame* WebFrameWidgetImpl::FocusedWebLocalFrameInWidget() const {
  return WebLocalFrameImpl::FromFrame(FocusedLocalFrameInWidget());
}

bool WebFrameWidgetImpl::ScrollFocusedEditableElementIntoView() {
  Element* element = FocusedElement();
  if (!element || !WebElement(element).IsEditable())
    return false;

  element->GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kSelection);

  if (!element->GetLayoutObject())
    return false;

  PhysicalRect rect_to_scroll;
  auto params =
      GetScrollParamsForFocusedEditableElement(*element, rect_to_scroll);
  element->GetLayoutObject()->ScrollRectToVisible(rect_to_scroll,
                                                  std::move(params));

  // Second phase for main frames is to schedule a zoom animation.
  if (ForMainFrame()) {
    LocalFrameView* main_frame_view = LocalRootImpl()->GetFrame()->View();

    View()->ZoomAndScrollToFocusedEditableElementRect(
        main_frame_view->RootFrameToDocument(
            element->GetDocument().View()->ConvertToRootFrame(
                element->GetLayoutObject()->AbsoluteBoundingBoxRect())),
        main_frame_view->RootFrameToDocument(
            element->GetDocument().View()->ConvertToRootFrame(
                element->GetDocument()
                    .GetFrame()
                    ->Selection()
                    .ComputeRectToScroll(kDoNotRevealExtent))),
        View()->ShouldZoomToLegibleScale(*element));
  }

  return true;
}

void WebFrameWidgetImpl::ResetMeaningfulLayoutStateForMainFrame() {
  MainFrameData& data = main_data();
  data.should_dispatch_first_visually_non_empty_layout = true;
  data.should_dispatch_first_layout_after_finished_parsing = true;
  data.should_dispatch_first_layout_after_finished_loading = true;
}

cc::LayerTreeHost* WebFrameWidgetImpl::InitializeCompositing(
    scheduler::WebThreadScheduler* main_thread_scheduler,
    cc::TaskGraphRunner* task_graph_runner,
    const ScreenInfo& screen_info,
    std::unique_ptr<cc::UkmRecorderFactory> ukm_recorder_factory,
    const cc::LayerTreeSettings* settings) {
  widget_base_->InitializeCompositing(
      main_thread_scheduler, task_graph_runner, is_for_child_local_root_,
      screen_info, std::move(ukm_recorder_factory), settings);

  LocalFrameView* frame_view;
  if (is_for_child_local_root_) {
    frame_view = LocalRootImpl()->GetFrame()->View();
  } else {
    // Scrolling for the root frame is special we need to pass null indicating
    // we are at the top of the tree when setting up the Animation. Which will
    // cause ownership of the timeline and animation host.
    // See ScrollingCoordinator::AnimationHostInitialized.
    frame_view = nullptr;
  }

  GetPage()->AnimationHostInitialized(*AnimationHost(), frame_view);
  return widget_base_->LayerTreeHost();
}

void WebFrameWidgetImpl::SetCompositorVisible(bool visible) {
  widget_base_->SetCompositorVisible(visible);
}

gfx::Size WebFrameWidgetImpl::Size() {
  return size_.value_or(gfx::Size());
}

void WebFrameWidgetImpl::Resize(const gfx::Size& new_size) {
  if (size_ && *size_ == new_size)
    return;

  if (ForMainFrame()) {
    size_ = new_size;
    View()->Resize(new_size);
    return;
  }

  if (child_data().did_suspend_parsing) {
    child_data().did_suspend_parsing = false;
    LocalRootImpl()->GetFrame()->Loader().GetDocumentLoader()->ResumeParser();
  }

  LocalFrameView* view = LocalRootImpl()->GetFrameView();
  DCHECK(view);

  size_ = new_size;

  view->SetLayoutSize(IntSize(*size_));
  view->Resize(IntSize(*size_));

  // FIXME: In WebViewImpl this layout was a precursor to setting the minimum
  // scale limit.  It is not clear if this is necessary for frame-level widget
  // resize.
  if (view->NeedsLayout())
    view->UpdateLayout();

  // FIXME: Investigate whether this is needed; comment from eseidel suggests
  // that this function is flawed.
  // TODO(kenrb): It would probably make more sense to check whether lifecycle
  // updates are throttled in the root's LocalFrameView, but for OOPIFs that
  // doesn't happen. Need to investigate if OOPIFs can be throttled during
  // load.
  if (LocalRootImpl()->GetFrame()->GetDocument()->IsLoadCompleted()) {
    // FIXME: This is wrong. The LocalFrameView is responsible sending a
    // resizeEvent as part of layout. Layout is also responsible for sending
    // invalidations to the embedder. This method and all callers may be wrong.
    // -- eseidel.
    LocalRootImpl()->GetFrame()->GetDocument()->EnqueueResizeEvent();

    // Pass the limits even though this is for subframes, as the limits will
    // be needed in setting the raster scale. We set this value when setting
    // up the compositor, but need to update it when the limits of the
    // WebViewImpl have changed.
    // TODO(wjmaclean): This is updating when the size of the *child frame*
    // have changed which are completely independent of the WebView, and in an
    // OOPIF where the main frame is remote, are these limits even useful?
    SetPageScaleStateAndLimits(1.f, false /* is_pinch_gesture_active */,
                               View()->MinimumPageScaleFactor(),
                               View()->MaximumPageScaleFactor());
  }
}

void WebFrameWidgetImpl::BeginMainFrame(base::TimeTicks last_frame_time) {
  TRACE_EVENT1("blink", "WebFrameWidgetImpl::BeginMainFrame", "frameTime",
               last_frame_time);
  DCHECK(!last_frame_time.is_null());
  CHECK(LocalRootImpl());

  // Dirty bit on MouseEventManager is not cleared in OOPIFs after scroll
  // or layout changes. Ensure the hover state is recomputed if necessary.
  LocalRootImpl()
      ->GetFrame()
      ->GetEventHandler()
      .RecomputeMouseHoverStateIfNeeded();

  // Adjusting frame anchor only happens on the main frame.
  if (ForMainFrame()) {
    if (LocalFrameView* view = LocalRootImpl()->GetFrameView()) {
      if (FragmentAnchor* anchor = view->GetFragmentAnchor())
        anchor->PerformPreRafActions();
    }
  }

  base::Optional<LocalFrameUkmAggregator::ScopedUkmHierarchicalTimer> ukm_timer;
  if (WidgetBase::ShouldRecordBeginMainFrameMetrics()) {
    ukm_timer.emplace(LocalRootImpl()
                          ->GetFrame()
                          ->View()
                          ->EnsureUkmAggregator()
                          .GetScopedTimer(LocalFrameUkmAggregator::kAnimate));
  }

  PageWidgetDelegate::Animate(*GetPage(), last_frame_time);
  // Animate can cause the local frame to detach.
  if (!LocalRootImpl())
    return;

  GetPage()->GetValidationMessageClient().LayoutOverlay();
}

void WebFrameWidgetImpl::BeginCommitCompositorFrame() {
  commit_compositor_frame_start_time_.emplace(base::TimeTicks::Now());
}

void WebFrameWidgetImpl::EndCommitCompositorFrame(
    base::TimeTicks commit_start_time) {
  DCHECK(commit_compositor_frame_start_time_.has_value());
  if (ForMainFrame()) {
    View()->Client()->DidCommitCompositorFrameForLocalMainFrame(
        commit_start_time);
    View()->UpdatePreferredSize();
    if (!View()->MainFrameImpl()) {
      // Trying to track down why the view's idea of the main frame varies
      // from LocalRootImpl's.
      // TODO(https://crbug.com/1139104): Remove this.
      std::string reason = View()->GetNullFrameReasonForBug1139104();
      DCHECK(false) << reason;
      SCOPED_CRASH_KEY_STRING32(Crbug1139104, NullFrameReason, reason);
      base::debug::DumpWithoutCrashing();
    }
  }

  LocalRootImpl()
      ->GetFrame()
      ->View()
      ->EnsureUkmAggregator()
      .RecordImplCompositorSample(commit_compositor_frame_start_time_.value(),
                                  commit_start_time, base::TimeTicks::Now());
  commit_compositor_frame_start_time_.reset();
}

void WebFrameWidgetImpl::ApplyViewportChanges(
    const ApplyViewportChangesArgs& args) {
  // Viewport changes only change the main frame.
  if (!ForMainFrame())
    return;
  View()->ApplyViewportChanges(args);
}

void WebFrameWidgetImpl::RecordManipulationTypeCounts(
    cc::ManipulationInfo info) {
  // Manipulation counts are only recorded for the main frame.
  if (!ForMainFrame())
    return;
  if ((info & cc::kManipulationInfoWheel) == cc::kManipulationInfoWheel) {
    UseCounter::Count(LocalRootImpl()->GetDocument(),
                      WebFeature::kScrollByWheel);
  }
  if ((info & cc::kManipulationInfoTouch) == cc::kManipulationInfoTouch) {
    UseCounter::Count(LocalRootImpl()->GetDocument(),
                      WebFeature::kScrollByTouch);
  }
  if ((info & cc::kManipulationInfoPinchZoom) ==
      cc::kManipulationInfoPinchZoom) {
    UseCounter::Count(LocalRootImpl()->GetDocument(), WebFeature::kPinchZoom);
  }
  if ((info & cc::kManipulationInfoPrecisionTouchPad) ==
      cc::kManipulationInfoPrecisionTouchPad) {
    UseCounter::Count(LocalRootImpl()->GetDocument(),
                      WebFeature::kScrollByPrecisionTouchPad);
  }
}

void WebFrameWidgetImpl::RecordDispatchRafAlignedInputTime(
    base::TimeTicks raf_aligned_input_start_time) {
  if (LocalRootImpl()) {
    LocalRootImpl()->GetFrame()->View()->EnsureUkmAggregator().RecordSample(
        LocalFrameUkmAggregator::kHandleInputEvents,
        raf_aligned_input_start_time, base::TimeTicks::Now());
  }
}

void WebFrameWidgetImpl::SetSuppressFrameRequestsWorkaroundFor704763Only(
    bool suppress_frame_requests) {
  GetPage()->Animator().SetSuppressFrameRequestsWorkaroundFor704763Only(
      suppress_frame_requests);
}

std::unique_ptr<cc::BeginMainFrameMetrics>
WebFrameWidgetImpl::GetBeginMainFrameMetrics() {
  if (!LocalRootImpl())
    return nullptr;

  return LocalRootImpl()
      ->GetFrame()
      ->View()
      ->EnsureUkmAggregator()
      .GetBeginMainFrameMetrics();
}

std::unique_ptr<cc::WebVitalMetrics> WebFrameWidgetImpl::GetWebVitalMetrics() {
  if (!LocalRootImpl())
    return nullptr;

  // This class should be called at most once per commit.
  WebPerformance perf = LocalRootImpl()->Performance();
  auto metrics = std::make_unique<cc::WebVitalMetrics>();
  if (perf.FirstInputDelay().has_value())
    metrics->first_input_delay = *perf.FirstInputDelay();

  base::TimeTicks start = perf.NavigationStartAsMonotonicTime();
  base::TimeTicks largest_contentful_paint =
      perf.LargestContentfulPaintAsMonotonicTime();
  if (largest_contentful_paint >= start)
    metrics->largest_contentful_paint = largest_contentful_paint - start;

  double layout_shift = LocalRootImpl()
                            ->GetFrame()
                            ->View()
                            ->GetLayoutShiftTracker()
                            .WeightedScore();
  if (layout_shift > 0.f)
    metrics->layout_shift = layout_shift;
  return metrics;
}

void WebFrameWidgetImpl::BeginUpdateLayers() {
  if (LocalRootImpl())
    update_layers_start_time_.emplace(base::TimeTicks::Now());
}

void WebFrameWidgetImpl::EndUpdateLayers() {
  if (LocalRootImpl()) {
    DCHECK(update_layers_start_time_);
    LocalRootImpl()->GetFrame()->View()->EnsureUkmAggregator().RecordSample(
        LocalFrameUkmAggregator::kUpdateLayers,
        update_layers_start_time_.value(), base::TimeTicks::Now());
    probe::LayerTreeDidChange(LocalRootImpl()->GetFrame());
  }
  update_layers_start_time_.reset();
}

void WebFrameWidgetImpl::RecordStartOfFrameMetrics() {
  if (!LocalRootImpl())
    return;

  LocalRootImpl()->GetFrame()->View()->EnsureUkmAggregator().BeginMainFrame();
}

void WebFrameWidgetImpl::RecordEndOfFrameMetrics(
    base::TimeTicks frame_begin_time,
    cc::ActiveFrameSequenceTrackers trackers) {
  if (!LocalRootImpl())
    return;

  LocalRootImpl()
      ->GetFrame()
      ->View()
      ->EnsureUkmAggregator()
      .RecordEndOfFrameMetrics(frame_begin_time, base::TimeTicks::Now(),
                               trackers);
}

bool WebFrameWidgetImpl::WillHandleGestureEvent(const WebGestureEvent& event) {
  possible_drag_event_info_.source = ui::mojom::blink::DragEventSource::kTouch;
  possible_drag_event_info_.location =
      gfx::ToFlooredPoint(event.PositionInScreen());

  bool move_cursor = false;
  switch (event.GetType()) {
    case WebInputEvent::Type::kGestureScrollBegin: {
      if (event.data.scroll_begin.cursor_control) {
        swipe_to_move_cursor_activated_ = true;
        move_cursor = true;
      }
      break;
    }
    case WebInputEvent::Type::kGestureScrollUpdate: {
      if (swipe_to_move_cursor_activated_)
        move_cursor = true;
      break;
    }
    case WebInputEvent::Type::kGestureScrollEnd: {
      if (swipe_to_move_cursor_activated_) {
        swipe_to_move_cursor_activated_ = false;
      }
      break;
    }
    default:
      break;
  }
  // TODO(crbug.com/1140106): Place cursor for scroll begin other than just move
  // cursor.
  if (move_cursor) {
    WebLocalFrame* focused_frame = FocusedWebLocalFrameInWidget();
    if (focused_frame) {
      gfx::Point base(event.PositionInWidget().x(),
                      event.PositionInWidget().y());
      focused_frame->MoveCaretSelection(base);
    }
    return true;
  }
  return false;
}

void WebFrameWidgetImpl::WillHandleMouseEvent(const WebMouseEvent& event) {
  possible_drag_event_info_.source = ui::mojom::blink::DragEventSource::kMouse;
  possible_drag_event_info_.location =
      gfx::Point(event.PositionInScreen().x(), event.PositionInScreen().y());
}

void WebFrameWidgetImpl::ObserveGestureEventAndResult(
    const WebGestureEvent& gesture_event,
    const gfx::Vector2dF& unused_delta,
    const cc::OverscrollBehavior& overscroll_behavior,
    bool event_processed) {
  if (!widget_base_->LayerTreeHost()->GetSettings().enable_elastic_overscroll)
    return;

  cc::InputHandlerScrollResult scroll_result;
  scroll_result.did_scroll = event_processed;
  scroll_result.did_overscroll_root = !unused_delta.IsZero();
  scroll_result.unused_scroll_delta = unused_delta;
  scroll_result.overscroll_behavior = overscroll_behavior;

  widget_base_->widget_input_handler_manager()->ObserveGestureEventOnMainThread(
      gesture_event, scroll_result);
}

void WebFrameWidgetImpl::DidHandleKeyEvent() {
  ClearEditCommands();
}

WebTextInputType WebFrameWidgetImpl::GetTextInputType() {
  if (ShouldDispatchImeEventsToPlugin()) {
    return GetFocusedPluginContainer()->GetPluginTextInputType();
  }

  WebInputMethodController* controller = GetActiveWebInputMethodController();
  if (!controller)
    return WebTextInputType::kWebTextInputTypeNone;
  return controller->TextInputType();
}

void WebFrameWidgetImpl::SetCursorVisibilityState(bool is_visible) {
  GetPage()->SetIsCursorVisible(is_visible);
}

void WebFrameWidgetImpl::ApplyViewportChangesForTesting(
    const ApplyViewportChangesArgs& args) {
  widget_base_->ApplyViewportChanges(args);
}

void WebFrameWidgetImpl::SetDisplayMode(mojom::blink::DisplayMode mode) {
  if (mode != display_mode_) {
    display_mode_ = mode;
    LocalFrame* frame = LocalRootImpl()->GetFrame();
    frame->MediaQueryAffectingValueChangedForLocalSubtree(
        MediaValueChange::kOther);
  }
}

void WebFrameWidgetImpl::SetWindowSegments(
    const std::vector<gfx::Rect>& window_segments_param) {
  WebVector<gfx::Rect> window_segments(window_segments_param);
  if (!window_segments_.Equals(window_segments)) {
    window_segments_ = window_segments;
    LocalFrame* frame = LocalRootImpl()->GetFrame();
    frame->WindowSegmentsChanged(window_segments_);

    ForEachRemoteFrameControlledByWidget(WTF::BindRepeating(
        [](const std::vector<gfx::Rect>& window_segments,
           RemoteFrame* remote_frame) {
          remote_frame->Client()->DidChangeRootWindowSegments(window_segments);
        },
        window_segments_param));
  }
}

void WebFrameWidgetImpl::SetCursor(const ui::Cursor& cursor) {
  widget_base_->SetCursor(cursor);
}

bool WebFrameWidgetImpl::HandlingInputEvent() {
  return widget_base_->input_handler().handling_input_event();
}

void WebFrameWidgetImpl::SetHandlingInputEvent(bool handling) {
  widget_base_->input_handler().set_handling_input_event(handling);
}

void WebFrameWidgetImpl::ProcessInputEventSynchronouslyForTesting(
    const WebCoalescedInputEvent& event,
    HandledEventCallback callback) {
  widget_base_->input_handler().HandleInputEvent(event, nullptr,
                                                 std::move(callback));
}

WebInputEventResult WebFrameWidgetImpl::DispatchBufferedTouchEvents() {
  CHECK(LocalRootImpl());

  if (WebDevToolsAgentImpl* devtools = LocalRootImpl()->DevToolsAgentImpl())
    devtools->DispatchBufferedTouchEvents();

  return LocalRootImpl()
      ->GetFrame()
      ->GetEventHandler()
      .DispatchBufferedTouchEvents();
}

WebInputEventResult WebFrameWidgetImpl::HandleInputEvent(
    const WebCoalescedInputEvent& coalesced_event) {
  const WebInputEvent& input_event = coalesced_event.Event();
  TRACE_EVENT1("input,rail", "WebFrameWidgetImpl::HandleInputEvent", "type",
               WebInputEvent::GetName(input_event.GetType()));
  DCHECK(!WebInputEvent::IsTouchEventType(input_event.GetType()));
  CHECK(LocalRootImpl());

  // Only record metrics for the main frame.
  if (ForMainFrame()) {
    GetPage()->GetVisualViewport().StartTrackingPinchStats();
  }

  // If a drag-and-drop operation is in progress, ignore input events except
  // PointerCancel.
  if (doing_drag_and_drop_ &&
      input_event.GetType() != WebInputEvent::Type::kPointerCancel)
    return WebInputEventResult::kHandledSuppressed;

  // Don't handle events once we've started shutting down.
  if (!GetPage())
    return WebInputEventResult::kNotHandled;

  if (WebDevToolsAgentImpl* devtools = LocalRootImpl()->DevToolsAgentImpl()) {
    auto result = devtools->HandleInputEvent(input_event);
    if (result != WebInputEventResult::kNotHandled)
      return result;
  }

  // Report the event to be NOT processed by WebKit, so that the browser can
  // handle it appropriately.
  if (IgnoreInputEvents())
    return WebInputEventResult::kNotHandled;

  base::AutoReset<const WebInputEvent*> current_event_change(
      &CurrentInputEvent::current_input_event_, &input_event);
  UIEventWithKeyState::ClearNewTabModifierSetFromIsolatedWorld();

  if (GetPage()->GetPointerLockController().IsPointerLocked() &&
      WebInputEvent::IsMouseEventType(input_event.GetType())) {
    PointerLockMouseEvent(coalesced_event);
    return WebInputEventResult::kHandledSystem;
  }

  /// These metrics are only captured for the main frame.
  if (ForMainFrame()) {
    Document& main_frame_document = *LocalRootImpl()->GetFrame()->GetDocument();

    if (input_event.GetType() != WebInputEvent::Type::kMouseMove) {
      FirstMeaningfulPaintDetector::From(main_frame_document)
          .NotifyInputEvent();
    }

    if (input_event.GetType() != WebInputEvent::Type::kMouseMove &&
        input_event.GetType() != WebInputEvent::Type::kMouseEnter &&
        input_event.GetType() != WebInputEvent::Type::kMouseLeave) {
      InteractiveDetector* interactive_detector(
          InteractiveDetector::From(main_frame_document));
      if (interactive_detector) {
        interactive_detector->OnInvalidatingInputEvent(input_event.TimeStamp());
      }
    }
  }

  NotifyInputObservers(coalesced_event);

  // Notify the focus frame of the input. Note that the other frames are not
  // notified as input is only handled by the focused frame.
  Frame* frame = FocusedCoreFrame();
  if (auto* local_frame = DynamicTo<LocalFrame>(frame)) {
    if (auto* content_capture_manager =
            local_frame->LocalFrameRoot().GetContentCaptureManager()) {
      content_capture_manager->NotifyInputEvent(input_event.GetType(),
                                                *local_frame);
    }
  }

  // Skip the pointerrawupdate for mouse capture case.
  if (mouse_capture_element_ &&
      input_event.GetType() == WebInputEvent::Type::kPointerRawUpdate)
    return WebInputEventResult::kHandledSystem;

  if (mouse_capture_element_ &&
      WebInputEvent::IsMouseEventType(input_event.GetType()))
    return HandleCapturedMouseEvent(coalesced_event);

  // FIXME: This should take in the intended frame, not the local frame
  // root.
  return PageWidgetDelegate::HandleInputEvent(*this, coalesced_event,
                                              LocalRootImpl()->GetFrame());
}

WebInputEventResult WebFrameWidgetImpl::HandleCapturedMouseEvent(
    const WebCoalescedInputEvent& coalesced_event) {
  const WebInputEvent& input_event = coalesced_event.Event();
  TRACE_EVENT1("input", "captured mouse event", "type", input_event.GetType());
  // Save |mouse_capture_element_| since |MouseCaptureLost()| will clear it.
  HTMLPlugInElement* element = mouse_capture_element_;

  // Not all platforms call mouseCaptureLost() directly.
  if (input_event.GetType() == WebInputEvent::Type::kMouseUp) {
    SetMouseCapture(false);
    MouseCaptureLost();
  }

  AtomicString event_type;
  switch (input_event.GetType()) {
    case WebInputEvent::Type::kMouseEnter:
      event_type = event_type_names::kMouseover;
      break;
    case WebInputEvent::Type::kMouseMove:
      event_type = event_type_names::kMousemove;
      break;
    case WebInputEvent::Type::kPointerRawUpdate:
      // There will be no mouse event for rawupdate events.
      event_type = event_type_names::kPointerrawupdate;
      break;
    case WebInputEvent::Type::kMouseLeave:
      event_type = event_type_names::kMouseout;
      break;
    case WebInputEvent::Type::kMouseDown:
      event_type = event_type_names::kMousedown;
      LocalFrame::NotifyUserActivation(
          element->GetDocument().GetFrame(),
          mojom::blink::UserActivationNotificationType::kInteraction);
      break;
    case WebInputEvent::Type::kMouseUp:
      event_type = event_type_names::kMouseup;
      break;
    default:
      NOTREACHED();
  }

  WebMouseEvent transformed_event =
      TransformWebMouseEvent(LocalRootImpl()->GetFrameView(),
                             static_cast<const WebMouseEvent&>(input_event));
  if (LocalFrame* frame = element->GetDocument().GetFrame()) {
    frame->GetEventHandler().HandleTargetedMouseEvent(
        element, transformed_event, event_type,
        TransformWebMouseEventVector(
            LocalRootImpl()->GetFrameView(),
            coalesced_event.GetCoalescedEventsPointers()),
        TransformWebMouseEventVector(
            LocalRootImpl()->GetFrameView(),
            coalesced_event.GetPredictedEventsPointers()));
  }
  return WebInputEventResult::kHandledSystem;
}

void WebFrameWidgetImpl::UpdateTextInputState() {
  widget_base_->UpdateTextInputState();
}

void WebFrameWidgetImpl::UpdateSelectionBounds() {
  widget_base_->UpdateSelectionBounds();
}

void WebFrameWidgetImpl::ShowVirtualKeyboard() {
  widget_base_->ShowVirtualKeyboard();
}

void WebFrameWidgetImpl::FlushInputProcessedCallback() {
  widget_base_->FlushInputProcessedCallback();
}

void WebFrameWidgetImpl::CancelCompositionForPepper() {
  widget_base_->CancelCompositionForPepper();
}

void WebFrameWidgetImpl::RequestMouseLock(
    bool has_transient_user_activation,
    bool request_unadjusted_movement,
    mojom::blink::WidgetInputHandlerHost::RequestMouseLockCallback callback) {
  mojom::blink::WidgetInputHandlerHost* host =
      widget_base_->widget_input_handler_manager()->GetWidgetInputHandlerHost();

  // If we don't have a host just leave the callback uncalled. This simulates
  // the browser indefinitely postponing the mouse request which is valid.
  // Note that |callback| is not a mojo bound callback (until it is passed
  // into the mojo interface) and can be destructed without invoking the
  // callback. It does share the same signature as the mojo definition
  // for simplicity.
  if (host) {
    host->RequestMouseLock(has_transient_user_activation,
                           request_unadjusted_movement, std::move(callback));
  }
}

void WebFrameWidgetImpl::MouseCaptureLost() {
  TRACE_EVENT_NESTABLE_ASYNC_END0("input", "capturing mouse",
                                  TRACE_ID_LOCAL(this));
  mouse_capture_element_ = nullptr;
}

void WebFrameWidgetImpl::ApplyVisualProperties(
    const VisualProperties& visual_properties) {
  widget_base_->UpdateVisualProperties(visual_properties);
}

bool WebFrameWidgetImpl::IsFullscreenGranted() {
  return is_fullscreen_granted_;
}

bool WebFrameWidgetImpl::PinchGestureActiveInMainFrame() {
  return is_pinch_gesture_active_in_mainframe_;
}

float WebFrameWidgetImpl::PageScaleInMainFrame() {
  return page_scale_factor_in_mainframe_;
}

void WebFrameWidgetImpl::UpdateSurfaceAndScreenInfo(
    const viz::LocalSurfaceId& new_local_surface_id,
    const gfx::Rect& compositor_viewport_pixel_rect,
    const ScreenInfo& new_screen_info) {
  widget_base_->UpdateSurfaceAndScreenInfo(
      new_local_surface_id, compositor_viewport_pixel_rect, new_screen_info);
}

void WebFrameWidgetImpl::UpdateScreenInfo(const ScreenInfo& new_screen_info) {
  widget_base_->UpdateScreenInfo(new_screen_info);
}

void WebFrameWidgetImpl::UpdateSurfaceAndCompositorRect(
    const viz::LocalSurfaceId& new_local_surface_id,
    const gfx::Rect& compositor_viewport_pixel_rect) {
  widget_base_->UpdateSurfaceAndCompositorRect(new_local_surface_id,
                                               compositor_viewport_pixel_rect);
}

void WebFrameWidgetImpl::UpdateCompositorViewportRect(
    const gfx::Rect& compositor_viewport_pixel_rect) {
  widget_base_->UpdateCompositorViewportRect(compositor_viewport_pixel_rect);
}

const ScreenInfo& WebFrameWidgetImpl::GetScreenInfo() {
  return widget_base_->GetScreenInfo();
}

gfx::Rect WebFrameWidgetImpl::WindowRect() {
  return widget_base_->WindowRect();
}

gfx::Rect WebFrameWidgetImpl::ViewRect() {
  return widget_base_->ViewRect();
}

void WebFrameWidgetImpl::SetScreenRects(const gfx::Rect& widget_screen_rect,
                                        const gfx::Rect& window_screen_rect) {
  widget_base_->SetScreenRects(widget_screen_rect, window_screen_rect);
}

gfx::Size WebFrameWidgetImpl::VisibleViewportSizeInDIPs() {
  return widget_base_->VisibleViewportSizeInDIPs();
}

void WebFrameWidgetImpl::SetPendingWindowRect(
    const gfx::Rect& window_screen_rect) {
  widget_base_->SetPendingWindowRect(window_screen_rect);
}

void WebFrameWidgetImpl::AckPendingWindowRect() {
  widget_base_->AckPendingWindowRect();
}

bool WebFrameWidgetImpl::IsHidden() const {
  return widget_base_->is_hidden();
}

WebString WebFrameWidgetImpl::GetLastToolTipTextForTesting() const {
  return GetPage()->GetChromeClient().GetLastToolTipTextForTesting();
}

float WebFrameWidgetImpl::GetEmulatorScale() {
  if (device_emulator_)
    return device_emulator_->scale();
  return 1.0f;
}

void WebFrameWidgetImpl::IntrinsicSizingInfoChanged(
    mojom::blink::IntrinsicSizingInfoPtr sizing_info) {
  DCHECK(ForSubframe());
  GetAssociatedFrameWidgetHost()->IntrinsicSizingInfoChanged(
      std::move(sizing_info));
}

void WebFrameWidgetImpl::AutoscrollStart(const gfx::PointF& position) {
  GetAssociatedFrameWidgetHost()->AutoscrollStart(std::move(position));
}

void WebFrameWidgetImpl::AutoscrollFling(const gfx::Vector2dF& velocity) {
  GetAssociatedFrameWidgetHost()->AutoscrollFling(std::move(velocity));
}

void WebFrameWidgetImpl::AutoscrollEnd() {
  GetAssociatedFrameWidgetHost()->AutoscrollEnd();
}

void WebFrameWidgetImpl::DidMeaningfulLayout(WebMeaningfulLayout layout_type) {
  if (layout_type == blink::WebMeaningfulLayout::kVisuallyNonEmpty) {
    NotifySwapAndPresentationTime(
        base::NullCallback(),
        WTF::Bind(&WebFrameWidgetImpl::PresentationCallbackForMeaningfulLayout,
                  WrapPersistent(this)));
  }

  ForEachLocalFrameControlledByWidget(
      local_root_->GetFrame(),
      WTF::BindRepeating(
          [](WebMeaningfulLayout layout_type, WebLocalFrame* local_frame) {
            local_frame->Client()->DidMeaningfulLayout(layout_type);
          },
          layout_type));
}

void WebFrameWidgetImpl::PresentationCallbackForMeaningfulLayout(
    blink::WebSwapResult,
    base::TimeTicks) {
  GetAssociatedFrameWidgetHost()->DidFirstVisuallyNonEmptyPaint();
}

void WebFrameWidgetImpl::RequestAnimationAfterDelay(
    const base::TimeDelta& delay) {
  widget_base_->RequestAnimationAfterDelay(delay);
}

void WebFrameWidgetImpl::SetRootLayer(scoped_refptr<cc::Layer> layer) {
  if (!View()->does_composite()) {
    DCHECK(ForMainFrame());
    DCHECK(!layer);
    return;
  }

  // Set up some initial state before we are setting the layer.
  if (ForSubframe() && layer) {
    // Child local roots will always have a transparent background color.
    widget_base_->LayerTreeHost()->set_background_color(SK_ColorTRANSPARENT);
    // Pass the limits even though this is for subframes, as the limits will
    // be needed in setting the raster scale.
    SetPageScaleStateAndLimits(1.f, false /* is_pinch_gesture_active */,
                               View()->MinimumPageScaleFactor(),
                               View()->MaximumPageScaleFactor());
  }

  bool root_layer_exists = !!layer;
  widget_base_->LayerTreeHost()->SetRootLayer(std::move(layer));

  // Notify the WebView that we did set a layer.
  if (ForMainFrame()) {
    View()->DidChangeRootLayer(root_layer_exists);
  }
}

base::WeakPtr<AnimationWorkletMutatorDispatcherImpl>
WebFrameWidgetImpl::EnsureCompositorMutatorDispatcher(
    scoped_refptr<base::SingleThreadTaskRunner>* mutator_task_runner) {
  if (!mutator_task_runner_) {
    widget_base_->LayerTreeHost()->SetLayerTreeMutator(
        AnimationWorkletMutatorDispatcherImpl::CreateCompositorThreadClient(
            &mutator_dispatcher_, &mutator_task_runner_));
  }

  DCHECK(mutator_task_runner_);
  *mutator_task_runner = mutator_task_runner_;
  return mutator_dispatcher_;
}

HitTestResult WebFrameWidgetImpl::CoreHitTestResultAt(
    const gfx::PointF& point_in_viewport) {
  LocalFrameView* view = LocalRootImpl()->GetFrameView();
  FloatPoint point_in_root_frame(
      view->ViewportToFrame(FloatPoint(point_in_viewport)));
  return HitTestResultForRootFramePos(point_in_root_frame);
}

cc::AnimationHost* WebFrameWidgetImpl::AnimationHost() const {
  return widget_base_->AnimationHost();
}

base::WeakPtr<PaintWorkletPaintDispatcher>
WebFrameWidgetImpl::EnsureCompositorPaintDispatcher(
    scoped_refptr<base::SingleThreadTaskRunner>* paint_task_runner) {
  // We check paint_task_runner_ not paint_dispatcher_ because the dispatcher is
  // a base::WeakPtr that should only be used on the compositor thread.
  if (!paint_task_runner_) {
    widget_base_->LayerTreeHost()->SetPaintWorkletLayerPainter(
        PaintWorkletPaintDispatcher::CreateCompositorThreadPainter(
            &paint_dispatcher_));
    paint_task_runner_ = Thread::CompositorThread()->GetTaskRunner();
  }
  DCHECK(paint_task_runner_);
  *paint_task_runner = paint_task_runner_;
  return paint_dispatcher_;
}

void WebFrameWidgetImpl::SetDelegatedInkMetadata(
    std::unique_ptr<viz::DelegatedInkMetadata> metadata) {
  widget_base_->LayerTreeHost()->SetDelegatedInkMetadata(std::move(metadata));
}

// Enables measuring and reporting both presentation times and swap times in
// swap promises.
class ReportTimeSwapPromise : public cc::SwapPromise {
 public:
  ReportTimeSwapPromise(WebReportTimeCallback swap_time_callback,
                        WebReportTimeCallback presentation_time_callback,
                        scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                        WebFrameWidgetImpl* widget)
      : swap_time_callback_(std::move(swap_time_callback)),
        presentation_time_callback_(std::move(presentation_time_callback)),
        task_runner_(std::move(task_runner)),
        widget_(widget) {}
  ~ReportTimeSwapPromise() override = default;

  void DidActivate() override {}

  void WillSwap(viz::CompositorFrameMetadata* metadata) override {
    DCHECK_GT(metadata->frame_token, 0u);
    // The interval between the current swap and its presentation time is
    // reported in UMA (see corresponding code in DidSwap() below).
    frame_token_ = metadata->frame_token;
  }

  void DidSwap() override {
    DCHECK_GT(frame_token_, 0u);
    PostCrossThreadTask(
        *task_runner_, FROM_HERE,
        CrossThreadBindOnce(
            &RunCallbackAfterSwap, widget_, base::TimeTicks::Now(),
            std::move(swap_time_callback_),
            std::move(presentation_time_callback_), frame_token_));
  }

  cc::SwapPromise::DidNotSwapAction DidNotSwap(
      DidNotSwapReason reason) override {
    WebSwapResult result;
    switch (reason) {
      case cc::SwapPromise::DidNotSwapReason::SWAP_FAILS:
        result = WebSwapResult::kDidNotSwapSwapFails;
        break;
      case cc::SwapPromise::DidNotSwapReason::COMMIT_FAILS:
        result = WebSwapResult::kDidNotSwapCommitFails;
        break;
      case cc::SwapPromise::DidNotSwapReason::COMMIT_NO_UPDATE:
        result = WebSwapResult::kDidNotSwapCommitNoUpdate;
        break;
      case cc::SwapPromise::DidNotSwapReason::ACTIVATION_FAILS:
        result = WebSwapResult::kDidNotSwapActivationFails;
        break;
    }
    // During a failed swap, return the current time regardless of whether we're
    // using presentation or swap timestamps.
    PostCrossThreadTask(
        *task_runner_, FROM_HERE,
        CrossThreadBindOnce(
            [](WebSwapResult result, base::TimeTicks swap_time,
               WebReportTimeCallback swap_time_callback,
               WebReportTimeCallback presentation_time_callback) {
              ReportTime(std::move(swap_time_callback), result, swap_time);
              ReportTime(std::move(presentation_time_callback), result,
                         swap_time);
            },
            result, base::TimeTicks::Now(), std::move(swap_time_callback_),
            std::move(presentation_time_callback_)));
    return DidNotSwapAction::BREAK_PROMISE;
  }

  int64_t TraceId() const override { return 0; }

 private:
  static void RunCallbackAfterSwap(
      WebFrameWidgetImpl* widget,
      base::TimeTicks swap_time,
      WebReportTimeCallback swap_time_callback,
      WebReportTimeCallback presentation_time_callback,
      int frame_token) {
    // If the widget was collected or the widget wasn't collected yet, but
    // it was closed don't schedule a presentation callback.
    if (widget && widget->widget_base_) {
      widget->widget_base_->AddPresentationCallback(
          frame_token,
          WTF::Bind(&RunCallbackAfterPresentation,
                    std::move(presentation_time_callback), swap_time));
      ReportTime(std::move(swap_time_callback), WebSwapResult::kDidSwap,
                 swap_time);
    } else {
      ReportTime(std::move(swap_time_callback), WebSwapResult::kDidSwap,
                 swap_time);
      ReportTime(std::move(presentation_time_callback), WebSwapResult::kDidSwap,
                 swap_time);
    }
  }

  static void RunCallbackAfterPresentation(
      WebReportTimeCallback presentation_time_callback,
      base::TimeTicks swap_time,
      base::TimeTicks presentation_time) {
    DCHECK(!swap_time.is_null());
    bool presentation_time_is_valid =
        !presentation_time.is_null() && (presentation_time > swap_time);
    UMA_HISTOGRAM_BOOLEAN("PageLoad.Internal.Renderer.PresentationTime.Valid",
                          presentation_time_is_valid);
    if (presentation_time_is_valid) {
      // This measures from 1ms to 10seconds.
      UMA_HISTOGRAM_TIMES(
          "PageLoad.Internal.Renderer.PresentationTime.DeltaFromSwapTime",
          presentation_time - swap_time);
    }
    ReportTime(std::move(presentation_time_callback), WebSwapResult::kDidSwap,
               presentation_time_is_valid ? presentation_time : swap_time);
  }

  static void ReportTime(WebReportTimeCallback callback,
                         WebSwapResult result,
                         base::TimeTicks time) {
    if (callback)
      std::move(callback).Run(result, time);
  }

  WebReportTimeCallback swap_time_callback_;
  WebReportTimeCallback presentation_time_callback_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  CrossThreadWeakPersistent<WebFrameWidgetImpl> widget_;
  uint32_t frame_token_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ReportTimeSwapPromise);
};

void WebFrameWidgetImpl::NotifySwapAndPresentationTimeInBlink(
    WebReportTimeCallback swap_time_callback,
    WebReportTimeCallback presentation_time_callback) {
  NotifySwapAndPresentationTime(std::move(swap_time_callback),
                                std::move(presentation_time_callback));
}

void WebFrameWidgetImpl::NotifySwapAndPresentationTime(
    WebReportTimeCallback swap_time_callback,
    WebReportTimeCallback presentation_time_callback) {
  if (!View()->does_composite())
    return;
  widget_base_->LayerTreeHost()->QueueSwapPromise(
      std::make_unique<ReportTimeSwapPromise>(
          std::move(swap_time_callback), std::move(presentation_time_callback),
          widget_base_->LayerTreeHost()
              ->GetTaskRunnerProvider()
              ->MainThreadTaskRunner(),
          this));
}

scheduler::WebRenderWidgetSchedulingState*
WebFrameWidgetImpl::RendererWidgetSchedulingState() {
  return widget_base_->RendererWidgetSchedulingState();
}

void WebFrameWidgetImpl::WaitForDebuggerWhenShown() {
  local_root_->WaitForDebuggerWhenShown();
}

void WebFrameWidgetImpl::SetTextZoomFactor(float text_zoom_factor) {
  local_root_->GetFrame()->SetTextZoomFactor(text_zoom_factor);
}

float WebFrameWidgetImpl::TextZoomFactor() {
  return local_root_->GetFrame()->TextZoomFactor();
}

void WebFrameWidgetImpl::SetMainFrameOverlayColor(SkColor color) {
  DCHECK(!local_root_->Parent());
  local_root_->GetFrame()->SetMainFrameColorOverlay(color);
}

void WebFrameWidgetImpl::AddEditCommandForNextKeyEvent(const WebString& name,
                                                       const WebString& value) {
  edit_commands_.push_back(mojom::blink::EditCommand::New(name, value));
}

bool WebFrameWidgetImpl::HandleCurrentKeyboardEvent() {
  bool did_execute_command = false;
  WebLocalFrame* frame = FocusedWebLocalFrameInWidget();
  if (!frame)
    frame = local_root_;
  for (const auto& command : edit_commands_) {
    // In gtk and cocoa, it's possible to bind multiple edit commands to one
    // key (but it's the exception). Once one edit command is not executed, it
    // seems safest to not execute the rest.
    if (!frame->ExecuteCommand(command->name, command->value))
      break;
    did_execute_command = true;
  }

  return did_execute_command;
}

void WebFrameWidgetImpl::ClearEditCommands() {
  edit_commands_ = Vector<mojom::blink::EditCommandPtr>();
}

WebTextInputInfo WebFrameWidgetImpl::TextInputInfo() {
  WebInputMethodController* controller = GetActiveWebInputMethodController();
  if (!controller)
    return WebTextInputInfo();
  return controller->TextInputInfo();
}

ui::mojom::blink::VirtualKeyboardVisibilityRequest
WebFrameWidgetImpl::GetLastVirtualKeyboardVisibilityRequest() {
  WebInputMethodController* controller = GetActiveWebInputMethodController();
  if (!controller)
    return ui::mojom::blink::VirtualKeyboardVisibilityRequest::NONE;
  return controller->GetLastVirtualKeyboardVisibilityRequest();
}

bool WebFrameWidgetImpl::ShouldSuppressKeyboardForFocusedElement() {
  WebLocalFrame* focused_frame = FocusedWebLocalFrameInWidget();
  if (!focused_frame)
    return false;
  return focused_frame->ShouldSuppressKeyboardForFocusedElement();
}

void WebFrameWidgetImpl::GetEditContextBoundsInWindow(
    base::Optional<gfx::Rect>* edit_context_control_bounds,
    base::Optional<gfx::Rect>* edit_context_selection_bounds) {
  WebInputMethodController* controller = GetActiveWebInputMethodController();
  if (!controller)
    return;
  WebRect control_bounds;
  WebRect selection_bounds;
  controller->GetLayoutBounds(&control_bounds, &selection_bounds);
  *edit_context_control_bounds =
      widget_base_->BlinkSpaceToEnclosedDIPs(gfx::Rect(control_bounds));
  if (controller->IsEditContextActive()) {
    *edit_context_selection_bounds =
        widget_base_->BlinkSpaceToEnclosedDIPs(gfx::Rect(selection_bounds));
  }
}

int32_t WebFrameWidgetImpl::ComputeWebTextInputNextPreviousFlags() {
  WebInputMethodController* controller = GetActiveWebInputMethodController();
  if (!controller)
    return 0;
  return controller->ComputeWebTextInputNextPreviousFlags();
}

void WebFrameWidgetImpl::ResetVirtualKeyboardVisibilityRequest() {
  WebInputMethodController* controller = GetActiveWebInputMethodController();
  if (!controller)
    return;
  controller->SetVirtualKeyboardVisibilityRequest(
      ui::mojom::blink::VirtualKeyboardVisibilityRequest::NONE);
  ;
}

bool WebFrameWidgetImpl::GetSelectionBoundsInWindow(
    gfx::Rect* focus,
    gfx::Rect* anchor,
    base::i18n::TextDirection* focus_dir,
    base::i18n::TextDirection* anchor_dir,
    bool* is_anchor_first) {
  if (ShouldDispatchImeEventsToPlugin()) {
    // TODO(kinaba) http://crbug.com/101101
    // Current Pepper IME API does not handle selection bounds. So we simply
    // use the caret position as an empty range for now. It will be updated
    // after Pepper API equips features related to surrounding text retrieval.
    gfx::Rect pepper_caret_in_dips = widget_base_->BlinkSpaceToEnclosedDIPs(
        GetFocusedPluginContainer()->GetPluginCaretBounds());
    if (pepper_caret_in_dips == *focus && pepper_caret_in_dips == *anchor)
      return false;
    *focus = pepper_caret_in_dips;
    *anchor = *focus;
    return true;
  }
  gfx::Rect focus_root_frame;
  gfx::Rect anchor_root_frame;
  CalculateSelectionBounds(focus_root_frame, anchor_root_frame);
  gfx::Rect focus_rect_in_dips =
      widget_base_->BlinkSpaceToEnclosedDIPs(gfx::Rect(focus_root_frame));
  gfx::Rect anchor_rect_in_dips =
      widget_base_->BlinkSpaceToEnclosedDIPs(gfx::Rect(anchor_root_frame));

  // if the bounds are the same return false.
  if (focus_rect_in_dips == *focus && anchor_rect_in_dips == *anchor)
    return false;
  *focus = focus_rect_in_dips;
  *anchor = anchor_rect_in_dips;

  WebLocalFrame* focused_frame = FocusedWebLocalFrameInWidget();
  if (!focused_frame)
    return true;
  focused_frame->SelectionTextDirection(*focus_dir, *anchor_dir);
  *is_anchor_first = focused_frame->IsSelectionAnchorFirst();
  return true;
}

void WebFrameWidgetImpl::ClearTextInputState() {
  widget_base_->ClearTextInputState();
}

bool WebFrameWidgetImpl::IsPasting() {
  return widget_base_->is_pasting();
}

bool WebFrameWidgetImpl::HandlingSelectRange() {
  return widget_base_->handling_select_range();
}

void WebFrameWidgetImpl::SetFocus(bool focus) {
  widget_base_->SetFocus(focus);
}

bool WebFrameWidgetImpl::HasFocus() {
  return widget_base_->has_focus();
}

void WebFrameWidgetImpl::SetToolTipText(const String& tooltip_text,
                                        TextDirection dir) {
  widget_base_->SetToolTipText(tooltip_text, dir);
}

void WebFrameWidgetImpl::DidOverscroll(
    const gfx::Vector2dF& overscroll_delta,
    const gfx::Vector2dF& accumulated_overscroll,
    const gfx::PointF& position,
    const gfx::Vector2dF& velocity) {
#if defined(OS_MAC)
  // On OSX the user can disable the elastic overscroll effect. If that's the
  // case, don't forward the overscroll notification.
  if (!widget_base_->LayerTreeHost()->GetSettings().enable_elastic_overscroll)
    return;
#endif

  cc::OverscrollBehavior overscroll_behavior =
      widget_base_->LayerTreeHost()->overscroll_behavior();
  if (!widget_base_->input_handler().DidOverscrollFromBlink(
          overscroll_delta, accumulated_overscroll, position, velocity,
          overscroll_behavior))
    return;

  // If we're currently handling an event, stash the overscroll data such that
  // it can be bundled in the event ack.
  if (mojom::blink::WidgetInputHandlerHost* host =
          widget_base_->widget_input_handler_manager()
              ->GetWidgetInputHandlerHost()) {
    host->DidOverscroll(mojom::blink::DidOverscrollParams::New(
        accumulated_overscroll, overscroll_delta, velocity, position,
        overscroll_behavior));
  }
}

void WebFrameWidgetImpl::InjectGestureScrollEvent(
    blink::WebGestureDevice device,
    const gfx::Vector2dF& delta,
    ui::ScrollGranularity granularity,
    cc::ElementId scrollable_area_element_id,
    blink::WebInputEvent::Type injected_type) {
  if (RuntimeEnabledFeatures::ScrollUnificationEnabled()) {
    // create a GestureScroll Event and post it to the compositor thread
    // TODO(crbug.com/1126098) use original input event's timestamp.
    // TODO(crbug.com/1082590) ensure continuity in scroll metrics collection
    base::TimeTicks now = base::TimeTicks::Now();
    std::unique_ptr<WebGestureEvent> gesture_event =
        WebGestureEvent::GenerateInjectedScrollGesture(
            injected_type, now, device, gfx::PointF(0, 0), delta, granularity);
    if (injected_type == WebInputEvent::Type::kGestureScrollBegin) {
      gesture_event->data.scroll_begin.scrollable_area_element_id =
          scrollable_area_element_id.GetStableId();
      gesture_event->data.scroll_begin.main_thread_hit_tested = true;
    }

    widget_base_->widget_input_handler_manager()
        ->DispatchScrollGestureToCompositor(std::move(gesture_event));
  } else {
    widget_base_->input_handler().InjectGestureScrollEvent(
        device, delta, granularity, scrollable_area_element_id, injected_type);
  }
}

void WebFrameWidgetImpl::DidChangeCursor(const ui::Cursor& cursor) {
  widget_base_->SetCursor(cursor);
  Client()->DidChangeCursor(cursor);
}

bool WebFrameWidgetImpl::SetComposition(
    const String& text,
    const Vector<ui::ImeTextSpan>& ime_text_spans,
    const gfx::Range& replacement_range,
    int selection_start,
    int selection_end) {
  WebInputMethodController* controller = GetActiveWebInputMethodController();
  if (!controller)
    return false;

  return controller->SetComposition(
      text, ime_text_spans,
      replacement_range.IsValid()
          ? WebRange(replacement_range.start(), replacement_range.length())
          : WebRange(),
      selection_start, selection_end);
}

void WebFrameWidgetImpl::CommitText(
    const String& text,
    const Vector<ui::ImeTextSpan>& ime_text_spans,
    const gfx::Range& replacement_range,
    int relative_cursor_pos) {
  WebInputMethodController* controller = GetActiveWebInputMethodController();
  if (!controller)
    return;
  controller->CommitText(
      text, ime_text_spans,
      replacement_range.IsValid()
          ? WebRange(replacement_range.start(), replacement_range.length())
          : WebRange(),
      relative_cursor_pos);
}

void WebFrameWidgetImpl::FinishComposingText(bool keep_selection) {
  WebInputMethodController* controller = GetActiveWebInputMethodController();
  if (!controller)
    return;
  controller->FinishComposingText(
      keep_selection ? WebInputMethodController::kKeepSelection
                     : WebInputMethodController::kDoNotKeepSelection);
}

bool WebFrameWidgetImpl::IsProvisional() {
  return LocalRoot()->IsProvisional();
}

uint64_t WebFrameWidgetImpl::GetScrollableContainerIdAt(
    const gfx::PointF& point_in_dips) {
  gfx::PointF point = widget_base_->DIPsToBlinkSpace(point_in_dips);
  return HitTestResultAt(point).GetScrollableContainerId();
}

bool WebFrameWidgetImpl::ShouldHandleImeEvents() {
  if (ForMainFrame()) {
    return HasFocus();
  } else {
    // TODO(ekaramad): main frame widget returns true only if it has focus.
    // We track page focus in all WebViews on the page but the WebFrameWidgets
    // corresponding to child local roots do not get the update. For now, this
    // method returns true when the WebFrameWidget is for a child local frame,
    // i.e., IME events will be processed regardless of page focus. We should
    // revisit this after page focus for OOPIFs has been fully resolved
    // (https://crbug.com/689777).
    return LocalRootImpl();
  }
}

void WebFrameWidgetImpl::SetEditCommandsForNextKeyEvent(
    Vector<mojom::blink::EditCommandPtr> edit_commands) {
  edit_commands_ = std::move(edit_commands);
}

void WebFrameWidgetImpl::FocusChangeComplete() {
  blink::WebLocalFrame* focused = LocalRoot()->View()->FocusedFrame();

  if (focused && focused->AutofillClient())
    focused->AutofillClient()->DidCompleteFocusChangeInFrame();
}

void WebFrameWidgetImpl::ShowVirtualKeyboardOnElementFocus() {
  widget_base_->ShowVirtualKeyboardOnElementFocus();
}

void WebFrameWidgetImpl::ProcessTouchAction(WebTouchAction touch_action) {
  widget_base_->ProcessTouchAction(touch_action);
}

void WebFrameWidgetImpl::DidHandleGestureEvent(const WebGestureEvent& event) {
#if defined(OS_ANDROID) || defined(USE_AURA)
  if (event.GetType() == WebInputEvent::Type::kGestureTap) {
    widget_base_->ShowVirtualKeyboard();
  } else if (event.GetType() == WebInputEvent::Type::kGestureLongPress) {
    WebInputMethodController* controller = GetActiveWebInputMethodController();
    if (!controller || controller->TextInputInfo().value.IsEmpty())
      widget_base_->UpdateTextInputState();
    else
      widget_base_->ShowVirtualKeyboard();
  }
#endif
}

void WebFrameWidgetImpl::SetHasPointerRawUpdateEventHandlers(
    bool has_handlers) {
  widget_base_->widget_input_handler_manager()
      ->input_event_queue()
      ->HasPointerRawUpdateEventHandlers(has_handlers);
}

void WebFrameWidgetImpl::SetNeedsLowLatencyInput(bool needs_low_latency) {
  widget_base_->widget_input_handler_manager()
      ->input_event_queue()
      ->SetNeedsLowLatency(needs_low_latency);
}

void WebFrameWidgetImpl::RequestUnbufferedInputEvents() {
  widget_base_->widget_input_handler_manager()
      ->input_event_queue()
      ->RequestUnbufferedInputEvents();
}

void WebFrameWidgetImpl::SetNeedsUnbufferedInputForDebugger(bool unbuffered) {
  widget_base_->widget_input_handler_manager()
      ->input_event_queue()
      ->SetNeedsUnbufferedInputForDebugger(unbuffered);
}

void WebFrameWidgetImpl::DidNavigate() {
  // The input handler wants to know about navigation so that it can
  // suppress input until the newly navigated page has a committed frame.
  // It also resets the state for UMA reporting of input arrival with respect
  // to document lifecycle.
  if (!widget_base_->widget_input_handler_manager())
    return;
  widget_base_->widget_input_handler_manager()->DidNavigate();
}

void WebFrameWidgetImpl::SetMouseCapture(bool capture) {
  if (mojom::blink::WidgetInputHandlerHost* host =
          widget_base_->widget_input_handler_manager()
              ->GetWidgetInputHandlerHost()) {
    host->SetMouseCapture(capture);
  }
}

gfx::Range WebFrameWidgetImpl::CompositionRange() {
  WebLocalFrame* focused_frame = FocusedWebLocalFrameInWidget();
  if (!focused_frame || ShouldDispatchImeEventsToPlugin())
    return gfx::Range::InvalidRange();

  blink::WebInputMethodController* controller =
      focused_frame->GetInputMethodController();
  WebRange web_range = controller->CompositionRange();
  if (web_range.IsNull())
    return gfx::Range::InvalidRange();
  return gfx::Range(web_range.StartOffset(), web_range.EndOffset());
}

void WebFrameWidgetImpl::GetCompositionCharacterBoundsInWindow(
    Vector<gfx::Rect>* bounds_in_dips) {
  WebLocalFrame* focused_frame = FocusedWebLocalFrameInWidget();
  if (!focused_frame || ShouldDispatchImeEventsToPlugin())
    return;
  blink::WebInputMethodController* controller =
      focused_frame->GetInputMethodController();
  blink::WebVector<blink::WebRect> bounds_from_blink;
  if (!controller->GetCompositionCharacterBounds(bounds_from_blink))
    return;

  for (auto& rect : bounds_from_blink) {
    bounds_in_dips->push_back(
        widget_base_->BlinkSpaceToEnclosedDIPs(gfx::Rect(rect)));
  }
}

void WebFrameWidgetImpl::AddImeTextSpansToExistingText(
    uint32_t start,
    uint32_t end,
    const Vector<ui::ImeTextSpan>& ime_text_spans) {
  WebLocalFrame* focused_frame = FocusedWebLocalFrameInWidget();
  if (!focused_frame)
    return;
  focused_frame->AddImeTextSpansToExistingText(ime_text_spans, start, end);
}

Vector<ui::mojom::blink::ImeTextSpanInfoPtr>
WebFrameWidgetImpl::GetImeTextSpansInfo(
    const WebVector<ui::ImeTextSpan>& ime_text_spans) {
  auto* focused_frame = FocusedWebLocalFrameInWidget();
  if (!focused_frame)
    return Vector<ui::mojom::blink::ImeTextSpanInfoPtr>();

  Vector<ui::mojom::blink::ImeTextSpanInfoPtr> ime_text_spans_info;

  for (const auto& ime_text_span : ime_text_spans) {
    WebRect webrect;
    unsigned length = ime_text_span.end_offset - ime_text_span.start_offset;
    focused_frame->FirstRectForCharacterRange(ime_text_span.start_offset,
                                              length, webrect);

    ime_text_spans_info.push_back(ui::mojom::blink::ImeTextSpanInfo::New(
        ime_text_span,
        widget_base_->BlinkSpaceToEnclosedDIPs(gfx::Rect(webrect))));
  }
  return ime_text_spans_info;
}

void WebFrameWidgetImpl::ClearImeTextSpansByType(uint32_t start,
                                                 uint32_t end,
                                                 ui::ImeTextSpan::Type type) {
  WebLocalFrame* focused_frame = FocusedWebLocalFrameInWidget();
  if (!focused_frame)
    return;
  focused_frame->ClearImeTextSpansByType(type, start, end);
}

void WebFrameWidgetImpl::SetCompositionFromExistingText(
    int32_t start,
    int32_t end,
    const Vector<ui::ImeTextSpan>& ime_text_spans) {
  WebLocalFrame* focused_frame = FocusedWebLocalFrameInWidget();
  if (!focused_frame)
    return;
  focused_frame->SetCompositionFromExistingText(start, end, ime_text_spans);
}

void WebFrameWidgetImpl::ExtendSelectionAndDelete(int32_t before,
                                                  int32_t after) {
  WebLocalFrame* focused_frame = FocusedWebLocalFrameInWidget();
  if (!focused_frame)
    return;
  focused_frame->ExtendSelectionAndDelete(before, after);
}

void WebFrameWidgetImpl::DeleteSurroundingText(int32_t before, int32_t after) {
  WebLocalFrame* focused_frame = FocusedWebLocalFrameInWidget();
  if (!focused_frame)
    return;
  focused_frame->DeleteSurroundingText(before, after);
}

void WebFrameWidgetImpl::DeleteSurroundingTextInCodePoints(int32_t before,
                                                           int32_t after) {
  WebLocalFrame* focused_frame = FocusedWebLocalFrameInWidget();
  if (!focused_frame)
    return;
  focused_frame->DeleteSurroundingTextInCodePoints(before, after);
}

void WebFrameWidgetImpl::SetEditableSelectionOffsets(int32_t start,
                                                     int32_t end) {
  WebLocalFrame* focused_frame = FocusedWebLocalFrameInWidget();
  if (!focused_frame)
    return;
  focused_frame->SetEditableSelectionOffsets(start, end);
}

void WebFrameWidgetImpl::ExecuteEditCommand(const String& command,
                                            const String& value) {
  WebLocalFrame* focused_frame = FocusedWebLocalFrameInWidget();
  if (!focused_frame)
    return;
  focused_frame->ExecuteCommand(command, value);
}

void WebFrameWidgetImpl::Undo() {
  WebLocalFrame* focused_frame = FocusedWebLocalFrameInWidget();
  if (!focused_frame)
    return;
  focused_frame->ExecuteCommand(WebString::FromLatin1("Undo"));
}

void WebFrameWidgetImpl::Redo() {
  WebLocalFrame* focused_frame = FocusedWebLocalFrameInWidget();
  if (!focused_frame)
    return;
  focused_frame->ExecuteCommand(WebString::FromLatin1("Redo"));
}

void WebFrameWidgetImpl::Cut() {
  WebLocalFrame* focused_frame = FocusedWebLocalFrameInWidget();
  if (!focused_frame)
    return;
  focused_frame->ExecuteCommand(WebString::FromLatin1("Cut"));
}

void WebFrameWidgetImpl::Copy() {
  WebLocalFrame* focused_frame = FocusedWebLocalFrameInWidget();
  if (!focused_frame)
    return;
  focused_frame->ExecuteCommand(WebString::FromLatin1("Copy"));
}

void WebFrameWidgetImpl::CopyToFindPboard() {
  WebLocalFrame* focused_frame = FocusedWebLocalFrameInWidget();
  if (!focused_frame)
    return;
  To<WebLocalFrameImpl>(focused_frame)->CopyToFindPboard();
}

void WebFrameWidgetImpl::Paste() {
  WebLocalFrame* focused_frame = FocusedWebLocalFrameInWidget();
  if (!focused_frame)
    return;
  focused_frame->ExecuteCommand(WebString::FromLatin1("Paste"));
}

void WebFrameWidgetImpl::PasteAndMatchStyle() {
  WebLocalFrame* focused_frame = FocusedWebLocalFrameInWidget();
  if (!focused_frame)
    return;
  focused_frame->ExecuteCommand(WebString::FromLatin1("PasteAndMatchStyle"));
}

void WebFrameWidgetImpl::Delete() {
  WebLocalFrame* focused_frame = FocusedWebLocalFrameInWidget();
  if (!focused_frame)
    return;
  focused_frame->ExecuteCommand(WebString::FromLatin1("Delete"));
}

void WebFrameWidgetImpl::SelectAll() {
  WebLocalFrame* focused_frame = FocusedWebLocalFrameInWidget();
  if (!focused_frame)
    return;
  focused_frame->ExecuteCommand(WebString::FromLatin1("SelectAll"));
}

void WebFrameWidgetImpl::CollapseSelection() {
  WebLocalFrame* focused_frame = FocusedWebLocalFrameInWidget();
  if (!focused_frame)
    return;
  const blink::WebRange& range =
      focused_frame->GetInputMethodController()->GetSelectionOffsets();
  if (range.IsNull())
    return;

  focused_frame->SelectRange(blink::WebRange(range.EndOffset(), 0),
                             blink::WebLocalFrame::kHideSelectionHandle,
                             mojom::blink::SelectionMenuBehavior::kHide);
}

void WebFrameWidgetImpl::Replace(const String& word) {
  WebLocalFrame* focused_frame = FocusedWebLocalFrameInWidget();
  if (!focused_frame)
    return;
  if (!focused_frame->HasSelection())
    focused_frame->SelectWordAroundCaret();
  focused_frame->ReplaceSelection(word);
  focused_frame->Client()->SyncSelectionIfRequired();
}

void WebFrameWidgetImpl::ReplaceMisspelling(const String& word) {
  WebLocalFrame* focused_frame = FocusedWebLocalFrameInWidget();
  if (!focused_frame)
    return;
  if (!focused_frame->HasSelection())
    return;
  focused_frame->ReplaceMisspelledRange(word);
}

void WebFrameWidgetImpl::SelectRange(const gfx::Point& base_in_dips,
                                     const gfx::Point& extent_in_dips) {
  WebLocalFrame* focused_frame = FocusedWebLocalFrameInWidget();
  if (!focused_frame)
    return;
  focused_frame->SelectRange(
      widget_base_->DIPsToRoundedBlinkSpace(base_in_dips),
      widget_base_->DIPsToRoundedBlinkSpace(extent_in_dips));
}

void WebFrameWidgetImpl::AdjustSelectionByCharacterOffset(
    int32_t start,
    int32_t end,
    mojom::blink::SelectionMenuBehavior selection_menu_behavior) {
  WebLocalFrame* focused_frame = FocusedWebLocalFrameInWidget();
  if (!focused_frame)
    return;
  blink::WebRange range =
      focused_frame->GetInputMethodController()->GetSelectionOffsets();
  if (range.IsNull())
    return;

  // Sanity checks to disallow empty and out of range selections.
  if (start - end > range.length() || range.StartOffset() + start < 0)
    return;

  // A negative adjust amount moves the selection towards the beginning of
  // the document, a positive amount moves the selection towards the end of
  // the document.
  focused_frame->SelectRange(blink::WebRange(range.StartOffset() + start,
                                             range.length() + end - start),
                             blink::WebLocalFrame::kPreserveHandleVisibility,
                             selection_menu_behavior);
}

void WebFrameWidgetImpl::MoveRangeSelectionExtent(
    const gfx::Point& extent_in_dips) {
  WebLocalFrame* focused_frame = FocusedWebLocalFrameInWidget();
  if (!focused_frame)
    return;
  focused_frame->MoveRangeSelectionExtent(
      widget_base_->DIPsToRoundedBlinkSpace(extent_in_dips));
}

void WebFrameWidgetImpl::ScrollFocusedEditableNodeIntoRect(
    const gfx::Rect& rect_in_dips) {
  WebLocalFrame* local_frame = FocusedWebLocalFrameInWidget();
  if (!local_frame)
    return;

  // OnSynchronizeVisualProperties does not call DidChangeVisibleViewport
  // on OOPIFs. Since we are starting a new scroll operation now, call
  // DidChangeVisibleViewport to ensure that we don't assume the element
  // is already in view and ignore the scroll.
  local_frame->Client()->ResetHasScrolledFocusedEditableIntoView();
  local_frame->Client()->ScrollFocusedEditableElementIntoRect(rect_in_dips);
}

void WebFrameWidgetImpl::ZoomToFindInPageRect(
    const WebRect& rect_in_root_frame) {
  if (ForMainFrame()) {
    View()->ZoomToFindInPageRect(rect_in_root_frame);
  } else {
    GetAssociatedFrameWidgetHost()->ZoomToFindInPageRectInMainFrame(
        gfx::Rect(rect_in_root_frame));
  }
}

void WebFrameWidgetImpl::MoveCaret(const gfx::Point& point_in_dips) {
  WebLocalFrame* focused_frame = FocusedWebLocalFrameInWidget();
  if (!focused_frame)
    return;
  focused_frame->MoveCaretSelection(
      widget_base_->DIPsToRoundedBlinkSpace(point_in_dips));
}

#if defined(OS_ANDROID)
void WebFrameWidgetImpl::SelectWordAroundCaret(
    SelectWordAroundCaretCallback callback) {
  auto* focused_frame = FocusedWebLocalFrameInWidget();
  if (!focused_frame) {
    std::move(callback).Run(false, 0, 0);
    return;
  }

  bool did_select = false;
  int start_adjust = 0;
  int end_adjust = 0;
  blink::WebRange initial_range = focused_frame->SelectionRange();
  SetHandlingInputEvent(true);
  if (!initial_range.IsNull())
    did_select = focused_frame->SelectWordAroundCaret();
  if (did_select) {
    blink::WebRange adjusted_range = focused_frame->SelectionRange();
    DCHECK(!adjusted_range.IsNull());
    start_adjust = adjusted_range.StartOffset() - initial_range.StartOffset();
    end_adjust = adjusted_range.EndOffset() - initial_range.EndOffset();
  }
  SetHandlingInputEvent(false);
  std::move(callback).Run(did_select, start_adjust, end_adjust);
}
#endif

void WebFrameWidgetImpl::ForEachRemoteFrameControlledByWidget(
    const base::RepeatingCallback<void(RemoteFrame*)>& callback) {
  ForEachRemoteFrameChildrenControlledByWidget(local_root_->GetFrame(),
                                               callback);
}

void WebFrameWidgetImpl::CalculateSelectionBounds(gfx::Rect& anchor_root_frame,
                                                  gfx::Rect& focus_root_frame) {
  const LocalFrame* local_frame = FocusedLocalFrameInWidget();
  if (!local_frame)
    return;

  IntRect anchor;
  IntRect focus;
  if (!local_frame->Selection().ComputeAbsoluteBounds(anchor, focus))
    return;

  // Apply the visual viewport for main frames this will apply the page scale.
  // For subframes it will just be a 1:1 transformation and the browser
  // will then apply later transformations to these rects.
  VisualViewport& visual_viewport = GetPage()->GetVisualViewport();
  anchor_root_frame = visual_viewport.RootFrameToViewport(
      local_frame->View()->ConvertToRootFrame(anchor));
  focus_root_frame = visual_viewport.RootFrameToViewport(
      local_frame->View()->ConvertToRootFrame(focus));
}

void WebFrameWidgetImpl::BatterySavingsChanged(WebBatterySavingsFlags savings) {
  widget_base_->LayerTreeHost()->SetEnableFrameRateThrottling(
      savings & kAllowReducedFrameRate);
}

const viz::LocalSurfaceId& WebFrameWidgetImpl::LocalSurfaceIdFromParent() {
  return widget_base_->local_surface_id_from_parent();
}

cc::LayerTreeHost* WebFrameWidgetImpl::LayerTreeHost() {
  return widget_base_->LayerTreeHost();
}

ScreenMetricsEmulator* WebFrameWidgetImpl::DeviceEmulator() {
  return device_emulator_;
}

bool WebFrameWidgetImpl::AutoResizeMode() {
  return View()->AutoResizeMode();
}

void WebFrameWidgetImpl::SetScreenMetricsEmulationParameters(
    bool enabled,
    const DeviceEmulationParams& params) {
  if (enabled)
    View()->ActivateDevToolsTransform(params);
  else
    View()->DeactivateDevToolsTransform();
}

void WebFrameWidgetImpl::SetScreenInfoAndSize(
    const ScreenInfo& screen_info,
    const gfx::Size& widget_size_in_dips,
    const gfx::Size& visible_viewport_size_in_dips) {
  // Emulation happens on regular main frames which don't use auto-resize mode.
  DCHECK(!AutoResizeMode());

  UpdateScreenInfo(screen_info);
  widget_base_->SetVisibleViewportSizeInDIPs(visible_viewport_size_in_dips);
  Resize(widget_base_->DIPsToCeiledBlinkSpace(widget_size_in_dips));
}

void WebFrameWidgetImpl::NotifyPageScaleFactorChanged(
    float page_scale_factor,
    bool is_pinch_gesture_active) {
  // Store the value to give to any new RemoteFrame that will be created as a
  // descendant of this widget.
  page_scale_factor_in_mainframe_ = page_scale_factor;
  is_pinch_gesture_active_in_mainframe_ = is_pinch_gesture_active;
  // Push the page scale factor down to any child RemoteFrames.
  // TODO(danakj): This ends up setting the page scale factor in the
  // RenderWidgetHost of the child WebFrameWidgetImpl, so that it can bounce
  // the value down to its WebFrameWidgetImpl. Since this is essentially a
  // global value per-page, we could instead store it once in the browser
  // (such as in RenderViewHost) and distribute it to each WebFrameWidgetImpl
  // from there.
  ForEachRemoteFrameControlledByWidget(WTF::BindRepeating(
      [](float page_scale_factor, bool is_pinch_gesture_active,
         RemoteFrame* remote_frame) {
        remote_frame->Client()->PageScaleFactorChanged(page_scale_factor,
                                                       is_pinch_gesture_active);
      },
      page_scale_factor, is_pinch_gesture_active));
}

void WebFrameWidgetImpl::SetPageScaleStateAndLimits(
    float page_scale_factor,
    bool is_pinch_gesture_active,
    float minimum,
    float maximum) {
  widget_base_->LayerTreeHost()->SetPageScaleFactorAndLimits(page_scale_factor,
                                                             minimum, maximum);

  // Only propagate page scale from the main frame.
  if (ForMainFrame()) {
    // If page scale hasn't changed, then just return without notifying
    // the remote frames.
    if (page_scale_factor == page_scale_factor_in_mainframe_ &&
        is_pinch_gesture_active == is_pinch_gesture_active_in_mainframe_) {
      return;
    }

    NotifyPageScaleFactorChanged(page_scale_factor, is_pinch_gesture_active);
  }
}

bool WebFrameWidgetImpl::UpdateScreenRects(
    const gfx::Rect& widget_screen_rect,
    const gfx::Rect& window_screen_rect) {
  if (!device_emulator_)
    return false;
  device_emulator_->OnUpdateScreenRects(widget_screen_rect, window_screen_rect);
  return true;
}

void WebFrameWidgetImpl::OrientationChanged() {
  local_root_->SendOrientationChangeEvent();
}

void WebFrameWidgetImpl::DidUpdateSurfaceAndScreen(
    const ScreenInfo& previous_original_screen_info) {
  ScreenInfo screen_info = widget_base_->GetScreenInfo();
  if (Platform::Current()->IsUseZoomForDSFEnabled()) {
    View()->SetZoomFactorForDeviceScaleFactor(screen_info.device_scale_factor);
  } else {
    View()->SetDeviceScaleFactor(screen_info.device_scale_factor);
  }

  if (Client()->ShouldAutoDetermineCompositingToLCDTextSetting()) {
    // This causes compositing state to be modified which dirties the
    // document lifecycle. Android Webview relies on the document
    // lifecycle being clean after the RenderWidget is initialized, in
    // order to send IPCs that query and change compositing state. So
    // WebFrameWidgetImpl::Resize() must come after this call, as it runs the
    // entire document lifecycle.
    View()->GetSettings()->SetPreferCompositingToLCDTextEnabled(
        widget_base_->ComputePreferCompositingToLCDText());
  }

  // When the device scale changes, the size and position of the popup would
  // need to be adjusted, which we can't do. Just close the popup, which is
  // also consistent with page zoom and resize behavior.
  if (previous_original_screen_info.device_scale_factor !=
      screen_info.device_scale_factor) {
    View()->CancelPagePopup();
  }

  // Propagate changes down to child local root RenderWidgets and BrowserPlugins
  // in other frame trees/processes.
  ScreenInfo original_screen_info = GetOriginalScreenInfo();
  if (previous_original_screen_info != original_screen_info) {
    ForEachRemoteFrameControlledByWidget(WTF::BindRepeating(
        [](const ScreenInfo& original_screen_info, RemoteFrame* remote_frame) {
          remote_frame->Client()->DidChangeScreenInfo(original_screen_info);
        },
        original_screen_info));
  }
}

gfx::Rect WebFrameWidgetImpl::ViewportVisibleRect() {
  if (ForMainFrame()) {
    return widget_base_->CompositorViewportRect();
  } else {
    return child_data().compositor_visible_rect;
  }
}

const ScreenInfo& WebFrameWidgetImpl::GetOriginalScreenInfo() {
  if (device_emulator_)
    return device_emulator_->original_screen_info();
  return widget_base_->GetScreenInfo();
}

base::Optional<blink::mojom::ScreenOrientation>
WebFrameWidgetImpl::ScreenOrientationOverride() {
  return View()->ScreenOrientationOverride();
}

void WebFrameWidgetImpl::WasHidden() {
  ForEachLocalFrameControlledByWidget(
      local_root_->GetFrame(),
      WTF::BindRepeating([](WebLocalFrame* local_frame) {
        local_frame->Client()->WasHidden();
      }));
}

void WebFrameWidgetImpl::WasShown(bool was_evicted) {
  ForEachLocalFrameControlledByWidget(
      local_root_->GetFrame(),
      WTF::BindRepeating([](WebLocalFrame* local_frame) {
        local_frame->Client()->WasShown();
      }));
  if (was_evicted) {
    ForEachRemoteFrameControlledByWidget(
        WTF::BindRepeating([](RemoteFrame* remote_frame) {
          remote_frame->Client()->WasEvicted();
        }));
  }
}

void WebFrameWidgetImpl::RunPaintBenchmark(int repeat_count,
                                           cc::PaintBenchmarkResult& result) {
  if (!ForMainFrame())
    return;
  if (auto* frame_view = LocalRootImpl()->GetFrameView())
    frame_view->RunPaintBenchmark(repeat_count, result);
}

void WebFrameWidgetImpl::NotifyInputObservers(
    const WebCoalescedInputEvent& coalesced_event) {
  LocalFrame* frame = FocusedLocalFrameInWidget();
  if (!frame)
    return;

  LocalFrameView* frame_view = frame->View();
  if (!frame_view)
    return;

  const WebInputEvent& input_event = coalesced_event.Event();
  auto& paint_timing_detector = frame_view->GetPaintTimingDetector();

  if (paint_timing_detector.NeedToNotifyInputOrScroll())
    paint_timing_detector.NotifyInputEvent(input_event.GetType());
}

Frame* WebFrameWidgetImpl::FocusedCoreFrame() const {
  return GetPage() ? GetPage()->GetFocusController().FocusedOrMainFrame()
                   : nullptr;
}

Element* WebFrameWidgetImpl::FocusedElement() const {
  LocalFrame* frame = GetPage()->GetFocusController().FocusedFrame();
  if (!frame)
    return nullptr;

  Document* document = frame->GetDocument();
  if (!document)
    return nullptr;

  return document->FocusedElement();
}

HitTestResult WebFrameWidgetImpl::HitTestResultForRootFramePos(
    const FloatPoint& pos_in_root_frame) {
  FloatPoint doc_point =
      LocalRootImpl()->GetFrame()->View()->ConvertFromRootFrame(
          pos_in_root_frame);
  HitTestLocation location(doc_point);
  HitTestResult result =
      LocalRootImpl()->GetFrame()->View()->HitTestWithThrottlingAllowed(
          location, HitTestRequest::kReadOnly | HitTestRequest::kActive);
  result.SetToShadowHostIfInRestrictedShadowRoot();
  return result;
}

bool WebFrameWidgetImpl::SynchronousResizeModeForTestingEnabled() {
  return main_data().synchronous_resize_mode_for_testing;
}

KURL WebFrameWidgetImpl::GetURLForDebugTrace() {
  WebFrame* main_frame = View()->MainFrame();
  if (main_frame->IsWebLocalFrame())
    return main_frame->ToWebLocalFrame()->GetDocument().Url();
  return {};
}

float WebFrameWidgetImpl::GetTestingDeviceScaleFactorOverride() {
  return device_scale_factor_for_testing_;
}

void WebFrameWidgetImpl::ReleaseMouseLockAndPointerCaptureForTesting() {
  GetPage()->GetPointerLockController().ExitPointerLock();
  MouseCaptureLost();
}

const viz::FrameSinkId& WebFrameWidgetImpl::GetFrameSinkId() {
  // It is valid to create a WebFrameWidget with an invalid frame sink id for
  // printing and placeholders. But if we go to use it, it should be valid.
  DCHECK(frame_sink_id_.is_valid());
  return frame_sink_id_;
}

WebHitTestResult WebFrameWidgetImpl::HitTestResultAt(const gfx::PointF& point) {
  return CoreHitTestResultAt(point);
}

void WebFrameWidgetImpl::SetZoomLevelForTesting(double zoom_level) {
  DCHECK(ForMainFrame());
  DCHECK_NE(zoom_level, -INFINITY);
  zoom_level_for_testing_ = zoom_level;
  SetZoomLevel(zoom_level);
}

void WebFrameWidgetImpl::ResetZoomLevelForTesting() {
  DCHECK(ForMainFrame());
  zoom_level_for_testing_ = -INFINITY;
  SetZoomLevel(0);
}

void WebFrameWidgetImpl::SetDeviceScaleFactorForTesting(float factor) {
  DCHECK(ForMainFrame());
  DCHECK_GE(factor, 0.f);

  // Stash the window size before we adjust the scale factor, as subsequent
  // calls to convert will use the new scale factor.
  gfx::Size size_in_dips = widget_base_->BlinkSpaceToFlooredDIPs(Size());
  device_scale_factor_for_testing_ = factor;

  // Receiving a 0 is used to reset between tests, it removes the override in
  // order to listen to the browser for the next test.
  if (!factor)
    return;

  // We are changing the device scale factor from the renderer, so allocate a
  // new viz::LocalSurfaceId to avoid surface invariants violations in tests.
  widget_base_->LayerTreeHost()->RequestNewLocalSurfaceId();

  ScreenInfo info = widget_base_->GetScreenInfo();
  info.device_scale_factor = factor;
  gfx::Size size_with_dsf = gfx::ScaleToCeiledSize(size_in_dips, factor);
  widget_base_->UpdateCompositorViewportAndScreenInfo(gfx::Rect(size_with_dsf),
                                                      info);
  if (!AutoResizeMode()) {
    // This picks up the new device scale factor as
    // `UpdateCompositorViewportAndScreenInfo()` has applied a new value.
    Resize(widget_base_->DIPsToCeiledBlinkSpace(size_in_dips));
  }
}

WebPlugin* WebFrameWidgetImpl::GetFocusedPluginContainer() {
  LocalFrame* focused_frame = FocusedLocalFrameInWidget();
  if (!focused_frame)
    return nullptr;
  if (auto* container = focused_frame->GetWebPluginContainer())
    return container->Plugin();
  return nullptr;
}

bool WebFrameWidgetImpl::CanComposeInline() {
  if (auto* plugin = GetFocusedPluginContainer())
    return plugin->CanComposeInline();
  return true;
}

bool WebFrameWidgetImpl::ShouldDispatchImeEventsToPlugin() {
  if (auto* plugin = GetFocusedPluginContainer())
    return plugin->ShouldDispatchImeEventsToPlugin();
  return false;
}

void WebFrameWidgetImpl::ImeSetCompositionForPlugin(
    const String& text,
    const Vector<ui::ImeTextSpan>& ime_text_spans,
    const gfx::Range& replacement_range,
    int selection_start,
    int selection_end) {
  if (auto* plugin = GetFocusedPluginContainer()) {
    plugin->ImeSetCompositionForPlugin(
        text,
        std::vector<ui::ImeTextSpan>(ime_text_spans.begin(),
                                     ime_text_spans.end()),
        replacement_range, selection_start, selection_end);
  }
}

void WebFrameWidgetImpl::ImeCommitTextForPlugin(
    const String& text,
    const Vector<ui::ImeTextSpan>& ime_text_spans,
    const gfx::Range& replacement_range,
    int relative_cursor_pos) {
  if (auto* plugin = GetFocusedPluginContainer()) {
    plugin->ImeCommitTextForPlugin(
        text,
        std::vector<ui::ImeTextSpan>(ime_text_spans.begin(),
                                     ime_text_spans.end()),
        replacement_range, relative_cursor_pos);
  }
}

void WebFrameWidgetImpl::ImeFinishComposingTextForPlugin(bool keep_selection) {
  if (auto* plugin = GetFocusedPluginContainer())
    plugin->ImeFinishComposingTextForPlugin(keep_selection);
}

void WebFrameWidgetImpl::SetWindowRect(const gfx::Rect& window_rect) {
  DCHECK(ForMainFrame());
  if (SynchronousResizeModeForTestingEnabled()) {
    // This is a web-test-only path. At one point, it was planned to be
    // removed. See https://crbug.com/309760.
    SetWindowRectSynchronously(window_rect);
    return;
  }

  SetPendingWindowRect(window_rect);
  View()->SendWindowRectToMainFrameHost(
      window_rect, WTF::Bind(&WebFrameWidgetImpl::AckPendingWindowRect,
                             WrapWeakPersistent(this)));
}

void WebFrameWidgetImpl::SetWindowRectSynchronouslyForTesting(
    const gfx::Rect& new_window_rect) {
  DCHECK(ForMainFrame());
  SetWindowRectSynchronously(new_window_rect);
}

void WebFrameWidgetImpl::SetWindowRectSynchronously(
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
  widget_base_->LayerTreeHost()->RequestNewLocalSurfaceId();

  gfx::Rect compositor_viewport_pixel_rect(gfx::ScaleToCeiledSize(
      new_window_rect.size(),
      widget_base_->GetScreenInfo().device_scale_factor));
  widget_base_->UpdateSurfaceAndScreenInfo(
      widget_base_->local_surface_id_from_parent(),
      compositor_viewport_pixel_rect, widget_base_->GetScreenInfo());

  Resize(new_window_rect.size());
  widget_base_->SetScreenRects(new_window_rect, new_window_rect);
}

void WebFrameWidgetImpl::DidCreateLocalRootView() {
  // If this WebWidget still hasn't received its size from the embedder, block
  // the parser. This is necessary, because the parser can cause layout to
  // happen, which needs to be done with the correct size.
  if (ForSubframe() && !size_) {
    child_data().did_suspend_parsing = true;
    LocalRootImpl()->GetFrame()->Loader().GetDocumentLoader()->BlockParser();
  }
}

mojom::blink::ScrollIntoViewParamsPtr
WebFrameWidgetImpl::GetScrollParamsForFocusedEditableElement(
    const Element& element,
    PhysicalRect& out_rect_to_scroll) {
  // For main frames, scrolling takes place in two phases.
  if (ForMainFrame()) {
    // Since the page has been resized, the layout may have changed. The page
    // scale animation started by ZoomAndScrollToFocusedEditableRect will scroll
    // only the visual and layout viewports. We'll call ScrollRectToVisible with
    // the stop_at_main_frame_layout_viewport param to ensure the element is
    // actually visible in the page.
    mojom::blink::ScrollIntoViewParamsPtr params =
        ScrollAlignment::CreateScrollIntoViewParams(
            ScrollAlignment::CenterIfNeeded(),
            ScrollAlignment::CenterIfNeeded(),
            mojom::blink::ScrollType::kProgrammatic, false,
            mojom::blink::ScrollBehavior::kInstant);
    params->stop_at_main_frame_layout_viewport = true;
    out_rect_to_scroll =
        PhysicalRect(element.GetLayoutObject()->AbsoluteBoundingBoxRect());
    return params;
  }

  LocalFrameView& frame_view = *element.GetDocument().View();
  IntRect absolute_element_bounds =
      element.GetLayoutObject()->AbsoluteBoundingBoxRect();
  IntRect absolute_caret_bounds =
      element.GetDocument().GetFrame()->Selection().AbsoluteCaretBounds();
  // Ideally, the chosen rectangle includes the element box and caret bounds
  // plus some margin on the left. If this does not work (i.e., does not fit
  // inside the frame view), then choose a subrect which includes the caret
  // bounds. It is preferable to also include element bounds' location and left
  // align the scroll. If this cant be satisfied, the scroll will be right
  // aligned.
  IntRect maximal_rect =
      UnionRect(absolute_element_bounds, absolute_caret_bounds);

  // Set the ideal margin.
  maximal_rect.ShiftXEdgeTo(
      maximal_rect.X() -
      static_cast<int>(kIdealPaddingRatio * absolute_element_bounds.Width()));

  bool maximal_rect_fits_in_frame =
      !(frame_view.Size() - maximal_rect.Size()).IsEmpty();

  if (!maximal_rect_fits_in_frame) {
    IntRect frame_rect(maximal_rect.Location(), frame_view.Size());
    maximal_rect.Intersect(frame_rect);
    IntPoint point_forced_to_be_visible =
        absolute_caret_bounds.MaxXMaxYCorner() +
        IntSize(kCaretPadding, kCaretPadding);
    if (!maximal_rect.Contains(point_forced_to_be_visible)) {
      // Move the rect towards the point until the point is barely contained.
      maximal_rect.Move(point_forced_to_be_visible -
                        maximal_rect.MaxXMaxYCorner());
    }
  }

  mojom::blink::ScrollIntoViewParamsPtr params =
      ScrollAlignment::CreateScrollIntoViewParams();
  params->zoom_into_rect = View()->ShouldZoomToLegibleScale(element);
  params->relative_element_bounds = NormalizeRect(
      Intersection(absolute_element_bounds, maximal_rect), maximal_rect);
  params->relative_caret_bounds = NormalizeRect(
      Intersection(absolute_caret_bounds, maximal_rect), maximal_rect);
  params->behavior = mojom::blink::ScrollBehavior::kInstant;
  out_rect_to_scroll = PhysicalRect(maximal_rect);
  return params;
}

}  // namespace blink
