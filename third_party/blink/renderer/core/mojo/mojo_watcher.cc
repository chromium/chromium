// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/mojo/mojo_watcher.h"

#include "base/single_thread_task_runner.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mojo_watch_callback.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/mojo/mojo_handle_signals.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

// static
MojoWatcher* MojoWatcher::Create(mojo::Handle handle,
                                 const MojoHandleSignals* signals_dict,
                                 V8MojoWatchCallback* callback,
                                 ExecutionContext* context) {
  MojoWatcher* watcher = MakeGarbageCollected<MojoWatcher>(context, callback);
  MojoResult result = watcher->Watch(handle, signals_dict);
  // TODO(alokp): Consider raising an exception.
  // Current clients expect to recieve the initial error returned by MojoWatch
  // via watch callback.
  //
  // Note that the usage of WrapPersistent is intentional so that the initial
  // error is guaranteed to be reported to the client in case where the given
  // handle is invalid and garbage collection happens before the callback
  // is scheduled.
  if (result != MOJO_RESULT_OK) {
    watcher->task_runner_->PostTask(
        FROM_HERE,
        WTF::Bind(&V8MojoWatchCallback::InvokeAndReportException,
                  WrapPersistent(callback), WrapPersistent(watcher), result));
  }
  return watcher;
}

MojoWatcher::~MojoWatcher() = default;

MojoResult MojoWatcher::cancel() {
  if (!trap_handle_.is_valid())
    return MOJO_RESULT_INVALID_ARGUMENT;

  trap_handle_.reset();
  return MOJO_RESULT_OK;
}

void MojoWatcher::Trace(blink::Visitor* visitor) {
  visitor->Trace(callback_);
  ScriptWrappable::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

bool MojoWatcher::HasPendingActivity() const {
  return handle_.is_valid();
}

void MojoWatcher::ContextDestroyed(ExecutionContext*) {
  cancel();
}

MojoWatcher::MojoWatcher(ExecutionContext* context,
                         V8MojoWatchCallback* callback)
    : ContextLifecycleObserver(context),
      task_runner_(context->GetTaskRunner(TaskType::kInternalDefault)),
      callback_(callback) {}

MojoResult MojoWatcher::Watch(mojo::Handle handle,
                              const MojoHandleSignals* signals_dict) {
  ::MojoHandleSignals signals = MOJO_HANDLE_SIGNAL_NONE;
  if (signals_dict->readable())
    signals |= MOJO_HANDLE_SIGNAL_READABLE;
  if (signals_dict->writable())
    signals |= MOJO_HANDLE_SIGNAL_WRITABLE;
  if (signals_dict->peerClosed())
    signals |= MOJO_HANDLE_SIGNAL_PEER_CLOSED;

  MojoResult result =
      mojo::CreateTrap(&MojoWatcher::OnHandleReady, &trap_handle_);
  DCHECK_EQ(MOJO_RESULT_OK, result);

  result = MojoAddTrigger(trap_handle_.get().value(), handle.value(), signals,
                          MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                          reinterpret_cast<uintptr_t>(this), nullptr);
  if (result != MOJO_RESULT_OK)
    return result;

  handle_ = handle;

  MojoResult ready_result;
  result = Arm(&ready_result);
  if (result == MOJO_RESULT_OK)
    return result;

  if (result == MOJO_RESULT_FAILED_PRECONDITION) {
    // We couldn't arm the watcher because the handle is already ready to
    // trigger a success notification. Post a notification manually.
    task_runner_->PostTask(FROM_HERE,
                           WTF::Bind(&MojoWatcher::RunReadyCallback,
                                     WrapPersistent(this), ready_result));
    return MOJO_RESULT_OK;
  }

  // If MojoAddTrigger succeeds but Arm does not, that means another thread
  // closed the watched handle in between. Treat it like we'd treat
  // MojoAddTrigger trying to watch an invalid handle.
  trap_handle_.reset();
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult MojoWatcher::Arm(MojoResult* ready_result) {
  // Nothing to do if the watcher is inactive.
  if (!handle_.is_valid())
    return MOJO_RESULT_OK;

  uint32_t num_blocking_events = 1;
  MojoTrapEvent blocking_event = {sizeof(blocking_event)};
  MojoResult result = MojoArmTrap(trap_handle_.get().value(), nullptr,
                                  &num_blocking_events, &blocking_event);
  if (result == MOJO_RESULT_OK)
    return MOJO_RESULT_OK;

  if (result == MOJO_RESULT_FAILED_PRECONDITION) {
    DCHECK_EQ(1u, num_blocking_events);
    DCHECK_EQ(reinterpret_cast<uintptr_t>(this),
              blocking_event.trigger_context);
    *ready_result = blocking_event.result;
    return result;
  }

  return result;
}

// static
void MojoWatcher::OnHandleReady(const MojoTrapEvent* event) {
  // It is safe to assume the MojoWathcer still exists. It stays alive at least
  // as long as |handle_| is valid, and |handle_| is only reset after we
  // dispatch a |MOJO_RESULT_CANCELLED| notification. That is always the last
  // notification received by this callback.
  MojoWatcher* watcher = reinterpret_cast<MojoWatcher*>(event->trigger_context);
  PostCrossThreadTask(
      *watcher->task_runner_, FROM_HERE,
      CrossThreadBindOnce(&MojoWatcher::RunReadyCallback,
                          WrapCrossThreadWeakPersistent(watcher),
                          event->result));
}

void MojoWatcher::RunReadyCallback(MojoResult result) {
  if (result == MOJO_RESULT_CANCELLED) {
    // Last notification.
    handle_ = mojo::Handle();

    // Only dispatch to the callback if this cancellation was implicit due to
    // |handle_| closure. If it was explicit, |trap_handlde_| has already been
    // reset.
    if (trap_handle_.is_valid()) {
      trap_handle_.reset();
      callback_->InvokeAndReportException(this, result);
    }
    return;
  }

  // Ignore callbacks if not watching.
  if (!trap_handle_.is_valid())
    return;

  callback_->InvokeAndReportException(this, result);

  // The user callback may have canceled watching.
  if (!trap_handle_.is_valid())
    return;

  // Rearm the watcher so another notification can fire.
  //
  // TODO(rockot): MojoWatcher should expose some better approximation of the
  // new watcher API, including explicit add and removal of handles from the
  // watcher, as well as explicit arming.
  MojoResult ready_result;
  MojoResult arm_result = Arm(&ready_result);
  if (arm_result == MOJO_RESULT_OK)
    return;

  if (arm_result == MOJO_RESULT_FAILED_PRECONDITION) {
    task_runner_->PostTask(FROM_HERE,
                           WTF::Bind(&MojoWatcher::RunReadyCallback,
                                     WrapWeakPersistent(this), ready_result));
    return;
  }
}

}  // namespace blink
