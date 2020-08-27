// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "third_party/blink/renderer/core/frame/web_view_frame_widget.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame_ukm_aggregator.h"
#include "third_party/blink/renderer/core/frame/screen_metrics_emulator.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/widget/widget_base.h"

namespace blink {

WebViewFrameWidget::WebViewFrameWidget(
    util::PassKey<WebFrameWidget>,
    WebWidgetClient& client,
    WebViewImpl& web_view,
    CrossVariantMojoAssociatedRemote<mojom::blink::FrameWidgetHostInterfaceBase>
        frame_widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::FrameWidgetInterfaceBase>
        frame_widget,
    CrossVariantMojoAssociatedRemote<mojom::blink::WidgetHostInterfaceBase>
        widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::WidgetInterfaceBase>
        widget,
    bool is_for_nested_main_frame)
    : WebFrameWidgetBase(client,
                         std::move(frame_widget_host),
                         std::move(frame_widget),
                         std::move(widget_host),
                         std::move(widget)),
      web_view_(&web_view),
      is_for_nested_main_frame_(is_for_nested_main_frame),
      self_keep_alive_(PERSISTENT_FROM_HERE, this) {
  web_view_->SetMainFrameWidgetBase(this);
}

WebViewFrameWidget::~WebViewFrameWidget() = default;

void WebViewFrameWidget::Close(
    scoped_refptr<base::SingleThreadTaskRunner> cleanup_runner) {
  GetPage()->WillCloseAnimationHost(nullptr);
  // Closing the WebViewFrameWidget happens in response to the local main frame
  // being detached from the Page/WebViewImpl.
  web_view_->SetMainFrameWidgetBase(nullptr);
  web_view_ = nullptr;
  WebFrameWidgetBase::Close(std::move(cleanup_runner));
  self_keep_alive_.Clear();
}

WebSize WebViewFrameWidget::Size() {
  return web_view_->Size();
}

void WebViewFrameWidget::Resize(const WebSize& size) {
  web_view_->Resize(size);
}

void WebViewFrameWidget::SetSuppressFrameRequestsWorkaroundFor704763Only(
    bool suppress_frame_requests) {
  web_view_->SetSuppressFrameRequestsWorkaroundFor704763Only(
      suppress_frame_requests);
}

void WebViewFrameWidget::BeginMainFrame(base::TimeTicks last_frame_time) {
  web_view_->BeginFrame(last_frame_time);
}

void WebViewFrameWidget::DidBeginMainFrame() {
  WebFrameWidgetBase::DidBeginMainFrame();

  auto* main_frame = web_view_->MainFrameImpl();
  DocumentLifecycle::AllowThrottlingScope throttling_scope(
      main_frame->GetFrame()->GetDocument()->Lifecycle());
  PageWidgetDelegate::DidBeginFrame(*main_frame->GetFrame());
}

void WebViewFrameWidget::BeginUpdateLayers() {
  web_view_->BeginUpdateLayers();
}

void WebViewFrameWidget::EndUpdateLayers() {
  web_view_->EndUpdateLayers();
}

void WebViewFrameWidget::BeginCommitCompositorFrame() {
  commit_compositor_frame_start_time_.emplace(base::TimeTicks::Now());
}

void WebViewFrameWidget::EndCommitCompositorFrame(
    base::TimeTicks commit_start_time) {
  DCHECK(commit_compositor_frame_start_time_.has_value());

  WebFrameWidgetBase::EndCommitCompositorFrame(commit_start_time);
  web_view_->MainFrameImpl()
      ->GetFrame()
      ->View()
      ->EnsureUkmAggregator()
      .RecordImplCompositorSample(commit_compositor_frame_start_time_.value(),
                                  commit_start_time, base::TimeTicks::Now());
  commit_compositor_frame_start_time_.reset();
}

void WebViewFrameWidget::RecordStartOfFrameMetrics() {
  web_view_->RecordStartOfFrameMetrics();
}

void WebViewFrameWidget::RecordEndOfFrameMetrics(
    base::TimeTicks frame_begin_time,
    cc::ActiveFrameSequenceTrackers trackers) {
  web_view_->RecordEndOfFrameMetrics(frame_begin_time, trackers);
}

std::unique_ptr<cc::BeginMainFrameMetrics>
WebViewFrameWidget::GetBeginMainFrameMetrics() {
  return web_view_->GetBeginMainFrameMetrics();
}

void WebViewFrameWidget::UpdateLifecycle(WebLifecycleUpdate requested_update,
                                         DocumentUpdateReason reason) {
  web_view_->UpdateLifecycle(requested_update, reason);
}

void WebViewFrameWidget::ThemeChanged() {
  web_view_->ThemeChanged();
}

WebInputEventResult WebViewFrameWidget::HandleInputEvent(
    const WebCoalescedInputEvent& event) {
  return web_view_->HandleInputEvent(event);
}

WebInputEventResult WebViewFrameWidget::DispatchBufferedTouchEvents() {
  return web_view_->DispatchBufferedTouchEvents();
}

void WebViewFrameWidget::SetCursorVisibilityState(bool is_visible) {
  web_view_->SetCursorVisibilityState(is_visible);
}

void WebViewFrameWidget::ApplyViewportChanges(
    const ApplyViewportChangesArgs& args) {
  web_view_->ApplyViewportChanges(args);
}

void WebViewFrameWidget::RecordManipulationTypeCounts(
    cc::ManipulationInfo info) {
  web_view_->RecordManipulationTypeCounts(info);
}
void WebViewFrameWidget::SendOverscrollEventFromImplSide(
    const gfx::Vector2dF& overscroll_delta,
    cc::ElementId scroll_latched_element_id) {
  web_view_->SendOverscrollEventFromImplSide(overscroll_delta,
                                             scroll_latched_element_id);
}
void WebViewFrameWidget::SendScrollEndEventFromImplSide(
    cc::ElementId scroll_latched_element_id) {
  web_view_->SendScrollEndEventFromImplSide(scroll_latched_element_id);
}

void WebViewFrameWidget::MouseCaptureLost() {
  web_view_->MouseCaptureLost();
}

void WebViewFrameWidget::FocusChanged(bool enable) {
  web_view_->SetFocus(enable);
  Client()->FocusChanged(enable);
}

float WebViewFrameWidget::GetDeviceScaleFactorForTesting() {
  return device_scale_factor_for_testing_;
}

gfx::Rect WebViewFrameWidget::ViewportVisibleRect() {
  return widget_base_->CompositorViewportRect();
}

bool WebViewFrameWidget::ShouldHandleImeEvents() {
  return HasFocus();
}

float WebViewFrameWidget::GetEmulatorScale() {
  if (device_emulator_)
    return device_emulator_->scale();
  return 1.0f;
}

bool WebViewFrameWidget::SelectionBounds(WebRect& anchor,
                                         WebRect& focus) const {
  return web_view_->SelectionBounds(anchor, focus);
}

WebURL WebViewFrameWidget::GetURLForDebugTrace() {
  return web_view_->GetURLForDebugTrace();
}

WebString WebViewFrameWidget::GetLastToolTipTextForTesting() const {
  return GetPage()->GetChromeClient().GetLastToolTipTextForTesting();
}

void WebViewFrameWidget::EnableDeviceEmulation(
    const DeviceEmulationParams& parameters) {
  if (!device_emulator_) {
    WebSize web_view_size = web_view_->GetSize();
    WebRect size_in_window(0, 0, web_view_size.width, web_view_size.height);
    Client()->ConvertViewportToWindow(&size_in_window);

    device_emulator_ = MakeGarbageCollected<ScreenMetricsEmulator>(
        this, widget_base_->GetScreenInfo(),
        gfx::Size(size_in_window.width, size_in_window.height),
        widget_base_->VisibleViewportSize(), widget_base_->WidgetScreenRect(),
        widget_base_->WindowScreenRect());
  }
  device_emulator_->ChangeEmulationParams(parameters);
}

void WebViewFrameWidget::DisableDeviceEmulation() {
  if (!device_emulator_)
    return;
  device_emulator_->DisableAndApply();
  device_emulator_ = nullptr;
}

void WebViewFrameWidget::DidDetachLocalFrameTree() {
  web_view_->DidDetachLocalMainFrame();
}

WebInputMethodController*
WebViewFrameWidget::GetActiveWebInputMethodController() const {
  return web_view_->GetActiveWebInputMethodController();
}

bool WebViewFrameWidget::ScrollFocusedEditableElementIntoView() {
  return web_view_->ScrollFocusedEditableElementIntoView();
}

void WebViewFrameWidget::SetRootLayer(scoped_refptr<cc::Layer> root_layer) {
  if (!web_view_->does_composite()) {
    DCHECK(!root_layer);
    return;
  }
  cc::LayerTreeHost* layer_tree_host = widget_base_->LayerTreeHost();
  layer_tree_host->SetRootLayer(root_layer);
  web_view_->DidChangeRootLayer(!!root_layer);
}

WebHitTestResult WebViewFrameWidget::HitTestResultAt(const gfx::PointF& point) {
  return web_view_->HitTestResultAt(point);
}

HitTestResult WebViewFrameWidget::CoreHitTestResultAt(
    const gfx::PointF& point) {
  return web_view_->CoreHitTestResultAt(point);
}

void WebViewFrameWidget::ZoomToFindInPageRect(
    const WebRect& rect_in_root_frame) {
  web_view_->ZoomToFindInPageRect(rect_in_root_frame);
}

void WebViewFrameWidget::Trace(Visitor* visitor) const {
  WebFrameWidgetBase::Trace(visitor);
  visitor->Trace(device_emulator_);
}

PageWidgetEventHandler* WebViewFrameWidget::GetPageWidgetEventHandler() {
  return web_view_.get();
}

LocalFrameView* WebViewFrameWidget::GetLocalFrameViewForAnimationScrolling() {
  // Scrolling for the root frame is special we need to pass null indicating
  // we are at the top of the tree when setting up the Animation. Which will
  // cause ownership of the timeline and animation host.
  // See ScrollingCoordinator::AnimationHostInitialized.
  return nullptr;
}

void WebViewFrameWidget::SetZoomLevelForTesting(double zoom_level) {
  DCHECK_NE(zoom_level, -INFINITY);
  zoom_level_for_testing_ = zoom_level;
  SetZoomLevel(zoom_level);
}

void WebViewFrameWidget::ResetZoomLevelForTesting() {
  zoom_level_for_testing_ = -INFINITY;
  SetZoomLevel(0);
}

void WebViewFrameWidget::SetDeviceScaleFactorForTesting(float factor) {
  DCHECK_GE(factor, 0.f);

  device_scale_factor_for_testing_ = factor;

  // Receiving a 0 is used to reset between tests, it removes the override in
  // order to listen to the browser for the next test.
  if (!factor)
    return;

  // We are changing the device scale factor from the renderer, so allocate a
  // new viz::LocalSurfaceId to avoid surface invariants violations in tests.
  widget_base_->LayerTreeHost()->RequestNewLocalSurfaceId();
}

void WebViewFrameWidget::SetZoomLevel(double zoom_level) {
  // Override the zoom level with the testing one if necessary
  if (zoom_level_for_testing_ != -INFINITY)
    zoom_level = zoom_level_for_testing_;
  WebFrameWidgetBase::SetZoomLevel(zoom_level);
}

void WebViewFrameWidget::SetAutoResizeMode(bool auto_resize,
                                           const gfx::Size& min_window_size,
                                           const gfx::Size& max_window_size,
                                           float device_scale_factor) {
  if (auto_resize) {
    if (!Platform::Current()->IsUseZoomForDSFEnabled())
      device_scale_factor = 1.f;
    web_view_->EnableAutoResizeMode(
        gfx::ScaleToCeiledSize(min_window_size, device_scale_factor),
        gfx::ScaleToCeiledSize(max_window_size, device_scale_factor));
  } else if (web_view_->AutoResizeMode()) {
    web_view_->DisableAutoResizeMode();
  }
}

void WebViewFrameWidget::SetIsNestedMainFrameWidget(bool is_nested) {
  is_for_nested_main_frame_ = is_nested;
}

void WebViewFrameWidget::SetPageScaleStateAndLimits(
    float page_scale_factor,
    bool is_pinch_gesture_active,
    float minimum,
    float maximum) {
  WebFrameWidgetBase::SetPageScaleStateAndLimits(
      page_scale_factor, is_pinch_gesture_active, minimum, maximum);

  // If page scale hasn't changed, then just return without notifying
  // the remote frames.
  if (page_scale_factor == page_scale_factor_in_mainframe_ &&
      is_pinch_gesture_active == is_pinch_gesture_active_in_mainframe_) {
    return;
  }

  NotifyPageScaleFactorChanged(page_scale_factor, is_pinch_gesture_active);
}

void WebViewFrameWidget::DidAutoResize(const gfx::Size& size) {
  WebRect new_size_in_window(0, 0, size.width(), size.height());
  Client()->ConvertViewportToWindow(&new_size_in_window);

  // TODO(ccameron): Note that this destroys any information differentiating
  // |size| from the compositor's viewport size. Also note that the
  // calculation of |new_compositor_viewport_pixel_rect| does not appear to
  // take into account device emulation.
  widget_base_->LayerTreeHost()->RequestNewLocalSurfaceId();
  gfx::Rect new_compositor_viewport_pixel_rect =
      gfx::Rect(gfx::ScaleToCeiledSize(
          gfx::Rect(new_size_in_window).size(),
          widget_base_->GetScreenInfo().device_scale_factor));
  widget_base_->UpdateCompositorViewportRect(
      new_compositor_viewport_pixel_rect);
}

void WebViewFrameWidget::SetDeviceColorSpaceForTesting(
    const gfx::ColorSpace& color_space) {
  // We are changing the device color space from the renderer, so allocate a
  // new viz::LocalSurfaceId to avoid surface invariants violations in tests.
  widget_base_->LayerTreeHost()->RequestNewLocalSurfaceId();

  blink::ScreenInfo info = widget_base_->GetScreenInfo();
  info.display_color_spaces = gfx::DisplayColorSpaces(color_space);
  widget_base_->UpdateScreenInfo(info);
}

bool WebViewFrameWidget::AutoResizeMode() {
  return web_view_->AutoResizeMode();
}

bool WebViewFrameWidget::UpdateScreenRects(
    const gfx::Rect& widget_screen_rect,
    const gfx::Rect& window_screen_rect) {
  if (!device_emulator_)
    return false;
  device_emulator_->OnUpdateScreenRects(widget_screen_rect, window_screen_rect);
  return true;
}

const ScreenInfo& WebViewFrameWidget::GetOriginalScreenInfo() {
  if (device_emulator_)
    return device_emulator_->original_screen_info();
  return GetScreenInfo();
}

ScreenMetricsEmulator* WebViewFrameWidget::DeviceEmulator() {
  return device_emulator_;
}

void WebViewFrameWidget::SetScreenMetricsEmulationParameters(
    bool enabled,
    const DeviceEmulationParams& params) {
  if (enabled)
    View()->ActivateDevToolsTransform(params);
  else
    View()->DeactivateDevToolsTransform();
}

void WebViewFrameWidget::SetScreenInfoAndSize(
    const ScreenInfo& screen_info,
    const gfx::Size& widget_size,
    const gfx::Size& visible_viewport_size) {
  // Emulation happens on regular main frames which don't use auto-resize mode.
  DCHECK(!web_view_->AutoResizeMode());

  UpdateScreenInfo(screen_info);
  widget_base_->SetVisibleViewportSize(visible_viewport_size);
  Client()->SetSize(widget_size);
}

}  // namespace blink
