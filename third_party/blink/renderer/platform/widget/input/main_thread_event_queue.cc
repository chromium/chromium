// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/input/main_thread_event_queue.h"

#include <utility>

#include "base/containers/circular_deque.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "cc/base/features.h"
#include "cc/metrics/event_metrics.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_input_event_attribution.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

constexpr base::TimeDelta kMaxRafDelay = base::Milliseconds(5 * 1000);

class QueuedClosure : public MainThreadEventQueueTask {
 public:
  QueuedClosure(base::OnceClosure closure) : closure_(std::move(closure)) {}

  ~QueuedClosure() override {}

  FilterResult FilterNewEvent(MainThreadEventQueueTask* other_task) override {
    return other_task->IsWebInputEvent() ? FilterResult::KeepIterating
                                         : FilterResult::StopIterating;
  }

  bool IsWebInputEvent() const override { return false; }

  void Dispatch(MainThreadEventQueue*) override { std::move(closure_).Run(); }

 private:
  base::OnceClosure closure_;
};

// Time interval at which touchmove events during scroll will be skipped
// during rAF signal.
constexpr base::TimeDelta kAsyncTouchMoveInterval = base::Milliseconds(200);

bool IsGestureScroll(WebInputEvent::Type type) {
  switch (type) {
    case WebGestureEvent::Type::kGestureScrollBegin:
    case WebGestureEvent::Type::kGestureScrollUpdate:
    case WebGestureEvent::Type::kGestureScrollEnd:
      return true;
    default:
      return false;
  }
}

}  // namespace

class QueuedWebInputEvent : public MainThreadEventQueueTask {
 public:
  QueuedWebInputEvent(std::unique_ptr<WebCoalescedInputEvent> event,
                      bool originally_cancelable,
                      HandledEventCallback callback,
                      bool known_by_scheduler,
                      const WebInputEventAttribution& attribution,
                      std::unique_ptr<cc::EventMetrics> metrics)
      : event_(std::move(event)),
        originally_cancelable_(originally_cancelable),
        callback_(std::move(callback)),
        known_by_scheduler_count_(known_by_scheduler ? 1 : 0),
        attribution_(attribution),
        metrics_(std::move(metrics)) {}

  ~QueuedWebInputEvent() override {}

  static std::unique_ptr<QueuedWebInputEvent> CreateForRawEvent(
      std::unique_ptr<WebCoalescedInputEvent> raw_event,
      const WebInputEventAttribution& attribution,
      const cc::EventMetrics* original_metrics) {
    DCHECK_EQ(raw_event->Event().GetType(),
              WebInputEvent::Type::kPointerRawUpdate);
    std::unique_ptr<cc::EventMetrics> metrics =
        cc::EventMetrics::CreateFromExisting(
            raw_event->Event().GetTypeAsUiEventType(),
            cc::EventMetrics::DispatchStage::kRendererCompositorFinished,
            original_metrics);
    return std::make_unique<QueuedWebInputEvent>(
        std::move(raw_event), false, HandledEventCallback(), false, attribution,
        std::move(metrics));
  }

  bool AreCoalescablePointerRawUpdateEvents(
      const QueuedWebInputEvent& other_event) {
    // There is no pointermove at this point in the queue.
    DCHECK(event_->Event().GetType() != WebInputEvent::Type::kPointerMove &&
           other_event.event_->Event().GetType() !=
               WebInputEvent::Type::kPointerMove);
    // Events with modifiers differing by kRelativeMotionEvent should not be
    // coalesced. In case of a pointer lock, kRelativeMotionEvent is sent
    // when the cursor is recentered. Events post the recentered event have
    // a big delta compared to the previous events and hence should not be
    // coalesced.
    return event_->Event().GetType() ==
               WebInputEvent::Type::kPointerRawUpdate &&
           other_event.event_->Event().GetType() ==
               WebInputEvent::Type::kPointerRawUpdate &&
           ((event_->Event().GetModifiers() &
             blink::WebInputEvent::Modifiers::kRelativeMotionEvent) ==
            (other_event.event_->Event().GetModifiers() &
             blink::WebInputEvent::Modifiers::kRelativeMotionEvent));
  }

  FilterResult FilterNewEvent(MainThreadEventQueueTask* other_task) override {
    if (!other_task->IsWebInputEvent())
      return FilterResult::StopIterating;

    QueuedWebInputEvent* other_event =
        static_cast<QueuedWebInputEvent*>(other_task);
    if (other_event->event_->Event().GetType() ==
        WebInputEvent::Type::kTouchScrollStarted) {
      return HandleTouchScrollStartQueued();
    }

    if (!event_->Event().IsSameEventClass(other_event->event_->Event()))
      return FilterResult::KeepIterating;

    if (!event_->CanCoalesceWith(*other_event->event_)) {
      // Two pointerevents may not be able to coalesce but we should continue
      // looking further down the queue if both of them were rawupdate or move
      // events and only their pointer_type, id, or event_type was different.
      if (AreCoalescablePointerRawUpdateEvents(*other_event))
        return FilterResult::KeepIterating;
      return FilterResult::StopIterating;
    }

    // If the other event was blocking store its callback to call later, but we
    // also save the trace_id to ensure the flow events correct show the
    // critical path.
    if (other_event->callback_) {
      blocking_coalesced_callbacks_.emplace_back(
          std::move(other_event->callback_),
          other_event->event_->latency_info().trace_id());
    }

    known_by_scheduler_count_ += other_event->known_by_scheduler_count_;
    event_->CoalesceWith(*other_event->event_);
    auto* metrics = metrics_ ? metrics_->AsScrollUpdate() : nullptr;
    auto* other_metrics = other_event->metrics_
                              ? other_event->metrics_->AsScrollUpdate()
                              : nullptr;
    if (metrics && other_metrics)
      metrics->CoalesceWith(*other_metrics);

    // The newest event (|other_item|) always wins when updating fields.
    originally_cancelable_ = other_event->originally_cancelable_;

    return FilterResult::CoalescedEvent;
  }

  bool IsWebInputEvent() const override { return true; }

  void Dispatch(MainThreadEventQueue* queue) override {
    if (RuntimeEnabledFeatures::UnblockTouchMoveEarlierEnabled() &&
        originally_cancelable_ &&
        event_->Event().GetType() == WebInputEvent::Type::kTouchMove) {
      auto* touch_event = static_cast<WebTouchEvent*>(event_->EventPointer());
      if (queue->GetMainThreadOnly().should_unblock_touch_moves) {
        // Though we have unblocked queued touch events when we set
        // should_unblock_touch_moves_ to true, there is still chance of newly
        // queued blocking touch events.
        touch_event->dispatch_type =
            WebInputEvent::DispatchType::kEventNonBlocking;
      }
      // If the touch move has been unblocked (above or in
      // HandleTouchScrollStartQueued()), run callbacks before dispatching.
      if (touch_event->dispatch_type ==
          WebInputEvent::DispatchType::kEventNonBlocking) {
        RunCallbacks(mojom::blink::InputEventResultState::kNotConsumed,
                     event_->latency_info(), nullptr, std::nullopt);
      }
    }

    HandledEventCallback callback =
        base::BindOnce(&QueuedWebInputEvent::HandledEvent,
                       base::Unretained(this), base::RetainedRef(queue));
    if (!queue->HandleEventOnMainThread(
            *event_, attribution(), std::move(metrics_), std::move(callback))) {
      // The |callback| won't be run, so our stored |callback_| should run
      // indicating error.
      HandledEvent(queue, mojom::blink::InputEventResultState::kNotConsumed,
                   event_->latency_info(), nullptr, std::nullopt);
    }
  }

  void HandledEvent(MainThreadEventQueue* queue,
                    mojom::blink::InputEventResultState ack_result,
                    const ui::LatencyInfo& latency_info,
                    mojom::blink::DidOverscrollParamsPtr overscroll,
                    std::optional<cc::TouchAction> touch_action) {
    RunCallbacks(ack_result, latency_info, std::move(overscroll), touch_action);

    // TODO(dtapuska): Change the scheduler API to take into account number of
    // events processed.
    for (size_t i = 0; i < known_by_scheduler_count_; ++i) {
      queue->widget_scheduler_->DidHandleInputEventOnMainThread(
          event_->Event(),
          ack_result == mojom::blink::InputEventResultState::kConsumed
              ? WebInputEventResult::kHandledApplication
              : WebInputEventResult::kNotHandled,
          queue->client_ ? queue->client_->RequestedMainFramePending() : false);
    }

    queue->UnblockQueuedBlockingTouchMovesIfNeeded(event_->Event(), ack_result);
  }

  struct CallbackInfo {
    HandledEventCallback callback;
    ui::LatencyInfo latency_info;
  };
  void TakeCallbacksInto(Vector<CallbackInfo>& callbacks) {
    if (callback_) {
      callbacks.emplace_back(std::move(callback_), event_->latency_info());
    }
    if (!blocking_coalesced_callbacks_.empty()) {
      ui::LatencyInfo coalesced_latency_info = event_->latency_info();
      coalesced_latency_info.set_coalesced();
      for (auto& callback : blocking_coalesced_callbacks_) {
        coalesced_latency_info.set_trace_id(callback.second);
        callbacks.emplace_back(std::move(callback.first),
                               coalesced_latency_info);
      }
      blocking_coalesced_callbacks_.clear();
    }
  }

  bool originally_cancelable() const { return originally_cancelable_; }

  const WebInputEventAttribution& attribution() const { return attribution_; }

  const WebInputEvent& Event() const { return event_->Event(); }

  WebCoalescedInputEvent* mutable_coalesced_event() { return event_.get(); }

  void SetQueuedTimeStamp(base::TimeTicks queued_time) {
    event_->EventPointer()->SetQueuedTimeStamp(queued_time);
  }

 private:
  void RunCallbacks(mojom::blink::InputEventResultState ack_result,
                    const ui::LatencyInfo& latency_info,
                    mojom::blink::DidOverscrollParamsPtr overscroll,
                    const std::optional<cc::TouchAction>& touch_action) {
    // callback_ is null if we have already run it, in cases
    // 1. the event had been a blocking touchmove before it was unblocked;
    // 2. the event is an non-blocking event, and its callback was called when
    //    the event was queued, then a blocking event was coalesced into the
    //    the event.
    if (callback_) {
      std::move(callback_).Run(ack_result, latency_info, std::move(overscroll),
                               touch_action);
    }

    if (!blocking_coalesced_callbacks_.empty()) {
      ui::LatencyInfo coalesced_latency_info = latency_info;
      coalesced_latency_info.set_coalesced();
      for (auto& callback : blocking_coalesced_callbacks_) {
        coalesced_latency_info.set_trace_id(callback.second);
        std::move(callback.first)
            .Run(ack_result, coalesced_latency_info, nullptr, std::nullopt);
      }
      blocking_coalesced_callbacks_.clear();
    }
  }

  FilterResult HandleTouchScrollStartQueued() {
    // A TouchScrollStart will queued after this touch move which will make all
    // previous touch moves that are queued uncancelable.
    switch (event_->Event().GetType()) {
      case WebInputEvent::Type::kTouchMove: {
        WebTouchEvent* touch_event =
            static_cast<WebTouchEvent*>(event_->EventPointer());
        if (touch_event->dispatch_type ==
            WebInputEvent::DispatchType::kBlocking) {
          touch_event->dispatch_type =
              WebInputEvent::DispatchType::kEventNonBlocking;
        }
        return FilterResult::KeepIterating;
      }
      case WebInputEvent::Type::kTouchStart:
      case WebInputEvent::Type::kTouchEnd:
        return FilterResult::StopIterating;
      default:
        return FilterResult::KeepIterating;
    }
  }

  std::unique_ptr<WebCoalescedInputEvent> event_;

  // Contains the pending callbacks to be called, along with their associated
  // trace_ids.
  base::circular_deque<std::pair<HandledEventCallback, int64_t>>
      blocking_coalesced_callbacks_;
  // Contains the number of non-blocking events coalesced.

  // Whether the received event was originally cancelable or not. The compositor
  // input handler can change the event based on presence of event handlers so
  // this is the state at which the renderer received the event from the
  // browser.
  bool originally_cancelable_;

  HandledEventCallback callback_;

  size_t known_by_scheduler_count_;

  const WebInputEventAttribution attribution_;

  std::unique_ptr<cc::EventMetrics> metrics_;
};

MainThreadEventQueue::MainThreadEventQueue(
    MainThreadEventQueueClient* client,
    const scoped_refptr<base::SingleThreadTaskRunner>& compositor_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<scheduler::WidgetScheduler> widget_scheduler,
    bool allow_raf_aligned_input)
    : client_(client),
      allow_raf_aligned_input_(allow_raf_aligned_input),
      main_task_runner_(std::move(main_task_runner)),
      widget_scheduler_(std::move(widget_scheduler)) {
  DCHECK(widget_scheduler_);
  raf_fallback_timer_ = std::make_unique<base::OneShotTimer>();
  raf_fallback_timer_->SetTaskRunner(main_task_runner_);

  event_predictor_ = std::make_unique<InputEventPrediction>(
      base::FeatureList::IsEnabled(blink::features::kResamplingInputEvents));

#if DCHECK_IS_ON()
  compositor_task_runner_ = compositor_task_runner;
#endif
}

MainThreadEventQueue::~MainThreadEventQueue() {}

bool MainThreadEventQueue::Allowed(const WebInputEvent& event,
                                   bool force_allow) {
  if (force_allow) {
    return true;
  }

  WebInputEvent::Type event_type = event.GetType();
  if (!IsGestureScroll(event_type)) {
    return true;
  }

  const WebGestureEvent& gesture_event =
      static_cast<const WebGestureEvent&>(event);
  if (event_type == WebInputEvent::Type::kGestureScrollBegin &&
      gesture_event.data.scroll_begin.cursor_control) {
    cursor_control_in_progress_ = true;
  }

  // The Android swipe-to-move-cursor feature still sends gesture scroll events
  // to the main thread.
  bool allowed = cursor_control_in_progress_;

  if (event_type == WebInputEvent::Type::kGestureScrollEnd &&
      cursor_control_in_progress_) {
    cursor_control_in_progress_ = false;
  }

  return allowed;
}

void MainThreadEventQueue::HandleEvent(
    std::unique_ptr<WebCoalescedInputEvent> event,
    DispatchType original_dispatch_type,
    mojom::blink::InputEventResultState ack_result,
    const WebInputEventAttribution& attribution,
    std::unique_ptr<cc::EventMetrics> metrics,
    HandledEventCallback callback,
    bool allow_main_gesture_scroll) {
  TRACE_EVENT2("input", "MainThreadEventQueue::HandleEvent", "dispatch_type",
               original_dispatch_type, "event_type", event->Event().GetType());
  DCHECK(original_dispatch_type == DispatchType::kBlocking ||
         original_dispatch_type == DispatchType::kNonBlocking);
  DCHECK(ack_result == mojom::blink::InputEventResultState::kSetNonBlocking ||
         ack_result ==
             mojom::blink::InputEventResultState::kSetNonBlockingDueToFling ||
         ack_result == mojom::blink::InputEventResultState::kNotConsumed);
  DCHECK(Allowed(event->Event(), allow_main_gesture_scroll));

  bool is_blocking =
      original_dispatch_type == DispatchType::kBlocking &&
      ack_result != mojom::blink::InputEventResultState::kSetNonBlocking;
  bool is_wheel = event->Event().GetType() == WebInputEvent::Type::kMouseWheel;
  bool is_touch = WebInputEvent::IsTouchEventType(event->Event().GetType());
  bool originally_cancelable = false;

  if (is_touch) {
    WebTouchEvent* touch_event =
        static_cast<WebTouchEvent*>(event->EventPointer());

    originally_cancelable =
        touch_event->dispatch_type == WebInputEvent::DispatchType::kBlocking;

    if (!is_blocking) {
      // Adjust the `dispatch_type` on the event since the compositor
      // determined all event listeners are passive.
      touch_event->dispatch_type =
          WebInputEvent::DispatchType::kListenersNonBlockingPassive;
    }

    bool& last_touch_start_forced_nonblocking_due_to_fling =
        GetCompositorThreadOnly()
            .last_touch_start_forced_nonblocking_due_to_fling;
    if (touch_event->GetType() == WebInputEvent::Type::kTouchStart) {
      last_touch_start_forced_nonblocking_due_to_fling = false;
    }
    if (touch_event->touch_start_or_first_touch_move &&
        touch_event->dispatch_type == WebInputEvent::DispatchType::kBlocking) {
      // If the touch start is forced to be passive due to fling, its following
      // touch move should also be passive.
      if (ack_result ==
              mojom::blink::InputEventResultState::kSetNonBlockingDueToFling ||
          last_touch_start_forced_nonblocking_due_to_fling) {
        touch_event->dispatch_type =
            WebInputEvent::DispatchType::kListenersForcedNonBlockingDueToFling;
        is_blocking = false;
        last_touch_start_forced_nonblocking_due_to_fling = true;
      }
    }

    // If the event is non-cancelable ACK it right away.
    if (is_blocking &&
        touch_event->dispatch_type != WebInputEvent::DispatchType::kBlocking) {
      is_blocking = false;
    }
  }

  if (is_wheel) {
    WebMouseWheelEvent* wheel_event =
        static_cast<WebMouseWheelEvent*>(event->EventPointer());
    originally_cancelable =
        wheel_event->dispatch_type == WebInputEvent::DispatchType::kBlocking;
    if (!is_blocking) {
      // Adjust the |dispatchType| on the event since the compositor
      // determined all event listeners are passive.
      wheel_event->dispatch_type =
          WebInputEvent::DispatchType::kListenersNonBlockingPassive;
    }
  }

  HandledEventCallback event_callback;
  if (is_blocking) {
    TRACE_EVENT_INSTANT0("input", "Blocking", TRACE_EVENT_SCOPE_THREAD);
    event_callback = std::move(callback);
  }

  if (has_pointerrawupdate_handlers_) {
    if (event->Event().GetType() == WebInputEvent::Type::kMouseMove) {
      auto raw_event = std::make_unique<WebCoalescedInputEvent>(
          std::make_unique<WebPointerEvent>(
              WebInputEvent::Type::kPointerRawUpdate,
              static_cast<const WebMouseEvent&>(event->Event())),
          event->latency_info());
      QueueEvent(QueuedWebInputEvent::CreateForRawEvent(
          std::move(raw_event), attribution, metrics.get()));
    } else if (event->Event().GetType() == WebInputEvent::Type::kTouchMove) {
      const WebTouchEvent& touch_event =
          static_cast<const WebTouchEvent&>(event->Event());
      for (unsigned i = 0; i < touch_event.touches_length; ++i) {
        const WebTouchPoint& touch_point = touch_event.touches[i];
        if (touch_point.state == WebTouchPoint::State::kStateMoved) {
          auto raw_event = std::make_unique<WebCoalescedInputEvent>(
              std::make_unique<WebPointerEvent>(touch_event, touch_point),
              event->latency_info());
          raw_event->EventPointer()->SetType(
              WebInputEvent::Type::kPointerRawUpdate);
          QueueEvent(QueuedWebInputEvent::CreateForRawEvent(
              std::move(raw_event), attribution, metrics.get()));
        }
      }
    }
  }

  ui::LatencyInfo cloned_latency_info;

  // Clone the latency info if we are calling the callback.
  if (callback)
    cloned_latency_info = event->latency_info();

  auto queued_event = std::make_unique<QueuedWebInputEvent>(
      std::move(event), originally_cancelable, std::move(event_callback),
      IsForwardedAndSchedulerKnown(ack_result), attribution,
      std::move(metrics));

  QueueEvent(std::move(queued_event));

  if (callback) {
    std::move(callback).Run(ack_result, cloned_latency_info, nullptr,
                            std::nullopt);
  }
}

void MainThreadEventQueue::QueueClosure(base::OnceClosure closure) {
  bool needs_post_task = false;
  std::unique_ptr<QueuedClosure> item(new QueuedClosure(std::move(closure)));
  {
    base::AutoLock lock(shared_state_lock_);
    shared_state_.events_.Enqueue(std::move(item));
    needs_post_task = !shared_state_.sent_post_task_;
    shared_state_.sent_post_task_ = true;
  }

  if (needs_post_task)
    PostTaskToMainThread();
}

void MainThreadEventQueue::PossiblyScheduleMainFrame() {
  bool needs_main_frame = false;
  {
    base::AutoLock lock(shared_state_lock_);
    if (!shared_state_.sent_main_frame_request_ &&
        !shared_state_.events_.empty() &&
        IsRafAlignedEvent(shared_state_.events_.front())) {
      needs_main_frame = true;
      shared_state_.sent_main_frame_request_ = true;
    }
  }
  if (needs_main_frame)
    SetNeedsMainFrame();
}

void MainThreadEventQueue::DispatchEvents() {
  size_t events_to_process;
  size_t queue_size;

  // Record the queue size so that we only process
  // that maximum number of events.
  {
    base::AutoLock lock(shared_state_lock_);
    shared_state_.sent_post_task_ = false;
    events_to_process = shared_state_.events_.size();

    // Don't process rAF aligned events at tail of queue.
    while (events_to_process > 0 &&
           !ShouldFlushQueue(shared_state_.events_.at(events_to_process - 1))) {
      --events_to_process;
    }
  }

  while (events_to_process--) {
    std::unique_ptr<MainThreadEventQueueTask> task;
    {
      base::AutoLock lock(shared_state_lock_);
      if (shared_state_.events_.empty())
        return;
      task = shared_state_.events_.Pop();
    }

    HandleEventResampling(task, base::TimeTicks::Now());
    // Dispatching the event is outside of critical section.
    task->Dispatch(this);
  }

  // Dispatch all raw move events as well regardless of where they are in the
  // queue
  {
    base::AutoLock lock(shared_state_lock_);
    queue_size = shared_state_.events_.size();
  }

  for (size_t current_task_index = 0; current_task_index < queue_size;
       ++current_task_index) {
    std::unique_ptr<MainThreadEventQueueTask> task;
    {
      base::AutoLock lock(shared_state_lock_);
      while (current_task_index < queue_size &&
             current_task_index < shared_state_.events_.size()) {
        if (!IsRafAlignedEvent(shared_state_.events_.at(current_task_index)))
          break;
        current_task_index++;
      }
      if (current_task_index >= queue_size ||
          current_task_index >= shared_state_.events_.size())
        break;
      if (IsRawUpdateEvent(shared_state_.events_.at(current_task_index))) {
        task = shared_state_.events_.remove(current_task_index);
        --queue_size;
        --current_task_index;
      } else if (!IsRafAlignedEvent(
                     shared_state_.events_.at(current_task_index))) {
        // Do not pass a non-rAF-aligned event to avoid delivering raw move
        // events and down/up events out of order to js.
        break;
      }
    }

    // Dispatching the event is outside of critical section.
    if (task)
      task->Dispatch(this);
  }

  PossiblyScheduleMainFrame();

  if (client_)
    client_->InputEventsDispatched(/*raf_aligned=*/false);
}

static bool IsAsyncTouchMove(
    const std::unique_ptr<MainThreadEventQueueTask>& queued_item) {
  if (!queued_item->IsWebInputEvent())
    return false;
  const QueuedWebInputEvent* event =
      static_cast<const QueuedWebInputEvent*>(queued_item.get());
  if (event->Event().GetType() != WebInputEvent::Type::kTouchMove)
    return false;
  const WebTouchEvent& touch_event =
      static_cast<const WebTouchEvent&>(event->Event());
  return touch_event.moved_beyond_slop_region &&
         !event->originally_cancelable();
}

void MainThreadEventQueue::RafFallbackTimerFired() {
  // This fallback fires when the browser doesn't produce main frames for a
  // variety of reasons. (eg. Tab gets hidden). We definitely don't want input
  // to stay forever in the queue.
  DispatchRafAlignedInput(base::TimeTicks::Now());
}

void MainThreadEventQueue::ClearRafFallbackTimerForTesting() {
  raf_fallback_timer_.reset();
}

bool MainThreadEventQueue::IsEmptyForTesting() {
  base::AutoLock lock(shared_state_lock_);
  return shared_state_.events_.empty();
}

void MainThreadEventQueue::DispatchRafAlignedInput(base::TimeTicks frame_time) {
  if (raf_fallback_timer_)
    raf_fallback_timer_->Stop();
  size_t queue_size_at_start;

  // Record the queue size so that we only process
  // that maximum number of events.
  {
    base::AutoLock lock(shared_state_lock_);
    shared_state_.sent_main_frame_request_ = false;
    queue_size_at_start = shared_state_.events_.size();
  }

  while (queue_size_at_start--) {
    std::unique_ptr<MainThreadEventQueueTask> task;
    {
      base::AutoLock lock(shared_state_lock_);

      if (shared_state_.events_.empty())
        return;

      if (IsRafAlignedEvent(shared_state_.events_.front())) {
        // Throttle touchmoves that are async.
        if (IsAsyncTouchMove(shared_state_.events_.front())) {
          if (shared_state_.events_.size() == 1 &&
              frame_time < shared_state_.last_async_touch_move_timestamp_ +
                               kAsyncTouchMoveInterval) {
            break;
          }
          shared_state_.last_async_touch_move_timestamp_ = frame_time;
        }
      }
      task = shared_state_.events_.Pop();
    }
    HandleEventResampling(task, frame_time);
    // Dispatching the event is outside of critical section.
    task->Dispatch(this);
  }

  PossiblyScheduleMainFrame();

  if (client_)
    client_->InputEventsDispatched(/*raf_aligned=*/true);
}

void MainThreadEventQueue::PostTaskToMainThread() {
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MainThreadEventQueue::DispatchEvents, this));
}

void MainThreadEventQueue::QueueEvent(
    std::unique_ptr<MainThreadEventQueueTask> event) {
  bool is_raf_aligned = IsRafAlignedEvent(event);
  bool needs_main_frame = false;
  bool needs_post_task = false;

  // Record the input event's type prior to enqueueing so that the scheduler
  // can be notified of its dispatch (if the event is not coalesced).
  bool is_input_event = event->IsWebInputEvent();
  WebInputEvent::Type input_event_type = WebInputEvent::Type::kUndefined;
  WebInputEventAttribution attribution;
  if (is_input_event) {
    auto* queued_input_event = static_cast<QueuedWebInputEvent*>(event.get());
    input_event_type = queued_input_event->Event().GetType();
    attribution = queued_input_event->attribution();
    queued_input_event->SetQueuedTimeStamp(base::TimeTicks::Now());
  }

  {
    base::AutoLock lock(shared_state_lock_);

    if (shared_state_.events_.Enqueue(std::move(event)) ==
        MainThreadEventQueueTaskList::EnqueueResult::kEnqueued) {
      if (!is_raf_aligned) {
        needs_post_task = !shared_state_.sent_post_task_;
        shared_state_.sent_post_task_ = true;
      } else {
        needs_main_frame = !shared_state_.sent_main_frame_request_;
        shared_state_.sent_main_frame_request_ = true;
      }

      // Notify the scheduler that we'll enqueue a task to the main thread.
      if (is_input_event) {
        widget_scheduler_->WillPostInputEventToMainThread(input_event_type,
                                                          attribution);
      }
    }
  }

  if (needs_post_task)
    PostTaskToMainThread();
  if (needs_main_frame)
    SetNeedsMainFrame();
}

bool MainThreadEventQueue::IsRawUpdateEvent(
    const std::unique_ptr<MainThreadEventQueueTask>& item) const {
  return item->IsWebInputEvent() &&
         static_cast<const QueuedWebInputEvent*>(item.get())
                 ->Event()
                 .GetType() == WebInputEvent::Type::kPointerRawUpdate;
}

bool MainThreadEventQueue::ShouldFlushQueue(
    const std::unique_ptr<MainThreadEventQueueTask>& item) const {
  if (IsRawUpdateEvent(item))
    return false;
  return !IsRafAlignedEvent(item);
}

bool MainThreadEventQueue::IsRafAlignedEvent(
    const std::unique_ptr<MainThreadEventQueueTask>& item) const {
  if (!item->IsWebInputEvent())
    return false;
  const QueuedWebInputEvent* event =
      static_cast<const QueuedWebInputEvent*>(item.get());
  switch (event->Event().GetType()) {
    case WebInputEvent::Type::kMouseMove:
    case WebInputEvent::Type::kMouseWheel:
    case WebInputEvent::Type::kTouchMove:
      return allow_raf_aligned_input_ && !needs_low_latency_ &&
             !needs_low_latency_until_pointer_up_ &&
             !needs_unbuffered_input_for_debugger_;
    default:
      return false;
  }
}

void MainThreadEventQueue::HandleEventResampling(
    const std::unique_ptr<MainThreadEventQueueTask>& item,
    base::TimeTicks frame_time) {
  if (item->IsWebInputEvent() && allow_raf_aligned_input_ && event_predictor_) {
    QueuedWebInputEvent* event = static_cast<QueuedWebInputEvent*>(item.get());
    event_predictor_->HandleEvents(*event->mutable_coalesced_event(),
                                   frame_time);
  }
}

bool MainThreadEventQueue::HandleEventOnMainThread(
    const WebCoalescedInputEvent& event,
    const WebInputEventAttribution& attribution,
    std::unique_ptr<cc::EventMetrics> metrics,
    HandledEventCallback handled_callback) {
  // Notify the scheduler that the main thread is about to execute handlers.
  widget_scheduler_->WillHandleInputEventOnMainThread(event.Event().GetType(),
                                                      attribution);

  bool handled = false;
  if (client_) {
    handled = client_->HandleInputEvent(event, std::move(metrics),
                                        std::move(handled_callback));
  }

  if (needs_low_latency_until_pointer_up_) {
    // Reset the needs low latency until pointer up mode if necessary.
    switch (event.Event().GetType()) {
      case WebInputEvent::Type::kMouseUp:
      case WebInputEvent::Type::kTouchCancel:
      case WebInputEvent::Type::kTouchEnd:
      case WebInputEvent::Type::kPointerCancel:
      case WebInputEvent::Type::kPointerUp:
        needs_low_latency_until_pointer_up_ = false;
        break;
      default:
        break;
    }
  }
  return handled;
}

void MainThreadEventQueue::SetNeedsMainFrame() {
  if (main_task_runner_->BelongsToCurrentThread()) {
    if (raf_fallback_timer_) {
      raf_fallback_timer_->Start(
          FROM_HERE, kMaxRafDelay,
          base::BindOnce(&MainThreadEventQueue::RafFallbackTimerFired, this));
    }
    if (client_)
      client_->SetNeedsMainFrame();
    return;
  }

  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MainThreadEventQueue::SetNeedsMainFrame, this));
}

void MainThreadEventQueue::ClearClient() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  client_ = nullptr;
  raf_fallback_timer_.reset();
}

void MainThreadEventQueue::SetNeedsLowLatency(bool low_latency) {
  needs_low_latency_ = low_latency;
}

void MainThreadEventQueue::SetNeedsUnbufferedInputForDebugger(bool unbuffered) {
  needs_unbuffered_input_for_debugger_ = unbuffered;
}

void MainThreadEventQueue::SetHasPointerRawUpdateEventHandlers(
    bool has_handlers) {
  has_pointerrawupdate_handlers_ = has_handlers;
}

void MainThreadEventQueue::RequestUnbufferedInputEvents() {
  needs_low_latency_until_pointer_up_ = true;
}

void MainThreadEventQueue::UnblockQueuedBlockingTouchMovesIfNeeded(
    const WebInputEvent& dispatched_event,
    mojom::blink::InputEventResultState ack_result) {
  if (!RuntimeEnabledFeatures::UnblockTouchMoveEarlierEnabled()) {
    return;
  }
  if (!WebInputEvent::IsTouchEventType(dispatched_event.GetType())) {
    return;
  }

  {
    bool& should_unblock_touch_moves =
        GetMainThreadOnly().should_unblock_touch_moves;
    bool& blocking_touch_start_not_consumed =
        GetMainThreadOnly().blocking_touch_start_not_consumed;
    auto& touch_event = static_cast<const WebTouchEvent&>(dispatched_event);
    if (touch_event.touch_start_or_first_touch_move) {
      bool is_not_consumed_blocking =
          touch_event.dispatch_type == WebInputEvent::DispatchType::kBlocking &&
          ack_result == mojom::blink::InputEventResultState::kNotConsumed;
      if (touch_event.GetType() == WebInputEvent::Type::kTouchStart) {
        blocking_touch_start_not_consumed = is_not_consumed_blocking;
        should_unblock_touch_moves = false;
      } else {
        // `event` is the first touch move.
        CHECK_EQ(touch_event.GetType(), WebInputEvent::Type::kTouchMove);
        should_unblock_touch_moves =
            blocking_touch_start_not_consumed && is_not_consumed_blocking;
      }
    }
    if (!should_unblock_touch_moves) {
      return;
    }
  }

  // Neither the touchstart nor the first touchmove was consumed. The browser
  // process will make the remaining of the touch sequence non-blocking, but
  // we need to unblock the already queued blocking touchmove events and run
  // the callbacks (collected in a vector to avoid locking during callbacks).
  Vector<QueuedWebInputEvent::CallbackInfo> callbacks;
  {
    base::AutoLock lock(shared_state_lock_);
    for (size_t i = 0; i < shared_state_.events_.size(); ++i) {
      MainThreadEventQueueTask* task = shared_state_.events_.at(i).get();
      if (!task->IsWebInputEvent()) {
        continue;
      }
      auto* queued_event = static_cast<QueuedWebInputEvent*>(task);
      WebInputEvent* event =
          queued_event->mutable_coalesced_event()->EventPointer();
      if (event->GetType() == WebInputEvent::Type::kTouchStart ||
          event->GetType() == WebInputEvent::Type::kTouchEnd) {
        break;
      }
      if (event->GetType() != WebInputEvent::Type::kTouchMove) {
        continue;
      }

      auto* touch_event = static_cast<WebTouchEvent*>(event);
      if (!touch_event->touch_start_or_first_touch_move &&
          touch_event->dispatch_type ==
              WebInputEvent::DispatchType::kBlocking) {
        touch_event->dispatch_type =
            WebInputEvent::DispatchType::kEventNonBlocking;
        queued_event->TakeCallbacksInto(callbacks);
      }
    }
  }
  for (auto& callback_info : callbacks) {
    std::move(callback_info.callback)
        .Run(mojom::blink::InputEventResultState::kNotConsumed,
             callback_info.latency_info, nullptr, std::nullopt);
  }
}

MainThreadEventQueue::MainThreadOnly&
MainThreadEventQueue::GetMainThreadOnly() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return main_thread_only_;
}

MainThreadEventQueue::CompositorThreadOnly&
MainThreadEventQueue::GetCompositorThreadOnly() {
#if DCHECK_IS_ON()
  DCHECK(compositor_task_runner_->BelongsToCurrentThread());
#endif
  return compositor_thread_only_;
}

}  // namespace blink
