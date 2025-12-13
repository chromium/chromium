// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_ios.h"

#import <UIKit/UIKit.h>

#include <cstdint>

#include "base/command_line.h"
#include "build/ios_buildflags.h"
#include "cc/mojom/render_frame_metadata.mojom-shared.h"
#include "components/input/events_helper.h"
#include "components/input/render_widget_host_input_event_router.h"
#include "components/input/switches.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/surfaces/frame_sink_id_allocator.h"
#include "content/browser/renderer_host/browser_compositor_ios.h"
#include "content/browser/renderer_host/input/motion_event_web.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target_ios.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/text_input_manager.h"
#include "content/common/content_switches_internal.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "ui/accelerated_widget_mac/display_ca_layer_tree.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/display/screen.h"
#include "ui/events/gesture_detection/gesture_provider_config_helper.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/native_ui_types.h"

#if BUILDFLAG(IS_IOS_TVOS)
#include "content/browser/renderer_host/render_widget_host_view_tvos_uiview.h"
#else
#include "content/browser/renderer_host/render_widget_host_view_ios_uiview.h"
#endif

@interface UIApplication (Testing)
- (BOOL)isRunningTests;
@end

@implementation UIApplication (Testing)
- (BOOL)isRunningTests {
  return NO;
}
@end

namespace {

// Used for setting the requested renderer size when testing.
constexpr gfx::Size kDefaultSizeForTesting = gfx::Size(800, 600);
constexpr gfx::Size KDefaultSizeForPreventResizingForTesting =
    gfx::Size(980, 735);

bool IsTesting() {
#if BUILDFLAG(IS_IOS_APP_EXTENSION)
  // This class shouldn't really be build with extension anyways.
  // Fix the build to avoid building browser code in extensions.
  return false;
#else
  return [[UIApplication sharedApplication] isRunningTests];
#endif
}

gfx::Rect GetDefaultSizeForTesting() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kPreventResizingContentsForTesting)
             ? gfx::Rect(KDefaultSizeForPreventResizingForTesting)
             : gfx::Rect(kDefaultSizeForTesting);
}

}  // namespace

namespace content {

// This class holds strongly so we don't leak that in the header of the
// RenderWidgetHostViewIOS.
class UIViewHolder {
 public:
  RenderWidgetUIView* __strong view_;
};

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewIOS, public:

RenderWidgetHostViewIOS::RenderWidgetHostViewIOS(RenderWidgetHost* widget)
    : RenderWidgetHostViewBase(widget),
      gesture_provider_(
          ui::GetGestureProviderConfig(
              ui::GestureProviderConfigType::CURRENT_PLATFORM,
              GetUIThreadTaskRunner({BrowserTaskType::kUserInput})),
          this) {
  ui_view_ = std::make_unique<UIViewHolder>();
  ui_view_->view_ =
      [[RenderWidgetUIView alloc] initWithWidget:weak_factory_.GetWeakPtr()];

  auto* screen = display::Screen::Get();
  screen_infos_ =
      screen->GetScreenInfosNearestDisplay(screen->GetPrimaryDisplay().id());

  browser_compositor_ = std::make_unique<BrowserCompositorIOS>(
      [ui_view_->view_ viewHandle], this, host()->IsHidden(),
      host()->GetFrameSinkId());

  if (IsTesting()) {
    view_bounds_ = GetDefaultSizeForTesting();
    browser_compositor_->UpdateSurfaceFromUIView(GetViewBounds().size());
  }

  CHECK(host()->GetFrameSinkId().is_valid());

  // Let the page-level input event router know about our surface ID
  // namespace for surface-based hit testing.
  if (ShouldRouteEvents()) {
    host()->delegate()->GetInputEventRouter()->AddFrameSinkIdOwner(
        GetFrameSinkId(), this);
  }

  if (GetTextInputManager()) {
    text_input_manager_->AddObserver(this);
  }

  host()->render_frame_metadata_provider()->AddObserver(this);
  host()
      ->render_frame_metadata_provider()
      ->UpdateRootScrollOffsetUpdateFrequency(
          cc::mojom::RootScrollOffsetUpdateFrequency::kAllUpdates);
  host()->SetView(this);
}

RenderWidgetHostViewIOS::~RenderWidgetHostViewIOS() = default;

void RenderWidgetHostViewIOS::Destroy() {
  [ui_view_->view_ removeView];
  host()->render_frame_metadata_provider()->RemoveObserver(this);
  if (text_input_manager_) {
    text_input_manager_->RemoveObserver(this);
  }
  browser_compositor_.reset();
  // Call this before the derived class is destroyed so that virtual function
  // calls back into `this` still work.
  NotifyObserversAboutShutdown();
  RenderWidgetHostViewBase::Destroy();
  delete this;
}

bool RenderWidgetHostViewIOS::IsSurfaceAvailableForCopy() {
  return browser_compositor_->GetDelegatedFrameHost()
      ->CanCopyFromCompositingSurface();
}

void RenderWidgetHostViewIOS::CopyFromSurface(
    const gfx::Rect& src_rect,
    const gfx::Size& dst_size,
    base::OnceCallback<void(const viz::CopyOutputBitmapWithMetadata&)>
        callback) {
  base::WeakPtr<RenderWidgetHostImpl> popup_host;
  base::WeakPtr<DelegatedFrameHost> popup_frame_host;
  RenderWidgetHostViewBase::CopyMainAndPopupFromSurface(
      host()->GetWeakPtr(),
      browser_compositor_->GetDelegatedFrameHost()->GetWeakPtr(), popup_host,
      popup_frame_host, src_rect, dst_size, GetDeviceScaleFactor(),
      std::move(callback));
}

ui::FilteredGestureProvider*
RenderWidgetHostViewIOS::GetFilteredGestureProviderForTesting() {
  return &gesture_provider_;
}

void RenderWidgetHostViewIOS::InitAsChild(gfx::NativeView parent_view) {}
void RenderWidgetHostViewIOS::SetSize(const gfx::Size& size) {}
void RenderWidgetHostViewIOS::SetBounds(const gfx::Rect& rect) {}

gfx::NativeView RenderWidgetHostViewIOS::GetNativeView() {
  return gfx::NativeView(ui_view_->view_);
}

gfx::NativeViewAccessible RenderWidgetHostViewIOS::GetNativeViewAccessible() {
  return gfx::NativeViewAccessible(ui_view_->view_);
}

gfx::NativeViewAccessible
RenderWidgetHostViewIOS::AccessibilityGetNativeViewAccessible() {
  return gfx::NativeViewAccessible(ui_view_->view_);
}

void RenderWidgetHostViewIOS::Focus() {
  // Ignore redundant calls, as they can cause unending loops of focus-setting.
  // crbug.com/998123, crbug.com/804184.
  if (is_first_responder_ || is_getting_focus_) {
    return;
  }

  base::AutoReset<bool> is_getting_focus_bit(&is_getting_focus_, true);
  [ui_view_->view_ becomeFirstResponder];
}

bool RenderWidgetHostViewIOS::HasFocus() {
  return is_first_responder_;
}

gfx::Rect RenderWidgetHostViewIOS::GetViewBounds() {
  return view_bounds_;
}
blink::mojom::PointerLockResult RenderWidgetHostViewIOS::LockPointer(bool) {
  return {};
}
blink::mojom::PointerLockResult RenderWidgetHostViewIOS::ChangePointerLock(
    bool) {
  return {};
}
void RenderWidgetHostViewIOS::UnlockPointer() {}

uint32_t RenderWidgetHostViewIOS::GetCaptureSequenceNumber() const {
  return latest_capture_sequence_number_;
}

void RenderWidgetHostViewIOS::EnsureSurfaceSynchronizedForWebTest() {
  ++latest_capture_sequence_number_;
  browser_compositor_->ForceNewSurfaceId();
}

void RenderWidgetHostViewIOS::TakeFallbackContentFrom(
    RenderWidgetHostView* view) {}

std::unique_ptr<SyntheticGestureTarget>
RenderWidgetHostViewIOS::CreateSyntheticGestureTarget() {
  RenderWidgetHostImpl* host =
      RenderWidgetHostImpl::From(GetRenderWidgetHost());
  return std::make_unique<SyntheticGestureTargetIOS>(host);
}

const viz::LocalSurfaceId& RenderWidgetHostViewIOS::GetLocalSurfaceId() const {
  return browser_compositor_->GetRendererLocalSurfaceId();
}

void RenderWidgetHostViewIOS::UpdateFrameSinkIdRegistration() {
  RenderWidgetHostViewBase::UpdateFrameSinkIdRegistration();
  browser_compositor_->GetDelegatedFrameHost()->SetIsFrameSinkIdOwner(
      is_frame_sink_id_owner());
}

const viz::FrameSinkId& RenderWidgetHostViewIOS::GetFrameSinkId() const {
  return browser_compositor_->GetDelegatedFrameHost()->frame_sink_id();
}

viz::FrameSinkId RenderWidgetHostViewIOS::GetRootFrameSinkId() {
  return browser_compositor_->GetRootFrameSinkId();
}

viz::SurfaceId RenderWidgetHostViewIOS::GetCurrentSurfaceId() const {
  // |browser_compositor_| could be null if this method is called during its
  // destruction.
  if (!browser_compositor_) {
    return viz::SurfaceId();
  }
  return browser_compositor_->GetDelegatedFrameHost()->GetCurrentSurfaceId();
}

void RenderWidgetHostViewIOS::InitAsPopup(
    RenderWidgetHostView* parent_host_view,
    const gfx::Rect& pos,
    const gfx::Rect& anchor_rect) {}
void RenderWidgetHostViewIOS::UpdateCursor(const ui::Cursor& cursor) {}
void RenderWidgetHostViewIOS::SetIsLoading(bool is_loading) {}

void RenderWidgetHostViewIOS::RenderProcessGone() {
  Destroy();
}

void RenderWidgetHostViewIOS::ShowWithVisibility(
    PageVisibilityState page_visibility) {
  if (IsTesting() && !is_visible_) {
    UpdateScreenInfo();
  }
  is_visible_ = true;
  browser_compositor_->SetViewVisible(is_visible_);
  OnShowWithPageVisibility(page_visibility);
}

void RenderWidgetHostViewIOS::NotifyHostAndDelegateOnWasShown(
    blink::mojom::RecordContentToVisibleTimeRequestPtr visible_time_request) {
  // SetRenderWidgetHostIsHidden may cause a state transition that switches to
  // a new instance of DelegatedFrameHost and calls WasShown, which causes
  // HasSavedFrame to always return true. So cache the HasSavedFrame result
  // before the transition, and do not save this DelegatedFrameHost* locally.
  bool has_saved_frame =
      browser_compositor_->GetDelegatedFrameHost()->HasSavedFrame();

  browser_compositor_->SetRenderWidgetHostIsHidden(false);

  const bool renderer_should_record_presentation_time = !has_saved_frame;
  host()->WasShown(renderer_should_record_presentation_time
                       ? visible_time_request.Clone()
                       : blink::mojom::RecordContentToVisibleTimeRequestPtr());

  // If the frame for the renderer is already available, then the
  // tab-switching time is the presentation time for the browser-compositor.
  // SetRenderWidgetHostIsHidden above will show the DelegatedFrameHost
  // in this state, but doesn't include the presentation time request.
  if (has_saved_frame && visible_time_request) {
    browser_compositor_->GetDelegatedFrameHost()
        ->RequestSuccessfulPresentationTimeForNextFrame(
            std::move(visible_time_request));
  }
}

void RenderWidgetHostViewIOS::Hide() {
  is_visible_ = false;
  browser_compositor_->SetViewVisible(is_visible_);
  browser_compositor_->SetRenderWidgetHostIsHidden(true);
  if (!host() || host()->IsHidden()) {
    return;
  }

  // Inform the renderer that we are being hidden so it can reduce its resource
  // utilization.
  host()->WasHidden();
}

bool RenderWidgetHostViewIOS::IsShowing() {
  // In testing, `view_` is not attached to the window.
  if (IsTesting()) {
    return is_visible_;
  }
  return is_visible_ && [ui_view_->view_ window];
}

gfx::Rect RenderWidgetHostViewIOS::GetBoundsInRootWindow() {
  return GetViewBounds();
}

gfx::Size RenderWidgetHostViewIOS::GetRequestedRendererSize() {
  return GetViewBounds().size();
}

std::optional<DisplayFeature> RenderWidgetHostViewIOS::GetDisplayFeature() {
  return display_feature_;
}

void RenderWidgetHostViewIOS::DisableDisplayFeatureOverrideForEmulation() {
  if (!display_feature_overridden_for_emulation_) {
    return;
  }

  display_feature_overridden_for_emulation_ = false;
  display_feature_ = std::nullopt;
  ComputeDisplayFeature();
  host()->SynchronizeVisualProperties();
}

void RenderWidgetHostViewIOS::OverrideDisplayFeatureForEmulation(
    const DisplayFeature* display_feature) {
  if (display_feature) {
    display_feature_ = *display_feature;
  } else {
    display_feature_ = std::nullopt;
  }
  display_feature_overridden_for_emulation_ = true;
  host()->SynchronizeVisualProperties();
}

void RenderWidgetHostViewIOS::UpdateBackgroundColor() {}

void RenderWidgetHostViewIOS::
    RequestSuccessfulPresentationTimeFromHostOrDelegate(
        blink::mojom::RecordContentToVisibleTimeRequestPtr
            visible_time_request) {
  // No state transition here so don't use
  // has_saved_frame_before_state_transition.
  if (browser_compositor_->GetDelegatedFrameHost()->HasSavedFrame()) {
    // If the frame for the renderer is already available, then the
    // tab-switching time is the presentation time for the browser-compositor.
    browser_compositor_->GetDelegatedFrameHost()
        ->RequestSuccessfulPresentationTimeForNextFrame(
            std::move(visible_time_request));
  } else {
    host()->RequestSuccessfulPresentationTimeForNextFrame(
        std::move(visible_time_request));
  }
}
void RenderWidgetHostViewIOS::
    CancelSuccessfulPresentationTimeRequestForHostAndDelegate() {
  host()->CancelSuccessfulPresentationTimeRequest();
  browser_compositor_->GetDelegatedFrameHost()
      ->CancelSuccessfulPresentationTimeRequest();
}

SkColor RenderWidgetHostViewIOS::BrowserCompositorIOSGetGutterColor() {
  // When making an element on the page fullscreen the element's background
  // may not match the page's, so use black as the gutter color to avoid
  // flashes of brighter colors during the transition.
  if (host()->delegate() && host()->delegate()->IsFullscreen()) {
    return SK_ColorBLACK;
  }
  if (GetBackgroundColor()) {
    return *GetBackgroundColor();
  }
  return SK_ColorWHITE;
}

bool RenderWidgetHostViewIOS::OnBrowserCompositorSurfaceIdChanged() {
  return host()->SynchronizeVisualProperties();
}

void RenderWidgetHostViewIOS::OnFrameTokenChanged(
    uint32_t frame_token,
    base::TimeTicks activation_time) {
  OnFrameTokenChangedForView(frame_token, activation_time);
}

std::vector<viz::SurfaceId>
RenderWidgetHostViewIOS::CollectSurfaceIdsForEviction() {
  return {};
}

void RenderWidgetHostViewIOS::UpdateScreenInfo() {
  if (host()->delegate()) {
    host()->delegate()->SendScreenRects();
  }

  auto* display_screen = display::Screen::Get();
  display::ScreenInfos new_screen_infos =
      display_screen->GetScreenInfosNearestDisplay(
          display_screen->GetPrimaryDisplay().id());

  gfx::Rect view_bounds_dips([ui_view_->view_ bounds]);
  const bool screen_info_changed = screen_infos_ != new_screen_infos;
  const bool size_changed =
      view_bounds_dips.size() != browser_compositor_->GetRendererSize();
  screen_infos_ = std::move(new_screen_infos);

  if (!IsTesting() && (size_changed || screen_info_changed)) {
    browser_compositor_->UpdateSurfaceFromUIView(view_bounds_dips.size());
  }
  ComputeDisplayFeature();

  // Notify the associated RenderWidgetHostImpl when screen info has changed.
  // That will synchronize visual properties needed for frame tree rendering
  // and for web platform APIs that expose screen and window info and events.
  if (size_changed || screen_info_changed) {
    host()->NotifyScreenInfoChanged();
  }
}

void RenderWidgetHostViewIOS::OnSynchronizedDisplayPropertiesChanged(
    bool rotation) {
  host()->SynchronizeVisualProperties();
}

void RenderWidgetHostViewIOS::UpdateCALayerTree(
    const gfx::CALayerParams& ca_layer_params) {}

void RenderWidgetHostViewIOS::OnOldViewDidNavigatePreCommit() {
  CHECK(browser_compositor_) << "Shouldn't be called during destruction!";
  browser_compositor_->DidNavigateMainFramePreCommit();
  gesture_provider_.ResetDetection();
}

void RenderWidgetHostViewIOS::OnNewViewDidNavigatePostCommit() {
  gesture_provider_.ResetDetection();
}

void RenderWidgetHostViewIOS::DidEnterBackForwardCache() {
  CHECK(browser_compositor_) << "Shouldn't be called during destruction!";
  browser_compositor_->DidEnterBackForwardCache();
  // If we have the fallback content timer running, force it to stop. Else, when
  // the page is restored the timer could also fire, setting whatever
  // `DelegatedFrameHost::first_local_surface_id_after_navigation_` as the
  // fallback to our Surfacelayer.
  //
  // This is safe for BFCache restore because we will supply specific fallback
  // surfaces for BFCache.
  //
  // We do not want to call this in `RWHImpl::WasHidden()` because in the case
  // of `Visibility::OCCLUDED` we still want to keep the timer running.
  //
  // Called after to prevent prematurely evict the BFCached surface.
  host()->ForceFirstFrameAfterNavigationTimeout();
}

void RenderWidgetHostViewIOS::ActivatedOrEvictedFromBackForwardCache() {
  browser_compositor_->ActivatedOrEvictedFromBackForwardCache();
}

void RenderWidgetHostViewIOS::DidNavigate() {
  browser_compositor_->DidNavigate();
}

viz::ScopedSurfaceIdAllocator
RenderWidgetHostViewIOS::DidUpdateVisualProperties(
    const cc::RenderFrameMetadata& metadata) {
  base::OnceCallback<void()> allocation_task = base::BindOnce(
      base::IgnoreResult(
          &RenderWidgetHostViewIOS::OnDidUpdateVisualPropertiesComplete),
      weak_factory_.GetWeakPtr(), metadata);
  return browser_compositor_->GetScopedRendererSurfaceIdAllocator(
      std::move(allocation_task));
}

void RenderWidgetHostViewIOS::OnDidUpdateVisualPropertiesComplete(
    const cc::RenderFrameMetadata& metadata) {
  browser_compositor_->UpdateSurfaceFromChild(
      host()->auto_resize_enabled(), metadata.device_scale_factor,
      metadata.viewport_size_in_pixels,
      metadata.local_surface_id.value_or(viz::LocalSurfaceId()));
}

void RenderWidgetHostViewIOS::InvalidateLocalSurfaceIdAndAllocationGroup() {
  browser_compositor_->InvalidateSurfaceAllocationGroup();
}

void RenderWidgetHostViewIOS::ClearFallbackSurfaceForCommitPending() {
  browser_compositor_->GetDelegatedFrameHost()
      ->ClearFallbackSurfaceForCommitPending();
  browser_compositor_->InvalidateLocalSurfaceIdOnEviction();
}

void RenderWidgetHostViewIOS::ResetFallbackToFirstNavigationSurface() {
  browser_compositor_->GetDelegatedFrameHost()
      ->ResetFallbackToFirstNavigationSurface();
}

bool RenderWidgetHostViewIOS::RequestRepaintOnNewSurface() {
  return browser_compositor_->ForceNewSurfaceId();
}

void RenderWidgetHostViewIOS::TransformPointToRootSurface(gfx::PointF* point) {
  browser_compositor_->TransformPointToRootSurface(point);
}

bool RenderWidgetHostViewIOS::HasFallbackSurface() const {
  return browser_compositor_->GetDelegatedFrameHost()->HasFallbackSurface();
}

bool RenderWidgetHostViewIOS::TransformPointToCoordSpaceForView(
    const gfx::PointF& point,
    RenderWidgetHostViewInput* target_view,
    gfx::PointF* transformed_point) {
  if (target_view == this) {
    *transformed_point = point;
    return true;
  }

  return target_view->TransformPointToLocalCoordSpace(point, GetFrameSinkId(),
                                                      transformed_point);
}

display::ScreenInfo RenderWidgetHostViewIOS::GetCurrentScreenInfo() const {
  return screen_infos_.current();
}

void RenderWidgetHostViewIOS::SetCurrentDeviceScaleFactor(
    float device_scale_factor) {
  // TODO(crbug.com/40229152): does this need to be upscaled by
  // scale_override_for_capture_ for HiDPI capture mode?
  screen_infos_.mutable_current().device_scale_factor = device_scale_factor;
}

void RenderWidgetHostViewIOS::SetActive(bool active) {
  if (host()) {
    UpdateActiveState(active);
    if (active) {
      if (HasFocus()) {
        host()->Focus();
      }
    } else {
      host()->Blur();
    }
  }
  // if (HasFocus())
  //  SetTextInputActive(active);
  if (!active) {
    UnlockPointer();
  }
}

bool RenderWidgetHostViewIOS::ShouldRouteEvents() const {
  DCHECK(host());
  return host()->delegate() && host()->delegate()->GetInputEventRouter();
}

void RenderWidgetHostViewIOS::OnTouchEvent(blink::WebTouchEvent web_event) {
  ui::FilteredGestureProvider::TouchHandlingResult result =
      gesture_provider_.OnTouchEvent(MotionEventWeb(web_event));
  if (!result.succeeded) {
    return;
  }

  web_event.moved_beyond_slop_region = result.moved_beyond_slop_region;
  ui::LatencyInfo latency_info;
  latency_info.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_UI_COMPONENT);
  if (ShouldRouteEvents()) {
    host()->delegate()->GetInputEventRouter()->RouteTouchEvent(this, &web_event,
                                                               latency_info);
  } else {
    host()->GetRenderInputRouter()->ForwardTouchEventWithLatencyInfo(
        web_event, latency_info);
  }
}

void RenderWidgetHostViewIOS::ProcessAckedTouchEvent(
    const input::TouchEventWithLatencyInfo& touch,
    blink::mojom::InputEventResultState ack_result) {
  const bool event_consumed =
      ack_result == blink::mojom::InputEventResultState::kConsumed;
  gesture_provider_.OnTouchEventAck(
      touch.event.unique_touch_event_id, event_consumed,
      input::InputEventResultStateIsSetBlocking(ack_result));
  if (touch.event.touch_start_or_first_touch_move && event_consumed &&
      ShouldRouteEvents()) {
    host()
        ->delegate()
        ->GetInputEventRouter()
        ->OnHandledTouchStartOrFirstTouchMove(
            touch.event.unique_touch_event_id);
  }
}

void RenderWidgetHostViewIOS::OnGestureEvent(
    const ui::GestureEventData& gesture) {
  if ((gesture.type() == ui::EventType::kGesturePinchBegin ||
       gesture.type() == ui::EventType::kGesturePinchUpdate ||
       gesture.type() == ui::EventType::kGesturePinchEnd) &&
      !input::switches::IsPinchToZoomEnabled()) {
    return;
  }

  blink::WebGestureEvent web_gesture =
      ui::CreateWebGestureEventFromGestureEventData(gesture);
  SendGestureEvent(web_gesture);
}

bool RenderWidgetHostViewIOS::RequiresDoubleTapGestureEvents() const {
  return true;
}

void RenderWidgetHostViewIOS::SendGestureEvent(
    const blink::WebGestureEvent& event) {
  InjectGestureEvent(event, ui::LatencyInfo());
}

void RenderWidgetHostViewIOS::InjectTouchEvent(
    const blink::WebTouchEvent& event,
    const ui::LatencyInfo& latency_info) {
  ui::FilteredGestureProvider::TouchHandlingResult result =
      gesture_provider_.OnTouchEvent(MotionEventWeb(event));
  if (!result.succeeded) {
    return;
  }

  if (ShouldRouteEvents()) {
    blink::WebTouchEvent touch_event(event);
    host()->delegate()->GetInputEventRouter()->RouteTouchEvent(
        this, &touch_event, latency_info);
  } else {
    host()->GetRenderInputRouter()->ForwardTouchEventWithLatencyInfo(
        event, latency_info);
  }
}

void RenderWidgetHostViewIOS::InjectGestureEvent(
    const blink::WebGestureEvent& event,
    const ui::LatencyInfo& latency_info) {
  if (ShouldRouteEvents()) {
    blink::WebGestureEvent gesture_event(event);
    host()->delegate()->GetInputEventRouter()->RouteGestureEvent(
        this, &gesture_event, latency_info);
  } else {
    host()->GetRenderInputRouter()->ForwardGestureEventWithLatencyInfo(
        event, latency_info);
  }
}

void RenderWidgetHostViewIOS::InjectMouseEvent(
    const blink::WebMouseEvent& web_mouse,
    const ui::LatencyInfo& latency_info) {
  if (ShouldRouteEvents()) {
    blink::WebMouseEvent mouse_event(web_mouse);
    host()->delegate()->GetInputEventRouter()->RouteMouseEvent(
        this, &mouse_event, latency_info);
  } else {
    host()->ForwardMouseEventWithLatencyInfo(web_mouse, latency_info);
  }
}

void RenderWidgetHostViewIOS::InjectMouseWheelEvent(
    const blink::WebMouseWheelEvent& web_wheel,
    const ui::LatencyInfo& latency_info) {
  if (ShouldRouteEvents()) {
    blink::WebMouseWheelEvent mouse_wheel_event(web_wheel);
    host()->delegate()->GetInputEventRouter()->RouteMouseWheelEvent(
        this, &mouse_wheel_event, latency_info);
  } else {
    host()->ForwardWheelEventWithLatencyInfo(web_wheel, latency_info);
  }
}

bool RenderWidgetHostViewIOS::CanBecomeFirstResponderForTesting() const {
  return IsTesting() && !is_first_responder_ && is_getting_focus_;
}

bool RenderWidgetHostViewIOS::CanResignFirstResponderForTesting() const {
  return IsTesting() && is_first_responder_;
}

void RenderWidgetHostViewIOS::UpdateNativeViewTree(gfx::NativeView view) {
  if (view) {
    [ui_view_->view_ updateView:(UIScrollView*)view.Get()];
    UpdateFrameBounds();
  } else {
    [ui_view_->view_ removeView];
  }
}

void RenderWidgetHostViewIOS::ImeSetComposition(
    const std::u16string& text,
    const std::vector<ui::ImeTextSpan>& spans,
    const gfx::Range& replacement_range,
    int selection_start,
    int selection_end) {
  if (auto* widget_host = GetActiveWidget()) {
    widget_host->ImeSetComposition(text, spans, replacement_range,
                                   selection_start, selection_end);
  }
}

void RenderWidgetHostViewIOS::ImeCommitText(const std::u16string& text,
                                            const gfx::Range& replacement_range,
                                            int relative_position) {
  if (auto* widget_host = GetActiveWidget()) {
    widget_host->ImeCommitText(text, std::vector<ui::ImeTextSpan>(),
                               replacement_range, relative_position);
  }
}

void RenderWidgetHostViewIOS::ImeFinishComposingText(bool keep_selection) {
  if (auto* widget_host = GetActiveWidget()) {
    widget_host->ImeFinishComposingText(keep_selection);
  }
}

RenderWidgetHostImpl* RenderWidgetHostViewIOS::GetActiveWidget() {
  return text_input_manager_ ? text_input_manager_->GetActiveWidget() : nullptr;
}

void RenderWidgetHostViewIOS::OnFirstResponderChanged() {
  bool is_first_responder = [ui_view_->view_ isFirstResponder] ||
                            (IsTesting() && is_getting_focus_);

  if (is_first_responder_ == is_first_responder) {
    return;
  }
  is_first_responder_ = is_first_responder;

  if (is_first_responder_) {
    host()->GotFocus();
  } else {
    host()->LostFocus();
  }
}

void RenderWidgetHostViewIOS::OnUpdateTextInputStateCalled(
    TextInputManager* text_input_manager,
    RenderWidgetHostViewBase* updated_view,
    bool did_update_state) {
  if (text_input_manager->GetActiveWidget()) {
    [ui_view_->view_
        onUpdateTextInputState:*text_input_manager->GetTextInputState()
                    withBounds:[ui_view_->view_ bounds]];
  } else {
    // If there are no active widgets, the TextInputState.type should be
    // reported as none.
    [ui_view_->view_ onUpdateTextInputState:ui::mojom::TextInputState()
                                 withBounds:[ui_view_->view_ bounds]];
  }
}

void RenderWidgetHostViewIOS::OnTextSelectionChanged(
    TextInputManager* text_input_manager,
    RenderWidgetHostViewBase* updated_view) {
#if !BUILDFLAG(IS_IOS_TVOS)
  DCHECK_EQ(GetTextInputManager(), text_input_manager);
  const TextInputManager::TextSelection* selection =
      text_input_manager->GetTextSelection(updated_view);
  if (selection && selection->selected_text().length()) {
    [[ui_view_->view_ textInteraction] refreshKeyboardUI];
    [[ui_view_->view_ textInteraction] textSelectionDisplayInteraction]
        .activated = YES;

    // This seems like a bug. BETextInput always sets the
    // textSelectionDisplayInteraction lolipop dot size to 16.5,16.5, expecting
    // the entire web content to be transformed down for some reason. Instead,
    // scale it down here with a very naive implementation.
    UITextSelectionDisplayInteraction* textSelectionDisplayInteraction =
        [ui_view_->view_ textInteraction].textSelectionDisplayInteraction;
    NSArray<UIView<UITextSelectionHandleView>*>* handleViews =
        textSelectionDisplayInteraction.handleViews;

    CGFloat shrink = handleViews[0].subviews[0].frame.size.height / 20;
    shrink = std::max(std::min(shrink, 1.0), 0.65);
    handleViews[0].subviews[1].layer.transform =
        CATransform3DMakeScale(shrink, shrink, 1);

    shrink = handleViews[1].subviews[0].frame.size.height / 20;
    shrink = std::max(std::min(shrink, 1.0), 0.65);
    handleViews[1].subviews[1].layer.transform =
        CATransform3DMakeScale(shrink, shrink, 1);
  } else {
    [[ui_view_->view_ textInteraction] textSelectionDisplayInteraction]
        .activated = NO;
  }
#endif
}

void RenderWidgetHostViewIOS::OnSelectionBoundsChanged(
    TextInputManager* text_input_manager,
    RenderWidgetHostViewBase* updated_view) {
#if !BUILDFLAG(IS_IOS_TVOS)
  [[ui_view_->view_ textInteraction]
          .textSelectionDisplayInteraction setNeedsSelectionUpdate];
#endif
}

ui::Compositor* RenderWidgetHostViewIOS::GetCompositor() {
  return browser_compositor_->GetCompositor();
}

void RenderWidgetHostViewIOS::GestureEventAck(
    const blink::WebGestureEvent& event,
    blink::mojom::InputEventResultSource ack_source,
    blink::mojom::InputEventResultState ack_result) {
  // Stop flinging if a GSU event with momentum phase is sent to the renderer
  // but not consumed.
  StopFlingingIfNecessary(event, ack_result);

  UIScrollView* scrollView = (UIScrollView*)[ui_view_->view_ superview];
  switch (event.GetType()) {
    case blink::WebInputEvent::Type::kGestureScrollBegin:
      is_scrolling_ = true;
      if (host()->delegate()) {
        host()->delegate()->SetTopControlsGestureScrollInProgress(true);
      }
      [[scrollView delegate] scrollViewWillBeginDragging:scrollView];
      break;
    case blink::WebInputEvent::Type::kGestureScrollUpdate:
      // TODO(crbug.com/40274032): Since ScrollResultData has been removed from
      // GestureEventAck, the invocation of ApplyRootScrollOffsetChanged here
      // has also been eliminated for now. We should address the
      // GestureScrollUpdate event after examining how the bug implements
      // GestureEventAck.
      break;
    case blink::WebInputEvent::Type::kGestureScrollEnd: {
      // Make sure our cached view bounds gets updated.
      if (!IsTesting()) {
        view_bounds_ = gfx::Rect([ui_view_->view_ bounds]);
      }
      if (host()->delegate()) {
        host()->delegate()->SetTopControlsGestureScrollInProgress(false);
      }
      is_scrolling_ = false;
      CGPoint targetOffset = [scrollView contentOffset];
      [[scrollView delegate] scrollViewWillEndDragging:scrollView
                                          withVelocity:CGPoint()
                                   targetContentOffset:&targetOffset];
      [[scrollView delegate] scrollViewDidEndDragging:scrollView
                                       willDecelerate:NO];
      host()->SynchronizeVisualProperties();
      break;
    }
    default:
      break;
  }
}

void RenderWidgetHostViewIOS::ChildDidAckGestureEvent(
    const blink::WebGestureEvent& event,
    blink::mojom::InputEventResultState ack_result) {
  // TODO(crbug.com/40274032): Since ScrollResultData has been removed from
  // GestureEventAck, the invocation of ApplyRootScrollOffsetChanged here has
  // also been eliminated for now. We should address the GestureScrollUpdate
  // event after examining how the bug implements GestureEventAck.
}

void RenderWidgetHostViewIOS::OnUnconfirmedTapConvertedToTap() {
  gesture_provider_.OnUnconfirmedTapConvertedToTap();
}

void RenderWidgetHostViewIOS::UpdateFrameBounds() {
  const gfx::PointF scrollOffset =
      last_root_scroll_offset_.value_or(gfx::PointF());
  const CGRect parentBounds = [[ui_view_->view_ superview] bounds];

  CGRect frameBounds;
  frameBounds.origin = scrollOffset.ToCGPoint();
  frameBounds.size = parentBounds.size;

  // If we are scrolling we don't resize the WebView immediately.
  if (!is_scrolling_ && !IsTesting()) {
    view_bounds_ = gfx::Rect(frameBounds);
  }
  [ui_view_->view_ setFrame:frameBounds];
}

void RenderWidgetHostViewIOS::ApplyRootScrollOffsetChanged(
    const gfx::PointF& root_scroll_offset,
    bool force) {
  if (last_root_scroll_offset_ != root_scroll_offset || force) {
    last_root_scroll_offset_ = root_scroll_offset;
    UpdateFrameBounds();
    UIScrollView* scrollView = (UIScrollView*)[ui_view_->view_ superview];
    [scrollView setContentOffset:root_scroll_offset.ToCGPoint()];
    [[scrollView delegate] scrollViewDidScroll:scrollView];
  } else {
    UpdateFrameBounds();
  }
}

void RenderWidgetHostViewIOS::OnRenderFrameMetadataChangedBeforeActivation(
    const cc::RenderFrameMetadata& metadata) {
  UIScrollView* scrollView = (UIScrollView*)[ui_view_->view_ superview];
  CGSize newContentSize = metadata.root_layer_size.ToCGSize();
  if (!CGSizeEqualToSize([scrollView contentSize], newContentSize)) {
    [scrollView setContentSize:newContentSize];
  }
  if (metadata.root_scroll_offset) {
    ApplyRootScrollOffsetChanged(*metadata.root_scroll_offset, /*force=*/false);
  }
}

void RenderWidgetHostViewIOS::OnRootScrollOffsetChanged(
    const gfx::PointF& root_scroll_offset) {
  ApplyRootScrollOffsetChanged(root_scroll_offset, /*force=*/false);
}

void RenderWidgetHostViewIOS::ContentInsetChanged() {
  if (last_root_scroll_offset_) {
    ApplyRootScrollOffsetChanged(*last_root_scroll_offset_, /*force=*/true);
  }
  if (!is_scrolling_) {
    host()->SynchronizeVisualProperties();
  }
}

void RenderWidgetHostViewIOS::ExtendSelectionAndDelete(int32_t before,
                                                       int32_t after) {
  auto* input_handler = GetFrameWidgetInputHandlerForFocusedWidget();
  if (!input_handler) {
    return;
  }
  input_handler->ExtendSelectionAndDelete(before, after);
}

void RenderWidgetHostViewIOS::ExtendSelectionAndReplace(
    uint32_t before,
    uint32_t after,
    const std::u16string& replacement_text) {
  auto* input_handler = GetFrameWidgetInputHandlerForFocusedWidget();
  if (!input_handler) {
    return;
  }
  input_handler->ExtendSelectionAndReplace(before, after, replacement_text);
}

void RenderWidgetHostViewIOS::ExecuteEditCommand(const std::string& command) {
  auto* input_handler = GetFrameWidgetInputHandlerForFocusedWidget();
  if (!input_handler) {
    return;
  }
  input_handler->ExecuteEditCommand(command, std::nullopt);
}

void RenderWidgetHostViewIOS::SendKeyEvent(
    const input::NativeWebKeyboardEvent& event) {
  auto* host = GetFocusedWidget();
  if (!host) {
    return;
  }
  ui::LatencyInfo latency_info;
  latency_info.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_UI_COMPONENT);
  host->ForwardKeyboardEventWithLatencyInfo(event, latency_info);
}

blink::mojom::FrameWidgetInputHandler*
RenderWidgetHostViewIOS::GetFrameWidgetInputHandlerForFocusedWidget() {
  auto* focused_widget = GetFocusedWidget();
  if (!focused_widget) {
    return nullptr;
  }
  return focused_widget->GetFrameWidgetInputHandler();
}

void RenderWidgetHostViewIOS::StartAutoscrollForSelectionToPoint(
    const gfx::PointF& point) {
  auto* input_handler = GetFrameWidgetInputHandlerForFocusedWidget();
  if (!input_handler) {
    return;
  }
  input_handler->StartAutoscrollForSelectionToPoint(point);
}

void RenderWidgetHostViewIOS::StopAutoscroll() {
  auto* input_handler = GetFrameWidgetInputHandlerForFocusedWidget();
  if (!input_handler) {
    return;
  }
  input_handler->StopAutoscroll();
}

void RenderWidgetHostViewIOS::RectForEditFieldChars(
    const gfx::Range& range,
    blink::mojom::FrameWidgetInputHandler::RectForEditFieldCharsCallback
        callback) {
  auto* input_handler = GetFrameWidgetInputHandlerForFocusedWidget();
  if (!input_handler) {
    std::move(callback).Run(gfx::Rect());
    return;
  }
  input_handler->RectForEditFieldChars(range, std::move(callback));
}

gfx::Size RenderWidgetHostViewIOS::GetCompositorViewportPixelSize() {
  return gfx::ScaleToCeiledSize(
      IsTesting() ? GetRequestedRendererSize() : GetScreenInfo().rect.size(),
      GetDeviceScaleFactor());
}

void RenderWidgetHostViewIOS::ComputeDisplayFeature() {
  if (display_feature_overridden_for_emulation_) {
    return;
  }

  display_feature_ = std::nullopt;
  gfx::Rect view_bounds([ui_view_->view_ bounds]);
  if (view_bounds.IsEmpty()) {
    return;
  }

  float dip_scale = 1 / GetDeviceScaleFactor();
  // Segments coming from the platform are in native resolution.
  gfx::Rect transformed_display_feature =
      gfx::ScaleToRoundedRect(view_bounds, dip_scale);
  transformed_display_feature.Offset(-view_bounds.x(), -view_bounds.y());
  transformed_display_feature.Intersect(gfx::Rect(GetVisibleViewportSize()));
  if (transformed_display_feature.x() == 0) {
    display_feature_ = {DisplayFeature::Orientation::kHorizontal,
                        transformed_display_feature.y(),
                        transformed_display_feature.height()};
  } else if (transformed_display_feature.y() == 0) {
    display_feature_ = {DisplayFeature::Orientation::kVertical,
                        transformed_display_feature.x(),
                        transformed_display_feature.width()};
  }
}

}  // namespace content
