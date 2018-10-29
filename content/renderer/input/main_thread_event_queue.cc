// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/input/main_thread_event_queue.h"

#include "base/containers/circular_deque.h"
#include "base/metrics/histogram_macros.h"
#include "content/common/input/event_with_latency_info.h"
#include "content/common/input_messages.h"
#include "content/renderer/render_widget.h"

namespace content {

namespace {

constexpr base::TimeDelta kMaxRafDelay =
    base::TimeDelta::FromMilliseconds(5 * 1000);

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
constexpr base::TimeDelta kAsyncTouchMoveInterval =
    base::TimeDelta::FromMilliseconds(200);

}  // namespace

class QueuedWebInputEvent : public ScopedWebInputEventWithLatencyInfo,
                            public MainThreadEventQueueTask {
 public:
  QueuedWebInputEvent(ui::WebScopedInputEvent event,
                      const ui::LatencyInfo& latency,
                      bool originally_cancelable,
                      HandledEventCallback callback,
                      bool known_by_scheduler)
      : ScopedWebInputEventWithLatencyInfo(std::move(event), latency),
        non_blocking_coalesced_count_(0),
        creation_timestamp_(base::TimeTicks::Now()),
        last_coalesced_timestamp_(creation_timestamp_),
        originally_cancelable_(originally_cancelable),
        callback_(std::move(callback)),
        known_by_scheduler_count_(known_by_scheduler ? 1 : 0) {}

  ~QueuedWebInputEvent() override {}

  bool ArePointerMoveEventTypes(QueuedWebInputEvent* other_event) {
    // There is no pointermove at this point in the queue.
    DCHECK(event().GetType() != WebInputEvent::kPointerMove &&
           other_event->event().GetType() != WebInputEvent::kPointerMove);
    return event().GetType() == WebInputEvent::kPointerRawMove &&
           other_event->event().GetType() == WebInputEvent::kPointerRawMove;
  }

  FilterResult FilterNewEvent(MainThreadEventQueueTask* other_task) override {
    if (!other_task->IsWebInputEvent())
      return FilterResult::StopIterating;

    QueuedWebInputEvent* other_event =
        static_cast<QueuedWebInputEvent*>(other_task);
    if (other_event->event().GetType() ==
        blink::WebInputEvent::kTouchScrollStarted) {
      return HandleTouchScrollStartQueued();
    }

    if (!event().IsSameEventClass(other_event->event()))
      return FilterResult::KeepIterating;

    if (!ScopedWebInputEventWithLatencyInfo::CanCoalesceWith(*other_event)) {
      // Two pointerevents may not be able to coalesce but we should continue
      // looking further down the queue if both of them were rawmove or move
      // events and only their pointer_type, id, or event_type was different.
      if (ArePointerMoveEventTypes(other_event))
        return FilterResult::KeepIterating;
      return FilterResult::StopIterating;
    }

    // If the other event was blocking store its callback to call later.
    if (other_event->callback_) {
      blocking_coalesced_callbacks_.push_back(
          std::move(other_event->callback_));
    } else {
      non_blocking_coalesced_count_++;
    }
    known_by_scheduler_count_ += other_event->known_by_scheduler_count_;
    ScopedWebInputEventWithLatencyInfo::CoalesceWith(*other_event);
    last_coalesced_timestamp_ = base::TimeTicks::Now();

    // The newest event (|other_item|) always wins when updating fields.
    originally_cancelable_ = other_event->originally_cancelable_;

    return FilterResult::CoalescedEvent;
  }

  bool IsWebInputEvent() const override { return true; }

  void Dispatch(MainThreadEventQueue* queue) override {
    HandledEventCallback callback =
        base::BindOnce(&QueuedWebInputEvent::HandledEvent,
                       base::Unretained(this), base::RetainedRef(queue));
    queue->HandleEventOnMainThread(coalesced_event(), latencyInfo(),
                                   std::move(callback));
  }

  void HandledEvent(MainThreadEventQueue* queue,
                    InputEventAckState ack_result,
                    const ui::LatencyInfo& latency_info,
                    std::unique_ptr<ui::DidOverscrollParams> overscroll,
                    base::Optional<cc::TouchAction> touch_action) {
    if (callback_) {
      std::move(callback_).Run(ack_result, latency_info, std::move(overscroll),
                               touch_action);
    } else {
      DCHECK(!overscroll) << "Unexpected overscroll for un-acked event";
    }

    if (!blocking_coalesced_callbacks_.empty()) {
      ui::LatencyInfo coalesced_latency_info = latency_info;
      coalesced_latency_info.set_coalesced();
      for (auto&& callback : blocking_coalesced_callbacks_) {
        std::move(callback).Run(ack_result, coalesced_latency_info, nullptr,
                                base::nullopt);
      }
    }

    if (queue->main_thread_scheduler_) {
      // TODO(dtapuska): Change the scheduler API to take into account number of
      // events processed.
      for (size_t i = 0; i < known_by_scheduler_count_; ++i) {
        queue->main_thread_scheduler_->DidHandleInputEventOnMainThread(
            event(), ack_result == INPUT_EVENT_ACK_STATE_CONSUMED
                         ? blink::WebInputEventResult::kHandledApplication
                         : blink::WebInputEventResult::kNotHandled);
      }
    }
  }

  bool originallyCancelable() const { return originally_cancelable_; }

 private:
  FilterResult HandleTouchScrollStartQueued() {
    // A TouchScrollStart will queued after this touch move which will make all
    // previous touch moves that are queued uncancelable.
    switch (event().GetType()) {
      case blink::WebInputEvent::kTouchMove: {
        blink::WebTouchEvent& touch_event =
            static_cast<blink::WebTouchEvent&>(event());
        if (touch_event.dispatch_type ==
            blink::WebInputEvent::DispatchType::kBlocking) {
          touch_event.dispatch_type =
              blink::WebInputEvent::DispatchType::kEventNonBlocking;
        }
        return FilterResult::KeepIterating;
      }
      case blink::WebInputEvent::kTouchStart:
      case blink::WebInputEvent::kTouchEnd:
        return FilterResult::StopIterating;
      default:
        return FilterResult::KeepIterating;
    }
  }

  base::TimeTicks creationTimestamp() const { return creation_timestamp_; }
  base::TimeTicks lastCoalescedTimestamp() const {
    return last_coalesced_timestamp_;
  }

  size_t coalescedCount() const {
    return non_blocking_coalesced_count_ + blocking_coalesced_callbacks_.size();
  }

  bool IsContinuousEvent() const {
    switch (event().GetType()) {
      case blink::WebInputEvent::kMouseMove:
      case blink::WebInputEvent::kMouseWheel:
      case blink::WebInputEvent::kTouchMove:
        return true;
      default:
        return false;
    }
  }

  // Contains the pending callbacks to be called.
  base::circular_deque<HandledEventCallback> blocking_coalesced_callbacks_;
  // Contains the number of non-blocking events coalesced.
  size_t non_blocking_coalesced_count_;
  base::TimeTicks creation_timestamp_;
  base::TimeTicks last_coalesced_timestamp_;

  // Whether the received event was originally cancelable or not. The compositor
  // input handler can change the event based on presence of event handlers so
  // this is the state at which the renderer received the event from the
  // browser.
  bool originally_cancelable_;

  HandledEventCallback callback_;

  size_t known_by_scheduler_count_;
};

MainThreadEventQueue::SharedState::SharedState()
    : sent_main_frame_request_(false), sent_post_task_(false) {}

MainThreadEventQueue::SharedState::~SharedState() {}

MainThreadEventQueue::MainThreadEventQueue(
    MainThreadEventQueueClient* client,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner,
    blink::scheduler::WebThreadScheduler* main_thread_scheduler,
    bool allow_raf_aligned_input)
    : client_(client),
      last_touch_start_forced_nonblocking_due_to_fling_(false),
      enable_fling_passive_listener_flag_(base::FeatureList::IsEnabled(
          features::kPassiveEventListenersDueToFling)),
      needs_low_latency_(false),
      allow_raf_aligned_input_(allow_raf_aligned_input),
      main_task_runner_(main_task_runner),
      main_thread_scheduler_(main_thread_scheduler),
      use_raf_fallback_timer_(true) {
  raf_fallback_timer_.SetTaskRunner(main_task_runner);

  event_predictor_ = std::make_unique<InputEventPrediction>(
      base::FeatureList::IsEnabled(features::kResamplingInputEvents));
}

MainThreadEventQueue::~MainThreadEventQueue() {}

void MainThreadEventQueue::HandleEvent(
    ui::WebScopedInputEvent event,
    const ui::LatencyInfo& latency,
    InputEventDispatchType original_dispatch_type,
    InputEventAckState ack_result,
    HandledEventCallback callback) {
  DCHECK(original_dispatch_type == DISPATCH_TYPE_BLOCKING ||
         original_dispatch_type == DISPATCH_TYPE_NON_BLOCKING);
  DCHECK(ack_result == INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING ||
         ack_result == INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING_DUE_TO_FLING ||
         ack_result == INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  bool non_blocking = original_dispatch_type == DISPATCH_TYPE_NON_BLOCKING ||
                      ack_result == INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING;
  bool is_wheel = event->GetType() == blink::WebInputEvent::kMouseWheel;
  bool is_touch = blink::WebInputEvent::IsTouchEventType(event->GetType());
  bool originally_cancelable = false;

  if (is_touch) {
    blink::WebTouchEvent* touch_event =
        static_cast<blink::WebTouchEvent*>(event.get());

    originally_cancelable =
        touch_event->dispatch_type == blink::WebInputEvent::kBlocking;

    // Adjust the |dispatchType| on the event since the compositor
    // determined all event listeners are passive.
    if (non_blocking) {
      touch_event->dispatch_type =
          blink::WebInputEvent::kListenersNonBlockingPassive;
    }
    if (touch_event->GetType() == blink::WebInputEvent::kTouchStart)
      last_touch_start_forced_nonblocking_due_to_fling_ = false;

    if (enable_fling_passive_listener_flag_ &&
        touch_event->touch_start_or_first_touch_move &&
        touch_event->dispatch_type == blink::WebInputEvent::kBlocking) {
      // If the touch start is forced to be passive due to fling, its following
      // touch move should also be passive.
      if (ack_result == INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING_DUE_TO_FLING ||
          last_touch_start_forced_nonblocking_due_to_fling_) {
        touch_event->dispatch_type =
            blink::WebInputEvent::kListenersForcedNonBlockingDueToFling;
        non_blocking = true;
        last_touch_start_forced_nonblocking_due_to_fling_ = true;
      }
    }

    // If the event is non-cancelable ACK it right away.
    if (!non_blocking &&
        touch_event->dispatch_type != blink::WebInputEvent::kBlocking)
      non_blocking = true;
  }

  if (is_wheel) {
    blink::WebMouseWheelEvent* wheel_event =
        static_cast<blink::WebMouseWheelEvent*>(event.get());
    originally_cancelable =
        wheel_event->dispatch_type == blink::WebInputEvent::kBlocking;
    if (non_blocking) {
      // Adjust the |dispatchType| on the event since the compositor
      // determined all event listeners are passive.
      wheel_event->dispatch_type =
          blink::WebInputEvent::kListenersNonBlockingPassive;
    }
  }

  HandledEventCallback event_callback;
  if (!non_blocking) {
    event_callback = std::move(callback);
  }

  if (has_pointerrawmove_handlers_) {
    if (event->GetType() == WebInputEvent::kMouseMove) {
      ui::WebScopedInputEvent raw_event(new blink::WebPointerEvent(
          WebInputEvent::kPointerRawMove,
          *(static_cast<blink::WebMouseEvent*>(event.get()))));
      std::unique_ptr<QueuedWebInputEvent> raw_queued_event(
          new QueuedWebInputEvent(std::move(raw_event), latency, false,
                                  HandledEventCallback(), false));

      QueueEvent(std::move(raw_queued_event));
    } else if (event->GetType() == WebInputEvent::kTouchMove) {
      const blink::WebTouchEvent& touch_event =
          *static_cast<const blink::WebTouchEvent*>(event.get());
      for (unsigned i = 0; i < touch_event.touches_length; ++i) {
        const blink::WebTouchPoint& touch_point = touch_event.touches[i];
        if (touch_point.state == blink::WebTouchPoint::kStateMoved) {
          ui::WebScopedInputEvent raw_event(
              new blink::WebPointerEvent(touch_event, touch_point));
          raw_event->SetType(WebInputEvent::kPointerRawMove);
          std::unique_ptr<QueuedWebInputEvent> raw_queued_event(
              new QueuedWebInputEvent(std::move(raw_event), latency, false,
                                      HandledEventCallback(), false));
          QueueEvent(std::move(raw_queued_event));
        }
      }
    }
  }

  std::unique_ptr<QueuedWebInputEvent> queued_event(new QueuedWebInputEvent(
      std::move(event), latency, originally_cancelable,
      std::move(event_callback), IsForwardedAndSchedulerKnown(ack_result)));

  QueueEvent(std::move(queued_event));

  if (callback)
    std::move(callback).Run(ack_result, latency, nullptr, base::nullopt);
}

void MainThreadEventQueue::QueueClosure(base::OnceClosure closure) {
  bool needs_post_task = false;
  std::unique_ptr<QueuedClosure> item(new QueuedClosure(std::move(closure)));
  {
    base::AutoLock lock(shared_state_lock_);
    shared_state_.events_.Queue(std::move(item));
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
      if (IsRawMoveEvent(shared_state_.events_.at(current_task_index))) {
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
}

static bool IsAsyncTouchMove(
    const std::unique_ptr<MainThreadEventQueueTask>& queued_item) {
  if (!queued_item->IsWebInputEvent())
    return false;
  const QueuedWebInputEvent* event =
      static_cast<const QueuedWebInputEvent*>(queued_item.get());
  if (event->event().GetType() != blink::WebInputEvent::kTouchMove)
    return false;
  const blink::WebTouchEvent& touch_event =
      static_cast<const blink::WebTouchEvent&>(event->event());
  return touch_event.moved_beyond_slop_region && !event->originallyCancelable();
}

void MainThreadEventQueue::RafFallbackTimerFired() {
  UMA_HISTOGRAM_BOOLEAN("Event.MainThreadEventQueue.FlushQueueNoBeginMainFrame",
                        true);
  DispatchRafAlignedInput(base::TimeTicks::Now());
}

void MainThreadEventQueue::DispatchRafAlignedInput(base::TimeTicks frame_time) {
  raf_fallback_timer_.Stop();
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
  {
    base::AutoLock lock(shared_state_lock_);
    size_t size_before = shared_state_.events_.size();
    shared_state_.events_.Queue(std::move(event));
    size_t size_after = shared_state_.events_.size();

    if (size_before != size_after) {
      if (!is_raf_aligned) {
        needs_post_task = !shared_state_.sent_post_task_;
        shared_state_.sent_post_task_ = true;
      } else {
        needs_main_frame = !shared_state_.sent_main_frame_request_;
        shared_state_.sent_main_frame_request_ = true;
      }
    }
  }

  if (needs_post_task)
    PostTaskToMainThread();
  if (needs_main_frame)
    SetNeedsMainFrame();
}

bool MainThreadEventQueue::IsRawMoveEvent(
    const std::unique_ptr<MainThreadEventQueueTask>& item) const {
  return item->IsWebInputEvent() &&
         static_cast<const QueuedWebInputEvent*>(item.get())
                 ->event()
                 .GetType() == blink::WebInputEvent::kPointerRawMove;
}

bool MainThreadEventQueue::ShouldFlushQueue(
    const std::unique_ptr<MainThreadEventQueueTask>& item) const {
  if (IsRawMoveEvent(item))
    return false;
  return !IsRafAlignedEvent(item);
}

bool MainThreadEventQueue::IsRafAlignedEvent(
    const std::unique_ptr<MainThreadEventQueueTask>& item) const {
  if (!item->IsWebInputEvent())
    return false;
  const QueuedWebInputEvent* event =
      static_cast<const QueuedWebInputEvent*>(item.get());
  switch (event->event().GetType()) {
    case blink::WebInputEvent::kMouseMove:
    case blink::WebInputEvent::kMouseWheel:
    case blink::WebInputEvent::kTouchMove:
      return allow_raf_aligned_input_ && !needs_low_latency_ &&
             !needs_low_latency_until_pointer_up_;
    default:
      return false;
  }
}

void MainThreadEventQueue::HandleEventResampling(
    const std::unique_ptr<MainThreadEventQueueTask>& item,
    base::TimeTicks frame_time) {
  if (item->IsWebInputEvent() && allow_raf_aligned_input_ && event_predictor_) {
    QueuedWebInputEvent* event = static_cast<QueuedWebInputEvent*>(item.get());
    event_predictor_->HandleEvents(event->coalesced_event(), frame_time);
  }
}

void MainThreadEventQueue::HandleEventOnMainThread(
    const blink::WebCoalescedInputEvent& event,
    const ui::LatencyInfo& latency,
    HandledEventCallback handled_callback) {
  if (client_)
    client_->HandleInputEvent(event, latency, std::move(handled_callback));

  if (needs_low_latency_until_pointer_up_) {
    // Reset the needs low latency until pointer up mode if necessary.
    switch (event.Event().GetType()) {
      case blink::WebInputEvent::kMouseUp:
      case blink::WebInputEvent::kTouchCancel:
      case blink::WebInputEvent::kTouchEnd:
      case blink::WebInputEvent::kPointerCancel:
      case blink::WebInputEvent::kPointerUp:
        needs_low_latency_until_pointer_up_ = false;
        break;
      default:
        break;
    }
  }
}

void MainThreadEventQueue::SetNeedsMainFrame() {
  if (main_task_runner_->BelongsToCurrentThread()) {
    if (use_raf_fallback_timer_) {
      raf_fallback_timer_.Start(
          FROM_HERE, kMaxRafDelay,
          base::BindOnce(&MainThreadEventQueue::RafFallbackTimerFired, this));
    }
    if (client_)
      client_->SetNeedsMainFrame();
    if (main_thread_scheduler_)
      main_thread_scheduler_->OnMainFrameRequestedForInput();
    return;
  }

  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MainThreadEventQueue::SetNeedsMainFrame, this));
}

void MainThreadEventQueue::ClearClient() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  client_ = nullptr;
}

void MainThreadEventQueue::SetNeedsLowLatency(bool low_latency) {
  needs_low_latency_ = low_latency;
}

void MainThreadEventQueue::HasPointerRawMoveEventHandlers(bool has_handlers) {
  has_pointerrawmove_handlers_ = has_handlers;
}

void MainThreadEventQueue::RequestUnbufferedInputEvents() {
  needs_low_latency_until_pointer_up_ = true;
}

}  // namespace content
