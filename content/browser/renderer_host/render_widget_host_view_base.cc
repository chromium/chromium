// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_base.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "components/viz/common/features.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/renderer_host/delegated_frame_host.h"
#include "content/browser/renderer_host/display_util.h"
#include "content/browser/renderer_host/event_with_latency_info.h"
#include "content/browser/renderer_host/input/mouse_wheel_phase_handler.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target_base.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/browser/renderer_host/render_widget_host_owner_delegate.h"
#include "content/browser/renderer_host/render_widget_host_view_base_observer.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/renderer_host/text_input_manager.h"
#include "content/common/content_switches_internal.h"
#include "third_party/blink/public/mojom/page/record_content_to_visible_time_request.mojom.h"
#include "ui/base/layout.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"

namespace content {

RenderWidgetHostViewBase::RenderWidgetHostViewBase(RenderWidgetHost* host)
    : host_(RenderWidgetHostImpl::From(host)) {
}

RenderWidgetHostViewBase::~RenderWidgetHostViewBase() {
  DCHECK(!keyboard_locked_);
  DCHECK(!IsMouseLocked());
  // We call this here to guarantee that observers are notified before we go
  // away. However, some subclasses may wish to call this earlier in their
  // shutdown process, e.g. to force removal from
  // RenderWidgetHostInputEventRouter's surface map before relinquishing a
  // host pointer. There is no harm in calling NotifyObserversAboutShutdown()
  // twice, as the observers are required to de-register on the first call, and
  // so the second call does nothing.
  NotifyObserversAboutShutdown();
  // If we have a live reference to |text_input_manager_|, we should unregister
  // so that the |text_input_manager_| will free its state.
  if (text_input_manager_)
    text_input_manager_->Unregister(this);
}

RenderWidgetHostImpl* RenderWidgetHostViewBase::GetFocusedWidget() const {
  return host() && host()->delegate()
             ? host()->delegate()->GetFocusedRenderWidgetHost(host())
             : nullptr;
}

RenderWidgetHost* RenderWidgetHostViewBase::GetRenderWidgetHost() {
  return host();
}

void RenderWidgetHostViewBase::SetContentBackgroundColor(SkColor color) {
  if (content_background_color_ == color)
    return;

  content_background_color_ = color;
  UpdateBackgroundColor();
}

void RenderWidgetHostViewBase::NotifyObserversAboutShutdown() {
  // Note: RenderWidgetHostInputEventRouter is an observer, and uses the
  // following notification to remove this view from its surface owners map.
  for (auto& observer : observers_)
    observer.OnRenderWidgetHostViewBaseDestroyed(this);
  // All observers are required to disconnect after they are notified.
  DCHECK(!observers_.might_have_observers());
}

MouseWheelPhaseHandler* RenderWidgetHostViewBase::GetMouseWheelPhaseHandler() {
  return nullptr;
}

void RenderWidgetHostViewBase::StopFlingingIfNecessary(
    const blink::WebGestureEvent& event,
    blink::mojom::InputEventResultState ack_result) {
  // Reset view_stopped_flinging_for_test_ at the beginning of the scroll
  // sequence.
  if (event.GetType() == blink::WebInputEvent::Type::kGestureScrollBegin)
    view_stopped_flinging_for_test_ = false;

  bool processed = blink::mojom::InputEventResultState::kConsumed == ack_result;
  if (!processed &&
      event.GetType() == blink::WebInputEvent::Type::kGestureScrollUpdate &&
      event.data.scroll_update.inertial_phase ==
          blink::WebGestureEvent::InertialPhaseState::kMomentum &&
      event.SourceDevice() != blink::WebGestureDevice::kSyntheticAutoscroll) {
    StopFling();
    view_stopped_flinging_for_test_ = true;
  }
}

void RenderWidgetHostViewBase::UpdateIntrinsicSizingInfo(
    blink::mojom::IntrinsicSizingInfoPtr sizing_info) {}

gfx::Size RenderWidgetHostViewBase::GetCompositorViewportPixelSize() {
  return gfx::ScaleToCeiledSize(GetRequestedRendererSize(),
                                GetDeviceScaleFactor());
}

void RenderWidgetHostViewBase::SelectionBoundsChanged(
    const gfx::Rect& anchor_rect,
    base::i18n::TextDirection anchor_dir,
    const gfx::Rect& focus_rect,
    base::i18n::TextDirection focus_dir,
    bool is_anchor_first) {
#if !defined(OS_ANDROID)
  if (GetTextInputManager())
    GetTextInputManager()->SelectionBoundsChanged(
        this, anchor_rect, anchor_dir, focus_rect, focus_dir, is_anchor_first);
#else
  NOTREACHED() << "Selection bounds should be routed through the compositor.";
#endif
}

int RenderWidgetHostViewBase::GetMouseWheelMinimumGranularity() const {
  // Most platforms can specify the floating-point delta in the wheel event so
  // they don't have a minimum granularity. Android is currently the only
  // platform that overrides this.
  return 0;
}

RenderWidgetHostViewBase* RenderWidgetHostViewBase::GetRootView() {
  return this;
}

void RenderWidgetHostViewBase::SelectionChanged(const base::string16& text,
                                                size_t offset,
                                                const gfx::Range& range) {
  if (GetTextInputManager())
    GetTextInputManager()->SelectionChanged(this, text, offset, range);
}

gfx::Size RenderWidgetHostViewBase::GetRequestedRendererSize() {
  return GetViewBounds().size();
}

uint32_t RenderWidgetHostViewBase::GetCaptureSequenceNumber() const {
  // TODO(vmpstr): Implement this for overrides other than aura and child frame.
  NOTIMPLEMENTED_LOG_ONCE();
  return 0u;
}

ui::TextInputClient* RenderWidgetHostViewBase::GetTextInputClient() {
  NOTREACHED();
  return nullptr;
}

void RenderWidgetHostViewBase::SetIsInVR(bool is_in_vr) {
  NOTIMPLEMENTED_LOG_ONCE();
}

bool RenderWidgetHostViewBase::IsInVR() const {
  return false;
}

viz::FrameSinkId RenderWidgetHostViewBase::GetRootFrameSinkId() {
  return viz::FrameSinkId();
}

bool RenderWidgetHostViewBase::IsSurfaceAvailableForCopy() {
  return false;
}

void RenderWidgetHostViewBase::CopyMainAndPopupFromSurface(
    base::WeakPtr<RenderWidgetHostImpl> main_host,
    base::WeakPtr<DelegatedFrameHost> main_frame_host,
    base::WeakPtr<RenderWidgetHostImpl> popup_host,
    base::WeakPtr<DelegatedFrameHost> popup_frame_host,
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    float scale_factor,
    base::OnceCallback<void(const SkBitmap&)> callback) {
  if (!main_host || !main_frame_host)
    return;

#if defined(OS_ANDROID)
  NOTREACHED()
      << "RenderWidgetHostViewAndroid::CopyFromSurface calls "
         "DelegatedFrameHostAndroid::CopyFromCompositingSurface directly, "
         "and popups are not supported.";
  return;
#else
  if (!popup_host || !popup_frame_host) {
    // No popup - just call CopyFromCompositingSurface once.
    main_frame_host->CopyFromCompositingSurface(src_subrect, dst_size,
                                                std::move(callback));
    return;
  }

  // First locate the popup relative to the main page, in DIPs
  const gfx::Point parent_location =
      main_host->GetView()->GetBoundsInRootWindow().origin();
  const gfx::Point popup_location =
      popup_host->GetView()->GetBoundsInRootWindow().origin();
  const gfx::Point offset_dips =
      PointAtOffsetFromOrigin(popup_location - parent_location);
  const gfx::Vector2d offset_physical =
      ScaleToFlooredPoint(offset_dips, scale_factor).OffsetFromOrigin();

  // Queue up the request for the MAIN frame image first, but with a
  // callback that launches a second request for the popup image.
  //  1. Call CopyFromCompositingSurface for the main frame, with callback
  //     |main_image_done_callback|. Inside |main_image_done_callback|:
  //    a. Call CopyFromCompositingSurface again, this time on the popup
  //       frame. For this call, build a new callback, |popup_done_callback|,
  //       which:
  //      i. Takes the main image as a parameter, combines the main image with
  //         the just-acquired popup image, and then calls the original
  //         (outer) callback with the combined image.
  auto main_image_done_callback = base::BindOnce(
      [](base::OnceCallback<void(const SkBitmap&)> final_callback,
         const gfx::Vector2d offset,
         base::WeakPtr<DelegatedFrameHost> popup_frame_host,
         const gfx::Rect src_subrect, const gfx::Size dst_size,
         const SkBitmap& main_image) {
        if (!popup_frame_host)
          return;

        // Build a new callback that actually combines images.
        auto popup_done_callback = base::BindOnce(
            [](base::OnceCallback<void(const SkBitmap&)> final_callback,
               const gfx::Vector2d offset, const SkBitmap& main_image,
               const SkBitmap& popup_image) {
              // Draw popup_image into main_image.
              SkCanvas canvas(main_image);
              canvas.drawBitmap(popup_image, offset.x(), offset.y());
              std::move(final_callback).Run(main_image);
            },
            std::move(final_callback), offset, std::move(main_image));

        // Second, request the popup image.
        gfx::Rect popup_subrect(src_subrect - offset);
        popup_frame_host->CopyFromCompositingSurface(
            popup_subrect, dst_size, std::move(popup_done_callback));
      },
      std::move(callback), offset_physical, popup_frame_host, src_subrect,
      dst_size);

  // Request the main image (happens first).
  main_frame_host->CopyFromCompositingSurface(
      src_subrect, dst_size, std::move(main_image_done_callback));
#endif
}

void RenderWidgetHostViewBase::CopyFromSurface(
    const gfx::Rect& src_rect,
    const gfx::Size& output_size,
    base::OnceCallback<void(const SkBitmap&)> callback) {
  NOTIMPLEMENTED_LOG_ONCE();
  std::move(callback).Run(SkBitmap());
}

std::unique_ptr<viz::ClientFrameSinkVideoCapturer>
RenderWidgetHostViewBase::CreateVideoCapturer() {
  std::unique_ptr<viz::ClientFrameSinkVideoCapturer> video_capturer =
      GetHostFrameSinkManager()->CreateVideoCapturer();
  video_capturer->ChangeTarget(GetFrameSinkId());
  return video_capturer;
}

base::string16 RenderWidgetHostViewBase::GetSelectedText() {
  if (!GetTextInputManager())
    return base::string16();
  return GetTextInputManager()->GetTextSelection(this)->selected_text();
}

void RenderWidgetHostViewBase::SetBackgroundColor(SkColor color) {
  // TODO(danakj): OPAQUE colors only make sense for main frame widgets,
  // as child frames are always transparent background. We should move this to
  // RenderView instead.
  DCHECK(SkColorGetA(color) == SK_AlphaOPAQUE ||
         SkColorGetA(color) == SK_AlphaTRANSPARENT);
  if (default_background_color_ == color)
    return;

  bool opaque = default_background_color_
                    ? SkColorGetA(*default_background_color_)
                    : SK_AlphaOPAQUE;
  default_background_color_ = color;
  UpdateBackgroundColor();
  if (opaque != (SkColorGetA(color) == SK_AlphaOPAQUE)) {
    if (host()->owner_delegate()) {
      host()->owner_delegate()->SetBackgroundOpaque(SkColorGetA(color) ==
                                                    SK_AlphaOPAQUE);
    }
  }
}

base::Optional<SkColor> RenderWidgetHostViewBase::GetBackgroundColor() {
  if (content_background_color_)
    return content_background_color_;
  return default_background_color_;
}

bool RenderWidgetHostViewBase::IsMouseLocked() {
  return false;
}

bool RenderWidgetHostViewBase::GetIsMouseLockedUnadjustedMovementForTesting() {
  return false;
}

bool RenderWidgetHostViewBase::LockKeyboard(
    base::Optional<base::flat_set<ui::DomCode>> codes) {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

void RenderWidgetHostViewBase::UnlockKeyboard() {
  NOTIMPLEMENTED_LOG_ONCE();
}

bool RenderWidgetHostViewBase::IsKeyboardLocked() {
  return keyboard_locked_;
}

base::flat_map<std::string, std::string>
RenderWidgetHostViewBase::GetKeyboardLayoutMap() {
  NOTIMPLEMENTED_LOG_ONCE();
  return base::flat_map<std::string, std::string>();
}

blink::mojom::InputEventResultState RenderWidgetHostViewBase::FilterInputEvent(
    const blink::WebInputEvent& input_event) {
  // By default, input events are simply forwarded to the renderer.
  return blink::mojom::InputEventResultState::kNotConsumed;
}

void RenderWidgetHostViewBase::WheelEventAck(
    const blink::WebMouseWheelEvent& event,
    blink::mojom::InputEventResultState ack_result) {}

void RenderWidgetHostViewBase::GestureEventAck(
    const blink::WebGestureEvent& event,
    blink::mojom::InputEventResultState ack_result) {}

void RenderWidgetHostViewBase::ChildDidAckGestureEvent(
    const blink::WebGestureEvent& event,
    blink::mojom::InputEventResultState ack_result) {}

void RenderWidgetHostViewBase::ForwardTouchpadZoomEventIfNecessary(
    const blink::WebGestureEvent& event,
    blink::mojom::InputEventResultState ack_result) {
  if (!event.IsTouchpadZoomEvent())
    return;
  if (!event.NeedsWheelEvent())
    return;

  switch (event.GetType()) {
    case blink::WebInputEvent::Type::kGesturePinchBegin:
      // Don't send the begin event until we get the first unconsumed update, so
      // that we elide pinch gesture steams consisting of only a begin and end.
      pending_touchpad_pinch_begin_ = event;
      pending_touchpad_pinch_begin_->SetNeedsWheelEvent(false);
      break;
    case blink::WebInputEvent::Type::kGesturePinchUpdate:
      if (ack_result != blink::mojom::InputEventResultState::kConsumed &&
          !event.data.pinch_update.zoom_disabled) {
        if (pending_touchpad_pinch_begin_) {
          host()->ForwardGestureEvent(*pending_touchpad_pinch_begin_);
          pending_touchpad_pinch_begin_.reset();
        }
        // Now that the synthetic wheel event has gone unconsumed, we have the
        // pinch event actually change the page scale.
        blink::WebGestureEvent pinch_event(event);
        pinch_event.SetNeedsWheelEvent(false);
        host()->ForwardGestureEvent(pinch_event);
      }
      break;
    case blink::WebInputEvent::Type::kGesturePinchEnd:
      if (pending_touchpad_pinch_begin_) {
        pending_touchpad_pinch_begin_.reset();
      } else {
        blink::WebGestureEvent pinch_end_event(event);
        pinch_end_event.SetNeedsWheelEvent(false);
        host()->ForwardGestureEvent(pinch_end_event);
      }
      break;
    case blink::WebInputEvent::Type::kGestureDoubleTap:
      if (ack_result != blink::mojom::InputEventResultState::kConsumed) {
        blink::WebGestureEvent double_tap(event);
        double_tap.SetNeedsWheelEvent(false);
        // TODO(mcnee): Support double-tap zoom gesture for OOPIFs. For now,
        // we naively send this to the main frame. If this is over an OOPIF,
        // then the iframe element will incorrectly be used for the scale
        // calculation rather than the element in the OOPIF.
        // https://crbug.com/758348
        host()->ForwardGestureEvent(double_tap);
      }
      break;
    default:
      NOTREACHED();
  }
}

bool RenderWidgetHostViewBase::HasFallbackSurface() const {
  NOTREACHED();
  return false;
}

void RenderWidgetHostViewBase::SetWidgetType(WidgetType widget_type) {
  widget_type_ = widget_type;
}

WidgetType RenderWidgetHostViewBase::GetWidgetType() {
  return widget_type_;
}

BrowserAccessibilityManager*
RenderWidgetHostViewBase::CreateBrowserAccessibilityManager(
    BrowserAccessibilityDelegate* delegate,
    bool for_root_frame) {
  NOTREACHED();
  return nullptr;
}

gfx::AcceleratedWidget
    RenderWidgetHostViewBase::AccessibilityGetAcceleratedWidget() {
  return gfx::kNullAcceleratedWidget;
}

gfx::NativeViewAccessible
    RenderWidgetHostViewBase::AccessibilityGetNativeViewAccessible() {
  return nullptr;
}

gfx::NativeViewAccessible
RenderWidgetHostViewBase::AccessibilityGetNativeViewAccessibleForWindow() {
  return nullptr;
}

bool RenderWidgetHostViewBase::RequestRepaintForTesting() {
  return false;
}

void RenderWidgetHostViewBase::ProcessAckedTouchEvent(
    const TouchEventWithLatencyInfo& touch,
    blink::mojom::InputEventResultState ack_result) {
  NOTREACHED();
}

void RenderWidgetHostViewBase::UpdateScreenInfo(gfx::NativeView view) {
  if (host() && host()->delegate())
    host()->delegate()->SendScreenRects();

  if (HasDisplayPropertyChanged(view) && host()) {
    OnSynchronizedDisplayPropertiesChanged();
    host()->NotifyScreenInfoChanged();
  }
}

bool RenderWidgetHostViewBase::HasDisplayPropertyChanged(gfx::NativeView view) {
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestView(view);
  if (current_display_area_ == display.work_area() &&
      current_device_scale_factor_ == display.device_scale_factor() &&
      current_display_rotation_ == display.rotation() &&
      current_display_color_spaces_ == display.color_spaces()) {
    return false;
  }

  current_display_area_ = display.work_area();
  current_device_scale_factor_ = display.device_scale_factor();
  current_display_rotation_ = display.rotation();
  current_display_color_spaces_ = display.color_spaces();
  return true;
}

void RenderWidgetHostViewBase::DidUnregisterFromTextInputManager(
    TextInputManager* text_input_manager) {
  DCHECK(text_input_manager && text_input_manager_ == text_input_manager);

  text_input_manager_ = nullptr;
}

void RenderWidgetHostViewBase::EnableAutoResize(const gfx::Size& min_size,
                                                const gfx::Size& max_size) {
  host()->SetAutoResize(true, min_size, max_size);
  host()->SynchronizeVisualProperties();
}

void RenderWidgetHostViewBase::DisableAutoResize(const gfx::Size& new_size) {
  if (!new_size.IsEmpty())
    SetSize(new_size);
  host()->SetAutoResize(false, gfx::Size(), gfx::Size());
  host()->SynchronizeVisualProperties();
}

viz::ScopedSurfaceIdAllocator
RenderWidgetHostViewBase::DidUpdateVisualProperties(
    const cc::RenderFrameMetadata& metadata) {
  // This doesn't suppress allocation. Derived classes that need suppression
  // should override this function.
  base::OnceCallback<void()> allocation_task =
      base::BindOnce(&RenderWidgetHostViewBase::SynchronizeVisualProperties,
                     weak_factory_.GetWeakPtr());
  return viz::ScopedSurfaceIdAllocator(std::move(allocation_task));
}

base::WeakPtr<RenderWidgetHostViewBase> RenderWidgetHostViewBase::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void RenderWidgetHostViewBase::GetScreenInfo(blink::ScreenInfo* screen_info) {
  DisplayUtil::GetNativeViewScreenInfo(screen_info, GetNativeView());
}

float RenderWidgetHostViewBase::GetDeviceScaleFactor() {
  blink::ScreenInfo screen_info;
  GetScreenInfo(&screen_info);
  return screen_info.device_scale_factor;
}

void RenderWidgetHostViewBase::OnAutoscrollStart() {
  if (!GetMouseWheelPhaseHandler())
    return;

  // End the current scrolling seqeunce when autoscrolling starts.
  GetMouseWheelPhaseHandler()->DispatchPendingWheelEndEvent();
}

gfx::Size RenderWidgetHostViewBase::GetVisibleViewportSize() {
  return GetViewBounds().size();
}

void RenderWidgetHostViewBase::SetInsets(const gfx::Insets& insets) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void RenderWidgetHostViewBase::DisplayCursor(const WebCursor& cursor) {
  return;
}

CursorManager* RenderWidgetHostViewBase::GetCursorManager() {
  return nullptr;
}

void RenderWidgetHostViewBase::TransformPointToRootSurface(gfx::PointF* point) {
  return;
}

const DisplayFeature* RenderWidgetHostViewBase::GetDisplayFeature() {
  return base::OptionalOrNullptr(display_feature_);
}

void RenderWidgetHostViewBase::SetDisplayFeatureForTesting(
    base::Optional<DisplayFeature> display_feature) {
  display_feature_ = std::move(display_feature);
}

void RenderWidgetHostViewBase::OnDidNavigateMainFrameToNewPage() {
}

void RenderWidgetHostViewBase::OnFrameTokenChangedForView(
    uint32_t frame_token) {
  if (host())
    host()->DidProcessFrame(frame_token);
}

bool RenderWidgetHostViewBase::ScreenRectIsUnstableFor(
    const blink::WebInputEvent& event) {
  return false;
}

void RenderWidgetHostViewBase::ProcessMouseEvent(
    const blink::WebMouseEvent& event,
    const ui::LatencyInfo& latency) {
  // TODO(crbug.com/814674): Figure out the reason |host| is null here in all
  // Process* functions.
  if (!host())
    return;

  PreProcessMouseEvent(event);
  host()->ForwardMouseEventWithLatencyInfo(event, latency);
}

void RenderWidgetHostViewBase::ProcessMouseWheelEvent(
    const blink::WebMouseWheelEvent& event,
    const ui::LatencyInfo& latency) {
  if (!host())
    return;
  host()->ForwardWheelEventWithLatencyInfo(event, latency);
}

void RenderWidgetHostViewBase::ProcessTouchEvent(
    const blink::WebTouchEvent& event,
    const ui::LatencyInfo& latency) {
  if (!host())
    return;

  PreProcessTouchEvent(event);
  host()->ForwardTouchEventWithLatencyInfo(event, latency);
}

void RenderWidgetHostViewBase::ProcessGestureEvent(
    const blink::WebGestureEvent& event,
    const ui::LatencyInfo& latency) {
  if (!host())
    return;
  host()->ForwardGestureEventWithLatencyInfo(event, latency);
}

gfx::PointF RenderWidgetHostViewBase::TransformPointToRootCoordSpaceF(
    const gfx::PointF& point) {
  return point;
}

gfx::PointF RenderWidgetHostViewBase::TransformRootPointToViewCoordSpace(
    const gfx::PointF& point) {
  return point;
}

bool RenderWidgetHostViewBase::TransformPointToCoordSpaceForView(
    const gfx::PointF& point,
    RenderWidgetHostViewBase* target_view,
    gfx::PointF* transformed_point) {
  NOTREACHED();
  return true;
}

bool RenderWidgetHostViewBase::IsRenderWidgetHostViewChildFrame() {
  return false;
}

bool RenderWidgetHostViewBase::HasSize() const {
  return true;
}

void RenderWidgetHostViewBase::Destroy() {
  host_ = nullptr;
}

bool RenderWidgetHostViewBase::CanSynchronizeVisualProperties() {
  return true;
}

std::vector<std::unique_ptr<ui::TouchEvent>>
RenderWidgetHostViewBase::ExtractAndCancelActiveTouches() {
  return {};
}

void RenderWidgetHostViewBase::TextInputStateChanged(
    const ui::mojom::TextInputState& text_input_state) {
  if (GetTextInputManager())
    GetTextInputManager()->UpdateTextInputState(this, text_input_state);
}

void RenderWidgetHostViewBase::ImeCancelComposition() {
  if (GetTextInputManager())
    GetTextInputManager()->ImeCancelComposition(this);
}

void RenderWidgetHostViewBase::ImeCompositionRangeChanged(
    const gfx::Range& range,
    const std::vector<gfx::Rect>& character_bounds) {
  if (GetTextInputManager()) {
    GetTextInputManager()->ImeCompositionRangeChanged(this, range,
                                                      character_bounds);
  }
}

TextInputManager* RenderWidgetHostViewBase::GetTextInputManager() {
  if (text_input_manager_)
    return text_input_manager_;

  if (!host() || !host()->delegate())
    return nullptr;

  // This RWHV needs to be registered with the TextInputManager so that the
  // TextInputManager starts tracking its state, and observing its lifetime.
  text_input_manager_ = host()->delegate()->GetTextInputManager();
  if (text_input_manager_)
    text_input_manager_->Register(this);

  return text_input_manager_;
}

void RenderWidgetHostViewBase::StopFling() {
  if (!host())
    return;

  host()->StopFling();

  // In case of scroll bubbling tells the child's fling controller which is in
  // charge of generating GSUs to stop flinging.
  if (host()->delegate() && host()->delegate()->GetInputEventRouter()) {
    host()->delegate()->GetInputEventRouter()->StopFling();
  }
}

void RenderWidgetHostViewBase::AddObserver(
    RenderWidgetHostViewBaseObserver* observer) {
  observers_.AddObserver(observer);
}

void RenderWidgetHostViewBase::RemoveObserver(
    RenderWidgetHostViewBaseObserver* observer) {
  observers_.RemoveObserver(observer);
}

TouchSelectionControllerClientManager*
RenderWidgetHostViewBase::GetTouchSelectionControllerClientManager() {
  return nullptr;
}

void RenderWidgetHostViewBase::SetRecordContentToVisibleTimeRequest(
    base::TimeTicks start_time,
    bool destination_is_loaded,
    bool show_reason_tab_switching,
    bool show_reason_unoccluded,
    bool show_reason_bfcache_restore) {
  auto record_tab_switch_time_request =
      blink::mojom::RecordContentToVisibleTimeRequest::New(
          start_time, destination_is_loaded, show_reason_tab_switching,
          show_reason_unoccluded, show_reason_bfcache_restore);

  if (last_record_tab_switch_time_request_) {
    blink::UpdateRecordContentToVisibleTimeRequest(
        *record_tab_switch_time_request, *last_record_tab_switch_time_request_);
  } else {
    last_record_tab_switch_time_request_ =
        std::move(record_tab_switch_time_request);
  }
}

blink::mojom::RecordContentToVisibleTimeRequestPtr
RenderWidgetHostViewBase::TakeRecordContentToVisibleTimeRequest() {
  return std::move(last_record_tab_switch_time_request_);
}

void RenderWidgetHostViewBase::SynchronizeVisualProperties() {
  if (host())
    host()->SynchronizeVisualProperties();
}

void RenderWidgetHostViewBase::DidNavigate() {
  if (host())
    host()->SynchronizeVisualProperties();
}

// TODO(wjmaclean): Would it simplify this function if we re-implemented it
// using GetTransformToViewCoordSpace()?
bool RenderWidgetHostViewBase::TransformPointToTargetCoordSpace(
    RenderWidgetHostViewBase* original_view,
    RenderWidgetHostViewBase* target_view,
    const gfx::PointF& point,
    gfx::PointF* transformed_point) const {
  DCHECK(original_view);
  DCHECK(target_view);
  viz::FrameSinkId root_frame_sink_id = original_view->GetRootFrameSinkId();
  if (!root_frame_sink_id.is_valid())
    return false;
  const auto& display_hit_test_query_map =
      GetHostFrameSinkManager()->display_hit_test_query();
  const auto iter = display_hit_test_query_map.find(root_frame_sink_id);
  if (iter == display_hit_test_query_map.end())
    return false;
  viz::HitTestQuery* query = iter->second.get();

  std::vector<viz::FrameSinkId> target_ancestors;
  target_ancestors.push_back(target_view->GetFrameSinkId());

  RenderWidgetHostViewBase* cur_view = target_view;
  while (cur_view->IsRenderWidgetHostViewChildFrame()) {
    cur_view =
        static_cast<RenderWidgetHostViewChildFrame*>(cur_view)->GetParentView();
    if (!cur_view)
      return false;
    target_ancestors.push_back(cur_view->GetFrameSinkId());
  }
  target_ancestors.push_back(root_frame_sink_id);

  float device_scale_factor = original_view->GetDeviceScaleFactor();
  DCHECK_GT(device_scale_factor, 0.0f);
  gfx::Point3F point_in_pixels =
      gfx::Point3F(gfx::ConvertPointToPixels(point, device_scale_factor));
  // TODO(crbug.com/966995): Optimize so that |point_in_pixels| doesn't need to
  // be in the coordinate space of the root surface in HitTestQuery.
  gfx::Transform transform_root_to_original;
  query->GetTransformToTarget(original_view->GetFrameSinkId(),
                              &transform_root_to_original);
  if (!transform_root_to_original.TransformPointReverse(&point_in_pixels))
    return false;
  gfx::PointF transformed_point_in_physical_pixels;
  if (!query->TransformLocationForTarget(
          target_ancestors, point_in_pixels.AsPointF(),
          &transformed_point_in_physical_pixels)) {
    return false;
  }
  *transformed_point = gfx::ConvertPointToDips(
      transformed_point_in_physical_pixels, device_scale_factor);
  return true;
}

bool RenderWidgetHostViewBase::GetTransformToViewCoordSpace(
    RenderWidgetHostViewBase* target_view,
    gfx::Transform* transform) {
  DCHECK(transform);
  if (target_view == this) {
    transform->MakeIdentity();
    return true;
  }

  viz::FrameSinkId root_frame_sink_id = GetRootFrameSinkId();
  if (!root_frame_sink_id.is_valid())
    return false;

  const auto& display_hit_test_query_map =
      GetHostFrameSinkManager()->display_hit_test_query();
  const auto iter = display_hit_test_query_map.find(root_frame_sink_id);
  if (iter == display_hit_test_query_map.end())
    return false;
  viz::HitTestQuery* query = iter->second.get();

  gfx::Transform transform_this_to_root;
  if (GetFrameSinkId() != root_frame_sink_id) {
    gfx::Transform transform_root_to_this;
    if (!query->GetTransformToTarget(GetFrameSinkId(), &transform_root_to_this))
      return false;
    if (!transform_root_to_this.GetInverse(&transform_this_to_root))
      return false;
  }
  gfx::Transform transform_root_to_target;
  if (!query->GetTransformToTarget(target_view->GetFrameSinkId(),
                                   &transform_root_to_target)) {
    return false;
  }

  // TODO(wjmaclean): In TransformPointToTargetCoordSpace the device scale
  // factor is taken from the original view ... does that matter? Presumably
  // all the views have the same dsf.
  float device_scale_factor = GetDeviceScaleFactor();
  gfx::Transform transform_to_pixel;
  transform_to_pixel.Scale(device_scale_factor, device_scale_factor);
  gfx::Transform transform_from_pixel;
  transform_from_pixel.Scale(1.f / device_scale_factor,
                             1.f / device_scale_factor);

  // Note: gfx::Transform includes optimizations to early-out for scale = 1 or
  // concatenating an identity matrix, so we don't add those checks here.
  transform->MakeIdentity();

  transform->ConcatTransform(transform_to_pixel);
  transform->ConcatTransform(transform_this_to_root);
  transform->ConcatTransform(transform_root_to_target);
  transform->ConcatTransform(transform_from_pixel);

  return true;
}

bool RenderWidgetHostViewBase::TransformPointToLocalCoordSpace(
    const gfx::PointF& point,
    const viz::SurfaceId& original_surface,
    gfx::PointF* transformed_point) {
  viz::FrameSinkId original_frame_sink_id = original_surface.frame_sink_id();
  viz::FrameSinkId target_frame_sink_id = GetFrameSinkId();
  if (!original_frame_sink_id.is_valid() || !target_frame_sink_id.is_valid())
    return false;
  if (original_frame_sink_id == target_frame_sink_id)
    return true;
  if (!host() || !host()->delegate())
    return false;
  auto* router = host()->delegate()->GetInputEventRouter();
  if (!router)
    return false;
  *transformed_point = point;
  return TransformPointToTargetCoordSpace(
      router->FindViewFromFrameSinkId(original_frame_sink_id),
      router->FindViewFromFrameSinkId(target_frame_sink_id), point,
      transformed_point);
}

}  // namespace content
