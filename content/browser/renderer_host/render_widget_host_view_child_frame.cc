// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/input/cursor_manager.h"
#include "components/input/render_widget_host_input_event_router.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/renderer_host/cross_process_frame_connector.h"
#include "content/browser/renderer_host/input/touch_selection_controller_client_child_frame.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_event_handler.h"
#include "content/browser/renderer_host/text_input_manager.h"
#include "content/common/features.h"
#include "content/common/input/synthetic_gesture_target.h"
#include "content/public/browser/render_process_host.h"
#include "third_party/blink/public/common/frame/frame_visual_properties.h"
#include "third_party/blink/public/common/input/web_touch_event.h"
#include "third_party/blink/public/mojom/frame/intrinsic_sizing_info.mojom.h"
#include "third_party/blink/public/mojom/frame/viewport_intersection_state.mojom.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/ime/mojom/text_input_state.mojom.h"
#include "ui/display/display_util.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/touch_selection/touch_selection_controller.h"

namespace content {

// static
RenderWidgetHostViewChildFrame* RenderWidgetHostViewChildFrame::Create(
    RenderWidgetHost* widget,
    const display::ScreenInfos& parent_screen_infos) {
  RenderWidgetHostViewChildFrame* view =
      new RenderWidgetHostViewChildFrame(widget, parent_screen_infos);
  view->Init();
  return view;
}

RenderWidgetHostViewChildFrame::RenderWidgetHostViewChildFrame(
    RenderWidgetHost* widget_host,
    const display::ScreenInfos& parent_screen_infos)
    : RenderWidgetHostViewBase(widget_host),
      frame_sink_id_(widget_host->GetFrameSinkId()),
      frame_connector_(nullptr) {
  // TODO(enne): this appears to have a null current() in some tests.
  screen_infos_ = parent_screen_infos;

  host()->render_frame_metadata_provider()->AddObserver(this);
}

RenderWidgetHostViewChildFrame::~RenderWidgetHostViewChildFrame() {
  // TODO(wjmaclean): The next two lines are a speculative fix for
  // https://crbug.com/760074, based on the theory that perhaps something is
  // destructing the class without calling Destroy() first.
  if (frame_connector_)
    DetachFromTouchSelectionClientManagerIfNecessary();

  if (is_frame_sink_id_owner() && GetHostFrameSinkManager()) {
    GetHostFrameSinkManager()->InvalidateFrameSinkId(frame_sink_id_, this);
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

void RenderWidgetHostViewChildFrame::SetFrameConnector(
    CrossProcessFrameConnector* frame_connector) {
  if (frame_connector_ == frame_connector)
    return;

  if (frame_connector_) {
    SetParentFrameSinkId(viz::FrameSinkId());

    // Unlocks the mouse if this RenderWidgetHostView holds the lock.
    UnlockPointer();
    DetachFromTouchSelectionClientManagerIfNecessary();
  }
  frame_connector_ = frame_connector;
  if (!frame_connector_)
    return;

  RenderWidgetHostViewBase* parent_view =
      frame_connector_->GetParentRenderWidgetHostView();

  if (parent_view) {
    DCHECK(parent_view->GetFrameSinkId().is_valid());
    SetParentFrameSinkId(parent_view->GetFrameSinkId());
  }

  UpdateScreenInfo();

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
}

void RenderWidgetHostViewChildFrame::UpdateIntrinsicSizingInfo(
    blink::mojom::IntrinsicSizingInfoPtr sizing_info) {
  if (frame_connector_)
    frame_connector_->SendIntrinsicSizingInfoToParent(std::move(sizing_info));
}

std::unique_ptr<SyntheticGestureTarget>
RenderWidgetHostViewChildFrame::CreateSyntheticGestureTarget() {
  // Sythetic gestures should be sent to the root view.
  NOTREACHED_IN_MIGRATION();
  return nullptr;
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
  NOTREACHED_IN_MIGRATION();
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

void RenderWidgetHostViewChildFrame::Focus() {
  if (!frame_connector_) {
    return;
  }
  if (frame_connector_->HasFocus() ==
      CrossProcessFrameConnector::RootViewFocusState::kNotFocused) {
    return frame_connector_->FocusRootView();
  }
}

bool RenderWidgetHostViewChildFrame::HasFocus() {
  if (!frame_connector_) {
    return false;
  }
  return frame_connector_->HasFocus() ==
         CrossProcessFrameConnector::RootViewFocusState::kFocused;
}

bool RenderWidgetHostViewChildFrame::IsSurfaceAvailableForCopy() {
  return GetLocalSurfaceId().is_valid();
}

void RenderWidgetHostViewChildFrame::EnsureSurfaceSynchronizedForWebTest() {
  // The capture sequence number which would normally be updated here is
  // actually retrieved from the frame connector.
}

uint32_t RenderWidgetHostViewChildFrame::GetCaptureSequenceNumber() const {
  if (!frame_connector_)
    return 0u;
  return frame_connector_->capture_sequence_number();
}

void RenderWidgetHostViewChildFrame::ShowWithVisibility(
    PageVisibilityState /*page_visibility*/) {
  if (!host()->is_hidden())
    return;

  if (!CanBecomeVisible())
    return;

  host()->WasShown({} /* record_tab_switch_time_request */);

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

void RenderWidgetHostViewChildFrame::WasOccluded() {
  Hide();
}

void RenderWidgetHostViewChildFrame::WasUnOccluded() {
  Show();
}

gfx::Rect RenderWidgetHostViewChildFrame::GetViewBounds() {
  gfx::Rect screen_space_rect;
  if (frame_connector_) {
    screen_space_rect = frame_connector_->rect_in_parent_view_in_dip();

    RenderWidgetHostView* parent_view =
        frame_connector_->GetParentRenderWidgetHostView();

    // The parent_view can be null in tests when using a TestWebContents.
    if (parent_view) {
      // Translate screen_space_rect by the parent's RenderWidgetHostView
      // offset.
      screen_space_rect.Offset(parent_view->GetViewBounds().OffsetFromOrigin());
    }
    // TODO(wjmaclean): GetViewBounds is a bit of a mess. It's used to determine
    // the size of the renderer content and where to place context menus and so
    // on. We want the location of the frame in screen coordinates to place
    // popups but we want the size in local coordinates to produce the right-
    // sized CompositorFrames. https://crbug.com/928825.
    screen_space_rect.set_size(frame_connector_->local_frame_size_in_dip());
  }
  return screen_space_rect;
}

gfx::Size RenderWidgetHostViewChildFrame::GetVisibleViewportSize() {
  // For subframes, the visual viewport corresponds to the main frame size so
  // this method would not even be called, the main frame's value should be
  // used instead. However a nested WebContents will have a ChildFrame view used
  // for the main frame.
  DCHECK(host()->owner_delegate());

  gfx::Rect requested_rect(GetRequestedRendererSize());
  requested_rect.Inset(insets_);
  return requested_rect.size();
}

void RenderWidgetHostViewChildFrame::SetInsets(const gfx::Insets& insets) {
  // Insets are used only for <webview> and are used to let the UI know it's
  // being obscured (for e.g. by the virtual keyboard).
  insets_ = insets;
  host()->SynchronizeVisualProperties(!insets_.IsEmpty());
}

gfx::NativeView RenderWidgetHostViewChildFrame::GetNativeView() {
  if (!frame_connector_)
    return gfx::NativeView();

  RenderWidgetHostView* parent_view =
      frame_connector_->GetParentRenderWidgetHostView();
  return parent_view ? parent_view->GetNativeView() : gfx::NativeView();
}

gfx::NativeViewAccessible
RenderWidgetHostViewChildFrame::GetNativeViewAccessible() {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

void RenderWidgetHostViewChildFrame::UpdateFrameSinkIdRegistration() {
  RenderWidgetHostViewBase::UpdateFrameSinkIdRegistration();
  if (is_frame_sink_id_owner()) {
    GetHostFrameSinkManager()->RegisterFrameSinkId(
        frame_sink_id_, this, viz::ReportFirstSurfaceActivation::kNo);
    GetHostFrameSinkManager()->SetFrameSinkDebugLabel(
        frame_sink_id_, "RenderWidgetHostViewChildFrame");
  }
}

void RenderWidgetHostViewChildFrame::UpdateBackgroundColor() {
  DCHECK(GetBackgroundColor());

  SkColor color = *GetBackgroundColor();
  DCHECK(SkColorGetA(color) == SK_AlphaOPAQUE ||
         SkColorGetA(color) == SK_AlphaTRANSPARENT);
  if (host()->owner_delegate()) {
    host()->owner_delegate()->SetBackgroundOpaque(SkColorGetA(color) ==
                                                  SK_AlphaOPAQUE);
  }
}

std::optional<DisplayFeature>
RenderWidgetHostViewChildFrame::GetDisplayFeature() {
  NOTREACHED_IN_MIGRATION();
  return std::nullopt;
}

void RenderWidgetHostViewChildFrame::SetDisplayFeatureForTesting(
    const DisplayFeature*) {
  NOTREACHED_IN_MIGRATION();
}

void RenderWidgetHostViewChildFrame::NotifyHostAndDelegateOnWasShown(
    blink::mojom::RecordContentToVisibleTimeRequestPtr) {
  NOTREACHED_IN_MIGRATION();
}

void RenderWidgetHostViewChildFrame::
    RequestSuccessfulPresentationTimeFromHostOrDelegate(
        blink::mojom::RecordContentToVisibleTimeRequestPtr) {
  NOTREACHED_IN_MIGRATION();
}

void RenderWidgetHostViewChildFrame::
    CancelSuccessfulPresentationTimeRequestForHostAndDelegate() {
  NOTREACHED_IN_MIGRATION();
}

gfx::Size RenderWidgetHostViewChildFrame::GetCompositorViewportPixelSize() {
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
    const gfx::Rect& bounds,
    const gfx::Rect& anchor_rect) {
  NOTREACHED_IN_MIGRATION();
}

void RenderWidgetHostViewChildFrame::UpdateCursor(const ui::Cursor& cursor) {
  if (frame_connector_)
    frame_connector_->UpdateCursor(cursor);
}

void RenderWidgetHostViewChildFrame::UpdateScreenInfo() {
  if (frame_connector_)
    screen_infos_ = frame_connector_->screen_infos();
}

void RenderWidgetHostViewChildFrame::SendInitialPropertiesIfNeeded() {
  if (initial_properties_sent_ || !frame_connector_)
    return;
  UpdateViewportIntersection(frame_connector_->intersection_state(),
                             std::nullopt);
  SetIsInert();
  UpdateInheritedEffectiveTouchAction();
  UpdateRenderThrottlingStatus();
  initial_properties_sent_ = true;
}

void RenderWidgetHostViewChildFrame::SetIsLoading(bool is_loading) {
  // It is valid for an inner WebContents's SetIsLoading() to end up here.
  // This is because an inner WebContents's main frame's RenderWidgetHostView
  // is a RenderWidgetHostViewChildFrame. In contrast, when there is no
  // inner/outer WebContents, only subframe's RenderWidgetHostView can be a
  // RenderWidgetHostViewChildFrame which do not get a SetIsLoading() call.
}

void RenderWidgetHostViewChildFrame::RenderProcessGone() {
  if (frame_connector_)
    frame_connector_->RenderProcessGone();
  Destroy();
}

void RenderWidgetHostViewChildFrame::Destroy() {
  host()->render_frame_metadata_provider()->RemoveObserver(this);

  // FrameSinkIds registered with RenderWidgetHostInputEventRouter
  // have already been cleared when RenderWidgetHostViewBase notified its
  // observers of our impending destruction.
  if (frame_connector_) {
    frame_connector_->SetView(nullptr, /*allow_paint_holding=*/false);
    SetFrameConnector(nullptr);
  }

  // We notify our observers about shutdown here since we are about to release
  // host_ and do not want any event calls coming from
  // RenderWidgetHostInputEventRouter afterwards.
  NotifyObserversAboutShutdown();

  RenderWidgetHostViewBase::Destroy();

  delete this;
}

void RenderWidgetHostViewChildFrame::UpdateTooltipUnderCursor(
    const std::u16string& tooltip_text) {
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

  if (cursor_manager->IsViewUnderCursor(this))
    root_view->UpdateTooltip(tooltip_text);
}

void RenderWidgetHostViewChildFrame::UpdateTooltipFromKeyboard(
    const std::u16string& tooltip_text,
    const gfx::Rect& bounds) {
  if (!frame_connector_)
    return;

  auto* root_view = frame_connector_->GetRootRenderWidgetHostView();
  if (!root_view)
    return;

  // TODO(bebeaudr): Keyboard-triggered tooltips are not positioned correctly
  // when set for an element in an OOPIF. See https://crbug.com/1210269.
  gfx::Rect adjusted_bounds(TransformPointToRootCoordSpace(bounds.origin()),
                            bounds.size());
  root_view->UpdateTooltipFromKeyboard(tooltip_text, adjusted_bounds);
}

void RenderWidgetHostViewChildFrame::ClearKeyboardTriggeredTooltip() {
  if (!frame_connector_)
    return;

  auto* root_view = frame_connector_->GetRootRenderWidgetHostView();
  if (!root_view)
    return;

  root_view->ClearKeyboardTriggeredTooltip();
}

RenderWidgetHostViewBase* RenderWidgetHostViewChildFrame::GetParentViewInput() {
  if (!frame_connector_)
    return nullptr;
  return frame_connector_->GetParentRenderWidgetHostView();
}

void RenderWidgetHostViewChildFrame::RegisterFrameSinkId() {
  UpdateFrameSinkIdRegistration();
}

void RenderWidgetHostViewChildFrame::UnregisterFrameSinkId() {
  DCHECK(host());
  UpdateFrameSinkIdRegistration();
  DetachFromTouchSelectionClientManagerIfNecessary();
}

void RenderWidgetHostViewChildFrame::UpdateViewportIntersection(
    const blink::mojom::ViewportIntersectionState& intersection_state,
    const std::optional<blink::VisualProperties>& visual_properties) {
  if (host()) {
    host()->SetIntersectsViewport(
        !intersection_state.viewport_intersection.IsEmpty());

    // Do not send |visual_properties| to main frames.
    DCHECK(!visual_properties.has_value() || !host()->owner_delegate());

    bool is_fenced_frame = host()->frame_tree()->is_fenced_frame();
    if (!host()->owner_delegate() || is_fenced_frame) {
      host()->GetAssociatedFrameWidget()->SetViewportIntersection(
          intersection_state.Clone(), visual_properties);
    }
  }
}

void RenderWidgetHostViewChildFrame::SetIsInert() {
  // Do not send inert to main frames.
  if (host() && frame_connector_ && !host()->owner_delegate()) {
    host_->GetAssociatedFrameWidget()->SetIsInertForSubFrame(
        frame_connector_->IsInert());
  }
}

void RenderWidgetHostViewChildFrame::UpdateInheritedEffectiveTouchAction() {
  // Do not send inherited touch action to main frames.
  if (host_ && frame_connector_ && !host()->owner_delegate()) {
    host_->GetAssociatedFrameWidget()
        ->SetInheritedEffectiveTouchActionForSubFrame(
            frame_connector_->InheritedEffectiveTouchAction());
  }
}

void RenderWidgetHostViewChildFrame::UpdateRenderThrottlingStatus() {
  // Do not send throttling status to main frames.
  if (host() && frame_connector_ && !host()->owner_delegate()) {
    host_->GetAssociatedFrameWidget()->UpdateRenderThrottlingStatusForSubFrame(
        frame_connector_->IsThrottled(), frame_connector_->IsSubtreeThrottled(),
        frame_connector_->IsDisplayLocked());
  }
}

void RenderWidgetHostViewChildFrame::StopFlingingIfNecessary(
    const blink::WebGestureEvent& event,
    blink::mojom::InputEventResultState ack_result) {
  // In case of scroll bubbling the target view is in charge of stopping the
  // fling if needed.
  if (is_scroll_sequence_bubbling_)
    return;

  RenderWidgetHostViewBase::StopFlingingIfNecessary(event, ack_result);
}

void RenderWidgetHostViewChildFrame::GestureEventAck(
    const blink::WebGestureEvent& event,
    blink::mojom::InputEventResultSource ack_source,
    blink::mojom::InputEventResultState ack_result) {
  TRACE_EVENT1("input", "RenderWidgetHostViewChildFrame::GestureEventAck",
               "type", blink::WebInputEvent::GetName(event.GetType()));

  // Stop flinging if a GSU event with momentum phase is sent to the renderer
  // but not consumed.
  StopFlingingIfNecessary(event, ack_result);

  HandleSwipeToMoveCursorGestureAck(event);

  if (!frame_connector_)
    return;

  if (event.IsTouchpadZoomEvent())
    ProcessTouchpadZoomEventAckInRoot(event, ack_source, ack_result);

  // GestureScrollBegin is a blocking event; It is forwarded for bubbling if
  // its ack is not consumed. For the rest of the scroll events
  // (GestureScrollUpdate, GestureScrollEnd) are bubbled if the
  // GestureScrollBegin was bubbled. If the browser consumed the event, the
  // event was filtered and shouldn't affect the state of scroll bubbling.
  bool event_filtered =
      ack_source == blink::mojom::InputEventResultSource::kBrowser &&
      ack_result == blink::mojom::InputEventResultState::kConsumed;

  // TODO(crbug.com/346629231): Remove flag guard once this lands. Prior to the
  // fix this section was always entered.
  if (!event_filtered ||
      !base::FeatureList::IsEnabled(features::kScrollBubblingFix)) {
    if (event.GetType() == blink::WebInputEvent::Type::kGestureScrollBegin) {
      DCHECK(!is_scroll_sequence_bubbling_);
      is_scroll_sequence_bubbling_ =
          ack_result == blink::mojom::InputEventResultState::kNotConsumed ||
          ack_result == blink::mojom::InputEventResultState::kNoConsumerExists;
    }

    if (is_scroll_sequence_bubbling_ &&
        (event.GetType() == blink::WebInputEvent::Type::kGestureScrollBegin ||
         event.GetType() == blink::WebInputEvent::Type::kGestureScrollUpdate ||
         event.GetType() == blink::WebInputEvent::Type::kGestureScrollEnd)) {
      const bool can_continue = frame_connector_->BubbleScrollEvent(event);
      if (event.GetType() == blink::WebInputEvent::Type::kGestureScrollEnd ||
          !can_continue) {
        is_scroll_sequence_bubbling_ = false;
      }
    }
  }

  TRACE_EVENT_INSTANT0("input", "Did_Ack_To_Frame_Connector",
                       TRACE_EVENT_SCOPE_THREAD);
  frame_connector_->DidAckGestureEvent(event, ack_result);
}

void RenderWidgetHostViewChildFrame::ProcessTouchpadZoomEventAckInRoot(
    const blink::WebGestureEvent& event,
    blink::mojom::InputEventResultSource ack_source,
    blink::mojom::InputEventResultState ack_result) {
  DCHECK(event.IsTouchpadZoomEvent());

  frame_connector_->ForwardAckedTouchpadZoomEvent(event, ack_source,
                                                  ack_result);
}

void RenderWidgetHostViewChildFrame::ForwardTouchpadZoomEventIfNecessary(
    const blink::WebGestureEvent& event,
    blink::mojom::InputEventResultState ack_result) {
  // ACKs of synthetic wheel events for touchpad pinch or double tap are
  // processed in the root RWHV.
  NOTREACHED_IN_MIGRATION();
}

void RenderWidgetHostViewChildFrame::SetParentFrameSinkId(
    const viz::FrameSinkId& parent_frame_sink_id) {
  if (parent_frame_sink_id_ == parent_frame_sink_id)
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

void RenderWidgetHostViewChildFrame::FirstSurfaceActivation(
    const viz::SurfaceInfo& surface_info) {
  if (frame_connector_)
    frame_connector_->FirstSurfaceActivation(surface_info);
}

void RenderWidgetHostViewChildFrame::TransformPointToRootSurface(
    gfx::PointF* point) {
  // This function is called by RenderWidgetHostInputEventRouter only for
  // root-views.
  NOTREACHED_IN_MIGRATION();
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

blink::mojom::PointerLockResult RenderWidgetHostViewChildFrame::LockPointer(
    bool request_unadjusted_movement) {
  if (frame_connector_)
    return frame_connector_->LockPointer(request_unadjusted_movement);
  return blink::mojom::PointerLockResult::kWrongDocument;
}

blink::mojom::PointerLockResult
RenderWidgetHostViewChildFrame::ChangePointerLock(
    bool request_unadjusted_movement) {
  if (frame_connector_)
    return frame_connector_->ChangePointerLock(request_unadjusted_movement);
  return blink::mojom::PointerLockResult::kWrongDocument;
}

void RenderWidgetHostViewChildFrame::UnlockPointer() {
  if (host()->delegate() && host()->delegate()->HasPointerLock(host()) &&
      frame_connector_) {
    frame_connector_->UnlockPointer();
  }
}

bool RenderWidgetHostViewChildFrame::IsPointerLocked() {
  if (!host()->delegate())
    return false;

  return host()->delegate()->HasPointerLock(host());
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

void RenderWidgetHostViewChildFrame::NotifyHitTestRegionUpdated(
    const viz::AggregatedHitTestRegion& region) {
  if (selection_controller_client_) {
    selection_controller_client_->OnHitTestRegionUpdated();
  }

  std::optional<gfx::RectF> screen_rect =
      region.transform.InverseMapRect(gfx::RectF(region.rect));
  if (!screen_rect) {
    last_stable_screen_rect_ = gfx::RectF();
    last_stable_screen_rect_for_iov2_ = gfx::RectF();
    screen_rect_stable_since_ = base::TimeTicks::Now();
    screen_rect_stable_since_for_iov2_ = base::TimeTicks::Now();
    return;
  }

  // Convert to DIP
  screen_rect->Scale(1. / screen_infos_.current().device_scale_factor);

  // Movement as a proportion of frame size
  double horizontal_movement =
      screen_rect->width()
          ? std::abs(last_stable_screen_rect_.x() - screen_rect->x()) /
                screen_rect->width()
          : 0.0;
  double vertical_movement =
      screen_rect->height()
          ? std::abs(last_stable_screen_rect_.y() - screen_rect->y()) /
                screen_rect->height()
          : 0.0;
  if ((ToRoundedSize(screen_rect->size()) !=
       ToRoundedSize(last_stable_screen_rect_.size())) ||
      horizontal_movement >
          blink::FrameVisualProperties::MaxChildFrameScreenRectMovement() ||
      vertical_movement >
          blink::FrameVisualProperties::MaxChildFrameScreenRectMovement()) {
    last_stable_screen_rect_ = *screen_rect;
    screen_rect_stable_since_ = base::TimeTicks::Now();
  }
  // The legacy logic is based on manhattan distance.
  if ((ToRoundedSize(screen_rect->size()) !=
       ToRoundedSize(last_stable_screen_rect_for_iov2_.size())) ||
      (std::abs(last_stable_screen_rect_for_iov2_.x() - screen_rect->x()) +
           std::abs(last_stable_screen_rect_for_iov2_.y() - screen_rect->y()) >
       blink::FrameVisualProperties::
           MaxChildFrameScreenRectMovementForIOv2())) {
    last_stable_screen_rect_for_iov2_ = *screen_rect;
    screen_rect_stable_since_for_iov2_ = base::TimeTicks::Now();
  }
}

bool RenderWidgetHostViewChildFrame::ScreenRectIsUnstableFor(
    const blink::WebInputEvent& event) {
  // Some tests generate events with artificial timestamps; ignore these.
  if (event.TimeStamp() < screen_rect_stable_since_) {
    return false;
  }
  if (event.TimeStamp() -
          base::Milliseconds(
              blink::FrameVisualProperties::MinScreenRectStableTimeMs()) <
      screen_rect_stable_since_) {
    return true;
  }
  if (RenderWidgetHostViewBase* parent = GetParentViewInput()) {
    return parent->ScreenRectIsUnstableFor(event);
  }
  return false;
}

bool RenderWidgetHostViewChildFrame::ScreenRectIsUnstableForIOv2For(
    const blink::WebInputEvent& event) {
  // Some tests generate events with artificial timestamps; ignore these.
  if (event.TimeStamp() < screen_rect_stable_since_for_iov2_) {
    return false;
  }
  if (event.TimeStamp() -
          base::Milliseconds(blink::FrameVisualProperties::
                                 MinScreenRectStableTimeMsForIOv2()) <
      screen_rect_stable_since_for_iov2_) {
    return true;
  }
  if (RenderWidgetHostViewBase* parent = GetParentViewInput()) {
    return parent->ScreenRectIsUnstableForIOv2For(event);
  }
  return false;
}

void RenderWidgetHostViewChildFrame::PreProcessTouchEvent(
    const blink::WebTouchEvent& event) {
  if (event.GetType() != blink::WebInputEvent::Type::kTouchStart) {
    return;
  }

  if (!frame_connector_) {
    return;
  }

  CrossProcessFrameConnector::RootViewFocusState state =
      frame_connector_->HasFocus();
#if BUILDFLAG(IS_ANDROID)
  UMA_HISTOGRAM_ENUMERATION(
      "Android.FocusChanged.RenderWidgetHostViewChildFrame.RootViewFocusState",
      state);
#endif

  if (state == CrossProcessFrameConnector::RootViewFocusState::kNotFocused) {
    Focus();
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
  return viz::SurfaceId(frame_sink_id_, GetLocalSurfaceId());
}

bool RenderWidgetHostViewChildFrame::HasSize() const {
  return frame_connector_ && frame_connector_->has_size();
}

double RenderWidgetHostViewChildFrame::GetCSSZoomFactor() const {
  return frame_connector_ ? frame_connector_->css_zoom_factor() : 1.0;
}

gfx::PointF RenderWidgetHostViewChildFrame::TransformPointToRootCoordSpaceF(
    const gfx::PointF& point) {
  viz::SurfaceId surface_id = GetCurrentSurfaceId();
  if (!frame_connector_)
    return point;

  return frame_connector_->TransformPointToRootCoordSpace(point, surface_id);
}

bool RenderWidgetHostViewChildFrame::TransformPointToCoordSpaceForView(
    const gfx::PointF& point,
    input::RenderWidgetHostViewInput* target_view,
    gfx::PointF* transformed_point) {
  viz::SurfaceId surface_id = GetCurrentSurfaceId();
  if (!frame_connector_)
    return false;

  if (target_view == this) {
    *transformed_point = point;
    return true;
  }

  return frame_connector_->TransformPointToCoordSpaceForView(
      point, target_view, surface_id, transformed_point);
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

void RenderWidgetHostViewChildFrame::
    InvalidateLocalSurfaceIdAndAllocationGroup() {
  // This should only be handled by the top frame.
  NOTREACHED_IN_MIGRATION();
}

#if BUILDFLAG(IS_MAC)
void RenderWidgetHostViewChildFrame::SetActive(bool active) {}

void RenderWidgetHostViewChildFrame::ShowDefinitionForSelection() {
  if (frame_connector_) {
    frame_connector_->GetRootRenderWidgetHostView()
        ->ShowDefinitionForSelection();
  }
}

void RenderWidgetHostViewChildFrame::SpeakSelection() {}

void RenderWidgetHostViewChildFrame::SetWindowFrameInScreen(
    const gfx::Rect& rect) {}

void RenderWidgetHostViewChildFrame::ShowSharePicker(
    const std::string& title,
    const std::string& text,
    const std::string& url,
    const std::vector<std::string>& file_paths,
    blink::mojom::ShareService::ShareCallback callback) {}

uint64_t RenderWidgetHostViewChildFrame::GetNSViewId() const {
  return 0;
}

#endif  // BUILDFLAG(IS_MAC)

void RenderWidgetHostViewChildFrame::CopyFromSurface(
    const gfx::Rect& src_subrect,
    const gfx::Size& output_size,
    base::OnceCallback<void(const SkBitmap&)> callback) {
  if (!IsSurfaceAvailableForCopy()) {
    std::move(callback).Run(SkBitmap());
    return;
  }

  std::unique_ptr<viz::CopyOutputRequest> request =
      std::make_unique<viz::CopyOutputRequest>(
          viz::CopyOutputRequest::ResultFormat::RGBA,
          viz::CopyOutputRequest::ResultDestination::kSystemMemory,
          base::BindOnce(
              [](base::OnceCallback<void(const SkBitmap&)> callback,
                 std::unique_ptr<viz::CopyOutputResult> result) {
                auto scoped_bitmap = result->ScopedAccessSkBitmap();
                std::move(callback).Run(scoped_bitmap.GetOutScopedBitmap());
              },
              std::move(callback)));

  // Run result callback on the current thread in case `callback` needs to run
  // on the current thread. See http://crbug.com/1431363.
  request->set_result_task_runner(
      base::SingleThreadTaskRunner::GetCurrentDefault());

  if (src_subrect.IsEmpty()) {
    request->set_area(gfx::Rect(GetCompositorViewportPixelSize()));
  } else {
    // |src_subrect| is in DIP coordinates; convert to Surface coordinates.
    request->set_area(
        gfx::ScaleToRoundedRect(src_subrect, GetDeviceScaleFactor()));
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

  GetHostFrameSinkManager()->RequestCopyOfOutput(GetCurrentSurfaceId(),
                                                 std::move(request));
}

void RenderWidgetHostViewChildFrame::OnFirstSurfaceActivation(
    const viz::SurfaceInfo& surface_info) {}

void RenderWidgetHostViewChildFrame::OnFrameTokenChanged(
    uint32_t frame_token,
    base::TimeTicks activation_time) {
  OnFrameTokenChangedForView(frame_token, activation_time);
}

TouchSelectionControllerClientManager*
RenderWidgetHostViewChildFrame::GetTouchSelectionControllerClientManager() {
  if (!frame_connector_)
    return nullptr;
  auto* root_view = frame_connector_->GetRootRenderWidgetHostView();
  if (!root_view)
    return nullptr;

  // There is only ever one manager, and it's owned by the root view.
  return root_view->GetTouchSelectionControllerClientManager();
}

void RenderWidgetHostViewChildFrame::
    OnRenderFrameMetadataChangedAfterActivation(
        base::TimeTicks activation_time) {
  if (selection_controller_client_) {
    const cc::RenderFrameMetadata& metadata =
        host()->render_frame_metadata_provider()->LastRenderFrameMetadata();
    selection_controller_client_->UpdateSelectionBoundsIfNeeded(
        metadata.selection, GetDeviceScaleFactor());
  }
}

void RenderWidgetHostViewChildFrame::TakeFallbackContentFrom(
    RenderWidgetHostView* view) {
  // This method only makes sense for top-level views.
}

blink::mojom::InputEventResultState
RenderWidgetHostViewChildFrame::FilterInputEvent(
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
    if (gesture_event.SourceDevice() == blink::WebGestureDevice::kTouchscreen) {
      return blink::mojom::InputEventResultState::kConsumed;
    }
    DUMP_WILL_BE_NOTREACHED();
  }

  if (input_event.GetType() == blink::WebInputEvent::Type::kGestureFlingStart) {
    const blink::WebGestureEvent& gesture_event =
        static_cast<const blink::WebGestureEvent&>(input_event);
    // Zero-velocity touchpad flings are an Aura-specific signal that the
    // touchpad scroll has ended, and should not be forwarded to the renderer.
    if (gesture_event.SourceDevice() == blink::WebGestureDevice::kTouchpad &&
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
      return blink::mojom::InputEventResultState::kNoConsumerExists;
    }
  }

  if (is_scroll_sequence_bubbling_ &&
      (input_event.GetType() ==
       blink::WebInputEvent::Type::kGestureScrollUpdate) &&
      frame_connector_) {
    // If we're bubbling, then to preserve latching behaviour, the child should
    // not consume this event. If the child has added its viewport to the scroll
    // chain, then any GSU events we send to the renderer could be consumed,
    // even though we intend for them to be bubbled. So we immediately bubble
    // any scroll updates without giving the child a chance to consume them.
    // If the child has not added its viewport to the scroll chain, then we
    // know that it will not attempt to consume the rest of the scroll
    // sequence.
    return blink::mojom::InputEventResultState::kNoConsumerExists;
  }

  return blink::mojom::InputEventResultState::kNotConsumed;
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

ui::TextInputType RenderWidgetHostViewChildFrame::GetTextInputType() const {
  if (!text_input_manager_)
    return ui::TEXT_INPUT_TYPE_NONE;

  if (text_input_manager_->GetTextInputState())
    return text_input_manager_->GetTextInputState()->type;
  return ui::TEXT_INPUT_TYPE_NONE;
}

bool RenderWidgetHostViewChildFrame::GetTextRange(gfx::Range* range) const {
  if (!text_input_manager_ || !GetFocusedWidget()) {
    return false;
  }

  const ui::mojom::TextInputState* state =
      text_input_manager_->GetTextInputState();
  if (!state) {
    return false;
  }

  range->set_start(0);
  range->set_end(state->value ? state->value->length() : 0);
  return true;
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

  RenderWidgetHostViewBase* parent_view = GetParentViewInput();
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

ui::Compositor* RenderWidgetHostViewChildFrame::GetCompositor() {
  if (!GetRootView())
    return nullptr;
  return GetRootView()->GetCompositor();
}

void RenderWidgetHostViewChildFrame::HandleSwipeToMoveCursorGestureAck(
    const blink::WebGestureEvent& event) {
  if (!selection_controller_client_) {
    return;
  }

  switch (event.GetType()) {
    case blink::WebInputEvent::Type::kGestureScrollBegin: {
      if (!event.data.scroll_begin.cursor_control) {
        break;
      }
      swipe_to_move_cursor_activated_ = true;
      selection_controller_client_->OnSwipeToMoveCursorBegin();
      break;
    }
    case blink::WebInputEvent::Type::kGestureScrollEnd: {
      if (!swipe_to_move_cursor_activated_) {
        break;
      }
      swipe_to_move_cursor_activated_ = false;
      selection_controller_client_->OnSwipeToMoveCursorEnd();
      break;
    }
    default:
      break;
  }
}

}  // namespace content
