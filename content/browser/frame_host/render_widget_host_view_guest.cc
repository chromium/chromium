// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/render_widget_host_view_guest.h"

#include <memory>
#include <utility>

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_hittest.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/renderer_host/cursor_manager.h"
#include "content/browser/renderer_host/input/input_router.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/common/browser_plugin/browser_plugin_messages.h"
#include "content/common/frame_messages.h"
#include "content/common/input/web_touch_event_traits.h"
#include "content/common/text_input_state.h"
#include "content/common/widget_messages.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/use_zoom_for_dsf_policy.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "skia/ext/platform_canvas.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/geometry/dip_util.h"

#if defined(USE_AURA)
#include "content/browser/renderer_host/ui_events_helper.h"
#include "ui/aura/env.h"
#endif

namespace content {
namespace {

class ScopedInputScaleDisabler {
 public:
  ScopedInputScaleDisabler(RenderWidgetHostImpl* host, float scale_factor)
      : host_(host), scale_factor_(scale_factor) {
    if (IsUseZoomForDSFEnabled())
      host_->input_router()->SetDeviceScaleFactor(1.0f);
  }

  ~ScopedInputScaleDisabler() {
    if (IsUseZoomForDSFEnabled())
      host_->input_router()->SetDeviceScaleFactor(scale_factor_);
  }

 private:
  RenderWidgetHostImpl* host_;
  float scale_factor_;

  DISALLOW_COPY_AND_ASSIGN(ScopedInputScaleDisabler);
};

}  // namespace

// static
RenderWidgetHostViewGuest* RenderWidgetHostViewGuest::Create(
    RenderWidgetHost* widget,
    BrowserPluginGuest* guest,
    base::WeakPtr<RenderWidgetHostViewBase> platform_view) {
  RenderWidgetHostViewGuest* view =
      new RenderWidgetHostViewGuest(widget, guest, platform_view);
  view->Init();
  return view;
}

// static
RenderWidgetHostViewBase* RenderWidgetHostViewGuest::GetRootView(
    RenderWidgetHostViewBase* rwhv) {
  // If we're a pdf in a WebView, we could have nested guest views here.
  while (rwhv && rwhv->IsRenderWidgetHostViewGuest()) {
    rwhv = static_cast<RenderWidgetHostViewGuest*>(rwhv)
               ->GetOwnerRenderWidgetHostView();
  }
  if (!rwhv)
    return nullptr;

  // We could be a guest inside an oopif frame, in which case we're not the
  // root.
  if (rwhv->IsRenderWidgetHostViewChildFrame()) {
    rwhv = static_cast<RenderWidgetHostViewChildFrame*>(rwhv)
               ->GetRootRenderWidgetHostView();
  }
  return rwhv;
}

RenderWidgetHostViewBase* RenderWidgetHostViewGuest::GetParentView() {
  return GetOwnerRenderWidgetHostView();
}

RenderWidgetHostViewGuest::RenderWidgetHostViewGuest(
    RenderWidgetHost* widget_host,
    BrowserPluginGuest* guest,
    base::WeakPtr<RenderWidgetHostViewBase> platform_view)
    : RenderWidgetHostViewChildFrame(widget_host),
      // |guest| is NULL during test.
      guest_(guest ? guest->AsWeakPtr() : base::WeakPtr<BrowserPluginGuest>()),
      platform_view_(platform_view),
      weak_ptr_factory_(this) {
  // In tests |guest_| and therefore |owner| can be null.
  auto* owner = GetOwnerRenderWidgetHostView();
  if (owner)
    SetParentFrameSinkId(owner->GetFrameSinkId());

  gfx::NativeView view = GetNativeView();
  if (view)
    UpdateScreenInfo(view);
}

RenderWidgetHostViewGuest::~RenderWidgetHostViewGuest() {}

bool RenderWidgetHostViewGuest::OnMessageReceivedFromEmbedder(
    const IPC::Message& message,
    RenderWidgetHostImpl* embedder) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(RenderWidgetHostViewGuest, message,
                                   embedder)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_HandleInputEvent,
                        OnHandleInputEvent)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void RenderWidgetHostViewGuest::Show() {
  // If the WebContents associated with us showed an interstitial page in the
  // beginning, the teardown path might call WasShown() while |host_| is in
  // the process of destruction. Avoid calling WasShown below in this case.
  // TODO(lazyboy): We shouldn't be showing interstitial pages in guests in the
  // first place: http://crbug.com/273089.
  //
  // |guest_| is NULL during test.
  if ((guest_ && guest_->is_in_destruction()) || !host()->is_hidden())
    return;
  // Make sure the size of this view matches the size of the WebContentsView.
  // The two sizes may fall out of sync if we switch RenderWidgetHostViews,
  // resize, and then switch page, as is the case with interstitial pages.
  // NOTE: |guest_| is NULL in unit tests.
  if (guest_) {
    SetSize(guest_->web_contents()->GetViewBounds().size());
    // Since we were last shown, our renderer may have had a different surface
    // set (e.g. showing an interstitial), so we resend our current surface to
    // the renderer.
    SendSurfaceInfoToEmbedder();
  }
  host()->WasShown(false /* record_presentation_time */);
}

void RenderWidgetHostViewGuest::Hide() {
  // |guest_| is NULL during test.
  if ((guest_ && guest_->is_in_destruction()) || host()->is_hidden())
    return;
  host()->WasHidden();
}

void RenderWidgetHostViewGuest::SetSize(const gfx::Size& size) {
}

void RenderWidgetHostViewGuest::SetBounds(const gfx::Rect& rect) {
}

void RenderWidgetHostViewGuest::Focus() {
  // InterstitialPageImpl focuses views directly, so we place focus logic here.
  // InterstitialPages are not WebContents, and so BrowserPluginGuest does not
  // have direct access to the interstitial page's RenderWidgetHost.
  if (guest_)
    guest_->SetFocus(host(), true, blink::kWebFocusTypeNone);
}

bool RenderWidgetHostViewGuest::HasFocus() const {
  if (!guest_)
    return false;
  return guest_->focused();
}

void RenderWidgetHostViewGuest::PreProcessMouseEvent(
    const blink::WebMouseEvent& event) {
  if (event.GetType() == blink::WebInputEvent::kMouseDown) {
    DCHECK(guest_->GetOwnerRenderWidgetHostView());
    RenderWidgetHost* embedder =
        guest_->GetOwnerRenderWidgetHostView()->GetRenderWidgetHost();
    if (!embedder->GetView()->HasFocus())
      embedder->GetView()->Focus();

    // With direct routing, the embedder would not know to focus the guest on
    // click. Sends a synthetic event for the focusing side effect.
    // TODO(wjmaclean): When we remove BrowserPlugin, delete this code.
    // http://crbug.com/533069
    MaybeSendSyntheticTapGesture(event.PositionInWidget(),
                                 event.PositionInScreen());
  }
}

void RenderWidgetHostViewGuest::PreProcessTouchEvent(
    const blink::WebTouchEvent& event) {
  if (event.GetType() == blink::WebInputEvent::kTouchStart) {
    DCHECK(guest_->GetOwnerRenderWidgetHostView());
    RenderWidgetHost* embedder =
        guest_->GetOwnerRenderWidgetHostView()->GetRenderWidgetHost();
    if (!embedder->GetView()->HasFocus())
      embedder->GetView()->Focus();

    // With direct routing, the embedder would not know to focus the guest on
    // touch. Sends a synthetic event for the focusing side effect.
    // TODO(wjmaclean): When we remove BrowserPlugin, delete this code.
    // http://crbug.com/533069
    MaybeSendSyntheticTapGesture(event.touches[0].PositionInWidget(),
                                 event.touches[0].PositionInScreen());
  }
}

gfx::Rect RenderWidgetHostViewGuest::GetViewBounds() const {
  if (!guest_)
    return gfx::Rect();

  RenderWidgetHostViewBase* rwhv = GetOwnerRenderWidgetHostView();
  gfx::Rect embedder_bounds;
  if (rwhv)
    embedder_bounds = rwhv->GetViewBounds();
  return gfx::Rect(guest_->GetScreenCoordinates(embedder_bounds.origin()),
                   guest_->frame_rect().size());
}

gfx::Rect RenderWidgetHostViewGuest::GetBoundsInRootWindow() {
  return GetViewBounds();
}

gfx::PointF RenderWidgetHostViewGuest::TransformPointToRootCoordSpaceF(
    const gfx::PointF& point) {
  // LocalSurfaceId is not needed in Viz hit-test.
  if (!guest_ ||
      (!use_viz_hit_test_ && !last_activated_surface_info_.is_valid())) {
    return point;
  }

  RenderWidgetHostViewBase* root_rwhv = GetRootView(this);
  if (!root_rwhv)
    return point;

  gfx::PointF transformed_point = point;
  // TODO(wjmaclean): If we knew that TransformPointToLocalCoordSpace would
  // guarantee not to change transformed_point on failure, then we could skip
  // checking the function return value and directly return transformed_point.
  if (!root_rwhv->TransformPointToLocalCoordSpace(
          point, last_activated_surface_info_.id(), &transformed_point)) {
    return point;
  }
  return transformed_point;
}

bool RenderWidgetHostViewGuest::TransformPointToLocalCoordSpaceLegacy(
    const gfx::PointF& point,
    const viz::SurfaceId& original_surface,
    gfx::PointF* transformed_point) {
  *transformed_point = point;
  if (!guest_ || !last_activated_surface_info_.is_valid())
    return false;

  if (original_surface == last_activated_surface_info_.id())
    return true;

  *transformed_point =
      gfx::ConvertPointToPixel(current_surface_scale_factor(), point);
  viz::SurfaceHittest hittest(nullptr,
                              GetFrameSinkManager()->surface_manager());
  if (!hittest.TransformPointToTargetSurface(original_surface,
                                             last_activated_surface_info_.id(),
                                             transformed_point)) {
    return false;
  }

  *transformed_point = gfx::ConvertPointToDIP(current_surface_scale_factor(),
                                              *transformed_point);
  return true;
}

gfx::PointF RenderWidgetHostViewGuest::TransformRootPointToViewCoordSpace(
    const gfx::PointF& point) {
  RenderWidgetHostViewBase* root_rwhv = GetRootView(this);
  if (!root_rwhv)
    return point;

  gfx::PointF transformed_point;
  if (!root_rwhv->TransformPointToCoordSpaceForView(point, this,
                                                    &transformed_point)) {
    return point;
  }
  return transformed_point;
}

void RenderWidgetHostViewGuest::RenderProcessGone(
    base::TerminationStatus status,
    int error_code) {
  // The |platform_view_| gets destroyed before we get here if this view
  // is for an InterstitialPage.
  if (platform_view_)
    platform_view_->RenderProcessGone(status, error_code);

  RenderWidgetHostViewChildFrame::RenderProcessGone(status, error_code);
}

void RenderWidgetHostViewGuest::Destroy() {
  if (platform_view_)  // The platform view might have been destroyed already.
    platform_view_->Destroy();

  RenderWidgetHostViewBase* root_view = GetRootView(this);
  if (root_view)
    root_view->GetCursorManager()->ViewBeingDestroyed(this);

  // RenderWidgetHostViewChildFrame::Destroy destroys this object.
  RenderWidgetHostViewChildFrame::Destroy();
}

gfx::Size RenderWidgetHostViewGuest::GetCompositorViewportPixelSize() const {
  gfx::Size size;
  if (guest_) {
    size = gfx::ScaleToCeiledSize(guest_->frame_rect().size(),
                                  guest_->screen_info().device_scale_factor);
  }
  return size;
}

base::string16 RenderWidgetHostViewGuest::GetSelectedText() {
  return platform_view_->GetSelectedText();
}

base::string16 RenderWidgetHostViewGuest::GetSurroundingText() {
  return platform_view_->GetSurroundingText();
}

gfx::Range RenderWidgetHostViewGuest::GetSelectedRange() {
  return platform_view_->GetSelectedRange();
}

size_t RenderWidgetHostViewGuest::GetOffsetForSurroundingText() {
  return platform_view_->GetOffsetForSurroundingText();
}

void RenderWidgetHostViewGuest::SetNeedsBeginFrames(bool needs_begin_frames) {
  if (platform_view_)
    platform_view_->SetNeedsBeginFrames(needs_begin_frames);
}

TouchSelectionControllerClientManager*
RenderWidgetHostViewGuest::GetTouchSelectionControllerClientManager() {
  RenderWidgetHostView* root_view = GetRootView(this);
  if (!root_view)
    return nullptr;

  // There is only ever one manager, and it's owned by the root view.
  return root_view->GetTouchSelectionControllerClientManager();
}

void RenderWidgetHostViewGuest::SetTooltipText(
    const base::string16& tooltip_text) {
  RenderWidgetHostViewBase* root_view = GetRootView(this);
  if (root_view)
    root_view->GetCursorManager()->SetTooltipTextForView(this, tooltip_text);
}

void RenderWidgetHostViewGuest::OnDidUpdateVisualPropertiesComplete(
    const cc::RenderFrameMetadata& metadata) {
  if (guest_)
    guest_->DidUpdateVisualProperties(metadata);
  host()->SynchronizeVisualProperties();
}

void RenderWidgetHostViewGuest::OnAttached() {
  RegisterFrameSinkId();
#if defined(USE_AURA)
  if (features::IsMultiProcessMash()) {
    aura::Env::GetInstance()->ScheduleEmbed(
        GetWindowTreeClientFromRenderer(),
        base::BindOnce(&RenderWidgetHostViewGuest::OnGotEmbedToken,
                       weak_ptr_factory_.GetWeakPtr()));
  }
#endif
  SendSurfaceInfoToEmbedder();
}

RenderWidgetHostViewBase* RenderWidgetHostViewGuest::GetRootView() {
  return GetRootView(this);
}

void RenderWidgetHostViewGuest::InitAsChild(
    gfx::NativeView parent_view) {
  // This should never get called.
  NOTREACHED();
}

void RenderWidgetHostViewGuest::InitAsPopup(
    RenderWidgetHostView* parent_host_view, const gfx::Rect& bounds) {
  // This should never get called.
  NOTREACHED();
}

void RenderWidgetHostViewGuest::InitAsFullscreen(
    RenderWidgetHostView* reference_host_view) {
  // This should never get called.
  NOTREACHED();
}

gfx::NativeView RenderWidgetHostViewGuest::GetNativeView() const {
  if (!guest_)
    return gfx::NativeView();

  RenderWidgetHostView* rwhv = guest_->GetOwnerRenderWidgetHostView();
  if (!rwhv)
    return gfx::NativeView();
  return rwhv->GetNativeView();
}

gfx::NativeViewAccessible RenderWidgetHostViewGuest::GetNativeViewAccessible() {
  if (!guest_)
    return gfx::NativeViewAccessible();

  RenderWidgetHostView* rwhv = guest_->GetOwnerRenderWidgetHostView();
  if (!rwhv)
    return gfx::NativeViewAccessible();
  return rwhv->GetNativeViewAccessible();
}

void RenderWidgetHostViewGuest::UpdateCursor(const WebCursor& cursor) {
  // InterstitialPages are not WebContents so we cannot intercept
  // WidgetHostMsg_SetCursor for interstitial pages in BrowserPluginGuest.
  // All guest RenderViewHosts have RenderWidgetHostViewGuests however,
  // and so we will always hit this code path.
  if (!guest_)
    return;
  RenderWidgetHostViewBase* rwhvb = GetRootView(this);
  if (rwhvb && rwhvb->GetCursorManager())
    rwhvb->GetCursorManager()->UpdateCursor(this, cursor);
}

void RenderWidgetHostViewGuest::SetIsLoading(bool is_loading) {
  platform_view_->SetIsLoading(is_loading);
}

bool RenderWidgetHostViewGuest::HasSize() const {
  // RenderWidgetHostViewGuests are always hosting main frames, so the renderer
  // always have a size, which is sent on the CreateView IPC.
  return true;
}

void RenderWidgetHostViewGuest::TextInputStateChanged(
    const TextInputState& params) {
  if (!guest_)
    return;

  RenderWidgetHostViewBase* rwhv = GetOwnerRenderWidgetHostView();
  if (!rwhv)
    return;
  // Forward the information to embedding RWHV.
  rwhv->TextInputStateChanged(params);

  should_forward_text_selection_ =
      (params.type != ui::TEXT_INPUT_TYPE_NONE) && guest_ && guest_->focused();
}

void RenderWidgetHostViewGuest::ImeCancelComposition() {
  if (!guest_)
    return;

  RenderWidgetHostViewBase* rwhv = GetOwnerRenderWidgetHostView();
  if (!rwhv)
    return;
  // Forward the information to embedding RWHV.
  rwhv->ImeCancelComposition();
}

#if defined(OS_MACOSX) || defined(USE_AURA)
void RenderWidgetHostViewGuest::ImeCompositionRangeChanged(
    const gfx::Range& range,
    const std::vector<gfx::Rect>& character_bounds) {
  if (!guest_)
    return;

  RenderWidgetHostViewBase* rwhv = GetOwnerRenderWidgetHostView();
  if (!rwhv)
    return;
  std::vector<gfx::Rect> guest_character_bounds;
  for (size_t i = 0; i < character_bounds.size(); ++i) {
    guest_character_bounds.push_back(gfx::Rect(
        guest_->GetScreenCoordinates(character_bounds[i].origin()),
        character_bounds[i].size()));
  }
  // Forward the information to embedding RWHV.
  rwhv->ImeCompositionRangeChanged(range, guest_character_bounds);
}
#endif

void RenderWidgetHostViewGuest::SelectionChanged(const base::string16& text,
                                                 size_t offset,
                                                 const gfx::Range& range) {
  RenderWidgetHostViewBase* view = should_forward_text_selection_
                                       ? GetOwnerRenderWidgetHostView()
                                       : platform_view_.get();
  if (view)
    view->SelectionChanged(text, offset, range);
}

void RenderWidgetHostViewGuest::SelectionBoundsChanged(
    const WidgetHostMsg_SelectionBounds_Params& params) {
  if (!guest_)
    return;

  RenderWidgetHostViewBase* rwhv = GetOwnerRenderWidgetHostView();
  if (!rwhv)
    return;
  WidgetHostMsg_SelectionBounds_Params guest_params(params);
  guest_params.anchor_rect.set_origin(
      guest_->GetScreenCoordinates(params.anchor_rect.origin()));
  guest_params.focus_rect.set_origin(
      guest_->GetScreenCoordinates(params.focus_rect.origin()));
  rwhv->SelectionBoundsChanged(guest_params);
}

void RenderWidgetHostViewGuest::DidStopFlinging() {
  RenderWidgetHostViewBase* rwhv = this;
  // If we're a pdf in a WebView, we could have nested guest views here.
  while (rwhv && rwhv->IsRenderWidgetHostViewGuest()) {
    rwhv = static_cast<RenderWidgetHostViewGuest*>(rwhv)
               ->GetOwnerRenderWidgetHostView();
  }
  // DidStopFlinging() is used by TouchSelection to correctly detect the end of
  // scroll events, so we forward this to the top-level RenderWidgetHostViewBase
  // so it can be passed along to its TouchSelectionController.
  if (rwhv)
    rwhv->DidStopFlinging();
}

bool RenderWidgetHostViewGuest::LockMouse() {
  return platform_view_->LockMouse();
}

void RenderWidgetHostViewGuest::UnlockMouse() {
  platform_view_->UnlockMouse();
}

viz::FrameSinkId RenderWidgetHostViewGuest::GetRootFrameSinkId() {
  RenderWidgetHostViewBase* root_rwhv = GetRootView(this);
  if (root_rwhv)
    return root_rwhv->GetRootFrameSinkId();
  return viz::FrameSinkId();
}

const viz::LocalSurfaceId& RenderWidgetHostViewGuest::GetLocalSurfaceId()
    const {
  if (guest_)
    return guest_->local_surface_id();
  return viz::ParentLocalSurfaceIdAllocator::InvalidLocalSurfaceId();
}

void RenderWidgetHostViewGuest::DidCreateNewRendererCompositorFrameSink(
    viz::mojom::CompositorFrameSinkClient* renderer_compositor_frame_sink) {
  RenderWidgetHostViewChildFrame::DidCreateNewRendererCompositorFrameSink(
      renderer_compositor_frame_sink);
  platform_view_->DidCreateNewRendererCompositorFrameSink(
      renderer_compositor_frame_sink);
}

#if defined(OS_MACOSX)
void RenderWidgetHostViewGuest::SetActive(bool active) {
  platform_view_->SetActive(active);
}

void RenderWidgetHostViewGuest::ShowDefinitionForSelection() {
  // Note that if there were a dictionary overlay, that dictionary overlay
  // would target |guest_|. This path does not actually support getting the
  // attributed string and its point on the page, so it will not create an
  // overlay (it will open Dictionary.app), so the target NSView need not be
  // specified.
  // https://crbug.com/152438
  platform_view_->ShowDefinitionForSelection();
}

void RenderWidgetHostViewGuest::SpeakSelection() {
  platform_view_->SpeakSelection();
}
#endif  // defined(OS_MACOSX)

RenderWidgetHostViewBase*
RenderWidgetHostViewGuest::GetOwnerRenderWidgetHostView() const {
  return guest_ ? static_cast<RenderWidgetHostViewBase*>(
                      guest_->GetOwnerRenderWidgetHostView())
                : nullptr;
}

void RenderWidgetHostViewGuest::MaybeSendSyntheticTapGestureForTest(
    const blink::WebFloatPoint& position,
    const blink::WebFloatPoint& screen_position) const {
  MaybeSendSyntheticTapGesture(position, screen_position);
}

// TODO(wjmaclean): When we remove BrowserPlugin, delete this code.
// http://crbug.com/533069
void RenderWidgetHostViewGuest::MaybeSendSyntheticTapGesture(
    const blink::WebFloatPoint& position,
    const blink::WebFloatPoint& screen_position) const {
  if (!HasFocus()) {
    // We need to a account for the position of the guest view within the
    // embedder, as well as the fact that the embedder's host will add its
    // offset in screen coordinates before sending the event (with the latter
    // component just serving to confuse the renderer, hence why it should be
    // removed).
    gfx::Vector2d offset =
        GetViewBounds().origin() -
        GetOwnerRenderWidgetHostView()->GetBoundsInRootWindow().origin();
    blink::WebGestureEvent gesture_tap_event(
        blink::WebGestureEvent::kGestureTapDown,
        blink::WebInputEvent::kNoModifiers, ui::EventTimeForNow(),
        blink::kWebGestureDeviceTouchscreen);
    gesture_tap_event.SetPositionInWidget(
        blink::WebFloatPoint(position.x + offset.x(), position.y + offset.y()));
    gesture_tap_event.SetPositionInScreen(screen_position);
    // The touch action may not be set yet because this is still at the
    // Pre-processing stage of a mouse or a touch event. In this case, set the
    // touch action to Auto to prevent crashing.
    static_cast<RenderWidgetHostImpl*>(
        GetOwnerRenderWidgetHostView()->GetRenderWidgetHost())
        ->input_router()
        ->ForceSetTouchActionAuto();
    GetOwnerRenderWidgetHostView()->ProcessGestureEvent(
        gesture_tap_event, ui::LatencyInfo(ui::SourceEventType::TOUCH));

    gesture_tap_event.SetType(blink::WebGestureEvent::kGestureTapCancel);
    GetOwnerRenderWidgetHostView()->ProcessGestureEvent(
        gesture_tap_event, ui::LatencyInfo(ui::SourceEventType::TOUCH));
  }
}

void RenderWidgetHostViewGuest::WheelEventAck(
    const blink::WebMouseWheelEvent& event,
    InputEventAckState ack_result) {
  if (ack_result == INPUT_EVENT_ACK_STATE_NOT_CONSUMED ||
      ack_result == INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS) {
    guest_->ResendEventToEmbedder(event);
  }
}

void RenderWidgetHostViewGuest::GestureEventAck(
    const blink::WebGestureEvent& event,
    InputEventAckState ack_result) {
  bool not_consumed = ack_result == INPUT_EVENT_ACK_STATE_NOT_CONSUMED ||
                      ack_result == INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS;
  // GestureScrollBegin/End are always consumed by the guest, so we only
  // forward GestureScrollUpdate.
  // Consumed GestureScrollUpdates and GestureScrollBegins must still be
  // forwarded to the owner RWHV so it may update its state.
  if (event.GetType() == blink::WebInputEvent::kGestureScrollUpdate &&
      not_consumed) {
    guest_->ResendEventToEmbedder(event);
  } else if (event.GetType() == blink::WebInputEvent::kGestureScrollUpdate ||
             event.GetType() == blink::WebInputEvent::kGestureScrollBegin) {
    GetOwnerRenderWidgetHostView()->GestureEventAck(event, ack_result);
  }

  if (event.IsTouchpadZoomEvent())
    ProcessTouchpadZoomEventAckInRoot(event, ack_result);
}

void RenderWidgetHostViewGuest::ProcessTouchpadZoomEventAckInRoot(
    const blink::WebGestureEvent& event,
    InputEventAckState ack_result) {
  DCHECK(event.IsTouchpadZoomEvent());

  RenderWidgetHostViewBase* root_rwhv = GetRootView(this);
  if (!root_rwhv)
    return;

  blink::WebGestureEvent root_event(event);
  const gfx::PointF root_point =
      TransformPointToRootCoordSpaceF(event.PositionInWidget());
  root_event.SetPositionInWidget(root_point);
  root_rwhv->GestureEventAck(root_event, ack_result);
}

InputEventAckState RenderWidgetHostViewGuest::FilterInputEvent(
    const blink::WebInputEvent& input_event) {
  InputEventAckState ack_state =
      RenderWidgetHostViewChildFrame::FilterInputEvent(input_event);
  if (ack_state != INPUT_EVENT_ACK_STATE_NOT_CONSUMED)
    return ack_state;

  // The owner RWHV may want to consume the guest's GestureScrollUpdates.
  // Also, we don't resend GestureFlingStarts, GestureScrollBegins, or
  // GestureScrollEnds, so we let the owner RWHV know about them here.
  if (input_event.GetType() == blink::WebInputEvent::kGestureScrollUpdate ||
      input_event.GetType() == blink::WebInputEvent::kGestureFlingStart ||
      input_event.GetType() == blink::WebInputEvent::kGestureScrollBegin ||
      input_event.GetType() == blink::WebInputEvent::kGestureScrollEnd) {
    const blink::WebGestureEvent& gesture_event =
        static_cast<const blink::WebGestureEvent&>(input_event);
    return GetOwnerRenderWidgetHostView()->FilterChildGestureEvent(
        gesture_event);
  }

  return INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
}

void RenderWidgetHostViewGuest::GetScreenInfo(ScreenInfo* screen_info) const {
  DCHECK(screen_info);
  if (guest_)
    *screen_info = guest_->screen_info();
  else
    RenderWidgetHostViewBase::GetScreenInfo(screen_info);
}

void RenderWidgetHostViewGuest::EnableAutoResize(const gfx::Size& min_size,
                                                 const gfx::Size& max_size) {
  if (guest_)
    guest_->EnableAutoResize(min_size, max_size);
}

void RenderWidgetHostViewGuest::DisableAutoResize(const gfx::Size& new_size) {
  if (guest_)
    guest_->DisableAutoResize();
}

viz::ScopedSurfaceIdAllocator
RenderWidgetHostViewGuest::DidUpdateVisualProperties(
    const cc::RenderFrameMetadata& metadata) {
  base::OnceCallback<void()> allocation_task = base::BindOnce(
      &RenderWidgetHostViewGuest::OnDidUpdateVisualPropertiesComplete,
      weak_ptr_factory_.GetWeakPtr(), metadata);
  return viz::ScopedSurfaceIdAllocator(std::move(allocation_task));
}

bool RenderWidgetHostViewGuest::IsRenderWidgetHostViewGuest() {
  return true;
}

void RenderWidgetHostViewGuest::OnHandleInputEvent(
    RenderWidgetHostImpl* embedder,
    int browser_plugin_instance_id,
    const blink::WebInputEvent* event) {
  // WebMouseWheelEvents go into a queue, and may not be forwarded to the
  // renderer until after this method goes out of scope. Therefore we need to
  // explicitly remove the additional device scale factor from the coordinates
  // before allowing the event to be queued.
  if (IsUseZoomForDSFEnabled() &&
      event->GetType() == blink::WebInputEvent::kMouseWheel) {
    blink::WebMouseWheelEvent rescaled_event =
        *static_cast<const blink::WebMouseWheelEvent*>(event);
    rescaled_event.SetPositionInWidget(
        rescaled_event.PositionInWidget().x / current_device_scale_factor(),
        rescaled_event.PositionInWidget().y / current_device_scale_factor());
    rescaled_event.delta_x /= current_device_scale_factor();
    rescaled_event.delta_y /= current_device_scale_factor();
    rescaled_event.wheel_ticks_x /= current_device_scale_factor();
    rescaled_event.wheel_ticks_y /= current_device_scale_factor();
    ui::LatencyInfo latency_info(ui::SourceEventType::WHEEL);
    host()->ForwardWheelEventWithLatencyInfo(rescaled_event, latency_info);
    return;
  }

  ScopedInputScaleDisabler disable(host(), current_device_scale_factor());
  if (blink::WebInputEvent::IsMouseEventType(event->GetType())) {
    host()->ForwardMouseEvent(*static_cast<const blink::WebMouseEvent*>(event));
    return;
  }

  if (event->GetType() == blink::WebInputEvent::kMouseWheel) {
    ui::LatencyInfo latency_info(ui::SourceEventType::WHEEL);
    host()->ForwardWheelEventWithLatencyInfo(
        *static_cast<const blink::WebMouseWheelEvent*>(event), latency_info);
    return;
  }

  if (blink::WebInputEvent::IsKeyboardEventType(event->GetType())) {
    NativeWebKeyboardEvent keyboard_event(
        *static_cast<const blink::WebKeyboardEvent*>(event), GetNativeView());
    host()->ForwardKeyboardEvent(keyboard_event);
    return;
  }

  if (blink::WebInputEvent::IsTouchEventType(event->GetType())) {
    if (event->GetType() == blink::WebInputEvent::kTouchStart &&
        !embedder->GetView()->HasFocus()) {
      embedder->GetView()->Focus();
    }
    ui::LatencyInfo latency_info(ui::SourceEventType::TOUCH);
    host()->ForwardTouchEventWithLatencyInfo(
        *static_cast<const blink::WebTouchEvent*>(event), latency_info);
    return;
  }

  if (blink::WebInputEvent::IsGestureEventType(event->GetType())) {
    const blink::WebGestureEvent& gesture_event =
        *static_cast<const blink::WebGestureEvent*>(event);

    // We don't forward inertial GestureScrollUpdates to the guest anymore
    // since it now receives GestureFlingStart and will have its own fling
    // curve generating GestureScrollUpdate events for it.
    // TODO(wjmaclean): Should we try to avoid creating a fling curve in the
    // embedder renderer in this case? BrowserPlugin can return 'true' for
    // handleInputEvent() on a GestureFlingStart, and we could use this as
    // a signal to let the guest handle the fling, though we'd need to be
    // sure other plugins would behave appropriately (i.e. return 'false').
    if (gesture_event.GetType() == blink::WebInputEvent::kGestureScrollUpdate &&
        gesture_event.data.scroll_update.inertial_phase ==
            blink::WebGestureEvent::kMomentumPhase) {
      return;
    }
    host()->ForwardGestureEvent(gesture_event);
    return;
  }
}

#if defined(USE_AURA)
void RenderWidgetHostViewGuest::OnGotEmbedToken(
    const base::UnguessableToken& token) {
  if (!guest_)
    return;

  guest_->SendMessageToEmbedder(
      std::make_unique<BrowserPluginMsg_SetMusEmbedToken>(
          guest_->browser_plugin_instance_id(), token));
}
#endif

}  // namespace content
