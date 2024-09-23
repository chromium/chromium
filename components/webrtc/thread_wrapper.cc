// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webrtc/thread_wrapper.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "third_party/webrtc/rtc_base/physical_socket_server.h"
#include "third_party/webrtc_overrides/api/location.h"
#include "third_party/webrtc_overrides/metronome_source.h"
#include "third_party/webrtc_overrides/timer_based_tick_provider.h"

namespace webrtc {
namespace {

constexpr base::TimeDelta kTaskLatencySampleDuration = base::Seconds(3);

constinit thread_local ThreadWrapper* jingle_thread_wrapper = nullptr;

}  // namespace

// Class intended to conditionally live for the duration of ThreadWrapper
// that periodically captures task latencies (definition in docs for
// SetLatencyAndTaskDurationCallbacks).
class ThreadWrapper::PostTaskLatencySampler {
 public:
  PostTaskLatencySampler(
      ::scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      SampledDurationCallback task_latency_callback)
      : task_runner_(task_runner),
        task_latency_callback_(std::move(task_latency_callback)) {
    ScheduleDelayedSample();
  }

  bool ShouldSampleNextTaskDuration() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(current_);
    bool time_to_sample = should_sample_next_task_duration_;
    should_sample_next_task_duration_ = false;
    return time_to_sample;
  }

 private:
  void ScheduleDelayedSample() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(current_);
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&PostTaskLatencySampler::TakeSample,
                       base::Unretained(this)),
        kTaskLatencySampleDuration);
  }

  void TakeSample() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(current_);
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&PostTaskLatencySampler::FinishSample,
                       base::Unretained(this), base::TimeTicks::Now()));
  }

  void FinishSample(base::TimeTicks post_timestamp) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(current_);
    task_latency_callback_.Run(base::TimeTicks::Now() - post_timestamp);
    ScheduleDelayedSample();
    should_sample_next_task_duration_ = true;
  }

  SEQUENCE_CHECKER(current_);
  ::scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::RepeatingCallback<void(base::TimeDelta)> task_latency_callback_
      GUARDED_BY_CONTEXT(current_);
  bool should_sample_next_task_duration_ GUARDED_BY_CONTEXT(current_) = false;
};

struct ThreadWrapper::PendingSend {
  explicit PendingSend(rtc::FunctionView<void()> functor)
      : functor(functor),
        done_event(base::WaitableEvent::ResetPolicy::MANUAL,
                   base::WaitableEvent::InitialState::NOT_SIGNALED) {}

  rtc::FunctionView<void()> functor;
  base::WaitableEvent done_event;
};

// static
void ThreadWrapper::EnsureForCurrentMessageLoop() {
  if (ThreadWrapper::current() == nullptr) {
    std::unique_ptr<ThreadWrapper> wrapper = ThreadWrapper::WrapTaskRunner(
        base::SingleThreadTaskRunner::GetCurrentDefault());
    base::CurrentThread::Get()->AddDestructionObserver(wrapper.release());
  }

  DCHECK_EQ(rtc::Thread::Current(), current());
}

std::unique_ptr<ThreadWrapper> ThreadWrapper::WrapTaskRunner(
    ::scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(task_runner->BelongsToCurrentThread());
  return base::WrapUnique(new ThreadWrapper(task_runner));
}

// static
ThreadWrapper* ThreadWrapper::current() {
  return jingle_thread_wrapper;
}

void ThreadWrapper::SetLatencyAndTaskDurationCallbacks(
    SampledDurationCallback task_latency_callback,
    SampledDurationCallback task_duration_callback) {
  task_latency_callback_ = std::move(task_latency_callback);
  task_duration_callback_ = std::move(task_duration_callback);
}

ThreadWrapper::ThreadWrapper(
    ::scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : Thread(std::make_unique<rtc::PhysicalSocketServer>()),
      resetter_(&jingle_thread_wrapper, this, nullptr),
      task_runner_(task_runner),
      send_allowed_(false),
      pending_send_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                          base::WaitableEvent::InitialState::NOT_SIGNALED) {
  DCHECK(task_runner->BelongsToCurrentThread());
  DCHECK(!rtc::Thread::Current());
  weak_ptr_ = weak_ptr_factory_.GetWeakPtr();
  rtc::ThreadManager::Add(this);
  SafeWrapCurrent();
}

ThreadWrapper::~ThreadWrapper() {
  DCHECK_EQ(this, ThreadWrapper::current());
  DCHECK_EQ(this, rtc::Thread::Current());

  UnwrapCurrent();
  rtc::ThreadManager::Instance()->SetCurrentThread(nullptr);
  rtc::ThreadManager::Remove(this);

  CHECK(pending_send_messages_.empty());
  coalesced_tasks_.Clear();
}

rtc::SocketServer* ThreadWrapper::SocketServer() {
  return rtc::Thread::socketserver();
}

void ThreadWrapper::WillDestroyCurrentMessageLoop() {
  delete this;
}

void ThreadWrapper::BlockingCallImpl(rtc::FunctionView<void()> functor,
                                     const webrtc::Location& location) {
  ThreadWrapper* current_thread = ThreadWrapper::current();
  DCHECK(current_thread != nullptr) << "BlockingCall() can be called only from "
                                       "a thread that has ThreadWrapper.";

  if (current_thread == this) {
    functor();
    return;
  }

  // Send message from a thread different than |this|.

  // Allow inter-thread send only from threads that have
  // |send_allowed_| flag set.
  DCHECK(current_thread->send_allowed_)
      << "Send()'ing synchronous "
         "messages is not allowed from the current thread.";

  PendingSend pending_send(functor);
  {
    base::AutoLock auto_lock(lock_);
    pending_send_messages_.push_back(&pending_send);
  }

  // Need to signal |pending_send_event_| here in case the thread is
  // sending message to another thread.
  pending_send_event_.Signal();
  task_runner_->PostTask(
      location, base::BindOnce(&ThreadWrapper::ProcessPendingSends, weak_ptr_));

  while (!pending_send.done_event.IsSignaled()) {
    base::WaitableEvent* events[] = {&pending_send.done_event,
                                     &current_thread->pending_send_event_};
    size_t event = base::WaitableEvent::WaitMany(events, std::size(events));
    DCHECK(event == 0 || event == 1);

    if (event == 1) {
      current_thread->ProcessPendingSends();
    }
  }
}

void ThreadWrapper::ProcessPendingSends() {
  while (true) {
    PendingSend* pending_send = nullptr;
    {
      base::AutoLock auto_lock(lock_);
      if (!pending_send_messages_.empty()) {
        pending_send = pending_send_messages_.front();
        pending_send_messages_.pop_front();
      } else {
        // Reset the event while |lock_| is still locked.
        pending_send_event_.Reset();
        break;
      }
    }
    if (pending_send) {
      pending_send->functor();
      pending_send->done_event.Signal();
    }
  }
}

void ThreadWrapper::PostTaskImpl(absl::AnyInvocable<void() &&> task,
                                 const PostTaskTraits& traits,
                                 const Location& location) {
  task_runner_->PostTask(
      location, base::BindOnce(&ThreadWrapper::RunTaskQueueTask, weak_ptr_,
                               std::move(task)));
}

void ThreadWrapper::PostDelayedTaskImpl(absl::AnyInvocable<void() &&> task,
                                        TimeDelta delay,
                                        const PostDelayedTaskTraits& traits,
                                        const Location& location) {
  const base::TimeTicks target_time =
      base::TimeTicks::Now() + base::Microseconds(delay.us());
  // Coalesce low precision tasks onto the metronome.
  const base::TimeTicks snapped_target_time =
      blink::TimerBasedTickProvider::TimeSnappedToNextTick(
          target_time, blink::TimerBasedTickProvider::kDefaultPeriod);
  if (!traits.high_precision &&
      coalesced_tasks_.QueueDelayedTask(target_time, std::move(task),
                                        snapped_target_time)) {
    task_runner_->PostDelayedTaskAt(
        base::subtle::PostDelayedTaskPassKey(), location,
        base::BindOnce(&ThreadWrapper::RunCoalescedTaskQueueTasks, weak_ptr_,
                       snapped_target_time),
        snapped_target_time, base::subtle::DelayPolicy::kPrecise);
  } else if (traits.high_precision) {
    task_runner_->PostDelayedTaskAt(
        base::subtle::PostDelayedTaskPassKey(), location,
        base::BindOnce(&ThreadWrapper::RunTaskQueueTask, weak_ptr_,
                       std::move(task)),
        target_time, base::subtle::DelayPolicy::kPrecise);
  }
}

std::optional<base::TimeTicks> ThreadWrapper::PrepareRunTask() {
  if (!latency_sampler_ && task_latency_callback_) {
    latency_sampler_ = std::make_unique<PostTaskLatencySampler>(
        task_runner_, std::move(task_latency_callback_));
  }
  std::optional<base::TimeTicks> task_start_timestamp;
  if (!task_duration_callback_.is_null() && latency_sampler_ &&
      latency_sampler_->ShouldSampleNextTaskDuration()) {
    task_start_timestamp = base::TimeTicks::Now();
  }
  return task_start_timestamp;
}

void ThreadWrapper::RunTaskQueueTask(absl::AnyInvocable<void() &&> task) {
  std::optional<base::TimeTicks> task_start_timestamp = PrepareRunTask();

  std::move(task)();
  task = nullptr;

  FinalizeRunTask(std::move(task_start_timestamp));
}

void ThreadWrapper::RunCoalescedTaskQueueTasks(base::TimeTicks scheduled_time) {
  // base::Unretained(this) is safe here because these callbacks are only used
  // for the duration of the RunScheduledTasks() call.
  coalesced_tasks_.RunScheduledTasks(
      scheduled_time,
      base::BindRepeating(&ThreadWrapper::PrepareRunTask,
                          base::Unretained(this)),
      base::BindRepeating(&ThreadWrapper::FinalizeRunTask,
                          base::Unretained(this)));
}

void ThreadWrapper::FinalizeRunTask(
    std::optional<base::TimeTicks> task_start_timestamp) {
  if (task_start_timestamp.has_value())
    task_duration_callback_.Run(base::TimeTicks::Now() - *task_start_timestamp);
}

bool ThreadWrapper::IsQuitting() {
  NOTREACHED_IN_MIGRATION();
  return false;
}

// All methods below are marked as not reached. See comments in the
// header for more details.
void ThreadWrapper::Quit() {
  NOTREACHED_IN_MIGRATION();
}

void ThreadWrapper::Restart() {
  NOTREACHED_IN_MIGRATION();
}

int ThreadWrapper::GetDelay() {
  NOTREACHED_IN_MIGRATION();
  return 0;
}

void ThreadWrapper::Stop() {
  NOTREACHED_IN_MIGRATION();
}

void ThreadWrapper::Run() {
  NOTREACHED_IN_MIGRATION();
}

}  // namespace webrtc
