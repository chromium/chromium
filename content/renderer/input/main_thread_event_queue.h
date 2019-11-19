// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_INPUT_MAIN_THREAD_EVENT_QUEUE_H_
#define CONTENT_RENDERER_INPUT_MAIN_THREAD_EVENT_QUEUE_H_

#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "cc/input/touch_action.h"
#include "content/common/content_export.h"
#include "content/common/input/input_event_dispatch_type.h"
#include "content/public/common/content_features.h"
#include "content/public/common/input_event_ack_state.h"
#include "content/renderer/input/input_event_prediction.h"
#include "content/renderer/input/main_thread_event_queue_task_list.h"
#include "content/renderer/input/scoped_web_input_event_with_latency_info.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "ui/events/blink/did_overscroll_params.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/latency/latency_info.h"

namespace content {

using HandledEventCallback =
    base::OnceCallback<void(InputEventAckState ack_state,
                            const ui::LatencyInfo& latency_info,
                            std::unique_ptr<ui::DidOverscrollParams>,
                            base::Optional<cc::TouchAction>)>;

// All interaction with the MainThreadEventQueueClient will occur
// on the main thread.
class CONTENT_EXPORT MainThreadEventQueueClient {
 public:
  // Handle an |event| that was previously queued (possibly coalesced with
  // another event). Returns false if the event will not be handled, and the
  // |handled_callback| will not be run.
  virtual bool HandleInputEvent(const blink::WebCoalescedInputEvent& event,
                                const ui::LatencyInfo& latency_info,
                                HandledEventCallback handled_callback) = 0;
  // Requests a BeginMainFrame callback from the compositor.
  virtual void SetNeedsMainFrame() = 0;
};

// MainThreadEventQueue implements a queue for events that need to be
// queued between the compositor and main threads. This queue is managed
// by a lock where events are enqueued by the compositor thread
// and dequeued by the main thread.
//
// Below some example flows are how the code behaves.
// Legend: B=Browser, C=Compositor, M=Main Thread, NB=Non-blocking
//         BL=Blocking, PT=Post Task, ACK=Acknowledgement
//
// Normal blocking event sent to main thread.
//   B        C        M
//   ---(BL)-->
//         (queue)
//            ---(PT)-->
//                  (deque)
//   <-------(ACK)------
//
// Non-blocking event sent to main thread.
//   B        C        M
//   ---(NB)-->
//         (queue)
//            ---(PT)-->
//                  (deque)
//
// Non-blocking followed by blocking event sent to main thread.
//   B        C        M
//   ---(NB)-->
//         (queue)
//            ---(PT)-->
//   ---(BL)-->
//         (queue)
//            ---(PT)-->
//                  (deque)
//                  (deque)
//   <-------(ACK)------
//
class CONTENT_EXPORT MainThreadEventQueue
    : public base::RefCountedThreadSafe<MainThreadEventQueue> {
 public:
  MainThreadEventQueue(
      MainThreadEventQueueClient* client,
      const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner,
      blink::scheduler::WebThreadScheduler* main_thread_scheduler,
      bool allow_raf_aligned_input);

  // Called once the compositor has handled |event| and indicated that it is
  // a non-blocking event to be queued to the main thread.
  void HandleEvent(ui::WebScopedInputEvent event,
                   const ui::LatencyInfo& latency,
                   InputEventDispatchType dispatch_type,
                   InputEventAckState ack_result,
                   HandledEventCallback handled_callback);
  void DispatchRafAlignedInput(base::TimeTicks frame_time);
  void QueueClosure(base::OnceClosure closure);

  void ClearClient();
  void SetNeedsLowLatency(bool low_latency);
  void SetNeedsUnbufferedInputForDebugger(bool unbuffered);

  void HasPointerRawUpdateEventHandlers(bool has_handlers);

  // Request unbuffered input events until next pointerup.
  void RequestUnbufferedInputEvents();

  // Resampling event before dispatch it.
  void HandleEventResampling(
      const std::unique_ptr<MainThreadEventQueueTask>& item,
      base::TimeTicks frame_time);

  static bool IsForwardedAndSchedulerKnown(InputEventAckState ack_state) {
    return ack_state == INPUT_EVENT_ACK_STATE_NOT_CONSUMED ||
           ack_state == INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING_DUE_TO_FLING;
  }

 protected:
  friend class base::RefCountedThreadSafe<MainThreadEventQueue>;
  virtual ~MainThreadEventQueue();
  void QueueEvent(std::unique_ptr<MainThreadEventQueueTask> event);
  void PostTaskToMainThread();
  void DispatchEvents();
  void PossiblyScheduleMainFrame();
  void SetNeedsMainFrame();
  // Returns false if the event can not be handled and the HandledEventCallback
  // will not be run.
  bool HandleEventOnMainThread(const blink::WebCoalescedInputEvent& event,
                               const ui::LatencyInfo& latency,
                               HandledEventCallback handled_callback);

  bool IsRawUpdateEvent(
      const std::unique_ptr<MainThreadEventQueueTask>& item) const;
  bool ShouldFlushQueue(
      const std::unique_ptr<MainThreadEventQueueTask>& item) const;
  bool IsRafAlignedEvent(
      const std::unique_ptr<MainThreadEventQueueTask>& item) const;
  void RafFallbackTimerFired();

  void set_use_raf_fallback_timer(bool use_timer) {
    use_raf_fallback_timer_ = use_timer;
  }

  friend class QueuedWebInputEvent;
  friend class MainThreadEventQueueTest;
  friend class MainThreadEventQueueInitializationTest;
  MainThreadEventQueueClient* client_;
  bool last_touch_start_forced_nonblocking_due_to_fling_;
  bool enable_fling_passive_listener_flag_;
  bool needs_low_latency_;
  bool needs_unbuffered_input_for_debugger_;
  bool allow_raf_aligned_input_;
  bool needs_low_latency_until_pointer_up_ = false;
  bool has_pointerrawupdate_handlers_ = false;

  // Contains data to be shared between main thread and compositor thread.
  struct SharedState {
    SharedState();
    ~SharedState();

    MainThreadEventQueueTaskList events_;
    // A BeginMainFrame has been requested but not received yet.
    bool sent_main_frame_request_;
    // A PostTask to the main thread has been sent but not executed yet.
    bool sent_post_task_;
    base::TimeTicks last_async_touch_move_timestamp_;
  };

  // Lock used to serialize |shared_state_|.
  base::Lock shared_state_lock_;
  SharedState shared_state_;

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  blink::scheduler::WebThreadScheduler* main_thread_scheduler_;
  base::OneShotTimer raf_fallback_timer_;
  bool use_raf_fallback_timer_;

  std::unique_ptr<InputEventPrediction> event_predictor_;

  DISALLOW_COPY_AND_ASSIGN(MainThreadEventQueue);
};

}  // namespace content

#endif  // CONTENT_RENDERER_INPUT_MAIN_THREAD_EVENT_QUEUE_H_
