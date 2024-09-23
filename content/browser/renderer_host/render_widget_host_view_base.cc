// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_base.h"

#include <optional>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "components/input/event_with_latency_info.h"
#include "components/input/render_widget_host_input_event_router.h"
#include "components/input/render_widget_host_view_input_observer.h"
#include "components/viz/common/features.h"
#include "components/viz/common/surfaces/subtree_capture_id.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/device_posture/device_posture_provider_impl.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/renderer_host/delegated_frame_host.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/input/mouse_wheel_phase_handler.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target_base.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_owner_delegate.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/renderer_host/scoped_view_transition_resources.h"
#include "content/browser/renderer_host/text_input_manager.h"
#include "content/browser/renderer_host/visible_time_request_trigger.h"
#include "content/common/content_switches_internal.h"
#include "content/public/common/page_visibility_state.h"
#include "third_party/blink/public/mojom/frame/intrinsic_sizing_info.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/display_util.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace content {

RenderWidgetHostViewBase::RenderWidgetHostViewBase(RenderWidgetHost* host)
    : host_(RenderWidgetHostImpl::From(host)),
      // `screen_infos_` must be initialized, to permit unconditional access to
      // its current display. A placeholder ScreenInfo is used here, so the
      // first call to UpdateScreenInfo will trigger the expected updates.
      screen_infos_(display::ScreenInfos(display::ScreenInfo())) {}

RenderWidgetHostViewBase::~RenderWidgetHostViewBase() {
  CHECK(!keyboard_locked_);
  CHECK(!IsPointerLocked());
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

MouseWheelPhaseHandler* RenderWidgetHostViewBase::GetMouseWheelPhaseHandler() {
  return nullptr;
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
    const gfx::Rect& bounding_box,
    bool is_anchor_first) {
#if !BUILDFLAG(IS_ANDROID)
  if (GetTextInputManager())
    GetTextInputManager()->SelectionBoundsChanged(
        this, anchor_rect, anchor_dir, focus_rect, focus_dir, bounding_box,
        is_anchor_first);
#else
  NOTREACHED_IN_MIGRATION()
      << "Selection bounds should be routed through the compositor.";
#endif
}

RenderWidgetHostViewBase* RenderWidgetHostViewBase::GetRootView() {
  return this;
}

void RenderWidgetHostViewBase::SelectionChanged(const std::u16string& text,
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
  return nullptr;
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

#if BUILDFLAG(IS_ANDROID)
  NOTREACHED_IN_MIGRATION()
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
              SkCanvas canvas(main_image, SkSurfaceProps{});
              canvas.drawImage(popup_image.asImage(), offset.x(), offset.y());
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

void RenderWidgetHostViewBase::CopyFromExactSurface(
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
  video_capturer->ChangeTarget(viz::VideoCaptureTarget(GetFrameSinkId()),
                               /*sub_capture_target_version=*/0);
  return video_capturer;
}

std::u16string RenderWidgetHostViewBase::GetSelectedText() {
  if (!GetTextInputManager())
    return std::u16string();
  return GetTextInputManager()->GetTextSelection(this)->selected_text();
}

void RenderWidgetHostViewBase::SetBackgroundColor(SkColor color) {
  // TODO(danakj): OPAQUE colors only make sense for main frame widgets,
  // as child frames are always transparent background. We should move this to
  // `blink::WebView` instead.
  CHECK(SkColorGetA(color) == SK_AlphaOPAQUE ||
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

std::optional<SkColor> RenderWidgetHostViewBase::GetBackgroundColor() {
  if (content_background_color_)
    return content_background_color_;
  return default_background_color_;
}

bool RenderWidgetHostViewBase::IsBackgroundColorOpaque() {
  std::optional<SkColor> bg_color = GetBackgroundColor();
  return bg_color ? SkColorGetA(*bg_color) == SK_AlphaOPAQUE : true;
}

void RenderWidgetHostViewBase::CopyBackgroundColorIfPresentFrom(
    const RenderWidgetHostView& other) {
  const RenderWidgetHostViewBase& other_base =
      static_cast<const RenderWidgetHostViewBase&>(other);
  if (!other_base.content_background_color_ &&
      !other_base.default_background_color_) {
    return;
  }
  if (content_background_color_ == other_base.content_background_color_ &&
      default_background_color_ == other_base.default_background_color_) {
    return;
  }
  bool was_opaque = IsBackgroundColorOpaque();
  content_background_color_ = other_base.content_background_color_;
  default_background_color_ = other_base.default_background_color_;
  UpdateBackgroundColor();
  bool opaque = IsBackgroundColorOpaque();
  if (was_opaque != opaque && host()->owner_delegate()) {
    host()->owner_delegate()->SetBackgroundOpaque(opaque);
  }
}

bool RenderWidgetHostViewBase::IsPointerLocked() {
  return false;
}

const viz::DisplayHitTestQueryMap&
RenderWidgetHostViewBase::GetDisplayHitTestQuery() const {
  return GetHostFrameSinkManager()->GetDisplayHitTestQuery();
}

bool RenderWidgetHostViewBase::
    GetIsPointerLockedUnadjustedMovementForTesting() {
  return false;
}

bool RenderWidgetHostViewBase::CanBePointerLocked() {
  return HasFocus();
}

bool RenderWidgetHostViewBase::AccessibilityHasFocus() {
  return HasFocus();
}

bool RenderWidgetHostViewBase::LockKeyboard(
    std::optional<base::flat_set<ui::DomCode>> codes) {
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

bool RenderWidgetHostViewBase::HasFallbackSurface() const {
  NOTREACHED_IN_MIGRATION();
  return false;
}

viz::SurfaceId RenderWidgetHostViewBase::GetFallbackSurfaceIdForTesting()
    const {
  NOTREACHED_IN_MIGRATION();
  return viz::SurfaceId();
}

void RenderWidgetHostViewBase::SetWidgetType(WidgetType widget_type) {
  widget_type_ = widget_type;
}

WidgetType RenderWidgetHostViewBase::GetWidgetType() {
  return widget_type_;
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

bool RenderWidgetHostViewBase::ShouldInitiateStylusWriting() {
  return false;
}

bool RenderWidgetHostViewBase::RequestRepaintForTesting() {
  return false;
}

// Send system cursor size to the renderer via UpdateScreenInfo().
void RenderWidgetHostViewBase::UpdateSystemCursorSize(
    const gfx::Size& cursor_size) {
  system_cursor_size_ = cursor_size;
  UpdateScreenInfo();
}

void RenderWidgetHostViewBase::UpdateScreenInfo() {
  bool force_sync_visual_properties = false;
  // Delegate, which is usually WebContentsImpl, do not send rect updates for
  // popups as they are not registered as FrameTreeNodes. Instead, send screen
  // rects directly to host and force synchronization of visual properties so
  // that blink knows host changed bounds. This only happens if the change was
  // instantiated by system server/compositor (for example, Wayland, which
  // may reposition a popup if part of it is going to be shown outside a
  // display's work area. Note that Wayland clients cannot know where their
  // windows are located and cannot adjust bounds).
  if (widget_type_ == WidgetType::kPopup) {
    if (host()) {
      force_sync_visual_properties = true;
      host()->SendScreenRects();
    }
  } else {
    if (host() && host()->delegate())
      host()->delegate()->SendScreenRects();
  }

  auto new_screen_infos = GetNewScreenInfosForUpdate();

  if (scale_override_for_capture_ != 1.0f) {
    // If HiDPI capture mode is active, adjust the device scale factor to
    // increase the rendered pixel count. |new_screen_infos| always contains the
    // unmodified original values for the display, and a copy of it is saved in
    // |screen_infos_|, with a modification applied if applicable. When HiDPI
    // mode is turned off (the scale override is 1.0), the original
    // |new_screen_infos| value gets copied unchanged to |screen_infos_|.
    const float old_device_scale_factor =
        new_screen_infos.current().device_scale_factor;
    new_screen_infos.mutable_current().device_scale_factor =
        old_device_scale_factor * scale_override_for_capture_;
    DVLOG(1) << __func__ << ": Overriding device_scale_factor from "
             << old_device_scale_factor << " to "
             << new_screen_infos.current().device_scale_factor
             << " for capture.";
  }

#if BUILDFLAG(IS_OZONE)
  // There are platforms where no global screen coordinates are available for
  // client applications, and scaling is done in a per-window basis (rather than
  // per-display) and controlled by the display server. In such cases, the
  // ScreenInfo Web API is mostly pointless. To avoid distorted graphics in web
  // contents, override the display scale with the preferred window scale here.
  // TODO(crbug.com/336007385): Consolidate screen representation and a less
  // hacky scale handling in platforms that support per-window scaling.
  if (ui::OzonePlatform::GetInstance()
          ->GetPlatformRuntimeProperties()
          .supports_per_window_scaling) {
    const float window_scale =
        display::Screen::GetScreen()
            ->GetPreferredScaleFactorForView(GetNativeView())
            .value_or(1.0f);
    auto& screen = new_screen_infos.mutable_current();
    const float old = screen.device_scale_factor;
    if (window_scale != old) {
      VLOG(1) << __func__ << ": Overriding scale for screen '" << screen.label
              << "' from " << old << " with windows scale " << window_scale;
      screen.device_scale_factor = window_scale;
      force_sync_visual_properties = true;
    }
  }
#endif  // BUILDFLAG(IS_OZONE)

  if (screen_infos_ == new_screen_infos && !force_sync_visual_properties)
    return;

  // We need to look at `orientation_type` which is marked as kUndefined at
  // startup. Unlike `orientation_angle` that uses 0 degrees as the default.
  // This accounts for devices which have a default landscape orientation, such
  // as tablets. We do not want the first UpdateScreenInfo to be treated as a
  // rotation.
  const bool has_rotation_changed =
      screen_infos_.current().orientation_type !=
          display::mojom::ScreenOrientation::kUndefined &&
      screen_infos_.current().orientation_type !=
          new_screen_infos.current().orientation_type;
  screen_infos_ = std::move(new_screen_infos);

  // Notify the associated RenderWidgetHostImpl when screen info has changed.
  // That will synchronize visual properties needed for frame tree rendering
  // and for web platform APIs that expose screen and window info and events.
  if (host()) {
    OnSynchronizedDisplayPropertiesChanged(has_rotation_changed);
    host()->NotifyScreenInfoChanged();
  }
}

void RenderWidgetHostViewBase::UpdateActiveState(bool active) {
  // Send active state through the delegate if there is one to make sure
  // it stays consistent across all widgets in the tab. Not every
  // RenderWidgetHost has a delegate (for example, drop-down widgets).
  if (host()->delegate())
    host()->delegate()->SendActiveState(active);
  else
    host()->SetActive(active);
}

void RenderWidgetHostViewBase::DidUnregisterFromTextInputManager(
    TextInputManager* text_input_manager) {
  CHECK(text_input_manager && text_input_manager_ == text_input_manager);

  text_input_manager_ = nullptr;
}

void RenderWidgetHostViewBase::EnableAutoResize(const gfx::Size& min_size,
                                                const gfx::Size& max_size) {
  host()->SetAutoResize(true, min_size, max_size);
  host()->SynchronizeVisualProperties();
}

bool RenderWidgetHostViewBase::IsAutoResizeEnabled() {
  return host()->auto_resize_enabled();
}

void RenderWidgetHostViewBase::DisableAutoResize(const gfx::Size& new_size) {
  // Note that for some subclasses, such as RenderWidgetHostViewAura, setting
  // the view size may trigger the synchronization on the visual properties. As
  // a result, it may crete the intermediate status that the view size has
  // changed while the auto resize status is obsolete, and then brings
  // unnecessary updates in view layout. Hence we should disable the auto
  // resize before setting the view size.
  host()->SetAutoResize(false, gfx::Size(), gfx::Size());
  if (!new_size.IsEmpty())
    SetSize(new_size);
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

display::ScreenInfo RenderWidgetHostViewBase::GetScreenInfo() const {
  return screen_infos_.current();
}

display::ScreenInfos RenderWidgetHostViewBase::GetScreenInfos() const {
  return screen_infos_;
}

void RenderWidgetHostViewBase::ResetGestureDetection() {}

float RenderWidgetHostViewBase::GetDeviceScaleFactor() const {
  return screen_infos_.current().device_scale_factor;
}

base::WeakPtr<input::RenderWidgetHostViewInput>
RenderWidgetHostViewBase::GetInputWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

input::RenderInputRouter* RenderWidgetHostViewBase::GetViewRenderInputRouter() {
  return host()->GetRenderInputRouter();
}

void RenderWidgetHostViewBase::SetScaleOverrideForCapture(float scale) {
  DVLOG(1) << __func__ << ": override=" << scale;
  scale_override_for_capture_ = scale;
  UpdateScreenInfo();
}

float RenderWidgetHostViewBase::GetScaleOverrideForCapture() const {
  return scale_override_for_capture_;
}

void RenderWidgetHostViewBase::OnAutoscrollStart() {
  if (!GetMouseWheelPhaseHandler())
    return;

  // End the current scrolling seqeunce when autoscrolling starts.
  GetMouseWheelPhaseHandler()->DispatchPendingWheelEndEvent();
}

DevicePosturePlatformProvider*
RenderWidgetHostViewBase::GetDevicePosturePlatformProvider() {
  if (!host() || !host()->delegate()) {
    return nullptr;
  }

  DevicePostureProviderImpl* posture_provider =
      static_cast<DevicePostureProviderImpl*>(
          host()->delegate()->GetDevicePostureProvider());
  if (!posture_provider) {
    return nullptr;
  }

  return posture_provider->platform_provider();
}

gfx::Size RenderWidgetHostViewBase::GetVisibleViewportSize() {
  return GetViewBounds().size();
}

void RenderWidgetHostViewBase::SetInsets(const gfx::Insets& insets) {
  NOTIMPLEMENTED_LOG_ONCE();
}

const viz::LocalSurfaceId&
RenderWidgetHostViewBase::IncrementSurfaceIdForNavigation() {
  NOTREACHED();
}

void RenderWidgetHostViewBase::OnOldViewDidNavigatePreCommit() {}

void RenderWidgetHostViewBase::OnNewViewDidNavigatePostCommit() {}

void RenderWidgetHostViewBase::OnFrameTokenChangedForView(
    uint32_t frame_token,
    base::TimeTicks activation_time) {
  if (host())
    host()->DidProcessFrame(frame_token, activation_time);
}

void RenderWidgetHostViewBase::ProcessMouseEvent(
    const blink::WebMouseEvent& event,
    const ui::LatencyInfo& latency) {
  // TODO(crbug.com/40564125): Figure out the reason |host| is null here in all
  // Process* functions.
  if (!host())
    return;

  // Ensure the event is not routed to a prerendered page.
  if (host()->frame_tree() && host()->frame_tree()->is_prerendering()) {
    NOTREACHED();
  }

  PreProcessMouseEvent(event);
  host()->ForwardMouseEventWithLatencyInfo(event, latency);
}

void RenderWidgetHostViewBase::ProcessMouseWheelEvent(
    const blink::WebMouseWheelEvent& event,
    const ui::LatencyInfo& latency) {
  if (!host())
    return;

  // Ensure the event is not routed to a prerendered page.
  if (host()->frame_tree() && host()->frame_tree()->is_prerendering()) {
    NOTREACHED();
  }

  host()->ForwardWheelEventWithLatencyInfo(event, latency);
}

void RenderWidgetHostViewBase::ProcessTouchEvent(
    const blink::WebTouchEvent& event,
    const ui::LatencyInfo& latency) {
  if (!host())
    return;

  // Ensure the event is not routed to a prerendered page.
  if (host()->frame_tree() && host()->frame_tree()->is_prerendering()) {
    NOTREACHED();
  }

  PreProcessTouchEvent(event);
  host()->GetRenderInputRouter()->ForwardTouchEventWithLatencyInfo(event,
                                                                   latency);
}

void RenderWidgetHostViewBase::ProcessGestureEvent(
    const blink::WebGestureEvent& event,
    const ui::LatencyInfo& latency) {
  if (!host())
    return;

  // Ensure the event is not routed to a prerendered page.
  if (host()->frame_tree() && host()->frame_tree()->is_prerendering()) {
    NOTREACHED();
  }

  host()->GetRenderInputRouter()->ForwardGestureEventWithLatencyInfo(event,
                                                                     latency);
}

gfx::PointF RenderWidgetHostViewBase::TransformPointToRootCoordSpaceF(
    const gfx::PointF& point) {
  return point;
}

bool RenderWidgetHostViewBase::IsRenderWidgetHostViewChildFrame() {
  return false;
}

bool RenderWidgetHostViewBase::HasSize() const {
  return true;
}

void RenderWidgetHostViewBase::Show() {
  ShowWithVisibility(PageVisibilityState::kVisible);
}

void RenderWidgetHostViewBase::Destroy() {
  host_ = nullptr;
}

bool RenderWidgetHostViewBase::CanSynchronizeVisualProperties() {
  return true;
}

double RenderWidgetHostViewBase::GetCSSZoomFactor() const {
  return 1.0;
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
    const std::optional<std::vector<gfx::Rect>>& character_bounds,
    const std::optional<std::vector<gfx::Rect>>& line_bounds) {
  if (GetTextInputManager()) {
    GetTextInputManager()->ImeCompositionRangeChanged(
        this, range, character_bounds, line_bounds);
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

TouchSelectionControllerClientManager*
RenderWidgetHostViewBase::GetTouchSelectionControllerClientManager() {
  return nullptr;
}

void RenderWidgetHostViewBase::SynchronizeVisualProperties() {
  if (host())
    host()->SynchronizeVisualProperties();
}

display::ScreenInfos RenderWidgetHostViewBase::GetNewScreenInfosForUpdate() {
  // RWHVChildFrame gets its ScreenInfos from the CrossProcessFrameConnector.
  CHECK(!IsRenderWidgetHostViewChildFrame());

  display::ScreenInfos screen_infos;

  if (auto* screen = display::Screen::GetScreen()) {
    gfx::NativeView native_view = GetNativeView();
    const auto& display = native_view
                              ? screen->GetDisplayNearestView(native_view)
                              : screen->GetPrimaryDisplay();
    screen_infos = screen->GetScreenInfosNearestDisplay(display.id());
  } else {
    // If there is no Screen, create fake ScreenInfos (for tests).
    screen_infos = display::ScreenInfos(display::ScreenInfo());
  }

  // Set system cursor size separately as it's not a property of screen or
  // display.
  screen_infos.system_cursor_size = system_cursor_size_;

  return screen_infos;
}

void RenderWidgetHostViewBase::DidNavigate() {
  if (host())
    host()->SynchronizeVisualProperties();
}

WebContentsAccessibility*
RenderWidgetHostViewBase::GetWebContentsAccessibility() {
  return nullptr;
}

void RenderWidgetHostViewBase::SetTooltipObserverForTesting(
    TooltipObserver* observer) {
  tooltip_observer_for_testing_ = observer;
}

ui::Compositor* RenderWidgetHostViewBase::GetCompositor() {
  return nullptr;
}

ui::mojom::VirtualKeyboardMode
RenderWidgetHostViewBase::GetVirtualKeyboardMode() {
  // Only platforms supporting these APIs will implement this.
  return ui::mojom::VirtualKeyboardMode::kUnset;
}

bool RenderWidgetHostViewBase::IsHTMLFormPopup() const {
  return false;
}

void RenderWidgetHostViewBase::OnShowWithPageVisibility(
    PageVisibilityState page_visibility) {
  if (!host())
    return;

  EnsurePlatformVisibility(page_visibility);

  VisibleTimeRequestTrigger& visible_time_request_trigger =
      host_->GetVisibleTimeRequestTrigger();

  // NB: don't call visible_time_request_trigger.TakeRequest() unless the
  // request will be used. If it isn't used here it must be left in the trigger
  // for the next call.

  const bool web_contents_is_visible =
      page_visibility == PageVisibilityState::kVisible;

  if (host_->is_hidden()) {
    // If the WebContents is becoming visible, ask the compositor to report the
    // visibility time for metrics. Otherwise the widget is being rendered even
    // though the WebContents is hidden or occluded, for example due to being
    // captured, so it should not be included in visibility time metrics.
    NotifyHostAndDelegateOnWasShown(
        web_contents_is_visible ? visible_time_request_trigger.TakeRequest()
                                : nullptr);
    return;
  }

  // `page_visibility` changed while the widget remains visible (kVisible ->
  // kHiddenButPainting or vice versa). Nothing to do except update the
  // visible time request, if any.
  if (web_contents_is_visible) {
    // The widget is already rendering, but now the WebContents is becoming
    // visible, so send any visibility time request to the compositor now.
    if (auto visible_time_request =
            visible_time_request_trigger.TakeRequest()) {
      RequestSuccessfulPresentationTimeFromHostOrDelegate(
          std::move(visible_time_request));
    }
    return;
  }

  // The widget should keep rendering but the WebContents is no longer
  // visible. If the compositor didn't already report the visibility time,
  // it's too late. (For example, if the WebContents is being captured and
  // was put in the foreground and then quickly hidden again before the
  // compositor submitted a frame. The compositor will keep submitting
  // frames for the capture but they should not be included in the
  // visibility metrics.)
  CancelSuccessfulPresentationTimeRequestForHostAndDelegate();
  return;
}

void RenderWidgetHostViewBase::SetIsFrameSinkIdOwner(bool is_owner) {
  if (is_frame_sink_id_owner_ == is_owner) {
    return;
  }

  is_frame_sink_id_owner_ = is_owner;
  UpdateFrameSinkIdRegistration();
}

void RenderWidgetHostViewBase::UpdateFrameSinkIdRegistration() {
  // If Destroy() has been called before we get here, host_ may be null.
  if (!host() || !host()->delegate() ||
      !host()->delegate()->GetInputEventRouter()) {
    return;
  }

  // Let the page-level input event router know about our frame sink ID
  // for surface-based hit testing.
  auto* router = host()->delegate()->GetInputEventRouter();
  if (is_frame_sink_id_owner_) {
    if (!router->IsViewInMap(this)) {
      router->AddFrameSinkIdOwner(GetFrameSinkId(), this);
    }
  } else if (router->IsViewInMap(this)) {
    // Ensure this view is the owner before removing the associated FrameSinkId
    // from input tracking. Speculative views start as non-owing and will not
    // register until ownership has been transferred.
    router->RemoveFrameSinkIdOwner(GetFrameSinkId());
  }
}

void RenderWidgetHostViewBase::SetViewTransitionResources(
    std::unique_ptr<ScopedViewTransitionResources> resources) {
  view_transition_resources_ = std::move(resources);
}

}  // namespace content
