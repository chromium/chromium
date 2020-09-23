// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/exported/web_external_widget_impl.h"

#include "cc/trees/layer_tree_host.h"
#include "cc/trees/ukm_manager.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom-blink.h"
#include "third_party/blink/public/platform/scheduler/web_render_widget_scheduling_state.h"
#include "third_party/blink/renderer/platform/widget/input/widget_input_handler_manager.h"
#include "third_party/blink/renderer/platform/widget/widget_base.h"

namespace blink {

std::unique_ptr<WebExternalWidget> WebExternalWidget::Create(
    WebExternalWidgetClient* client,
    const blink::WebURL& debug_url,
    CrossVariantMojoAssociatedRemote<mojom::blink::WidgetHostInterfaceBase>
        widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::WidgetInterfaceBase>
        widget) {
  return std::make_unique<WebExternalWidgetImpl>(
      client, debug_url, std::move(widget_host), std::move(widget));
}

WebExternalWidgetImpl::WebExternalWidgetImpl(
    WebExternalWidgetClient* client,
    const WebURL& debug_url,
    CrossVariantMojoAssociatedRemote<mojom::blink::WidgetHostInterfaceBase>
        widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::WidgetInterfaceBase>
        widget)
    : client_(client),
      debug_url_(debug_url),
      widget_base_(std::make_unique<WidgetBase>(this,
                                                std::move(widget_host),
                                                std::move(widget),
                                                /*hidden=*/false,
                                                /*never_composited=*/false)) {
  DCHECK(client_);
}

WebExternalWidgetImpl::~WebExternalWidgetImpl() = default;

cc::LayerTreeHost* WebExternalWidgetImpl::InitializeCompositing(
    scheduler::WebThreadScheduler* main_thread_scheduler,
    cc::TaskGraphRunner* task_graph_runner,
    bool for_child_local_root_frame,
    const ScreenInfo& screen_info,
    std::unique_ptr<cc::UkmRecorderFactory> ukm_recorder_factory,
    const cc::LayerTreeSettings* settings) {
  widget_base_->InitializeCompositing(
      main_thread_scheduler, task_graph_runner, for_child_local_root_frame,
      screen_info, std::move(ukm_recorder_factory), settings);
  return widget_base_->LayerTreeHost();
}

void WebExternalWidgetImpl::Close(
    scoped_refptr<base::SingleThreadTaskRunner> cleanup_runner) {
  widget_base_->Shutdown(std::move(cleanup_runner));
  widget_base_.reset();
}

void WebExternalWidgetImpl::SetCompositorVisible(bool visible) {
  widget_base_->SetCompositorVisible(visible);
}

WebHitTestResult WebExternalWidgetImpl::HitTestResultAt(const gfx::PointF&) {
  NOTIMPLEMENTED();
  return {};
}

WebURL WebExternalWidgetImpl::GetURLForDebugTrace() {
  return debug_url_;
}

WebSize WebExternalWidgetImpl::Size() {
  return size_;
}

void WebExternalWidgetImpl::Resize(const WebSize& size) {
  if (size_ == size)
    return;
  size_ = size;
  client_->DidResize(gfx::Size(size));
}

WebInputEventResult WebExternalWidgetImpl::HandleInputEvent(
    const WebCoalescedInputEvent& coalesced_event) {
  return client_->HandleInputEvent(coalesced_event);
}

WebInputEventResult WebExternalWidgetImpl::DispatchBufferedTouchEvents() {
  return client_->DispatchBufferedTouchEvents();
}

scheduler::WebRenderWidgetSchedulingState*
WebExternalWidgetImpl::RendererWidgetSchedulingState() {
  return widget_base_->RendererWidgetSchedulingState();
}

void WebExternalWidgetImpl::SetCursor(const ui::Cursor& cursor) {
  widget_base_->SetCursor(cursor);
}

bool WebExternalWidgetImpl::HandlingInputEvent() {
  return widget_base_->input_handler().handling_input_event();
}

void WebExternalWidgetImpl::SetHandlingInputEvent(bool handling) {
  widget_base_->input_handler().set_handling_input_event(handling);
}

void WebExternalWidgetImpl::ProcessInputEventSynchronouslyForTesting(
    const WebCoalescedInputEvent& event,
    HandledEventCallback callback) {
  widget_base_->input_handler().HandleInputEvent(event, std::move(callback));
}

void WebExternalWidgetImpl::UpdateTextInputState() {
  widget_base_->UpdateTextInputState();
}

void WebExternalWidgetImpl::UpdateSelectionBounds() {
  widget_base_->UpdateSelectionBounds();
}

void WebExternalWidgetImpl::ShowVirtualKeyboard() {
  widget_base_->ShowVirtualKeyboard();
}

void WebExternalWidgetImpl::SetFocus(bool focus) {
  widget_base_->SetFocus(focus);
}

bool WebExternalWidgetImpl::HasFocus() {
  return widget_base_->has_focus();
}

void WebExternalWidgetImpl::FlushInputProcessedCallback() {
  widget_base_->FlushInputProcessedCallback();
}

void WebExternalWidgetImpl::CancelCompositionForPepper() {
  widget_base_->CancelCompositionForPepper();
}

void WebExternalWidgetImpl::RequestMouseLock(
    bool has_transient_user_activation,
    bool priviledged,
    bool request_unadjusted_movement,
    base::OnceCallback<void(
        mojom::blink::PointerLockResult,
        CrossVariantMojoRemote<mojom::blink::PointerLockContextInterfaceBase>)>
        callback) {
  widget_base_->RequestMouseLock(has_transient_user_activation, priviledged,
                                 request_unadjusted_movement,
                                 std::move(callback));
}

#if defined(OS_ANDROID)
SynchronousCompositorRegistry*
WebExternalWidgetImpl::GetSynchronousCompositorRegistry() {
  return widget_base_->widget_input_handler_manager()
      ->GetSynchronousCompositorRegistry();
}
#endif

void WebExternalWidgetImpl::ApplyVisualProperties(
    const VisualProperties& visual_properties) {
  widget_base_->UpdateVisualProperties(visual_properties);
}

void WebExternalWidgetImpl::UpdateVisualProperties(
    const VisualProperties& visual_properties) {
  widget_base_->UpdateSurfaceAndScreenInfo(
      visual_properties.local_surface_id_allocation.value_or(
          viz::LocalSurfaceIdAllocation()),
      visual_properties.compositor_viewport_pixel_rect,
      visual_properties.screen_info);
  widget_base_->SetVisibleViewportSizeInDIPs(
      visual_properties.visible_viewport_size);
  Resize(WebSize(widget_base_->DIPsToBlinkSpace(visual_properties.new_size)));
  client_->DidUpdateVisualProperties();
}

const ScreenInfo& WebExternalWidgetImpl::GetScreenInfo() {
  return widget_base_->GetScreenInfo();
}

gfx::Rect WebExternalWidgetImpl::WindowRect() {
  return widget_base_->WindowRect();
}

gfx::Rect WebExternalWidgetImpl::ViewRect() {
  return widget_base_->ViewRect();
}

void WebExternalWidgetImpl::SetScreenRects(
    const gfx::Rect& widget_screen_rect,
    const gfx::Rect& window_screen_rect) {
  widget_base_->SetScreenRects(widget_screen_rect, window_screen_rect);
}

gfx::Size WebExternalWidgetImpl::VisibleViewportSizeInDIPs() {
  return widget_base_->VisibleViewportSizeInDIPs();
}

void WebExternalWidgetImpl::SetPendingWindowRect(
    const gfx::Rect* window_screen_rect) {
  widget_base_->SetPendingWindowRect(window_screen_rect);
}

bool WebExternalWidgetImpl::IsHidden() const {
  return widget_base_->is_hidden();
}

void WebExternalWidgetImpl::DidOverscrollForTesting(
    const gfx::Vector2dF& overscroll_delta,
    const gfx::Vector2dF& accumulated_overscroll,
    const gfx::PointF& position,
    const gfx::Vector2dF& velocity) {
  cc::OverscrollBehavior overscroll_behavior =
      widget_base_->LayerTreeHost()->overscroll_behavior();
  widget_base_->input_handler().DidOverscrollFromBlink(
      overscroll_delta, accumulated_overscroll, position, velocity,
      overscroll_behavior);
}

void WebExternalWidgetImpl::SetRootLayer(scoped_refptr<cc::Layer> layer) {
  widget_base_->LayerTreeHost()->SetNonBlinkManagedRootLayer(layer);
}

void WebExternalWidgetImpl::RequestNewLayerTreeFrameSink(
    LayerTreeFrameSinkCallback callback) {
  client_->RequestNewLayerTreeFrameSink(std::move(callback));
}

void WebExternalWidgetImpl::RecordTimeToFirstActivePaint(
    base::TimeDelta duration) {
  client_->RecordTimeToFirstActivePaint(duration);
}

void WebExternalWidgetImpl::DidCommitAndDrawCompositorFrame() {
  client_->DidCommitAndDrawCompositorFrame();
}

bool WebExternalWidgetImpl::WillHandleGestureEvent(
    const WebGestureEvent& event) {
  return client_->WillHandleGestureEvent(event);
}

bool WebExternalWidgetImpl::WillHandleMouseEvent(const WebMouseEvent& event) {
  return false;
}

void WebExternalWidgetImpl::ObserveGestureEventAndResult(
    const WebGestureEvent& gesture_event,
    const gfx::Vector2dF& unused_delta,
    const cc::OverscrollBehavior& overscroll_behavior,
    bool event_processed) {
  client_->DidHandleGestureScrollEvent(gesture_event, unused_delta,
                                       overscroll_behavior, event_processed);
}

bool WebExternalWidgetImpl::SupportsBufferedTouchEvents() {
  return client_->SupportsBufferedTouchEvents();
}

const ScreenInfo& WebExternalWidgetImpl::GetOriginalScreenInfo() {
  return widget_base_->GetScreenInfo();
}

gfx::Rect WebExternalWidgetImpl::ViewportVisibleRect() {
  return widget_base_->CompositorViewportRect();
}

}  // namespace blink
