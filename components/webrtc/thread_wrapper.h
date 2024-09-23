// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBRTC_THREAD_WRAPPER_H_
#define COMPONENTS_WEBRTC_THREAD_WRAPPER_H_

#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <optional>

#include "base/auto_reset.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "third_party/webrtc/rtc_base/thread.h"
#include "third_party/webrtc_overrides/api/location.h"
#include "third_party/webrtc_overrides/coalesced_tasks.h"

namespace webrtc {

// ThreadWrapper implements rtc::Thread interface on top of
// Chromium's SingleThreadTaskRunner interface. Currently only the bare minimum
// that is used by P2P part of libjingle is implemented. There are two ways to
// create this object:
//
// - Call EnsureForCurrentMessageLoop(). This approach works only on threads
//   that have MessageLoop In this case ThreadWrapper deletes itself
//   automatically when MessageLoop is destroyed.
// - Using ThreadWrapper() constructor. In this case the creating code
//   must pass a valid task runner for the current thread and also delete the
//   wrapper later.
class ThreadWrapper : public base::CurrentThread::DestructionObserver,
                      public rtc::Thread {
 public:
  // A repeating callback whose TimeDelta argument indicates a duration sample.
  // What the duration represents is contextual.
  using SampledDurationCallback =
      base::RepeatingCallback<void(base::TimeDelta)>;

  // Create ThreadWrapper for the current thread if it hasn't been created
  // yet. The thread wrapper is destroyed automatically when the current
  // MessageLoop is destroyed.
  static void EnsureForCurrentMessageLoop();

  // Creates ThreadWrapper for |task_runner| that runs tasks on the
  // current thread.
  static std::unique_ptr<ThreadWrapper> WrapTaskRunner(
      ::scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // Returns thread wrapper for the current thread or nullptr if it doesn't
  // exist.
  static ThreadWrapper* current();

  // Sets task latency & duration sample callbacks intended to gather UMA
  // statistics. Samples are acquired periodically every several seconds by
  // ThreadWrapper. In this context,
  // * task latency is defined as the duration between the moment a task is
  //   scheduled from ThreadWrapper's task runner, and the moment
  //   it begins running.
  // * task duration is defined as the duration between the moment the
  //   ThreadWrapper begins running a task and the moment it ends
  //   executing it. It only measures durations of tasks posted to rtc::Thread.
  // The passed callbacks are called in the ThreadWrapper's task runner
  // context.
  void SetLatencyAndTaskDurationCallbacks(
      SampledDurationCallback task_latency_callback,
      SampledDurationCallback task_duration_callback);

  ThreadWrapper(const ThreadWrapper&) = delete;
  ThreadWrapper& operator=(const ThreadWrapper&) = delete;

  ~ThreadWrapper() override;

  // Sets whether the thread can be used to send messages
  // synchronously to another thread using BlockingCall() method. Set to false
  // by default to avoid potential jankiness when BlockingCall() used on
  // renderer thread. It should be set explicitly for threads that
  // need to call BlockingCall() for other threads.
  void set_send_allowed(bool allowed) { send_allowed_ = allowed; }

  rtc::SocketServer* SocketServer();

  // CurrentThread::DestructionObserver implementation.
  void WillDestroyCurrentMessageLoop() override;

  // Following methods are not supported. They are overriden just to
  // ensure that they are not called (each of them contain NOTREACHED
  // in the body). Some of this methods can be implemented if it
  // becomes necessary to use webrtc code that calls them.
  bool IsQuitting() override;
  void Quit() override;
  void Restart() override;
  int GetDelay() override;

  // rtc::Thread overrides.
  void Stop() override;
  void Run() override;

 private:
  struct PendingSend;
  class PostTaskLatencySampler;

  explicit ThreadWrapper(
      ::scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // rtc::Thread overrides.
  void BlockingCallImpl(rtc::FunctionView<void()> functor,
                        const webrtc::Location& location) override;
  // TaskQueueBase overrides.
  void PostTaskImpl(absl::AnyInvocable<void() &&> task,
                    const PostTaskTraits& traits,
                    const Location& location) override;
  void PostDelayedTaskImpl(absl::AnyInvocable<void() &&> task,
                           TimeDelta delay,
                           const PostDelayedTaskTraits& traits,
                           const Location& location) override;

  void ProcessPendingSends();

  // Executes WebRTC queued tasks from TaskQueueBase overrides on
  // |task_runner_|.
  void RunTaskQueueTask(absl::AnyInvocable<void() &&> task);
  void RunCoalescedTaskQueueTasks(base::TimeTicks scheduled_time);

  // Called before a task runs, returns an opaque optional timestamp which
  // should be passed into FinalizeRunTask.
  std::optional<base::TimeTicks> PrepareRunTask();
  // Called after a task has run. Move the return value of PrepareRunTask as
  // |task_start_timestamp|.
  void FinalizeRunTask(std::optional<base::TimeTicks> task_start_timestamp);

  const base::AutoReset<ThreadWrapper*> resetter_;

  // Task runner used to execute messages posted on this thread.
  ::scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  bool send_allowed_;

  // |lock_| must be locked when accessing |pending_send_messages_|.
  base::Lock lock_;
  std::list<raw_ptr<PendingSend, CtnExperimental>> pending_send_messages_;
  base::WaitableEvent pending_send_event_;
  std::unique_ptr<PostTaskLatencySampler> latency_sampler_;
  SampledDurationCallback task_latency_callback_;
  SampledDurationCallback task_duration_callback_;
  // Low precision tasks are coalesced onto metronome ticks and stored in
  // `coalesced_tasks_` until they are ready to run.
  blink::CoalescedTasks coalesced_tasks_;

  base::WeakPtr<ThreadWrapper> weak_ptr_;
  base::WeakPtrFactory<ThreadWrapper> weak_ptr_factory_{this};
};

}  // namespace webrtc

#endif  // COMPONENTS_WEBRTC_THREAD_WRAPPER_H_
