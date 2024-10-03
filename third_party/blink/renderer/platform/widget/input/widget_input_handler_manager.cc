// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/input/widget_input_handler_manager.h"

#include <utility>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "base/types/optional_ref.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "cc/base/features.h"
#include "cc/input/browser_controls_offset_tags_info.h"
#include "cc/metrics/event_metrics.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/paint_holding_reason.h"
#include "components/viz/common/features.h"
#include "services/tracing/public/cpp/perfetto/flow_event_utils.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/input/web_input_event_attribution.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/scheduler/public/agent_group_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/compositor_thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/widget_scheduler.h"
#include "third_party/blink/renderer/platform/widget/frame_widget.h"
#include "third_party/blink/renderer/platform/widget/input/elastic_overscroll_controller.h"
#include "third_party/blink/renderer/platform/widget/input/main_thread_event_queue.h"
#include "third_party/blink/renderer/platform/widget/input/widget_input_handler_impl.h"
#include "third_party/blink/renderer/platform/widget/widget_base.h"
#include "third_party/blink/renderer/platform/widget/widget_base_client.h"
#include "ui/latency/latency_info.h"

#if BUILDFLAG(IS_ANDROID)
#include "third_party/blink/renderer/platform/widget/compositing/android_webview/synchronous_compositor_registry.h"
#include "third_party/blink/renderer/platform/widget/input/synchronous_compositor_proxy.h"
#endif

namespace blink {

using ::perfetto::protos::pbzero::ChromeLatencyInfo2;
using ::perfetto::protos::pbzero::TrackEvent;

namespace {
// We will count dropped pointerdown by posting a task in the main thread.
// To avoid blocking the main thread, we need a timer to send the data
// intermittently. The time delay of the timer is 10X of the threshold of
// long tasks which block the main thread 50 ms or longer.
const base::TimeDelta kEventCountsTimerDelay = base::Milliseconds(500);

// The 99th percentile of the delay between navigation start and first paint is
// around 10sec on most platforms.  We are setting the max acceptable limit to
// 1.5x to avoid false positives on slow devices.
const base::TimeDelta kFirstPaintMaxAcceptableDelay = base::Seconds(15);

mojom::blink::DidOverscrollParamsPtr ToDidOverscrollParams(
    const InputHandlerProxy::DidOverscrollParams* overscroll_params) {
  if (!overscroll_params)
    return nullptr;
  return mojom::blink::DidOverscrollParams::New(
      overscroll_params->accumulated_overscroll,
      overscroll_params->latest_overscroll_delta,
      overscroll_params->current_fling_velocity,
      overscroll_params->causal_event_viewport_point,
      overscroll_params->overscroll_behavior);
}

void CallCallback(
    mojom::blink::WidgetInputHandler::DispatchEventCallback callback,
    mojom::blink::InputEventResultState result_state,
    const ui::LatencyInfo& latency_info,
    mojom::blink::DidOverscrollParamsPtr overscroll_params,
    std::optional<cc::TouchAction> touch_action) {
  int64_t trace_id = latency_info.trace_id();
  TRACE_EVENT("input,benchmark,latencyInfo", "LatencyInfo.Flow",
              [&](perfetto::EventContext ctx) {
                ui::LatencyInfo::FillTraceEvent(
                    ctx, trace_id,
                    ChromeLatencyInfo2::Step::STEP_HANDLED_INPUT_EVENT_IMPL);
              });

  std::move(callback).Run(
      mojom::blink::InputEventResultSource::kMainThread, latency_info,
      result_state, std::move(overscroll_params),
      touch_action
          ? mojom::blink::TouchActionOptional::New(touch_action.value())
          : nullptr);
}

mojom::blink::InputEventResultState InputEventDispositionToAck(
    InputHandlerProxy::EventDisposition disposition) {
  switch (disposition) {
    case InputHandlerProxy::DID_HANDLE:
      return mojom::blink::InputEventResultState::kConsumed;
    case InputHandlerProxy::DID_NOT_HANDLE:
      return mojom::blink::InputEventResultState::kNotConsumed;
    case InputHandlerProxy::DID_NOT_HANDLE_NON_BLOCKING_DUE_TO_FLING:
      return mojom::blink::InputEventResultState::kSetNonBlockingDueToFling;
    case InputHandlerProxy::DROP_EVENT:
      return mojom::blink::InputEventResultState::kNoConsumerExists;
    case InputHandlerProxy::DID_NOT_HANDLE_NON_BLOCKING:
      return mojom::blink::InputEventResultState::kSetNonBlocking;
    case InputHandlerProxy::REQUIRES_MAIN_THREAD_HIT_TEST:
    default:
      NOTREACHED_IN_MIGRATION();
      return mojom::blink::InputEventResultState::kUnknown;
  }
}

}  // namespace

#if BUILDFLAG(IS_ANDROID)
class SynchronousCompositorProxyRegistry
    : public SynchronousCompositorRegistry {
 public:
  explicit SynchronousCompositorProxyRegistry(
      scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
      base::PlatformThreadId io_thread_id,
      base::PlatformThreadId main_thread_id)
      : compositor_thread_default_task_runner_(
            std::move(compositor_task_runner)),
        io_thread_id_(io_thread_id),
        main_thread_id_(main_thread_id) {}

  ~SynchronousCompositorProxyRegistry() override {
    // Ensure the proxy has already been release on the compositor thread
    // before destroying this object.
    DCHECK(!proxy_);
  }

  void CreateProxy(InputHandlerProxy* handler) {
    DCHECK(compositor_thread_default_task_runner_->BelongsToCurrentThread());
    proxy_ = std::make_unique<SynchronousCompositorProxy>(handler);

    proxy_->Init();

    if (base::FeatureList::IsEnabled(::features::kWebViewEnableADPF)) {
      Vector<base::PlatformThreadId> renderer_thread_ids;
      renderer_thread_ids.push_back(base::PlatformThread::CurrentId());
      if (io_thread_id_ != base::kInvalidThreadId) {
        renderer_thread_ids.push_back(io_thread_id_);
      }
      if (main_thread_id_ != base::kInvalidThreadId &&
          base::FeatureList::IsEnabled(
              ::features::kWebViewEnableADPFRendererMain)) {
        renderer_thread_ids.push_back(main_thread_id_);
      }
      proxy_->SetThreadIds(renderer_thread_ids);
    }

    if (sink_)
      proxy_->SetLayerTreeFrameSink(sink_);
  }

  SynchronousCompositorProxy* proxy() { return proxy_.get(); }

  void RegisterLayerTreeFrameSink(
      SynchronousLayerTreeFrameSink* layer_tree_frame_sink) override {
    DCHECK(compositor_thread_default_task_runner_->BelongsToCurrentThread());
    DCHECK_EQ(nullptr, sink_);
    sink_ = layer_tree_frame_sink;
    if (proxy_)
      proxy_->SetLayerTreeFrameSink(layer_tree_frame_sink);
  }

  void UnregisterLayerTreeFrameSink(
      SynchronousLayerTreeFrameSink* layer_tree_frame_sink) override {
    DCHECK(compositor_thread_default_task_runner_->BelongsToCurrentThread());
    DCHECK_EQ(layer_tree_frame_sink, sink_);
    sink_ = nullptr;
  }

  void DestroyProxy() {
    DCHECK(compositor_thread_default_task_runner_->BelongsToCurrentThread());
    proxy_.reset();
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner>
      compositor_thread_default_task_runner_;
  std::unique_ptr<SynchronousCompositorProxy> proxy_;
  raw_ptr<SynchronousLayerTreeFrameSink> sink_ = nullptr;
  base::PlatformThreadId io_thread_id_;
  base::PlatformThreadId main_thread_id_;
};

#endif

scoped_refptr<WidgetInputHandlerManager> WidgetInputHandlerManager::Create(
    base::WeakPtr<WidgetBase> widget,
    base::WeakPtr<mojom::blink::FrameWidgetInputHandler>
        frame_widget_input_handler,
    bool never_composited,
    CompositorThreadScheduler* compositor_thread_scheduler,
    scoped_refptr<scheduler::WidgetScheduler> widget_scheduler,
    bool uses_input_handler,
    bool allow_scroll_resampling,
    base::PlatformThreadId io_thread_id,
    base::PlatformThreadId main_thread_id) {
  DCHECK(widget_scheduler);
  auto manager = base::MakeRefCounted<WidgetInputHandlerManager>(
      base::PassKey<WidgetInputHandlerManager>(), std::move(widget),
      std::move(frame_widget_input_handler), never_composited,
      compositor_thread_scheduler, std::move(widget_scheduler),
      allow_scroll_resampling, io_thread_id, main_thread_id);

  manager->InitializeInputEventSuppressionStates();
  if (uses_input_handler)
    manager->InitInputHandler();

  // A compositor thread implies we're using an input handler.
  DCHECK(!manager->compositor_thread_default_task_runner_ ||
         uses_input_handler);
  // Conversely, if we don't use an input handler we must not have a compositor
  // thread.
  DCHECK(uses_input_handler ||
         !manager->compositor_thread_default_task_runner_);

  return manager;
}

WidgetInputHandlerManager::WidgetInputHandlerManager(
    base::PassKey<WidgetInputHandlerManager>,
    base::WeakPtr<WidgetBase> widget,
    base::WeakPtr<mojom::blink::FrameWidgetInputHandler>
        frame_widget_input_handler,
    bool never_composited,
    CompositorThreadScheduler* compositor_thread_scheduler,
    scoped_refptr<scheduler::WidgetScheduler> widget_scheduler,
    bool allow_scroll_resampling,
    base::PlatformThreadId io_thread_id,
    base::PlatformThreadId main_thread_id)
    : widget_(std::move(widget)),
      frame_widget_input_handler_(std::move(frame_widget_input_handler)),
      widget_scheduler_(std::move(widget_scheduler)),
      widget_is_embedded_(widget_ && widget_->is_embedded()),
      main_thread_task_runner_(widget_scheduler_->InputTaskRunner()),
      compositor_thread_default_task_runner_(
          compositor_thread_scheduler
              ? compositor_thread_scheduler->DefaultTaskRunner()
              : nullptr),
      compositor_thread_input_blocking_task_runner_(
          compositor_thread_scheduler
              ? compositor_thread_scheduler->InputTaskRunner()
              : nullptr),
      input_event_queue_(base::MakeRefCounted<MainThreadEventQueue>(
          this,
          InputThreadTaskRunner(),
          widget_scheduler_->InputTaskRunner(),
          widget_scheduler_,
          /*allow_raf_aligned_input=*/!never_composited)),
      allow_scroll_resampling_(allow_scroll_resampling) {
#if BUILDFLAG(IS_ANDROID)
  if (compositor_thread_default_task_runner_) {
    synchronous_compositor_registry_ =
        std::make_unique<SynchronousCompositorProxyRegistry>(
            compositor_thread_default_task_runner_, io_thread_id,
            main_thread_id);
  }
#endif
}

void WidgetInputHandlerManager::DidFirstVisuallyNonEmptyPaint(
    const base::TimeTicks& first_paint_time) {
  suppressing_input_events_state_ &=
      ~static_cast<uint16_t>(SuppressingInputEventsBits::kHasNotPainted);

  RecordEventMetricsForPaintTiming(first_paint_time);
}

void WidgetInputHandlerManager::InitInputHandler() {
  bool sync_compositing = false;
#if BUILDFLAG(IS_ANDROID)
  sync_compositing =
      Platform::Current()->IsSynchronousCompositingEnabledForAndroidWebView();
#endif
  uses_input_handler_ = true;
  base::OnceClosure init_closure = base::BindOnce(
      &WidgetInputHandlerManager::InitOnInputHandlingThread,
      weak_ptr_factory_.GetWeakPtr(),
      widget_->LayerTreeHost()->GetDelegateForInput(), sync_compositing);
  InputThreadTaskRunner()->PostTask(FROM_HERE, std::move(init_closure));
}

WidgetInputHandlerManager::~WidgetInputHandlerManager() = default;

void WidgetInputHandlerManager::AddInterface(
    mojo::PendingReceiver<mojom::blink::WidgetInputHandler> receiver,
    mojo::PendingRemote<mojom::blink::WidgetInputHandlerHost> host) {
  if (compositor_thread_default_task_runner_) {
    host_ = mojo::SharedRemote<mojom::blink::WidgetInputHandlerHost>(
        std::move(host), compositor_thread_default_task_runner_);
    // Mojo channel bound on compositor thread.
    compositor_thread_default_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&WidgetInputHandlerManager::BindChannel, this,
                                  std::move(receiver)));
  } else {
    host_ = mojo::SharedRemote<mojom::blink::WidgetInputHandlerHost>(
        std::move(host));
    // Mojo channel bound on main thread.
    BindChannel(std::move(receiver));
  }
}

bool WidgetInputHandlerManager::HandleInputEvent(
    const WebCoalescedInputEvent& event,
    std::unique_ptr<cc::EventMetrics> metrics,
    HandledEventCallback handled_callback) {
  WidgetBaseInputHandler::HandledEventCallback blink_callback = base::BindOnce(
      [](HandledEventCallback callback,
         blink::mojom::InputEventResultState ack_state,
         const ui::LatencyInfo& latency_info,
         std::unique_ptr<InputHandlerProxy::DidOverscrollParams>
             overscroll_params,
         std::optional<cc::TouchAction> touch_action) {
        if (!callback)
          return;
        std::move(callback).Run(ack_state, latency_info,
                                ToDidOverscrollParams(overscroll_params.get()),
                                touch_action);
      },
      std::move(handled_callback));
  widget_->input_handler().HandleInputEvent(event, std::move(metrics),
                                            std::move(blink_callback));

  if (!widget_) {
    // The `HandleInputEvent()` call above might result in deletion of
    // `widget_`.
    return true;
  }
  // TODO(szager): Should this be limited to discrete input events by
  // conditioning on (!scheduler::PendingUserInput::IsContinuousEventType())?
  widget_->LayerTreeHost()->proxy()->SetInputResponsePending();

  return true;
}

void WidgetInputHandlerManager::InputEventsDispatched(bool raf_aligned) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  // Immediately after dispatching rAF-aligned events, a frame is still in
  // progress. There is no need to check and break swap promises here, because
  // when the frame is finished, they will be broken if there is no update (see
  // `LayerTreeHostImpl::BeginMainFrameAborted`). Also, unlike non-rAF-aligned
  // events, checking `RequestedMainFramePending()` would not work here, because
  // it is reset before dispatching rAF-aligned events.
  if (raf_aligned)
    return;

  // If no main frame request is pending after dispatching non-rAF-aligned
  // events, there will be no updated frame to submit to Viz; so, break
  // outstanding swap promises here due to no update.
  if (widget_ && !widget_->LayerTreeHost()->RequestedMainFramePending()) {
    widget_->LayerTreeHost()->GetSwapPromiseManager()->BreakSwapPromises(
        cc::SwapPromise::DidNotSwapReason::COMMIT_NO_UPDATE);
  }
}

void WidgetInputHandlerManager::SetNeedsMainFrame() {
  widget_->RequestAnimationAfterDelay(base::TimeDelta());
}

bool WidgetInputHandlerManager::RequestedMainFramePending() {
  return widget_->LayerTreeHost()->RequestedMainFramePending();
}

void WidgetInputHandlerManager::WillShutdown() {
#if BUILDFLAG(IS_ANDROID)
  if (synchronous_compositor_registry_)
    synchronous_compositor_registry_->DestroyProxy();
#endif
  input_handler_proxy_.reset();
  dropped_event_counts_timer_.reset();
}

void WidgetInputHandlerManager::FindScrollTargetOnMainThread(
    const gfx::PointF& point,
    ElementAtPointCallback callback) {
  TRACE_EVENT2("input",
               "WidgetInputHandlerManager::FindScrollTargetOnMainThread",
               "point.x", point.x(), "point.y", point.y());
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  cc::ElementId element_id;
  if (widget_) {
    element_id =
        widget_->client()->FrameWidget()->GetScrollableContainerIdAt(point);
  }

  InputThreadTaskRunner(TaskRunnerType::kInputBlocking)
      ->PostTask(FROM_HERE, base::BindOnce(std::move(callback), element_id));
}

void WidgetInputHandlerManager::DidStartScrollingViewport() {
  mojom::blink::WidgetInputHandlerHost* host = GetWidgetInputHandlerHost();
  if (!host)
    return;
  host->DidStartScrollingViewport();
}

void WidgetInputHandlerManager::SetAllowedTouchAction(
    cc::TouchAction touch_action) {
  compositor_allowed_touch_action_ = touch_action;
}

void WidgetInputHandlerManager::ProcessTouchAction(
    cc::TouchAction touch_action) {
  if (mojom::blink::WidgetInputHandlerHost* host = GetWidgetInputHandlerHost())
    host->SetTouchActionFromMain(touch_action);
}

mojom::blink::WidgetInputHandlerHost*
WidgetInputHandlerManager::GetWidgetInputHandlerHost() {
  if (host_)
    return host_.get();
  return nullptr;
}

#if BUILDFLAG(IS_ANDROID)
void WidgetInputHandlerManager::AttachSynchronousCompositor(
    mojo::PendingRemote<mojom::blink::SynchronousCompositorControlHost>
        control_host,
    mojo::PendingAssociatedRemote<mojom::blink::SynchronousCompositorHost> host,
    mojo::PendingAssociatedReceiver<mojom::blink::SynchronousCompositor>
        compositor_request) {
  DCHECK(synchronous_compositor_registry_);
  if (synchronous_compositor_registry_->proxy()) {
    synchronous_compositor_registry_->proxy()->BindChannel(
        std::move(control_host), std::move(host),
        std::move(compositor_request));
  }
}
#endif

void WidgetInputHandlerManager::ObserveGestureEventOnMainThread(
    const WebGestureEvent& gesture_event,
    const cc::InputHandlerScrollResult& scroll_result) {
  base::OnceClosure observe_gesture_event_closure = base::BindOnce(
      &WidgetInputHandlerManager::ObserveGestureEventOnInputHandlingThread,
      this, gesture_event, scroll_result);
  InputThreadTaskRunner()->PostTask(FROM_HERE,
                                    std::move(observe_gesture_event_closure));
}

void WidgetInputHandlerManager::LogInputTimingUMA() {
  bool should_emit_uma;
  {
    base::AutoLock lock(uma_data_lock_);
    should_emit_uma = !uma_data_.have_emitted_uma;
    uma_data_.have_emitted_uma = true;
  }

  if (!should_emit_uma)
    return;

  InitialInputTiming lifecycle_state = InitialInputTiming::kBeforeLifecycle;
  if (!(suppressing_input_events_state_ &
        (unsigned)SuppressingInputEventsBits::kDeferMainFrameUpdates)) {
    if (suppressing_input_events_state_ &
        (unsigned)SuppressingInputEventsBits::kDeferCommits) {
      lifecycle_state = InitialInputTiming::kBeforeCommit;
    } else if (suppressing_input_events_state_ &
               (unsigned)SuppressingInputEventsBits::kHasNotPainted) {
      lifecycle_state = InitialInputTiming::kBeforeFirstPaint;
    } else {
      lifecycle_state = InitialInputTiming::kAfterFirstPaint;
    }
  }

  UMA_HISTOGRAM_ENUMERATION("PaintHolding.InputTiming4", lifecycle_state);
}

void WidgetInputHandlerManager::RecordEventMetricsForPaintTiming(
    std::optional<base::TimeTicks> first_paint_time) {
  CHECK(main_thread_task_runner_->BelongsToCurrentThread());

  if (recorded_event_metric_for_paint_timing_) {
    return;
  }
  recorded_event_metric_for_paint_timing_ = true;

  if (first_paint_max_delay_timer_ &&
      first_paint_max_delay_timer_->IsRunning()) {
    first_paint_max_delay_timer_->Stop();
  }

  bool first_paint_max_delay_reached = !first_paint_time.has_value();

  // Initialize to 0 timestamp and log 0 if there was no suppressed event or
  // the most recent suppressed event was before the first_paint_time
  auto diff = base::TimeDelta();
  int suppressed_interactions_count = 0;
  int suppressed_events_count = 0;
  {
    base::AutoLock lock(uma_data_lock_);
    if (first_paint_max_delay_reached) {
      diff = kFirstPaintMaxAcceptableDelay;
    } else if (uma_data_.most_recent_suppressed_event_time >
               first_paint_time.value()) {
      diff = uma_data_.most_recent_suppressed_event_time -
             first_paint_time.value();
    }

    suppressed_interactions_count = uma_data_.suppressed_interactions_count;
    suppressed_events_count = uma_data_.suppressed_events_count;
  }

  UMA_HISTOGRAM_TIMES("PageLoad.Internal.SuppressedEventsTimingBeforePaint3",
                      diff);
  UMA_HISTOGRAM_COUNTS(
      "PageLoad.Internal.SuppressedInteractionsCountBeforePaint3",
      suppressed_interactions_count);
  UMA_HISTOGRAM_COUNTS("PageLoad.Internal.SuppressedEventsCountBeforePaint3",
                       suppressed_events_count);
  UMA_HISTOGRAM_BOOLEAN(
      "PageLoad.Internal.SuppressedEventsBeforeMissingFirstPaint",
      first_paint_max_delay_reached);
}

void WidgetInputHandlerManager::StartFirstPaintMaxDelayTimer() {
  if (first_paint_max_delay_timer_ || recorded_event_metric_for_paint_timing_) {
    return;
  }
  first_paint_max_delay_timer_ = std::make_unique<base::OneShotTimer>();
  first_paint_max_delay_timer_->Start(
      FROM_HERE, kFirstPaintMaxAcceptableDelay,
      base::BindOnce(
          &WidgetInputHandlerManager::RecordEventMetricsForPaintTiming, this,
          std::nullopt));
}

void WidgetInputHandlerManager::DispatchScrollGestureToCompositor(
    std::unique_ptr<WebGestureEvent> event) {
  std::unique_ptr<WebCoalescedInputEvent> web_scoped_gesture_event =
      std::make_unique<WebCoalescedInputEvent>(std::move(event),
                                               ui::LatencyInfo());
  // input thread task runner is |main_thread_task_runner_| only in tests
  InputThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&WidgetInputHandlerManager::
                         HandleInputEventWithLatencyOnInputHandlingThread,
                     this, std::move(web_scoped_gesture_event)));
}

void WidgetInputHandlerManager::
    HandleInputEventWithLatencyOnInputHandlingThread(
        std::unique_ptr<WebCoalescedInputEvent> event) {
  DCHECK(input_handler_proxy_);
  input_handler_proxy_->HandleInputEventWithLatencyInfo(
      std::move(event), nullptr, base::DoNothing());
}

void WidgetInputHandlerManager::DispatchEventOnInputThreadForTesting(
    std::unique_ptr<blink::WebCoalescedInputEvent> event,
    mojom::blink::WidgetInputHandler::DispatchEventCallback callback) {
  InputThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&WidgetInputHandlerManager::DispatchEvent, this,
                                std::move(event), std::move(callback)));
}

void WidgetInputHandlerManager::DispatchEvent(
    std::unique_ptr<WebCoalescedInputEvent> event,
    mojom::blink::WidgetInputHandler::DispatchEventCallback callback) {
  WebInputEvent::Type event_type = event->Event().GetType();
  bool event_is_mouse_or_pointer_move =
      event_type == WebInputEvent::Type::kMouseMove ||
      event_type == WebInputEvent::Type::kPointerMove;
  if (!event_is_mouse_or_pointer_move &&
      event_type != WebInputEvent::Type::kTouchMove) {
    LogInputTimingUMA();

    // We only count it if the only reason we are suppressing is because we
    // haven't painted yet.
    if (suppressing_input_events_state_ ==
        static_cast<uint16_t>(SuppressingInputEventsBits::kHasNotPainted)) {
      base::AutoLock lock(uma_data_lock_);
      uma_data_.most_recent_suppressed_event_time = base::TimeTicks::Now();
      uma_data_.suppressed_events_count += 1;

      // Each of the events in the condition below represents a single
      // interaction by the user even though some of these events can fire
      // multiple JS events.  For example, further downstream from here Blink
      // `EventHandler` fires a JS "pointerdown" event (and under certain
      // conditions even a "mousedown" event) for single a kTouchStart event
      // here.
      if (event_type == WebInputEvent::Type::kMouseDown ||
          event_type == WebInputEvent::Type::kRawKeyDown ||
          event_type == WebInputEvent::Type::kKeyDown ||
          event_type == WebInputEvent::Type::kTouchStart ||
          event_type == WebInputEvent::Type::kPointerDown) {
        uma_data_.suppressed_interactions_count += 1;
      }
    }
  }

  if (!widget_is_embedded_ &&
      (suppressing_input_events_state_ &
       static_cast<uint16_t>(SuppressingInputEventsBits::kHasNotPainted))) {
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WidgetInputHandlerManager::StartFirstPaintMaxDelayTimer,
                       this));
  }

  // Drop input if we are deferring a rendering pipeline phase, unless it's a
  // move event, or we are waiting for first visually non empty paint.
  // We don't want users interacting with stuff they can't see, so we drop it.
  // We allow moves because we need to keep the current pointer location up
  // to date. Tests and other code can allow pre-commit input through the
  // "allow-pre-commit-input" command line flag.
  // TODO(schenney): Also allow scrolls? This would make some tests not flaky,
  // it seems, because they sometimes crash on seeing a scroll update/end
  // without a begin. Scrolling, pinch-zoom etc. don't seem dangerous.

  uint16_t suppress_input = suppressing_input_events_state_;

  bool ignore_first_paint = !base::FeatureList::IsEnabled(
      blink::features::kDropInputEventsBeforeFirstPaint);
  // TODO(https://crbug.com/1490296): Investigate the possibility of a stale
  // subframe after navigation.
  if (widget_is_embedded_) {
    ignore_first_paint = true;
  }
  if (ignore_first_paint) {
    suppress_input &=
        ~static_cast<uint16_t>(SuppressingInputEventsBits::kHasNotPainted);
  }

  if (suppress_input && !allow_pre_commit_input_ &&
      !event_is_mouse_or_pointer_move) {
    if (callback) {
      std::move(callback).Run(
          mojom::blink::InputEventResultSource::kMainThread, ui::LatencyInfo(),
          mojom::blink::InputEventResultState::kNotConsumed, nullptr, nullptr);
    }
    return;
  }

  // If TimeTicks is not consistent across processes we cannot use the event's
  // platform timestamp in this process. Instead use the time that the event is
  // received as the event's timestamp.
  if (!base::TimeTicks::IsConsistentAcrossProcesses()) {
    event->EventPointer()->SetTimeStamp(base::TimeTicks::Now());
  }

  // TODO(b/224960731): Fix tests and add
  // `DCHECK(!arrived_in_browser_main_timestamp.is_null())`.
  //  We expect that `arrived_in_browser_main_timestamp` is always
  //  found, but there are a lot of tests where this component is not set.
  //  Currently EventMetrics knows how to handle null timestamp, so we
  //  don't process it here.
  const base::TimeTicks arrived_in_browser_main_timestamp =
      event->Event()
          .GetEventLatencyMetadata()
          .arrived_in_browser_main_timestamp;
  std::unique_ptr<cc::EventMetrics> metrics;
  if (event->Event().IsGestureScroll()) {
    const auto& gesture_event =
        static_cast<const WebGestureEvent&>(event->Event());
    const bool is_inertial = gesture_event.InertialPhase() ==
                             WebGestureEvent::InertialPhaseState::kMomentum;
    //'scrolls_blocking_touch_dispatched_to_renderer' can be null. It is set
    // by the Browser only if the corresponding TouchMove was blocking.
    base::TimeTicks blocking_touch_dispatched_to_renderer_timestamp =
        event->Event()
            .GetEventLatencyMetadata()
            .scrolls_blocking_touch_dispatched_to_renderer;

    if (gesture_event.GetType() == WebInputEvent::Type::kGestureScrollUpdate) {
      metrics = cc::ScrollUpdateEventMetrics::Create(
          gesture_event.GetTypeAsUiEventType(),
          gesture_event.GetScrollInputType(), is_inertial,
          has_seen_first_gesture_scroll_update_after_begin_
              ? cc::ScrollUpdateEventMetrics::ScrollUpdateType::kContinued
              : cc::ScrollUpdateEventMetrics::ScrollUpdateType::kStarted,
          gesture_event.data.scroll_update.delta_y, event->Event().TimeStamp(),
          arrived_in_browser_main_timestamp,
          blocking_touch_dispatched_to_renderer_timestamp,
          base::IdType64<class ui::LatencyInfo>(
              event->latency_info().trace_id()));
      has_seen_first_gesture_scroll_update_after_begin_ = true;
    } else {
      metrics = cc::ScrollEventMetrics::Create(
          gesture_event.GetTypeAsUiEventType(),
          gesture_event.GetScrollInputType(), is_inertial,
          event->Event().TimeStamp(), arrived_in_browser_main_timestamp,
          blocking_touch_dispatched_to_renderer_timestamp,
          base::IdType64<class ui::LatencyInfo>(
              event->latency_info().trace_id()));
      has_seen_first_gesture_scroll_update_after_begin_ = false;
    }
  } else if (WebInputEvent::IsPinchGestureEventType(event_type)) {
    const auto& gesture_event =
        static_cast<const WebGestureEvent&>(event->Event());
    metrics = cc::PinchEventMetrics::Create(
        gesture_event.GetTypeAsUiEventType(),
        gesture_event.GetScrollInputType(), event->Event().TimeStamp(),
        base::IdType64<class ui::LatencyInfo>(
            event->latency_info().trace_id()));
  } else {
    metrics = cc::EventMetrics::Create(event->Event().GetTypeAsUiEventType(),
                                       event->Event().TimeStamp(),
                                       arrived_in_browser_main_timestamp,
                                       base::IdType64<class ui::LatencyInfo>(
                                           event->latency_info().trace_id()));
  }

  if (uses_input_handler_) {
    // If the input_handler_proxy has disappeared ensure we just ack event.
    if (!input_handler_proxy_) {
      if (callback) {
        std::move(callback).Run(
            mojom::blink::InputEventResultSource::kMainThread,
            ui::LatencyInfo(),
            mojom::blink::InputEventResultState::kNotConsumed, nullptr,
            nullptr);
      }
      return;
    }

    // The InputHandlerProxy will be the first to try handling the event on the
    // compositor thread. It will respond to this class by calling
    // DidHandleInputEventSentToCompositor with the result of its attempt. Based
    // on the resulting disposition, DidHandleInputEventSentToCompositor will
    // either ACK the event as handled to the browser or forward it to the main
    // thread.
    input_handler_proxy_->HandleInputEventWithLatencyInfo(
        std::move(event), std::move(metrics),
        base::BindOnce(
            &WidgetInputHandlerManager::DidHandleInputEventSentToCompositor,
            this, std::move(callback)));
  } else {
    DCHECK(!input_handler_proxy_);
    DispatchDirectlyToWidget(std::move(event), std::move(metrics),
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

static void WaitForInputProcessedFromMain(base::WeakPtr<WidgetBase> widget) {
  // If the widget is destroyed while we're posting to the main thread, the
  // Mojo message will be acked in WidgetInputHandlerImpl's destructor.
  if (!widget)
    return;

  WidgetInputHandlerManager* manager = widget->widget_input_handler_manager();

  // TODO(bokan): Temporary to unblock synthetic gesture events running under
  // VR. https://crbug.com/940063
  bool ack_immediately = widget->client()->ShouldAckSyntheticInputImmediately();

  // If the WidgetBase is hidden, we won't produce compositor frames for it
  // so just ACK the input to prevent blocking the browser indefinitely.
  if (widget->is_hidden() || ack_immediately) {
    manager->InvokeInputProcessedCallback();
    return;
  }

  auto redraw_complete_callback =
      base::BindOnce(&WidgetInputHandlerManager::InvokeInputProcessedCallback,
                     manager->AsWeakPtr());

  // Since wheel-events can kick off animations, we can not consider
  // all observable effects of an input gesture to be processed
  // when the CompositorFrame caused by that input has been produced, send, and
  // displayed. Therefore, explicitly request the presentation *after* any
  // ongoing scroll-animation ends. After the scroll-animation ends (if any),
  // the call will force a commit and redraw and callback when the
  // CompositorFrame has been displayed in the display service. Some examples of
  // non-trivial effects that require waiting that long: committing
  // MainThreadScrollHitTestRegion to the compositor, sending touch-action rects
  // to the browser, and sending updated surface information to the display
  // compositor for up-to-date OOPIF hit-testing.

  widget->RequestPresentationAfterScrollAnimationEnd(
      std::move(redraw_complete_callback));
}

void WidgetInputHandlerManager::WaitForInputProcessed(
    base::OnceClosure callback) {
  // Note, this will be called from the mojo-bound thread which could be either
  // main or compositor.
  DCHECK(!input_processed_callback_);
  input_processed_callback_ = std::move(callback);

  // We mustn't touch widget_ from the impl thread so post all the setup to the
  // main thread. Make sure the callback runs after all the queued events are
  // dispatched.
  base::OnceClosure closure =
      base::BindOnce(&MainThreadEventQueue::QueueClosure, input_event_queue_,
                     base::BindOnce(&WaitForInputProcessedFromMain, widget_));

  // If there are frame-aligned input events waiting to be dispatched, wait for
  // that to happen before posting to the main thread input queue.
  if (input_handler_proxy_) {
    input_handler_proxy_->RequestCallbackAfterEventQueueFlushed(
        std::move(closure));
  } else {
    std::move(closure).Run();
  }
}

void WidgetInputHandlerManager::InitializeInputEventSuppressionStates() {
  suppressing_input_events_state_ =
      static_cast<uint16_t>(SuppressingInputEventsBits::kHasNotPainted);

  first_paint_max_delay_timer_.reset();
  recorded_event_metric_for_paint_timing_ = false;

  base::AutoLock lock(uma_data_lock_);
  uma_data_.have_emitted_uma = false;
  uma_data_.most_recent_suppressed_event_time = base::TimeTicks();
  uma_data_.suppressed_interactions_count = 0;
  uma_data_.suppressed_events_count = 0;
}

void WidgetInputHandlerManager::OnDeferMainFrameUpdatesChanged(bool status) {
  if (status) {
    suppressing_input_events_state_ |= static_cast<uint16_t>(
        SuppressingInputEventsBits::kDeferMainFrameUpdates);
  } else {
    suppressing_input_events_state_ &= ~static_cast<uint16_t>(
        SuppressingInputEventsBits::kDeferMainFrameUpdates);
  }
}

void WidgetInputHandlerManager::OnDeferCommitsChanged(
    bool status,
    cc::PaintHoldingReason reason) {
  if (status && reason == cc::PaintHoldingReason::kFirstContentfulPaint) {
    suppressing_input_events_state_ |=
        static_cast<uint16_t>(SuppressingInputEventsBits::kDeferCommits);
  } else {
    suppressing_input_events_state_ &=
        ~static_cast<uint16_t>(SuppressingInputEventsBits::kDeferCommits);
  }
}

void WidgetInputHandlerManager::InitOnInputHandlingThread(
    const base::WeakPtr<cc::CompositorDelegateForInput>& compositor_delegate,
    bool sync_compositing) {
  DCHECK(InputThreadTaskRunner()->BelongsToCurrentThread());
  DCHECK(uses_input_handler_);

  // It is possible that the input_handler has already been destroyed before
  // this Init() call was invoked. If so, early out.
  if (!compositor_delegate)
    return;

  // The input handler is created and ownership is passed to the compositor
  // delegate; hence we only receive a WeakPtr back.
  base::WeakPtr<cc::InputHandler> input_handler =
      cc::InputHandler::Create(*compositor_delegate);
  DCHECK(input_handler);

  input_handler_proxy_ =
      std::make_unique<InputHandlerProxy>(*input_handler.get(), this);

#if BUILDFLAG(IS_ANDROID)
  if (sync_compositing) {
    DCHECK(synchronous_compositor_registry_);
    synchronous_compositor_registry_->CreateProxy(input_handler_proxy_.get());
  }
#endif
}

void WidgetInputHandlerManager::BindChannel(
    mojo::PendingReceiver<mojom::blink::WidgetInputHandler> receiver) {
  if (!receiver.is_valid())
    return;
  // Passing null for |input_event_queue_| tells the handler that we don't have
  // a compositor thread. (Single threaded-mode shouldn't use the queue, or else
  // events might get out of order - see crrev.com/519829).
  WidgetInputHandlerImpl* handler = new WidgetInputHandlerImpl(
      this,
      compositor_thread_default_task_runner_ ? input_event_queue_ : nullptr,
      widget_, frame_widget_input_handler_);
  handler->SetReceiver(std::move(receiver));
}

void WidgetInputHandlerManager::DispatchDirectlyToWidget(
    std::unique_ptr<WebCoalescedInputEvent> event,
    std::unique_ptr<cc::EventMetrics> metrics,
    mojom::blink::WidgetInputHandler::DispatchEventCallback callback) {
  // This path should only be taken by non-frame WidgetBase that don't use a
  // compositor (e.g. popups, plugins). Events bounds for a frame WidgetBase
  // must be passed through the InputHandlerProxy first.
  DCHECK(!uses_input_handler_);

  // Input messages must not be processed if the WidgetBase was destroyed or
  // was just recreated for a provisional frame.
  if (!widget_ || widget_->IsForProvisionalFrame()) {
    if (callback) {
      std::move(callback).Run(mojom::blink::InputEventResultSource::kMainThread,
                              event->latency_info(),
                              mojom::blink::InputEventResultState::kNotConsumed,
                              nullptr, nullptr);
    }
    return;
  }

  auto send_callback = base::BindOnce(
      &WidgetInputHandlerManager::DidHandleInputEventSentToMainFromWidgetBase,
      this, std::move(callback));

  widget_->input_handler().HandleInputEvent(*event, std::move(metrics),
                                            std::move(send_callback));
  InputEventsDispatched(/*raf_aligned=*/false);
}

void WidgetInputHandlerManager::FindScrollTargetReply(
    std::unique_ptr<WebCoalescedInputEvent> event,
    std::unique_ptr<cc::EventMetrics> metrics,
    mojom::blink::WidgetInputHandler::DispatchEventCallback browser_callback,
    cc::ElementId hit_test_result) {
  TRACE_EVENT1("input", "WidgetInputHandlerManager::FindScrollTargetReply",
               "hit_test_result", hit_test_result.ToString());
  DCHECK(InputThreadTaskRunner()->BelongsToCurrentThread());

  // If the input_handler was destroyed in the mean time just ACK the event as
  // unconsumed to the browser and drop further handling.
  if (!input_handler_proxy_) {
    std::move(browser_callback)
        .Run(mojom::blink::InputEventResultSource::kMainThread,
             ui::LatencyInfo(),
             mojom::blink::InputEventResultState::kNotConsumed, nullptr,
             nullptr);
    return;
  }

  input_handler_proxy_->ContinueScrollBeginAfterMainThreadHitTest(
      std::move(event), std::move(metrics),
      base::BindOnce(
          &WidgetInputHandlerManager::DidHandleInputEventSentToCompositor, this,
          std::move(browser_callback)),
      hit_test_result);

  // Let the main frames flow.
  input_handler_proxy_->SetDeferBeginMainFrame(false);
}

void WidgetInputHandlerManager::SendDroppedPointerDownCounts() {
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WidgetBase::CountDroppedPointerDownForEventTiming,
                     widget_, dropped_pointer_down_));
  dropped_pointer_down_ = 0;
}

void WidgetInputHandlerManager::DidHandleInputEventSentToCompositor(
    mojom::blink::WidgetInputHandler::DispatchEventCallback callback,
    InputHandlerProxy::EventDisposition event_disposition,
    std::unique_ptr<WebCoalescedInputEvent> event,
    std::unique_ptr<InputHandlerProxy::DidOverscrollParams> overscroll_params,
    const WebInputEventAttribution& attribution,
    std::unique_ptr<cc::EventMetrics> metrics) {
  TRACE_EVENT1("input",
               "WidgetInputHandlerManager::DidHandleInputEventSentToCompositor",
               "Disposition", event_disposition);

  int64_t trace_id = event->latency_info().trace_id();
  TRACE_EVENT(
      "input,benchmark,latencyInfo", "LatencyInfo.Flow",
      [&](perfetto::EventContext ctx) {
        ui::LatencyInfo::FillTraceEvent(
            ctx, trace_id,
            ChromeLatencyInfo2::Step::STEP_DID_HANDLE_INPUT_AND_OVERSCROLL);
      });

  DCHECK(InputThreadTaskRunner()->BelongsToCurrentThread());

  if (event_disposition == InputHandlerProxy::DROP_EVENT &&
      event->Event().GetType() == blink::WebInputEvent::Type::kTouchStart) {
    const WebTouchEvent touch_event =
        static_cast<const WebTouchEvent&>(event->Event());
    for (unsigned i = 0; i < touch_event.touches_length; ++i) {
      const WebTouchPoint& touch_point = touch_event.touches[i];
      if (touch_point.state == WebTouchPoint::State::kStatePressed) {
        dropped_pointer_down_++;
      }
    }
    if (dropped_pointer_down_ > 0) {
      if (!dropped_event_counts_timer_) {
        dropped_event_counts_timer_ = std::make_unique<base::OneShotTimer>();
      }

      if (!dropped_event_counts_timer_->IsRunning()) {
        dropped_event_counts_timer_->Start(
            FROM_HERE, kEventCountsTimerDelay,
            base::BindOnce(
                &WidgetInputHandlerManager::SendDroppedPointerDownCounts,
                this));
      }
    }
  }

  if (event_disposition == InputHandlerProxy::REQUIRES_MAIN_THREAD_HIT_TEST) {
    TRACE_EVENT_INSTANT0("input", "PostingHitTestToMainThread",
                         TRACE_EVENT_SCOPE_THREAD);
    DCHECK_EQ(event->Event().GetType(),
              WebInputEvent::Type::kGestureScrollBegin);
    DCHECK(input_handler_proxy_);

    gfx::PointF event_position =
        static_cast<const WebGestureEvent&>(event->Event()).PositionInWidget();

    ElementAtPointCallback result_callback = base::BindOnce(
        &WidgetInputHandlerManager::FindScrollTargetReply, this,
        std::move(event), std::move(metrics), std::move(callback));

    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WidgetInputHandlerManager::FindScrollTargetOnMainThread,
                       this, event_position, std::move(result_callback)));

    // The hit test is on the critical path of the scroll. Don't post any
    // BeginMainFrame tasks until we've returned from the hit test and handled
    // the rest of the input in the compositor event queue.
    //
    // NOTE: setting this in FindScrollTargetOnMainThread would be too late; we
    // might have already posted a BeginMainFrame by then. Even though the
    // scheduler prioritizes the hit test, that main frame won't see the updated
    // scroll offset because the task is bound to CompositorCommitData from the
    // time it was posted. We'd then have to wait for a SECOND BeginMainFrame to
    // actually repaint the scroller at the right offset.
    input_handler_proxy_->SetDeferBeginMainFrame(true);
    return;
  }

  std::optional<cc::TouchAction> touch_action =
      compositor_allowed_touch_action_;
  compositor_allowed_touch_action_.reset();

  mojom::blink::InputEventResultState ack_state =
      InputEventDispositionToAck(event_disposition);
  if (ack_state == mojom::blink::InputEventResultState::kConsumed) {
    widget_scheduler_->DidHandleInputEventOnCompositorThread(
        event->Event(), scheduler::WidgetScheduler::InputEventState::
                            EVENT_CONSUMED_BY_COMPOSITOR);
  } else if (MainThreadEventQueue::IsForwardedAndSchedulerKnown(ack_state)) {
    widget_scheduler_->DidHandleInputEventOnCompositorThread(
        event->Event(), scheduler::WidgetScheduler::InputEventState::
                            EVENT_FORWARDED_TO_MAIN_THREAD);
  }

  if (ack_state == mojom::blink::InputEventResultState::kSetNonBlocking ||
      ack_state ==
          mojom::blink::InputEventResultState::kSetNonBlockingDueToFling ||
      ack_state == mojom::blink::InputEventResultState::kNotConsumed) {
    DCHECK(!overscroll_params);
    DCHECK(!event->latency_info().coalesced());
    MainThreadEventQueue::DispatchType dispatch_type =
        callback.is_null() ? MainThreadEventQueue::DispatchType::kNonBlocking
                           : MainThreadEventQueue::DispatchType::kBlocking;
    HandledEventCallback handled_event = base::BindOnce(
        &WidgetInputHandlerManager::DidHandleInputEventSentToMain, this,
        std::move(callback), touch_action);
    input_event_queue_->HandleEvent(std::move(event), dispatch_type, ack_state,
                                    attribution, std::move(metrics),
                                    std::move(handled_event));
    return;
  }

  if (callback) {
    std::move(callback).Run(
        mojom::blink::InputEventResultSource::kCompositorThread,
        event->latency_info(), ack_state,
        ToDidOverscrollParams(overscroll_params.get()),
        touch_action
            ? mojom::blink::TouchActionOptional::New(touch_action.value())
            : nullptr);
  }
}

void WidgetInputHandlerManager::DidHandleInputEventSentToMainFromWidgetBase(
    mojom::blink::WidgetInputHandler::DispatchEventCallback callback,
    mojom::blink::InputEventResultState ack_state,
    const ui::LatencyInfo& latency_info,
    std::unique_ptr<blink::InputHandlerProxy::DidOverscrollParams>
        overscroll_params,
    std::optional<cc::TouchAction> touch_action) {
  DidHandleInputEventSentToMain(
      std::move(callback), std::nullopt, ack_state, latency_info,
      ToDidOverscrollParams(overscroll_params.get()), touch_action);
}

void WidgetInputHandlerManager::DidHandleInputEventSentToMain(
    mojom::blink::WidgetInputHandler::DispatchEventCallback callback,
    std::optional<cc::TouchAction> touch_action_from_compositor,
    mojom::blink::InputEventResultState ack_state,
    const ui::LatencyInfo& latency_info,
    mojom::blink::DidOverscrollParamsPtr overscroll_params,
    std::optional<cc::TouchAction> touch_action_from_main) {
  if (!callback)
    return;

  TRACE_EVENT1("input",
               "WidgetInputHandlerManager::DidHandleInputEventSentToMain",
               "ack_state", ack_state);

  int64_t trace_id = latency_info.trace_id();
  TRACE_EVENT(
      "input,benchmark,latencyInfo", "LatencyInfo.Flow",
      [&](perfetto::EventContext ctx) {
        ui::LatencyInfo::FillTraceEvent(
            ctx, trace_id,
            ChromeLatencyInfo2::Step::STEP_HANDLED_INPUT_EVENT_MAIN_OR_IMPL);
      });

  std::optional<cc::TouchAction> touch_action_for_ack = touch_action_from_main;
  if (!touch_action_for_ack.has_value()) {
    TRACE_EVENT_INSTANT0("input", "Using allowed_touch_action",
                         TRACE_EVENT_SCOPE_THREAD);
    touch_action_for_ack = touch_action_from_compositor;
  }

  // This method is called from either the main thread or the compositor thread.
  bool is_compositor_thread =
      compositor_thread_default_task_runner_ &&
      compositor_thread_default_task_runner_->BelongsToCurrentThread();

  // If there is a compositor task runner and the current thread isn't the
  // compositor thread proxy it over to the compositor thread.
  if (compositor_thread_default_task_runner_ && !is_compositor_thread) {
    TRACE_EVENT_INSTANT0("input", "PostingToCompositor",
                         TRACE_EVENT_SCOPE_THREAD);
    compositor_thread_default_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(CallCallback, std::move(callback), ack_state,
                                  latency_info, std::move(overscroll_params),
                                  touch_action_for_ack));
  } else {
    // Otherwise call the callback immediately.
    std::move(callback).Run(
        is_compositor_thread
            ? mojom::blink::InputEventResultSource::kCompositorThread
            : mojom::blink::InputEventResultSource::kMainThread,
        latency_info, ack_state, std::move(overscroll_params),
        touch_action_for_ack ? mojom::blink::TouchActionOptional::New(
                                   touch_action_for_ack.value())
                             : nullptr);
  }
}

void WidgetInputHandlerManager::ObserveGestureEventOnInputHandlingThread(
    const WebGestureEvent& gesture_event,
    const cc::InputHandlerScrollResult& scroll_result) {
  if (!input_handler_proxy_)
    return;
  // The elastic overscroll controller on android can be dynamically created or
  // removed by changing prefers-reduced-motion. When removed, we do not need to
  // observe the event.
  if (!input_handler_proxy_->elastic_overscroll_controller())
    return;
  input_handler_proxy_->elastic_overscroll_controller()
      ->ObserveGestureEventAndResult(gesture_event, scroll_result);
}

const scoped_refptr<base::SingleThreadTaskRunner>&
WidgetInputHandlerManager::InputThreadTaskRunner(TaskRunnerType type) const {
  if (compositor_thread_input_blocking_task_runner_ &&
      type == TaskRunnerType::kInputBlocking) {
    return compositor_thread_input_blocking_task_runner_;
  } else if (compositor_thread_default_task_runner_) {
    return compositor_thread_default_task_runner_;
  }
  return main_thread_task_runner_;
}

#if BUILDFLAG(IS_ANDROID)
SynchronousCompositorRegistry*
WidgetInputHandlerManager::GetSynchronousCompositorRegistry() {
  DCHECK(synchronous_compositor_registry_);
  return synchronous_compositor_registry_.get();
}
#endif

void WidgetInputHandlerManager::ClearClient() {
  first_paint_max_delay_timer_.reset();
  recorded_event_metric_for_paint_timing_ = false;
  input_event_queue_->ClearClient();
}

void WidgetInputHandlerManager::UpdateBrowserControlsState(
    cc::BrowserControlsState constraints,
    cc::BrowserControlsState current,
    bool animate,
    base::optional_ref<const cc::BrowserControlsOffsetTagsInfo>
        offset_tags_info) {
  if (!input_handler_proxy_) {
    return;
  }

  DCHECK(InputThreadTaskRunner()->BelongsToCurrentThread());
  input_handler_proxy_->UpdateBrowserControlsState(constraints, current,
                                                   animate, offset_tags_info);
}

void WidgetInputHandlerManager::FlushCompositorQueueForTesting() {
  CHECK(InputThreadTaskRunner()->BelongsToCurrentThread());
  if (!input_handler_proxy_) {
    return;
  }
  input_handler_proxy_->FlushQueuedEventsForTesting();
}

void WidgetInputHandlerManager::FlushMainThreadQueueForTesting(
    base::OnceClosure done) {
  CHECK(main_thread_task_runner_->BelongsToCurrentThread());
  input_event_queue()->DispatchRafAlignedInput(base::TimeTicks::Now());
  CHECK(input_event_queue()->IsEmptyForTesting());
  std::move(done).Run();
}

void WidgetInputHandlerManager::FlushEventQueuesForTesting(
    base::OnceClosure done_callback) {
  CHECK(main_thread_task_runner_->BelongsToCurrentThread());

  auto flush_compositor_queue = base::BindOnce(
      &WidgetInputHandlerManager::FlushCompositorQueueForTesting, this);

  auto flush_main_queue =
      base::BindOnce(&WidgetInputHandlerManager::FlushMainThreadQueueForTesting,
                     this, std::move(done_callback));

  // Flush the compositor queue first since dispatching compositor events may
  // bounce them back into the main thread event queue.
  InputThreadTaskRunner()->PostTaskAndReply(FROM_HERE,
                                            std::move(flush_compositor_queue),
                                            std::move(flush_main_queue));
}

}  // namespace blink
