// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_WEB_EXTERNAL_WIDGET_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_WEB_EXTERNAL_WIDGET_IMPL_H_

#include "third_party/blink/public/web/web_external_widget.h"

#include "build/build_config.h"
#include "third_party/blink/public/mojom/input/pointer_lock_context.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/input/pointer_lock_result.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/page/widget.mojom-blink.h"
#include "third_party/blink/public/platform/cross_variant_mojo_util.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/renderer/platform/widget/widget_base_client.h"

namespace blink {
class WidgetBase;

class WebExternalWidgetImpl : public WebExternalWidget,
                              public WidgetBaseClient {
 public:
  WebExternalWidgetImpl(
      WebExternalWidgetClient* client,
      const WebURL& debug_url,
      CrossVariantMojoAssociatedRemote<mojom::blink::WidgetHostInterfaceBase>
          widget_host,
      CrossVariantMojoAssociatedReceiver<mojom::blink::WidgetInterfaceBase>
          widget);
  ~WebExternalWidgetImpl() override;

  // WebWidget overrides:
  cc::LayerTreeHost* InitializeCompositing(
      scheduler::WebThreadScheduler* main_thread_scheduler,
      cc::TaskGraphRunner* task_graph_runner,
      bool for_child_local_root_frame,
      const ScreenInfo& screen_info,
      std::unique_ptr<cc::UkmRecorderFactory> ukm_recorder_factory,
      const cc::LayerTreeSettings* settings) override;
  void SetCompositorVisible(bool visible) override;
  void Close(
      scoped_refptr<base::SingleThreadTaskRunner> cleanup_runner) override;
  WebHitTestResult HitTestResultAt(const gfx::PointF&) override;
  WebURL GetURLForDebugTrace() override;
  gfx::Size Size() override;
  void Resize(const gfx::Size& size) override;
  WebInputEventResult HandleInputEvent(
      const WebCoalescedInputEvent& coalesced_event) override;
  WebInputEventResult DispatchBufferedTouchEvents() override;
  scheduler::WebRenderWidgetSchedulingState* RendererWidgetSchedulingState()
      override;
  void SetCursor(const ui::Cursor& cursor) override;
  bool HandlingInputEvent() override;
  void SetHandlingInputEvent(bool handling) override;
  void ProcessInputEventSynchronouslyForTesting(const WebCoalescedInputEvent&,
                                                HandledEventCallback) override;
  void DidOverscrollForTesting(
      const gfx::Vector2dF& overscroll_delta,
      const gfx::Vector2dF& accumulated_overscroll,
      const gfx::PointF& position_in_viewport,
      const gfx::Vector2dF& velocity_in_viewport) override;
  void UpdateTextInputState() override;
  void UpdateSelectionBounds() override;
  void ShowVirtualKeyboard() override;
  bool HasFocus() override;
  void SetFocus(bool focus) override;
  void RequestMouseLock(
      bool has_transient_user_activation,
      bool priviledged,
      bool request_unadjusted_movement,
      base::OnceCallback<
          void(mojom::blink::PointerLockResult,
               CrossVariantMojoRemote<
                   mojom::blink::PointerLockContextInterfaceBase>)>) override;
#if defined(OS_ANDROID)
  SynchronousCompositorRegistry* GetSynchronousCompositorRegistry() override;
#endif
  void ApplyVisualProperties(
      const VisualProperties& visual_properties) override;
  const ScreenInfo& GetScreenInfo() override;
  gfx::Rect WindowRect() override;
  gfx::Rect ViewRect() override;
  void SetScreenRects(const gfx::Rect& widget_screen_rect,
                      const gfx::Rect& window_screen_rect) override;
  gfx::Size VisibleViewportSizeInDIPs() override;
  void SetPendingWindowRect(const gfx::Rect* window_screen_rect) override;
  bool IsHidden() const override;

  // WebExternalWidget overrides:
  void SetRootLayer(scoped_refptr<cc::Layer>) override;

  // WidgetBaseClient overrides:
  void BeginMainFrame(base::TimeTicks last_frame_time) override {}
  void RecordTimeToFirstActivePaint(base::TimeDelta duration) override;
  void UpdateLifecycle(WebLifecycleUpdate requested_update,
                       DocumentUpdateReason reason) override {}
  void RequestNewLayerTreeFrameSink(
      LayerTreeFrameSinkCallback callback) override;
  void DidCommitAndDrawCompositorFrame() override;
  bool WillHandleGestureEvent(const WebGestureEvent& event) override;
  bool WillHandleMouseEvent(const WebMouseEvent& event) override;
  void ObserveGestureEventAndResult(
      const WebGestureEvent& gesture_event,
      const gfx::Vector2dF& unused_delta,
      const cc::OverscrollBehavior& overscroll_behavior,
      bool event_processed) override;
  bool SupportsBufferedTouchEvents() override;
  void FlushInputProcessedCallback() override;
  void CancelCompositionForPepper() override;
  void UpdateVisualProperties(
      const VisualProperties& visual_properties) override;
  const ScreenInfo& GetOriginalScreenInfo() override;
  gfx::Rect ViewportVisibleRect() override;

 private:
  WebExternalWidgetClient* const client_;
  const WebURL debug_url_;
  gfx::Size size_;
  std::unique_ptr<WidgetBase> widget_base_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_WEB_EXTERNAL_WIDGET_IMPL_H_
