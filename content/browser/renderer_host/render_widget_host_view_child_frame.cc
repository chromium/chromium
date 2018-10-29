// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/debug/dump_without_crashing.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/renderer_host/cursor_manager.h"
#include "content/browser/renderer_host/display_util.h"
#include "content/browser/renderer_host/frame_connector_delegate.h"
#include "content/browser/renderer_host/input/touch_selection_controller_client_child_frame.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/browser/renderer_host/render_widget_host_view_event_handler.h"
#include "content/browser/renderer_host/text_input_manager.h"
#include "content/common/text_input_state.h"
#include "content/common/widget_messages.h"
#include "content/public/browser/guest_mode.h"
#include "content/public/browser/render_process_host.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "services/service_manager/runner/common/client_util.h"
#include "third_party/blink/public/platform/web_touch_event.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/touch_selection/touch_selection_controller.h"

#if defined(USE_AURA)
#include "services/ws/public/mojom/window_tree.mojom.h"
#include "ui/aura/env.h"
#endif

namespace content {

// static
RenderWidgetHostViewChildFrame* RenderWidgetHostViewChildFrame::Create(
    RenderWidgetHost* widget) {
  RenderWidgetHostViewChildFrame* view =
      new RenderWidgetHostViewChildFrame(widget);
  view->Init();
  return view;
}

RenderWidgetHostViewChildFrame::RenderWidgetHostViewChildFrame(
    RenderWidgetHost* widget_host)
    : RenderWidgetHostViewBase(widget_host),
      frame_sink_id_(
          base::checked_cast<uint32_t>(widget_host->GetProcess()->GetID()),
          base::checked_cast<uint32_t>(widget_host->GetRoutingID())),
      frame_connector_(nullptr),
      enable_viz_(
          base::FeatureList::IsEnabled(features::kVizDisplayCompositor)),
      weak_factory_(this) {
  if (features::IsMultiProcessMash()) {
    // In Mus the RenderFrameProxy will eventually assign a viz::FrameSinkId
    // until then set ours invalid, as operations using it will be disregarded.
    frame_sink_id_ = viz::FrameSinkId();
  } else {
    GetHostFrameSinkManager()->RegisterFrameSinkId(
        frame_sink_id_, this, viz::ReportFirstSurfaceActivation::kYes);
    GetHostFrameSinkManager()->SetFrameSinkDebugLabel(
        frame_sink_id_, "RenderWidgetHostViewChildFrame");
    CreateCompositorFrameSinkSupport();
  }
}

RenderWidgetHostViewChildFrame::~RenderWidgetHostViewChildFrame() {
  // TODO(wjmaclean): The next two lines are a speculative fix for
  // https://crbug.com/760074, based on the theory that perhaps something is
  // destructing the class without calling Destroy() first.
  if (frame_connector_)
    DetachFromTouchSelectionClientManagerIfNecessary();

  if (!features::IsMultiProcessMash()) {
    ResetCompositorFrameSinkSupport();
    if (GetHostFrameSinkManager())
      GetHostFrameSinkManager()->InvalidateFrameSinkId(frame_sink_id_);
  }
}

void RenderWidgetHostViewChildFrame::Init() {
  RegisterFrameSinkId();
  host()->SetView(this);
  GetTextInputManager();
}

void RenderWidgetHostViewChildFrame::
    DetachFromTouchSelectionClientManagerIfNecessary() {
  if (!selection_controller_client_)
    return;

  auto* root_view = frame_connector_->GetRootRenderWidgetHostView();
  if (root_view) {
    auto* manager = root_view->GetTouchSelectionControllerClientManager();
    if (manager)
      manager->RemoveObserver(this);
  } else {
    // We should never get here, but maybe we are? Test this out with a
    // diagnostic we can track. If we do get here, it would explain
    // https://crbug.com/760074.
    base::debug::DumpWithoutCrashing();
  }

  selection_controller_client_.reset();
}

void RenderWidgetHostViewChildFrame::SetFrameConnectorDelegate(
    FrameConnectorDelegate* frame_connector) {
  if (frame_connector_ == frame_connector)
    return;

  if (frame_connector_) {
    SetParentFrameSinkId(viz::FrameSinkId());

    // Unlocks the mouse if this RenderWidgetHostView holds the lock.
    UnlockMouse();
    DetachFromTouchSelectionClientManagerIfNecessary();
  }
  frame_connector_ = frame_connector;
  if (!frame_connector_)
    return;

  RenderWidgetHostViewBase* parent_view =
      frame_connector_->GetParentRenderWidgetHostView();

  if (parent_view) {
    DCHECK(parent_view->GetFrameSinkId().is_valid() ||
           features::IsMultiProcessMash());
    SetParentFrameSinkId(parent_view->GetFrameSinkId());
  }

  current_device_scale_factor_ =
      frame_connector_->screen_info().device_scale_factor;

  auto* root_view = frame_connector_->GetRootRenderWidgetHostView();
  if (root_view) {
    auto* manager = root_view->GetTouchSelectionControllerClientManager();
    if (manager) {
      // We have managers in Aura and Android, as well as outside of content/.
      // There is no manager for Mac OS.
      selection_controller_client_ =
          std::make_unique<TouchSelectionControllerClientChildFrame>(this,
                                                                     manager);
      manager->AddObserver(this);
    }
  }

#if defined(USE_AURA)
  if (features::IsMultiProcessMash()) {
    frame_connector_->EmbedRendererWindowTreeClientInParent(
        GetWindowTreeClientFromRenderer());
  }
#endif

  SendSurfaceInfoToEmbedder();
}

#if defined(USE_AURA)
void RenderWidgetHostViewChildFrame::SetFrameSinkId(
    const viz::FrameSinkId& frame_sink_id) {
  if (features::IsMultiProcessMash())
    frame_sink_id_ = frame_sink_id;
}
#endif  // defined(USE_AURA)

void RenderWidgetHostViewChildFrame::UpdateIntrinsicSizingInfo(
    const blink::WebIntrinsicSizingInfo& sizing_info) {
  if (frame_connector_)
    frame_connector_->SendIntrinsicSizingInfoToParent(sizing_info);
}

void RenderWidgetHostViewChildFrame::OnManagerWillDestroy(
    TouchSelectionControllerClientManager* manager) {
  // We get the manager via the observer callback instead of through the
  // frame_connector_ since our connection to the root_view may disappear by
  // the time this function is called, but before frame_connector_ is reset.
  manager->RemoveObserver(this);
  selection_controller_client_.reset();
}

void RenderWidgetHostViewChildFrame::InitAsChild(gfx::NativeView parent_view) {
  NOTREACHED();
}

void RenderWidgetHostViewChildFrame::SetSize(const gfx::Size& size) {
  // Resizing happens in CrossProcessFrameConnector for child frames.
}

void RenderWidgetHostViewChildFrame::SetBounds(const gfx::Rect& rect) {
  // Resizing happens in CrossProcessFrameConnector for child frames.
  if (rect != last_screen_rect_) {
    last_screen_rect_ = rect;
    host()->SendScreenRects();
  }
}

void RenderWidgetHostViewChildFrame::Focus() {}

bool RenderWidgetHostViewChildFrame::HasFocus() const {
  if (frame_connector_)
    return frame_connector_->HasFocus();
  return false;
}

bool RenderWidgetHostViewChildFrame::IsSurfaceAvailableForCopy() const {
  return has_frame_;
}

void RenderWidgetHostViewChildFrame::EnsureSurfaceSynchronizedForLayoutTest() {
  // The capture sequence number which would normally be updated here is
  // actually retrieved from the frame connector.
}

uint32_t RenderWidgetHostViewChildFrame::GetCaptureSequenceNumber() const {
  if (!frame_connector_)
    return 0u;
  return frame_connector_->capture_sequence_number();
}

void RenderWidgetHostViewChildFrame::Show() {
  if (!host()->is_hidden())
    return;

  if (!CanBecomeVisible())
    return;

  host()->WasShown(false /* record_presentation_time */);

  if (frame_connector_)
    frame_connector_->SetVisibilityForChildViews(true);
}

void RenderWidgetHostViewChildFrame::Hide() {
  if (host()->is_hidden())
    return;
  host()->WasHidden();

  if (frame_connector_)
    frame_connector_->SetVisibilityForChildViews(false);
}

bool RenderWidgetHostViewChildFrame::IsShowing() {
  return !host()->is_hidden();
}

gfx::Rect RenderWidgetHostViewChildFrame::GetViewBounds() const {
  gfx::Rect rect;
  if (frame_connector_) {
    rect = frame_connector_->screen_space_rect_in_dip();

    RenderWidgetHostView* parent_view =
        frame_connector_->GetParentRenderWidgetHostView();

    // The parent_view can be null in tests when using a TestWebContents.
    if (parent_view) {
      // Translate screen_space_rect by the parent's RenderWidgetHostView
      // offset.
      rect.Offset(parent_view->GetViewBounds().OffsetFromOrigin());
    }
    // TODO(fsamuel): GetViewBounds is a bit of a mess. It's used to determine
    // the size of the renderer content and where to place context menus and so
    // on. We want the location of the frame in screen coordinates to place
    // popups but we want the size in local coordinates to produce the right-
    // sized CompositorFrames.
    rect.set_size(frame_connector_->local_frame_size_in_dip());
  }
  return rect;
}

gfx::Size RenderWidgetHostViewChildFrame::GetVisibleViewportSize() const {
  // For subframes, the visual viewport corresponds to the main frame size, so
  // this bubbles up to the parent until it hits the main frame's
  // RenderWidgetHostView.
  //
  // Currently this excludes webview guests, since they expect the visual
  // viewport to return the guest's size rather than the page's; one reason why
  // is that Blink ends up using the visual viewport to calculate things like
  // window.innerWidth/innerHeight for main frames, and a guest is considered
  // to be a main frame.  This should be cleaned up eventually.
  bool is_guest = BrowserPluginGuest::IsGuest(RenderViewHostImpl::From(host()));
  if (frame_connector_ && !is_guest) {
    // An auto-resize set by the top-level frame overrides what would be
    // reported by embedding RenderWidgetHostViews.
    if (host()->delegate() &&
        !host()->delegate()->GetAutoResizeSize().IsEmpty())
      return host()->delegate()->GetAutoResizeSize();

    RenderWidgetHostView* parent_view =
        frame_connector_->GetParentRenderWidgetHostView();
    // The parent_view can be null in unit tests when using a TestWebContents.
    if (parent_view)
      return parent_view->GetVisibleViewportSize();
  }

  gfx::Rect bounds = GetViewBounds();

  // It doesn't make sense to set insets on an OOP iframe. The only time this
  // should happen is when the virtual keyboard comes up on a <webview>.
  if (is_guest)
    bounds.Inset(insets_);

  return bounds.size();
}

void RenderWidgetHostViewChildFrame::SetInsets(const gfx::Insets& insets) {
  // Insets are used only for <webview> and are used to let the UI know it's
  // being obscured (for e.g. by the virtual keyboard).
  insets_ = insets;
  host()->SynchronizeVisualProperties(!insets_.IsEmpty());
}

gfx::NativeView RenderWidgetHostViewChildFrame::GetNativeView() const {
  // TODO(ekaramad): To accomodate MimeHandlerViewGuest while embedded inside
  // OOPIF-webview, we need to return the native view to be used by
  // RenderWidgetHostViewGuest. Remove this once https://crbug.com/642826 is
  // fixed.
  if (!frame_connector_)
    return nullptr;

  RenderWidgetHostView* parent_view =
      frame_connector_->GetParentRenderWidgetHostView();
  return parent_view ? parent_view->GetNativeView() : nullptr;
}

gfx::NativeViewAccessible
RenderWidgetHostViewChildFrame::GetNativeViewAccessible() {
  NOTREACHED();
  return nullptr;
}

void RenderWidgetHostViewChildFrame::UpdateBackgroundColor() {
  DCHECK(GetBackgroundColor());

  SkColor color = *GetBackgroundColor();
  DCHECK(SkColorGetA(color) == SK_AlphaOPAQUE ||
         SkColorGetA(color) == SK_AlphaTRANSPARENT);
  host()->SetBackgroundOpaque(SkColorGetA(color) == SK_AlphaOPAQUE);
}

gfx::Size RenderWidgetHostViewChildFrame::GetCompositorViewportPixelSize()
    const {
  if (frame_connector_)
    return frame_connector_->local_frame_size_in_pixels();
  return gfx::Size();
}

RenderWidgetHostViewBase* RenderWidgetHostViewChildFrame::GetRootView() {
  return frame_connector_ ? frame_connector_->GetRootRenderWidgetHostView()
                          : nullptr;
}

void RenderWidgetHostViewChildFrame::InitAsPopup(
    RenderWidgetHostView* parent_host_view,
    const gfx::Rect& bounds) {
  NOTREACHED();
}

void RenderWidgetHostViewChildFrame::InitAsFullscreen(
    RenderWidgetHostView* reference_host_view) {
  NOTREACHED();
}

void RenderWidgetHostViewChildFrame::UpdateCursor(const WebCursor& cursor) {
  if (frame_connector_)
    frame_connector_->UpdateCursor(cursor);
}

void RenderWidgetHostViewChildFrame::SetIsLoading(bool is_loading) {
  // It is valid for an inner WebContents's SetIsLoading() to end up here.
  // This is because an inner WebContents's main frame's RenderWidgetHostView
  // is a RenderWidgetHostViewChildFrame. In contrast, when there is no
  // inner/outer WebContents, only subframe's RenderWidgetHostView can be a
  // RenderWidgetHostViewChildFrame which do not get a SetIsLoading() call.
}

void RenderWidgetHostViewChildFrame::RenderProcessGone(
    base::TerminationStatus status,
    int error_code) {
  if (frame_connector_)
    frame_connector_->RenderProcessGone();
  Destroy();
}

void RenderWidgetHostViewChildFrame::Destroy() {
  // FrameSinkIds registered with RenderWidgetHostInputEventRouter
  // have already been cleared when RenderWidgetHostViewBase notified its
  // observers of our impending destruction.
  if (frame_connector_) {
    frame_connector_->SetView(nullptr);
    SetFrameConnectorDelegate(nullptr);
  }

  // We notify our observers about shutdown here since we are about to release
  // host_ and do not want any event calls coming from
  // RenderWidgetHostInputEventRouter afterwards.
  NotifyObserversAboutShutdown();

  RenderWidgetHostViewBase::Destroy();

  delete this;
}

void RenderWidgetHostViewChildFrame::SetTooltipText(
    const base::string16& tooltip_text) {
  if (!frame_connector_)
    return;

  auto* root_view = frame_connector_->GetRootRenderWidgetHostView();
  if (!root_view)
    return;

  auto* cursor_manager = root_view->GetCursorManager();
  // If there's no CursorManager then we're on Android, and setting tooltips
  // is a null-opt there, so it's ok to early out.
  if (!cursor_manager)
    return;

  cursor_manager->SetTooltipTextForView(this, tooltip_text);
}

RenderWidgetHostViewBase* RenderWidgetHostViewChildFrame::GetParentView() {
  if (!frame_connector_)
    return nullptr;
  return frame_connector_->GetParentRenderWidgetHostView();
}

void RenderWidgetHostViewChildFrame::RegisterFrameSinkId() {
  // If Destroy() has been called before we get here, host_ may be null.
  if (host() && host()->delegate() &&
      host()->delegate()->GetInputEventRouter()) {
    RenderWidgetHostInputEventRouter* router =
        host()->delegate()->GetInputEventRouter();
    if (!router->is_registered(frame_sink_id_))
      router->AddFrameSinkIdOwner(frame_sink_id_, this);
  }
}

void RenderWidgetHostViewChildFrame::UnregisterFrameSinkId() {
  DCHECK(host());
  if (host()->delegate() && host()->delegate()->GetInputEventRouter()) {
    host()->delegate()->GetInputEventRouter()->RemoveFrameSinkIdOwner(
        frame_sink_id_);
    DetachFromTouchSelectionClientManagerIfNecessary();
  }
}

void RenderWidgetHostViewChildFrame::UpdateViewportIntersection(
    const gfx::Rect& viewport_intersection,
    const gfx::Rect& compositor_visible_rect,
    bool occluded_or_obscured) {
  if (host()) {
    host()->SetIntersectsViewport(!viewport_intersection.IsEmpty());
    host()->Send(new WidgetMsg_SetViewportIntersection(
        host()->GetRoutingID(), viewport_intersection, compositor_visible_rect,
        occluded_or_obscured));
  }
}

void RenderWidgetHostViewChildFrame::SetIsInert() {
  if (host() && frame_connector_) {
    host()->Send(new WidgetMsg_SetIsInert(host()->GetRoutingID(),
                                          frame_connector_->IsInert()));
  }
}

void RenderWidgetHostViewChildFrame::UpdateInheritedEffectiveTouchAction() {
  if (host_ && frame_connector_) {
    host_->Send(new WidgetMsg_SetInheritedEffectiveTouchAction(
        host_->GetRoutingID(),
        frame_connector_->InheritedEffectiveTouchAction()));
  }
}

void RenderWidgetHostViewChildFrame::UpdateRenderThrottlingStatus() {
  if (host() && frame_connector_) {
    host()->Send(new WidgetMsg_UpdateRenderThrottlingStatus(
        host()->GetRoutingID(), frame_connector_->IsThrottled(),
        frame_connector_->IsSubtreeThrottled()));
  }
}

void RenderWidgetHostViewChildFrame::GestureEventAck(
    const blink::WebGestureEvent& event,
    InputEventAckState ack_result) {
  if (!frame_connector_)
    return;

  if (event.IsTouchpadZoomEvent())
    ProcessTouchpadZoomEventAckInRoot(event, ack_result);

  const bool should_bubble =
      ack_result == INPUT_EVENT_ACK_STATE_NOT_CONSUMED ||
      ack_result == INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS ||
      ack_result == INPUT_EVENT_ACK_STATE_CONSUMED_SHOULD_BUBBLE;

  bool touchpad_fling_start_bubbles = false;
// GSU events with inertial phase are generated from touchpad fling only on
// ChromeOS. Other platforms either don't have touchpad-based GSUs with inertial
// phase (e.g., Linux/Android, or Windows with low precision touchpads) or
// generate them from the OS rather than the fling controller (e.g., Mac, or
// Windows with high precision touchpads). |touchpad_fling_start_bubbles| is
// true on ChromeOS to show that on this platform we do not need to bubble
// touchpad-based GSU events with inertial phase, since touchpad-based fling
// start gets bubbled instead and the fling controller of the parent will take
// care of touchpad-based inertial GSUs event creation from the bubbled touchpad
// fling.
#if defined(CHROMEOS)
  touchpad_fling_start_bubbles = true;
#endif  // defined(CHROMEOS)

  // When a GFS is bubbled, we still send it to the fling controller of the
  // child view to finish the scroll sequence. However the GSU and GSE events
  // that are generated by the child view's fling controller do not need to get
  // bubbled since the GFS event itself is bubbled and the target's fling
  // controller will take care of flinging.
  if (((event.GetType() == blink::WebInputEvent::kGestureScrollEnd &&
        event.data.scroll_end.inertial_phase ==
            blink::WebGestureEvent::kMomentumPhase) ||
       (event.GetType() == blink::WebInputEvent::kGestureScrollUpdate &&
        event.data.scroll_update.inertial_phase ==
            blink::WebGestureEvent::kMomentumPhase)) &&
      (event.SourceDevice() != blink::kWebGestureDeviceTouchpad ||
       touchpad_fling_start_bubbles)) {
    return;
  }

  if ((event.GetType() == blink::WebInputEvent::kGestureScrollBegin) &&
      should_bubble) {
    DCHECK(!is_scroll_sequence_bubbling_);
    is_scroll_sequence_bubbling_ = true;
  } else if (event.GetType() == blink::WebInputEvent::kGestureScrollEnd ||
             event.GetType() == blink::WebInputEvent::kGestureFlingStart) {
    is_scroll_sequence_bubbling_ = false;
  }

  // GestureScrollBegin is a blocking event; It is forwarded for bubbling if
  // its ack is not consumed. For the rest of the scroll events
  // (GestureScrollUpdate, GestureScrollEnd, GestureFlingStart) the
  // frame_connector_ decides to forward them for bubbling if the
  // GestureScrollBegin event is forwarded.
  if ((event.GetType() == blink::WebInputEvent::kGestureScrollBegin &&
       should_bubble) ||
      event.GetType() == blink::WebInputEvent::kGestureScrollUpdate ||
      event.GetType() == blink::WebInputEvent::kGestureScrollEnd ||
      event.GetType() == blink::WebInputEvent::kGestureFlingStart ||
      event.GetType() == blink::WebInputEvent::kGestureFlingCancel) {
    frame_connector_->BubbleScrollEvent(event);
  }
}

void RenderWidgetHostViewChildFrame::ProcessTouchpadZoomEventAckInRoot(
    const blink::WebGestureEvent& event,
    InputEventAckState ack_result) {
  DCHECK(event.IsTouchpadZoomEvent());

  frame_connector_->ForwardAckedTouchpadZoomEvent(event, ack_result);
}

void RenderWidgetHostViewChildFrame::ForwardTouchpadZoomEventIfNecessary(
    const blink::WebGestureEvent& event,
    InputEventAckState ack_result) {
  // ACKs of synthetic wheel events for touchpad pinch or double tap are
  // processed in the root RWHV.
  NOTREACHED();
}

void RenderWidgetHostViewChildFrame::DidReceiveCompositorFrameAck(
    const std::vector<viz::ReturnedResource>& resources) {
  if (renderer_compositor_frame_sink_)
    renderer_compositor_frame_sink_->DidReceiveCompositorFrameAck(resources);
}

void RenderWidgetHostViewChildFrame::DidPresentCompositorFrame(
    uint32_t presentation_token,
    const gfx::PresentationFeedback& feedback) {
  renderer_compositor_frame_sink_->DidPresentCompositorFrame(presentation_token,
                                                             feedback);
}

void RenderWidgetHostViewChildFrame::DidCreateNewRendererCompositorFrameSink(
    viz::mojom::CompositorFrameSinkClient* renderer_compositor_frame_sink) {
  ResetCompositorFrameSinkSupport();
  renderer_compositor_frame_sink_ = renderer_compositor_frame_sink;
  CreateCompositorFrameSinkSupport();
  has_frame_ = false;
}

void RenderWidgetHostViewChildFrame::SetParentFrameSinkId(
    const viz::FrameSinkId& parent_frame_sink_id) {
  if (parent_frame_sink_id_ == parent_frame_sink_id ||
      features::IsMultiProcessMash())
    return;

  auto* host_frame_sink_manager = GetHostFrameSinkManager();

  // Unregister hierarchy for the current parent, only if set.
  if (parent_frame_sink_id_.is_valid()) {
    host_frame_sink_manager->UnregisterFrameSinkHierarchy(parent_frame_sink_id_,
                                                          frame_sink_id_);
  }

  parent_frame_sink_id_ = parent_frame_sink_id;

  // Register hierarchy for the new parent, only if set.
  if (parent_frame_sink_id_.is_valid()) {
    host_frame_sink_manager->RegisterFrameSinkHierarchy(parent_frame_sink_id_,
                                                        frame_sink_id_);
  }
}

void RenderWidgetHostViewChildFrame::SendSurfaceInfoToEmbedder() {
  if (features::IsMultiProcessMash())
    return;
  if (!last_activated_surface_info_.is_valid())
    return;
  FirstSurfaceActivation(last_activated_surface_info_);
}

void RenderWidgetHostViewChildFrame::FirstSurfaceActivation(
    const viz::SurfaceInfo& surface_info) {
  if (frame_connector_)
    frame_connector_->FirstSurfaceActivation(surface_info);
}

void RenderWidgetHostViewChildFrame::SubmitCompositorFrame(
    const viz::LocalSurfaceId& local_surface_id,
    viz::CompositorFrame frame,
    base::Optional<viz::HitTestRegionList> hit_test_region_list) {
  DCHECK(!enable_viz_);
  TRACE_EVENT0("content",
               "RenderWidgetHostViewChildFrame::OnSwapCompositorFrame");
  support_->SubmitCompositorFrame(local_surface_id, std::move(frame),
                                  std::move(hit_test_region_list));
}

void RenderWidgetHostViewChildFrame::OnDidNotProduceFrame(
    const viz::BeginFrameAck& ack) {
  DCHECK(!enable_viz_);
  support_->DidNotProduceFrame(ack);
}

void RenderWidgetHostViewChildFrame::ProcessFrameSwappedCallbacks() {
  std::vector<base::OnceClosure> process_callbacks;
  // Swap the vectors to avoid re-entrancy issues due to calls to
  // RegisterFrameSwappedCallback() while running the OnceClosures.
  process_callbacks.swap(frame_swapped_callbacks_);
  for (base::OnceClosure& callback : process_callbacks)
    std::move(callback).Run();
}

void RenderWidgetHostViewChildFrame::TransformPointToRootSurface(
    gfx::PointF* point) {
  // This function is called by RenderWidgetHostInputEventRouter only for
  // root-views.
  NOTREACHED();
  return;
}

gfx::Rect RenderWidgetHostViewChildFrame::GetBoundsInRootWindow() {
  gfx::Rect rect;
  if (frame_connector_) {
    RenderWidgetHostViewBase* root_view =
        frame_connector_->GetRootRenderWidgetHostView();

    // The root_view can be null in tests when using a TestWebContents.
    if (root_view)
      rect = root_view->GetBoundsInRootWindow();
  }
  return rect;
}

void RenderWidgetHostViewChildFrame::DidStopFlinging() {
  if (selection_controller_client_)
    selection_controller_client_->DidStopFlinging();
}

bool RenderWidgetHostViewChildFrame::LockMouse() {
  if (frame_connector_)
    return frame_connector_->LockMouse();
  return false;
}

void RenderWidgetHostViewChildFrame::UnlockMouse() {
  if (host()->delegate() && host()->delegate()->HasMouseLock(host()) &&
      frame_connector_)
    frame_connector_->UnlockMouse();
}

bool RenderWidgetHostViewChildFrame::IsMouseLocked() {
  if (!host()->delegate())
    return false;

  return host()->delegate()->HasMouseLock(host());
}

const viz::FrameSinkId& RenderWidgetHostViewChildFrame::GetFrameSinkId() const {
  return frame_sink_id_;
}

const viz::LocalSurfaceId& RenderWidgetHostViewChildFrame::GetLocalSurfaceId()
    const {
  if (frame_connector_)
    return frame_connector_->local_surface_id();
  return viz::ParentLocalSurfaceIdAllocator::InvalidLocalSurfaceId();
}

base::TimeTicks
RenderWidgetHostViewChildFrame::GetLocalSurfaceIdAllocationTime() const {
  if (frame_connector_)
    return frame_connector_->local_surface_id_allocation_time();
  return base::TimeTicks();
}

void RenderWidgetHostViewChildFrame::PreProcessTouchEvent(
    const blink::WebTouchEvent& event) {
  if (event.GetType() == blink::WebInputEvent::kTouchStart &&
      frame_connector_ && !frame_connector_->HasFocus()) {
    frame_connector_->FocusRootView();
  }
}

viz::FrameSinkId RenderWidgetHostViewChildFrame::GetRootFrameSinkId() {
  if (frame_connector_) {
    RenderWidgetHostViewBase* root_view =
        frame_connector_->GetRootRenderWidgetHostView();

    // The root_view can be null in tests when using a TestWebContents.
    if (root_view)
      return root_view->GetRootFrameSinkId();
  }
  return viz::FrameSinkId();
}

viz::SurfaceId RenderWidgetHostViewChildFrame::GetCurrentSurfaceId() const {
  return last_activated_surface_info_.id();
}

bool RenderWidgetHostViewChildFrame::HasSize() const {
  return frame_connector_ && frame_connector_->has_size();
}

gfx::PointF RenderWidgetHostViewChildFrame::TransformPointToRootCoordSpaceF(
    const gfx::PointF& point) {
  // LocalSurfaceId is not needed in Viz hit-test.
  if (!frame_connector_ ||
      (!use_viz_hit_test_ && !last_activated_surface_info_.is_valid())) {
    return point;
  }

  return frame_connector_->TransformPointToRootCoordSpace(
      point, last_activated_surface_info_.id());
}

bool RenderWidgetHostViewChildFrame::TransformPointToLocalCoordSpaceLegacy(
    const gfx::PointF& point,
    const viz::SurfaceId& original_surface,
    gfx::PointF* transformed_point) {
  *transformed_point = point;
  if (!frame_connector_ || !last_activated_surface_info_.is_valid())
    return false;

  return frame_connector_->TransformPointToLocalCoordSpaceLegacy(
      point, original_surface, last_activated_surface_info_.id(),
      transformed_point);
}

bool RenderWidgetHostViewChildFrame::TransformPointToCoordSpaceForView(
    const gfx::PointF& point,
    RenderWidgetHostViewBase* target_view,
    gfx::PointF* transformed_point,
    viz::EventSource source) {
  // LocalSurfaceId is not needed in Viz hit-test.
  if (!frame_connector_ ||
      (!use_viz_hit_test_ && !last_activated_surface_info_.is_valid())) {
    return false;
  }

  if (target_view == this) {
    *transformed_point = point;
    return true;
  }

  return frame_connector_->TransformPointToCoordSpaceForView(
      point, target_view, last_activated_surface_info_.id(), transformed_point,
      source);
}

gfx::PointF RenderWidgetHostViewChildFrame::TransformRootPointToViewCoordSpace(
    const gfx::PointF& point) {
  if (!frame_connector_)
    return point;

  RenderWidgetHostViewBase* root_rwhv =
      frame_connector_->GetRootRenderWidgetHostView();
  if (!root_rwhv)
    return point;

  gfx::PointF transformed_point;
  if (!root_rwhv->TransformPointToCoordSpaceForView(point, this,
                                                    &transformed_point)) {
    return point;
  }
  return transformed_point;
}

bool RenderWidgetHostViewChildFrame::IsRenderWidgetHostViewChildFrame() {
  return true;
}

void RenderWidgetHostViewChildFrame::WillSendScreenRects() {
  // TODO(kenrb): These represent post-initialization state updates that are
  // needed by the renderer. During normal OOPIF setup these are unnecessary,
  // as the parent renderer will send the information and it will be
  // immediately propagated to the OOPIF. However when an OOPIF navigates from
  // one process to another, the parent doesn't know that, and certain
  // browser-side state needs to be sent again. There is probably a less
  // spammy way to do this, but triggering on SendScreenRects() is reasonable
  // until somebody figures that out. RWHVCF::Init() is too early.
  if (frame_connector_) {
    UpdateViewportIntersection(frame_connector_->viewport_intersection_rect(),
                               frame_connector_->compositor_visible_rect(),
                               frame_connector_->occluded_or_obscured());
    SetIsInert();
    UpdateInheritedEffectiveTouchAction();
    UpdateRenderThrottlingStatus();
  }
}

#if defined(OS_MACOSX)
void RenderWidgetHostViewChildFrame::SetActive(bool active) {}

void RenderWidgetHostViewChildFrame::ShowDefinitionForSelection() {
  if (frame_connector_) {
    frame_connector_->GetRootRenderWidgetHostView()
        ->ShowDefinitionForSelection();
  }
}

void RenderWidgetHostViewChildFrame::SpeakSelection() {}
#endif  // defined(OS_MACOSX)

void RenderWidgetHostViewChildFrame::RegisterFrameSwappedCallback(
    base::OnceClosure callback) {
  frame_swapped_callbacks_.emplace_back(std::move(callback));
}

void RenderWidgetHostViewChildFrame::CopyFromSurface(
    const gfx::Rect& src_subrect,
    const gfx::Size& output_size,
    base::OnceCallback<void(const SkBitmap&)> callback) {
  // TODO(crbug.com/812059): Need a "copy from surface" VIZ API.
  if (enable_viz_) {
    std::move(callback).Run(SkBitmap());
    return;
  }

  if (!IsSurfaceAvailableForCopy()) {
    // Defer submitting the copy request until after a frame is drawn, at which
    // point we should be guaranteed that the surface is available.
    RegisterFrameSwappedCallback(base::BindOnce(
        &RenderWidgetHostViewChildFrame::CopyFromSurface, AsWeakPtr(),
        src_subrect, output_size, std::move(callback)));
    return;
  }

  std::unique_ptr<viz::CopyOutputRequest> request =
      std::make_unique<viz::CopyOutputRequest>(
          viz::CopyOutputRequest::ResultFormat::RGBA_BITMAP,
          base::BindOnce(
              [](base::OnceCallback<void(const SkBitmap&)> callback,
                 std::unique_ptr<viz::CopyOutputResult> result) {
                std::move(callback).Run(result->AsSkBitmap());
              },
              std::move(callback)));

  if (src_subrect.IsEmpty()) {
    request->set_area(gfx::Rect(last_activated_surface_info_.size_in_pixels()));
  } else {
    // |src_subrect| is in DIP coordinates; convert to Surface coordinates.
    request->set_area(gfx::ScaleToRoundedRect(
        src_subrect, last_activated_surface_info_.device_scale_factor()));
  }

  if (!output_size.IsEmpty()) {
    if (request->area().IsEmpty()) {
      // Viz would normally return an empty result for an empty source area.
      // However, this guard here is still necessary to protect against setting
      // an illegal scaling ratio.
      return;
    }
    request->set_result_selection(gfx::Rect(output_size));
    request->SetScaleRatio(
        gfx::Vector2d(request->area().width(), request->area().height()),
        gfx::Vector2d(output_size.width(), output_size.height()));
  }

  GetHostFrameSinkManager()->RequestCopyOfOutput(
      last_activated_surface_info_.id(), std::move(request));
}

void RenderWidgetHostViewChildFrame::ReclaimResources(
    const std::vector<viz::ReturnedResource>& resources) {
  if (renderer_compositor_frame_sink_)
    renderer_compositor_frame_sink_->ReclaimResources(resources);
}

void RenderWidgetHostViewChildFrame::OnBeginFrame(
    const viz::BeginFrameArgs& args) {
  host_->ProgressFlingIfNeeded(args.frame_time);
  if (renderer_compositor_frame_sink_)
    renderer_compositor_frame_sink_->OnBeginFrame(args);
}

void RenderWidgetHostViewChildFrame::OnBeginFramePausedChanged(bool paused) {
  if (renderer_compositor_frame_sink_)
    renderer_compositor_frame_sink_->OnBeginFramePausedChanged(paused);
}

void RenderWidgetHostViewChildFrame::OnFirstSurfaceActivation(
    const viz::SurfaceInfo& surface_info) {
  last_activated_surface_info_ = surface_info;
  has_frame_ = true;
  FirstSurfaceActivation(surface_info);
  ProcessFrameSwappedCallbacks();
}

void RenderWidgetHostViewChildFrame::OnFrameTokenChanged(uint32_t frame_token) {
  OnFrameTokenChangedForView(frame_token);
}

void RenderWidgetHostViewChildFrame::SetNeedsBeginFrames(
    bool needs_begin_frames) {
  if (support_)
    support_->SetNeedsBeginFrame(needs_begin_frames);
}

TouchSelectionControllerClientManager*
RenderWidgetHostViewChildFrame::GetTouchSelectionControllerClientManager() {
  auto* root_view = frame_connector_->GetRootRenderWidgetHostView();
  if (!root_view)
    return nullptr;

  // There is only ever one manager, and it's owned by the root view.
  return root_view->GetTouchSelectionControllerClientManager();
}

void RenderWidgetHostViewChildFrame::
    OnRenderFrameMetadataChangedAfterActivation() {
  RenderWidgetHostViewBase::OnRenderFrameMetadataChangedAfterActivation();
  if (selection_controller_client_) {
    const cc::RenderFrameMetadata& metadata =
        host()->render_frame_metadata_provider()->LastRenderFrameMetadata();
    selection_controller_client_->UpdateSelectionBoundsIfNeeded(
        metadata.selection, current_device_scale_factor_);
  }
}

void RenderWidgetHostViewChildFrame::SetWantsAnimateOnlyBeginFrames() {
  if (support_)
    support_->SetWantsAnimateOnlyBeginFrames();
}

void RenderWidgetHostViewChildFrame::TakeFallbackContentFrom(
    RenderWidgetHostView* view) {
  // This method only makes sense for top-level views.
}

InputEventAckState RenderWidgetHostViewChildFrame::FilterInputEvent(
    const blink::WebInputEvent& input_event) {
  // A child renderer should never receive a GesturePinch event. Pinch events
  // can still be targeted to a child, but they must be processed without
  // sending the pinch event to the child (e.g. touchpad pinch synthesizes
  // wheel events to send to the child renderer).
  if (blink::WebInputEvent::IsPinchGestureEventType(input_event.GetType())) {
    const blink::WebGestureEvent& gesture_event =
        static_cast<const blink::WebGestureEvent&>(input_event);
    // Touchscreen pinch events may be targeted to a child in order to have the
    // child's TouchActionFilter filter them, but we may encounter
    // https://crbug.com/771330 which would let the pinch events through.
    if (gesture_event.SourceDevice() == blink::kWebGestureDeviceTouchscreen) {
      return INPUT_EVENT_ACK_STATE_CONSUMED;
    }
    NOTREACHED();
  }

  if (input_event.GetType() == blink::WebInputEvent::kGestureFlingStart) {
    const blink::WebGestureEvent& gesture_event =
        static_cast<const blink::WebGestureEvent&>(input_event);
    // Zero-velocity touchpad flings are an Aura-specific signal that the
    // touchpad scroll has ended, and should not be forwarded to the renderer.
    if (gesture_event.SourceDevice() == blink::kWebGestureDeviceTouchpad &&
        !gesture_event.data.fling_start.velocity_x &&
        !gesture_event.data.fling_start.velocity_y) {
      // Here we indicate that there was no consumer for this event, as
      // otherwise the fling animation system will try to run an animation
      // and will also expect a notification when the fling ends. Since
      // CrOS just uses the GestureFlingStart with zero-velocity as a means
      // of indicating that touchpad scroll has ended, we don't actually want
      // a fling animation.
      // Note: this event handling is modeled on similar code in
      // TenderWidgetHostViewAura::FilterInputEvent().
      return INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS;
    }
  }

  if (is_scroll_sequence_bubbling_ &&
      (input_event.GetType() == blink::WebInputEvent::kGestureScrollUpdate) &&
      frame_connector_) {
    // If we're bubbling, then to preserve latching behaviour, the child should
    // not consume this event. If the child has added its viewport to the scroll
    // chain, then any GSU events we send to the renderer could be consumed,
    // even though we intend for them to be bubbled. So we immediately bubble
    // any scroll updates without giving the child a chance to consume them.
    // If the child has not added its viewport to the scroll chain, then we
    // know that it will not attempt to consume the rest of the scroll
    // sequence.
    return INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS;
  }

  return INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
}

InputEventAckState RenderWidgetHostViewChildFrame::FilterChildGestureEvent(
    const blink::WebGestureEvent& gesture_event) {
  // We may be the owner of a RenderWidgetHostViewGuest,
  // so we talk to the root RWHV on its behalf.
  // TODO(mcnee): Remove once MimeHandlerViewGuest is based on OOPIF.
  // See crbug.com/659750
  if (frame_connector_)
    return frame_connector_->GetRootRenderWidgetHostView()
        ->FilterChildGestureEvent(gesture_event);
  return INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
}

BrowserAccessibilityManager*
RenderWidgetHostViewChildFrame::CreateBrowserAccessibilityManager(
    BrowserAccessibilityDelegate* delegate,
    bool for_root_frame) {
  return BrowserAccessibilityManager::Create(
      BrowserAccessibilityManager::GetEmptyDocument(), delegate);
}

void RenderWidgetHostViewChildFrame::GetScreenInfo(
    ScreenInfo* screen_info) const {
  if (frame_connector_)
    *screen_info = frame_connector_->screen_info();
  else
    DisplayUtil::GetDefaultScreenInfo(screen_info);
}

void RenderWidgetHostViewChildFrame::EnableAutoResize(
    const gfx::Size& min_size,
    const gfx::Size& max_size) {
  if (frame_connector_)
    frame_connector_->EnableAutoResize(min_size, max_size);
}

void RenderWidgetHostViewChildFrame::DisableAutoResize(
    const gfx::Size& new_size) {
  // For child frames, the size comes from the parent when auto-resize is
  // disabled so we ignore |new_size| here.
  if (frame_connector_)
    frame_connector_->DisableAutoResize();
}

viz::ScopedSurfaceIdAllocator
RenderWidgetHostViewChildFrame::DidUpdateVisualProperties(
    const cc::RenderFrameMetadata& metadata) {
  base::OnceCallback<void()> allocation_task = base::BindOnce(
      base::IgnoreResult(
          &RenderWidgetHostViewChildFrame::OnDidUpdateVisualPropertiesComplete),
      weak_factory_.GetWeakPtr(), metadata);
  return viz::ScopedSurfaceIdAllocator(std::move(allocation_task));
}

void RenderWidgetHostViewChildFrame::CreateCompositorFrameSinkSupport() {
  if (features::IsMultiProcessMash() || enable_viz_)
    return;

  DCHECK(!support_);
  constexpr bool is_root = false;
  constexpr bool needs_sync_points = true;
  support_ = GetHostFrameSinkManager()->CreateCompositorFrameSinkSupport(
      this, frame_sink_id_, is_root, needs_sync_points);
  if (parent_frame_sink_id_.is_valid()) {
    GetHostFrameSinkManager()->RegisterFrameSinkHierarchy(parent_frame_sink_id_,
                                                          frame_sink_id_);
  }
  if (host()->needs_begin_frames())
    support_->SetNeedsBeginFrame(true);
}

void RenderWidgetHostViewChildFrame::ResetCompositorFrameSinkSupport() {
  if (!support_)
    return;
  if (parent_frame_sink_id_.is_valid()) {
    GetHostFrameSinkManager()->UnregisterFrameSinkHierarchy(
        parent_frame_sink_id_, frame_sink_id_);
  }
  support_.reset();
}

bool RenderWidgetHostViewChildFrame::GetSelectionRange(
    gfx::Range* range) const {
  if (!text_input_manager_ || !GetFocusedWidget())
    return false;

  const TextInputManager::TextSelection* selection =
      text_input_manager_->GetTextSelection(GetFocusedWidget()->GetView());
  if (!selection)
    return false;

  range->set_start(selection->range().start());
  range->set_end(selection->range().end());

  return true;
}

ui::TextInputType RenderWidgetHostViewChildFrame::GetTextInputType() const {
  if (!text_input_manager_)
    return ui::TEXT_INPUT_TYPE_NONE;

  if (text_input_manager_->GetTextInputState())
    return text_input_manager_->GetTextInputState()->type;
  return ui::TEXT_INPUT_TYPE_NONE;
}

RenderWidgetHostViewBase*
RenderWidgetHostViewChildFrame::GetRootRenderWidgetHostView() const {
  return frame_connector_ ? frame_connector_->GetRootRenderWidgetHostView()
                          : nullptr;
}

bool RenderWidgetHostViewChildFrame::CanBecomeVisible() {
  if (!frame_connector_)
    return true;

  if (frame_connector_->IsHidden())
    return false;

  RenderWidgetHostViewBase* parent_view = GetParentView();
  if (!parent_view || !parent_view->IsRenderWidgetHostViewChildFrame()) {
    // Root frame does not have a CSS visibility property.
    return true;
  }

  return static_cast<RenderWidgetHostViewChildFrame*>(parent_view)
      ->CanBecomeVisible();
}

void RenderWidgetHostViewChildFrame::OnDidUpdateVisualPropertiesComplete(
    const cc::RenderFrameMetadata& metadata) {
  if (frame_connector_)
    frame_connector_->DidUpdateVisualProperties(metadata);
  host()->SynchronizeVisualProperties();
}

void RenderWidgetHostViewChildFrame::DidNavigate() {
  host()->SynchronizeVisualProperties();
}

}  // namespace content
