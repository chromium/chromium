// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/system/simple_watcher.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/heap_profiler.h"
#include "base/trace_event/trace_event.h"
#include "mojo/public/c/system/trap.h"

namespace mojo {

// Thread-safe Context object used to schedule trap events from arbitrary
// threads.
class SimpleWatcher::Context : public base::RefCountedThreadSafe<Context> {
 public:
  // Creates a |Context| instance for a new watch on |watcher|, to observe
  // |signals| on |handle|.
  static scoped_refptr<Context> Create(
      base::WeakPtr<SimpleWatcher> watcher,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      TrapHandle trap_handle,
      Handle handle,
      MojoHandleSignals signals,
      MojoTriggerCondition condition,
      int watch_id,
      MojoResult* result) {
    scoped_refptr<Context> context =
        new Context(watcher, task_runner, watch_id);

    // If MojoAddTrigger succeeds, it effectively assumes ownership of a
    // reference to |context|. In that case, this reference is balanced in
    // CallNotify() when |result| is |MOJO_RESULT_CANCELLED|.
    context->AddRef();

    *result = MojoAddTrigger(trap_handle.value(), handle.value(), signals,
                             condition, context->value(), nullptr);
    if (*result != MOJO_RESULT_OK) {
      context->cancelled_ = true;

      // Balanced by the AddRef() above since MojoAddTrigger failed.
      context->Release();
      return nullptr;
    }

    return context;
  }

  static void CallNotify(const MojoTrapEvent* event) {
    auto* context = reinterpret_cast<Context*>(event->trigger_context);
    context->Notify(event->result, event->signals_state, event->flags);

    // The trigger was removed. We can release the ref it owned, which in turn
    // may delete the Context.
    if (event->result == MOJO_RESULT_CANCELLED)
      context->Release();
  }

  uintptr_t value() const { return reinterpret_cast<uintptr_t>(this); }

  void DisableCancellationNotifications() {
    base::AutoLock lock(lock_);
    enable_cancellation_notifications_ = false;
  }

 private:
  friend class base::RefCountedThreadSafe<Context>;

  Context(base::WeakPtr<SimpleWatcher> weak_watcher,
          scoped_refptr<base::SequencedTaskRunner> task_runner,
          int watch_id)
      : weak_watcher_(weak_watcher),
        task_runner_(task_runner),
        watch_id_(watch_id) {}

  ~Context() {
    // TODO(https://crbug.com/896419): Remove this once it's been live for a
    // while. This is intended to catch possible double-frees of SimpleWatchers,
    // due to, e.g., invalid cross-thread usage of bindings endpoints. If this
    // CHECK fails, then the Context is being destroyed before a cancellation
    // notification fired. In that case we know a Context ref has been
    // double-released and we can catch its stack.
    base::AutoLock lock(lock_);
    CHECK(cancelled_);
  }

  void Notify(MojoResult result,
              MojoHandleSignalsState signals_state,
              MojoTrapEventFlags flags) {
    if (result == MOJO_RESULT_CANCELLED) {
      // The SimpleWatcher may have explicitly removed this trigger, so we don't
      // bother dispatching the notification - it would be ignored anyway.
      //
      // TODO(rockot): This shouldn't really be necessary, but there are already
      // instances today where bindings object may be bound and subsequently
      // closed due to pipe error, all before the thread's TaskRunner has been
      // properly initialized.
      base::AutoLock lock(lock_);
      cancelled_ = true;
      if (!enable_cancellation_notifications_)
        return;
    }

    HandleSignalsState state(signals_state.satisfied_signals,
                             signals_state.satisfiable_signals);
    if (!(flags & MOJO_TRAP_EVENT_FLAG_WITHIN_API_CALL) &&
        task_runner_->RunsTasksInCurrentSequence() && weak_watcher_ &&
        weak_watcher_->is_default_task_runner_) {
      // System notifications will trigger from the task runner passed to
      // mojo::core::ScopedIPCSupport. In Chrome this happens to always be
      // the default task runner for the IO thread.
      weak_watcher_->OnHandleReady(watch_id_, result, state);
    } else {
      task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&SimpleWatcher::OnHandleReady,
                                    weak_watcher_, watch_id_, result, state));
    }
  }

  const base::WeakPtr<SimpleWatcher> weak_watcher_;
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  const int watch_id_;

  base::Lock lock_;
  bool cancelled_ = false;
  bool enable_cancellation_notifications_ = true;

  DISALLOW_COPY_AND_ASSIGN(Context);
};

SimpleWatcher::SimpleWatcher(const base::Location& from_here,
                             ArmingPolicy arming_policy,
                             scoped_refptr<base::SequencedTaskRunner> runner)
    : arming_policy_(arming_policy),
      task_runner_(std::move(runner)),
      is_default_task_runner_(base::ThreadTaskRunnerHandle::IsSet() &&
                              task_runner_ ==
                                  base::ThreadTaskRunnerHandle::Get()),
      heap_profiler_tag_(from_here.file_name()) {
  MojoResult rv = CreateTrap(&Context::CallNotify, &trap_handle_);
  DCHECK_EQ(MOJO_RESULT_OK, rv);
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
}

SimpleWatcher::~SimpleWatcher() {
  if (IsWatching())
    Cancel();
}

bool SimpleWatcher::IsWatching() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return context_ != nullptr;
}

MojoResult SimpleWatcher::Watch(Handle handle,
                                MojoHandleSignals signals,
                                MojoTriggerCondition condition,
                                ReadyCallbackWithState callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!IsWatching());
  DCHECK(!callback.is_null());

  callback_ = std::move(callback);
  handle_ = handle;
  watch_id_ += 1;

  MojoResult result = MOJO_RESULT_UNKNOWN;
  context_ = Context::Create(weak_factory_.GetWeakPtr(), task_runner_,
                             trap_handle_.get(), handle_, signals, condition,
                             watch_id_, &result);
  if (!context_) {
    handle_.set_value(kInvalidHandleValue);
    callback_.Reset();
    DCHECK_EQ(MOJO_RESULT_INVALID_ARGUMENT, result);
    return result;
  }

  if (arming_policy_ == ArmingPolicy::AUTOMATIC)
    ArmOrNotify();

  return MOJO_RESULT_OK;
}

void SimpleWatcher::Cancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The watcher may have already been cancelled if the handle was closed.
  if (!context_)
    return;

  // Prevent the cancellation notification from being dispatched to
  // OnHandleReady() when cancellation is explicit. See the note in the
  // implementation of DisableCancellationNotifications() above.
  context_->DisableCancellationNotifications();

  handle_.set_value(kInvalidHandleValue);
  callback_.Reset();

  // Ensure |context_| is unset by the time we call MojoRemoveTrigger, as it may
  // re-enter the notification callback and we want to ensure |context_| is
  // unset by then.
  scoped_refptr<Context> context;
  std::swap(context, context_);
  MojoResult rv =
      MojoRemoveTrigger(trap_handle_.get().value(), context->value(), nullptr);

  // It's possible this cancellation could race with a handle closure
  // notification, in which case the watch may have already been implicitly
  // cancelled.
  DCHECK(rv == MOJO_RESULT_OK || rv == MOJO_RESULT_NOT_FOUND);

  weak_factory_.InvalidateWeakPtrs();
}

MojoResult SimpleWatcher::Arm(MojoResult* ready_result,
                              HandleSignalsState* ready_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  uint32_t num_blocking_events = 1;
  MojoTrapEvent blocking_event = {sizeof(blocking_event)};
  MojoResult rv = MojoArmTrap(trap_handle_.get().value(), nullptr,
                              &num_blocking_events, &blocking_event);
  if (rv == MOJO_RESULT_FAILED_PRECONDITION) {
    DCHECK(context_);
    DCHECK_EQ(1u, num_blocking_events);
    DCHECK_EQ(context_->value(), blocking_event.trigger_context);
    if (ready_result)
      *ready_result = blocking_event.result;
    if (ready_state) {
      *ready_state =
          HandleSignalsState(blocking_event.signals_state.satisfied_signals,
                             blocking_event.signals_state.satisfiable_signals);
    }
  }

  return rv;
}

void SimpleWatcher::ArmOrNotify() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Already cancelled, nothing to do.
  if (!IsWatching())
    return;

  MojoResult ready_result;
  HandleSignalsState ready_state;
  MojoResult rv = Arm(&ready_result, &ready_state);
  if (rv == MOJO_RESULT_OK)
    return;

  DCHECK_EQ(MOJO_RESULT_FAILED_PRECONDITION, rv);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SimpleWatcher::OnHandleReady, weak_factory_.GetWeakPtr(),
                     watch_id_, ready_result, ready_state));
}

void SimpleWatcher::OnHandleReady(int watch_id,
                                  MojoResult result,
                                  const HandleSignalsState& state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This notification may be for a previously watched context, in which case
  // we just ignore it.
  if (watch_id != watch_id_)
    return;

  ReadyCallbackWithState callback = callback_;
  if (result == MOJO_RESULT_CANCELLED) {
    // Implicit cancellation due to someone closing the watched handle. We clear
    // the SimppleWatcher's state before dispatching this.
    context_ = nullptr;
    handle_.set_value(kInvalidHandleValue);
    callback_.Reset();
  }

  // NOTE: It's legal for |callback| to delete |this|.
  if (!callback.is_null()) {
    TRACE_HEAP_PROFILER_API_SCOPED_TASK_EXECUTION event(heap_profiler_tag_);
    // Lot of janks caused are grouped to OnHandleReady tasks. This trace event helps identify the
    // cause of janks. It is ok to pass |heap_profiler_tag_| here since it is a string literal.
    // TODO(927206): Consider renaming |heap_profiler_tag_|.
    TRACE_EVENT0("toplevel", heap_profiler_tag_);

    base::WeakPtr<SimpleWatcher> weak_self = weak_factory_.GetWeakPtr();
    callback.Run(result, state);
    if (!weak_self)
      return;

    // Prevent |MOJO_RESULT_FAILED_PRECONDITION| task spam by only notifying
    // at most once in AUTOMATIC arming mode.
    if (result == MOJO_RESULT_FAILED_PRECONDITION)
      return;

    if (arming_policy_ == ArmingPolicy::AUTOMATIC && IsWatching())
      ArmOrNotify();
  }
}

}  // namespace mojo
