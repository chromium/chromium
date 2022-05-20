// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webrtc/thread_wrapper.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/waitable_event.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/webrtc/rtc_base/physical_socket_server.h"
#include "third_party/webrtc_overrides/metronome_source.h"

namespace webrtc {
namespace {
constexpr base::TimeDelta kTaskLatencySampleDuration = base::Seconds(3);
}

const base::Feature kThreadWrapperUsesMetronome{
    "ThreadWrapperUsesMetronome", base::FEATURE_ENABLED_BY_DEFAULT};

// Class intended to conditionally live for the duration of ThreadWrapper
// that periodically captures task latencies (definition in docs for
// SetLatencyAndTaskDurationCallbacks).
class ThreadWrapper::PostTaskLatencySampler {
 public:
  PostTaskLatencySampler(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
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
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::RepeatingCallback<void(base::TimeDelta)> task_latency_callback_
      GUARDED_BY_CONTEXT(current_);
  bool should_sample_next_task_duration_ GUARDED_BY_CONTEXT(current_) = false;
};

struct ThreadWrapper::PendingSend {
  explicit PendingSend(const rtc::Message& message_value)
      : sending_thread(ThreadWrapper::current()),
        message(message_value),
        done_event(base::WaitableEvent::ResetPolicy::MANUAL,
                   base::WaitableEvent::InitialState::NOT_SIGNALED) {
    DCHECK(sending_thread);
  }

  raw_ptr<ThreadWrapper> sending_thread;
  rtc::Message message;
  base::WaitableEvent done_event;
};

base::LazyInstance<base::ThreadLocalPointer<ThreadWrapper>>::DestructorAtExit
    g_jingle_thread_wrapper = LAZY_INSTANCE_INITIALIZER;

// static
void ThreadWrapper::EnsureForCurrentMessageLoop() {
  if (ThreadWrapper::current() == nullptr) {
    std::unique_ptr<ThreadWrapper> wrapper =
        ThreadWrapper::WrapTaskRunner(base::ThreadTaskRunnerHandle::Get());
    base::CurrentThread::Get()->AddDestructionObserver(wrapper.release());
  }

  DCHECK_EQ(rtc::Thread::Current(), current());
}

std::unique_ptr<ThreadWrapper> ThreadWrapper::WrapTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(!ThreadWrapper::current());
  DCHECK(task_runner->BelongsToCurrentThread());

  std::unique_ptr<ThreadWrapper> result(new ThreadWrapper(task_runner));
  g_jingle_thread_wrapper.Get().Set(result.get());
  return result;
}

// static
ThreadWrapper* ThreadWrapper::current() {
  return g_jingle_thread_wrapper.Get().Get();
}

void ThreadWrapper::SetLatencyAndTaskDurationCallbacks(
    SampledDurationCallback task_latency_callback,
    SampledDurationCallback task_duration_callback) {
  task_latency_callback_ = std::move(task_latency_callback);
  task_duration_callback_ = std::move(task_duration_callback);
}

ThreadWrapper::ThreadWrapper(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : Thread(std::make_unique<rtc::PhysicalSocketServer>()),
      task_runner_(task_runner),
      send_allowed_(false),
      use_metronome_(base::FeatureList::IsEnabled(kThreadWrapperUsesMetronome)),
      last_task_id_(0),
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
  g_jingle_thread_wrapper.Get().Set(nullptr);

  Clear(nullptr, rtc::MQID_ANY, nullptr);
  coalesced_tasks_.Clear();
}

void ThreadWrapper::WillDestroyCurrentMessageLoop() {
  delete this;
}

void ThreadWrapper::Post(const rtc::Location& posted_from,
                         rtc::MessageHandler* handler,
                         uint32_t message_id,
                         rtc::MessageData* data,
                         bool time_sensitive) {
  PostTaskInternal(posted_from, 0, handler, message_id, data);
}

void ThreadWrapper::PostDelayed(const rtc::Location& posted_from,
                                int delay_ms,
                                rtc::MessageHandler* handler,
                                uint32_t message_id,
                                rtc::MessageData* data) {
  PostTaskInternal(posted_from, delay_ms, handler, message_id, data);
}

void ThreadWrapper::Clear(rtc::MessageHandler* handler,
                          uint32_t id,
                          rtc::MessageList* removed) {
  base::AutoLock auto_lock(lock_);

  for (MessagesQueue::iterator it = messages_.begin(); it != messages_.end();) {
    MessagesQueue::iterator next = it;
    ++next;

    if (it->second.Match(handler, id)) {
      if (removed) {
        removed->push_back(it->second);
      } else {
        delete it->second.pdata;
      }
      messages_.erase(it);
    }

    it = next;
  }

  for (std::list<PendingSend*>::iterator it = pending_send_messages_.begin();
       it != pending_send_messages_.end();) {
    std::list<PendingSend*>::iterator next = it;
    ++next;

    if ((*it)->message.Match(handler, id)) {
      if (removed) {
        removed->push_back((*it)->message);
      } else {
        delete (*it)->message.pdata;
      }
      (*it)->done_event.Signal();
      pending_send_messages_.erase(it);
    }

    it = next;
  }
}

void ThreadWrapper::Dispatch(rtc::Message* message) {
  TRACE_EVENT2("webrtc", "ThreadWrapper::Dispatch", "src_file_and_line",
               message->posted_from.file_and_line(), "src_func",
               message->posted_from.function_name());
  message->phandler->OnMessage(message);
}

void ThreadWrapper::Send(const rtc::Location& posted_from,
                         rtc::MessageHandler* handler,
                         uint32_t id,
                         rtc::MessageData* data) {
  ThreadWrapper* current_thread = ThreadWrapper::current();
  DCHECK(current_thread != nullptr) << "Send() can be called only from a "
                                       "thread that has ThreadWrapper.";

  rtc::Message message;
  message.posted_from = posted_from;
  message.phandler = handler;
  message.message_id = id;
  message.pdata = data;

  if (current_thread == this) {
    Dispatch(&message);
    return;
  }

  // Send message from a thread different than |this|.

  // Allow inter-thread send only from threads that have
  // |send_allowed_| flag set.
  DCHECK(current_thread->send_allowed_)
      << "Send()'ing synchronous "
         "messages is not allowed from the current thread.";

  PendingSend pending_send(message);
  {
    base::AutoLock auto_lock(lock_);
    pending_send_messages_.push_back(&pending_send);
  }

  // Need to signal |pending_send_event_| here in case the thread is
  // sending message to another thread.
  pending_send_event_.Signal();
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ThreadWrapper::ProcessPendingSends, weak_ptr_));

  while (!pending_send.done_event.IsSignaled()) {
    base::WaitableEvent* events[] = {&pending_send.done_event,
                                     &current_thread->pending_send_event_};
    size_t event = base::WaitableEvent::WaitMany(events, std::size(events));
    DCHECK(event == 0 || event == 1);

    if (event == 1)
      current_thread->ProcessPendingSends();
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
      Dispatch(&pending_send->message);
      pending_send->done_event.Signal();
    }
  }
}

void ThreadWrapper::PostTaskInternal(const rtc::Location& posted_from,
                                     int delay_ms,
                                     rtc::MessageHandler* handler,
                                     uint32_t message_id,
                                     rtc::MessageData* data) {
  int task_id;
  rtc::Message message;
  message.posted_from = posted_from;
  message.phandler = handler;
  message.message_id = message_id;
  message.pdata = data;
  {
    base::AutoLock auto_lock(lock_);
    task_id = ++last_task_id_;
    messages_.insert(std::pair<int, rtc::Message>(task_id, message));
  }

  if (delay_ms <= 0) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ThreadWrapper::RunTask, weak_ptr_, task_id));
  } else {
    task_runner_->PostDelayedTaskAt(
        base::subtle::PostDelayedTaskPassKey(), FROM_HERE,
        base::BindOnce(&ThreadWrapper::RunTask, weak_ptr_, task_id),
        base::TimeTicks::Now() + base::Milliseconds(delay_ms),
        base::subtle::DelayPolicy::kPrecise);
  }
}

void ThreadWrapper::PostTask(std::unique_ptr<webrtc::QueuedTask> task) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ThreadWrapper::RunTaskQueueTask, weak_ptr_,
                                std::move(task)));
}

void ThreadWrapper::PostDelayedTask(std::unique_ptr<webrtc::QueuedTask> task,
                                    uint32_t milliseconds) {
  base::TimeTicks target_time =
      base::TimeTicks::Now() + base::Milliseconds(milliseconds);
  if (use_metronome_) {
    // Coalesce tasks onto the metronome.
    base::TimeTicks snapped_target_time =
        blink::MetronomeSource::TimeSnappedToNextTick(target_time);
    if (coalesced_tasks_.QueueDelayedTask(target_time, std::move(task),
                                          snapped_target_time)) {
      task_runner_->PostDelayedTaskAt(
          base::subtle::PostDelayedTaskPassKey(), FROM_HERE,
          base::BindOnce(&ThreadWrapper::RunCoalescedTaskQueueTasks, weak_ptr_,
                         snapped_target_time),
          snapped_target_time, base::subtle::DelayPolicy::kPrecise);
    }
    return;
  }
  // Do not coalesce tasks onto the metronome.
  task_runner_->PostDelayedTaskAt(
      base::subtle::PostDelayedTaskPassKey(), FROM_HERE,
      base::BindOnce(&ThreadWrapper::RunTaskQueueTask, weak_ptr_,
                     std::move(task)),
      target_time, base::subtle::DelayPolicy::kPrecise);
}

void ThreadWrapper::PostDelayedHighPrecisionTask(
    std::unique_ptr<webrtc::QueuedTask> task,
    uint32_t milliseconds) {
  base::TimeTicks target_time =
      base::TimeTicks::Now() + base::Milliseconds(milliseconds);
  task_runner_->PostDelayedTaskAt(
      base::subtle::PostDelayedTaskPassKey(), FROM_HERE,
      base::BindOnce(&ThreadWrapper::RunTaskQueueTask, weak_ptr_,
                     std::move(task)),
      target_time, base::subtle::DelayPolicy::kPrecise);
}

absl::optional<base::TimeTicks> ThreadWrapper::PrepareRunTask() {
  if (!latency_sampler_ && task_latency_callback_) {
    latency_sampler_ = std::make_unique<PostTaskLatencySampler>(
        task_runner_, std::move(task_latency_callback_));
  }
  absl::optional<base::TimeTicks> task_start_timestamp;
  if (!task_duration_callback_.is_null() && latency_sampler_ &&
      latency_sampler_->ShouldSampleNextTaskDuration()) {
    task_start_timestamp = base::TimeTicks::Now();
  }
  return task_start_timestamp;
}

void ThreadWrapper::RunTaskQueueTask(std::unique_ptr<webrtc::QueuedTask> task) {
  absl::optional<base::TimeTicks> task_start_timestamp = PrepareRunTask();

  // Follow QueuedTask::Run() semantics: delete if it returns true, release
  // otherwise.
  if (task->Run())
    task.reset();
  else
    task.release();

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

void ThreadWrapper::RunTask(int task_id) {
  absl::optional<base::TimeTicks> task_start_timestamp = PrepareRunTask();

  RunTaskInternal(task_id);

  FinalizeRunTask(std::move(task_start_timestamp));
}

void ThreadWrapper::FinalizeRunTask(
    absl::optional<base::TimeTicks> task_start_timestamp) {
  if (task_start_timestamp.has_value())
    task_duration_callback_.Run(base::TimeTicks::Now() - *task_start_timestamp);
}

void ThreadWrapper::RunTaskInternal(int task_id) {
  bool have_message = false;
  rtc::Message message;
  {
    base::AutoLock auto_lock(lock_);
    MessagesQueue::iterator it = messages_.find(task_id);
    if (it != messages_.end()) {
      have_message = true;
      message = it->second;
      messages_.erase(it);
    }
  }

  if (have_message) {
    if (message.message_id == rtc::MQID_DISPOSE) {
      DCHECK(message.phandler == nullptr);
      delete message.pdata;
    } else {
      Dispatch(&message);
    }
  }
}

bool ThreadWrapper::IsQuitting() {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

// All methods below are marked as not reached. See comments in the
// header for more details.
void ThreadWrapper::Quit() {
  NOTREACHED();
}

void ThreadWrapper::Restart() {
  NOTREACHED();
}

bool ThreadWrapper::Get(rtc::Message*, int, bool) {
  NOTREACHED();
  return false;
}

bool ThreadWrapper::Peek(rtc::Message*, int) {
  NOTREACHED();
  return false;
}

int ThreadWrapper::GetDelay() {
  NOTREACHED();
  return 0;
}

void ThreadWrapper::Stop() {
  NOTREACHED();
}

void ThreadWrapper::Run() {
  NOTREACHED();
}

}  // namespace webrtc
