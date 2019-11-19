// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_INPUT_WIDGET_INPUT_HANDLER_MANAGER_H_
#define CONTENT_RENDERER_INPUT_WIDGET_INPUT_HANDLER_MANAGER_H_

#include <atomic>
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/common/input/input_handler.mojom.h"
#include "content/renderer/render_frame_impl.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "ui/events/blink/input_handler_proxy.h"
#include "ui/events/blink/input_handler_proxy_client.h"

namespace blink {
namespace scheduler {
class WebThreadScheduler;
}  // namespace scheduler
}  // namespace blink

namespace gfx {
struct PresentationFeedback;
}  // namespace gfx

namespace content {
class MainThreadEventQueue;
class SynchronousCompositorRegistry;
class SynchronousCompositorProxyRegistry;

// This class maintains the compositor InputHandlerProxy and is
// responsible for passing input events on the compositor and main threads.
// The lifecycle of this object matches that of the RenderWidget.
class CONTENT_EXPORT WidgetInputHandlerManager final
    : public base::RefCountedThreadSafe<WidgetInputHandlerManager>,
      public ui::InputHandlerProxyClient,
      public base::SupportsWeakPtr<WidgetInputHandlerManager> {
  // Used in UMA metrics reporting. Do not re-order, and rename the metric if
  // additional states are required.
  enum class InitialInputTiming {
    // Input comes before lifecycle update
    kBeforeLifecycle = 0,
    // Input is before commit
    kBeforeCommit = 1,
    // Input comes only after commit
    kAfterCommit = 2,
    kMaxValue = kAfterCommit
  };

  // For use in bitfields to keep track of what, if anything, the rendering
  // pipeline is currently deferring. Input is suppressed if anything is
  // being deferred, and we use the combination of states to correctly report
  // UMA for input that is suppressed.
  enum class RenderingDeferralBits {
    kDeferMainFrameUpdates = 1,
    kDeferCommits = 2
  };

 public:
  static scoped_refptr<WidgetInputHandlerManager> Create(
      base::WeakPtr<RenderWidget> render_widget,
      scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
      blink::scheduler::WebThreadScheduler* main_thread_scheduler,
      bool needs_input_handler);
  void AddAssociatedInterface(
      mojo::PendingAssociatedReceiver<mojom::WidgetInputHandler> receiver,
      mojo::PendingRemote<mojom::WidgetInputHandlerHost> host);

  void AddInterface(mojo::PendingReceiver<mojom::WidgetInputHandler> receiver,
                    mojo::PendingRemote<mojom::WidgetInputHandlerHost> host);

  // InputHandlerProxyClient overrides.
  void WillShutdown() override;
  void DispatchNonBlockingEventToMainThread(
      ui::WebScopedInputEvent event,
      const ui::LatencyInfo& latency_info) override;

  void DidOverscroll(
      const gfx::Vector2dF& accumulated_overscroll,
      const gfx::Vector2dF& latest_overscroll_delta,
      const gfx::Vector2dF& current_fling_velocity,
      const gfx::PointF& causal_event_viewport_point,
      const cc::OverscrollBehavior& overscroll_behavior) override;
  void DidAnimateForInput() override;
  void DidStartScrollingViewport() override;
  void GenerateScrollBeginAndSendToMainThread(
      const blink::WebGestureEvent& update_event) override;
  void SetWhiteListedTouchAction(
      cc::TouchAction touch_action,
      uint32_t unique_touch_event_id,
      ui::InputHandlerProxy::EventDisposition event_disposition) override;

  void ObserveGestureEventOnMainThread(
      const blink::WebGestureEvent& gesture_event,
      const cc::InputHandlerScrollResult& scroll_result);

  void DispatchEvent(std::unique_ptr<content::InputEvent> event,
                     mojom::WidgetInputHandler::DispatchEventCallback callback);

  void ProcessTouchAction(cc::TouchAction touch_action);

  mojom::WidgetInputHandlerHost* GetWidgetInputHandlerHost();

  void AttachSynchronousCompositor(
      mojo::PendingRemote<mojom::SynchronousCompositorControlHost> control_host,
      mojo::PendingAssociatedRemote<mojom::SynchronousCompositorHost> host,
      mojo::PendingAssociatedReceiver<mojom::SynchronousCompositor>
          compositor_request);

#if defined(OS_ANDROID)
  content::SynchronousCompositorRegistry* GetSynchronousCompositorRegistry();
#endif

  void InvokeInputProcessedCallback();
  void InputWasProcessed(const gfx::PresentationFeedback& feedback);
  void WaitForInputProcessed(base::OnceClosure callback);

  void FallbackCursorModeLockCursor(bool left, bool right, bool up, bool down);
  void FallbackCursorModeSetCursorVisibility(bool visible);

  // Called when the RenderWidget is notified of a navigation. Resets
  // the renderer pipeline deferral status, and resets the UMA recorder for
  // time of first input.
  void DidNavigate();

  // Called to inform us when the system starts or stops main frame updates.
  void OnDeferMainFrameUpdatesChanged(bool);

  // Called to inform us when the system starts or stops deferring commits.
  void OnDeferCommitsChanged(bool);

  // Allow tests, headless etc. to have input events processed before the
  // compositor is ready to commit frames.
  // TODO(schenney): Fix this somehow, forcing all tests to wait for
  // hit test regions.
  void AllowPreCommitInput() { allow_pre_commit_input_ = true; }

 protected:
  friend class base::RefCountedThreadSafe<WidgetInputHandlerManager>;
  ~WidgetInputHandlerManager() override;

 private:
  WidgetInputHandlerManager(
      base::WeakPtr<RenderWidget> render_widget,
      scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
      blink::scheduler::WebThreadScheduler* main_thread_scheduler);
  void InitInputHandler();
  void InitOnInputHandlingThread(
      const base::WeakPtr<cc::InputHandler>& input_handler,
      bool smooth_scroll_enabled,
      bool sync_compositing);
  void BindAssociatedChannel(
      mojo::PendingAssociatedReceiver<mojom::WidgetInputHandler> receiver);
  void BindChannel(mojo::PendingReceiver<mojom::WidgetInputHandler> receiver);
  void HandleInputEvent(
      const ui::WebScopedInputEvent& event,
      const ui::LatencyInfo& latency,
      mojom::WidgetInputHandler::DispatchEventCallback callback);
  void DidHandleInputEventAndOverscroll(
      mojom::WidgetInputHandler::DispatchEventCallback callback,
      ui::InputHandlerProxy::EventDisposition event_disposition,
      ui::WebScopedInputEvent input_event,
      const ui::LatencyInfo& latency_info,
      std::unique_ptr<ui::DidOverscrollParams> overscroll_params);
  void HandledInputEvent(
      mojom::WidgetInputHandler::DispatchEventCallback callback,
      InputEventAckState ack_state,
      const ui::LatencyInfo& latency_info,
      std::unique_ptr<ui::DidOverscrollParams> overscroll_params,
      base::Optional<cc::TouchAction> touch_action);
  void ObserveGestureEventOnInputHandlingThread(
      const blink::WebGestureEvent& gesture_event,
      const cc::InputHandlerScrollResult& scroll_result);

  // Returns the task runner for the thread that receives input. i.e. the
  // "Mojo-bound" thread.
  const scoped_refptr<base::SingleThreadTaskRunner>& InputThreadTaskRunner()
      const;

  void LogInputTimingUMA();

  // Only valid to be called on the main thread.
  base::WeakPtr<RenderWidget> render_widget_;
  blink::scheduler::WebThreadScheduler* main_thread_scheduler_;

  // InputHandlerProxy is only interacted with on the compositor
  // thread.
  std::unique_ptr<ui::InputHandlerProxy> input_handler_proxy_;

  // The WidgetInputHandlerHost is bound on the compositor task runner
  // but class can be called on the compositor and main thread.
  mojo::SharedRemote<mojom::WidgetInputHandlerHost> host_;

  // Host that was passed as part of the FrameInputHandler associated
  // channel.
  mojo::SharedRemote<mojom::WidgetInputHandlerHost> associated_host_;

  // Any thread can access these variables.
  scoped_refptr<MainThreadEventQueue> input_event_queue_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;

  base::Optional<cc::TouchAction> white_listed_touch_action_;

  // Callback used to respond to the WaitForInputProcessed Mojo message. This
  // callback is set from and must be invoked from the Mojo-bound thread (i.e.
  // the InputThreadTaskRunner thread), it will invoke the Mojo reply.
  base::OnceClosure input_processed_callback_;

  // Whether this widget uses an InputHandler or forwards all input to the
  // WebWidget (Popups, Plugins).
  bool uses_input_handler_ = false;

  // State tracking which parts of the rendering pipeline are currently
  // deferred. We use this state to suppress all events until the user can see
  // the content; that is, while rendering stages are being deferred and
  // this value is zero.
  // Move events are still processed to allow tracking of mouse position.
  // Metrics also report the lifecycle state when the first non-move event is
  // seen.
  // This is a bitfield, using the bit values from RenderingDeferralBits.
  // The compositor thread accesses this value when processing input (to decide
  // whether to suppress input) and the renderer thread accesses it when the
  // status of deferrals changes, so it needs to be thread safe.
  std::atomic<uint16_t> renderer_deferral_state_{0};

  // Allow input suppression to be disabled for tests and non-browser uses
  // of chromium that do not wait for the first commit, or that may never
  // commit. Over time, tests should be fixed so they provide additional
  // coverage for input suppression: crbug.com/987626
  bool allow_pre_commit_input_ = false;

  // Control of UMA. We emit one UMA metric per navigation telling us
  // whether any non-move input arrived before we starting updating the page or
  // displaying content to the user.
  bool have_emitted_uma_ = false;

#if defined(OS_ANDROID)
  std::unique_ptr<SynchronousCompositorProxyRegistry>
      synchronous_compositor_registry_;
#endif

  DISALLOW_COPY_AND_ASSIGN(WidgetInputHandlerManager);
};

}  // namespace content

#endif  // CONTENT_RENDERER_INPUT_WIDGET_INPUT_HANDLER_MANAGER_H_
