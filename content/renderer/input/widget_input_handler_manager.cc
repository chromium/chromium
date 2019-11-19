// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/input/widget_input_handler_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "content/common/input_messages.h"
#include "content/renderer/compositor/layer_tree_view.h"
#include "content/renderer/ime_event_guard.h"
#include "content/renderer/input/widget_input_handler_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_widget.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/platform/web_coalesced_input_event.h"
#include "third_party/blink/public/platform/web_keyboard_event.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "ui/events/base_event_utils.h"

#if defined(OS_ANDROID)
#include "content/public/common/content_client.h"
#include "content/renderer/input/synchronous_compositor_proxy.h"
#include "content/renderer/input/synchronous_compositor_registry.h"
#endif

namespace content {
namespace {
void CallCallback(mojom::WidgetInputHandler::DispatchEventCallback callback,
                  InputEventAckState ack_state,
                  const ui::LatencyInfo& latency_info,
                  std::unique_ptr<ui::DidOverscrollParams> overscroll_params,
                  base::Optional<cc::TouchAction> touch_action) {
  std::move(callback).Run(
      InputEventAckSource::MAIN_THREAD, latency_info, ack_state,
      overscroll_params
          ? base::Optional<ui::DidOverscrollParams>(*overscroll_params)
          : base::nullopt,
      touch_action);
}

InputEventAckState InputEventDispositionToAck(
    ui::InputHandlerProxy::EventDisposition disposition) {
  switch (disposition) {
    case ui::InputHandlerProxy::DID_HANDLE:
      return INPUT_EVENT_ACK_STATE_CONSUMED;
    case ui::InputHandlerProxy::DID_NOT_HANDLE:
      return INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
    case ui::InputHandlerProxy::DID_NOT_HANDLE_NON_BLOCKING_DUE_TO_FLING:
      return INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING_DUE_TO_FLING;
    case ui::InputHandlerProxy::DROP_EVENT:
      return INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS;
    case ui::InputHandlerProxy::DID_HANDLE_NON_BLOCKING:
      return INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING;
    case ui::InputHandlerProxy::DID_HANDLE_SHOULD_BUBBLE:
      return INPUT_EVENT_ACK_STATE_CONSUMED_SHOULD_BUBBLE;
  }
  NOTREACHED();
  return INPUT_EVENT_ACK_STATE_UNKNOWN;
}

}  // namespace

#if defined(OS_ANDROID)
class SynchronousCompositorProxyRegistry
    : public SynchronousCompositorRegistry {
 public:
  explicit SynchronousCompositorProxyRegistry(
      scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner)
      : compositor_task_runner_(std::move(compositor_task_runner)) {}

  ~SynchronousCompositorProxyRegistry() override {
    // Ensure the proxy has already been release on the compositor thread
    // before destroying this object.
    DCHECK(!proxy_);
  }

  void CreateProxy(ui::SynchronousInputHandlerProxy* handler) {
    DCHECK(compositor_task_runner_->BelongsToCurrentThread());
    proxy_ = std::make_unique<SynchronousCompositorProxy>(handler);
    proxy_->Init();

    if (sink_)
      proxy_->SetLayerTreeFrameSink(sink_);
  }

  SynchronousCompositorProxy* proxy() { return proxy_.get(); }

  void RegisterLayerTreeFrameSink(
      int routing_id,
      SynchronousLayerTreeFrameSink* layer_tree_frame_sink) override {
    DCHECK(compositor_task_runner_->BelongsToCurrentThread());
    DCHECK_EQ(nullptr, sink_);
    sink_ = layer_tree_frame_sink;
    if (proxy_)
      proxy_->SetLayerTreeFrameSink(layer_tree_frame_sink);
  }

  void UnregisterLayerTreeFrameSink(
      int routing_id,
      SynchronousLayerTreeFrameSink* layer_tree_frame_sink) override {
    DCHECK(compositor_task_runner_->BelongsToCurrentThread());
    DCHECK_EQ(layer_tree_frame_sink, sink_);
    sink_ = nullptr;
  }

  void DestroyProxy() {
    DCHECK(compositor_task_runner_->BelongsToCurrentThread());
    proxy_.reset();
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;
  std::unique_ptr<SynchronousCompositorProxy> proxy_;
  SynchronousLayerTreeFrameSink* sink_ = nullptr;
};

#endif

scoped_refptr<WidgetInputHandlerManager> WidgetInputHandlerManager::Create(
    base::WeakPtr<RenderWidget> render_widget,
    scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
    blink::scheduler::WebThreadScheduler* main_thread_scheduler,
    bool uses_input_handler) {
  scoped_refptr<WidgetInputHandlerManager> manager =
      new WidgetInputHandlerManager(std::move(render_widget),
                                    std::move(compositor_task_runner),
                                    main_thread_scheduler);
  if (uses_input_handler)
    manager->InitInputHandler();

  return manager;
}

WidgetInputHandlerManager::WidgetInputHandlerManager(
    base::WeakPtr<RenderWidget> render_widget,
    scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
    blink::scheduler::WebThreadScheduler* main_thread_scheduler)
    : render_widget_(render_widget),
      main_thread_scheduler_(main_thread_scheduler),
      input_event_queue_(render_widget->GetInputEventQueue()),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      compositor_task_runner_(std::move(compositor_task_runner)) {
#if defined(OS_ANDROID)
  if (compositor_task_runner_) {
    synchronous_compositor_registry_ =
        std::make_unique<SynchronousCompositorProxyRegistry>(
            compositor_task_runner_);
  }
#endif
}

void WidgetInputHandlerManager::InitInputHandler() {
  bool sync_compositing = false;
#if defined(OS_ANDROID)
  sync_compositing = GetContentClient()->UsingSynchronousCompositing();
#endif
  uses_input_handler_ = true;
  base::OnceClosure init_closure = base::BindOnce(
      &WidgetInputHandlerManager::InitOnInputHandlingThread, this,
      render_widget_->layer_tree_host()->GetInputHandler(),
      render_widget_->compositor_deps()->IsScrollAnimatorEnabled(),
      sync_compositing);
  InputThreadTaskRunner()->PostTask(FROM_HERE, std::move(init_closure));
}

WidgetInputHandlerManager::~WidgetInputHandlerManager() = default;

void WidgetInputHandlerManager::AddAssociatedInterface(
    mojo::PendingAssociatedReceiver<mojom::WidgetInputHandler> receiver,
    mojo::PendingRemote<mojom::WidgetInputHandlerHost> host) {
  if (compositor_task_runner_) {
    associated_host_ = mojo::SharedRemote<mojom::WidgetInputHandlerHost>(
        std::move(host), compositor_task_runner_);
    // Mojo channel bound on compositor thread.
    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WidgetInputHandlerManager::BindAssociatedChannel, this,
                       std::move(receiver)));
  } else {
    associated_host_ =
        mojo::SharedRemote<mojom::WidgetInputHandlerHost>(std::move(host));
    // Mojo channel bound on main thread.
    BindAssociatedChannel(std::move(receiver));
  }
}

void WidgetInputHandlerManager::AddInterface(
    mojo::PendingReceiver<mojom::WidgetInputHandler> receiver,
    mojo::PendingRemote<mojom::WidgetInputHandlerHost> host) {
  if (compositor_task_runner_) {
    host_ = mojo::SharedRemote<mojom::WidgetInputHandlerHost>(
        std::move(host), compositor_task_runner_);
    // Mojo channel bound on compositor thread.
    compositor_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&WidgetInputHandlerManager::BindChannel, this,
                                  std::move(receiver)));
  } else {
    host_ = mojo::SharedRemote<mojom::WidgetInputHandlerHost>(std::move(host));
    // Mojo channel bound on main thread.
    BindChannel(std::move(receiver));
  }
}

void WidgetInputHandlerManager::WillShutdown() {
#if defined(OS_ANDROID)
  if (synchronous_compositor_registry_)
    synchronous_compositor_registry_->DestroyProxy();
#endif
  input_handler_proxy_.reset();
}

void WidgetInputHandlerManager::DispatchNonBlockingEventToMainThread(
    ui::WebScopedInputEvent event,
    const ui::LatencyInfo& latency_info) {
  DCHECK(input_event_queue_);
  input_event_queue_->HandleEvent(
      std::move(event), latency_info, DISPATCH_TYPE_NON_BLOCKING,
      INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING, HandledEventCallback());
}

void WidgetInputHandlerManager::DidOverscroll(
    const gfx::Vector2dF& accumulated_overscroll,
    const gfx::Vector2dF& latest_overscroll_delta,
    const gfx::Vector2dF& current_fling_velocity,
    const gfx::PointF& causal_event_viewport_point,
    const cc::OverscrollBehavior& overscroll_behavior) {
  mojom::WidgetInputHandlerHost* host = GetWidgetInputHandlerHost();
  if (!host)
    return;
  ui::DidOverscrollParams params;
  params.accumulated_overscroll = accumulated_overscroll;
  params.latest_overscroll_delta = latest_overscroll_delta;
  params.current_fling_velocity = current_fling_velocity;
  params.causal_event_viewport_point = causal_event_viewport_point;
  params.overscroll_behavior = overscroll_behavior;
  host->DidOverscroll(params);
}

void WidgetInputHandlerManager::DidAnimateForInput() {
  main_thread_scheduler_->DidAnimateForInputOnCompositorThread();
}

void WidgetInputHandlerManager::DidStartScrollingViewport() {
  mojom::WidgetInputHandlerHost* host = GetWidgetInputHandlerHost();
  if (!host)
    return;
  host->DidStartScrollingViewport();
}

void WidgetInputHandlerManager::GenerateScrollBeginAndSendToMainThread(
    const blink::WebGestureEvent& update_event) {
  DCHECK_EQ(update_event.GetType(), blink::WebInputEvent::kGestureScrollUpdate);
  blink::WebGestureEvent scroll_begin =
      ui::ScrollBeginFromScrollUpdate(update_event);

  DispatchNonBlockingEventToMainThread(
      ui::WebInputEventTraits::Clone(scroll_begin), ui::LatencyInfo());
}

void WidgetInputHandlerManager::SetWhiteListedTouchAction(
    cc::TouchAction touch_action,
    uint32_t unique_touch_event_id,
    ui::InputHandlerProxy::EventDisposition event_disposition) {
  if (base::FeatureList::IsEnabled(features::kCompositorTouchAction)) {
    white_listed_touch_action_ = touch_action;
    return;
  }
  mojom::WidgetInputHandlerHost* host = GetWidgetInputHandlerHost();
  if (!host)
    return;
  InputEventAckState ack_state = InputEventDispositionToAck(event_disposition);
  host->SetWhiteListedTouchAction(touch_action, unique_touch_event_id,
                                  ack_state);
}

void WidgetInputHandlerManager::ProcessTouchAction(
    cc::TouchAction touch_action) {
  if (mojom::WidgetInputHandlerHost* host = GetWidgetInputHandlerHost())
    host->SetTouchActionFromMain(touch_action);
}

mojom::WidgetInputHandlerHost*
WidgetInputHandlerManager::GetWidgetInputHandlerHost() {
  if (associated_host_)
    return associated_host_.get();
  if (host_)
    return host_.get();
  return nullptr;
}

void WidgetInputHandlerManager::AttachSynchronousCompositor(
    mojo::PendingRemote<mojom::SynchronousCompositorControlHost> control_host,
    mojo::PendingAssociatedRemote<mojom::SynchronousCompositorHost> host,
    mojo::PendingAssociatedReceiver<mojom::SynchronousCompositor>
        compositor_request) {
#if defined(OS_ANDROID)
  DCHECK(synchronous_compositor_registry_);
  synchronous_compositor_registry_->proxy()->BindChannel(
      std::move(control_host), std::move(host), std::move(compositor_request));
#endif
}

void WidgetInputHandlerManager::ObserveGestureEventOnMainThread(
    const blink::WebGestureEvent& gesture_event,
    const cc::InputHandlerScrollResult& scroll_result) {
  base::OnceClosure observe_gesture_event_closure = base::BindOnce(
      &WidgetInputHandlerManager::ObserveGestureEventOnInputHandlingThread,
      this, gesture_event, scroll_result);
  InputThreadTaskRunner()->PostTask(FROM_HERE,
                                    std::move(observe_gesture_event_closure));
}

void WidgetInputHandlerManager::LogInputTimingUMA() {
  if (!have_emitted_uma_) {
    InitialInputTiming lifecycle_state = InitialInputTiming::kBeforeLifecycle;
    if (!(renderer_deferral_state_ &
          (unsigned)RenderingDeferralBits::kDeferMainFrameUpdates)) {
      if (renderer_deferral_state_ &
          (unsigned)RenderingDeferralBits::kDeferCommits) {
        lifecycle_state = InitialInputTiming::kBeforeCommit;
      } else {
        lifecycle_state = InitialInputTiming::kAfterCommit;
      }
    }
    UMA_HISTOGRAM_ENUMERATION("PaintHolding.InputTiming2", lifecycle_state);
    have_emitted_uma_ = true;
  }
}

void WidgetInputHandlerManager::DispatchEvent(
    std::unique_ptr<content::InputEvent> event,
    mojom::WidgetInputHandler::DispatchEventCallback callback) {
  if (!event || !event->web_event) {
    // Call |callback| if it was available indicating this event wasn't
    // handled.
    if (callback) {
      std::move(callback).Run(
          InputEventAckSource::MAIN_THREAD, ui::LatencyInfo(),
          INPUT_EVENT_ACK_STATE_NOT_CONSUMED, base::nullopt, base::nullopt);
    }
    return;
  }

  bool event_is_move =
      event->web_event->GetType() == WebInputEvent::Type::kMouseMove ||
      event->web_event->GetType() == WebInputEvent::Type::kPointerMove;
  if (!event_is_move)
    LogInputTimingUMA();

  // Drop input if we are deferring a rendring pipeline phase, unless it's a
  // move event.
  // We don't want users interacting with stuff they can't see, so we drop it.
  // We allow moves because we need to keep the current pointer location up
  // to date. Tests and other code can allow pre-commit input through the
  // "allow-pre-commit-input" command line flag.
  // TODO(schenney): Also allow scrolls? This would make some tests not flaky,
  // it seems, because they sometimes crash on seeing a scroll update/end
  // without a begin. Scrolling, pinch-zoom etc. don't seem dangerous.
  if (renderer_deferral_state_ && !allow_pre_commit_input_ && !event_is_move) {
    if (callback) {
      std::move(callback).Run(
          InputEventAckSource::MAIN_THREAD, ui::LatencyInfo(),
          INPUT_EVENT_ACK_STATE_NOT_CONSUMED, base::nullopt, base::nullopt);
    }
    return;
  }

  // If TimeTicks is not consistent across processes we cannot use the event's
  // platform timestamp in this process. Instead use the time that the event is
  // received as the event's timestamp.
  if (!base::TimeTicks::IsConsistentAcrossProcesses()) {
    event->web_event->SetTimeStamp(base::TimeTicks::Now());
  }

  if (uses_input_handler_) {
    // If the input_handler_proxy has disappeared ensure we just ack event.
    if (!input_handler_proxy_) {
      if (callback) {
        std::move(callback).Run(
            InputEventAckSource::MAIN_THREAD, ui::LatencyInfo(),
            INPUT_EVENT_ACK_STATE_NOT_CONSUMED, base::nullopt, base::nullopt);
      }
      return;
    }
    input_handler_proxy_->HandleInputEventWithLatencyInfo(
        std::move(event->web_event), event->latency_info,
        base::BindOnce(
            &WidgetInputHandlerManager::DidHandleInputEventAndOverscroll, this,
            std::move(callback)));
  } else {
    HandleInputEvent(std::move(event->web_event), event->latency_info,
                     std::move(callback));
  }
}

void WidgetInputHandlerManager::InvokeInputProcessedCallback() {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  // We can call this method even if we didn't request a callback (e.g. when
  // the renderer becomes hidden).
  if (!input_processed_callback_)
    return;

  // The handler's method needs to respond to the mojo message so it needs to
  // run on the input handling thread.  Even if we're already on the correct
  // thread, we PostTask for symmetry.
  InputThreadTaskRunner()->PostTask(FROM_HERE,
                                    std::move(input_processed_callback_));
}

void WidgetInputHandlerManager::InputWasProcessed(
    const gfx::PresentationFeedback&) {
  InvokeInputProcessedCallback();
}

static void WaitForInputProcessedFromMain(
    base::WeakPtr<RenderWidget> render_widget) {
  // If the widget is destroyed while we're posting to the main thread, the
  // Mojo message will be acked in WidgetInputHandlerImpl's destructor.
  if (!render_widget)
    return;

  WidgetInputHandlerManager* manager =
      render_widget->widget_input_handler_manager();

  // TODO(bokan): Temporary to unblock synthetic gesture events running under
  // VR. https://crbug.com/940063
  bool ack_immediately =
      render_widget->delegate()->ShouldAckSyntheticInputImmediately();

  // If the RenderWidget is hidden, we won't produce compositor frames for it
  // so just ACK the input to prevent blocking the browser indefinitely.
  if (render_widget->is_hidden() || ack_immediately) {
    manager->InvokeInputProcessedCallback();
    return;
  }

  auto redraw_complete_callback = base::BindOnce(
      &WidgetInputHandlerManager::InputWasProcessed, manager->AsWeakPtr());

  // We consider all observable effects of an input gesture to be processed
  // when the CompositorFrame caused by that input has been produced, send, and
  // displayed. RequestPresentation will force a a commit and redraw and
  // callback when the CompositorFrame has been displayed in the display
  // service. Some examples of non-trivial effects that require waiting that
  // long: committing NonFastScrollRegions to the compositor, sending
  // touch-action rects to the browser, and sending updated surface information
  // to the display compositor for up-to-date OOPIF hit-testing.
  render_widget->RequestPresentation(std::move(redraw_complete_callback));
}

void WidgetInputHandlerManager::WaitForInputProcessed(
    base::OnceClosure callback) {
  // Note, this will be called from the mojo-bound thread which could be either
  // main or compositor.
  DCHECK(!input_processed_callback_);
  input_processed_callback_ = std::move(callback);

  // We mustn't touch render_widget_ from the impl thread so post all the setup
  // to the main thread.
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WaitForInputProcessedFromMain, render_widget_));
}

void WidgetInputHandlerManager::FallbackCursorModeLockCursor(bool left,
                                                             bool right,
                                                             bool up,
                                                             bool down) {
#if defined(OS_ANDROID)
  if (mojom::WidgetInputHandlerHost* host = GetWidgetInputHandlerHost())
    host->FallbackCursorModeLockCursor(left, right, up, down);
#endif
}

void WidgetInputHandlerManager::FallbackCursorModeSetCursorVisibility(
    bool visible) {
#if defined(OS_ANDROID)
  if (mojom::WidgetInputHandlerHost* host = GetWidgetInputHandlerHost())
    host->FallbackCursorModeSetCursorVisibility(visible);
#endif
}

void WidgetInputHandlerManager::DidNavigate() {
  renderer_deferral_state_ = 0;
  have_emitted_uma_ = false;
}

void WidgetInputHandlerManager::OnDeferMainFrameUpdatesChanged(bool status) {
  if (status) {
    renderer_deferral_state_ |=
        static_cast<uint16_t>(RenderingDeferralBits::kDeferMainFrameUpdates);
  } else {
    renderer_deferral_state_ &=
        ~static_cast<uint16_t>(RenderingDeferralBits::kDeferMainFrameUpdates);
  }
}

void WidgetInputHandlerManager::OnDeferCommitsChanged(bool status) {
  if (status) {
    renderer_deferral_state_ |=
        static_cast<uint16_t>(RenderingDeferralBits::kDeferCommits);
  } else {
    renderer_deferral_state_ &=
        ~static_cast<uint16_t>(RenderingDeferralBits::kDeferCommits);
  }
}

void WidgetInputHandlerManager::InitOnInputHandlingThread(
    const base::WeakPtr<cc::InputHandler>& input_handler,
    bool smooth_scroll_enabled,
    bool sync_compositing) {
  DCHECK(InputThreadTaskRunner()->BelongsToCurrentThread());

  // It is possible that the input_handle has already been destroyed before this
  // Init() call was invoked. If so, early out.
  if (!input_handler)
    return;

  // If there's no compositor thread (i.e. we're in a LayoutTest), force input
  // to go through the main thread.
  bool force_input_handling_on_main = !compositor_task_runner_;

  input_handler_proxy_ = std::make_unique<ui::InputHandlerProxy>(
      input_handler.get(), this, force_input_handling_on_main);

  input_handler_proxy_->set_smooth_scroll_enabled(smooth_scroll_enabled);

#if defined(OS_ANDROID)
  if (sync_compositing) {
    DCHECK(synchronous_compositor_registry_);
    synchronous_compositor_registry_->CreateProxy(input_handler_proxy_.get());
  }
#endif
}

void WidgetInputHandlerManager::BindAssociatedChannel(
    mojo::PendingAssociatedReceiver<mojom::WidgetInputHandler> receiver) {
  if (!receiver.is_valid())
    return;
  // Don't pass the |input_event_queue_| on if we don't have a
  // |compositor_task_runner_| as events might get out of order.
  WidgetInputHandlerImpl* handler = new WidgetInputHandlerImpl(
      this, main_thread_task_runner_,
      compositor_task_runner_ ? input_event_queue_ : nullptr, render_widget_);
  handler->SetAssociatedReceiver(std::move(receiver));
}

void WidgetInputHandlerManager::BindChannel(
    mojo::PendingReceiver<mojom::WidgetInputHandler> receiver) {
  if (!receiver.is_valid())
    return;
  // Don't pass the |input_event_queue_| on if we don't have a
  // |compositor_task_runner_| as events might get out of order.
  WidgetInputHandlerImpl* handler = new WidgetInputHandlerImpl(
      this, main_thread_task_runner_,
      compositor_task_runner_ ? input_event_queue_ : nullptr, render_widget_);
  handler->SetReceiver(std::move(receiver));
}

void WidgetInputHandlerManager::HandleInputEvent(
    const ui::WebScopedInputEvent& event,
    const ui::LatencyInfo& latency,
    mojom::WidgetInputHandler::DispatchEventCallback callback) {
  if (!render_widget_ || render_widget_->IsUndeadOrProvisional()) {
    if (callback) {
      std::move(callback).Run(InputEventAckSource::MAIN_THREAD, latency,
                              INPUT_EVENT_ACK_STATE_NOT_CONSUMED, base::nullopt,
                              base::nullopt);
    }
    return;
  }
  auto send_callback = base::BindOnce(
      &WidgetInputHandlerManager::HandledInputEvent, this, std::move(callback));

  blink::WebCoalescedInputEvent coalesced_event(*event);
  render_widget_->HandleInputEvent(coalesced_event, latency,
                                   std::move(send_callback));
}

void WidgetInputHandlerManager::DidHandleInputEventAndOverscroll(
    mojom::WidgetInputHandler::DispatchEventCallback callback,
    ui::InputHandlerProxy::EventDisposition event_disposition,
    ui::WebScopedInputEvent input_event,
    const ui::LatencyInfo& latency_info,
    std::unique_ptr<ui::DidOverscrollParams> overscroll_params) {
  TRACE_EVENT1("input",
               "WidgetInputHandlerManager::DidHandleInputEventAndOverscroll",
               "Disposition", event_disposition);

  InputEventAckState ack_state = InputEventDispositionToAck(event_disposition);
  if (ack_state == INPUT_EVENT_ACK_STATE_CONSUMED) {
    main_thread_scheduler_->DidHandleInputEventOnCompositorThread(
        *input_event, blink::scheduler::WebThreadScheduler::InputEventState::
                          EVENT_CONSUMED_BY_COMPOSITOR);
  } else if (MainThreadEventQueue::IsForwardedAndSchedulerKnown(ack_state)) {
    main_thread_scheduler_->DidHandleInputEventOnCompositorThread(
        *input_event, blink::scheduler::WebThreadScheduler::InputEventState::
                          EVENT_FORWARDED_TO_MAIN_THREAD);
  }

  if (ack_state == INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING ||
      ack_state == INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING_DUE_TO_FLING ||
      ack_state == INPUT_EVENT_ACK_STATE_NOT_CONSUMED) {
    DCHECK(!overscroll_params);
    DCHECK(!latency_info.coalesced());
    InputEventDispatchType dispatch_type = callback.is_null()
                                               ? DISPATCH_TYPE_NON_BLOCKING
                                               : DISPATCH_TYPE_BLOCKING;
    HandledEventCallback handled_event =
        base::BindOnce(&WidgetInputHandlerManager::HandledInputEvent, this,
                       std::move(callback));
    input_event_queue_->HandleEvent(std::move(input_event), latency_info,
                                    dispatch_type, ack_state,
                                    std::move(handled_event));
    return;
  }
  if (callback) {
    std::move(callback).Run(
        InputEventAckSource::COMPOSITOR_THREAD, latency_info, ack_state,
        overscroll_params
            ? base::Optional<ui::DidOverscrollParams>(*overscroll_params)
            : base::nullopt,
        base::nullopt);
  }
}

void WidgetInputHandlerManager::HandledInputEvent(
    mojom::WidgetInputHandler::DispatchEventCallback callback,
    InputEventAckState ack_state,
    const ui::LatencyInfo& latency_info,
    std::unique_ptr<ui::DidOverscrollParams> overscroll_params,
    base::Optional<cc::TouchAction> touch_action) {
  if (!callback)
    return;

  TRACE_EVENT1("input", "WidgetInputHandlerManager::HandledInputEvent",
               "ack_state", ack_state);

  if (!touch_action.has_value()) {
    touch_action = white_listed_touch_action_;
    white_listed_touch_action_.reset();
  }
  // This method is called from either the main thread or the compositor thread.
  bool is_compositor_thread = compositor_task_runner_ &&
                              compositor_task_runner_->BelongsToCurrentThread();

  // If there is a compositor task runner and the current thread isn't the
  // compositor thread proxy it over to the compositor thread.
  if (compositor_task_runner_ && !is_compositor_thread) {
    TRACE_EVENT_INSTANT0("input", "PostingToCompositor",
                         TRACE_EVENT_SCOPE_THREAD);
    compositor_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(CallCallback, std::move(callback), ack_state,
                                  latency_info, std::move(overscroll_params),
                                  touch_action));
  } else {
    // Otherwise call the callback immediately.
    std::move(callback).Run(
        is_compositor_thread ? InputEventAckSource::COMPOSITOR_THREAD
                             : InputEventAckSource::MAIN_THREAD,
        latency_info, ack_state,
        overscroll_params
            ? base::Optional<ui::DidOverscrollParams>(*overscroll_params)
            : base::nullopt,
        touch_action);
  }
}

void WidgetInputHandlerManager::ObserveGestureEventOnInputHandlingThread(
    const blink::WebGestureEvent& gesture_event,
    const cc::InputHandlerScrollResult& scroll_result) {
  if (!input_handler_proxy_)
    return;
  DCHECK(input_handler_proxy_->scroll_elasticity_controller());
  input_handler_proxy_->scroll_elasticity_controller()
      ->ObserveGestureEventAndResult(gesture_event, scroll_result);
}

const scoped_refptr<base::SingleThreadTaskRunner>&
WidgetInputHandlerManager::InputThreadTaskRunner() const {
  if (compositor_task_runner_)
    return compositor_task_runner_;
  return main_thread_task_runner_;
}

#if defined(OS_ANDROID)
content::SynchronousCompositorRegistry*
WidgetInputHandlerManager::GetSynchronousCompositorRegistry() {
  DCHECK(synchronous_compositor_registry_);
  return synchronous_compositor_registry_.get();
}
#endif

}  // namespace content
