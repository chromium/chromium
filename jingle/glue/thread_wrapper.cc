// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/glue/thread_wrapper.h"

#include <stddef.h>
#include <stdint.h>
#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/cxx17_backports.h"
#include "base/lazy_instance.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/webrtc/rtc_base/physical_socket_server.h"

namespace jingle_glue {
namespace {
constexpr base::TimeDelta kTaskLatencySampleDuration =
    base::TimeDelta::FromSeconds(3);
}

// Class intended to conditionally live for the duration of JingleThreadWrapper
// that periodically captures task latencies (definition in docs for
// SetLatencyAndTaskDurationCallbacks).
class JingleThreadWrapper::PostTaskLatencySampler {
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

struct JingleThreadWrapper::PendingSend {
  explicit PendingSend(const rtc::Message& message_value)
      : sending_thread(JingleThreadWrapper::current()),
        message(message_value),
        done_event(base::WaitableEvent::ResetPolicy::MANUAL,
                   base::WaitableEvent::InitialState::NOT_SIGNALED) {
    DCHECK(sending_thread);
  }

  JingleThreadWrapper* sending_thread;
  rtc::Message message;
  base::WaitableEvent done_event;
};

base::LazyInstance<base::ThreadLocalPointer<JingleThreadWrapper>>::
    DestructorAtExit g_jingle_thread_wrapper = LAZY_INSTANCE_INITIALIZER;

// static
void JingleThreadWrapper::EnsureForCurrentMessageLoop() {
  if (JingleThreadWrapper::current() == nullptr) {
    std::unique_ptr<JingleThreadWrapper> wrapper =
        JingleThreadWrapper::WrapTaskRunner(
            base::ThreadTaskRunnerHandle::Get());
    base::CurrentThread::Get()->AddDestructionObserver(wrapper.release());
  }

  DCHECK_EQ(rtc::Thread::Current(), current());
}

std::unique_ptr<JingleThreadWrapper> JingleThreadWrapper::WrapTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(!JingleThreadWrapper::current());
  DCHECK(task_runner->BelongsToCurrentThread());

  std::unique_ptr<JingleThreadWrapper> result(
      new JingleThreadWrapper(task_runner));
  g_jingle_thread_wrapper.Get().Set(result.get());
  return result;
}

// static
JingleThreadWrapper* JingleThreadWrapper::current() {
  return g_jingle_thread_wrapper.Get().Get();
}

void JingleThreadWrapper::SetLatencyAndTaskDurationCallbacks(
    SampledDurationCallback task_latency_callback,
    SampledDurationCallback task_duration_callback) {
  task_latency_callback_ = std::move(task_latency_callback);
  task_duration_callback_ = std::move(task_duration_callback);
}

JingleThreadWrapper::JingleThreadWrapper(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : Thread(std::make_unique<rtc::PhysicalSocketServer>()),
      task_runner_(task_runner),
      send_allowed_(false),
      last_task_id_(0),
      pending_send_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                          base::WaitableEvent::InitialState::NOT_SIGNALED) {
  DCHECK(task_runner->BelongsToCurrentThread());
  DCHECK(!rtc::Thread::Current());
  weak_ptr_ = weak_ptr_factory_.GetWeakPtr();
  rtc::ThreadManager::Add(this);
  SafeWrapCurrent();
}

JingleThreadWrapper::~JingleThreadWrapper() {
  DCHECK_EQ(this, JingleThreadWrapper::current());
  DCHECK_EQ(this, rtc::Thread::Current());

  UnwrapCurrent();
  rtc::ThreadManager::Instance()->SetCurrentThread(nullptr);
  rtc::ThreadManager::Remove(this);
  g_jingle_thread_wrapper.Get().Set(nullptr);

  Clear(nullptr, rtc::MQID_ANY, nullptr);
}

void JingleThreadWrapper::WillDestroyCurrentMessageLoop() {
  delete this;
}

void JingleThreadWrapper::Post(const rtc::Location& posted_from,
                               rtc::MessageHandler* handler,
                               uint32_t message_id,
                               rtc::MessageData* data,
                               bool time_sensitive) {
  PostTaskInternal(posted_from, 0, handler, message_id, data);
}

void JingleThreadWrapper::PostDelayed(const rtc::Location& posted_from,
                                      int delay_ms,
                                      rtc::MessageHandler* handler,
                                      uint32_t message_id,
                                      rtc::MessageData* data) {
  PostTaskInternal(posted_from, delay_ms, handler, message_id, data);
}

void JingleThreadWrapper::Clear(rtc::MessageHandler* handler,
                                uint32_t id,
                                rtc::MessageList* removed) {
  base::AutoLock auto_lock(lock_);

  for (MessagesQueue::iterator it = messages_.begin();
       it != messages_.end();) {
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
        removed ->push_back((*it)->message);
      } else {
        delete (*it)->message.pdata;
      }
      (*it)->done_event.Signal();
      pending_send_messages_.erase(it);
    }

    it = next;
  }
}

void JingleThreadWrapper::Dispatch(rtc::Message* message) {
  TRACE_EVENT2("webrtc", "JingleThreadWrapper::Dispatch", "src_file_and_line",
               message->posted_from.file_and_line(), "src_func",
               message->posted_from.function_name());
  message->phandler->OnMessage(message);
}

void JingleThreadWrapper::Send(const rtc::Location& posted_from,
                               rtc::MessageHandler* handler,
                               uint32_t id,
                               rtc::MessageData* data) {
  JingleThreadWrapper* current_thread = JingleThreadWrapper::current();
  DCHECK(current_thread != nullptr) << "Send() can be called only from a "
      "thread that has JingleThreadWrapper.";

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
  DCHECK(current_thread->send_allowed_) << "Send()'ing synchronous "
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
      base::BindOnce(&JingleThreadWrapper::ProcessPendingSends, weak_ptr_));

  while (!pending_send.done_event.IsSignaled()) {
    base::WaitableEvent* events[] = {&pending_send.done_event,
                                     &current_thread->pending_send_event_};
    size_t event = base::WaitableEvent::WaitMany(events, base::size(events));
    DCHECK(event == 0 || event == 1);

    if (event == 1)
      current_thread->ProcessPendingSends();
  }
}

void JingleThreadWrapper::ProcessPendingSends() {
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

void JingleThreadWrapper::PostTaskInternal(const rtc::Location& posted_from,
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
        FROM_HERE,
        base::BindOnce(&JingleThreadWrapper::RunTask, weak_ptr_, task_id));
  } else {
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&JingleThreadWrapper::RunTask, weak_ptr_, task_id),
        base::TimeDelta::FromMilliseconds(delay_ms));
  }
}

void JingleThreadWrapper::PostTask(std::unique_ptr<webrtc::QueuedTask> task) {
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&JingleThreadWrapper::RunTaskQueueTask,
                                        weak_ptr_, std::move(task)));
}

void JingleThreadWrapper::PostDelayedTask(
    std::unique_ptr<webrtc::QueuedTask> task,
    uint32_t milliseconds) {
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&JingleThreadWrapper::RunTaskQueueTask, weak_ptr_,
                     std::move(task)),
      base::TimeDelta::FromMilliseconds(milliseconds));
}

absl::optional<base::TimeTicks> JingleThreadWrapper::PrepareRunTask() {
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

void JingleThreadWrapper::RunTaskQueueTask(
    std::unique_ptr<webrtc::QueuedTask> task) {
  absl::optional<base::TimeTicks> task_start_timestamp = PrepareRunTask();

  // Follow QueuedTask::Run() semantics: delete if it returns true, release
  // otherwise.
  if (task->Run())
    task.reset();
  else
    task.release();

  FinalizeRunTask(std::move(task_start_timestamp));
}

void JingleThreadWrapper::RunTask(int task_id) {
  absl::optional<base::TimeTicks> task_start_timestamp = PrepareRunTask();

  RunTaskInternal(task_id);

  FinalizeRunTask(std::move(task_start_timestamp));
}

void JingleThreadWrapper::FinalizeRunTask(
    absl::optional<base::TimeTicks> task_start_timestamp) {
  if (task_start_timestamp.has_value())
    task_duration_callback_.Run(base::TimeTicks::Now() - *task_start_timestamp);
}

void JingleThreadWrapper::RunTaskInternal(int task_id) {
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

bool JingleThreadWrapper::IsQuitting() {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

// All methods below are marked as not reached. See comments in the
// header for more details.
void JingleThreadWrapper::Quit() {
  NOTREACHED();
}

void JingleThreadWrapper::Restart() {
  NOTREACHED();
}

bool JingleThreadWrapper::Get(rtc::Message*, int, bool) {
  NOTREACHED();
  return false;
}

bool JingleThreadWrapper::Peek(rtc::Message*, int) {
  NOTREACHED();
  return false;
}

int JingleThreadWrapper::GetDelay() {
  NOTREACHED();
  return 0;
}

void JingleThreadWrapper::Stop() {
  NOTREACHED();
}

void JingleThreadWrapper::Run() {
  NOTREACHED();
}

}  // namespace jingle_glue
